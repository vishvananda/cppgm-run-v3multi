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

bool PA11SemanticModel::semantic_local_class_initializer(
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
		set_semantic_children(variable, std::vector<SemanticFactId>(1,
			semantic_constructor_action(storage, source, arguments, access_scope)));
		return true;
	}
	return false;
}

ConstructorSelection PA11SemanticModel::select_constructor(
	NamedRecordId record_id, ScopeId access_scope,
	const std::vector<const PA10AstNode*>& argument_nodes,
	bool allow_implicit_default, ConstructorInitializationContext context)
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
	const ValueList* constructor_values =
		scopes_[class_scope.value].values.find(record.name);
	FlatIndex<BindingId, bool, IdentityHash<BindingId> > seen;
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
	if (candidates.empty() && argument_nodes.empty() &&
		allow_implicit_default && !has_constructor_declaration(record_id))
	{
		const BindingId implicit = ensure_implicit_default_constructor(record_id);
		if (!implicit.valid())
			throw std::runtime_error("PA12 implicit constructor is missing");
		candidates.push_back(ValueRef(class_scope, implicit));
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
	if (!member_accessible(selected.binding, selected.scope, access_scope,
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
	const auto append_action = [this, &actions, &arguments](
		const ConstructorActionFact& action,
		const std::vector<SemanticFactId>& action_arguments) {
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
	};
	const auto append_class = [this, function_scope, &append_action](
		ConstructorActionTarget target_kind, NamedRecordId base,
		BindingId member, const PA10AstNode* argument) {
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
			append_action(action, std::vector<SemanticFactId>());
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
		action.constructor = selection.binding;
		append_action(action, selection.arguments);
	};
	const auto append_scalar = [this, function_scope, &append_action](
		BindingId member, const PA10AstNode* argument) {
		if (argument == NULL)
			return;
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
				ConstructorActionFact action(ConstructorActionTarget::Member,
					NamedRecordId(), member);
				action.object_type = target_type;
				action.initializer = empty_initializer;
				append_action(action, std::vector<SemanticFactId>());
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
				semantic_facts_[expression.fact.value].source);
		ConstructorActionFact action(ConstructorActionTarget::Member,
			NamedRecordId(), member);
		action.object_type = target_type;
		action.initializer = converted.fact;
		append_action(action, std::vector<SemanticFactId>());
	};
	if (record.has_base)
	{
		if (base_initializer != NULL)
			append_class(ConstructorActionTarget::Base, record.direct_base,
				BindingId(), base_initializer);
		else if (constructor_requires_runtime(record.direct_base))
			append_class(ConstructorActionTarget::Base, record.direct_base,
				BindingId(), NULL);
	}
	// append_class may synthesize a constructor and append bindings/facts.
	// Keep the declaration-order member IDs independent of those publishes.
	const std::vector<BindingId> class_members =
		scopes_[record.scope.value].bindings;
	for (std::size_t i = 0; i < class_members.size(); ++i)
	{
		const BindingId member_id = class_members[i];
		if (!member_id.valid() || member_id.value >= bindings_.size() ||
			member_id.value >= binding_owners_.size() ||
			binding_owners_[member_id.value] != record.scope)
			throw std::runtime_error("PA12 constructor member identity is invalid");
		const Binding member = binding(member_id);
		if (member.kind != BindingKind::Variable || is_static_member(member_id))
			continue;
		const PA10AstNode* const* explicit_init =
			member_initializers.find(member_id);
		if (explicit_init != NULL)
		{
			const NamedRecordId member_record =
				class_record_for_object_type(member.type);
			if (member_record.valid() && member_record.value < named_.size() &&
				named_[member_record.value].kind == NamedKind::Class)
				append_class(ConstructorActionTarget::Member, NamedRecordId(),
					member_id, *explicit_init);
			else
				append_scalar(member_id, *explicit_init);
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
					for (std::size_t child = 0; child < initializer.child_count; ++child)
						initializer_arguments.push_back(semantic_children_[
							initializer.child_begin + child]);
				}
				append_action(action, initializer_arguments);
			}
			else
			{
				ConstructorActionFact action(ConstructorActionTarget::Member,
					NamedRecordId(), member_id);
				action.object_type = member.type;
				action.initializer = sidecar->default_member_initializer;
				append_action(action, std::vector<SemanticFactId>());
			}
			continue;
		}
		const NamedRecordId member_record =
			class_record_for_object_type(member.type);
		if (member_record.valid() && constructor_requires_runtime(member_record))
			append_class(ConstructorActionTarget::Member, NamedRecordId(), member_id,
				NULL);
	}
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
