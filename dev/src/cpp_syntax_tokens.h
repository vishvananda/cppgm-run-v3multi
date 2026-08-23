#pragma once

#include <string>
#include <vector>

#include "posttoken.h"

// This is the one posttoken syntax representation consumed by the staged
// declaration parsers.  It keeps the production facts common to both stages:
// fixed-token enums, spelling identities, decoded literals, an explicit UDL
// marker, and EOF.  PA6-only mock categories and spelling-sensitive tokens
// are produced by its cold observer adapter, not by ordinary PA7 collection.
enum class CppSyntaxTokenKind
{
	Fixed,
	Identifier,
	Literal,
	UserDefinedLiteral,
	End
};

struct CppSyntaxToken
{
	CppSyntaxTokenKind kind;
	SimpleTokenType fixed;
	PPSpellingId spelling;
	LiteralData literal;

	CppSyntaxToken(CppSyntaxTokenKind kind = CppSyntaxTokenKind::End,
		SimpleTokenType fixed = SimpleTokenType::OP_SEMICOLON,
		PPSpellingId spelling = 0)
		: kind(kind), fixed(fixed), spelling(spelling), literal()
	{}
};

// Optional cold-sidecar observer. PA7 passes NULL, so ordinary nsdecl
// collection performs no PA6 spelling classification or compatibility
// adaptation. PA6 supplies its adapter here while the canonical token stream
// remains unchanged.
class CppSyntaxTokenObserver
{
public:
	virtual ~CppSyntaxTokenObserver() {}
	virtual void on_simple(PPSpellingId spelling, const std::string& source,
		SimpleTokenType type) = 0;
	virtual void on_identifier(PPSpellingId spelling,
		const std::string& source) = 0;
	virtual void on_literal(const std::string& source,
		const LiteralData& value) = 0;
	virtual void on_user_defined_literal(
		const UserDefinedLiteralData& value) = 0;
	virtual void on_eof() = 0;
};

class CppSyntaxTokenCollector : public IPostTokenOutput
{
public:
	explicit CppSyntaxTokenCollector(CppSyntaxTokenObserver* observer = NULL)
		: tokens(), invalid(false), observer_(observer)
	{}

	void emit_invalid(const std::string& source)
	{
		(void)source;
		invalid = true;
	}

	void emit_simple(const std::string& source, SimpleTokenType type)
	{
		tokens.push_back(CppSyntaxToken(CppSyntaxTokenKind::Fixed, type));
		if (observer_ != NULL)
			observer_->on_simple(0, source, type);
	}

	void emit_simple_identifier(const std::string& source,
		SimpleTokenType type)
	{
		emit_simple_identifier_with_spelling(0, source, type);
	}

	void emit_simple_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source, SimpleTokenType type)
	{
		tokens.push_back(CppSyntaxToken(CppSyntaxTokenKind::Fixed, type,
			spelling));
		if (observer_ != NULL)
			observer_->on_simple(spelling, source, type);
	}

	void emit_identifier(const std::string& source)
	{
		emit_identifier_with_spelling(0, source);
	}

	void emit_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source)
	{
		tokens.push_back(CppSyntaxToken(CppSyntaxTokenKind::Identifier,
			SimpleTokenType::OP_SEMICOLON, spelling));
		if (observer_ != NULL)
			observer_->on_identifier(spelling, source);
	}

	void emit_literal(const std::string& source, const LiteralData& value)
	{
		CppSyntaxToken token(CppSyntaxTokenKind::Literal);
		token.literal = value;
		tokens.push_back(token);
		if (observer_ != NULL)
			observer_->on_literal(source, value);
	}

	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		// PA7's grammar accepts only ordinary literals here.  Keep an
		// explicit non-literal marker; the decoded UDL payload belongs to the
		// PA6 observer (which maps it to PA6's literal category).
		CppSyntaxToken token(CppSyntaxTokenKind::UserDefinedLiteral);
		tokens.push_back(token);
		if (observer_ != NULL)
			observer_->on_user_defined_literal(value);
	}

	void emit_eof()
	{
		if (tokens.empty() || tokens.back().kind != CppSyntaxTokenKind::End)
			tokens.push_back(CppSyntaxToken(CppSyntaxTokenKind::End));
		if (observer_ != NULL)
			observer_->on_eof();
	}

	std::vector<CppSyntaxToken> tokens;
	bool invalid;

private:
	CppSyntaxTokenObserver* observer_;
};
