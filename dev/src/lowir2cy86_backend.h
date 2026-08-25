#pragma once

#include <string>
#include <vector>

namespace lowir2cy86
{

// Parse, validate, and lower one command-line LowIR program to CY86 text.
// The output file is only replaced after parsing and validation succeed.
void compile(const std::vector<std::string>& source_files,
             const std::string& output_file);

} // namespace lowir2cy86
