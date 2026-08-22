#include "macro.h"

#include <algorithm>
#include <climits>
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
	PPSpellingTable& spellings;
	std::vector<PPToken> tokens;

	explicit TokenCollector(PPSpellingTable& spellings)
		: spellings(spellings), tokens()
	{}

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
		push(PPTokenKind::HeaderName, data);
	}

	void emit_identifier(const std::string& data)
	{
		push(PPTokenKind::Identifier, data);
	}

	void emit_identifier_as_preprocessing_op_or_punc(
		PPTokenFixedIdentity fixed_identity, const std::string& data)
	{
		tokens.push_back(PPToken(
			PPTokenKind::IdentifierAsPreprocessingOpOrPunc,
			spellings.intern(data), fixed_identity));
	}

	void emit_identifier_as_preprocessing_op_or_punc(const std::string& data)
	{
		push(PPTokenKind::IdentifierAsPreprocessingOpOrPunc, data);
	}

	void emit_pp_number(const std::string& data)
	{
		push(PPTokenKind::PPNumber, data);
	}

	void emit_character_literal(const std::string& data)
	{
		push(PPTokenKind::CharacterLiteral, data);
	}

	void emit_user_defined_character_literal(const std::string& data)
	{
		push(PPTokenKind::UserDefinedCharacterLiteral, data);
	}

	void emit_string_literal(const std::string& data)
	{
		push(PPTokenKind::StringLiteral, data);
	}

	void emit_user_defined_string_literal(const std::string& data)
	{
		push(PPTokenKind::UserDefinedStringLiteral, data);
	}

	void emit_preprocessing_op_or_punc(const std::string& data)
	{
		(void)data;
		throw std::runtime_error("untyped punctuator callback");
	}

	void emit_punctuator(PPTokenFixedIdentity fixed_identity,
		const std::string& data)
	{
		tokens.push_back(PPToken(PPTokenKind::Punctuator,
			spellings.intern(data), fixed_identity));
	}

	void emit_non_whitespace_char(const std::string& data)
	{
		push(PPTokenKind::NonWhitespaceCharacter, data);
	}

	void emit_eof()
	{
		tokens.push_back(PPToken(PPTokenKind::EndOfFile));
	}

private:
	void push(PPTokenKind kind, const std::string& data)
	{
		tokens.push_back(PPToken(kind, spellings.intern(data)));
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
	PPSpellingId name;
	std::size_t id;
	bool function_like;
	bool variadic;
	std::vector<PPSpellingId> parameters;
	std::unordered_map<PPSpellingId, std::size_t> parameter_ids;
	std::vector<PPToken> replacement;

	MacroDefinition()
		: name(0), id(0), function_like(false), variadic(false),
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

// Persistent binary trie for token-local unavailable macro sets.  Every
// update copies only the fixed-width path for one macro ID; no paint record
// owns or copies the whole active set.  Root zero is the empty set and leaf
// keys are non-zero macro IDs.
struct PaintTrieNode
{
	std::size_t key;
	std::size_t left;
	std::size_t right;

	PaintTrieNode(std::size_t key = 0, std::size_t left = 0,
		std::size_t right = 0)
		: key(key), left(left), right(right)
	{}
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

bool is_punctuator(const PPToken& token, PPTokenFixedIdentity punctuator)
{
	return token.kind == PPTokenKind::Punctuator &&
		token.fixed_identity == punctuator;
}

bool is_hash(const PPToken& token)
{
	return is_punctuator(token, PPTokenFixedIdentity::Hash);
}

bool is_paste(const PPToken& token)
{
	return is_punctuator(token, PPTokenFixedIdentity::HashHash);
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
	explicit MacroProcessor(PPSpellingTable& spellings)
		: spellings_(spellings),
		  define_id_(spellings.intern("define")),
		  undef_id_(spellings.intern("undef")),
		  va_args_id_(spellings.intern("__VA_ARGS__")),
		  macros_(), next_macro_id_(1), paint_nodes_(1, PaintTrieNode())
	{
	}

	void process(const std::string& source, std::vector<PPToken>* output)
	{
		TokenCollector collector(spellings_);
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
	PPSpellingTable& spellings_;
	PPSpellingId define_id_;
	PPSpellingId undef_id_;
	PPSpellingId va_args_id_;
	std::unordered_map<PPSpellingId, MacroDefinition> macros_;
	std::size_t next_macro_id_;
	std::vector<PaintTrieNode> paint_nodes_;

	bool is_parameter(const MacroDefinition& macro,
		const PPToken& token, std::size_t* parameter_index)
	{
		if (!is_identifier(token))
			return false;
		std::unordered_map<PPSpellingId, std::size_t>::const_iterator found =
			macro.parameter_ids.find(token.spelling);
		if (found != macro.parameter_ids.end())
		{
			if (parameter_index != NULL)
				*parameter_index = found->second;
			return true;
		}
		if (macro.variadic && token.spelling == va_args_id_)
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
		if (left.size() != right.size())
			return false;
		for (std::size_t i = 0; i < left.size(); ++i)
		{
			const PPTokenKind left_kind = is_whitespace(left[i]) ?
				PPTokenKind::WhitespaceSequence : left[i].kind;
			const PPTokenKind right_kind = is_whitespace(right[i]) ?
				PPTokenKind::WhitespaceSequence : right[i].kind;
			if (left_kind != right_kind ||
				left[i].fixed_identity != right[i].fixed_identity ||
				left[i].spelling != right[i].spelling)
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

	static unsigned paint_bits()
	{
		return static_cast<unsigned>(sizeof(std::size_t) * CHAR_BIT);
	}

	std::size_t branch(std::size_t left, std::size_t right)
	{
		if (left == 0 && right == 0)
			return 0;
		paint_nodes_.push_back(PaintTrieNode(0, left, right));
		return paint_nodes_.size() - 1;
	}

	std::size_t insert_paint(std::size_t root, std::size_t macro_id,
		unsigned depth)
	{
		if (depth == paint_bits())
		{
			if (root != 0 && paint_nodes_[root].key == macro_id)
				return root;
			paint_nodes_.push_back(PaintTrieNode(macro_id));
			return paint_nodes_.size() - 1;
		}

		const unsigned shift = paint_bits() - depth - 1;
		const bool high = ((macro_id >> shift) & 1u) != 0;
		const std::size_t old_child = root == 0 ? 0 :
			(high ? paint_nodes_[root].right : paint_nodes_[root].left);
		const std::size_t new_child = insert_paint(old_child, macro_id,
			depth + 1);
		const std::size_t old_left = root == 0 ? 0 : paint_nodes_[root].left;
		const std::size_t old_right = root == 0 ? 0 : paint_nodes_[root].right;
		return high ? branch(old_left, new_child) :
			branch(new_child, old_right);
	}

	std::size_t union_paint(std::size_t left, std::size_t right,
		unsigned depth)
	{
		if (left == 0)
			return right;
		if (right == 0 || left == right)
			return left;
		if (depth == paint_bits())
			return paint_nodes_[left].key == paint_nodes_[right].key ?
				left : 0;
		return branch(
			union_paint(paint_nodes_[left].left,
				paint_nodes_[right].left, depth + 1),
			union_paint(paint_nodes_[left].right,
				paint_nodes_[right].right, depth + 1));
	}

	std::size_t difference_paint(std::size_t left, std::size_t right,
		unsigned depth)
	{
		if (left == 0 || right == 0 || left == right)
			return left == right ? 0 : left;
		if (depth == paint_bits())
			return paint_nodes_[left].key == paint_nodes_[right].key ? 0 : left;
		return branch(
			difference_paint(paint_nodes_[left].left,
				paint_nodes_[right].left, depth + 1),
				difference_paint(paint_nodes_[left].right,
					paint_nodes_[right].right, depth + 1));
	}

	std::size_t maximum_paint(std::size_t root) const
	{
		while (root != 0)
		{
			const PaintTrieNode& node = paint_nodes_[root];
			if (node.key != 0)
				return node.key;
			root = node.right != 0 ? node.right : node.left;
		}
		return 0;
	}

	std::size_t add_paint(std::size_t inherited, std::size_t macro_id)
	{
		return painted(inherited, macro_id) ? inherited :
			insert_paint(inherited, macro_id, 0);
	}

	std::size_t merge_paint(std::size_t left, std::size_t right)
	{
		return union_paint(left, right, 0);
	}

	std::size_t parameter_paint(const PaintedToken& token,
		std::size_t inherited, std::size_t current_macro_paint)
	{
		if (token.deferred && !token.blocked)
		{
			// A deferred token that already carries paint from a parent
			// replacement has crossed a parameter boundary.  Drop the parent
			// portion of that paint, while retaining unrelated unavailable names.
			if (token.paint != 0)
				return difference_paint(token.paint, inherited, 0);
			if (inherited != 0)
			{
				const MacroDefinition* token_macro = is_identifier(token.token) ?
					find_macro(token.token.spelling) : NULL;
				if (token_macro != NULL &&
					token_macro->id == maximum_paint(inherited))
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
		for (unsigned depth = 0; paint != 0 && depth < paint_bits(); ++depth)
		{
			const PaintTrieNode& node = paint_nodes_[paint];
			if (node.key != 0)
				return node.key == macro_id;
			const unsigned shift = paint_bits() - depth - 1;
			paint = ((macro_id >> shift) & 1u) != 0 ?
				node.right : node.left;
		}
		return paint != 0 && paint_nodes_[paint].key == macro_id;
	}

	const MacroDefinition* find_macro(PPSpellingId name) const
	{
		std::unordered_map<PPSpellingId, MacroDefinition>::const_iterator found =
			macros_.find(name);
		return found == macros_.end() ? NULL : &found->second;
	}

	const std::string& spelling(const PPToken& token) const
	{
		return spellings_.get(token.spelling);
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
		const PPSpellingId directive = tokens[at].spelling;
		++at;
		if (directive == define_id_)
			parse_define(tokens, line_begin, line_end, at);
		else if (directive == undef_id_)
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
			tokens[at].spelling == va_args_id_)
			fail("invalid undef macro name");
		const PPSpellingId name = tokens[at].spelling;
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
			tokens[at].spelling == va_args_id_)
			fail("invalid define macro name");

		MacroDefinition definition;
		definition.name = tokens[at].spelling;
		++at;

		if (at < line_end && is_punctuator(tokens[at],
			PPTokenFixedIdentity::LeftParen))
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

		std::unordered_map<PPSpellingId, MacroDefinition>::const_iterator old =
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
		if (*at < end && is_punctuator(tokens[*at],
			PPTokenFixedIdentity::RightParen))
		{
			++*at;
			return;
		}

		while (true)
		{
			*at = skip_whitespace(tokens, *at, end);
			if (*at >= end)
				fail("unterminated macro parameter list");

			if (is_punctuator(tokens[*at], PPTokenFixedIdentity::Ellipsis))
			{
				if (definition->variadic)
					fail("duplicate variadic marker");
				definition->variadic = true;
				++*at;
				*at = skip_whitespace(tokens, *at, end);
				if (*at >= end || !is_punctuator(tokens[*at],
					PPTokenFixedIdentity::RightParen))
					fail("variadic marker must end parameter list");
				++*at;
				return;
			}

			if (!is_identifier(tokens[*at]) ||
				tokens[*at].spelling == va_args_id_)
				fail("invalid macro parameter");
			for (std::size_t i = 0; i < definition->parameters.size(); ++i)
				if (definition->parameters[i] == tokens[*at].spelling)
					fail("duplicate macro parameter");
			const std::size_t parameter_index = definition->parameters.size();
			definition->parameters.push_back(tokens[*at].spelling);
			definition->parameter_ids[tokens[*at].spelling] = parameter_index;
			++*at;
			*at = skip_whitespace(tokens, *at, end);

			if (*at < end && is_punctuator(tokens[*at],
				PPTokenFixedIdentity::RightParen))
			{
				++*at;
				return;
			}
			if (*at >= end || !is_punctuator(tokens[*at],
				PPTokenFixedIdentity::Comma))
				fail("missing comma in macro parameter list");
			++*at;
			*at = skip_whitespace(tokens, *at, end);
			if (*at < end && is_punctuator(tokens[*at],
				PPTokenFixedIdentity::RightParen))
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
				token.spelling == va_args_id_ && !definition.variadic)
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
				current.token.spelling == va_args_id_)
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
		return at != work->end() && is_punctuator(at->token,
			PPTokenFixedIdentity::LeftParen) &&
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
		if (work->empty() || !is_punctuator(work->front().token,
			PPTokenFixedIdentity::LeftParen))
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
			if (is_punctuator(token.token, PPTokenFixedIdentity::LeftParen))
			{
				++nesting;
				current.push_back(token);
			}
			else if (is_punctuator(token.token, PPTokenFixedIdentity::RightParen))
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
			else if (is_punctuator(token.token, PPTokenFixedIdentity::Comma) &&
				nesting == 0)
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
		std::size_t first)
	{
		std::vector<PaintedToken> result;
		for (std::size_t i = first; i < arguments.size(); ++i)
		{
			if (i != first)
			{
				result.push_back(PaintedToken(PPToken(PPTokenKind::Punctuator,
					spellings_.intern(","), PPTokenFixedIdentity::Comma)));
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
		TokenCollector collector(spellings_);
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
			const std::string& token_spelling = this->spelling(token);
			for (std::size_t j = 0; j < token_spelling.size(); ++j)
			{
				if (token_spelling[j] == '"' ||
					(token_spelling[j] == '\\' && literal))
					spelling.push_back('\\');
				spelling.push_back(token_spelling[j]);
			}
			emitted = true;
		}
		spelling.push_back('"');
		// The generated literal is already a classified preprocessing token.
		// Retokenizing it would apply phase-1 trigraph replacement to text that
		// came from a raw-string token, changing the required stringized spelling.
		return std::vector<PaintedToken>(1, PaintedToken(
			PPToken(PPTokenKind::StringLiteral,
				spellings_.intern(spelling)), paint));
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
					is_punctuator(macro->replacement[left],
						PPTokenFixedIdentity::Comma) &&
					is_parameter(*macro, macro->replacement[right],
						&right_parameter) &&
					macro->replacement[right].spelling == va_args_id_ &&
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
				is_punctuator(left_unit.token.token, PPTokenFixedIdentity::Comma))
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
				const std::string pasted = spelling(left_unit.token.token) +
					spelling(right_unit.token.token);
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
	PPTokenBuffer* output)
{
	if (output == NULL)
		throw std::invalid_argument("null macro output");
	output->clear();
	MacroProcessor processor(output->spellings);
	processor.process(source, &output->tokens);
}
