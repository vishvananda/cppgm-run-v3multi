#include "cy86_backend.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "posttoken.h"
#include "preproc_session.h"

namespace
{

const std::uint64_t kImageBase = 0x400000ULL;
const std::size_t kLoadOffset = 0x1000;
const std::size_t kStartupSize = 27;

extern "C" long int syscall(long int n, ...) throw ();

typedef std::size_t NameId;
typedef std::size_t LiteralId;

class NameTable
{
public:
	NameTable() : values_(1, std::string()), ids_() {}

	NameId intern(const std::string& spelling)
	{
		std::unordered_map<std::string, NameId>::const_iterator found =
			ids_.find(spelling);
		if (found != ids_.end())
			return found->second;
		const NameId id = values_.size();
		values_.push_back(spelling);
		ids_[values_.back()] = id;
		return id;
	}

	const std::string& get(NameId id) const
	{
		return values_.at(id);
	}

private:
	std::vector<std::string> values_;
	std::unordered_map<std::string, NameId> ids_;
};

struct StoredLiteral
{
	FundamentalType type;
	std::size_t element_count;
	std::size_t byte_begin;
	std::size_t byte_count;

	StoredLiteral()
		: type(FundamentalType::Int), element_count(0), byte_begin(0),
		  byte_count(0)
	{}
};

class LiteralStore
{
public:
	LiteralStore() : values_(), bytes_() {}

	LiteralId add(const LiteralData& literal)
	{
		StoredLiteral stored;
		stored.type = literal.type;
		stored.element_count = literal.element_count;
		stored.byte_begin = bytes_.size();
		stored.byte_count = literal.bytes.size();
		bytes_.insert(bytes_.end(), literal.bytes.begin(), literal.bytes.end());
		values_.push_back(stored);
		return values_.size() - 1;
	}

	const StoredLiteral& get(LiteralId id) const
	{
		return values_.at(id);
	}

	std::uint8_t byte(LiteralId id, std::size_t at) const
	{
		const StoredLiteral& literal = get(id);
		return bytes_.at(literal.byte_begin + at);
	}

	std::size_t size() const
	{
		return values_.size();
	}

private:
	std::vector<StoredLiteral> values_;
	std::vector<std::uint8_t> bytes_;
};

enum CyTokenKind
{
	CY_TOKEN_IDENTIFIER,
	CY_TOKEN_SIMPLE,
	CY_TOKEN_LITERAL
};

struct CyToken
{
	CyTokenKind kind;
	NameId name;
	SimpleTokenType simple;
	LiteralId literal;

	CyToken()
		: kind(CY_TOKEN_SIMPLE), name(0), simple(SimpleTokenType::OP_SEMICOLON),
		  literal(0)
	{}
};

class CyTokenCollector : public IPostTokenOutput
{
public:
	CyTokenCollector(NameTable& names, LiteralStore& literals,
		std::vector<CyToken>& tokens)
		: names_(names), literals_(literals), tokens_(tokens), invalid_(false)
	{}

	bool invalid() const { return invalid_; }

	void emit_invalid(const std::string& source)
	{
		(void)source;
		invalid_ = true;
	}

	void emit_simple(const std::string& source, SimpleTokenType type)
	{
		(void)source;
		CyToken token;
		token.kind = CY_TOKEN_SIMPLE;
		token.simple = type;
		tokens_.push_back(token);
	}

	void emit_identifier(const std::string& source)
	{
		CyToken token;
		token.kind = CY_TOKEN_IDENTIFIER;
		token.name = names_.intern(source);
		tokens_.push_back(token);
	}

	void emit_literal(const std::string& source, const LiteralData& value)
	{
		(void)source;
		CyToken token;
		token.kind = CY_TOKEN_LITERAL;
		token.literal = literals_.add(value);
		tokens_.push_back(token);
	}

	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		(void)value;
		invalid_ = true;
	}

	void emit_simple_identifier(const std::string& source,
		SimpleTokenType type)
	{
		(void)source;
		CyToken token;
		token.kind = CY_TOKEN_SIMPLE;
		token.simple = type;
		tokens_.push_back(token);
	}

	void emit_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source)
	{
		(void)spelling;
		emit_identifier(source);
	}

	void emit_simple_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source, SimpleTokenType type)
	{
		(void)spelling;
		emit_simple_identifier(source, type);
	}

	void emit_new_line() {}
	void emit_eof() {}

private:
	NameTable& names_;
	LiteralStore& literals_;
	std::vector<CyToken>& tokens_;
	bool invalid_;
};

bool read_source(const std::string& path, std::string* result)
{
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if (!input)
		return false;
	std::ostringstream stream;
	stream << input.rdbuf();
	*result = stream.str();
	return true;
}

enum RegisterGroup
{
	REG_SP,
	REG_BP,
	REG_X,
	REG_Y,
	REG_Z,
	REG_T
};

struct CyRegister
{
	RegisterGroup group;
	unsigned width;

	CyRegister(RegisterGroup group = REG_X, unsigned width = 64)
		: group(group), width(width)
	{}
};

bool parse_register(const std::string& name, CyRegister* result)
{
	if (name == "sp")
	{
		*result = CyRegister(REG_SP, 64);
		return true;
	}
	if (name == "bp")
	{
		*result = CyRegister(REG_BP, 64);
		return true;
	}
	if (name.size() != 2 && name.size() != 3)
		return false;
	const char family = name[0];
	RegisterGroup group;
	if (family == 'x') group = REG_X;
	else if (family == 'y') group = REG_Y;
	else if (family == 'z') group = REG_Z;
	else if (family == 't') group = REG_T;
	else return false;
	const std::string suffix = name.substr(1);
	unsigned width = 0;
	if (suffix == "8") width = 8;
	else if (suffix == "16") width = 16;
	else if (suffix == "32") width = 32;
	else if (suffix == "64") width = 64;
	else return false;
	*result = CyRegister(group, width);
	return true;
}

enum OpFamily
{
	OP_INVALID,
	OP_DATA,
	OP_MOVE,
	OP_JUMP,
	OP_JUMPIF,
	OP_CALL,
	OP_RET,
	OP_NOT,
	OP_AND,
	OP_OR,
	OP_XOR,
	OP_LSHIFT,
	OP_SRSHIFT,
	OP_URSHIFT,
	OP_INT_TO_F80,
	OP_FLOAT_TO_F80,
	OP_F80_TO_INT,
	OP_F80_TO_FLOAT,
	OP_IADD,
	OP_ISUB,
	OP_FADD,
	OP_FSUB,
	OP_SMUL,
	OP_UMUL,
	OP_FMUL,
	OP_SDIV,
	OP_UDIV,
	OP_FDIV,
	OP_SMOD,
	OP_UMOD,
	OP_ICMP,
	OP_FCMP,
	OP_SYSCALL
};

enum CompareKind
{
	CMP_EQ,
	CMP_NE,
	CMP_LT,
	CMP_GT,
	CMP_LE,
	CMP_GE
};

struct OpcodeInfo
{
	OpFamily family;
	unsigned width;
	unsigned auxiliary_width;
	unsigned syscall_arguments;
	bool signed_operation;
	CompareKind compare;

	OpcodeInfo()
		: family(OP_INVALID), width(0), auxiliary_width(0),
		  syscall_arguments(0), signed_operation(false), compare(CMP_EQ)
	{}
};

bool starts_with(const std::string& value, const char* prefix)
{
	const std::string p(prefix);
	return value.size() >= p.size() && value.compare(0, p.size(), p) == 0;
}

bool ends_with(const std::string& value, const char* suffix)
{
	const std::string s(suffix);
	return value.size() >= s.size() &&
		value.compare(value.size() - s.size(), s.size(), s) == 0;
}

bool parse_width_suffix(const std::string& value, std::size_t at,
	unsigned* width)
{
	if (value.compare(at, std::string::npos, "8") == 0)
	{
		*width = 8;
		return true;
	}
	if (value.compare(at, std::string::npos, "16") == 0)
	{
		*width = 16;
		return true;
	}
	if (value.compare(at, std::string::npos, "32") == 0)
	{
		*width = 32;
		return true;
	}
	if (value.compare(at, std::string::npos, "64") == 0)
	{
		*width = 64;
		return true;
	}
	if (value.compare(at, std::string::npos, "80") == 0)
	{
		*width = 80;
		return true;
	}
	return false;
}

bool classify_opcode(const std::string& name, OpcodeInfo* result)
{
	OpcodeInfo info;
	if (name == "data8" || name == "data16" || name == "data32" ||
		name == "data64")
	{
		info.family = OP_DATA;
		info.width = static_cast<unsigned>(name[4] - '0');
		if (name == "data16") info.width = 16;
		if (name == "data32") info.width = 32;
		if (name == "data64") info.width = 64;
	}
	else if (starts_with(name, "move") &&
		parse_width_suffix(name, 4, &info.width))
		info.family = OP_MOVE;
	else if (name == "jump") info.family = OP_JUMP;
	else if (name == "jumpif") info.family = OP_JUMPIF;
	else if (name == "call") info.family = OP_CALL;
	else if (name == "ret") info.family = OP_RET;
	else if (starts_with(name, "not") &&
		parse_width_suffix(name, 3, &info.width))
		info.family = OP_NOT;
	else if (starts_with(name, "and") &&
		parse_width_suffix(name, 3, &info.width))
		info.family = OP_AND;
	else if (starts_with(name, "or") &&
		parse_width_suffix(name, 2, &info.width))
		info.family = OP_OR;
	else if (starts_with(name, "xor") &&
		parse_width_suffix(name, 3, &info.width))
		info.family = OP_XOR;
	else if (starts_with(name, "lshift") &&
		parse_width_suffix(name, 6, &info.width))
		info.family = OP_LSHIFT;
	else if (starts_with(name, "srshift") &&
		parse_width_suffix(name, 7, &info.width))
		info.family = OP_SRSHIFT;
	else if (starts_with(name, "urshift") &&
		parse_width_suffix(name, 7, &info.width))
		info.family = OP_URSHIFT;
	else if (ends_with(name, "convf80"))
	{
		const std::string prefix = name.substr(0, name.size() - 7);
		if (prefix.size() > 1 &&
			(prefix[0] == 's' || prefix[0] == 'u' || prefix[0] == 'f') &&
			parse_width_suffix(prefix, 1, &info.auxiliary_width))
		{
			info.family = prefix[0] == 'f' ? OP_FLOAT_TO_F80 : OP_INT_TO_F80;
			info.width = 80;
			info.signed_operation = prefix[0] == 's';
		}
	}
	else if (starts_with(name, "f80conv"))
	{
		const std::string suffix = name.substr(7);
		if (suffix.size() > 1 && suffix[0] == 'f' &&
			parse_width_suffix(suffix, 1, &info.auxiliary_width))
		{
			info.family = OP_F80_TO_FLOAT;
			info.width = info.auxiliary_width;
		}
		else if (suffix.size() > 1 &&
			(suffix[0] == 's' || suffix[0] == 'u') &&
			parse_width_suffix(suffix, 1, &info.auxiliary_width))
		{
			info.family = OP_F80_TO_INT;
			info.width = info.auxiliary_width;
			info.signed_operation = suffix[0] == 's';
		}
	}
	else
	{
		struct Prefix
		{
			const char* text;
			OpFamily family;
			bool signed_operation;
		};
		const Prefix prefixes[] =
		{
			{"iadd", OP_IADD, false}, {"isub", OP_ISUB, false},
			{"fadd", OP_FADD, false}, {"fsub", OP_FSUB, false},
			{"smul", OP_SMUL, true}, {"umul", OP_UMUL, false},
			{"fmul", OP_FMUL, false}, {"sdiv", OP_SDIV, true},
			{"udiv", OP_UDIV, false}, {"fdiv", OP_FDIV, false},
			{"smod", OP_SMOD, true}, {"umod", OP_UMOD, false}
		};
		for (std::size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
		{
			const std::string prefix(prefixes[i].text);
			if (starts_with(name, prefixes[i].text) &&
				parse_width_suffix(name, prefix.size(), &info.width))
			{
				info.family = prefixes[i].family;
				info.signed_operation = prefixes[i].signed_operation;
				break;
			}
		}
		if (info.family == OP_INVALID)
		{
			const Prefix compare_prefixes[] =
			{
				{"ieq", OP_ICMP, false}, {"ine", OP_ICMP, false},
				{"slt", OP_ICMP, true}, {"ult", OP_ICMP, false},
				{"sgt", OP_ICMP, true}, {"ugt", OP_ICMP, false},
				{"sle", OP_ICMP, true}, {"ule", OP_ICMP, false},
				{"sge", OP_ICMP, true}, {"uge", OP_ICMP, false},
				{"feq", OP_FCMP, false}, {"fne", OP_FCMP, false},
				{"flt", OP_FCMP, false}, {"fgt", OP_FCMP, false},
				{"fle", OP_FCMP, false}, {"fge", OP_FCMP, false}
			};
			for (std::size_t i = 0;
				i < sizeof(compare_prefixes) / sizeof(compare_prefixes[0]); ++i)
			{
				const std::string prefix(compare_prefixes[i].text);
				if (!starts_with(name, compare_prefixes[i].text) ||
					!parse_width_suffix(name, prefix.size(), &info.width))
					continue;
				info.family = compare_prefixes[i].family;
				info.signed_operation = compare_prefixes[i].signed_operation;
				if (prefix == "ieq" || prefix == "feq") info.compare = CMP_EQ;
				else if (prefix == "ine" || prefix == "fne") info.compare = CMP_NE;
				else if (prefix == "slt" || prefix == "ult" || prefix == "flt") info.compare = CMP_LT;
				else if (prefix == "sgt" || prefix == "ugt" || prefix == "fgt") info.compare = CMP_GT;
				else if (prefix == "sle" || prefix == "ule" || prefix == "fle") info.compare = CMP_LE;
				else info.compare = CMP_GE;
				break;
			}
		}
	}

	if (info.family == OP_INVALID && starts_with(name, "syscall") &&
		name.size() == 8 && name[7] >= '0' && name[7] <= '6')
	{
		info.family = OP_SYSCALL;
		info.width = 64;
		info.syscall_arguments = static_cast<unsigned>(name[7] - '0');
	}
	if (info.family == OP_INVALID)
		return false;
	*result = info;
	return true;
}

enum RawExprKind
{
	RAW_NAME,
	RAW_LITERAL
};

enum ExprResolvedKind
{
	EXPR_UNRESOLVED,
	EXPR_REGISTER,
	EXPR_LABEL,
	EXPR_LITERAL
};

struct CyExpr
{
	RawExprKind raw_kind;
	NameId name;
	LiteralId literal;
	bool force_label;
	bool negative;
	bool has_offset;
	bool offset_negative;
	LiteralId offset_literal;
	ExprResolvedKind resolved;
	CyRegister reg;

	CyExpr()
		: raw_kind(RAW_LITERAL), name(0), literal(0), force_label(false),
		  negative(false), has_offset(false), offset_negative(false),
		  offset_literal(0), resolved(EXPR_UNRESOLVED), reg()
	{}
};

struct CyOperand
{
	enum Kind
	{
		RAW_IMMEDIATE,
		RAW_MEMORY,
		REGISTER,
		IMMEDIATE,
		MEMORY
	};

	Kind kind;
	CyExpr expr;
	CyRegister reg;

	CyOperand() : kind(RAW_IMMEDIATE), expr(), reg() {}
};

struct CyStatement
{
	enum Kind
	{
		INSTRUCTION,
		LITERAL_DATA
	};

	Kind kind;
	std::vector<NameId> labels;
	NameId opcode_name;
	OpcodeInfo opcode;
	std::vector<CyOperand> operands;
	LiteralId literal;
	bool negative_literal;

	CyStatement()
		: kind(INSTRUCTION), labels(), opcode_name(0), opcode(), operands(),
		  literal(0), negative_literal(false)
	{}
};

class CyParser
{
public:
	CyParser(const std::vector<CyToken>& tokens, const LiteralStore& literals,
		const NameTable& names)
		: tokens_(tokens), literals_(literals), names_(names), at_(0),
		  statements_(), label_names_()
	{}

	void parse(std::vector<CyStatement>* statements,
		std::unordered_set<NameId>* label_names)
	{
		while (at_ < tokens_.size())
		{
			CyStatement statement;
			parse_statement(&statement);
			statements_.push_back(statement);
		}
		*statements = statements_;
		*label_names = label_names_;
	}

private:
	const std::vector<CyToken>& tokens_;
	const LiteralStore& literals_;
	const NameTable& names_;
	std::size_t at_;
	std::vector<CyStatement> statements_;
	std::unordered_set<NameId> label_names_;

	const CyToken& current() const
	{
		if (at_ >= tokens_.size())
			throw std::runtime_error("unexpected end of CY86 program");
		return tokens_[at_];
	}

	bool is_simple(SimpleTokenType type) const
	{
		return at_ < tokens_.size() && tokens_[at_].kind == CY_TOKEN_SIMPLE &&
			tokens_[at_].simple == type;
	}

	void require_simple(SimpleTokenType type)
	{
		if (!is_simple(type))
			throw std::runtime_error("invalid CY86 grammar");
		++at_;
	}

	NameId require_identifier()
	{
		if (at_ >= tokens_.size() || tokens_[at_].kind != CY_TOKEN_IDENTIFIER)
			throw std::runtime_error("expected CY86 identifier");
		return tokens_[at_++].name;
	}

	LiteralId require_literal()
	{
		if (at_ >= tokens_.size() || tokens_[at_].kind != CY_TOKEN_LITERAL)
			throw std::runtime_error("expected CY86 literal");
		return tokens_[at_++].literal;
	}

	CyExpr parse_expression(bool force_label)
	{
		CyExpr expression;
		expression.force_label = force_label;
		if (at_ < tokens_.size() && tokens_[at_].kind == CY_TOKEN_LITERAL)
		{
			expression.raw_kind = RAW_LITERAL;
			expression.literal = require_literal();
			return expression;
		}
		expression.raw_kind = RAW_NAME;
		expression.name = require_identifier();
		return expression;
	}

	CyOperand parse_parenthesized_operand()
	{
		CyOperand operand;
		operand.kind = CyOperand::RAW_IMMEDIATE;
		require_simple(SimpleTokenType::OP_LPAREN);
		if (is_simple(SimpleTokenType::OP_MINUS))
		{
			++at_;
			operand.expr.raw_kind = RAW_LITERAL;
			operand.expr.literal = require_literal();
			operand.expr.negative = true;
			require_simple(SimpleTokenType::OP_RPAREN);
			return operand;
		}
		operand.expr = parse_expression(true);
		if (is_simple(SimpleTokenType::OP_PLUS) ||
			is_simple(SimpleTokenType::OP_MINUS))
		{
			operand.expr.has_offset = true;
			operand.expr.offset_negative = is_simple(SimpleTokenType::OP_MINUS);
			++at_;
			operand.expr.offset_literal = require_literal();
		}
		require_simple(SimpleTokenType::OP_RPAREN);
		return operand;
	}

	CyOperand parse_memory_operand()
	{
		CyOperand operand;
		operand.kind = CyOperand::RAW_MEMORY;
		require_simple(SimpleTokenType::OP_LSQUARE);
		operand.expr = parse_expression(false);
		if (is_simple(SimpleTokenType::OP_PLUS) ||
			is_simple(SimpleTokenType::OP_MINUS))
		{
			operand.expr.has_offset = true;
			operand.expr.offset_negative = is_simple(SimpleTokenType::OP_MINUS);
			++at_;
			operand.expr.offset_literal = require_literal();
		}
		require_simple(SimpleTokenType::OP_RSQUARE);
		return operand;
	}

	CyOperand parse_operand()
	{
		if (is_simple(SimpleTokenType::OP_LSQUARE))
			return parse_memory_operand();
		if (is_simple(SimpleTokenType::OP_LPAREN))
			return parse_parenthesized_operand();

		CyOperand operand;
		operand.kind = CyOperand::RAW_IMMEDIATE;
		if (at_ < tokens_.size() &&
			tokens_[at_].kind == CY_TOKEN_LITERAL)
			operand.expr = parse_expression(false);
		else if (at_ < tokens_.size() &&
			tokens_[at_].kind == CY_TOKEN_IDENTIFIER)
			operand.expr = parse_expression(false);
		else
			throw std::runtime_error("invalid CY86 operand");
		return operand;
	}

	void parse_statement(CyStatement* result)
	{
		while (at_ + 1 < tokens_.size() &&
			tokens_[at_].kind == CY_TOKEN_IDENTIFIER &&
			is_simple_at(at_ + 1, SimpleTokenType::OP_COLON))
		{
			const NameId label = tokens_[at_].name;
			result->labels.push_back(label);
			label_names_.insert(label);
			at_ += 2;
		}

		if (at_ >= tokens_.size())
			throw std::runtime_error("label without CY86 statement");
		if (tokens_[at_].kind == CY_TOKEN_LITERAL ||
			(is_simple(SimpleTokenType::OP_MINUS) && at_ + 1 < tokens_.size() &&
				tokens_[at_ + 1].kind == CY_TOKEN_LITERAL))
		{
			result->kind = CyStatement::LITERAL_DATA;
			result->negative_literal = is_simple(SimpleTokenType::OP_MINUS);
			if (result->negative_literal)
				++at_;
			result->literal = require_literal();
			require_simple(SimpleTokenType::OP_SEMICOLON);
			return;
		}

		const NameId opcode_name = require_identifier();
		result->opcode_name = opcode_name;
		classify_opcode(names_.get(opcode_name), &result->opcode);
		while (!is_simple(SimpleTokenType::OP_SEMICOLON))
		{
			if (at_ >= tokens_.size())
				throw std::runtime_error("unterminated CY86 statement");
			result->operands.push_back(parse_operand());
		}
		require_simple(SimpleTokenType::OP_SEMICOLON);
	}

	bool is_simple_at(std::size_t position, SimpleTokenType type) const
	{
		return position < tokens_.size() && tokens_[position].kind == CY_TOKEN_SIMPLE &&
			tokens_[position].simple == type;
	}
};

enum ValueKind
{
	VALUE_INTEGER,
	VALUE_FLOAT,
	VALUE_ADDRESS,
	VALUE_ANY
};

struct OperandConstraint
{
	bool write;
	bool immediate_only;
	bool address;
	unsigned width;
	ValueKind value_kind;

	OperandConstraint(bool write = false, bool immediate_only = false,
		bool address = false, unsigned width = 0,
		ValueKind value_kind = VALUE_ANY)
		: write(write), immediate_only(immediate_only), address(address),
		  width(width), value_kind(value_kind)
	{}
};

bool is_integral_type(FundamentalType type)
{
	return type == FundamentalType::SignedChar || type == FundamentalType::ShortInt ||
		type == FundamentalType::Int || type == FundamentalType::LongInt ||
		type == FundamentalType::LongLongInt || type == FundamentalType::UnsignedChar ||
		type == FundamentalType::UnsignedShortInt || type == FundamentalType::UnsignedInt ||
		type == FundamentalType::UnsignedLongInt || type == FundamentalType::UnsignedLongLongInt ||
		type == FundamentalType::WcharT || type == FundamentalType::Char ||
		type == FundamentalType::Char16T || type == FundamentalType::Char32T ||
		type == FundamentalType::Bool;
}

bool is_signed_type(FundamentalType type)
{
	return type == FundamentalType::SignedChar || type == FundamentalType::ShortInt ||
		type == FundamentalType::Int || type == FundamentalType::LongInt ||
		type == FundamentalType::LongLongInt || type == FundamentalType::WcharT ||
		type == FundamentalType::Char;
}

bool is_floating_type(FundamentalType type)
{
	return type == FundamentalType::Float || type == FundamentalType::Double ||
		type == FundamentalType::LongDouble;
}

bool literal_is_valid_scalar(const LiteralStore& literals, LiteralId id,
	ValueKind value_kind)
{
	const StoredLiteral& literal = literals.get(id);
	if (literal.element_count != 0)
		return false;
	if (value_kind == VALUE_INTEGER || value_kind == VALUE_ADDRESS)
		return is_integral_type(literal.type);
	if (value_kind == VALUE_FLOAT)
		return is_floating_type(literal.type);
	return true;
}

bool literal_is_valid_immediate(const LiteralStore& literals, LiteralId id,
	ValueKind value_kind)
{
	const StoredLiteral& literal = literals.get(id);
	if (value_kind == VALUE_INTEGER)
		return is_integral_type(literal.type) && literal.byte_count != 0;
	if (value_kind == VALUE_ADDRESS)
		return literal_is_valid_scalar(literals, id, value_kind);
	if (value_kind == VALUE_FLOAT)
		return literal_is_valid_scalar(literals, id, value_kind);
	return literal.byte_count != 0;
}

class CySemantic
{
public:
	CySemantic(const NameTable& names, const LiteralStore& literals,
		std::vector<CyStatement>& statements,
		const std::unordered_set<NameId>& label_names)
		: names_(names), literals_(literals), statements_(statements),
		  label_names_(label_names), label_indices_()
	{}

	const std::unordered_map<NameId, std::size_t>& label_indices() const
	{
		return label_indices_;
	}

	void validate()
	{
		for (std::size_t i = 0; i < statements_.size(); ++i)
		{
			const CyStatement& statement = statements_[i];
			for (std::size_t j = 0; j < statement.labels.size(); ++j)
			{
				const NameId label = statement.labels[j];
				CyRegister ignored;
				OpcodeInfo ignored_opcode;
				if (parse_register(names_.get(label), &ignored) ||
					classify_opcode(names_.get(label), &ignored_opcode))
					throw std::runtime_error("CY86 label conflicts with fixed name");
				if (!label_indices_.insert(std::make_pair(label, i)).second)
					throw std::runtime_error("duplicate CY86 label");
			}
		}

		for (std::size_t i = 0; i < statements_.size(); ++i)
		{
			CyStatement& statement = statements_[i];
			if (statement.kind == CyStatement::LITERAL_DATA)
			{
				const StoredLiteral& literal = literals_.get(statement.literal);
				if (statement.negative_literal &&
					(literal.element_count != 0 ||
						(!is_integral_type(literal.type) &&
						 !is_floating_type(literal.type))))
					throw std::runtime_error("invalid negated CY86 literal");
				continue;
			}
			if (statement.opcode.family == OP_INVALID)
				throw std::runtime_error("unknown CY86 opcode");
			std::vector<OperandConstraint> constraints;
			make_constraints(statement.opcode, &constraints);
			if (constraints.size() != statement.operands.size())
				throw std::runtime_error("wrong CY86 operand count");
			for (std::size_t j = 0; j < statement.operands.size(); ++j)
			{
				resolve_operand(&statement.operands[j]);
				validate_operand(statement.operands[j], constraints[j]);
			}
		}
	}

private:
	const NameTable& names_;
	const LiteralStore& literals_;
	std::vector<CyStatement>& statements_;
	const std::unordered_set<NameId>& label_names_;
	std::unordered_map<NameId, std::size_t> label_indices_;

	void make_constraints(const OpcodeInfo& opcode,
		std::vector<OperandConstraint>* result) const
	{
		result->clear();
		const unsigned w = opcode.width;
		switch (opcode.family)
		{
		case OP_DATA: result->push_back(OperandConstraint(false, true, false, w, VALUE_ANY)); break;
		case OP_MOVE:
			result->push_back(OperandConstraint(true, false, false, w,
				VALUE_ANY));
			result->push_back(OperandConstraint(false, false, false, w,
				VALUE_ANY));
			break;
		case OP_JUMP: result->push_back(OperandConstraint(false, false, true, 64, VALUE_ADDRESS)); break;
		case OP_JUMPIF:
			result->push_back(OperandConstraint(false, false, false, 8, VALUE_INTEGER));
			result->push_back(OperandConstraint(false, false, true, 64, VALUE_ADDRESS));
			break;
		case OP_CALL: result->push_back(OperandConstraint(false, false, true, 64, VALUE_ADDRESS)); break;
		case OP_RET: break;
		case OP_NOT:
			result->push_back(OperandConstraint(true, false, false, w, VALUE_INTEGER));
			result->push_back(OperandConstraint(false, false, false, w, VALUE_INTEGER));
			break;
		case OP_AND: case OP_OR: case OP_XOR:
		case OP_IADD: case OP_ISUB: case OP_SMUL: case OP_UMUL:
		case OP_SDIV: case OP_UDIV: case OP_SMOD: case OP_UMOD:
			for (unsigned i = 0; i < 3; ++i)
				result->push_back(OperandConstraint(i == 0, false, false, w, VALUE_INTEGER));
			break;
		case OP_LSHIFT: case OP_SRSHIFT: case OP_URSHIFT:
			result->push_back(OperandConstraint(true, false, false, w, VALUE_INTEGER));
			result->push_back(OperandConstraint(false, false, false, w, VALUE_INTEGER));
			result->push_back(OperandConstraint(false, false, false, 8, VALUE_INTEGER));
			break;
		case OP_INT_TO_F80:
			result->push_back(OperandConstraint(true, false, false, 80, VALUE_FLOAT));
			result->push_back(OperandConstraint(false, false, false,
				opcode.auxiliary_width, VALUE_INTEGER));
			break;
		case OP_FLOAT_TO_F80:
			result->push_back(OperandConstraint(true, false, false, 80, VALUE_FLOAT));
			result->push_back(OperandConstraint(false, false, false,
				opcode.auxiliary_width, VALUE_FLOAT));
			break;
		case OP_F80_TO_INT:
			result->push_back(OperandConstraint(true, false, false,
				opcode.auxiliary_width, VALUE_INTEGER));
			result->push_back(OperandConstraint(false, false, false, 80, VALUE_FLOAT));
			break;
		case OP_F80_TO_FLOAT:
			result->push_back(OperandConstraint(true, false, false, opcode.width, VALUE_FLOAT));
			result->push_back(OperandConstraint(false, false, false, 80, VALUE_FLOAT));
			break;
		case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV:
			for (unsigned i = 0; i < 3; ++i)
				result->push_back(OperandConstraint(i == 0, false, false, w, VALUE_FLOAT));
			break;
		case OP_ICMP: case OP_FCMP:
			result->push_back(OperandConstraint(true, false, false, 8, VALUE_INTEGER));
			result->push_back(OperandConstraint(false, false, false, w,
				opcode.family == OP_ICMP ? VALUE_INTEGER : VALUE_FLOAT));
			result->push_back(OperandConstraint(false, false, false, w,
				opcode.family == OP_ICMP ? VALUE_INTEGER : VALUE_FLOAT));
			break;
		case OP_SYSCALL:
			result->push_back(OperandConstraint(true, false, false, 64, VALUE_INTEGER));
			for (unsigned i = 0; i < opcode.syscall_arguments + 1; ++i)
				result->push_back(OperandConstraint(false, false, false, 64, VALUE_INTEGER));
			break;
		default: break;
		}
	}

	void resolve_expr(CyExpr* expression, bool allow_register)
	{
		if (expression->raw_kind == RAW_LITERAL)
		{
			expression->resolved = EXPR_LITERAL;
			return;
		}
		CyRegister reg;
		if (allow_register && !expression->force_label &&
			parse_register(names_.get(expression->name), &reg))
		{
			expression->resolved = EXPR_REGISTER;
			expression->reg = reg;
			if (expression->has_offset &&
				!literal_is_valid_scalar(literals_, expression->offset_literal,
					VALUE_INTEGER))
				throw std::runtime_error("non-integral CY86 address offset");
			return;
		}
		if (label_names_.find(expression->name) == label_names_.end())
			throw std::runtime_error("undefined CY86 label");
		expression->resolved = EXPR_LABEL;
		if (expression->has_offset &&
			!literal_is_valid_scalar(literals_, expression->offset_literal,
				VALUE_INTEGER))
			throw std::runtime_error("non-integral CY86 label offset");
	}

	void resolve_operand(CyOperand* operand)
	{
		if (operand->kind == CyOperand::RAW_MEMORY)
		{
			resolve_expr(&operand->expr, true);
			operand->kind = CyOperand::MEMORY;
			if (operand->expr.resolved == EXPR_LITERAL &&
				!literal_is_valid_scalar(literals_, operand->expr.literal,
					VALUE_ADDRESS))
				throw std::runtime_error("non-integral CY86 memory address");
			if (operand->expr.resolved == EXPR_REGISTER &&
				operand->expr.reg.width != 64)
				throw std::runtime_error("CY86 memory base must be 64-bit");
			return;
		}
		resolve_expr(&operand->expr, true);
		if (operand->expr.resolved == EXPR_REGISTER)
		{
			operand->kind = CyOperand::REGISTER;
			operand->reg = operand->expr.reg;
		}
		else
			operand->kind = CyOperand::IMMEDIATE;
	}

	void validate_operand(const CyOperand& operand,
		const OperandConstraint& constraint) const
	{
		if (constraint.immediate_only && operand.kind != CyOperand::IMMEDIATE)
			throw std::runtime_error("CY86 operand must be immediate");
		if (constraint.write && operand.kind == CyOperand::IMMEDIATE)
			throw std::runtime_error("CY86 destination cannot be immediate");
		if (operand.kind == CyOperand::REGISTER)
		{
			if (constraint.address && operand.reg.width != 64)
				throw std::runtime_error("CY86 address register width mismatch");
			if (!constraint.address && constraint.value_kind == VALUE_FLOAT)
				throw std::runtime_error("CY86 has no floating register");
			if (constraint.width != 0 && operand.reg.width != constraint.width)
				throw std::runtime_error("CY86 register width mismatch");
			return;
		}
		if (operand.kind == CyOperand::MEMORY)
			return;
		if (operand.expr.resolved == EXPR_LABEL)
		{
			if (constraint.value_kind == VALUE_FLOAT)
				throw std::runtime_error("label is not a floating CY86 value");
			return;
		}
		if (operand.expr.resolved != EXPR_LITERAL)
			throw std::runtime_error("unresolved CY86 operand");
		if (!literal_is_valid_immediate(literals_, operand.expr.literal,
			constraint.value_kind))
			throw std::runtime_error("CY86 immediate type mismatch");
	}
};

enum X86Reg
{
	X86_RAX = 0,
	X86_RCX = 1,
	X86_RDX = 2,
	X86_RBX = 3,
	X86_RSP = 4,
	X86_RBP = 5,
	X86_RSI = 6,
	X86_RDI = 7,
	X86_R8 = 8,
	X86_R9 = 9,
	X86_R10 = 10,
	X86_R11 = 11,
	X86_R12 = 12,
	X86_R13 = 13,
	X86_R14 = 14,
	X86_R15 = 15
};

X86Reg x86_register(const CyRegister& reg)
{
	if (reg.group == REG_SP) return X86_RSP;
	if (reg.group == REG_BP) return X86_RBP;
	if (reg.group == REG_X) return X86_R12;
	if (reg.group == REG_Y) return X86_R13;
	if (reg.group == REG_Z) return X86_R14;
	return X86_R15;
}

X86Reg choose_offset_scratch(X86Reg avoid1, X86Reg avoid2,
	X86Reg avoid3)
{
	const X86Reg candidates[] =
		{X86_RAX, X86_RCX, X86_RDX, X86_R10, X86_R11};
	for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i)
		if (candidates[i] != avoid1 && candidates[i] != avoid2 &&
			candidates[i] != avoid3)
			return candidates[i];
	throw std::runtime_error("no safe CY86 address scratch register");
}

bool literal_is_signed(const StoredLiteral& literal)
{
	return is_signed_type(literal.type);
}

std::uint64_t width_mask(unsigned width)
{
	if (width >= 64)
		return std::numeric_limits<std::uint64_t>::max();
	return (static_cast<std::uint64_t>(1) << width) - 1;
}

std::uint64_t literal_integer_value(const LiteralStore& literals,
	LiteralId id, unsigned target_width, bool negative)
{
	const StoredLiteral& literal = literals.get(id);
	if (!is_integral_type(literal.type))
		throw std::runtime_error("expected integral CY86 literal");
	const unsigned source_width = static_cast<unsigned>(literal.byte_count * 8);
	if (source_width == 0 || source_width > 64 || target_width > 64)
		throw std::runtime_error("unsupported CY86 literal width");
	std::uint64_t value = 0;
	for (std::size_t i = 0; i < literal.byte_count; ++i)
		value |= static_cast<std::uint64_t>(literals.byte(id, i)) << (i * 8);
	if (negative)
	{
		value = static_cast<std::uint64_t>(0) - value;
		value &= width_mask(source_width);
	}
	if (target_width < source_width)
		value &= width_mask(target_width);
	else if (target_width > source_width && literal_is_signed(literal) &&
		(value & (static_cast<std::uint64_t>(1) << (source_width - 1))) != 0)
		value |= ~width_mask(source_width);
	return value & width_mask(target_width);
}

std::uint64_t literal_offset_value(const LiteralStore& literals,
	LiteralId id, bool negative)
{
	return literal_integer_value(literals, id, 64, negative);
}

void negate_literal_bytes(std::vector<std::uint8_t>* bytes)
{
	unsigned carry = 1;
	for (std::size_t i = 0; i < bytes->size(); ++i)
	{
		const unsigned value =
			(static_cast<unsigned>(~(*bytes)[i]) & 0xFFU) + carry;
		(*bytes)[i] = static_cast<std::uint8_t>(value);
		carry = value >> 8;
	}
}

void literal_value_bytes(const LiteralStore& literals, LiteralId id,
	bool negative, std::vector<std::uint8_t>* output)
{
	const StoredLiteral& literal = literals.get(id);
	output->clear();
	for (std::size_t i = 0; i < literal.byte_count; ++i)
		output->push_back(literals.byte(id, i));
	if (!negative)
		return;
	if (literal.element_count != 0)
		throw std::runtime_error("cannot negate a CY86 array literal");
	if (is_integral_type(literal.type))
	{
		negate_literal_bytes(output);
		return;
	}
	if (is_floating_type(literal.type) && !output->empty())
	{
		const std::size_t sign_byte =
			literal.type == FundamentalType::LongDouble ? 9 : output->size() - 1;
		if (sign_byte < output->size())
			(*output)[sign_byte] ^= 0x80;
		return;
	}
	throw std::runtime_error("cannot negate this CY86 literal");
}

void converted_literal_bytes(const LiteralStore& literals, LiteralId id,
	unsigned target_width, bool negative, std::vector<std::uint8_t>* output)
{
	const std::size_t target_bytes = target_width / 8;
	const StoredLiteral& literal = literals.get(id);
	std::vector<std::uint8_t> source;
	literal_value_bytes(literals, id, negative, &source);
	const bool sign_extend = literal.element_count == 0 &&
		is_integral_type(literal.type) && is_signed_type(literal.type) &&
		!source.empty() && (source.back() & 0x80) != 0;
	if (source.size() > target_bytes)
		source.resize(target_bytes);
	output->assign(source.begin(), source.end());
	while (output->size() < target_bytes)
		output->push_back(sign_extend ? 0xFF : 0x00);
}

void converted_address_bytes(std::uint64_t value, unsigned target_width,
	std::vector<std::uint8_t>* output)
{
	output->clear();
	for (unsigned i = 0; i < target_width / 8; ++i)
	{
		if (i < 8)
			output->push_back(static_cast<std::uint8_t>(value >> (i * 8)));
		else
			output->push_back(0);
	}
}

class CodeEmitter
{
public:
	CodeEmitter(std::vector<std::uint8_t>* output, bool dry_run,
		const NameTable& names, const LiteralStore& literals,
		const std::unordered_map<NameId, std::uint64_t>& labels)
		: output_(output), dry_run_(dry_run), names_(names), literals_(literals),
		  labels_(labels), position_(0)
	{}

	std::size_t position() const { return position_; }

	void set_position(std::size_t position)
	{
		position_ = position;
	}

	void byte(std::uint8_t value)
	{
		if (!dry_run_)
			output_->push_back(value);
		++position_;
	}

	void bytes(const std::vector<std::uint8_t>& values)
	{
		for (std::size_t i = 0; i < values.size(); ++i)
			byte(values[i]);
	}

	void u32(std::uint32_t value)
	{
		for (unsigned at = 0; at < 4; ++at)
			byte(static_cast<std::uint8_t>(value >> (at * 8)));
	}

	void u16(std::uint16_t value)
	{
		byte(static_cast<std::uint8_t>(value));
		byte(static_cast<std::uint8_t>(value >> 8));
	}

	void u64(std::uint64_t value)
	{
		for (unsigned at = 0; at < 8; ++at)
			byte(static_cast<std::uint8_t>(value >> (at * 8)));
	}

	std::uint64_t runtime_address(std::size_t position) const
	{
		return kImageBase + kLoadOffset + position;
	}

	std::uint64_t expression_value(const CyExpr& expression) const
	{
		std::uint64_t value = 0;
		if (expression.resolved == EXPR_LITERAL)
			value = literal_integer_value(literals_, expression.literal, 64,
				expression.negative);
		else if (expression.resolved == EXPR_LABEL)
		{
			std::unordered_map<NameId, std::uint64_t>::const_iterator found =
				labels_.find(expression.name);
			if (found == labels_.end())
			{
				if (dry_run_)
					return 0;
				throw std::runtime_error("missing CY86 label address");
			}
			value = found->second;
		}
		else
			throw std::runtime_error("register used as CY86 constant");

		if (expression.has_offset)
		{
			const std::uint64_t offset = literal_offset_value(literals_,
				expression.offset_literal, expression.offset_negative);
			value += offset;
		}
		return value;
	}

	void align_absolute(std::size_t alignment)
	{
		if (alignment == 0)
			return;
		while (runtime_address(position_) % alignment != 0)
			byte(0);
	}

	void emit_rex(bool wide, int reg_field, int rm_field,
		bool force = false)
	{
		const bool r = reg_field >= 8;
		const bool b = rm_field >= 8;
		const std::uint8_t rex = static_cast<std::uint8_t>(0x40 |
			(wide ? 8 : 0) | (r ? 4 : 0) | (b ? 1 : 0));
		if (force || rex != 0x40)
			byte(rex);
	}

	void emit_prefix(unsigned width)
	{
		if (width == 16)
			byte(0x66);
	}

	void modrm(unsigned mode, unsigned reg_field, unsigned rm_field)
	{
		byte(static_cast<std::uint8_t>((mode << 6) |
			((reg_field & 7) << 3) | (rm_field & 7)));
	}

	void modrm_memory_disp32(X86Reg base, unsigned reg_field,
		std::int32_t displacement)
	{
		modrm(2, reg_field, static_cast<unsigned>(base));
		if ((static_cast<unsigned>(base) & 7) == 4)
			byte(0x24);
		u32(static_cast<std::uint32_t>(displacement));
	}

	void mov_reg_imm(X86Reg destination, unsigned width,
		std::uint64_t value)
	{
		if (width == 8)
		{
			emit_rex(false, -1, static_cast<int>(destination));
			byte(static_cast<std::uint8_t>(0xB0 +
				(static_cast<unsigned>(destination) & 7)));
			byte(static_cast<std::uint8_t>(value));
			return;
		}
		emit_prefix(width);
		emit_rex(width == 64, -1, static_cast<int>(destination));
		byte(static_cast<std::uint8_t>(0xB8 +
			(static_cast<unsigned>(destination) & 7)));
		if (width == 16)
			u16(static_cast<std::uint16_t>(value));
		else if (width == 32)
			u32(static_cast<std::uint32_t>(value));
		else
			u64(value);
	}

	void mov_reg_reg(X86Reg destination, X86Reg source, unsigned width)
	{
		emit_prefix(width);
		emit_rex(width == 64, static_cast<int>(source),
			static_cast<int>(destination));
		byte(width == 8 ? 0x88 : 0x89);
		modrm(3, static_cast<unsigned>(source),
			static_cast<unsigned>(destination));
	}

	void mov_reg_mem(X86Reg destination, X86Reg base, std::int32_t disp,
		unsigned width)
	{
		emit_prefix(width);
		emit_rex(width == 64, static_cast<int>(destination),
			static_cast<int>(base));
		byte(width == 8 ? 0x8A : 0x8B);
		modrm_memory_disp32(base, static_cast<unsigned>(destination), disp);
	}

	void mov_mem_reg(X86Reg base, std::int32_t disp, X86Reg source,
		unsigned width)
	{
		emit_prefix(width);
		emit_rex(width == 64, static_cast<int>(source),
			static_cast<int>(base));
		byte(width == 8 ? 0x88 : 0x89);
		modrm_memory_disp32(base, static_cast<unsigned>(source), disp);
	}

	void write_memory_bytes(X86Reg base, std::int32_t displacement,
		const std::vector<std::uint8_t>& values)
	{
		std::size_t at = 0;
		while (at + 8 <= values.size())
		{
			std::uint64_t value = 0;
			for (unsigned i = 0; i < 8; ++i)
				value |= static_cast<std::uint64_t>(values[at + i]) << (i * 8);
			mov_reg_imm(X86_RAX, 64, value);
			mov_mem_reg(base, displacement + static_cast<std::int32_t>(at),
				X86_RAX, 64);
			at += 8;
		}
		while (at + 4 <= values.size())
		{
			std::uint32_t value = 0;
			for (unsigned i = 0; i < 4; ++i)
				value |= static_cast<std::uint32_t>(values[at + i]) << (i * 8);
			mov_reg_imm(X86_RAX, 32, value);
			mov_mem_reg(base, displacement + static_cast<std::int32_t>(at),
				X86_RAX, 32);
			at += 4;
		}
		while (at + 2 <= values.size())
		{
			std::uint16_t value = static_cast<std::uint16_t>(values[at]) |
				static_cast<std::uint16_t>(values[at + 1]) << 8;
			mov_reg_imm(X86_RAX, 16, value);
			mov_mem_reg(base, displacement + static_cast<std::int32_t>(at),
				X86_RAX, 16);
			at += 2;
		}
		while (at < values.size())
		{
			mov_reg_imm(X86_RAX, 8, values[at]);
			mov_mem_reg(base, displacement + static_cast<std::int32_t>(at),
				X86_RAX, 8);
			++at;
		}
	}

	void binary_reg(X86Reg destination, X86Reg source, unsigned width,
		std::uint8_t opcode)
	{
		emit_prefix(width);
		emit_rex(width == 64, static_cast<int>(source),
			static_cast<int>(destination));
		byte(opcode);
		modrm(3, static_cast<unsigned>(source),
			static_cast<unsigned>(destination));
	}

	void binary_reg_0f(X86Reg destination, X86Reg source, unsigned width,
		std::uint8_t opcode)
	{
		emit_prefix(width);
		emit_rex(width == 64, static_cast<int>(destination),
			static_cast<int>(source));
		byte(0x0F);
		byte(opcode);
		modrm(3, static_cast<unsigned>(destination),
			static_cast<unsigned>(source));
	}

	void unary_reg(X86Reg reg, unsigned width, unsigned extension,
		std::uint8_t byte_opcode, std::uint8_t word_opcode)
	{
		emit_prefix(width);
		emit_rex(width == 64, static_cast<int>(extension),
			static_cast<int>(reg));
		byte(width == 8 ? byte_opcode : word_opcode);
		modrm(3, extension, static_cast<unsigned>(reg));
	}

	void setcc(X86Reg destination, unsigned condition)
	{
		emit_rex(false, 0, static_cast<int>(destination));
		byte(0x0F);
		byte(static_cast<std::uint8_t>(0x90 + condition));
		modrm(3, 0, static_cast<unsigned>(destination));
	}

	void movzx_eax_ah()
	{
		byte(0x0F);
		byte(0xB6);
		modrm(3, static_cast<unsigned>(X86_RAX), 4);
	}

	void test_reg(X86Reg left, X86Reg right, unsigned width)
	{
		emit_prefix(width);
		 emit_rex(width == 64, static_cast<int>(right),
			static_cast<int>(left));
		byte(width == 8 ? 0x84 : 0x85);
		modrm(3, static_cast<unsigned>(right), static_cast<unsigned>(left));
	}

	void cmp_reg(X86Reg left, X86Reg right, unsigned width)
	{
		binary_reg(left, right, width, width == 8 ? 0x38 : 0x39);
	}

	void shift_reg(X86Reg destination, unsigned width, unsigned extension)
	{
		emit_prefix(width);
		emit_rex(width == 64, static_cast<int>(extension),
			static_cast<int>(destination));
		byte(width == 8 ? 0xD2 : 0xD3);
		modrm(3, extension, static_cast<unsigned>(destination));
	}

	void div_reg(X86Reg divisor, unsigned width, unsigned extension)
	{
		emit_prefix(width);
		emit_rex(width == 64, static_cast<int>(extension),
			static_cast<int>(divisor));
		byte(width == 8 ? 0xF6 : 0xF7);
		modrm(3, extension, static_cast<unsigned>(divisor));
	}

	void jmp_reg(X86Reg target, bool call)
	{
		emit_rex(false, call ? 2 : 4, static_cast<int>(target));
		byte(0xFF);
		modrm(3, call ? 2 : 4, static_cast<unsigned>(target));
	}

	void rel32(std::uint64_t target)
	{
		if (dry_run_)
		{
			u32(0);
			return;
		}
		const std::int64_t difference = static_cast<std::int64_t>(target) -
			static_cast<std::int64_t>(runtime_address(position_ + 4));
		if (difference < std::numeric_limits<std::int32_t>::min() ||
			difference > std::numeric_limits<std::int32_t>::max())
			throw std::runtime_error("CY86 branch is out of range");
		u32(static_cast<std::uint32_t>(static_cast<std::int32_t>(difference)));
	}

	void jump_condition(unsigned condition, std::uint64_t target)
	{
		byte(0x0F);
		byte(static_cast<std::uint8_t>(0x80 + condition));
		rel32(target);
	}

	void jump_relative_to_position(unsigned condition, std::size_t target)
	{
		byte(0x0F);
		byte(static_cast<std::uint8_t>(0x80 + condition));
		if (dry_run_)
		{
			u32(0);
			return;
		}
		const std::int64_t difference = static_cast<std::int64_t>(
			runtime_address(target)) - static_cast<std::int64_t>(
			runtime_address(position_ + 4));
		u32(static_cast<std::uint32_t>(static_cast<std::int32_t>(difference)));
	}

	std::size_t jump_placeholder(unsigned condition)
	{
		byte(0x0F);
		byte(static_cast<std::uint8_t>(0x80 + condition));
		const std::size_t displacement = position_;
		u32(0);
		return displacement;
	}

	std::size_t jmp_placeholder()
	{
		byte(0xE9);
		const std::size_t displacement = position_;
		u32(0);
		return displacement;
	}

	void patch_relative(std::size_t displacement, std::size_t target)
	{
		if (dry_run_)
			return;
		const std::int64_t difference = static_cast<std::int64_t>(
			runtime_address(target)) - static_cast<std::int64_t>(
			runtime_address(displacement + 4));
		if (difference < std::numeric_limits<std::int32_t>::min() ||
			difference > std::numeric_limits<std::int32_t>::max())
			throw std::runtime_error("local CY86 branch is out of range");
		const std::uint32_t value = static_cast<std::uint32_t>(
			static_cast<std::int32_t>(difference));
		for (unsigned at = 0; at < 4; ++at)
			(*output_)[displacement + at] =
				static_cast<std::uint8_t>(value >> (at * 8));
	}

	void materialize_memory(const CyOperand& operand, X86Reg address,
		X86Reg offset_scratch)
	{
		if (operand.kind != CyOperand::MEMORY)
			throw std::runtime_error("expected CY86 memory operand");
		if (address == offset_scratch)
			throw std::runtime_error("CY86 address and offset scratch collide");
		const CyExpr& expression = operand.expr;
		if (expression.resolved == EXPR_REGISTER)
		{
			const X86Reg base = x86_register(expression.reg);
			mov_reg_reg(address, base, 64);
			if (expression.has_offset)
			{
				mov_reg_imm(offset_scratch, 64, literal_offset_value(literals_,
					expression.offset_literal, expression.offset_negative));
				binary_reg(address, offset_scratch, 64, 0x01);
			}
		}
		else
			mov_reg_imm(address, 64, expression_value(expression));
	}

	void load_operand(const CyOperand& operand, X86Reg destination,
		unsigned width, X86Reg address_scratch)
	{
		if (operand.kind == CyOperand::REGISTER)
		{
			mov_reg_reg(destination, x86_register(operand.reg), width);
			return;
		}
		if (operand.kind == CyOperand::IMMEDIATE)
		{
			if (operand.expr.resolved == EXPR_LITERAL)
			{
				std::vector<std::uint8_t> bytes;
				converted_literal_bytes(literals_, operand.expr.literal, width,
					operand.expr.negative, &bytes);
				std::uint64_t value = 0;
				for (std::size_t i = 0; i < bytes.size() && i < 8; ++i)
					value |= static_cast<std::uint64_t>(bytes[i]) << (i * 8);
				mov_reg_imm(destination, width, value);
			}
			else
				mov_reg_imm(destination, width, expression_value(operand.expr));
			return;
		}
		X86Reg address = address_scratch;
		if (address == destination)
			address = choose_offset_scratch(address, destination, X86_RBX);
		const X86Reg offset_scratch = choose_offset_scratch(address, destination,
			X86_RBX);
		materialize_memory(operand, address, offset_scratch);
		mov_reg_mem(destination, address, 0, width);
	}

	void load_operand_from_address(const CyOperand& operand,
		X86Reg destination, unsigned width, X86Reg captured_address)
	{
		if (operand.kind == CyOperand::MEMORY)
		{
			mov_reg_mem(destination, captured_address, 0, width);
			return;
		}
		load_operand(operand, destination, width, X86_R10);
	}

	void store_operand(const CyOperand& operand, X86Reg source,
		unsigned width, X86Reg address_scratch)
	{
		if (operand.kind == CyOperand::REGISTER)
		{
			mov_reg_reg(x86_register(operand.reg), source, width);
			return;
		}
		if (operand.kind != CyOperand::MEMORY)
			throw std::runtime_error("invalid CY86 destination");
		X86Reg address = address_scratch;
		if (address == source)
			address = choose_offset_scratch(address, source, X86_RBX);
		const X86Reg offset_scratch = choose_offset_scratch(address, source,
			X86_RBX);
		materialize_memory(operand, address, offset_scratch);
		mov_mem_reg(address, 0, source, width);
	}

	void emit_startup(std::uint64_t entry)
	{
		binary_reg(X86_R12, X86_R12, 64, 0x31);
		binary_reg(X86_R13, X86_R13, 64, 0x31);
		binary_reg(X86_R14, X86_R14, 64, 0x31);
		binary_reg(X86_R15, X86_R15, 64, 0x31);
		mov_reg_reg(X86_RBP, X86_RSP, 64);
		mov_reg_imm(X86_RAX, 64, entry);
		jmp_reg(X86_RAX, false);
	}

	void emit_empty_exit()
	{
		binary_reg(X86_R12, X86_R12, 64, 0x31);
		binary_reg(X86_R13, X86_R13, 64, 0x31);
		binary_reg(X86_R14, X86_R14, 64, 0x31);
		binary_reg(X86_R15, X86_R15, 64, 0x31);
		mov_reg_reg(X86_RBP, X86_RSP, 64);
		mov_reg_imm(X86_RAX, 64, 60);
		mov_reg_imm(X86_RDI, 64, 0);
		byte(0x0F);
		byte(0x05);
		byte(0x0F);
		byte(0x0B);
	}

	void extend_reg(X86Reg destination, X86Reg source, unsigned source_width,
		bool sign_extend)
	{
		if (source_width == 32 && !sign_extend)
			return;
		if (source_width == 32 && sign_extend)
		{
			emit_rex(true, static_cast<int>(destination),
				static_cast<int>(source));
				byte(0x63);
			modrm(3, static_cast<unsigned>(destination),
				static_cast<unsigned>(source));
			return;
		}
		emit_rex(true, static_cast<int>(destination),
			static_cast<int>(source));
		byte(0x0F);
		byte(static_cast<std::uint8_t>(sign_extend ?
			(source_width == 8 ? 0xBE : 0xBF) :
			(source_width == 8 ? 0xB6 : 0xB7)));
		modrm(3, static_cast<unsigned>(destination),
			static_cast<unsigned>(source));
	}

	void x87_memory(std::uint8_t opcode, unsigned extension, X86Reg base,
		std::int32_t displacement)
	{
		emit_rex(false, static_cast<int>(extension), static_cast<int>(base));
		byte(opcode);
		modrm_memory_disp32(base, extension, displacement);
	}

	void x87_register(std::uint8_t first, std::uint8_t second)
	{
		byte(first);
		byte(second);
	}

private:
	std::vector<std::uint8_t>* output_;
	bool dry_run_;
	const NameTable& names_;
	const LiteralStore& literals_;
	const std::unordered_map<NameId, std::uint64_t>& labels_;
	std::size_t position_;
};

unsigned integer_condition(const OpcodeInfo& opcode)
{
	if (opcode.compare == CMP_EQ) return 0x4;
	if (opcode.compare == CMP_NE) return 0x5;
	if (opcode.compare == CMP_LT) return opcode.signed_operation ? 0xC : 0x2;
	if (opcode.compare == CMP_GT) return opcode.signed_operation ? 0xF : 0x7;
	if (opcode.compare == CMP_LE) return opcode.signed_operation ? 0xE : 0x6;
	return opcode.signed_operation ? 0xD : 0x3;
}

unsigned floating_condition(const OpcodeInfo& opcode)
{
	if (opcode.compare == CMP_EQ) return 0x4;
	if (opcode.compare == CMP_NE) return 0x5;
	if (opcode.compare == CMP_LT) return 0x2;
	if (opcode.compare == CMP_GT) return 0x7;
	if (opcode.compare == CMP_LE) return 0x6;
	return 0x3;
}

std::size_t literal_alignment(const StoredLiteral& literal)
{
	if (literal.element_count != 0)
	{
		switch (literal.type)
		{
		case FundamentalType::Char:
		case FundamentalType::SignedChar:
		case FundamentalType::UnsignedChar:
		case FundamentalType::Bool:
			return 1;
		case FundamentalType::Char16T:
		case FundamentalType::ShortInt:
		case FundamentalType::UnsignedShortInt:
			return 2;
		case FundamentalType::Float:
		case FundamentalType::Int:
		case FundamentalType::UnsignedInt:
		case FundamentalType::WcharT:
		case FundamentalType::Char32T:
			return 4;
		case FundamentalType::Double:
		case FundamentalType::LongInt:
		case FundamentalType::UnsignedLongInt:
		case FundamentalType::LongLongInt:
		case FundamentalType::UnsignedLongLongInt:
			return 8;
		case FundamentalType::LongDouble:
			return 16;
		default:
			return 1;
		}
	}
	switch (literal.type)
	{
	case FundamentalType::LongDouble:
		return 16;
	case FundamentalType::Double:
	case FundamentalType::LongInt:
	case FundamentalType::UnsignedLongInt:
	case FundamentalType::LongLongInt:
	case FundamentalType::UnsignedLongLongInt:
		return 8;
	case FundamentalType::Float:
	case FundamentalType::Int:
	case FundamentalType::UnsignedInt:
	case FundamentalType::WcharT:
	case FundamentalType::Char32T:
		return 4;
	case FundamentalType::ShortInt:
	case FundamentalType::UnsignedShortInt:
	case FundamentalType::Char16T:
		return 2;
	default:
		return 1;
	}
}

class CyNativeEmitter
{
public:
	CyNativeEmitter(const NameTable& names, const LiteralStore& literals,
		const std::unordered_map<NameId, std::uint64_t>& labels)
		: names_(names), literals_(literals), labels_(labels)
	{}

	void emit_statement(CodeEmitter& emitter, const CyStatement& statement)
	{
		if (statement.kind == CyStatement::LITERAL_DATA)
		{
			std::vector<std::uint8_t> bytes;
			literal_value_bytes(literals_, statement.literal,
				statement.negative_literal, &bytes);
			emitter.bytes(bytes);
			return;
		}

		const OpcodeInfo& opcode = statement.opcode;
		switch (opcode.family)
		{
		case OP_DATA: emit_data(emitter, statement); break;
		case OP_MOVE: emit_move(emitter, statement); break;
		case OP_JUMP: emit_jump(emitter, statement.operands[0], false); break;
		case OP_JUMPIF: emit_jumpif(emitter, statement); break;
		case OP_CALL: emit_jump(emitter, statement.operands[0], true); break;
		case OP_RET: emitter.byte(0xC3); break;
		case OP_NOT: emit_unary(emitter, statement, 0x2); break;
		case OP_AND: emit_binary(emitter, statement, 0x21, false); break;
		case OP_OR: emit_binary(emitter, statement, 0x09, false); break;
		case OP_XOR: emit_binary(emitter, statement, 0x31, false); break;
		case OP_LSHIFT: emit_shift(emitter, statement, 4); break;
		case OP_SRSHIFT: emit_shift(emitter, statement, 7); break;
		case OP_URSHIFT: emit_shift(emitter, statement, 5); break;
		case OP_IADD: emit_binary(emitter, statement, 0x01, false); break;
		case OP_ISUB: emit_binary(emitter, statement, 0x29, false); break;
		case OP_SMUL: case OP_UMUL: emit_binary(emitter, statement, 0xAF, true); break;
		case OP_SDIV: case OP_UDIV: emit_division(emitter, statement, true); break;
		case OP_SMOD: case OP_UMOD: emit_division(emitter, statement, false); break;
		case OP_ICMP: emit_integer_compare(emitter, statement); break;
		case OP_SYSCALL: emit_syscall(emitter, statement); break;
		case OP_INT_TO_F80: case OP_FLOAT_TO_F80: emit_to_f80(emitter, statement); break;
		case OP_F80_TO_INT: case OP_F80_TO_FLOAT: emit_from_f80(emitter, statement); break;
		case OP_FADD: case OP_FSUB: case OP_FMUL: case OP_FDIV:
				emit_float_binary(emitter, statement); break;
		case OP_FCMP: emit_float_compare(emitter, statement); break;
		default: throw std::runtime_error("invalid CY86 opcode family");
		}
	}

private:
	const NameTable& names_;
	const LiteralStore& literals_;
	const std::unordered_map<NameId, std::uint64_t>& labels_;

	bool same_register(const CyOperand& left, const CyOperand& right) const
	{
		return left.kind == CyOperand::REGISTER &&
			right.kind == CyOperand::REGISTER &&
			left.reg.group == right.reg.group;
	}

	bool capture_address(CodeEmitter& emitter, const CyOperand& operand,
		X86Reg address) const
	{
		if (operand.kind != CyOperand::MEMORY)
			return false;
		emitter.materialize_memory(operand, address, X86_RAX);
		return true;
	}

	void load_captured_or_operand(CodeEmitter& emitter,
		const CyOperand& operand, X86Reg destination, unsigned width,
		bool captured, X86Reg address) const
	{
		if (captured)
			emitter.load_operand_from_address(operand, destination, width, address);
		else
			emitter.load_operand(operand, destination, width, address);
	}

	void emit_data(CodeEmitter& emitter, const CyStatement& statement)
	{
		emitter.align_absolute(statement.opcode.width / 8);
		const CyOperand& operand = statement.operands[0];
		if (operand.kind != CyOperand::IMMEDIATE)
			throw std::runtime_error("CY86 data requires immediate");
		std::vector<std::uint8_t> bytes;
		if (operand.expr.resolved == EXPR_LITERAL)
			converted_literal_bytes(literals_, operand.expr.literal,
				statement.opcode.width, operand.expr.negative, &bytes);
		else
			converted_address_bytes(emitter.expression_value(operand.expr),
				statement.opcode.width, &bytes);
		emitter.bytes(bytes);
	}

	void emit_move(CodeEmitter& emitter, const CyStatement& statement)
	{
		const unsigned width = statement.opcode.width;
		const CyOperand& destination = statement.operands[0];
		const CyOperand& source = statement.operands[1];
		if (width == 80)
		{
			if (destination.kind != CyOperand::MEMORY)
				throw std::runtime_error("CY86 move80 destination requires memory");
			if (source.kind == CyOperand::IMMEDIATE)
			{
				std::vector<std::uint8_t> bytes;
				if (source.expr.resolved == EXPR_LITERAL)
					converted_literal_bytes(literals_, source.expr.literal, 80,
						source.expr.negative, &bytes);
				else
					converted_address_bytes(emitter.expression_value(source.expr),
						80, &bytes);
				emitter.materialize_memory(destination, X86_R10, X86_R11);
				std::uint64_t low = 0;
				for (unsigned i = 0; i < 8; ++i)
					low |= static_cast<std::uint64_t>(bytes[i]) << (i * 8);
				std::uint16_t high = static_cast<std::uint16_t>(bytes[8]) |
					static_cast<std::uint16_t>(bytes[9]) << 8;
				emitter.mov_reg_imm(X86_RAX, 64, low);
				emitter.mov_mem_reg(X86_R10, 0, X86_RAX, 64);
				emitter.mov_reg_imm(X86_RCX, 16, high);
				emitter.mov_mem_reg(X86_R10, 8, X86_RCX, 16);
				return;
			}
			if (source.kind != CyOperand::MEMORY)
				throw std::runtime_error("CY86 move80 source requires memory or literal");
			const bool captured_source = capture_address(emitter, source, X86_R10);
			load_captured_or_operand(emitter, source, X86_RAX, 64,
				captured_source, X86_R10);
			emitter.mov_reg_mem(X86_RCX, X86_R10, 8, 16);
			emitter.materialize_memory(destination, X86_R11, X86_R10);
			emitter.mov_mem_reg(X86_R11, 0, X86_RAX, 64);
			emitter.mov_mem_reg(X86_R11, 8, X86_RCX, 16);
			return;
		}
		if (destination.kind == CyOperand::REGISTER)
		{
			const bool captured_source = capture_address(emitter, source, X86_R10);
			load_captured_or_operand(emitter, source,
				x86_register(destination.reg), width, captured_source, X86_R10);
			return;
		}
		const bool captured_source = capture_address(emitter, source, X86_R10);
		load_captured_or_operand(emitter, source, X86_RAX, width,
			captured_source, X86_R10);
		emitter.store_operand(destination, X86_RAX, width, X86_R11);
	}

	void emit_unary(CodeEmitter& emitter, const CyStatement& statement,
		unsigned extension)
	{
		const unsigned width = statement.opcode.width;
		const CyOperand& destination = statement.operands[0];
		const X86Reg result = destination.kind == CyOperand::REGISTER ?
			x86_register(destination.reg) : X86_RAX;
		const bool captured_source = capture_address(emitter,
			statement.operands[1], X86_R10);
		load_captured_or_operand(emitter, statement.operands[1], result, width,
			captured_source, X86_R10);
		emitter.unary_reg(result, width, extension, 0xF6, 0xF7);
		if (destination.kind != CyOperand::REGISTER)
			emitter.store_operand(destination, result, width, X86_R11);
	}

	void emit_binary(CodeEmitter& emitter, const CyStatement& statement,
		std::uint8_t opcode, bool two_byte)
	{
		const unsigned width = statement.opcode.width;
		const CyOperand& destination = statement.operands[0];
		const X86Reg result = (two_byte && width == 8) ? X86_RAX :
			(destination.kind == CyOperand::REGISTER ?
				x86_register(destination.reg) : X86_RAX);
		const bool captured_left = capture_address(emitter,
			statement.operands[1], X86_R10);
		const bool captured_right = capture_address(emitter,
			statement.operands[2], X86_R11);
		if (same_register(destination, statement.operands[2]))
		{
			load_captured_or_operand(emitter, statement.operands[2], X86_RCX,
				width, captured_right, X86_R11);
			load_captured_or_operand(emitter, statement.operands[1], result,
				width, captured_left, X86_R10);
		}
		else
		{
			load_captured_or_operand(emitter, statement.operands[1], result,
				width, captured_left, X86_R10);
			load_captured_or_operand(emitter, statement.operands[2], X86_RCX,
				width, captured_right, X86_R11);
		}
		if (two_byte)
		{
			if (width == 8)
			{
				emitter.binary_reg_0f(X86_RAX, X86_RAX, 32, 0xB6);
				emitter.binary_reg_0f(X86_RCX, X86_RCX, 32, 0xB6);
				emitter.binary_reg_0f(X86_RAX, X86_RCX, 32, opcode);
			}
			else
				emitter.binary_reg_0f(result, X86_RCX, width, opcode);
		}
		else
			emitter.binary_reg(result, X86_RCX, width, opcode);
		if (destination.kind != CyOperand::REGISTER ||
			(two_byte && width == 8))
			emitter.store_operand(destination, result, width, X86_R10);
	}

	void emit_shift(CodeEmitter& emitter, const CyStatement& statement,
		unsigned extension)
	{
		const unsigned width = statement.opcode.width;
		const CyOperand& destination = statement.operands[0];
		const X86Reg result = destination.kind == CyOperand::REGISTER ?
			x86_register(destination.reg) : X86_RAX;
		const bool captured_left = capture_address(emitter,
			statement.operands[1], X86_R10);
		const bool captured_right = capture_address(emitter,
			statement.operands[2], X86_R11);
		if (same_register(destination, statement.operands[2]))
		{
			load_captured_or_operand(emitter, statement.operands[2], X86_RCX,
				8, captured_right, X86_R11);
			load_captured_or_operand(emitter, statement.operands[1], result,
				width, captured_left, X86_R10);
		}
		else
		{
			load_captured_or_operand(emitter, statement.operands[1], result,
				width, captured_left, X86_R10);
			load_captured_or_operand(emitter, statement.operands[2], X86_RCX,
				8, captured_right, X86_R11);
		}
		emitter.shift_reg(result, width, extension);
		if (destination.kind != CyOperand::REGISTER)
			emitter.store_operand(destination, result, width, X86_R10);
	}

	void emit_division(CodeEmitter& emitter, const CyStatement& statement,
		bool quotient)
	{
		const unsigned width = statement.opcode.width;
		const bool signed_operation = statement.opcode.signed_operation;
		const CyOperand& destination = statement.operands[0];
		const bool captured_left = capture_address(emitter,
			statement.operands[1], X86_R10);
		const bool captured_right = capture_address(emitter,
			statement.operands[2], X86_R11);
		load_captured_or_operand(emitter, statement.operands[1], X86_RAX, width,
			captured_left, X86_R10);
		load_captured_or_operand(emitter, statement.operands[2], X86_RCX, width,
			captured_right, X86_R11);
		if (signed_operation)
		{
			if (width == 8)
			{
				emitter.byte(0x66);
				emitter.byte(0x98);
			}
			else if (width == 16)
			{
				emitter.byte(0x66);
				emitter.byte(0x99);
			}
			else if (width == 32) emitter.byte(0x99);
			else
			{
				emitter.byte(0x48);
				emitter.byte(0x99);
			}
		}
		else
		{
			if (width == 8)
				emitter.binary_reg_0f(X86_RAX, X86_RAX, 32, 0xB6);
			else if (width == 16)
				emitter.binary_reg_0f(X86_RAX, X86_RAX, 32, 0xB7);
			emitter.byte(0x31);
			emitter.modrm(3, static_cast<unsigned>(X86_RDX),
				static_cast<unsigned>(X86_RDX));
		}
		emitter.div_reg(X86_RCX, width, signed_operation ? 7 : 6);
		if (!quotient && width == 8)
			emitter.movzx_eax_ah();
		emitter.store_operand(destination, quotient ? X86_RAX :
			(width == 8 ? X86_RAX : X86_RDX), width, X86_R10);
	}

	void emit_integer_compare(CodeEmitter& emitter,
		const CyStatement& statement)
	{
		const unsigned width = statement.opcode.width;
		const bool captured_left = capture_address(emitter,
			statement.operands[1], X86_R10);
		const bool captured_right = capture_address(emitter,
			statement.operands[2], X86_R11);
		load_captured_or_operand(emitter, statement.operands[1], X86_RAX, width,
			captured_left, X86_R10);
		load_captured_or_operand(emitter, statement.operands[2], X86_RCX, width,
			captured_right, X86_R11);
		emitter.cmp_reg(X86_RAX, X86_RCX, width);
		const unsigned condition = integer_condition(statement.opcode);
		if (statement.operands[0].kind == CyOperand::REGISTER)
			emitter.setcc(x86_register(statement.operands[0].reg), condition);
		else
		{
			emitter.setcc(X86_RDX, condition);
			emitter.store_operand(statement.operands[0], X86_RDX, 8, X86_R10);
		}
	}

	void emit_syscall(CodeEmitter& emitter, const CyStatement& statement)
	{
		emitter.load_operand(statement.operands[1], X86_RAX, 64, X86_R10);
		const X86Reg arguments[] =
			{X86_RDI, X86_RSI, X86_RDX, X86_R10, X86_R8, X86_R9};
		for (unsigned i = 0; i < statement.opcode.syscall_arguments; ++i)
			emitter.load_operand(statement.operands[2 + i], arguments[i], 64,
				arguments[i] == X86_R10 ? X86_R11 : X86_R11);
		emitter.byte(0x0F);
		emitter.byte(0x05);
		emitter.store_operand(statement.operands[0], X86_RAX, 64, X86_R10);
	}

	void emit_fld(CodeEmitter& emitter, const CyOperand& operand,
		unsigned width, X86Reg address_scratch)
	{
		X86Reg base = address_scratch;
		std::int32_t displacement = 0;
		if (operand.kind == CyOperand::MEMORY)
		{
			emitter.materialize_memory(operand, address_scratch,
				choose_offset_scratch(address_scratch, X86_RBX, X86_RSI));
		}
		else if (operand.kind == CyOperand::IMMEDIATE &&
			operand.expr.resolved == EXPR_LITERAL)
		{
			std::vector<std::uint8_t> bytes;
			converted_literal_bytes(literals_, operand.expr.literal, width,
				operand.expr.negative, &bytes);
			base = X86_RSP;
			displacement = -64;
			emitter.write_memory_bytes(base, displacement, bytes);
		}
		else
			throw std::runtime_error("invalid floating CY86 source");
		if (width == 32) emitter.x87_memory(0xD9, 0, base, displacement);
		else if (width == 64) emitter.x87_memory(0xDD, 0, base, displacement);
		else if (width == 80) emitter.x87_memory(0xDB, 5, base, displacement);
		else throw std::runtime_error("invalid floating CY86 width");
	}

	void emit_fstp(CodeEmitter& emitter, const CyOperand& operand,
		unsigned width, X86Reg address_scratch)
	{
		if (operand.kind != CyOperand::MEMORY)
			throw std::runtime_error("floating CY86 destination must be memory");
		emitter.materialize_memory(operand, address_scratch,
			choose_offset_scratch(address_scratch, X86_RBX, X86_RSI));
		if (width == 32) emitter.x87_memory(0xD9, 3, address_scratch, 0);
		else if (width == 64) emitter.x87_memory(0xDD, 3, address_scratch, 0);
		else if (width == 80) emitter.x87_memory(0xDB, 7, address_scratch, 0);
		else throw std::runtime_error("invalid floating CY86 width");
	}

	void emit_fild_qword(CodeEmitter& emitter, std::int32_t displacement)
	{
		emitter.x87_memory(0xDF, 5, X86_RSP, displacement);
	}

	void emit_fistp_qword(CodeEmitter& emitter, std::int32_t displacement)
	{
		emitter.x87_memory(0xDF, 7, X86_RSP, displacement);
	}

	void emit_integer_to_f80(CodeEmitter& emitter,
		const CyStatement& statement)
	{
		const OpcodeInfo& opcode = statement.opcode;
		const unsigned width = opcode.auxiliary_width;
		const bool signed_operation = opcode.signed_operation;
		if (!signed_operation && width == 64)
		{
			emitter.load_operand(statement.operands[1], X86_RAX, 64, X86_R10);
			emitter.test_reg(X86_RAX, X86_RAX, 64);
			const std::size_t low_displacement = emitter.jump_placeholder(0x9);

			// High unsigned values are represented as (value - 2^63) + 2^63.
			emitter.mov_reg_imm(X86_RCX, 64, 0x8000000000000000ULL);
			emitter.binary_reg(X86_RAX, X86_RCX, 64, 0x31);
			emitter.mov_mem_reg(X86_RSP, -16, X86_RAX, 64);
			emitter.mov_mem_reg(X86_RSP, -24, X86_RCX, 64);
			emit_fild_qword(emitter, -16);
			emit_fild_qword(emitter, -24);
			emitter.x87_register(0xD9, 0xE0); // fchs
			emitter.x87_register(0xDE, 0xC1); // faddp st(1), st(0)
			emit_fstp(emitter, statement.operands[0], 80, X86_R10);
			const std::size_t done_displacement = emitter.jmp_placeholder();

			const std::size_t low_position = emitter.position();
			emitter.load_operand(statement.operands[1], X86_RAX, 64, X86_R10);
			emitter.mov_mem_reg(X86_RSP, -16, X86_RAX, 64);
			emit_fild_qword(emitter, -16);
			emit_fstp(emitter, statement.operands[0], 80, X86_R10);
			const std::size_t done_position = emitter.position();
			emitter.patch_relative(low_displacement, low_position);
			emitter.patch_relative(done_displacement, done_position);
			return;
		}

		emitter.load_operand(statement.operands[1], X86_RAX, width, X86_R10);
		if (width < 64)
			emitter.extend_reg(X86_RAX, X86_RAX, width, signed_operation);
		emitter.mov_mem_reg(X86_RSP, -16, X86_RAX, 64);
		emit_fild_qword(emitter, -16);
		emit_fstp(emitter, statement.operands[0], 80, X86_R10);
	}

	void emit_to_f80(CodeEmitter& emitter, const CyStatement& statement)
	{
		if (statement.opcode.family == OP_INT_TO_F80)
		{
			emit_integer_to_f80(emitter, statement);
			return;
		}
		emit_fld(emitter, statement.operands[1],
			statement.opcode.auxiliary_width, X86_R10);
		emit_fstp(emitter, statement.operands[0], 80, X86_R11);
	}

	void emit_from_f80(CodeEmitter& emitter, const CyStatement& statement)
	{
		const OpcodeInfo& opcode = statement.opcode;
		if (opcode.family == OP_F80_TO_FLOAT)
		{
			emit_fld(emitter, statement.operands[1], 80, X86_R10);
			emit_fstp(emitter, statement.operands[0], opcode.width, X86_R11);
			return;
		}
		if (!opcode.signed_operation && opcode.auxiliary_width == 64)
		{
			// Compare the input with +2^63.  The memory value is the signed
			// bit pattern whose x87 interpretation is -2^63; fchs makes it
			// the positive threshold.
			emitter.mov_reg_imm(X86_RAX, 64, 0x8000000000000000ULL);
			emitter.mov_mem_reg(X86_RSP, -24, X86_RAX, 64);
			emit_fld(emitter, statement.operands[1], 80, X86_R10);
			emit_fild_qword(emitter, -24);
			emitter.x87_register(0xD9, 0xE0); // fchs
			emitter.x87_register(0xDF, 0xF1); // fcomip st(1), st(0)
			const std::size_t low_displacement = emitter.jump_placeholder(0x7);

			// High path: value - 2^63, then restore the high bit.
			emit_fild_qword(emitter, -24);
			emitter.x87_register(0xD9, 0xE0);
			emitter.x87_register(0xDE, 0xE9); // fsubp st(1), st(0)
			emit_fistp_qword(emitter, -16);
			emitter.mov_reg_mem(X86_RAX, X86_RSP, -16, 64);
			emitter.mov_reg_imm(X86_RCX, 64, 0x8000000000000000ULL);
			emitter.binary_reg(X86_RAX, X86_RCX, 64, 0x09);
			emitter.store_operand(statement.operands[0], X86_RAX,
				64, X86_R10);
			const std::size_t done_displacement = emitter.jmp_placeholder();

			const std::size_t low_position = emitter.position();
			emit_fistp_qword(emitter, -16);
			emitter.mov_reg_mem(X86_RAX, X86_RSP, -16, 64);
			emitter.store_operand(statement.operands[0], X86_RAX,
				64, X86_R10);
			const std::size_t done_position = emitter.position();
			emitter.patch_relative(low_displacement, low_position);
			emitter.patch_relative(done_displacement, done_position);
			return;
		}

		emit_fld(emitter, statement.operands[1], 80, X86_R10);
		emit_fistp_qword(emitter, -16);
		emitter.mov_reg_mem(X86_RAX, X86_RSP, -16, 64);
		emitter.store_operand(statement.operands[0], X86_RAX,
			opcode.auxiliary_width, X86_R10);
	}

	void emit_float_binary(CodeEmitter& emitter,
		const CyStatement& statement)
	{
		const unsigned width = statement.opcode.width;
		emit_fld(emitter, statement.operands[1], width, X86_R10);
		emit_fld(emitter, statement.operands[2], width, X86_R11);
		if (statement.opcode.family == OP_FADD)
			emitter.x87_register(0xDE, 0xC1); // faddp
		else if (statement.opcode.family == OP_FSUB)
			emitter.x87_register(0xDE, 0xE9); // fsubp
		else if (statement.opcode.family == OP_FMUL)
			emitter.x87_register(0xDE, 0xC9); // fmulp
		else
			emitter.x87_register(0xDE, 0xF9); // fdivp
		emit_fstp(emitter, statement.operands[0], width, X86_R10);
	}

	void emit_float_compare(CodeEmitter& emitter,
		const CyStatement& statement)
	{
		const unsigned width = statement.opcode.width;
		// FCOMIP compares ST(0) against ST(i).  Load op2 first so that
		// ST(0) is op1 and the flags have the CY86 operand order.
		emit_fld(emitter, statement.operands[2], width, X86_R10);
		emit_fld(emitter, statement.operands[1], width, X86_R11);
		emitter.x87_register(0xDF, 0xF1); // fcomip st(1), st(0)
		emitter.x87_register(0xDD, 0xD8); // discard the remaining value
		const CompareKind compare = statement.opcode.compare;
		if (compare == CMP_EQ || compare == CMP_LT || compare == CMP_LE)
		{
			// x87 reports unordered as ZF=CF=PF=1.  C++ ordered
			// predicates must additionally require PF=0.
			emitter.setcc(X86_RAX, floating_condition(statement.opcode));
			emitter.setcc(X86_RCX, 0xB); // setnp
			emitter.binary_reg(X86_RAX, X86_RCX, 8, 0x21); // and al, cl
		}
		else if (compare == CMP_NE)
		{
			// C++ != is true for unordered values.
			emitter.setcc(X86_RAX, 0x5); // setne
			emitter.setcc(X86_RCX, 0xA); // setp
			emitter.binary_reg(X86_RAX, X86_RCX, 8, 0x09); // or al, cl
		}
		else
			emitter.setcc(X86_RAX, floating_condition(statement.opcode));
		emitter.store_operand(statement.operands[0], X86_RAX, 8, X86_R10);
	}

	void emit_dynamic_target(CodeEmitter& emitter, const CyOperand& target,
		bool call)
	{
		if (target.kind == CyOperand::REGISTER)
		{
			emitter.mov_reg_reg(X86_R11, x86_register(target.reg), 64);
			emitter.jmp_reg(X86_R11, call);
			return;
		}
		if (target.kind == CyOperand::IMMEDIATE)
		{
			emitter.mov_reg_imm(X86_R11, 64,
				emitter.expression_value(target.expr));
			emitter.jmp_reg(X86_R11, call);
			return;
		}
		emitter.materialize_memory(target, X86_R10,
			choose_offset_scratch(X86_R10, X86_R11, X86_RBX));
		emitter.mov_reg_mem(X86_R11, X86_R10, 0, 64);
		emitter.jmp_reg(X86_R11, call);
	}

	void emit_jump(CodeEmitter& emitter, const CyOperand& target, bool call)
	{
		if (target.kind == CyOperand::IMMEDIATE &&
			target.expr.resolved == EXPR_LABEL)
		{
			emitter.byte(call ? 0xE8 : 0xE9);
			emitter.rel32(emitter.expression_value(target.expr));
			return;
		}
		emit_dynamic_target(emitter, target, call);
	}

	void emit_jumpif(CodeEmitter& emitter, const CyStatement& statement)
	{
		emitter.load_operand(statement.operands[0], X86_RAX, 8, X86_R10);
		emitter.test_reg(X86_RAX, X86_RAX, 8);
		const CyOperand& target = statement.operands[1];
		if (target.kind == CyOperand::IMMEDIATE &&
			target.expr.resolved == EXPR_LABEL)
		{
			emitter.jump_condition(0x5,
				emitter.expression_value(target.expr));
			return;
		}
		const std::size_t skip_displacement = emitter.jump_placeholder(0x4);
		emit_dynamic_target(emitter, target, false);
		emitter.patch_relative(skip_displacement, emitter.position());
	}
};

struct LayoutResult
{
	std::unordered_map<NameId, std::uint64_t> labels;
	std::vector<std::size_t> statement_positions;
	std::uint64_t first_statement;
	std::size_t body_size;

	LayoutResult()
		: labels(), statement_positions(), first_statement(0), body_size(0)
	{}
};

std::size_t statement_alignment(const CyStatement& statement,
	const LiteralStore& literals)
{
	if (statement.kind == CyStatement::LITERAL_DATA)
		return literal_alignment(literals.get(statement.literal));
	if (statement.opcode.family == OP_DATA)
		if (statement.opcode.width == 80)
			return 16;
	if (statement.opcode.family == OP_DATA)
		return statement.opcode.width / 8;
	return 1;
}

LayoutResult layout_program(const NameTable& names,
	const LiteralStore& literals, const std::vector<CyStatement>& statements)
{
	LayoutResult result;
	CodeEmitter emitter(NULL, true, names, literals, result.labels);
	emitter.set_position(kStartupSize);
	CyNativeEmitter native(names, literals, result.labels);
	for (std::size_t i = 0; i < statements.size(); ++i)
	{
		emitter.align_absolute(statement_alignment(statements[i], literals));
		if (i == 0)
			result.first_statement = emitter.runtime_address(emitter.position());
		result.statement_positions.push_back(emitter.position());
		for (std::size_t j = 0; j < statements[i].labels.size(); ++j)
		{
			const NameId label = statements[i].labels[j];
			result.labels[label] = emitter.runtime_address(emitter.position());
		}
		native.emit_statement(emitter, statements[i]);
	}
	result.body_size = emitter.position() - kStartupSize;
	return result;
}

struct Elf64Header
{
	std::uint8_t ident[16];
	std::uint16_t type;
	std::uint16_t machine;
	std::uint32_t version;
	std::uint64_t entry;
	std::uint64_t phoff;
	std::uint64_t shoff;
	std::uint32_t flags;
	std::uint16_t ehsize;
	std::uint16_t phentsize;
	std::uint16_t phnum;
	std::uint16_t shentsize;
	std::uint16_t shnum;
	std::uint16_t shstrndx;
};

struct Elf64ProgramHeader
{
	std::uint32_t type;
	std::uint32_t flags;
	std::uint64_t offset;
	std::uint64_t vaddr;
	std::uint64_t paddr;
	std::uint64_t filesz;
	std::uint64_t memsz;
	std::uint64_t align;
};

void write_elf(const std::string& outfile, std::uint64_t entry,
	const std::vector<std::uint8_t>& payload)
{
	Elf64Header header;
	std::memset(&header, 0, sizeof(header));
	header.ident[0] = 0x7F;
	header.ident[1] = 'E';
	header.ident[2] = 'L';
	header.ident[3] = 'F';
	header.ident[4] = 2;
	header.ident[5] = 1;
	header.ident[6] = 1;
	header.type = 2;
	header.machine = 0x3E;
	header.version = 1;
	header.entry = entry;
	header.phoff = sizeof(Elf64Header);
	header.shoff = 0;
	header.flags = 0;
	header.ehsize = sizeof(Elf64Header);
	header.phentsize = sizeof(Elf64ProgramHeader);
	header.phnum = 1;
	header.shentsize = 0;
	header.shnum = 0;
	header.shstrndx = 0;

	Elf64ProgramHeader program;
	std::memset(&program, 0, sizeof(program));
	program.type = 1;
	program.flags = 1 | 2 | 4;
	program.offset = 0;
	program.vaddr = kImageBase;
	program.paddr = kImageBase;
	program.filesz = kLoadOffset + payload.size();
	program.memsz = program.filesz;
	program.align = 0x1000;

	std::ofstream output(outfile.c_str(),
		std::ios::out | std::ios::binary | std::ios::trunc);
	if (!output)
		throw std::runtime_error("unable to open CY86 output");
	output.write(reinterpret_cast<const char*>(&header), sizeof(header));
	output.write(reinterpret_cast<const char*>(&program), sizeof(program));
	const std::size_t padding = kLoadOffset - sizeof(header) - sizeof(program);
	std::vector<std::uint8_t> zeroes(padding, 0);
	output.write(reinterpret_cast<const char*>(zeroes.data()),
		static_cast<std::streamsize>(zeroes.size()));
	if (!payload.empty())
		output.write(reinterpret_cast<const char*>(payload.data()),
			static_cast<std::streamsize>(payload.size()));
	if (!output)
		throw std::runtime_error("unable to write CY86 output");
	output.close();

	if (syscall(90, outfile.c_str(), 0755) != 0)
		throw std::runtime_error("unable to make CY86 output executable");
}

int compile_impl(const std::vector<std::string>& args)
{
	std::string outfile;
	std::vector<std::string> source_paths;
	for (std::size_t i = 0; i < args.size(); ++i)
	{
		if (args[i] == "--target")
		{
			if (i + 1 >= args.size())
				throw std::logic_error("missing target after --target");
			++i;
			continue;
		}
		if (args[i] == "-o")
		{
			if (i + 1 >= args.size())
				throw std::logic_error("missing output after -o");
			outfile = args[++i];
			continue;
		}
		source_paths.push_back(args[i]);
	}
	if (outfile.empty() || source_paths.empty())
		throw std::logic_error("invalid CY86 usage");

	NameTable names;
	LiteralStore literals;
	std::vector<CyToken> tokens;
	for (std::size_t i = 0; i < source_paths.size(); ++i)
	{
		std::string source;
		if (!read_source(source_paths[i], &source))
			throw std::runtime_error("unable to open source file: " + source_paths[i]);
		PPPreprocessConfig config;
		PPPreprocessingSession preprocessing(config);
		const PPTokenBuffer& phase3 = preprocessing.preprocess(source_paths[i], source);
		CyTokenCollector collector(names, literals, tokens);
		posttokenize_cpp_tokens(phase3, collector);
		if (collector.invalid())
			throw std::runtime_error("invalid CY86 token");
	}

	std::vector<CyStatement> statements;
	std::unordered_set<NameId> label_names;
	CyParser parser(tokens, literals, names);
	parser.parse(&statements, &label_names);
	CySemantic semantic(names, literals, statements, label_names);
	semantic.validate();

	if (statements.empty())
	{
		std::vector<std::uint8_t> payload;
		std::unordered_map<NameId, std::uint64_t> empty_labels;
		CodeEmitter emitter(&payload, false, names, literals,
			empty_labels);
		emitter.emit_empty_exit();
		write_elf(outfile, kImageBase + kLoadOffset, payload);
		return 0;
	}

	LayoutResult layout = layout_program(names, literals, statements);
	const NameId start_name = names.intern("start");
	std::uint64_t entry = layout.first_statement;
	std::unordered_map<NameId, std::uint64_t>::const_iterator start =
		layout.labels.find(start_name);
	if (start != layout.labels.end())
		entry = start->second;

	std::vector<std::uint8_t> payload;
	payload.reserve(kStartupSize + layout.body_size);
	CodeEmitter emitter(&payload, false, names, literals, layout.labels);
	emitter.emit_startup(entry);
	if (emitter.position() != kStartupSize)
		throw std::runtime_error("CY86 startup size drift");
	CyNativeEmitter native(names, literals, layout.labels);
	for (std::size_t i = 0; i < statements.size(); ++i)
	{
		emitter.align_absolute(statement_alignment(statements[i], literals));
		if (emitter.position() != layout.statement_positions[i])
			throw std::runtime_error("CY86 layout/emission size drift");
		native.emit_statement(emitter, statements[i]);
	}
	if (emitter.position() != kStartupSize + layout.body_size)
		throw std::runtime_error("CY86 final size drift");
	write_elf(outfile, kImageBase + kLoadOffset, payload);
	return 0;
}

} // namespace

int cy86_compile(const std::vector<std::string>& args)
{
	return compile_impl(args);
}
