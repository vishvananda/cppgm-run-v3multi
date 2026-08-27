#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

TypeId PA11SemanticModel::member_access_type(TypeId object, TypeId member)
{
	const unsigned int qualifiers = cv_qualifiers(expression_object_type(object));
	return qualifiers == 0 ? member : make_cv(member, qualifiers);
}
BindingId PA11SemanticModel::member_binding(TypeId object, NameId name) const
{
	const TypeId record_type = strip_cv_type(expression_object_type(object));
	if (type_kind(record_type) != TypeKind::Named)
		return BindingId();
	const ScopeId scope = class_scope_for_type(record_type);
	if (!scope.valid())
		return BindingId();
	const ValueList* values = scopes_[scope.value].values.find(name);
	if (values == NULL || values->entries.size() != 1)
		return BindingId();
	return values->entries.front().binding;
}
std::vector<ValueRef> PA11SemanticModel::member_function_candidates(
	TypeId object, NameId name) const
{
	std::vector<ValueRef> result;
	const TypeId record_type = strip_cv_type(expression_object_type(object));
	if (type_kind(record_type) != TypeKind::Named)
		return result;
	const ScopeId member_scope = class_scope_for_type(record_type);
	if (!member_scope.valid() || member_scope.value >= scopes_.size())
		return result;
	const ValueList* values = scopes_[member_scope.value].values.find(name);
	if (values == NULL)
		return result;
	for (std::size_t i = 0; i < values->entries.size(); ++i)
	{
		const BindingId candidate_id = values->entries[i].binding;
		const Binding& candidate = binding(candidate_id);
		if (candidate.kind == BindingKind::Function &&
			type_kind(candidate.type) == TypeKind::Function &&
			!is_static_member(candidate_id))
			result.push_back(ValueRef(member_scope, candidate_id));
	}
	return result;
}
bool PA11SemanticModel::member_accessible(BindingId binding_id,
	ScopeId member_scope, ScopeId access_scope) const
{
	if (member_access(binding_id) == MemberAccess::Public)
		return true;
	ScopeId cursor = access_scope;
	while (cursor.valid() && cursor.value < scopes_.size())
	{
		if (cursor == member_scope)
			return true;
		cursor = scopes_[cursor.value].parent;
	}
	return false;
}
BindingId PA11SemanticModel::implicit_this_binding(ScopeId scope) const
{
	ScopeId cursor = scope;
	while (cursor.valid() && cursor.value < scopes_.size())
	{
		const Scope& current = scopes_[cursor.value];
		if (current.kind == ScopeKind::Function)
			return current.implicit_object_binding;
		cursor = current.parent;
	}
	return BindingId();
}
ExprInfo PA11SemanticModel::semantic_this_expression(
	const PA10AstNode& node, ScopeId scope)
{
	const BindingId this_id = implicit_this_binding(scope);
	if (!this_id.valid())
		throw std::runtime_error("PA12 this is outside a non-static member function");
	const Binding& this_binding = binding(this_id);
	if (this_binding.kind != BindingKind::Parameter ||
		type_kind(this_binding.type) != TypeKind::Pointer)
		throw std::runtime_error("PA12 implicit this binding is invalid");
	SemanticFact fact(SemanticFactKind::IdExpression, this_binding.type,
		SemanticValueCategory::Prvalue, &node);
	fact.binding = this_id;
	const SemanticFactId result = make_semantic_fact(fact);
	return ExprInfo(result, this_binding.type, SemanticValueCategory::Prvalue,
		false);
}

ExprInfo PA11SemanticModel::semantic_member_expression(
	const PA10AstNode& node, ScopeId scope)
{
	if (node.kind != PA10NodeKind::MemberExpression ||
		node.children.size() != 2 || !node.has_token ||
		(node.token != SimpleTokenType::OP_DOT &&
			node.token != SimpleTokenType::OP_ARROW) ||
		node.children[1].kind != PA10NodeKind::Identifier)
		throw std::runtime_error("PA12 invalid member expression");
	const ExprInfo object = semantic_expression(node.children.front(), scope);
	const NamePath member_name = name_path(node.children.back());
	if (member_name.global || member_name.components.size() != 1)
		throw std::runtime_error("PA12 qualified member is unsupported");
	TypeId record_object = object.type;
	if (node.token == SimpleTokenType::OP_ARROW)
	{
		const TypeId pointer = strip_cv_type(expression_object_type(object.type));
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 arrow operand is not a pointer");
		record_object = types_[pointer.value].child;
		const TypeId pointer_value = strip_top_cv_type(object.type);
		record_builtin_conversion(object, pointer_value);
	}
	else if (type_kind(strip_cv_type(expression_object_type(record_object))) !=
		TypeKind::Named)
		throw std::runtime_error("PA12 dot operand is not a record");
	const BindingId member_id = member_binding(record_object,
		member_name.last());
	if (!member_id.valid())
		throw std::runtime_error("PA12 unknown record member");
	const Binding& member = binding(member_id);
	if (member.kind != BindingKind::Variable)
		throw std::runtime_error("PA12 member function access is unsupported");
	const TypeId type = member_access_type(record_object, member.type);
	SemanticFact fact(SemanticFactKind::MemberExpression, type,
		SemanticValueCategory::Lvalue, &node);
	fact.token = node.token;
	fact.binding = member_id;
	fact.selected_binding = member_id;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, member_name);
	set_semantic_children(result,
		std::vector<SemanticFactId>(1, object.fact));
	return ExprInfo(result, type, SemanticValueCategory::Lvalue, false);
}

ExprInfo PA11SemanticModel::semantic_member_call_expression(
	const PA10AstNode& node, const PA10AstNode& member_node, ScopeId scope)
{
	if (member_node.kind != PA10NodeKind::MemberExpression ||
		member_node.children.size() != 2 || !member_node.has_token ||
		(member_node.token != SimpleTokenType::OP_DOT &&
			member_node.token != SimpleTokenType::OP_ARROW) ||
		member_node.children[1].kind != PA10NodeKind::Identifier)
		throw std::runtime_error("PA12 invalid member call expression");
	const NamePath member_name = name_path(member_node.children.back());
	if (member_name.global || member_name.components.size() != 1)
		throw std::runtime_error("PA12 qualified member call is unsupported");
	const ExprInfo object = semantic_expression(member_node.children.front(),
		scope);
	TypeId record_object = object.type;
	TypeId actual_object = object.type;
	if (member_node.token == SimpleTokenType::OP_ARROW)
	{
		const TypeId pointer = strip_cv_type(expression_object_type(object.type));
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 arrow operand is not a pointer");
		record_object = types_[pointer.value].child;
		actual_object = record_object;
	}
	else
	{
		record_object = strip_cv_type(expression_object_type(record_object));
		actual_object = expression_object_type(object.type);
		if (type_kind(record_object) != TypeKind::Named)
			return ExprInfo();
	}
	const ScopeId member_scope = class_scope_for_type(record_object);
	if (!member_scope.valid())
		return ExprInfo();
	const std::vector<ValueRef> candidates = member_function_candidates(
		record_object, member_name.last());
	if (candidates.empty())
		return ExprInfo();
	if (member_node.token == SimpleTokenType::OP_DOT &&
		object.category != SemanticValueCategory::Lvalue)
		throw std::runtime_error("PA12 member call needs an lvalue object");
	if (member_node.token == SimpleTokenType::OP_ARROW)
	{
		const TypeId pointer_value = strip_top_cv_type(object.type);
		record_builtin_conversion(object, pointer_value);
	}
	const TypeId actual_record = strip_cv_type(actual_object);
	if (class_scope_for_type(actual_record) != member_scope)
		throw std::runtime_error("PA12 member call object owner mismatch");

	const PA10AstNode& argument_node = node.children.back();
	std::vector<ExprInfo> arguments;
	for (std::size_t i = 0; i < argument_node.children.size(); ++i)
	{
		if (target_function_id(argument_node.children[i], scope) != NULL)
			arguments.push_back(ExprInfo());
		else
			arguments.push_back(semantic_expression(argument_node.children[i], scope));
	}
	struct CandidateScore
	{
		ValueRef value;
		TypeId type;
		std::vector<unsigned int> ranks;
	};
	std::vector<CandidateScore> viable;
	const unsigned int ellipsis_rank = std::numeric_limits<unsigned int>::max() / 4;
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const Binding& candidate = binding(candidates[i].binding);
		const TypeKey& function = types_[candidate.type.value];
		const TypeId required_object = member_object_type(candidate.type,
			member_scope);
		if (!required_object.valid() ||
			!qualification_convertible(actual_object, required_object))
			continue;
		std::size_t required = function.parameters.size();
		while (required != 0 && function_default_argument(
			candidates[i].binding, required - 1).valid())
			--required;
		if (arguments.size() < required ||
			(!function.variadic && arguments.size() > function.parameters.size()))
			continue;
		CandidateScore score;
		score.value = candidates[i];
		score.type = candidate.type;
		score.ranks.reserve(arguments.size() + 1);
		// The implicit object is the first call argument for overload
		// ranking.  conversion_for intentionally ignores top-level cv for
		// ordinary by-value arguments, so rank this qualification explicitly.
		score.ranks.push_back(actual_object == required_object ? 0 : 1);
		bool arguments_viable = true;
		for (std::size_t arg = 0; arg < arguments.size(); ++arg)
		{
			if (arg >= function.parameters.size())
			{
				if (!arguments[arg].fact.valid())
				{
					arguments_viable = false;
					break;
				}
				score.ranks.push_back(ellipsis_rank);
				continue;
			}
			ConversionChoice choice;
			if (arguments[arg].fact.valid())
				choice = conversion_for(arguments[arg].type,
					arguments[arg].category, function.parameters[arg],
					semantic_facts_[arguments[arg].fact.value].source,
					arguments[arg].integer_zero);
			else
			{
				const PA10AstNode* function_id = target_function_id(
					argument_node.children[arg], scope);
				if (function_id != NULL)
				{
					const FunctionIdResolution resolution = resolve_function_id_target(
						*function_id, scope, function.parameters[arg]);
					choice = resolution.conversion;
				}
			}
			if (!choice.valid)
			{
				arguments_viable = false;
				break;
			}
			score.ranks.push_back(choice.rank);
		}
		if (arguments_viable)
			viable.push_back(score);
	}
	if (viable.empty())
		throw std::runtime_error("PA12 no viable member call");
	const auto better = [](const CandidateScore& left,
		const CandidateScore& right) -> bool
	{
		bool strict = false;
		for (std::size_t i = 0; i < left.ranks.size(); ++i)
		{
			if (left.ranks[i] > right.ranks[i])
				return false;
			if (left.ranks[i] < right.ranks[i])
				strict = true;
		}
		return strict;
	};
	std::size_t best_index = 0;
	for (std::size_t i = 1; i < viable.size(); ++i)
		if (better(viable[i], viable[best_index]))
			best_index = i;
	for (std::size_t i = 0; i < viable.size(); ++i)
		if (i != best_index && !better(viable[best_index], viable[i]))
			throw std::runtime_error("PA12 ambiguous member call");
	const ValueRef selected = viable[best_index].value;
	const TypeId selected_type = viable[best_index].type;
	if (!member_accessible(selected.binding, member_scope, scope))
		throw std::runtime_error("PA12 member call is inaccessible");
	if (function_declaration_kind(selected.binding) ==
		FunctionDeclarationKind::Deleted)
		throw std::runtime_error("PA12 member call selects deleted function");
	const TypeKey& function = types_[selected_type.value];
	const std::size_t explicit_count = arguments.size();
	for (std::size_t arg = explicit_count;
		arg < function.parameters.size(); ++arg)
	{
		const SemanticFactId default_fact = function_default_argument(
			selected.binding, arg);
		if (!default_fact.valid())
			throw std::runtime_error("PA12 selected member default is missing");
		const SemanticFact& value = semantic_facts_[default_fact.value];
		arguments.push_back(ExprInfo(default_fact, value.type, value.category,
			false));
	}
	const std::size_t fixed_explicit = explicit_count < function.parameters.size() ?
		explicit_count : function.parameters.size();
	for (std::size_t arg = 0; arg < fixed_explicit; ++arg)
	{
		if (!arguments[arg].fact.valid())
			arguments[arg] = semantic_expression_for_target(
				argument_node.children[arg], scope, function.parameters[arg]);
		arguments[arg] = apply_context_conversion(arguments[arg],
			function.parameters[arg],
			semantic_facts_[arguments[arg].fact.value].source);
	}
	apply_call_argument_conversions(arguments, selected_type, scope);
	const TypeId result_type = function_result_type(selected_type);
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	SemanticFact fact(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	fact.token = member_node.token;
	fact.has_callee = true;
	fact.has_implicit_object = true;
	fact.selected_binding = selected.binding;
	fact.selected_scope = selected.scope;
	fact.callable_type = member_function_expression_type(selected_type,
		member_scope, selected.binding);
	if (type_kind(fact.callable_type) != TypeKind::Function)
		throw std::runtime_error("PA12 member call has no hidden object signature");
	std::vector<SemanticFactId> children;
	children.push_back(object.fact);
	for (std::size_t i = 0; i < arguments.size(); ++i)
		children.push_back(arguments[i].fact);
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_children(result, children);
	return ExprInfo(result, result_type, result_category, false);
}

ExprInfo PA11SemanticModel::semantic_member_call_probe(
	const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 2)
		return ExprInfo();
	const PA10AstNode* member_node = &node.children.front();
	while (member_node->kind == PA10NodeKind::ParenthesizedExpression &&
		member_node->children.size() == 1)
		member_node = &member_node->children.front();
	if (member_node->kind != PA10NodeKind::MemberExpression)
		return ExprInfo();
	SemanticTailGuard member_tail(*this);
	const ExprInfo member_call = semantic_member_call_expression(
		node, *member_node, scope);
	if (!member_call.fact.valid())
		return ExprInfo();
	member_tail.commit();
	return member_call;
}

} // namespace pa11_semantic_internal
