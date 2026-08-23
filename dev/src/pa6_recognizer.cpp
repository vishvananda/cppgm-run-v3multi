#include "pa6_recognizer.h"
#include "pa6_parser.h"
#include "cpp_declaration_syntax.h"

#include <cstddef>
#include <string>
#include <vector>

namespace
{

enum MockNameCategory
{
	MockClass = 1u << 0,
	MockTemplate = 1u << 1,
	MockTypedef = 1u << 2,
	MockEnum = 1u << 3,
	MockNamespace = 1u << 4
};

unsigned int mock_categories(const std::string& source)
{
	unsigned int result = 0;
	if (source.find('C') != std::string::npos)
		result |= MockClass;
	if (source.find('T') != std::string::npos)
		result |= MockTemplate;
	if (source.find('Y') != std::string::npos)
		result |= MockTypedef;
	if (source.find('E') != std::string::npos)
		result |= MockEnum;
	if (source.find('N') != std::string::npos)
		result |= MockNamespace;
	return result;
}

class PA6TokenAdapter : public CppSyntaxTokenObserver
{
public:
	PA6TokenAdapter() : tokens() {}

	void on_simple(PPSpellingId spelling, const std::string& source,
		SimpleTokenType type)
	{
		(void)spelling;
		(void)source;
		if (type == SimpleTokenType::OP_RSHIFT)
		{
			tokens.push_back(PA6Token(PA6TokenKind::ST_RSHIFT_1));
			tokens.push_back(PA6Token(PA6TokenKind::ST_RSHIFT_2));
		}
		else
			tokens.push_back(PA6Token(PA6TokenKind::Fixed, type));
	}

	void on_identifier(PPSpellingId spelling, const std::string& source)
	{
		(void)spelling;
		if (source == "override")
		{
			tokens.push_back(PA6Token(PA6TokenKind::ST_OVERRIDE));
			return;
		}
		if (source == "final")
		{
			tokens.push_back(PA6Token(PA6TokenKind::ST_FINAL));
			return;
		}
		tokens.push_back(PA6Token(PA6TokenKind::Identifier,
			SimpleTokenType::OP_SEMICOLON, mock_categories(source)));
	}

	void on_literal(const std::string& source, const LiteralData& value)
	{
		(void)value;
		if (source == "\"\"")
			tokens.push_back(PA6Token(PA6TokenKind::ST_EMPTYSTR));
		else if (source == "0")
			tokens.push_back(PA6Token(PA6TokenKind::ST_ZERO));
		else
			tokens.push_back(PA6Token(PA6TokenKind::Literal));
	}

	void on_user_defined_literal(const UserDefinedLiteralData& value)
	{
		(void)value;
		tokens.push_back(PA6Token(PA6TokenKind::Literal));
	}

	void on_eof()
	{
		if (tokens.empty() || tokens.back().kind != PA6TokenKind::ST_EOF)
			tokens.push_back(PA6Token(PA6TokenKind::ST_EOF));
	}

	std::vector<PA6Token> tokens;
};

class PA6CommonSyntaxPolicy : public CppDeclarationSyntaxConsumer
{
public:
	explicit PA6CommonSyntaxPolicy(const PPSpellingTable& spellings)
		: spellings_(spellings)
	{}

	bool accept_type_name(const CppSyntaxQualifiedName& name) const
	{
		return !name.components.empty() &&
			valid_prefix(name, name.components.size() - 1) &&
			(category(name.components.back()) &
				(MockClass | MockEnum | MockTypedef)) != 0;
	}

	bool accept_namespace_name(
		const CppSyntaxQualifiedName& name) const
	{
		return !name.components.empty() &&
			valid_prefix(name, name.components.size() - 1) &&
			(category(name.components.back()) & MockNamespace) != 0;
	}

	bool accept_nested_name_specifier(
		const CppSyntaxQualifiedName& name) const
	{
		if (name.global)
			return true;
		return name.components.size() > 1 &&
			valid_prefix(name, name.components.size() - 1);
	}

private:
	const PPSpellingTable& spellings_;

	unsigned int category(PPSpellingId spelling) const
	{
		return mock_categories(spellings_.get(spelling));
	}

	bool valid_prefix(const CppSyntaxQualifiedName& name,
		std::size_t count) const
	{
		// A template-name is accepted by PA6 only when followed by its
		// angle argument list.  The shared common grammar deliberately does
		// not own template-id productions, so a bare T-category component
		// must fall through to the legacy parser instead of being treated as
		// a nested type/namespace prefix.
		const unsigned int allowed = MockClass | MockTypedef | MockEnum |
			MockNamespace;
		for (std::size_t i = 0; i < count; ++i)
			if ((category(name.components[i]) & allowed) == 0)
				return false;
		return true;
	}
};

} // namespace

bool PA6Recognizer::recognize(const PPTokenBuffer& input, std::string* reason) const
{
	PA6TokenAdapter adapter;
	CppSyntaxTokenCollector collector(&adapter);
	posttokenize_cpp_tokens(input, collector);
	if (collector.invalid)
	{
		if (reason != NULL)
			*reason = "invalid posttoken";
		return false;
	}
	// The shared parser is allowed to accept only the category-exact common
	// subset. A policy failure falls through to PA6's complete grammar, so the
	// fast path cannot turn a mock lookup mismatch into success.
	try
	{
		PA6CommonSyntaxPolicy policy(input.spellings);
		CppDeclarationSyntaxParser parser(collector.tokens, policy);
		parser.parse();
		return true;
	}
	catch (const std::runtime_error&)
	{
		// Continue to PA6's broader grammar below.
	}
	pa6_internal::PA6Parser parser(adapter.tokens);
	return parser.parse(reason);
}
