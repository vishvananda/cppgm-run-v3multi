#include "pa6_recognizer.h"
#include "pa6_parser.h"
#include "cpp_declaration_syntax.h"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

PA6Token adapt_pa6_token(const CppSyntaxToken& token)
{
switch (token.kind)
	{
	case CppSyntaxTokenKind::Fixed:
		return PA6Token(PA6TokenKind::Fixed, token.fixed);
	case CppSyntaxTokenKind::Identifier:
		return PA6Token(PA6TokenKind::Identifier,
			SimpleTokenType::OP_SEMICOLON, token.name_categories);
	case CppSyntaxTokenKind::Literal:
		return PA6Token(PA6TokenKind::Literal);
	case CppSyntaxTokenKind::UserDefinedLiteral:
		// PA6's posttoken contract treats a UDL as its literal mock token.
		return PA6Token(PA6TokenKind::Literal);
	case CppSyntaxTokenKind::EmptyString:
		return PA6Token(PA6TokenKind::ST_EMPTYSTR);
	case CppSyntaxTokenKind::Zero:
		return PA6Token(PA6TokenKind::ST_ZERO);
	case CppSyntaxTokenKind::Override:
		return PA6Token(PA6TokenKind::ST_OVERRIDE);
	case CppSyntaxTokenKind::Final:
		return PA6Token(PA6TokenKind::ST_FINAL);
	case CppSyntaxTokenKind::Rshift1:
		return PA6Token(PA6TokenKind::ST_RSHIFT_1);
	case CppSyntaxTokenKind::Rshift2:
		return PA6Token(PA6TokenKind::ST_RSHIFT_2);
	case CppSyntaxTokenKind::End:
		return PA6Token(PA6TokenKind::ST_EOF);
	}
	return PA6Token(PA6TokenKind::ST_EOF);
}

std::vector<PA6Token> adapt_pa6_tokens(
	const std::vector<CppSyntaxToken>& canonical)
{
	std::vector<PA6Token> result;
	result.reserve(canonical.size());
	for (std::size_t i = 0; i < canonical.size(); ++i)
		result.push_back(adapt_pa6_token(canonical[i]));
	return result;
}

} // namespace

bool PA6Recognizer::recognize(const PPTokenBuffer& input, std::string* reason) const
{
	CppSyntaxTokenCollector collector;
	posttokenize_cpp_tokens(input, collector);
	if (collector.invalid)
	{
		if (reason != NULL)
			*reason = "invalid posttoken";
		return false;
	}
	// The PA7 declaration grammar is a real shared production syntax owner.
	// PA6 uses it directly for the common subset; its existing parser remains
	// the extension path for constructs outside that subset.  A failed shared
	// parse is therefore not a second parse of ordinary PA7 input.
	try
	{
		CppDeclarationSyntaxConsumer consumer;
		CppDeclarationSyntaxParser parser(collector.tokens, consumer, true);
		parser.parse();
		return true;
	}
	catch (const std::runtime_error&)
	{
		// Continue to PA6's broader grammar below.
	}
	std::vector<PA6Token> tokens = adapt_pa6_tokens(collector.tokens);
	pa6_internal::PA6Parser parser(tokens);
	return parser.parse(reason);
}
