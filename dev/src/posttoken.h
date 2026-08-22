#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "IPPTokenStream.h"

// The posttoken stage owns these typed facts.  The command-line program only
// renders them; consumers that need a different presentation can implement
// IPostTokenOutput without reparsing the rendered stream.
enum class FundamentalType
{
	SignedChar,
	ShortInt,
	Int,
	LongInt,
	LongLongInt,
	UnsignedChar,
	UnsignedShortInt,
	UnsignedInt,
	UnsignedLongInt,
	UnsignedLongLongInt,
	WcharT,
	Char,
	Char16T,
	Char32T,
	Bool,
	Float,
	Double,
	LongDouble,
	Void,
	NullptrT
};

const char* fundamental_type_name(FundamentalType type);

enum class SimpleTokenType
{
	KW_ALIGNAS,
	KW_ALIGNOF,
	KW_ASM,
	KW_AUTO,
	KW_BOOL,
	KW_BREAK,
	KW_CASE,
	KW_CATCH,
	KW_CHAR,
	KW_CHAR16_T,
	KW_CHAR32_T,
	KW_CLASS,
	KW_CONST,
	KW_CONSTEXPR,
	KW_CONST_CAST,
	KW_CONTINUE,
	KW_DECLTYPE,
	KW_DEFAULT,
	KW_DELETE,
	KW_DO,
	KW_DOUBLE,
	KW_DYNAMIC_CAST,
	KW_ELSE,
	KW_ENUM,
	KW_EXPLICIT,
	KW_EXPORT,
	KW_EXTERN,
	KW_FALSE,
	KW_FLOAT,
	KW_FOR,
	KW_FRIEND,
	KW_GOTO,
	KW_IF,
	KW_INLINE,
	KW_INT,
	KW_LONG,
	KW_MUTABLE,
	KW_NAMESPACE,
	KW_NEW,
	KW_NOEXCEPT,
	KW_NULLPTR,
	KW_OPERATOR,
	KW_PRIVATE,
	KW_PROTECTED,
	KW_PUBLIC,
	KW_REGISTER,
	KW_REINTERPET_CAST,
	KW_RETURN,
	KW_SHORT,
	KW_SIGNED,
	KW_SIZEOF,
	KW_STATIC,
	KW_STATIC_ASSERT,
	KW_STATIC_CAST,
	KW_STRUCT,
	KW_SWITCH,
	KW_TEMPLATE,
	KW_THIS,
	KW_THREAD_LOCAL,
	KW_THROW,
	KW_TRUE,
	KW_TRY,
	KW_TYPEDEF,
	KW_TYPEID,
	KW_TYPENAME,
	KW_UNION,
	KW_UNSIGNED,
	KW_USING,
	KW_VIRTUAL,
	KW_VOID,
	KW_VOLATILE,
	KW_WCHAR_T,
	KW_WHILE,

	OP_LBRACE,
	OP_RBRACE,
	OP_LSQUARE,
	OP_RSQUARE,
	OP_LPAREN,
	OP_RPAREN,
	OP_BOR,
	OP_XOR,
	OP_COMPL,
	OP_AMP,
	OP_LNOT,
	OP_SEMICOLON,
	OP_COLON,
	OP_DOTS,
	OP_QMARK,
	OP_COLON2,
	OP_DOT,
	OP_DOTSTAR,
	OP_PLUS,
	OP_MINUS,
	OP_STAR,
	OP_DIV,
	OP_MOD,
	OP_ASS,
	OP_LT,
	OP_GT,
	OP_PLUSASS,
	OP_MINUSASS,
	OP_STARASS,
	OP_DIVASS,
	OP_MODASS,
	OP_XORASS,
	OP_BANDASS,
	OP_BORASS,
	OP_LSHIFT,
	OP_RSHIFT,
	OP_RSHIFTASS,
	OP_LSHIFTASS,
	OP_EQ,
	OP_NE,
	OP_LE,
	OP_GE,
	OP_LAND,
	OP_LOR,
	OP_INC,
	OP_DEC,
	OP_COMMA,
	OP_ARROWSTAR,
	OP_ARROW
};

const char* simple_token_type_name(SimpleTokenType type);
bool lookup_simple_token_type(const std::string& source,
	SimpleTokenType* type);

struct LiteralData
{
	FundamentalType type;
	// Zero denotes a scalar.  A non-zero value is the number of array
	// elements, including a string's terminating code unit.
	std::size_t element_count;
	std::vector<std::uint8_t> bytes;

	LiteralData()
		: type(FundamentalType::Int), element_count(0), bytes()
	{}
};

enum class UserDefinedLiteralKind
{
	Integer,
	Floating,
	Character,
	String
};

struct UserDefinedLiteralData
{
	std::string source;
	std::string suffix;
	UserDefinedLiteralKind kind;
	// Integer and floating UDLs use prefix.  Character and string UDLs use
	// value instead.
	std::string prefix;
	LiteralData value;
};

struct IPostTokenOutput
{
	virtual void emit_invalid(const std::string& source) = 0;
	virtual void emit_simple(const std::string& source, SimpleTokenType type) = 0;
	virtual void emit_identifier(const std::string& source) = 0;
	virtual void emit_literal(const std::string& source,
		const LiteralData& value) = 0;
	virtual void emit_user_defined_literal(const UserDefinedLiteralData& value) = 0;
	// The source token was an identifier/keyword before PA2 mapped its
	// spelling to a SimpleTokenType.  Existing PA2 consumers receive the same
	// simple event through the default forwarding implementation.
	virtual void emit_simple_identifier(const std::string& source,
		SimpleTokenType type)
	{
		emit_simple(source, type);
	}
	// Existing PA2 consumers ignore line boundaries.  PA3 overrides this
	// optional event only through the explicit line-aware entry point below.
	virtual void emit_new_line() {}
	virtual void emit_eof() = 0;

	virtual ~IPostTokenOutput() {}
};

// Execute PA1's phases 1--3 and PA2's token conversion through the typed
// output boundary above.
void posttokenize_cpp_source(const std::string& source,
	IPostTokenOutput& output);

// The same typed conversion with logical-new-line events retained.  The
// ordinary entry point intentionally keeps newline a no-op for PA2.
void posttokenize_cpp_source_by_line(const std::string& source,
	IPostTokenOutput& output);

// Consume already-classified phase-3 tokens.  This is the typed boundary
// used by PA4 after macro replacement; it does not render and re-tokenize a
// source spelling.
void posttokenize_cpp_tokens(const std::vector<PPToken>& tokens,
	IPostTokenOutput& output);

// These compatibility decoders are intentionally retained for PA2's required
// floating-point bit representation.
float PA2Decode_float(const std::string& source);
double PA2Decode_double(const std::string& source);
long double PA2Decode_long_double(const std::string& source);
