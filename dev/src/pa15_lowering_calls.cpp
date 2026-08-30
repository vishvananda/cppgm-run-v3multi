#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

static lowir_model::CallEffectsMode builtin_effects_mode(
	BuiltinCallEffects effects)
{
	switch (effects)
	{
	case BuiltinCallEffects::Default:
		return lowir_model::CFXM_DEFAULT;
	case BuiltinCallEffects::ReadNone:
		return lowir_model::CFXM_READNONE;
	case BuiltinCallEffects::ReadOnly:
		return lowir_model::CFXM_READONLY;
	case BuiltinCallEffects::ReadWrite:
		return lowir_model::CFXM_READWRITE;
	}
	throw std::runtime_error("PA15 invalid builtin effects fact");
}

static lowir_model::CallUnwindMode builtin_unwind_mode(
	BuiltinCallUnwind unwind)
{
	switch (unwind)
	{
	case BuiltinCallUnwind::Default:
		return lowir_model::CUM_DEFAULT;
	case BuiltinCallUnwind::May:
		return lowir_model::CUM_MAY;
	case BuiltinCallUnwind::No:
		return lowir_model::CUM_NO;
	}
	throw std::runtime_error("PA15 invalid builtin unwind fact");
}

static lowir_model::CallReturnMode builtin_return_mode(
	BuiltinCallReturn returns)
{
	switch (returns)
	{
	case BuiltinCallReturn::Default:
		return lowir_model::CRM_DEFAULT;
	case BuiltinCallReturn::Returns:
		return lowir_model::CRM_RETURNS;
	case BuiltinCallReturn::NoReturn:
		return lowir_model::CRM_NORETURN;
	}
	throw std::runtime_error("PA15 invalid builtin return fact");
}

static lowir_model::ParamCaptureMode builtin_capture_mode(
	BuiltinParameterCapture capture)
{
	switch (capture)
	{
	case BuiltinParameterCapture::Default:
		return lowir_model::PCM_DEFAULT;
	case BuiltinParameterCapture::NoCapture:
		return lowir_model::PCM_NOCAPTURE;
	case BuiltinParameterCapture::MayCapture:
		return lowir_model::PCM_MAYCAPTURE;
	}
	throw std::runtime_error("PA15 invalid builtin capture fact");
}

static lowir_model::ParamAccessMode builtin_access_mode(
	BuiltinParameterAccess access)
{
	switch (access)
	{
	case BuiltinParameterAccess::Default:
		return lowir_model::PAM_DEFAULT;
	case BuiltinParameterAccess::None:
		return lowir_model::PAM_NONE;
	case BuiltinParameterAccess::Read:
		return lowir_model::PAM_READ;
	case BuiltinParameterAccess::Write:
		return lowir_model::PAM_WRITE;
	case BuiltinParameterAccess::ReadWrite:
		return lowir_model::PAM_READWRITE;
	}
	throw std::runtime_error("PA15 invalid builtin access fact");
}

static lowir_model::ParamAliasMode builtin_alias_mode(
	BuiltinParameterAlias alias)
{
	switch (alias)
	{
	case BuiltinParameterAlias::Default:
		return lowir_model::PALM_DEFAULT;
	case BuiltinParameterAlias::NoAlias:
		return lowir_model::PALM_NOALIAS;
	}
	throw std::runtime_error("PA15 invalid builtin alias fact");
}

void Pa15Lowerer::collect_demanded_member_functions(
	std::vector<unsigned char>* demanded,
	std::vector<unsigned char>* declarations,
	std::vector<TypeId>* declaration_types) const
{
	if (demanded == NULL || demanded->size() != model_.function_facts_.size() ||
		declarations == NULL || declarations->size() != model_.bindings_.size() ||
		declaration_types == NULL || declaration_types->size() !=
		model_.bindings_.size())
		throw std::runtime_error("PA15 member demand output is missing");
	// Build the canonical class-owner relation once from scope-owned binding
	// identities.  Static call facts can then validate their selected owner in
	// O(1); duplicate or malformed ownership is rejected before the reachable
	// worklist starts, rather than being recovered textually at each call.
	std::vector<ScopeId> class_binding_owners(model_.bindings_.size());
	std::vector<unsigned char> class_binding_seen(model_.bindings_.size(), 0);
	for (std::size_t scope_index = 0; scope_index < model_.scopes_.size();
		++scope_index)
	{
		const Scope& scope = model_.scopes_[scope_index];
		if (scope.kind != ScopeKind::Class)
			continue;
		if (!scope.record.valid() || scope.record.value >= model_.named_.size() ||
			model_.named_[scope.record.value].kind != NamedKind::Class ||
			model_.named_[scope.record.value].scope != ScopeId(scope_index))
			throw std::runtime_error("PA15 class scope ownership is invalid");
		for (std::size_t i = 0; i < scope.bindings.size(); ++i)
		{
			const BindingId binding_id = scope.bindings[i];
			if (!binding_id.valid() || binding_id.value >= model_.bindings_.size() ||
				binding_id.value >= model_.binding_owners_.size() ||
				model_.binding_owners_[binding_id.value] != ScopeId(scope_index) ||
				class_binding_seen[binding_id.value] != 0)
				throw std::runtime_error(
					"PA15 class member binding ownership is invalid");
			class_binding_seen[binding_id.value] = 1;
			class_binding_owners[binding_id.value] = ScopeId(scope_index);
		}
	}
	std::vector<FunctionFactId> function_work;
	std::vector<unsigned char> scanned_functions(
		model_.function_facts_.size(), 0);
	for (std::size_t i = 0; i < model_.function_facts_.size(); ++i)
	{
		const FunctionFact& fact = model_.function_facts_[i];
		if (!fact.owner.valid() || fact.owner.value >= model_.scopes_.size() ||
			model_.scopes_[fact.owner.value].kind != ScopeKind::Namespace ||
			!fact.function_scope.valid())
			continue;
		function_work.push_back(FunctionFactId(i));
	}
	// A fact can be shared by a global initializer and an ordinary function.
	// Global roots may inline a bounded subset of synthetic aggregate helpers,
	// while function lowering must still demand that helper when its call is
	// reachable.  Keep the two reachability domains independent; one visited
	// bit must never turn a context-sensitive demand into a global shortcut.
	// Each typed fact may be reached by an ordinary global root (where a
	// proven aggregate constructor can be inlined) and by a TLS root (where
	// the constructor must remain a demanded call).  Keep one visited bit per
	// root mode so the first traversal cannot suppress the other semantics.
	std::vector<unsigned char> scanned_global_fact_modes(
		model_.semantic_facts_.size(), 0);
	std::vector<unsigned char> scanned_runtime_facts(
		model_.semantic_facts_.size(), 0);
	const auto validate_fact_base_path = [this](const SemanticFact& fact) {
		if (fact.base_path_count == 0)
		{
			if (fact.base_path_begin != InvalidIdentityValue)
				throw std::runtime_error("PA15 semantic base path is malformed");
			return;
		}
		if (fact.base_path_begin == InvalidIdentityValue ||
			fact.base_path_begin > model_.semantic_base_paths_.size() ||
			fact.base_path_count > model_.semantic_base_paths_.size() -
				fact.base_path_begin)
			throw std::runtime_error("PA15 semantic base path is malformed");
	};
	const auto demand_special_member_base_entry = [this, demanded,
		&function_work](BindingId source, bool constructor) {
		const BindingId* entry = constructor ?
			model_.constructor_base_entry_bindings_.find(source) :
			model_.destructor_base_entry_bindings_.find(source);
		if (entry == NULL)
			return;
		if (!entry->valid() || entry->value >= model_.bindings_.size())
			throw std::runtime_error(
				"PA15 special-member base entry identity is invalid");
		const FunctionFactId* entry_id =
			model_.function_binding_fact_index_.find(*entry);
		if (entry_id == NULL || !entry_id->valid() || entry_id->value >=
			model_.function_facts_.size() || model_.function_facts_[entry_id->value].binding !=
			*entry || (constructor ?
				!model_.function_facts_[entry_id->value].constructor_base_entry :
				!model_.function_facts_[entry_id->value].destructor_base_entry))
			throw std::runtime_error(
				"PA15 special-member base entry fact is invalid");
		if (!(*demanded)[entry_id->value])
		{
			(*demanded)[entry_id->value] = 1;
			function_work.push_back(*entry_id);
		}
	};
	const auto demand_constructor_fact = [this, demanded, declarations,
		declaration_types, &function_work, &demand_special_member_base_entry](
		const SemanticFact& fact, bool global_root) {
		if (!fact.has_callee || !fact.selected_binding.valid() ||
			fact.selected_binding.value >= model_.bindings_.size())
			throw std::runtime_error("PA15 constructor demand fact is incomplete");
		const FunctionFactId* target_id =
			model_.function_binding_fact_index_.find(fact.selected_binding);
		if (target_id == NULL || !target_id->valid() || target_id->value >=
			model_.function_facts_.size() ||
			model_.function_facts_[target_id->value].binding !=
			fact.selected_binding || !model_.function_facts_[target_id->value].is_constructor)
			throw std::runtime_error("PA15 constructor demand target is invalid");
		const BindingSidecar* sidecar =
			model_.binding_sidecar(fact.selected_binding);
		if (sidecar == NULL || !sidecar->constructor_record.valid() ||
			sidecar->constructor_record.value >= model_.named_.size())
			throw std::runtime_error("PA15 constructor demand owner is invalid");
		const NamedRecord& record =
			model_.named_[sidecar->constructor_record.value];
		const FunctionFact& target = model_.function_facts_[target_id->value];
		const Binding& target_binding = model_.binding(fact.selected_binding);
		const bool external_declaration =
			record.kind == NamedKind::Class &&
			record.class_tag != ClassTag::Union && record.name.valid() &&
			!target.body_fact.valid() && !target.synthetic &&
			!target_binding.has_definition;
		bool no_op = false;
		if (record.kind == NamedKind::Class && record.class_tag != ClassTag::Union &&
			record.name.valid())
		{
			if (!fact.selected_scope.valid() || fact.selected_scope.value >=
				model_.scopes_.size() || fact.selected_scope != record.scope ||
				!fact.callable_type.valid() || model_.type_kind(fact.callable_type) !=
				TypeKind::Function)
				throw std::runtime_error("PA15 constructor demand type is invalid");
			if (!external_declaration)
			{
				const FunctionFact& checked = checked_constructor_function(
					fact.selected_binding, sidecar->constructor_record);
				no_op = checked.synthetic && checked.constructor_action_count == 0;
			}
		}
		if (global_root && !no_op &&
			global_aggregate_constructor_inline_eligible(fact))
			return;
		if (external_declaration)
		{
			if (fact.selected_binding.value >= declarations->size() ||
				!fact.callable_type.valid() ||
				model_.type_kind(fact.callable_type) != TypeKind::Function)
				throw std::runtime_error(
					"PA15 constructor declaration signature is missing");
			if ((*declaration_types)[fact.selected_binding.value].valid() &&
				(*declaration_types)[fact.selected_binding.value] !=
				fact.callable_type)
				throw std::runtime_error(
					"PA15 constructor declaration signature changed");
			(*declarations)[fact.selected_binding.value] = 1;
			(*declaration_types)[fact.selected_binding.value] =
				fact.callable_type;
			return;
		}
		if (!no_op)
		{
			if (!(*demanded)[target_id->value])
			{
				(*demanded)[target_id->value] = 1;
				function_work.push_back(*target_id);
			}
			demand_special_member_base_entry(fact.selected_binding, true);
		}
	};
	const auto demand_destructor_fact = [this, demanded, declarations,
		declaration_types, &function_work,
		&demand_special_member_base_entry](const SemanticFact& fact) {
		if (!fact.has_callee || !fact.has_implicit_object ||
			!fact.selected_binding.valid() || fact.selected_binding.value >=
			model_.bindings_.size() || !fact.selected_scope.valid() ||
			fact.selected_scope.value >= model_.scopes_.size() ||
			model_.scopes_[fact.selected_scope.value].kind != ScopeKind::Class ||
			!fact.callable_type.valid() || model_.type_kind(fact.callable_type) !=
			TypeKind::Function)
			throw std::runtime_error("PA15 destructor demand fact is incomplete");
		const Binding& binding = model_.binding(fact.selected_binding);
		const BindingSidecar* sidecar = model_.binding_sidecar(
			fact.selected_binding);
		if (binding.kind != BindingKind::Function ||
			model_.type_kind(binding.type) != TypeKind::Function || sidecar == NULL ||
			!sidecar->destructor_record.valid() ||
			sidecar->destructor_record.value >= model_.named_.size())
			throw std::runtime_error("PA15 destructor demand owner is invalid");
		const NamedRecord& record = model_.named_[sidecar->destructor_record.value];
		if (record.kind != NamedKind::Class || record.class_tag == ClassTag::Union ||
			!record.scope.valid() || record.scope != fact.selected_scope)
			throw std::runtime_error("PA15 destructor demand scope is invalid");
		const TypeKey& raw = model_.types_[binding.type.value];
		const TypeKey& callable = model_.types_[fact.callable_type.value];
		if (raw.variadic || !raw.parameters.empty() || raw.result != fact.type ||
			callable.variadic || callable.parameters.size() != 1 ||
			callable.result != fact.type)
			throw std::runtime_error("PA15 destructor demand type is invalid");
		const TypeId hidden = model_.strip_cv_type(
			model_.expression_object_type(callable.parameters.front()));
		if (!hidden.valid() || model_.type_kind(hidden) != TypeKind::Pointer ||
			model_.class_scope_for_type(model_.strip_cv_type(
				model_.types_[hidden.value].child)) != fact.selected_scope)
			throw std::runtime_error("PA15 destructor demand hidden object is invalid");
		const FunctionFactId* target_id =
			model_.function_binding_fact_index_.find(fact.selected_binding);
		if (target_id == NULL || !target_id->valid() || target_id->value >=
			model_.function_facts_.size() ||
			!model_.function_facts_[target_id->value].is_destructor ||
			model_.function_facts_[target_id->value].binding != fact.selected_binding)
			throw std::runtime_error("PA15 destructor demand target is invalid");
		const FunctionFact& target = model_.function_facts_[target_id->value];
		const bool external_declaration = !target.body_fact.valid() &&
			!target.synthetic && !binding.has_definition;
		if (external_declaration)
		{
			if (fact.selected_binding.value >= declarations->size() ||
				fact.selected_binding.value >= declaration_types->size())
				throw std::runtime_error("PA15 destructor declaration is invalid");
			if ((*declaration_types)[fact.selected_binding.value].valid() &&
				(*declaration_types)[fact.selected_binding.value] !=
				fact.callable_type)
				throw std::runtime_error(
					"PA15 destructor declaration signature changed");
			(*declarations)[fact.selected_binding.value] = 1;
			(*declaration_types)[fact.selected_binding.value] = fact.callable_type;
			return;
		}
		(void)checked_destructor_function(fact.selected_binding,
			sidecar->destructor_record);
		if (!(*demanded)[target_id->value])
		{
			(*demanded)[target_id->value] = 1;
			function_work.push_back(*target_id);
		}
		demand_special_member_base_entry(fact.selected_binding, false);
	};
	// Global/static roots are lowered after function collection and therefore
	// are not reachable from a namespace function body.  Walk their typed
	// semantic roots once so a constructor selected by an aggregate edge is
	// still emitted when constant-data lowering elides the runtime call.
	std::vector<std::pair<SemanticFactId, bool> > root_fact_work;
	for (std::map<std::size_t, SemanticFactId>::const_iterator root =
		variable_facts_.begin(); root != variable_facts_.end(); ++root)
	{
		if (!root->second.valid() || root->first >= model_.bindings_.size())
			continue;
		// Only namespace and static-member definitions are lowered by the
		// separate global initializer pass.  Automatic variables remain owned by
		// their enclosing function's demand walk; marking their facts here would
		// hide a constructor call from that walk without gaining any global
		// demand information.
		const std::map<std::size_t, const DeclarationFact*>::const_iterator
			declaration = declaration_by_binding_.find(root->first);
		const bool namespace_definition = declaration != declaration_by_binding_.end() &&
			declaration->second != NULL && declaration->second->scope.valid() &&
			declaration->second->scope.value < model_.scopes_.size() &&
			model_.scopes_[declaration->second->scope.value].kind ==
			ScopeKind::Namespace;
		const bool static_member_definition = model_.is_static_member(
			BindingId(root->first));
		if (namespace_definition || static_member_definition)
		{
			const std::map<std::size_t, bool>::const_iterator tls =
				thread_local_by_binding_.find(root->first);
			// TLS construction runs through its guarded helper, so its typed
			// aggregate constructor must remain a real demand even when an
			// ordinary namespace aggregate may use the static-data fast path.
			const bool inline_global_root = tls == thread_local_by_binding_.end() ||
				!tls->second;
			root_fact_work.push_back(std::make_pair(root->second,
				inline_global_root));
		}
	}
	while (!root_fact_work.empty())
	{
		const std::pair<SemanticFactId, bool> work = root_fact_work.back();
		root_fact_work.pop_back();
		const SemanticFactId fact_id = work.first;
		const bool global_root = work.second;
		if (!fact_id.valid() || fact_id.value >= model_.semantic_facts_.size())
			throw std::runtime_error("PA15 root demand fact is invalid");
		const unsigned char global_mode = global_root ? 1 : 2;
		if ((scanned_global_fact_modes[fact_id.value] & global_mode) != 0)
			continue;
		scanned_global_fact_modes[fact_id.value] |= global_mode;
		const SemanticFact& fact = model_.semantic_facts_[fact_id.value];
		validate_fact_base_path(fact);
		if (model_.aggregate_ranges_.find(fact_id) != NULL)
		{
			std::size_t element_count = 0;
			std::size_t total_count = 0;
			const AggregateElementFact* elements = aggregate_elements(fact_id,
				&element_count, &total_count);
			(void) total_count;
			for (std::size_t i = 0; i < element_count; ++i)
				root_fact_work.push_back(std::make_pair(elements[i].initializer,
					global_root));
		}
		if (fact.child_count != 0 &&
			(fact.child_begin == InvalidIdentityValue ||
				fact.child_begin > model_.semantic_children_.size() ||
				fact.child_count > model_.semantic_children_.size() -
					fact.child_begin))
			throw std::runtime_error("PA15 root demand child range is invalid");
		if (fact.kind == SemanticFactKind::ConstructorAction && fact.has_callee)
			demand_constructor_fact(fact, global_root);
		// A storage-backed constructor action owns a typed CallExpression child
		// rather than duplicating the callee on the wrapper fact.  Global roots
		// must consume that child for emission demand just as function-body roots
		// do; otherwise the later initializer materializer can reach a direct
		// constructor call whose selected function was never emitted.
		if (fact.kind == SemanticFactKind::CallExpression && fact.has_callee &&
			!fact.has_implicit_object && fact.selected_binding.valid() &&
			fact.selected_binding.value < model_.bindings_.size())
		{
			const BindingSidecar* sidecar = model_.binding_sidecar(
				fact.selected_binding);
			if (sidecar != NULL && sidecar->constructor_record.valid() &&
				sidecar->constructor_record.value < model_.named_.size() &&
				model_.named_[sidecar->constructor_record.value].name.valid())
					demand_constructor_fact(fact, global_root);
		}
		if (fact.kind == SemanticFactKind::DestructorCall && fact.has_callee)
			demand_destructor_fact(fact);
		for (std::size_t i = 0; i < fact.child_count; ++i)
			root_fact_work.push_back(std::make_pair(
				model_.semantic_children_[fact.child_begin + i], global_root));
	}
	for (std::size_t i = 0; i < model_.lifetime_facts_.size(); ++i)
	{
		const LifetimeFact& lifetime = model_.lifetime_facts_[i];
		const FunctionFactId* target_id =
			model_.function_binding_fact_index_.find(lifetime.destructor);
		if (target_id == NULL || !target_id->valid() ||
			target_id->value >= model_.function_facts_.size() ||
			!model_.function_facts_[target_id->value].is_destructor ||
			model_.function_facts_[target_id->value].binding != lifetime.destructor)
			throw std::runtime_error("PA15 lifetime destructor demand is invalid");
		if (!(*demanded)[target_id->value])
		{
			(*demanded)[target_id->value] = 1;
			function_work.push_back(*target_id);
		}
		demand_special_member_base_entry(lifetime.destructor, false);
	}
	while (!function_work.empty())
	{
		const FunctionFactId function_id = function_work.back();
		function_work.pop_back();
		if (!function_id.valid() || function_id.value >=
			model_.function_facts_.size())
			throw std::runtime_error("PA15 member demand function is invalid");
		if (scanned_functions[function_id.value] != 0)
			continue;
		scanned_functions[function_id.value] = 1;
		const FunctionFact& function = model_.function_facts_[function_id.value];
		std::vector<SemanticFactId> fact_work;
		if (!function.binding.valid() || function.binding.value >=
			model_.bindings_.size() || !function.owner.valid() ||
			function.owner.value >= model_.scopes_.size())
			throw std::runtime_error("PA15 member demand function identity is invalid");
		if (function.function_scope.valid() &&
			(function.function_scope.value >= model_.scopes_.size() ||
				model_.scopes_[function.function_scope.value].kind != ScopeKind::Function ||
				model_.scopes_[function.function_scope.value].parent != function.owner))
			throw std::runtime_error("PA15 member demand function scope is invalid");
		if (function.is_constructor)
		{
			if (function.constructor_action_begin == InvalidIdentityValue ||
				function.constructor_action_begin >
				model_.constructor_actions_.size() ||
				function.constructor_action_count >
				model_.constructor_actions_.size() -
				function.constructor_action_begin)
				throw std::runtime_error(
					"PA15 constructor action range is invalid");
			for (std::size_t action_index = 0;
				action_index < function.constructor_action_count; ++action_index)
			{
				const ConstructorActionFact& action =
					model_.constructor_actions_[function.constructor_action_begin +
						action_index];
				if (action.argument_count != 0 &&
					(action.argument_begin == InvalidIdentityValue ||
						action.argument_begin > model_.constructor_arguments_.size() ||
						action.argument_count > model_.constructor_arguments_.size() -
							action.argument_begin))
						throw std::runtime_error(
							"PA15 constructor argument range is invalid");
				if (action.argument_count == 0 &&
					action.argument_begin != InvalidIdentityValue &&
					action.argument_begin > model_.constructor_arguments_.size())
						throw std::runtime_error(
							"PA15 constructor argument range is invalid");
				if (action.constructor.valid() && action.initializer.valid())
					throw std::runtime_error(
						"PA15 constructor action has multiple operations");
				if (!action.constructor.valid() && !action.initializer.valid())
					throw std::runtime_error(
						"PA15 constructor action has no operation");
				if (action.constructor.valid() && action.constructor.value >=
					model_.bindings_.size())
					throw std::runtime_error(
						"PA15 constructor action binding is invalid");
				if (action.initializer.valid() && action.initializer.value >=
					model_.semantic_facts_.size())
					throw std::runtime_error(
						"PA15 constructor action initializer is invalid");
				if (action.constructor.valid())
				{
					NamedRecordId target_record;
					if (action.target == ConstructorActionTarget::Base)
					{
						if (!action.base_record.valid() ||
							action.base_record.value >= model_.named_.size())
							throw std::runtime_error(
								"PA15 constructor action base identity is invalid");
						target_record = action.base_record;
					}
					else if (action.target == ConstructorActionTarget::Member)
					{
						if (!action.member.valid() || action.member.value >=
							model_.bindings_.size())
							throw std::runtime_error(
								"PA15 constructor action member identity is invalid");
						target_record = model_.class_record_for_object_type(
							model_.binding(action.member).type);
					}
					else
						throw std::runtime_error(
							"PA15 constructor action target is invalid");
					const FunctionFactId* target_id =
						model_.function_binding_fact_index_.find(action.constructor);
					if (target_id == NULL || !target_id->valid() ||
						target_id->value >= model_.function_facts_.size() ||
						!model_.function_facts_[target_id->value].is_constructor ||
						model_.function_facts_[target_id->value].binding !=
							action.constructor)
						throw std::runtime_error(
							"PA15 constructor action target is missing");
					const FunctionFact& target_function =
						model_.function_facts_[target_id->value];
					const Binding& target_binding = model_.binding(action.constructor);
					const bool external_declaration =
						target_record.valid() && target_record.value < model_.named_.size() &&
						model_.named_[target_record.value].kind == NamedKind::Class &&
						model_.named_[target_record.value].class_tag != ClassTag::Union &&
						model_.named_[target_record.value].name.valid() &&
						!target_function.body_fact.valid() &&
						!target_function.synthetic && !target_binding.has_definition;
					bool no_op = false;
					if (target_record.valid() && target_record.value <
						model_.named_.size() && model_.named_[target_record.value].kind ==
						NamedKind::Class && model_.named_[target_record.value].class_tag !=
						ClassTag::Union && model_.named_[target_record.value].name.valid())
					{
						if (!external_declaration)
						{
							const FunctionFact& checked =
								checked_constructor_function(action.constructor, target_record);
							no_op = checked.synthetic &&
								checked.constructor_action_count == 0;
						}
					}
					if (external_declaration)
					{
						if (!action.callable_type.valid() ||
							model_.type_kind(action.callable_type) != TypeKind::Function)
							throw std::runtime_error(
								"PA15 constructor action declaration signature is missing");
						if ((*declaration_types)[action.constructor.value].valid() &&
							(*declaration_types)[action.constructor.value] !=
							action.callable_type)
							throw std::runtime_error(
								"PA15 constructor action declaration signature changed");
						(*declarations)[action.constructor.value] = 1;
						(*declaration_types)[action.constructor.value] =
							action.callable_type;
					}
					else if (!no_op && !(*demanded)[target_id->value])
					{
						(*demanded)[target_id->value] = 1;
						function_work.push_back(*target_id);
					}
				}
				if (action.initializer.valid())
					fact_work.push_back(action.initializer);
				if (action.argument_begin != InvalidIdentityValue)
					for (std::size_t argument = 0;
						argument < action.argument_count; ++argument)
						fact_work.push_back(model_.constructor_arguments_[
							action.argument_begin + argument]);
			}
		}
		if (function.body_fact.valid())
			fact_work.push_back(function.body_fact);
		if (fact_work.empty() && !function.is_destructor)
			continue;
		while (!fact_work.empty())
		{
			const SemanticFactId fact_id = fact_work.back();
			fact_work.pop_back();
			if (!fact_id.valid() || fact_id.value >= model_.semantic_facts_.size())
				throw std::runtime_error("PA15 member demand fact is invalid");
			if (scanned_runtime_facts[fact_id.value] != 0)
				continue;
			scanned_runtime_facts[fact_id.value] = 1;
			const SemanticFact& fact = model_.semantic_facts_[fact_id.value];
			validate_fact_base_path(fact);
			if (model_.aggregate_ranges_.find(fact_id) != NULL)
			{
				std::size_t element_count = 0;
				std::size_t total_count = 0;
				const AggregateElementFact* elements = aggregate_elements(fact_id,
					&element_count, &total_count);
				(void) total_count;
				for (std::size_t element = 0; element < element_count; ++element)
					fact_work.push_back(elements[element].initializer);
			}
			if (fact.child_count != 0 &&
				(fact.child_begin == InvalidIdentityValue ||
					fact.child_begin > model_.semantic_children_.size() ||
					fact.child_count > model_.semantic_children_.size() -
						fact.child_begin))
				throw std::runtime_error("PA15 member demand child range is invalid");
			if (fact.child_count == 0 && fact.child_begin != InvalidIdentityValue &&
				fact.child_begin > model_.semantic_children_.size())
				throw std::runtime_error("PA15 member demand child range is invalid");
			if (fact.kind == SemanticFactKind::ConstructorAction &&
				fact.has_callee)
				demand_constructor_fact(fact, false);
			if (fact.kind == SemanticFactKind::DestructorCall && fact.has_callee)
				demand_destructor_fact(fact);
			if (fact.kind == SemanticFactKind::CallExpression &&
				fact.has_implicit_object)
			{
				if (!fact.has_callee || !fact.selected_binding.valid() ||
					fact.selected_binding.value >= model_.bindings_.size())
					throw std::runtime_error("PA15 member call demand is incomplete");
				const FunctionFactId* target_id =
					model_.function_binding_fact_index_.find(fact.selected_binding);
				if (fact.selected_binding.value >= class_binding_owners.size() ||
					!fact.selected_scope.valid() ||
					fact.selected_scope.value >= model_.scopes_.size() ||
					model_.scopes_[fact.selected_scope.value].kind != ScopeKind::Class ||
					class_binding_owners[fact.selected_binding.value] !=
					fact.selected_scope)
					throw std::runtime_error(
						"PA15 member call owner binding is invalid");
				if (target_id != NULL)
				{
					if (!target_id->valid() || target_id->value >=
						model_.function_facts_.size())
						throw std::runtime_error("PA15 member demand function is invalid");
					const FunctionFact& target =
						model_.function_facts_[target_id->value];
					if (!fact.selected_scope.valid() ||
						fact.selected_scope.value >= model_.scopes_.size() ||
						model_.scopes_[fact.selected_scope.value].kind !=
						ScopeKind::Class || !target.owner.valid() ||
						target.owner != fact.selected_scope || target.owner.value >=
						model_.scopes_.size() || !target.function_scope.valid() ||
						target.function_scope.value >= model_.scopes_.size() ||
						!target.body_fact.valid())
						throw std::runtime_error(
							"PA15 member demand definition is invalid");
					if (!(*demanded)[target_id->value])
					{
						(*demanded)[target_id->value] = 1;
						function_work.push_back(*target_id);
					}
				}
				else
				{
					const Binding& target = model_.binding(fact.selected_binding);
					if (!fact.selected_scope.valid() ||
						fact.selected_scope.value >= model_.scopes_.size() ||
						model_.scopes_[fact.selected_scope.value].kind !=
						ScopeKind::Class || target.kind != BindingKind::Function ||
						model_.type_kind(target.type) != TypeKind::Function ||
						model_.is_static_member(fact.selected_binding))
						throw std::runtime_error(
							"PA15 member declaration demand is invalid");
					if (!fact.callable_type.valid() ||
						model_.type_kind(fact.callable_type) != TypeKind::Function)
						throw std::runtime_error(
							"PA15 member declaration signature is missing");
					if ((*declaration_types)[fact.selected_binding.value].valid() &&
						(*declaration_types)[fact.selected_binding.value] !=
						fact.callable_type)
						throw std::runtime_error(
							"PA15 member declaration signature changed");
					(*declarations)[fact.selected_binding.value] = 1;
					(*declaration_types)[fact.selected_binding.value] =
						fact.callable_type;
				}
			}
			if (fact.kind == SemanticFactKind::CallExpression &&
				fact.has_callee && !fact.has_implicit_object &&
				fact.selected_binding.valid() && fact.selected_binding.value <
				model_.bindings_.size())
			{
				const Binding& target = model_.binding(fact.selected_binding);
				const FunctionFactId* target_id =
					model_.function_binding_fact_index_.find(
						fact.selected_binding);
				const BindingSidecar* sidecar = model_.binding_sidecar(
					fact.selected_binding);
				const bool named_constructor = sidecar != NULL &&
					sidecar->constructor_record.valid() &&
					sidecar->constructor_record.value < model_.named_.size() &&
					model_.named_[sidecar->constructor_record.value].name.valid();
				if (named_constructor)
				{
					if (target.kind != BindingKind::Function ||
						model_.type_kind(target.type) != TypeKind::Function ||
						target_id == NULL || !target_id->valid() ||
						target_id->value >= model_.function_facts_.size() ||
						!model_.function_facts_[target_id->value].is_constructor ||
						!fact.callable_type.valid() ||
						model_.type_kind(fact.callable_type) != TypeKind::Function)
						throw std::runtime_error(
							"PA15 constructor demand is not typed");
					const FunctionFact& target_function =
						model_.function_facts_[target_id->value];
					const bool external_declaration =
						model_.named_[sidecar->constructor_record.value].kind ==
							NamedKind::Class &&
						model_.named_[sidecar->constructor_record.value].class_tag !=
							ClassTag::Union &&
						!target_function.body_fact.valid() &&
						!target_function.synthetic && !target.has_definition;
					bool no_op = false;
					const BindingSidecar* constructor_sidecar =
						model_.binding_sidecar(fact.selected_binding);
					if (constructor_sidecar != NULL &&
						constructor_sidecar->constructor_record.valid() &&
						constructor_sidecar->constructor_record.value <
						model_.named_.size())
					{
						const NamedRecord& constructor_record = model_.named_[
							constructor_sidecar->constructor_record.value];
						if (constructor_record.kind == NamedKind::Class &&
							constructor_record.class_tag != ClassTag::Union &&
							constructor_record.name.valid())
						{
							if (!external_declaration)
							{
								const FunctionFact& checked =
									checked_constructor_function(fact.selected_binding,
										constructor_sidecar->constructor_record);
								const RecordLayout& target_layout = model_.record_layout(
									constructor_sidecar->constructor_record);
								const bool value_initialized_aggregate = fact.value_initialize &&
									target_layout.state == RecordLayoutState::Complete &&
									!target_layout.members.empty();
								no_op = checked.synthetic &&
									checked.constructor_action_count == 0 &&
										(!fact.temporary_object || value_initialized_aggregate);
							}
						}
					}
					if (external_declaration)
					{
						if ((*declaration_types)[fact.selected_binding.value].valid() &&
							(*declaration_types)[fact.selected_binding.value] !=
							fact.callable_type)
							throw std::runtime_error(
								"PA15 constructor declaration signature changed");
						(*declarations)[fact.selected_binding.value] = 1;
						(*declaration_types)[fact.selected_binding.value] =
							fact.callable_type;
					}
					else if (!no_op && !(*demanded)[target_id->value])
					{
						(*demanded)[target_id->value] = 1;
						function_work.push_back(*target_id);
					}
					if (!external_declaration && !no_op)
						demand_special_member_base_entry(
							fact.selected_binding, true);
				}
				const bool static_target = target.kind == BindingKind::Function &&
					model_.type_kind(target.type) == TypeKind::Function &&
					model_.is_static_member(fact.selected_binding);
				if (static_target)
				{
					ScopeId target_owner = fact.selected_scope;
					const FunctionFact* target_function = NULL;
					if (target_id != NULL)
					{
						if (!target_id->valid() || target_id->value >=
							model_.function_facts_.size())
							throw std::runtime_error(
								"PA15 static member demand function is invalid");
						target_function = &model_.function_facts_[target_id->value];
						if (!target_function->binding.valid() ||
							target_function->binding != fact.selected_binding ||
							!target_function->owner.valid())
							throw std::runtime_error(
								"PA15 static member demand identity is invalid");
						target_owner = target_function->owner;
					}
					if (!fact.selected_scope.valid() ||
						fact.selected_scope.value >= model_.scopes_.size() ||
						fact.selected_scope != target_owner ||
						model_.scopes_[fact.selected_scope.value].kind !=
						ScopeKind::Class || !fact.callable_type.valid() ||
						model_.type_kind(fact.callable_type) != TypeKind::Function ||
						fact.callable_type != target.type || target_owner.value >=
						model_.scopes_.size() ||
						!model_.scopes_[target_owner.value].record.valid() ||
						model_.scopes_[target_owner.value].record.value >=
						model_.named_.size() ||
						model_.named_[model_.scopes_[target_owner.value].record.value].kind !=
						NamedKind::Class ||
						model_.named_[model_.scopes_[target_owner.value].record.value].scope !=
						target_owner)
						throw std::runtime_error(
							"PA15 static member demand is not typed");
					if (fact.selected_binding.value >= class_binding_owners.size() ||
						class_binding_owners[fact.selected_binding.value] != target_owner)
						throw std::runtime_error(
							"PA15 static member owner binding is invalid");
					if (target_function != NULL)
					{
						if (target_function->body_fact.valid())
						{
							if (!target_function->function_scope.valid() ||
								target_function->function_scope.value >=
								model_.scopes_.size() ||
								model_.scopes_[target_function->function_scope.value].kind !=
								ScopeKind::Function || !target_function->body_scope.valid() ||
								target_function->body_scope.value >=
								model_.scopes_.size() ||
								model_.scopes_[target_function->body_scope.value].kind !=
								ScopeKind::Block || target_function->body_fact.value >=
								model_.semantic_facts_.size() || !target.has_definition)
								throw std::runtime_error(
									"PA15 static member definition is invalid");
							if (!(*demanded)[target_id->value])
							{
								(*demanded)[target_id->value] = 1;
								function_work.push_back(*target_id);
							}
						}
						else if (target_function->function_scope.valid() ||
							target_function->body_scope.valid() || target.has_definition)
							throw std::runtime_error(
								"PA15 static member declaration is invalid");
					}
					else if (target.has_definition)
						throw std::runtime_error(
							"PA15 static member definition fact is missing");
					if (target_function == NULL || !target_function->body_fact.valid())
					{
						if (fact.selected_binding.value >= declarations->size())
							throw std::runtime_error(
								"PA15 static member declaration is invalid");
						if ((*declaration_types)[fact.selected_binding.value].valid() &&
							(*declaration_types)[fact.selected_binding.value] !=
							fact.callable_type)
							throw std::runtime_error(
								"PA15 static member declaration signature changed");
						(*declarations)[fact.selected_binding.value] = 1;
						(*declaration_types)[fact.selected_binding.value] =
							fact.callable_type;
					}
				}
			}
			if (fact.child_count == 0)
				continue;
			if (fact.child_begin == InvalidIdentityValue ||
				fact.child_count > model_.semantic_children_.size() ||
				fact.child_begin > model_.semantic_children_.size() -
					fact.child_count)
				throw std::runtime_error("PA15 member demand child range is invalid");
			for (std::size_t child = 0; child < fact.child_count; ++child)
				fact_work.push_back(model_.semantic_children_[
					fact.child_begin + child]);
		}
		if (function.is_destructor)
		{
			if (function.destructor_action_begin == InvalidIdentityValue ||
				function.destructor_action_begin > model_.destructor_actions_.size() ||
				function.destructor_action_count > model_.destructor_actions_.size() -
					function.destructor_action_begin)
				throw std::runtime_error("PA15 destructor action range is invalid");
			for (std::size_t action_index = 0;
				action_index < function.destructor_action_count; ++action_index)
			{
				const DestructorActionFact& action =
					model_.destructor_actions_[function.destructor_action_begin +
						action_index];
				if (!action.destructor.valid() || action.destructor.value >=
					model_.bindings_.size())
					throw std::runtime_error("PA15 destructor action binding is invalid");
				const FunctionFactId* target_id =
					model_.function_binding_fact_index_.find(action.destructor);
				if (target_id == NULL || !target_id->valid() ||
					target_id->value >= model_.function_facts_.size() ||
					!model_.function_facts_[target_id->value].is_destructor ||
					model_.function_facts_[target_id->value].binding != action.destructor)
					throw std::runtime_error("PA15 destructor action target is missing");
				if (!(*demanded)[target_id->value])
				{
					(*demanded)[target_id->value] = 1;
					function_work.push_back(*target_id);
				}
			}
		}
	}
}

void Pa15Lowerer::collect_function_declarations(){
	for (std::size_t scope_index = 0; scope_index < model_.scopes_.size();
		++scope_index)
	{
		const Scope& scope = model_.scopes_[scope_index];
		const bool namespace_scope = scope.kind == ScopeKind::Namespace;
		const bool member_scope = scope.kind == ScopeKind::Class;
		if (!namespace_scope && !member_scope) continue;
		for (std::size_t i = 0; i < scope.bindings.size(); ++i)
		{
			const BindingId binding_id = scope.bindings[i];
			const Binding& binding = model_.binding(binding_id);
			const FunctionFact* function_fact =
				model_.function_fact_for_binding(binding_id);
			if (binding.kind != BindingKind::Function ||
				model_.type_kind(binding.type) != TypeKind::Function ||
				function_symbols_.find(binding_id.value) != function_symbols_.end())
				continue;
			if (member_scope && (binding_id.value >=
				demanded_member_declarations_.size() ||
				demanded_member_declarations_[binding_id.value] == 0))
				continue;
			const ScopeId owner(scope_index);
			const TypeKey& type = model_.types_[binding.type.value];
			TypeId member_callable_type;
			if (member_scope && !model_.is_static_member(binding_id))
			{
				if (binding_id.value >= demanded_member_declaration_types_.size())
					throw std::runtime_error(
						"PA15 member declaration signature is missing");
				member_callable_type =
					demanded_member_declaration_types_[binding_id.value];
				if (!member_callable_type.valid() ||
					model_.type_kind(member_callable_type) != TypeKind::Function)
					throw std::runtime_error(
						"PA15 member declaration signature is missing");
				const TypeKey& callable = model_.types_[
					member_callable_type.value];
				if (callable.cv != 0 || callable.variadic != type.variadic ||
					callable.result != type.result || callable.parameters.size() !=
					type.parameters.size() + 1)
					throw std::runtime_error(
						"PA15 member declaration boundary is invalid");
				for (std::size_t parameter = 0;
					parameter < type.parameters.size(); ++parameter)
					if (callable.parameters[parameter + 1] !=
						type.parameters[parameter])
						throw std::runtime_error(
							"PA15 member declaration parameter is invalid");
			}
			else if (member_scope)
			{
				if (binding_id.value >= demanded_member_declaration_types_.size())
					throw std::runtime_error(
						"PA15 static member declaration signature is missing");
				member_callable_type =
					demanded_member_declaration_types_[binding_id.value];
				if (!member_callable_type.valid() || member_callable_type !=
					binding.type)
					throw std::runtime_error(
						"PA15 static member declaration boundary is invalid");
			}
			FunctionDeclaration declaration;
			declaration.symbol_id = SymbolId(next_symbol_++);
			std::string declaration_name = internal_value_name(owner, binding.name);
			const bool base_entry = function_fact != NULL &&
				(function_fact->constructor_base_entry ||
					function_fact->destructor_base_entry);
			if (base_entry)
				declaration_name += "__base_entry";
			const bool has_definition = binding.has_definition;
			const bool hidden_parameter_uses_arg = member_scope &&
				!model_.is_static_member(binding_id) &&
				(base_entry || !has_definition);
			declaration.name_id = symbol_spelling(declaration_name);
			declaration.return_type = function_result_low_type(type.result);
			const BindingSidecar* sidecar = model_.binding_sidecar(binding_id);
			if (sidecar != NULL && sidecar->nonthrowing)
				declaration.boundary.unwind = lowir_model::CUM_NO;
			declaration.metadata.binding = binding.internal_linkage ?
				lowir_model::SBM_INTERNAL : lowir_model::SBM_STRONG;
			if (binding.language_linkage != LanguageLinkage::C ||
				binding.internal_linkage)
			{
				const std::string object_symbol = base_entry ? abi_symbol(*function_fact,
					function_fact->constructor_base_entry ?
						abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_BASE :
						abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_BASE) :
					abi_function_symbol(binding_id, owner);
				declaration.metadata.object_symbol_id = intern_spelling(object_symbol);
			}
			if (binding.language_linkage == LanguageLinkage::C)
				declaration.metadata.linkage = lowir_model::LLM_C;
			declaration.boundary.arity = type.variadic ?
				lowir_model::CAM_VARIADIC : lowir_model::CAM_FIXED;
			if (member_scope && !model_.is_static_member(binding_id))
			{
				Parameter parameter_record;
				// Definitions expose the established hidden `this` spelling, while
				// an externally declared member has no body-owned frame name and
				// therefore uses the ordinary declaration parameter convention.
				parameter_record.name_id = intern_spelling(
					hidden_parameter_uses_arg ? "%arg0" : "%this");
				parameter_record.type = low_type(model_.types_[
					member_callable_type.value].parameters.front());
				declaration.params.push_back(parameter_record);
			}
			for (std::size_t parameter = 0; parameter < type.parameters.size();
				++parameter)
			{
				Parameter parameter_record;
				std::ostringstream parameter_name;
				parameter_name << "%arg" << parameter +
					(hidden_parameter_uses_arg ? 1 : 0);
				parameter_record.name_id = intern_spelling(parameter_name.str());
				parameter_record.type = low_type(type.parameters[parameter]);
				const TypeKind parameter_kind = model_.type_kind(
					model_.strip_cv_type(type.parameters[parameter]));
				if (parameter_kind == TypeKind::LvalueReference ||
					parameter_kind == TypeKind::RvalueReference)
					parameter_record.metadata.passing = lowir_model::PPM_REFERENCE;
				declaration.params.push_back(parameter_record);
			}
			function_declaration_plans_[binding_id.value] = declaration;
			// Member declaration demand is established before this pass creates
			// the plan.  Carry that typed demand into the common materializer so
			// declarations reached through constructor actions are emitted too.
			if (member_scope)
				demanded_function_declarations_.insert(binding_id.value);
			function_symbols_[binding_id.value] = declaration.symbol_id;
			function_name_ids_[binding_id.value] = declaration.name_id;
			symbol_name_ids_[declaration.symbol_id.index] = declaration.name_id;
		}
	}
	// Compiler-provided functions are intentionally outside ordinary lexical
	// scope bindings.  Their typed PA12 facts are the sole source for this
	// declaration boundary; the vector is populated only when a builtin is
	// semantically selected, so unused helpers do not acquire symbols here.
	for (std::size_t i = 0; i < model_.builtin_function_facts_.size(); ++i)
	{
		const BuiltinFunctionFact& builtin =
			model_.builtin_function_facts_[i];
		if (!builtin.binding.valid() || builtin.binding.value >=
			model_.bindings_.size() || builtin.binding.value >=
			model_.binding_owners_.size() || !builtin.object_symbol.valid())
			throw std::runtime_error("PA15 builtin declaration identity is invalid");
		const Binding& binding = model_.binding(builtin.binding);
		if (binding.kind != BindingKind::Function ||
			model_.type_kind(binding.type) != TypeKind::Function)
			throw std::runtime_error("PA15 builtin declaration type is invalid");
		if (function_declaration_plans_.find(builtin.binding.value) !=
			function_declaration_plans_.end())
			throw std::runtime_error("PA15 builtin declaration is duplicated");
		const TypeKey& type = model_.types_[binding.type.value];
		if (type.variadic || builtin.parameters.size() != type.parameters.size())
			throw std::runtime_error("PA15 builtin declaration boundary is invalid");
		FunctionDeclaration declaration;
		declaration.symbol_id = SymbolId(next_symbol_++);
		declaration.name_id = symbol_spelling(internal_value_name(
			model_.binding_owners_[builtin.binding.value], binding.name));
		declaration.return_type = function_result_low_type(type.result);
		declaration.boundary.arity = lowir_model::CAM_FIXED;
		declaration.boundary.effects = builtin_effects_mode(builtin.effects);
		declaration.boundary.unwind = builtin_unwind_mode(builtin.unwind);
		declaration.boundary.returns = builtin_return_mode(builtin.returns);
		declaration.metadata.binding = lowir_model::SBM_STRONG;
		declaration.metadata.object_symbol_id = intern_spelling(
			model_.name_text(builtin.object_symbol));
		for (std::size_t parameter = 0; parameter < type.parameters.size();
			++parameter)
		{
			const BuiltinParameterFact& parameter_fact =
				builtin.parameters[parameter];
			Parameter parameter_record;
			std::ostringstream parameter_name;
			parameter_name << "%arg" << parameter;
			parameter_record.name_id = intern_spelling(parameter_name.str());
			parameter_record.type = low_type(type.parameters[parameter]);
			parameter_record.metadata.capture = builtin_capture_mode(
				parameter_fact.capture);
			parameter_record.metadata.access = builtin_access_mode(
				parameter_fact.access);
			parameter_record.metadata.alias = builtin_alias_mode(
				parameter_fact.alias);
			declaration.params.push_back(parameter_record);
		}
		function_declaration_plans_[builtin.binding.value] = declaration;
		function_symbols_[builtin.binding.value] = declaration.symbol_id;
		function_name_ids_[builtin.binding.value] = declaration.name_id;
		symbol_name_ids_[declaration.symbol_id.index] = declaration.name_id;
	}
}

LoweredValue Pa15Lowerer::lower_call(SemanticFactId id)
{
	const SemanticFact& fact = model_.semantic_facts_[id.value];
	const std::vector<SemanticFactId> facts = children(id);
	const Binding* callee_binding = NULL;
	const TypeKey* function_type = NULL;
	std::size_t argument_begin = 0;
	bool constructor_call = false;
	bool class_value_constructor_argument = false;
	bool class_value_source_has_declaration_address = false;
	LoweredValue class_value_source;
	LoweredValue class_value_temporary;
	Instruction instruction;
	instruction.kind = Instruction::IK_CALL;
	if (!fact.callable_type.valid() ||
		model_.type_kind(fact.callable_type) != TypeKind::Function)
		throw std::runtime_error("PA15 call has no typed callable signature");
	function_type = &model_.types_[fact.callable_type.value];
	if (fact.has_implicit_object && !fact.has_callee)
		throw std::runtime_error("PA15 member call is not direct");
	if (fact.has_callee)
	{
		if (!fact.selected_binding.valid())
			throw std::runtime_error("PA15 direct call has no selected binding");
		std::map<std::size_t, SymbolId>::const_iterator symbol =
			function_symbols_.find(fact.selected_binding.value);
		if (symbol == function_symbols_.end())
			throw std::runtime_error("PA15 direct call target was not emitted");
		demand_function_declaration(fact.selected_binding);
		callee_binding = &model_.binding(fact.selected_binding);
		if (model_.type_kind(callee_binding->type) != TypeKind::Function)
			throw std::runtime_error("PA15 direct call target is not a function");
		const FunctionFact* selected_function =
			model_.function_fact_for_binding(fact.selected_binding);
		if (!function_abi_supported(fact.selected_binding, selected_function,
			callee_binding->type))
			throw std::runtime_error("PA15 unsupported class-value function ABI");
		instruction.direct_callee_id = symbol->second;
		instruction.first = global_operand(symbol->second,
			function_name_ids_.find(fact.selected_binding.value)->second);
		const BindingSidecar* constructor_sidecar = model_.binding_sidecar(
			fact.selected_binding);
		constructor_call = constructor_sidecar != NULL &&
			constructor_sidecar->constructor_record.valid() &&
			constructor_sidecar->constructor_record.value < model_.named_.size() &&
			model_.named_[constructor_sidecar->constructor_record.value].name.valid();
		if (constructor_call)
		{
			if (fact.has_implicit_object || facts.empty())
				throw std::runtime_error("PA15 constructor call object is missing");
			// Constructor semantic children own the hidden destination first;
			// every remaining child is the already-converted/defaulted typed
			// constructor argument sequence.
			argument_begin = 1;
			std::size_t class_value_count = 0;
			TypeId class_value_target;
			for (std::size_t i = argument_begin; i < facts.size(); ++i)
			{
				const SemanticFact& argument = model_.semantic_facts_[
					facts[i].value];
				if (argument.conversion_count == 0)
					continue;
				if (argument.conversion_begin == InvalidIdentityValue ||
					argument.conversion_begin > model_.conversion_facts_.size() ||
					argument.conversion_count > model_.conversion_facts_.size() -
						argument.conversion_begin)
					throw std::runtime_error(
						"PA15 constructor argument conversion range is invalid");
				for (std::size_t conversion = 0;
					conversion < argument.conversion_count; ++conversion)
				{
					const ConversionFact& selected = model_.conversion_facts_[
						argument.conversion_begin + conversion];
					if (selected.kind != ConversionKind::ClassValue)
						continue;
					++class_value_count;
					class_value_target = selected.target;
				}
			}
			if (class_value_count != 0)
			{
				if (class_value_count != 1 || facts.size() - argument_begin != 1 ||
					!class_value_target.valid())
					throw std::runtime_error(
						"PA15 unsupported class-value constructor argument shape");
				const FunctionFact* constructor_fact =
					model_.function_fact_for_binding(fact.selected_binding);
				if (constructor_fact == NULL || !constructor_fact->is_constructor ||
					!model_.narrow_class_value_constructor(*constructor_fact) ||
					!model_.binding(fact.selected_binding).type.valid() ||
					model_.binding(fact.selected_binding).type.value >=
						model_.types_.size() ||
					model_.type_kind(model_.binding(fact.selected_binding).type) !=
						TypeKind::Function ||
					model_.types_[model_.binding(fact.selected_binding).type.value].parameters.front() !=
						class_value_target ||
					!model_.empty_class_value_type(class_value_target))
					throw std::runtime_error(
						"PA15 class-value constructor boundary is invalid");
				const SemanticFact& class_value_argument =
					model_.semantic_facts_[facts[argument_begin].value];
				if (class_value_argument.kind == SemanticFactKind::IdExpression &&
					class_value_argument.binding.valid())
				{
					const std::map<std::size_t, SemanticFactId>::const_iterator
						variable = variable_facts_.find(
							class_value_argument.binding.value);
					if (variable != variable_facts_.end())
					{
						const std::vector<SemanticFactId> declaration_initializers =
							children(variable->second);
						if (declaration_initializers.size() == 1)
						{
							const SemanticFact& declaration_initializer =
								model_.semantic_facts_[declaration_initializers.front().value];
							class_value_source_has_declaration_address =
								automatic_local_declaration(
									class_value_argument.binding) &&
								declaration_initializer.kind ==
									SemanticFactKind::ConstructorAction &&
								storage_for(class_value_argument.binding).type.is_object() &&
								class_object_type(model_.binding(
									class_value_argument.binding).type) &&
								constructor_action_is_noop(declaration_initializer);
						}
					}
				}
				class_value_source = lower_expression_impl(facts[argument_begin],
					false, false, false, true);
				if (!class_value_source.lvalue || !class_value_source.type.is_object())
					throw std::runtime_error(
						"PA15 class-value constructor source is not an object lvalue");
				// A no-op local class declaration has already materialized its
				// declaration-owned address.  The class-value ABI path retains its
				// later source-address operation, but must not duplicate the
				// pre-copy address solely because declaration lowering now owns it.
				if (!class_value_source_has_declaration_address)
					(void)address_of_storage(class_value_source);
				class_value_constructor_argument = true;
			}
			instruction.args.push_back(lower_expression(facts.front()).value);
			if (class_value_constructor_argument)
			{
				class_value_temporary = generated_slot(low_type(class_value_target),
					"argobj");
				(void)address_of_storage(class_value_temporary);
				(void)address_of_storage(class_value_source);
			}
		}
		else if (fact.has_implicit_object)
		{
			if (facts.empty() || (fact.token != SimpleTokenType::OP_DOT &&
				fact.token != SimpleTokenType::OP_ARROW))
				throw std::runtime_error("PA15 member call object is missing");
			const FunctionFact* member_fact =
				model_.function_fact_for_binding(fact.selected_binding);
			ScopeId member_owner = member_fact == NULL ? fact.selected_scope :
				member_fact->owner;
			if (!member_owner.valid() || member_owner.value >=
				model_.scopes_.size() || model_.scopes_[member_owner.value].kind !=
				ScopeKind::Class || (member_fact != NULL &&
				fact.selected_scope != member_owner))
				throw std::runtime_error("PA15 member call owner is invalid");
			if (member_fact == NULL && function_declaration_plans_.find(
				fact.selected_binding.value) == function_declaration_plans_.end())
				throw std::runtime_error(
					"PA15 member declaration target was not planned");
			const Binding& member = model_.binding(fact.selected_binding);
			if (member.kind != BindingKind::Function ||
				model_.type_kind(member.type) != TypeKind::Function ||
				model_.is_static_member(fact.selected_binding))
				throw std::runtime_error("PA15 member call target is not an ordinary method");
			const TypeKey& member_signature = model_.types_[member.type.value];
			if (function_type->cv != 0 || function_type->variadic !=
				member_signature.variadic || function_type->result !=
				member_signature.result || function_type->parameters.size() !=
				member_signature.parameters.size() + 1)
				throw std::runtime_error("PA15 member call signature mismatch");
			for (std::size_t parameter = 0;
				parameter < member_signature.parameters.size(); ++parameter)
				if (function_type->parameters[parameter + 1] !=
					member_signature.parameters[parameter])
					throw std::runtime_error("PA15 member call parameter mismatch");
			const TypeId hidden_pointer = model_.strip_cv_type(
				model_.expression_object_type(function_type->parameters.front()));
			if (model_.type_kind(hidden_pointer) != TypeKind::Pointer)
				throw std::runtime_error("PA15 member call hidden object is not a pointer");
			const TypeId required_object = model_.types_[hidden_pointer.value].child;
			if (model_.class_scope_for_type(model_.strip_cv_type(required_object)) !=
				member_owner || model_.cv_qualifiers(required_object) !=
				member_signature.cv)
				throw std::runtime_error("PA15 member call hidden object mismatch");
			if (member_fact != NULL)
			{
				if (!member_fact->function_scope.valid() ||
					member_fact->function_scope.value >= model_.scopes_.size())
					throw std::runtime_error(
						"PA15 member call function scope is missing");
				const Scope& function_scope = model_.scopes_[
					member_fact->function_scope.value];
				const BindingId hidden_this = function_scope.implicit_object_binding;
				if (!hidden_this.valid() || model_.binding(hidden_this).kind !=
					BindingKind::Parameter || model_.binding(hidden_this).type !=
					function_type->parameters.front())
					throw std::runtime_error(
						"PA15 member call hidden object mismatch");
			}
			const SemanticFact& object_fact = model_.semantic_facts_[
				facts.front().value];
			TypeId actual_object;
			LoweredValue object;
			if (fact.token == SimpleTokenType::OP_ARROW)
			{
				const TypeId pointer = model_.strip_cv_type(
					model_.expression_object_type(object_fact.type));
				if (model_.type_kind(pointer) != TypeKind::Pointer)
					throw std::runtime_error("PA15 member call arrow object is not a pointer");
				actual_object = model_.types_[pointer.value].child;
				object = lower_expression(facts.front());
			}
			else
			{
				actual_object = model_.expression_object_type(object_fact.type);
				object = lower_address(facts.front());
			}
			actual_object = model_.strip_cv_type(
				model_.expression_object_type(actual_object));
			if (!validate_typed_base_path(actual_object, required_object,
				member_owner, fact.base_path_begin, fact.base_path_count) ||
				!object.type.is_pointer())
				throw std::runtime_error("PA15 member call object is incompatible");
			NamedRecordId current_record = model_.named_record_for_type(
				actual_object);
			for (std::size_t i = 0; i < fact.base_path_count; ++i)
			{
				if (!current_record.valid() || current_record.value >=
					model_.named_.size())
					throw std::runtime_error(
						"PA15 member call base record is invalid");
				const NamedRecordId base_record = model_.semantic_base_paths_[
					fact.base_path_begin + i];
				const LowType offset_type = size_low_type();
				const LoweredValue offset(integer_operand(0, offset_type),
					offset_type, false);
				LowType byte;
				byte.kind = LowType::TYPE_INTEGER;
				byte.integer_kind = LowType::INTEGER_I8;
				object = emit_index(object, offset, byte,
					lowir_model::IPK_BASE_SUBOBJECT);
				current_record = base_record;
			}
			instruction.args.push_back(object.value);
			argument_begin = 1;
		}
	}
	else
	{
		if (facts.empty()) throw std::runtime_error("PA15 indirect call has no callee");
		if (!function_abi_supported(BindingId(), NULL, fact.callable_type))
			throw std::runtime_error("PA15 unsupported class-value function ABI");
		argument_begin = 1;
		instruction.has_call_signature = true;
	}
	const std::size_t explicit_argument_count = facts.size() - argument_begin;
	const std::size_t argument_count = explicit_argument_count +
		(fact.has_implicit_object ? 1 : 0) + (constructor_call ? 1 : 0);
	if (!function_type || (!function_type->variadic &&
		argument_count != function_type->parameters.size()) ||
		(function_type->variadic && argument_count < function_type->parameters.size()))
		throw std::runtime_error("PA15 call arity mismatch");
	instruction.call_return_type = low_type(function_type->result);
	instruction.call_returns_void = instruction.call_return_type.is_void();
	instruction.call_boundary.arity = function_type->variadic ?
		lowir_model::CAM_VARIADIC : lowir_model::CAM_FIXED;
	for (std::size_t i = 0; i < explicit_argument_count; ++i)
	{
		const SemanticFactId argument = facts[argument_begin + i];
		instruction.args.push_back(class_value_constructor_argument && i == 0 ?
			class_value_temporary.value : lower_expression(argument).value);
	}
	if (!fact.has_callee)
		instruction.first = lower_expression(facts.front()).value;
	if (instruction.has_call_signature)
		for (std::size_t i = 0; i < function_type->parameters.size(); ++i)
		{
			Parameter parameter;
			std::ostringstream name;
			name << "%arg" << i;
			parameter.name_id = intern_spelling(name.str());
			parameter.type = low_type(function_type->parameters[i]);
			TypeId parameter_type = function_type->parameters[i];
			while (model_.type_kind(parameter_type) == TypeKind::Cv)
				parameter_type = model_.types_[parameter_type.value].child;
			const TypeKind kind = model_.type_kind(parameter_type);
			if (kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference)
				parameter.metadata.passing = lowir_model::PPM_REFERENCE;
			instruction.call_params.push_back(parameter);
		}
	if (instruction.call_returns_void)
	{
		block().instructions.push_back(instruction);
		return LoweredValue(Operand(), instruction.call_return_type, false);
	}
	const ValueId value = destination(instruction.call_return_type, &instruction);
	block().instructions.push_back(instruction);
	const bool reference_result = model_.type_kind(model_.strip_cv_type(
		function_type->result)) == TypeKind::LvalueReference ||
		model_.type_kind(model_.strip_cv_type(function_type->result)) ==
		TypeKind::RvalueReference;
	if (reference_result)
		return LoweredValue(temporary_operand(value, instruction.destination_name_id),
			low_reference_value_type(function_type->result), true,
			instruction.call_return_type);
	return LoweredValue(temporary_operand(value, instruction.destination_name_id),
		instruction.call_return_type, false);
}

} // namespace pa11_semantic_internal
