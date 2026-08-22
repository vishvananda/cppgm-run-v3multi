#pragma once

#include <string>
#include <vector>

#include "IPPTokenStream.h"

// Execute PA4's directive parsing and macro replacement.  The returned
// vector is still a typed preprocessing-token stream; PA4's caller passes it
// directly to posttokenize_cpp_tokens for phases 5--7.
void preprocess_cpp_source(const std::string& source,
	std::vector<PPToken>* output);
