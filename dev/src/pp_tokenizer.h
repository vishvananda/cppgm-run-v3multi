#pragma once

#include <string>

#include "IPPTokenStream.h"

// Execute PA1's physical-source through translation phases 1--3 and emit the
// resulting preprocessing-token stream through the supplied adapter.
void tokenize_cpp_source(const std::string& source, IPPTokenStream& output);

// The typed producer seam used by PA4 and later stages.  It preserves the
// tokenizer's fixed identities and interns arbitrary spellings in the caller's
// arena without passing through a rendered token format.
void tokenize_cpp_source_to_tokens(const std::string& source,
	PPSpellingTable& spellings, std::vector<PPToken>* output);
