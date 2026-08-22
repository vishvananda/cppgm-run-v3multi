#pragma once

#include <iosfwd>

// Run PA3's controlling-expression adapter on one source stream.  The
// implementation consumes phases 1--3 and the shared PA2 typed post-token
// conversion; this function owns all PA3 parsing and evaluation policy.
int run_ctrlexpr(std::istream& input, std::ostream& output,
	std::ostream& errors);
