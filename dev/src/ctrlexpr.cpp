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

class Evaluator
{
public:
	explicit Evaluator(const std::vector<CtrlToken>& tokens)
		: tokens_(tokens), position_(0)
	{}

	Value evaluate()
	{
		if (tokens_.empty())
			throw ParseError();
		const Value result = parse_controlling_expression(true);
		if (!at_end())
			throw ParseError();
		return result;
	}

private:
	const std::vector<CtrlToken>& tokens_;
	std::size_t position_;

	bool at_end() const
	{
		return position_ == tokens_.size();
	}

	const CtrlToken& peek() const
	{
		if (at_end())
			throw ParseError();
		return tokens_[position_];
	}

	bool consume(SimpleTokenType type)
	{
		if (!at_end() && is_simple(tokens_[position_], type))
		{
			++position_;
			return true;
		}
		return false;
	}

	void expect(SimpleTokenType type)
	{
		if (!consume(type))
			throw ParseError();
	}

	Value parse_controlling_expression(bool evaluate_value)
	{
		Value condition = parse_logical_or(evaluate_value);
		if (!consume(SimpleTokenType::OP_QMARK))
			return condition;

		const bool condition_known = evaluate_value && condition.valid;
		const bool condition_true = condition_known && truth(condition);
		Value when_true = parse_controlling_expression(
			evaluate_value && condition_true);
		expect(SimpleTokenType::OP_COLON);
		Value when_false = parse_controlling_expression(
			evaluate_value && condition_known && !condition_true);

		const bool result_unsigned = common_unsigned(when_true, when_false);
		if (!evaluate_value)
			return Value(0, result_unsigned, true);
		if (!condition.valid)
			return invalid_value(result_unsigned);

		const Value& selected = condition_true ? when_true : when_false;
		if (!selected.valid)
			return invalid_value(result_unsigned);
		return Value(selected.bits, result_unsigned, true);
	}

	Value parse_logical_or(bool evaluate_value)
	{
		Value left = parse_logical_and(evaluate_value);
		while (consume(SimpleTokenType::OP_LOR))
		{
			const bool right_active = evaluate_value && left.valid && !truth(left);
			Value right = parse_logical_and(right_active);
			if (!evaluate_value)
				left = Value(0, false, true);
			else if (!left.valid)
				left = invalid_value(false);
			else if (truth(left))
				left = Value(1, false, true);
			else if (!right.valid)
				left = invalid_value(false);
			else
				left = Value(truth(right) ? 1 : 0, false, true);
		}
		return left;
	}

	Value parse_logical_and(bool evaluate_value)
	{
		Value left = parse_inclusive_or(evaluate_value);
		while (consume(SimpleTokenType::OP_LAND))
		{
			const bool right_active = evaluate_value && left.valid && truth(left);
			Value right = parse_inclusive_or(right_active);
			if (!evaluate_value)
				left = Value(0, false, true);
			else if (!left.valid)
				left = invalid_value(false);
			else if (!truth(left))
				left = Value(0, false, true);
			else if (!right.valid)
				left = invalid_value(false);
			else
				left = Value(truth(right) ? 1 : 0, false, true);
		}
		return left;
	}

	Value parse_inclusive_or(bool evaluate_value)
	{
		Value left = parse_exclusive_or(evaluate_value);
		while (consume(SimpleTokenType::OP_BOR))
		{
			Value right = parse_exclusive_or(evaluate_value);
			left = apply_binary(SimpleTokenType::OP_BOR, left, right,
				evaluate_value);
		}
		return left;
	}

	Value parse_exclusive_or(bool evaluate_value)
	{
		Value left = parse_and(evaluate_value);
		while (consume(SimpleTokenType::OP_XOR))
		{
			Value right = parse_and(evaluate_value);
			left = apply_binary(SimpleTokenType::OP_XOR, left, right,
				evaluate_value);
		}
		return left;
	}

	Value parse_and(bool evaluate_value)
	{
		Value left = parse_equality(evaluate_value);
		while (consume(SimpleTokenType::OP_AMP))
		{
			Value right = parse_equality(evaluate_value);
			left = apply_binary(SimpleTokenType::OP_AMP, left, right,
				evaluate_value);
		}
		return left;
	}

	Value parse_equality(bool evaluate_value)
	{
		Value left = parse_relational(evaluate_value);
		while (true)
		{
			SimpleTokenType operation;
			if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_EQ))
				operation = SimpleTokenType::OP_EQ;
			else if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_NE))
				operation = SimpleTokenType::OP_NE;
			else
				break;
			++position_;
			Value right = parse_relational(evaluate_value);
			left = apply_binary(operation, left, right, evaluate_value);
		}
		return left;
	}

	Value parse_relational(bool evaluate_value)
	{
		Value left = parse_shift(evaluate_value);
		while (true)
		{
			SimpleTokenType operation;
			if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_LT))
				operation = SimpleTokenType::OP_LT;
			else if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_GT))
				operation = SimpleTokenType::OP_GT;
			else if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_LE))
				operation = SimpleTokenType::OP_LE;
			else if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_GE))
				operation = SimpleTokenType::OP_GE;
			else
				break;
			++position_;
			Value right = parse_shift(evaluate_value);
			left = apply_binary(operation, left, right, evaluate_value);
		}
		return left;
	}

	Value parse_shift(bool evaluate_value)
	{
		Value left = parse_additive(evaluate_value);
		while (true)
		{
			SimpleTokenType operation;
			if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_LSHIFT))
				operation = SimpleTokenType::OP_LSHIFT;
			else if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_RSHIFT))
				operation = SimpleTokenType::OP_RSHIFT;
			else
				break;
			++position_;
			Value right = parse_additive(evaluate_value);
			left = apply_binary(operation, left, right, evaluate_value);
		}
		return left;
	}

	Value parse_additive(bool evaluate_value)
	{
		Value left = parse_multiplicative(evaluate_value);
		while (true)
		{
			SimpleTokenType operation;
			if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_PLUS))
				operation = SimpleTokenType::OP_PLUS;
			else if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_MINUS))
				operation = SimpleTokenType::OP_MINUS;
			else
				break;
			++position_;
			Value right = parse_multiplicative(evaluate_value);
			left = apply_binary(operation, left, right, evaluate_value);
		}
		return left;
	}

	Value parse_multiplicative(bool evaluate_value)
	{
		Value left = parse_unary(evaluate_value);
		while (true)
		{
			SimpleTokenType operation;
			if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_STAR))
				operation = SimpleTokenType::OP_STAR;
			else if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_DIV))
				operation = SimpleTokenType::OP_DIV;
			else if (!at_end() && is_simple(tokens_[position_], SimpleTokenType::OP_MOD))
				operation = SimpleTokenType::OP_MOD;
			else
				break;
			++position_;
			Value right = parse_unary(evaluate_value);
			left = apply_binary(operation, left, right, evaluate_value);
		}
		return left;
	}

	Value parse_unary(bool evaluate_value)
	{
		std::vector<SimpleTokenType> operations;
		while (!at_end())
		{
			const SimpleTokenType type = tokens_[position_].simple;
			if (tokens_[position_].kind != CtrlTokenKind::Simple ||
				(type != SimpleTokenType::OP_PLUS &&
				 type != SimpleTokenType::OP_MINUS &&
				 type != SimpleTokenType::OP_LNOT &&
				 type != SimpleTokenType::OP_COMPL))
				break;
			// Alternative word operators remain identifier-origin tokens.  When
			// one appears without a following unary-expression, the primary
			// identifier-or-keyword production is the valid interpretation.
			if (tokens_[position_].identifier_like &&
				!can_start_unary(position_ + 1))
				break;
			operations.push_back(type);
			++position_;
		}

		Value result = parse_primary(evaluate_value);
		for (std::size_t i = operations.size(); i != 0; --i)
		{
			const SimpleTokenType operation = operations[i - 1];
			if (operation == SimpleTokenType::OP_LNOT)
			{
				// Logical-not is always a signed result, including in an
				// inactive branch whose value is deliberately not computed.
				const bool was_valid = result.valid;
				result.is_unsigned = false;
				if (!evaluate_value || !was_valid)
					continue;
				result.bits = truth(result) ? 0 : 1;
				continue;
			}
			if (!evaluate_value || !result.valid)
				continue;
			if (operation == SimpleTokenType::OP_PLUS)
				continue;
			if (operation == SimpleTokenType::OP_MINUS)
				result.bits = static_cast<std::uint64_t>(0) - result.bits;
			else
				result.bits = ~result.bits;
		}
		return result;
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
		if (token.kind != CtrlTokenKind::Simple)
			return false;
		return token.simple == SimpleTokenType::OP_PLUS ||
			token.simple == SimpleTokenType::OP_MINUS ||
			token.simple == SimpleTokenType::OP_LNOT ||
			token.simple == SimpleTokenType::OP_COMPL;
	}

	Value parse_primary(bool evaluate_value)
	{
		if (at_end())
			throw ParseError();

		if (consume(SimpleTokenType::OP_LPAREN))
		{
			Value result = parse_controlling_expression(evaluate_value);
			expect(SimpleTokenType::OP_RPAREN);
			return result;
		}

		if (is_defined_operator())
			return parse_defined(evaluate_value);

		if (tokens_[position_].kind == CtrlTokenKind::Literal)
		{
			const CtrlToken& token = tokens_[position_];
			++position_;
			return Value(token.bits, token.is_unsigned, true);
		}

		if (is_identifier_or_keyword(tokens_[position_]))
			return parse_identifier_or_keyword(evaluate_value);

		throw ParseError();
	}

	bool is_defined_operator() const
	{
		return tokens_[position_].kind == CtrlTokenKind::Identifier &&
			tokens_[position_].defined_spelling;
	}

	Value parse_identifier_or_keyword(bool evaluate_value)
	{
		if (!is_identifier_or_keyword(peek()))
			throw ParseError();

		const CtrlToken& token = tokens_[position_];
		const bool is_true = token.kind == CtrlTokenKind::Simple &&
			token.simple == SimpleTokenType::KW_TRUE;
		const bool is_false = token.kind == CtrlTokenKind::Simple &&
			token.simple == SimpleTokenType::KW_FALSE;
		++position_;
		if (!evaluate_value)
			return Value(0, false, true);
		if (is_true)
			return Value(1, false, true);
		if (is_false)
			return Value(0, false, true);
		return Value(0, false, true);
	}

	Value parse_defined(bool evaluate_value)
	{
		++position_; // the identifier spelling "defined"
		bool operand_parity_odd = false;
		if (consume(SimpleTokenType::OP_LPAREN))
		{
			operand_parity_odd = parse_defined_operand();
			expect(SimpleTokenType::OP_RPAREN);
		}
		else
			operand_parity_odd = parse_defined_operand();

		if (!evaluate_value)
			return Value(0, false, true);
		return Value(operand_parity_odd ? 1 : 0, false, true);
	}

	bool parse_defined_operand()
	{
		if (at_end() || !is_identifier_or_keyword(tokens_[position_]))
			throw ParseError();
		const bool operand_parity_odd =
			tokens_[position_].identifier_parity_odd;
		++position_;
		return operand_parity_odd;
	}

	Value apply_binary(SimpleTokenType operation, const Value& left,
		const Value& right, bool evaluate_value)
	{
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
