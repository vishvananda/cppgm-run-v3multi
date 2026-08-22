#pragma once

#include <string>
#include <vector>

#include "IPPTokenStream.h"

// A builtin may produce one already-classified token at expansion time.  The
// callback is intentionally a typed seam: the macro owner never renders a
// token to text and re-tokenizes it.
typedef bool (*PPMacroBuiltinResolver)(void* context,
	const PPToken& invocation, PPToken* replacement);

class PPMacroSession
{
public:
	explicit PPMacroSession(PPSpellingTable& spellings);
	~PPMacroSession();

	// `after_directive` points just after the `define` or `undef` identifier in
	// one logical directive line.
	void define(const std::vector<PPToken>& tokens, std::size_t line_begin,
		std::size_t line_end, std::size_t after_directive);
	void undef(const std::vector<PPToken>& tokens, std::size_t line_begin,
		std::size_t line_end, std::size_t after_directive);
	void undef(PPSpellingId name);
	void expand(const std::vector<PPToken>& input,
		std::vector<PPToken>* output);
	void expand_control(const std::vector<PPToken>& input,
		std::vector<PPToken>* output);

	bool is_defined(PPSpellingId name) const;
	void register_builtin(PPSpellingId name);
	void set_builtin_resolver(PPMacroBuiltinResolver resolver, void* context);

private:
	struct Impl;
	Impl* impl_;

	PPMacroSession(const PPMacroSession&);
	PPMacroSession& operator=(const PPMacroSession&);
};

// Execute PA4's directive parsing and macro replacement.  The returned
// buffer is still a typed preprocessing-token stream; PA4's caller passes it
// directly to posttokenize_cpp_tokens for phases 5--7.  The buffer owns the
// arbitrary spellings referenced by its token IDs.
void preprocess_cpp_source(const std::string& source,
	PPTokenBuffer* output);
