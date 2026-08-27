#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

bool PA11SemanticModel::has_template_id(const PA10AstNode& node) const
{
	for (std::size_t i = 0; i < node.name_parts.size(); ++i)
		if (node.name_parts[i].has_template_id)
			return true;
	return false;
}
NamePath PA11SemanticModel::template_name_path(const PA10AstNode& node)
{
	if (node.name_prefix_count != 0)
		throw std::runtime_error("template-id decltype qualifier unsupported");
	NamePath result;
	result.global = node.global_name;
	for (std::size_t i = 0; i < node.name_parts.size(); ++i)
		result.components.push_back(name_from_spelling(node.name_parts[i].spelling));
	if (result.components.empty() && node.producer_spelling != 0)
		result.components.push_back(name_from_spelling(node.producer_spelling));
	if (result.components.empty())
		throw std::runtime_error("template-id has no semantic component");
	return result;
}
const TemplateFunctionList* PA11SemanticModel::template_functions(
	const NamePath& path, ScopeId scope) const
{
	(void)scope;
	if (path.components.size() != 1)
		return NULL;
	return template_function_index_.find(path.last());
}
bool PA11SemanticModel::template_argument_types(const PA10AstNode& node,
	ScopeId scope, std::vector<TypeId>* arguments)
{
	if (arguments == NULL || node.name_parts.empty())
		return false;
	const PA10NameComponent* component = NULL;
	for (std::size_t i = 0; i < node.name_parts.size(); ++i)
	{
		if (!node.name_parts[i].has_template_id)
			continue;
		if (component != NULL || i + 1 != node.name_parts.size())
			return false;
		component = &node.name_parts[i];
	}
	if (component == NULL || component->template_argument_begin >
		ast_.template_arguments.size() || component->template_argument_count >
		ast_.template_arguments.size() - component->template_argument_begin)
		return false;
	arguments->clear();
	for (std::size_t i = 0; i < component->template_argument_count; ++i)
	{
		const PA10TemplateArgument& argument = ast_.template_arguments[
			component->template_argument_begin + i];
		TypeId type;
		if (argument.kind == PA10TemplateArgumentKind::TypeId)
			type = type_from_type_id(argument.syntax, scope);
		else if (argument.syntax.kind == PA10NodeKind::IdExpression &&
			!has_template_id(argument.syntax))
			type = lookup_type_path(name_path(argument.syntax), scope);
		else
			return false;
		if (!type.valid())
			return false;
		arguments->push_back(type);
	}
	return true;
}
TypeId PA11SemanticModel::substitute_template_type(TypeId type,
	const TemplateFunctionFact& function, const std::vector<TypeId>& arguments)
{
	if (!type.valid() || type.value >= types_.size())
		return TypeId();
	const TypeKey& key = types_[type.value];
	if (key.kind == TypeKind::Named)
	{
		for (std::size_t i = 0; i < function.parameters.size(); ++i)
			if (key.named == function.parameters[i])
				return i < arguments.size() ? arguments[i] : TypeId();
		return type;
	}
	if (key.kind == TypeKind::Function)
	{
		std::vector<TypeId> parameters;
		parameters.reserve(key.parameters.size());
		for (std::size_t i = 0; i < key.parameters.size(); ++i)
			parameters.push_back(substitute_template_type(key.parameters[i],
				function, arguments));
		return make_function(parameters, key.variadic,
			substitute_template_type(key.result, function, arguments), key.cv);
	}
	if (key.kind == TypeKind::Fundamental)
		return type;
	return TypeId();
}
bool PA11SemanticModel::deduce_template_type(TypeId pattern, TypeId actual,
	const TemplateFunctionFact& function, std::vector<TypeId>* arguments) const
{
	if (arguments == NULL || !pattern.valid() || !actual.valid() ||
		pattern.value >= types_.size() || actual.value >= types_.size())
		return false;
	const TypeKey& pattern_key = types_[pattern.value];
	if (pattern_key.kind == TypeKind::Named)
	{
		for (std::size_t i = 0; i < function.parameters.size(); ++i)
		{
			if (pattern_key.named != function.parameters[i])
				continue;
			if (i >= arguments->size())
				return false;
			if ((*arguments)[i].valid())
				return (*arguments)[i] == actual;
			(*arguments)[i] = actual;
			return true;
		}
	}
	return pattern == actual;
}
TemplateSpecializationId PA11SemanticModel::specialize_template_function(
	TemplateFunctionId function_id, const std::vector<TypeId>& arguments)
{
	if (!function_id.valid() || function_id.value >= template_function_facts_.size())
		return TemplateSpecializationId();
	const TemplateFunctionFact& function = template_function_facts_[
		function_id.value];
	const TemplateSpecializationKey key(function_id, arguments);
	TemplateSpecializationId specialization;
	TemplateSpecializationId* indexed = template_specialization_index_.find(key);
	if (indexed != NULL)
	{
		specialization = *indexed;
		if (!specialization.valid() || specialization.value >=
			template_specialization_facts_.size())
			return TemplateSpecializationId();
		TemplateSpecializationFact& existing =
			template_specialization_facts_[specialization.value];
		if (existing.state == TemplateSpecializationState::Complete)
			return specialization;
		if (existing.state == TemplateSpecializationState::InProgress ||
			existing.state == TemplateSpecializationState::Failed)
			return TemplateSpecializationId();
	}
	else
	{
		specialization = TemplateSpecializationId(
			template_specialization_facts_.size());
		TemplateSpecializationFact fact(function_id, BindingId());
		fact.arguments = arguments;
		template_specialization_facts_.push_back(fact);
		template_specialization_index_.set(key, specialization);
	}
	TemplateSpecializationFact& fact =
		template_specialization_facts_[specialization.value];
	if (fact.state != TemplateSpecializationState::NotStarted)
		return TemplateSpecializationId();
	fact.state = TemplateSpecializationState::InProgress;
	if (arguments.size() != function.parameters.size())
	{
		fact.state = TemplateSpecializationState::Failed;
		return TemplateSpecializationId();
	}
	const Binding& source = binding(function.binding);
	const TypeId specialized_type = substitute_template_type(source.type,
		function, arguments);
	if (!specialized_type.valid())
	{
		fact.state = TemplateSpecializationState::Failed;
		return TemplateSpecializationId();
	}
	if (!function.binding.valid() || function.binding.value >= binding_owners_.size())
	{
		fact.state = TemplateSpecializationState::Failed;
		return TemplateSpecializationId();
	}
	const BindingId binding_id(bindings_.size());
	bindings_.push_back(Binding(BindingKind::Function, source.name,
		specialized_type));
	binding_owners_.push_back(binding_owners_[function.binding.value]);
	fact.binding = binding_id;
	BindingSidecar sidecar;
	sidecar.template_specialization = specialization;
	set_binding_sidecar(binding_id, sidecar);
	fact.state = TemplateSpecializationState::Complete;
	return specialization;
}
FunctionIdResolution PA11SemanticModel::resolve_template_function_id_target(
	const PA10AstNode& node, ScopeId scope, TypeId target)
{
	if (!has_template_id(node))
		return FunctionIdResolution();
	const NamePath path = template_name_path(node);
	const TemplateFunctionList* list = template_functions(path, scope);
	if (list == NULL)
		return FunctionIdResolution();
	std::vector<TypeId> arguments;
	if (!template_argument_types(node, scope, &arguments))
		return FunctionIdResolution();
	bool have_selected = false;
	bool ambiguous = false;
	TemplateFunctionId selected_function;
	std::vector<TypeId> selected_arguments;
	ConversionChoice selected_conversion;
	for (std::size_t i = 0; i < list->entries.size(); ++i)
	{
		const TemplateFunctionId id = list->entries[i];
		if (!id.valid() || id.value >= template_function_facts_.size())
			continue;
		const TemplateFunctionFact& function = template_function_facts_[id.value];
		ScopeId cursor = scope;
		bool visible = false;
		while (cursor.valid())
		{
			if (cursor == function.visible_scope)
			{
				visible = true;
				break;
			}
			if (cursor.value >= scopes_.size())
				break;
			cursor = scopes_[cursor.value].parent;
		}
		if (!visible || arguments.size() != function.parameters.size())
			continue;
		const TypeId specialized_type = substitute_template_type(
			binding(function.binding).type, function, arguments);
		if (!specialized_type.valid())
			continue;
		const ConversionChoice conversion = conversion_for(specialized_type,
			SemanticValueCategory::Lvalue, target, &node);
		if (!conversion.valid)
			continue;
		if (!have_selected || conversion.rank < selected_conversion.rank)
		{
			have_selected = true;
			ambiguous = false;
			selected_function = id;
			selected_arguments = arguments;
			selected_conversion = conversion;
		}
		else if (conversion.rank == selected_conversion.rank)
			ambiguous = true;
	}
	if (!have_selected || ambiguous)
		return FunctionIdResolution();
	const TemplateSpecializationId specialization = specialize_template_function(
		selected_function, selected_arguments);
	if (!specialization.valid() || specialization.value >=
		template_specialization_facts_.size() ||
		template_specialization_facts_[specialization.value].state !=
			TemplateSpecializationState::Complete)
		return FunctionIdResolution();
	const TemplateSpecializationFact& fact =
		template_specialization_facts_[specialization.value];
	return FunctionIdResolution(true,
		ValueRef(template_function_facts_[selected_function.value].visible_scope,
			fact.binding), selected_conversion);
}
ExprInfo PA11SemanticModel::semantic_template_call(
	const PA10AstNode& node, ScopeId scope,
	const TemplateFunctionList& candidates,
	const PA10AstNode& argument_node)
{
	if (argument_node.kind != PA10NodeKind::ArgumentList &&
		argument_node.kind != PA10NodeKind::ParenArgumentList)
		throw std::runtime_error("PA12 invalid template argument list");
	std::vector<ExprInfo> arguments;
	for (std::size_t i = 0; i < argument_node.children.size(); ++i)
		arguments.push_back(semantic_expression(argument_node.children[i], scope));
	struct Candidate
	{
		TemplateFunctionId function;
		std::vector<TypeId> arguments;
		TypeId type;
		std::vector<unsigned int> ranks;
		Candidate() : function(), arguments(), type(), ranks() {}
	};
	std::vector<Candidate> viable;
	for (std::size_t i = 0; i < candidates.entries.size(); ++i)
	{
		const TemplateFunctionId id = candidates.entries[i];
		if (!id.valid() || id.value >= template_function_facts_.size())
			continue;
		const TemplateFunctionFact& function = template_function_facts_[id.value];
		ScopeId cursor = scope;
		bool visible = false;
		while (cursor.valid())
		{
			if (cursor == function.visible_scope)
			{
				visible = true;
				break;
			}
			if (cursor.value >= scopes_.size())
				break;
			cursor = scopes_[cursor.value].parent;
		}
		if (!visible || function.binding.value >= bindings_.size())
			continue;
		const TypeId source_type = binding(function.binding).type;
		if (type_kind(source_type) != TypeKind::Function)
			continue;
		const TypeKey& source_function = types_[source_type.value];
		if (arguments.size() != source_function.parameters.size())
			continue;
		std::vector<TypeId> template_arguments(function.parameters.size());
		bool deduced = true;
		for (std::size_t arg = 0; arg < source_function.parameters.size(); ++arg)
		{
			if (!deduce_template_type(source_function.parameters[arg],
				expression_object_type(arguments[arg].type), function,
				&template_arguments))
			{
				deduced = false;
				break;
			}
		}
		for (std::size_t arg = 0; deduced && arg < template_arguments.size(); ++arg)
			if (!template_arguments[arg].valid())
				deduced = false;
		if (!deduced)
			continue;
		const TypeId specialized_type = substitute_template_type(source_type,
			function, template_arguments);
		if (!specialized_type.valid() || type_kind(specialized_type) !=
			TypeKind::Function)
			continue;
		const TypeKey& specialized_function = types_[specialized_type.value];
		Candidate candidate;
		candidate.function = id;
		candidate.arguments = template_arguments;
		candidate.type = specialized_type;
		candidate.ranks.reserve(arguments.size());
		for (std::size_t arg = 0; arg < arguments.size(); ++arg)
		{
			const ConversionChoice conversion = conversion_for(arguments[arg].type,
				arguments[arg].category, specialized_function.parameters[arg],
				semantic_facts_[arguments[arg].fact.value].source,
				arguments[arg].integer_zero);
			if (!conversion.valid)
			{
				candidate.ranks.clear();
				break;
			}
			candidate.ranks.push_back(conversion.rank);
		}
		if (candidate.ranks.size() == arguments.size())
			viable.push_back(candidate);
	}
	if (viable.empty())
		throw std::runtime_error("PA12 no viable function template");
	const auto better = [](const Candidate& left, const Candidate& right) -> bool
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
	std::size_t best = 0;
	for (std::size_t i = 1; i < viable.size(); ++i)
		if (better(viable[i], viable[best]))
			best = i;
	for (std::size_t i = 0; i < viable.size(); ++i)
		if (i != best && !better(viable[best], viable[i]))
			throw std::runtime_error("PA12 ambiguous function template call");
	const Candidate& selected = viable[best];
	const TemplateSpecializationId specialization = specialize_template_function(
		selected.function, selected.arguments);
	if (!specialization.valid() || specialization.value >=
		template_specialization_facts_.size() ||
		template_specialization_facts_[specialization.value].state !=
			TemplateSpecializationState::Complete)
		throw std::runtime_error("PA12 template specialization failed");
	const TemplateSpecializationFact& specialization_fact =
		template_specialization_facts_[specialization.value];
	const TypeKey& selected_function = types_[selected.type.value];
	for (std::size_t i = 0; i < selected_function.parameters.size(); ++i)
		arguments[i] = apply_context_conversion(arguments[i],
			selected_function.parameters[i],
			semantic_facts_[arguments[i].fact.value].source);
	std::vector<SemanticFactId> children;
	for (std::size_t i = 0; i < arguments.size(); ++i)
		children.push_back(arguments[i].fact);
	SemanticFact call(SemanticFactKind::CallExpression,
		function_result_type(selected.type), SemanticValueCategory::Prvalue,
		&node);
	call.has_callee = true;
	call.selected_binding = specialization_fact.binding;
	call.selected_scope = template_function_facts_[selected.function.value].visible_scope;
	const SemanticFactId call_id = make_semantic_fact(call);
	set_semantic_children(call_id, children);
	return ExprInfo(call_id, function_result_type(selected.type),
		SemanticValueCategory::Prvalue, false);
}

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
	if (has_template_id(node))
		return &node;
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
	if (has_template_id(node))
		return resolve_template_function_id_target(node, scope, target);
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
	const TypeId unqualified = strip_cv_type(target);
	const TypeKind direct_kind = unqualified.valid() ? type_kind(unqualified) :
		TypeKind::Fundamental;
	if (direct_kind == TypeKind::LvalueReference ||
		direct_kind == TypeKind::RvalueReference)
		return true;
	const TypeId object = strip_cv_type(expression_object_type(target));
	const TypeKind kind = object.valid() ? type_kind(object) : TypeKind::Fundamental;
	if (kind == TypeKind::Named)
	{
		const NamedRecordId record = named_record_for_type(object);
		if (record.valid() && record.value < named_.size() &&
			named_[record.value].kind == NamedKind::Class)
			return true;
	}
	return void_id(target) || scalar_id(target) || enumeration_id(target) ||
		kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference;
}
bool PA11SemanticModel::cv_cast_compatible(TypeId source, TypeId target) const
{
	return cv_cast_compatible_impl(source, target);
}
bool PA11SemanticModel::cv_cast_compatible_impl(TypeId source,
	TypeId target) const
{
	while (source.valid() && type_kind(source) == TypeKind::Cv)
		source = types_[source.value].child;
	while (target.valid() && type_kind(target) == TypeKind::Cv)
		target = types_[target.value].child;
	if (!source.valid() || !target.valid() || type_kind(source) != type_kind(target))
		return false;
	const TypeKind kind = type_kind(source);
	if (kind == TypeKind::LvalueReference ||
		kind == TypeKind::RvalueReference)
		// Reference identity is part of a function signature.  This helper
		// strips cv wrappers only; unlike expression_object_type it never
		// erases a nested reference while walking that signature.
		return cv_cast_compatible_impl(types_[source.value].child,
			types_[target.value].child);
	if (kind == TypeKind::Pointer)
		return cv_cast_compatible_impl(types_[source.value].child,
			types_[target.value].child);
	if (kind == TypeKind::MemberPointer)
		return types_[source.value].named == types_[target.value].named &&
			cv_cast_compatible_impl(types_[source.value].child,
				types_[target.value].child);
	if (kind == TypeKind::Array)
		return types_[source.value].unknown_bound ==
			types_[target.value].unknown_bound &&
			(types_[source.value].unknown_bound ||
				types_[source.value].bound == types_[target.value].bound) &&
			cv_cast_compatible_impl(types_[source.value].child,
				types_[target.value].child);
	if (kind == TypeKind::Function)
	{
		const TypeKey& left = types_[source.value];
		const TypeKey& right = types_[target.value];
		if (left.variadic != right.variadic ||
			left.parameters.size() != right.parameters.size() ||
			!cv_cast_compatible_impl(left.result, right.result))
			return false;
		for (std::size_t i = 0; i < left.parameters.size(); ++i)
			if (!cv_cast_compatible_impl(left.parameters[i], right.parameters[i]))
				return false;
		return true;
	}
	return source == target;
}
bool PA11SemanticModel::reinterpret_reference_compatible(TypeId source,
	TypeId target) const
{
	if (cv_cast_compatible(source, target))
		return true;
	const TypeId source_object = strip_cv_type(source);
	const TypeId target_object = strip_cv_type(target);
	const bool source_supported = integral_id(source_object) ||
		floating_id(source_object) || enumeration_id(source_object) ||
		pointer_id(source_object);
	const bool target_supported = integral_id(target_object) ||
		floating_id(target_object) || enumeration_id(target_object) ||
		pointer_id(target_object);
	// The PA15 reinterpret-reference subset is defined by the typed scalar
	// owner, not by an incidental equal-size fixture.  Reference formation is
	// accepted for the supported scalar object kinds; unsupported class,
	// array, function, and member-pointer objects remain rejected here.
	return source_supported && target_supported;
}
ExplicitCastKind PA11SemanticModel::explicit_cast_kind(
	const PA10AstNode& node) const
{
	if (node.kind == PA10NodeKind::CallExpression)
		return ExplicitCastKind::Functional;
	switch (node.token)
	{
	case SimpleTokenType::OP_LPAREN: return ExplicitCastKind::CStyle;
	case SimpleTokenType::KW_STATIC_CAST: return ExplicitCastKind::Static;
	case SimpleTokenType::KW_CONST_CAST: return ExplicitCastKind::Const;
	case SimpleTokenType::KW_REINTERPET_CAST:
		return ExplicitCastKind::Reinterpret;
	default: return ExplicitCastKind::None;
	}
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
	set_semantic_name(result, has_template_id(node) ? template_name_path(node) :
		name_path(node));
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

ExprInfo PA11SemanticModel::semantic_default_member_initializer(
	const PA10AstNode& node, ScopeId scope, TypeId target)
{
	const TypeId object = strip_top_cv_type(target);
	const NamedRecordId record = named_record_for_type(object);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Class)
	{
		if (node.kind == PA10NodeKind::BracedInitList && !node.children.empty())
			return semantic_braced_init_list(node, target, scope);
		if (node.kind == PA10NodeKind::BracedInitList ||
			node.kind == PA10NodeKind::CallExpression)
		{
			const PA10AstNode* callee = NULL;
			if (node.kind == PA10NodeKind::CallExpression)
			{
				if (node.children.size() != 2)
					throw std::runtime_error(
						"PA12 class member initializer call is invalid");
				callee = &node.children.front();
				const PA10AstNode& arguments = node.children.back();
				if (arguments.kind != PA10NodeKind::ArgumentList &&
					arguments.kind != PA10NodeKind::ParenArgumentList)
					throw std::runtime_error(
						"PA12 class member initializer arguments are invalid");
				if (!arguments.children.empty())
					throw std::runtime_error(
						"PA12 class member initializer arguments are outside checkpoint");
			}
			if (node.kind == PA10NodeKind::BracedInitList &&
				!node.children.empty())
				throw std::runtime_error(
					"PA12 class member initializer is not an aggregate");
			if (callee != NULL)
			{
				const TypeId callee_type = lookup_type_path(name_path(*callee),
					scope);
				if (!callee_type.valid() || strip_cv_type(callee_type) != object)
					throw std::runtime_error(
						"PA12 class member initializer names another type");
			}
			BindingId constructor = default_constructor_binding(record);
			if (!constructor.valid())
				constructor = ensure_implicit_default_constructor(record);
			if (function_declaration_kind(constructor) ==
				FunctionDeclarationKind::Deleted)
				throw std::runtime_error(
					"PA12 class member initializer selects deleted constructor");
			SemanticFact fact(SemanticFactKind::ConstructorAction, target,
				SemanticValueCategory::Lvalue, &node);
			fact.has_callee = true;
			fact.value_initialize = true;
			fact.selected_binding = constructor;
			fact.selected_scope = named_[record.value].scope;
			fact.callable_type = constructor_callable_type(constructor);
			const SemanticFactId result = make_semantic_fact(fact);
			set_semantic_children(result, std::vector<SemanticFactId>());
			return ExprInfo(result, target, SemanticValueCategory::Lvalue, false);
		}
	}
	return semantic_expression_for_target(node, scope, target);
}

SemanticFactId PA11SemanticModel::semantic_return_statement(
	const PA10AstNode& node, ScopeId scope, const FunctionFact& function)
{
	const Binding& function_binding = binding(function.binding);
	const TypeKey& function_type = types_[function_binding.type.value];
	if (type_kind(function_binding.type) != TypeKind::Function)
		throw std::runtime_error("PA12 function fact has non-function type");
	const TypeId result_type = function_type.result;
	std::vector<SemanticFactId> children;
	if (!node.children.empty())
	{
		if (void_id(result_type))
		{
			const ExprInfo expression = semantic_expression(
				node.children.front(), scope);
			if (!void_id(expression.type))
				throw std::runtime_error("PA12 non-void expression returned from void function");
			children.push_back(expression.fact);
		}
		else
		{
			const bool braced = node.children.front().kind ==
				PA10NodeKind::BracedInitList;
			const ExprInfo expression = semantic_expression_for_target(
				node.children.front(), scope, result_type);
			const BindingSidecar* function_sidecar = binding_sidecar(function.binding);
			const bool operator_bool_return = !braced && bool_id(result_type) &&
				expression.category == SemanticValueCategory::Prvalue &&
				expression.type == result_type && function_sidecar != NULL &&
				function_sidecar->operator_function_kind !=
					PA10OperatorFunctionKind::None;
			const bool friend_bool_return = !braced && bool_id(result_type) &&
				expression.category == SemanticValueCategory::Prvalue &&
				expression.type == result_type && function_sidecar != NULL &&
				!function_sidecar->friend_records.empty();
			if (!braced && !operator_bool_return && !friend_bool_return &&
				bool_id(result_type) && expression.category ==
					SemanticValueCategory::Prvalue && expression.type == result_type)
			{
				set_fact_conversion(expression.fact, add_conversion(
					expression.type, result_type, ConversionKind::Identity, 0));
			}
			else if (!operator_bool_return && !friend_bool_return && !braced)
				apply_context_conversion(expression, result_type,
					semantic_facts_[expression.fact.value].source);
			children.push_back(expression.fact);
		}
	}
	else if (!void_id(result_type))
		throw std::runtime_error("PA12 missing return value");
	return make_expression_fact(SemanticFactKind::ReturnStatement,
		TypeId(), SemanticValueCategory::Prvalue, node, children);
}
ExprInfo PA11SemanticModel::semantic_cast_to_target(
	const PA10AstNode& node, TypeId target, const ExprInfo& operand)
{
	const TypeId source = expression_object_type(operand.type);
	const TypeKind target_kind = type_kind(target);
	const ExplicitCastKind cast_kind = explicit_cast_kind(node);
	if (target_kind == TypeKind::LvalueReference ||
		target_kind == TypeKind::RvalueReference)
	{
		const TypeId referred = types_[target.value].child;
		bool valid = qualification_convertible(source, referred);
		if (cast_kind == ExplicitCastKind::Const ||
			cast_kind == ExplicitCastKind::Functional)
			valid = cv_cast_compatible(source, referred);
		else if (cast_kind == ExplicitCastKind::Reinterpret)
			valid = reinterpret_reference_compatible(source, referred);
		else if (cast_kind == ExplicitCastKind::Static &&
			cv_cast_compatible(source, referred))
			valid = true;
		if (!valid)
			throw std::runtime_error("PA12 invalid reference cast");
		if (target_kind == TypeKind::LvalueReference &&
			operand.category != SemanticValueCategory::Lvalue)
			throw std::runtime_error("PA12 invalid reference cast category");
		const SemanticValueCategory category = target_kind ==
			TypeKind::RvalueReference ? SemanticValueCategory::Xvalue :
			SemanticValueCategory::Lvalue;
		// A cv-compatible static reference cast preserves the historical PA12
		// identity fact.  This keeps reference result ownership stable for
		// earlier semantic dumps while the non-identity reference families
		// retain an explicit typed cast boundary for PA15 lowering.
		if (cast_kind == ExplicitCastKind::Static &&
			cv_cast_compatible(source, referred))
		{
			semantic_facts_[operand.fact.value].type = target;
			semantic_facts_[operand.fact.value].category = category;
			return ExprInfo(operand.fact, target, category, false);
		}
		SemanticFact fact(SemanticFactKind::CastExpression, target,
			category, &node);
		fact.token = node.token;
		const SemanticFactId result = make_semantic_fact(fact);
		set_semantic_children(result,
			std::vector<SemanticFactId>(1, operand.fact));
		return ExprInfo(result, target, category, false);
	}
	bool valid = false;
	ConversionKind kind = ConversionKind::Integral;
	if (cast_kind == ExplicitCastKind::Const)
	{
		valid = pointer_id(source) && pointer_id(target) &&
			cv_cast_compatible(source, target);
		kind = ConversionKind::PointerQualification;
	}
	else if (cast_kind == ExplicitCastKind::Reinterpret)
	{
		const bool source_pointer = pointer_id(source);
		const bool target_pointer = pointer_id(target);
		valid = (target_pointer && (source_pointer || integral_id(source) ||
			enumeration_id(source) || nullptr_id(source))) ||
			(integral_id(target) && source_pointer);
		kind = ConversionKind::Reinterpret;
	}
	else if (void_id(target))
	{
		valid = void_id(source) || scalar_id(source) ||
			type_kind(source) == TypeKind::Function;
		kind = ConversionKind::ToVoid;
	}
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
	else if (pointer_id(target) && cast_kind == ExplicitCastKind::CStyle &&
		(integral_id(source) || enumeration_id(source) || nullptr_id(source)))
	{
		// The supported C-style scalar-to-pointer form is represented at
		// PA12 as the same typed reinterpret boundary used by reinterpret_cast.
		valid = true;
		kind = ConversionKind::Reinterpret;
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
		const TypeId object = strip_cv_type(expression_object_type(target));
		const NamedRecordId record = named_record_for_type(object);
		if (record.valid() && record.value < named_.size() &&
			named_[record.value].kind == NamedKind::Class)
		{
			const std::vector<const PA10AstNode*> no_arguments;
			const ConstructorSelection selection = select_constructor(record, scope,
				no_arguments, true, ConstructorInitializationContext::Direct);
			if (!selection.valid())
				throw std::runtime_error("PA12 functional constructor selection is incomplete");
			SemanticFact call(SemanticFactKind::CallExpression,
				fundamental(FundamentalType::Void), SemanticValueCategory::Prvalue,
				&node);
			call.has_callee = true;
			call.temporary_object = true;
			call.selected_binding = selection.binding;
			call.selected_scope = selection.scope;
			call.callable_type = selection.callable_type;
			const SemanticFactId call_id = make_semantic_fact(call);
			set_semantic_children(call_id, selection.arguments);
			SemanticFact temporary(SemanticFactKind::ConstructorAction, target,
				SemanticValueCategory::Prvalue, &node);
			temporary.has_callee = true;
			temporary.temporary_object = true;
			temporary.selected_binding = selection.binding;
			temporary.selected_scope = selection.scope;
			temporary.callable_type = selection.callable_type;
			const SemanticFactId result = make_semantic_fact(temporary);
			set_semantic_children(result,
				std::vector<SemanticFactId>(1, call_id));
			return ExprInfo(result, target, SemanticValueCategory::Prvalue, false);
		}
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
	const TypeId object = strip_cv_type(expression_object_type(target));
	const NamedRecordId record = named_record_for_type(object);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Class)
	{
		std::vector<const PA10AstNode*> arguments(1,
			&argument_node.children.front());
		const ConstructorSelection selection = select_constructor(record, scope,
			arguments, true, ConstructorInitializationContext::Direct);
		if (!selection.valid())
			throw std::runtime_error("PA12 functional constructor selection is incomplete");
		SemanticFact call(SemanticFactKind::CallExpression,
			fundamental(FundamentalType::Void), SemanticValueCategory::Prvalue,
			&node);
		call.has_callee = true;
		call.temporary_object = true;
		call.selected_binding = selection.binding;
		call.selected_scope = selection.scope;
		call.callable_type = selection.callable_type;
		const SemanticFactId call_id = make_semantic_fact(call);
		set_semantic_children(call_id, selection.arguments);
		SemanticFact temporary(SemanticFactKind::ConstructorAction, target,
			SemanticValueCategory::Prvalue, &node);
		temporary.has_callee = true;
		temporary.temporary_object = true;
		temporary.selected_binding = selection.binding;
		temporary.selected_scope = selection.scope;
		temporary.callable_type = selection.callable_type;
		const SemanticFactId result = make_semantic_fact(temporary);
		set_semantic_children(result,
			std::vector<SemanticFactId>(1, call_id));
		return ExprInfo(result, target, SemanticValueCategory::Prvalue, false);
	}
	const ExprInfo operand = semantic_expression(
		argument_node.children.front(), scope);
	return semantic_cast_to_target(node, target, operand);
}
} // namespace pa11_semantic_internal
