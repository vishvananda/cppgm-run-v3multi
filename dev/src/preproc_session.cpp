#include "preproc_session.h"

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ctrlexpr.h"
#include "macro.h"
#include "posttoken.h"
#include "pp_tokenizer.h"

namespace
{

typedef std::pair<unsigned long int, unsigned long int> PA5FileId;

// Includes are semantically recursive, but an implementation must fail
// before a malformed cycle can exhaust the process stack.  This is a
// resource limit at the include owner; ordinary guarded/once'd headers remain
// valid because the limit applies only to the active recursive depth.
const std::size_t kMaxIncludeDepth = 256;

extern "C" long int syscall(long int n, ...) throw ();

bool PA5GetFileId(const std::string& path, PA5FileId& out_fileid)
{
	struct
	{
		unsigned long int dev;
		unsigned long int ino;
		long int unused[16];
	} data = {};

	const int result = static_cast<int>(syscall(4, path.c_str(), &data));
	out_fileid = std::make_pair(data.dev, data.ino);
	return result == 0;
}

struct FileIdHash
{
	std::size_t operator()(const PA5FileId& value) const
	{
		const std::size_t left = static_cast<std::size_t>(value.first);
		const std::size_t right = static_cast<std::size_t>(value.second);
		return left ^ (right + static_cast<std::size_t>(0x9e3779b9) +
			(left << 6) + (left >> 2));
	}
};

bool is_whitespace(const PPToken& token)
{
	return token.kind == PPTokenKind::WhitespaceSequence ||
		token.kind == PPTokenKind::NewLine;
}

bool is_identifier(const PPToken& token)
{
	return token.kind == PPTokenKind::Identifier ||
		token.kind == PPTokenKind::IdentifierAsPreprocessingOpOrPunc;
}

bool is_punctuator(const PPToken& token, PPTokenFixedIdentity identity)
{
	return token.kind == PPTokenKind::Punctuator &&
		token.fixed_identity == identity;
}

std::size_t skip_whitespace(const std::vector<PPToken>& tokens,
	std::size_t at, std::size_t end)
{
	while (at < end && is_whitespace(tokens[at]))
		++at;
	return at;
}

std::string ordinary_string_literal_bytes(const LiteralData& value)
{
	if (value.type != FundamentalType::Char || value.element_count == 0 ||
		value.bytes.size() != value.element_count || value.bytes.back() != 0)
		throw std::runtime_error("expected ordinary string literal");

	std::string result;
	result.reserve(value.bytes.size() - 1);
	for (std::size_t i = 0; i + 1 < value.bytes.size(); ++i)
		result.push_back(static_cast<char>(value.bytes[i]));
	return result;
}

// C++11 _Pragma destringization is deliberately narrower than ordinary
// literal decoding: only escaped quotes and escaped backslashes are removed.
// Other backslash sequences remain text for the pragma processor.
std::string destringize_pragma(const std::string& source)
{
	const std::size_t quote = source.find('"');
	if (quote == std::string::npos || quote + 1 >= source.size() ||
		source[source.size() - 1] != '"')
		throw std::runtime_error("invalid _Pragma string literal");

	std::string result;
	for (std::size_t at = quote + 1; at + 1 < source.size(); ++at)
	{
		if (source[at] == '\\' && at + 1 < source.size() - 1 &&
			(source[at + 1] == '\\' || source[at + 1] == '"'))
		{
			result.push_back(source[++at]);
		}
		else
			result.push_back(source[at]);
	}
	return result;
}

std::string make_string_literal(const std::string& value)
{
	std::string result("\"");
	for (std::size_t i = 0; i < value.size(); ++i)
	{
		if (value[i] == '\\' || value[i] == '"')
			result.push_back('\\');
		result.push_back(value[i]);
	}
	result.push_back('"');
	return result;
}

std::string make_number(std::size_t value)
{
	std::ostringstream stream;
	stream << value;
	return stream.str();
}

std::string read_file(const std::string& path)
{
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if (!input)
		throw std::runtime_error("unable to open source file: " + path);
	std::ostringstream source;
	source << input.rdbuf();
	return source.str();
}

enum class DirectiveKind
{
	Unknown,
	If,
	Ifdef,
	Ifndef,
	Elif,
	Else,
	Endif,
	Include,
	Define,
	Undef,
	Line,
	Error,
	Pragma
};

enum class BuiltinKind
{
	Cppgm,
	Cplusplus,
	Stdchosted,
	Author,
	File,
	Line,
	Date,
	Time,
	Counter
};

struct ConditionalFrame
{
	bool parent_active;
	bool branch_taken;
	bool active;
	bool saw_else;

	ConditionalFrame(bool parent_active = false, bool branch_taken = false,
		bool active = false)
		: parent_active(parent_active), branch_taken(branch_taken),
		  active(active), saw_else(false)
	{}
};

struct FileFrame
{
	std::string actual_path;
	std::string presumed_file;
	PPSpellingId presumed_file_id;
	std::size_t line;
	std::size_t next_line;
	bool line_override;
	bool use_physical_lines;
	std::vector<PPToken> tokens;

	FileFrame(const std::string& actual_path, const std::string& presumed_file)
		: actual_path(actual_path), presumed_file(presumed_file),
		  presumed_file_id(0), line(1),
		  next_line(0), line_override(false), use_physical_lines(true),
		  tokens()
	{}
};

class LiteralOperandOutput : public IPostTokenOutput
{
public:
	LiteralOperandOutput() : literals(), other_tokens(0), invalid(false) {}

	void emit_invalid(const std::string& source)
	{
		(void)source;
		invalid = true;
	}

	void emit_simple(const std::string& source, SimpleTokenType type)
	{
		(void)source;
		(void)type;
		++other_tokens;
	}

	void emit_identifier(const std::string& source)
	{
		(void)source;
		++other_tokens;
	}

	void emit_literal(const std::string& source, const LiteralData& value)
	{
		(void)source;
		literals.push_back(value);
	}

	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		(void)value;
		invalid = true;
	}

	void emit_eof() {}

	std::vector<LiteralData> literals;
	std::size_t other_tokens;
	bool invalid;
};

} // namespace

struct PPPreprocessingSession::Impl
{
	PPPreprocessConfig config;
	PPTokenBuffer output;
	std::unique_ptr<PPMacroSession> macros;
	std::unordered_set<PA5FileId, FileIdHash> pragma_once_files;
	std::vector<ConditionalFrame> conditionals;
	std::unordered_map<PPSpellingId, DirectiveKind> directives;
	std::unordered_map<PPSpellingId, BuiltinKind> builtins;
	PPSpellingId pragma_once_id;
	PPSpellingId pragma_operator_id;
	std::size_t counter;
	std::size_t include_depth;
	FileFrame* current;

	 explicit Impl(const PPPreprocessConfig& config)
		: config(config), output(), macros(), pragma_once_files(),
		  conditionals(), directives(), builtins(), pragma_once_id(0),
		  pragma_operator_id(0), counter(0),
		  include_depth(0), current(NULL)
	{}

	const PPTokenBuffer& run(const std::string& source_path,
		const std::string& source)
	{
		output.clear();
		macros.reset(new PPMacroSession(output.spellings));
		pragma_once_files.clear();
		conditionals.clear();
		directives.clear();
		builtins.clear();
		counter = 0;
		include_depth = 0;
		current = NULL;
		initialize_names();
		process_file(source_path, source_path, source);
		if (!conditionals.empty())
			throw std::runtime_error("unterminated conditional group");
		output.tokens.push_back(PPToken(PPTokenKind::EndOfFile));
		return output;
	}

	void initialize_names()
	{
		pragma_once_id = output.spellings.intern("once");
		pragma_operator_id = output.spellings.intern("_Pragma");
		add_directive("if", DirectiveKind::If);
		add_directive("ifdef", DirectiveKind::Ifdef);
		add_directive("ifndef", DirectiveKind::Ifndef);
		add_directive("elif", DirectiveKind::Elif);
		add_directive("else", DirectiveKind::Else);
		add_directive("endif", DirectiveKind::Endif);
		add_directive("include", DirectiveKind::Include);
		add_directive("define", DirectiveKind::Define);
		add_directive("undef", DirectiveKind::Undef);
		add_directive("line", DirectiveKind::Line);
		add_directive("error", DirectiveKind::Error);
		add_directive("pragma", DirectiveKind::Pragma);

		add_builtin("__CPPGM__", BuiltinKind::Cppgm);
		add_builtin("__cplusplus", BuiltinKind::Cplusplus);
		add_builtin("__STDC_HOSTED__", BuiltinKind::Stdchosted);
		add_builtin("__CPPGM_AUTHOR__", BuiltinKind::Author);
		add_builtin("__FILE__", BuiltinKind::File);
		add_builtin("__LINE__", BuiltinKind::Line);
		add_builtin("__DATE__", BuiltinKind::Date);
		add_builtin("__TIME__", BuiltinKind::Time);
		add_builtin("__COUNTER__", BuiltinKind::Counter);
		macros->set_builtin_resolver(&Impl::resolve_builtin, this);
	}

	void add_directive(const char* spelling, DirectiveKind kind)
	{
		directives[output.spellings.intern(spelling)] = kind;
	}

	void add_builtin(const char* spelling, BuiltinKind kind)
	{
		const PPSpellingId id = output.spellings.intern(spelling);
		builtins[id] = kind;
		macros->register_builtin(id);
	}

	bool active() const
	{
		return conditionals.empty() || conditionals.back().active;
	}

	static bool resolve_builtin(void* context, const PPToken& invocation,
		PPToken* replacement)
	{
		Impl* self = static_cast<Impl*>(context);
		if (self == NULL || self->macros.get() == NULL ||
			!self->macros->is_defined(invocation.spelling))
			return false;
		std::unordered_map<PPSpellingId, BuiltinKind>::const_iterator found =
			self->builtins.find(invocation.spelling);
		if (found == self->builtins.end())
			return false;

		std::string spelling;
		switch (found->second)
		{
		case BuiltinKind::Cppgm: spelling = "201303L"; break;
		case BuiltinKind::Cplusplus: spelling = "201103L"; break;
		case BuiltinKind::Stdchosted: spelling = "1"; break;
		case BuiltinKind::Author: spelling = make_string_literal(
			self->config.author); break;
		case BuiltinKind::File:
		{
			const PPSpellingId file_id =
				invocation.source_location.presumed_file;
			const std::string file = file_id < self->output.spellings.values.size() ?
				self->output.spellings.get(file_id) : std::string();
			spelling = make_string_literal(file);
			break;
		}
		case BuiltinKind::Line: spelling = make_number(
			invocation.source_location.line); break;
		case BuiltinKind::Date: spelling = make_string_literal(
			self->config.build_date); break;
		case BuiltinKind::Time: spelling = make_string_literal(
			self->config.build_time); break;
		case BuiltinKind::Counter: spelling = make_number(self->counter++); break;
		}
		const PPTokenKind kind = spelling.size() > 0 && spelling[0] == '"' ?
			PPTokenKind::StringLiteral : PPTokenKind::PPNumber;
		*replacement = PPToken(kind, self->output.spellings.intern(spelling),
			PPTokenFixedIdentity::None, invocation.source_location);
		return true;
	}

	void process_file(const std::string& actual_path,
		const std::string& presumed_file, const std::string& source)
	{
		FileFrame frame(actual_path, presumed_file);
		tokenize_cpp_source_to_tokens(source, output.spellings, &frame.tokens);
		frame.presumed_file_id = output.spellings.intern(frame.presumed_file);
		const std::size_t condition_base = conditionals.size();
		FileFrame* previous = current;
		current = &frame;
		try
		{
			std::size_t at = 0;
			std::vector<PPToken> text_segment;
			while (at < frame.tokens.size())
			{
				if (frame.tokens[at].kind == PPTokenKind::EndOfFile)
					break;
				const std::size_t line_begin = at;
				while (at < frame.tokens.size() &&
					frame.tokens[at].kind != PPTokenKind::NewLine &&
					frame.tokens[at].kind != PPTokenKind::EndOfFile)
					++at;
				const std::size_t line_end = at;
				const bool has_newline = at < frame.tokens.size() &&
					frame.tokens[at].kind == PPTokenKind::NewLine;
				const std::size_t physical_newline_line = has_newline ?
					frame.tokens[at].source_location.line : 0;
				const PPSourceLocation location(frame.presumed_file_id,
					frame.line);
				for (std::size_t i = line_begin; i < line_end; ++i)
					frame.tokens[i].source_location = location;
				if (has_newline)
					frame.tokens[at].source_location = location;

				const std::size_t first = skip_whitespace(frame.tokens,
					line_begin, line_end);
				if (first < line_end && is_punctuator(frame.tokens[first],
					PPTokenFixedIdentity::Hash))
				{
					flush_text(&text_segment);
					handle_directive(frame, line_begin, line_end, first);
				}
				else if (active())
				{
					text_segment.insert(text_segment.end(),
						frame.tokens.begin() + line_begin,
						frame.tokens.begin() + line_end);
					if (has_newline)
						text_segment.push_back(frame.tokens[at]);
				}
				if (frame.line_override)
				{
					frame.line = frame.next_line;
					frame.line_override = false;
				}
				else if (frame.use_physical_lines &&
					physical_newline_line != 0)
					frame.line = physical_newline_line + 1;
				else
					++frame.line;
				if (has_newline)
					++at;
				else
				break;
			}
			flush_text(&text_segment);
			if (conditionals.size() != condition_base)
				throw std::runtime_error("unterminated conditional in include");
		}
		catch (...)
		{
			current = previous;
			throw;
		}
		current = previous;
	}

	void flush_text(std::vector<PPToken>* text_segment)
	{
		if (text_segment == NULL || text_segment->empty())
			return;
		std::vector<PPToken> expanded;
		macros->expand(*text_segment, &expanded);
		std::vector<PPToken> text;
		consume_pragmas(expanded, &text);
		output.tokens.insert(output.tokens.end(), text.begin(), text.end());
		text_segment->clear();
	}

	void handle_directive(FileFrame& frame, std::size_t line_begin,
		std::size_t line_end, std::size_t hash)
	{
		std::size_t at = skip_whitespace(frame.tokens, hash + 1, line_end);
		if (at >= line_end)
			return;
		if (!is_identifier(frame.tokens[at]))
		{
			if (active())
				throw std::runtime_error("invalid preprocessing directive");
			return;
		}
		const PPSpellingId name = frame.tokens[at].spelling;
		++at;
		std::unordered_map<PPSpellingId, DirectiveKind>::const_iterator found =
			directives.find(name);
		if (found == directives.end())
		{
			if (active())
				throw std::runtime_error("unsupported preprocessing directive");
			return;
		}

		switch (found->second)
		{
		case DirectiveKind::If: handle_if(frame.tokens, line_end, at); return;
		case DirectiveKind::Ifdef: handle_ifdef(frame.tokens, line_end, at, false); return;
		case DirectiveKind::Ifndef: handle_ifdef(frame.tokens, line_end, at, true); return;
		case DirectiveKind::Elif: handle_elif(frame.tokens, line_end, at); return;
		case DirectiveKind::Else: handle_else(frame.tokens, line_end, at); return;
		case DirectiveKind::Endif: handle_endif(frame.tokens, line_end, at); return;
		case DirectiveKind::Define:
			if (active())
				macros->define(frame.tokens, line_begin, line_end, at);
			return;
		case DirectiveKind::Undef:
			if (active())
				macros->undef(frame.tokens, line_begin, line_end, at);
			return;
		case DirectiveKind::Include:
			if (active())
				handle_include(frame.tokens, line_end, at);
			return;
		case DirectiveKind::Line:
			if (active())
				handle_line(frame.tokens, line_end, at);
			return;
		case DirectiveKind::Error:
			if (active())
				throw std::runtime_error("#error directive");
			return;
		case DirectiveKind::Pragma:
			if (active())
				handle_pragma(frame, line_end, at);
			return;
		case DirectiveKind::Unknown:
			return;
		}
	}

	void handle_if(const std::vector<PPToken>& tokens, std::size_t end,
		std::size_t at)
	{
		const bool parent = active();
		if (!parent)
		{
			conditionals.push_back(ConditionalFrame(false, false, false));
			return;
		}
		std::vector<PPToken> input(tokens.begin() + at, tokens.begin() + end);
		std::vector<PPToken> expanded;
		macros->expand_control(input, &expanded);
		const bool value = evaluate_condition(expanded);
		conditionals.push_back(ConditionalFrame(true, value, value));
	}

	void handle_ifdef(const std::vector<PPToken>& tokens, std::size_t end,
		std::size_t at, bool negate)
	{
		const bool parent = active();
		if (!parent)
		{
			conditionals.push_back(ConditionalFrame(false, false, false));
			return;
		}
		at = skip_whitespace(tokens, at, end);
		if (at >= end || !is_identifier(tokens[at]))
			throw std::runtime_error("invalid ifdef operand");
		const PPSpellingId name = tokens[at].spelling;
		++at;
		if (skip_whitespace(tokens, at, end) != end)
			throw std::runtime_error("extra ifdef tokens");
		bool value = macros->is_defined(name);
		if (negate)
			value = !value;
		conditionals.push_back(ConditionalFrame(true, value, value));
	}

	void handle_elif(const std::vector<PPToken>& tokens, std::size_t end,
		std::size_t at)
	{
		if (conditionals.empty())
			throw std::runtime_error("elif without if");
		ConditionalFrame& frame = conditionals.back();
		if (frame.saw_else)
			throw std::runtime_error("elif after else");
		if (!frame.parent_active || frame.branch_taken)
		{
			frame.active = false;
			return;
		}
		std::vector<PPToken> input(tokens.begin() + at, tokens.begin() + end);
		std::vector<PPToken> expanded;
		macros->expand_control(input, &expanded);
		const bool value = evaluate_condition(expanded);
		frame.active = value;
		frame.branch_taken = value;
	}

	void handle_else(const std::vector<PPToken>& tokens, std::size_t end,
		std::size_t at)
	{
		if (conditionals.empty())
			throw std::runtime_error("else without if");
		ConditionalFrame& frame = conditionals.back();
		if (frame.saw_else)
			throw std::runtime_error("duplicate else");
		frame.saw_else = true;
		frame.active = frame.parent_active && !frame.branch_taken;
		frame.branch_taken = true;
		if (frame.active && skip_whitespace(tokens, at, end) != end)
			throw std::runtime_error("extra else tokens");
	}

	void handle_endif(const std::vector<PPToken>& tokens, std::size_t end,
		std::size_t at)
	{
		if (conditionals.empty())
			throw std::runtime_error("endif without if");
		const bool was_active = conditionals.back().active;
		if (was_active && skip_whitespace(tokens, at, end) != end)
			throw std::runtime_error("extra endif tokens");
		conditionals.pop_back();
	}

	bool evaluate_condition(const std::vector<PPToken>& tokens)
	{
		std::vector<PPToken> expression(tokens);
		expression.push_back(PPToken(PPTokenKind::EndOfFile));
		PPControlExpressionValue value;
		const bool parsed = evaluate_cpp_control_expression_ids(
			output.spellings, expression, &Impl::macro_defined, this, &value);
		if (!parsed || !value.valid)
			throw std::runtime_error("invalid controlling expression");
		return value.bits != 0;
	}

	static bool macro_defined(void* context, PPSpellingId spelling)
	{
		Impl* self = static_cast<Impl*>(context);
		if (self == NULL || self->macros.get() == NULL)
			return false;
		return self->macros->is_defined(spelling);
	}

	void handle_include(const std::vector<PPToken>& tokens, std::size_t end,
		std::size_t at)
	{
		std::vector<PPToken> input(tokens.begin() + at, tokens.begin() + end);
		std::vector<PPToken> expanded;
		macros->expand(input, &expanded);
		std::vector<PPToken> nonwhite;
		for (std::size_t i = 0; i < expanded.size(); ++i)
			if (!is_whitespace(expanded[i]))
				nonwhite.push_back(expanded[i]);
		if (nonwhite.size() != 1 ||
			(nonwhite[0].kind != PPTokenKind::HeaderName &&
				 nonwhite[0].kind != PPTokenKind::StringLiteral))
			throw std::runtime_error("invalid include operand");

		const std::string data = output.spellings.get(nonwhite[0].spelling);
		std::string nextf;
		if (nonwhite[0].kind == PPTokenKind::HeaderName && data.size() >= 2 &&
			((data[0] == '<' && data[data.size() - 1] == '>') ||
			 (data[0] == '"' && data[data.size() - 1] == '"')))
			nextf = data.substr(1, data.size() - 2);
		else
		{
			std::vector<PPToken> typed(1, nonwhite[0]);
			typed.push_back(PPToken(PPTokenKind::EndOfFile));
			LiteralOperandOutput converted;
			posttokenize_cpp_tokens(output.spellings, typed, converted);
			if (converted.invalid || converted.other_tokens != 0 ||
				converted.literals.size() != 1)
				throw std::runtime_error("invalid include string literal");
			nextf = ordinary_string_literal_bytes(converted.literals[0]);
		}
		include_file(nextf);
	}

	void include_file(const std::string& nextf)
	{
		if (nextf.find('\0') != std::string::npos)
			throw std::runtime_error("include path contains NUL");

		std::string pathrel;
		const std::size_t slash = current->presumed_file.rfind('/');
		if (slash != std::string::npos)
			pathrel = current->presumed_file.substr(0, slash + 1) + nextf;

		std::string chosen;
		PA5FileId fileid;
		if (!pathrel.empty() && pathrel.find('\0') == std::string::npos &&
			PA5GetFileId(pathrel, fileid))
			chosen = pathrel;
		else if (PA5GetFileId(nextf, fileid))
			chosen = nextf;
		else
			throw std::runtime_error("include file not found: " + nextf);

		if (pragma_once_files.find(fileid) != pragma_once_files.end())
			return;

		// Resolve identity and honor once first.  The limit applies only at the
		// edge that would read and recurse into another active frame.
		if (include_depth >= kMaxIncludeDepth)
			throw std::runtime_error("maximum include depth reached");

		const std::string included_source = read_file(chosen);
		++include_depth;
		try
		{
			process_file(chosen, chosen, included_source);
		}
		catch (...)
		{
			--include_depth;
			throw;
		}
		--include_depth;
	}

	void handle_line(const std::vector<PPToken>& tokens, std::size_t end,
		std::size_t at)
	{
		std::vector<PPToken> input(tokens.begin() + at, tokens.begin() + end);
		std::vector<PPToken> expanded;
		macros->expand(input, &expanded);
		std::vector<PPToken> nonwhite;
		for (std::size_t i = 0; i < expanded.size(); ++i)
			if (!is_whitespace(expanded[i]))
				nonwhite.push_back(expanded[i]);
		if (nonwhite.empty() || nonwhite.size() > 2 ||
			nonwhite[0].kind != PPTokenKind::PPNumber ||
			(nonwhite.size() == 2 &&
				nonwhite[1].kind != PPTokenKind::StringLiteral))
			throw std::runtime_error("invalid line directive");

		std::vector<PPToken> typed = nonwhite;
		typed.push_back(PPToken(PPTokenKind::EndOfFile));
		LiteralOperandOutput converted;
		posttokenize_cpp_tokens(output.spellings, typed, converted);
		if (converted.invalid || converted.other_tokens != 0 ||
			converted.literals.size() != nonwhite.size())
			throw std::runtime_error("invalid line directive literal");
		const LiteralData& number = converted.literals[0];
		if (number.element_count != 0 || number.bytes.empty() ||
			number.bytes.size() > sizeof(std::size_t))
			throw std::runtime_error("invalid line number");
		std::size_t value = 0;
		for (std::size_t i = 0; i < number.bytes.size(); ++i)
			value |= static_cast<std::size_t>(number.bytes[i]) << (i * 8);
		if (value == 0)
			throw std::runtime_error("line number must be positive");
		current->next_line = value;
		current->line_override = true;
		current->use_physical_lines = false;
		if (nonwhite.size() == 2)
			current->presumed_file = ordinary_string_literal_bytes(
				converted.literals[1]);
		current->presumed_file_id = output.spellings.intern(
			current->presumed_file);
	}

	void handle_pragma(FileFrame& frame, std::size_t end, std::size_t at)
	{
		at = skip_whitespace(frame.tokens, at, end);
		if (at >= end || !is_identifier(frame.tokens[at]))
			return;
		if (frame.tokens[at].spelling != pragma_once_id)
			return;
		++at;
		if (skip_whitespace(frame.tokens, at, end) != end)
			return;
		PA5FileId fileid;
		if (PA5GetFileId(frame.actual_path, fileid))
			pragma_once_files.insert(fileid);
	}

	void consume_pragmas(const std::vector<PPToken>& input,
		std::vector<PPToken>* output_tokens)
	{
		output_tokens->clear();
		std::size_t at = 0;
		while (at < input.size())
		{
			if (!is_identifier(input[at]) ||
				input[at].spelling != pragma_operator_id)
			{
				output_tokens->push_back(input[at++]);
				continue;
			}
			std::size_t open = skip_whitespace(input, at + 1, input.size());
			if (open >= input.size() || !is_punctuator(input[open],
				PPTokenFixedIdentity::LeftParen))
				throw std::runtime_error("malformed _Pragma operator");
			std::size_t literal = skip_whitespace(input, open + 1,
				input.size());
			if (literal >= input.size() ||
				input[literal].kind != PPTokenKind::StringLiteral)
				throw std::runtime_error("_Pragma requires string literal");
			std::size_t close = skip_whitespace(input, literal + 1,
				input.size());
			if (close >= input.size() || !is_punctuator(input[close],
				PPTokenFixedIdentity::RightParen))
				throw std::runtime_error("malformed _Pragma operator");
			execute_pragma(destringize_pragma(
				output.spellings.get(input[literal].spelling)));
			at = close + 1;
		}
	}

	void execute_pragma(const std::string& spelling)
	{
		std::size_t first = 0;
		while (first < spelling.size() &&
			(spelling[first] == ' ' || spelling[first] == '\t'))
			++first;
		std::size_t last = first;
		while (last < spelling.size() && spelling[last] != ' ' &&
			spelling[last] != '\t')
			++last;
		if (spelling.substr(first, last - first) != "once")
			return;
		PA5FileId fileid;
		if (PA5GetFileId(current->actual_path, fileid))
			pragma_once_files.insert(fileid);
	}
};

PPPreprocessingSession::PPPreprocessingSession(
	const PPPreprocessConfig& config)
	: impl_(new Impl(config))
{}

PPPreprocessingSession::~PPPreprocessingSession()
{
	delete impl_;
}

const PPTokenBuffer& PPPreprocessingSession::preprocess(
	const std::string& source_path, const std::string& source)
{
	return impl_->run(source_path, source);
}
