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

void PA11SemanticModel::build_constructor_actions(FunctionFactId function_id)
{
	if (!function_id.valid() || function_id.value >= function_facts_.size())
		throw std::runtime_error("PA12 constructor fact identity is invalid");
	const FunctionFact& initial_function = function_facts_[function_id.value];
	if (!initial_function.is_constructor ||
		!initial_function.constructor_record.valid() ||
		initial_function.constructor_record.value >= named_.size())
		throw std::runtime_error("PA12 constructor action owner is missing");
	if (initial_function.constructor_action_begin != InvalidIdentityValue)
		return;
	const NamedRecordId function_record = initial_function.constructor_record;
	const ScopeId function_scope = initial_function.function_scope;
	const PA10AstNode* function_node = initial_function.node;
	const NamedRecord record = named_[function_record.value];
	if (record.kind != NamedKind::Class || !record.scope.valid() ||
		record.scope.value >= scopes_.size())
		throw std::runtime_error("PA12 constructor action owner is invalid");
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
	const auto append_action = [&actions, &arguments](
		const ConstructorActionFact& action,
		const std::vector<SemanticFactId>& action_arguments) {
		ConstructorActionFact stored = action;
		if (!action_arguments.empty())
		{
			stored.argument_begin = arguments.size();
			stored.argument_count = action_arguments.size();
			arguments.insert(arguments.end(), action_arguments.begin(),
				action_arguments.end());
		}
		actions.push_back(stored);
	};
	const auto constructor_for = [this](NamedRecordId target) -> BindingId {
		BindingId result = default_constructor_binding(target);
		if (!result.valid())
			result = ensure_implicit_default_constructor(target);
		if (!result.valid() || function_declaration_kind(result) ==
			FunctionDeclarationKind::Deleted)
			throw std::runtime_error("PA12 default constructor is unavailable");
		return result;
	};
	const auto append_class = [this, function_scope, &append_action, &constructor_for](
		ConstructorActionTarget target_kind, NamedRecordId base,
		BindingId member, const PA10AstNode* argument) {
		const TypeId target_type = target_kind == ConstructorActionTarget::Base ?
			named_type(base) : binding(member).type;
		const NamedRecordId target_record = named_record_for_type(target_type);
		if (!target_record.valid() || target_record.value >= named_.size() ||
			named_[target_record.value].kind != NamedKind::Class)
			throw std::runtime_error("PA12 class constructor action target is invalid");
		ConstructorActionFact action(target_kind, base, member);
		std::vector<SemanticFactId> arguments;
		if (argument != NULL && argument->kind == PA10NodeKind::BracedInitList &&
			!argument->children.empty())
		{
			action.initializer = semantic_braced_init_list(*argument,
				target_type, function_scope).fact;
			append_action(action, arguments);
			return;
		}
		if (argument != NULL && argument->kind == PA10NodeKind::ParenArgumentList &&
			!argument->children.empty())
			throw std::runtime_error(
				"PA12 class constructor arguments are outside checkpoint");
		action.constructor = constructor_for(target_record);
		append_action(action, arguments);
	};
	const auto append_scalar = [this, function_scope, &append_action](
		BindingId member, const PA10AstNode* argument) {
		if (argument == NULL)
			return;
		const TypeId target_type = binding(member).type;
		const PA10AstNode* expression_node = argument;
		if (argument->kind == PA10NodeKind::ParenArgumentList)
		{
			if (argument->children.size() != 1)
				throw std::runtime_error("PA12 scalar mem-initializer arity mismatch");
			expression_node = &argument->children.front();
		}
		const ExprInfo expression = semantic_expression_for_target(*expression_node,
			function_scope, target_type);
		if (argument->kind != PA10NodeKind::BracedInitList)
			apply_context_conversion(expression, target_type,
				semantic_facts_[expression.fact.value].source);
		ConstructorActionFact action(ConstructorActionTarget::Member,
			NamedRecordId(), member);
		action.initializer = expression.fact;
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
		const Binding member = binding(member_id);
		if (member.kind != BindingKind::Variable || is_static_member(member_id))
			continue;
		const PA10AstNode* const* explicit_init =
			member_initializers.find(member_id);
		if (explicit_init != NULL)
		{
			const NamedRecordId member_record = named_record_for_type(member.type);
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
			const SemanticFact& initializer = semantic_facts_[
				sidecar->default_member_initializer.value];
			if (initializer.kind == SemanticFactKind::ConstructorAction)
			{
				ConstructorActionFact action(ConstructorActionTarget::Member,
					NamedRecordId(), member_id, initializer.selected_binding);
				std::vector<SemanticFactId> arguments;
				if (initializer.child_count != 0)
				{
					if (initializer.child_begin == InvalidIdentityValue ||
						initializer.child_begin + initializer.child_count >
						semantic_children_.size())
						throw std::runtime_error("PA12 constructor initializer range is invalid");
					for (std::size_t child = 0; child < initializer.child_count; ++child)
						arguments.push_back(semantic_children_[initializer.child_begin + child]);
				}
				append_action(action, arguments);
			}
			else
			{
				ConstructorActionFact action(ConstructorActionTarget::Member,
					NamedRecordId(), member_id);
				action.initializer = sidecar->default_member_initializer;
				append_action(action, std::vector<SemanticFactId>());
			}
			continue;
		}
		const NamedRecordId member_record = named_record_for_type(member.type);
		if (member_record.valid() && constructor_requires_runtime(member_record))
			append_class(ConstructorActionTarget::Member, NamedRecordId(), member_id,
				NULL);
	}
	const std::size_t action_begin = constructor_actions_.size();
	const std::size_t argument_begin = constructor_arguments_.size();
	constructor_arguments_.insert(constructor_arguments_.end(), arguments.begin(),
		arguments.end());
	for (std::size_t i = 0; i < actions.size(); ++i)
	{
		if (actions[i].argument_begin != InvalidIdentityValue)
			actions[i].argument_begin += argument_begin;
		constructor_actions_.push_back(actions[i]);
	}
	FunctionFact& function = function_facts_[function_id.value];
	function.constructor_action_begin = action_begin;
	function.constructor_action_count = actions.size();
}

void PA11SemanticModel::analyze_special_member(const PA10AstNode& node,
	ScopeId scope)
{
	const FunctionFactId* function_id = function_fact_index_.find(&node);
	if (function_id == NULL || !function_id->valid() ||
		function_id->value >= function_facts_.size() ||
		!function_facts_[function_id->value].is_constructor)
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
	build_constructor_actions(id);
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
