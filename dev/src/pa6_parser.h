#pragma once

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "cpp_syntax_core.h"
#include "pa6_recognizer.h"

namespace pa6_internal
{

struct PA6SyntaxTraits
{
	static bool is_end(const PA6Token& token)
	{
		return token.kind == PA6TokenKind::ST_EOF;
	}

	static bool is_fixed(const PA6Token& token, SimpleTokenType wanted)
	{
		return token.kind == PA6TokenKind::Fixed && token.fixed == wanted;
	}

	static bool is_identifier(const PA6Token& token)
	{
		return token.kind == PA6TokenKind::Identifier ||
			token.kind == PA6TokenKind::ST_OVERRIDE ||
			token.kind == PA6TokenKind::ST_FINAL;
	}

	static bool is_literal(const PA6Token& token)
	{
		return token.kind == PA6TokenKind::Literal ||
			token.kind == PA6TokenKind::ST_EMPTYSTR ||
			token.kind == PA6TokenKind::ST_ZERO;
	}

	static std::size_t max_nesting()
	{
		return 1024;
	}

	static std::size_t work_limit_for(std::size_t token_count)
	{
		const std::size_t minimum = 10000;
		const std::size_t per_token = 512;
		const std::size_t maximum = std::numeric_limits<std::size_t>::max();
		if (token_count > maximum / per_token)
			return maximum;
		const std::size_t scaled = token_count * per_token;
		return scaled < minimum ? minimum : scaled;
	}
};

class PA6Parser : private CppSyntaxCore<PA6Token, PA6SyntaxTraits>
{
public:
	explicit PA6Parser(const std::vector<PA6Token>& tokens);

	bool parse(std::string* reason);

private:
	enum TypeSpecifierClass
	{
		TypeSpecifierNonCv,
		TypeSpecifierCv
	};

	struct Mark
	{
		std::size_t position;
		std::size_t angle_depth;
		std::size_t non_angle_depth;
		std::size_t angle_base_count;
	};

	std::size_t angle_depth_;
	std::vector<std::size_t> angle_bases_;

	bool fail(std::string* reason, const char* message) const;
	bool restore_and_fail(const Mark& saved);
	bool tick();
	Mark mark() const;
	void restore(const Mark& saved);
	bool kind(PA6TokenKind wanted, std::size_t offset = 0) const;
	bool category(unsigned int wanted, std::size_t offset = 0) const;
	bool consume_kind(PA6TokenKind wanted);
	bool consume_fixed(SimpleTokenType wanted);
	bool consume_identifier();
	bool consume_literal();
	bool consume_current();
	bool begin_non_angle();
	void end_non_angle();
	bool begin_angle();
	bool close_angle();
	bool shift_operator();
	bool can_use_angle_operator() const;

	bool parse_translation_unit(), parse_declaration(), parse_block_declaration(),
		parse_function_definition(), parse_simple_declaration(),
		parse_init_declarator_list(), parse_init_declarator(),
		parse_alias_declaration(), parse_static_assert_declaration(),
		parse_empty_declaration(), parse_attribute_declaration(),
		parse_asm_definition(), parse_namespace_definition(),
		parse_namespace_alias_definition(), parse_using_declaration(),
		parse_using_directive(), parse_linkage_specification(),
		parse_template_declaration(), parse_explicit_instantiation(),
		parse_explicit_specialization();

	bool parse_decl_specifier_seq(), parse_type_specifier_seq();
	bool parse_type_specifier(TypeSpecifierClass* classification = NULL);
	bool parse_trailing_type_specifier(), parse_simple_type_specifier(),
		parse_elaborated_type_specifier(), parse_class_specifier(),
		parse_class_head(), parse_class_head_name(), parse_enum_specifier(),
		parse_enum_head(), parse_enum_key(), parse_enum_base(),
		parse_enumerator_list(), parse_enumerator_definition(),
		parse_opaque_enum_declaration(), parse_namespace_body(),
		parse_qualified_namespace_specifier(), parse_storage_class_specifier(),
		parse_function_specifier(), parse_cv_qualifier();

	bool parse_type_name(), parse_class_name(), parse_enum_name(),
		parse_typedef_name(), parse_namespace_name(), parse_template_name(),
		parse_simple_template_id(), parse_template_id(),
		parse_template_argument_list(), parse_template_argument_dots(),
		parse_template_argument(), parse_typename_specifier(),
		parse_decltype_specifier(), parse_nested_name_specifier(),
		parse_nested_name_suffix(), parse_qualified_id(), parse_unqualified_id(),
		parse_id_expression(), parse_operator_function_id(),
		parse_literal_operator_id(), parse_conversion_function_id(),
		parse_pseudo_destructor_name();

	bool parse_declarator(bool* has_function = NULL);
	bool parse_ptr_declarator(bool* has_function = NULL);
	bool parse_noptr_declarator(bool* has_function = NULL);
	bool parse_noptr_declarator_root(bool* has_function = NULL);
	bool parse_noptr_declarator_suffix(bool* has_function = NULL);
	bool parse_declarator_id(), parse_ptr_operator(),
		parse_parameters_and_qualifiers(), parse_parameter_declaration_clause(),
		parse_parameter_declaration_list(), parse_parameter_declaration(),
		parse_abstract_declarator(), parse_ptr_abstract_declarator(),
		parse_noptr_abstract_declarator(), parse_abstract_pack_declarator(),
		parse_trailing_return_type();

	bool parse_initializer(), parse_brace_or_equal_initializer(),
		parse_initializer_clause(), parse_initializer_list(),
		parse_initializer_clause_dots(), parse_expression_list(),
		parse_braced_init_list();

	bool parse_statement(), parse_labeled_statement(),
		parse_expression_statement(), parse_compound_statement(),
		parse_selection_statement(), parse_condition_declaration(),
		parse_condition(), parse_iteration_statement(), parse_for_init_statement(),
		parse_for_range_declaration(), parse_for_range_initializer(),
		parse_jump_statement(), parse_try_block(), parse_function_try_block(),
		parse_handler(), parse_exception_declaration();

	bool parse_expression(), parse_assignment_expression(),
		parse_conditional_expression();
	bool parse_binary_expression(int level);
	bool parse_pm_expression(), parse_cast_expression(), parse_unary_expression(),
		parse_postfix_expression(), parse_postfix_root(), parse_postfix_suffix(),
		parse_primary_expression(), parse_lambda_expression(),
		parse_lambda_introducer(), parse_lambda_capture(), parse_capture_list(),
		parse_capture(), parse_lambda_declarator(), parse_noexcept_expression(),
		parse_new_expression(), parse_new_placement(), parse_new_type_id(),
		parse_new_declarator(), parse_noptr_new_declarator(),
		parse_new_initializer(), parse_delete_expression(), parse_cast_operator();
	bool can_start_assignment_expression() const;
	bool parse_throw_expression();

	bool parse_attribute_specifier(), parse_attribute_specifier_seq(),
		parse_alignment_specifier(), parse_attribute_list(), parse_attribute_part(),
		parse_attribute(), parse_attribute_token(),
		parse_attribute_argument_clause(), parse_balanced_token();

	bool parse_class_member_declaration(), parse_member_declarator_list(),
		parse_member_declarator(), parse_member_specification(),
		parse_access_specifier(), parse_class_or_decltype(), parse_base_clause(),
		parse_base_specifier_list(), parse_base_specifier_dots(),
		parse_base_specifier(), parse_base_type_specifier(),
		parse_virt_specifier(), parse_pure_specifier(), parse_ctor_initializer(),
		parse_mem_initializer_list(), parse_mem_initializer_dots(),
		parse_mem_initializer(), parse_mem_initializer_id();

	bool parse_operator_template_suffix(), parse_template_parameter_list(),
		parse_template_parameter(), parse_type_parameter(), parse_function_body(),
		parse_exception_specification(), parse_dynamic_exception_specification(),
		parse_type_id_list(), parse_type_id_dots(), parse_noexcept_specification(),
		parse_type_id(), parse_constant_expression();
};

} // namespace pa6_internal
