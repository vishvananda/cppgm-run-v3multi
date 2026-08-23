#pragma once

#include <cstddef>
#include <vector>

#include "pa10_ast.h"

namespace PA10ParserSupport
{

bool collect_tokens(const PPTokenBuffer& input, std::vector<PA10Token>& tokens);
bool is_cv(SimpleTokenType type);
bool is_type_keyword(SimpleTokenType type);
bool is_operator_function_token(SimpleTokenType type);

void build_indexes(const std::vector<PA10Token>& tokens,
	std::vector<std::size_t>& template_close_index,
	std::vector<unsigned char>& template_top_level_or,
	std::vector<unsigned char>& rshift_piece1_nested_close,
	std::vector<std::size_t>& delimiter_close_index);

bool special_member_definition_start(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t position, bool in_class_member, std::size_t* charged_work);

bool skip_attribute_specifiers(const std::vector<PA10Token>& tokens,
	std::size_t position, std::size_t* after, std::size_t* consumed);

} // namespace PA10ParserSupport
