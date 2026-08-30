#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

#include <algorithm>
#include <limits>

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

namespace
{

FunctionDeclarationKind special_member_initializer_kind_impl(
	const PA10AstNode& node)
{
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode& child = node.children[i];
		if (child.kind != PA10NodeKind::Initializer ||
			child.children.size() != 1)
			continue;
		const PA10AstNode& initializer = child.children.front();
		if (initializer.kind != PA10NodeKind::SpecialInitializer ||
			!initializer.has_token)
			continue;
		if (initializer.token == SimpleTokenType::KW_DEFAULT)
			return FunctionDeclarationKind::Defaulted;
		if (initializer.token == SimpleTokenType::KW_DELETE)
			return FunctionDeclarationKind::Deleted;
	}
	return FunctionDeclarationKind::Normal;
}

bool special_member_has_bare_noexcept(const PA10AstNode& declarator)
{
	for (std::size_t i = 0; i < declarator.children.size(); ++i)
		if (declarator.children[i].kind == PA10NodeKind::FunctionQualifier &&
			declarator.children[i].has_token &&
			declarator.children[i].token == SimpleTokenType::KW_NOEXCEPT &&
			declarator.children[i].children.empty())
			return true;
	return false;
}

bool special_member_is_inline(const PA10AstNode* member_specifiers)
{
	if (member_specifiers == NULL)
		return false;
	for (std::size_t i = 0; i < member_specifiers->children.size(); ++i)
		if (member_specifiers->children[i].kind == PA10NodeKind::MemberSpecifier &&
			member_specifiers->children[i].has_token &&
			member_specifiers->children[i].token == SimpleTokenType::KW_INLINE)
			return true;
	return false;
}

bool special_member_is_destructor(const PA10AstNode& node)
{
	if (node.kind == PA10NodeKind::Identifier &&
		node.unqualified_id_kind == PA10UnqualifiedIdKind::Destructor)
		return true;
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (special_member_is_destructor(node.children[i]))
			return true;
	return false;
}

}

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
	{
		const FunctionFact* function = function_fact(node);
		if (function == NULL || (!function->is_constructor &&
			!function->is_destructor) || node.children.empty())
			throw std::runtime_error("PA12 special member fact is missing");
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind == PA10NodeKind::CompoundStatement)
				prepare_pa12_compound(node.children[i], function->function_scope);
		return;
	}
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
	bool declaration_definition, NamedRecordId record,
	const PA10AstNode* direct_operand,
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
	// PA11 owns this exact per-declarator bit.  Binding::has_definition is
	// intentionally redeclaration-merged, so it cannot classify this source
	// declarator without recovering syntax here.  Function definitions carry
	// the same bit; only the variable path below uses it for object ownership.
	const bool defines_variable = value.kind == BindingKind::Variable &&
		declaration_definition;
	const bool namespace_object_scope = value.kind == BindingKind::Variable &&
		defines_variable && declaration.scope.valid() &&
		declaration.scope.value < scopes_.size() &&
		scopes_[declaration.scope.value].kind == ScopeKind::Namespace &&
		(record.valid() && record.value < named_.size());
	const bool static_member_definition = value.kind == BindingKind::Variable &&
		defines_variable && is_static_member(storage) && declaration.scope.valid() &&
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
	if (defines_variable &&
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
			const MemberLookup selection = member_lookup(named_type(
				function_record), name.last());
			bool inherited_constructor_name = selection.kind ==
				MemberLookupKind::Value && selection.owner.valid() &&
				selection.owner != record.scope && selection.owner.value < scopes_.size() &&
				scopes_[selection.owner.value].kind == ScopeKind::Class &&
				scopes_[selection.owner.value].record.valid();
			if (inherited_constructor_name)
			{
				const ValueList* values = scopes_[selection.owner.value].values.find(
					name.last());
				const NamedRecordId owner_record =
					scopes_[selection.owner.value].record;
				inherited_constructor_name = values != NULL &&
					!values->entries.empty();
				for (std::size_t value = 0;
					inherited_constructor_name && value < values->entries.size(); ++value)
				{
					const BindingId binding_id = values->entries[value].binding;
					inherited_constructor_name = binding_id.valid() &&
						binding_id.value < bindings_.size();
					if (inherited_constructor_name)
					{
						const BindingSidecar* sidecar = binding_sidecar(binding_id);
						inherited_constructor_name =
							binding(binding_id).kind == BindingKind::Function &&
							sidecar != NULL &&
							sidecar->constructor_record == owner_record;
					}
				}
			}
			const ValueList* blocked_values = NULL;
			if (selection.kind == MemberLookupKind::Blocked &&
				selection.owner.valid() && selection.owner.value < scopes_.size())
				blocked_values = scopes_[selection.owner.value].values.find(name.last());
			const bool member_value_claimed =
				(selection.kind == MemberLookupKind::Value &&
					!inherited_constructor_name) ||
				(selection.kind == MemberLookupKind::Blocked &&
					blocked_values != NULL && !blocked_values->entries.empty());
			if (member_value_claimed)
			{
				// Class member lookup wins over a type alias for a single
				// unqualified mem-initializer-id.  A base member or an
				// ambiguous value set therefore cannot be recovered as a base.
				if (selection.kind != MemberLookupKind::Value ||
					selection.owner != record.scope ||
					!selection.binding.valid() ||
					binding(selection.binding).kind != BindingKind::Variable ||
					is_static_member(selection.binding))
					throw std::runtime_error("PA12 mem-initializer is not a direct field");
				if (member_initializers.find(selection.binding) != NULL)
					throw std::runtime_error("PA12 duplicate member mem-initializer");
				member_initializers.set(selection.binding, argument);
				continue;
			}
			// A mem-initializer names a base by its resolved type, not by the
			// spelling of the direct base declaration.  Preserve the spelling
			// fallback for the ordinary injected class-name case, but let the
			// canonical PA11 type identity recognize aliases as well.
			NamedRecordId resolved_base;
			TypeId resolved_type;
			BindingId resolved_declaration;
			if (record.has_base && record.direct_base.valid())
			{
				// The constructor definition point preserves declaration-time
				// visibility, while its function scope is the class-member access
				// context for both the lexical search and access check.
				const SourcePoint point = function_node != NULL ?
					SourcePoint(function_node->source_begin) :
					lookup_source_point(function_scope);
				resolved_type = lookup_type_path(name, function_scope, point,
					&resolved_declaration, function_scope);
				// A base mem-initializer needs the exact named class type;
				// object-type lookup must not turn an array alias into a base.
				resolved_base = named_record_for_type(resolved_type);
			}
			if (record.has_base && record.direct_base.valid() &&
				record.direct_base.value < named_.size() &&
				((!resolved_type.valid() && !resolved_declaration.valid() &&
					named_[record.direct_base.value].name == name.last()) ||
					resolved_base == record.direct_base))
			{
				if (base_initializer != NULL)
					throw std::runtime_error("PA12 duplicate base mem-initializer");
				base_initializer = argument;
				continue;
			}
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

BindingId PA11SemanticModel::matching_constructor_declaration(
	NamedRecordId record_id, ScopeId owner_scope,
	const TypeKey& requested_signature) const
{
	const NamedRecord& record = named_[record_id.value];
	BindingId declared_constructor;
	const ValueList* values = scopes_[owner_scope.value].values.find(record.name);
	if (values != NULL)
		for (std::size_t i = 0; i < values->entries.size(); ++i)
		{
			const ValueEntry& entry = values->entries[i];
			if (entry.origin != owner_scope)
				continue;
			if (!entry.binding.valid() || entry.binding.value >= bindings_.size() ||
				entry.binding.value >= binding_owners_.size() ||
				binding_owners_[entry.binding.value] != owner_scope)
				throw std::runtime_error(
					"PA11 constructor declaration index identity is invalid");
			const Binding& candidate = binding(entry.binding);
			if (candidate.kind != BindingKind::Function ||
				candidate.name != record.name || !candidate.type.valid() ||
				candidate.type.value >= types_.size() ||
				type_kind(candidate.type) != TypeKind::Function)
				continue;
			const BindingSidecar* candidate_sidecar =
				binding_sidecar(entry.binding);
			const FunctionFact* candidate_fact =
				function_fact_for_binding(entry.binding);
			if (candidate_sidecar == NULL || candidate_sidecar->constructor_record !=
				record_id || candidate_fact == NULL || !candidate_fact->is_constructor ||
				candidate_fact->constructor_record != record_id ||
				candidate_fact->owner != owner_scope || candidate_fact->binding !=
				entry.binding || candidate_fact->node == NULL ||
				candidate_fact->inheriting_constructor)
				continue;
			const TypeKey& candidate_signature = types_[candidate.type.value];
			if (!(candidate_signature == requested_signature) ||
				candidate.language_linkage != current_language_linkage_ ||
				candidate.internal_linkage)
				continue;
			if (declared_constructor.valid())
				throw std::runtime_error(
					"PA11 constructor declaration identity is ambiguous");
			declared_constructor = entry.binding;
		}
	if (!declared_constructor.valid())
		throw std::runtime_error(
			"PA11 out-of-class constructor definition does not match declaration");
	return declared_constructor;
}

void PA11SemanticModel::process_special_member(const PA10AstNode& node,
	ScopeId scope)
{
	if (node.kind != PA10NodeKind::SpecialMemberDeclaration &&
		node.kind != PA10NodeKind::SpecialMemberDefinition)
		throw std::runtime_error("invalid PA11 special member");
	const PA10AstNode* declarator = NULL;
	const PA10AstNode* member_specifiers = NULL;
	const PA10AstNode* body = NULL;
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		if (node.children[i].kind == PA10NodeKind::Declarator)
			declarator = &node.children[i];
		else if (node.children[i].kind == PA10NodeKind::MemberSpecifiers)
			member_specifiers = &node.children[i];
		else if (node.children[i].kind == PA10NodeKind::CompoundStatement)
			body = &node.children[i];
	}
	if (declarator == NULL)
		throw std::runtime_error("PA11 special member declarator is missing");
	const bool destructor = special_member_is_destructor(*declarator);
	const DeclaratorName name = declarator_name(*declarator);
	if (!name.found || name.path.components.empty())
		throw std::runtime_error("PA11 special member name is missing");
	const ScopeId owner_scope = declaration_scope(name.path, scope);
	if (!owner_scope.valid() || owner_scope.value >= scopes_.size() ||
		scopes_[owner_scope.value].kind != ScopeKind::Class ||
		!scopes_[owner_scope.value].record.valid() ||
		scopes_[owner_scope.value].record.value >= named_.size())
		throw std::runtime_error("PA11 special member has no class owner");
	const NamedRecordId record_id = scopes_[owner_scope.value].record;
	const NamedRecord& record = named_[record_id.value];
	if (!record.name.valid() || name.path.last() != record.name)
		throw std::runtime_error("PA11 special member name does not match class");
	if (member_specifiers != NULL)
		for (std::size_t i = 0; i < member_specifiers->children.size(); ++i)
			if (member_specifiers->children[i].has_token &&
				member_specifiers->children[i].token == SimpleTokenType::KW_STATIC)
				throw std::runtime_error("PA11 static special member is invalid");
	const bool explicit_constructor = !destructor && member_specifiers != NULL &&
		std::find_if(member_specifiers->children.begin(),
			member_specifiers->children.end(),
			[](const PA10AstNode& child) {
				return child.has_token && child.token == SimpleTokenType::KW_EXPLICIT;
			}) != member_specifiers->children.end();
	const PA10AstNode* clause = top_parameter_clause(*declarator);
	if (clause == NULL)
		throw std::runtime_error("PA11 special member parameter clause is missing");
	bool variadic = false;
	std::vector<ParamFact> parameters;
	const std::vector<TypeId> parameter_types_value = parameter_types(*clause,
		owner_scope, &variadic, &parameters);
	const FunctionDeclarationKind declaration_kind =
		special_member_initializer_kind_impl(node);
	const bool definition = body != NULL || declaration_kind !=
		FunctionDeclarationKind::Normal;
	const bool nonthrowing = special_member_has_bare_noexcept(*declarator);
	const bool inline_member = special_member_is_inline(member_specifiers);
	const TypeId type = make_function(parameter_types_value, variadic,
		fundamental(FundamentalType::Void));
	const TypeKey requested_signature = types_[type.value];
	const bool out_of_class_definition =
		node.kind == PA10NodeKind::SpecialMemberDefinition &&
		owner_scope != scope;
	bool has_class_value_parameter = false;
	for (std::size_t parameter = 0;
		parameter < parameter_types_value.size(); ++parameter)
		if (class_value_type(parameter_types_value[parameter]))
			has_class_value_parameter = true;
	const bool narrow_class_value_signature = !requested_signature.variadic &&
		requested_signature.parameters.size() == 1 &&
		has_class_value_parameter &&
		empty_class_value_type(requested_signature.parameters.front());
	const bool constructor_declaration_only = !destructor && body == NULL &&
		declaration_kind == FunctionDeclarationKind::Normal &&
		node.kind == PA10NodeKind::SpecialMemberDeclaration;
	const bool narrow_class_value_compatibility =
		narrow_class_value_signature &&
		(constructor_declaration_only ||
			(out_of_class_definition &&
				declaration_kind == FunctionDeclarationKind::Normal));
	if (!destructor && has_class_value_parameter &&
		!narrow_class_value_compatibility)
		throw std::runtime_error(
			"PA11 unsupported class-value constructor boundary");
	if (!destructor && out_of_class_definition &&
		!has_constructor_declaration(record_id))
		throw std::runtime_error(
			"PA11 out-of-class constructor definition has no declaration");
	BindingId declared_constructor;
	if (!destructor && out_of_class_definition)
	{
		declared_constructor = matching_constructor_declaration(record_id,
			owner_scope, requested_signature);
		const BindingSidecar* declared_sidecar =
			binding_sidecar(declared_constructor);
		if (declared_sidecar != NULL &&
			declared_sidecar->nonthrowing != nonthrowing)
			throw std::runtime_error(
				"PA11 conflicting constructor exception specification");
		if (binding(declared_constructor).has_definition)
			throw std::runtime_error("duplicate constructor definition");
	}
	BindingId function_binding;
	bool mark_definition = false;
	if (destructor)
	{
		function_binding = destructor_binding(record_id);
		if (node.kind == PA10NodeKind::SpecialMemberDefinition &&
			owner_scope != scope && !function_binding.valid())
			throw std::runtime_error(
				"PA11 special member definition has no declaration");
		if (function_binding.valid())
		{
			if (function_binding.value >= bindings_.size() ||
				function_binding.value >= binding_owners_.size() ||
				binding_owners_[function_binding.value] != owner_scope)
				throw std::runtime_error("PA11 destructor binding owner is invalid");
			Binding& existing_binding = binding(function_binding);
			if (existing_binding.kind != BindingKind::Function ||
				existing_binding.name != record.name ||
				existing_binding.type != type ||
				existing_binding.language_linkage != current_language_linkage_ ||
				existing_binding.internal_linkage)
				throw std::runtime_error("PA11 conflicting destructor declaration");
			const BindingSidecar* existing_sidecar =
				binding_sidecar(function_binding);
			if (existing_sidecar != NULL &&
				existing_sidecar->nonthrowing != nonthrowing)
				throw std::runtime_error(
					"PA11 conflicting destructor exception specification");
			if (definition && existing_binding.has_definition)
				throw std::runtime_error("duplicate destructor definition");
			if (definition)
				existing_binding.has_definition = true;
		}
		else
		{
			Binding value(BindingKind::Function, record.name, type);
			value.has_definition = definition;
			value.language_linkage = current_language_linkage_;
			function_binding = store_binding(owner_scope, value);
		}
	}
	else if (out_of_class_definition)
	{
		// The definition must attach to the already-indexed declaration.  In
		// particular, never let add_value manufacture a new constructor overload
		// before discovering that the qualified definition was mismatched.
		function_binding = declared_constructor;
		mark_definition = true;
	}
	else
		function_binding = add_value(owner_scope, record.name, type, true,
			definition, true, BindingId(), SourcePoint(node.source_begin), false,
			current_language_linkage_, declaration_kind);
	record_function_declarator(function_binding, name, *declarator,
		declaration_kind);
	BindingSidecar sidecar;
	const BindingSidecar* existing = binding_sidecar(function_binding);
	if (existing != NULL)
		sidecar = *existing;
	if (destructor)
		sidecar.destructor_record = record_id;
	else
	{
		sidecar.constructor_record = record_id;
		sidecar.explicit_constructor = sidecar.explicit_constructor ||
			explicit_constructor;
	}
	sidecar.nonthrowing = sidecar.nonthrowing || nonthrowing;
	sidecar.inline_member = sidecar.inline_member || inline_member;
	set_binding_sidecar(function_binding, sidecar);
	NamedRecordSidecar record_sidecar;
	const NamedRecordSidecar* existing_record = named_record_sidecar(record_id);
	if (existing_record != NULL)
		record_sidecar = *existing_record;
	if (destructor)
	{
		record_sidecar.has_destructor_declaration = true;
		record_sidecar.destructor_binding = function_binding;
	}
	else
	{
		record_sidecar.has_constructor_declaration = true;
		if (parameter_types_value.empty())
			record_sidecar.default_constructor_binding = function_binding;
	}
	set_named_record_sidecar(record_id, record_sidecar);
	const ScopeId function_scope = create_scope(ScopeKind::Function, owner_scope,
		record.name);
	function_bindings_.set(function_scope, function_binding);
	function_definition_points_.set(function_scope,
		SourcePoint(node.source_begin));
	for (std::size_t i = 0; i < parameters.size(); ++i)
	{
		const BindingId parameter = store_binding(function_scope,
			Binding(BindingKind::Parameter, parameters[i].name,
				parameters[i].type));
		if (parameters[i].name.valid())
			append_value_index(function_scope, parameters[i].name, parameter,
				ScopeId(), SourcePoint(node.source_begin));
	}
	FunctionFact function_fact(&node, owner_scope, function_binding, function_scope,
		ScopeId());
	function_fact.is_constructor = !destructor;
	function_fact.is_destructor = destructor;
	function_fact.out_of_class_definition = out_of_class_definition;
	if (mark_definition)
		binding(function_binding).has_definition = true;
	if (destructor)
		function_fact.destructor_record = record_id;
	else
		function_fact.constructor_record = record_id;
	if (declaration_kind == FunctionDeclarationKind::Defaulted)
		function_fact.synthetic = true;
	if (body != NULL)
		function_fact.body_scope = process_compound_statement(*body,
			function_scope);
	const FunctionFactId function_id(function_facts_.size());
	function_facts_.push_back(function_fact);
	function_fact_index_.set(&node, function_id);
	function_binding_fact_index_.set(function_binding, function_id);
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
	const FunctionFact special_member = function_facts_[id.value];
	const BindingSidecar* sidecar = binding_sidecar(special_member.binding);
	if (special_member.out_of_class_definition &&
		(sidecar == NULL || !sidecar->inline_member))
	{
		if (special_member.is_constructor)
		{
			(void)ensure_constructor_base_entry(special_member.binding);
		}
		else
			(void)ensure_destructor_base_entry(special_member.binding);
	}
}

void PA11SemanticModel::prepare_pa12()
{
	for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
		prepare_pa12_node(ast_.root.children[i], global_);
}

class PA11SemanticModel::CanonicalTruthFinalizer
{
public:
	explicit CanonicalTruthFinalizer(PA11SemanticModel& model)
		: model_(model),
		  semantic_count_(model.semantic_facts_.size()),
		  binding_count_(model.bindings_.size()),
		  binding_base_(semantic_count_),
		  function_base_(0),
		  invalid_(),
		  node_count_(0)
	{
		initialize_domain();
	}

	void build_edges();
	void propagate();
	void publish();

private:
	struct ResultNodeId
	{
		std::size_t value;

		explicit ResultNodeId(std::size_t value = InvalidIdentityValue)
			: value(value)
		{}

		bool valid() const
		{
			return value != InvalidIdentityValue;
		}
	};

	struct ResultEdge
	{
		ResultNodeId target;
		ResultNodeId next;

		ResultEdge(ResultNodeId target = ResultNodeId(),
			ResultNodeId next = ResultNodeId())
			: target(target), next(next)
		{}
	};

	void initialize_domain();
	void add_edge(ResultNodeId source, ResultNodeId target);
	void add_semantic_edge(SemanticFactId source, ResultNodeId target);
	void seed(ResultNodeId node);
	bool is_result_edge(const SemanticFact& owner, std::size_t child) const;
	void build_binding_edges(const SemanticFact& owner, std::size_t node);
	void build_call_edge(const SemanticFact& owner, std::size_t node);
	void build_child_edges(const SemanticFact& owner, std::size_t node);
	void build_return_owner_edges();
	void publish_functions();
	void publish_semantic();
	void publish_conversions();

	PA11SemanticModel& model_;
	std::size_t semantic_count_;
	std::size_t binding_count_;
	std::size_t binding_base_;
	std::size_t function_base_;
	ResultNodeId invalid_;
	std::size_t node_count_;
	std::vector<FunctionFactId> definition_by_binding_;
	std::vector<FunctionFactId> defined_functions_;
	std::vector<ResultNodeId> function_nodes_;
	std::vector<ResultNodeId> heads_;
	std::vector<ResultEdge> edges_;
	std::vector<unsigned char> true_nodes_;
	std::vector<unsigned char> queued_;
	std::vector<ResultNodeId> worklist_;
};

void PA11SemanticModel::CanonicalTruthFinalizer::initialize_domain()
{
	const std::size_t max_value =
		std::numeric_limits<std::size_t>::max();
	binding_base_ = semantic_count_;
	if (binding_count_ > max_value - binding_base_)
		throw std::runtime_error(
			"PA12 canonical truth binding node domain overflow");
	function_base_ = binding_base_ + binding_count_;
	if (model_.function_facts_.size() > max_value - function_base_)
		throw std::runtime_error(
			"PA12 canonical truth function node domain overflow");

	definition_by_binding_.assign(binding_count_, FunctionFactId());
	for (std::size_t i = 0; i < model_.function_facts_.size(); ++i)
	{
		const FunctionFact& function = model_.function_facts_[i];
		if (function.node == NULL ||
			function.node->kind != PA10NodeKind::FunctionDefinition)
			continue;
		const FunctionFactId id(i);
		defined_functions_.push_back(id);
		if (function.binding.valid() &&
			function.binding.value < binding_count_ &&
			!definition_by_binding_[function.binding.value].valid())
			definition_by_binding_[function.binding.value] = id;
	}
	if (defined_functions_.size() > max_value - function_base_)
		throw std::runtime_error(
			"PA12 canonical truth result node domain overflow");
	node_count_ = function_base_ + defined_functions_.size();

	function_nodes_.assign(model_.function_facts_.size(), invalid_);
	for (std::size_t i = 0; i < defined_functions_.size(); ++i)
	{
		const FunctionFactId function = defined_functions_[i];
		function_nodes_[function.value] =
			ResultNodeId(function_base_ + i);
	}
	heads_.assign(node_count_, invalid_);
	true_nodes_.assign(node_count_, 0);
	queued_.assign(node_count_, 0);
}

void PA11SemanticModel::CanonicalTruthFinalizer::add_edge(
	ResultNodeId source, ResultNodeId target)
{
	if (!source.valid() || !target.valid() ||
		source.value >= node_count_ || target.value >= node_count_)
		throw std::runtime_error(
			"PA12 canonical truth edge endpoint is out of range");
	const std::size_t edge = edges_.size();
	edges_.push_back(ResultEdge(target, heads_[source.value]));
	heads_[source.value] = ResultNodeId(edge);
}

void PA11SemanticModel::CanonicalTruthFinalizer::add_semantic_edge(
	SemanticFactId source, ResultNodeId target)
{
	if (!source.valid())
		return;
	if (source.value >= semantic_count_)
		throw std::runtime_error(
			"PA12 canonical truth semantic edge endpoint is out of range");
	add_edge(ResultNodeId(source.value), target);
}

void PA11SemanticModel::CanonicalTruthFinalizer::seed(ResultNodeId node)
{
	if (!node.valid() || node.value >= node_count_)
		throw std::runtime_error(
			"PA12 canonical truth seed endpoint is out of range");
	if (true_nodes_[node.value] != 0)
		return;
	true_nodes_[node.value] = 1;
	if (queued_[node.value] == 0)
	{
		queued_[node.value] = 1;
		worklist_.push_back(node);
	}
}

bool PA11SemanticModel::CanonicalTruthFinalizer::is_result_edge(
	const SemanticFact& owner, std::size_t child) const
{
	switch (owner.kind)
	{
	case SemanticFactKind::Variable:
	case SemanticFactKind::ReturnStatement:
		return child == 0;
	case SemanticFactKind::UnaryExpression:
		return owner.token != SimpleTokenType::OP_AMP &&
			child == 0;
	case SemanticFactKind::PostfixExpression:
	case SemanticFactKind::CastExpression:
		return child == 0;
	case SemanticFactKind::BinaryExpression:
		return owner.token != SimpleTokenType::OP_COMMA ||
			child == 1;
	case SemanticFactKind::AssignmentExpression:
		return owner.token == SimpleTokenType::OP_ASS ?
			child == 1 : child < 2;
	case SemanticFactKind::ConditionalExpression:
		return child == 1 || child == 2;
	case SemanticFactKind::SubscriptExpression:
		return child == 0;
	default:
		return false;
	}
}

void PA11SemanticModel::CanonicalTruthFinalizer::build_binding_edges(
	const SemanticFact& owner, std::size_t node)
{
	if (owner.binding.valid() &&
		owner.binding.value >= binding_count_)
		throw std::runtime_error(
			"PA12 canonical truth binding endpoint is out of range");
	const bool variable_binding = owner.binding.valid() &&
		owner.binding.value < binding_count_ &&
		model_.binding(owner.binding).kind == BindingKind::Variable;
	if ((owner.kind == SemanticFactKind::Variable ||
		owner.kind == SemanticFactKind::AssignmentExpression) &&
		variable_binding &&
		(owner.kind == SemanticFactKind::AssignmentExpression ||
			owner.child_count != 0))
		add_edge(ResultNodeId(node),
			ResultNodeId(binding_base_ + owner.binding.value));
	if (owner.kind == SemanticFactKind::IdExpression &&
		variable_binding)
		add_edge(ResultNodeId(binding_base_ + owner.binding.value),
			ResultNodeId(node));
}

void PA11SemanticModel::CanonicalTruthFinalizer::build_call_edge(
	const SemanticFact& owner, std::size_t node)
{
	if (owner.kind != SemanticFactKind::CallExpression ||
		!owner.has_callee || !owner.selected_binding.valid())
		return;
	if (owner.selected_binding.value >= binding_count_)
		throw std::runtime_error(
			"PA12 canonical truth selected binding endpoint is out of range");
	const FunctionFactId definition =
		definition_by_binding_[owner.selected_binding.value];
	if (!definition.valid())
		return;
	if (definition.value >= function_nodes_.size())
		throw std::runtime_error(
			"PA12 canonical truth function endpoint is out of range");
	const ResultNodeId function_node = function_nodes_[definition.value];
	if (!function_node.valid())
		throw std::runtime_error(
			"PA12 canonical truth definition node is invalid");
	add_edge(function_node, ResultNodeId(node));
}

void PA11SemanticModel::CanonicalTruthFinalizer::build_child_edges(
	const SemanticFact& owner, std::size_t node)
{
	if (owner.child_count == 0)
		return;
	if (owner.child_begin == invalid_.value ||
		owner.child_begin > model_.semantic_children_.size() ||
		owner.child_count > model_.semantic_children_.size() -
			owner.child_begin)
		throw std::runtime_error(
			"PA12 semantic result edge range is invalid");
	for (std::size_t child = 0; child < owner.child_count; ++child)
	{
		if (!is_result_edge(owner, child))
			continue;
		const SemanticFactId source =
			model_.semantic_children_[owner.child_begin + child];
		add_semantic_edge(source, ResultNodeId(node));
	}
}

void PA11SemanticModel::CanonicalTruthFinalizer::build_return_owner_edges()
{
	for (std::size_t i = 0;
		i < model_.function_return_owners_.size(); ++i)
	{
		const std::pair<SemanticFactId, FunctionFactId>& owner =
			model_.function_return_owners_[i];
		const SemanticFactId result = owner.first;
		const FunctionFactId function = owner.second;
		if (!result.valid() || result.value >= semantic_count_ ||
			!function.valid() ||
			function.value >= model_.function_facts_.size())
			throw std::runtime_error(
				"PA12 retained return owner identity is malformed");
		if (model_.semantic_facts_[result.value].kind !=
			SemanticFactKind::ReturnStatement)
			throw std::runtime_error(
				"PA12 retained return owner result is not a return statement");
		const FunctionFact& source_function =
			model_.function_facts_[function.value];
		if (source_function.node == NULL ||
			source_function.node->kind != PA10NodeKind::FunctionDefinition ||
			!source_function.body_fact.valid() ||
			source_function.body_fact.value >= semantic_count_ ||
			!source_function.binding.valid() ||
			source_function.binding.value >= binding_count_)
			throw std::runtime_error(
				"PA12 retained return owner is not definition-owned");
		const FunctionFactId definition =
			definition_by_binding_[source_function.binding.value];
		if (!definition.valid() ||
			definition.value >= function_nodes_.size() ||
			!function_nodes_[definition.value].valid())
			throw std::runtime_error(
				"PA12 retained return owner has no canonical definition");
		add_edge(ResultNodeId(result.value),
			function_nodes_[definition.value]);
	}
}

void PA11SemanticModel::CanonicalTruthFinalizer::build_edges()
{
	for (std::size_t i = 0; i < semantic_count_; ++i)
	{
		if (model_.semantic_facts_[i].contains_member_value)
			seed(ResultNodeId(i));
	}
	for (std::size_t i = 0; i < semantic_count_; ++i)
	{
		const SemanticFact& owner = model_.semantic_facts_[i];
		build_binding_edges(owner, i);
		build_call_edge(owner, i);
		build_child_edges(owner, i);
	}
	build_return_owner_edges();
}

void PA11SemanticModel::CanonicalTruthFinalizer::propagate()
{
	for (std::size_t cursor = 0; cursor < worklist_.size(); ++cursor)
	{
		const ResultNodeId source = worklist_[cursor];
		if (!source.valid() || source.value >= node_count_)
			throw std::runtime_error(
				"PA12 canonical truth worklist endpoint is out of range");
		for (ResultNodeId edge = heads_[source.value];
			edge.valid(); edge = edges_[edge.value].next)
		{
			if (edge.value >= edges_.size())
				throw std::runtime_error(
					"PA12 canonical truth edge index is out of range");
			const ResultNodeId target = edges_[edge.value].target;
			if (!target.valid() || target.value >= node_count_)
				throw std::runtime_error(
					"PA12 canonical truth edge endpoint is out of range");
			if (true_nodes_[target.value] != 0)
				continue;
			true_nodes_[target.value] = 1;
			if (queued_[target.value] == 0)
			{
				queued_[target.value] = 1;
				worklist_.push_back(target);
			}
		}
	}
}

void PA11SemanticModel::CanonicalTruthFinalizer::publish_functions()
{
	for (std::size_t i = 0; i < defined_functions_.size(); ++i)
	{
		const FunctionFactId function = defined_functions_[i];
		const ResultNodeId node = function_nodes_[function.value];
		if (!node.valid())
			throw std::runtime_error(
				"PA12 canonical truth function node is invalid");
		model_.function_facts_[function.value].
			return_result_contains_member_value =
			true_nodes_[node.value] != 0;
	}
}

void PA11SemanticModel::CanonicalTruthFinalizer::publish_semantic()
{
	for (std::size_t i = 0; i < semantic_count_; ++i)
	{
		if (true_nodes_[i] == 0)
			continue;
		SemanticFact& fact = model_.semantic_facts_[i];
		fact.contains_member_value = true;
		if (fact.kind != SemanticFactKind::CastExpression ||
			fact.child_count == 0)
			continue;
		if (fact.child_begin == invalid_.value ||
			fact.child_begin >= model_.semantic_children_.size())
			throw std::runtime_error(
				"PA12 canonical truth cast child endpoint is out of range");
		const SemanticFactId source =
			model_.semantic_children_[fact.child_begin];
		if (source.valid() && source.value >= semantic_count_)
			throw std::runtime_error(
				"PA12 canonical truth semantic edge endpoint is out of range");
		if (source.valid() &&
			model_.semantic_facts_[source.value].canonical_truth &&
			model_.semantic_facts_[source.value].contains_member_value)
		{
			fact.canonical_truth = true;
			fact.direct_bool_boundary = true;
		}
	}
}

void PA11SemanticModel::CanonicalTruthFinalizer::publish_conversions()
{
	for (std::size_t i = 0; i < semantic_count_; ++i)
	{
		SemanticFact& fact = model_.semantic_facts_[i];
		if (!fact.canonical_truth)
			continue;
		const bool member_truth = true_nodes_[i] != 0;
		const bool class_pointer_operation = fact.kind ==
			SemanticFactKind::BinaryExpression && fact.operation_type.valid() &&
			fact.operation_type.value < model_.types_.size() &&
			model_.types_[fact.operation_type.value].kind == TypeKind::Pointer;
		// class_record_for_object_type intentionally unwraps arrays for object
		// layout queries.  A pointer-to-array is not a pointer to a class object,
		// so keep this truth boundary exact to the pointer's cv-stripped pointee.
		const TypeId pointer_pointee = class_pointer_operation ?
			model_.strip_cv_type(
				model_.types_[fact.operation_type.value].child) : TypeId();
		const bool class_pointer_truth = class_pointer_operation &&
			pointer_pointee.valid() && pointer_pointee.value < model_.types_.size() &&
			model_.type_kind(pointer_pointee) == TypeKind::Named &&
			model_.class_record_for_object_type(pointer_pointee).valid();
		const bool typed_truth = member_truth || fact.size_type_derived ||
			class_pointer_truth;
		if (member_truth)
			fact.direct_bool_boundary = true;
		if (fact.conversion_count == 0)
			continue;
		if (fact.conversion_begin == invalid_.value ||
			fact.conversion_begin > model_.conversion_facts_.size() ||
			fact.conversion_count > model_.conversion_facts_.size() -
				fact.conversion_begin)
			throw std::runtime_error(
				"PA12 canonical truth conversion range is invalid");
		for (std::size_t conversion = 0;
			conversion < fact.conversion_count; ++conversion)
		{
			ConversionFact& owned = model_.conversion_facts_[
				fact.conversion_begin + conversion];
			// Canonical comparisons already carry an i64 LowIR truth value.
			// Preserve that physical value across non-bool consumers; a bool
			// destination remains an ABI/storage boundary and materializes u8.
			if (model_.bool_id(owned.source) &&
				((typed_truth && !model_.bool_id(owned.target)) ||
					(member_truth && model_.bool_id(owned.target))))
				owned.canonical_truth_policy =
					CanonicalTruthPolicy::Preserve;
		}
	}
}

void PA11SemanticModel::CanonicalTruthFinalizer::publish()
{
	publish_functions();
	publish_semantic();
	publish_conversions();
}

void PA11SemanticModel::finalize_canonical_truth()
{
	CanonicalTruthFinalizer finalizer(*this);
	finalizer.build_edges();
	finalizer.propagate();
	finalizer.publish();
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
	finalize_canonical_truth();
}
} // namespace pa11_semantic_internal

void emit_pa12_semantics(const PA10Ast& ast, std::ostream& output)
{
	pa11_semantic_internal::PA11SemanticModel model(ast);
	model.analyze();
	model.analyze_pa12();
	model.dump_pa12(output);
}
