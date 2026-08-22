#include "pa6_recognizer.h"
#include "pa6_parser.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

class PA6TokenCollector : public IPostTokenOutput
{
public:
	PA6TokenCollector() : tokens(), invalid(false) {}

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
			tokens.push_back(PA6Token(PA6TokenKind::ST_RSHIFT_1));
			tokens.push_back(PA6Token(PA6TokenKind::ST_RSHIFT_2));
			return;
		}
		tokens.push_back(PA6Token(PA6TokenKind::Fixed, type));
	}

	void emit_simple_identifier(const std::string& source,
		SimpleTokenType type)
	{
		(void)source;
		tokens.push_back(PA6Token(PA6TokenKind::Fixed, type));
	}

	void emit_simple_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source, SimpleTokenType type)
	{
		(void)spelling;
		(void)source;
		tokens.push_back(PA6Token(PA6TokenKind::Fixed, type));
	}

	void emit_identifier(const std::string& source)
	{
		emit_identifier_with_spelling(0, source);
	}

	void emit_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source)
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
		unsigned int categories = 0;
		if (source.find('C') != std::string::npos)
			categories |= PA6_NAME_CLASS;
		if (source.find('T') != std::string::npos)
			categories |= PA6_NAME_TEMPLATE;
		if (source.find('Y') != std::string::npos)
			categories |= PA6_NAME_TYPEDEF;
		if (source.find('E') != std::string::npos)
			categories |= PA6_NAME_ENUM;
		if (source.find('N') != std::string::npos)
			categories |= PA6_NAME_NAMESPACE;
		tokens.push_back(PA6Token(PA6TokenKind::Identifier,
			SimpleTokenType::OP_SEMICOLON, categories));
	}

	void emit_literal(const std::string& source, const LiteralData& value)
	{
		(void)value;
		if (source == "\"\"")
			tokens.push_back(PA6Token(PA6TokenKind::ST_EMPTYSTR));
		else if (source == "0")
			tokens.push_back(PA6Token(PA6TokenKind::ST_ZERO));
		else
			tokens.push_back(PA6Token(PA6TokenKind::Literal));
	}

	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		(void)value;
		tokens.push_back(PA6Token(PA6TokenKind::Literal));
	}

	void emit_eof()
	{
		if (tokens.empty() || tokens.back().kind != PA6TokenKind::ST_EOF)
			tokens.push_back(PA6Token(PA6TokenKind::ST_EOF));
	}

	std::vector<PA6Token> tokens;
	bool invalid;
};

} // namespace

bool PA6Recognizer::recognize(const PPTokenBuffer& input, std::string* reason) const
{
	PA6TokenCollector collector;
	posttokenize_cpp_tokens(input, collector);
	if (collector.invalid)
	{
		if (reason != NULL)
			*reason = "invalid posttoken";
		return false;
	}
	pa6_internal::PA6Parser parser(collector.tokens);
	return parser.parse(reason);
}
