#include "ctrlexpr.h"

#include <cstdint>
#include <exception>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "posttoken.h"

namespace
{

enum class CtrlTokenKind : std::uint8_t
{
	Invalid,
	Simple,
	Identifier,
	Literal
};

struct CtrlToken
{
	CtrlTokenKind kind;
	SimpleTokenType simple;
	std::uint64_t bits;
	bool is_unsigned;
	bool identifier_like;
	bool defined_spelling;
	bool identifier_parity_odd;

	CtrlToken()
		: kind(CtrlTokenKind::Invalid), simple(SimpleTokenType::OP_LBRACE),
		  bits(0), is_unsigned(false), identifier_like(false),
		  defined_spelling(false), identifier_parity_odd(false)
	{}
};

bool is_identifier_or_keyword(const CtrlToken& token)
{
	return token.kind == CtrlTokenKind::Identifier ||
		(token.kind == CtrlTokenKind::Simple && token.identifier_like);
}

bool is_unsigned_type(FundamentalType type)
{
	switch (type)
	{
	case FundamentalType::UnsignedChar:
	case FundamentalType::UnsignedShortInt:
	case FundamentalType::UnsignedInt:
	case FundamentalType::UnsignedLongInt:
	case FundamentalType::UnsignedLongLongInt:
	case FundamentalType::Char16T:
	case FundamentalType::Char32T:
		return true;
	default:
		return false;
	}
}

bool is_integral_type(FundamentalType type)
{
	switch (type)
	{
	case FundamentalType::SignedChar:
	case FundamentalType::ShortInt:
	case FundamentalType::Int:
	case FundamentalType::LongInt:
	case FundamentalType::LongLongInt:
	case FundamentalType::UnsignedChar:
	case FundamentalType::UnsignedShortInt:
	case FundamentalType::UnsignedInt:
	case FundamentalType::UnsignedLongInt:
	case FundamentalType::UnsignedLongLongInt:
	case FundamentalType::WcharT:
	case FundamentalType::Char:
	case FundamentalType::Char16T:
	case FundamentalType::Char32T:
	case FundamentalType::Bool:
		return true;
	default:
		return false;
	}
}

std::size_t integral_literal_width(FundamentalType type)
{
	switch (type)
	{
	case FundamentalType::SignedChar:
	case FundamentalType::UnsignedChar:
	case FundamentalType::Char:
	case FundamentalType::Bool:
		return 1;
	case FundamentalType::ShortInt:
	case FundamentalType::UnsignedShortInt:
	case FundamentalType::Char16T:
		return 2;
	case FundamentalType::Int:
	case FundamentalType::UnsignedInt:
	case FundamentalType::WcharT:
	case FundamentalType::Char32T:
		return 4;
	case FundamentalType::LongInt:
	case FundamentalType::LongLongInt:
	case FundamentalType::UnsignedLongInt:
	case FundamentalType::UnsignedLongLongInt:
		return 8;
	default:
		return 0;
	}
}

bool decode_integral_literal(const LiteralData& literal,
	std::uint64_t* bits, bool* is_unsigned)
{
	if (!is_integral_type(literal.type) || literal.element_count != 0)
		return false;
	const std::size_t width = integral_literal_width(literal.type);
	if (width == 0 || literal.bytes.size() != width)
		return false;

	std::uint64_t decoded = 0;
	const std::size_t count = width;
	for (std::size_t i = 0; i < count; ++i)
		decoded |= static_cast<std::uint64_t>(literal.bytes[i]) << (i * 8);

	if (!is_unsigned_type(literal.type) && count < sizeof(decoded) &&
		(literal.bytes[count - 1] & 0x80) != 0)
		decoded |= std::numeric_limits<std::uint64_t>::max() << (count * 8);
	*bits = decoded;
	*is_unsigned = is_unsigned_type(literal.type);
	return true;
}

bool first_code_unit_is_odd(const std::string& source)
{
	return !source.empty() &&
		(static_cast<unsigned char>(source[0]) & 1) != 0;
}

std::int64_t signed_bits(std::uint64_t bits)
{
	if (bits <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
		return static_cast<std::int64_t>(bits);

	// This computes the negative two's-complement value without converting an
	// out-of-range unsigned value to a signed type.
	return static_cast<std::int64_t>(-1) -
		static_cast<std::int64_t>(~bits);
}

std::uint64_t unsigned_bits(std::int64_t value)
{
	if (value >= 0)
		return static_cast<std::uint64_t>(value);

	// value + 1 is representable even for INT64_MIN; the final addition is
	// performed in uint64_t and therefore also handles that endpoint.
	const std::uint64_t magnitude =
		static_cast<std::uint64_t>(-(value + 1)) + 1;
	return static_cast<std::uint64_t>(0) - magnitude;
}

struct Value
{
	std::uint64_t bits;
	bool is_unsigned;
	bool valid;

	Value(std::uint64_t bits = 0, bool is_unsigned = false,
		bool valid = true)
		: bits(bits), is_unsigned(is_unsigned), valid(valid)
	{}
};

Value invalid_value(bool is_unsigned)
{
	return Value(0, is_unsigned, false);
}

bool truth(const Value& value)
{
	return value.bits != 0;
}

bool common_unsigned(const Value& left, const Value& right)
{
	return left.is_unsigned || right.is_unsigned;
}

bool is_simple(const CtrlToken& token, SimpleTokenType type)
{
	return token.kind == CtrlTokenKind::Simple && token.simple == type;
}

class ParseError : public std::runtime_error
{
public:
	ParseError() : std::runtime_error("invalid controlling expression") {}
};

enum class ExprNodeKind : std::uint8_t
{
	Literal,
	Identifier,
	Defined,
	Unary,
	Binary,
	Conditional
};

enum ExprNodeFlags
{
	ExprNodeUnsigned = 1,
	ExprNodeTrue = 2,
	ExprNodeFalse = 4,
	ExprNodeParityOdd = 8
};

struct ExprNode
{
	ExprNodeKind kind;
	SimpleTokenType operation;
	std::size_t left;
	std::size_t right;
	std::size_t third;
	std::uint64_t bits;
	std::uint8_t flags;

	ExprNode()
		: kind(ExprNodeKind::Literal), operation(SimpleTokenType::OP_LBRACE),
		  left(0), right(0), third(0), bits(0), flags(0)
	{}
};

enum class ParseOperatorKind : std::uint8_t
{
	LeftParen,
	Question,
	Unary,
	Binary,
	Conditional
};

struct ParseOperator
{
	ParseOperatorKind kind;
	SimpleTokenType operation;
	std::size_t value_base;
	std::size_t condition;
	std::size_t when_true;

	ParseOperator(ParseOperatorKind kind = ParseOperatorKind::LeftParen)
		: kind(kind), operation(SimpleTokenType::OP_LBRACE), value_base(0),
		  condition(0), when_true(0)
	{}
};

bool is_prefix_unary(SimpleTokenType type)
{
	return type == SimpleTokenType::OP_PLUS ||
		type == SimpleTokenType::OP_MINUS ||
		type == SimpleTokenType::OP_LNOT ||
		type == SimpleTokenType::OP_COMPL;
}

bool is_binary_operator(SimpleTokenType type)
{
	switch (type)
	{
	case SimpleTokenType::OP_STAR:
	case SimpleTokenType::OP_DIV:
	case SimpleTokenType::OP_MOD:
	case SimpleTokenType::OP_PLUS:
	case SimpleTokenType::OP_MINUS:
	case SimpleTokenType::OP_LSHIFT:
	case SimpleTokenType::OP_RSHIFT:
	case SimpleTokenType::OP_LT:
	case SimpleTokenType::OP_GT:
	case SimpleTokenType::OP_LE:
	case SimpleTokenType::OP_GE:
	case SimpleTokenType::OP_EQ:
	case SimpleTokenType::OP_NE:
	case SimpleTokenType::OP_AMP:
	case SimpleTokenType::OP_XOR:
	case SimpleTokenType::OP_BOR:
	case SimpleTokenType::OP_LAND:
	case SimpleTokenType::OP_LOR:
		return true;
	default:
		return false;
	}
}

int binary_precedence(SimpleTokenType type)
{
	switch (type)
	{
	case SimpleTokenType::OP_LOR: return 2;
	case SimpleTokenType::OP_LAND: return 3;
	case SimpleTokenType::OP_BOR: return 4;
	case SimpleTokenType::OP_XOR: return 5;
	case SimpleTokenType::OP_AMP: return 6;
	case SimpleTokenType::OP_EQ:
	case SimpleTokenType::OP_NE: return 7;
	case SimpleTokenType::OP_LT:
	case SimpleTokenType::OP_GT:
	case SimpleTokenType::OP_LE:
	case SimpleTokenType::OP_GE: return 8;
	case SimpleTokenType::OP_LSHIFT:
	case SimpleTokenType::OP_RSHIFT: return 9;
	case SimpleTokenType::OP_PLUS:
	case SimpleTokenType::OP_MINUS: return 10;
	case SimpleTokenType::OP_STAR:
	case SimpleTokenType::OP_DIV:
	case SimpleTokenType::OP_MOD: return 11;
	default:
		return -1;
	}
}

int parse_operator_precedence(const ParseOperator& operation)
{
	if (operation.kind == ParseOperatorKind::Unary)
		return 12;
	if (operation.kind == ParseOperatorKind::Binary)
		return binary_precedence(operation.operation);
	if (operation.kind == ParseOperatorKind::Conditional)
		return 1;
	return -1;
}

std::size_t estimated_node_count(const std::vector<CtrlToken>& tokens)
{
	std::size_t count = 0;
	for (std::size_t i = 0; i < tokens.size(); ++i)
	{
		const CtrlToken& token = tokens[i];
		if (token.kind == CtrlTokenKind::Literal ||
			is_identifier_or_keyword(token))
			++count;
		else if (token.kind == CtrlTokenKind::Simple &&
			(is_prefix_unary(token.simple) ||
				is_binary_operator(token.simple) ||
				token.simple == SimpleTokenType::OP_QMARK))
			++count;
	}
	return count;
}

class ExpressionParser
{
public:
	explicit ExpressionParser(const std::vector<CtrlToken>& tokens)
		: tokens_(tokens), position_(0), nodes_(), operators_(), values_()
	{
		// This counts node-producing typed tokens without retaining another
		// representation.  Parentheses and colons therefore do not cause a
		// large line to reserve arena space that it cannot use.
		nodes_.reserve(estimated_node_count(tokens_));
	}

	std::size_t parse()
	{
		if (tokens_.empty())
			throw ParseError();

		bool expect_operand = true;
		while (!at_end())
		{
			if (expect_operand)
				expect_operand = consume_operand();
			else
				expect_operand = consume_after_operand();
		}
		if (expect_operand)
			throw ParseError();

		while (!operators_.empty())
		{
			if (operators_.back().kind == ParseOperatorKind::LeftParen ||
				operators_.back().kind == ParseOperatorKind::Question)
				throw ParseError();
			reduce_top();
		}
		if (values_.size() != 1)
			throw ParseError();
		return values_.back();
	}

	const std::vector<ExprNode>& nodes() const
	{
		return nodes_;
	}

private:
	const std::vector<CtrlToken>& tokens_;
	std::size_t position_;
	std::vector<ExprNode> nodes_;
	std::vector<ParseOperator> operators_;
	std::vector<std::size_t> values_;

	bool at_end() const
	{
		return position_ == tokens_.size();
	}

	const CtrlToken& current() const
	{
		if (at_end())
			throw ParseError();
		return tokens_[position_];
	}

	std::size_t add_node(const ExprNode& node)
	{
		const std::size_t result = nodes_.size();
		nodes_.push_back(node);
		return result;
	}

	std::size_t add_literal(const CtrlToken& token)
	{
		ExprNode node;
		node.kind = ExprNodeKind::Literal;
		node.bits = token.bits;
		if (token.is_unsigned)
			node.flags |= ExprNodeUnsigned;
		return add_node(node);
	}

	std::size_t add_identifier(const CtrlToken& token)
	{
		ExprNode node;
		node.kind = ExprNodeKind::Identifier;
		if (token.kind == CtrlTokenKind::Simple &&
			token.simple == SimpleTokenType::KW_TRUE)
			node.flags |= ExprNodeTrue;
		else if (token.kind == CtrlTokenKind::Simple &&
			token.simple == SimpleTokenType::KW_FALSE)
			node.flags |= ExprNodeFalse;
		return add_node(node);
	}

	std::size_t add_defined(bool parity_odd)
	{
		ExprNode node;
		node.kind = ExprNodeKind::Defined;
		if (parity_odd)
			node.flags |= ExprNodeParityOdd;
		return add_node(node);
	}

	std::size_t add_unary(SimpleTokenType operation, std::size_t operand)
	{
		ExprNode node;
		node.kind = ExprNodeKind::Unary;
		node.operation = operation;
		node.left = operand;
		return add_node(node);
	}

	std::size_t add_binary(SimpleTokenType operation, std::size_t left,
		std::size_t right)
	{
		ExprNode node;
		node.kind = ExprNodeKind::Binary;
		node.operation = operation;
		node.left = left;
		node.right = right;
		return add_node(node);
	}

	std::size_t add_conditional(std::size_t condition,
		std::size_t when_true, std::size_t when_false)
	{
		ExprNode node;
		node.kind = ExprNodeKind::Conditional;
		node.left = condition;
		node.right = when_true;
		node.third = when_false;
		return add_node(node);
	}

	bool can_start_unary(std::size_t position) const
	{
		if (position >= tokens_.size())
			return false;
		const CtrlToken& token = tokens_[position];
		if (token.kind == CtrlTokenKind::Literal ||
			is_identifier_or_keyword(token) ||
			is_simple(token, SimpleTokenType::OP_LPAREN))
			return true;
		return token.kind == CtrlTokenKind::Simple &&
			is_prefix_unary(token.simple);
	}

	bool is_defined_operator() const
	{
		return current().kind == CtrlTokenKind::Identifier &&
			current().defined_spelling;
	}

	std::size_t parse_defined()
	{
		++position_; // the identifier spelling "defined"
		bool operand_parity_odd = false;
		if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_LPAREN))
		{
			++position_;
			if (at_end() || !is_identifier_or_keyword(tokens_[position_]))
				throw ParseError();
			operand_parity_odd = tokens_[position_].identifier_parity_odd;
			++position_;
			if (at_end() || !is_simple(tokens_[position_], SimpleTokenType::OP_RPAREN))
				throw ParseError();
			++position_;
		}
		else
		{
			if (at_end() || !is_identifier_or_keyword(tokens_[position_]))
				throw ParseError();
			operand_parity_odd = tokens_[position_].identifier_parity_odd;
			++position_;
		}
		return add_defined(operand_parity_odd);
	}

	bool consume_operand()
	{
		const CtrlToken& token = current();
		if (token.kind == CtrlTokenKind::Literal)
		{
			values_.push_back(add_literal(token));
			++position_;
			return false;
		}

		if (is_simple(token, SimpleTokenType::OP_LPAREN))
		{
			ParseOperator operation(ParseOperatorKind::LeftParen);
			operation.value_base = values_.size();
			operators_.push_back(operation);
			++position_;
			return true;
		}

		if (is_defined_operator())
		{
			values_.push_back(parse_defined());
			return false;
		}

		if (token.kind == CtrlTokenKind::Simple &&
			is_prefix_unary(token.simple) &&
			(!token.identifier_like || can_start_unary(position_ + 1)))
		{
			ParseOperator operation(ParseOperatorKind::Unary);
			operation.operation = token.simple;
			operation.value_base = values_.size();
			operators_.push_back(operation);
			++position_;
			return true;
		}

		if (is_identifier_or_keyword(token))
		{
			values_.push_back(add_identifier(token));
			++position_;
			return false;
		}
		throw ParseError();
	}

	bool consume_after_operand()
	{
		const CtrlToken& token = current();
		if (is_simple(token, SimpleTokenType::OP_RPAREN))
		{
			close_parenthesis();
			return false;
		}
		if (is_simple(token, SimpleTokenType::OP_QMARK))
		{
			begin_conditional();
			return true;
		}
		if (is_simple(token, SimpleTokenType::OP_COLON))
		{
			finish_conditional();
			return true;
		}
		if (token.kind != CtrlTokenKind::Simple ||
			!is_binary_operator(token.simple))
			throw ParseError();

		const int precedence = binary_precedence(token.simple);
		while (!operators_.empty())
		{
			const ParseOperator& top = operators_.back();
			if (top.kind == ParseOperatorKind::LeftParen ||
				top.kind == ParseOperatorKind::Question ||
				parse_operator_precedence(top) < precedence)
				break;
			reduce_top();
		}
		ParseOperator operation(ParseOperatorKind::Binary);
		operation.operation = token.simple;
		operation.value_base = values_.empty() ? 0 : values_.size() - 1;
		operators_.push_back(operation);
		++position_;
		return true;
	}

	void begin_conditional()
	{
		while (!operators_.empty())
		{
			const ParseOperator& top = operators_.back();
			if (top.kind == ParseOperatorKind::LeftParen ||
				top.kind == ParseOperatorKind::Question ||
				parse_operator_precedence(top) <= 1)
				break;
			reduce_top();
		}
		if (values_.empty())
			throw ParseError();

		ParseOperator operation(ParseOperatorKind::Question);
		operation.condition = values_.back();
		values_.pop_back();
		operation.value_base = values_.size();
		operators_.push_back(operation);
		++position_;
	}

	void finish_conditional()
	{
		while (!operators_.empty() &&
			operators_.back().kind != ParseOperatorKind::Question)
		{
			if (operators_.back().kind == ParseOperatorKind::LeftParen)
				throw ParseError();
			reduce_top();
		}
		if (operators_.empty())
			throw ParseError();

		ParseOperator operation = operators_.back();
		if (values_.size() != operation.value_base + 1)
			throw ParseError();
		operation.when_true = values_.back();
		values_.pop_back();
		operation.kind = ParseOperatorKind::Conditional;
		operators_.back() = operation;
		++position_;
	}

	void close_parenthesis()
	{
		while (!operators_.empty() &&
			operators_.back().kind != ParseOperatorKind::LeftParen)
		{
			if (operators_.back().kind == ParseOperatorKind::Question)
				throw ParseError();
			reduce_top();
		}
		if (operators_.empty())
			throw ParseError();

		const ParseOperator operation = operators_.back();
		if (values_.size() != operation.value_base + 1)
			throw ParseError();
		operators_.pop_back();
		++position_;
	}

	void reduce_top()
	{
		if (operators_.empty())
			throw ParseError();
		const ParseOperator operation = operators_.back();
		operators_.pop_back();
		if (operation.kind == ParseOperatorKind::Question ||
			operation.kind == ParseOperatorKind::LeftParen)
			throw ParseError();

		if (operation.kind == ParseOperatorKind::Unary)
		{
			if (values_.size() != operation.value_base + 1)
				throw ParseError();
			const std::size_t operand = values_.back();
			values_.pop_back();
			values_.push_back(add_unary(operation.operation, operand));
			return;
		}

		if (operation.kind == ParseOperatorKind::Binary)
		{
			if (values_.size() != operation.value_base + 2)
				throw ParseError();
			const std::size_t right = values_.back();
			values_.pop_back();
			const std::size_t left = values_.back();
			values_.pop_back();
			values_.push_back(add_binary(operation.operation, left, right));
			return;
		}

		if (operation.kind == ParseOperatorKind::Conditional)
		{
			if (values_.size() != operation.value_base + 1)
				throw ParseError();
			const std::size_t when_false = values_.back();
			values_.pop_back();
			values_.push_back(add_conditional(operation.condition,
				operation.when_true, when_false));
			return;
		}
		throw ParseError();
	}
};

struct EvaluationFrame
{
	std::size_t node;
	std::uint8_t stage;
	bool evaluate_value;
	bool condition_known;
	bool condition_true;
	Value first;
	Value second;

	EvaluationFrame(std::size_t node, bool evaluate_value)
		: node(node), stage(0), evaluate_value(evaluate_value),
		  condition_known(false), condition_true(false), first(), second()
	{}
};

class Evaluator
{
public:
	explicit Evaluator(const std::vector<CtrlToken>& tokens)
		: tokens_(tokens)
	{}

	Value evaluate()
	{
		ExpressionParser parser(tokens_);
		const std::size_t root = parser.parse();
		return evaluate_tree(parser.nodes(), root);
	}

private:
	const std::vector<CtrlToken>& tokens_;

	static Value pop_result(std::vector<Value>* results)
	{
		if (results->empty())
			throw ParseError();
		const Value result = results->back();
		results->pop_back();
		return result;
	}

	static Value apply_unary(SimpleTokenType operation, const Value& operand,
		bool evaluate_value)
	{
		if (operation == SimpleTokenType::OP_LNOT)
		{
			Value result = operand;
			result.is_unsigned = false;
			if (!evaluate_value || !operand.valid)
			{
				result.bits = 0;
				return result;
			}
			result.bits = truth(operand) ? 0 : 1;
			return result;
		}
		if (!evaluate_value || !operand.valid ||
			operation == SimpleTokenType::OP_PLUS)
			return operand;
		if (operation == SimpleTokenType::OP_MINUS)
			return Value(static_cast<std::uint64_t>(0) - operand.bits,
				operand.is_unsigned, true);
		if (operation == SimpleTokenType::OP_COMPL)
			return Value(~operand.bits, operand.is_unsigned, true);
		throw ParseError();
	}

	static Value apply_logical(SimpleTokenType operation, const Value& left,
		const Value& right, bool evaluate_value)
	{
		if (!evaluate_value)
			return Value(0, false, true);
		if (!left.valid)
			return invalid_value(false);
		if (operation == SimpleTokenType::OP_LAND)
		{
			if (!truth(left))
				return Value(0, false, true);
		}
		else
		{
			if (truth(left))
				return Value(1, false, true);
		}
		if (!right.valid)
			return invalid_value(false);
		return Value(truth(right) ? 1 : 0, false, true);
	}

	static Value apply_binary(SimpleTokenType operation, const Value& left,
		const Value& right, bool evaluate_value)
	{
		if (operation == SimpleTokenType::OP_LAND ||
			operation == SimpleTokenType::OP_LOR)
			return apply_logical(operation, left, right, evaluate_value);

		bool result_unsigned = common_unsigned(left, right);
		if (operation == SimpleTokenType::OP_LSHIFT ||
			operation == SimpleTokenType::OP_RSHIFT)
			result_unsigned = left.is_unsigned;
		else if (operation == SimpleTokenType::OP_LT ||
			operation == SimpleTokenType::OP_GT ||
			operation == SimpleTokenType::OP_LE ||
			operation == SimpleTokenType::OP_GE ||
			operation == SimpleTokenType::OP_EQ ||
			operation == SimpleTokenType::OP_NE)
			result_unsigned = false;

		if (!evaluate_value)
			return Value(0, result_unsigned, true);
		if (!left.valid || !right.valid)
			return invalid_value(result_unsigned);

		if (operation == SimpleTokenType::OP_LSHIFT ||
			operation == SimpleTokenType::OP_RSHIFT)
		{
			std::uint64_t count = right.bits;
			if (!right.is_unsigned)
			{
				const std::int64_t signed_count = signed_bits(right.bits);
				if (signed_count < 0 || signed_count >= 64)
					return invalid_value(result_unsigned);
				count = static_cast<std::uint64_t>(signed_count);
			}
			else if (count >= 64)
				return invalid_value(result_unsigned);

			if (operation == SimpleTokenType::OP_LSHIFT)
				return Value(left.bits << count, result_unsigned, true);

			if (result_unsigned || (left.bits & (static_cast<std::uint64_t>(1) << 63)) == 0)
				return Value(left.bits >> count, result_unsigned, true);
			if (count == 0)
				return Value(left.bits, result_unsigned, true);
			const std::uint64_t fill = std::numeric_limits<std::uint64_t>::max()
				<< (64 - count);
			return Value((left.bits >> count) | fill, result_unsigned, true);
		}

		if (operation == SimpleTokenType::OP_LT ||
			operation == SimpleTokenType::OP_GT ||
			operation == SimpleTokenType::OP_LE ||
			operation == SimpleTokenType::OP_GE ||
			operation == SimpleTokenType::OP_EQ ||
			operation == SimpleTokenType::OP_NE)
		{
			const bool use_unsigned = common_unsigned(left, right);
			const std::uint64_t lhs_unsigned = left.bits;
			const std::uint64_t rhs_unsigned = right.bits;
			const std::int64_t lhs_signed = signed_bits(left.bits);
			const std::int64_t rhs_signed = signed_bits(right.bits);
			bool result = false;
			if (operation == SimpleTokenType::OP_EQ)
				result = use_unsigned ? lhs_unsigned == rhs_unsigned :
					lhs_signed == rhs_signed;
			else if (operation == SimpleTokenType::OP_NE)
				result = use_unsigned ? lhs_unsigned != rhs_unsigned :
					lhs_signed != rhs_signed;
			else if (operation == SimpleTokenType::OP_LT)
				result = use_unsigned ? lhs_unsigned < rhs_unsigned :
					lhs_signed < rhs_signed;
			else if (operation == SimpleTokenType::OP_GT)
				result = use_unsigned ? lhs_unsigned > rhs_unsigned :
					lhs_signed > rhs_signed;
			else if (operation == SimpleTokenType::OP_LE)
				result = use_unsigned ? lhs_unsigned <= rhs_unsigned :
					lhs_signed <= rhs_signed;
			else
				result = use_unsigned ? lhs_unsigned >= rhs_unsigned :
					lhs_signed >= rhs_signed;
			return Value(result ? 1 : 0, false, true);
		}

		const std::uint64_t lhs = left.bits;
		const std::uint64_t rhs = right.bits;
		switch (operation)
		{
		case SimpleTokenType::OP_PLUS:
			return Value(lhs + rhs, result_unsigned, true);
		case SimpleTokenType::OP_MINUS:
			return Value(lhs - rhs, result_unsigned, true);
		case SimpleTokenType::OP_STAR:
			return Value(lhs * rhs, result_unsigned, true);
		case SimpleTokenType::OP_AMP:
			return Value(lhs & rhs, result_unsigned, true);
		case SimpleTokenType::OP_XOR:
			return Value(lhs ^ rhs, result_unsigned, true);
		case SimpleTokenType::OP_BOR:
			return Value(lhs | rhs, result_unsigned, true);
		case SimpleTokenType::OP_DIV:
		case SimpleTokenType::OP_MOD:
			if (rhs == 0)
				return invalid_value(result_unsigned);
			if (!result_unsigned)
			{
				const std::int64_t lhs_signed = signed_bits(lhs);
				const std::int64_t rhs_signed = signed_bits(rhs);
				if (lhs_signed == std::numeric_limits<std::int64_t>::min() &&
					rhs_signed == -1)
					return invalid_value(result_unsigned);
				if (operation == SimpleTokenType::OP_DIV)
					return Value(unsigned_bits(lhs_signed / rhs_signed), false, true);
				return Value(unsigned_bits(lhs_signed % rhs_signed), false, true);
			}
			if (operation == SimpleTokenType::OP_DIV)
				return Value(lhs / rhs, true, true);
			return Value(lhs % rhs, true, true);
		default:
			throw ParseError();
		}
	}

	static Value evaluate_tree(const std::vector<ExprNode>& nodes,
		std::size_t root)
	{
		std::vector<EvaluationFrame> frames;
		std::vector<Value> results;
		// Flat binary trees reach roughly half their node count in evaluation
		// depth.  This small warm reserve avoids repeated growth there while
		// leaving unary- or branch-heavy trees to grow only as needed.
		frames.reserve(nodes.size() / 2 + 2);
		frames.push_back(EvaluationFrame(root, true));

		while (!frames.empty())
		{
			EvaluationFrame& frame = frames.back();
			const ExprNode& node = nodes[frame.node];
			if (node.kind == ExprNodeKind::Literal ||
				node.kind == ExprNodeKind::Identifier ||
				node.kind == ExprNodeKind::Defined)
			{
				Value result;
				if (node.kind == ExprNodeKind::Literal)
					result = Value(node.bits,
						(node.flags & ExprNodeUnsigned) != 0, true);
				else if (node.kind == ExprNodeKind::Defined)
					result = Value(frame.evaluate_value &&
						(node.flags & ExprNodeParityOdd) ? 1 : 0, false, true);
				else if (!frame.evaluate_value)
					result = Value(0, false, true);
				else if (node.flags & ExprNodeTrue)
					result = Value(1, false, true);
				else
					result = Value(0, false, true);
				frames.pop_back();
				results.push_back(result);
				continue;
			}

			if (node.kind == ExprNodeKind::Unary)
			{
				if (frame.stage == 0)
				{
					frame.stage = 1;
					frames.push_back(EvaluationFrame(node.left,
						frame.evaluate_value));
					continue;
				}
				const Value operand = pop_result(&results);
				const Value result = apply_unary(node.operation, operand,
					frame.evaluate_value);
				frames.pop_back();
				results.push_back(result);
				continue;
			}

			if (node.kind == ExprNodeKind::Binary)
			{
				if (frame.stage == 0)
				{
					frame.stage = 1;
					frames.push_back(EvaluationFrame(node.left,
						frame.evaluate_value));
					continue;
				}
				if (frame.stage == 1)
				{
					frame.first = pop_result(&results);
					frame.stage = 2;
					bool right_active = frame.evaluate_value;
					if (node.operation == SimpleTokenType::OP_LAND)
						right_active = right_active && frame.first.valid &&
							truth(frame.first);
					else if (node.operation == SimpleTokenType::OP_LOR)
						right_active = right_active && frame.first.valid &&
							!truth(frame.first);
					frames.push_back(EvaluationFrame(node.right, right_active));
					continue;
				}
				const Value right = pop_result(&results);
				const Value result = apply_binary(node.operation, frame.first,
					right, frame.evaluate_value);
				frames.pop_back();
				results.push_back(result);
				continue;
			}

			if (node.kind == ExprNodeKind::Conditional)
			{
				if (frame.stage == 0)
				{
					frame.stage = 1;
					frames.push_back(EvaluationFrame(node.left,
						frame.evaluate_value));
					continue;
				}
				if (frame.stage == 1)
				{
					frame.first = pop_result(&results);
					frame.condition_known = frame.evaluate_value &&
						frame.first.valid;
					frame.condition_true = frame.condition_known &&
						truth(frame.first);
					frame.stage = 2;
					frames.push_back(EvaluationFrame(node.right,
						frame.evaluate_value && frame.condition_true));
					continue;
				}
				if (frame.stage == 2)
				{
					frame.second = pop_result(&results);
					frame.stage = 3;
					frames.push_back(EvaluationFrame(node.third,
						frame.evaluate_value && frame.condition_known &&
							!frame.condition_true));
					continue;
				}

				const Value when_false = pop_result(&results);
				const bool result_unsigned = common_unsigned(frame.second,
					when_false);
				Value result;
				if (!frame.evaluate_value)
					result = Value(0, result_unsigned, true);
				else if (!frame.first.valid)
					result = invalid_value(result_unsigned);
				else
				{
					const Value& selected = frame.condition_true ?
						frame.second : when_false;
					result = selected.valid ?
						Value(selected.bits, result_unsigned, true) :
						invalid_value(result_unsigned);
				}
				frames.pop_back();
				results.push_back(result);
				continue;
			}
			throw ParseError();
		}

		if (results.size() != 1)
			throw ParseError();
		return results.back();
	}
};

class LineOutput : public IPostTokenOutput
{
public:
	explicit LineOutput(std::ostream& output)
		: output_(output), tokens_(), invalid_(false)
	{}

	void emit_invalid(const std::string& source)
	{
		(void)source;
		invalid_ = true;
	}

	void emit_simple(const std::string& source, SimpleTokenType type)
	{
		(void)source;
		CtrlToken token;
		token.kind = CtrlTokenKind::Simple;
		token.simple = type;
		tokens_.push_back(token);
	}

	void emit_simple_identifier(const std::string& source,
		SimpleTokenType type)
	{
		CtrlToken token;
		token.kind = CtrlTokenKind::Simple;
		token.simple = type;
		token.identifier_like = true;
		token.defined_spelling = source == "defined";
		token.identifier_parity_odd = first_code_unit_is_odd(source);
		tokens_.push_back(token);
	}

	void emit_identifier(const std::string& source)
	{
		CtrlToken token;
		token.kind = CtrlTokenKind::Identifier;
		token.identifier_like = true;
		token.defined_spelling = source == "defined";
		token.identifier_parity_odd = first_code_unit_is_odd(source);
		tokens_.push_back(token);
	}

	void emit_literal(const std::string& source, const LiteralData& value)
	{
		(void)source;
		std::uint64_t bits = 0;
		bool is_unsigned = false;
		if (!decode_integral_literal(value, &bits, &is_unsigned))
		{
			invalid_ = true;
			return;
		}
		CtrlToken token;
		token.kind = CtrlTokenKind::Literal;
		token.bits = bits;
		token.is_unsigned = is_unsigned;
		tokens_.push_back(token);
	}

	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		(void)value;
		invalid_ = true;
	}

	void emit_new_line()
	{
		finish_line();
	}

	void emit_eof()
	{
		if (!tokens_.empty() || invalid_)
			finish_line();
		output_ << "eof\n";
	}

private:
	std::ostream& output_;
	std::vector<CtrlToken> tokens_;
	bool invalid_;

	void finish_line()
	{
		if (tokens_.empty() && !invalid_)
		{
			tokens_.clear();
			return;
		}

		if (invalid_)
			output_ << "error\n";
		else
		{
			try
			{
				const Value result = Evaluator(tokens_).evaluate();
				if (!result.valid)
					output_ << "error\n";
				else if (result.is_unsigned)
					output_ << result.bits << 'u' << '\n';
				else
					output_ << signed_bits(result.bits) << '\n';
			}
			catch (const ParseError&)
			{
				output_ << "error\n";
			}
		}
		tokens_.clear();
		invalid_ = false;
	}
};

} // namespace

int run_ctrlexpr(std::istream& input, std::ostream& output,
	std::ostream& errors)
{
	try
	{
		std::ostringstream source;
		source << input.rdbuf();
		LineOutput line_output(output);
		posttokenize_cpp_source_by_line(source.str(), line_output);
		return EXIT_SUCCESS;
	}
	catch (const std::exception& e)
	{
		errors << "ERROR: " << e.what() << '\n';
		return EXIT_FAILURE;
	}
}
