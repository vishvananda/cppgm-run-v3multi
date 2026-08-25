#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

void PA11SemanticModel::record_constant_initializer(SemanticFactId fact_id,
	ScopeId scope)
{
	if (!fact_id.valid() || fact_id.value >= semantic_facts_.size() ||
		!scope.valid() || scope.value >= scopes_.size() ||
		scopes_[scope.value].kind != ScopeKind::Namespace)
		return;
	SemanticFact& fact = semantic_facts_[fact_id.value];
	if (fact.kind == SemanticFactKind::Variable ||
		fact.kind == SemanticFactKind::BracedInitList)
	{
		if (fact.child_count == 0 || fact.child_begin == InvalidIdentityValue)
			return;
		for (std::size_t i = 0; i < fact.child_count; ++i)
			record_constant_initializer(
				semantic_children_[fact.child_begin + i], scope);
		return;
	}
	const TypeId object_type = expression_object_type(fact.type);
	if (!fact.constant_value_evaluated && fact.source != NULL &&
		(integral_id(object_type) || enumeration_id(object_type)))
	{
		// This is the PA12 owner boundary: each direct scalar initializer fact is
		// evaluated once, and the typed result (or the attempted=false result) is
		// retained for PA15.  No lowering path retries this evaluator.
		fact.constant_value_evaluated = true;
		try
		{
			const ConstValue value = eval_constexpr(*fact.source, scope);
			if (value.valid)
			{
				fact.has_constant_value = true;
				fact.constant_value = value.value;
				fact.constant_value_unsigned = value.is_unsigned;
			}
		}
		catch (const NonConstantExpression&)
		{
			// A typed nonconstant fact is expected for an initializer that
			// cannot be represented by a static LowIR value.
		}
	}
}

SemanticFactId PA11SemanticModel::semantic_literal(const PA10AstNode& node)
{
	TypeId type;
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	std::size_t element_count = 0;
	if (node.kind == PA10NodeKind::Literal)
	{
		type = fundamental(node.literal.type);
		element_count = node.literal.element_count;
		if (element_count != 0)
		{
			type = make_array(make_cv(type, 1u), false,
				ArrayBound(element_count));
			category = SemanticValueCategory::Lvalue;
		}
	}
	else if (node.kind == PA10NodeKind::KeywordLiteral)
	{
		if (node.token == SimpleTokenType::KW_TRUE ||
			node.token == SimpleTokenType::KW_FALSE)
			type = fundamental(FundamentalType::Bool);
		else if (node.token == SimpleTokenType::KW_NULLPTR)
			type = fundamental(FundamentalType::NullptrT);
		else
			throw std::runtime_error("PA12 unsupported keyword literal");
	}
	else
		throw std::runtime_error("PA12 expected literal");
	SemanticFact fact(SemanticFactKind::Literal, type, category, &node);
	fact.token = node.token;
	fact.literal_element_count = element_count;
	if (element_count == 0 && node.kind == PA10NodeKind::Literal &&
		integral_type(node.literal.type))
	{
		const ConstValue value = literal_constant(node);
		fact.has_constant_value = value.valid;
		fact.constant_value = value.value;
		fact.constant_value_unsigned = value.is_unsigned;
		fact.constant_value_evaluated = true;
	}
	return make_semantic_fact(fact);
}

ExprInfo PA11SemanticModel::semantic_unary_expression(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 1 || !node.has_token)
		throw std::runtime_error("PA12 invalid unary expression");
	const ExprInfo operand = semantic_expression(node.children.front(), scope);
	TypeId type = expression_object_type(operand.type);
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	switch (node.token)
	{
	case SimpleTokenType::OP_AMP:
		if (operand.category != SemanticValueCategory::Lvalue)
			throw std::runtime_error("PA12 address-of requires lvalue");
		if (node.children.front().kind == PA10NodeKind::IdExpression)
		{
			const std::vector<ValueRef> values = lookup_value_path(
				name_path(node.children.front()), scope);
			if (values.size() == 1 && operand.fact.valid() &&
				values.front().binding ==
				semantic_facts_[operand.fact.value].binding &&
				!is_static_member(values.front().binding) &&
				values.front().scope.valid() &&
				values.front().scope.value < scopes_.size() &&
				scopes_[values.front().scope.value].kind == ScopeKind::Class)
			{
				const NamedRecordId owner =
					scopes_[values.front().scope.value].record;
				type = make_member_pointer(owner,
					binding(values.front().binding).type);
				break;
			}
		}
		type = make_pointer(operand.type);
		break;
	case SimpleTokenType::OP_STAR:
	{
		TypeId pointer = strip_cv_type(expression_object_type(operand.type));
		if (type_kind(pointer) == TypeKind::Array)
		{
			pointer = make_pointer(types_[pointer.value].child);
			record_builtin_conversion(operand, pointer);
		}
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 dereference requires pointer");
		if (type_kind(strip_cv_type(expression_object_type(operand.type))) ==
			TypeKind::Pointer)
			record_builtin_conversion(operand, pointer);
		type = types_[pointer.value].child;
		category = SemanticValueCategory::Lvalue;
		break;
	}
	case SimpleTokenType::OP_INC:
	case SimpleTokenType::OP_DEC:
		if (operand.category != SemanticValueCategory::Lvalue ||
			!modifiable_lvalue(operand.type) ||
			(!integral_id(operand.type) && !floating_id(operand.type) && !pointer_id(operand.type)))
			throw std::runtime_error("PA12 increment requires modifiable lvalue");
		type = strip_top_cv_type(operand.type);
		record_builtin_conversion(operand, integral_id(operand.type) ?
			promote_integral_type(operand.type) : type);
		category = SemanticValueCategory::Lvalue;
		break;
	case SimpleTokenType::OP_PLUS:
		if (type_kind(strip_cv_type(type)) == TypeKind::Array)
		{
			const TypeId array = strip_cv_type(type);
			type = make_pointer(types_[array.value].child);
			record_builtin_conversion(operand, type);
			break;
		}
	case SimpleTokenType::OP_MINUS:
	case SimpleTokenType::OP_COMPL:
		if (!integral_id(operand.type) &&
			(node.token == SimpleTokenType::OP_COMPL || !floating_id(operand.type)))
			throw std::runtime_error("PA12 unary arithmetic requires integral");
		type = node.token == SimpleTokenType::OP_COMPL ?
			common_integral_type(operand.type, operand.type) :
			common_arithmetic_type(operand.type, operand.type);
		record_builtin_conversion(operand, type);
		break;
	case SimpleTokenType::OP_LNOT:
		if (!scalar_id(operand.type))
			throw std::runtime_error("PA12 logical negation requires scalar");
		record_builtin_conversion(operand, fundamental(FundamentalType::Bool));
		type = fundamental(FundamentalType::Bool);
		break;
	default:
		throw std::runtime_error("PA12 unsupported unary operator");
	}
	return ExprInfo(make_expression_fact(SemanticFactKind::UnaryExpression,
		type, category, node, std::vector<SemanticFactId>(1, operand.fact)),
		type, category, false);
}

} // namespace pa11_semantic_internal
