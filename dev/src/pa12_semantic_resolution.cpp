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
} // namespace pa11_semantic_internal
