#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

std::string Pa15Lowerer::abi_tls_wrapper_symbol(BindingId binding_id,
	ScopeId owner) const{
		const Binding& binding = model_.binding(binding_id);
		abi_mangle::AbiFactCase facts;
		abi_mangle::AbiFactRecord record;
		record.kind = abi_mangle::ABI_FACT_RECORD_TARGET;
		record.target.kind = abi_mangle::ABI_TARGET_FACT_THREAD_LOCAL_WRAPPER;
		record.target.linkage = (binding.internal_linkage ||
			binding.language_linkage == LanguageLinkage::Cxx) ?
			abi_mangle::ABI_LINKAGE_CXX : abi_mangle::ABI_LINKAGE_C;
		record.target.name.components = value_components(owner, binding.name);
		facts.records.push_back(record);
		return abi_mangle::mangle_abi_fact_case(facts);
	}

void Pa15Lowerer::index_global_storage_demands(){
		required_global_bindings_.assign(model_.bindings_.size(), 0);
		const std::size_t fact_count = model_.semantic_facts_.size();
		const std::size_t child_count = model_.semantic_children_.size();
		const std::size_t conversion_count = model_.conversion_facts_.size();
		for (std::size_t i = 0; i < fact_count; ++i)
		{
			const SemanticFact& fact = model_.semantic_facts_[i];
			if (fact.child_count != 0 &&
				(fact.child_begin == InvalidIdentityValue ||
				 fact.child_begin > child_count ||
				 fact.child_count > child_count - fact.child_begin))
				throw std::runtime_error("PA15 global demand child range is invalid");
			if (fact.child_count == 0 && fact.child_begin != InvalidIdentityValue &&
				fact.child_begin > child_count)
				throw std::runtime_error("PA15 global demand child range is invalid");
			if (fact.conversion_count != 0 &&
				(fact.conversion_begin == InvalidIdentityValue ||
				 fact.conversion_begin > conversion_count ||
				 fact.conversion_count > conversion_count - fact.conversion_begin))
				throw std::runtime_error(
					"PA15 global demand conversion range is invalid");
			if (fact.conversion_count == 0 &&
				fact.conversion_begin != InvalidIdentityValue &&
				fact.conversion_begin > conversion_count)
				throw std::runtime_error(
					"PA15 global demand conversion range is invalid");
			for (std::size_t j = 0; j < fact.child_count; ++j)
			{
				const SemanticFactId child = model_.semantic_children_[
					fact.child_begin + j];
				if (!child.valid() || child.value >= fact_count)
					throw std::runtime_error("PA15 global demand child is invalid");
			}
		}
		// Validate the typed semantic DAG once.  Shared facts are ordinary DAG
		// nodes, not malformed multiple-parent edges; only a real cycle is bad.
		std::vector<unsigned char> graph_state(fact_count, 0);
		std::vector<std::size_t> graph_stack;
		std::vector<std::size_t> graph_next;
		for (std::size_t root = 0; root < fact_count; ++root)
		{
			if (graph_state[root] != 0)
				continue;
			graph_state[root] = 1;
			graph_stack.push_back(root);
			graph_next.push_back(0);
			while (!graph_stack.empty())
			{
				const std::size_t current = graph_stack.back();
				const SemanticFact& fact = model_.semantic_facts_[current];
				std::size_t& next = graph_next.back();
				if (next == fact.child_count)
				{
					graph_state[current] = 2;
					graph_stack.pop_back();
					graph_next.pop_back();
					continue;
				}
				const SemanticFactId child = model_.semantic_children_[
					fact.child_begin + next++];
				if (graph_state[child.value] == 1)
					throw std::runtime_error("PA15 semantic fact graph has a cycle");
				if (graph_state[child.value] == 0)
				{
					graph_state[child.value] = 1;
					graph_stack.push_back(child.value);
					graph_next.push_back(0);
				}
			}
		}
		auto static_variable = [this](const SemanticFact& fact) -> BindingId {
			BindingId binding_id;
			if (fact.kind == SemanticFactKind::IdExpression)
				binding_id = fact.binding;
			else if (fact.kind == SemanticFactKind::MemberExpression)
				binding_id = fact.selected_binding;
			else
				return BindingId();
			if (!binding_id.valid() || binding_id.value >= model_.bindings_.size())
				throw std::runtime_error("PA15 global demand binding is invalid");
			const Binding& binding = model_.binding(binding_id);
			if (binding.kind != BindingKind::Variable ||
				!model_.is_static_member(binding_id))
				return BindingId();
			return binding_id;
		};
		auto mark_static_demand = [this, &static_variable](
			const SemanticFact& fact) {
			const BindingId binding_id = static_variable(fact);
			if (binding_id.valid())
				required_global_bindings_[binding_id.value] = 1;
		};
		std::vector<unsigned char> transparent_cast_seen(fact_count, 0);
		std::vector<SemanticFactId> transparent_cast_work;
		auto queue_transparent_cast = [&](SemanticFactId cast_id) {
			if (!cast_id.valid() || cast_id.value >= fact_count ||
				model_.semantic_facts_[cast_id.value].kind !=
					SemanticFactKind::CastExpression)
				throw std::runtime_error("PA15 global demand cast is invalid");
			if (transparent_cast_seen[cast_id.value] == 0)
			{
				transparent_cast_seen[cast_id.value] = 1;
				transparent_cast_work.push_back(cast_id);
			}
		};
		for (std::size_t i = 0; i < fact_count; ++i)
		{
			const SemanticFact& fact = model_.semantic_facts_[i];
			const BindingId binding_id = static_variable(fact);
			if (fact.constant_address.valid())
			{
				if (fact.constant_address.value >=
					model_.constant_address_facts_.size())
					throw std::runtime_error(
						"PA15 global demand address fact is invalid");
				const ConstantAddressFact& address =
					model_.constant_address_facts_[fact.constant_address.value];
				if (address.valid && address.kind != ConstantAddressKind::Literal &&
					(!address.target.valid() ||
					 address.target.value >= model_.bindings_.size()))
					throw std::runtime_error(
						"PA15 global demand address target is invalid");
				if (address.valid && address.target.valid() &&
					address.target.value < model_.bindings_.size())
				{
					const Binding& target = model_.binding(address.target);
					if (target.kind == BindingKind::Variable &&
						model_.is_static_member(address.target))
						required_global_bindings_[address.target.value] = 1;
				}
			}
			if (binding_id.valid())
			{
				const Binding& binding = model_.binding(binding_id);
				if (!binding.has_value)
					required_global_bindings_[binding_id.value] = 1;
				if (fact.category == SemanticValueCategory::Lvalue)
				{
					for (std::size_t j = 0; j < fact.conversion_count; ++j)
						if (model_.conversion_facts_[fact.conversion_begin + j].kind ==
							ConversionKind::ReferenceBinding ||
							model_.conversion_facts_[fact.conversion_begin + j].kind ==
							ConversionKind::DerivedToBase)
							required_global_bindings_[binding_id.value] = 1;
				}
			}
			if (fact.kind == SemanticFactKind::CastExpression &&
				fact.category == SemanticValueCategory::Lvalue)
			{
				for (std::size_t j = 0; j < fact.conversion_count; ++j)
					if (model_.conversion_facts_[fact.conversion_begin + j].kind ==
						ConversionKind::ReferenceBinding ||
						model_.conversion_facts_[fact.conversion_begin + j].kind ==
						ConversionKind::DerivedToBase)
					{
						queue_transparent_cast(SemanticFactId(i));
						break;
					}
			}
			if (fact.kind == SemanticFactKind::UnaryExpression &&
				fact.token == SimpleTokenType::OP_AMP)
			{
				if (fact.child_count != 1)
					throw std::runtime_error(
						"PA15 global demand address arity is invalid");
				const SemanticFactId operand = model_.semantic_children_[
					fact.child_begin];
				if (model_.semantic_facts_[operand.value].kind ==
					SemanticFactKind::CastExpression)
					queue_transparent_cast(operand);
				else
					mark_static_demand(model_.semantic_facts_[operand.value]);
			}
		}
		while (!transparent_cast_work.empty())
		{
			const SemanticFactId cast_id = transparent_cast_work.back();
			transparent_cast_work.pop_back();
			const SemanticFact& cast = model_.semantic_facts_[cast_id.value];
			if (cast.child_count != 1)
				throw std::runtime_error(
					"PA15 global demand cast arity is invalid");
			const SemanticFactId operand = model_.semantic_children_[
				cast.child_begin];
			if (model_.semantic_facts_[operand.value].kind ==
				SemanticFactKind::CastExpression)
				queue_transparent_cast(operand);
			else
				mark_static_demand(model_.semantic_facts_[operand.value]);
		}
	}

void Pa15Lowerer::append_tls_wrapper(BindingId binding_id, ScopeId owner,
	SpellingId global_name){
	if (emitted_tls_wrappers_.find(binding_id.value) !=
		emitted_tls_wrappers_.end())
		return;
	const std::map<std::size_t, bool>::const_iterator tls_it =
		thread_local_by_binding_.find(binding_id.value);
	if (tls_it == thread_local_by_binding_.end() || !tls_it->second)
		return;
	if (!global_name.valid() || !global_symbols_.count(binding_id.value))
		throw std::runtime_error("PA15 TLS wrapper target is missing");
	const Binding& binding = model_.binding(binding_id);
	const std::string internal_name = internal_value_name(owner, binding.name);
	FunctionDeclaration wrapper;
	wrapper.symbol_id = SymbolId(next_symbol_++);
	wrapper.name_id = symbol_spelling("__cppgm_tls_wrapper__" + internal_name);
	wrapper.return_type.kind = LowType::TYPE_POINTER;
	wrapper.metadata.binding = binding.internal_linkage ?
		lowir_model::SBM_INTERNAL : lowir_model::SBM_STRONG;
	if (!binding.internal_linkage &&
		binding.language_linkage != LanguageLinkage::C)
		wrapper.metadata.object_symbol_id = intern_spelling(
			abi_tls_wrapper_symbol(binding_id, owner));
	wrapper.metadata.tls_for_name_id = global_name;
	program_.function_declarations.push_back(wrapper);
	symbol_name_ids_[wrapper.symbol_id.index] = wrapper.name_id;
	emitted_tls_wrappers_.insert(binding_id.value);
}

bool Pa15Lowerer::class_object_type(TypeId type) const{
	type = model_.strip_cv_type(model_.expression_object_type(type));
	if (!type.valid() || model_.type_kind(type) != TypeKind::Named)
		return false;
	const NamedRecordId record = model_.types_[type.value].named;
	return record.valid() && record.value < model_.named_.size() &&
		model_.named_[record.value].kind == NamedKind::Class;
}

bool Pa15Lowerer::checkpoint_zero_storage_eligible(TypeId type) const{
	// RecordLayout owns the class summary.  PA15 only unwraps type wrappers;
	// it never walks a class binding scope while collecting globals.
	type = model_.strip_cv_type(type);
	if (!type.valid()) return false;
	const TypeKind kind = model_.type_kind(type);
	if (kind == TypeKind::Cv)
		return checkpoint_zero_storage_eligible(model_.types_[type.value].child);
	if (kind == TypeKind::Fundamental)
		return model_.types_[type.value].fundamental != FundamentalType::Void;
	if (kind == TypeKind::Pointer)
		return true;
	if (kind == TypeKind::Array)
	{
		return !model_.types_[type.value].unknown_bound &&
			checkpoint_zero_storage_eligible(model_.types_[type.value].child);
	}
	if (kind == TypeKind::MemberPointer || kind == TypeKind::LvalueReference ||
		kind == TypeKind::RvalueReference || kind == TypeKind::Function)
		return false;
	if (kind != TypeKind::Named)
		return false;
	const NamedRecordId record_id = model_.types_[type.value].named;
	if (!record_id.valid() || record_id.value >= model_.named_.size())
		return false;
	const NamedRecord& record = model_.named_[record_id.value];
	if (record.kind == NamedKind::Enum)
		return true;
	if (record.kind != NamedKind::Class || record.class_tag == ClassTag::Union ||
		record.has_virtual_member || record.direct_base_virtual)
		return false;
	const RecordLayout& layout = model_.record_layout(record_id);
	return layout.state == RecordLayoutState::Complete &&
		layout.checkpoint_zero_storage_eligible;
}

LowType Pa15Lowerer::function_result_low_type(TypeId type) const{
	const TypeId object = model_.strip_cv_type(type);
	if (object.valid() && model_.type_kind(object) == TypeKind::Named)
	{
		const NamedRecordId record = model_.types_[object.value].named;
		if (record.valid() && record.value < model_.named_.size() &&
			model_.named_[record.value].kind == NamedKind::Class)
		{
			const RecordLayout& layout = model_.record_layout(record);
			if (layout.state == RecordLayoutState::Incomplete)
			{
				// A declaration may mention an incomplete class even though
				// this checkpoint has no object representation for it yet.
				LowType result;
				result.kind = LowType::TYPE_VOID;
				return result;
			}
		}
	}
	return low_type(type);
}

bool Pa15Lowerer::constructor_initializer_is_nothrow(SemanticFactId root)
{
	const std::size_t fact_count = model_.semantic_facts_.size();
	if (!root.valid() || root.value >= fact_count ||
		semantic_nothrow_states_.size() != fact_count ||
		semantic_nothrow_results_.size() != fact_count ||
		semantic_nothrow_invalid_.size() != fact_count)
		return false;
	if (semantic_nothrow_states_[root.value] ==
		ConstructorRuntimeCacheState::Complete)
		return semantic_nothrow_results_[root.value] != 0 &&
			semantic_nothrow_invalid_[root.value] == 0;
	if (semantic_nothrow_states_[root.value] ==
		ConstructorRuntimeCacheState::InProgress)
	{
		semantic_nothrow_states_[root.value] =
			ConstructorRuntimeCacheState::Complete;
		semantic_nothrow_results_[root.value] = 0;
		semantic_nothrow_invalid_[root.value] = 1;
		return false;
	}

	// The enter/finish worklist memoizes every reachable semantic fact.  It is
	// deliberately iterative: a large expression DAG cannot consume the call
	// stack, and an in-progress edge is a conservative cycle boundary.
	std::vector<std::pair<SemanticFactId, unsigned char> > work;
	work.push_back(std::make_pair(root, static_cast<unsigned char>(0)));
	while (!work.empty())
	{
		const SemanticFactId id = work.back().first;
		const unsigned char finish = work.back().second;
		work.pop_back();
		if (!id.valid() || id.value >= fact_count)
			continue;
		if (finish != 0)
		{
			if (semantic_nothrow_states_[id.value] !=
				ConstructorRuntimeCacheState::InProgress)
				continue;
			const SemanticFact& fact = model_.semantic_facts_[id.value];
			bool result = fact.kind != SemanticFactKind::CallExpression &&
				fact.kind != SemanticFactKind::ConstructorAction;
			bool invalid = semantic_nothrow_invalid_[id.value] != 0;
			if (fact.child_count != 0 &&
				(fact.child_begin == InvalidIdentityValue ||
				 fact.child_begin > model_.semantic_children_.size() ||
				 fact.child_count > model_.semantic_children_.size() -
				 fact.child_begin))
			{
				result = false;
				invalid = true;
			}
			else
			{
				for (std::size_t child = 0; child < fact.child_count; ++child)
				{
					const SemanticFactId child_id = model_.semantic_children_[
						fact.child_begin + child];
					if (!child_id.valid() || child_id.value >= fact_count ||
						semantic_nothrow_states_[child_id.value] !=
							ConstructorRuntimeCacheState::Complete ||
						semantic_nothrow_results_[child_id.value] == 0)
						result = false;
					if (!child_id.valid() || child_id.value >= fact_count ||
						semantic_nothrow_invalid_[child_id.value] != 0)
						invalid = true;
				}
			}
			semantic_nothrow_states_[id.value] =
				ConstructorRuntimeCacheState::Complete;
			semantic_nothrow_results_[id.value] = result ? 1 : 0;
			semantic_nothrow_invalid_[id.value] = invalid ? 1 : 0;
			continue;
		}
		if (semantic_nothrow_states_[id.value] ==
			ConstructorRuntimeCacheState::Complete)
			continue;
		if (semantic_nothrow_states_[id.value] ==
			ConstructorRuntimeCacheState::InProgress)
		{
			semantic_nothrow_states_[id.value] =
				ConstructorRuntimeCacheState::Complete;
			semantic_nothrow_results_[id.value] = 0;
			semantic_nothrow_invalid_[id.value] = 1;
			continue;
		}
		semantic_nothrow_states_[id.value] =
			ConstructorRuntimeCacheState::InProgress;
		work.push_back(std::make_pair(id, static_cast<unsigned char>(1)));
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::CallExpression ||
			fact.kind == SemanticFactKind::ConstructorAction)
			continue;
		if (fact.child_count != 0 &&
			(fact.child_begin == InvalidIdentityValue ||
			 fact.child_begin > model_.semantic_children_.size() ||
			 fact.child_count > model_.semantic_children_.size() -
			 fact.child_begin))
		{
			semantic_nothrow_invalid_[id.value] = 1;
			continue;
		}
		for (std::size_t child = fact.child_count; child != 0; --child)
		{
			const SemanticFactId child_id = model_.semantic_children_[
				fact.child_begin + child - 1];
			if (!child_id.valid() || child_id.value >= fact_count)
			{
				semantic_nothrow_invalid_[id.value] = 1;
				continue;
			}
			if (semantic_nothrow_states_[child_id.value] ==
				ConstructorRuntimeCacheState::InProgress)
			{
				semantic_nothrow_states_[child_id.value] =
					ConstructorRuntimeCacheState::Complete;
				semantic_nothrow_results_[child_id.value] = 0;
				semantic_nothrow_invalid_[child_id.value] = 1;
				continue;
			}
			if (semantic_nothrow_states_[child_id.value] !=
				ConstructorRuntimeCacheState::Complete)
				work.push_back(std::make_pair(child_id,
					static_cast<unsigned char>(0)));
		}
	}
	return semantic_nothrow_states_[root.value] ==
		ConstructorRuntimeCacheState::Complete &&
		semantic_nothrow_results_[root.value] != 0 &&
		semantic_nothrow_invalid_[root.value] == 0;
}

bool Pa15Lowerer::constructor_is_nothrow(FunctionFactId function_id)
{
	const std::size_t function_count = model_.function_facts_.size();
	if (!function_id.valid() || function_id.value >= function_count ||
		constructor_nothrow_states_.size() != function_count ||
		constructor_nothrow_results_.size() != function_count ||
		constructor_nothrow_invalid_.size() != function_count)
		return false;
	if (constructor_nothrow_states_[function_id.value] ==
		ConstructorRuntimeCacheState::Complete)
		return constructor_nothrow_results_[function_id.value] != 0 &&
			constructor_nothrow_invalid_[function_id.value] == 0;
	if (constructor_nothrow_states_[function_id.value] ==
		ConstructorRuntimeCacheState::InProgress)
	{
		constructor_nothrow_states_[function_id.value] =
			ConstructorRuntimeCacheState::Complete;
		constructor_nothrow_results_[function_id.value] = 0;
		constructor_nothrow_invalid_[function_id.value] = 1;
		return false;
	}
	constructor_nothrow_states_[function_id.value] =
		ConstructorRuntimeCacheState::InProgress;
	const FunctionFact& fact = model_.function_facts_[function_id.value];
	bool result = fact.is_constructor && fact.synthetic;
	bool invalid = false;
	if (result && (fact.constructor_action_begin == InvalidIdentityValue ||
		fact.constructor_action_begin > model_.constructor_actions_.size() ||
		fact.constructor_action_count > model_.constructor_actions_.size() -
		fact.constructor_action_begin))
	{
		result = false;
		invalid = true;
	}
	if (result)
	{
		for (std::size_t i = 0; i < fact.constructor_action_count; ++i)
		{
			const ConstructorActionFact& action = model_.constructor_actions_[
				fact.constructor_action_begin + i];
			bool action_result = true;
			if (action.argument_count != 0 &&
				(action.argument_begin == InvalidIdentityValue ||
					action.argument_begin > model_.constructor_arguments_.size() ||
					action.argument_count > model_.constructor_arguments_.size() -
					action.argument_begin))
			{
				action_result = false;
				invalid = true;
			}
			else if (action.argument_count == 0 &&
				action.argument_begin != InvalidIdentityValue &&
				action.argument_begin > model_.constructor_arguments_.size())
			{
				action_result = false;
				invalid = true;
			}
			if (action_result && action.initializer.valid() &&
				!constructor_initializer_is_nothrow(action.initializer))
				action_result = false;
			for (std::size_t argument = 0; action_result &&
				argument < action.argument_count; ++argument)
			{
				const SemanticFactId argument_id = model_.constructor_arguments_[
					action.argument_begin + argument];
				if (!constructor_initializer_is_nothrow(argument_id))
				{
					action_result = false;
					if (argument_id.valid() && argument_id.value <
						semantic_nothrow_invalid_.size() &&
						semantic_nothrow_invalid_[argument_id.value] != 0)
						invalid = true;
				}
			}
			if (action_result != false && action.constructor.valid())
			{
				const FunctionFactId* target =
					model_.function_binding_fact_index_.find(action.constructor);
				if (target == NULL || !constructor_is_nothrow(*target))
				{
					action_result = false;
					if (target == NULL)
						invalid = true;
					if (target != NULL && target->value <
						constructor_nothrow_invalid_.size() &&
						constructor_nothrow_invalid_[target->value] != 0)
						invalid = true;
				}
			}
			else if (!action_result || !action.constructor.valid())
			{
				if (!action.constructor.valid() && !action.initializer.valid())
					action_result = false;
			}
			if (!action_result)
			{
				result = false;
				if (action.initializer.valid() &&
					action.initializer.value < semantic_nothrow_invalid_.size() &&
					semantic_nothrow_invalid_[action.initializer.value] != 0)
					invalid = true;
			}
		}
	}
	if (constructor_nothrow_invalid_[function_id.value] != 0)
		invalid = true;
	constructor_nothrow_states_[function_id.value] =
		ConstructorRuntimeCacheState::Complete;
	constructor_nothrow_results_[function_id.value] = result ? 1 : 0;
	constructor_nothrow_invalid_[function_id.value] = invalid ? 1 : 0;
	return result && !invalid;
}

LowType Pa15Lowerer::low_reference_value_type(TypeId type) const{
	const TypeId object = model_.expression_object_type(type);
	if (object.valid() && model_.type_kind(model_.strip_cv_type(object)) ==
		TypeKind::Function)
	{
		LowType pointer;
		pointer.kind = LowType::TYPE_POINTER;
		return pointer;
	}
	return low_type(object);
}

void Pa15Lowerer::demand_function_declaration(BindingId binding){
		if (!binding.valid()) return;
		if (function_declaration_plans_.find(binding.value) !=
			function_declaration_plans_.end())
			demanded_function_declarations_.insert(binding.value);
}

void Pa15Lowerer::materialize_function_declarations(){
		for (std::size_t scope_index = 0; scope_index < model_.scopes_.size();
			++scope_index)
		{
			const Scope& scope = model_.scopes_[scope_index];
			if (scope.kind != ScopeKind::Namespace &&
				scope.kind != ScopeKind::Class) continue;
			for (std::size_t i = 0; i < scope.bindings.size(); ++i)
			{
				const BindingId binding_id = scope.bindings[i];
				if (demanded_function_declarations_.find(binding_id.value) ==
					demanded_function_declarations_.end())
					continue;
				const std::map<std::size_t, FunctionDeclaration>::const_iterator plan =
					function_declaration_plans_.find(binding_id.value);
				if (plan != function_declaration_plans_.end())
					program_.function_declarations.push_back(plan->second);
			}
		}
		for (std::size_t i = 0; i < model_.builtin_function_facts_.size(); ++i)
		{
			const BuiltinFunctionFact& builtin =
				model_.builtin_function_facts_[i];
			if (demanded_function_declarations_.find(builtin.binding.value) ==
				demanded_function_declarations_.end())
				continue;
			const std::map<std::size_t, FunctionDeclaration>::const_iterator plan =
				function_declaration_plans_.find(builtin.binding.value);
			if (plan == function_declaration_plans_.end())
				throw std::runtime_error("PA15 demanded builtin declaration is missing");
			program_.function_declarations.push_back(plan->second);
		}
	}

LoweredValue Pa15Lowerer::lower_binary_expression(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const std::vector<SemanticFactId> operands = children(id);
		if (operands.size() != 2)
			throw std::runtime_error("PA15 invalid binary fact");
		if (fact.token == SimpleTokenType::OP_COMMA)
		{
			lower_discarded_expression(operands.front());
			if (model_.semantic_facts_[operands.back().value].category !=
				SemanticValueCategory::Prvalue)
				return lower_lvalue(operands.back());
			return lower_expression(operands.back());
		}
		if (fact.token == SimpleTokenType::OP_LAND ||
			fact.token == SimpleTokenType::OP_LOR)
			return lower_logical(id);
		const bool left_pointer = pointer_like(
			model_.semantic_facts_[operands[0].value].type);
		const bool right_pointer = pointer_like(
			model_.semantic_facts_[operands[1].value].type);
		const bool force_size_operands = fact.size_type_derived;
		LoweredValue left = lower_expression_impl(operands[0], false, true,
			force_size_operands, true);
		LoweredValue right = lower_expression_impl(operands[1], false, true,
			force_size_operands, true);
		left = apply_conversions(operands[0], left, false, true,
			force_size_operands);
		right = apply_conversions(operands[1], right, false, true,
			force_size_operands);
		if ((fact.token == SimpleTokenType::OP_PLUS ||
			fact.token == SimpleTokenType::OP_MINUS) && left_pointer &&
			!right_pointer && left.type.is_pointer())
		{
			return pointer_offset(left,
				model_.semantic_facts_[operands[0].value].type, right,
				model_.semantic_facts_[operands[1].value].type,
				fact.token == SimpleTokenType::OP_MINUS);
		}
		if (fact.token == SimpleTokenType::OP_PLUS && right_pointer &&
			!left_pointer && right.type.is_pointer())
		{
			return pointer_offset(right,
				model_.semantic_facts_[operands[1].value].type, left,
				model_.semantic_facts_[operands[0].value].type, false);
		}
		if (fact.token == SimpleTokenType::OP_MINUS && left_pointer &&
			right_pointer && left.type.is_pointer() && right.type.is_pointer())
		{
			const LowType pointer = left.type;
			const LoweredValue difference = emit_binary_value(
				lowir_model::BOP_SUB, pointer, left, right);
			const LowType i64 = []() {
				LowType value;
				value.kind = LowType::TYPE_INTEGER;
				value.integer_kind = LowType::INTEGER_I64;
				return value;
			}();
			const std::size_t element_size = pointer_element_size(
				model_.semantic_facts_[operands[0].value].type);
			LoweredValue quotient = difference;
			if (element_size != 1)
				quotient = emit_binary_value(lowir_model::BOP_DIV, i64,
					difference, LoweredValue(integer_operand(
						static_cast<long long>(element_size), i64), i64, false));
			return quotient;
		}
		if (is_comparison(fact.token))
		{
			const TypeId operation_type = fact.operation_type.valid() ?
				fact.operation_type : model_.expression_object_type(
					model_.semantic_facts_[operands[0].value].type);
			const NamedRecordId operation_record =
				model_.named_record_for_type(operation_type);
			const bool scoped_enum_operation = operation_record.valid() &&
				operation_record.value < model_.named_.size() &&
				model_.named_[operation_record.value].kind == NamedKind::Enum &&
				model_.named_[operation_record.value].scoped_enum;
			LowType compare_type = operation_type.valid() ?
				low_type(operation_type) : left.physical_type;
			if (!compare_type.valid())
				compare_type = low_type(model_.expression_object_type(
					model_.semantic_facts_[operands[0].value].type));
			if (!scoped_enum_operation && !compare_type.is_pointer() &&
				compare_type.is_integer() && compare_type.integer_width() < 32)
			{
				compare_type.kind = LowType::TYPE_INTEGER;
				compare_type.integer_kind = LowType::INTEGER_I32;
			}
			return emit_compare_value(compare_predicate(fact.token,
				operation_type.valid() && unsigned_type_for(operation_type)),
				compare_type,
				LoweredValue(left.value, compare_type, false, compare_type),
				LoweredValue(right.value, compare_type, false, compare_type));
		}
		const TypeId operation_type = fact.operation_type.valid() ?
			fact.operation_type : fact.type;
		LowType type = low_type(operation_type);
		if (fact.size_type_derived && type.is_integer() &&
			type.integer_width() == 64)
			type = size_low_type();
		const lowir_model::BinaryOperator operation = binary_operator(
			fact.token, unsigned_type_for(operation_type));
		if (operation == lowir_model::BOP_INVALID)
			throw std::runtime_error("PA15 unsupported binary operator");
		return emit_binary_value(operation, type, left,
			LoweredValue(right.value, type, false));
}

LoweredValue Pa15Lowerer::lower_logical(SemanticFactId id){
		const std::vector<SemanticFactId> operands = children(id);
		if (operands.size() != 2)
			throw std::runtime_error("PA15 invalid logical expression");
		LowType result_type;
		result_type.kind = LowType::TYPE_INTEGER;
		result_type.integer_kind = LowType::INTEGER_I64;
		const bool conjunction = model_.semantic_facts_[id.value].token ==
			SimpleTokenType::OP_LAND;
		bool left_truth = false;
		const bool left_short_circuits =
			constant_truth(operands.front(), &left_truth) &&
			((conjunction && !left_truth) || (!conjunction && left_truth));
		if (left_short_circuits)
		{
			LoweredValue result(integer_operand(conjunction ? 0 : 1,
				result_type), result_type, false);
			result.canonical_truth = true;
			return result;
		}
		const LoweredValue slot = generated_slot(result_type,
			model_.semantic_facts_[id.value].token == SimpleTokenType::OP_LAND ?
			"land" : "lor");
		const BlockId rhs_block = block_id(new_block(conjunction ?
			"land_rhs" : "lor_rhs"));
		const BlockId short_block = block_id(new_block(conjunction ?
			"land_short" : "lor_short"));
		const BlockId join_block = block_id(new_block(conjunction ?
			"land_end" : "lor_end"));



		const LoweredValue left = lower_condition_expression(operands.front());
		if (conjunction)
			emit_branch(left.value, rhs_block, short_block);
		else
			emit_branch(left.value, short_block, rhs_block);
		set_current(rhs_block);
		const LoweredValue right = lower_condition_expression(operands.back());
		LowType compare_type = right.physical_type;
		const SemanticFact& right_fact =
			model_.semantic_facts_[operands.back().value];
		// Typed ABI bool results use the established i64 boolean-context
		// comparison when they enter this value-producing logical RHS.  The
		// semantic call boundary owns this fact; literals and built-in scalar
		// values retain their established physical type.
		if (right_fact.bool_context_operand &&
			model_.bool_id(right_fact.type) && compare_type.is_integer() &&
			compare_type.integer_width() < result_type.integer_width())
		{
			compare_type.kind = LowType::TYPE_INTEGER;
			compare_type.integer_kind = LowType::INTEGER_I64;
		}
		Operand compare_value = right.value;
		if (compare_value.kind == Operand::OP_INTEGER && compare_type.is_integer())
			compare_value.literal_type = compare_type;
		if (!compare_type.is_integer() && !compare_type.is_pointer())
			throw std::runtime_error("PA15 logical RHS is not scalar");
		const LoweredValue truth = emit_compare_value(lowir_model::CPP_NE,
			compare_type, LoweredValue(compare_value, compare_type, false),
			LoweredValue(integer_operand(0, compare_type), compare_type, false));
		emit_store(result_type, truth.value, slot.value);
		if (!terminated(block())) emit_jump(join_block);
		set_current(short_block);
		emit_store(result_type, integer_operand(conjunction ? 0 : 1,
			result_type), slot.value);
		if (!terminated(block())) emit_jump(join_block);
		set_current(join_block);
		const ValueId value = emit_load(slot, result_type);
		const Instruction& emitted = block().instructions.back();
		LoweredValue result(temporary_operand(value, emitted.destination_name_id),
			result_type, false);
		result.canonical_truth = true;
		return result;
	}

LoweredValue Pa15Lowerer::lower_expression_impl(SemanticFactId id, bool omit_boolean_context,
		bool materialize_lvalue, bool force_integral_literal_conversion, bool defer_conversions){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		LoweredValue result;
		switch (fact.kind)
		{
		case SemanticFactKind::Variable:
			result = lower_variable_expression(id);
			break;
		case SemanticFactKind::Literal:
			result = fact.literal_element_count != 0 ? lower_address(id) :
				literal(fact);
			break;
		case SemanticFactKind::SizeofExpression:
			result = lower_sizeof(fact);
			break;
		case SemanticFactKind::IdExpression:
			if (model_.binding(fact.binding).kind == BindingKind::Function)
				result = lower_address(id);
			else
			{
				const Binding& binding = model_.binding(fact.binding);
				const bool has_storage = global_symbols_.find(fact.binding.value) !=
					global_symbols_.end() || slot_by_binding_.find(fact.binding.value) !=
					slot_by_binding_.end();
				if (binding.kind == BindingKind::Variable && binding.has_value &&
					!has_storage)
				{
					const LowType type = low_type(fact.type);
					const long long value = binding.value_unsigned ?
						static_cast<long long>(binding.value_bits) :
						static_cast<long long>(binding.value);
					result = LoweredValue(integer_operand(value, type), type, false);
				}
				else
					result = lower_lvalue(id);
			}
			break;
		case SemanticFactKind::MemberExpression:
		{
			const Binding& binding = model_.binding(fact.selected_binding);
			const bool has_storage = global_symbols_.find(
				fact.selected_binding.value) != global_symbols_.end();
			if (model_.is_static_member(fact.selected_binding) &&
				binding.has_value && !has_storage)
			{
				const std::vector<SemanticFactId> object = children(id);
				if (object.size() != 1)
					throw std::runtime_error(
						"PA15 static member object fact is invalid");
				lower_discarded_expression(object.front());
				const LowType type = low_type(fact.type);
				const long long value = binding.value_unsigned ?
					static_cast<long long>(binding.value_bits) :
					static_cast<long long>(binding.value);
				result = LoweredValue(integer_operand(value, type), type, false);
			}
			else
				result = lower_lvalue(id);
			break;
		}
		case SemanticFactKind::UnaryExpression:
		{
			const std::vector<SemanticFactId> operands = children(id);
			if (operands.size() != 1) throw std::runtime_error("PA15 invalid unary fact");
			if (fact.token == SimpleTokenType::OP_AMP)
				result = lower_address(operands.front());
			else if (fact.token == SimpleTokenType::OP_STAR)
			{
				const LoweredValue operand = lower_expression(operands.front());
				if (!operand.type.is_pointer())
					throw std::runtime_error("PA15 dereference operand is not a pointer");
				result = LoweredValue(operand.value, lvalue_type(id), true,
					operand.physical_type);
			}
			else if (fact.token == SimpleTokenType::OP_INC ||
				fact.token == SimpleTokenType::OP_DEC)
				result = lower_incdec(id, false);
			else
			{
				const LoweredValue operand = lower_expression(operands.front());
				if (fact.token == SimpleTokenType::OP_PLUS)
					result = operand;
				else if (fact.token == SimpleTokenType::OP_LNOT)
				{
					const LowType compare_type = operand.physical_type;
					if (!compare_type.is_integer() && !compare_type.is_pointer())
						throw std::runtime_error("PA15 logical negation operand is not scalar");
					result = emit_compare_value(lowir_model::CPP_EQ, compare_type,
						operand, LoweredValue(integer_operand(0, compare_type),
							compare_type, false));
				}
				else
				{
					Instruction instruction;
					instruction.kind = Instruction::IK_UNARY;
					instruction.type = low_type(fact.type);
					instruction.first = operand.value;
					instruction.unary_operator = fact.token == SimpleTokenType::OP_MINUS ?
						lowir_model::UOP_NEG : fact.token == SimpleTokenType::OP_LNOT ?
						lowir_model::UOP_NOT : fact.token == SimpleTokenType::OP_COMPL ?
						lowir_model::UOP_BITNOT : lowir_model::UOP_INVALID;
					if (instruction.unary_operator == lowir_model::UOP_INVALID)
						throw std::runtime_error("PA15 unsupported unary operator");
					const ValueId value = destination(instruction.type, &instruction);
					block().instructions.push_back(instruction);
					result = LoweredValue(temporary_operand(value,
						instruction.destination_name_id), instruction.type, false);
				}
			}
			break;
		}
		case SemanticFactKind::PostfixExpression:
			result = lower_incdec(id, true);
			break;
		case SemanticFactKind::SubscriptExpression:
			result = lower_lvalue(id);
			break;
		case SemanticFactKind::BinaryExpression:
			result = lower_binary_expression(id);
			break;
		case SemanticFactKind::AssignmentExpression:
			result = lower_assignment(id);
			break;
		case SemanticFactKind::ConditionalExpression:
			if (conditional_address_result(id))
			{
				const LoweredValue address = lower_conditional_address(id);
				bool array_to_pointer = false;
				if (fact.conversion_begin != InvalidIdentityValue)
					for (std::size_t i = 0; i < fact.conversion_count; ++i)
						if (model_.conversion_facts_[fact.conversion_begin + i].kind ==
							ConversionKind::ArrayToPointer)
							array_to_pointer = true;
				if (fact.category == SemanticValueCategory::Prvalue ||
					array_to_pointer)
					result = address;
				else
					result = LoweredValue(address.value, lvalue_type(id), true,
						address.physical_type);
			}
			else
				result = lower_conditional_value(id);
			break;
		case SemanticFactKind::CallExpression:
			result = lower_call(id);
			break;
		case SemanticFactKind::ConstructorAction:
			result = lower_constructor_expression(id);
			break;
		case SemanticFactKind::CastExpression:
		{
			const std::vector<SemanticFactId> operands = children(id);
			if (operands.size() != 1)
				throw std::runtime_error("PA15 unsupported cast expression");
			const TypeKind target_kind = model_.type_kind(
				model_.strip_cv_type(fact.type));
			if (target_kind == TypeKind::LvalueReference ||
				target_kind == TypeKind::RvalueReference)
			{
				const SemanticFact& operand_fact = model_.semantic_facts_[
					operands.front().value];
				const BindingId operand_binding = operand_fact.binding.valid() ?
					operand_fact.binding : operand_fact.selected_binding;
				if (model_.bit_field_fact(operand_binding) != NULL)
				{
					// PA12 represents a bit-field reference cast as a typed value
					// temporary followed by ReferenceBinding.  Defer the child
					// conversions so the first LvalueToRvalue uses the canonical
					// operation type and cannot take the packed-unit address.
					result = lower_expression_impl(operands.front(), false, true,
						false, true);
				}
				else
				{
					// PA12 owns reference-cast validity and keeps the typed source
					// fact.  Preserve the address for ordinary reference casts.
					const LoweredValue address = lower_address(operands.front());
					result = LoweredValue(address.value,
						low_reference_value_type(fact.type), true,
						address.physical_type);
				}
			}
			else if (model_.void_id(fact.type))
			{
				// A discarded conversion to void evaluates the source, but its
				// scalar result is not needed.  Keep that context all the way
				// through the source so side-effecting lvalues are not reloaded.
				lower_discarded_expression(operands.front(), true);
				result = LoweredValue(Operand(), low_type(fact.type), false);
			}
			else
				result = lower_expression(operands.front());
			break;
		}
		default:
			throw std::runtime_error("PA15 unsupported scalar expression fact");
		}
		if (result.canonical_truth && fact.type.valid() &&
			model_.bool_id(fact.type))
			result.type = low_type(fact.type);
		if (defer_conversions)
		{
			// Keep a bit-field lvalue tagged until its first PA12 conversion is
			// applied.  That conversion carries the semantic operation type and
			// lets the projection load use the exact requested result width.
			if (materialize_lvalue && result.lvalue && !result.type.is_object() &&
				!result.bit_field_lvalue)
				materialize_lvalue_value(&result, result.type);
			return result;
		}
		return apply_conversions(id, result, omit_boolean_context,
			materialize_lvalue, force_integral_literal_conversion);
	}

void Pa15Lowerer::lower_discarded_expression(SemanticFactId id,
	bool materialize_class_lvalue){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const TypeId object_type = model_.expression_object_type(fact.type);
		const bool volatile_lvalue =
			fact.category == SemanticValueCategory::Lvalue &&
			(model_.cv_qualifiers(object_type) & 2u) != 0;
		if (volatile_lvalue && model_.scalar_id(object_type))
		{
			(void)lower_expression_impl(id, false, true);
			return;
		}
		if (fact.kind == SemanticFactKind::IdExpression &&
			!volatile_lvalue && fact.conversion_count == 0 && fact.binding.valid())
		{
			const Binding& binding = model_.binding(fact.binding);
			if (binding.kind == BindingKind::Function ||
				reference_binding(fact.binding))
				return;
		}
		if (materialize_class_lvalue &&
			fact.category == SemanticValueCategory::Lvalue &&
			model_.class_scope_for_type(object_type).valid())
		{
			const LoweredValue discarded = lower_expression_impl(id, false, false);
			if (discarded.lvalue)
				(void)address_of_storage(discarded);
			return;
		}
		if (fact.kind == SemanticFactKind::BinaryExpression &&
			fact.token == SimpleTokenType::OP_COMMA)
		{
			const std::vector<SemanticFactId> facts = children(id);
			if (facts.size() != 2)
				throw std::runtime_error("PA15 invalid discarded comma expression");
			lower_discarded_expression(facts.front());
			lower_discarded_expression(facts.back());
			return;
		}
		if (fact.kind == SemanticFactKind::ConditionalExpression &&
			model_.void_id(fact.type))
		{
			const std::vector<SemanticFactId> facts = children(id);
			if (facts.size() != 3)
				throw std::runtime_error("PA15 invalid discarded conditional expression");
			const BlockId then_block = block_id(new_block("discard_cond_then"));
			const BlockId else_block = block_id(new_block("discard_cond_else"));
			const BlockId end_block = block_id(new_block("discard_cond_end"));
			if (has_direct_short_circuit(facts[0]))
				lower_condition_branch(facts[0], then_block, else_block);
			else
			{
				const LoweredValue condition = lower_condition(facts[0]);
				emit_branch(condition.value, then_block, else_block);
			}
			set_current(then_block);
			lower_discarded_expression(facts[1]);
			if (!terminated(block())) emit_jump(end_block);
			set_current(else_block);
			lower_discarded_expression(facts[2]);
			if (!terminated(block())) emit_jump(end_block);
			set_current(end_block);
			return;
		}
		(void)lower_expression_impl(id, false, false);
}

LoweredValue Pa15Lowerer::lower_conditional_address(SemanticFactId id){
	const std::vector<SemanticFactId> facts = children(id);
	if (facts.size() != 3)
		throw std::runtime_error("PA15 invalid conditional expression");
	LowType pointer;
	pointer.kind = LowType::TYPE_POINTER;
	const LoweredValue result_slot = generated_slot(pointer, "condaddr");
	const BlockId then_block = block_id(new_block("condaddr_then"));
	const BlockId else_block = block_id(new_block("condaddr_else"));
	const BlockId join_block = block_id(new_block("condaddr_end"));
	if (has_direct_short_circuit(facts[0]))
		lower_condition_branch(facts[0], then_block, else_block);
	else
	{
		const LoweredValue condition = lower_condition(facts[0]);
		emit_branch(condition.value, then_block, else_block);
	}
	const auto lower_branch_address = [this](SemanticFactId branch) {
		LoweredValue result = lower_address(branch);
		const SemanticFact& branch_fact = model_.semantic_facts_[branch.value];
		if (branch_fact.conversion_begin != InvalidIdentityValue)
			for (std::size_t i = 0; i < branch_fact.conversion_count; ++i)
			{
				const ConversionFact& conversion = model_.conversion_facts_[
					branch_fact.conversion_begin + i];
				if (conversion.kind == ConversionKind::DerivedToBase)
					result = apply_derived_base_conversion(result, conversion,
						low_type(conversion.target), true);
			}
		return result;
	};
	set_current(then_block);
	const LoweredValue when_true = lower_branch_address(facts[1]);
	emit_store(pointer, when_true.value, result_slot.value);
	if (!terminated(block())) emit_jump(join_block);
	set_current(else_block);
	const LoweredValue when_false = lower_branch_address(facts[2]);
	emit_store(pointer, when_false.value, result_slot.value);
	if (!terminated(block())) emit_jump(join_block);
	set_current(join_block);
	const ValueId value = emit_load(result_slot, pointer);
	const Instruction& emitted = block().instructions.back();
	return LoweredValue(temporary_operand(value, emitted.destination_name_id),
		pointer, false);
}

bool Pa15Lowerer::constant_truth(SemanticFactId id, bool* value){
		if (value == NULL || !id.valid() || id.value >= model_.semantic_facts_.size())
			return false;
		if (id.value >= constant_truth_cache_.size())
			return false;
		unsigned char& state = constant_truth_cache_[id.value];
		if (state == 1 || state == 2)
		{
			*value = state == 2;
			return true;
		}
		if (state == 0)
			return false;
		// Mark the fact unknown while visiting it.  Semantic facts form an
		// acyclic tree, but this also makes a malformed cycle fail closed.
		state = 0;
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::BinaryExpression &&
			(fact.token == SimpleTokenType::OP_LAND ||
			 fact.token == SimpleTokenType::OP_LOR))
		{
			const std::vector<SemanticFactId> operands = children(id);
			if (operands.size() != 2)
				return false;
			bool left = false;
			if (!constant_truth(operands.front(), &left))
				return false;
			if (fact.token == SimpleTokenType::OP_LAND && !left)
			{
				*value = false;
				state = 1;
				return true;
			}
			if (fact.token == SimpleTokenType::OP_LOR && left)
			{
				*value = true;
				state = 2;
				return true;
			}
			if (!constant_truth(operands.back(), value))
				return false;
			state = *value ? 2 : 1;
			return true;
		}
		if (fact.kind != SemanticFactKind::Literal)
			return false;
		if (fact.token == SimpleTokenType::KW_TRUE ||
			fact.token == SimpleTokenType::KW_FALSE)
		{
			*value = fact.token == SimpleTokenType::KW_TRUE;
			state = *value ? 2 : 1;
			return true;
		}
		__int128 integer = 0;
		if (fact.has_constant_value)
			integer = fact.constant_value;
		else if (fact.has_literal_value)
		{
			integer = static_cast<__int128>(fact.literal_value);
			if (fact.literal_value_negative) integer = -integer;
		}
		else
			return false;
		*value = integer != 0;
		state = *value ? 2 : 1;
		return true;
}

void Pa15Lowerer::lower_condition_branch(SemanticFactId id, BlockId true_target,
		BlockId false_target){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::Condition)
		{
			if (fact.child_count != 1)
				throw std::runtime_error("PA15 empty branching condition");
			lower_condition_branch(
				model_.semantic_children_[fact.child_begin], true_target,
				false_target);
			return;
		}
		if (fact.kind == SemanticFactKind::ConditionDeclaration)
		{
			const std::vector<SemanticFactId> values = children(id);
			if (values.size() != 1)
				throw std::runtime_error("PA15 invalid branching condition declaration");
			const LoweredValue condition = lower_condition_expression(values.front());
			emit_branch(condition.value, true_target, false_target);
			return;
		}
		if (fact.kind == SemanticFactKind::BinaryExpression &&
			(fact.token == SimpleTokenType::OP_LAND ||
			 fact.token == SimpleTokenType::OP_LOR))
		{
			const std::vector<SemanticFactId> operands = children(id);
			if (operands.size() != 2)
				throw std::runtime_error("PA15 invalid logical condition fact");
			bool left_truth = false;
			if (constant_truth(operands[0], &left_truth))
			{
				const bool short_circuit = fact.token == SimpleTokenType::OP_LAND ?
					!left_truth : left_truth;
				if (short_circuit)
					lower_condition_branch(operands[0], true_target,
						false_target);
				else
					lower_condition_branch(operands[1], true_target, false_target);
				return;
			}
			const std::size_t rhs_index = new_block(fact.token ==
				SimpleTokenType::OP_LAND ? "land_rhs" : "lor_rhs");
			const BlockId rhs_target = block_id(rhs_index);
			if (fact.token == SimpleTokenType::OP_LAND)
			{
				lower_condition_branch(operands[0], rhs_target, false_target);
				set_current(rhs_target);
				lower_condition_branch(operands[1], true_target, false_target);
			}
			else
			{
				lower_condition_branch(operands[0], true_target, rhs_target);
				set_current(rhs_target);
				lower_condition_branch(operands[1], true_target, false_target);
			}
			return;
		}
		const LoweredValue condition = lower_condition_expression(id);
		emit_branch(condition.value, true_target, false_target);
	}

BlockId Pa15Lowerer::switch_label_existing_target(SemanticFactId id) const{
		if (switch_stack_.empty())
			throw std::runtime_error("PA15 switch label context is missing");
		const SwitchContext& context = switch_stack_.back();
		const std::map<std::size_t, BlockId>::const_iterator found =
			context.labels.find(id.value);
		if (found == context.labels.end())
			throw std::runtime_error("PA15 switch label owner mismatch");
		return found->second;
	}

void Pa15Lowerer::terminate_unreachable_block(BlockId id){
		if (terminated(id)) return;
		if (is_reachable(id))
			throw std::runtime_error("PA15 reachable block cannot be a sink");
		const BlockId saved_current = current_block_id();
		set_current(id);



		emit_jump(id);
		if (saved_current.valid()) set_current(saved_current);
		else current_block_ = InvalidIdentityValue;
	}

bool Pa15Lowerer::lower_switch_label_recovery(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (current_block_ == InvalidIdentityValue)
		{
			if (switch_stack_.empty())
				throw std::runtime_error("PA15 switch label lifetime context is missing");
			active_lifetimes_ = switch_stack_.back().entry_lifetimes;
		}
		const bool already_lowered = switch_label_was_lowered(id);
		const BlockId target = already_lowered ?
			switch_label_existing_target(id) : switch_label_target(id);
		if (already_lowered)
		{



			if (current_block_ != InvalidIdentityValue &&
				current_block_id() != target && !terminated(block()))
				emit_jump(target);
			current_block_ = InvalidIdentityValue;
			return true;
		}
		if (current_block_ != InvalidIdentityValue &&
			current_block_id() != target && !terminated(block()))
			emit_jump(target);
		set_current(target);
		const std::vector<SemanticFactId> facts = children(id);
		if (fact.kind == SemanticFactKind::CaseStatement)
		{
			if (facts.size() != 2)
				throw std::runtime_error("PA15 invalid case statement");
			lower_statement(facts.back());
		}
		else
		{
			if (facts.size() != 1)
				throw std::runtime_error("PA15 invalid default statement");
			lower_statement(facts.front());
		}
		return true;
	}

bool Pa15Lowerer::collect_switch_labels(SemanticFactId id, SwitchContext* context){
		const SemanticFact& fact = model_.semantic_facts_[id.value];



		if (fact.kind == SemanticFactKind::SwitchStatement) return false;
		bool found_label = false;
		if (fact.kind == SemanticFactKind::CaseStatement)
		{
			const std::vector<SemanticFactId> label_children = children(id);
			if (label_children.size() != 2)
				throw std::runtime_error("PA15 invalid case fact");
			const LoweredValue value = literal(
				model_.semantic_facts_[label_children.front().value]);
			const BlockId target = block_id(new_block("switch_case"));
			if (!context->labels.insert(std::make_pair(id.value, target)).second)
				throw std::runtime_error("PA15 duplicate switch label fact");
			context->arms.push_back(SwitchArm(id, target, false, value.value));
			found_label = true;
			if (collect_switch_labels(label_children.back(), context))
				found_label = true;
		}
		else if (fact.kind == SemanticFactKind::DefaultStatement)
		{
			const std::vector<SemanticFactId> label_children = children(id);
			if (label_children.size() != 1)
				throw std::runtime_error("PA15 invalid default fact");
			const BlockId target = block_id(new_block("switch_default"));
			if (!context->labels.insert(std::make_pair(id.value, target)).second)
				throw std::runtime_error("PA15 duplicate switch label fact");
			context->default_target = target;
			context->arms.push_back(SwitchArm(id, target, true, Operand()));
			found_label = true;
			if (collect_switch_labels(label_children.front(), context))
				found_label = true;
		}
		else
		{
			const std::vector<SemanticFactId> facts = children(id);
			for (std::size_t i = 0; i < facts.size(); ++i)
				if (collect_switch_labels(facts[i], context))
					found_label = true;
		}
		if (found_label) context->label_subtrees.insert(id.value);
		return found_label;
	}

static BlockId loop_flow_continue_target(const LoopFlow& flow){
	return flow.kind == SemanticFactKind::ForStatement ?
		flow.iteration : flow.condition;
}

void Pa15Lowerer::finish_switch_loop(const LoopFlow& target){
		control_stack_.pop_back();




		set_current(target.end);
	}

	void Pa15Lowerer::recover_existing_switch_loop(SemanticFactId body_fact,
	const LoopFlow& target){
		const std::size_t loop_depth = active_lifetimes_.size();
		control_stack_.push_back(ControlTarget(true, target.end,
			loop_flow_continue_target(target), loop_depth, loop_depth));
		current_block_ = InvalidIdentityValue;
		lower_switch_body(body_fact);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(loop_flow_continue_target(target));
		finish_switch_loop(target);
}

void Pa15Lowerer::lower_switch_while(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid recovered while fact");
		const LoopFlow* existing = remembered_loop_flow(id);
		if (existing != NULL)
		{
			recover_existing_switch_loop(facts[1], *existing);
			return;
		}
		const BlockId condition = block_id(new_block("while_cond"));
		const BlockId body = block_id(new_block("while_body"));
		const BlockId end = block_id(new_block("while_end"));
		store_loop_flow(id, LoopFlow(SemanticFactKind::WhileStatement,
			condition, body, condition, end));
		set_current(condition);
		if (has_direct_short_circuit(facts[0]))
			lower_condition_branch(facts[0], body, end);
		else
		{
			const LoweredValue value = lower_condition(facts[0]);
			emit_branch(value.value, body, end);
		}
		const std::size_t loop_depth = active_lifetimes_.size();
		control_stack_.push_back(ControlTarget(true, end, condition,
			loop_depth, loop_depth));
		set_current(body);
		lower_statement(facts[1]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		current_block_ = InvalidIdentityValue;



		lower_switch_body(facts[1]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		const LoopFlow* flow = remembered_loop_flow(id);
		if (flow == NULL)
			throw std::runtime_error("PA15 recovered while flow disappeared");
		finish_switch_loop(*flow);
	}

void Pa15Lowerer::lower_switch_do(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid recovered do fact");
		const LoopFlow* existing = remembered_loop_flow(id);
		if (existing != NULL)
		{
			recover_existing_switch_loop(facts[0], *existing);
			return;
		}
		const BlockId body = block_id(new_block("do_body"));
		const BlockId condition = block_id(new_block("do_cond"));
		const BlockId end = block_id(new_block("do_end"));
		store_loop_flow(id, LoopFlow(SemanticFactKind::DoStatement,
			condition, body, condition, end));
		const std::size_t loop_depth = active_lifetimes_.size();
		control_stack_.push_back(ControlTarget(true, end, condition,
			loop_depth, loop_depth));
		set_current(body);
		lower_statement(facts[0]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		current_block_ = InvalidIdentityValue;
		lower_switch_body(facts[0]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		set_current(condition);
		if (has_direct_short_circuit(facts[1]))
			lower_condition_branch(facts[1], body, end);
		else
		{
			const LoweredValue value = lower_condition(facts[1]);
			emit_branch(value.value, body, end);
		}
		const LoopFlow* flow = remembered_loop_flow(id);
		if (flow == NULL)
			throw std::runtime_error("PA15 recovered do flow disappeared");
		finish_switch_loop(*flow);
	}

void Pa15Lowerer::lower_switch_for(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() < 2)
			throw std::runtime_error("PA15 invalid recovered for fact");
		SemanticFactId condition_fact;
		SemanticFactId iteration_fact;
		for (std::size_t i = 1; i + 1 < facts.size(); ++i)
		{
			const SemanticFactKind kind = model_.semantic_facts_[facts[i].value].kind;
			if (kind == SemanticFactKind::Condition) condition_fact = facts[i];
			else if (kind == SemanticFactKind::Iteration) iteration_fact = facts[i];
		}
		const LoopFlow* existing = remembered_loop_flow(id);
		if (existing != NULL)
		{
			recover_existing_switch_loop(facts.back(), *existing);
			return;
		}
		const BlockId initialization = block_id(new_block("for_init"));
		const BlockId condition = block_id(new_block("for_cond"));
		const BlockId body = block_id(new_block("for_body"));
		const BlockId iteration = block_id(new_block("for_iter"));
		const BlockId end = block_id(new_block("for_end"));



		store_loop_flow(id, LoopFlow(SemanticFactKind::ForStatement,
			condition, body, iteration, end));
		set_current(initialization);
		lower_statement(facts[0]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		set_current(condition);
		if (!condition_fact.valid() || condition_is_empty(condition_fact))
			emit_jump(body);
		else if (has_direct_short_circuit(condition_fact))
			lower_condition_branch(condition_fact, body, end);
		else
		{
			const LoweredValue value = lower_condition(condition_fact);
			emit_branch(value.value, body, end);
		}
		const std::size_t loop_depth = active_lifetimes_.size();
		control_stack_.push_back(ControlTarget(true, end, iteration,
			loop_depth, loop_depth));
		set_current(body);
		lower_statement(facts.back());
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(iteration);
		current_block_ = InvalidIdentityValue;
		lower_switch_body(facts.back());
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(iteration);
		set_current(iteration);
		if (iteration_fact.valid())
		{
			const std::vector<SemanticFactId> expression = children(iteration_fact);
			if (expression.size() != 1)
				throw std::runtime_error("PA15 invalid recovered for iteration fact");
			lower_discarded_expression(expression.front());
		}
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		const LoopFlow* flow = remembered_loop_flow(id);
		if (flow == NULL)
			throw std::runtime_error("PA15 recovered for flow disappeared");
		finish_switch_loop(*flow);
	}

bool Pa15Lowerer::lower_switch_body(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];


		if (fact.kind == SemanticFactKind::SwitchStatement)
		{
			if (current_block_ != InvalidIdentityValue) lower_statement(id);
			return false;
		}
		if (fact.kind == SemanticFactKind::IfStatement &&
			current_block_ == InvalidIdentityValue)
		{
			const std::vector<SemanticFactId> branches = children(id);
			std::vector<BlockId> fallthroughs;
			std::vector<std::vector<BindingId> > fallthrough_lifetimes;
			std::vector<std::vector<BindingId> > terminal_lifetimes;
			const std::vector<BindingId> incoming_lifetimes = active_lifetimes_;
			bool entered_label = false;
			for (std::size_t i = 0; i < branches.size(); ++i)
			{
				const SemanticFactKind kind =
					model_.semantic_facts_[branches[i].value].kind;
				if (kind != SemanticFactKind::ThenBranch &&
					kind != SemanticFactKind::ElseBranch)
					continue;
				if (!switch_subtree_has_label(branches[i])) continue;
				if (entered_label) current_block_ = InvalidIdentityValue;
				active_lifetimes_ = incoming_lifetimes;
				if (!lower_switch_body(branches[i])) continue;
				entered_label = true;
				const std::vector<BindingId> branch_lifetimes = active_lifetimes_;
				if (current_block_ != InvalidIdentityValue)
				{
					fallthroughs.push_back(current_block_id());
					fallthrough_lifetimes.push_back(branch_lifetimes);
				}
				else
					terminal_lifetimes.push_back(branch_lifetimes);
			}
			if (fallthroughs.empty())
			{
				if (!terminal_lifetimes.empty())
				{
					for (std::size_t i = 1; i < terminal_lifetimes.size(); ++i)
						if (terminal_lifetimes[i] != terminal_lifetimes.front())
							throw std::runtime_error(
								"PA15 switch exits have divergent active lifetime states");
					active_lifetimes_ = terminal_lifetimes.front();
				}
				else
					active_lifetimes_ = incoming_lifetimes;
				current_block_ = InvalidIdentityValue;
			}
			else if (fallthroughs.size() == 1)
			{
				active_lifetimes_ = fallthrough_lifetimes.front();
				set_current(fallthroughs.front());
			}
			else
			{
				for (std::size_t i = 1; i < fallthrough_lifetimes.size(); ++i)
					if (fallthrough_lifetimes[i] != fallthrough_lifetimes.front())
						throw std::runtime_error(
							"PA15 switch join has divergent active lifetime states");
				const BlockId join = block_id(new_block("switch_if_end"));
				for (std::size_t i = 0; i < fallthroughs.size(); ++i)
				{
					active_lifetimes_ = fallthrough_lifetimes[i];
					set_current(fallthroughs[i]);
					if (!terminated(block())) emit_jump(join);
				}
				active_lifetimes_ = fallthrough_lifetimes.front();
				set_current(join);
			}
			return entered_label;
		}
		if (current_block_ == InvalidIdentityValue &&
			switch_subtree_has_label(id))
		{
			if (fact.kind == SemanticFactKind::WhileStatement)
			{
				lower_switch_while(id);
				return true;
			}
			if (fact.kind == SemanticFactKind::DoStatement)
			{
				lower_switch_do(id);
				return true;
			}
			if (fact.kind == SemanticFactKind::ForStatement)
			{
				lower_switch_for(id);
				return true;
			}
		}
		if (current_block_ == InvalidIdentityValue &&
			fact.kind != SemanticFactKind::CaseStatement &&
			fact.kind != SemanticFactKind::DefaultStatement &&
			fact.kind != SemanticFactKind::SwitchStatement &&
			!switch_subtree_has_label(id) &&
			referenced_label_subtree(id))
		{
			// Switch labels are handled by the switch-specific walk above;
			// ordinary labels need the typed PA15 ancestry walk so a label
			// hidden after an earlier terminating case is not discarded.
			lower_referenced_label_subtree(id);
			return true;
		}
		if (current_block_ == InvalidIdentityValue &&
			!switch_subtree_has_label(id)) return false;
	if (fact.kind == SemanticFactKind::CompoundStatement)
	{
		const std::vector<SemanticFactId> facts = children(id);
		bool entered_label = false;
		push_label_recovery_boundary(id);
		for (std::size_t i = 0; i < facts.size(); ++i)
		{
			if (lower_switch_body(facts[i])) entered_label = true;
			drain_label_recovery_queue(id, i + 1);
		}
		pop_label_recovery_boundary();
		return entered_label;
		}
		if (fact.kind == SemanticFactKind::CaseStatement ||
			fact.kind == SemanticFactKind::DefaultStatement)
		{
			return lower_switch_label_recovery(id);
		}
		if (current_block_ != InvalidIdentityValue)
		{
			lower_statement(id);
			return false;
		}






		const std::vector<SemanticFactId> facts = children(id);
		bool entered_label = false;
		for (std::size_t i = 0; i < facts.size(); ++i)
			if (lower_switch_body(facts[i])) entered_label = true;
		return entered_label;
	}

void Pa15Lowerer::finish_switch_labels(){
		if (switch_stack_.empty())
			throw std::runtime_error("PA15 switch label context is missing");
		const SwitchContext& context = switch_stack_.back();
		if (context.lowered_labels.size() != context.labels.size())
			throw std::runtime_error("PA15 switch label was not lowered");
		for (std::map<std::size_t, BlockId>::const_iterator label =
			context.labels.begin(); label != context.labels.end(); ++label)
			if (context.lowered_labels.find(label->first) ==
				context.lowered_labels.end())
				throw std::runtime_error("PA15 switch label was not visited");
		for (std::size_t i = 0; i < context.arms.size(); ++i)
		{
			const BlockId target = context.arms[i].target;
			if (!terminated(target))
			{
				set_current(target);
				emit_jump(context.end_target);
			}
		}
	}

void Pa15Lowerer::lower_switch(SemanticFactId id,
	const std::vector<SemanticFactId>& facts){
		if (facts.size() < 1 || facts.size() > 2)
			throw std::runtime_error("PA15 invalid switch fact");
		if (current_block_ == InvalidIdentityValue)
		{
			const BlockId selector_block = block_id(new_block("switch_selector"));
			set_current(selector_block);
		}
		const LoweredValue selector = lower_condition(facts.front());
		const BlockId dispatch = block_id(new_block("switch_dispatch"));
		const BlockId end = block_id(new_block("switch_end"));
		SwitchContext context(end, dispatch);
		context.entry_lifetimes = active_lifetimes_;
		if (facts.size() == 2)
			collect_switch_labels(facts[1], &context);
		store_switch_flow(id, context);
		emit_jump(dispatch);
		set_current(dispatch);
		Instruction instruction;
		instruction.kind = Instruction::IK_SWITCH;
		instruction.first = selector.value;
		instruction.second = block_operand(context.default_target);
		for (std::size_t i = 0; i < context.arms.size(); ++i)
		{
			if (context.arms[i].is_default) continue;
			instruction.args.push_back(context.arms[i].value);
			instruction.args.push_back(block_operand(context.arms[i].target));
		}
		block().instructions.push_back(instruction);
		const BlockId dispatch_source = current_block_id();
		propagate_edge(dispatch_source, context.default_target);
		for (std::size_t i = 0; i < context.arms.size(); ++i)
			if (!context.arms[i].is_default)
				propagate_edge(dispatch_source, context.arms[i].target);

		switch_stack_.push_back(context);
		const std::size_t switch_depth = active_lifetimes_.size();
		control_stack_.push_back(ControlTarget(false, end, BlockId(),
			switch_depth, switch_depth));
		if (facts.size() == 2)
		{



			current_block_ = InvalidIdentityValue;
			lower_switch_body(facts[1]);
			finish_switch_labels();
		}
		if (current_block_ != InvalidIdentityValue &&
			current_block_id() != end && !terminated(block()))
			emit_jump(end);
		store_switch_flow(id, switch_stack_.back());
		control_stack_.pop_back();
		switch_stack_.pop_back();

		active_lifetimes_ = context.entry_lifetimes;
		set_current(end);
	}

void Pa15Lowerer::lower_while(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid while fact");
		const std::vector<BindingId> incoming_lifetimes = active_lifetimes_;
		const BlockId condition = block_id(new_block("while_cond"));
		const BlockId body = block_id(new_block("while_body"));
		const BlockId end = block_id(new_block("while_end"));
		store_loop_flow(id, LoopFlow(SemanticFactKind::WhileStatement,
			condition, body, condition, end));
		if (current_block_ != InvalidIdentityValue)
			emit_jump(condition);
		set_current(condition);
		if (has_direct_short_circuit(facts[0]))
			lower_condition_branch(facts[0], body, end);
		else
		{
			const LoweredValue value = lower_condition(facts[0]);
			emit_branch(value.value, body, end);
		}
		const std::size_t loop_depth = active_lifetimes_.size();
		control_stack_.push_back(ControlTarget(true, end, condition,
			loop_depth, loop_depth));
		set_current(body);
		lower_scoped_statement(facts[1]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		control_stack_.pop_back();
		active_lifetimes_ = incoming_lifetimes;
		set_current(end);
	}

void Pa15Lowerer::lower_do(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid do fact");
		const std::vector<BindingId> incoming_lifetimes = active_lifetimes_;
		const BlockId body = block_id(new_block("do_body"));
		const BlockId condition = block_id(new_block("do_cond"));
		const BlockId end = block_id(new_block("do_end"));
		store_loop_flow(id, LoopFlow(SemanticFactKind::DoStatement,
			condition, body, condition, end));
		if (current_block_ != InvalidIdentityValue)
			emit_jump(body);
		const std::size_t loop_depth = active_lifetimes_.size();
		control_stack_.push_back(ControlTarget(true, end, condition,
			loop_depth, loop_depth));
		set_current(body);
		lower_scoped_statement(facts[0]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		active_lifetimes_ = incoming_lifetimes;
		set_current(condition);
		if (has_direct_short_circuit(facts[1]))
			lower_condition_branch(facts[1], body, end);
		else
		{
			const LoweredValue value = lower_condition(facts[1]);
			emit_branch(value.value, body, end);
		}
		control_stack_.pop_back();
		active_lifetimes_ = incoming_lifetimes;
		set_current(end);
	}

void Pa15Lowerer::lower_for(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() < 2)
			throw std::runtime_error("PA15 invalid for fact");
		const std::vector<BindingId> incoming_lifetimes = active_lifetimes_;
		const StatementFact* statement = model_.semantic_facts_[id.value].source == NULL ?
			NULL : model_.statement_fact(*model_.semantic_facts_[id.value].source);
		const ScopeId control_scope = statement == NULL ? ScopeId() : statement->scope;
		const std::size_t loop_depth = incoming_lifetimes.size();
		if (control_scope.valid())
		{
			if (control_scope.value >= model_.scopes_.size())
				throw std::runtime_error("PA15 for lifetime scope is invalid");
			lifetime_scope_stack_.push_back(control_scope);
			lifetime_scope_depths_.push_back(loop_depth);
		}
		lower_statement(facts[0]);
		const std::vector<BindingId> init_lifetimes = active_lifetimes_;
		const std::size_t continue_depth = init_lifetimes.size();
		const bool has_control_lifetimes = continue_depth > loop_depth;
		if (has_control_lifetimes && !control_scope.valid())
			throw std::runtime_error("PA15 for lifetime scope is missing");
		SemanticFactId condition_fact;
		SemanticFactId iteration_fact;
		for (std::size_t i = 1; i + 1 < facts.size(); ++i)
		{
			const SemanticFactKind kind = model_.semantic_facts_[facts[i].value].kind;
			if (kind == SemanticFactKind::Condition) condition_fact = facts[i];
			else if (kind == SemanticFactKind::Iteration) iteration_fact = facts[i];
		}
		const BlockId condition = block_id(new_block("for_cond"));
		const BlockId body = block_id(new_block("for_body"));
		const BlockId iteration = block_id(new_block("for_iter"));
		const BlockId end = block_id(new_block("for_end"));
		const BlockId normal_end = has_control_lifetimes && condition_fact.valid() &&
			!condition_is_empty(condition_fact) ?
			block_id(new_block("for_scope_end")) : end;
		store_loop_flow(id, LoopFlow(SemanticFactKind::ForStatement,
			condition, body, iteration, end));
		if (current_block_ != InvalidIdentityValue)
			emit_jump(condition);
		set_current(condition);
		if (!condition_fact.valid() || condition_is_empty(condition_fact))
			emit_jump(body);
		else if (has_direct_short_circuit(condition_fact))
			lower_condition_branch(condition_fact, body, normal_end);
		else
		{
			const LoweredValue value = lower_condition(condition_fact);
			emit_branch(value.value, body, normal_end);
		}
		control_stack_.push_back(ControlTarget(true, end, iteration,
			loop_depth, continue_depth));
		set_current(body);
		lower_scoped_statement(facts.back());
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(iteration);
		active_lifetimes_ = init_lifetimes;
		set_current(iteration);
		if (iteration_fact.valid())
		{
			const std::vector<SemanticFactId> expression = children(iteration_fact);
			if (expression.size() != 1)
				throw std::runtime_error("PA15 invalid for iteration fact");
			lower_discarded_expression(expression.front());
		}
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		control_stack_.pop_back();
		if (normal_end != end)
		{
			active_lifetimes_ = init_lifetimes;
			set_current(normal_end);
			emit_scope_destructors(control_scope, loop_depth);
			emit_jump(end);
		}
		active_lifetimes_ = incoming_lifetimes;
		if (control_scope.valid())
		{
			lifetime_scope_stack_.pop_back();
			lifetime_scope_depths_.pop_back();
		}
		set_current(end);
	}

void Pa15Lowerer::lower_statement(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (current_block_ == InvalidIdentityValue &&
			fact.kind != SemanticFactKind::CaseStatement &&
			fact.kind != SemanticFactKind::DefaultStatement &&
			!(fact.kind == SemanticFactKind::LabeledStatement &&
				current_label_target(fact.label)))
			return;
		const std::vector<SemanticFactId> facts = children(id);
		switch (fact.kind)
		{
		case SemanticFactKind::CompoundStatement:
		{
			const ScopeId* compound_scope = fact.source == NULL ? NULL :
				model_.compound_scope_index_.find(fact.source);
			const std::size_t lifetime_depth = active_lifetimes_.size();
			if (compound_scope != NULL)
			{
				lifetime_scope_stack_.push_back(*compound_scope);
				lifetime_scope_depths_.push_back(lifetime_depth);
			}
			push_label_recovery_boundary(id);
			for (std::size_t i = 0; i < facts.size(); ++i)
			{
					if (current_block_ == InvalidIdentityValue)
					{
						if (referenced_label_subtree(facts[i]))
							lower_referenced_label_subtree(facts[i]);
					}
					else
					{
						lower_statement(facts[i]);
					}
					drain_label_recovery_queue(id, i + 1);
			}
			if (compound_scope != NULL && current_block_ != InvalidIdentityValue)
				emit_scope_destructors(*compound_scope, lifetime_depth);
			else
				restore_lifetime_depth(lifetime_depth);
			pop_label_recovery_boundary();
			if (compound_scope != NULL)
			{
				lifetime_scope_stack_.pop_back();
				lifetime_scope_depths_.pop_back();
			}
			break;
		}
		case SemanticFactKind::SimpleDeclaration:
			for (std::size_t i = 0; i < facts.size(); ++i)
				lower_statement(facts[i]);
			break;
		case SemanticFactKind::Variable:
			if (facts.size() > 1) throw std::runtime_error("PA15 invalid local initializer");
			if (facts.size() == 1)
			{
				const LoweredValue storage = storage_for(fact.binding);
				const SemanticFact& initializer =
					model_.semantic_facts_[facts.front().value];
				if (initializer.kind == SemanticFactKind::ConstructorAction)
				{
					if (initializer.value_initialize)
						initialize_constructor_value(model_.binding(fact.binding).type,
							facts.front(), address_of_storage(storage));
					else if (!constructor_action_is_noop(initializer))
						(void)lower_expression(facts.front());
				}
				else if (storage.type.is_object() &&
					initializer.kind == SemanticFactKind::BracedInitList)
				{
					const TypeId declared_type = model_.binding(fact.binding).type;
					const TypeId object_type = model_.strip_cv_type(
						model_.expression_object_type(declared_type));
					if (model_.type_kind(object_type) == TypeKind::Array)
						initialize_array(fact.binding, facts.front(), storage);
					else
					{
						const LoweredValue address = address_of_storage(storage);
						const std::vector<ConstructorAddressStep> empty_path;
						initialize_constructor_value(declared_type, facts.front(),
							address, NULL, &empty_path, NULL, &storage, declared_type);
					}
				}
				else
				{
					const LoweredValue value = lower_expression(facts.front());
					emit_store(storage.type, value.value, storage.value);
				}
			}
			else if (fact.binding.valid() &&
				model_.binding(fact.binding).kind == BindingKind::Variable)
			{
				const LoweredValue storage = storage_for(fact.binding);
				if (storage.type.is_object() && class_object_type(
					model_.binding(fact.binding).type))
					(void)address_of_storage(storage);
			}
			activate_lifetime(fact.binding);
			break;
		case SemanticFactKind::ExpressionStatement:
			if (facts.size() == 1) lower_discarded_expression(facts.front());
			break;
		case SemanticFactKind::ReturnStatement:
		{
			Instruction instruction;
			instruction.kind = Instruction::IK_RETURN;
			instruction.type = function().return_type;
			if (facts.size() == 1)
			{
				if (instruction.type.is_void())
					lower_discarded_expression(facts.front());
				else
					instruction.first = lower_expression(facts.front()).value;
			}
				else if (!instruction.type.is_void())
					throw std::runtime_error("PA15 missing return operand");
				if (current_block_ != InvalidIdentityValue)
				{
					emit_active_scope_destructors();
					emit_active_destructor_actions();
				}
				block().instructions.push_back(instruction);
			current_block_ = InvalidIdentityValue;
			break;
		}
		case SemanticFactKind::IfStatement:
			lower_if(id, facts);
			break;
		case SemanticFactKind::SwitchStatement:
			lower_switch(id, facts);
			break;
		case SemanticFactKind::WhileStatement:
			lower_while(id);
			break;
		case SemanticFactKind::DoStatement:
			lower_do(id);
			break;
		case SemanticFactKind::ForStatement:
			lower_for(id);
			break;
		case SemanticFactKind::ForInitStatement:
			for (std::size_t i = 0; i < facts.size(); ++i)
			{
				if (model_.semantic_facts_[facts[i].value].kind ==
					SemanticFactKind::SimpleDeclaration)
					lower_statement(facts[i]);
				else
					lower_discarded_expression(facts[i]);
			}
			break;
		case SemanticFactKind::Iteration:
			if (facts.size() != 1)
				throw std::runtime_error("PA15 invalid iteration fact");
			lower_discarded_expression(facts.front());
			break;
		case SemanticFactKind::BreakStatement:
			emit_control_lifetime_destructors(false);
			current_block_ = InvalidIdentityValue;
			break;
		case SemanticFactKind::ContinueStatement:
			emit_control_lifetime_destructors(true);
			current_block_ = InvalidIdentityValue;
			break;
		case SemanticFactKind::CaseStatement:
		{
			const BlockId target = switch_label_target(id);
			if (current_block_ != InvalidIdentityValue &&
				current_block_id() != target && !terminated(block()))
				emit_jump(target);
			set_current(target);
			if (facts.size() != 2)
				throw std::runtime_error("PA15 invalid case statement");
			lower_statement(facts.back());
			break;
		}
		case SemanticFactKind::DefaultStatement:
		{
			const BlockId target = switch_label_target(id);
			if (current_block_ != InvalidIdentityValue &&
				current_block_id() != target && !terminated(block()))
				emit_jump(target);
			set_current(target);
			if (facts.size() != 1)
				throw std::runtime_error("PA15 invalid default statement");
			lower_statement(facts.front());
			break;
		}
		case SemanticFactKind::LabeledStatement:
		{
			if (!fact.label.valid() || fact.label.value >= label_lowered_.size() ||
				facts.size() > 1)
				throw std::runtime_error("PA15 invalid labeled statement");
			const BlockId target = label_target(fact.label);
			if (label_lowered_generations_[fact.label.value] ==
				label_generation_ && label_lowered_[fact.label.value] != 0)
			{
				if (current_block_ != InvalidIdentityValue &&
					current_block_id() != target && !terminated(block()))
					emit_jump(target);
				current_block_ = InvalidIdentityValue;
				break;
			}
			label_lowered_generations_[fact.label.value] = label_generation_;
			label_lowered_[fact.label.value] = 1;
			if (current_block_ != InvalidIdentityValue &&
				current_block_id() != target && !terminated(block()))
				emit_jump(target);
			set_current(target);
			if (facts.size() == 1)
				lower_statement(facts.front());
			break;
		}
		case SemanticFactKind::GotoStatement:
			if (!fact.label.valid() || !facts.empty())
				throw std::runtime_error("PA15 invalid goto statement");
			if (function_has_nontrivial_lifetime_)
				throw std::runtime_error(
					"PA15 goto across active lifetime is outside checkpoint");
			emit_jump(label_target(fact.label));
			current_block_ = InvalidIdentityValue;
			break;
		case SemanticFactKind::ThenBranch:
		case SemanticFactKind::ElseBranch:
			if (facts.size() == 1) lower_scoped_statement(facts.front());
			break;
		default:
			throw std::runtime_error("PA15 unsupported scalar statement fact");
		}
	}

LoweredValue Pa15Lowerer::lower_condition(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const std::vector<SemanticFactId> facts = children(id);
		if (fact.kind == SemanticFactKind::Condition && facts.size() == 1)
			return lower_condition(facts.front());
		if (fact.kind == SemanticFactKind::ConditionDeclaration && facts.size() == 1)
			return lower_condition_expression(facts.front());
		return lower_condition_expression(id);
	}

void Pa15Lowerer::lower_if(SemanticFactId id,
	const std::vector<SemanticFactId>& facts){
		if (facts.size() < 2 || facts.size() > 3)
			throw std::runtime_error("PA15 invalid if fact");
		if (current_block_ == InvalidIdentityValue)
			set_current(block_id(new_block("if_selector")));
		const bool direct = has_direct_short_circuit(facts[0]);
		const bool implicit_else = facts.size() == 2;
		BlockId then_block;
		BlockId else_block;
		BlockId join_block;
		if (direct)
		{
			const std::size_t begin = function().blocks.size();
			then_block = block_id(new_block("if_then"));
			else_block = block_id(new_block("if_else"));
			if (implicit_else)
				join_block = block_id(new_block("if_end"));
			lower_condition_branch(facts[0], then_block, else_block);
			const BlockId saved_current = current_block_id();
			reorder_condition_blocks(begin, implicit_else ? 3 : 2,
				saved_current);
		}
		else
		{
			const LoweredValue condition = lower_condition(facts[0]);
			then_block = block_id(new_block("if_then"));
			else_block = block_id(new_block("if_else"));
			emit_branch(condition.value, then_block, else_block);
		}
		// Condition lowering is complete.  Both mutually exclusive bodies must
		// start from exactly the same typed lifetime state, regardless of what
		// the first body does to the mutable lowering cursor.
		const std::vector<BindingId> incoming_lifetimes = active_lifetimes_;
		active_lifetimes_ = incoming_lifetimes;
		set_current(then_block);
		lower_statement(facts[1]);
		const BlockId then_exit = current_block_id();
		const bool then_terminated = !then_exit.valid() ||
			terminated(then_exit);
		const std::vector<BindingId> then_lifetimes = active_lifetimes_;
		active_lifetimes_ = incoming_lifetimes;
		set_current(else_block);
		if (!implicit_else) lower_statement(facts[2]);
		const BlockId else_exit = current_block_id();
		const bool else_terminated = !else_exit.valid() ||
			terminated(else_exit);
		const std::vector<BindingId> else_lifetimes = active_lifetimes_;
		if (then_terminated && else_terminated)
		{
			// There is no ordinary join.  Preserve only a state shared by all
			// emitted exit paths; choosing one divergent branch would make the
			// enclosing sequential walk resurrect or lose a lifetime.
			if (then_lifetimes != else_lifetimes)
				throw std::runtime_error(
					"PA15 if exits have divergent active lifetime states");
			active_lifetimes_ = then_lifetimes;
			current_block_ = InvalidIdentityValue;
			return;
		}
		if (!then_terminated && !else_terminated &&
			then_lifetimes != else_lifetimes)
			throw std::runtime_error(
				"PA15 if join has divergent active lifetime states");
		if (!join_block.valid())
			join_block = block_id(new_block("if_end"));
		if (!then_terminated)
		{
			active_lifetimes_ = then_lifetimes;
			set_current(then_exit);
			emit_jump(join_block);
		}
		if (!else_terminated)
		{
			active_lifetimes_ = else_lifetimes;
			set_current(else_exit);
			emit_jump(join_block);
		}
		active_lifetimes_ = then_terminated ? else_lifetimes : then_lifetimes;
		store_if_flow(id, IfFlow(then_block, else_block, join_block));
		set_current(join_block);
}

void Pa15Lowerer::lower_function(const FunctionPlan& plan){
		current_function_ = plan.program_index;
		current_block_ = InvalidIdentityValue;
		temp_ordinal_ = 0;
		block_ordinal_ = 0;
		generated_slot_ordinal_ = 0;
		block_indexes_.clear();
		control_stack_.clear();
		switch_stack_.clear();
		lifetime_scope_stack_.clear();
		lifetime_scope_depths_.clear();
		active_lifetimes_.clear();
	recovery_control_head_ = RecoveryControlIndex();
		recovery_control_base_depth_ = 0;
		recovery_control_active_ = false;
		block_order_.clear();
		ordered_block_ids_.clear();
		reachability_base_ = next_block_;
		reachable_blocks_.clear();
		reachability_work_.clear();
		Function& target = function();
		slot_by_binding_ = plan.slot_bindings;
		used_slot_names_.clear();
		slot_collision_counters_.clear();
		for (std::size_t slot = 0; slot < target.slots.size(); ++slot)
			used_slot_names_.insert(spelling(target.slots[slot].name_id));
		used_value_names_.clear();
		for (std::size_t i = 0; i < target.params.size(); ++i)
			used_value_names_.insert(spelling(target.params[i].name_id));
		target.value_begin = ValueId(next_value_);
		for (std::size_t i = 0; i < target.params.size(); ++i)
			target.params[i].value_id = allocate_value();
		const std::size_t value_begin = target.value_begin.index;
		const BlockId entry = block_id(new_block("entry"));
		set_current(entry);
		mark_reachable(entry);
		for (std::size_t i = 0; i < target.params.size(); ++i)
		{
			const lowir_model::Parameter& parameter = target.params[i];
			if (i < target.slots.size())
			{
				Instruction store;
				store.kind = Instruction::IK_STORE;
				store.type = parameter.type;
				store.first = temporary_operand(parameter.value_id, parameter.name_id);
				store.second = slot_operand(target.slots[i].slot_id);
				block().instructions.push_back(store);
			}
		}
		const FunctionFact& fact = model_.function_facts_[plan.fact_index];
		if (!fact.function_scope.valid() || fact.function_scope.value >=
			lifetime_function_scope_flags_.size() ||
			model_.scopes_[fact.function_scope.value].kind != ScopeKind::Function)
			throw std::runtime_error("PA15 function scope is invalid");
		function_has_nontrivial_lifetime_ =
			lifetime_function_scope_flags_[fact.function_scope.value] != 0;
		active_constructor_record_ = NamedRecordId();
		active_constructor_this_ = BindingId();
		active_destructor_record_ = NamedRecordId();
		active_destructor_this_ = BindingId();
		if (fact.is_constructor)
		{
			if (!fact.constructor_record.valid() ||
				fact.constructor_record.value >= model_.named_.size() ||
				!fact.function_scope.valid() ||
				fact.function_scope.value >= model_.scopes_.size() ||
				model_.scopes_[fact.function_scope.value].kind != ScopeKind::Function ||
				model_.scopes_[fact.function_scope.value].parent != fact.owner ||
				!model_.scopes_[fact.function_scope.value].implicit_object_binding.valid())
				throw std::runtime_error("PA15 constructor object parameter is missing");
			const Scope& function_scope = model_.scopes_[fact.function_scope.value];
			const BindingId this_binding = function_scope.implicit_object_binding;
			if (this_binding.value >= model_.bindings_.size() ||
				this_binding.value >= model_.binding_owners_.size() ||
				model_.binding_owners_[this_binding.value] != fact.function_scope ||
				model_.binding(this_binding).kind != BindingKind::Parameter)
				throw std::runtime_error("PA15 constructor object parameter is invalid");
			active_constructor_record_ = fact.constructor_record;
			active_constructor_this_ = this_binding;
			if (fact.constructor_action_begin == InvalidIdentityValue ||
				fact.constructor_action_begin > model_.constructor_actions_.size() ||
				fact.constructor_action_count > model_.constructor_actions_.size() -
				fact.constructor_action_begin)
				throw std::runtime_error("PA15 constructor action range is invalid");
			BitFieldInitializationContext constructor_context;
			for (std::size_t action = 0; action < fact.constructor_action_count;
				++action)
			{
				const ConstructorActionFact& action_fact = model_.constructor_actions_[
					fact.constructor_action_begin + action];
				lower_constructor_action(action_fact, constructor_context);
			}
		}
		else if (fact.is_destructor)
		{
			if (!fact.destructor_record.valid() ||
				fact.destructor_record.value >= model_.named_.size() ||
				!fact.function_scope.valid() ||
				fact.function_scope.value >= model_.scopes_.size() ||
				model_.scopes_[fact.function_scope.value].kind != ScopeKind::Function ||
				model_.scopes_[fact.function_scope.value].parent != fact.owner ||
				!model_.scopes_[fact.function_scope.value].implicit_object_binding.valid())
				throw std::runtime_error("PA15 destructor object parameter is missing");
			const BindingId this_binding = model_.scopes_[
				fact.function_scope.value].implicit_object_binding;
			if (this_binding.value >= model_.bindings_.size() ||
				this_binding.value >= model_.binding_owners_.size() ||
				model_.binding_owners_[this_binding.value] != fact.function_scope ||
				model_.binding(this_binding).kind != BindingKind::Parameter)
				throw std::runtime_error("PA15 destructor object parameter is invalid");
			active_destructor_record_ = fact.destructor_record;
			active_destructor_this_ = this_binding;
		}
		if (fact.body_fact.valid())
		{
			initialize_label_flow(fact.body_fact);
			lower_statement(fact.body_fact);
			drain_label_recovery_queue(fact.body_fact);
		}
		else if ((!fact.is_constructor && !fact.is_destructor) || !fact.synthetic)
			throw std::runtime_error("PA15 function body fact is missing");
		if (!label_recovery_queue_.empty())
			drain_label_recovery_queue(SemanticFactId());
		if (!label_recovery_queue_.empty())
			throw std::runtime_error("PA15 reachable deferred label was not drained");
		if (fact.is_destructor && current_block_ != InvalidIdentityValue &&
			!terminated(block()))
			emit_active_destructor_actions();
		if (current_block_ != InvalidIdentityValue &&
			!terminated(block()))
		{
			if (!active_lifetimes_.empty())
				emit_active_scope_destructors();
			const BlockId continuation = current_block_id();
			if (!is_reachable(continuation))
			{


				terminate_unreachable_block(continuation);
				current_block_ = InvalidIdentityValue;
			}
			else
			{
				if (target.metadata.role == lowir_model::SR_ENTRY &&
					!target.return_type.is_void())
				{
					Instruction instruction;
					instruction.kind = Instruction::IK_RETURN;
					instruction.type = target.return_type;
					instruction.first = integer_operand(0, target.return_type);
					block().instructions.push_back(instruction);
				}
				else if (!target.return_type.is_void())
					throw std::runtime_error("PA15 function falls through without return");
				else
				{
					Instruction instruction;
					instruction.kind = Instruction::IK_RETURN;
					instruction.type = target.return_type;
					block().instructions.push_back(instruction);
				}
			}
		}
		target.value_count = next_value_ - value_begin;
		target.slot_count = next_slot_ - target.slot_begin.index;
		reorder_function_blocks();
		active_constructor_record_ = NamedRecordId();
		active_constructor_this_ = BindingId();
		active_destructor_record_ = NamedRecordId();
		active_destructor_this_ = BindingId();
		lifetime_scope_stack_.clear();
		lifetime_scope_depths_.clear();
		active_lifetimes_.clear();
		function_has_nontrivial_lifetime_ = false;
	}
}
