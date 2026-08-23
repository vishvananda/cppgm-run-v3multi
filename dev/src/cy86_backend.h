#pragma once

#include <string>
#include <vector>

// Compile one PA9 command line.  The function throws std::exception on an
// invalid source or output failure; the executable adapter owns diagnostics.
int cy86_compile(const std::vector<std::string>& args);
