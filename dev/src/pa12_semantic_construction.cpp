#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

TypeId PA11SemanticModel::named_type(NamedRecordId named) const
{
	TypeKey key;
	key.kind = TypeKind::Named;
	key.named = named;
	const TypeId* found = type_ids_.find(key);
	if (found == NULL)
		throw std::runtime_error("PA11 named type identity is missing");
	return *found;
}

void PA11SemanticModel::prepare_pa12_node(const PA10AstNode& node,
	ScopeId scope)
{
	switch (node.kind)
	{
	case PA10NodeKind::NamespaceDefinition:
	{
		const NamespaceFact* namespace_fact = this->namespace_fact(node);
		if (namespace_fact == NULL)
			throw std::runtime_error("PA12 namespace fact is missing");
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind != PA10NodeKind::InlineMarker)
				prepare_pa12_node(node.children[i], namespace_fact->scope);
		return;
	}
	case PA10NodeKind::LinkageSpecification:
		for (std::size_t i = 0; i < node.children.size(); ++i)
			prepare_pa12_node(node.children[i], scope);
		return;
	case PA10NodeKind::ClassSpecifier:
	{
		const NamePath name = class_name(node);
		if (name.empty())
			return;
		const ScopeId class_scope = class_scope_for_type(
			lookup_type_path(name, scope));
		if (!class_scope.valid())
			throw std::runtime_error("PA12 class semantic scope is missing");
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind == PA10NodeKind::FunctionDefinition ||
				node.children[i].kind == PA10NodeKind::ClassSpecifier)
				prepare_pa12_node(node.children[i], class_scope);
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind == PA10NodeKind::SpecialMemberDefinition)
			{
				for (std::size_t child = 0; child < node.children[i].children.size(); ++child)
					if (node.children[i].children[child].kind ==
						PA10NodeKind::CompoundStatement)
						prepare_pa12_compound(node.children[i].children[child],
							function_fact(node.children[i])->function_scope);
			}
		return;
	}
	case PA10NodeKind::FunctionDefinition:
	{
		const FunctionFact* function = function_fact(node);
		if (function == NULL || node.children.empty())
			throw std::runtime_error("PA12 function fact is missing");
		prepare_pa12_compound(node.children.back(), function->function_scope);
		return;
	}
	case PA10NodeKind::SpecialMemberDefinition:
	case PA10NodeKind::SpecialMemberDeclaration:
		return;
	default:
		return;
	}
}

void PA11SemanticModel::prepare_pa12_member_parameter(FunctionFact& function)
{
	if (!function.owner.valid() || function.owner.value >= scopes_.size() ||
		scopes_[function.owner.value].kind != ScopeKind::Class ||
		is_static_member(function.binding))
		return;
	if (!function.function_scope.valid() ||
		function.function_scope.value >= scopes_.size() ||
		scopes_[function.function_scope.value].kind != ScopeKind::Function)
		throw std::runtime_error("PA12 member function scope is invalid");
	const TypeId object_pointer = member_object_pointer_type(
		binding(function.binding).type, function.owner);
	if (!object_pointer.valid())
		throw std::runtime_error("PA12 member function has no object type");
	Scope& function_scope = scopes_[function.function_scope.value];
	if (function_scope.implicit_object_binding.valid())
	{
		const Binding& parameter = binding(function_scope.implicit_object_binding);
		if (parameter.kind != BindingKind::Parameter ||
			parameter.type != object_pointer)
			throw std::runtime_error("PA12 member function has invalid this binding");
		return;
	}
	const BindingId this_binding = store_binding(function.function_scope,
		Binding(BindingKind::Parameter, intern_name("this"), object_pointer), 0);
	function_scope.implicit_object_binding = this_binding;
}

bool PA11SemanticModel::semantic_class_object_initializer(
	BindingId storage, SemanticFactId variable, const PA10AstNode& source,
	const PA10AstNode* direct_operand, const PA10AstNode* clause,
	NamedRecordId record, ScopeId access_scope,
	ConstructorInitializationContext context)
{
	if (direct_operand != NULL)
	{
		std::vector<const PA10AstNode*> arguments(1, direct_operand);
		set_semantic_children(variable, std::vector<SemanticFactId>(1,
			semantic_constructor_action(storage, source, arguments, access_scope)));
		return true;
	}
	if (clause == NULL)
		return false;
	const bool aggregate = aggregate_class_initialization_supported(record);
	if (context != ConstructorInitializationContext::Direct &&
		has_constructor_declaration(record) && !aggregate)
	{
		std::vector<const PA10AstNode*> arguments;
		if (clause->kind == PA10NodeKind::BracedInitList)
		{
			arguments.reserve(clause->children.size());
			for (std::size_t i = 0; i < clause->children.size(); ++i)
				arguments.push_back(&clause->children[i]);
		}
		else
			arguments.push_back(clause);
		set_semantic_children(variable, std::vector<SemanticFactId>(1,
			semantic_constructor_action(storage, source, arguments, access_scope,
				context)));
		return true;
	}
	if (context == ConstructorInitializationContext::Direct &&
		(clause->kind == PA10NodeKind::BracedInitList ||
			clause->kind == PA10NodeKind::ParenInitializer) &&
		(clause->kind == PA10NodeKind::ParenInitializer ||
			(has_constructor_declaration(record) && !aggregate)))
	{
		std::vector<const PA10AstNode*> arguments;
		arguments.reserve(clause->children.size());
		for (std::size_t i = 0; i < clause->children.size(); ++i)
			arguments.push_back(&clause->children[i]);
		const SemanticFactId action = semantic_constructor_action(storage, source,
			arguments, access_scope);
		if (clause->kind == PA10NodeKind::ParenInitializer &&
			clause->children.empty())
			semantic_facts_[action.value].value_initialize = true;
		set_semantic_children(variable, std::vector<SemanticFactId>(1, action));
		return true;
	}
	return false;
}

void PA11SemanticModel::semantic_variable_initializer(
	BindingId storage, SemanticFactId variable, const PA10AstNode& source,
	const Binding& value, const DeclarationFact& declaration,
	NamedRecordId record, const PA10AstNode* direct_operand,
	const PA10AstNode* clause, ConstructorInitializationContext context,
	SemanticFactId* initializer_fact)
{
	if (initializer_fact == NULL)
		throw std::runtime_error("PA12 variable initializer has no result");
	*initializer_fact = SemanticFactId();
	const NamedRecordSidecar* record_sidecar = named_record_sidecar(record);
	const bool anonymous_union_object = record.valid() &&
		record.value < named_.size() && named_[record.value].kind == NamedKind::Class &&
		named_[record.value].class_tag == ClassTag::Union &&
		record_sidecar != NULL && record_sidecar->local_object_name;
	const bool local_object_scope = declaration.scope.valid() &&
		declaration.scope.value < scopes_.size() &&
		scopes_[declaration.scope.value].kind == ScopeKind::Block;
	const bool default_object = source.children.size() == 1 && local_object_scope &&
		record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Class;
	const bool legacy_empty_default_object = default_object &&
		!named_[record.value].has_base && named_[record.value].scope.valid() &&
		named_[record.value].scope.value < scopes_.size() &&
		scopes_[named_[record.value].scope.value].bindings.empty();
	const bool namespace_object_scope = value.kind == BindingKind::Variable &&
		value.has_definition && declaration.scope.valid() &&
		declaration.scope.value < scopes_.size() &&
		scopes_[declaration.scope.value].kind == ScopeKind::Namespace &&
		(record.valid() && record.value < named_.size());
	const bool static_member_definition = value.kind == BindingKind::Variable &&
		value.has_definition && is_static_member(storage) && declaration.scope.valid() &&
		declaration.scope.value < scopes_.size() &&
		scopes_[declaration.scope.value].kind == ScopeKind::Namespace &&
		record.valid() && record.value < named_.size();
	const bool nonautomatic_class_object = value.kind == BindingKind::Variable &&
		(record.valid() && record.value < named_.size()) &&
		(namespace_object_scope || static_member_definition);
	const TypeId declared_object = strip_top_cv_type(value.type);
	const bool named_class_object = declared_object.valid() &&
		type_kind(declared_object) == TypeKind::Named && record.valid() &&
		record.value < named_.size() && named_[record.value].kind == NamedKind::Class &&
		named_[record.value].class_tag != ClassTag::Union;
	const bool class_object_initializer = value.kind == BindingKind::Variable &&
		(local_object_scope || (nonautomatic_class_object && named_class_object)) &&
		named_class_object;
	const bool namespace_default_object = nonautomatic_class_object &&
		named_class_object && source.children.size() == 1;
	if ((default_object || namespace_default_object) && direct_operand == NULL &&
		!has_constructor_declaration(record))
		(void)implicit_default_constructor_supported(record);
	const bool constructor_initializer = class_object_initializer &&
		semantic_class_object_initializer(storage, variable, source, direct_operand,
			clause, record, declaration.scope, context);
	const bool special_function_initializer = value.kind == BindingKind::Function &&
		function_declaration_kind(storage) != FunctionDeclarationKind::Normal;
	if (!special_function_initializer && !constructor_initializer &&
		direct_operand != NULL)
	{
		const ExprInfo expression = semantic_expression_for_target(
			*direct_operand, declaration.scope, value.type);
		const ExprInfo converted = apply_context_conversion(expression, value.type,
			semantic_facts_[expression.fact.value].source, declaration.scope);
		if (converted.fact.valid() &&
			semantic_facts_[converted.fact.value].literal_element_count != 0)
			record_constant_address(converted.fact, declaration.scope);
		set_semantic_children(variable,
			std::vector<SemanticFactId>(1, converted.fact));
		*initializer_fact = converted.fact;
	}
	else if (!special_function_initializer && !constructor_initializer &&
		clause != NULL && source.children.size() > 1)
	{
		const PA10AstNode* expression_clause = clause;
		if (expression_clause->kind == PA10NodeKind::ParenInitializer)
		{
			if (expression_clause->children.size() != 1)
				throw std::runtime_error("PA12 invalid parenthesized initializer");
			expression_clause = &expression_clause->children.front();
		}
		const bool class_member_declaration = declaration.scope.valid() &&
			declaration.scope.value < scopes_.size() &&
			scopes_[declaration.scope.value].kind == ScopeKind::Class;
		const ExprInfo expression = class_member_declaration ?
			semantic_default_member_initializer(*expression_clause,
				declaration.scope, value.type) :
			semantic_expression_for_target(*expression_clause,
				declaration.scope, value.type);
		ExprInfo converted = expression;
		if (expression_clause->kind != PA10NodeKind::BracedInitList)
			converted = apply_context_conversion(expression, value.type,
				semantic_facts_[expression.fact.value].source, declaration.scope);
		if (converted.fact.valid() &&
			semantic_facts_[converted.fact.value].literal_element_count != 0)
			record_constant_address(converted.fact, declaration.scope);
		if (declaration.is_constexpr)
			retarget_constexpr_literal(converted.fact, value.type);
		set_semantic_children(variable,
			std::vector<SemanticFactId>(1, converted.fact));
		*initializer_fact = converted.fact;
	}
	else if (anonymous_union_object || legacy_empty_default_object ||
		((default_object || namespace_default_object) &&
			constructor_requires_runtime(record)))
		set_semantic_children(variable, std::vector<SemanticFactId>(1,
			semantic_constructor_action(storage, source)));
	if (value.kind == BindingKind::Variable && value.has_definition &&
		nonautomatic_class_object && !declaration.is_thread_local)
		record_namespace_lifetime(storage, value.type,
			binding_owners_[storage.value]);
}

ExprInfo PA11SemanticModel::semantic_aggregate_constructor_value(
	const PA10AstNode& source, TypeId target, ScopeId access_scope,
	const std::vector<const PA10AstNode*>& argument_nodes,
	bool value_initialize)
{
	const TypeId object = strip_top_cv_type(target);
	const NamedRecordId record = named_record_for_type(object);
	if (!record.valid() || record.value >= named_.size() ||
		named_[record.value].kind != NamedKind::Class ||
		named_[record.value].class_tag == ClassTag::Union)
		throw std::runtime_error(
			"PA12 aggregate constructor target is not a supported class");
	BindingId aggregate_binding;
	if (!argument_nodes.empty() && !has_constructor_declaration(record) &&
		aggregate_class_initialization_supported(record))
		aggregate_binding = ensure_aggregate_constructor(record);
	const ConstructorSelection selection = select_constructor(record,
		access_scope, argument_nodes, true,
		ConstructorInitializationContext::Direct, aggregate_binding);
	if (!selection.valid())
		throw std::runtime_error("PA12 aggregate constructor selection is incomplete");
	if (function_declaration_kind(selection.binding) ==
		FunctionDeclarationKind::Deleted)
		throw std::runtime_error(
			"PA12 aggregate constructor selects a deleted constructor");
	SemanticFact fact(SemanticFactKind::ConstructorAction, target,
		SemanticValueCategory::Lvalue, &source);
	fact.has_callee = true;
	fact.value_initialize = value_initialize;
	fact.selected_binding = selection.binding;
	fact.selected_scope = selection.scope;
	fact.callable_type = selection.callable_type;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_children(result, selection.arguments);
	return ExprInfo(result, target, SemanticValueCategory::Lvalue, false);
}

void PA11SemanticModel::expand_inheriting_constructor_candidates(
	NamedRecordId record_id, ConstructorInitializationContext context,
	std::vector<ValueRef>& candidates, std::vector<NamedRecordId>& active)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag == ClassTag::Union)
		throw std::runtime_error("PA12 inherited candidate record is invalid");
	const NamedRecord record = named_[record_id.value];
	if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id || !record.name.valid())
		throw std::runtime_error("PA12 inherited candidate owner is invalid");
	const ScopeId record_scope = record.scope;
	const NamedRecordId record_base = record.direct_base;
	for (std::size_t i = 0; i < active.size(); ++i)
		if (active[i] == record_id)
			throw std::runtime_error("PA12 inherited candidate relation cycle");
	if (active.size() >= named_.size())
		throw std::runtime_error("PA12 inherited candidate depth is invalid");
	active.push_back(record_id);
	FlatIndex<BindingId, bool, IdentityHash<BindingId> > seen;
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		if (!candidates[i].binding.valid() ||
			candidates[i].binding.value >= bindings_.size() ||
			candidates[i].binding.value >= binding_owners_.size() ||
			candidates[i].scope != record.scope ||
			binding_owners_[candidates[i].binding.value] != record.scope)
			throw std::runtime_error("PA12 inherited candidate identity is invalid");
		if (seen.find(candidates[i].binding) != NULL)
			throw std::runtime_error("PA12 duplicate inherited candidate identity");
		seen.set(candidates[i].binding, true);
	}
	std::vector<InheritingConstructorRelation> relations;
	const NamedRecordSidecar* sidecar = named_record_sidecar(record_id);
	if (sidecar != NULL)
		relations = sidecar->inheriting_constructors;
	for (std::size_t relation_index = 0;
		relation_index < relations.size(); ++relation_index)
	{
		const NamedRecordId base_record = relations[relation_index].base_record;
		if (!base_record.valid() || base_record.value >= named_.size() ||
			base_record != record_base ||
			named_[base_record.value].kind != NamedKind::Class)
			throw std::runtime_error(
				"PA12 inherited candidate relation is invalid");
		const NamedRecord base = named_[base_record.value];
		if (!base.scope.valid() || base.scope.value >= scopes_.size() ||
			scopes_[base.scope.value].kind != ScopeKind::Class ||
			scopes_[base.scope.value].record != base_record ||
			!base.name.valid())
			throw std::runtime_error(
				"PA12 inherited candidate base owner is invalid");
		const ScopeId base_scope = base.scope;
		const NameId base_name = base.name;

		std::vector<ValueRef> base_direct_candidates;
		const ValueList* base_values =
			scopes_[base_scope.value].values.find(base_name);
		if (base_values != NULL)
		{
			FlatIndex<BindingId, bool, IdentityHash<BindingId> > base_seen;
			for (std::size_t i = 0; i < base_values->entries.size(); ++i)
			{
				const ValueEntry& entry = base_values->entries[i];
				const BindingId candidate_id = entry.binding;
				if (!candidate_id.valid() ||
					candidate_id.value >= bindings_.size() ||
					candidate_id.value >= binding_owners_.size() ||
					binding_owners_[candidate_id.value] != base_scope ||
					entry.origin != base_scope)
					throw std::runtime_error(
						"PA12 inherited candidate value index is invalid");
				if (base_seen.find(candidate_id) != NULL)
					throw std::runtime_error(
						"PA12 duplicate inherited candidate value index");
				base_seen.set(candidate_id, true);
				const Binding& candidate = binding(candidate_id);
				const FunctionFact* function =
					function_fact_for_binding(candidate_id);
				if (function != NULL && function->inheriting_constructor)
					continue;
				if (candidate.kind != BindingKind::Function ||
					!candidate.type.valid() ||
					candidate.type.value >= types_.size() ||
					type_kind(candidate.type) != TypeKind::Function)
					continue;
				const BindingSidecar* candidate_sidecar =
					binding_sidecar(candidate_id);
				if (candidate_sidecar == NULL ||
					candidate_sidecar->constructor_record != base_record ||
					(context == ConstructorInitializationContext::Copy &&
						candidate_sidecar->explicit_constructor))
					continue;
				base_direct_candidates.push_back(
					ValueRef(base_scope, candidate_id));
			}
		}
		expand_inheriting_constructor_candidates(base_record, context,
			base_direct_candidates, active);
		// Recursive publication may grow the base value arena.  Copy the
		// expanded entries before wrapper publication can demand more facts.
		std::vector<ValueEntry> expanded_entries;
		const ValueList* expanded_values =
			scopes_[base_scope.value].values.find(base_name);
		if (expanded_values != NULL)
			expanded_entries = expanded_values->entries;
		for (std::size_t base_index = 0;
			base_index < expanded_entries.size(); ++base_index)
		{
			const ValueEntry& entry = expanded_entries[base_index];
			if (!entry.binding.valid() ||
				entry.binding.value >= bindings_.size() ||
				entry.binding.value >= binding_owners_.size() ||
				binding_owners_[entry.binding.value] != base_scope ||
				entry.origin != base_scope)
				throw std::runtime_error(
					"PA12 inherited candidate value index is invalid");
			const Binding& base_candidate = binding(entry.binding);
			const BindingSidecar* base_sidecar_pointer =
				binding_sidecar(entry.binding);
			if (base_candidate.kind != BindingKind::Function ||
				!base_candidate.type.valid() ||
				base_candidate.type.value >= types_.size() ||
				type_kind(base_candidate.type) != TypeKind::Function ||
				base_sidecar_pointer == NULL ||
				base_sidecar_pointer->constructor_record != base_record)
				continue;
			const BindingSidecar base_sidecar = *base_sidecar_pointer;
			const TypeKey base_signature = types_[base_candidate.type.value];
			if (base_signature.variadic)
				throw std::runtime_error(
					"PA16 variadic inheriting constructors are outside checkpoint");
			if (base_signature.parameters.empty())
				continue;
			const std::size_t minimum_arity =
				inherited_constructor_minimum_arity(entry.binding,
					base_signature);
			for (std::size_t arity = base_signature.parameters.size();
				arity != 0 && arity >= minimum_arity; --arity)
			{
				std::vector<TypeId> wrapper_parameters(
					base_signature.parameters.begin(),
					base_signature.parameters.begin() + arity);
				const TypeId wrapper_type = make_function(wrapper_parameters,
					false, base_signature.result, base_signature.cv);
				bool hidden_by_direct = false;
				for (std::size_t direct = 0; direct < candidates.size(); ++direct)
				{
					const BindingId direct_id = candidates[direct].binding;
					const FunctionFact* direct_function =
						function_fact_for_binding(direct_id);
					if (direct_function != NULL &&
						direct_function->inheriting_constructor)
						continue;
					if (binding(direct_id).type == wrapper_type)
					{
						hidden_by_direct = true;
						break;
					}
				}
				if (hidden_by_direct ||
					(context == ConstructorInitializationContext::Copy &&
						base_sidecar.explicit_constructor))
					continue;
				const BindingId wrapper = ensure_inheriting_constructor(record_id,
					base_record, entry.binding, arity);
				if (seen.find(wrapper) != NULL)
					continue;
				seen.set(wrapper, true);
				candidates.push_back(ValueRef(record_scope, wrapper));
			}
		}
	}
	active.pop_back();
}

ConstructorSelection PA11SemanticModel::select_constructor(
	NamedRecordId record_id, ScopeId access_scope,
	const std::vector<const PA10AstNode*>& argument_nodes,
	bool allow_implicit_default, ConstructorInitializationContext context,
	BindingId forced_binding)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag == ClassTag::Union)
		throw std::runtime_error("PA12 constructor selection record is invalid");
	const NamedRecord& record = named_[record_id.value];
	if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id)
		throw std::runtime_error("PA12 constructor selection owner is invalid");
	const ScopeId class_scope = record.scope;
	std::vector<ValueRef> candidates;
	if (!record.name.valid())
		throw std::runtime_error("PA12 constructor selection name is invalid");
	// Constructors are the canonical class-name value list.  Do not copy or
	// scan the owning scope's unrelated fields, types, or methods on every use.
	// The list is copied into ValueRefs before implicit-default publication so
	// no reference into an arena-backed container survives that growth.
	FlatIndex<BindingId, bool, IdentityHash<BindingId> > seen;
	const bool implicit_default_candidate = argument_nodes.empty() &&
		allow_implicit_default && !forced_binding.valid() &&
		!has_constructor_declaration(record_id);
	if (forced_binding.valid())
	{
		if (forced_binding.value >= bindings_.size() ||
			forced_binding.value >= binding_owners_.size() ||
			binding_owners_[forced_binding.value] != class_scope)
			throw std::runtime_error(
				"PA12 forced constructor owner is invalid");
		const Binding& candidate = binding(forced_binding);
		const BindingSidecar* sidecar = binding_sidecar(forced_binding);
		if (candidate.kind != BindingKind::Function ||
			!candidate.type.valid() || candidate.type.value >= types_.size() ||
			type_kind(candidate.type) != TypeKind::Function || sidecar == NULL ||
			sidecar->constructor_record != record_id)
			throw std::runtime_error("PA12 forced constructor identity is invalid");
		seen.set(forced_binding, true);
		candidates.push_back(ValueRef(class_scope, forced_binding));
	}
	else
	{
		const ValueList* constructor_values =
			scopes_[class_scope.value].values.find(record.name);
		if (constructor_values != NULL)
		{
			for (std::size_t i = 0; i < constructor_values->entries.size(); ++i)
			{
				const ValueEntry& entry = constructor_values->entries[i];
				const BindingId candidate_id = entry.binding;
				if (!candidate_id.valid() || candidate_id.value >= bindings_.size() ||
					candidate_id.value >= binding_owners_.size() ||
					binding_owners_[candidate_id.value] != class_scope ||
					entry.origin != class_scope)
					throw std::runtime_error(
						"PA12 constructor value index identity is invalid");
				if (seen.find(candidate_id) != NULL)
					throw std::runtime_error(
						"PA12 duplicate constructor value index entry");
				seen.set(candidate_id, true);
				const Binding& candidate = binding(candidate_id);
				const FunctionFact* candidate_function =
					function_fact_for_binding(candidate_id);
				if (candidate_function != NULL &&
					candidate_function->inheriting_constructor)
					continue;
				if (candidate.kind != BindingKind::Function ||
					!candidate.type.valid() || candidate.type.value >= types_.size() ||
					type_kind(candidate.type) != TypeKind::Function)
					continue;
				const BindingSidecar* sidecar = binding_sidecar(candidate_id);
				if (sidecar == NULL || sidecar->constructor_record != record_id ||
					(context == ConstructorInitializationContext::Copy &&
						sidecar->explicit_constructor))
					continue;
				candidates.push_back(ValueRef(class_scope, candidate_id));
			}
		}
	}
	if (!forced_binding.valid() && !implicit_default_candidate)
	{
		std::vector<NamedRecordId> active;
		expand_inheriting_constructor_candidates(record_id, context,
			candidates, active);
	}
	if (implicit_default_candidate)
	{
		const BindingId implicit = ensure_implicit_default_constructor(record_id);
		if (!implicit.valid())
			throw std::runtime_error("PA12 implicit constructor is missing");
		if (seen.find(implicit) == NULL)
		{
			seen.set(implicit, true);
			candidates.push_back(ValueRef(class_scope, implicit));
		}
	}
	if (candidates.empty())
		throw std::runtime_error("PA12 no viable constructor");

	std::vector<ExprInfo> arguments;
	arguments.reserve(argument_nodes.size());
	for (std::size_t i = 0; i < argument_nodes.size(); ++i)
	{
		if (argument_nodes[i] == NULL)
			throw std::runtime_error("PA12 constructor argument is missing");
		const PA10AstNode& argument = *argument_nodes[i];
		if (argument.kind == PA10NodeKind::BracedInitList)
			throw std::runtime_error(
				"PA16 nested braced constructor arguments are outside checkpoint");
		if (target_function_id(argument, access_scope) != NULL)
			arguments.push_back(ExprInfo());
		else if (argument.kind == PA10NodeKind::DeclSpecifier &&
			argument.identifier_declspecifier)
			arguments.push_back(semantic_id_expression(argument, access_scope));
		else
			arguments.push_back(semantic_expression(argument, access_scope));
	}

	const TypedFunctionSelection function_selection = select_typed_function(
		candidates, argument_nodes, arguments, access_scope, true);
	if (!function_selection.valid())
		throw std::runtime_error("PA12 constructor selection is incomplete");
	const ValueRef selected = function_selection.selected;
	const FunctionFact* selected_function =
		function_fact_for_binding(selected.binding);
	if (selected_function != NULL && selected_function->inheriting_constructor)
	{
		// The using-declaration publishes a derived wrapper, but accessibility is
		// checked against the original selected base constructor.  Following the
		// typed wrapper chain also preserves that rule for transitive inheritance.
		NamedRecordId access_record = record_id;
		BindingId access_binding = selected.binding;
		bool accessible = false;
		for (std::size_t depth = 0; depth <= named_.size(); ++depth)
		{
			if (!access_record.valid() || access_record.value >= named_.size() ||
				!access_binding.valid() ||
				access_binding.value >= bindings_.size())
				throw std::runtime_error(
					"PA12 inherited constructor chain identity is invalid");
			const FunctionFact* inherited = function_fact_for_binding(access_binding);
			if (inherited == NULL || !inherited->is_constructor ||
				inherited->constructor_record != access_record)
				throw std::runtime_error(
					"PA12 inherited constructor chain is invalid");
			if (!inherited->inheriting_constructor)
			{
				const BindingSidecar* base_sidecar =
					binding_sidecar(access_binding);
				if (base_sidecar == NULL ||
					base_sidecar->constructor_record != access_record ||
					!named_[access_record.value].scope.valid())
					throw std::runtime_error(
						"PA12 inherited constructor access owner is invalid");
				accessible = member_accessible(access_binding,
					named_[access_record.value].scope, access_scope,
					named_type(record_id));
				break;
			}
			const NamedRecord& current_record = named_[access_record.value];
			if (!current_record.has_base ||
				!inherited->inherited_base_record.valid() ||
				inherited->inherited_base_record != current_record.direct_base ||
				!inherited->inherited_base_constructor.valid())
				throw std::runtime_error(
					"PA12 inherited constructor relation chain is invalid");
			access_record = inherited->inherited_base_record;
			access_binding = inherited->inherited_base_constructor;
		}
		if (!accessible)
			throw std::runtime_error("PA12 constructor is inaccessible");
	}
	else if (!member_accessible(selected.binding, selected.scope, access_scope,
		named_type(record_id)))
		throw std::runtime_error("PA12 constructor is inaccessible");
	const BindingSidecar* selected_sidecar = binding_sidecar(selected.binding);
	if (selected_sidecar == NULL ||
		selected_sidecar->constructor_record != record_id)
		throw std::runtime_error("PA12 selected constructor owner is invalid");
	if (context == ConstructorInitializationContext::CopyList &&
		selected_sidecar->explicit_constructor)
		throw std::runtime_error(
			"PA12 copy-list initialization selected an explicit constructor");
	ConstructorSelection result(selected.binding, selected.scope,
		constructor_callable_type(selected.binding));
	result.arguments.reserve(function_selection.arguments.size());
	for (std::size_t i = 0; i < function_selection.arguments.size(); ++i)
	{
		if (!function_selection.arguments[i].fact.valid() ||
			function_selection.arguments[i].fact.value >=
				semantic_facts_.size())
			throw std::runtime_error("PA12 constructor argument fact is invalid");
		result.arguments.push_back(function_selection.arguments[i].fact);
	}
	return result;
}

void PA11SemanticModel::append_constructor_action(
	std::vector<ConstructorActionFact>& actions,
	std::vector<SemanticFactId>& arguments,
	const ConstructorActionFact& action,
	const std::vector<SemanticFactId>& action_arguments)
{
	if ((action.target != ConstructorActionTarget::Base &&
		action.target != ConstructorActionTarget::Member) ||
		(action.target == ConstructorActionTarget::Base &&
			(!action.base_record.valid() || action.member.valid())) ||
		(action.target == ConstructorActionTarget::Member &&
			(!action.member.valid() || action.base_record.valid())) ||
		(action.constructor.valid() && action.initializer.valid()) ||
		(!action.constructor.valid() && !action.initializer.valid()) ||
		(action.value_initialize && !action.constructor.valid()))
		throw std::runtime_error("PA12 constructor action identity is invalid");
	for (std::size_t i = 0; i < action_arguments.size(); ++i)
		if (!action_arguments[i].valid() || action_arguments[i].value >=
			semantic_facts_.size())
			throw std::runtime_error("PA12 constructor argument identity is invalid");
	ConstructorActionFact stored = action;
	if (!action_arguments.empty())
	{
		if (arguments.size() > std::numeric_limits<std::size_t>::max() -
			action_arguments.size())
			throw std::runtime_error("PA12 constructor argument arena overflow");
		stored.argument_begin = arguments.size();
		stored.argument_count = action_arguments.size();
		arguments.insert(arguments.end(), action_arguments.begin(),
			action_arguments.end());
	}
	actions.push_back(stored);
}

void PA11SemanticModel::append_constructor_class_action(
	ScopeId function_scope, ConstructorActionTarget target_kind,
	NamedRecordId base, BindingId member, const PA10AstNode* argument,
	std::vector<ConstructorActionFact>& actions,
	std::vector<SemanticFactId>& arguments)
{
	if (target_kind != ConstructorActionTarget::Base &&
		target_kind != ConstructorActionTarget::Member)
		throw std::runtime_error("PA12 constructor action target is invalid");
	if (target_kind == ConstructorActionTarget::Base &&
		(!base.valid() || member.valid()))
		throw std::runtime_error("PA12 base constructor action identity is invalid");
	if (target_kind == ConstructorActionTarget::Member &&
		(!member.valid() || base.valid() || member.value >= bindings_.size()))
		throw std::runtime_error("PA12 member constructor action identity is invalid");
	const TypeId target_type = target_kind == ConstructorActionTarget::Base ?
		named_type(base) : binding(member).type;
	const NamedRecordId target_record = class_record_for_object_type(target_type);
	if (!target_record.valid() || target_record.value >= named_.size() ||
		named_[target_record.value].kind != NamedKind::Class)
		throw std::runtime_error("PA12 class constructor action target is invalid");
	ConstructorActionFact action(target_kind, base, member);
	action.object_type = target_type;
	if (argument != NULL &&
		((argument->kind == PA10NodeKind::BracedInitList ||
			argument->kind == PA10NodeKind::ParenArgumentList) &&
		argument->children.empty()))
		action.value_initialize = true;
	if (argument != NULL && argument->kind == PA10NodeKind::BracedInitList &&
		!argument->children.empty() &&
		aggregate_class_initialization_supported(target_record))
	{
		action.initializer = semantic_braced_init_list(*argument,
			target_type, function_scope).fact;
		append_constructor_action(actions, arguments, action,
			std::vector<SemanticFactId>());
		return;
	}
	std::vector<const PA10AstNode*> argument_nodes;
	if (argument != NULL)
	{
		if (argument->kind != PA10NodeKind::BracedInitList &&
			argument->kind != PA10NodeKind::ParenArgumentList)
			throw std::runtime_error("PA12 invalid class constructor arguments");
		argument_nodes.reserve(argument->children.size());
		for (std::size_t i = 0; i < argument->children.size(); ++i)
			argument_nodes.push_back(&argument->children[i]);
	}
	const ConstructorSelection selection = select_constructor(target_record,
		function_scope, argument_nodes, true,
		ConstructorInitializationContext::Direct);
	action.constructor = target_kind == ConstructorActionTarget::Base ?
		ensure_constructor_base_entry(selection.binding) : selection.binding;
	action.callable_type = selection.callable_type;
	append_constructor_action(actions, arguments, action, selection.arguments);
}

void PA11SemanticModel::append_constructor_scalar_action(
	ScopeId function_scope, BindingId member, const PA10AstNode* argument,
	std::vector<ConstructorActionFact>& actions,
	std::vector<SemanticFactId>& arguments)
{
	if (!member.valid() || member.value >= bindings_.size() || argument == NULL)
		throw std::runtime_error("PA12 scalar constructor action is invalid");
	const TypeId target_type = binding(member).type;
	const PA10AstNode* expression_node = argument;
	if (argument->kind == PA10NodeKind::ParenArgumentList)
	{
		if (argument->children.empty() && type_kind(strip_top_cv_type(
			target_type)) == TypeKind::Array)
		{
			const SemanticFactId empty_initializer = make_expression_fact(
				SemanticFactKind::BracedInitList,
				strip_top_cv_type(target_type),
				SemanticValueCategory::Lvalue, *argument,
				std::vector<SemanticFactId>());
			const TypeId array_type = strip_top_cv_type(target_type);
			if (!array_type.valid() || array_type.value >= types_.size() ||
				types_[array_type.value].unknown_bound)
				throw std::runtime_error(
					"PA12 empty array initializer has no complete bound");
			std::vector<AggregateElementFact> elements;
			set_semantic_aggregate_elements(empty_initializer, elements,
				types_[array_type.value].bound.value);
			ConstructorActionFact action(ConstructorActionTarget::Member,
				NamedRecordId(), member);
			action.object_type = target_type;
			action.initializer = empty_initializer;
			append_constructor_action(actions, arguments, action,
				std::vector<SemanticFactId>());
			return;
		}
		if (argument->children.size() != 1)
			throw std::runtime_error("PA12 scalar mem-initializer arity mismatch");
		expression_node = &argument->children.front();
	}
	const ExprInfo expression = semantic_expression_for_target(*expression_node,
		function_scope, target_type);
	ExprInfo converted = expression;
	if (argument->kind != PA10NodeKind::BracedInitList)
		converted = apply_context_conversion(expression, target_type,
			semantic_facts_[expression.fact.value].source, function_scope);
	ConstructorActionFact action(ConstructorActionTarget::Member,
		NamedRecordId(), member);
	action.object_type = target_type;
	action.initializer = converted.fact;
	append_constructor_action(actions, arguments, action,
		std::vector<SemanticFactId>());
}

void PA11SemanticModel::append_constructor_member_actions(
	NamedRecordId record_id, ScopeId function_scope,
	const ConstructorMemberInitializerIndex& member_initializers,
	std::vector<ConstructorActionFact>& actions,
	std::vector<SemanticFactId>& arguments)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		!function_scope.valid() || function_scope.value >= scopes_.size() ||
		scopes_[function_scope.value].kind != ScopeKind::Function)
		throw std::runtime_error("PA12 constructor member owner is invalid");
	const NamedRecord& record = named_[record_id.value];
	if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id ||
		scopes_[function_scope.value].parent != record.scope)
		throw std::runtime_error("PA12 constructor member scope is invalid");
	const RecordLayout& layout = record_layout(record_id);
	if (layout.state != RecordLayoutState::Complete)
		throw std::runtime_error("PA12 constructor member layout is incomplete");
	// A member default can demand more facts and grow the layout/type arenas.
	// Copy the declaration-ordered member sequence before starting that work.
	const std::vector<RecordLayoutMember> members = layout.members;
	for (std::size_t i = 0; i < members.size(); ++i)
	{
		const BindingId member_id = members[i].binding;
		if (!member_id.valid() || member_id.value >= bindings_.size() ||
			member_id.value >= binding_owners_.size() ||
			binding_owners_[member_id.value] != record.scope ||
			binding(member_id).kind != BindingKind::Variable ||
			is_static_member(member_id))
			throw std::runtime_error("PA12 constructor member identity is invalid");
		const Binding member = binding(member_id);
		const PA10AstNode* const* explicit_init =
			member_initializers.find(member_id);
		if (explicit_init != NULL)
		{
			const NamedRecordId member_record =
				class_record_for_object_type(member.type);
			if (member_record.valid() && member_record.value < named_.size() &&
				named_[member_record.value].kind == NamedKind::Class)
				append_constructor_class_action(function_scope,
					ConstructorActionTarget::Member, NamedRecordId(), member_id,
					*explicit_init, actions, arguments);
			else
				append_constructor_scalar_action(function_scope, member_id,
					*explicit_init, actions, arguments);
			continue;
		}
		const BindingSidecar* sidecar = binding_sidecar(member_id);
		if (sidecar != NULL && sidecar->default_member_initializer.valid())
		{
			const SemanticFactId initializer_id =
				sidecar->default_member_initializer;
			if (!initializer_id.valid() || initializer_id.value >=
				semantic_facts_.size())
				throw std::runtime_error(
					"PA12 default member initializer identity is invalid");
			const SemanticFact& initializer = semantic_facts_[initializer_id.value];
			if (initializer.kind == SemanticFactKind::ConstructorAction)
			{
				if (!initializer.selected_binding.valid() ||
					initializer.selected_binding.value >= bindings_.size())
					throw std::runtime_error(
						"PA12 default member constructor identity is invalid");
				ConstructorActionFact action(ConstructorActionTarget::Member,
					NamedRecordId(), member_id, initializer.selected_binding);
				action.object_type = member.type;
				action.callable_type = initializer.callable_type;
				action.value_initialize = initializer.value_initialize;
				std::vector<SemanticFactId> initializer_arguments;
				if (initializer.child_count != 0)
				{
					if (initializer.child_begin == InvalidIdentityValue ||
						initializer.child_begin > semantic_children_.size() ||
						initializer.child_count > semantic_children_.size() -
							initializer.child_begin)
						throw std::runtime_error(
							"PA12 constructor initializer range is invalid");
					for (std::size_t child = 0;
						child < initializer.child_count; ++child)
						initializer_arguments.push_back(semantic_children_[
							initializer.child_begin + child]);
				}
				append_constructor_action(actions, arguments, action,
					initializer_arguments);
			}
			else
			{
				ConstructorActionFact action(ConstructorActionTarget::Member,
					NamedRecordId(), member_id);
				action.object_type = member.type;
				action.initializer = initializer_id;
				append_constructor_action(actions, arguments, action,
					std::vector<SemanticFactId>());
			}
			continue;
		}
		const NamedRecordId member_record =
			class_record_for_object_type(member.type);
		if (member_record.valid() && constructor_requires_runtime(member_record))
			append_constructor_class_action(function_scope,
				ConstructorActionTarget::Member, NamedRecordId(), member_id, NULL,
				actions, arguments);
	}
}

void PA11SemanticModel::build_constructor_actions(FunctionFactId function_id)
{
	if (!function_id.valid() || function_id.value >= function_facts_.size())
		throw std::runtime_error("PA12 constructor fact identity is invalid");
	const FunctionFact& initial_function = function_facts_[function_id.value];
	if (!initial_function.is_constructor ||
		!initial_function.constructor_record.valid() ||
		initial_function.constructor_record.value >= named_.size())
		throw std::runtime_error("PA12 constructor action owner is missing");
	if (!initial_function.binding.valid() ||
		initial_function.binding.value >= bindings_.size() ||
		initial_function.binding.value >= binding_owners_.size() ||
		binding_owners_[initial_function.binding.value] != initial_function.owner ||
		binding(initial_function.binding).kind != BindingKind::Function ||
		!initial_function.function_scope.valid() ||
		initial_function.function_scope.value >= scopes_.size() ||
		scopes_[initial_function.function_scope.value].kind != ScopeKind::Function ||
		scopes_[initial_function.function_scope.value].parent != initial_function.owner)
		throw std::runtime_error("PA12 constructor function identity is invalid");
	if (initial_function.constructor_action_begin != InvalidIdentityValue)
		return;
	const NamedRecordId function_record = initial_function.constructor_record;
	const ScopeId function_scope = initial_function.function_scope;
	const PA10AstNode* function_node = initial_function.node;
	const NamedRecord record = named_[function_record.value];
	if (record.kind != NamedKind::Class || !record.scope.valid() ||
		record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != function_record ||
		initial_function.owner != record.scope)
		throw std::runtime_error("PA12 constructor action owner is invalid");
	if (record.direct_base_virtual)
		throw std::runtime_error("PA12 virtual base construction is outside checkpoint");
	if (!record.has_base && record.direct_base.valid())
		throw std::runtime_error("PA12 direct base metadata is invalid");
	if (record.has_base && (!record.direct_base.valid() ||
		record.direct_base.value >= named_.size() ||
		named_[record.direct_base.value].kind != NamedKind::Class ||
		named_[record.direct_base.value].class_tag == ClassTag::Union))
		throw std::runtime_error("PA12 direct base metadata is invalid");
	std::vector<ConstructorActionFact> actions;
	std::vector<SemanticFactId> arguments;
	FlatIndex<BindingId, const PA10AstNode*, IdentityHash<BindingId> >
		member_initializers;
	const PA10AstNode* base_initializer = NULL;
	const PA10AstNode* ctor_initializer = NULL;
	if (function_node != NULL)
	{
		for (std::size_t i = 0; i < function_node->children.size(); ++i)
			if (function_node->children[i].kind == PA10NodeKind::CtorInitializer)
				ctor_initializer = &function_node->children[i];
	}
	if (ctor_initializer != NULL)
	{
		for (std::size_t i = 0; i < ctor_initializer->children.size(); ++i)
		{
			const PA10AstNode& initializer = ctor_initializer->children[i];
			if (initializer.kind != PA10NodeKind::MemInitializer ||
				initializer.children.size() != 2)
				throw std::runtime_error("PA12 invalid mem-initializer");
			const NamePath name = name_path(initializer.children.front());
			if (name.components.size() != 1 || name.global)
				throw std::runtime_error("PA12 qualified mem-initializer is unsupported");
			const PA10AstNode* argument = &initializer.children.back();
			if (argument->kind != PA10NodeKind::ParenArgumentList &&
				argument->kind != PA10NodeKind::BracedInitList)
				throw std::runtime_error("PA12 invalid mem-initializer arguments");
			if (record.has_base && record.direct_base.valid() &&
				record.direct_base.value < named_.size() &&
				named_[record.direct_base.value].name == name.last())
			{
				if (base_initializer != NULL)
					throw std::runtime_error("PA12 duplicate base mem-initializer");
				base_initializer = argument;
				continue;
			}
			const MemberLookup selection = member_lookup(named_type(
				function_record), name.last());
			if (selection.kind != MemberLookupKind::Value ||
				!selection.binding.valid() || selection.owner != record.scope ||
				binding(selection.binding).kind != BindingKind::Variable ||
				is_static_member(selection.binding))
				throw std::runtime_error("PA12 mem-initializer is not a direct field");
			if (member_initializers.find(selection.binding) != NULL)
				throw std::runtime_error("PA12 duplicate member mem-initializer");
			member_initializers.set(selection.binding, argument);
		}
	}
	if (record.has_base)
	{
		if (base_initializer != NULL)
			append_constructor_class_action(function_scope,
				ConstructorActionTarget::Base, record.direct_base, BindingId(),
				base_initializer, actions, arguments);
		else if (constructor_requires_runtime(record.direct_base))
			append_constructor_class_action(function_scope,
				ConstructorActionTarget::Base, record.direct_base, BindingId(), NULL,
				actions, arguments);
	}
	append_constructor_member_actions(function_record, function_scope,
		member_initializers, actions, arguments);
	const std::size_t action_begin = constructor_actions_.size();
	const std::size_t argument_begin = constructor_arguments_.size();
	if (argument_begin > std::numeric_limits<std::size_t>::max() -
		arguments.size())
		throw std::runtime_error("PA12 constructor argument arena overflow");
	constructor_arguments_.insert(constructor_arguments_.end(), arguments.begin(),
		arguments.end());
	for (std::size_t i = 0; i < actions.size(); ++i)
	{
		if (actions[i].argument_begin != InvalidIdentityValue)
		{
			if (actions[i].argument_begin > std::numeric_limits<std::size_t>::max() -
				argument_begin)
				throw std::runtime_error("PA12 constructor argument range overflow");
			actions[i].argument_begin += argument_begin;
		}
		constructor_actions_.push_back(actions[i]);
	}
	FunctionFact& function = function_facts_[function_id.value];
	function.constructor_action_begin = action_begin;
	function.constructor_action_count = actions.size();
}

void PA11SemanticModel::publish_class_constructor_defaults(
	const PA10AstNode& node, ScopeId class_scope)
{
	if (node.kind != PA10NodeKind::ClassSpecifier ||
		!class_scope.valid() || class_scope.value >= scopes_.size() ||
		scopes_[class_scope.value].kind != ScopeKind::Class)
		throw std::runtime_error("PA12 constructor default prepass owner is invalid");
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode& child = node.children[i];
		if (child.kind != PA10NodeKind::SpecialMemberDeclaration &&
			child.kind != PA10NodeKind::SpecialMemberDefinition)
			continue;
		const FunctionFactId* found = function_fact_index_.find(&child);
		if (found == NULL || !found->valid() ||
			found->value >= function_facts_.size())
			throw std::runtime_error("PA12 constructor default fact is missing");
		const FunctionFactId function_id = *found;
		const FunctionFact function = function_facts_[function_id.value];
		if (function.is_destructor)
			continue;
		if (!function.is_constructor || function.owner != class_scope ||
			function.constructor_record != scopes_[class_scope.value].record)
			throw std::runtime_error(
				"PA12 constructor default fact owner is invalid");
		if (function.default_argument_begin != InvalidIdentityValue)
			continue;
		// Each special-member AST node is published once here.  The analysis
		// pass below consumes the owned fact and does not reanalyze defaults.
		record_function_default_arguments(function_id, child, 0);
	}
}

void PA11SemanticModel::analyze_special_member(const PA10AstNode& node,
	ScopeId scope)
{
	const FunctionFactId* function_id = function_fact_index_.find(&node);
	if (function_id == NULL || !function_id->valid() ||
		function_id->value >= function_facts_.size() ||
		(!function_facts_[function_id->value].is_constructor &&
			!function_facts_[function_id->value].is_destructor))
		throw std::runtime_error("PA12 special member fact is missing");
	const FunctionFactId id = *function_id;
	prepare_pa12_member_parameter(function_facts_[id.value]);
	const PA10AstNode* body = NULL;
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (node.children[i].kind == PA10NodeKind::CompoundStatement)
			body = &node.children[i];
	if (body != NULL)
	{
		prepare_pa12_labels(*body, function_facts_[id.value]);
		const ScopeId function_scope = function_facts_[id.value].function_scope;
		const FunctionFact semantic_function = function_facts_[id.value];
		const SemanticFactId body_fact = semantic_compound(*body,
			function_scope, semantic_function, 0, 0, NULL);
		function_facts_[id.value].body_fact = body_fact;
	}
	if (function_facts_[id.value].is_constructor)
		build_constructor_actions(id);
	else
		build_destructor_actions(id);
}

void PA11SemanticModel::prepare_pa12()
{
	for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
		prepare_pa12_node(ast_.root.children[i], global_);
}

void PA11SemanticModel::analyze_pa12()
{
	if (ast_.root.kind != PA10NodeKind::TranslationUnit)
		throw std::runtime_error("PA12 root is not a translation unit");
	pa12_render_mode_ = true;
	prepare_pa12();
	constructor_runtime_states_.assign(named_.size(),
		ConstructorRuntimeCacheState::Unseen);
	constructor_runtime_results_.assign(named_.size(), 0);
	constructor_runtime_invalid_.assign(named_.size(), 0);
	destructor_runtime_states_.assign(named_.size(),
		ConstructorRuntimeCacheState::Unseen);
	destructor_runtime_results_.assign(named_.size(), 0);
	destructor_runtime_invalid_.assign(named_.size(), 0);
	for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
		analyze_pa12_node(ast_.root.children[i], global_);
}
} // namespace pa11_semantic_internal

void emit_pa12_semantics(const PA10Ast& ast, std::ostream& output)
{
	pa11_semantic_internal::PA11SemanticModel model(ast);
	model.analyze();
	model.analyze_pa12();
	model.dump_pa12(output);
}
