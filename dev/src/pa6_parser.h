#pragma once

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "pa6_recognizer.h"

namespace pa6_internal
{

class PA6Parser
{
public:
	explicit PA6Parser(const std::vector<PA6Token>& tokens)
		: tokens_(tokens), position_(0), angle_depth_(0), non_angle_depth_(0),
		  angle_bases_(),
		  work_(0), work_limit_(std::max<std::size_t>(10000,
			tokens.size() * 512)), exhausted_(false)
	{}

	bool parse(std::string* reason)
	{
		if (tokens_.empty() || tokens_.back().kind != PA6TokenKind::ST_EOF)
			return fail(reason, "missing EOF");
		if (!parse_translation_unit())
		{
			if (exhausted_)
				return fail(reason, "recognizer work bound exceeded");
			std::ostringstream message;
			message << "translation unit does not match PA6 grammar at token "
				<< position_;
			if (position_ < tokens_.size())
			{
				if (tokens_[position_].kind == PA6TokenKind::Fixed)
					message << " (" << simple_token_type_name(tokens_[position_].fixed)
						<< ")";
				else
					message << " (kind " << static_cast<int>(tokens_[position_].kind)
						<< ")";
			}
			if (reason != NULL)
				*reason = message.str();
			return false;
		}
		return true;
	}

private:
	struct Mark
	{
		std::size_t position;
		std::size_t angle_depth;
		std::size_t non_angle_depth;
		std::size_t angle_base_count;
	};

	const std::vector<PA6Token>& tokens_;
	std::size_t position_;
	std::size_t angle_depth_;
	std::size_t non_angle_depth_;
	std::vector<std::size_t> angle_bases_;
	std::size_t work_;
	std::size_t work_limit_;
	bool exhausted_;

	static const std::size_t kMaxNesting = 1024;

	bool fail(std::string* reason, const char* message) const
	{
		if (reason != NULL)
			*reason = message;
		return false;
	}

	bool tick()
	{
		if (work_ == work_limit_)
		{
			exhausted_ = true;
			return false;
		}
		++work_;
		return true;
	}

	Mark mark() const
	{
		Mark result = {position_, angle_depth_, non_angle_depth_,
			angle_bases_.size()};
		return result;
	}

	void restore(const Mark& saved)
	{
		position_ = saved.position;
		angle_depth_ = saved.angle_depth;
		non_angle_depth_ = saved.non_angle_depth;
		angle_bases_.resize(saved.angle_base_count);
	}

	bool eof() const
	{
		return position_ >= tokens_.size() ||
			tokens_[position_].kind == PA6TokenKind::ST_EOF;
	}

	const PA6Token* look(std::size_t offset = 0) const
	{
		if (position_ + offset >= tokens_.size())
			return NULL;
		return &tokens_[position_ + offset];
	}

	bool kind(PA6TokenKind wanted, std::size_t offset = 0) const
	{
		const PA6Token* token = look(offset);
		return token != NULL && token->kind == wanted;
	}

	bool fixed(SimpleTokenType wanted, std::size_t offset = 0) const
	{
		const PA6Token* token = look(offset);
		return token != NULL && token->kind == PA6TokenKind::Fixed &&
			token->fixed == wanted;
	}

	bool identifier(std::size_t offset = 0) const
	{
		const PA6Token* token = look(offset);
		return token != NULL &&
			(token->kind == PA6TokenKind::Identifier ||
			 token->kind == PA6TokenKind::ST_OVERRIDE ||
			 token->kind == PA6TokenKind::ST_FINAL);
	}

	bool literal(std::size_t offset = 0) const
	{
		const PA6Token* token = look(offset);
		return token != NULL &&
			(token->kind == PA6TokenKind::Literal ||
			 token->kind == PA6TokenKind::ST_EMPTYSTR ||
			 token->kind == PA6TokenKind::ST_ZERO);
	}

	bool category(unsigned int wanted, std::size_t offset = 0) const
	{
		const PA6Token* token = look(offset);
		return token != NULL && token->kind == PA6TokenKind::Identifier &&
			(token->name_categories & wanted) != 0;
	}

	bool consume_kind(PA6TokenKind wanted)
	{
		if (!kind(wanted) || !tick())
			return false;
		++position_;
		return true;
	}

	bool consume_fixed(SimpleTokenType wanted)
	{
		if (!fixed(wanted) || !tick())
			return false;
		++position_;
		return true;
	}

	bool consume_identifier()
	{
		if (!identifier() || !tick())
			return false;
		++position_;
		return true;
	}

	bool consume_literal()
	{
		if (!literal() || !tick())
			return false;
		++position_;
		return true;
	}

	bool begin_non_angle()
	{
		if (non_angle_depth_ == kMaxNesting)
		{
			exhausted_ = true;
			return false;
		}
		++non_angle_depth_;
		return true;
	}

	void end_non_angle()
	{
		if (non_angle_depth_ != 0)
			--non_angle_depth_;
	}

	bool begin_angle()
	{
		if (angle_depth_ == kMaxNesting)
		{
			exhausted_ = true;
			return false;
		}
		++angle_depth_;
		angle_bases_.push_back(non_angle_depth_);
		return true;
	}

	bool close_angle()
	{
		if (angle_depth_ == 0 ||
			(!fixed(SimpleTokenType::OP_GT) &&
			 !kind(PA6TokenKind::ST_RSHIFT_1) &&
			 !kind(PA6TokenKind::ST_RSHIFT_2)))
			return false;
		if (fixed(SimpleTokenType::OP_GT))
		{
			if (!consume_fixed(SimpleTokenType::OP_GT))
				return false;
		}
		else if (kind(PA6TokenKind::ST_RSHIFT_1))
		{
			if (!consume_kind(PA6TokenKind::ST_RSHIFT_1))
				return false;
		}
		else if (!consume_kind(PA6TokenKind::ST_RSHIFT_2))
			return false;
		--angle_depth_;
		angle_bases_.pop_back();
		return true;
	}

	bool shift_operator()
	{
		if (fixed(SimpleTokenType::OP_LSHIFT))
			return consume_fixed(SimpleTokenType::OP_LSHIFT);
		if (!kind(PA6TokenKind::ST_RSHIFT_1) ||
			!kind(PA6TokenKind::ST_RSHIFT_2, 1))
			return false;
		return consume_kind(PA6TokenKind::ST_RSHIFT_1) &&
			consume_kind(PA6TokenKind::ST_RSHIFT_2);
	}

	bool can_use_angle_operator() const
	{
		return angle_depth_ == 0 ||
			non_angle_depth_ > angle_bases_.back();
	}

	bool parse_translation_unit();
	bool parse_declaration();
	bool parse_block_declaration();
	bool parse_function_definition();
	bool parse_simple_declaration();
	bool parse_init_declarator_list();
	bool parse_init_declarator();
	bool parse_alias_declaration();
	bool parse_static_assert_declaration();
	bool parse_empty_declaration();
	bool parse_attribute_declaration();
	bool parse_asm_definition();
	bool parse_namespace_definition();
	bool parse_namespace_alias_definition();
	bool parse_using_declaration();
	bool parse_using_directive();
	bool parse_linkage_specification();
	bool parse_template_declaration();
	bool parse_explicit_instantiation();
	bool parse_explicit_specialization();

	bool parse_decl_specifier_seq();
	bool parse_type_specifier_seq();
	bool parse_type_specifier();
	bool parse_trailing_type_specifier();
	bool parse_simple_type_specifier();
	bool parse_elaborated_type_specifier();
	bool parse_class_specifier();
	bool parse_class_head();
	bool parse_class_head_name();
	bool parse_enum_specifier();
	bool parse_enum_head();
	bool parse_enum_key();
	bool parse_enum_base();
	bool parse_enumerator_list();
	bool parse_enumerator_definition();
	bool parse_opaque_enum_declaration();
	bool parse_namespace_body();
	bool parse_qualified_namespace_specifier();
	bool parse_storage_class_specifier();
	bool parse_function_specifier();
	bool parse_cv_qualifier();

	bool parse_type_name();
	bool parse_class_name();
	bool parse_enum_name();
	bool parse_typedef_name();
	bool parse_namespace_name();
	bool parse_template_name();
	bool parse_simple_template_id();
	bool parse_template_id();
	bool parse_template_argument_list();
	bool parse_template_argument_dots();
	bool parse_template_argument();
	bool parse_typename_specifier();
	bool parse_decltype_specifier();
	bool parse_nested_name_specifier();
	bool parse_nested_name_suffix();
	bool parse_qualified_id();
	bool parse_unqualified_id();
	bool parse_id_expression();
	bool parse_operator_function_id();
	bool parse_literal_operator_id();
	bool parse_conversion_function_id();
	bool parse_pseudo_destructor_name();

	bool parse_declarator(bool* has_function = NULL);
	bool parse_ptr_declarator(bool* has_function = NULL);
	bool parse_noptr_declarator(bool* has_function = NULL);
	bool parse_noptr_declarator_root(bool* has_function = NULL);
	bool parse_noptr_declarator_suffix(bool* has_function = NULL);
	bool parse_declarator_id();
	bool parse_ptr_operator();
	bool parse_parameters_and_qualifiers();
	bool parse_parameter_declaration_clause();
	bool parse_parameter_declaration_list();
	bool parse_parameter_declaration();
	bool parse_abstract_declarator();
	bool parse_ptr_abstract_declarator();
	bool parse_noptr_abstract_declarator();
	bool parse_abstract_pack_declarator();
	bool parse_trailing_return_type();

	bool parse_initializer();
	bool parse_brace_or_equal_initializer();
	bool parse_initializer_clause();
	bool parse_initializer_list();
	bool parse_initializer_clause_dots();
	bool parse_expression_list();
	bool parse_braced_init_list();

	bool parse_statement();
	bool parse_labeled_statement();
	bool parse_expression_statement();
	bool parse_compound_statement();
	bool parse_selection_statement();
	bool parse_condition_declaration();
	bool parse_condition();
	bool parse_iteration_statement();
	bool parse_for_init_statement();
	bool parse_for_range_declaration();
	bool parse_for_range_initializer();
	bool parse_jump_statement();
	bool parse_try_block();
	bool parse_function_try_block();
	bool parse_handler();
	bool parse_exception_declaration();

	bool parse_expression();
	bool parse_assignment_expression();
	bool parse_conditional_expression();
	bool parse_binary_expression(int level);
	bool parse_pm_expression();
	bool parse_cast_expression();
	bool parse_unary_expression();
	bool parse_postfix_expression();
	bool parse_postfix_root();
	bool parse_postfix_suffix();
	bool parse_primary_expression();
	bool parse_lambda_expression();
	bool parse_lambda_introducer();
	bool parse_lambda_capture();
	bool parse_capture_list();
	bool parse_capture();
	bool parse_lambda_declarator();
	bool parse_noexcept_expression();
	bool parse_new_expression();
	bool parse_new_placement();
	bool parse_new_type_id();
	bool parse_new_declarator();
	bool parse_noptr_new_declarator();
	bool parse_new_initializer();
	bool parse_delete_expression();
	bool parse_cast_operator();
	bool can_start_assignment_expression() const;
	bool parse_throw_expression();

	bool parse_attribute_specifier();
	bool parse_attribute_specifier_seq();
	bool parse_alignment_specifier();
	bool parse_attribute_list();
	bool parse_attribute_part();
	bool parse_attribute();
	bool parse_attribute_token();
	bool parse_attribute_argument_clause();
	bool parse_balanced_token();

	bool parse_class_member_declaration();
	bool parse_member_declarator_list();
	bool parse_member_declarator();
	bool parse_member_specification();
	bool parse_access_specifier();
	bool parse_class_or_decltype();
	bool parse_base_clause();
	bool parse_base_specifier_list();
	bool parse_base_specifier_dots();
	bool parse_base_specifier();
	bool parse_base_type_specifier();
	bool parse_virt_specifier();
	bool parse_pure_specifier();
	bool parse_ctor_initializer();
	bool parse_mem_initializer_list();
	bool parse_mem_initializer_dots();
	bool parse_mem_initializer();
	bool parse_mem_initializer_id();

	bool parse_operator_template_suffix();
	bool parse_template_parameter_list();
	bool parse_template_parameter();
	bool parse_type_parameter();
	bool parse_function_body();
	bool parse_exception_specification();
	bool parse_dynamic_exception_specification();
	bool parse_type_id_list();
	bool parse_type_id_dots();
	bool parse_noexcept_specification();
	bool parse_type_id();
	bool parse_constant_expression();
};

} // namespace pa6_internal
