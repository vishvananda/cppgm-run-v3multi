#pragma once

#include <cstddef>
#include <vector>

#include "pa10_ast.h"

namespace PA10ParserSupport
{

bool collect_tokens(const PPTokenBuffer& input, std::vector<PA10Token>& tokens);
bool is_cv(SimpleTokenType type);
bool is_type_keyword(SimpleTokenType type);
bool is_decl_specifier(SimpleTokenType type);
bool is_assignment_operator(SimpleTokenType type);
bool is_binary_operator(int level, SimpleTokenType type);
bool declaration_follow_is_valid(const std::vector<PA10Token>& tokens,
	std::size_t close);
bool virt_specifier_start(const std::vector<PA10Token>& tokens,
	std::size_t position);
// The PA10 simple-type-specifier domain used by a function-style cast.  It
// intentionally excludes KW_AUTO, which is a declaration specifier but is
// not a simple-type-specifier in pa10.gram.
bool is_builtin_function_style_cast_keyword(SimpleTokenType type);
bool is_operator_function_token(SimpleTokenType type);

enum class PA10FunctionStyleCastKind : unsigned char
{
	None,
	LegacyBuiltin,
	TypeId
};

struct PA10FunctionStyleCastClassification
{
	PA10FunctionStyleCastKind kind;
	std::size_t consumed;
	std::size_t charged_work;

	PA10FunctionStyleCastClassification()
		: kind(PA10FunctionStyleCastKind::None), consumed(0),
		  charged_work(0)
	{}
};

// Classify the function-style cast prefix beginning at position.  Built-in
// and cv specifiers are scanned once; decltype uses the indexed delimiter
// table.  consumed and charged_work are published for every result so the
// parser can charge this bounded classification exactly once.
PA10FunctionStyleCastClassification classify_function_style_cast(
	const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t position);

struct PA10LambdaIntroducerFacts
{
	PA10LambdaCaptureDefault capture_default;
	std::vector<PA10LambdaCapture> captures;
	std::size_t consumed;
	std::size_t charged_work;

	PA10LambdaIntroducerFacts()
		: capture_default(PA10LambdaCaptureDefault::None), captures(),
		  consumed(0), charged_work(0)
	{}
};

// Scan only the lambda-introducer production beginning immediately after the
// opening `[`.  The result is typed capture/default/pack data; consumed and
// charged_work are published on both success and failure so the parser can
// account for this one bounded forward pass.
bool scan_lambda_introducer_facts(
	const std::vector<PA10Token>& tokens, std::size_t position,
	PA10LambdaIntroducerFacts* facts);

bool find_template_close(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	std::size_t absolute_lt, std::size_t* absolute_close);
bool template_follow_is_valid(
	const std::vector<PA10Token>& tokens,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	std::size_t absolute_close, std::size_t* charged_work);

// One indexed fact for each parenthesized delimiter group.  The same fact is
// consumed by declaration/declarator routing and by new-expression type-id
// parsing; it is not an ownership claim for either parser context.
enum class PA10ParenthesizedGroupKind : unsigned char
{
	None,
	AbstractDeclarator,
	ParameterClause,
	// A nested parameter-shaped group retained for parenthesized type-ids.
	NestedParameter,
	// A pointer-led group with a named declarator-id.
	NamedDeclarator
};

// These indexed new-expression facts are pure token-shape predicates.  The
// parser owns consumption; the support module owns only the bounded routing
// facts and the one charged placement probe.
bool new_type_id_start_at(const std::vector<PA10Token>& tokens,
	std::size_t absolute);
bool parenthesized_type_id_start_at(
	const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t absolute);
bool new_parenthesized_abstract_declarator_start(
	const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& delimiter_close_index,
	const std::vector<PA10ParenthesizedGroupKind>& parenthesized_group_kind,
	std::size_t position, bool parenthesized_new_type_id);
bool new_first_parenthesized_group_is_placement(
	const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t position, std::size_t* charged_work);
bool member_pointer_operator_start(
	const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	std::size_t position, std::size_t* charged_work);

// build_indexes sizes and clears every output index to its sentinel/default;
// an uninitialized or reused side index is not a valid caller-owned state.
// The return value is the exact counted index-work total (the ordinary index
// pass plus the parenthesized-group fact pass); the parser charges it against
// its global work limit.
std::size_t build_indexes(const std::vector<PA10Token>& tokens,
	std::vector<std::size_t>& template_close_index,
	std::vector<unsigned char>& template_top_level_or,
	std::vector<unsigned char>& template_top_level_comma,
	std::vector<unsigned char>& rshift_piece1_nested_close,
	std::vector<std::size_t>& delimiter_close_index,
	std::vector<PA10ParenthesizedGroupKind>& parenthesized_group_kind);

bool special_member_definition_start(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t position, bool in_class_member, std::size_t* charged_work);

// after and consumed are initialized and published on both success and
// failure.  consumed is the number of token positions examined by the
// bounded attribute scan, including a present failing token.  Standard
// attributes must have a delimiter-indexed [[...]] wrapper; GNU and alignas
// retain their existing parenthesis-owned paths.
bool skip_attribute_specifiers(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t position, std::size_t* after, std::size_t* consumed);
bool attribute_specifier_start(const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t position);

enum class PA10ElaboratedSpecifierContext
{
	NonElaborated,
	EmbeddedOrDeclarator,
	StandaloneForward,
	StandaloneDefinition
};

struct PA10ElaboratedSpecifierClassification
{
	PA10ElaboratedSpecifierContext context;
	bool has_body;
	bool has_colon_clause;
	std::size_t charged_work;

	PA10ElaboratedSpecifierClassification()
		: context(PA10ElaboratedSpecifierContext::NonElaborated),
		  has_body(false), has_colon_clause(false), charged_work(0)
	{}
};

// Classify only the current elaborated-specifier header and its immediate
// declaration delimiter.  charged_work is the exact number of bounded
// lookahead steps the caller must charge against its parser work limit.
PA10ElaboratedSpecifierClassification classify_elaborated_specifier(
	const std::vector<PA10Token>& tokens,
	const std::vector<std::size_t>& template_close_index,
	const std::vector<unsigned char>& rshift_piece1_nested_close,
	const std::vector<std::size_t>& delimiter_close_index,
	std::size_t position);

} // namespace PA10ParserSupport
