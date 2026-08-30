#include "pa11_semantic_model.h"

#include <limits>

namespace pa11_semantic_internal
{

ExprInfo PA11SemanticModel::semantic_new_expression(const PA10AstNode& node,
	ScopeId scope)
{
	if (node.kind != PA10NodeKind::NewExpression || node.children.empty())
		throw std::runtime_error("PA12 invalid new expression");
	std::size_t child = 0;
	const bool global_scope = node.children[child].kind ==
		PA10NodeKind::GlobalScope;
	if (global_scope)
		++child;
	if (child >= node.children.size() || node.children[child].kind !=
		PA10NodeKind::NewPlacement)
		throw std::runtime_error("PA12 placement new is required in this checkpoint");
	const PA10AstNode& placement = node.children[child++];
	if (placement.children.size() != 1 || placement.children.front().kind !=
		PA10NodeKind::ParenArgumentList)
		throw std::runtime_error("PA12 invalid new placement arguments");
	if (child >= node.children.size() || node.children[child].kind !=
		PA10NodeKind::TypeId)
		throw std::runtime_error("PA12 new expression type-id is missing");
	const TypeId allocated_type = type_from_type_id(node.children[child++], scope);
	const TypeId object = strip_top_cv_type(allocated_type);
	const NamedRecordId record = class_record_for_object_type(object);
	if (!complete_object_type(object) || !record.valid() ||
		record.value >= named_.size() || named_[record.value].kind != NamedKind::Class ||
		named_[record.value].class_tag == ClassTag::Union ||
		named_[record.value].has_virtual_member)
		throw std::runtime_error(
			"PA12 new expression requires a complete non-polymorphic class");

	// The allocation size is a compiler-synthesized integer value.  Keeping it
	// as a normal semantic literal lets the existing typed call selector publish
	// the size_t conversion, which is also the LowIR boundary used by the PA15
	// builtin size arguments.
	const std::size_t object_size = type_size(object);
	if (object_size > static_cast<std::size_t>(
		std::numeric_limits<long long>::max()))
		throw std::runtime_error("PA12 new expression size is too large");
	const TypeId size_type = object_size <= static_cast<std::size_t>(
		std::numeric_limits<int>::max()) ? fundamental(FundamentalType::Int) :
		fundamental(FundamentalType::UnsignedLongInt);
	SemanticFact size_fact(SemanticFactKind::Literal, size_type,
		SemanticValueCategory::Prvalue, &node);
	size_fact.has_literal_value = true;
	size_fact.literal_value = static_cast<std::uint64_t>(object_size);
	size_fact.has_constant_value = true;
	size_fact.constant_value = static_cast<__int128>(object_size);
	size_fact.constant_value_evaluated = true;
	const ExprInfo size_argument(make_semantic_fact(size_fact), size_type,
		SemanticValueCategory::Prvalue, false);

	const PA10AstNode& placement_arguments = placement.children.front();
	std::vector<const PA10AstNode*> allocation_argument_nodes;
	std::vector<ExprInfo> allocation_arguments;
	allocation_argument_nodes.reserve(placement_arguments.children.size() + 1);
	allocation_arguments.reserve(placement_arguments.children.size() + 1);
	// There is no source node for the inserted size expression; the owning new
	// node is its typed source boundary and is never reparsed or rendered here.
	allocation_argument_nodes.push_back(&node);
	allocation_arguments.push_back(size_argument);
	for (std::size_t i = 0; i < placement_arguments.children.size(); ++i)
	{
		const PA10AstNode& argument = placement_arguments.children[i];
		allocation_argument_nodes.push_back(&argument);
		if (target_function_id(argument, scope) != NULL)
			allocation_arguments.push_back(ExprInfo());
		else
			allocation_arguments.push_back(semantic_expression(argument, scope));
	}
	NamePath allocation_path;
	allocation_path.global = global_scope;
	allocation_path.components.push_back(operator_name(
		PA10OperatorFunctionKind::New, SimpleTokenType::OP_SEMICOLON));
	const std::vector<ValueRef> candidates = lookup_value_path(allocation_path,
		scope, SourcePoint(node.source_begin));
	if (candidates.empty())
		throw std::runtime_error("PA12 placement allocation function is missing");
	const TypedFunctionSelection selection = select_typed_function(candidates,
		allocation_argument_nodes, allocation_arguments, scope);
	if (!selection.valid())
		throw std::runtime_error("PA12 placement allocation selection is incomplete");
	const TypeId allocation_result = function_result_type(selection.type);
	if (type_kind(strip_cv_type(expression_object_type(allocation_result))) !=
		TypeKind::Pointer)
		throw std::runtime_error("PA12 allocation function does not return a pointer");
	SemanticFact allocation_call(SemanticFactKind::CallExpression,
		allocation_result, SemanticValueCategory::Prvalue, &node);
	allocation_call.has_callee = true;
	allocation_call.bool_context_operand = bool_id(allocation_result);
	allocation_call.direct_bool_boundary = bool_id(allocation_result);
	allocation_call.selected_binding = selection.selected.binding;
	allocation_call.selected_scope = selection.selected.scope;
	allocation_call.callable_type = selection.type;
	const SemanticFactId allocation_id = make_semantic_fact(allocation_call);
	std::vector<SemanticFactId> allocation_children;
	allocation_children.reserve(selection.arguments.size());
	for (std::size_t i = 0; i < selection.arguments.size(); ++i)
	{
		if (!selection.arguments[i].fact.valid())
			throw std::runtime_error("PA12 allocation argument fact is missing");
		allocation_children.push_back(selection.arguments[i].fact);
	}
	set_semantic_children(allocation_id, allocation_children);

	std::vector<const PA10AstNode*> constructor_arguments;
	bool value_initialize = false;
	if (child < node.children.size())
	{
		const PA10AstNode& initializer = node.children[child++];
		if (initializer.kind != PA10NodeKind::Initializer ||
			initializer.children.size() != 1)
			throw std::runtime_error("PA12 invalid new initializer");
		const PA10AstNode& clause = initializer.children.front();
		if (clause.kind == PA10NodeKind::ParenInitializer)
		{
			value_initialize = clause.children.empty();
			for (std::size_t i = 0; i < clause.children.size(); ++i)
				constructor_arguments.push_back(&clause.children[i]);
		}
		else if (clause.kind == PA10NodeKind::BracedInitList)
		{
			for (std::size_t i = 0; i < clause.children.size(); ++i)
				constructor_arguments.push_back(&clause.children[i]);
		}
		else
			throw std::runtime_error("PA12 unsupported new initializer");
	}
	if (child != node.children.size())
		throw std::runtime_error("PA12 new expression has extra children");
	const ExprInfo constructor = semantic_aggregate_constructor_value(node,
		allocated_type, scope, constructor_arguments, value_initialize);
	SemanticFact result_fact(SemanticFactKind::NewExpression,
		make_pointer(allocated_type), SemanticValueCategory::Prvalue, &node);
	const SemanticFactId result = make_semantic_fact(result_fact);
	std::vector<SemanticFactId> result_children;
	result_children.push_back(allocation_id);
	result_children.push_back(constructor.fact);
	set_semantic_children(result, result_children);
	return ExprInfo(result, result_fact.type, SemanticValueCategory::Prvalue, false);
}

} // namespace pa11_semantic_internal
