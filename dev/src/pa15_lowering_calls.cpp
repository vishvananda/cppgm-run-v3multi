#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

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
		for (std::size_t i = 0; i < scope.bindings.size(); ++i)
		{
			const BindingId binding_id = scope.bindings[i];
			if (!binding_id.valid() || binding_id.value >= model_.bindings_.size() ||
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
	std::vector<unsigned char> scanned_facts(
		model_.semantic_facts_.size(), 0);
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
				if (action.argument_begin != InvalidIdentityValue &&
					(action.argument_begin > model_.constructor_arguments_.size() ||
					action.argument_count > model_.constructor_arguments_.size() -
						action.argument_begin))
					throw std::runtime_error(
						"PA15 constructor argument range is invalid");
				if (action.constructor.valid())
				{
					const FunctionFactId* target_id =
						model_.function_binding_fact_index_.find(action.constructor);
					if (target_id == NULL || !target_id->valid() ||
						target_id->value >= model_.function_facts_.size() ||
						!model_.function_facts_[target_id->value].is_constructor)
						throw std::runtime_error(
							"PA15 constructor action target is missing");
					if (!(*demanded)[target_id->value])
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
		if (fact_work.empty())
			continue;
		while (!fact_work.empty())
		{
			const SemanticFactId fact_id = fact_work.back();
			fact_work.pop_back();
			if (!fact_id.valid() || fact_id.value >= model_.semantic_facts_.size())
				throw std::runtime_error("PA15 member demand fact is invalid");
			if (scanned_facts[fact_id.value] != 0)
				continue;
			scanned_facts[fact_id.value] = 1;
			const SemanticFact& fact = model_.semantic_facts_[fact_id.value];
			if (fact.kind == SemanticFactKind::CallExpression &&
				fact.has_implicit_object)
			{
				if (!fact.has_callee || !fact.selected_binding.valid() ||
					fact.selected_binding.value >= model_.bindings_.size())
					throw std::runtime_error("PA15 member call demand is incomplete");
				const FunctionFactId* target_id =
					model_.function_binding_fact_index_.find(fact.selected_binding);
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
					if (!(*demanded)[target_id->value])
					{
						(*demanded)[target_id->value] = 1;
						function_work.push_back(*target_id);
					}
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
			declaration.name_id = symbol_spelling(internal_value_name(
				owner, binding.name));
			declaration.return_type = function_result_low_type(type.result);
			const BindingSidecar* sidecar = model_.binding_sidecar(binding_id);
			if (sidecar != NULL && sidecar->nonthrowing)
				declaration.boundary.unwind = lowir_model::CUM_NO;
			declaration.metadata.binding = binding.internal_linkage ?
				lowir_model::SBM_INTERNAL : lowir_model::SBM_STRONG;
			if (binding.language_linkage != LanguageLinkage::C ||
				binding.internal_linkage)
				declaration.metadata.object_symbol_id = intern_spelling(
					abi_function_symbol(binding_id, owner));
			if (binding.language_linkage == LanguageLinkage::C)
				declaration.metadata.linkage = lowir_model::LLM_C;
			declaration.boundary.arity = type.variadic ?
				lowir_model::CAM_VARIADIC : lowir_model::CAM_FIXED;
			if (member_scope && !model_.is_static_member(binding_id))
			{
				Parameter parameter_record;
				parameter_record.name_id = intern_spelling("%this");
				parameter_record.type = low_type(model_.types_[
					member_callable_type.value].parameters.front());
				declaration.params.push_back(parameter_record);
			}
			for (std::size_t parameter = 0; parameter < type.parameters.size();
				++parameter)
			{
				Parameter parameter_record;
				std::ostringstream parameter_name;
				parameter_name << "%arg" << parameter;
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
			function_symbols_[binding_id.value] = declaration.symbol_id;
			function_name_ids_[binding_id.value] = declaration.name_id;
			symbol_name_ids_[declaration.symbol_id.index] = declaration.name_id;
		}
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
			instruction.args.push_back(lower_expression(facts.front()).value);
			argument_begin = facts.size();
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
			std::vector<NamedRecordId> base_path;
			if (!model_.member_object_convertible(actual_object,
				required_object, member_owner, &base_path) ||
				!object.type.is_pointer())
				throw std::runtime_error("PA15 member call object is incompatible");
			NamedRecordId current_record = model_.named_record_for_type(
				actual_object);
			for (std::size_t i = 0; i < base_path.size(); ++i)
			{
				if (!current_record.valid() || current_record.value >=
					model_.named_.size())
					throw std::runtime_error(
						"PA15 member call base record is invalid");
				const NamedRecord& current = model_.named_[current_record.value];
				const NamedRecordId base_record = base_path[i];
				if (current.kind != NamedKind::Class || !current.has_base ||
					current.direct_base_virtual || current.direct_base != base_record)
					throw std::runtime_error(
						"PA15 member call base relation is invalid");
				const RecordLayout& layout = model_.record_layout(current_record);
				if (layout.state != RecordLayoutState::Complete ||
					!layout.has_direct_base ||
					layout.direct_base.record != base_record ||
					layout.direct_base.offset != 0)
					throw std::runtime_error(
						"PA15 member call base layout is invalid");
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
		instruction.args.push_back(lower_expression(argument).value);
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
