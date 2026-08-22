#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "posttoken.h"

// PA6's parser consumes this canonical token model.  Fixed vocabulary is
// represented by enums; arbitrary identifier spelling is reduced to the mock
// lookup categories at the posttoken boundary.  Literal payloads are already
// decoded by PA2, so the recognizer needs only their grammar category and the
// two PA6 spelling-sensitive special cases.
enum class PA6TokenKind
{
	Fixed,
	Identifier,
	Literal,
	ST_EMPTYSTR,
	ST_ZERO,
	ST_OVERRIDE,
	ST_FINAL,
	ST_RSHIFT_1,
	ST_RSHIFT_2,
	ST_EOF
};

enum PA6NameCategory
{
	PA6_NAME_CLASS = 1u << 0,
	PA6_NAME_TEMPLATE = 1u << 1,
	PA6_NAME_TYPEDEF = 1u << 2,
	PA6_NAME_ENUM = 1u << 3,
	PA6_NAME_NAMESPACE = 1u << 4
};

struct PA6Token
{
	PA6TokenKind kind;
	SimpleTokenType fixed;
	unsigned int name_categories;

	PA6Token(PA6TokenKind kind = PA6TokenKind::ST_EOF,
		SimpleTokenType fixed = SimpleTokenType::OP_SEMICOLON,
		unsigned int name_categories = 0)
		: kind(kind), fixed(fixed), name_categories(name_categories)
	{}
};

class PA6Recognizer
{
public:
	// Syntax failure is reported as false.  The optional reason is intended
	// for diagnostics only; it is not part of recog's output contract.
	bool recognize(const PPTokenBuffer& input, std::string* reason = NULL) const;
};
