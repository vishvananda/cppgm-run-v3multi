#include "pa6_parser.h"

namespace pa6_internal
{

bool PA6Parser::parse_expression()
{
	Mark saved = mark();
	if (!parse_assignment_expression())
		return restore_and_fail(saved);
	while (fixed(SimpleTokenType::OP_COMMA))
	{
		if (!consume_fixed(SimpleTokenType::OP_COMMA) ||
			!parse_assignment_expression())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_assignment_expression()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::KW_THROW))
		return parse_throw_expression();
	if (!parse_conditional_expression())
		return restore_and_fail(saved);
	const SimpleTokenType operators[] =
	{
		SimpleTokenType::OP_ASS, SimpleTokenType::OP_STARASS,
		SimpleTokenType::OP_DIVASS, SimpleTokenType::OP_MODASS,
		SimpleTokenType::OP_PLUSASS, SimpleTokenType::OP_MINUSASS,
		SimpleTokenType::OP_RSHIFTASS, SimpleTokenType::OP_LSHIFTASS,
		SimpleTokenType::OP_BANDASS, SimpleTokenType::OP_XORASS,
		SimpleTokenType::OP_BORASS
	};
	for (std::size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); ++i)
	{
		if (fixed(operators[i]))
		{
			if (!consume_fixed(operators[i]) || !parse_initializer_clause())
				return restore_and_fail(saved);
			return true;
		}
	}
	return true;
}

bool PA6Parser::parse_conditional_expression()
{
	Mark saved = mark();
	if (!parse_binary_expression(10))
		return restore_and_fail(saved);
	if (!consume_fixed(SimpleTokenType::OP_QMARK))
		return true;
	if (!parse_expression() || !consume_fixed(SimpleTokenType::OP_COLON))
		return restore_and_fail(saved);
	if (!parse_assignment_expression())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_binary_expression(int level)
{
	Mark saved = mark();
	if (level == 0)
		return parse_pm_expression();
	if (!parse_binary_expression(level - 1))
		return restore_and_fail(saved);
	while (tick())
	{
		bool is_operator = false;
		if (level == 1)
			is_operator = fixed(SimpleTokenType::OP_STAR) ||
				fixed(SimpleTokenType::OP_DIV) || fixed(SimpleTokenType::OP_MOD);
		else if (level == 2)
			is_operator = fixed(SimpleTokenType::OP_PLUS) ||
				fixed(SimpleTokenType::OP_MINUS);
		else if (level == 3)
			is_operator = (fixed(SimpleTokenType::OP_LSHIFT) ||
				(kind(PA6TokenKind::ST_RSHIFT_1) &&
				 kind(PA6TokenKind::ST_RSHIFT_2, 1))) && can_use_angle_operator();
		else if (level == 4)
			is_operator = fixed(SimpleTokenType::OP_LT) ||
				(fixed(SimpleTokenType::OP_GT) && can_use_angle_operator()) ||
				fixed(SimpleTokenType::OP_LE) ||
				fixed(SimpleTokenType::OP_GE);
		else if (level == 5)
			is_operator = fixed(SimpleTokenType::OP_EQ) ||
				fixed(SimpleTokenType::OP_NE);
		else if (level == 6)
			is_operator = fixed(SimpleTokenType::OP_AMP);
		else if (level == 7)
			is_operator = fixed(SimpleTokenType::OP_XOR);
		else if (level == 8)
			is_operator = fixed(SimpleTokenType::OP_BOR);
		else if (level == 9)
			is_operator = fixed(SimpleTokenType::OP_LAND);
		else
			is_operator = fixed(SimpleTokenType::OP_LOR);
		if (!is_operator)
			break;
		if (level == 3)
		{
			if (!shift_operator())
				return restore_and_fail(saved);
		}
		else
		{
			if (!consume_current())
				return restore_and_fail(saved);
		}
		if (!parse_binary_expression(level - 1))
			return restore_and_fail(saved);
	}
	if (exhausted_)
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_pm_expression()
{
	Mark saved = mark();
	if (!parse_cast_expression())
		return restore_and_fail(saved);
	while (fixed(SimpleTokenType::OP_DOTSTAR) ||
		fixed(SimpleTokenType::OP_ARROWSTAR))
	{
		if (!consume_current())
			return restore_and_fail(saved);
		if (!parse_cast_expression())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_cast_expression()
{
	Mark saved = mark();
	const SimpleTokenType operators[] =
	{
		SimpleTokenType::OP_INC, SimpleTokenType::OP_DEC,
		SimpleTokenType::OP_STAR, SimpleTokenType::OP_AMP,
		SimpleTokenType::OP_PLUS, SimpleTokenType::OP_MINUS,
		SimpleTokenType::OP_LNOT, SimpleTokenType::OP_COMPL
	};
	while (true)
	{
		if (fixed(SimpleTokenType::OP_LPAREN))
		{
			Mark cast = mark();
			if (parse_cast_operator())
				continue;
			restore(cast);
		}
		bool consumed_operator = false;
		for (std::size_t i = 0;
			i < sizeof(operators) / sizeof(operators[0]); ++i)
		{
			if (fixed(operators[i]))
			{
				if (!consume_current())
					return restore_and_fail(saved);
				consumed_operator = true;
				break;
			}
		}
		if (!consumed_operator)
			break;
	}
	if (!parse_unary_expression())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_unary_expression()
{
	Mark saved = mark();
	const SimpleTokenType operators[] =
	{
		SimpleTokenType::OP_INC, SimpleTokenType::OP_DEC,
		SimpleTokenType::OP_STAR, SimpleTokenType::OP_AMP,
		SimpleTokenType::OP_PLUS, SimpleTokenType::OP_MINUS,
		SimpleTokenType::OP_LNOT, SimpleTokenType::OP_COMPL
	};
	for (std::size_t i = 0; i < sizeof(operators) / sizeof(operators[0]); ++i)
	{
		if (fixed(operators[i]))
		{
			if (!consume_current() || !parse_cast_expression())
				return restore_and_fail(saved);
			return true;
		}
	}
	if (consume_fixed(SimpleTokenType::KW_SIZEOF))
	{
		if (consume_fixed(SimpleTokenType::OP_DOTS))
		{
			if (!consume_fixed(SimpleTokenType::OP_LPAREN) ||
				!begin_non_angle() || !consume_identifier() ||
				!consume_fixed(SimpleTokenType::OP_RPAREN))
				return restore_and_fail(saved);
			end_non_angle();
			return true;
		}
		if (consume_fixed(SimpleTokenType::OP_LPAREN))
		{
			Mark type = mark();
			if (begin_non_angle() && parse_type_id() &&
				consume_fixed(SimpleTokenType::OP_RPAREN))
			{
				end_non_angle();
				return true;
			}
			restore(type);
			// The opening parenthesis was a grouping expression.
			if (!begin_non_angle() || !parse_expression() ||
				!consume_fixed(SimpleTokenType::OP_RPAREN))
				return restore_and_fail(saved);
			end_non_angle();
			return true;
		}
		if (!parse_unary_expression())
			return restore_and_fail(saved);
		return true;
	}
	if (consume_fixed(SimpleTokenType::KW_ALIGNOF))
	{
		if (!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle() ||
			!parse_type_id() || !consume_fixed(SimpleTokenType::OP_RPAREN))
			return restore_and_fail(saved);
		end_non_angle();
		return true;
	}
	if (fixed(SimpleTokenType::KW_NOEXCEPT))
		return parse_noexcept_expression();
	if (fixed(SimpleTokenType::OP_COLON2) && fixed(SimpleTokenType::KW_NEW, 1))
		return parse_new_expression();
	if (fixed(SimpleTokenType::KW_NEW))
		return parse_new_expression();
	if (fixed(SimpleTokenType::OP_COLON2) && fixed(SimpleTokenType::KW_DELETE, 1))
		return parse_delete_expression();
	if (fixed(SimpleTokenType::KW_DELETE))
		return parse_delete_expression();
	if (!parse_postfix_expression())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_postfix_expression()
{
	Mark saved = mark();
	if (!parse_postfix_root())
		return restore_and_fail(saved);
	while (parse_postfix_suffix()) {}
	if (exhausted_)
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_postfix_root()
{
	Mark saved = mark();
	if (parse_primary_expression())
		return true;
	restore(saved);
	if (parse_simple_type_specifier())
	{
		if (consume_fixed(SimpleTokenType::OP_LPAREN))
		{
			if (!begin_non_angle() ||
				(!fixed(SimpleTokenType::OP_RPAREN) && !parse_expression_list()) ||
				!consume_fixed(SimpleTokenType::OP_RPAREN))
			{
				restore(saved);
				return false;
			}
			end_non_angle();
			return true;
		}
		if (fixed(SimpleTokenType::OP_LBRACE) && parse_braced_init_list())
			return true;
	}
	restore(saved);
	if (parse_typename_specifier())
	{
		if (consume_fixed(SimpleTokenType::OP_LPAREN))
		{
			if (!begin_non_angle() ||
				(!fixed(SimpleTokenType::OP_RPAREN) && !parse_expression_list()) ||
				!consume_fixed(SimpleTokenType::OP_RPAREN))
			{
				restore(saved);
				return false;
			}
			end_non_angle();
			return true;
		}
		if (fixed(SimpleTokenType::OP_LBRACE) && parse_braced_init_list())
			return true;
	}
	restore(saved);
	const SimpleTokenType casts[] =
	{
		SimpleTokenType::KW_DYNAMIC_CAST, SimpleTokenType::KW_STATIC_CAST,
		SimpleTokenType::KW_REINTERPET_CAST, SimpleTokenType::KW_CONST_CAST
	};
	for (std::size_t i = 0; i < sizeof(casts) / sizeof(casts[0]); ++i)
	{
		if (consume_fixed(casts[i]))
		{
			if (!consume_fixed(SimpleTokenType::OP_LT) || !begin_angle() ||
				!parse_type_id() || !close_angle() ||
				!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle() ||
				!parse_expression() || !consume_fixed(SimpleTokenType::OP_RPAREN))
			{
				restore(saved);
				return false;
			}
			end_non_angle();
			return true;
		}
		restore(saved);
	}
	if (consume_fixed(SimpleTokenType::KW_TYPEID))
	{
		if (!consume_fixed(SimpleTokenType::OP_LPAREN))
		{
			restore(saved);
			return false;
		}
		Mark content = mark();
		if (begin_non_angle() && parse_type_id() &&
			consume_fixed(SimpleTokenType::OP_RPAREN))
		{
			end_non_angle();
			return true;
		}
		restore(content);
		if (!begin_non_angle() || !parse_expression() ||
			!consume_fixed(SimpleTokenType::OP_RPAREN))
		{
			restore(saved);
			return false;
		}
		end_non_angle();
		return true;
	}
	restore(saved);
	return false;
}

bool PA6Parser::parse_postfix_suffix()
{
	Mark saved = mark();
	if (consume_fixed(SimpleTokenType::OP_LSQUARE))
	{
		if (!begin_non_angle())
			return restore_and_fail(saved);
		bool ok = fixed(SimpleTokenType::OP_LBRACE) ?
			parse_braced_init_list() : parse_expression();
		ok = ok && consume_fixed(SimpleTokenType::OP_RSQUARE);
		if (!ok)
		{
			restore(saved);
			return false;
		}
		end_non_angle();
		return true;
	}
	if (consume_fixed(SimpleTokenType::OP_LPAREN))
	{
		if (!begin_non_angle() ||
			(!fixed(SimpleTokenType::OP_RPAREN) && !parse_expression_list()) ||
			!consume_fixed(SimpleTokenType::OP_RPAREN))
		{
			restore(saved);
			return false;
		}
		end_non_angle();
		return true;
	}
	if (consume_fixed(SimpleTokenType::OP_DOT) ||
		consume_fixed(SimpleTokenType::OP_ARROW))
	{
		if (fixed(SimpleTokenType::OP_COMPL) && parse_pseudo_destructor_name())
			return true;
		if (consume_fixed(SimpleTokenType::KW_TEMPLATE))
		{
			if (!parse_id_expression())
			{
				restore(saved);
				return false;
			}
			return true;
		}
		if (!parse_id_expression())
		{
			restore(saved);
			return false;
		}
		return true;
	}
	if (consume_fixed(SimpleTokenType::OP_INC) ||
		consume_fixed(SimpleTokenType::OP_DEC))
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_primary_expression()
{
	if (consume_fixed(SimpleTokenType::KW_TRUE) ||
		consume_fixed(SimpleTokenType::KW_FALSE) ||
		consume_fixed(SimpleTokenType::KW_NULLPTR) || consume_literal() ||
		consume_fixed(SimpleTokenType::KW_THIS))
		return true;
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		Mark saved = mark();
		consume_fixed(SimpleTokenType::OP_LPAREN);
		if (!begin_non_angle() || !parse_expression() ||
			!consume_fixed(SimpleTokenType::OP_RPAREN))
		{
			restore(saved);
			return false;
		}
		end_non_angle();
		return true;
	}
	if (fixed(SimpleTokenType::OP_LSQUARE))
		return parse_lambda_expression();
	return parse_id_expression();
}

bool PA6Parser::parse_lambda_expression()
{
	Mark saved = mark();
	if (!parse_lambda_introducer())
	{
		restore(saved);
		return false;
	}
	parse_lambda_declarator();
	if (!parse_compound_statement())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_lambda_introducer()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LSQUARE) || !begin_non_angle())
		return restore_and_fail(saved);
	if (!fixed(SimpleTokenType::OP_RSQUARE) && !parse_lambda_capture())
		return restore_and_fail(saved);
	if (!consume_fixed(SimpleTokenType::OP_RSQUARE))
		return restore_and_fail(saved);
	end_non_angle();
	return true;
}

bool PA6Parser::parse_lambda_capture()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_AMP) || fixed(SimpleTokenType::OP_ASS))
	{
		if (!consume_current())
			return restore_and_fail(saved);
		if (fixed(SimpleTokenType::OP_RSQUARE))
			return true;
		if (!consume_fixed(SimpleTokenType::OP_COMMA) || !parse_capture_list())
		{
			restore(saved);
			return false;
		}
		return true;
	}
	if (!parse_capture_list())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_capture_list()
{
	Mark saved = mark();
	if (!parse_capture())
		return restore_and_fail(saved);
	if (fixed(SimpleTokenType::OP_DOTS))
		consume_fixed(SimpleTokenType::OP_DOTS);
	while (consume_fixed(SimpleTokenType::OP_COMMA))
	{
		if (!parse_capture())
			return restore_and_fail(saved);
		if (fixed(SimpleTokenType::OP_DOTS))
			consume_fixed(SimpleTokenType::OP_DOTS);
	}
	return true;
}

bool PA6Parser::parse_capture()
{
	Mark saved = mark();
	if (consume_fixed(SimpleTokenType::KW_THIS))
		return true;
	if (consume_fixed(SimpleTokenType::OP_AMP))
	{
		if (consume_identifier())
			return true;
		return restore_and_fail(saved);
	}
	if (!consume_identifier())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_lambda_declarator()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LPAREN))
		return restore_and_fail(saved);
	if (!begin_non_angle() || !parse_parameter_declaration_clause() ||
		!consume_fixed(SimpleTokenType::OP_RPAREN))
		return restore_and_fail(saved);
	end_non_angle();
	if (fixed(SimpleTokenType::KW_MUTABLE))
		consume_fixed(SimpleTokenType::KW_MUTABLE);
	parse_exception_specification();
	parse_attribute_specifier_seq();
	if (fixed(SimpleTokenType::OP_ARROW) && !parse_trailing_return_type())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_noexcept_expression()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_NOEXCEPT) ||
		!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle() ||
		!parse_expression() || !consume_fixed(SimpleTokenType::OP_RPAREN))
		return restore_and_fail(saved);
	end_non_angle();
	return true;
}

bool PA6Parser::parse_new_expression()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_COLON2))
		consume_fixed(SimpleTokenType::OP_COLON2);
	if (!consume_fixed(SimpleTokenType::KW_NEW))
	{
		restore(saved);
		return false;
	}
	Mark placement = mark();
	if (fixed(SimpleTokenType::OP_LPAREN) && parse_new_placement())
	{
		if (!parse_new_type_id() || !parse_new_initializer())
		{
			restore(saved);
			return false;
		}
		return true;
	}
	restore(placement);
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		if (!begin_non_angle() || !parse_type_id() ||
			!consume_fixed(SimpleTokenType::OP_RPAREN))
		{
			restore(saved);
			return false;
		}
		end_non_angle();
		if (!parse_new_initializer())
			return restore_and_fail(saved);
		return true;
	}
	if (!parse_new_type_id())
	{
		restore(saved);
		return false;
	}
	if (!parse_new_initializer())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_new_placement()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle() ||
		!parse_expression_list() || !consume_fixed(SimpleTokenType::OP_RPAREN))
		return restore_and_fail(saved);
	end_non_angle();
	return true;
}

bool PA6Parser::parse_new_type_id()
{
	Mark saved = mark();
	if (!parse_type_specifier_seq())
		return restore_and_fail(saved);
	if ((fixed(SimpleTokenType::OP_STAR) || fixed(SimpleTokenType::OP_AMP) ||
		 fixed(SimpleTokenType::OP_LAND) || fixed(SimpleTokenType::OP_LSQUARE)) &&
		!parse_new_declarator())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_new_declarator()
{
	Mark saved = mark();
	while (parse_ptr_operator()) {}
	if (parse_noptr_new_declarator())
		return true;
	restore(saved);
	bool saw_ptr = false;
	while (parse_ptr_operator())
		saw_ptr = true;
	return saw_ptr;
}

bool PA6Parser::parse_noptr_new_declarator()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LSQUARE) || !begin_non_angle() ||
		!parse_expression() || !consume_fixed(SimpleTokenType::OP_RSQUARE))
		return restore_and_fail(saved);
	end_non_angle();
	parse_attribute_specifier_seq();
	while (fixed(SimpleTokenType::OP_LSQUARE))
	{
		consume_fixed(SimpleTokenType::OP_LSQUARE);
		if (!begin_non_angle() || !parse_constant_expression() ||
			!consume_fixed(SimpleTokenType::OP_RSQUARE))
			return restore_and_fail(saved);
		end_non_angle();
		parse_attribute_specifier_seq();
	}
	return true;
}

bool PA6Parser::parse_new_initializer()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		if (!begin_non_angle() ||
			(!fixed(SimpleTokenType::OP_RPAREN) && !parse_expression_list()) ||
			!consume_fixed(SimpleTokenType::OP_RPAREN))
			return restore_and_fail(saved);
		end_non_angle();
		return true;
	}
	if (fixed(SimpleTokenType::OP_LBRACE))
		return parse_braced_init_list();
	return true;
}

bool PA6Parser::parse_delete_expression()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_COLON2))
		consume_fixed(SimpleTokenType::OP_COLON2);
	if (!consume_fixed(SimpleTokenType::KW_DELETE))
		return restore_and_fail(saved);
	if (consume_fixed(SimpleTokenType::OP_LSQUARE))
	{
		if (!consume_fixed(SimpleTokenType::OP_RSQUARE))
			return restore_and_fail(saved);
	}
	if (!parse_cast_expression())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_cast_operator()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle() ||
		!parse_type_id() || !consume_fixed(SimpleTokenType::OP_RPAREN))
		return restore_and_fail(saved);
	end_non_angle();
	return true;
}

bool PA6Parser::can_start_assignment_expression() const
{
	if (identifier() || literal())
		return true;
	const SimpleTokenType starters[] =
	{
		SimpleTokenType::KW_TRUE, SimpleTokenType::KW_FALSE,
		SimpleTokenType::KW_NULLPTR, SimpleTokenType::KW_THIS,
		SimpleTokenType::KW_SIZEOF, SimpleTokenType::KW_ALIGNOF,
		SimpleTokenType::KW_NOEXCEPT, SimpleTokenType::KW_NEW,
		SimpleTokenType::KW_DELETE, SimpleTokenType::KW_TYPEID,
		SimpleTokenType::KW_DYNAMIC_CAST, SimpleTokenType::KW_STATIC_CAST,
		SimpleTokenType::KW_REINTERPET_CAST, SimpleTokenType::KW_CONST_CAST,
		SimpleTokenType::KW_THROW, SimpleTokenType::KW_CHAR,
		SimpleTokenType::KW_CHAR16_T, SimpleTokenType::KW_CHAR32_T,
		SimpleTokenType::KW_WCHAR_T, SimpleTokenType::KW_BOOL,
		SimpleTokenType::KW_SHORT, SimpleTokenType::KW_INT,
		SimpleTokenType::KW_LONG, SimpleTokenType::KW_SIGNED,
		SimpleTokenType::KW_UNSIGNED, SimpleTokenType::KW_FLOAT,
		SimpleTokenType::KW_DOUBLE, SimpleTokenType::KW_VOID,
		SimpleTokenType::KW_AUTO, SimpleTokenType::KW_DECLTYPE,
		SimpleTokenType::KW_TYPENAME, SimpleTokenType::OP_LPAREN,
		SimpleTokenType::OP_LSQUARE, SimpleTokenType::OP_COLON2,
		SimpleTokenType::OP_INC, SimpleTokenType::OP_DEC,
		SimpleTokenType::OP_STAR, SimpleTokenType::OP_AMP,
		SimpleTokenType::OP_PLUS, SimpleTokenType::OP_MINUS,
		SimpleTokenType::OP_LNOT, SimpleTokenType::OP_COMPL
	};
	for (std::size_t i = 0; i < sizeof(starters) / sizeof(starters[0]); ++i)
	{
		if (fixed(starters[i]))
			return true;
	}
	return false;
}

bool PA6Parser::parse_throw_expression()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_THROW))
		return restore_and_fail(saved);
	Mark operand = mark();
	if (parse_assignment_expression())
		return true;
	restore(operand);
	if (can_start_assignment_expression())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_attribute_specifier()
{
	if (parse_alignment_specifier())
		return true;
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LSQUARE) ||
		!consume_fixed(SimpleTokenType::OP_LSQUARE) || !begin_non_angle() ||
		!parse_attribute_list() ||
		!consume_fixed(SimpleTokenType::OP_RSQUARE) ||
		!consume_fixed(SimpleTokenType::OP_RSQUARE))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	return true;
}

bool PA6Parser::parse_attribute_specifier_seq()
{
	while (parse_attribute_specifier()) {}
	return true;
}

bool PA6Parser::parse_alignment_specifier()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_ALIGNAS) ||
		!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle())
	{
		restore(saved);
		return false;
	}
	Mark type = mark();
	if (parse_type_id() && (!fixed(SimpleTokenType::OP_DOTS) ||
		consume_fixed(SimpleTokenType::OP_DOTS)) &&
		consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		end_non_angle();
		return true;
	}
	restore(type);
	if (!parse_assignment_expression() ||
		(fixed(SimpleTokenType::OP_DOTS) && !consume_fixed(SimpleTokenType::OP_DOTS)) ||
		!consume_fixed(SimpleTokenType::OP_RPAREN))
	{
		restore(saved);
		return false;
	}
	end_non_angle();
	return true;
}

bool PA6Parser::parse_attribute_list()
{
	Mark saved = mark();
	if (!parse_attribute_part())
		return restore_and_fail(saved);
	while (consume_fixed(SimpleTokenType::OP_COMMA))
	{
		if (!parse_attribute_part())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_attribute_part()
{
	if (parse_attribute())
	{
		if (fixed(SimpleTokenType::OP_DOTS))
			consume_fixed(SimpleTokenType::OP_DOTS);
		return true;
	}
	return true;
}

bool PA6Parser::parse_attribute()
{
	if (!parse_attribute_token())
		return false;
	if (fixed(SimpleTokenType::OP_LPAREN))
		return parse_attribute_argument_clause();
	return true;
}

bool PA6Parser::parse_attribute_token()
{
	Mark saved = mark();
	if (consume_identifier() && consume_fixed(SimpleTokenType::OP_COLON2) &&
		consume_identifier())
		return true;
	restore(saved);
	if (!consume_identifier())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_attribute_argument_clause()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle())
		return restore_and_fail(saved);
	while (!fixed(SimpleTokenType::OP_RPAREN))
	{
		if (eof() || !parse_balanced_token())
			return restore_and_fail(saved);
	}
	if (!consume_fixed(SimpleTokenType::OP_RPAREN))
		return restore_and_fail(saved);
	end_non_angle();
	return true;
}

bool PA6Parser::parse_balanced_token()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_LPAREN))
	{
		if (!consume_fixed(SimpleTokenType::OP_LPAREN))
			return restore_and_fail(saved);
		if (!begin_non_angle())
			return restore_and_fail(saved);
		while (!fixed(SimpleTokenType::OP_RPAREN))
		{
			if (eof() || !parse_balanced_token())
				return restore_and_fail(saved);
		}
		if (!consume_fixed(SimpleTokenType::OP_RPAREN))
			return restore_and_fail(saved);
		end_non_angle();
		return true;
	}
	if (fixed(SimpleTokenType::OP_LSQUARE))
	{
		if (!consume_fixed(SimpleTokenType::OP_LSQUARE))
			return restore_and_fail(saved);
		if (!begin_non_angle())
			return restore_and_fail(saved);
		while (!fixed(SimpleTokenType::OP_RSQUARE))
		{
			if (eof() || !parse_balanced_token())
				return restore_and_fail(saved);
		}
		if (!consume_fixed(SimpleTokenType::OP_RSQUARE))
			return restore_and_fail(saved);
		end_non_angle();
		return true;
	}
	if (fixed(SimpleTokenType::OP_LBRACE))
	{
		if (!consume_fixed(SimpleTokenType::OP_LBRACE))
			return restore_and_fail(saved);
		if (!begin_non_angle())
			return restore_and_fail(saved);
		while (!fixed(SimpleTokenType::OP_RBRACE))
		{
			if (eof() || !parse_balanced_token())
				return restore_and_fail(saved);
		}
		if (!consume_fixed(SimpleTokenType::OP_RBRACE))
			return restore_and_fail(saved);
		end_non_angle();
		return true;
	}
	if (eof() || fixed(SimpleTokenType::OP_RPAREN) ||
		fixed(SimpleTokenType::OP_RSQUARE) || fixed(SimpleTokenType::OP_RBRACE))
		return restore_and_fail(saved);
	if (!consume_current())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_class_member_declaration()
{
	Mark saved = mark();
	if (parse_attribute_specifier_seq() &&
		(parse_template_declaration() || parse_using_declaration() ||
		 parse_alias_declaration() || parse_static_assert_declaration()))
		return true;
	restore(saved);
	if (parse_attribute_specifier_seq() && parse_decl_specifier_seq())
	{
		if (parse_member_declarator_list() &&
			consume_fixed(SimpleTokenType::OP_SEMICOLON))
			return true;
	}
	restore(saved);
	if (parse_function_definition() &&
		(!fixed(SimpleTokenType::OP_SEMICOLON) ||
		 consume_fixed(SimpleTokenType::OP_SEMICOLON)))
		return true;
	restore(saved);
	return false;
}

bool PA6Parser::parse_member_declarator_list()
{
	Mark saved = mark();
	if (!parse_member_declarator())
		return restore_and_fail(saved);
	while (consume_fixed(SimpleTokenType::OP_COMMA))
	{
		if (!parse_member_declarator())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_member_declarator()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::OP_COLON))
	{
		consume_fixed(SimpleTokenType::OP_COLON);
		if (!parse_constant_expression())
			return restore_and_fail(saved);
		return true;
	}
	if (!parse_declarator())
	{
		restore(saved);
		return false;
	}
	while (kind(PA6TokenKind::ST_OVERRIDE) || kind(PA6TokenKind::ST_FINAL))
	{
		const PA6TokenKind wanted = kind(PA6TokenKind::ST_OVERRIDE) ?
			PA6TokenKind::ST_OVERRIDE : PA6TokenKind::ST_FINAL;
		if (!consume_kind(wanted))
			return restore_and_fail(saved);
	}
	if (fixed(SimpleTokenType::OP_ASS) && kind(PA6TokenKind::ST_ZERO, 1))
	{
		if (!parse_pure_specifier())
			return restore_and_fail(saved);
		return true;
	}
	if (fixed(SimpleTokenType::OP_COLON))
	{
		consume_fixed(SimpleTokenType::OP_COLON);
		if (!parse_constant_expression())
			return restore_and_fail(saved);
		return true;
	}
	if (fixed(SimpleTokenType::OP_ASS) || fixed(SimpleTokenType::OP_LBRACE))
	{
		if (!parse_brace_or_equal_initializer())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_member_specification()
{
	Mark saved = mark();
	while (!fixed(SimpleTokenType::OP_RBRACE))
	{
		if (eof() || !tick())
			return restore_and_fail(saved);
		if (parse_access_specifier())
		{
			if (!consume_fixed(SimpleTokenType::OP_COLON))
				return restore_and_fail(saved);
			continue;
		}
		if (!parse_class_member_declaration())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_access_specifier()
{
	if (fixed(SimpleTokenType::KW_PRIVATE) || fixed(SimpleTokenType::KW_PROTECTED) ||
		fixed(SimpleTokenType::KW_PUBLIC))
		return consume_current();
	return false;
}

bool PA6Parser::parse_class_or_decltype()
{
	return parse_class_name() || parse_decltype_specifier();
}

bool PA6Parser::parse_base_clause()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_COLON) ||
		!parse_base_specifier_list())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_base_specifier_list()
{
	Mark saved = mark();
	if (!parse_base_specifier_dots())
		return restore_and_fail(saved);
	while (consume_fixed(SimpleTokenType::OP_COMMA))
	{
		if (!parse_base_specifier_dots())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_base_specifier_dots()
{
	Mark saved = mark();
	if (!parse_base_specifier())
		return restore_and_fail(saved);
	if (fixed(SimpleTokenType::OP_DOTS))
		consume_fixed(SimpleTokenType::OP_DOTS);
	return true;
}

bool PA6Parser::parse_base_specifier()
{
	Mark saved = mark();
	parse_attribute_specifier_seq();
	if (fixed(SimpleTokenType::KW_VIRTUAL))
		consume_fixed(SimpleTokenType::KW_VIRTUAL);
	if (parse_access_specifier())
	{
		if (fixed(SimpleTokenType::KW_VIRTUAL))
			consume_fixed(SimpleTokenType::KW_VIRTUAL);
	}
	if (!parse_base_type_specifier())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_base_type_specifier()
{
	return parse_class_or_decltype();
}

bool PA6Parser::parse_virt_specifier()
{
	while (kind(PA6TokenKind::ST_OVERRIDE) || kind(PA6TokenKind::ST_FINAL))
	{
		const PA6TokenKind wanted = kind(PA6TokenKind::ST_OVERRIDE) ?
			PA6TokenKind::ST_OVERRIDE : PA6TokenKind::ST_FINAL;
		if (!consume_kind(wanted))
			return false;
	}
	return true;
}

bool PA6Parser::parse_pure_specifier()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_ASS) ||
		!consume_kind(PA6TokenKind::ST_ZERO))
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_ctor_initializer()
{
	Mark saved = mark();
	if (!fixed(SimpleTokenType::OP_COLON))
		return true;
	if (!consume_fixed(SimpleTokenType::OP_COLON) ||
		!parse_mem_initializer_list())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_mem_initializer_list()
{
	Mark saved = mark();
	if (!parse_mem_initializer_dots())
		return restore_and_fail(saved);
	while (consume_fixed(SimpleTokenType::OP_COMMA))
	{
		if (!parse_mem_initializer_dots())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_mem_initializer_dots()
{
	Mark saved = mark();
	if (!parse_mem_initializer())
		return restore_and_fail(saved);
	if (fixed(SimpleTokenType::OP_DOTS))
		consume_fixed(SimpleTokenType::OP_DOTS);
	return true;
}

bool PA6Parser::parse_mem_initializer()
{
	Mark saved = mark();
	if (!parse_mem_initializer_id())
		return restore_and_fail(saved);
	if (consume_fixed(SimpleTokenType::OP_LPAREN))
	{
		if (!begin_non_angle() ||
			(!fixed(SimpleTokenType::OP_RPAREN) && !parse_expression_list()) ||
			!consume_fixed(SimpleTokenType::OP_RPAREN))
			return restore_and_fail(saved);
		end_non_angle();
		return true;
	}
	if (!parse_braced_init_list())
		return restore_and_fail(saved);
	return true;
}

bool PA6Parser::parse_mem_initializer_id()
{
	return parse_class_or_decltype() || consume_identifier();
}

bool PA6Parser::parse_operator_template_suffix()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::OP_LT) || !begin_angle())
		return restore_and_fail(saved);
	if (!close_angle())
	{
		if (!parse_template_argument_list() || !close_angle())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_template_parameter_list()
{
	Mark saved = mark();
	if (!parse_template_parameter())
		return restore_and_fail(saved);
	while (consume_fixed(SimpleTokenType::OP_COMMA))
	{
		if (!parse_template_parameter())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_template_parameter()
{
	return parse_type_parameter() || parse_parameter_declaration();
}

bool PA6Parser::parse_type_parameter()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::KW_CLASS) || fixed(SimpleTokenType::KW_TYPENAME))
	{
		if (!consume_current())
			return restore_and_fail(saved);
		if (fixed(SimpleTokenType::OP_DOTS))
			consume_fixed(SimpleTokenType::OP_DOTS);
		if (identifier())
			consume_identifier();
		if (consume_fixed(SimpleTokenType::OP_ASS) && !parse_type_id())
		{
			restore(saved);
			return false;
		}
		return true;
	}
	if (!consume_fixed(SimpleTokenType::KW_TEMPLATE))
	{
		restore(saved);
		return false;
	}
	if (!consume_fixed(SimpleTokenType::OP_LT) || !begin_angle() ||
		!parse_template_parameter_list() || !close_angle() ||
		!consume_fixed(SimpleTokenType::KW_CLASS))
	{
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::OP_DOTS))
		consume_fixed(SimpleTokenType::OP_DOTS);
	if (identifier())
		consume_identifier();
	if (consume_fixed(SimpleTokenType::OP_ASS) && !parse_id_expression())
	{
		restore(saved);
		return false;
	}
	return true;
}

bool PA6Parser::parse_function_body()
{
	Mark saved = mark();
	if (fixed(SimpleTokenType::KW_TRY))
		return parse_function_try_block();
	if (fixed(SimpleTokenType::OP_COLON))
		if (!parse_ctor_initializer())
			return restore_and_fail(saved);
	if (fixed(SimpleTokenType::OP_LBRACE))
		return parse_compound_statement();
	if (consume_fixed(SimpleTokenType::OP_ASS))
	{
		if (consume_fixed(SimpleTokenType::KW_DEFAULT) ||
			consume_fixed(SimpleTokenType::KW_DELETE))
		{
			if (consume_fixed(SimpleTokenType::OP_SEMICOLON))
				return true;
			return restore_and_fail(saved);
		}
	}
	return restore_and_fail(saved);
}

bool PA6Parser::parse_exception_specification()
{
	Mark saved = mark();
	if (parse_dynamic_exception_specification() || parse_noexcept_specification())
		return true;
	return restore_and_fail(saved);
}

bool PA6Parser::parse_dynamic_exception_specification()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_THROW) ||
		!consume_fixed(SimpleTokenType::OP_LPAREN) || !begin_non_angle())
	{
		restore(saved);
		return false;
	}
	if (!fixed(SimpleTokenType::OP_RPAREN) && !parse_type_id_list())
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
	return true;
}

bool PA6Parser::parse_type_id_list()
{
	Mark saved = mark();
	if (!parse_type_id_dots())
		return restore_and_fail(saved);
	while (consume_fixed(SimpleTokenType::OP_COMMA))
	{
		if (!parse_type_id_dots())
			return restore_and_fail(saved);
	}
	return true;
}

bool PA6Parser::parse_type_id_dots()
{
	Mark saved = mark();
	if (!parse_type_id())
		return restore_and_fail(saved);
	if (fixed(SimpleTokenType::OP_DOTS))
		consume_fixed(SimpleTokenType::OP_DOTS);
	return true;
}

bool PA6Parser::parse_noexcept_specification()
{
	Mark saved = mark();
	if (!consume_fixed(SimpleTokenType::KW_NOEXCEPT))
		return restore_and_fail(saved);
	if (consume_fixed(SimpleTokenType::OP_LPAREN))
	{
		if (!begin_non_angle() || !parse_constant_expression() ||
			!consume_fixed(SimpleTokenType::OP_RPAREN))
			return restore_and_fail(saved);
		end_non_angle();
	}
	return true;
}

bool PA6Parser::parse_type_id()
{
	Mark saved = mark();
	if (!parse_type_specifier_seq())
	{
		restore(saved);
		return false;
	}
	if (fixed(SimpleTokenType::OP_STAR) || fixed(SimpleTokenType::OP_AMP) ||
		fixed(SimpleTokenType::OP_LAND) || fixed(SimpleTokenType::OP_LPAREN) ||
		fixed(SimpleTokenType::OP_LSQUARE) || fixed(SimpleTokenType::OP_DOTS))
	{
		Mark abstract = mark();
		if (!parse_abstract_declarator())
			restore(abstract);
	}
	return true;
}

bool PA6Parser::parse_constant_expression()
{
	return parse_conditional_expression();
}

} // namespace pa6_internal
