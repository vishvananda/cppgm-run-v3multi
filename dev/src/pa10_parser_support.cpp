#include "pa10_parser_support.h"

#include <utility>

namespace PA10ParserSupport
{
namespace
{

PA10ContextualIdentifierKind classify_contextual_identifier(
	const std::string& source)
{
	if (source == "override")
		return PA10ContextualIdentifierKind::Override;
	if (source == "final")
		return PA10ContextualIdentifierKind::Final;
	if (source == "__attribute__" || source == "__attribute")
		return PA10ContextualIdentifierKind::AttributeIntroducer;
	return PA10ContextualIdentifierKind::None;
}

class PA10PostTokenCollector : public IPostTokenOutput
{
public:
	PA10PostTokenCollector() : tokens(), invalid(false) {}
	void emit_invalid(const std::string& source)
	{
		(void)source;
		invalid = true;
	}
	void emit_simple(const std::string& source, SimpleTokenType type)
	{
		if (type == SimpleTokenType::OP_RSHIFT)
		{
			tokens.push_back(PA10Token(PA10TokenKind::RShiftPiece1,
				SimpleTokenType::OP_GT, 0, ">"));
			tokens.push_back(PA10Token(PA10TokenKind::RShiftPiece2,
				SimpleTokenType::OP_GT, 0, ">"));
		}
		else
			tokens.push_back(PA10Token(PA10TokenKind::Fixed, type, 0, source));
	}
	void emit_identifier(const std::string& source)
	{
		emit_identifier_with_spelling(0, source);
	}
	void emit_simple_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source, SimpleTokenType type)
	{
		tokens.push_back(PA10Token(PA10TokenKind::Fixed, type, spelling, source));
	}
	void emit_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source)
	{
		PA10Token token(PA10TokenKind::Identifier,
			SimpleTokenType::OP_SEMICOLON, spelling, source);
		token.contextual_identifier = classify_contextual_identifier(source);
		tokens.push_back(std::move(token));
	}
	void emit_literal(const std::string& source, const LiteralData& value)
	{
		PA10Token token(PA10TokenKind::Literal, SimpleTokenType::OP_SEMICOLON,
			0, source);
		token.literal = value;
		tokens.push_back(token);
	}
	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		PA10Token token(PA10TokenKind::UserDefinedLiteral,
			SimpleTokenType::OP_SEMICOLON, 0, value.source);
		token.user_defined = value;
		tokens.push_back(token);
	}
	void emit_eof()
	{
		if (tokens.empty() || tokens.back().kind != PA10TokenKind::End)
			tokens.push_back(PA10Token(PA10TokenKind::End));
	}
	std::vector<PA10Token> tokens;
	bool invalid;
};

bool token_identifier_at(const std::vector<PA10Token>& tokens,
	std::size_t absolute, std::size_t offset = 0)
{
	return absolute < tokens.size() && offset < tokens.size() - absolute &&
		tokens[absolute + offset].kind == PA10TokenKind::Identifier;
}

bool token_fixed_at(const std::vector<PA10Token>& tokens,
	std::size_t absolute, std::size_t offset, SimpleTokenType type)
{
	return absolute < tokens.size() && offset < tokens.size() - absolute &&
		tokens[absolute + offset].kind == PA10TokenKind::Fixed &&
		tokens[absolute + offset].fixed == type;
}

bool is_cv_impl(SimpleTokenType type)
{
	return type == SimpleTokenType::KW_CONST ||
		type == SimpleTokenType::KW_VOLATILE;
}

bool is_type_keyword_impl(SimpleTokenType type)
{
	switch (type)
	{
	case SimpleTokenType::KW_AUTO:
	case SimpleTokenType::KW_BOOL:
	case SimpleTokenType::KW_CHAR:
	case SimpleTokenType::KW_CHAR16_T:
	case SimpleTokenType::KW_CHAR32_T:
	case SimpleTokenType::KW_DOUBLE:
	case SimpleTokenType::KW_FLOAT:
	case SimpleTokenType::KW_INT:
	case SimpleTokenType::KW_LONG:
	case SimpleTokenType::KW_SHORT:
	case SimpleTokenType::KW_SIGNED:
	case SimpleTokenType::KW_UNSIGNED:
	case SimpleTokenType::KW_VOID:
	case SimpleTokenType::KW_WCHAR_T:
		return true;
	default:
		return false;
	}
}

bool is_builtin_function_style_cast_keyword_impl(SimpleTokenType type)
{
	switch (type)
	{
	case SimpleTokenType::KW_BOOL:
	case SimpleTokenType::KW_CHAR:
	case SimpleTokenType::KW_CHAR16_T:
	case SimpleTokenType::KW_CHAR32_T:
	case SimpleTokenType::KW_DOUBLE:
	case SimpleTokenType::KW_FLOAT:
	case SimpleTokenType::KW_INT:
	case SimpleTokenType::KW_LONG:
	case SimpleTokenType::KW_SHORT:
	case SimpleTokenType::KW_SIGNED:
	case SimpleTokenType::KW_UNSIGNED:
	case SimpleTokenType::KW_VOID:
	case SimpleTokenType::KW_WCHAR_T:
		return true;
	default:
		return false;
	}
}

bool is_operator_function_token_impl(SimpleTokenType type)
{
	switch (type)
	{
	case SimpleTokenType::KW_NEW:
	case SimpleTokenType::KW_DELETE:
	case SimpleTokenType::OP_PLUS:
	case SimpleTokenType::OP_MINUS:
	case SimpleTokenType::OP_STAR:
	case SimpleTokenType::OP_DIV:
	case SimpleTokenType::OP_MOD:
	case SimpleTokenType::OP_XOR:
	case SimpleTokenType::OP_AMP:
	case SimpleTokenType::OP_BOR:
	case SimpleTokenType::OP_COMPL:
	case SimpleTokenType::OP_LNOT:
	case SimpleTokenType::OP_ASS:
	case SimpleTokenType::OP_LT:
	case SimpleTokenType::OP_GT:
	case SimpleTokenType::OP_PLUSASS:
	case SimpleTokenType::OP_MINUSASS:
	case SimpleTokenType::OP_STARASS:
	case SimpleTokenType::OP_DIVASS:
	case SimpleTokenType::OP_MODASS:
	case SimpleTokenType::OP_XORASS:
	case SimpleTokenType::OP_BANDASS:
	case SimpleTokenType::OP_BORASS:
	case SimpleTokenType::OP_LSHIFT:
	case SimpleTokenType::OP_RSHIFT:
	case SimpleTokenType::OP_LSHIFTASS:
	case SimpleTokenType::OP_RSHIFTASS:
	case SimpleTokenType::OP_EQ:
	case SimpleTokenType::OP_NE:
	case SimpleTokenType::OP_LE:
	case SimpleTokenType::OP_GE:
	case SimpleTokenType::OP_LAND:
	case SimpleTokenType::OP_LOR:
	case SimpleTokenType::OP_INC:
	case SimpleTokenType::OP_DEC:
	case SimpleTokenType::OP_COMMA:
	case SimpleTokenType::OP_ARROWSTAR:
	case SimpleTokenType::OP_ARROW:
	case SimpleTokenType::OP_DOTSTAR:
		return true;
	default:
		return false;
	}
}

bool find_template_close(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& close_index, std::size_t absolute_lt,
	std::size_t* absolute_close)
{
	if (!token_fixed_at(tokens, absolute_lt, 0, SimpleTokenType::OP_LT) ||
		absolute_lt >= close_index.size() ||
		close_index[absolute_lt] >= tokens.size())
		return false;
	if (absolute_close != NULL)
		*absolute_close = close_index[absolute_lt];
	return true;
}

bool member_function_specifier_at(const std::vector<PA10Token>& tokens,
	std::size_t absolute)
{
	return token_fixed_at(tokens, absolute, 0, SimpleTokenType::KW_INLINE) ||
		token_fixed_at(tokens, absolute, 0, SimpleTokenType::KW_VIRTUAL) ||
		token_fixed_at(tokens, absolute, 0, SimpleTokenType::KW_EXPLICIT) ||
		token_fixed_at(tokens, absolute, 0, SimpleTokenType::KW_CONSTEXPR) ||
		token_fixed_at(tokens, absolute, 0, SimpleTokenType::KW_FRIEND) ||
		token_fixed_at(tokens, absolute, 0, SimpleTokenType::KW_STATIC);
}

bool attribute_start_at(const std::vector<PA10Token>& tokens,
	std::size_t absolute)
{
	return (token_identifier_at(tokens, absolute) &&
		tokens[absolute].contextual_identifier ==
			PA10ContextualIdentifierKind::AttributeIntroducer) ||
		token_fixed_at(tokens, absolute, 0, SimpleTokenType::KW_ALIGNAS) ||
	(token_fixed_at(tokens, absolute, 0, SimpleTokenType::OP_LSQUARE) &&
	 token_fixed_at(tokens, absolute, 1, SimpleTokenType::OP_LSQUARE));
}

bool attribute_after_at(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t absolute, std::size_t* after)
{
	if (token_identifier_at(tokens, absolute) &&
		tokens[absolute].contextual_identifier ==
			PA10ContextualIdentifierKind::AttributeIntroducer)
	{
		if (!token_fixed_at(tokens, absolute, 1, SimpleTokenType::OP_LPAREN))
			return false;
		const std::size_t open = absolute + 1;
		if (open >= delimiter_close_index.size() ||
			delimiter_close_index[open] >= tokens.size())
			return false;
		*after = delimiter_close_index[open] + 1;
		return true;
	}
	if (token_fixed_at(tokens, absolute, 0, SimpleTokenType::KW_ALIGNAS))
	{
		if (!token_fixed_at(tokens, absolute, 1, SimpleTokenType::OP_LPAREN))
			return false;
		const std::size_t open = absolute + 1;
		if (open >= delimiter_close_index.size() ||
			delimiter_close_index[open] >= tokens.size())
			return false;
		*after = delimiter_close_index[open] + 1;
		return true;
	}
	if (token_fixed_at(tokens, absolute, 0, SimpleTokenType::OP_LSQUARE) &&
		token_fixed_at(tokens, absolute, 1, SimpleTokenType::OP_LSQUARE))
	{
		if (absolute >= delimiter_close_index.size() ||
			delimiter_close_index[absolute] >= tokens.size())
			return false;
		*after = delimiter_close_index[absolute] + 1;
		return true;
	}
	return false;
}

bool conversion_operator_start_at(const std::vector<PA10Token>& tokens,
	std::size_t absolute)
{
	if (!token_fixed_at(tokens, absolute, 0, SimpleTokenType::KW_OPERATOR) ||
		absolute + 1 >= tokens.size())
		return false;
	const PA10Token& target = tokens[absolute + 1];
	if (target.kind == PA10TokenKind::Identifier)
		return true;
	if (target.kind != PA10TokenKind::Fixed)
		return false;
	if (is_operator_function_token_impl(target.fixed) ||
		target.fixed == SimpleTokenType::OP_LPAREN ||
		target.fixed == SimpleTokenType::OP_LSQUARE)
		return false;
	if (target.fixed == SimpleTokenType::OP_COLON2)
		return token_identifier_at(tokens, absolute, 2);
	return target.fixed == SimpleTokenType::KW_DECLTYPE ||
		target.fixed == SimpleTokenType::KW_TYPENAME ||
		is_type_keyword_impl(target.fixed) || is_cv_impl(target.fixed);
}

bool special_name_start_at(const std::vector<PA10Token>& tokens,
	std::size_t absolute)
{
	return conversion_operator_start_at(tokens, absolute) ||
		token_fixed_at(tokens, absolute, 0, SimpleTokenType::OP_COMPL) ||
		(token_identifier_at(tokens, absolute) &&
		 token_fixed_at(tokens, absolute, 1, SimpleTokenType::OP_LPAREN));
}

bool skip_balanced_delimiters(const std::vector<PA10Token>& tokens,
	std::size_t position, std::size_t* after, std::size_t* consumed)
{
	std::vector<SimpleTokenType> closes;
	std::size_t cursor = position;
	std::size_t count = 0;
	while (true)
	{
		if (cursor >= tokens.size() || tokens[cursor].kind == PA10TokenKind::End)
			return false;
		const PA10Token& token = tokens[cursor];
		if (token.kind == PA10TokenKind::Fixed)
		{
			SimpleTokenType close = SimpleTokenType::OP_SEMICOLON;
			bool opening = false;
			switch (token.fixed)
			{
			case SimpleTokenType::OP_LPAREN:
				close = SimpleTokenType::OP_RPAREN;
				opening = true;
				break;
			case SimpleTokenType::OP_LSQUARE:
				close = SimpleTokenType::OP_RSQUARE;
				opening = true;
				break;
			case SimpleTokenType::OP_LBRACE:
				close = SimpleTokenType::OP_RBRACE;
				opening = true;
				break;
			case SimpleTokenType::OP_RPAREN:
			case SimpleTokenType::OP_RSQUARE:
			case SimpleTokenType::OP_RBRACE:
				if (closes.empty() || closes.back() != token.fixed)
					return false;
				closes.pop_back();
				break;
			default:
				break;
			}
			if (opening)
				closes.push_back(close);
		}
		++cursor;
		++count;
		if (closes.empty())
		{
			*after = cursor;
			*consumed = count;
			return true;
		}
	}
}

enum NewAbstractGroupKind
{
	NewAbstractNone = 0,
	NewAbstractShape = 1,
	NewParameterClause = 2,
	// A nested parameter-only group is an abstract form only in the
	// parenthesized type-id spelling; keep it distinct from an initializer.
	NewNestedParameter = 3
};

void fact_step(std::size_t& work)
{
	++work;
}

bool fact_identifier_at(const std::vector<PA10Token>& tokens,
	std::size_t absolute, std::size_t offset, std::size_t& work)
{
	fact_step(work);
	return token_identifier_at(tokens, absolute, offset);
}

bool fact_fixed_at(const std::vector<PA10Token>& tokens,
	std::size_t absolute, std::size_t offset, SimpleTokenType type,
	std::size_t& work)
{
	fact_step(work);
	return token_fixed_at(tokens, absolute, offset, type);
}

bool fact_cv_at(const std::vector<PA10Token>& tokens,
	std::size_t absolute, std::size_t& work)
{
	fact_step(work);
	return absolute < tokens.size() &&
		tokens[absolute].kind == PA10TokenKind::Fixed &&
		is_cv_impl(tokens[absolute].fixed);
}

unsigned char fact_group_kind_at(const std::vector<unsigned char>& groups,
	std::size_t absolute, std::size_t& work)
{
	fact_step(work);
	return absolute < groups.size() ? groups[absolute] :
		static_cast<unsigned char>(NewAbstractNone);
}

bool member_pointer_end_at(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	std::size_t begin, std::size_t end, std::size_t* after,
	std::size_t& work);

bool parameter_clause_start_at(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t open, std::size_t& work)
{
	fact_step(work);
	if (!fact_fixed_at(tokens, open, 0, SimpleTokenType::OP_LPAREN, work) ||
		open >= delimiter_close_index.size() ||
		delimiter_close_index[open] >= tokens.size())
		return false;
	if (delimiter_close_index[open] == open + 1 ||
		fact_fixed_at(tokens, open, 1, SimpleTokenType::OP_DOTS, work))
		return true;
	fact_step(work);
	if (tokens[open + 1].kind == PA10TokenKind::Fixed)
	{
		const SimpleTokenType type = tokens[open + 1].fixed;
		fact_step(work);
		return is_type_keyword_impl(type) || is_cv_impl(type) ||
			type == SimpleTokenType::KW_TYPEDEF ||
			type == SimpleTokenType::KW_EXTERN ||
			type == SimpleTokenType::KW_STATIC ||
			type == SimpleTokenType::KW_INLINE ||
			type == SimpleTokenType::KW_VIRTUAL ||
			type == SimpleTokenType::KW_CONSTEXPR ||
			type == SimpleTokenType::KW_THREAD_LOCAL ||
			type == SimpleTokenType::KW_TYPENAME ||
			type == SimpleTokenType::KW_DECLTYPE;
	}
	if (fact_fixed_at(tokens, open, 1, SimpleTokenType::OP_COLON2, work))
	{
		std::size_t after = 0;
		if (member_pointer_end_at(tokens, template_close_index,
			rshift_piece1_nested_close, open + 1,
			delimiter_close_index[open], &after, work))
			return false;
		return fact_identifier_at(tokens, open, 2, work) ||
			fact_fixed_at(tokens, open, 2, SimpleTokenType::KW_TEMPLATE, work);
	}
	if (!fact_identifier_at(tokens, open, 1, work))
		return false;
	if (fact_fixed_at(tokens, open, 2, SimpleTokenType::OP_COLON2, work))
	{
		std::size_t after = 0;
		if (member_pointer_end_at(tokens, template_close_index,
			rshift_piece1_nested_close, open + 1,
			delimiter_close_index[open], &after, work))
			return false;
	}
	if (fact_fixed_at(tokens, open, 2, SimpleTokenType::OP_LT, work))
	{
		const std::size_t angle = open + 2;
		fact_step(work);
		if (angle < template_close_index.size())
		{
			const std::size_t close = template_close_index[angle];
			fact_step(work);
			if (close < delimiter_close_index[open])
			{
				std::size_t after = close + 1;
				fact_step(work);
				if (tokens[close].kind == PA10TokenKind::RShiftPiece1 &&
					after < delimiter_close_index[open] &&
					close < rshift_piece1_nested_close.size() &&
					rshift_piece1_nested_close[close])
					++after;
				if (fact_fixed_at(tokens, after, 0,
					SimpleTokenType::OP_COLON2, work))
				{
					if (member_pointer_end_at(tokens, template_close_index,
						rshift_piece1_nested_close, open + 1,
						delimiter_close_index[open], &after, work))
						return false;
				}
				fact_step(work);
				return after == delimiter_close_index[open] ||
					fact_identifier_at(tokens, after, 0, work) ||
					fact_fixed_at(tokens, after, 0, SimpleTokenType::OP_STAR, work) ||
					fact_fixed_at(tokens, after, 0, SimpleTokenType::OP_AMP, work) ||
					fact_fixed_at(tokens, after, 0, SimpleTokenType::OP_LAND, work) ||
					fact_fixed_at(tokens, after, 0, SimpleTokenType::OP_COLON2, work) ||
					fact_fixed_at(tokens, after, 0, SimpleTokenType::OP_COMMA, work);
			}
		}
	}
	fact_step(work);
	return delimiter_close_index[open] == open + 2 ||
		fact_fixed_at(tokens, open, 2, SimpleTokenType::OP_DOTS, work) ||
		fact_identifier_at(tokens, open, 2, work) ||
		fact_fixed_at(tokens, open, 2, SimpleTokenType::OP_STAR, work) ||
		fact_fixed_at(tokens, open, 2, SimpleTokenType::OP_AMP, work) ||
		fact_fixed_at(tokens, open, 2, SimpleTokenType::OP_LAND, work) ||
		fact_fixed_at(tokens, open, 2, SimpleTokenType::OP_COLON2, work);
}

bool member_pointer_end_at(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	std::size_t begin, std::size_t end, std::size_t* after,
	std::size_t& work)
{
	fact_step(work);
	std::size_t cursor = begin;
	if (fact_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_COLON2, work))
		++cursor;
	while (cursor < end)
	{
		fact_step(work);
		if (fact_fixed_at(tokens, cursor, 0, SimpleTokenType::KW_TEMPLATE, work))
			++cursor;
		if (!fact_identifier_at(tokens, cursor, 0, work))
			return false;
		++cursor;
		if (fact_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_LT, work))
		{
			fact_step(work);
			if (cursor >= template_close_index.size() ||
				template_close_index[cursor] >= end)
				return false;
			const std::size_t close = template_close_index[cursor];
			fact_step(work);
			cursor = close + 1;
			if (tokens[close].kind == PA10TokenKind::RShiftPiece1 &&
				cursor < end && tokens[cursor].kind == PA10TokenKind::RShiftPiece2 &&
				close < rshift_piece1_nested_close.size() &&
				rshift_piece1_nested_close[close])
				++cursor;
		}
		if (!fact_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_COLON2, work))
			return false;
		++cursor;
		if (fact_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_STAR, work))
		{
			*after = cursor + 1;
			return true;
		}
	}
	return false;
}

unsigned char new_abstract_group_at(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	const std::vector<std::size_t>& delimiter_close_index,
	const std::vector<unsigned char>& groups, std::size_t open,
	std::size_t& work)
{
	fact_step(work);
	if (open >= delimiter_close_index.size() ||
		delimiter_close_index[open] >= tokens.size())
		return NewAbstractNone;
	const std::size_t end = delimiter_close_index[open];
	if (parameter_clause_start_at(tokens, template_close_index,
		rshift_piece1_nested_close, delimiter_close_index, open, work))
		return NewParameterClause;
	std::size_t cursor = open + 1;
	bool pointer = false;
	while (cursor < end)
	{
		fact_step(work);
		if (fact_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_STAR, work) ||
			fact_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_AMP, work) ||
			fact_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_LAND, work))
		{
			pointer = true;
			++cursor;
			while (cursor < end && fact_cv_at(tokens, cursor, work))
				++cursor;
			continue;
		}
		std::size_t after = 0;
		if (member_pointer_end_at(tokens, template_close_index,
			rshift_piece1_nested_close, cursor, end, &after, work))
		{
			pointer = true;
			cursor = after;
			while (cursor < end && fact_cv_at(tokens, cursor, work))
				++cursor;
			continue;
		}
		break;
	}
	if (pointer)
	{
		if (cursor == end || fact_fixed_at(tokens, cursor, 0,
			SimpleTokenType::OP_LSQUARE, work))
			return NewAbstractShape;
		if (fact_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_LPAREN, work))
		{
			const bool parameter = parameter_clause_start_at(tokens, template_close_index,
				rshift_piece1_nested_close, delimiter_close_index, cursor, work);
			const unsigned char nested = fact_group_kind_at(groups, cursor, work);
			if (parameter || nested == NewParameterClause ||
				nested == NewNestedParameter)
				return NewNestedParameter;
			if (nested == NewAbstractShape)
				return NewAbstractShape;
		}
		return NewAbstractNone;
	}
	if (fact_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_LSQUARE, work))
		return NewAbstractShape;
	if (fact_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_LPAREN, work))
	{
		const unsigned char nested = fact_group_kind_at(groups, cursor, work);
		if (nested == NewAbstractShape)
			return NewAbstractShape;
		if (nested == NewParameterClause || nested == NewNestedParameter)
			return NewNestedParameter;
	}
	return NewAbstractNone;
}

std::size_t build_new_abstract_declarator_groups(
	const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	const std::vector<std::size_t>& delimiter_close_index,
	std::vector<unsigned char>& groups)
{
	// Every delimiter group is classified once in reverse token order, so a
	// nested group's result is available before its enclosing group is read.
	// A group owns only its leading pointer/member-pointer spine; a nested
	// delimiter stops that scan.  Thus the variable scans are disjoint apart
	// from the constant duplicate check shared with parameter classification.
	// The returned counter records each indexed predicate and scan step.
	std::size_t work = tokens.size();
	groups.assign(tokens.size(), 0);
	for (std::size_t reverse = tokens.size(); reverse != 0; --reverse)
	{
		fact_step(work);
		const std::size_t open = reverse - 1;
		if (fact_fixed_at(tokens, open, 0, SimpleTokenType::OP_LPAREN, work))
			groups[open] = new_abstract_group_at(tokens, template_close_index,
				rshift_piece1_nested_close, delimiter_close_index, groups, open,
				work);
	}
	return work;
}

} // namespace

bool is_cv(SimpleTokenType type)
{
	return is_cv_impl(type);
}

bool is_type_keyword(SimpleTokenType type)
{
	return is_type_keyword_impl(type);
}

bool is_builtin_function_style_cast_keyword(SimpleTokenType type)
{
	return is_builtin_function_style_cast_keyword_impl(type);
}

bool is_operator_function_token(SimpleTokenType type)
{
	return is_operator_function_token_impl(type);
}

bool collect_tokens(const PPTokenBuffer& input, std::vector<PA10Token>& tokens)
{
	PA10PostTokenCollector collector;
	posttokenize_cpp_tokens(input, collector);
	if (collector.invalid)
		return false;
	tokens.swap(collector.tokens);
	return true;
}

std::size_t build_indexes(const std::vector<PA10Token>& tokens,
	std::vector<std::size_t>& template_close_index,
	std::vector<unsigned char>& template_top_level_or,
	std::vector<unsigned char>& rshift_piece1_nested_close,
	std::vector<std::size_t>& delimiter_close_index,
	std::vector<unsigned char>& new_abstract_declarator_group)
{
	template_close_index.assign(tokens.size(), tokens.size());
	template_top_level_or.assign(tokens.size(), 0);
	rshift_piece1_nested_close.assign(tokens.size(), 0);
	delimiter_close_index.assign(tokens.size(), tokens.size());
	std::vector<std::vector<std::size_t> > angle_stacks(1);
	struct DelimiterFrame
	{
		SimpleTokenType kind;
		std::size_t open;
		DelimiterFrame(SimpleTokenType kind, std::size_t open)
			: kind(kind), open(open)
		{}
	};
	std::vector<DelimiterFrame> delimiters;
	for (std::size_t i = 0; i < tokens.size(); ++i)
	{
		const PA10Token& token = tokens[i];
		if (token.kind == PA10TokenKind::RShiftPiece1 ||
			token.kind == PA10TokenKind::RShiftPiece2)
		{
			if (!angle_stacks.back().empty())
			{
				if (token.kind == PA10TokenKind::RShiftPiece2 && i != 0 &&
					tokens[i - 1].kind == PA10TokenKind::RShiftPiece1)
					rshift_piece1_nested_close[i - 1] = 1;
				const std::size_t open = angle_stacks.back().back();
				angle_stacks.back().pop_back();
				template_close_index[open] = i;
			}
			continue;
		}
		if (token.kind != PA10TokenKind::Fixed)
			continue;
		switch (token.fixed)
		{
		case SimpleTokenType::OP_LPAREN:
		case SimpleTokenType::OP_LSQUARE:
		case SimpleTokenType::OP_LBRACE:
			delimiters.push_back(DelimiterFrame(token.fixed, i));
			angle_stacks.push_back(std::vector<std::size_t>());
			break;
		case SimpleTokenType::OP_RPAREN:
		case SimpleTokenType::OP_RSQUARE:
		case SimpleTokenType::OP_RBRACE:
			if (!delimiters.empty())
			{
				const SimpleTokenType open = delimiters.back().kind;
				const bool matching =
					(open == SimpleTokenType::OP_LPAREN &&
					 token.fixed == SimpleTokenType::OP_RPAREN) ||
					(open == SimpleTokenType::OP_LSQUARE &&
					 token.fixed == SimpleTokenType::OP_RSQUARE) ||
					(open == SimpleTokenType::OP_LBRACE &&
					 token.fixed == SimpleTokenType::OP_RBRACE);
				if (!matching)
					break;
				delimiter_close_index[delimiters.back().open] = i;
				delimiters.pop_back();
				angle_stacks.pop_back();
			}
			break;
		case SimpleTokenType::OP_LT:
			angle_stacks.back().push_back(i);
			break;
		case SimpleTokenType::OP_GT:
			if (!angle_stacks.back().empty())
			{
				const std::size_t open = angle_stacks.back().back();
				angle_stacks.back().pop_back();
				template_close_index[open] = i;
			}
			break;
		case SimpleTokenType::OP_LOR:
			if (!angle_stacks.back().empty())
				template_top_level_or[angle_stacks.back().back()] = 1;
			break;
		default:
			break;
		}
	}
	return tokens.size() + build_new_abstract_declarator_groups(tokens, template_close_index,
		rshift_piece1_nested_close, delimiter_close_index,
		new_abstract_declarator_group);
}

bool special_member_definition_start(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t position, bool in_class_member, std::size_t* charged_work)
{
	std::size_t work = 0;
	const auto finish = [&work, charged_work](bool result) {
		if (charged_work != NULL)
			*charged_work = work;
		return result;
	};
	std::size_t cursor = position;
	while (true)
	{
		while (member_function_specifier_at(tokens, cursor))
		{
			++work;
			++cursor;
		}
		if (!attribute_start_at(tokens, cursor))
			break;
		std::size_t after = 0;
		if (!attribute_after_at(tokens, delimiter_close_index, cursor, &after))
			return finish(false);
		++work;
		cursor = after;
	}

	if (special_name_start_at(tokens, cursor))
		return finish(in_class_member);

	if (token_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_COLON2))
	{
		++work;
		++cursor;
	}
	std::size_t last_component = tokens.size();
	while (true)
	{
		if (token_fixed_at(tokens, cursor, 0, SimpleTokenType::KW_TEMPLATE))
		{
			++work;
			++cursor;
		}
		if (!token_identifier_at(tokens, cursor))
			return finish(false);
		last_component = cursor;
		++work;
		++cursor;
		if (token_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_LT))
		{
			std::size_t close = 0;
			if (!find_template_close(tokens, template_close_index, cursor,
				&close))
				return finish(false);
			++work;
			cursor = close + 1;
			if (close < tokens.size() &&
				tokens[close].kind == PA10TokenKind::RShiftPiece1 &&
				cursor < tokens.size() &&
				tokens[cursor].kind == PA10TokenKind::RShiftPiece2 &&
				close < rshift_piece1_nested_close.size() &&
				rshift_piece1_nested_close[close])
			{
				++work;
				++cursor;
			}
		}
		if (!token_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_COLON2))
			return finish(false);
		++work;
		++cursor;
		if (special_name_start_at(tokens, cursor))
		{
			if (conversion_operator_start_at(tokens, cursor) ||
				token_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_COMPL))
				return finish(true);
			if (last_component < tokens.size() &&
				tokens[last_component].spelling == tokens[cursor].spelling)
				return finish(true);
		}
	}
}

bool skip_attribute_specifiers(const std::vector<PA10Token>& tokens,
	std::size_t position, std::size_t* after, std::size_t* consumed)
{
	std::size_t cursor = position;
	std::size_t count = 0;
	while (attribute_start_at(tokens, cursor))
	{
		if (token_identifier_at(tokens, cursor) ||
			token_fixed_at(tokens, cursor, 0, SimpleTokenType::KW_ALIGNAS))
		{
			++cursor;
			++count;
			if (!token_fixed_at(tokens, cursor, 0, SimpleTokenType::OP_LPAREN))
				return false;
		}
		std::size_t next = 0;
		std::size_t nested = 0;
		if (!skip_balanced_delimiters(tokens, cursor, &next, &nested))
			return false;
		cursor = next;
		count += nested;
	}
	*after = cursor;
	*consumed = count;
	return true;
}

} // namespace PA10ParserSupport
