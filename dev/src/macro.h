#pragma once

#include <string>
#include <vector>

#include "IPPTokenStream.h"

// Execute PA4's directive parsing and macro replacement.  The returned
// buffer is still a typed preprocessing-token stream; PA4's caller passes it
// directly to posttokenize_cpp_tokens for phases 5--7.  The buffer owns the
// arbitrary spellings referenced by its token IDs.
void preprocess_cpp_source(const std::string& source,
	PPTokenBuffer* output);
