#pragma once

#include <string>
#include <vector>

#include "posttoken.h"

// This is the one posttoken syntax representation consumed by the staged
// declaration parsers.  It is deliberately a superset: PA6's mock categories
// and spelling-sensitive tokens remain available to its adapter while PA7
// keeps the canonical spelling and decoded literal payload.
enum class CppSyntaxTokenKind
{
	Fixed,
	Identifier,
	Literal,
	UserDefinedLiteral,
	EmptyString,
	Zero,
	Override,
	Final,
	Rshift1,
	Rshift2,
	End
};

enum CppSyntaxNameCategory
{
	CPP_SYNTAX_NAME_CLASS = 1u << 0,
	CPP_SYNTAX_NAME_TEMPLATE = 1u << 1,
	CPP_SYNTAX_NAME_TYPEDEF = 1u << 2,
	CPP_SYNTAX_NAME_ENUM = 1u << 3,
	CPP_SYNTAX_NAME_NAMESPACE = 1u << 4
};

struct CppSyntaxToken
{
	CppSyntaxTokenKind kind;
	SimpleTokenType fixed;
	PPSpellingId spelling;
	unsigned int name_categories;
	bool user_defined_literal;
	LiteralData literal;

	CppSyntaxToken(CppSyntaxTokenKind kind = CppSyntaxTokenKind::End,
		SimpleTokenType fixed = SimpleTokenType::OP_SEMICOLON,
		PPSpellingId spelling = 0)
		: kind(kind), fixed(fixed), spelling(spelling), name_categories(0),
		  user_defined_literal(false), literal()
	{}
};

class CppSyntaxTokenCollector : public IPostTokenOutput
{
public:
	CppSyntaxTokenCollector() : tokens(), invalid(false) {}

	void emit_invalid(const std::string& source)
	{
		(void)source;
		invalid = true;
	}

	void emit_simple(const std::string& source, SimpleTokenType type)
	{
		(void)source;
		if (type == SimpleTokenType::OP_RSHIFT)
		{
			tokens.push_back(CppSyntaxToken(CppSyntaxTokenKind::Rshift1));
			tokens.push_back(CppSyntaxToken(CppSyntaxTokenKind::Rshift2));
			return;
		}
		tokens.push_back(CppSyntaxToken(CppSyntaxTokenKind::Fixed, type));
	}

	void emit_simple_identifier(const std::string& source,
		SimpleTokenType type)
	{
		emit_simple_identifier_with_spelling(0, source, type);
	}

	void emit_simple_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source, SimpleTokenType type)
	{
		(void)source;
		tokens.push_back(CppSyntaxToken(CppSyntaxTokenKind::Fixed, type,
			spelling));
	}

	void emit_identifier(const std::string& source)
	{
		emit_identifier_with_spelling(0, source);
	}

	void emit_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source)
	{
		CppSyntaxToken token(CppSyntaxTokenKind::Identifier,
			SimpleTokenType::OP_SEMICOLON, spelling);
		if (source == "override")
			token.kind = CppSyntaxTokenKind::Override;
		else if (source == "final")
			token.kind = CppSyntaxTokenKind::Final;
		else
		{
			if (source.find('C') != std::string::npos)
				token.name_categories |= CPP_SYNTAX_NAME_CLASS;
			if (source.find('T') != std::string::npos)
				token.name_categories |= CPP_SYNTAX_NAME_TEMPLATE;
			if (source.find('Y') != std::string::npos)
				token.name_categories |= CPP_SYNTAX_NAME_TYPEDEF;
			if (source.find('E') != std::string::npos)
				token.name_categories |= CPP_SYNTAX_NAME_ENUM;
			if (source.find('N') != std::string::npos)
				token.name_categories |= CPP_SYNTAX_NAME_NAMESPACE;
		}
		tokens.push_back(token);
	}

	void emit_literal(const std::string& source, const LiteralData& value)
	{
		CppSyntaxToken token(CppSyntaxTokenKind::Literal,
			SimpleTokenType::OP_SEMICOLON, 0);
		token.literal = value;
		if (source == "\"\"")
			token.kind = CppSyntaxTokenKind::EmptyString;
		else if (source == "0")
			token.kind = CppSyntaxTokenKind::Zero;
		tokens.push_back(token);
	}

	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		(void)value;
		CppSyntaxToken token(CppSyntaxTokenKind::UserDefinedLiteral);
		token.user_defined_literal = true;
		tokens.push_back(token);
	}

	void emit_eof()
	{
		if (tokens.empty() || tokens.back().kind != CppSyntaxTokenKind::End)
			tokens.push_back(CppSyntaxToken(CppSyntaxTokenKind::End));
	}

	std::vector<CppSyntaxToken> tokens;
	bool invalid;
};
