#pragma once

#include <string>

// This is the typed phase-3 token fact shared by the macro owner and the
// posttoken consumer.  Whitespace, logical new-lines, and EOF have no
// spelling; all other kinds retain the tokenizer's exact spelling.
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
	PreprocessingOpOrPunc,
	NonWhitespaceCharacter,
	EndOfFile
};

struct PPToken
{
	PPTokenKind kind;
	std::string spelling;

	PPToken(PPTokenKind kind = PPTokenKind::EndOfFile,
		const std::string& spelling = std::string())
		: kind(kind), spelling(spelling)
	{}
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
	// Alternative operator words are preprocessing identifiers in phase 3,
	// even though their post-token spelling is an operator.  The default keeps
	// the PA1 observable event unchanged for existing consumers.
	virtual void emit_identifier_as_preprocessing_op_or_punc(
		const std::string& data)
	{
		emit_preprocessing_op_or_punc(data);
	}
	virtual void emit_non_whitespace_char(const std::string& data) = 0;
	virtual void emit_eof() = 0;

	virtual ~IPPTokenStream() {}
};
