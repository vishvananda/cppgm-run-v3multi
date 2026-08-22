#include "macro.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pp_tokenizer.h"

namespace
{

class TokenCollector : public IPPTokenStream
{
public:
	std::vector<PPToken> tokens;

	void emit_whitespace_sequence()
	{
		tokens.push_back(PPToken(PPTokenKind::WhitespaceSequence));
	}

	void emit_new_line()
	{
		tokens.push_back(PPToken(PPTokenKind::NewLine));
	}

	void emit_header_name(const std::string& data)
	{
		tokens.push_back(PPToken(PPTokenKind::HeaderName, data));
	}

	void emit_identifier(const std::string& data)
	{
		tokens.push_back(PPToken(PPTokenKind::Identifier, data));
	}

	void emit_identifier_as_preprocessing_op_or_punc(const std::string& data)
	{
		tokens.push_back(PPToken(
			PPTokenKind::IdentifierAsPreprocessingOpOrPunc, data));
	}

	void emit_pp_number(const std::string& data)
	{
		tokens.push_back(PPToken(PPTokenKind::PPNumber, data));
	}

	void emit_character_literal(const std::string& data)
	{
		tokens.push_back(PPToken(PPTokenKind::CharacterLiteral, data));
	}

	void emit_user_defined_character_literal(const std::string& data)
	{
		tokens.push_back(PPToken(
			PPTokenKind::UserDefinedCharacterLiteral, data));
	}

	void emit_string_literal(const std::string& data)
	{
		tokens.push_back(PPToken(PPTokenKind::StringLiteral, data));
	}

	void emit_user_defined_string_literal(const std::string& data)
	{
		tokens.push_back(PPToken(
			PPTokenKind::UserDefinedStringLiteral, data));
	}

	void emit_preprocessing_op_or_punc(const std::string& data)
	{
		tokens.push_back(PPToken(PPTokenKind::PreprocessingOpOrPunc, data));
	}

	void emit_non_whitespace_char(const std::string& data)
	{
		tokens.push_back(PPToken(PPTokenKind::NonWhitespaceCharacter, data));
	}

	void emit_eof()
	{
		tokens.push_back(PPToken(PPTokenKind::EndOfFile));
	}
};

struct PaintedToken
{
	PPToken token;
	std::size_t paint;
	bool from_replacement;
	bool deferred;
	bool blocked;

	PaintedToken(const PPToken& token = PPToken(), std::size_t paint = 0,
		bool from_replacement = false, bool deferred = false,
		bool blocked = false)
		: token(token), paint(paint), from_replacement(from_replacement),
		  deferred(deferred), blocked(blocked)
	{}
};

struct MacroDefinition
{
	std::string name;
	std::size_t id;
	bool function_like;
	bool variadic;
	std::vector<std::string> parameters;
	std::unordered_map<std::string, std::size_t> parameter_ids;
	std::vector<PPToken> replacement;

	MacroDefinition()
		: name(), id(0), function_like(false), variadic(false),
		  parameters(), parameter_ids(), replacement()
	{}
};

struct SubstitutionUnit
{
	bool paste_operator;
	bool placemarker;
	bool variadic_placemarker;
	PaintedToken token;

	SubstitutionUnit()
		: paste_operator(false), placemarker(false),
		  variadic_placemarker(false), token()
	{}

	static SubstitutionUnit token_unit(const PaintedToken& token)
	{
		SubstitutionUnit result;
		result.token = token;
		return result;
	}

	static SubstitutionUnit paste_unit(const PaintedToken& token)
	{
		SubstitutionUnit result;
		result.paste_operator = true;
		result.token = token;
		return result;
	}

	static SubstitutionUnit placemarker_unit(bool variadic = false)
	{
		SubstitutionUnit result;
		result.placemarker = true;
		result.variadic_placemarker = variadic;
		return result;
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

bool is_punctuator(const PPToken& token, const char* spelling)
{
	return token.kind == PPTokenKind::PreprocessingOpOrPunc &&
		token.spelling == spelling;
}

bool is_hash(const PPToken& token)
{
	return is_punctuator(token, "#") || is_punctuator(token, "%:");
}

bool is_paste(const PPToken& token)
{
	return is_punctuator(token, "##") || is_punctuator(token, "%:%:");
}

std::vector<PaintedToken> trim_argument(
	const std::vector<PaintedToken>& argument)
{
	std::size_t first = 0;
	while (first < argument.size() && is_whitespace(argument[first].token))
		++first;
	std::size_t last = argument.size();
	while (last > first && is_whitespace(argument[last - 1].token))
		--last;
	return std::vector<PaintedToken>(argument.begin() + first,
		argument.begin() + last);
}

std::size_t skip_whitespace(const std::vector<PPToken>& tokens,
	std::size_t at, std::size_t end)
{
	while (at < end && is_whitespace(tokens[at]))
		++at;
	return at;
}

std::size_t previous_nonwhitespace(const std::vector<PPToken>& tokens,
	std::size_t at)
{
	while (at > 0)
	{
		--at;
		if (!is_whitespace(tokens[at]))
			return at;
	}
	return std::string::npos;
}

std::size_t next_nonwhitespace(const std::vector<PPToken>& tokens,
	std::size_t at, std::size_t end)
{
	while (at < end)
	{
		if (!is_whitespace(tokens[at]))
			return at;
		++at;
	}
	return std::string::npos;
}

class MacroProcessor
{
public:
	MacroProcessor()
		: macros_(), next_macro_id_(1), paints_(), paint_ids_()
	{
		paints_.push_back(std::vector<std::size_t>());
		paint_ids_[paints_[0]] = 0;
	}

	void process(const std::string& source, std::vector<PPToken>* output)
	{
		TokenCollector collector;
		tokenize_cpp_source(source, collector);

		std::vector<PaintedToken> final_tokens;
		std::vector<PaintedToken> text_segment;
		std::size_t at = 0;
		while (at < collector.tokens.size())
		{
			if (collector.tokens[at].kind == PPTokenKind::EndOfFile)
				break;

			const std::size_t line_begin = at;
			while (at < collector.tokens.size() &&
				collector.tokens[at].kind != PPTokenKind::NewLine &&
				collector.tokens[at].kind != PPTokenKind::EndOfFile)
				++at;
			const std::size_t line_end = at;
			const bool has_newline = at < collector.tokens.size() &&
				collector.tokens[at].kind == PPTokenKind::NewLine;

			std::size_t first = skip_whitespace(
				collector.tokens, line_begin, line_end);
			if (first < line_end && is_hash(collector.tokens[first]))
			{
				expand_text(text_segment, &final_tokens);
				text_segment.clear();
				parse_directive(collector.tokens, line_begin, line_end, first);
			}
			else
			{
				for (std::size_t i = line_begin; i < line_end; ++i)
					text_segment.push_back(PaintedToken(collector.tokens[i]));
				if (has_newline)
					text_segment.push_back(PaintedToken(collector.tokens[at]));
			}

			if (has_newline)
				++at;
			else
				break;
		}

		expand_text(text_segment, &final_tokens);
		output->clear();
		for (std::size_t i = 0; i < final_tokens.size(); ++i)
			output->push_back(final_tokens[i].token);
		output->push_back(PPToken(PPTokenKind::EndOfFile));
	}

private:
	std::unordered_map<std::string, MacroDefinition> macros_;
	std::size_t next_macro_id_;
	std::vector<std::vector<std::size_t> > paints_;
	std::map<std::vector<std::size_t>, std::size_t> paint_ids_;

	static bool is_parameter(const MacroDefinition& macro,
		const PPToken& token, std::size_t* parameter_index)
	{
		if (!is_identifier(token))
			return false;
		std::unordered_map<std::string, std::size_t>::const_iterator found =
			macro.parameter_ids.find(token.spelling);
		if (found != macro.parameter_ids.end())
		{
			if (parameter_index != NULL)
				*parameter_index = found->second;
			return true;
		}
		if (macro.variadic && token.spelling == "__VA_ARGS__")
		{
			if (parameter_index != NULL)
				*parameter_index = macro.parameters.size();
			return true;
		}
		return false;
	}

	static bool same_replacement(const std::vector<PPToken>& left,
		const std::vector<PPToken>& right)
	{
		std::vector<PPToken> a;
		std::vector<PPToken> b;
		for (std::size_t i = 0; i < left.size(); ++i)
			a.push_back(is_whitespace(left[i]) ?
				PPToken(PPTokenKind::WhitespaceSequence) : left[i]);
		for (std::size_t i = 0; i < right.size(); ++i)
			b.push_back(is_whitespace(right[i]) ?
				PPToken(PPTokenKind::WhitespaceSequence) : right[i]);
		if (a.size() != b.size())
			return false;
		for (std::size_t i = 0; i < a.size(); ++i)
		{
			if (a[i].kind != b[i].kind || a[i].spelling != b[i].spelling)
				return false;
		}
		return true;
	}

	static bool equivalent(const MacroDefinition& left,
		const MacroDefinition& right)
	{
		return left.function_like == right.function_like &&
			left.variadic == right.variadic &&
			left.parameters == right.parameters &&
			same_replacement(left.replacement, right.replacement);
	}

	std::size_t intern_paint(std::vector<std::size_t> names)
	{
		std::sort(names.begin(), names.end());
		names.erase(std::unique(names.begin(), names.end()), names.end());
		std::map<std::vector<std::size_t>, std::size_t>::iterator found =
			paint_ids_.find(names);
		if (found != paint_ids_.end())
			return found->second;
		const std::size_t result = paints_.size();
		paints_.push_back(names);
		paint_ids_[paints_.back()] = result;
		return result;
	}

	std::size_t add_paint(std::size_t inherited, std::size_t macro_id)
	{
		std::vector<std::size_t> names = paints_[inherited];
		names.push_back(macro_id);
		return intern_paint(names);
	}

	std::size_t merge_paint(std::size_t left, std::size_t right)
	{
		std::vector<std::size_t> names = paints_[left];
		names.insert(names.end(), paints_[right].begin(), paints_[right].end());
		return intern_paint(names);
	}

	std::size_t remove_paint(std::size_t paint, std::size_t macro_id)
	{
		std::vector<std::size_t> names;
		const std::vector<std::size_t>& painted_names = paints_[paint];
		for (std::size_t i = 0; i < painted_names.size(); ++i)
			if (painted_names[i] != macro_id)
				names.push_back(painted_names[i]);
		return intern_paint(names);
	}

	std::size_t parameter_paint(const PaintedToken& token,
		std::size_t inherited, std::size_t current_macro_paint)
	{
		if (token.deferred && !token.blocked)
		{
			// A deferred token that already carries paint from a parent
			// replacement has crossed a parameter boundary.  Drop the parent
			// portion of that paint, while retaining unrelated unavailable names.
			if (!paints_[token.paint].empty())
			{
				std::size_t result = token.paint;
				for (std::size_t i = 0; i < paints_[inherited].size(); ++i)
					result = remove_paint(result, paints_[inherited][i]);
				return result;
			}
			if (!paints_[inherited].empty())
			{
				const MacroDefinition* token_macro = is_identifier(token.token) ?
					find_macro(token.token.spelling) : NULL;
				if (token_macro != NULL &&
					token_macro->id == paints_[inherited].back())
					return merge_paint(current_macro_paint,
						add_paint(0, token_macro->id));
				return 0;
			}
			return current_macro_paint;
		}
		// A token that was already examined as an ordinary source token is
		// carried across parameter substitution with its own paint.  It does
		// not acquire the current replacement-list paint.
		return token.paint;
	}

	bool painted(std::size_t paint, std::size_t macro_id) const
	{
		const std::vector<std::size_t>& names = paints_[paint];
		return std::binary_search(names.begin(), names.end(), macro_id);
	}

	const MacroDefinition* find_macro(const std::string& name) const
	{
		std::unordered_map<std::string, MacroDefinition>::const_iterator found =
			macros_.find(name);
		return found == macros_.end() ? NULL : &found->second;
	}

	static void fail(const char* message)
	{
		throw std::runtime_error(message);
	}

	void parse_directive(const std::vector<PPToken>& tokens,
		std::size_t line_begin, std::size_t line_end, std::size_t hash)
	{
		std::size_t at = skip_whitespace(tokens, hash + 1, line_end);
		if (at >= line_end || !is_identifier(tokens[at]))
			fail("missing preprocessing directive name");
		const std::string directive = tokens[at].spelling;
		++at;
		if (directive == "define")
			parse_define(tokens, line_begin, line_end, at);
		else if (directive == "undef")
			parse_undef(tokens, line_begin, line_end, at);
		else
			fail("unsupported preprocessing directive");
	}

	void parse_undef(const std::vector<PPToken>& tokens,
		std::size_t line_begin, std::size_t line_end, std::size_t at)
	{
		(void)line_begin;
		at = skip_whitespace(tokens, at, line_end);
		if (at >= line_end || !is_identifier(tokens[at]) ||
			tokens[at].spelling == "__VA_ARGS__")
			fail("invalid undef macro name");
		const std::string name = tokens[at].spelling;
		++at;
		at = skip_whitespace(tokens, at, line_end);
		if (at != line_end)
			fail("extra tokens after undef macro name");
		macros_.erase(name);
	}

	void parse_define(const std::vector<PPToken>& tokens,
		std::size_t line_begin, std::size_t line_end, std::size_t at)
	{
		(void)line_begin;
		at = skip_whitespace(tokens, at, line_end);
		if (at >= line_end || !is_identifier(tokens[at]) ||
			tokens[at].spelling == "__VA_ARGS__")
			fail("invalid define macro name");

		MacroDefinition definition;
		definition.name = tokens[at].spelling;
		++at;

		if (at < line_end && is_punctuator(tokens[at], "("))
		{
			definition.function_like = true;
			parse_parameters(tokens, line_end, &at, &definition);
			at = skip_whitespace(tokens, at, line_end);
		}
		else
		{
			if (at < line_end && !is_whitespace(tokens[at]))
				fail("object-like macro needs whitespace before replacement");
			at = skip_whitespace(tokens, at, line_end);
		}

		definition.replacement.assign(tokens.begin() + at,
			tokens.begin() + line_end);
		while (!definition.replacement.empty() &&
			is_whitespace(definition.replacement.back()))
			definition.replacement.pop_back();
		validate_replacement(definition);

		std::unordered_map<std::string, MacroDefinition>::const_iterator old =
			macros_.find(definition.name);
		if (old != macros_.end())
		{
			if (!equivalent(old->second, definition))
				fail("incompatible macro redefinition");
			return;
		}

		definition.id = next_macro_id_++;
		macros_[definition.name] = definition;
	}

	void parse_parameters(const std::vector<PPToken>& tokens,
		std::size_t end, std::size_t* at, MacroDefinition* definition)
	{
		// The caller has already established that the opening parenthesis is
		// immediately adjacent to the macro name.
		++*at;
		*at = skip_whitespace(tokens, *at, end);
		if (*at < end && is_punctuator(tokens[*at], ")"))
		{
			++*at;
			return;
		}

		while (true)
		{
			*at = skip_whitespace(tokens, *at, end);
			if (*at >= end)
				fail("unterminated macro parameter list");

			if (is_punctuator(tokens[*at], "..."))
			{
				if (definition->variadic)
					fail("duplicate variadic marker");
				definition->variadic = true;
				++*at;
				*at = skip_whitespace(tokens, *at, end);
				if (*at >= end || !is_punctuator(tokens[*at], ")"))
					fail("variadic marker must end parameter list");
				++*at;
				return;
			}

			if (!is_identifier(tokens[*at]) ||
				tokens[*at].spelling == "__VA_ARGS__")
				fail("invalid macro parameter");
			for (std::size_t i = 0; i < definition->parameters.size(); ++i)
				if (definition->parameters[i] == tokens[*at].spelling)
					fail("duplicate macro parameter");
			const std::size_t parameter_index = definition->parameters.size();
			definition->parameters.push_back(tokens[*at].spelling);
			definition->parameter_ids[tokens[*at].spelling] = parameter_index;
			++*at;
			*at = skip_whitespace(tokens, *at, end);

			if (*at < end && is_punctuator(tokens[*at], ")"))
			{
				++*at;
				return;
			}
			if (*at >= end || !is_punctuator(tokens[*at], ","))
				fail("missing comma in macro parameter list");
			++*at;
			*at = skip_whitespace(tokens, *at, end);
			if (*at < end && is_punctuator(tokens[*at], ")"))
				fail("trailing comma in macro parameter list");
		}
	}

	void validate_replacement(const MacroDefinition& definition)
	{
		for (std::size_t i = 0; i < definition.replacement.size(); ++i)
		{
			const PPToken& token = definition.replacement[i];
			if (is_paste(token))
			{
				const std::size_t left = previous_nonwhitespace(
					definition.replacement, i);
				const std::size_t right = next_nonwhitespace(
					definition.replacement, i + 1,
					definition.replacement.size());
				if (left == std::string::npos || right == std::string::npos)
					fail("token paste at replacement-list edge");
			}
			else if (is_hash(token))
			{
				if (!definition.function_like)
					continue;
				const std::size_t right = next_nonwhitespace(
					definition.replacement, i + 1,
					definition.replacement.size());
				std::size_t parameter_index = 0;
				if (right == std::string::npos ||
					!is_parameter(definition, definition.replacement[right],
						&parameter_index))
					fail("hash must precede a macro parameter");
				(void)parameter_index;
			}
			else if (is_identifier(token) &&
				token.spelling == "__VA_ARGS__" && !definition.variadic)
			{
				fail("__VA_ARGS__ outside variadic macro");
			}
		}
	}

	void expand_text(const std::vector<PaintedToken>& input,
		std::vector<PaintedToken>* output)
	{
		std::vector<PaintedToken> expanded;
		expand_tokens(input, &expanded);
		output->insert(output->end(), expanded.begin(), expanded.end());
	}

	void expand_tokens(const std::vector<PaintedToken>& input,
		std::vector<PaintedToken>* output)
	{
		std::deque<PaintedToken> work(input.begin(), input.end());
		while (!work.empty())
		{
			PaintedToken current = work.front();
			work.pop_front();

			if (is_identifier(current.token) &&
				current.token.spelling == "__VA_ARGS__")
				fail("__VA_ARGS__ outside variadic substitution");

			const MacroDefinition* macro = is_identifier(current.token) ?
				find_macro(current.token.spelling) : NULL;
			const bool tail_invocation = macro != NULL &&
				macro->function_like && current.from_replacement &&
				follows_source_invocation(&work);
			if (macro == NULL ||
				((current.blocked || painted(current.paint, macro->id)) &&
					!tail_invocation))
			{
				if (macro != NULL && painted(current.paint, macro->id))
				{
					current.blocked = true;
					current.deferred = false;
				}
				output->push_back(current);
				continue;
			}

			std::vector<std::vector<PaintedToken> > arguments;
			if (macro->function_like && !collect_invocation(
				macro, &work, &arguments))
			{
				current.deferred = true;
				output->push_back(current);
				continue;
			}
			current.deferred = false;
			current.blocked = false;

			std::vector<PaintedToken> replacement;
			if (macro->function_like)
				substitute_function(macro, current, arguments, &replacement);
			else
				substitute_object(macro, current, &replacement);
			for (std::size_t i = replacement.size(); i > 0; --i)
				work.push_front(replacement[i - 1]);
		}
	}

	static bool follows_source_invocation(
		const std::deque<PaintedToken>* work)
	{
		std::deque<PaintedToken>::const_iterator at = work->begin();
		while (at != work->end() && is_whitespace(at->token))
			++at;
		return at != work->end() && is_punctuator(at->token, "(") &&
			!at->from_replacement;
	}

	bool collect_invocation(const MacroDefinition* macro,
		std::deque<PaintedToken>* work,
		std::vector<std::vector<PaintedToken> >* arguments)
	{
		std::vector<PaintedToken> lookahead;
		while (!work->empty() && is_whitespace(work->front().token))
		{
			lookahead.push_back(work->front());
			work->pop_front();
		}
		if (work->empty() || !is_punctuator(work->front().token, "("))
		{
			for (std::size_t i = lookahead.size(); i > 0; --i)
				work->push_front(lookahead[i - 1]);
			return false;
		}
		work->pop_front();

		std::vector<PaintedToken> current;
		std::size_t nesting = 0;
		bool closed = false;
		while (!work->empty())
		{
			PaintedToken token = work->front();
			work->pop_front();
			if (is_punctuator(token.token, "("))
			{
				++nesting;
				current.push_back(token);
			}
			else if (is_punctuator(token.token, ")"))
			{
				if (nesting != 0)
				{
					--nesting;
					current.push_back(token);
				}
				else
				{
					const std::vector<PaintedToken> trimmed =
						trim_argument(current);
					if (arguments->empty() && trimmed.empty())
					{
						if (!macro->parameters.empty())
							arguments->push_back(trimmed);
					}
					else
						arguments->push_back(trimmed);
					closed = true;
					break;
				}
			}
			else if (is_punctuator(token.token, ",") && nesting == 0)
			{
				arguments->push_back(trim_argument(current));
				current.clear();
			}
			else
				current.push_back(token);
		}

		if (!closed)
			fail("unterminated macro invocation");
		if (!macro->variadic && arguments->size() != macro->parameters.size())
			fail("wrong number of macro arguments");
		if (macro->variadic && arguments->size() < macro->parameters.size())
			fail("missing fixed macro argument");
		return true;
	}

	void substitute_object(const MacroDefinition* macro,
		const PaintedToken& head, std::vector<PaintedToken>* replacement)
	{
		const std::size_t base_paint = add_paint(head.paint, macro->id);
		std::vector<SubstitutionUnit> units;
		for (std::size_t i = 0; i < macro->replacement.size(); ++i)
		{
			if (is_paste(macro->replacement[i]))
				units.push_back(SubstitutionUnit::paste_unit(
					PaintedToken(macro->replacement[i], base_paint)));
			else
				units.push_back(SubstitutionUnit::token_unit(
					PaintedToken(macro->replacement[i], base_paint, true)));
		}
		apply_pastes(&units);
		for (std::size_t i = 0; i < units.size(); ++i)
		{
			if (!units[i].paste_operator && !units[i].placemarker)
				replacement->push_back(units[i].token);
		}
	}

	std::vector<PaintedToken> variadic_argument(
		const MacroDefinition* macro,
		const std::vector<std::vector<PaintedToken> >& arguments,
		std::size_t first) const
	{
		std::vector<PaintedToken> result;
		for (std::size_t i = first; i < arguments.size(); ++i)
		{
			if (i != first)
			{
				result.push_back(PaintedToken(PPToken(
					PPTokenKind::PreprocessingOpOrPunc, ",")));
				// The argument separator is part of the raw spelling of
				// __VA_ARGS__.  The tokenizer has already collapsed source
				// whitespace, so retain one separator whitespace event for
				// stringization while remaining invisible to posttoken.
				result.push_back(PaintedToken(PPToken(
					PPTokenKind::WhitespaceSequence)));
			}
			result.insert(result.end(), arguments[i].begin(), arguments[i].end());
		}
		(void)macro;
		return result;
	}

	static bool parameter_uses_paste_or_hash(
		const MacroDefinition& macro, std::size_t at)
	{
		const std::size_t left = previous_nonwhitespace(macro.replacement, at);
		const std::size_t right = next_nonwhitespace(macro.replacement,
			at + 1, macro.replacement.size());
		return (left != std::string::npos &&
			is_hash(macro.replacement[left])) ||
			(left != std::string::npos &&
				is_paste(macro.replacement[left])) ||
			(right != std::string::npos &&
				is_paste(macro.replacement[right]));
	}

	std::vector<PaintedToken> retokenize_one(const std::string& spelling)
	{
		TokenCollector collector;
		tokenize_cpp_source(spelling, collector);
		std::vector<PPToken> nonwhite;
		for (std::size_t i = 0; i < collector.tokens.size(); ++i)
		{
			if (collector.tokens[i].kind == PPTokenKind::EndOfFile ||
				is_whitespace(collector.tokens[i]))
				continue;
			nonwhite.push_back(collector.tokens[i]);
		}
		if (nonwhite.size() != 1)
			throw std::runtime_error(std::string(
				"pasted text is not one preprocessing token: ") + spelling);
		return std::vector<PaintedToken>(1, PaintedToken(nonwhite[0]));
	}

	std::vector<PaintedToken> stringize(
		const std::vector<PaintedToken>& raw, std::size_t paint)
	{
		std::string spelling;
		spelling.push_back('"');
		bool pending_space = false;
		bool emitted = false;
		for (std::size_t i = 0; i < raw.size(); ++i)
		{
			const PPToken& token = raw[i].token;
			if (is_whitespace(token))
			{
				if (emitted)
					pending_space = true;
				continue;
			}
			if (pending_space)
			{
				spelling.push_back(' ');
				pending_space = false;
			}
			const bool literal =
				token.kind == PPTokenKind::CharacterLiteral ||
				token.kind == PPTokenKind::UserDefinedCharacterLiteral ||
				token.kind == PPTokenKind::StringLiteral ||
				token.kind == PPTokenKind::UserDefinedStringLiteral;
			for (std::size_t j = 0; j < token.spelling.size(); ++j)
			{
				if (token.spelling[j] == '"' ||
					(token.spelling[j] == '\\' && literal))
					spelling.push_back('\\');
				spelling.push_back(token.spelling[j]);
			}
			emitted = true;
		}
		spelling.push_back('"');
		// The generated literal is already a classified preprocessing token.
		// Retokenizing it would apply phase-1 trigraph replacement to text that
		// came from a raw-string token, changing the required stringized spelling.
		return std::vector<PaintedToken>(1, PaintedToken(
			PPToken(PPTokenKind::StringLiteral, spelling), paint));
	}

	void substitute_function(const MacroDefinition* macro,
		const PaintedToken& head,
		const std::vector<std::vector<PaintedToken> >& arguments,
		std::vector<PaintedToken>* replacement)
	{
		std::vector<std::vector<PaintedToken> > raw(macro->parameters.size() +
			(macro->variadic ? 1 : 0));
		std::vector<std::vector<PaintedToken> > expanded(raw.size());
		std::vector<bool> expanded_ready(raw.size(), false);
		for (std::size_t i = 0; i < macro->parameters.size(); ++i)
			raw[i] = arguments[i];
		if (macro->variadic)
			raw[macro->parameters.size()] = variadic_argument(
				macro, arguments, macro->parameters.size());

		for (std::size_t i = 0; i < macro->replacement.size(); ++i)
		{
			std::size_t parameter_index = 0;
			if (is_parameter(*macro, macro->replacement[i], &parameter_index) &&
				!parameter_uses_paste_or_hash(*macro, i))
			{
				if (!expanded_ready[parameter_index])
				{
					expanded_ready[parameter_index] = true;
					std::vector<PaintedToken> prescan_input =
						raw[parameter_index];
					if (!prescan_input.empty())
						expand_tokens(prescan_input,
							&expanded[parameter_index]);
				}
			}
		}

		const std::size_t base_paint = add_paint(head.paint, macro->id);
		const std::size_t current_macro_paint = add_paint(0, macro->id);
		std::vector<SubstitutionUnit> units;
		for (std::size_t i = 0; i < macro->replacement.size(); ++i)
		{
			const PPToken& token = macro->replacement[i];
			if (is_hash(token))
			{
				const std::size_t parameter = next_nonwhitespace(
					macro->replacement, i + 1,
					macro->replacement.size());
				std::size_t parameter_index = 0;
				if (parameter == std::string::npos ||
					!is_parameter(*macro, macro->replacement[parameter],
						&parameter_index))
					fail("invalid stringization operator");
				std::vector<PaintedToken> value = stringize(
					raw[parameter_index], base_paint);
				value[0].from_replacement = true;
				units.push_back(SubstitutionUnit::token_unit(value[0]));
				i = parameter;
				continue;
			}
			if (is_paste(token))
			{
				const std::size_t left = previous_nonwhitespace(
					macro->replacement, i);
				const std::size_t right = next_nonwhitespace(
					macro->replacement, i + 1,
					macro->replacement.size());
				std::size_t right_parameter = 0;
				const bool gnu_nonempty_variadic =
					macro->variadic && left != std::string::npos &&
					right != std::string::npos &&
					is_punctuator(macro->replacement[left], ",") &&
					is_parameter(*macro, macro->replacement[right],
						&right_parameter) &&
					macro->replacement[right].spelling == "__VA_ARGS__" &&
					right_parameter == macro->parameters.size() &&
					!raw[right_parameter].empty();
				if (gnu_nonempty_variadic)
					continue;
				units.push_back(SubstitutionUnit::paste_unit(
					PaintedToken(token, base_paint, true)));
				continue;
			}

			std::size_t parameter_index = 0;
			if (is_parameter(*macro, token, &parameter_index))
			{
				const bool raw_use = parameter_uses_paste_or_hash(*macro, i);
				const std::vector<PaintedToken>& value = raw_use ?
					raw[parameter_index] : expanded[parameter_index];
				if (value.empty() && raw_use)
				{
					const std::size_t left = previous_nonwhitespace(
						macro->replacement, i);
					const std::size_t right = next_nonwhitespace(
						macro->replacement, i + 1,
						macro->replacement.size());
					if ((left != std::string::npos &&
						is_paste(macro->replacement[left])) ||
						(right != std::string::npos &&
						is_paste(macro->replacement[right])))
						units.push_back(SubstitutionUnit::placemarker_unit(
							parameter_index == macro->parameters.size() &&
							macro->variadic));
				}
				else
				{
					for (std::size_t j = 0; j < value.size(); ++j)
					{
						PaintedToken substituted = value[j];
						substituted.paint = parameter_paint(
							substituted, head.paint, current_macro_paint);
						units.push_back(SubstitutionUnit::token_unit(
							substituted));
					}
				}
				continue;
			}

			units.push_back(SubstitutionUnit::token_unit(
				PaintedToken(token, base_paint, true)));
		}

		apply_pastes(&units);
		for (std::size_t i = 0; i < units.size(); ++i)
		{
			if (!units[i].paste_operator && !units[i].placemarker)
				replacement->push_back(units[i].token);
		}
	}

	void apply_pastes(std::vector<SubstitutionUnit>* units)
	{
		std::vector<SubstitutionUnit> reduced;
		reduced.reserve(units->size());
		std::size_t i = 0;
		while (i < units->size())
		{
			if (!(*units)[i].paste_operator)
			{
				reduced.push_back((*units)[i]);
				++i;
				continue;
			}

			while (!reduced.empty() &&
				is_whitespace(reduced.back().token.token))
				reduced.pop_back();
			if (reduced.empty())
				fail("invalid token paste operands");

			std::size_t right = i + 1;
			while (right < units->size() &&
				is_whitespace((*units)[right].token.token))
				++right;
			if (right == units->size() || (*units)[right].paste_operator)
				fail("invalid token paste operands");

			const SubstitutionUnit left_unit = reduced.back();
			const SubstitutionUnit right_unit = (*units)[right];
			SubstitutionUnit result;
			if (left_unit.placemarker && right_unit.placemarker)
				result = SubstitutionUnit::placemarker_unit(
					left_unit.variadic_placemarker ||
					right_unit.variadic_placemarker);
			else if (right_unit.placemarker &&
				right_unit.variadic_placemarker &&
				!left_unit.placemarker &&
				left_unit.token.token.spelling == ",")
			{
				// GNU's checked-in extension gives `, ##__VA_ARGS__` a
				// distinct empty-pack meaning: remove the comma.  The
				// provenance bit prevents ordinary empty fixed arguments
				// from inheriting this behavior.
				result = SubstitutionUnit::placemarker_unit();
			}
			else if (left_unit.placemarker)
			{
				result = right_unit;
			}
			else if (right_unit.placemarker)
			{
				result = left_unit;
			}
			else
			{
				const std::string pasted = left_unit.token.token.spelling +
					right_unit.token.token.spelling;
				std::vector<PaintedToken> retokenized =
					retokenize_one(pasted);
				result = SubstitutionUnit::token_unit(retokenized[0]);
				result.token.from_replacement = true;
				result.token.paint = merge_paint(
					left_unit.token.paint, right_unit.token.paint);
			}
			reduced.back() = result;
			i = right + 1;
		}
		units->swap(reduced);
	}
};

} // namespace

void preprocess_cpp_source(const std::string& source,
	std::vector<PPToken>* output)
{
	if (output == NULL)
		throw std::invalid_argument("null macro output");
	MacroProcessor processor;
	processor.process(source, output);
}
