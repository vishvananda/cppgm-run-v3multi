#include "posttoken.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "IPPTokenStream.h"
#include "pp_tokenizer.h"

namespace
{

const char* const kFundamentalTypeNames[] =
{
	"signed char",
	"short int",
	"int",
	"long int",
	"long long int",
	"unsigned char",
	"unsigned short int",
	"unsigned int",
	"unsigned long int",
	"unsigned long long int",
	"wchar_t",
	"char",
	"char16_t",
	"char32_t",
	"bool",
	"float",
	"double",
	"long double",
	"void",
	"nullptr_t"
};

const char* const kSimpleTokenTypeNames[] =
{
	"KW_ALIGNAS",
	"KW_ALIGNOF",
	"KW_ASM",
	"KW_AUTO",
	"KW_BOOL",
	"KW_BREAK",
	"KW_CASE",
	"KW_CATCH",
	"KW_CHAR",
	"KW_CHAR16_T",
	"KW_CHAR32_T",
	"KW_CLASS",
	"KW_CONST",
	"KW_CONSTEXPR",
	"KW_CONST_CAST",
	"KW_CONTINUE",
	"KW_DECLTYPE",
	"KW_DEFAULT",
	"KW_DELETE",
	"KW_DO",
	"KW_DOUBLE",
	"KW_DYNAMIC_CAST",
	"KW_ELSE",
	"KW_ENUM",
	"KW_EXPLICIT",
	"KW_EXPORT",
	"KW_EXTERN",
	"KW_FALSE",
	"KW_FLOAT",
	"KW_FOR",
	"KW_FRIEND",
	"KW_GOTO",
	"KW_IF",
	"KW_INLINE",
	"KW_INT",
	"KW_LONG",
	"KW_MUTABLE",
	"KW_NAMESPACE",
	"KW_NEW",
	"KW_NOEXCEPT",
	"KW_NULLPTR",
	"KW_OPERATOR",
	"KW_PRIVATE",
	"KW_PROTECTED",
	"KW_PUBLIC",
	"KW_REGISTER",
	"KW_REINTERPET_CAST",
	"KW_RETURN",
	"KW_SHORT",
	"KW_SIGNED",
	"KW_SIZEOF",
	"KW_STATIC",
	"KW_STATIC_ASSERT",
	"KW_STATIC_CAST",
	"KW_STRUCT",
	"KW_SWITCH",
	"KW_TEMPLATE",
	"KW_THIS",
	"KW_THREAD_LOCAL",
	"KW_THROW",
	"KW_TRUE",
	"KW_TRY",
	"KW_TYPEDEF",
	"KW_TYPEID",
	"KW_TYPENAME",
	"KW_UNION",
	"KW_UNSIGNED",
	"KW_USING",
	"KW_VIRTUAL",
	"KW_VOID",
	"KW_VOLATILE",
	"KW_WCHAR_T",
	"KW_WHILE",
	"OP_LBRACE",
	"OP_RBRACE",
	"OP_LSQUARE",
	"OP_RSQUARE",
	"OP_LPAREN",
	"OP_RPAREN",
	"OP_BOR",
	"OP_XOR",
	"OP_COMPL",
	"OP_AMP",
	"OP_LNOT",
	"OP_SEMICOLON",
	"OP_COLON",
	"OP_DOTS",
	"OP_QMARK",
	"OP_COLON2",
	"OP_DOT",
	"OP_DOTSTAR",
	"OP_PLUS",
	"OP_MINUS",
	"OP_STAR",
	"OP_DIV",
	"OP_MOD",
	"OP_ASS",
	"OP_LT",
	"OP_GT",
	"OP_PLUSASS",
	"OP_MINUSASS",
	"OP_STARASS",
	"OP_DIVASS",
	"OP_MODASS",
	"OP_XORASS",
	"OP_BANDASS",
	"OP_BORASS",
	"OP_LSHIFT",
	"OP_RSHIFT",
	"OP_RSHIFTASS",
	"OP_LSHIFTASS",
	"OP_EQ",
	"OP_NE",
	"OP_LE",
	"OP_GE",
	"OP_LAND",
	"OP_LOR",
	"OP_INC",
	"OP_DEC",
	"OP_COMMA",
	"OP_ARROWSTAR",
	"OP_ARROW"
};

struct SimpleTokenEntry
{
	const char* source;
	SimpleTokenType type;
};

// Keep this fixed vocabulary source-sorted so lookup needs no node-based
// allocation or mutable hash index.  Aliases intentionally share a typed
// SimpleTokenType value.
const SimpleTokenEntry kSimpleTokenEntries[] =
{
	{"!", SimpleTokenType::OP_LNOT},
	{"!=", SimpleTokenType::OP_NE},
	{"%", SimpleTokenType::OP_MOD},
	{"%=", SimpleTokenType::OP_MODASS},
	{"%>", SimpleTokenType::OP_RBRACE},
	{"&", SimpleTokenType::OP_AMP},
	{"&&", SimpleTokenType::OP_LAND},
	{"&=", SimpleTokenType::OP_BANDASS},
	{"(", SimpleTokenType::OP_LPAREN},
	{")", SimpleTokenType::OP_RPAREN},
	{"*", SimpleTokenType::OP_STAR},
	{"*=", SimpleTokenType::OP_STARASS},
	{"+", SimpleTokenType::OP_PLUS},
	{"++", SimpleTokenType::OP_INC},
	{"+=", SimpleTokenType::OP_PLUSASS},
	{",", SimpleTokenType::OP_COMMA},
	{"-", SimpleTokenType::OP_MINUS},
	{"--", SimpleTokenType::OP_DEC},
	{"-=", SimpleTokenType::OP_MINUSASS},
	{"->", SimpleTokenType::OP_ARROW},
	{"->*", SimpleTokenType::OP_ARROWSTAR},
	{".", SimpleTokenType::OP_DOT},
	{".*", SimpleTokenType::OP_DOTSTAR},
	{"...", SimpleTokenType::OP_DOTS},
	{"/", SimpleTokenType::OP_DIV},
	{"/=", SimpleTokenType::OP_DIVASS},
	{":", SimpleTokenType::OP_COLON},
	{"::", SimpleTokenType::OP_COLON2},
	{":>", SimpleTokenType::OP_RSQUARE},
	{";", SimpleTokenType::OP_SEMICOLON},
	{"<", SimpleTokenType::OP_LT},
	{"<%", SimpleTokenType::OP_LBRACE},
	{"<:", SimpleTokenType::OP_LSQUARE},
	{"<<", SimpleTokenType::OP_LSHIFT},
	{"<<=", SimpleTokenType::OP_LSHIFTASS},
	{"<=", SimpleTokenType::OP_LE},
	{"=", SimpleTokenType::OP_ASS},
	{"==", SimpleTokenType::OP_EQ},
	{">", SimpleTokenType::OP_GT},
	{">=", SimpleTokenType::OP_GE},
	{">>", SimpleTokenType::OP_RSHIFT},
	{">>=", SimpleTokenType::OP_RSHIFTASS},
	{"?", SimpleTokenType::OP_QMARK},
	{"[", SimpleTokenType::OP_LSQUARE},
	{"]", SimpleTokenType::OP_RSQUARE},
	{"^", SimpleTokenType::OP_XOR},
	{"^=", SimpleTokenType::OP_XORASS},
	{"alignas", SimpleTokenType::KW_ALIGNAS},
	{"alignof", SimpleTokenType::KW_ALIGNOF},
	{"and", SimpleTokenType::OP_LAND},
	{"and_eq", SimpleTokenType::OP_BANDASS},
	{"asm", SimpleTokenType::KW_ASM},
	{"auto", SimpleTokenType::KW_AUTO},
	{"bitand", SimpleTokenType::OP_AMP},
	{"bitor", SimpleTokenType::OP_BOR},
	{"bool", SimpleTokenType::KW_BOOL},
	{"break", SimpleTokenType::KW_BREAK},
	{"case", SimpleTokenType::KW_CASE},
	{"catch", SimpleTokenType::KW_CATCH},
	{"char", SimpleTokenType::KW_CHAR},
	{"char16_t", SimpleTokenType::KW_CHAR16_T},
	{"char32_t", SimpleTokenType::KW_CHAR32_T},
	{"class", SimpleTokenType::KW_CLASS},
	{"compl", SimpleTokenType::OP_COMPL},
	{"const", SimpleTokenType::KW_CONST},
	{"const_cast", SimpleTokenType::KW_CONST_CAST},
	{"constexpr", SimpleTokenType::KW_CONSTEXPR},
	{"continue", SimpleTokenType::KW_CONTINUE},
	{"decltype", SimpleTokenType::KW_DECLTYPE},
	{"default", SimpleTokenType::KW_DEFAULT},
	{"delete", SimpleTokenType::KW_DELETE},
	{"do", SimpleTokenType::KW_DO},
	{"double", SimpleTokenType::KW_DOUBLE},
	{"dynamic_cast", SimpleTokenType::KW_DYNAMIC_CAST},
	{"else", SimpleTokenType::KW_ELSE},
	{"enum", SimpleTokenType::KW_ENUM},
	{"explicit", SimpleTokenType::KW_EXPLICIT},
	{"export", SimpleTokenType::KW_EXPORT},
	{"extern", SimpleTokenType::KW_EXTERN},
	{"false", SimpleTokenType::KW_FALSE},
	{"float", SimpleTokenType::KW_FLOAT},
	{"for", SimpleTokenType::KW_FOR},
	{"friend", SimpleTokenType::KW_FRIEND},
	{"goto", SimpleTokenType::KW_GOTO},
	{"if", SimpleTokenType::KW_IF},
	{"inline", SimpleTokenType::KW_INLINE},
	{"int", SimpleTokenType::KW_INT},
	{"long", SimpleTokenType::KW_LONG},
	{"mutable", SimpleTokenType::KW_MUTABLE},
	{"namespace", SimpleTokenType::KW_NAMESPACE},
	{"new", SimpleTokenType::KW_NEW},
	{"noexcept", SimpleTokenType::KW_NOEXCEPT},
	{"not", SimpleTokenType::OP_LNOT},
	{"not_eq", SimpleTokenType::OP_NE},
	{"nullptr", SimpleTokenType::KW_NULLPTR},
	{"operator", SimpleTokenType::KW_OPERATOR},
	{"or", SimpleTokenType::OP_LOR},
	{"or_eq", SimpleTokenType::OP_BORASS},
	{"private", SimpleTokenType::KW_PRIVATE},
	{"protected", SimpleTokenType::KW_PROTECTED},
	{"public", SimpleTokenType::KW_PUBLIC},
	{"register", SimpleTokenType::KW_REGISTER},
	{"reinterpret_cast", SimpleTokenType::KW_REINTERPET_CAST},
	{"return", SimpleTokenType::KW_RETURN},
	{"short", SimpleTokenType::KW_SHORT},
	{"signed", SimpleTokenType::KW_SIGNED},
	{"sizeof", SimpleTokenType::KW_SIZEOF},
	{"static", SimpleTokenType::KW_STATIC},
	{"static_assert", SimpleTokenType::KW_STATIC_ASSERT},
	{"static_cast", SimpleTokenType::KW_STATIC_CAST},
	{"struct", SimpleTokenType::KW_STRUCT},
	{"switch", SimpleTokenType::KW_SWITCH},
	{"template", SimpleTokenType::KW_TEMPLATE},
	{"this", SimpleTokenType::KW_THIS},
	{"thread_local", SimpleTokenType::KW_THREAD_LOCAL},
	{"throw", SimpleTokenType::KW_THROW},
	{"true", SimpleTokenType::KW_TRUE},
	{"try", SimpleTokenType::KW_TRY},
	{"typedef", SimpleTokenType::KW_TYPEDEF},
	{"typeid", SimpleTokenType::KW_TYPEID},
	{"typename", SimpleTokenType::KW_TYPENAME},
	{"union", SimpleTokenType::KW_UNION},
	{"unsigned", SimpleTokenType::KW_UNSIGNED},
	{"using", SimpleTokenType::KW_USING},
	{"virtual", SimpleTokenType::KW_VIRTUAL},
	{"void", SimpleTokenType::KW_VOID},
	{"volatile", SimpleTokenType::KW_VOLATILE},
	{"wchar_t", SimpleTokenType::KW_WCHAR_T},
	{"while", SimpleTokenType::KW_WHILE},
	{"xor", SimpleTokenType::OP_XOR},
	{"xor_eq", SimpleTokenType::OP_XORASS},
	{"{", SimpleTokenType::OP_LBRACE},
	{"|", SimpleTokenType::OP_BOR},
	{"|=", SimpleTokenType::OP_BORASS},
	{"||", SimpleTokenType::OP_LOR},
	{"}", SimpleTokenType::OP_RBRACE},
	{"~", SimpleTokenType::OP_COMPL}
};

bool is_hex_digit(char c)
{
	return (c >= '0' && c <= '9') ||
		(c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

unsigned int hex_value(char c)
{
	if (c >= '0' && c <= '9') return static_cast<unsigned int>(c - '0');
	if (c >= 'a' && c <= 'f') return static_cast<unsigned int>(c - 'a' + 10);
	return static_cast<unsigned int>(c - 'A' + 10);
}

struct Utf8Character
{
	std::uint32_t value;
	std::size_t length;
};

bool decode_utf8_at(const std::string& source, std::size_t at,
	Utf8Character* character)
{
	if (at >= source.size())
		return false;

	const unsigned char first = static_cast<unsigned char>(source[at]);
	std::uint32_t value = 0;
	std::size_t length = 0;
	unsigned char second_min = 0x80;
	unsigned char second_max = 0xBF;

	if (first <= 0x7F)
	{
		value = first;
		length = 1;
	}
	else if (first >= 0xC2 && first <= 0xDF)
	{
		value = first & 0x1F;
		length = 2;
	}
	else if (first == 0xE0)
	{
		value = first & 0x0F;
		length = 3;
		second_min = 0xA0;
	}
	else if (first >= 0xE1 && first <= 0xEC)
	{
		value = first & 0x0F;
		length = 3;
	}
	else if (first == 0xED)
	{
		value = first & 0x0F;
		length = 3;
		second_max = 0x9F;
	}
	else if (first >= 0xEE && first <= 0xEF)
	{
		value = first & 0x0F;
		length = 3;
	}
	else if (first == 0xF0)
	{
		value = first & 0x07;
		length = 4;
		second_min = 0x90;
	}
	else if (first >= 0xF1 && first <= 0xF3)
	{
		value = first & 0x07;
		length = 4;
	}
	else if (first == 0xF4)
	{
		value = first & 0x07;
		length = 4;
		second_max = 0x8F;
	}
	else
		return false;

	if (at + length > source.size())
		return false;
	for (std::size_t i = 1; i < length; ++i)
	{
		const unsigned char next = static_cast<unsigned char>(source[at + i]);
		if ((next & 0xC0) != 0x80 ||
			(i == 1 && (next < second_min || next > second_max)))
			return false;
		value = (value << 6) | (next & 0x3F);
	}

	if (value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
		return false;
	character->value = value;
	character->length = length;
	return true;
}

bool valid_unicode_value(std::uint64_t value)
{
	return value <= 0x10FFFF && !(value >= 0xD800 && value <= 0xDFFF);
}

struct Range
{
	std::uint32_t first;
	std::uint32_t last;
};

// These are the same Annex E ranges used by the PA1 identifier boundary.  A
// UDL suffix is checked here only after PA1 has already recognized the
// enclosing preprocessing-token.
const Range kIdentifierE1[] =
{
	{0xA8, 0xA8}, {0xAA, 0xAA}, {0xAD, 0xAD}, {0xAF, 0xAF},
	{0xB2, 0xB5}, {0xB7, 0xBA}, {0xBC, 0xBE}, {0xC0, 0xD6},
	{0xD8, 0xF6}, {0xF8, 0xFF}, {0x100, 0x167F}, {0x1681, 0x180D},
	{0x180F, 0x1FFF}, {0x200B, 0x200D}, {0x202A, 0x202E},
	{0x203F, 0x2040}, {0x2054, 0x2054}, {0x2060, 0x206F},
	{0x2070, 0x218F}, {0x2460, 0x24FF}, {0x2776, 0x2793},
	{0x2C00, 0x2DFF}, {0x2E80, 0x2FFF}, {0x3004, 0x3007},
	{0x3021, 0x302F}, {0x3031, 0x303F}, {0x3040, 0xD7FF},
	{0xF900, 0xFD3D}, {0xFD40, 0xFDCF}, {0xFDF0, 0xFE44},
	{0xFE47, 0xFFFD}, {0x10000, 0x1FFFD}, {0x20000, 0x2FFFD},
	{0x30000, 0x3FFFD}, {0x40000, 0x4FFFD}, {0x50000, 0x5FFFD},
	{0x60000, 0x6FFFD}, {0x70000, 0x7FFFD}, {0x80000, 0x8FFFD},
	{0x90000, 0x9FFFD}, {0xA0000, 0xAFFFD}, {0xB0000, 0xBFFFD},
	{0xC0000, 0xCFFFD}, {0xD0000, 0xDFFFD}, {0xE0000, 0xEFFFD}
};

const Range kIdentifierE2[] =
{
	{0x300, 0x36F}, {0x1DC0, 0x1DFF}, {0x20D0, 0x20FF}, {0xFE20, 0xFE2F}
};

bool in_ranges(std::uint32_t value, const Range* ranges, std::size_t count)
{
	for (std::size_t i = 0; i < count; ++i)
	{
		if (value < ranges[i].first)
			return false;
		if (value <= ranges[i].last)
			return true;
	}
	return false;
}

bool identifier_start(std::uint32_t value)
{
	return (value >= 'a' && value <= 'z') ||
		(value >= 'A' && value <= 'Z') || value == '_' ||
		(in_ranges(value, kIdentifierE1,
			sizeof(kIdentifierE1) / sizeof(kIdentifierE1[0])) &&
		 !in_ranges(value, kIdentifierE2,
			sizeof(kIdentifierE2) / sizeof(kIdentifierE2[0])));
}

bool identifier_body(std::uint32_t value)
{
	return identifier_start(value) ||
		(value >= '0' && value <= '9') ||
		in_ranges(value, kIdentifierE2,
			sizeof(kIdentifierE2) / sizeof(kIdentifierE2[0]));
}

bool valid_ud_suffix(const std::string& suffix)
{
	if (suffix.empty() || suffix[0] != '_')
		return false;

	std::size_t at = 0;
	while (at < suffix.size())
	{
		Utf8Character character;
		if (!decode_utf8_at(suffix, at, &character))
			return false;
		if (at == 0)
		{
			if (character.value != '_')
				return false;
		}
		else if (!identifier_body(character.value))
			return false;
		at += character.length;
	}
	return true;
}

bool starts_with(const std::string& source, const char* prefix)
{
	const std::size_t length = std::strlen(prefix);
	return source.size() >= length && source.compare(0, length, prefix) == 0;
}

void append_little_endian(std::vector<std::uint8_t>* bytes,
	std::uint64_t value, std::size_t width)
{
	for (std::size_t i = 0; i < width; ++i)
		bytes->push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
}

enum class IntegerSuffix
{
	None,
	Unsigned,
	Long,
	LongLong,
	UnsignedLong,
	UnsignedLongLong
};

struct IntegerSyntax
{
	bool valid;
	bool decimal;
	std::uint64_t value;
	bool overflow;
	IntegerSuffix suffix;
	std::string core;

	IntegerSyntax()
		: valid(false), decimal(false), value(0), overflow(false),
		  suffix(IntegerSuffix::None), core()
	{}
};

bool parse_integer_core(const std::string& core, std::uint64_t* value,
	bool* overflow, bool* decimal)
{
	if (core.empty())
		return false;

	unsigned int base = 10;
	std::size_t at = 0;
	if (core.size() >= 2 && core[0] == '0' &&
		(core[1] == 'x' || core[1] == 'X'))
	{
		base = 16;
		at = 2;
		if (at == core.size())
			return false;
	}
	else if (core[0] == '0')
	{
		base = 8;
		// Keep the leading zero in the digit scan so that the zero literal
		// itself has one digit.
		at = 0;
	}
	else if (core[0] >= '1' && core[0] <= '9')
	{
		base = 10;
		at = 0;
	}
	else
		return false;

	std::uint64_t parsed = 0;
	bool did_digit = false;
	bool did_overflow = false;
	for (; at < core.size(); ++at)
	{
		const char c = core[at];
		unsigned int digit = 0;
		if (c >= '0' && c <= '9')
			digit = static_cast<unsigned int>(c - '0');
		else if (base == 16 && is_hex_digit(c))
			digit = hex_value(c);
		else
			return false;
		if (digit >= base)
			return false;
		did_digit = true;
		if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / base)
			did_overflow = true;
		else if (!did_overflow)
			parsed = parsed * base + digit;
	}

	if (!did_digit)
		return false;
	*value = parsed;
	*overflow = did_overflow;
	*decimal = base == 10;
	return true;
}

struct IntegerSuffixPattern
{
	const char* spelling;
	IntegerSuffix suffix;
};

// Long-long is deliberately enumerated with matching case.  Mixed l/L
// spellings such as 0lL are not part of the C++11 suffix grammar.
const IntegerSuffixPattern kIntegerSuffixPatterns[] =
{
	{"ull", IntegerSuffix::UnsignedLongLong},
	{"llu", IntegerSuffix::UnsignedLongLong},
	{"uLL", IntegerSuffix::UnsignedLongLong},
	{"LLu", IntegerSuffix::UnsignedLongLong},
	{"Ull", IntegerSuffix::UnsignedLongLong},
	{"llU", IntegerSuffix::UnsignedLongLong},
	{"ULL", IntegerSuffix::UnsignedLongLong},
	{"LLU", IntegerSuffix::UnsignedLongLong},
	{"ul", IntegerSuffix::UnsignedLong},
	{"lu", IntegerSuffix::UnsignedLong},
	{"uL", IntegerSuffix::UnsignedLong},
	{"Lu", IntegerSuffix::UnsignedLong},
	{"Ul", IntegerSuffix::UnsignedLong},
	{"lU", IntegerSuffix::UnsignedLong},
	{"UL", IntegerSuffix::UnsignedLong},
	{"LU", IntegerSuffix::UnsignedLong},
	{"ll", IntegerSuffix::LongLong},
	{"LL", IntegerSuffix::LongLong},
	{"u", IntegerSuffix::Unsigned},
	{"U", IntegerSuffix::Unsigned},
	{"l", IntegerSuffix::Long},
	{"L", IntegerSuffix::Long},
	{"", IntegerSuffix::None}
};

bool ends_with(const std::string& source, const char* suffix)
{
	const std::size_t length = std::strlen(suffix);
	return source.size() >= length &&
		source.compare(source.size() - length, length, suffix) == 0;
}

bool parse_integer_literal(const std::string& source,
	IntegerSyntax* result, bool allow_suffix)
{
	for (std::size_t i = 0; i < sizeof(kIntegerSuffixPatterns) /
		sizeof(kIntegerSuffixPatterns[0]); ++i)
	{
		const IntegerSuffixPattern& pattern = kIntegerSuffixPatterns[i];
		if (!allow_suffix && pattern.suffix != IntegerSuffix::None)
			continue;
		if (!ends_with(source, pattern.spelling))
			continue;
		const std::size_t suffix_length = std::strlen(pattern.spelling);
		const std::string core = source.substr(0, source.size() - suffix_length);
		std::uint64_t value = 0;
		bool overflow = false;
		bool decimal = false;
		if (!parse_integer_core(core, &value, &overflow, &decimal))
			continue;
		result->valid = true;
		result->decimal = decimal;
		result->value = value;
		result->overflow = overflow;
		result->suffix = pattern.suffix;
		result->core = core;
		return true;
	}
	return false;
}

bool parse_float_core(const std::string& source)
{
	if (source.empty())
		return false;

	std::size_t at = 0;
	std::size_t before_dot = 0;
	while (at < source.size() && source[at] >= '0' && source[at] <= '9')
		++at;
	before_dot = at;

	bool has_dot = false;
	std::size_t after_dot = 0;
	if (at < source.size() && source[at] == '.')
	{
		has_dot = true;
		++at;
		const std::size_t fraction_begin = at;
		while (at < source.size() && source[at] >= '0' && source[at] <= '9')
			++at;
		after_dot = at - fraction_begin;
		if (before_dot == 0 && after_dot == 0)
			return false;
	}

	if (before_dot == 0 && !has_dot)
		return false;

	bool has_exponent = false;
	if (at < source.size() && (source[at] == 'e' || source[at] == 'E'))
	{
		has_exponent = true;
		++at;
		if (at < source.size() && (source[at] == '+' || source[at] == '-'))
			++at;
		const std::size_t exponent_begin = at;
		while (at < source.size() && source[at] >= '0' && source[at] <= '9')
			++at;
		if (at == exponent_begin)
			return false;
	}

	if (at != source.size())
		return false;
	// A digit-sequence without a decimal point is a floating literal only
	// when it has an exponent part.
	return has_dot || has_exponent;
}

struct FloatingSyntax
{
	bool valid;
	char suffix;
	std::string core;

	FloatingSyntax() : valid(false), suffix(0), core() {}
};

bool parse_floating_literal(const std::string& source,
	FloatingSyntax* result, bool allow_suffix)
{
	std::size_t suffix_length = 0;
	char suffix = 0;
	if (!source.empty())
	{
		const char last = source[source.size() - 1];
		if (last == 'f' || last == 'F' || last == 'l' || last == 'L')
		{
			suffix_length = 1;
			suffix = last;
		}
	}
	if (!allow_suffix && suffix_length != 0)
		return false;

	const std::string core = source.substr(0, source.size() - suffix_length);
	if (!parse_float_core(core))
		return false;
	result->valid = true;
	result->suffix = suffix;
	result->core = core;
	return true;
}

struct NumericResult
{
	bool valid;
	bool user_defined;
	UserDefinedLiteralKind user_kind;
	std::string prefix;
	std::string suffix;
	IntegerSyntax integer;
	FloatingSyntax floating;

	NumericResult()
		: valid(false), user_defined(false),
		  user_kind(UserDefinedLiteralKind::Integer), prefix(), suffix(),
		  integer(), floating()
	{}
};

NumericResult analyze_number(const std::string& source)
{
	NumericResult result;
	const std::size_t underscore = source.find('_');
	if (underscore != std::string::npos)
	{
		const std::string prefix = source.substr(0, underscore);
		const std::string suffix = source.substr(underscore);
		if (!valid_ud_suffix(suffix))
			return result;

		IntegerSyntax integer;
		if (parse_integer_literal(prefix, &integer, false))
		{
			result.valid = true;
			result.user_defined = true;
			result.user_kind = UserDefinedLiteralKind::Integer;
			result.prefix = prefix;
			result.suffix = suffix;
			result.integer = integer;
			return result;
		}

		FloatingSyntax floating;
		if (parse_floating_literal(prefix, &floating, false))
		{
			result.valid = true;
			result.user_defined = true;
			result.user_kind = UserDefinedLiteralKind::Floating;
			result.prefix = prefix;
			result.suffix = suffix;
			result.floating = floating;
		}
		return result;
	}

	if (parse_integer_literal(source, &result.integer, true))
	{
		result.valid = true;
		return result;
	}
	if (parse_floating_literal(source, &result.floating, true))
	{
		result.valid = true;
		return result;
	}
	return result;
}

struct IntegerCandidate
{
	FundamentalType type;
	std::uint64_t maximum;
	std::size_t width;
};

IntegerCandidate candidate(FundamentalType type)
{
	IntegerCandidate result;
	result.type = type;
	result.width = (type == FundamentalType::Int ||
		type == FundamentalType::UnsignedInt) ? 4 : 8;
	if (type == FundamentalType::Int || type == FundamentalType::LongInt ||
		type == FundamentalType::LongLongInt)
		result.maximum = type == FundamentalType::Int ? 0x7FFFFFFFULL :
			0x7FFFFFFFFFFFFFFFULL;
	else
		result.maximum = result.width == 4 ? 0xFFFFFFFFULL :
			std::numeric_limits<std::uint64_t>::max();
	return result;
}

bool choose_integer_type(const IntegerSyntax& syntax,
	IntegerCandidate* result)
{
	FundamentalType types[6];
	std::size_t type_count = 0;
	if (syntax.decimal)
	{
		switch (syntax.suffix)
		{
		case IntegerSuffix::None:
			types[type_count++] = FundamentalType::Int;
			types[type_count++] = FundamentalType::LongInt;
			types[type_count++] = FundamentalType::LongLongInt;
			break;
		case IntegerSuffix::Unsigned:
			types[type_count++] = FundamentalType::UnsignedInt;
			types[type_count++] = FundamentalType::UnsignedLongInt;
			types[type_count++] = FundamentalType::UnsignedLongLongInt;
			break;
		case IntegerSuffix::Long:
			types[type_count++] = FundamentalType::LongInt;
			types[type_count++] = FundamentalType::LongLongInt;
			break;
		case IntegerSuffix::LongLong:
			types[type_count++] = FundamentalType::LongLongInt;
			break;
		case IntegerSuffix::UnsignedLong:
			types[type_count++] = FundamentalType::UnsignedLongInt;
			types[type_count++] = FundamentalType::UnsignedLongLongInt;
			break;
		case IntegerSuffix::UnsignedLongLong:
			types[type_count++] = FundamentalType::UnsignedLongLongInt;
			break;
		}
	}
	else
	{
		switch (syntax.suffix)
		{
		case IntegerSuffix::None:
			types[type_count++] = FundamentalType::Int;
			types[type_count++] = FundamentalType::UnsignedInt;
			types[type_count++] = FundamentalType::LongInt;
			types[type_count++] = FundamentalType::UnsignedLongInt;
			types[type_count++] = FundamentalType::LongLongInt;
			types[type_count++] = FundamentalType::UnsignedLongLongInt;
			break;
		case IntegerSuffix::Unsigned:
			types[type_count++] = FundamentalType::UnsignedInt;
			types[type_count++] = FundamentalType::UnsignedLongInt;
			types[type_count++] = FundamentalType::UnsignedLongLongInt;
			break;
		case IntegerSuffix::Long:
			types[type_count++] = FundamentalType::LongInt;
			types[type_count++] = FundamentalType::UnsignedLongInt;
			types[type_count++] = FundamentalType::LongLongInt;
			types[type_count++] = FundamentalType::UnsignedLongLongInt;
			break;
		case IntegerSuffix::LongLong:
			types[type_count++] = FundamentalType::LongLongInt;
			types[type_count++] = FundamentalType::UnsignedLongLongInt;
			break;
		case IntegerSuffix::UnsignedLong:
			types[type_count++] = FundamentalType::UnsignedLongInt;
			types[type_count++] = FundamentalType::UnsignedLongLongInt;
			break;
		case IntegerSuffix::UnsignedLongLong:
			types[type_count++] = FundamentalType::UnsignedLongLongInt;
			break;
		}
	}

	if (syntax.overflow)
		return false;
	for (std::size_t i = 0; i < type_count; ++i)
	{
		const IntegerCandidate possible = candidate(types[i]);
		if (syntax.value <= possible.maximum)
		{
			*result = possible;
			return true;
		}
	}
	return false;
}

LiteralData floating_value(const std::string& source,
	const FloatingSyntax& syntax)
{
	LiteralData result;
	if (syntax.suffix == 'f' || syntax.suffix == 'F')
	{
		const float value = PA2Decode_float(source);
		result.type = FundamentalType::Float;
		result.bytes.resize(sizeof(value));
		std::memcpy(result.bytes.data(), &value, sizeof(value));
	}
	else if (syntax.suffix == 'l' || syntax.suffix == 'L')
	{
		const long double value = PA2Decode_long_double(source);
		result.type = FundamentalType::LongDouble;
		result.bytes.resize(sizeof(value));
		std::memcpy(result.bytes.data(), &value, sizeof(value));
	}
	else
	{
		const double value = PA2Decode_double(source);
		result.type = FundamentalType::Double;
		result.bytes.resize(sizeof(value));
		std::memcpy(result.bytes.data(), &value, sizeof(value));
	}
	return result;
}

bool parse_escape(const std::string& source, std::size_t at,
	std::size_t* end, std::uint64_t* value, bool* numeric)
{
	if (at + 1 >= source.size() || source[at] != '\\')
		return false;
	const char next = source[at + 1];
	*numeric = false;
	switch (next)
	{
	case '\'': *value = '\''; *end = at + 2; return true;
	case '"': *value = '"'; *end = at + 2; return true;
	case '?': *value = '?'; *end = at + 2; return true;
	case '\\': *value = '\\'; *end = at + 2; return true;
	case 'a': *value = 7; *end = at + 2; return true;
	case 'b': *value = 8; *end = at + 2; return true;
	case 'f': *value = 12; *end = at + 2; return true;
	case 'n': *value = 10; *end = at + 2; return true;
	case 'r': *value = 13; *end = at + 2; return true;
	case 't': *value = 9; *end = at + 2; return true;
	case 'v': *value = 11; *end = at + 2; return true;
	default: break;
	}

	if (next >= '0' && next <= '7')
	{
		std::size_t cursor = at + 1;
		std::uint64_t parsed = 0;
		for (std::size_t count = 0; count < 3 && cursor < source.size(); ++count)
		{
			const char c = source[cursor];
			if (c < '0' || c > '7')
				break;
			parsed = parsed * 8 + static_cast<unsigned int>(c - '0');
			++cursor;
		}
		*value = parsed;
		*end = cursor;
		*numeric = true;
		return true;
	}

	if (next == 'x')
	{
		std::size_t cursor = at + 2;
		if (cursor == source.size() || !is_hex_digit(source[cursor]))
			return false;
		std::uint64_t parsed = 0;
		bool overflow = false;
		while (cursor < source.size() && is_hex_digit(source[cursor]))
		{
			const unsigned int digit = hex_value(source[cursor]);
			if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 16)
				overflow = true;
			else if (!overflow)
				parsed = parsed * 16 + digit;
			++cursor;
		}
		if (overflow)
			return false;
		*value = parsed;
		*end = cursor;
		*numeric = true;
		return true;
	}
	return false;
}

enum class StringEncoding
{
	Ordinary,
	Utf8,
	Utf16,
	Utf32,
	Wchar32
};

enum class StringElementKind
{
	CodePoint,
	NumericCodeUnit
};

struct StringElement
{
	StringElementKind kind;
	std::uint64_t value;
};

struct StringPart
{
	std::string source;
	std::string literal_source;
	StringEncoding encoding;
	std::string suffix;
	bool valid;
	std::vector<StringElement> elements;

	StringPart()
		: source(), literal_source(), encoding(StringEncoding::Ordinary),
		  suffix(), valid(false),
		  elements()
	{}
};

void add_codepoint_element(StringPart* part, std::uint64_t value)
{
	StringElement element;
	element.kind = StringElementKind::CodePoint;
	element.value = value;
	part->elements.push_back(element);
}

void add_numeric_element(StringPart* part, std::uint64_t value)
{
	StringElement element;
	element.kind = StringElementKind::NumericCodeUnit;
	element.value = value;
	part->elements.push_back(element);
}

bool decode_raw_content(const std::string& content, StringPart* part)
{
	std::size_t at = 0;
	while (at < content.size())
	{
		Utf8Character character;
		if (!decode_utf8_at(content, at, &character))
			return false;
		add_codepoint_element(part, character.value);
		at += character.length;
	}
	return true;
}

bool parse_string_part(const std::string& source, StringPart* part)
{
	part->source = source;
	part->literal_source.clear();
	part->valid = false;
	part->elements.clear();

	std::size_t quote = std::string::npos;
	bool raw = false;
	if (starts_with(source, "u8R\""))
	{
		part->encoding = StringEncoding::Utf8;
		quote = 3;
		raw = true;
	}
	else if (starts_with(source, "uR\""))
	{
		part->encoding = StringEncoding::Utf16;
		quote = 2;
		raw = true;
	}
	else if (starts_with(source, "UR\""))
	{
		part->encoding = StringEncoding::Utf32;
		quote = 2;
		raw = true;
	}
	else if (starts_with(source, "LR\""))
	{
		part->encoding = StringEncoding::Wchar32;
		quote = 2;
		raw = true;
	}
	else if (starts_with(source, "R\""))
	{
		part->encoding = StringEncoding::Ordinary;
		quote = 1;
		raw = true;
	}
	else if (starts_with(source, "u8\""))
	{
		part->encoding = StringEncoding::Utf8;
		quote = 2;
	}
	else if (starts_with(source, "u\""))
	{
		part->encoding = StringEncoding::Utf16;
		quote = 1;
	}
	else if (starts_with(source, "U\""))
	{
		part->encoding = StringEncoding::Utf32;
		quote = 1;
	}
	else if (starts_with(source, "L\""))
	{
		part->encoding = StringEncoding::Wchar32;
		quote = 1;
	}
	else if (starts_with(source, "\""))
	{
		part->encoding = StringEncoding::Ordinary;
		quote = 0;
	}
	else
		return false;

	std::size_t suffix_begin = std::string::npos;
	if (raw)
	{
		const std::size_t open = source.find('(', quote + 1);
		if (open == std::string::npos)
			return false;
		const std::string delimiter = source.substr(quote + 1,
			open - quote - 1);
		const std::string closing = ")" + delimiter + "\"";
		const std::size_t close = source.find(closing, open + 1);
		if (close == std::string::npos)
			return false;
		const std::string content = source.substr(open + 1,
			close - open - 1);
		if (!decode_raw_content(content, part))
			return false;
		suffix_begin = close + closing.size();
	}
	else
	{
		std::size_t at = quote + 1;
		bool closed = false;
		while (at < source.size())
		{
			if (source[at] == '\\')
			{
				std::size_t end = 0;
				std::uint64_t value = 0;
				bool numeric = false;
				if (!parse_escape(source, at, &end, &value, &numeric))
					return false;
				if (numeric)
					add_numeric_element(part, value);
				else
				{
					if (!valid_unicode_value(value))
						return false;
					add_codepoint_element(part, value);
				}
				at = end;
				continue;
			}

			Utf8Character character;
			if (!decode_utf8_at(source, at, &character))
				return false;
			if (character.value == '"')
			{
				at += character.length;
				suffix_begin = at;
				closed = true;
				break;
			}
			add_codepoint_element(part, character.value);
			at += character.length;
		}
		if (!closed)
			return false;
	}

	if (suffix_begin == std::string::npos || suffix_begin > source.size())
		return false;
	part->literal_source = source.substr(0, suffix_begin);
	part->suffix = source.substr(suffix_begin);
	if (!part->suffix.empty() && !valid_ud_suffix(part->suffix))
		return false;
	part->valid = true;
	return true;
}

std::size_t encoding_width(StringEncoding encoding)
{
	switch (encoding)
	{
	case StringEncoding::Ordinary:
	case StringEncoding::Utf8:
		return 1;
	case StringEncoding::Utf16:
		return 2;
	case StringEncoding::Utf32:
	case StringEncoding::Wchar32:
		return 4;
	}
	return 1;
}

FundamentalType encoding_type(StringEncoding encoding)
{
	switch (encoding)
	{
	case StringEncoding::Ordinary:
	case StringEncoding::Utf8:
		return FundamentalType::Char;
	case StringEncoding::Utf16:
		return FundamentalType::Char16T;
	case StringEncoding::Utf32:
		return FundamentalType::Char32T;
	case StringEncoding::Wchar32:
		return FundamentalType::WcharT;
	}
	return FundamentalType::Char;
}

struct EncodedCodePoint
{
	std::uint32_t units[4];
	std::size_t count;
};

bool encode_codepoint(StringEncoding encoding, std::uint32_t value,
	EncodedCodePoint* encoded)
{
	if (!valid_unicode_value(value))
		return false;
	if (encoding == StringEncoding::Ordinary || encoding == StringEncoding::Utf8)
	{
		if (value <= 0x7F)
		{
			encoded->count = 1;
			encoded->units[0] = value;
		}
		else if (value <= 0x7FF)
		{
			encoded->count = 2;
			encoded->units[0] = 0xC0 | (value >> 6);
			encoded->units[1] = 0x80 | (value & 0x3F);
		}
		else if (value <= 0xFFFF)
		{
			encoded->count = 3;
			encoded->units[0] = 0xE0 | (value >> 12);
			encoded->units[1] = 0x80 | ((value >> 6) & 0x3F);
			encoded->units[2] = 0x80 | (value & 0x3F);
		}
		else
		{
			encoded->count = 4;
			encoded->units[0] = 0xF0 | (value >> 18);
			encoded->units[1] = 0x80 | ((value >> 12) & 0x3F);
			encoded->units[2] = 0x80 | ((value >> 6) & 0x3F);
			encoded->units[3] = 0x80 | (value & 0x3F);
		}
		return true;
	}

	if (encoding == StringEncoding::Utf16)
	{
		if (value <= 0xFFFF)
		{
			encoded->count = 1;
			encoded->units[0] = value;
		}
		else
		{
			const std::uint32_t adjusted = value - 0x10000;
			encoded->count = 2;
			encoded->units[0] = 0xD800 | (adjusted >> 10);
			encoded->units[1] = 0xDC00 | (adjusted & 0x3FF);
		}
		return true;
	}

	encoded->count = 1;
	encoded->units[0] = value;
	return true;
}

std::size_t encoded_element_bytes(const StringElement& element,
	StringEncoding encoding)
{
	if (element.kind == StringElementKind::NumericCodeUnit)
		return encoding_width(encoding);

	EncodedCodePoint encoded;
	if (!encode_codepoint(encoding, static_cast<std::uint32_t>(element.value),
		&encoded))
		return 0;
	return encoded.count * encoding_width(encoding);
}

void append_unit(std::vector<std::uint8_t>* bytes, std::size_t* count,
	std::uint64_t value, std::size_t width)
{
	append_little_endian(bytes, value, width);
	++*count;
}

bool append_codepoint(std::vector<std::uint8_t>* bytes, std::size_t* count,
	StringEncoding encoding, std::uint32_t value)
{
	EncodedCodePoint encoded;
	if (!encode_codepoint(encoding, value, &encoded))
		return false;
	const std::size_t width = encoding_width(encoding);
	for (std::size_t i = 0; i < encoded.count; ++i)
		append_unit(bytes, count, encoded.units[i], width);
	return true;
}

bool append_string_part(const StringPart& part, StringEncoding encoding,
	std::vector<std::uint8_t>* bytes, std::size_t* count)
{
	const std::uint64_t maximum = encoding_width(encoding) == 1 ? 0xFFULL :
		encoding_width(encoding) == 2 ? 0xFFFFULL : 0xFFFFFFFFULL;
	for (std::size_t i = 0; i < part.elements.size(); ++i)
	{
		const StringElement& element = part.elements[i];
		if (element.kind == StringElementKind::NumericCodeUnit)
		{
			if (element.value > maximum)
				return false;
			append_unit(bytes, count, element.value, encoding_width(encoding));
		}
		else if (!append_codepoint(bytes, count, encoding,
			static_cast<std::uint32_t>(element.value)))
			return false;
	}
	return true;
}

struct CharacterResult
{
	bool valid;
	bool user_defined;
	std::string suffix;
	FundamentalType type;
	std::uint32_t value;

	CharacterResult()
		: valid(false), user_defined(false), suffix(),
		  type(FundamentalType::Char), value(0)
	{}
};

CharacterResult analyze_character(const std::string& source)
{
	CharacterResult result;
	std::size_t quote = std::string::npos;
	FundamentalType prefixed_type = FundamentalType::Char;
	bool ordinary = false;
	if (starts_with(source, "u'"))
	{
		quote = 1;
		prefixed_type = FundamentalType::Char16T;
	}
	else if (starts_with(source, "U'"))
	{
		quote = 1;
		prefixed_type = FundamentalType::Char32T;
	}
	else if (starts_with(source, "L'"))
	{
		quote = 1;
		prefixed_type = FundamentalType::WcharT;
	}
	else if (starts_with(source, "'"))
	{
		quote = 0;
		ordinary = true;
	}
	else
		return result;

	std::size_t at = quote + 1;
	std::size_t close = std::string::npos;
	std::vector<std::uint64_t> values;
	while (at < source.size())
	{
		if (source[at] == '\\')
		{
			std::size_t end = 0;
			std::uint64_t value = 0;
			bool numeric = false;
			if (!parse_escape(source, at, &end, &value, &numeric) ||
				!valid_unicode_value(value))
				return result;
			values.push_back(value);
			at = end;
			continue;
		}

		Utf8Character character;
		if (!decode_utf8_at(source, at, &character))
			return result;
		if (character.value == '\'')
		{
			close = at;
			at += character.length;
			break;
		}
		values.push_back(character.value);
		at += character.length;
	}
	if (close == std::string::npos || values.size() != 1)
		return result;

	result.suffix = source.substr(at);
	if (!result.suffix.empty())
	{
		if (!valid_ud_suffix(result.suffix))
			return result;
		result.user_defined = true;
	}

	const std::uint64_t value = values[0];
	if (ordinary)
	{
		result.type = value <= 127 ? FundamentalType::Char : FundamentalType::Int;
	}
	else
	{
		if (prefixed_type == FundamentalType::Char16T && value > 0xFFFF)
			return result;
		result.type = prefixed_type;
	}
	result.value = static_cast<std::uint32_t>(value);
	result.valid = true;
	return result;
}

LiteralData character_value(const CharacterResult& character)
{
	LiteralData result;
	result.type = character.type;
	const std::size_t width = character.type == FundamentalType::Char ? 1 :
		(character.type == FundamentalType::Char16T ? 2 : 4);
	append_little_endian(&result.bytes, character.value, width);
	return result;
}

class PostTokenStream : public IPPTokenStream
{
public:
	PostTokenStream(IPostTokenOutput& output, bool line_aware)
		: output_(output), line_aware_(line_aware), pending_strings_(),
		  operator_pending_(false)
	{}

	void emit_whitespace_sequence() {}

	void emit_new_line()
	{
		if (!line_aware_)
			return;
		flush_strings();
		operator_pending_ = false;
		output_.emit_new_line();
	}

	void emit_header_name(const std::string& data)
	{
		flush_strings();
		operator_pending_ = false;
		output_.emit_invalid(data);
	}

	void emit_identifier(const std::string& data)
	{
		flush_strings();
		operator_pending_ = false;
		SimpleTokenType type;
		if (lookup_simple_token_type(data, &type))
		{
			output_.emit_simple_identifier(data, type);
			operator_pending_ = type == SimpleTokenType::KW_OPERATOR;
		}
		else
			output_.emit_identifier(data);
	}

	void emit_identifier_as_preprocessing_op_or_punc(
		const std::string& data)
	{
		// Preserve the phase-3 identifier-origin fact for typed post-token
		// consumers while retaining the ordinary PA1 event for other streams.
		emit_identifier(data);
	}

	void emit_pp_number(const std::string& data)
	{
		flush_strings();
		operator_pending_ = false;
		const NumericResult result = analyze_number(data);
		if (!result.valid)
		{
			output_.emit_invalid(data);
			return;
		}
		if (result.user_defined)
		{
			UserDefinedLiteralData value;
			value.source = data;
			value.suffix = result.suffix;
			value.kind = result.user_kind;
			value.prefix = result.prefix;
			output_.emit_user_defined_literal(value);
			return;
		}
		LiteralData value;
		if (result.integer.valid)
		{
			IntegerCandidate selected;
			if (!choose_integer_type(result.integer, &selected))
			{
				output_.emit_invalid(data);
				return;
			}
			value.type = selected.type;
			append_little_endian(&value.bytes, result.integer.value, selected.width);
		}
		else
			value = floating_value(data, result.floating);
		output_.emit_literal(data, value);
	}

	void emit_character_literal(const std::string& data)
	{
		flush_strings();
		operator_pending_ = false;
		emit_character(data);
	}

	void emit_user_defined_character_literal(const std::string& data)
	{
		flush_strings();
		operator_pending_ = false;
		emit_character(data);
	}

	void emit_string_literal(const std::string& data)
	{
		append_string(data);
	}

	void emit_user_defined_string_literal(const std::string& data)
	{
		append_string(data);
	}

	void emit_preprocessing_op_or_punc(const std::string& data)
	{
		flush_strings();
		operator_pending_ = false;
		if (data == "#" || data == "##" || data == "%:" || data == "%:%:")
		{
			output_.emit_invalid(data);
			return;
		}
		SimpleTokenType type;
		if (lookup_simple_token_type(data, &type))
			output_.emit_simple(data, type);
		else
			output_.emit_invalid(data);
	}

	void emit_non_whitespace_char(const std::string& data)
	{
		flush_strings();
		operator_pending_ = false;
		output_.emit_invalid(data);
	}

	void emit_eof()
	{
		flush_strings();
		operator_pending_ = false;
		output_.emit_eof();
	}

private:
	IPostTokenOutput& output_;
	bool line_aware_;
	std::vector<StringPart> pending_strings_;
	bool operator_pending_;

	void append_string(const std::string& data)
	{
		StringPart part;
		parse_string_part(data, &part);
		if (operator_pending_)
		{
			// PA1 recognizes the adjacent identifier as part of a
			// user-defined-string preprocessing-token.  In the special
			// operator""suffix spelling, however, PA2 exposes the empty
			// string token and the suffix identifier separately.
			if (!part.suffix.empty() && part.suffix[0] != '_' &&
				!part.literal_source.empty())
			{
				LiteralData literal;
				literal.type = encoding_type(part.encoding);
				if (append_string_part(part, part.encoding, &literal.bytes,
					&literal.element_count))
				{
					append_unit(&literal.bytes, &literal.element_count, 0,
						encoding_width(part.encoding));
					output_.emit_literal(part.literal_source, literal);
					output_.emit_identifier(part.suffix);
					operator_pending_ = false;
					return;
				}
			}
			operator_pending_ = false;
		}
		pending_strings_.push_back(part);
	}

	void emit_character(const std::string& data)
	{
		const CharacterResult result = analyze_character(data);
		if (!result.valid)
		{
			output_.emit_invalid(data);
			return;
		}
		if (!result.user_defined)
		{
			output_.emit_literal(data, character_value(result));
			return;
		}
		UserDefinedLiteralData value;
		value.source = data;
		value.suffix = result.suffix;
		value.kind = UserDefinedLiteralKind::Character;
		value.value = character_value(result);
		output_.emit_user_defined_literal(value);
	}

	void flush_strings()
	{
		if (pending_strings_.empty())
			return;

		std::size_t source_size = 0;
		for (std::size_t i = 0; i < pending_strings_.size(); ++i)
			source_size += pending_strings_[i].source.size();
		const std::string separator = " ";
		if (pending_strings_.size() > 1)
			source_size += pending_strings_.size() - 1;
		std::string source;
		source.reserve(source_size);
		for (std::size_t i = 0; i < pending_strings_.size(); ++i)
		{
			if (i != 0)
				source += separator;
			source += pending_strings_[i].source;
		}

		bool valid = true;
		bool has_encoding = false;
		StringEncoding encoding = StringEncoding::Ordinary;
		std::string suffix;
		bool has_suffix = false;
		for (std::size_t i = 0; i < pending_strings_.size(); ++i)
		{
			const StringPart& part = pending_strings_[i];
			if (!part.valid)
				valid = false;
			if (part.encoding != StringEncoding::Ordinary)
			{
				if (!has_encoding)
				{
					has_encoding = true;
					encoding = part.encoding;
				}
				else if (encoding != part.encoding)
					valid = false;
			}
			if (!part.suffix.empty())
			{
				if (!has_suffix)
				{
					has_suffix = true;
					suffix = part.suffix;
				}
				else if (suffix != part.suffix)
					valid = false;
			}
		}

		if (valid)
		{
			std::size_t estimated_bytes = encoding_width(encoding);
			for (std::size_t i = 0; i < pending_strings_.size(); ++i)
			{
				const StringPart& part = pending_strings_[i];
				for (std::size_t j = 0; j < part.elements.size(); ++j)
					estimated_bytes += encoded_element_bytes(part.elements[j], encoding);
			}
			LiteralData value;
			value.type = encoding_type(encoding);
			value.element_count = 0;
			value.bytes.reserve(estimated_bytes);
			for (std::size_t i = 0; i < pending_strings_.size(); ++i)
			{
				if (!append_string_part(pending_strings_[i], encoding,
					&value.bytes, &value.element_count))
				{
					valid = false;
					break;
				}
			}
			if (valid)
				append_unit(&value.bytes, &value.element_count, 0,
					encoding_width(encoding));

			if (valid)
			{
				if (!has_suffix)
					output_.emit_literal(source, value);
				else
				{
					UserDefinedLiteralData user_defined;
					user_defined.source = source;
					user_defined.suffix = suffix;
					user_defined.kind = UserDefinedLiteralKind::String;
					user_defined.value = value;
					output_.emit_user_defined_literal(user_defined);
				}
			}
		}

		if (!valid)
			output_.emit_invalid(source);
		pending_strings_.clear();
	}
};

} // namespace

const char* fundamental_type_name(FundamentalType type)
{
	const std::size_t index = static_cast<std::size_t>(type);
	return index < sizeof(kFundamentalTypeNames) /
		sizeof(kFundamentalTypeNames[0]) ? kFundamentalTypeNames[index] : "";
}

const char* simple_token_type_name(SimpleTokenType type)
{
	const std::size_t index = static_cast<std::size_t>(type);
	return index < sizeof(kSimpleTokenTypeNames) /
		sizeof(kSimpleTokenTypeNames[0]) ? kSimpleTokenTypeNames[index] : "";
}

bool lookup_simple_token_type(const std::string& source,
	SimpleTokenType* type)
{
	std::size_t first = 0;
	std::size_t last = sizeof(kSimpleTokenEntries) /
		sizeof(kSimpleTokenEntries[0]);
	while (first < last)
	{
		const std::size_t middle = first + (last - first) / 2;
		const int comparison = source.compare(kSimpleTokenEntries[middle].source);
		if (comparison == 0)
		{
			*type = kSimpleTokenEntries[middle].type;
			return true;
		}
		if (comparison < 0)
			last = middle;
		else
			first = middle + 1;
	}
	return false;
}

float PA2Decode_float(const std::string& source)
{
	std::istringstream stream(source);
	float value;
	stream >> value;
	return value;
}

double PA2Decode_double(const std::string& source)
{
	std::istringstream stream(source);
	double value;
	stream >> value;
	return value;
}

long double PA2Decode_long_double(const std::string& source)
{
	std::istringstream stream(source);
	long double value;
	stream >> value;
	return value;
}

void posttokenize_cpp_source(const std::string& source,
	IPostTokenOutput& output)
{
	PostTokenStream stream(output, false);
	tokenize_cpp_source(source, stream);
}

void posttokenize_cpp_source_by_line(const std::string& source,
	IPostTokenOutput& output)
{
	PostTokenStream stream(output, true);
	tokenize_cpp_source(source, stream);
}
