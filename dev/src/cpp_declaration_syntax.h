#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include "cpp_syntax_core.h"
#include "cpp_syntax_tokens.h"

// Neutral syntax facts shared by PA6's common grammar path and PA7's
// semantic actions. Names remain spelling identities and literals remain
// decoded posttoken facts until a stage-specific owner consumes them.
struct CppSyntaxQualifiedName
{
	bool global;
	std::vector<PPSpellingId> components;

	CppSyntaxQualifiedName() : global(false), components() {}
};

struct CppSyntaxDeclSpec
{
	bool is_typedef;
	bool has_named_type;
	CppSyntaxQualifiedName named_type;
	unsigned int cv;
	bool has_char;
	bool has_short;
	bool has_int;
	unsigned int long_count;
	bool has_signed;
	bool has_unsigned;
	bool has_bool;
	bool has_wchar;
	bool has_char16;
	bool has_char32;
	bool has_float;
	bool has_double;
	bool has_void;

	CppSyntaxDeclSpec()
		: is_typedef(false), has_named_type(false), named_type(), cv(0),
		  has_char(false), has_short(false), has_int(false), long_count(0),
		  has_signed(false), has_unsigned(false), has_bool(false),
		  has_wchar(false), has_char16(false), has_char32(false),
		  has_float(false), has_double(false), has_void(false)
	{}
};

enum class CppSyntaxDeclaratorOpKind
{
	Pointer,
	LvalueReference,
	RvalueReference,
	Array,
	Function
};

struct CppSyntaxDeclaratorOp;
struct CppSyntaxParameter;

struct CppSyntaxDeclarator
{
	bool has_name;
	CppSyntaxQualifiedName name;
	std::vector<CppSyntaxDeclaratorOp> operations;

	CppSyntaxDeclarator() : has_name(false), name(), operations() {}
};

struct CppSyntaxDeclaratorOp
{
	CppSyntaxDeclaratorOpKind kind;
	unsigned int cv;
	bool unknown_bound;
	std::size_t bound;
	std::vector<CppSyntaxParameter> parameters;
	bool variadic;

	CppSyntaxDeclaratorOp(
		CppSyntaxDeclaratorOpKind kind = CppSyntaxDeclaratorOpKind::Pointer)
		: kind(kind), cv(0), unknown_bound(false), bound(0), parameters(),
		  variadic(false)
	{}
};

struct CppSyntaxParameter
{
	CppSyntaxDeclSpec spec;
	bool has_declarator;
	CppSyntaxDeclarator declarator;

	CppSyntaxParameter() : spec(), has_declarator(false), declarator() {}
};

struct CppSyntaxTypeId
{
	CppSyntaxDeclSpec spec;
	bool has_declarator;
	CppSyntaxDeclarator declarator;

	CppSyntaxTypeId() : spec(), has_declarator(false), declarator() {}
};

// This is intentionally an action boundary, not a second grammar. The parser
// below owns declaration/declarator/parameter production syntax once; PA6
// supplies an acceptance action and PA7 supplies semantic actions.
class CppDeclarationSyntaxConsumer
{
public:
	virtual ~CppDeclarationSyntaxConsumer() {}

	virtual void on_empty_declaration() {}
	virtual void on_namespace_begin(bool inline_namespace,
		bool anonymous_namespace, PPSpellingId name)
	{
		(void)inline_namespace;
		(void)anonymous_namespace;
		(void)name;
	}
	virtual void on_namespace_end() {}
	virtual void on_namespace_alias(PPSpellingId name,
		const CppSyntaxQualifiedName& target)
	{
		(void)name;
		(void)target;
	}
	virtual void on_using_directive(const CppSyntaxQualifiedName& target)
	{
		(void)target;
	}
	virtual void on_using_declaration(const CppSyntaxQualifiedName& name)
	{
		(void)name;
	}
	virtual void on_alias_declaration(PPSpellingId name,
		const CppSyntaxTypeId& type)
	{
		(void)name;
		(void)type;
	}
	virtual void on_simple_declaration(const CppSyntaxDeclSpec& spec,
		const std::vector<CppSyntaxDeclarator>& declarators)
	{
		(void)spec;
		(void)declarators;
	}

	// These are policy queries at the point where the grammar needs a
	// potentially ambiguous name category. PA6 answers from its lexical mock
	// categories; PA7 answers from the current semantic namespace.
	virtual bool accept_type_name(const CppSyntaxQualifiedName& name) const
	{
		(void)name;
		return true;
	}
	virtual bool accept_namespace_name(
		const CppSyntaxQualifiedName& name) const
	{
		(void)name;
		return true;
	}
	virtual bool accept_nested_name_specifier(
		const CppSyntaxQualifiedName& name) const
	{
		(void)name;
		return true;
	}
};

struct CppSyntaxDeclarationTraits
{
	static bool is_end(const CppSyntaxToken& token)
	{
		return token.kind == CppSyntaxTokenKind::End;
	}

	static bool is_fixed(const CppSyntaxToken& token, SimpleTokenType wanted)
	{
		return token.kind == CppSyntaxTokenKind::Fixed &&
			token.fixed == wanted;
	}

	static bool is_identifier(const CppSyntaxToken& token)
	{
		return token.kind == CppSyntaxTokenKind::Identifier;
	}

	static bool is_literal(const CppSyntaxToken& token)
	{
		return token.kind == CppSyntaxTokenKind::Literal;
	}

	static std::size_t max_nesting()
	{
		return 4096;
	}

	static std::size_t work_limit_for(std::size_t token_count)
	{
		const std::size_t overhead = 1024;
		const std::size_t per_token = 64;
		const std::size_t maximum = std::numeric_limits<std::size_t>::max();
		if (token_count > (maximum - overhead) / per_token)
			return maximum;
		return token_count * per_token + overhead;
	}
};

class CppDeclarationSyntaxParser : private CppSyntaxCore<
	CppSyntaxToken, CppSyntaxDeclarationTraits>
{
public:
	CppDeclarationSyntaxParser(const std::vector<CppSyntaxToken>& tokens,
		CppDeclarationSyntaxConsumer& consumer);

	void parse();

private:
	CppDeclarationSyntaxConsumer& consumer_;

	const CppSyntaxToken& look(std::size_t offset = 0) const;
	bool fixed(SimpleTokenType type, std::size_t offset = 0) const;
	bool identifier(std::size_t offset = 0) const;
	bool literal(std::size_t offset = 0) const;
	void tick();
	void consume_fixed(SimpleTokenType type);
	PPSpellingId consume_identifier();
	LiteralData consume_literal();
	void enter_nesting();
	void leave_nesting();
	void parse_declaration();
	void parse_namespace(bool inline_namespace);
	void parse_using();
	CppSyntaxQualifiedName parse_qualified_name();
	void parse_simple_declaration();
	bool is_cv(SimpleTokenType type) const;
	bool consume_decl_specifier(CppSyntaxDeclSpec* spec);
	CppSyntaxDeclSpec parse_decl_specifiers();
	CppSyntaxTypeId parse_type_id();
	std::size_t parse_array_bound();
	std::vector<CppSyntaxParameter> parse_parameter_clause(bool* variadic);
	bool abstract_parenthesis_is_grouped() const;
	CppSyntaxDeclarator parse_noptr_declarator(bool allow_abstract);
	CppSyntaxDeclarator parse_ptr_declarator(bool allow_abstract);
};
