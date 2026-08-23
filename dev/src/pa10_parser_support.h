#pragma once

#include <cstddef>
#include <vector>

#include "pa10_ast.h"

namespace PA10ParserSupport
{

bool collect_tokens(const PPTokenBuffer& input, std::vector<PA10Token>& tokens);
bool is_cv(SimpleTokenType type);
bool is_type_keyword(SimpleTokenType type);
// The PA10 simple-type-specifier domain used by a function-style cast.  It
// intentionally excludes KW_AUTO, which is a declaration specifier but is
// not a simple-type-specifier in pa10.gram.
bool is_builtin_function_style_cast_keyword(SimpleTokenType type);
bool is_operator_function_token(SimpleTokenType type);

// build_indexes sizes and clears every output index to tokens.size(); an
// uninitialized or reused side index is not a valid caller-owned state.  The
// return value is the exact counted index-work total (the ordinary index pass
// plus the new-expression fact pass); the parser charges it against its
// global work limit.
std::size_t build_indexes(const std::vector<PA10Token>& tokens,
	std::vector<std::size_t>& template_close_index,
	std::vector<unsigned char>& template_top_level_or,
	std::vector<unsigned char>& rshift_piece1_nested_close,
	std::vector<std::size_t>& delimiter_close_index,
	std::vector<unsigned char>& new_abstract_declarator_group);

bool special_member_definition_start(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t position, bool in_class_member, std::size_t* charged_work);

bool skip_attribute_specifiers(const std::vector<PA10Token>& tokens,
	std::size_t position, std::size_t* after, std::size_t* consumed);

} // namespace PA10ParserSupport
