#pragma once

#include <string>

#include "IPPTokenStream.h"

// Execute PA1's physical-source through translation phases 1--3 and emit the
// resulting preprocessing-token stream through the supplied adapter.
void tokenize_cpp_source(const std::string& source, IPPTokenStream& output);
