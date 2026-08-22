#include "pp_tokenizer.h"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct Unit
{
	size_t begin;
	size_t end;
	int cp;
	bool from_ucn;

	Unit(int cp, size_t begin, size_t end, bool from_ucn = false)
		: begin(begin), end(end), cp(cp), from_ucn(from_ucn)
	{}
};

struct Range
{
	int first;
	int last;
};

enum class TokenKind
{
	HeaderName,
	Identifier,
	PPNumber,
	CharacterLiteral,
	UserDefinedCharacterLiteral,
	StringLiteral,
	UserDefinedStringLiteral,
	PreprocessingOpOrPunc,
	NonWhitespaceCharacter
};

const Range kAnnexE1[] =
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

const Range kAnnexE2[] =
{
	{0x300, 0x36F}, {0x1DC0, 0x1DFF}, {0x20D0, 0x20FF}, {0xFE20, 0xFE2F}
};

const char* const kIdentifierLikeOperators[] =
{
	"new", "delete", "and", "and_eq", "bitand", "bitor", "compl",
	"not", "not_eq", "or", "or_eq", "xor", "xor_eq"
};

const char* const kPunctuators[] =
{
	"%:%:", "->*", ">>=", "<<=", "##", "<:", ":>", "<%", "%>", "%:",
	"...", ".*", "+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "<<",
	">>", "<=", ">=", "&&", "==", "!=", "||", "++", "--", "->", "::",
	"{", "}", "[", "]", "#", "(", ")", ";", ":", "?", ".", "+", "-",
	"*", "/", "%", "^", "&", "|", "~", "!", "=", "<", ">", ","
};

bool is_hex_digit(int cp)
{
	return (cp >= '0' && cp <= '9') ||
		(cp >= 'a' && cp <= 'f') ||
		(cp >= 'A' && cp <= 'F');
}

int hex_value(int cp)
{
	if (cp >= '0' && cp <= '9') return cp - '0';
	if (cp >= 'a' && cp <= 'f') return cp - 'a' + 10;
	return cp - 'A' + 10;
}

bool in_ranges(int cp, const Range* ranges, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		if (cp < ranges[i].first)
			return false;
		if (cp <= ranges[i].last)
			return true;
	}
	return false;
}

bool in_annex_e1(int cp)
{
	return in_ranges(cp, kAnnexE1, sizeof(kAnnexE1) / sizeof(kAnnexE1[0]));
}

bool in_annex_e2(int cp)
{
	return in_ranges(cp, kAnnexE2, sizeof(kAnnexE2) / sizeof(kAnnexE2[0]));
}

bool is_ascii_nondigit(int cp)
{
	return (cp >= 'a' && cp <= 'z') ||
		(cp >= 'A' && cp <= 'Z') || cp == '_';
}

bool is_ascii_digit(int cp)
{
	return cp >= '0' && cp <= '9';
}

bool is_identifier_start(int cp)
{
	return is_ascii_nondigit(cp) || (in_annex_e1(cp) && !in_annex_e2(cp));
}

bool is_identifier_body(int cp)
{
	return is_identifier_start(cp) || is_ascii_digit(cp) || in_annex_e2(cp);
}

bool is_identifier_nondigit(int cp)
{
	return is_identifier_body(cp) && !is_ascii_digit(cp);
}

bool is_non_newline_whitespace(int cp)
{
	return cp == ' ' || cp == '\t' || cp == '\v' || cp == '\f' || cp == '\r';
}

bool is_control_code_point(int cp)
{
	return (cp >= 0x00 && cp <= 0x1F) || (cp >= 0x7F && cp <= 0x9F);
}

bool is_basic_source_character(int cp)
{
	if (is_ascii_nondigit(cp) || is_ascii_digit(cp))
		return true;
	if (cp == ' ' || cp == '\t' || cp == '\v' || cp == '\f' || cp == '\n')
		return true;

	switch (cp)
	{
	case '_': case '{': case '}': case '[': case ']': case '#':
	case '(': case ')': case '<': case '>': case '%': case ':':
	case ';': case '.': case '?': case '*': case '+': case '-':
	case '/': case '^': case '&': case '|': case '~': case '!':
	case '=': case ',': case '\\': case '"': case '\'':
		return true;
	default:
		return false;
	}
}

bool is_forbidden_external_ucn(int cp)
{
	return is_control_code_point(cp) || is_basic_source_character(cp);
}

void append_utf8(std::string* result, int cp)
{
	if (cp < 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
		throw std::runtime_error("invalid unicode code point");

	if (cp <= 0x7F)
	{
		result->push_back(static_cast<char>(cp));
	}
	else if (cp <= 0x7FF)
	{
		result->push_back(static_cast<char>(0xC0 | (cp >> 6)));
		result->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	else if (cp <= 0xFFFF)
	{
		result->push_back(static_cast<char>(0xE0 | (cp >> 12)));
		result->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		result->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
	else
	{
		result->push_back(static_cast<char>(0xF0 | (cp >> 18)));
		result->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
		result->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
		result->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
	}
}

std::vector<int> decode_utf8(const std::string& source)
{
	std::vector<int> result;
	result.reserve(source.size());

	for (size_t i = 0; i < source.size();)
	{
		const unsigned char first = static_cast<unsigned char>(source[i]);
		int cp = 0;
		size_t count = 0;
		int second_min = 0x80;
		int second_max = 0xBF;

		if (first <= 0x7F)
		{
			cp = first;
			count = 1;
		}
		else if (first >= 0xC2 && first <= 0xDF)
		{
			cp = first & 0x1F;
			count = 2;
		}
		else if (first == 0xE0)
		{
			cp = first & 0x0F;
			count = 3;
			second_min = 0xA0;
		}
		else if (first >= 0xE1 && first <= 0xEC)
		{
			cp = first & 0x0F;
			count = 3;
		}
		else if (first == 0xED)
		{
			cp = first & 0x0F;
			count = 3;
			second_max = 0x9F;
		}
		else if (first >= 0xEE && first <= 0xEF)
		{
			cp = first & 0x0F;
			count = 3;
		}
		else if (first == 0xF0)
		{
			cp = first & 0x07;
			count = 4;
			second_min = 0x90;
		}
		else if (first >= 0xF1 && first <= 0xF3)
		{
			cp = first & 0x07;
			count = 4;
		}
		else if (first == 0xF4)
		{
			cp = first & 0x07;
			count = 4;
			second_max = 0x8F;
		}
		else
		{
			throw std::runtime_error("invalid UTF-8 leading byte");
		}

		if (i + count > source.size())
			throw std::runtime_error("truncated UTF-8 sequence");

		for (size_t j = 1; j < count; ++j)
		{
			const unsigned char continuation =
				static_cast<unsigned char>(source[i + j]);
			if ((continuation & 0xC0) != 0x80 ||
				(j == 1 && (continuation < second_min || continuation > second_max)))
				throw std::runtime_error("invalid UTF-8 continuation byte");
			cp = (cp << 6) | (continuation & 0x3F);
		}

		if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
			throw std::runtime_error("invalid UTF-8 code point");
		result.push_back(cp);
		i += count;
	}

	return result;
}

int trigraph_replacement(int cp)
{
	switch (cp)
	{
	case '=': return '#';
	case '/': return '\\';
	case '\'': return '^';
	case '(': return '[';
	case ')': return ']';
	case '!': return '|';
	case '<': return '{';
	case '>': return '}';
	case '-': return '~';
	default: return -1;
	}
}

std::vector<Unit> phase1_trigraphs(const std::vector<int>& physical)
{
	std::vector<Unit> result;
	result.reserve(physical.size());

	for (size_t i = 0; i < physical.size();)
	{
		if (i + 2 < physical.size() && physical[i] == '?' && physical[i + 1] == '?')
		{
			const int replacement = trigraph_replacement(physical[i + 2]);
			if (replacement >= 0)
			{
				result.push_back(Unit(replacement, i, i + 3));
				i += 3;
				continue;
			}
		}

		result.push_back(Unit(physical[i], i, i + 1));
		++i;
	}
	return result;
}

bool read_ucn(const std::vector<Unit>& input, size_t at, int* value, size_t* count)
{
	if (at + 1 >= input.size() || input[at].cp != '\\' ||
		(input[at + 1].cp != 'u' && input[at + 1].cp != 'U'))
		return false;

	const size_t digits = input[at + 1].cp == 'u' ? 4 : 8;
	if (at + 2 + digits > input.size())
		return false;

	int result = 0;
	for (size_t i = 0; i < digits; ++i)
	{
		if (!is_hex_digit(input[at + 2 + i].cp))
			return false;
		result = result * 16 + hex_value(input[at + 2 + i].cp);
	}

	*value = result;
	*count = digits + 2;
	return true;
}

void phase1_ucns(std::vector<Unit>& units)
{
	size_t read = 0;
	size_t write = 0;
	while (read < units.size())
	{
		// A pair of backslashes is an escape spelling in literals and must not
		// expose the second slash as a fresh UCN introducer.
		if (units[read].cp == '\\' && read + 1 < units.size() &&
			units[read + 1].cp == '\\')
		{
			units[write++] = units[read++];
			units[write++] = units[read++];
			continue;
		}

		int value = 0;
		size_t count = 0;
		if (read_ucn(units, read, &value, &count))
		{
			const bool valid_value = value <= 0x10FFFF &&
				!(value >= 0xD800 && value <= 0xDFFF);
			if (valid_value)
			{
				const Unit converted(value, units[read].begin,
					units[read + count - 1].end, true);
				units[write++] = converted;
				read += count;
				continue;
			}

			// Keep an invalid value in its source spelling.  The tokenizer will
			// reject it outside a raw literal, while raw-string reversal can
			// correctly retain the physical spelling.
			for (size_t j = 0; j < count; ++j)
				units[write++] = units[read + j];
			read += count;
			continue;
		}

		units[write++] = units[read++];
	}
	units.erase(units.begin() + write, units.end());
}

void phase2_line_splicing(std::vector<Unit>& units, size_t physical_size)
{
	size_t read = 0;
	size_t write = 0;
	while (read < units.size())
	{
		if (units[read].cp == '\\' && read + 1 < units.size() &&
			units[read + 1].cp == '\n')
		{
			read += 2;
			continue;
		}
		units[write++] = units[read++];
	}
	units.erase(units.begin() + write, units.end());

	if (physical_size != 0 && (units.empty() || units.back().cp != '\n'))
		units.push_back(Unit('\n', physical_size, physical_size));
}

bool starts_ucn(const std::vector<Unit>& input, size_t at)
{
	return at + 1 < input.size() && input[at].cp == '\\' &&
		(input[at + 1].cp == 'u' || input[at + 1].cp == 'U');
}

bool invalid_or_unconverted_ucn(const std::vector<Unit>& input, size_t at)
{
	if (!starts_ucn(input, at))
		return false;

	int value = 0;
	size_t count = 0;
	if (!read_ucn(input, at, &value, &count))
		return true;
	return value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF);
}

class Tokenizer
{
public:
	Tokenizer(const std::vector<int>& physical, const std::vector<Unit>& units,
		IPPTokenStream& output)
		: physical_(physical), units_(units), output_(output), pos_(0),
		  line_start_(true), directive_hash_(false), header_allowed_(false)
	{}

	void run()
	{
		while (pos_ < units_.size())
		{
			if (scan_whitespace())
				continue;
			if (units_[pos_].cp == '\n')
			{
				output_.emit_new_line();
				++pos_;
				line_start_ = true;
				directive_hash_ = false;
				header_allowed_ = false;
				continue;
			}

			validate_external_unit(pos_);

			if (header_allowed_ && (units_[pos_].cp == '<' || units_[pos_].cp == '"'))
			{
				scan_header_name();
				continue;
			}
			if (scan_raw_string())
				continue;
			if (scan_literal())
				continue;
			if (scan_identifier())
				continue;
			if (scan_pp_number())
				continue;
			if (scan_punctuator())
				continue;

			if (units_[pos_].cp == '\'' || units_[pos_].cp == '"')
				throw std::runtime_error("unterminated literal");

			const std::string data = data_from_units(pos_, pos_ + 1);
			output_.emit_non_whitespace_char(data);
			++pos_;
			mark_nonwhite(TokenKind::NonWhitespaceCharacter, data);
		}

		output_.emit_eof();
	}

private:
	const std::vector<int>& physical_;
	const std::vector<Unit>& units_;
	IPPTokenStream& output_;
	size_t pos_;
	bool line_start_;
	bool directive_hash_;
	bool header_allowed_;

	std::string data_from_units(size_t begin, size_t end) const
	{
		std::string result;
		for (size_t i = begin; i < end; ++i)
			append_utf8(&result, units_[i].cp);
		return result;
	}

	std::string data_from_physical(size_t begin, size_t end) const
	{
		std::string result;
		for (size_t i = begin; i < end; ++i)
			append_utf8(&result, physical_[i]);
		return result;
	}

	bool matches(size_t at, const char* text) const
	{
		size_t i = at;
		for (size_t j = 0; text[j] != '\0'; ++j, ++i)
		{
			if (i >= units_.size() || units_[i].cp != static_cast<unsigned char>(text[j]))
				return false;
		}
		return true;
	}

	void validate_external_unit(size_t at) const
	{
		if (invalid_or_unconverted_ucn(units_, at))
			throw std::runtime_error("invalid universal-character-name");
		if (units_[at].from_ucn && is_forbidden_external_ucn(units_[at].cp))
			throw std::runtime_error("universal-character-name names a basic source character");
	}

	void validate_external_range(size_t begin, size_t end) const
	{
		for (size_t i = begin; i < end; ++i)
			validate_external_unit(i);
	}

	bool scan_whitespace()
	{
		if (pos_ >= units_.size())
			return false;

		validate_external_unit(pos_);
		bool consumed = false;
		while (pos_ < units_.size())
		{
			if (is_non_newline_whitespace(units_[pos_].cp))
			{
				validate_external_unit(pos_);
				++pos_;
				consumed = true;
				continue;
			}

			if (units_[pos_].cp == '/' && pos_ + 1 < units_.size() &&
				units_[pos_ + 1].cp == '/')
			{
				validate_external_unit(pos_ + 1);
				pos_ += 2;
				while (pos_ < units_.size() && units_[pos_].cp != '\n')
				{
					validate_external_unit(pos_);
					++pos_;
				}
				consumed = true;
				continue;
			}

			if (units_[pos_].cp == '/' && pos_ + 1 < units_.size() &&
				units_[pos_ + 1].cp == '*')
			{
				validate_external_unit(pos_ + 1);
				pos_ += 2;
				bool closed = false;
				while (pos_ < units_.size())
				{
					validate_external_unit(pos_);
					if (units_[pos_].cp == '*' && pos_ + 1 < units_.size() &&
						units_[pos_ + 1].cp == '/')
					{
						pos_ += 2;
						closed = true;
						break;
					}
					++pos_;
				}
				if (!closed)
					throw std::runtime_error("unterminated block comment");
				consumed = true;
				continue;
			}

			break;
		}

		if (consumed)
		{
			output_.emit_whitespace_sequence();
			return true;
		}
		return false;
	}

	void scan_header_name()
	{
		const size_t begin = pos_;
		const int closing = units_[pos_].cp == '<' ? '>' : '"';
		bool has_content = false;
		++pos_;

		while (pos_ < units_.size())
		{
			validate_external_unit(pos_);
			if (units_[pos_].cp == '\n')
				throw std::runtime_error("unterminated header name");
			if (units_[pos_].cp == closing)
			{
				if (!has_content)
					throw std::runtime_error("empty header name");
				++pos_;
				const std::string data = data_from_units(begin, pos_);
				output_.emit_header_name(data);
				mark_nonwhite(TokenKind::HeaderName, data);
				return;
			}
			has_content = true;
			++pos_;
		}

		throw std::runtime_error("unterminated header name");
	}

	size_t raw_prefix_length(size_t at) const
	{
		if (matches(at, "u8R\"")) return 4;
		if (matches(at, "uR\"")) return 3;
		if (matches(at, "UR\"")) return 3;
		if (matches(at, "LR\"")) return 3;
		if (matches(at, "R\"")) return 2;
		return 0;
	}

	bool raw_d_char(int cp) const
	{
		return cp != ' ' && cp != '\t' && cp != '\v' && cp != '\f' &&
			cp != '\r' && cp != '\n' && cp != '(' && cp != ')' && cp != '\\';
	}

	bool scan_raw_string()
	{
		const size_t prefix_length = raw_prefix_length(pos_);
		if (prefix_length == 0)
			return false;

		const size_t opening_quote = pos_ + prefix_length - 1;
		validate_external_range(pos_, opening_quote + 1);
		const size_t physical_content_begin = units_[opening_quote].end;
		if (physical_content_begin > physical_.size())
			throw std::runtime_error("invalid raw string source span");

		std::vector<int> delimiter;
		size_t open = physical_content_begin;
		while (open < physical_.size() && physical_[open] != '(')
		{
			if (!raw_d_char(physical_[open]))
				throw std::runtime_error("invalid raw string delimiter");
			if (delimiter.size() == 16)
				throw std::runtime_error("raw string delimiter too long");
			delimiter.push_back(physical_[open]);
			++open;
		}
		if (open == physical_.size())
			throw std::runtime_error("unterminated raw string literal");

		size_t close = open + 1;
		while (close < physical_.size())
		{
			if (physical_[close] == ')' &&
				close + 1 + delimiter.size() < physical_.size())
			{
				bool same = true;
				for (size_t i = 0; i < delimiter.size(); ++i)
				{
					if (physical_[close + 1 + i] != delimiter[i])
					{
						same = false;
						break;
					}
				}
				if (same && physical_[close + 1 + delimiter.size()] == '"')
				{
					const size_t physical_end = close + 2 + delimiter.size();
					size_t logical_end = pos_;
					while (logical_end < units_.size() &&
						units_[logical_end].begin < physical_end)
						++logical_end;

					if (logical_end == pos_)
						throw std::runtime_error("invalid raw string source span");

					const size_t suffix_begin = logical_end;
					size_t suffix_end = suffix_begin;
					if (suffix_begin < units_.size() &&
						is_identifier_start(units_[suffix_begin].cp))
					{
						suffix_end = consume_identifier(suffix_begin);
					}

					std::string data = data_from_units(pos_, opening_quote + 1);
					data += data_from_physical(physical_content_begin, physical_end);
					if (suffix_end != suffix_begin)
						data += data_from_units(suffix_begin, suffix_end);

					if (suffix_end != suffix_begin)
						output_.emit_user_defined_string_literal(data);
					else
						output_.emit_string_literal(data);
					pos_ = suffix_end == suffix_begin ? logical_end : suffix_end;
					mark_nonwhite(suffix_end == suffix_begin ?
						TokenKind::StringLiteral : TokenKind::UserDefinedStringLiteral, data);
					return true;
				}
			}
			++close;
		}

		throw std::runtime_error("unterminated raw string literal");
	}

	void validate_literal_unit(size_t at) const
	{
		if (invalid_or_unconverted_ucn(units_, at))
			throw std::runtime_error("invalid universal-character-name");
	}

	size_t consume_escape(size_t at) const
	{
		if (at + 1 >= units_.size())
			throw std::runtime_error("unterminated escape sequence");

		const int next = units_[at + 1].cp;
		if (next == '\'' || next == '"' || next == '?' || next == '\\' ||
			next == 'a' || next == 'b' || next == 'f' || next == 'n' ||
			next == 'r' || next == 't' || next == 'v')
			return at + 2;

		if (next >= '0' && next <= '7')
		{
			size_t end = at + 2;
			while (end < units_.size() && end < at + 4 &&
				units_[end].cp >= '0' && units_[end].cp <= '7')
				++end;
			return end;
		}

		if (next == 'x')
		{
			if (at + 2 >= units_.size() || !is_hex_digit(units_[at + 2].cp))
				throw std::runtime_error("invalid hex escape sequence");
			size_t end = at + 3;
			while (end < units_.size() && is_hex_digit(units_[end].cp))
				++end;
			return end;
		}

		throw std::runtime_error("invalid escape sequence");
	}

	bool literal_prefix(size_t at, bool* character, size_t* quote) const
	{
		if (at >= units_.size())
			return false;
		if (units_[at].cp == '\'' || units_[at].cp == '"')
		{
			*character = units_[at].cp == '\'';
			*quote = at;
			return true;
		}
		if ((units_[at].cp == 'u' || units_[at].cp == 'U' || units_[at].cp == 'L') &&
			at + 1 < units_.size() &&
			(units_[at + 1].cp == '\'' || units_[at + 1].cp == '"'))
		{
			*character = units_[at + 1].cp == '\'';
			*quote = at + 1;
			return true;
		}
		if (matches(at, "u8\""))
		{
			*character = false;
			*quote = at + 2;
			return true;
		}
		return false;
	}

	bool scan_literal()
	{
		bool character = false;
		size_t quote = 0;
		if (!literal_prefix(pos_, &character, &quote))
			return false;

		const size_t begin = pos_;
		validate_external_range(begin, quote + 1);
		size_t i = quote + 1;
		bool has_content = false;
		while (i < units_.size())
		{
			validate_literal_unit(i);
			if (units_[i].cp == '\n')
				throw std::runtime_error("unterminated literal");
			if (units_[i].from_ucn)
			{
				has_content = true;
				++i;
				continue;
			}
			if (units_[i].cp == '\\')
			{
				i = consume_escape(i);
				has_content = true;
				continue;
			}
			if (units_[i].cp == (character ? '\'' : '"'))
			{
				if (character && !has_content)
					throw std::runtime_error("empty character literal");
				++i;
				size_t suffix_end = i;
				if (suffix_end < units_.size() &&
					is_identifier_start(units_[suffix_end].cp))
					suffix_end = consume_identifier(suffix_end);

				const std::string data = data_from_units(begin, suffix_end);
				if (character)
				{
					if (suffix_end != i)
						output_.emit_user_defined_character_literal(data);
					else
						output_.emit_character_literal(data);
				}
				else
				{
					if (suffix_end != i)
						output_.emit_user_defined_string_literal(data);
					else
						output_.emit_string_literal(data);
				}
				pos_ = suffix_end;
				if (character)
					mark_nonwhite(suffix_end != i ?
						TokenKind::UserDefinedCharacterLiteral : TokenKind::CharacterLiteral, data);
				else
					mark_nonwhite(suffix_end != i ?
						TokenKind::UserDefinedStringLiteral : TokenKind::StringLiteral, data);
				return true;
			}
			has_content = true;
			++i;
		}

		throw std::runtime_error("unterminated literal");
	}

	size_t consume_identifier(size_t at) const
	{
		size_t end = at;
		while (end < units_.size() && is_identifier_body(units_[end].cp))
		{
			validate_external_unit(end);
			++end;
		}
		return end;
	}

	bool scan_identifier()
	{
		if (pos_ >= units_.size() || !is_identifier_start(units_[pos_].cp))
			return false;

		const size_t begin = pos_;
		const size_t end = consume_identifier(pos_);
		const std::string data = data_from_units(begin, end);
		pos_ = end;

		bool alternative = false;
		for (size_t i = 0; i < sizeof(kIdentifierLikeOperators) /
			sizeof(kIdentifierLikeOperators[0]); ++i)
		{
			if (data == kIdentifierLikeOperators[i])
			{
				alternative = true;
				break;
			}
		}

		if (alternative)
			output_.emit_preprocessing_op_or_punc(data);
		else
			output_.emit_identifier(data);
		mark_nonwhite(alternative ? TokenKind::PreprocessingOpOrPunc : TokenKind::Identifier, data);
		return true;
	}

	bool scan_pp_number()
	{
		if (pos_ >= units_.size())
			return false;
		if (!is_ascii_digit(units_[pos_].cp) &&
			!(units_[pos_].cp == '.' && pos_ + 1 < units_.size() &&
				is_ascii_digit(units_[pos_ + 1].cp)))
			return false;

		const size_t begin = pos_;
		++pos_;
		while (pos_ < units_.size())
		{
			validate_external_unit(pos_);
			const int cp = units_[pos_].cp;
			if (is_ascii_digit(cp) || cp == '.')
			{
				++pos_;
				continue;
			}
			if (is_identifier_nondigit(cp))
			{
				if ((cp == 'e' || cp == 'E') && pos_ + 1 < units_.size() &&
					(units_[pos_ + 1].cp == '+' || units_[pos_ + 1].cp == '-'))
				{
					++pos_;
					validate_external_unit(pos_);
					++pos_;
				}
				else
				{
					++pos_;
				}
				continue;
			}
			break;
		}

		const std::string data = data_from_units(begin, pos_);
		output_.emit_pp_number(data);
		mark_nonwhite(TokenKind::PPNumber, data);
		return true;
	}

	bool scan_punctuator()
	{
		if (pos_ >= units_.size())
			return false;

		if (units_[pos_].cp == '<' && matches(pos_, "<::"))
		{
			const size_t after = pos_ + 3;
			if (after >= units_.size() ||
				(units_[after].cp != ':' && units_[after].cp != '>'))
			{
				const std::string data = "<";
				output_.emit_preprocessing_op_or_punc(data);
				++pos_;
				mark_nonwhite(TokenKind::PreprocessingOpOrPunc, data);
				return true;
			}
		}

		for (size_t i = 0; i < sizeof(kPunctuators) / sizeof(kPunctuators[0]); ++i)
		{
			if (!matches(pos_, kPunctuators[i]))
				continue;
			const std::string data(kPunctuators[i]);
			const size_t end = pos_ + data.size();
			validate_external_range(pos_, end);
			pos_ = end;
			output_.emit_preprocessing_op_or_punc(data);
			mark_nonwhite(TokenKind::PreprocessingOpOrPunc, data);
			return true;
		}
		return false;
	}

	void mark_nonwhite(TokenKind kind, const std::string& data)
	{
		if (line_start_)
		{
			if (kind == TokenKind::PreprocessingOpOrPunc &&
				(data == "#" || data == "%:"))
			{
				directive_hash_ = true;
				header_allowed_ = false;
			}
			else
			{
				directive_hash_ = false;
				header_allowed_ = false;
			}
		}
		else if (directive_hash_ && !header_allowed_)
		{
			if (kind == TokenKind::Identifier && data == "include")
				header_allowed_ = true;
			else
				directive_hash_ = false;
		}
		else if (header_allowed_)
		{
			header_allowed_ = false;
			directive_hash_ = false;
		}

		line_start_ = false;
	}
};

} // namespace

void tokenize_cpp_source(const std::string& source, IPPTokenStream& output)
{
	std::vector<int> physical = decode_utf8(source);
	if (!physical.empty() && physical[0] == 0xFEFF)
		physical.erase(physical.begin());

	std::vector<Unit> logical = phase1_trigraphs(physical);
	phase1_ucns(logical);
	phase2_line_splicing(logical, physical.size());

	Tokenizer tokenizer(physical, logical, output);
	tokenizer.run();
}
