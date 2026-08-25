#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

namespace
{

bool address_addend_fits(__int128 value, long long* result)
{
	if (value < static_cast<__int128>(std::numeric_limits<long long>::min()) ||
		value > static_cast<__int128>(std::numeric_limits<long long>::max()))
		return false;
	*result = static_cast<long long>(value);
	return true;
}

}

bool PA11SemanticModel::constant_integer_value(SemanticFactId fact_id,
	__int128* value, bool* is_unsigned) const
{
	if (!fact_id.valid() || fact_id.value >= semantic_facts_.size() ||
		value == NULL || is_unsigned == NULL)
		return false;
	const SemanticFact& fact = semantic_facts_[fact_id.value];
	if (fact.has_constant_value)
	{
		*value = fact.constant_value;
		*is_unsigned = fact.constant_value_unsigned;
		return true;
	}
	if (fact.has_literal_value)
	{
		__int128 literal = static_cast<__int128>(fact.literal_value);
		if (fact.literal_value_negative) literal = -literal;
		*value = literal;
		*is_unsigned = fact.literal_value_unsigned;
		return true;
	}
	return false;
}

void PA11SemanticModel::record_constant_expression_value(
	SemanticFactId fact_id, ScopeId scope)
{
	if (!fact_id.valid() || fact_id.value >= semantic_facts_.size())
		return;
	const SemanticFact& fact = semantic_facts_[fact_id.value];
	const TypeId object_type = expression_object_type(fact.type);
	SemanticFact& owner = semantic_facts_[fact_id.value];
	if (!owner.constant_value_evaluated && owner.source != NULL &&
		(integral_id(object_type) || enumeration_id(object_type)))
	{
		// PA12 is the sole owner of this bounded constant-expression attempt.
		// The evaluated bit is set even when the typed value is invalid so a
		// later lowering phase cannot retry semantic evaluation.
		owner.constant_value_evaluated = true;
		try
		{
			const ConstValue value = eval_constexpr(*owner.source, scope);
			if (value.valid)
			{
				owner.has_constant_value = true;
				owner.constant_value = value.value;
				owner.constant_value_unsigned = value.is_unsigned;
			}
		}
		catch (const NonConstantExpression&)
		{
			// The typed invalid result is retained by constant_value_evaluated.
		}
	}
}

ConstantAddressFactId PA11SemanticModel::make_constant_address_fact(
	const ConstantAddressFact& fact)
{
	const ConstantAddressFactId result(constant_address_facts_.size());
	constant_address_facts_.push_back(fact);
	return result;
}

bool PA11SemanticModel::resolve_constant_address(SemanticFactId fact_id,
	ScopeId scope, ConstantAddressContext context, ConstantAddressFact* result)
{
	if (result == NULL || !fact_id.valid() ||
		fact_id.value >= semantic_facts_.size())
		return false;
	result->evaluated = true;
	const SemanticFact& fact = semantic_facts_[fact_id.value];
	if (fact.kind == SemanticFactKind::IdExpression)
	{
		if (!fact.binding.valid() || fact.binding.value >= bindings_.size())
			return false;
		const Binding& target = binding(fact.binding);
		if (target.kind == BindingKind::Function)
		{
			// A function identity is already an address-producing semantic fact;
			// both direct function-to-pointer use and explicit &function preserve
			// the canonical function binding.
			result->kind = ConstantAddressKind::SymbolAddend;
			result->target = fact.binding;
			result->valid = true;
			return true;
		}
		if (target.kind != BindingKind::Variable)
			return false;
		bool array_decay = false;
		if (fact.conversion_begin != InvalidIdentityValue)
			for (std::size_t i = 0; i < fact.conversion_count; ++i)
				if (conversion_facts_[fact.conversion_begin + i].kind ==
					ConversionKind::ArrayToPointer)
				{
					array_decay = true;
					break;
				}
		const bool object_address =
			context == ConstantAddressContext::ObjectAddress &&
			fact.category == SemanticValueCategory::Lvalue;
		const TypeId object_type = strip_cv_type(
			expression_object_type(fact.type));
		const bool decay_context =
			context == ConstantAddressContext::ArrayDecay && object_type.valid() &&
			type_kind(object_type) == TypeKind::Array;
		if (!object_address && !array_decay &&
			!decay_context)
			return false;
		result->kind = ConstantAddressKind::SymbolAddend;
		result->target = fact.binding;
		result->valid = true;
		return true;
	}

	std::vector<SemanticFactId> children;
	if (fact.child_begin != InvalidIdentityValue)
		for (std::size_t i = 0; i < fact.child_count; ++i)
			children.push_back(semantic_children_[fact.child_begin + i]);
	if (fact.kind == SemanticFactKind::UnaryExpression && children.size() == 1 &&
		(fact.token == SimpleTokenType::OP_AMP ||
		 fact.token == SimpleTokenType::OP_PLUS))
		return resolve_constant_address(children.front(), scope,
			fact.token == SimpleTokenType::OP_AMP ?
			ConstantAddressContext::ObjectAddress : context, result);
	if (fact.kind == SemanticFactKind::CastExpression && children.size() == 1)
	{
		ConstantAddressContext child_context = context;
		if (fact.conversion_begin != InvalidIdentityValue)
			for (std::size_t i = 0; i < fact.conversion_count; ++i)
				if (conversion_facts_[fact.conversion_begin + i].kind ==
					ConversionKind::ArrayToPointer)
				{
					child_context = ConstantAddressContext::ArrayDecay;
					break;
				}
		return resolve_constant_address(children.front(), scope,
			child_context, result);
	}

	if (fact.kind == SemanticFactKind::BinaryExpression && children.size() == 2 &&
		(fact.token == SimpleTokenType::OP_PLUS ||
		 fact.token == SimpleTokenType::OP_MINUS))
	{
		const TypeId left = strip_cv_type(expression_object_type(
			semantic_facts_[children[0].value].type));
		const TypeId right = strip_cv_type(expression_object_type(
			semantic_facts_[children[1].value].type));
		const bool left_pointer = left.valid() &&
			(type_kind(left) == TypeKind::Pointer ||
			 type_kind(left) == TypeKind::Array);
		const bool right_pointer = right.valid() &&
			(type_kind(right) == TypeKind::Pointer ||
			 type_kind(right) == TypeKind::Array);
		SemanticFactId pointer_fact;
		SemanticFactId integer_fact;
		bool negate = false;
		if (left_pointer && !right_pointer &&
			fact.token == SimpleTokenType::OP_PLUS)
		{
			pointer_fact = children[0];
			integer_fact = children[1];
		}
		else if (left_pointer && !right_pointer &&
			fact.token == SimpleTokenType::OP_MINUS)
		{
			pointer_fact = children[0];
			integer_fact = children[1];
			negate = true;
		}
		else if (!left_pointer && right_pointer &&
			fact.token == SimpleTokenType::OP_PLUS)
		{
			pointer_fact = children[1];
			integer_fact = children[0];
		}
		else
			return false;

		ConstantAddressFact base;
		if (!resolve_constant_address(pointer_fact, scope,
			ConstantAddressContext::Value, &base)) return false;
		__int128 index = 0;
		bool index_unsigned = false;
		if (!constant_integer_value(integer_fact, &index, &index_unsigned))
		{
			record_constant_expression_value(integer_fact, scope);
		}
		if (!constant_integer_value(integer_fact, &index, &index_unsigned))
			return false;
		TypeId pointer_type = strip_cv_type(expression_object_type(
			semantic_facts_[pointer_fact.value].type));
		if (!pointer_type.valid() ||
			(type_kind(pointer_type) != TypeKind::Pointer &&
			 type_kind(pointer_type) != TypeKind::Array))
			return false;
		const TypeId element = types_[pointer_type.value].child;
		const __int128 scale = static_cast<__int128>(type_size(element));
		const __int128 offset = index * scale * (negate ? -1 : 1);
		long long addend = 0;
		if (!address_addend_fits(
			static_cast<__int128>(base.byte_addend) + offset, &addend))
			return false;
		*result = base;
		result->evaluated = true;
		result->valid = true;
		result->kind = ConstantAddressKind::SymbolAddend;
		result->byte_addend = addend;
		result->element_type = TypeId();
		result->index_type = TypeId();
		result->index_fact = SemanticFactId();
		result->index_value = 0;
		result->index_unsigned = false;
		return true;
	}

	if (fact.kind == SemanticFactKind::SubscriptExpression &&
		children.size() == 2)
	{
		TypeId sequence = strip_cv_type(expression_object_type(
			semantic_facts_[children.front().value].type));
		// A global pointer's stored value is not a static address expression;
		// only an array object can retain the direct array-element projection.
		if (context != ConstantAddressContext::ObjectAddress ||
			!sequence.valid() || type_kind(sequence) != TypeKind::Array)
			return false;
		ConstantAddressFact base;
		if (!resolve_constant_address(children.front(), scope,
			ConstantAddressContext::Value, &base) ||
			base.kind != ConstantAddressKind::SymbolAddend)
			return false;
		__int128 index = 0;
		bool index_unsigned = false;
		if (!constant_integer_value(children.back(), &index, &index_unsigned))
		{
			record_constant_expression_value(children.back(), scope);
		}
		if (!constant_integer_value(children.back(), &index, &index_unsigned))
			return false;
		const TypeId element = types_[sequence.value].child;
		const __int128 offset = index * static_cast<__int128>(type_size(element));
		long long addend = 0;
		if (!address_addend_fits(
			static_cast<__int128>(base.byte_addend) + offset, &addend))
			return false;
		if (base.byte_addend != 0)
		{
			*result = base;
			result->evaluated = true;
			result->valid = true;
			result->byte_addend = addend;
			return true;
		}
		*result = base;
		result->evaluated = true;
		result->valid = true;
		result->kind = ConstantAddressKind::ArrayElement;
		result->byte_addend = addend;
		result->element_type = fact.type;
		result->index_type = expression_object_type(
			semantic_facts_[children.back().value].type);
		result->index_fact = children.back();
		result->index_value = index;
		result->index_unsigned = index_unsigned;
		return true;
	}
	return false;
}

void PA11SemanticModel::record_constant_address(SemanticFactId fact_id,
	ScopeId scope)
{
	if (!fact_id.valid() || fact_id.value >= semantic_facts_.size())
		return;
	SemanticFact& fact = semantic_facts_[fact_id.value];
	if (fact.constant_address.valid()) return;
	ConstantAddressFact result;
	result.evaluated = true;
	result.valid = resolve_constant_address(fact_id, scope,
		ConstantAddressContext::Value, &result);
	fact.constant_address = make_constant_address_fact(result);
}

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
	record_constant_expression_value(fact_id, scope);
	// The complete expression tree has now been folded once where its typed
	// facts permit it.  Resolve the address/relocation relation exactly once at
	// the initializer owner; descendants are never retried independently.
	record_constant_address(fact_id, scope);
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
