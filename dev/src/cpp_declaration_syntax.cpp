#include "cpp_declaration_syntax.h"

CppDeclarationSyntaxParser::CppDeclarationSyntaxParser(
	const std::vector<CppSyntaxToken>& tokens,
	CppDeclarationSyntaxConsumer& consumer, bool use_mock_type_categories)
	: CppSyntaxCore<CppSyntaxToken, CppSyntaxDeclarationTraits>(tokens),
	  consumer_(consumer), known_type_spellings_(),
	  use_mock_type_categories_(use_mock_type_categories)
{}

void CppDeclarationSyntaxParser::parse()
{
	if (tokens_.empty() ||
		tokens_.back().kind != CppSyntaxTokenKind::End)
		throw std::runtime_error("missing C++ syntax EOF");
	while (!eof())
		parse_declaration();
	if (!eof())
		throw std::runtime_error("C++ syntax parser did not consume input");
}

const CppSyntaxToken& CppDeclarationSyntaxParser::look(
	std::size_t offset) const
{
	const CppSyntaxToken* token =
		CppSyntaxCore<CppSyntaxToken, CppSyntaxDeclarationTraits>::look(
			offset);
	if (token == NULL)
		throw std::runtime_error("C++ syntax parser read past end");
	return *token;
}

bool CppDeclarationSyntaxParser::fixed(SimpleTokenType type,
	std::size_t offset) const
{
	return CppSyntaxCore<CppSyntaxToken,
		CppSyntaxDeclarationTraits>::fixed(type, offset);
}

bool CppDeclarationSyntaxParser::identifier(std::size_t offset) const
{
	return CppSyntaxCore<CppSyntaxToken,
		CppSyntaxDeclarationTraits>::identifier(offset);
}

bool CppDeclarationSyntaxParser::literal(std::size_t offset) const
{
	return CppSyntaxCore<CppSyntaxToken,
		CppSyntaxDeclarationTraits>::literal(offset);
}

void CppDeclarationSyntaxParser::tick()
{
	if (!charge())
		throw std::runtime_error("C++ syntax parser work limit reached");
}

void CppDeclarationSyntaxParser::consume_fixed(SimpleTokenType type)
{
	if (!fixed(type))
		throw std::runtime_error("unexpected C++ syntax fixed token");
	tick();
	advance();
}

PPSpellingId CppDeclarationSyntaxParser::consume_identifier()
{
	if (!identifier())
		throw std::runtime_error("expected C++ syntax identifier");
	tick();
	const PPSpellingId result = look().spelling;
	advance();
	return result;
}

LiteralData CppDeclarationSyntaxParser::consume_literal()
{
	if (!literal())
		throw std::runtime_error("expected C++ syntax literal");
	tick();
	const LiteralData result = look().literal;
	advance();
	return result;
}

void CppDeclarationSyntaxParser::enter_nesting()
{
	if (!begin_nesting())
		throw std::runtime_error("C++ syntax nesting limit reached");
}

void CppDeclarationSyntaxParser::leave_nesting()
{
	if (nesting_ == 0)
		throw std::runtime_error("C++ syntax nesting underflow");
	end_nesting();
}

void CppDeclarationSyntaxParser::parse_declaration()
{
	if (fixed(SimpleTokenType::OP_SEMICOLON))
	{
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		consumer_.on_empty_declaration();
		return;
	}
	if (fixed(SimpleTokenType::KW_INLINE))
	{
		consume_fixed(SimpleTokenType::KW_INLINE);
		if (!fixed(SimpleTokenType::KW_NAMESPACE))
			throw std::runtime_error("inline declaration is not a namespace");
		parse_namespace(true);
		return;
	}
	if (fixed(SimpleTokenType::KW_NAMESPACE))
	{
		parse_namespace(false);
		return;
	}
	if (fixed(SimpleTokenType::KW_USING))
	{
		parse_using();
		return;
	}
	parse_simple_declaration();
}

void CppDeclarationSyntaxParser::parse_namespace(bool inline_namespace)
{
	consume_fixed(SimpleTokenType::KW_NAMESPACE);
	PPSpellingId name = 0;
	bool anonymous = true;
	if (identifier())
	{
		name = consume_identifier();
		anonymous = false;
	}
	if (fixed(SimpleTokenType::OP_ASS))
	{
		if (anonymous)
			throw std::runtime_error("unnamed namespace alias");
		consume_fixed(SimpleTokenType::OP_ASS);
		CppSyntaxQualifiedName target = parse_qualified_name();
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		consumer_.on_namespace_alias(name, target);
		return;
	}
	consume_fixed(SimpleTokenType::OP_LBRACE);
	consumer_.on_namespace_begin(inline_namespace, anonymous, name);
	enter_nesting();
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (eof())
			throw std::runtime_error("unterminated C++ namespace");
		parse_declaration();
	}
	consume_fixed(SimpleTokenType::OP_RBRACE);
	leave_nesting();
	consumer_.on_namespace_end();
}

void CppDeclarationSyntaxParser::parse_using()
{
	consume_fixed(SimpleTokenType::KW_USING);
	if (fixed(SimpleTokenType::KW_NAMESPACE))
	{
		consume_fixed(SimpleTokenType::KW_NAMESPACE);
		CppSyntaxQualifiedName target = parse_qualified_name();
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		consumer_.on_using_directive(target);
		return;
	}
	CppSyntaxQualifiedName name = parse_qualified_name();
	if (fixed(SimpleTokenType::OP_ASS))
	{
		if (name.global || name.components.size() != 1)
			throw std::runtime_error("qualified alias declaration");
		consume_fixed(SimpleTokenType::OP_ASS);
		CppSyntaxTypeId type = parse_type_id();
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		remember_type(name.components.back());
		consumer_.on_alias_declaration(name.components.back(), type);
		return;
	}
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	consumer_.on_using_declaration(name);
}

CppSyntaxQualifiedName CppDeclarationSyntaxParser::parse_qualified_name()
{
	CppSyntaxQualifiedName result;
	if (fixed(SimpleTokenType::OP_COLON2))
	{
		result.global = true;
		consume_fixed(SimpleTokenType::OP_COLON2);
	}
	if (!identifier())
		throw std::runtime_error("expected qualified-name component");
	result.components.push_back(consume_identifier());
	while (fixed(SimpleTokenType::OP_COLON2))
	{
		consume_fixed(SimpleTokenType::OP_COLON2);
		if (!identifier())
			throw std::runtime_error("missing qualified-name component");
		result.components.push_back(consume_identifier());
	}
	return result;
}

void CppDeclarationSyntaxParser::parse_simple_declaration()
{
	CppSyntaxDeclSpec spec = parse_decl_specifiers();
	std::vector<CppSyntaxDeclarator> declarators;
	do
	{
		declarators.push_back(parse_ptr_declarator(false));
		if (!fixed(SimpleTokenType::OP_COMMA))
			break;
		consume_fixed(SimpleTokenType::OP_COMMA);
	} while (true);
	consume_fixed(SimpleTokenType::OP_SEMICOLON);
	if (spec.is_typedef)
		for (std::size_t i = 0; i < declarators.size(); ++i)
			if (declarators[i].has_name &&
				declarators[i].name.components.size() == 1 &&
				!declarators[i].name.global)
				remember_type(declarators[i].name.components.back());
	consumer_.on_simple_declaration(spec, declarators);
}

bool CppDeclarationSyntaxParser::is_cv(SimpleTokenType type) const
{
	return type == SimpleTokenType::KW_CONST ||
		type == SimpleTokenType::KW_VOLATILE;
}

bool CppDeclarationSyntaxParser::consume_decl_specifier(
	CppSyntaxDeclSpec* spec)
{
	if (fixed(SimpleTokenType::KW_TYPEDEF))
	{
		spec->is_typedef = true;
		consume_fixed(SimpleTokenType::KW_TYPEDEF);
	}
	else if (fixed(SimpleTokenType::KW_CONST))
	{
		spec->cv |= 1u;
		consume_fixed(SimpleTokenType::KW_CONST);
	}
	else if (fixed(SimpleTokenType::KW_VOLATILE))
	{
		spec->cv |= 2u;
		consume_fixed(SimpleTokenType::KW_VOLATILE);
	}
	else if (fixed(SimpleTokenType::KW_CHAR))
	{
		spec->has_char = true;
		consume_fixed(SimpleTokenType::KW_CHAR);
	}
	else if (fixed(SimpleTokenType::KW_CHAR16_T))
	{
		spec->has_char16 = true;
		consume_fixed(SimpleTokenType::KW_CHAR16_T);
	}
	else if (fixed(SimpleTokenType::KW_CHAR32_T))
	{
		spec->has_char32 = true;
		consume_fixed(SimpleTokenType::KW_CHAR32_T);
	}
	else if (fixed(SimpleTokenType::KW_WCHAR_T))
	{
		spec->has_wchar = true;
		consume_fixed(SimpleTokenType::KW_WCHAR_T);
	}
	else if (fixed(SimpleTokenType::KW_BOOL))
	{
		spec->has_bool = true;
		consume_fixed(SimpleTokenType::KW_BOOL);
	}
	else if (fixed(SimpleTokenType::KW_SHORT))
	{
		spec->has_short = true;
		consume_fixed(SimpleTokenType::KW_SHORT);
	}
	else if (fixed(SimpleTokenType::KW_INT))
	{
		spec->has_int = true;
		consume_fixed(SimpleTokenType::KW_INT);
	}
	else if (fixed(SimpleTokenType::KW_LONG))
	{
		++spec->long_count;
		consume_fixed(SimpleTokenType::KW_LONG);
	}
	else if (fixed(SimpleTokenType::KW_SIGNED))
	{
		spec->has_signed = true;
		consume_fixed(SimpleTokenType::KW_SIGNED);
	}
	else if (fixed(SimpleTokenType::KW_UNSIGNED))
	{
		spec->has_unsigned = true;
		consume_fixed(SimpleTokenType::KW_UNSIGNED);
	}
	else if (fixed(SimpleTokenType::KW_FLOAT))
	{
		spec->has_float = true;
		consume_fixed(SimpleTokenType::KW_FLOAT);
	}
	else if (fixed(SimpleTokenType::KW_DOUBLE))
	{
		spec->has_double = true;
		consume_fixed(SimpleTokenType::KW_DOUBLE);
	}
	else if (fixed(SimpleTokenType::KW_VOID))
	{
		spec->has_void = true;
		consume_fixed(SimpleTokenType::KW_VOID);
	}
	else if (fixed(SimpleTokenType::KW_STATIC) ||
		fixed(SimpleTokenType::KW_THREAD_LOCAL) ||
		fixed(SimpleTokenType::KW_EXTERN))
	{
		if (fixed(SimpleTokenType::KW_STATIC))
			consume_fixed(SimpleTokenType::KW_STATIC);
		else if (fixed(SimpleTokenType::KW_THREAD_LOCAL))
			consume_fixed(SimpleTokenType::KW_THREAD_LOCAL);
		else
			consume_fixed(SimpleTokenType::KW_EXTERN);
	}
	else
		return false;
	return true;
}

CppSyntaxDeclSpec CppDeclarationSyntaxParser::parse_decl_specifiers()
{
	CppSyntaxDeclSpec result;
	bool consumed = false;
	while (true)
	{
		if (identifier() || fixed(SimpleTokenType::OP_COLON2))
		{
			if (result.has_named_type || result.has_char ||
				result.has_short || result.has_int || result.long_count != 0 ||
				result.has_signed || result.has_unsigned || result.has_bool ||
				result.has_wchar || result.has_char16 || result.has_char32 ||
				result.has_float || result.has_double || result.has_void)
				break;
			result.has_named_type = true;
			result.named_type = parse_qualified_name();
			consumed = true;
			continue;
		}
		if (!consume_decl_specifier(&result))
			break;
		consumed = true;
	}
	if (!consumed)
		throw std::runtime_error("missing declaration specifier");
	return result;
}

CppSyntaxTypeId CppDeclarationSyntaxParser::parse_type_id()
{
	CppSyntaxTypeId result;
	result.spec = parse_decl_specifiers();
	if (fixed(SimpleTokenType::OP_STAR) ||
		fixed(SimpleTokenType::OP_AMP) ||
		fixed(SimpleTokenType::OP_LAND) ||
		fixed(SimpleTokenType::OP_LPAREN) ||
		fixed(SimpleTokenType::OP_LSQUARE))
	{
		result.has_declarator = true;
		result.declarator = parse_ptr_declarator(true);
	}
	return result;
}

std::size_t CppDeclarationSyntaxParser::parse_array_bound()
{
	const LiteralData value = consume_literal();
	if (value.bytes.size() > sizeof(std::uint64_t))
		throw std::runtime_error("array bound literal is too wide");
	std::uint64_t result = 0;
	for (std::size_t i = 0; i < value.bytes.size(); ++i)
		result |= static_cast<std::uint64_t>(value.bytes[i]) << (i * 8);
	if (result == 0 || result > std::numeric_limits<std::size_t>::max())
		throw std::runtime_error("invalid array bound");
	return static_cast<std::size_t>(result);
}

std::vector<CppSyntaxParameter>
CppDeclarationSyntaxParser::parse_parameter_clause(bool* variadic)
{
	consume_fixed(SimpleTokenType::OP_LPAREN);
	std::vector<CppSyntaxParameter> result;
	*variadic = false;
	if (fixed(SimpleTokenType::OP_RPAREN))
	{
		consume_fixed(SimpleTokenType::OP_RPAREN);
		return result;
	}
	while (true)
	{
		if (fixed(SimpleTokenType::OP_DOTS))
		{
			consume_fixed(SimpleTokenType::OP_DOTS);
			*variadic = true;
			break;
		}
		CppSyntaxParameter parameter;
		parameter.spec = parse_decl_specifiers();
		if (!fixed(SimpleTokenType::OP_COMMA) &&
			!fixed(SimpleTokenType::OP_RPAREN) &&
			!fixed(SimpleTokenType::OP_DOTS))
		{
			parameter.has_declarator = true;
			parameter.declarator = parse_ptr_declarator(true);
		}
		result.push_back(parameter);
		if (fixed(SimpleTokenType::OP_DOTS))
		{
			consume_fixed(SimpleTokenType::OP_DOTS);
			*variadic = true;
			break;
		}
		if (fixed(SimpleTokenType::OP_RPAREN))
			break;
		consume_fixed(SimpleTokenType::OP_COMMA);
	}
	consume_fixed(SimpleTokenType::OP_RPAREN);
	return result;
}

bool CppDeclarationSyntaxParser::known_type(PPSpellingId spelling,
	unsigned int categories) const
{
	if (use_mock_type_categories_ &&
		(categories & CPP_SYNTAX_NAME_TYPEDEF) != 0)
		return true;
	for (std::size_t i = 0; i < known_type_spellings_.size(); ++i)
		if (known_type_spellings_[i] == spelling)
			return true;
	return false;
}

void CppDeclarationSyntaxParser::remember_type(PPSpellingId spelling)
{
	if (!known_type(spelling))
		known_type_spellings_.push_back(spelling);
}

bool CppDeclarationSyntaxParser::abstract_parenthesis_is_grouped() const
{
	if (!fixed(SimpleTokenType::OP_LPAREN))
		return false;
	const CppSyntaxToken* next =
		CppSyntaxCore<CppSyntaxToken,
			CppSyntaxDeclarationTraits>::look(1);
	if (next == NULL)
		return false;
	if (next->kind == CppSyntaxTokenKind::Identifier ||
		next->kind == CppSyntaxTokenKind::Override ||
		next->kind == CppSyntaxTokenKind::Final)
		return !known_type(next->spelling, next->name_categories);
	return next->kind == CppSyntaxTokenKind::Fixed &&
		(next->fixed == SimpleTokenType::OP_STAR ||
		 next->fixed == SimpleTokenType::OP_AMP ||
		 next->fixed == SimpleTokenType::OP_LAND ||
		 next->fixed == SimpleTokenType::OP_LSQUARE ||
		 next->fixed == SimpleTokenType::OP_LPAREN);
}

CppSyntaxDeclarator
CppDeclarationSyntaxParser::parse_noptr_declarator(bool allow_abstract)
{
	CppSyntaxDeclarator result;
	if (identifier() || fixed(SimpleTokenType::OP_COLON2))
	{
		result.has_name = true;
		result.name = parse_qualified_name();
	}
	else if (fixed(SimpleTokenType::OP_LPAREN) && allow_abstract &&
		!abstract_parenthesis_is_grouped())
	{
		// The initial parenthesis is the function suffix of an abstract
		// declarator, handled by the suffix loop below.
	}
	else if (fixed(SimpleTokenType::OP_LPAREN))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		result = parse_ptr_declarator(allow_abstract);
		consume_fixed(SimpleTokenType::OP_RPAREN);
	}
	else if (!allow_abstract)
		throw std::runtime_error("expected declarator-id");

	while (fixed(SimpleTokenType::OP_LPAREN) ||
		fixed(SimpleTokenType::OP_LSQUARE))
	{
		if (fixed(SimpleTokenType::OP_LPAREN))
		{
			CppSyntaxDeclaratorOp operation(
				CppSyntaxDeclaratorOpKind::Function);
			operation.parameters = parse_parameter_clause(
				&operation.variadic);
			result.operations.push_back(operation);
		}
		else
		{
			consume_fixed(SimpleTokenType::OP_LSQUARE);
			CppSyntaxDeclaratorOp operation(
				CppSyntaxDeclaratorOpKind::Array);
			if (literal())
				operation.bound = parse_array_bound();
			else
				operation.unknown_bound = true;
			consume_fixed(SimpleTokenType::OP_RSQUARE);
			result.operations.push_back(operation);
		}
	}
	return result;
}

CppSyntaxDeclarator
CppDeclarationSyntaxParser::parse_ptr_declarator(bool allow_abstract)
{
	enter_nesting();
	std::vector<CppSyntaxDeclaratorOp> prefixes;
	while (fixed(SimpleTokenType::OP_STAR) ||
		fixed(SimpleTokenType::OP_AMP) ||
		fixed(SimpleTokenType::OP_LAND))
	{
		CppSyntaxDeclaratorOp operation;
		if (fixed(SimpleTokenType::OP_STAR))
		{
			operation.kind = CppSyntaxDeclaratorOpKind::Pointer;
			consume_fixed(SimpleTokenType::OP_STAR);
			while (is_cv(look().fixed))
			{
				if (fixed(SimpleTokenType::KW_CONST))
				{
					operation.cv |= 1u;
					consume_fixed(SimpleTokenType::KW_CONST);
				}
				else
				{
					operation.cv |= 2u;
					consume_fixed(SimpleTokenType::KW_VOLATILE);
				}
			}
		}
		else if (fixed(SimpleTokenType::OP_AMP))
		{
			operation.kind = CppSyntaxDeclaratorOpKind::LvalueReference;
			consume_fixed(SimpleTokenType::OP_AMP);
		}
		else
		{
			operation.kind = CppSyntaxDeclaratorOpKind::RvalueReference;
			consume_fixed(SimpleTokenType::OP_LAND);
		}
		prefixes.push_back(operation);
	}
	CppSyntaxDeclarator result = parse_noptr_declarator(allow_abstract);
	result.operations.insert(result.operations.end(), prefixes.begin(),
		prefixes.end());
	leave_nesting();
	return result;
}
