#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

// Fixed phase-3 identity.  Alternative spellings (for example "<:" and
// "[") share one identity; PPToken retains the exact spelling for paste,
// stringization, and presentation.  The identifier-like operator words are
// phase-3 identifiers, but their fixed posttoken identities travel through
// this same field so they are not recovered from spelling later.
enum class PPTokenFixedIdentity
{
	None,
	IdentifierNew,
	IdentifierDelete,
	IdentifierAnd,
	IdentifierAndEq,
	IdentifierBitand,
	IdentifierBitor,
	IdentifierCompl,
	IdentifierNot,
	IdentifierNotEq,
	IdentifierOr,
	IdentifierOrEq,
	IdentifierXor,
	IdentifierXorEq,
	ArrowStar,
	ShiftRightAssign,
	ShiftLeftAssign,
	HashHash,
	LeftBracket,
	RightBracket,
	LeftBrace,
	RightBrace,
	Hash,
	Ellipsis,
	DotStar,
	PlusAssign,
	MinusAssign,
	StarAssign,
	SlashAssign,
	PercentAssign,
	CaretAssign,
	AmpersandAssign,
	PipeAssign,
	ShiftLeft,
	ShiftRight,
	LessEqual,
	GreaterEqual,
	LogicalAnd,
	EqualEqual,
	NotEqual,
	LogicalOr,
	Increment,
	Decrement,
	Arrow,
	Scope,
	LeftParen,
	RightParen,
	Semicolon,
	Colon,
	Question,
	Dot,
	Plus,
	Minus,
	Star,
	Slash,
	Percent,
	Caret,
	Ampersand,
	Pipe,
	Tilde,
	Bang,
	Equal,
	Less,
	Greater,
	Comma
};

typedef std::size_t PPSpellingId;

// Arbitrary source spellings are stage-owned presentation/payload facts.  A
// token carries only the stable ID; the table owns each distinct spelling once
// for the lifetime of the PA4 token buffer.
struct PPSpellingTable
{
	std::vector<std::string> values;
	std::unordered_map<std::string, PPSpellingId> ids;

	PPSpellingTable()
		: values(1, std::string()), ids()
	{}

	void clear()
	{
		values.clear();
		values.push_back(std::string());
		ids.clear();
	}

	PPSpellingId intern(const std::string& spelling)
	{
		std::unordered_map<std::string, PPSpellingId>::const_iterator found =
			ids.find(spelling);
		if (found != ids.end())
			return found->second;
		const PPSpellingId result = values.size();
		values.push_back(spelling);
		ids[values.back()] = result;
		return result;
	}

	const std::string& get(PPSpellingId id) const
	{
		return values.at(id);
	}
};

// This is the typed phase-3 token fact shared by the macro owner and the
// posttoken consumer.  Whitespace, logical new-lines, and EOF have no
// spelling; all other kinds retain the tokenizer's exact spelling through the
// PPSpellingTable owned by the containing PPTokenBuffer.
enum class PPTokenKind
{
	WhitespaceSequence,
	NewLine,
	HeaderName,
	Identifier,
	IdentifierAsPreprocessingOpOrPunc,
	PPNumber,
	CharacterLiteral,
	UserDefinedCharacterLiteral,
	StringLiteral,
	UserDefinedStringLiteral,
	Punctuator,
	NonWhitespaceCharacter,
	EndOfFile
};

struct PPToken
{
	PPTokenKind kind;
	PPTokenFixedIdentity fixed_identity;
	PPSpellingId spelling;

	PPToken(PPTokenKind kind = PPTokenKind::EndOfFile,
		PPSpellingId spelling = 0,
		PPTokenFixedIdentity fixed_identity = PPTokenFixedIdentity::None)
		: kind(kind), fixed_identity(fixed_identity), spelling(spelling)
	{}
};

struct PPTokenBuffer
{
	PPSpellingTable spellings;
	std::vector<PPToken> tokens;

	void clear()
	{
		spellings.clear();
		tokens.clear();
	}
};

struct IPPTokenStream
{
	virtual void emit_whitespace_sequence() = 0;
	virtual void emit_new_line() = 0;
	virtual void emit_header_name(const std::string& data) = 0;
	virtual void emit_identifier(const std::string& data) = 0;
	virtual void emit_pp_number(const std::string& data) = 0;
	virtual void emit_character_literal(const std::string& data) = 0;
	virtual void emit_user_defined_character_literal(const std::string& data) = 0;
	virtual void emit_string_literal(const std::string& data) = 0;
	virtual void emit_user_defined_string_literal(const std::string& data) = 0;
	virtual void emit_preprocessing_op_or_punc(const std::string& data) = 0;
	// The tokenizer supplies fixed identity at the canonical producer seam.
	// The default preserves the PA1 callback for legacy adapters; typed
	// collectors override it and do not recover identity from the spelling.
	virtual void emit_punctuator(PPTokenFixedIdentity fixed_identity,
		const std::string& data)
	{
		(void)fixed_identity;
		emit_preprocessing_op_or_punc(data);
	}
	// Alternative operator words are preprocessing identifiers in phase 3,
	// even though their post-token spelling is an operator.  The typed default
	// keeps the PA1 observable event unchanged for existing consumers.
	virtual void emit_identifier_as_preprocessing_op_or_punc(
		PPTokenFixedIdentity fixed_identity, const std::string& data)
	{
		(void)fixed_identity;
		emit_identifier_as_preprocessing_op_or_punc(data);
	}
	// Legacy PA1 callback retained for old textual consumers.
	virtual void emit_identifier_as_preprocessing_op_or_punc(
		const std::string& data)
	{
		emit_preprocessing_op_or_punc(data);
	}
	virtual void emit_non_whitespace_char(const std::string& data) = 0;
	virtual void emit_eof() = 0;

	virtual ~IPPTokenStream() {}
};
