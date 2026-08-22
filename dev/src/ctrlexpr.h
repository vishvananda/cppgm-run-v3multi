#pragma once

#include <cstdint>
#include <iosfwd>

#include "IPPTokenStream.h"

struct PPControlExpressionValue
{
	std::uint64_t bits;
	bool is_unsigned;
	bool valid;

	PPControlExpressionValue(std::uint64_t bits = 0,
		bool is_unsigned = false, bool valid = true)
		: bits(bits), is_unsigned(is_unsigned), valid(valid)
	{}
};

// The callback answers the PA5 meaning of `defined` for an arbitrary
// identifier spelling.  The expression itself remains a typed PPTokenBuffer;
// the spelling is exposed only at this identifier lookup boundary.
typedef bool (*PPControlMacroDefined)(void* context,
	const std::string& spelling);
// PA5's typed path uses the existing spelling identity directly, avoiding a
// render-and-relookup cycle for each `defined` operand.
typedef bool (*PPControlMacroDefinedId)(void* context,
	PPSpellingId spelling);

bool evaluate_cpp_control_expression(const PPTokenBuffer& tokens,
	PPControlMacroDefined macro_defined, void* context,
	PPControlExpressionValue* result);
bool evaluate_cpp_control_expression(const PPSpellingTable& spellings,
	const std::vector<PPToken>& tokens, PPControlMacroDefined macro_defined,
	void* context, PPControlExpressionValue* result);
bool evaluate_cpp_control_expression_ids(const PPSpellingTable& spellings,
	const std::vector<PPToken>& tokens, PPControlMacroDefinedId macro_defined,
	void* context, PPControlExpressionValue* result);

// Run PA3's controlling-expression adapter on one source stream.  The
// implementation consumes phases 1--3 and the shared PA2 typed post-token
// conversion; this function owns all PA3 parsing and evaluation policy.
int run_ctrlexpr(std::istream& input, std::ostream& output,
	std::ostream& errors);
