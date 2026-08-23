#include "pa6_parser.h"

namespace pa6_internal
{

bool PA6Parser::parse_translation_unit()
{
	Mark saved = mark();
	while (!eof())
	{
		if (!tick() || !parse_declaration())
			return restore_and_fail(saved);
	}
	if (!consume_kind(PA6TokenKind::ST_EOF))
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_declaration()
{
	Mark saved = mark();
	if (parse_empty_declaration() || parse_attribute_declaration())
		return true;
	restore(saved);
	if (fixed(SimpleTokenType::KW_TEMPLATE))
	{
		Mark template_mark = mark();
		if (parse_explicit_specialization() || parse_template_declaration())
			return true;
		restore(template_mark);
		return false;
	}
	if (fixed(SimpleTokenType::KW_EXTERN) && literal(1))
	{
		if (parse_linkage_specification())
			return true;
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::KW_EXTERN) &&
		fixed(SimpleTokenType::KW_TEMPLATE, 1))
	{
		if (parse_explicit_instantiation())
			return true;
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::KW_NAMESPACE))
	{
		if (parse_namespace_alias_definition() || parse_namespace_definition())
			return true;
		restore(saved);
		return false;
	}
	if (parse_function_definition() || parse_block_declaration())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_block_declaration()
{
	Mark saved = mark();
	if (parse_alias_declaration() || parse_using_declaration() ||
		parse_using_directive() || parse_static_assert_declaration() ||
		parse_asm_definition() || parse_opaque_enum_declaration() ||
		parse_simple_declaration())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_function_definition()
{
	Mark saved = mark();
	if (!parse_attribute_specifier_seq() || !parse_decl_specifier_seq())
	{
		restore(saved);
		return false;
	}
	bool has_function = false;
	if (!parse_declarator(&has_function) || !has_function ||
		!parse_virt_specifier() || !parse_function_body())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_simple_declaration()
{
	Mark saved = mark();
	if (!parse_attribute_specifier_seq() || !parse_decl_specifier_seq())
	{
		restore(saved);
		return false;
	}
	if (!fixed(SimpleTokenType::OP_SEMICOLON))
	{
		if (!parse_init_declarator_list())
		{
			restore(saved);
			return false;
		}
	}
	if (!consume_fixed(SimpleTokenType::OP_SEMICOLON))
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_init_declarator_list()
{
	Mark saved = mark();
	if (!parse_init_declarator())
		return restore_and_fail(saved);
	while (fixed(SimpleTokenType::OP_COMMA))
	{
		if (!tick() || !consume_fixed(SimpleTokenType::OP_COMMA) ||
			!parse_init_declarator())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_init_declarator()
{
	Mark saved = mark();
	if (!parse_declarator() ||
		((fixed(SimpleTokenType::OP_ASS) ||
		  fixed(SimpleTokenType::OP_LPAREN) ||
		  fixed(SimpleTokenType::OP_LBRACE)) && !parse_initializer()))
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_alias_declaration()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_USING) || !consume_identifier() ||
		!parse_attribute_specifier_seq() ||
		!consume_fixed(SimpleTokenType::OP_ASS) || !parse_type_id() ||
		!consume_fixed(SimpleTokenType::OP_SEMICOLON))
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_static_assert_declaration()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_STATIC_ASSERT) ||
		!consume_fixed(SimpleTokenType::OP_LPAREN) ||
		!begin_non_angle() || !parse_constant_expression() ||
		!consume_fixed(SimpleTokenType::OP_COMMA) || !consume_literal() ||
		!consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	if (!consume_fixed(SimpleTokenType::OP_SEMICOLON))
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_empty_declaration()
{
	return consume_fixed(SimpleTokenType::OP_SEMICOLON);
}

bool PA6Parser::parse_attribute_declaration()
{
	Mark saved = mark();
	if (!parse_attribute_specifier() ||
		!parse_attribute_specifier_seq() ||
		!consume_fixed(SimpleTokenType::OP_SEMICOLON))
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_asm_definition()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_ASM) ||
		!consume_fixed(SimpleTokenType::OP_LPAREN) || !consume_literal() ||
		!consume_fixed(SimpleTokenType::OP_RPAREN) ||
		!consume_fixed(SimpleTokenType::OP_SEMICOLON))
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_namespace_definition()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::KW_INLINE) &&
		!consume_fixed(SimpleTokenType::KW_INLINE))
		return restore_and_fail(saved);
	if (!consume_fixed(SimpleTokenType::KW_NAMESPACE))
	{
		restore(saved);
		return false;
	}
	if (identifier() && !consume_identifier())
	{
		restore(saved);
		return false;
	}
	if (!consume_fixed(SimpleTokenType::OP_LBRACE) || !begin_non_angle() ||
		!parse_namespace_body() ||
		!consume_fixed(SimpleTokenType::OP_RBRACE))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	return true;
}

bool PA6Parser::parse_namespace_alias_definition()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_NAMESPACE) || !consume_identifier() ||
		!consume_fixed(SimpleTokenType::OP_ASS) ||
		!parse_qualified_namespace_specifier() ||
		!consume_fixed(SimpleTokenType::OP_SEMICOLON))
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_using_declaration()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_USING))
	{
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::KW_TYPENAME) &&
		!consume_fixed(SimpleTokenType::KW_TYPENAME))
	{
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::OP_COLON2))
	{
		if (!consume_fixed(SimpleTokenType::OP_COLON2) ||
			!parse_unqualified_id())
		{
			restore(saved);
			return false;
		}
	}
	else if (!parse_nested_name_specifier() || !parse_unqualified_id())
	{
		restore(saved);
		return false;
	}
	if (!consume_fixed(SimpleTokenType::OP_SEMICOLON))
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_using_directive()
{
	Mark saved = mark();
	if (!parse_attribute_specifier_seq() ||
		!consume_fixed(SimpleTokenType::KW_USING) ||
		!consume_fixed(SimpleTokenType::KW_NAMESPACE))
	{
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::OP_COLON2))
	{
		if (!consume_fixed(SimpleTokenType::OP_COLON2))
		{
			restore(saved);
			return false;
		}
	}
	else if (identifier() && fixed(SimpleTokenType::OP_COLON2, 1))
	{
		if (!parse_nested_name_specifier())
		{
			restore(saved);
			return false;
		}
	}
	if (!parse_namespace_name() ||
		!consume_fixed(SimpleTokenType::OP_SEMICOLON))
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_linkage_specification()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_EXTERN) || !consume_literal())
	{
		restore(saved);
		return false;
	}
	if (consume_fixed(SimpleTokenType::OP_LBRACE))
	{
		if (!begin_non_angle() || !parse_namespace_body() ||
			!consume_fixed(SimpleTokenType::OP_RBRACE))
		{
			restore(saved);
			return false;
		}
		end_non_angle();
		// The handout grammar omits this optional null declaration, but the
		// PA6 fixture uses the conventional `};` spelling for a linkage block.
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		return true;
	}
	if (!parse_declaration())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_template_declaration()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_TEMPLATE) ||
		!consume_fixed(SimpleTokenType::OP_LT) || !begin_angle() ||
		!parse_template_parameter_list() || !close_angle() ||
		!parse_declaration())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_explicit_instantiation()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::KW_EXTERN) &&
		!consume_fixed(SimpleTokenType::KW_EXTERN))
	{
		restore(saved);
		return false;
	}
	if (!consume_fixed(SimpleTokenType::KW_TEMPLATE) ||
		!parse_declaration())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_explicit_specialization()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_TEMPLATE) ||
		!consume_fixed(SimpleTokenType::OP_LT) || !begin_angle() ||
		!close_angle() || !parse_declaration())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_decl_specifier_seq()
{
	Mark saved = mark();
	bool any = false;
	bool has_non_cv_type = false;
	while (tick())
	{
		if (parse_storage_class_specifier() || parse_function_specifier() ||
			consume_fixed(SimpleTokenType::KW_FRIEND) ||
			consume_fixed(SimpleTokenType::KW_TYPEDEF) ||
			consume_fixed(SimpleTokenType::KW_CONSTEXPR))
		{
			any = true;
			continue;
		}
		if (parse_cv_qualifier())
		{
			any = true;
			continue;
		}
		if (has_non_cv_type && identifier() &&
			(category(PA6_NAME_CLASS) || category(PA6_NAME_ENUM) ||
			 category(PA6_NAME_TYPEDEF) || category(PA6_NAME_TEMPLATE)))
			break;
		Mark type_mark = mark();
		TypeSpecifierClass classification = TypeSpecifierNonCv;
		if (!parse_type_specifier(&classification))
		{
			restore(type_mark);
			break;
		}
		any = true;
		if (classification == TypeSpecifierNonCv)
			has_non_cv_type = true;
	}
	if (exhausted_)
		return restore_and_fail(saved);
	if (!any)
	{
		return restore_and_fail(saved);
	}
	parse_attribute_specifier_seq();
	return true;
}

bool PA6Parser::parse_type_specifier_seq()
{
	Mark saved = mark();
	bool any = false;
	bool has_non_cv_type = false;
	while (tick())
	{
		if (has_non_cv_type && identifier() &&
			(category(PA6_NAME_CLASS) || category(PA6_NAME_ENUM) ||
			 category(PA6_NAME_TYPEDEF) || category(PA6_NAME_TEMPLATE)))
			break;
		Mark type_mark = mark();
		TypeSpecifierClass classification = TypeSpecifierNonCv;
		if (!parse_type_specifier(&classification))
		{
			restore(type_mark);
			break;
		}
		any = true;
		if (classification == TypeSpecifierNonCv)
			has_non_cv_type = true;
	}
	if (exhausted_)
		return restore_and_fail(saved);
	if (!any)
	{
		return restore_and_fail(saved);
	}
	parse_attribute_specifier_seq();
	return true;
}

bool PA6Parser::parse_type_specifier(TypeSpecifierClass* classification)
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::KW_CLASS) || fixed(SimpleTokenType::KW_STRUCT) ||
		fixed(SimpleTokenType::KW_UNION))
	{
		if (parse_class_specifier() || parse_elaborated_type_specifier())
		{
			if (classification != NULL)
				*classification = TypeSpecifierNonCv;
			return true;
		}
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::KW_ENUM))
	{
		if (parse_enum_specifier() || parse_elaborated_type_specifier())
		{
			if (classification != NULL)
				*classification = TypeSpecifierNonCv;
			return true;
		}
		restore(saved);
		return false;
	}
	if (parse_typename_specifier() || parse_decltype_specifier() ||
		parse_simple_type_specifier())
	{
		if (classification != NULL)
			*classification = TypeSpecifierNonCv;
		return true;
	}
	if (parse_cv_qualifier())
	{
		if (classification != NULL)
			*classification = TypeSpecifierCv;
		return true;
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_trailing_type_specifier()
{
	return parse_simple_type_specifier() || parse_elaborated_type_specifier() ||
		parse_typename_specifier() || parse_cv_qualifier();
}

bool PA6Parser::parse_simple_type_specifier()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::KW_CHAR) || fixed(SimpleTokenType::KW_CHAR16_T) ||
		fixed(SimpleTokenType::KW_CHAR32_T) || fixed(SimpleTokenType::KW_WCHAR_T) ||
		fixed(SimpleTokenType::KW_BOOL) || fixed(SimpleTokenType::KW_SHORT) ||
		fixed(SimpleTokenType::KW_INT) || fixed(SimpleTokenType::KW_LONG) ||
		fixed(SimpleTokenType::KW_SIGNED) || fixed(SimpleTokenType::KW_UNSIGNED) ||
		fixed(SimpleTokenType::KW_FLOAT) || fixed(SimpleTokenType::KW_DOUBLE) ||
		fixed(SimpleTokenType::KW_VOID) || fixed(SimpleTokenType::KW_AUTO))
	{
		return consume_current();
	}
	if (fixed(SimpleTokenType::KW_DECLTYPE))
		return parse_decltype_specifier();
	if (fixed(SimpleTokenType::OP_COLON2) ||
		(identifier() && fixed(SimpleTokenType::OP_COLON2, 1)))
	{
		if (parse_nested_name_specifier() &&
			(fixed(SimpleTokenType::KW_TEMPLATE) ?
				(consume_fixed(SimpleTokenType::KW_TEMPLATE) &&
				 parse_simple_template_id()) : parse_type_name()))
			return true;
		restore(saved);
		return false;
	}
	if (parse_type_name())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_elaborated_type_specifier()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::KW_CLASS) || fixed(SimpleTokenType::KW_STRUCT) ||
		fixed(SimpleTokenType::KW_UNION))
	{
		if (!consume_current())
			return restore_and_fail(saved);
		parse_attribute_specifier_seq();
		if (fixed(SimpleTokenType::OP_COLON2) ||
			(identifier() && fixed(SimpleTokenType::OP_COLON2, 1)))
			parse_nested_name_specifier();
		if (fixed(SimpleTokenType::KW_TEMPLATE))
			consume_fixed(SimpleTokenType::KW_TEMPLATE);
		if (!parse_type_name())
		{
			restore(saved);
			return false;
		}
		return true;
	}
	if (!consume_fixed(SimpleTokenType::KW_ENUM))
	{
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::OP_COLON2) ||
		(identifier() && fixed(SimpleTokenType::OP_COLON2, 1)))
		parse_nested_name_specifier();
	if (!parse_enum_name())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_class_specifier()
{
	Mark saved = mark();
	if (!parse_class_head() || !consume_fixed(SimpleTokenType::OP_LBRACE) ||
		!begin_non_angle() || !parse_member_specification() ||
		!consume_fixed(SimpleTokenType::OP_RBRACE))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	return true;
}

bool PA6Parser::parse_class_head()
{
	Mark saved = mark();
	if (!(consume_fixed(SimpleTokenType::KW_CLASS) ||
		  consume_fixed(SimpleTokenType::KW_STRUCT) ||
		  consume_fixed(SimpleTokenType::KW_UNION)))
	{
		restore(saved);
		return false;
	}
	parse_attribute_specifier_seq();
	if (!fixed(SimpleTokenType::OP_LBRACE) && !fixed(SimpleTokenType::OP_COLON))
	{
		if (!parse_class_head_name())
		{
			restore(saved);
			return false;
		}
		if (kind(PA6TokenKind::ST_FINAL) && !consume_kind(PA6TokenKind::ST_FINAL))
		{
			restore(saved);
			return false;
		}
	}
	if (fixed(SimpleTokenType::OP_COLON) && !parse_base_clause())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_class_head_name()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_COLON2) ||
		(identifier() && fixed(SimpleTokenType::OP_COLON2, 1)))
		parse_nested_name_specifier();
	if (!parse_class_name())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_enum_specifier()
{
	Mark saved = mark();
	if (!parse_enum_head() || !consume_fixed(SimpleTokenType::OP_LBRACE) ||
		!begin_non_angle())
	{
		restore(saved);
		return false;
	}
	if (!fixed(SimpleTokenType::OP_RBRACE) && !parse_enumerator_list())
	{
		restore(saved);
		return false;
	}
	if (!consume_fixed(SimpleTokenType::OP_RBRACE))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	return true;
}

bool PA6Parser::parse_enum_head()
{
	Mark saved = mark();
	if (!parse_enum_key())
		return restore_and_fail(saved);
	parse_attribute_specifier_seq();
	if (fixed(SimpleTokenType::OP_COLON2) ||
		(identifier() && fixed(SimpleTokenType::OP_COLON2, 1)))
	{
		if (!parse_nested_name_specifier() || !consume_identifier())
		{
			restore(saved);
			return false;
		}
	}
	else if (identifier())
		consume_identifier();
	if (fixed(SimpleTokenType::OP_COLON) && !parse_enum_base())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_enum_key()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_ENUM))
		return restore_and_fail(saved);
	if (fixed(SimpleTokenType::KW_CLASS) || fixed(SimpleTokenType::KW_STRUCT))
		if (!consume_current())
			return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_enum_base()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_COLON) ||
		!parse_type_specifier_seq())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_enumerator_list()
{
	Mark saved = mark();
	if (!parse_enumerator_definition())
		return restore_and_fail(saved);
	while (consume_fixed(SimpleTokenType::OP_COMMA))
	{
		if (fixed(SimpleTokenType::OP_RBRACE))
			return true;
		if (!parse_enumerator_definition())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_enumerator_definition()
{
	Mark saved = mark();
	if (!consume_identifier())
		return restore_and_fail(saved);
	if (consume_fixed(SimpleTokenType::OP_ASS))
	{
		if (!parse_constant_expression())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_opaque_enum_declaration()
{
	Mark saved = mark();
	if (!parse_enum_key())
	{
		restore(saved);
		return false;
	}
	parse_attribute_specifier_seq();
	if (!consume_identifier() || (fixed(SimpleTokenType::OP_COLON) &&
		!parse_enum_base()) || !consume_fixed(SimpleTokenType::OP_SEMICOLON))
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_namespace_body()
{
	Mark saved = mark();
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (eof() || !tick() || !parse_declaration())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_qualified_namespace_specifier()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_COLON2) ||
		(identifier() && fixed(SimpleTokenType::OP_COLON2, 1)))
		parse_nested_name_specifier();
	if (!parse_namespace_name())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_storage_class_specifier()
{
	if (fixed(SimpleTokenType::KW_REGISTER) || fixed(SimpleTokenType::KW_STATIC) ||
		fixed(SimpleTokenType::KW_THREAD_LOCAL) ||
		fixed(SimpleTokenType::KW_EXTERN) || fixed(SimpleTokenType::KW_MUTABLE))
	{
		return consume_current();
	}
	return false;
}

bool PA6Parser::parse_function_specifier()
{
	if (fixed(SimpleTokenType::KW_INLINE) || fixed(SimpleTokenType::KW_VIRTUAL) ||
		fixed(SimpleTokenType::KW_EXPLICIT))
	{
		return consume_current();
	}
	return false;
}

bool PA6Parser::parse_cv_qualifier()
{
	return consume_fixed(SimpleTokenType::KW_CONST) ||
		consume_fixed(SimpleTokenType::KW_VOLATILE);
}

bool PA6Parser::parse_type_name()
{
	Mark saved = mark();
	if (category(PA6_NAME_TEMPLATE) && fixed(SimpleTokenType::OP_LT, 1))
	{
		if (parse_simple_template_id())
			return true;
		restore(saved);
		return false;
	}
	if (parse_class_name() || parse_enum_name() || parse_typedef_name())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_class_name()
{
	Mark saved = mark();
	if (category(PA6_NAME_TEMPLATE) && fixed(SimpleTokenType::OP_LT, 1))
	{
		if (parse_simple_template_id())
			return true;
		restore(saved);
		return false;
	}
	if (category(PA6_NAME_CLASS) && consume_identifier())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_enum_name()
{
	return category(PA6_NAME_ENUM) && consume_identifier();
}

bool PA6Parser::parse_namespace_name()
{
	return category(PA6_NAME_NAMESPACE) && consume_identifier();
}

bool PA6Parser::parse_template_name()
{
	return category(PA6_NAME_TEMPLATE) && consume_identifier();
}

bool PA6Parser::parse_typedef_name()
{
	return category(PA6_NAME_TYPEDEF) && consume_identifier();
}

bool PA6Parser::parse_simple_template_id()
{
	Mark saved = mark();
	if (!parse_template_name() || !consume_fixed(SimpleTokenType::OP_LT) ||
		!begin_angle())
	{
		restore(saved);
		return false;
	}
	if (!close_angle())
	{
		if (!parse_template_argument_list() || !close_angle())
		{
			restore(saved);
			return false;
		}
	}
	return true;
}

bool PA6Parser::parse_template_id()
{
	Mark saved = mark();
	if (parse_simple_template_id())
		return true;
	restore(saved);
	if (parse_operator_function_id() || parse_literal_operator_id())
	{
		if (!parse_operator_template_suffix())
		{
			restore(saved);
			return false;
		}
		return true;
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_template_argument_list()
{
	Mark saved = mark();
	if (!parse_template_argument_dots())
		return restore_and_fail(saved);
	while (fixed(SimpleTokenType::OP_COMMA))
	{
		if (!consume_fixed(SimpleTokenType::OP_COMMA) ||
			!parse_template_argument_dots())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_template_argument_dots()
{
	if (!parse_template_argument())
		return false;
	if (fixed(SimpleTokenType::OP_DOTS))
		consume_fixed(SimpleTokenType::OP_DOTS);
	return true;
}

bool PA6Parser::parse_template_argument()
{
	Mark saved = mark();
	if (parse_constant_expression() &&
		(fixed(SimpleTokenType::OP_COMMA) ||
		 fixed(SimpleTokenType::OP_DOTS) || kind(PA6TokenKind::ST_RSHIFT_1) ||
		 kind(PA6TokenKind::ST_RSHIFT_2) || fixed(SimpleTokenType::OP_GT)))
		return true;
	restore(saved);
	if (parse_type_id() &&
		(fixed(SimpleTokenType::OP_COMMA) || fixed(SimpleTokenType::OP_DOTS) ||
		 kind(PA6TokenKind::ST_RSHIFT_1) || kind(PA6TokenKind::ST_RSHIFT_2) ||
		 fixed(SimpleTokenType::OP_GT)))
		return true;
	restore(saved);
	if (parse_id_expression())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_typename_specifier()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_TYPENAME) ||
		!parse_nested_name_specifier())
	{
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::KW_TEMPLATE))
	{
		if (!consume_fixed(SimpleTokenType::KW_TEMPLATE) ||
			!parse_simple_template_id())
		{
			restore(saved);
			return false;
		}
	}
	else if (!consume_identifier())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_decltype_specifier()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_DECLTYPE) ||
		!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle() ||
		!parse_expression() || !consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	return true;
}

bool PA6Parser::parse_nested_name_specifier()
{
	Mark saved = mark();
	if (consume_fixed(SimpleTokenType::OP_COLON2))
	{
		while (parse_nested_name_suffix()) {}
		return true;
	}
	restore(saved);
	if (!(parse_type_name() || parse_namespace_name() ||
		parse_decltype_specifier()) ||
		!consume_fixed(SimpleTokenType::OP_COLON2))
	{
		restore(saved);
		return false;
	}
	while (parse_nested_name_suffix()) {}
	return true;
}

bool PA6Parser::parse_nested_name_suffix()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::KW_TEMPLATE))
	{
		consume_fixed(SimpleTokenType::KW_TEMPLATE);
		if (!parse_simple_template_id() ||
			!consume_fixed(SimpleTokenType::OP_COLON2))
		{
			restore(saved);
			return false;
		}
		return true;
	}
	if (identifier() && fixed(SimpleTokenType::OP_COLON2, 1))
	{
		consume_identifier();
		consume_fixed(SimpleTokenType::OP_COLON2);
		return true;
	}
	if (category(PA6_NAME_TEMPLATE) && fixed(SimpleTokenType::OP_LT, 1))
	{
		if (parse_simple_template_id() &&
			consume_fixed(SimpleTokenType::OP_COLON2))
			return true;
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_qualified_id()
{
	Mark saved = mark();
	if (!parse_nested_name_specifier())
	{
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::KW_TEMPLATE) &&
		!consume_fixed(SimpleTokenType::KW_TEMPLATE))
	{
		restore(saved);
		return false;
	}
	if (!parse_unqualified_id())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_unqualified_id()
{
	Mark saved = mark();
	if (identifier())
	{
		if (category(PA6_NAME_TEMPLATE) && fixed(SimpleTokenType::OP_LT, 1))
		{
			if (parse_simple_template_id())
				return true;
			restore(saved);
			return false;
		}
		return consume_identifier();
	}
	if (fixed(SimpleTokenType::OP_COMPL))
	{
		consume_fixed(SimpleTokenType::OP_COMPL);
		if (parse_class_name() || parse_decltype_specifier())
			return true;
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::KW_OPERATOR))
	{
		if (parse_operator_function_id())
		{
			if (fixed(SimpleTokenType::OP_LT))
			{
				if (!parse_operator_template_suffix())
				{
					restore(saved);
					return false;
				}
			}
			return true;
		}
		restore(saved);
		if (parse_literal_operator_id())
		{
			if (fixed(SimpleTokenType::OP_LT) &&
				!parse_operator_template_suffix())
			{
				restore(saved);
				return false;
			}
			return true;
		}
		restore(saved);
		if (parse_conversion_function_id())
			return true;
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_id_expression()
{
	Mark saved = mark();
	if (parse_qualified_id() || parse_unqualified_id())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_operator_function_id()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_OPERATOR))
		return false;
	if (fixed(SimpleTokenType::KW_NEW) || fixed(SimpleTokenType::KW_DELETE))
	{
		if (!consume_current())
			return restore_and_fail(saved);
		if (fixed(SimpleTokenType::OP_LSQUARE))
		{
			if (!consume_fixed(SimpleTokenType::OP_LSQUARE) ||
				!consume_fixed(SimpleTokenType::OP_RSQUARE))
			{
				restore(saved);
				return false;
			}
		}
		return true;
	}
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		if (consume_fixed(SimpleTokenType::OP_LPAREN) &&
			consume_fixed(SimpleTokenType::OP_RPAREN))
			return true;
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::OP_LSQUARE))
	{
		if (consume_fixed(SimpleTokenType::OP_LSQUARE) &&
			consume_fixed(SimpleTokenType::OP_RSQUARE))
			return true;
		restore(saved);
		return false;
	}
	if (kind(PA6TokenKind::ST_RSHIFT_1) &&
		kind(PA6TokenKind::ST_RSHIFT_2, 1))
	{
		if (consume_kind(PA6TokenKind::ST_RSHIFT_1) &&
			consume_kind(PA6TokenKind::ST_RSHIFT_2))
			return true;
		return restore_and_fail(saved);
	}
	const SimpleTokenType operators[] =
	{
		SimpleTokenType::OP_PLUS, SimpleTokenType::OP_MINUS,
		SimpleTokenType::OP_STAR, SimpleTokenType::OP_DIV,
		SimpleTokenType::OP_MOD, SimpleTokenType::OP_XOR,
		SimpleTokenType::OP_AMP, SimpleTokenType::OP_BOR,
		SimpleTokenType::OP_COMPL, SimpleTokenType::OP_LNOT,
		SimpleTokenType::OP_ASS, SimpleTokenType::OP_LT,
		SimpleTokenType::OP_GT, SimpleTokenType::OP_PLUSASS,
		SimpleTokenType::OP_MINUSASS, SimpleTokenType::OP_STARASS,
		SimpleTokenType::OP_DIVASS, SimpleTokenType::OP_MODASS,
		SimpleTokenType::OP_XORASS, SimpleTokenType::OP_BANDASS,
		SimpleTokenType::OP_BORASS, SimpleTokenType::OP_RSHIFTASS,
		SimpleTokenType::OP_LSHIFTASS, SimpleTokenType::OP_EQ,
		SimpleTokenType::OP_NE, SimpleTokenType::OP_LE,
		SimpleTokenType::OP_GE, SimpleTokenType::OP_LAND,
		SimpleTokenType::OP_LOR, SimpleTokenType::OP_INC,
		SimpleTokenType::OP_DEC, SimpleTokenType::OP_COMMA,
		SimpleTokenType::OP_ARROWSTAR, SimpleTokenType::OP_ARROW,
		SimpleTokenType::OP_LSHIFT
	};
	for (std::size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); ++i)
	{
		if (fixed(operators[i]))
			return consume_current();
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_literal_operator_id()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_OPERATOR) ||
		!consume_kind(PA6TokenKind::ST_EMPTYSTR) || !consume_identifier())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_conversion_function_id()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_OPERATOR) ||
		!parse_type_specifier_seq())
		return restore_and_fail(saved);
	while (parse_ptr_operator()) {}
	return true;
}

bool PA6Parser::parse_pseudo_destructor_name()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_COMPL))
	{
		consume_fixed(SimpleTokenType::OP_COMPL);
		if (parse_type_name() || parse_decltype_specifier())
			return true;
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::OP_COLON2) ||
		(identifier() && fixed(SimpleTokenType::OP_COLON2, 1)))
	{
		if (parse_nested_name_specifier() &&
			consume_fixed(SimpleTokenType::OP_COMPL) && parse_type_name())
			return true;
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_declarator(bool* has_function)
{
	if (has_function != NULL)
		*has_function = false;
	Mark saved = mark();
	bool local_function = false;
	if (parse_ptr_declarator(&local_function) ||
		(parse_noptr_declarator(&local_function) &&
		 (parse_trailing_return_type() || true)))
	{
		if (has_function != NULL)
			*has_function = local_function;
		return true;
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_ptr_declarator(bool* has_function)
{
	Mark saved = mark();
	bool saw_ptr = false;
	while (parse_ptr_operator())
		saw_ptr = true;
	if (!saw_ptr)
	{
		restore(saved);
		return false;
	}
	bool nested_function = false;
	if (!parse_noptr_declarator(&nested_function))
	{
		restore(saved);
		return false;
	}
	if (has_function != NULL)
		*has_function = nested_function;
	return true;
}

bool PA6Parser::parse_noptr_declarator(bool* has_function)
{
	Mark saved = mark();
	bool local_function = false;
	if (!parse_noptr_declarator_root(&local_function))
	{
		restore(saved);
		return false;
	}
	while (parse_noptr_declarator_suffix(&local_function)) {}
	if (has_function != NULL)
		*has_function = local_function;
	return true;
}

bool PA6Parser::parse_noptr_declarator_root(bool* has_function)
{
	if (parse_declarator_id())
	{
		parse_attribute_specifier_seq();
		if (has_function != NULL)
			*has_function = false;
		return true;
	}
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle())
	{
		restore(saved);
		return false;
	}
	bool nested_function = false;
	if (!parse_ptr_declarator(&nested_function) ||
		!consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	parse_attribute_specifier_seq();
	if (has_function != NULL)
		*has_function = nested_function;
	return true;
}

bool PA6Parser::parse_noptr_declarator_suffix(bool* has_function)
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		if (!parse_parameters_and_qualifiers())
			return restore_and_fail(saved);
		if (has_function != NULL)
			*has_function = true;
		return true;
	}
	if (!consume_fixed(SimpleTokenType::OP_LSQUARE))
		return false;
	if (!begin_non_angle())
	{
		restore(saved);
		return false;
	}
	if (!fixed(SimpleTokenType::OP_RSQUARE) && !parse_constant_expression())
	{
		restore(saved);
		return false;
	}
	if (!consume_fixed(SimpleTokenType::OP_RSQUARE) ||
		!parse_attribute_specifier_seq())
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	return true;
}

bool PA6Parser::parse_declarator_id()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_DOTS))
		if (!consume_fixed(SimpleTokenType::OP_DOTS))
			return restore_and_fail(saved);
	if (!parse_id_expression())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_ptr_operator()
{
	Mark saved = mark();
	if (consume_fixed(SimpleTokenType::OP_STAR))
	{
		parse_attribute_specifier_seq();
		while (parse_cv_qualifier()) {}
		return true;
	}
	if (consume_fixed(SimpleTokenType::OP_AMP) ||
		consume_fixed(SimpleTokenType::OP_LAND))
	{
		parse_attribute_specifier_seq();
		return true;
	}
	if (fixed(SimpleTokenType::OP_COLON2) ||
		(identifier() && fixed(SimpleTokenType::OP_COLON2, 1)))
	{
		if (parse_nested_name_specifier() && consume_fixed(SimpleTokenType::OP_STAR))
		{
			parse_attribute_specifier_seq();
			while (parse_cv_qualifier()) {}
			return true;
		}
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_parameters_and_qualifiers()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle() ||
		!parse_parameter_declaration_clause() ||
		!consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	while (parse_cv_qualifier()) {}
	if (fixed(SimpleTokenType::OP_AMP) || fixed(SimpleTokenType::OP_LAND))
		if (!consume_current())
			return restore_and_fail(saved);
	parse_exception_specification();
	parse_attribute_specifier_seq();
	return true;
}

bool PA6Parser::parse_parameter_declaration_clause()
{
	if (fixed(SimpleTokenType::OP_RPAREN))
		return true;
	if (fixed(SimpleTokenType::OP_DOTS))
		return consume_fixed(SimpleTokenType::OP_DOTS);
	if (!parse_parameter_declaration_list())
		return false;
	if (fixed(SimpleTokenType::OP_DOTS))
		consume_fixed(SimpleTokenType::OP_DOTS);
	return true;
}

bool PA6Parser::parse_parameter_declaration_list()
{
	Mark saved = mark();
	if (!parse_parameter_declaration())
		return restore_and_fail(saved);
	while (fixed(SimpleTokenType::OP_COMMA))
	{
		consume_fixed(SimpleTokenType::OP_COMMA);
		if (fixed(SimpleTokenType::OP_DOTS))
			return consume_fixed(SimpleTokenType::OP_DOTS) ? true :
				restore_and_fail(saved);
		if (!parse_parameter_declaration())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_parameter_declaration()
{
	Mark saved = mark();
	if (!parse_attribute_specifier_seq() || !parse_decl_specifier_seq())
	{
		restore(saved);
		return false;
	}
	Mark declarator = mark();
	if (!parse_declarator())
	{
		restore(declarator);
		parse_abstract_declarator();
	}
	if (fixed(SimpleTokenType::OP_ASS))
	{
		consume_fixed(SimpleTokenType::OP_ASS);
		if (!parse_initializer_clause())
		{
			restore(saved);
			return false;
		}
	}
	return true;
}

bool PA6Parser::parse_abstract_declarator()
{
	Mark saved = mark();
	if (parse_abstract_pack_declarator() || parse_ptr_abstract_declarator())
		return true;
	restore(saved);
	if (parse_noptr_abstract_declarator())
	{
		parse_trailing_return_type();
		return true;
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_ptr_abstract_declarator()
{
	Mark saved = mark();
	bool saw = false;
	while (parse_ptr_operator())
		saw = true;
	if (!saw)
	{
		restore(saved);
		return false;
	}
	parse_noptr_abstract_declarator();
	return true;
}

bool PA6Parser::parse_noptr_abstract_declarator()
{
	Mark saved = mark();
	bool saw = false;
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		if (!begin_non_angle() || !parse_abstract_declarator() ||
			!consume_fixed(SimpleTokenType::OP_RPAREN))
		{
			restore(saved);
			return false;
		}
		end_non_angle();
		saw = true;
	}
	while (fixed(SimpleTokenType::OP_LPAREN) || fixed(SimpleTokenType::OP_LSQUARE))
	{
		if (fixed(SimpleTokenType::OP_LPAREN))
		{
			if (!parse_parameters_and_qualifiers())
				return restore_and_fail(saved);
		}
		else
		{
			consume_fixed(SimpleTokenType::OP_LSQUARE);
			if (!begin_non_angle())
				return restore_and_fail(saved);
			if (!fixed(SimpleTokenType::OP_RSQUARE) &&
				!parse_constant_expression())
				return restore_and_fail(saved);
			if (!consume_fixed(SimpleTokenType::OP_RSQUARE))
				return restore_and_fail(saved);
			end_non_angle();
			parse_attribute_specifier_seq();
		}
		saw = true;
	}
	if (!saw)
		restore(saved);
	return saw;
}

bool PA6Parser::parse_abstract_pack_declarator()
{
	Mark saved = mark();
	while (parse_ptr_operator()) {}
	if (!consume_fixed(SimpleTokenType::OP_DOTS))
	{
		restore(saved);
		return false;
	}
	while (parse_noptr_declarator_suffix()) {}
	return true;
}

bool PA6Parser::parse_trailing_return_type()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_ARROW) ||
		!parse_type_specifier_seq())
	{
		restore(saved);
		return false;
	}
	parse_abstract_declarator();
	return true;
}

bool PA6Parser::parse_initializer()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_ASS))
	{
		consume_fixed(SimpleTokenType::OP_ASS);
		if (!parse_initializer_clause())
			return restore_and_fail(saved);
		return true;
	}
	if (fixed(SimpleTokenType::OP_LBRACE))
		return parse_braced_init_list();
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		Mark saved = mark();
		consume_fixed(SimpleTokenType::OP_LPAREN);
		if (!begin_non_angle() || !parse_expression_list() ||
			!consume_fixed(SimpleTokenType::OP_RPAREN))
			return restore_and_fail(saved);
		end_non_angle();
		return true;
	}
	return false;
}

bool PA6Parser::parse_brace_or_equal_initializer()
{
	return parse_initializer();
}

bool PA6Parser::parse_initializer_clause()
{
	return parse_assignment_expression() || parse_braced_init_list();
}

bool PA6Parser::parse_initializer_list()
{
	Mark saved = mark();
	if (!parse_initializer_clause_dots())
		return restore_and_fail(saved);
	while (consume_fixed(SimpleTokenType::OP_COMMA))
	{
		if (!parse_initializer_clause_dots())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_initializer_clause_dots()
{
	if (!parse_initializer_clause())
		return false;
	if (fixed(SimpleTokenType::OP_DOTS))
		consume_fixed(SimpleTokenType::OP_DOTS);
	return true;
}

bool PA6Parser::parse_expression_list()
{
	return parse_initializer_list();
}

bool PA6Parser::parse_braced_init_list()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LBRACE) || !begin_non_angle())
	{
		restore(saved);
		return false;
	}
	if (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (!parse_initializer_list())
		{
			restore(saved);
			return false;
		}
		if (fixed(SimpleTokenType::OP_COMMA))
			consume_fixed(SimpleTokenType::OP_COMMA);
	}
	if (!consume_fixed(SimpleTokenType::OP_RBRACE))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	return true;
}

bool PA6Parser::parse_statement()
{
	Mark saved = mark();
	if (parse_labeled_statement())
		return true;
	restore(saved);
	if (parse_compound_statement() || parse_selection_statement() ||
		parse_iteration_statement() || parse_jump_statement() ||
		parse_try_block())
		return true;
	restore(saved);
	if (parse_block_declaration())
		return true;
	restore(saved);
	if (parse_expression_statement())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_labeled_statement()
{
	Mark saved = mark();
	parse_attribute_specifier_seq();
	if (identifier() && fixed(SimpleTokenType::OP_COLON, 1))
	{
		consume_identifier();
		consume_fixed(SimpleTokenType::OP_COLON);
		if (parse_statement())
			return true;
	}
	restore(saved);
	parse_attribute_specifier_seq();
	if (consume_fixed(SimpleTokenType::KW_CASE))
	{
		if (parse_constant_expression() &&
			consume_fixed(SimpleTokenType::OP_COLON) && parse_statement())
			return true;
	}
	restore(saved);
	parse_attribute_specifier_seq();
	if (consume_fixed(SimpleTokenType::KW_DEFAULT) &&
		consume_fixed(SimpleTokenType::OP_COLON) && parse_statement())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_expression_statement()
{
	Mark saved = mark();
	if (!fixed(SimpleTokenType::OP_SEMICOLON) && !parse_expression())
	{
		restore(saved);
		return false;
	}
	if (!consume_fixed(SimpleTokenType::OP_SEMICOLON))
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_compound_statement()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LBRACE) || !begin_non_angle())
	{
		restore(saved);
		return false;
	}
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (eof() || !tick() || !parse_statement())
		{
			restore(saved);
			return false;
		}
	}
	if (!consume_fixed(SimpleTokenType::OP_RBRACE))
		return restore_and_fail(saved);
	end_non_angle();
	return true;
}

bool PA6Parser::parse_selection_statement()
{
	Mark saved = mark();
	if (consume_fixed(SimpleTokenType::KW_IF))
	{
		if (consume_fixed(SimpleTokenType::OP_LPAREN) && begin_non_angle() &&
			parse_condition() && consume_fixed(SimpleTokenType::OP_RPAREN))
		{
			end_non_angle();
			if (parse_statement())
			{
				if (consume_fixed(SimpleTokenType::KW_ELSE) &&
					!parse_statement())
				{
					restore(saved);
					return false;
				}
				return true;
			}
		}
	}
	restore(saved);
	if (consume_fixed(SimpleTokenType::KW_SWITCH) &&
		consume_fixed(SimpleTokenType::OP_LPAREN) && begin_non_angle() &&
		parse_condition() && consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		end_non_angle();
		if (parse_statement())
			return true;
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_condition_declaration()
{
	Mark saved = mark();
	parse_attribute_specifier_seq();
	if (!parse_decl_specifier_seq() || !parse_declarator())
	{
		restore(saved);
		return false;
	}
	if (consume_fixed(SimpleTokenType::OP_ASS))
	{
		if (!parse_initializer_clause())
		{
			restore(saved);
			return false;
		}
		return true;
	}
	if (fixed(SimpleTokenType::OP_LBRACE) && parse_braced_init_list())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_condition()
{
	Mark saved = mark();
	if (parse_condition_declaration() || parse_expression())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_iteration_statement()
{
	Mark saved = mark();
	if (consume_fixed(SimpleTokenType::KW_WHILE) &&
		consume_fixed(SimpleTokenType::OP_LPAREN) && begin_non_angle() &&
		parse_condition() && consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		end_non_angle();
		if (parse_statement())
			return true;
	}
	restore(saved);
	if (consume_fixed(SimpleTokenType::KW_DO) && parse_statement() &&
		consume_fixed(SimpleTokenType::KW_WHILE) &&
		consume_fixed(SimpleTokenType::OP_LPAREN) && begin_non_angle() &&
		parse_expression() && consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		end_non_angle();
		if (consume_fixed(SimpleTokenType::OP_SEMICOLON))
			return true;
	}
	restore(saved);
	if (!consume_fixed(SimpleTokenType::KW_FOR) ||
		!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle())
	{
		restore(saved);
		return false;
	}
	Mark range = mark();
	if (parse_for_range_declaration() &&
		consume_fixed(SimpleTokenType::OP_COLON) &&
		parse_for_range_initializer() && consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		end_non_angle();
		if (parse_statement())
			return true;
		return restore_and_fail(saved);
	}
	restore(range);
	if (!parse_for_init_statement())
	{
		restore(saved);
		return false;
	}
	if (!fixed(SimpleTokenType::OP_SEMICOLON) && !parse_condition())
	{
		restore(saved);
		return false;
	}
	if (!consume_fixed(SimpleTokenType::OP_SEMICOLON))
	{
		restore(saved);
		return false;
	}
	if (!fixed(SimpleTokenType::OP_RPAREN) && !parse_expression())
	{
		restore(saved);
		return false;
	}
	if (!consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	if (!parse_statement())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_for_init_statement()
{
	Mark saved = mark();
	if (parse_simple_declaration() || parse_expression_statement())
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_for_range_declaration()
{
	Mark saved = mark();
	if (!parse_attribute_specifier_seq() || !parse_decl_specifier_seq() ||
		!parse_declarator())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_for_range_initializer()
{
	return parse_expression() || parse_braced_init_list();
}

bool PA6Parser::parse_jump_statement()
{
	Mark saved = mark();
	if (consume_fixed(SimpleTokenType::KW_BREAK) ||
		consume_fixed(SimpleTokenType::KW_CONTINUE))
	{
		if (consume_fixed(SimpleTokenType::OP_SEMICOLON))
			return true;
		return restore_and_fail(saved);
	}
	if (consume_fixed(SimpleTokenType::KW_GOTO))
	{
		if (consume_identifier() && consume_fixed(SimpleTokenType::OP_SEMICOLON))
			return true;
		return restore_and_fail(saved);
	}
	if (consume_fixed(SimpleTokenType::KW_RETURN))
	{
		if (fixed(SimpleTokenType::OP_LBRACE))
		{
			if (parse_braced_init_list() &&
				consume_fixed(SimpleTokenType::OP_SEMICOLON))
				return true;
			return restore_and_fail(saved);
		}
		if (!fixed(SimpleTokenType::OP_SEMICOLON) && !parse_expression())
			return restore_and_fail(saved);
		if (!consume_fixed(SimpleTokenType::OP_SEMICOLON))
			return restore_and_fail(saved);
		return true;
	}
	return restore_and_fail(saved);
}

bool PA6Parser::parse_try_block()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_TRY) || !parse_compound_statement() ||
		!parse_handler())
	{
		restore(saved);
		return false;
	}
	while (parse_handler()) {}
	return true;
}

bool PA6Parser::parse_function_try_block()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_TRY))
		return false;
	parse_ctor_initializer();
	if (!parse_compound_statement() || !parse_handler())
	{
		restore(saved);
		return false;
	}
	while (parse_handler()) {}
	return true;
}

bool PA6Parser::parse_handler()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_CATCH) ||
		!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle() ||
		!parse_exception_declaration() ||
		!consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	if (!parse_compound_statement())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_exception_declaration()
{
	if (consume_fixed(SimpleTokenType::OP_DOTS))
		return true;
	Mark saved = mark();
	if (!parse_attribute_specifier_seq() || !parse_type_specifier_seq())
	{
		restore(saved);
		return false;
	}
	parse_attribute_specifier_seq();
	Mark declarator = mark();
	if (!parse_declarator())
	{
		restore(declarator);
		parse_abstract_declarator();
	}
	return true;
}

} // namespace pa6_internal
