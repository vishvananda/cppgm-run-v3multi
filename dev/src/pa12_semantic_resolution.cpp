#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

TypeId PA11SemanticModel::member_function_expression_type(TypeId type,
	ScopeId member_scope, BindingId binding_id)
{
	if (!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class ||
		type_kind(type) != TypeKind::Function || is_static_member(binding_id))
		return type;
	const TypeId object_pointer = member_object_pointer_type(type, member_scope);
	if (!object_pointer.valid())
		return type;
	const TypeKey& function = types_[type.value];
	std::vector<TypeId> parameters;
	parameters.push_back(object_pointer);
	parameters.insert(parameters.end(), function.parameters.begin(),
		function.parameters.end());
	return make_function(parameters, function.variadic, function.result);
}
const PA10AstNode* PA11SemanticModel::target_function_id(
	const PA10AstNode& node, ScopeId scope)
{
	if (node.kind == PA10NodeKind::ParenthesizedExpression)
	{
		if (node.children.size() != 1)
			return NULL;
		return target_function_id(node.children.front(), scope);
	}
	if (node.kind != PA10NodeKind::IdExpression &&
		!(node.kind == PA10NodeKind::DeclSpecifier &&
			node.identifier_declspecifier))
		return NULL;
	const std::vector<ValueRef> values = lookup_value_path(name_path(node), scope);
	if (values.size() <= 1)
		return NULL;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		const Binding& value = binding(values[i].binding);
		if (value.kind != BindingKind::Function ||
			type_kind(value.type) != TypeKind::Function)
			return NULL;
	}
	return &node;
}
FunctionIdResolution PA11SemanticModel::resolve_function_id_target(
	const PA10AstNode& node, ScopeId scope, TypeId target)
{
	if (node.kind != PA10NodeKind::IdExpression &&
		!(node.kind == PA10NodeKind::DeclSpecifier &&
			node.identifier_declspecifier))
		return FunctionIdResolution();
	const std::vector<ValueRef> values = lookup_value_path(name_path(node), scope);
	ValueRef selected;
	ConversionChoice selected_conversion;
	bool have_selected = false;
	bool ambiguous = false;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		const Binding& value = binding(values[i].binding);
		if (value.kind != BindingKind::Function ||
			type_kind(value.type) != TypeKind::Function)
			continue;
		ConversionChoice conversion;
		const TypeId target_type = strip_cv_type(target);
		if (type_kind(target_type) == TypeKind::MemberPointer)
		{
			const TypeKey& member_pointer = types_[target_type.value];
			const bool owner_match = values[i].scope.valid() &&
				values[i].scope.value < scopes_.size() &&
				scopes_[values[i].scope.value].kind == ScopeKind::Class &&
				!is_static_member(values[i].binding) &&
				scopes_[values[i].scope.value].record ==
				member_pointer.named;
			conversion = owner_match && member_pointer.child == value.type ?
				ConversionChoice(true, 0, ConversionKind::Identity) :
				ConversionChoice();
		}
		else
			conversion = conversion_for(value.type,
				SemanticValueCategory::Lvalue, target, &node);
		if (!conversion.valid)
			continue;
		if (!have_selected || conversion.rank < selected_conversion.rank)
		{
			have_selected = true;
			selected = values[i];
			selected_conversion = conversion;
			ambiguous = false;
		}
		else if (conversion.rank == selected_conversion.rank)
			ambiguous = true;
	}
	return have_selected && !ambiguous ? FunctionIdResolution(true, selected,
		selected_conversion) : FunctionIdResolution();
}
bool PA11SemanticModel::functional_cast_target(const PA10AstNode& node,
	ScopeId scope, TypeId* target)
{
	if (builtin_cast_target(node, target))
		return true;
	if (node.kind == PA10NodeKind::TypeId)
	{
		*target = type_from_type_id(node, scope);
		return target->valid();
	}
	if (node.kind != PA10NodeKind::IdExpression || node.has_token)
		return false;
	const NamePath path = name_path(node);
	if (!lookup_value_path(path, scope).empty())
		return false;
	*target = lookup_type_path(path, scope);
	return target->valid();
}
bool PA11SemanticModel::functional_cast_target_supported(TypeId target) const
{
	return void_id(target) || scalar_id(target) || enumeration_id(target);
}
ExprInfo PA11SemanticModel::semantic_id_expression_selected(
	const PA10AstNode& node, ScopeId scope,
	const FunctionIdResolution& resolution)
{
	(void)scope;
	if (!resolution.valid)
		throw std::runtime_error("PA12 target does not select a function");
	const Binding& value = binding(resolution.selected.binding);
	if (value.kind != BindingKind::Function ||
		type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA12 target selected a non-function");
	const TypeId expression_type = member_function_expression_type(
		value.type, resolution.selected.scope, resolution.selected.binding);
	SemanticFact fact(SemanticFactKind::IdExpression, expression_type,
		SemanticValueCategory::Lvalue, &node);
	fact.binding = resolution.selected.binding;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, name_path(node));
	return ExprInfo(result, expression_type, SemanticValueCategory::Lvalue, false);
}
ExprInfo PA11SemanticModel::semantic_expression_for_target(
	const PA10AstNode& node, ScopeId scope, TypeId target)
{
	if (node.kind == PA10NodeKind::BracedInitList)
		return semantic_braced_init_list(node, target, scope);
	const PA10AstNode* function_id = target_function_id(node, scope);
	if (function_id == NULL)
	{
		if (node.kind == PA10NodeKind::DeclSpecifier &&
			node.identifier_declspecifier)
			return semantic_id_expression(node, scope);
		return semantic_expression(node, scope);
	}
	const FunctionIdResolution resolution = resolve_function_id_target(
		*function_id, scope, target);
	if (!resolution.valid)
		throw std::runtime_error("PA12 no function matches target type");
	return semantic_id_expression_selected(*function_id, scope, resolution);
}
ExprInfo PA11SemanticModel::semantic_cast_to_target(
	const PA10AstNode& node, TypeId target, const ExprInfo& operand)
{
	const TypeId source = expression_object_type(operand.type);
	const TypeKind target_kind = type_kind(target);
	if (target_kind == TypeKind::LvalueReference ||
		target_kind == TypeKind::RvalueReference)
	{
		const TypeId referred = types_[target.value].child;
		if (!qualification_convertible(source, referred))
			throw std::runtime_error("PA12 invalid reference cast");
		if (operand.category == SemanticValueCategory::Lvalue)
		{
			const SemanticValueCategory category =
				target_kind == TypeKind::RvalueReference ?
				SemanticValueCategory::Xvalue : SemanticValueCategory::Lvalue;
			semantic_facts_[operand.fact.value].type = target;
			semantic_facts_[operand.fact.value].category = category;
			return ExprInfo(operand.fact, target, category, false);
		}
		throw std::runtime_error("PA12 invalid reference cast category");
	}
	bool valid = false;
	ConversionKind kind = ConversionKind::Integral;
	if (void_id(target))
		valid = scalar_id(source) || type_kind(source) == TypeKind::Function;
	else if (integral_id(target))
	{
		if (bool_id(target))
		{
			const ConversionChoice choice = conversion_for(source,
				operand.category, target,
				semantic_facts_[operand.fact.value].source,
				operand.integer_zero);
			valid = choice.valid;
			kind = choice.kind;
		}
		else
		{
			valid = integral_id(source) || enumeration_id(source) ||
				nullptr_id(source);
			kind = ConversionKind::Integral;
		}
	}
	else if (floating_id(target) || pointer_id(target))
	{
		const ConversionChoice choice = conversion_for(source,
			operand.category, target,
			semantic_facts_[operand.fact.value].source,
			operand.integer_zero);
		valid = choice.valid;
		kind = choice.kind;
	}
	else if (type_kind(strip_cv_type(target)) == TypeKind::MemberPointer)
	{
		valid = type_kind(strip_cv_type(source)) == TypeKind::MemberPointer &&
			strip_cv_type(source) == strip_cv_type(target);
		kind = ConversionKind::Identity;
	}
	else if (type_kind(strip_cv_type(target)) == TypeKind::Named)
		valid = integral_id(source) || source == target;
	if (!valid)
		throw std::runtime_error("PA12 invalid explicit cast");
	if (node.token == SimpleTokenType::KW_STATIC_CAST &&
		type_kind(strip_cv_type(target)) == TypeKind::MemberPointer &&
		kind == ConversionKind::Identity &&
		strip_cv_type(source) == strip_cv_type(target))
		return operand;
	const SemanticFactId result = make_expression_fact(
		SemanticFactKind::CastExpression, target,
		SemanticValueCategory::Prvalue, node,
		std::vector<SemanticFactId>(1, operand.fact));
	set_fact_conversion(result,
		add_conversion(source, target, kind, 0));
	return ExprInfo(result, target, SemanticValueCategory::Prvalue, false);
}
ExprInfo PA11SemanticModel::semantic_functional_cast(
	const PA10AstNode& node, ScopeId scope, TypeId target,
	const PA10AstNode& argument_node)
{
	if (!functional_cast_target_supported(target))
		throw std::runtime_error("PA12 unsupported functional cast target");
	if (argument_node.children.empty())
	{
		if (void_id(target))
			throw std::runtime_error("PA12 invalid zero-argument functional cast");
		SemanticFact fact(SemanticFactKind::Literal, target,
			SemanticValueCategory::Prvalue, &node);
		fact.has_literal_value = true;
		fact.literal_value = 0;
		fact.literal_value_unsigned = false;
		fact.literal_value_negative = false;
		const SemanticFactId result = make_semantic_fact(fact);
		return ExprInfo(result, target, SemanticValueCategory::Prvalue,
			integral_id(target));
	}
	if (argument_node.children.size() != 1)
		throw std::runtime_error("PA12 invalid functional cast arity");
	const ExprInfo operand = semantic_expression(
		argument_node.children.front(), scope);
	return semantic_cast_to_target(node, target, operand);
}
} // namespace pa11_semantic_internal
