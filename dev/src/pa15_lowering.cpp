#include "pa15_lowering.h"
#include <cstring>

namespace pa11_semantic_internal
{

Pa15Lowerer::Pa15Lowerer(const PA11SemanticModel& model, Program& program)
		: model_(model), program_(program), spelling_ids_(), used_symbols_(),
		  used_slot_names_(), used_value_names_(), symbol_collision_counters_(),
		  slot_collision_counters_(), function_symbols_(),
		  function_name_ids_(), function_declaration_plans_(),
		  demanded_function_declarations_(), demanded_member_declarations_(),
		  demanded_member_declaration_types_(),
		  global_symbols_(), global_name_ids_(),
		  thread_local_by_binding_(), required_global_bindings_(),
		  emitted_tls_wrappers_(), tls_guard_symbols_(), tls_guard_name_ids_(),
		  tls_init_name_ids_(),
		  symbol_name_ids_(), literal_address_symbols_(), literal_content_symbols_(), label_blocks_(),
		  label_referenced_(), label_subtrees_(), label_index_states_(),
		  label_subtree_states_(), label_lowered_(), label_block_generations_(),
		  label_referenced_generations_(), fact_index_generations_(),
		  fact_subtree_generations_(), label_lowered_generations_(),
		  label_recovery_waiting_generations_(),
		  label_recovery_queued_generations_(), label_statement_facts_(),
		  fact_parents_(), fact_parent_indexes_(), fact_recovery_frames_(),
		  fact_recovery_frame_children_(), fact_recovery_frame_indexes_(),
		  fact_recovery_orders_(), fact_recovery_ends_(),
		  fact_switch_ancestors_(), fact_recovery_control_heads_(),
		  recovery_control_arena_(),
		  label_recovery_root_(),
		  label_recovery_order_(0),
		  label_recovery_boundaries_(),
		  label_recovery_queue_(),
		  label_generation_(0), recovery_control_head_(),
		  recovery_control_base_depth_(0), recovery_control_active_(false),
		  variable_facts_(),
		  declaration_by_binding_(), slot_by_binding_(), slot_spellings_(),
		  slot_source_begins_(), function_plans_(), pending_global_actions_(),
		  bit_field_address_projections_(),
		  needs_trivial_namespace_object_init_(false),
		  function_scope_variables_(), next_symbol_(0),
		  literal_backing_ordinal_(0), next_value_(program.values.size()),
		  next_slot_(0), next_block_(0), current_function_(0),
		  current_block_(InvalidIdentityValue), temp_ordinal_(0),
		  block_ordinal_(0), generated_slot_ordinal_(0),
		  active_constructor_record_(), active_constructor_this_(),
		  active_destructor_record_(), active_destructor_this_(), active_destructor_cleanup_(),
		  lifetime_by_binding_(), lifetime_function_scope_flags_(),
		  lifetime_scope_stack_(),
		  lifetime_scope_depths_(), active_lifetimes_(),
		  function_has_nontrivial_lifetime_(false),
		  constructor_nothrow_states_(), constructor_nothrow_results_(),
		  constructor_nothrow_invalid_(), semantic_nothrow_states_(),
		  semantic_nothrow_results_(), semantic_nothrow_invalid_(),
		  block_indexes_(), control_stack_(),
		  switch_stack_(), loop_flow_indexes_(), loop_flow_arena_(),
		  if_flow_indexes_(), if_flow_arena_(), switch_flow_indexes_(),
		  switch_flow_arena_(),
		  continuation_indexes_(), continuation_arena_(),
		  fact_recovery_exit_indexes_(),
		  block_order_(), ordered_block_ids_(),
	  reachability_base_(0), reachable_blocks_(),
	  reachability_work_(), constant_truth_cache_(){}

void Pa15Lowerer::run(){
		initialize_spelling_ids();
		initialize_identity_counters();
		clear_value_records();
		bit_field_address_projections_.clear();
		index_binding_facts();
		collect_functions();
		collect_function_declarations();
		index_global_storage_demands();
		collect_globals();
		materialize_pending_global_initializers();
		materialize_namespace_lifetime_destructors();
		constant_truth_cache_.assign(model_.semantic_facts_.size(), 255);
		const std::size_t fact_count = model_.semantic_facts_.size();
		loop_flow_indexes_.assign(fact_count, LoopFlowIndex());
		loop_flow_arena_.clear();
		if_flow_indexes_.assign(fact_count, IfFlowIndex());
		if_flow_arena_.clear();
		switch_flow_indexes_.assign(fact_count, SwitchFlowIndex());
		switch_flow_arena_.clear();
		continuation_indexes_.assign(fact_count, ContinuationIndex());
		continuation_arena_.clear();
		fact_recovery_control_heads_.assign(fact_count,
			RecoveryControlIndex());
		recovery_control_arena_.clear();
		initialize_label_storage();
		for (std::size_t i = 0; i < function_plans_.size(); ++i)
			lower_function(function_plans_[i]);
		materialize_function_declarations();
		finalize_value_records();
	}

void Pa15Lowerer::initialize_spelling_ids(){
		for (std::size_t i = 0; i < program_.presentation.size(); ++i)
			spelling_ids_[program_.presentation[i]] = SpellingId(i);
	}

void Pa15Lowerer::initialize_identity_counters(){
		for (std::size_t i = 0; i < program_.global_declarations.size(); ++i)
		{
			if (program_.global_declarations[i].symbol_id.valid())
				next_symbol_ = std::max(next_symbol_,
					program_.global_declarations[i].symbol_id.index + 1);
			if (program_.global_declarations[i].symbol_id.valid())
				symbol_name_ids_[program_.global_declarations[i].symbol_id.index] =
					program_.global_declarations[i].name_id;
		}
		for (std::size_t i = 0; i < program_.globals.size(); ++i)
		{
			if (program_.globals[i].symbol_id.valid())
				next_symbol_ = std::max(next_symbol_,
					program_.globals[i].symbol_id.index + 1);
			if (program_.globals[i].symbol_id.valid())
				symbol_name_ids_[program_.globals[i].symbol_id.index] =
					program_.globals[i].name_id;
		}
		for (std::size_t i = 0; i < program_.functions.size(); ++i)
		{
			const Function& function = program_.functions[i];
			if (function.symbol_id.valid())
			{
				next_symbol_ = std::max(next_symbol_,
					function.symbol_id.index + 1);
				symbol_name_ids_[function.symbol_id.index] = function.name_id;
			}
			for (std::size_t j = 0; j < function.slots.size(); ++j)
				if (function.slots[j].slot_id.valid())
				{
					next_slot_ = std::max(next_slot_,
						function.slots[j].slot_id.index + 1);
					if (slot_spellings_.size() <= function.slots[j].slot_id.index)
						slot_spellings_.resize(function.slots[j].slot_id.index + 1);
					slot_spellings_[function.slots[j].slot_id.index] =
						function.slots[j].name_id;
				}
			for (std::size_t j = 0; j < function.blocks.size(); ++j)
				if (function.blocks[j].block_id.valid())
					next_block_ = std::max(next_block_,
						function.blocks[j].block_id.index + 1);
		}
		for (std::size_t i = 0; i < program_.function_declarations.size(); ++i)
		{
			const FunctionDeclaration& declaration =
				program_.function_declarations[i];
			if (!declaration.symbol_id.valid()) continue;
			next_symbol_ = std::max(next_symbol_, declaration.symbol_id.index + 1);
			symbol_name_ids_[declaration.symbol_id.index] = declaration.name_id;
		}
		for (std::size_t i = 0; i < program_.global_declarations.size(); ++i)
			used_symbols_.insert(spelling(program_.global_declarations[i].name_id));
		for (std::size_t i = 0; i < program_.globals.size(); ++i)
			used_symbols_.insert(spelling(program_.globals[i].name_id));
		for (std::size_t i = 0; i < program_.functions.size(); ++i)
			used_symbols_.insert(spelling(program_.functions[i].name_id));
		for (std::size_t i = 0; i < program_.function_declarations.size(); ++i)
			used_symbols_.insert(spelling(program_.function_declarations[i].name_id));
	}

const std::string& Pa15Lowerer::spelling(SpellingId id) const{
		if (!id.valid() || id.index >= program_.presentation.size())
			throw std::runtime_error("PA15 invalid presentation identity");
		return program_.presentation[id.index];
	}

SpellingId Pa15Lowerer::intern_spelling(const std::string& value){
		std::map<std::string, SpellingId>::const_iterator found =
			spelling_ids_.find(value);
		if (found != spelling_ids_.end())
			return found->second;
		const SpellingId result(program_.presentation.size());
		program_.presentation.push_back(value);
		spelling_ids_[value] = result;
		return result;
	}

SpellingId Pa15Lowerer::symbol_spelling(const std::string& name){
		const std::string base = "@" + name;
		std::string result = base;
		if (used_symbols_.find(result) != used_symbols_.end())
		{
			std::size_t& suffix = symbol_collision_counters_[base];
			if (suffix == 0) suffix = 2;
			std::ostringstream candidate;
			candidate << base << "__" << suffix++;
			while (used_symbols_.find(candidate.str()) != used_symbols_.end())
			{
				candidate.str(std::string());
				candidate.clear();
				candidate << base << "__" << suffix++;
			}
			result = candidate.str();
		}
		used_symbols_.insert(result);
		return intern_spelling(result);
	}

std::size_t Pa15Lowerer::begin_generated_function(const std::string& base,
	lowir_model::SymbolRole role)
{
		return begin_generated_function(symbol_spelling(base), role);
	}

std::size_t Pa15Lowerer::begin_generated_function(SpellingId name_id,
	lowir_model::SymbolRole role)
{
		if (!name_id.valid())
			throw std::runtime_error("PA15 generated function name is missing");
		Function generated;
		generated.symbol_id = SymbolId(next_symbol_++);
		generated.name_id = name_id;
		generated.return_type.kind = LowType::TYPE_VOID;
		generated.metadata.role = role;
		generated.metadata.binding = lowir_model::SBM_INTERNAL;
		generated.value_begin = ValueId(next_value_);
		const std::size_t function_index = program_.functions.size();
		program_.functions.push_back(generated);
		symbol_name_ids_[generated.symbol_id.index] = generated.name_id;
		current_function_ = function_index;
		current_block_ = InvalidIdentityValue;
		temp_ordinal_ = 0;
		block_ordinal_ = 0;
		generated_slot_ordinal_ = 0;
		block_indexes_.clear();
		block_order_.clear();
		ordered_block_ids_.clear();
		reachability_base_ = next_block_;
		reachable_blocks_.clear();
		reachability_work_.clear();
		used_value_names_.clear();
		used_slot_names_.clear();
		slot_collision_counters_.clear();
		const std::size_t entry_index = new_block("entry");
		set_current(block_id(entry_index));
		mark_reachable(current_block_id());
		return function_index;
	}

void Pa15Lowerer::finish_generated_function()
{
		if (current_function_ == InvalidIdentityValue ||
			current_function_ >= program_.functions.size() ||
			current_block_ == InvalidIdentityValue)
			throw std::runtime_error("PA15 generated function is incomplete");
		Function& generated = function();
		generated.value_count = next_value_ - generated.value_begin.index;
		generated.slot_count = 0;
		reorder_function_blocks();
	}

std::string Pa15Lowerer::namespace_component(ScopeId scope) const
{
	if (!scope.valid() || scope.value >= model_.scopes_.size() ||
		model_.scopes_[scope.value].kind != ScopeKind::Namespace)
		throw std::runtime_error("PA15 namespace component scope is invalid");
	const Scope& namespace_scope = model_.scopes_[scope.value];
	if (namespace_scope.name.valid())
		return model_.name_text(namespace_scope.name);
	if (!namespace_scope.unnamed_namespace)
		return std::string();
	return "_GLOBAL__N_1";
}

std::vector<std::string> Pa15Lowerer::function_components(const FunctionFact& fact) const{
		std::vector<std::string> reversed;
		ScopeId scope = fact.owner;
		while (scope.valid())
		{
			const Scope& current = model_.scopes_[scope.value];
			if (current.kind == ScopeKind::Namespace)
			{
				const std::string component = namespace_component(scope);
				if (!component.empty()) reversed.push_back(component);
			}
			else if (current.kind == ScopeKind::Class && current.record.valid() &&
				current.record.value < model_.named_.size() &&
				model_.named_[current.record.value].name.valid())
				reversed.push_back(model_.name_text(
					model_.named_[current.record.value].name));
			scope = current.parent;
	}
	std::reverse(reversed.begin(), reversed.end());
	const BindingSidecar* sidecar = model_.binding_sidecar(fact.binding);
	const bool destructor = fact.is_destructor || (sidecar != NULL &&
		sidecar->destructor_record.valid());
	if (destructor)
	{
		const NamedRecordId record = fact.destructor_record.valid() ?
			fact.destructor_record : sidecar->destructor_record;
		if (!record.valid() || record.value >= model_.named_.size() ||
			!model_.named_[record.value].name.valid())
			throw std::runtime_error("PA15 destructor symbol owner is missing");
		reversed.push_back("_" + model_.name_text(
			model_.named_[record.value].name));
	}
	else
		reversed.push_back(model_.name_text(
			model_.binding(fact.binding).name));
	if (fact.constructor_base_entry || fact.destructor_base_entry)
		reversed.push_back("base_entry");
	return reversed;
}

std::vector<std::string> Pa15Lowerer::value_components(ScopeId owner, NameId name) const{
		std::vector<std::string> reversed;
		ScopeId scope = owner;
		while (scope.valid())
		{
			const Scope& current = model_.scopes_[scope.value];
			if (current.kind == ScopeKind::Namespace)
			{
				const std::string component = namespace_component(scope);
				if (!component.empty()) reversed.push_back(component);
			}
			else if (current.kind == ScopeKind::Class && current.record.valid() &&
				current.record.value < model_.named_.size() &&
				model_.named_[current.record.value].name.valid())
				reversed.push_back(model_.name_text(
					model_.named_[current.record.value].name));
			scope = current.parent;
		}
		std::reverse(reversed.begin(), reversed.end());
		reversed.push_back(model_.name_text(name));
		return reversed;
	}

LowType Pa15Lowerer::low_type(TypeId type) const{
		while (type.valid() && model_.type_kind(type) == TypeKind::Cv)
			type = model_.types_[type.value].child;
		if (!type.valid())
			throw std::runtime_error("PA15 invalid semantic type");
		const TypeKind kind = model_.type_kind(type);
		if (kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference)
		{
			LowType result;
			result.kind = LowType::TYPE_POINTER;
			return result;
		}
		if (kind == TypeKind::Array)
		{
			LowType result;
			result.kind = LowType::TYPE_OBJECT;

			result.object_bytes = model_.types_[type.value].unknown_bound ? 0 :
				model_.type_size(type);
			const LowType element = low_type(model_.types_[type.value].child);
			// Complete arrays use semantic element alignment; unknown-bound arrays
			// retain the LowIR fallback because they have no complete layout.
			result.object_alignment = model_.types_[type.value].unknown_bound ?
				element.storage_alignment() : model_.type_alignment(type);
			if (result.object_alignment == 0) result.object_alignment = 1;
			return result;
		}
		if (kind == TypeKind::Pointer)
		{
			LowType result;
			result.kind = LowType::TYPE_POINTER;
			return result;
		}
		if (kind == TypeKind::Named)
		{
			const NamedRecordId record = model_.types_[type.value].named;
			if (record.valid() && record.value < model_.named_.size() &&
				model_.named_[record.value].kind == NamedKind::Enum)
			{
				const NamedRecord& named = model_.named_[record.value];
				return low_type(named.has_underlying ? named.underlying :
					model_.fundamental(FundamentalType::Int));
			}
			if (record.valid() && record.value < model_.named_.size() &&
				model_.named_[record.value].kind == NamedKind::Class)
			{
				const RecordLayout& layout = model_.record_layout(record);
				if (layout.state != RecordLayoutState::Complete ||
					layout.size == 0 || layout.alignment == 0)
					throw std::runtime_error(
						"PA15 class storage requires a complete record layout");
				LowType result;
				result.kind = LowType::TYPE_OBJECT;
				result.object_bytes = layout.size;
				result.object_alignment = layout.alignment;
				return result;
			}
			throw std::runtime_error("PA15 unsupported named scalar type");
		}
		if (kind != TypeKind::Fundamental)
			throw std::runtime_error("PA15 unsupported scalar type");
		LowType result;
		switch (model_.types_[type.value].fundamental)
		{
		case FundamentalType::Void:
			result.kind = LowType::TYPE_VOID; return result;
		case FundamentalType::Bool:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_U8; return result;
		case FundamentalType::SignedChar:
		case FundamentalType::Char:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_I8; return result;
		case FundamentalType::UnsignedChar:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_U8; return result;
		case FundamentalType::ShortInt:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_I16; return result;
		case FundamentalType::UnsignedShortInt:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_U16; return result;
		case FundamentalType::Int:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_I32; return result;
		case FundamentalType::UnsignedInt:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_U32; return result;
		case FundamentalType::LongInt:
		case FundamentalType::LongLongInt:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_I64; return result;
		case FundamentalType::NullptrT:
			// NullptrT stays a distinct semantic/ABI type; PA15 carries its
			// value in the canonical 64-bit integer slot.
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_I64; return result;
		case FundamentalType::UnsignedLongInt:
		case FundamentalType::UnsignedLongLongInt:
			// PA13's scalar LowIR contract exposes the 64-bit slot as i64.
			// Enum lowering reaches this path through PA11's selected
			// underlying representation, so keep the boundary canonical.
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_I64; return result;
		case FundamentalType::WcharT:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_I32; return result;
		case FundamentalType::Char16T:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_U16; return result;
		case FundamentalType::Char32T:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_U32; return result;
		case FundamentalType::Float:
			result.kind = LowType::TYPE_FLOAT; result.float_kind = LowType::FLOAT_F32; return result;
		case FundamentalType::Double:
			result.kind = LowType::TYPE_FLOAT; result.float_kind = LowType::FLOAT_F64; return result;
		case FundamentalType::LongDouble:
			result.kind = LowType::TYPE_FLOAT; result.float_kind = LowType::FLOAT_F80; return result;
		default:
			throw std::runtime_error("PA15 unsupported fundamental type");
	}
}

void Pa15Lowerer::index_binding_facts(){
		index_lifetime_facts();
		if (model_.declaration_definition_flags_.size() !=
			model_.declaration_bindings_.size())
			throw std::runtime_error("PA15 declaration definition arena is discontinuous");
		for (std::size_t i = 0; i < model_.declaration_facts_.size(); ++i)
		{
			const DeclarationFact& declaration = model_.declaration_facts_[i];
			if (declaration.lifetime_begin == InvalidIdentityValue &&
				declaration.lifetime_count != 0)
				throw std::runtime_error("PA15 declaration lifetime range is invalid");
			if (declaration.lifetime_begin != InvalidIdentityValue &&
				(declaration.lifetime_begin > model_.lifetime_facts_.size() ||
				 declaration.lifetime_count > model_.lifetime_facts_.size() -
					declaration.lifetime_begin))
				throw std::runtime_error("PA15 declaration lifetime range is invalid");
			if (declaration.binding_begin == InvalidIdentityValue)
				continue;
			if (declaration.binding_begin > model_.declaration_bindings_.size() ||
				declaration.binding_count > model_.declaration_bindings_.size() -
					declaration.binding_begin)
				throw std::runtime_error("PA15 declaration binding range is invalid");
			if (declaration.binding_begin > model_.declaration_definition_flags_.size() ||
				declaration.binding_count > model_.declaration_definition_flags_.size() -
					declaration.binding_begin)
				throw std::runtime_error("PA15 declaration definition range is invalid");
			if (declaration.semantic_begin != InvalidIdentityValue &&
				(declaration.semantic_begin > model_.declaration_semantic_ids_.size() ||
				 declaration.semantic_count > model_.declaration_semantic_ids_.size() -
					declaration.semantic_begin))
				throw std::runtime_error("PA15 declaration semantic range is invalid");
			for (std::size_t j = 0; j < declaration.binding_count; ++j)
			{
				const unsigned char definition_flag =
					model_.declaration_definition_flags_[declaration.binding_begin + j];
				if (definition_flag > 1)
					throw std::runtime_error("PA15 declaration definition flag is invalid");
				const BindingId binding = model_.declaration_bindings_[
					declaration.binding_begin + j];
				if (!binding.valid() || binding.value >= model_.bindings_.size())
					throw std::runtime_error("PA15 declaration binding identity is invalid");
				if (declaration.is_thread_local)
					thread_local_by_binding_[binding.value] = true;
				if (declaration.semantic_begin != InvalidIdentityValue &&
					j < declaration.semantic_count)
				{
					const SemanticFactId candidate = model_.declaration_semantic_ids_[
						declaration.semantic_begin + j];
					if (!candidate.valid() || candidate.value >=
						model_.semantic_facts_.size())
						throw std::runtime_error(
							"PA15 declaration semantic identity is invalid");
					std::map<std::size_t, SemanticFactId>::const_iterator existing =
						variable_facts_.find(binding.value);
					const bool candidate_has_initializer =
						model_.semantic_facts_[candidate.value].child_count != 0;
					const bool existing_has_initializer = existing !=
						variable_facts_.end() && model_.semantic_facts_[
							existing->second.value].child_count != 0;
					// A later declaration may legally share the canonical binding but
					// has no initializer.  Preserve the earlier definition fact so
					// storage emission cannot erase its typed initializer.
					if (existing == variable_facts_.end() ||
						(candidate_has_initializer && !existing_has_initializer))
						variable_facts_[binding.value] = candidate;
				}
				// PA11's typed per-declarator definition bit is the sole owner
				// signal.  A bodyless redeclaration shares BindingId but must not
				// move construction or destruction relative to neighboring objects.
				if (declaration_by_binding_.find(binding.value) ==
					declaration_by_binding_.end() || definition_flag != 0)
					declaration_by_binding_[binding.value] = &declaration;
			}
		}
	}

bool Pa15Lowerer::constant_integer(SemanticFactId id, const LowType& type, Operand* result){
		if (!id.valid() || id.value >= model_.semantic_facts_.size() ||
			!type.is_integer())
			return false;
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		ConstValue value;
		if (fact.has_constant_value)
			value = ConstValue(true, fact.constant_value,
				fact.constant_value_unsigned);
		else if (fact.has_literal_value)
		{
			__int128 raw = static_cast<__int128>(fact.literal_value);
			if (fact.literal_value_negative) raw = -raw;
			value = ConstValue(true, raw, fact.literal_value_unsigned);
		}
		if (!value.valid) return false;
		*result = integer_operand(static_cast<long long>(value.value), type);
		return true;
	}

bool Pa15Lowerer::typed_pointer_zero(SemanticFactId id,
	TypeId destination) const{
		if (!id.valid() || id.value >= model_.semantic_facts_.size())
			return false;
		if (!destination.valid() || !model_.pointer_id(destination))
			return false;
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.conversion_begin == InvalidIdentityValue ||
			fact.conversion_count == 0 || fact.conversion_begin >
			model_.conversion_facts_.size() || fact.conversion_count >
			model_.conversion_facts_.size() - fact.conversion_begin)
			return false;
		std::size_t null_conversion = InvalidIdentityValue;
		for (std::size_t i = 0; i < fact.conversion_count; ++i)
		{
			const ConversionFact& conversion = model_.conversion_facts_[
				fact.conversion_begin + i];
			if (conversion.kind == ConversionKind::NullptrToPointer ||
				conversion.kind == ConversionKind::NullIntegerToPointer)
			{
				if (null_conversion != InvalidIdentityValue)
					return false;
				null_conversion = i;
			}
		}
		if (null_conversion == InvalidIdentityValue)
			return false;
		if (null_conversion != 0)
			return false;
		for (std::size_t i = null_conversion; i < fact.conversion_count; ++i)
		{
			const ConversionFact& conversion = model_.conversion_facts_[
				fact.conversion_begin + i];
			if (!conversion.source.valid() || !conversion.target.valid() ||
				(i != null_conversion && conversion.source !=
				model_.conversion_facts_[fact.conversion_begin + i - 1].target))
				return false;
			if (i == null_conversion)
			{
				if (conversion.kind != ConversionKind::NullptrToPointer &&
					conversion.kind != ConversionKind::NullIntegerToPointer)
					return false;
			}
			else if (conversion.kind != ConversionKind::Identity &&
				conversion.kind != ConversionKind::LvalueToRvalue &&
				conversion.kind != ConversionKind::PointerQualification &&
				conversion.kind != ConversionKind::PointerToVoid)
				return false;
			if (!model_.pointer_id(conversion.target))
				return false;
		}
		const ConversionFact& terminal = model_.conversion_facts_[
			fact.conversion_begin + fact.conversion_count - 1];
		// The terminal typed target is the initializer destination.  strip_cv_type
		// removes only an outer cv wrapper; pointer-object cv remains part of the
		// pointer TypeId, while pointee cv remains part of its child TypeId.
		return model_.strip_cv_type(terminal.target) ==
			model_.strip_cv_type(destination);
	}

std::string Pa15Lowerer::internal_value_name(ScopeId owner, NameId name) const{
		const std::vector<std::string> components = value_components(owner, name);
		std::string result;
		for (std::size_t i = 0; i < components.size(); ++i)
		{
			if (i != 0) result += "__";
			result += components[i];
		}
		return result;
	}

SpellingId Pa15Lowerer::symbol_name_for(SymbolId target) const{
		const std::map<std::size_t, SpellingId>::const_iterator found =
			symbol_name_ids_.find(target.index);
		if (found != symbol_name_ids_.end()) return found->second;
		return SpellingId();
	}

void Pa15Lowerer::global_declaration_position(BindingId binding_id,
	const DeclarationFact* declaration, std::size_t* source_declaration,
	std::size_t* source_declarator) const
{
	if (source_declaration == NULL || source_declarator == NULL ||
		declaration == NULL || model_.declaration_facts_.empty())
		throw std::runtime_error("PA15 global declaration order is missing");
	const DeclarationFact* declaration_begin =
		&model_.declaration_facts_.front();
	const DeclarationFact* declaration_end = declaration_begin +
		model_.declaration_facts_.size();
	if (declaration < declaration_begin || declaration >= declaration_end ||
		declaration->binding_begin == InvalidIdentityValue ||
		declaration->binding_begin > model_.declaration_bindings_.size() ||
		declaration->binding_count > model_.declaration_bindings_.size() -
		declaration->binding_begin ||
		declaration->binding_begin > model_.declaration_definition_flags_.size() ||
		declaration->binding_count > model_.declaration_definition_flags_.size() -
		declaration->binding_begin)
		throw std::runtime_error("PA15 global declaration order is invalid");
	*source_declaration = static_cast<std::size_t>(declaration - declaration_begin);
	*source_declarator = InvalidIdentityValue;
	for (std::size_t declarator = 0; declarator < declaration->binding_count;
		++declarator)
		if (model_.declaration_bindings_[declaration->binding_begin + declarator] ==
			binding_id)
		{
			*source_declarator = declarator;
			break;
		}
	if (*source_declarator == InvalidIdentityValue)
		throw std::runtime_error("PA15 global declaration position is missing");
}


void Pa15Lowerer::materialize_pending_global_initializers(){
		if (pending_global_actions_.empty() &&
			!needs_trivial_namespace_object_init_)
			return;
		std::stable_sort(pending_global_actions_.begin(),
			pending_global_actions_.end(),
			[](const PendingGlobalAction& left, const PendingGlobalAction& right) {
				if (left.source_declaration != right.source_declaration)
					return left.source_declaration < right.source_declaration;
				return left.source_declarator < right.source_declarator;
			});
		bool ordinary_initialization = needs_trivial_namespace_object_init_;
		for (std::size_t i = 0; i < pending_global_actions_.size(); ++i)
			if (pending_global_actions_[i].kind !=
				PendingGlobalAction::THREAD_LOCAL_CONSTRUCTION)
				ordinary_initialization = true;
		if (ordinary_initialization)
		{
			begin_generated_function("__cppgm_init", lowir_model::SR_INIT);
			for (std::size_t i = 0; i < pending_global_actions_.size(); ++i)
			{
				const PendingGlobalAction& pending = pending_global_actions_[i];
				if (pending.kind == PendingGlobalAction::THREAD_LOCAL_CONSTRUCTION)
					continue;
				const SpellingId global_name = symbol_name_for(pending.global);
				if (!global_name.valid())
					throw std::runtime_error("PA15 init global has no symbol name");
				if (pending.kind == PendingGlobalAction::ADDRESS_PROJECTION)
				{
					const SpellingId target_name = symbol_name_for(pending.target);
					if (!target_name.valid() || !pending.element_type.valid())
						throw std::runtime_error("PA15 init target has no symbol name");
					LoweredValue storage(global_operand(pending.target, target_name),
						pending.element_type, true);
					const LoweredValue address = address_of_storage(storage);
					const LoweredValue sequence = emit_decay(address);
					const LoweredValue index(pending.index, pending.index.literal_type, false);
					const LoweredValue element = emit_index(sequence, index,
						pending.element_type, lowir_model::IPK_ARRAY_ELEMENT);
					LowType pointer;
					pointer.kind = LowType::TYPE_POINTER;
					const Operand destination = global_operand(pending.global, global_name);
					emit_store(pointer, element.value, destination);
				}
				else if (pending.kind == PendingGlobalAction::SCALAR_VALUE)
				{
					if (!pending.type.valid() || !pending.initializer.valid())
						throw std::runtime_error("PA15 scalar init target is incomplete");
					const LowType type = low_type(pending.type);
					const LoweredValue destination(global_operand(pending.global,
						global_name), type, true);
					const LoweredValue value = lower_expression(pending.initializer);
					if (destination.type.is_object() || destination.type.is_void())
						throw std::runtime_error("PA15 scalar init target is not scalar");
					emit_store(destination.type, value.value, destination.value);
				}
				else if (pending.kind == PendingGlobalAction::AGGREGATE_VALUE)
				{
					if (!pending.type.valid() || !pending.initializer.valid())
						throw std::runtime_error("PA15 aggregate init target is incomplete");
					const std::vector<SemanticFactId> initializer_children =
						children(pending.initializer);
					const bool call_shaped_constructor =
						model_.semantic_facts_[pending.initializer.value].kind ==
							SemanticFactKind::ConstructorAction &&
						initializer_children.size() == 1 &&
						model_.semantic_facts_[initializer_children.front().value].kind ==
							SemanticFactKind::CallExpression;
					if (call_shaped_constructor)
						(void)lower_expression(initializer_children.front());
					else
					{
						const LowType object_type = low_type(pending.type);
						LoweredValue storage(global_operand(pending.global, global_name),
							object_type, true);
						const std::vector<ConstructorAddressStep> empty_path;
						initialize_constructor_value(pending.type, pending.initializer,
							storage, NULL, &empty_path, NULL, &storage, pending.type);
					}
				}
				else
					throw std::runtime_error("PA15 unknown pending global action");
			}
			Instruction ret;
			ret.kind = Instruction::IK_RETURN;
			ret.type.kind = LowType::TYPE_VOID;
			block().instructions.push_back(ret);
			finish_generated_function();
		}
		for (std::size_t i = 0; i < pending_global_actions_.size(); ++i)
		{
			const PendingGlobalAction& pending = pending_global_actions_[i];
			if (pending.kind != PendingGlobalAction::THREAD_LOCAL_CONSTRUCTION)
				continue;
			if (!pending.binding.valid() || !pending.initializer.valid())
				throw std::runtime_error("PA15 TLS initializer identity is incomplete");
			const std::map<std::size_t, SymbolId>::const_iterator guard_symbol =
				tls_guard_symbols_.find(pending.binding.value);
			const std::map<std::size_t, SpellingId>::const_iterator guard_name =
				tls_guard_name_ids_.find(pending.binding.value);
			if (guard_symbol == tls_guard_symbols_.end() ||
				guard_name == tls_guard_name_ids_.end())
				throw std::runtime_error("PA15 TLS initializer guard is missing");
			const std::map<std::size_t, SpellingId>::const_iterator init_name =
				tls_init_name_ids_.find(pending.binding.value);
			if (init_name == tls_init_name_ids_.end())
				throw std::runtime_error("PA15 TLS initializer name is missing");
			const SpellingId global_name = symbol_name_for(pending.global);
			if (!global_name.valid())
				throw std::runtime_error("PA15 TLS initializer global is missing");
			begin_generated_function(init_name->second, lowir_model::SR_NONE);
			LowType i64;
			i64.kind = LowType::TYPE_INTEGER;
			i64.integer_kind = LowType::INTEGER_I64;
			const LoweredValue guard(global_operand(guard_symbol->second,
				guard_name->second), i64, true);
			const ValueId loaded_guard_id = emit_load(guard, i64);
			const Instruction& loaded_guard_instruction = block().instructions.back();
			const LoweredValue loaded_guard(temporary_operand(loaded_guard_id,
				loaded_guard_instruction.destination_name_id), i64, false);
			const LoweredValue initialized = emit_compare_value(lowir_model::CPP_NE,
				i64, loaded_guard, LoweredValue(integer_operand(0, i64), i64, false));
			const BlockId run = block_id(new_block("local_static_ctor_run"));
			const BlockId done = block_id(new_block("local_static_ctor_done"));
			emit_branch(initialized.value, done, run);
			set_current(run);
			const LoweredValue storage(global_operand(pending.global, global_name),
				low_type(pending.type), true);
			const std::vector<SemanticFactId> initializer_children =
				children(pending.initializer);
			const bool call_shaped_constructor =
				model_.semantic_facts_[pending.initializer.value].kind ==
					SemanticFactKind::ConstructorAction &&
				initializer_children.size() == 1 &&
				model_.semantic_facts_[initializer_children.front().value].kind ==
					SemanticFactKind::CallExpression;
			if (call_shaped_constructor)
				(void)lower_expression(initializer_children.front());
			else
			{
				const std::vector<ConstructorAddressStep> empty_path;
				initialize_constructor_value(pending.type, pending.initializer,
					storage, NULL, &empty_path, NULL, &storage, pending.type);
			}
			emit_store(i64, integer_operand(1, i64), guard.value);
			emit_jump(done);
			set_current(done);
			Instruction ret;
			ret.kind = Instruction::IK_RETURN;
			ret.type.kind = LowType::TYPE_VOID;
			block().instructions.push_back(ret);
			finish_generated_function();
		}
	}

void Pa15Lowerer::materialize_namespace_lifetime_destructors(){
	struct OrderedLifetime
	{
		const LifetimeFact* fact;
		std::size_t declaration;
		std::size_t declarator;
	};
	std::vector<OrderedLifetime> lifetimes;
	for (std::size_t i = 0; i < model_.lifetime_facts_.size(); ++i)
	{
		const LifetimeFact& lifetime = model_.lifetime_facts_[i];
		if (lifetime.storage != LifetimeStorageKind::Namespace)
			continue;
		const std::map<std::size_t, const DeclarationFact*>::const_iterator declaration =
			declaration_by_binding_.find(lifetime.object.value);
		if (declaration == declaration_by_binding_.end() || declaration->second == NULL)
			throw std::runtime_error("PA15 namespace lifetime declaration is missing");
		OrderedLifetime ordered;
		ordered.fact = &lifetime;
		global_declaration_position(lifetime.object, declaration->second,
			&ordered.declaration, &ordered.declarator);
		lifetimes.push_back(ordered);
	}
	if (lifetimes.empty()) return;
	std::stable_sort(lifetimes.begin(), lifetimes.end(),
		[](const OrderedLifetime& left, const OrderedLifetime& right) {
			if (left.declaration != right.declaration)
				return left.declaration < right.declaration;
			return left.declarator < right.declarator;
		});
	begin_generated_function("__cppgm_fini", lowir_model::SR_FINI);
	for (std::size_t i = lifetimes.size(); i != 0; --i)
	{
		const LifetimeFact& lifetime = *lifetimes[i - 1].fact;
		const LoweredValue storage = storage_for(lifetime.object);
		emit_destructor_elements(lifetime.object_type,
			address_of_storage(storage), lifetime.destructor);
	}
	Instruction ret;
	ret.kind = Instruction::IK_RETURN;
	ret.type.kind = LowType::TYPE_VOID;
	block().instructions.push_back(ret);
	finish_generated_function();
}

void Pa15Lowerer::index_function_scope_variables(){
		function_scope_variables_.assign(model_.scopes_.size(),
			std::vector<BindingId>());
		std::vector<bool> collected_function_scope(model_.scopes_.size(), false);
		for (std::size_t i = 0; i < model_.function_facts_.size(); ++i)
		{
			const FunctionFact& fact = model_.function_facts_[i];
			if (!fact.owner.valid() || fact.owner.value >= model_.scopes_.size() ||
				(model_.scopes_[fact.owner.value].kind != ScopeKind::Namespace &&
					model_.scopes_[fact.owner.value].kind != ScopeKind::Class))
				continue;
			if (!fact.function_scope.valid())
				continue;
			if (fact.function_scope.value >= model_.scopes_.size() ||
				model_.scopes_[fact.function_scope.value].kind != ScopeKind::Function ||
				model_.scopes_[fact.function_scope.value].parent != fact.owner)
				throw std::runtime_error("PA15 function scope is missing");
			collected_function_scope[fact.function_scope.value] = true;
		}





		std::vector<std::size_t> owner(model_.scopes_.size(),
			InvalidIdentityValue);
		for (std::size_t scope = 0; scope < model_.scopes_.size(); ++scope)
		{
			const Scope& current = model_.scopes_[scope];
			if (collected_function_scope[scope])
				owner[scope] = scope;
			else if (current.parent.valid())
			{
				if (current.parent.value >= scope)
					throw std::runtime_error("PA15 scope parent order is invalid");
				owner[scope] = owner[current.parent.value];
			}
			if (owner[scope] == InvalidIdentityValue) continue;
			for (std::size_t i = 0; i < current.bindings.size(); ++i)
			{
				const BindingId id = current.bindings[i];
				if (model_.binding(id).kind == BindingKind::Variable)
					function_scope_variables_[owner[scope]].push_back(id);
			}
		}
	}

void Pa15Lowerer::collect_functions(){
		index_function_scope_variables();
		constructor_nothrow_states_.assign(model_.function_facts_.size(),
			ConstructorRuntimeCacheState::Unseen);
		constructor_nothrow_results_.assign(model_.function_facts_.size(), 0);
		constructor_nothrow_invalid_.assign(model_.function_facts_.size(), 0);
		semantic_nothrow_states_.assign(model_.semantic_facts_.size(),
			ConstructorRuntimeCacheState::Unseen);
		semantic_nothrow_results_.assign(model_.semantic_facts_.size(), 0);
		semantic_nothrow_invalid_.assign(model_.semantic_facts_.size(), 0);
		std::vector<unsigned char> demanded_member_functions(model_.function_facts_.size(), 0);
		std::vector<unsigned char> demanded_namespace_functions(model_.function_facts_.size(), 0);
		demanded_member_declarations_.assign(model_.bindings_.size(), 0);
		demanded_member_declaration_types_.assign(model_.bindings_.size(), TypeId());
		collect_demanded_functions(&demanded_member_functions, &demanded_namespace_functions,
			&demanded_member_declarations_, &demanded_member_declaration_types_);
		for (std::size_t i = 0; i < model_.function_facts_.size(); ++i)
		{
			const FunctionFact& fact = model_.function_facts_[i];
			if (!fact.binding.valid() || fact.binding.value >= model_.bindings_.size())
				throw std::runtime_error("PA15 function binding is missing");
			const Binding& fact_binding = model_.binding(fact.binding);
			const FunctionFact* canonical_function =
				model_.function_fact_for_binding(fact.binding);
			if (fact_binding.kind != BindingKind::Function ||
				model_.type_kind(fact_binding.type) != TypeKind::Function ||
				!function_abi_supported(fact.binding, canonical_function,
					fact_binding.type))
				throw std::runtime_error(
					"PA15 unsupported class-value function ABI");
			const BindingSidecar* sidecar = model_.binding_sidecar(fact.binding);
			if (sidecar != NULL && sidecar->hidden_friend && !fact_binding.internal_linkage &&
				fact.owner.valid() && fact.owner.value < model_.scopes_.size() &&
				model_.scopes_[fact.owner.value].kind == ScopeKind::Namespace &&
				demanded_namespace_functions[i] == 0)
				continue;
			if (fact.owner.valid() && fact.owner.value < model_.scopes_.size() &&
				model_.scopes_[fact.owner.value].kind == ScopeKind::Class &&
				demanded_member_functions[i] == 0)
				continue;
			if (!fact.owner.valid() || fact.owner.value >= model_.scopes_.size())
				throw std::runtime_error("PA15 function owner is missing");
			if (model_.scopes_[fact.owner.value].kind != ScopeKind::Namespace &&
				model_.scopes_[fact.owner.value].kind != ScopeKind::Class)
				continue;
			if (!fact.function_scope.valid())
				continue;
			if (fact.function_scope.value >= model_.scopes_.size() ||
				model_.scopes_[fact.function_scope.value].kind != ScopeKind::Function ||
				model_.scopes_[fact.function_scope.value].parent != fact.owner)
				throw std::runtime_error("PA15 function scope is invalid");
			const Binding& binding = model_.binding(fact.binding);
			if (binding.kind != BindingKind::Function ||
				model_.type_kind(binding.type) != TypeKind::Function)
				throw std::runtime_error("PA15 invalid procedural function fact");
			slot_by_binding_.clear();

			const std::vector<std::string> components = function_components(fact);
			std::string internal_name;
			for (std::size_t component = 0; component < components.size(); ++component)
			{
				if (component != 0) internal_name += "__";
				internal_name += components[component];
			}
			const SpellingId name_id = symbol_spelling(internal_name);
			Function function;
			function.symbol_id = SymbolId(next_symbol_++);
			function.name_id = name_id;
			function.return_type = function_result_low_type(
				model_.types_[binding.type.value].result);
			const bool is_constructor = fact.is_constructor || (sidecar != NULL &&
				sidecar->constructor_record.valid());
			const bool is_destructor = fact.is_destructor || (sidecar != NULL &&
				sidecar->destructor_record.valid());
			const bool is_special_member = is_constructor || is_destructor;
			if (sidecar != NULL && sidecar->nonthrowing)
				function.boundary.unwind = lowir_model::CUM_NO;
			if (is_constructor && fact.synthetic)
			{
				if (constructor_is_nothrow(FunctionFactId(i)))
					function.boundary.unwind = lowir_model::CUM_NO;
			}
			const bool strong_out_of_class_special = is_special_member &&
				fact.out_of_class_definition && sidecar != NULL &&
				!sidecar->inline_member;
			const bool trivial_lifecycle = is_constructor && fact.synthetic &&
				constructor_function_is_noop(FunctionFactId(i), false);
			function.metadata.binding = binding.internal_linkage ?
				lowir_model::SBM_INTERNAL : strong_out_of_class_special ?
				lowir_model::SBM_STRONG : is_special_member ?
				lowir_model::SBM_WEAK : lowir_model::SBM_STRONG;
			if (trivial_lifecycle)
				function.metadata.object_trivial_lifecycle = true;
			if (binding.language_linkage == LanguageLinkage::C)
				function.metadata.linkage = lowir_model::LLM_C;
			const bool is_main = components.size() == 1 && components.front() == "main";
			if (is_main)
			{
				function.metadata.role = lowir_model::SR_ENTRY;
				function.metadata.keep_internal_alias = true;
			}
			else
				function.metadata.object_symbol_id = intern_spelling(abi_symbol(fact,
					fact.constructor_base_entry ?
					abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_BASE :
					fact.destructor_base_entry ?
					abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_BASE :
					abi_mangle::ABI_SPECIAL_TERMINAL_NONE));

			const std::size_t function_index = program_.functions.size();
			program_.functions.push_back(function);
			function_plans_.push_back(FunctionPlan(i, function_index));
			function_symbols_[fact.binding.value] = function.symbol_id;
			function_name_ids_[fact.binding.value] = name_id;
			symbol_name_ids_[function.symbol_id.index] = name_id;
			// When PA12 publishes a demanded, corresponding base ABI entry for an
			// internal special member, that entry owns the C2/D2 object spelling.
			// Prove that ownership through typed identities and the demand vector;
			// a stale or incomplete relation must not suppress the complete-entry
			// alias.  Keep the existing external ABI alias behavior.
			const BindingId* mapped_base_entry = is_constructor ?
				model_.constructor_base_entry_bindings_.find(fact.binding) :
				is_destructor ?
				model_.destructor_base_entry_bindings_.find(fact.binding) : NULL;
			bool internal_special_member_pair = false;
			if (binding.internal_linkage && mapped_base_entry != NULL &&
				mapped_base_entry->valid() &&
				mapped_base_entry->value < model_.bindings_.size() &&
				mapped_base_entry->value < model_.binding_owners_.size() &&
				model_.binding_owners_[mapped_base_entry->value] == fact.owner)
			{
				const Binding& base_entry_binding =
					model_.binding(*mapped_base_entry);
				const FunctionFactId* base_entry_id =
					model_.function_binding_fact_index_.find(*mapped_base_entry);
				if (base_entry_binding.kind == BindingKind::Function &&
					base_entry_binding.type.valid() &&
					base_entry_binding.type.value < model_.types_.size() &&
					model_.type_kind(base_entry_binding.type) == TypeKind::Function &&
					base_entry_id != NULL && base_entry_id->valid() &&
					base_entry_id->value < model_.function_facts_.size() &&
					base_entry_id->value < demanded_member_functions.size())
				{
					const FunctionFact& base_entry =
						model_.function_facts_[base_entry_id->value];
					const bool matching_base_entry = is_constructor ?
						base_entry.is_constructor && base_entry.constructor_base_entry &&
						base_entry.constructor_entry_source == fact.binding &&
						base_entry.constructor_record == fact.constructor_record :
						base_entry.is_destructor && base_entry.destructor_base_entry &&
						base_entry.destructor_entry_source == fact.binding &&
						base_entry.destructor_record == fact.destructor_record;
					internal_special_member_pair =
						base_entry.binding == *mapped_base_entry &&
						base_entry.owner == fact.owner && matching_base_entry &&
						demanded_member_functions[base_entry_id->value] != 0;
				}
			}
			if (is_special_member && !fact.constructor_base_entry &&
				!fact.destructor_base_entry &&
				!internal_special_member_pair)
			{
				lowir_model::ObjectAlias alias;
				alias.object_name_id = intern_spelling(abi_symbol(fact,
					is_constructor ?
					abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_BASE :
					abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_BASE));
				alias.target_name_id = name_id;
				alias.target_id = function.symbol_id;
				program_.object_aliases.push_back(alias);
			}
			Function& stored = program_.functions.back();
			used_slot_names_.clear();
			slot_collision_counters_.clear();
			stored.slot_begin = lowir_model::SlotId(next_slot_);

			const Scope& function_scope = model_.scopes_[fact.function_scope.value];
			for (std::size_t parameter = 0; parameter < function_scope.bindings.size(); ++parameter)
			{
				const BindingId id = function_scope.bindings[parameter];
				const Binding& parameter_binding = model_.binding(id);
				if (parameter_binding.kind != BindingKind::Parameter) continue;
			const SpellingId parameter_name = parameter_value_name(
				parameter_binding.name, parameter);
				const LowType parameter_type = low_type(parameter_binding.type);
				lowir_model::Parameter parameter_record;
				parameter_record.name_id = intern_spelling(
					parameter_name.valid() ? spelling(parameter_name) : "%__pa15_param");
				parameter_record.type = parameter_type;
				if (model_.type_kind(model_.strip_cv_type(parameter_binding.type)) ==
					TypeKind::LvalueReference ||
					model_.type_kind(model_.strip_cv_type(parameter_binding.type)) ==
					TypeKind::RvalueReference)
					parameter_record.metadata.passing = lowir_model::PPM_REFERENCE;
				stored.params.push_back(parameter_record);
				add_slot(stored, id, parameter_binding.type,
					slot_name(parameter_binding.name, parameter, false),
					parameter_type);
			}
			std::map<std::size_t, std::size_t> active_names;
			for (std::size_t parameter = 0; parameter < function_scope.bindings.size(); ++parameter)
			{
				const Binding& parameter_binding = model_.binding(
					function_scope.bindings[parameter]);
					if (parameter_binding.kind == BindingKind::Parameter &&
						parameter_binding.name.valid())
						++active_names[parameter_binding.name.value];
				}
				collect_local_slots(stored, fact.function_scope, &active_names);
				const std::vector<BindingId>& owned_variables =
				function_scope_variables_[fact.function_scope.value];
			for (std::size_t i = 0; i < owned_variables.size(); ++i)
			{
				const BindingId id = owned_variables[i];
				const Binding& value = model_.binding(id);
				if (slot_by_binding_.find(id.value) != slot_by_binding_.end()) continue;
				add_slot(stored, id, value.type, slot_name(value.name,
					id.value, false), low_type(value.type));
			}
			stored.slot_count = next_slot_ - stored.slot_begin.index;
			stored.value_begin = ValueId();
			stored.value_count = 0;
			function_plans_.back().slot_bindings = slot_by_binding_;
		}
	}

SpellingId Pa15Lowerer::parameter_value_name(NameId name, std::size_t ordinal){
		if (name.valid()) return intern_spelling("%" + model_.name_text(name));
		std::ostringstream generated;
		generated << "%__param" << ordinal;
		return intern_spelling(generated.str());
	}

SpellingId Pa15Lowerer::slot_name(NameId name, std::size_t ordinal, bool shadowed){
		std::string result;
		if (name.valid()) result = "$" + model_.name_text(name);
		else
		{
			std::ostringstream generated;
			generated << "$__param" << ordinal;
			result = generated.str();
		}
		if (used_slot_names_.find(result) != used_slot_names_.end())
		{
			const std::string base = shadowed ? result + "__shadow" : result;
			std::size_t& suffix = slot_collision_counters_[base];
			if (suffix == 0) suffix = 2;
			std::ostringstream generated;
			if (shadowed) generated << base << suffix++;
			else generated << base << "__" << suffix++;
			while (used_slot_names_.find(generated.str()) != used_slot_names_.end())
			{
				generated.str(std::string());
				generated.clear();
				if (shadowed) generated << base << suffix++;
				else generated << base << "__" << suffix++;
			}
			result = generated.str();
		}
		used_slot_names_.insert(result);
		return intern_spelling(result);
	}

void Pa15Lowerer::collect_local_slots(Function& function, ScopeId scope,
		std::map<std::size_t, std::size_t>* active_names){
		if (!scope.valid()) return;
		const Scope& current = model_.scopes_[scope.value];
		std::vector<std::size_t> added_names;
		for (std::size_t i = 0; i < current.bindings.size(); ++i)
		{
			const BindingId id = current.bindings[i];
			const Binding& binding = model_.binding(id);
			if (binding.kind != BindingKind::Variable) continue;
			const LowType type = low_type(binding.type);
			add_slot(function, id, binding.type,
				slot_name(binding.name, id.value, binding.name.valid() &&
					active_names->find(binding.name.value) != active_names->end()), type);
			if (binding.name.valid())
			{
				++(*active_names)[binding.name.value];
				added_names.push_back(binding.name.value);
			}
		}
		for (std::size_t i = 0; i < current.children.size(); ++i)
			if (model_.scopes_[current.children[i].value].kind == ScopeKind::Block)
				collect_local_slots(function, current.children[i], active_names);
		for (std::size_t i = 0; i < added_names.size(); ++i)
		{
			std::map<std::size_t, std::size_t>::iterator found =
				active_names->find(added_names[i]);
			if (found != active_names->end())
			{
				if (--found->second == 0) active_names->erase(found);
			}
		}
	}

void Pa15Lowerer::add_slot(Function& function, BindingId binding,
	TypeId semantic_type, SpellingId name_id, const LowType& type)
{
		(void)semantic_type;
		if (slot_by_binding_.find(binding.value) != slot_by_binding_.end()) return;
		Function::Slot slot;
		slot.slot_id = lowir_model::SlotId(next_slot_++);
		slot.name_id = name_id;
		slot.type = type;
		function.slots.push_back(slot);
		if (slot_spellings_.size() <= slot.slot_id.index)
			slot_spellings_.resize(slot.slot_id.index + 1);
		slot_spellings_[slot.slot_id.index] = slot.name_id;
		if (slot_source_begins_.size() <= slot.slot_id.index)
			slot_source_begins_.resize(slot.slot_id.index + 1,
				InvalidIdentityValue);
		const std::map<std::size_t, const DeclarationFact*>::const_iterator
			declaration = declaration_by_binding_.find(binding.value);
		if (declaration != declaration_by_binding_.end() &&
			declaration->second != NULL && declaration->second->node != NULL)
			slot_source_begins_[slot.slot_id.index] =
				declaration->second->node->source_begin;
		slot_by_binding_[binding.value] = slot.slot_id;
}

ValueId Pa15Lowerer::allocate_value(){
		const ValueId id(next_value_++);
		lowir_model::ValueRecord record;
		record.id = id;
		program_.values.push_back(record);
		return id;
	}

void Pa15Lowerer::clear_value_records(){
		for (std::size_t i = 0; i < program_.values.size(); ++i)
		{
			program_.values[i].id = ValueId(i);
			program_.values[i].parameter = 0;
			program_.values[i].instruction = 0;
			program_.values[i].owner_function_id = SymbolId();
			program_.values[i].producer = lowir_model::ValueRecord::VALUE_UNDEFINED;
		}
	}

void Pa15Lowerer::claim_value(ValueId id, SymbolId owner,
		const lowir_model::Parameter* parameter,
		const lowir_model::Instruction* instruction,
		lowir_model::ValueRecord::ProducerKind producer){
		if (!id.valid() || id.index >= program_.values.size())
			throw std::runtime_error("PA15 value producer is out of range");
		lowir_model::ValueRecord& record = program_.values[id.index];
		if (record.producer != lowir_model::ValueRecord::VALUE_UNDEFINED)
			throw std::runtime_error("PA15 value has multiple producers");
		record.id = id;
		record.owner_function_id = owner;
		record.parameter = parameter;
		record.instruction = instruction;
		record.producer = producer;
	}

void Pa15Lowerer::finalize_value_records(){
		for (std::size_t i = 0; i < program_.values.size(); ++i)
		{
			program_.values[i].id = ValueId(i);
			program_.values[i].parameter = 0;
			program_.values[i].instruction = 0;
			program_.values[i].owner_function_id = SymbolId();
			program_.values[i].producer = lowir_model::ValueRecord::VALUE_UNDEFINED;
		}
		for (std::size_t i = 0; i < program_.functions.size(); ++i)
		{
			const Function& function = program_.functions[i];
			for (std::size_t j = 0; j < function.params.size(); ++j)
				claim_value(function.params[j].value_id, function.symbol_id,
					&function.params[j], 0,
					lowir_model::ValueRecord::VALUE_PARAMETER);
			for (std::size_t j = 0; j < function.blocks.size(); ++j)
				for (std::size_t k = 0; k < function.blocks[j].instructions.size(); ++k)
				{
					const Instruction& instruction = function.blocks[j].instructions[k];
					if (instruction.dest_id.valid())
						claim_value(instruction.dest_id, function.symbol_id, 0,
							&instruction,
							lowir_model::ValueRecord::VALUE_INSTRUCTION);
				}
		}
		for (std::size_t i = 0; i < program_.values.size(); ++i)
			if (program_.values[i].producer == lowir_model::ValueRecord::VALUE_UNDEFINED)
				throw std::runtime_error("PA15 value has no producer");
		for (std::size_t i = 0; i < program_.functions.size(); ++i)
		{
			const Function& function = program_.functions[i];
			if (!function.value_begin.valid())
				throw std::runtime_error("PA15 function has no value range");
			if (function.value_begin.index + function.value_count >
				program_.values.size())
				throw std::runtime_error("PA15 function value range is out of bounds");
			for (std::size_t j = 0; j < function.value_count; ++j)
				if (program_.values[function.value_begin.index + j].owner_function_id !=
					function.symbol_id)
					throw std::runtime_error("PA15 function value range crosses ownership");
		}
	}

Function& Pa15Lowerer::function(){
		return program_.functions[current_function_];
	}

const Function& Pa15Lowerer::function() const{
		return program_.functions[current_function_];
	}

Block& Pa15Lowerer::block(){
		return function().blocks[current_block_];
	}

bool Pa15Lowerer::terminated(const Block& current) const{
		if (current.instructions.empty()) return false;
		switch (current.instructions.back().kind)
		{
		case Instruction::IK_JUMP:
		case Instruction::IK_BRANCH:
		case Instruction::IK_SWITCH:
		case Instruction::IK_RETURN:
		case Instruction::IK_THROW:
		case Instruction::IK_RESUME:
			return true;
		default: return false;
		}
	}

bool Pa15Lowerer::terminated(BlockId id) const{
		return terminated(function().blocks[block_index(id)]);
	}

void Pa15Lowerer::reorder_condition_blocks(std::size_t begin, std::size_t destination_count,
		BlockId saved_current){
		Function& target = function();
		if (begin > target.blocks.size() ||
			destination_count > target.blocks.size() - begin)
			throw std::runtime_error("PA15 invalid condition block range");
		std::vector<Block> suffix(target.blocks.begin() + begin,
			target.blocks.end());
		std::vector<Block> reordered;



		for (std::size_t i = suffix.size(); i > destination_count; --i)
			reordered.push_back(suffix[i - 1]);
		for (std::size_t i = 0; i < destination_count; ++i)
			reordered.push_back(suffix[i]);
		target.blocks.erase(target.blocks.begin() + begin, target.blocks.end());
		target.blocks.insert(target.blocks.end(), reordered.begin(), reordered.end());
		rebuild_block_indexes();
		set_current(saved_current);
	}

SpellingId Pa15Lowerer::temporary_name(){
		std::ostringstream name;
		std::string candidate;
		do
		{
			name.str(std::string());
			name.clear();
			name << "%t" << ++temp_ordinal_;
			candidate = name.str();
		} while (used_value_names_.find(candidate) != used_value_names_.end());
		used_value_names_.insert(candidate);
		return intern_spelling(candidate);
	}

std::size_t Pa15Lowerer::new_block(const std::string& base){
		Block created;
		created.block_id = BlockId(next_block_++);
		std::ostringstream label;
		if (base == "entry") label << "^entry";
		else label << "^" << base << "_" << ++block_ordinal_;
		created.label_id = intern_spelling(label.str());
		function().blocks.push_back(created);
		const std::size_t index = function().blocks.size() - 1;
		block_indexes_[created.block_id.index] = index;
		if (created.block_id.index < reachability_base_)
			throw std::runtime_error("PA15 block identity precedes reachability base");
		reachable_blocks_.push_back(0);
		return index;
	}

void Pa15Lowerer::rebuild_block_indexes(){
		block_indexes_.clear();
		for (std::size_t i = 0; i < function().blocks.size(); ++i)
			block_indexes_[function().blocks[i].block_id.index] = i;
	}

std::size_t Pa15Lowerer::reachability_index(BlockId id) const{
		if (!id.valid() || id.index < reachability_base_ ||
			id.index - reachability_base_ >= reachable_blocks_.size())
			throw std::runtime_error("PA15 reachability identity is not owned");
		return id.index - reachability_base_;
	}

bool Pa15Lowerer::is_reachable(BlockId id) const{
		return reachable_blocks_[reachability_index(id)] != 0;
	}

BlockId Pa15Lowerer::edge_target(const Operand& operand) const{
		if (operand.kind != Operand::OP_LABEL || !operand.block_id.valid())
			throw std::runtime_error("PA15 terminator edge has no block target");
		return operand.block_id;
	}

void Pa15Lowerer::enqueue_reachable(BlockId id){
		const std::size_t index = reachability_index(id);
		if (reachable_blocks_[index] == 0)
		{
			reachable_blocks_[index] = 1;
			reachability_work_.push_back(id);
		}
	}

void Pa15Lowerer::propagate_existing_terminator_edges(BlockId source){
		const Block& current = function().blocks[block_index(source)];
		if (current.instructions.empty()) return;
		const Instruction& terminator = current.instructions.back();
		switch (terminator.kind)
		{
		case Instruction::IK_JUMP:
			enqueue_reachable(edge_target(terminator.first));
			break;
		case Instruction::IK_BRANCH:
			enqueue_reachable(edge_target(terminator.second));
			enqueue_reachable(edge_target(terminator.third));
			break;
		case Instruction::IK_SWITCH:
			enqueue_reachable(edge_target(terminator.second));
			if (terminator.args.size() % 2 != 0)
				throw std::runtime_error("PA15 switch terminator has odd arm data");
			for (std::size_t i = 1; i < terminator.args.size(); i += 2)
				enqueue_reachable(edge_target(terminator.args[i]));
			break;
		default:
			break;
		}
	}

void Pa15Lowerer::mark_reachable(BlockId start){
		if (is_reachable(start)) return;
		reachability_work_.clear();
		enqueue_reachable(start);
		while (!reachability_work_.empty())
		{
			const BlockId source = reachability_work_.back();
			reachability_work_.pop_back();
			propagate_existing_terminator_edges(source);
		}
	}

void Pa15Lowerer::propagate_edge(BlockId source, BlockId target){
		if (is_reachable(source)) mark_reachable(target);
	}

SemanticFactId Pa15Lowerer::enclosing_switch_fact(SemanticFactId id) const{
	if (!id.valid() || id.value >= fact_switch_ancestors_.size() ||
		id.value >= fact_index_generations_.size() ||
		fact_index_generations_[id.value] != label_generation_)
		return SemanticFactId();
	return fact_switch_ancestors_[id.value];
}

BlockId Pa15Lowerer::block_id(std::size_t index) const{
		if (index >= function().blocks.size())
			throw std::runtime_error("PA15 block index is out of range");
		return function().blocks[index].block_id;
	}

std::size_t Pa15Lowerer::block_index(BlockId id) const{
		const std::map<std::size_t, std::size_t>::const_iterator found =
			block_indexes_.find(id.index);
		if (!id.valid() || found == block_indexes_.end())
			throw std::runtime_error("PA15 block identity is not owned by function");
		return found->second;
	}

void Pa15Lowerer::set_current(BlockId id){
		if (!id.valid())
		{
			current_block_ = InvalidIdentityValue;
			return;
		}
		current_block_ = block_index(id);
		if (ordered_block_ids_.insert(id.index).second)
			block_order_.push_back(id);
	}

BlockId Pa15Lowerer::current_block_id() const{
		return current_block_ == InvalidIdentityValue ? BlockId() :
			block_id(current_block_);
	}

void Pa15Lowerer::reorder_function_blocks(){
		Function& target = function();
		const BlockId saved_current = current_block_id();
		std::set<std::size_t> allocated_block_ids;
		for (std::size_t i = 0; i < target.blocks.size(); ++i)
			if (!target.blocks[i].block_id.valid() ||
				!allocated_block_ids.insert(target.blocks[i].block_id.index).second)
				throw std::runtime_error("PA15 duplicate allocated block identity");
		if (ordered_block_ids_.size() != block_order_.size())
			throw std::runtime_error("PA15 block order has duplicate identity");
		std::set<std::size_t> ordered;
		for (std::size_t i = 0; i < block_order_.size(); ++i)
		{
			if (allocated_block_ids.find(block_order_[i].index) ==
				allocated_block_ids.end() ||
				!ordered.insert(block_order_[i].index).second)
				throw std::runtime_error("PA15 block order identity is invalid");
		}



		for (std::size_t i = 0; i < target.blocks.size(); ++i)
		{
			const std::size_t id = target.blocks[i].block_id.index;
			if (ordered.insert(id).second)
			{
				block_order_.push_back(target.blocks[i].block_id);
				ordered_block_ids_.insert(id);
			}
		}
		if (ordered.size() != allocated_block_ids.size())
			throw std::runtime_error("PA15 allocated block is missing from order");
		std::vector<Block> reordered;
		for (std::size_t i = 0; i < block_order_.size(); ++i)
		{
			const std::size_t index = block_index(block_order_[i]);
			reordered.push_back(target.blocks[index]);
		}
		target.blocks.swap(reordered);
		rebuild_block_indexes();
		set_current(saved_current);
	}

Operand Pa15Lowerer::temporary_operand(ValueId id, SpellingId name) const{
		Operand operand;
		operand.kind = Operand::OP_TEMP;
		operand.value_id = id;
		operand.presentation_id = name;
		return operand;
	}

Operand Pa15Lowerer::slot_operand(lowir_model::SlotId id) const{
		Operand operand;
		operand.kind = Operand::OP_SLOT;
		operand.slot_id = id;
		if (!id.valid() || id.index >= slot_spellings_.size() ||
			!slot_spellings_[id.index].valid())
			throw std::runtime_error("PA15 slot identity has no presentation record");
		operand.presentation_id = slot_spellings_[id.index];
		return operand;
	}

Operand Pa15Lowerer::global_operand(SymbolId id, SpellingId name) const{
		Operand operand;
		operand.kind = Operand::OP_GLOBAL;
		operand.symbol_id = id;
		operand.presentation_id = name;
		return operand;
	}

Operand Pa15Lowerer::block_operand(std::size_t index) const{
		return block_operand(block_id(index));
	}

Operand Pa15Lowerer::block_operand(BlockId id) const{
		Operand operand;
		operand.kind = Operand::OP_LABEL;
		operand.block_id = id;
		operand.presentation_id = function().blocks[block_index(id)].label_id;
		return operand;
	}

Operand Pa15Lowerer::integer_operand(long long value, const LowType& type) const{
		Operand operand;
		operand.kind = Operand::OP_INTEGER;
		operand.int_value = value;
		operand.literal_type = type;
		return operand;
	}

Operand Pa15Lowerer::floating_operand(long double value, const LowType& type) const{
		Operand operand;
		operand.kind = Operand::OP_FLOAT;
		operand.float_value = value;
		operand.literal_type = type;
		return operand;
	}

ValueId Pa15Lowerer::destination(const LowType& type, Instruction* instruction){
		const ValueId id = allocate_value();
		instruction->dest_id = id;
		instruction->destination_name_id = temporary_name();
		instruction->type = type;
		instruction->result_type = type;
		return id;
	}

ValueId Pa15Lowerer::emit_load(const LoweredValue& storage, const LowType& type){
	Instruction instruction;
	instruction.kind = Instruction::IK_LOAD;
	instruction.first = storage.value;
	const ValueId id = destination(type, &instruction);
	block().instructions.push_back(instruction);
	return id;
}

LoweredValue Pa15Lowerer::emit_index(const LoweredValue& base, const LoweredValue& offset,
		const LowType& element, lowir_model::IndexProjectionKind projection){
		if (!base.type.is_pointer() && !base.type.is_object())
			throw std::runtime_error("PA15 index base is not addressable");
		Instruction instruction;
		instruction.kind = Instruction::IK_INDEX;
		instruction.type = element;
		instruction.first = base.value;
		instruction.second = offset.value;
		instruction.index_projection = projection;
		LowType pointer;
		pointer.kind = LowType::TYPE_POINTER;
		const ValueId value = destination(pointer, &instruction);
		instruction.type = element;
		block().instructions.push_back(instruction);
		return LoweredValue(temporary_operand(value, instruction.destination_name_id),
			pointer, false);
	}

LoweredValue Pa15Lowerer::emit_decay(const LoweredValue& address){
		if (!address.type.is_pointer())
			throw std::runtime_error("PA15 decay operand is not a pointer");
		Instruction instruction;
		instruction.kind = Instruction::IK_UNARY;
		instruction.type = address.type;
		instruction.first = address.value;
		instruction.unary_operator = lowir_model::UOP_DECAY;
		const ValueId value = destination(address.type, &instruction);
		block().instructions.push_back(instruction);
		return LoweredValue(temporary_operand(value, instruction.destination_name_id),
			address.type, false);
	}

LoweredValue Pa15Lowerer::storage_for(BindingId binding) const{
		std::map<std::size_t, lowir_model::SlotId>::const_iterator found =
			slot_by_binding_.find(binding.value);
		if (found != slot_by_binding_.end())
		{
			const LowType type = low_type(model_.binding(binding).type);
			return LoweredValue(slot_operand(found->second), type, true);
		}
		std::map<std::size_t, SymbolId>::const_iterator global =
			global_symbols_.find(binding.value);
		if (global != global_symbols_.end())
		{
			const LowType type = low_type(model_.binding(binding).type);
			return LoweredValue(global_operand(global->second,
				global_name_ids_.find(binding.value)->second), type, true);
		}
		throw std::runtime_error("PA15 binding has no storage");
	}
bool Pa15Lowerer::automatic_local_declaration(BindingId binding) const{
		const std::map<std::size_t, const DeclarationFact*>::const_iterator found =
			declaration_by_binding_.find(binding.value);
		return binding.valid() && found != declaration_by_binding_.end() &&
			found->second != NULL && found->second->automatic_storage &&
			found->second->scope.valid() &&
			found->second->scope.value < model_.scopes_.size() &&
			model_.scopes_[found->second->scope.value].kind == ScopeKind::Block;
}

std::vector<SemanticFactId> Pa15Lowerer::children(SemanticFactId id) const{
		if (!id.valid() || id.value >= model_.semantic_facts_.size())
			throw std::runtime_error("PA15 invalid semantic fact identity");
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		std::vector<SemanticFactId> result;
		if (fact.child_count != 0 &&
			(fact.child_begin == InvalidIdentityValue ||
				fact.child_begin > model_.semantic_children_.size() ||
				fact.child_count > model_.semantic_children_.size() -
					fact.child_begin))
			throw std::runtime_error("PA15 invalid semantic child range");
		if (fact.child_count == 0 && fact.child_begin != InvalidIdentityValue &&
			fact.child_begin > model_.semantic_children_.size())
			throw std::runtime_error("PA15 invalid semantic child range");
		for (std::size_t i = 0; i < fact.child_count; ++i)
		{
			const SemanticFactId child = model_.semantic_children_[fact.child_begin + i];
			if (!child.valid() || child.value >= model_.semantic_facts_.size())
				throw std::runtime_error("PA15 invalid semantic child identity");
			result.push_back(child);
		}
		return result;
}

bool Pa15Lowerer::validate_typed_base_path(TypeId actual, TypeId required,
	ScopeId target, std::size_t begin, std::size_t count) const
{
	actual = model_.strip_cv_type(model_.expression_object_type(actual));
	required = model_.strip_cv_type(model_.expression_object_type(required));
	if (!actual.valid() || !required.valid() || !target.valid() ||
		model_.type_kind(actual) != TypeKind::Named ||
		model_.type_kind(required) != TypeKind::Named ||
		model_.class_scope_for_type(required) != target)
		return false;
	const NamedRecordId actual_record = model_.named_record_for_type(actual);
	const NamedRecordId required_record = model_.named_record_for_type(required);
	if (!actual_record.valid() || !required_record.valid() ||
		actual_record.value >= model_.named_.size() ||
		required_record.value >= model_.named_.size() ||
		actual_record.value >= model_.record_layouts_.size())
		return false;
	if (count == 0)
	{
		if (begin != InvalidIdentityValue)
			return false;
		return actual_record == required_record;
	}
	if (begin == InvalidIdentityValue || begin > model_.semantic_base_paths_.size() ||
		count > model_.semantic_base_paths_.size() - begin)
		return false;
	NamedRecordId current_record = actual_record;
	if (current_record == required_record)
		return false;
	for (std::size_t i = 0; i < count; ++i)
	{
		if (!current_record.valid() || current_record.value >= model_.named_.size() ||
			current_record.value >= model_.record_layouts_.size())
			return false;
		const NamedRecord& current = model_.named_[current_record.value];
		const NamedRecordId base_record = model_.semantic_base_paths_[begin + i];
		if (current.kind != NamedKind::Class || !current.has_base ||
			current.direct_base_virtual || current.direct_base != base_record ||
			!base_record.valid() || base_record.value >= model_.named_.size())
			return false;
		const RecordLayout& layout = model_.record_layout(current_record);
		if (layout.state != RecordLayoutState::Complete ||
			!layout.has_direct_base || layout.direct_base.record != base_record ||
			layout.direct_base.offset != 0)
			return false;
		current_record = base_record;
	}
	return current_record == required_record;
}

LowType Pa15Lowerer::lvalue_type(SemanticFactId id) const{
	const SemanticFact& fact = model_.semantic_facts_[id.value];
	return low_reference_value_type(fact.type);
}

bool Pa15Lowerer::reference_binding(BindingId binding) const{
		if (!binding.valid()) return false;
		const TypeKind kind = model_.type_kind(
			model_.strip_cv_type(model_.binding(binding).type));
		return kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference;
	}

LoweredValue Pa15Lowerer::generated_slot(const LowType& type,
	const std::string& prefix, const PA10AstNode* source)
{
	std::ostringstream name;
	SpellingId name_id;
	do
	{
		name.str(std::string());
		name.clear();
		name << "$" << prefix << "__" << ++generated_slot_ordinal_;
		name_id = intern_spelling(name.str());
	} while (used_slot_names_.find(name.str()) != used_slot_names_.end());
	used_slot_names_.insert(name.str());
	Function::Slot slot;
	slot.slot_id = lowir_model::SlotId(next_slot_++);
	slot.name_id = name_id;
	slot.type = type;
	if (slot_spellings_.size() <= slot.slot_id.index)
		slot_spellings_.resize(slot.slot_id.index + 1);
	slot_spellings_[slot.slot_id.index] = name_id;
	if (slot_source_begins_.size() <= slot.slot_id.index)
		slot_source_begins_.resize(slot.slot_id.index + 1,
			InvalidIdentityValue);
	slot_source_begins_[slot.slot_id.index] =
		source != NULL && source->source_begin != 0 ?
		source->source_begin : InvalidIdentityValue;
	function().slots.push_back(slot);
	if (source != NULL && source->source_begin != 0)
	{
		std::size_t insertion = function().slots.size() - 1;
		for (std::size_t i = 0; i + 1 < function().slots.size(); ++i)
		{
			const Function::Slot& existing = function().slots[i];
			const std::size_t existing_source =
				existing.slot_id.index < slot_source_begins_.size() ?
				slot_source_begins_[existing.slot_id.index] :
				InvalidIdentityValue;
			if (existing_source != InvalidIdentityValue &&
				existing_source > source->source_begin)
			{
				insertion = i;
				break;
			}
		}
		if (insertion != function().slots.size() - 1)
		{
			const Function::Slot generated = function().slots.back();
			function().slots.erase(function().slots.end() - 1);
			function().slots.insert(function().slots.begin() + insertion, generated);
		}
	}
	return LoweredValue(slot_operand(slot.slot_id), type, true);
}

LoweredValue Pa15Lowerer::lower_lvalue(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::IdExpression ||
			fact.kind == SemanticFactKind::Variable)
		{
			const LoweredValue storage = storage_for(fact.binding);
			if (model_.bit_field_fact(fact.binding) != NULL)
				return mark_bit_field_address(address_of_storage(storage),
					fact.binding);
			if (!reference_binding(fact.binding)) return storage;
			if (model_.callable_function_type(model_.binding(fact.binding).type).valid())
			{
				const ValueId value = emit_load(storage, storage.type);
				const Instruction& emitted = block().instructions.back();
				return LoweredValue(temporary_operand(value,
					emitted.destination_name_id), storage.type, false,
					storage.type);
			}
			const LowType object = lvalue_type(id);
			const ValueId value = emit_load(storage, storage.type);
			const Instruction& emitted = block().instructions.back();
			return LoweredValue(temporary_operand(value, emitted.destination_name_id),
				object, true, storage.type);
		}
		if (fact.kind == SemanticFactKind::MemberExpression &&
			model_.bit_field_fact(fact.selected_binding) != NULL)
			return lower_member_address(id);
		if (fact.kind == SemanticFactKind::MemberExpression &&
			reference_binding(fact.selected_binding))
		{
			const LoweredValue address = lower_member_address(id);
			const ValueId referent = emit_load(address, address.type);
			const Instruction& load = block().instructions.back();
			return LoweredValue(temporary_operand(referent,
				load.destination_name_id), lvalue_type(id), true, address.type);
		}
		const LoweredValue address = lower_address(id);
		return LoweredValue(address.value, lvalue_type(id), true);
	}

LowType Pa15Lowerer::size_low_type() const{
		LowType type;
		type.kind = LowType::TYPE_INTEGER;
		type.integer_kind = LowType::INTEGER_I64;
		return type;
	}
LoweredValue Pa15Lowerer::lower_sizeof(const SemanticFact& fact){
		if (!fact.has_literal_value)
			throw std::runtime_error("PA15 sizeof fact has no typed value");
		if (fact.literal_value > static_cast<std::uint64_t>(std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 sizeof value exceeds LowIR signed integer range");
		const LowType type = size_low_type();
		Instruction instruction;
		instruction.kind = Instruction::IK_CONST;
		instruction.first = integer_operand(
			static_cast<long long>(fact.literal_value), type);
		const ValueId value = destination(type, &instruction);
		block().instructions.push_back(instruction);
		return LoweredValue(temporary_operand(value,
			instruction.destination_name_id), low_type(fact.type), false, type);
	}

LoweredValue Pa15Lowerer::literal(const SemanticFact& fact){
	if (fact.literal_float != InvalidIdentityValue)
	{
		if (fact.literal_float >= model_.floating_literal_facts_.size())
			throw std::runtime_error("PA15 invalid floating literal fact");
		const FloatingLiteralFact& literal =
			model_.floating_literal_facts_[fact.literal_float];
		if (literal.byte_begin > model_.floating_literal_bytes_.size() ||
			literal.byte_count > model_.floating_literal_bytes_.size() -
			literal.byte_begin)
			throw std::runtime_error("PA15 invalid floating literal payload");
		FundamentalType fact_type;
		if (!model_.fundamental_of(fact.type, &fact_type) ||
			fact_type != literal.type)
			throw std::runtime_error("PA15 floating literal type mismatch");
		const LowType type = low_type(fact.type);
		if (!type.is_float())
			throw std::runtime_error("PA15 floating literal has non-floating type");
		Operand operand;
		operand.kind = Operand::OP_FLOAT;
		const std::uint8_t* bytes = model_.floating_literal_bytes_.data() +
			literal.byte_begin;
		if (literal.type == FundamentalType::Float)
		{
			if (literal.byte_count != sizeof(float))
				throw std::runtime_error("PA15 invalid f32 literal payload");
			float value;
			std::memcpy(&value, bytes, sizeof(value));
			operand.float_value = static_cast<long double>(value);
		}
		else if (literal.type == FundamentalType::Double)
		{
			if (literal.byte_count != sizeof(double))
				throw std::runtime_error("PA15 invalid f64 literal payload");
			double value;
			std::memcpy(&value, bytes, sizeof(value));
			operand.float_value = static_cast<long double>(value);
		}
		else if (literal.type == FundamentalType::LongDouble)
		{
			if (literal.byte_count != sizeof(long double))
				throw std::runtime_error("PA15 invalid f80 literal payload");
			long double value;
			std::memcpy(&value, bytes, sizeof(value));
			operand.float_value = value;
		}
		else
			throw std::runtime_error("PA15 unsupported floating literal type");
		operand.literal_type = type;
		// Preserve source spelling only for a source floating literal.  A typed
		// value-initialized zero has a braced source node whose spelling is not
		// a floating literal, so give the LowIR parser a typed spelling.
		if (fact.source != NULL && fact.source->kind == PA10NodeKind::Literal &&
			fact.source->text != 0)
			operand.presentation_id = intern_spelling(
				model_.ast_.spelling(fact.source->text));
		else
			operand.presentation_id = intern_spelling(
				type.float_kind == LowType::FLOAT_F32 ? "0.0F" :
				type.float_kind == LowType::FLOAT_F80 ? "0.0L" : "0.0");
		return LoweredValue(operand, type, false);
	}
	long long value = 0;
		if (fact.has_literal_value)
		{
			__int128 signed_value = static_cast<__int128>(fact.literal_value);
			if (fact.literal_value_negative) signed_value = -signed_value;
			value = static_cast<long long>(signed_value);
		}
		else if (fact.token == SimpleTokenType::KW_TRUE)
			value = 1;
		else if (fact.token == SimpleTokenType::KW_FALSE)
			value = 0;
		else if (fact.token == SimpleTokenType::KW_NULLPTR)
		{
			const LowType type = low_type(fact.type);
			if (type.is_pointer())
			{
				Operand operand = integer_operand(0, type);
				operand.presentation_id = intern_spelling("nullptr");
				Instruction instruction;
				instruction.kind = Instruction::IK_COPY;
				instruction.type = type;
				instruction.first = operand;
				const ValueId result = destination(type, &instruction);
				block().instructions.push_back(instruction);
				return LoweredValue(temporary_operand(result,
					instruction.destination_name_id), type, false);
			}
			if (!type.is_integer() || type.integer_width() != 64)
				throw std::runtime_error("PA15 nullptr literal has invalid carrier");
			Operand operand = integer_operand(0, type);
			operand.presentation_id = intern_spelling("nullptr");
			return LoweredValue(operand, type, false);
		}
		else if (fact.source != NULL && fact.source->kind == PA10NodeKind::Literal)
		{
			const ConstValue decoded = model_.literal_constant(*fact.source);
			if (!decoded.valid) throw std::runtime_error("PA15 invalid literal value");
			value = static_cast<long long>(decoded.value);
		}
		else
			throw std::runtime_error("PA15 unsupported literal value");
		const LowType type = low_type(fact.type);
		Operand operand = integer_operand(value, type);
		if (type.is_pointer() && value == 0 && fact.source != NULL &&
			fact.source->kind == PA10NodeKind::BracedInitList &&
			fact.source->children.empty())
			operand.presentation_id = intern_spelling("nullptr");
		return LoweredValue(operand, type, false);
	}

LoweredValue Pa15Lowerer::apply_reinterpret_conversion(LoweredValue result,
	const LowType& target){
	if (result.lvalue && result.type.is_pointer())
		materialize_lvalue_value(&result, result.type);
	if (result.value.kind == Operand::OP_INTEGER && target.is_pointer())
	{
		// PA13 permits a pointer literal only for the typed null value.  A
		// nonzero integer-to-pointer reinterpret has no representable LowIR
		// conversion in this stage; reject it before emitting an invalid copy.
		if (result.value.int_value != 0)
			throw std::runtime_error(
				"PA15 unsupported nonzero integer-to-pointer reinterpret");
		result.value.literal_type = target;
		Instruction instruction;
		instruction.kind = Instruction::IK_COPY;
		instruction.type = target;
		instruction.first = result.value;
		const ValueId value = destination(target, &instruction);
		block().instructions.push_back(instruction);
		result = LoweredValue(temporary_operand(value,
			instruction.destination_name_id), target, false);
	}
	else if (!(result.type.is_pointer() && target.is_pointer()))
		throw std::runtime_error("PA15 unsupported reinterpret conversion");
	result.type = target;
	result.physical_type = target;
	result.lvalue = false;
	return result;
}
bool Pa15Lowerer::apply_structural_conversion(LoweredValue* result,
	const ConversionFact& conversion, const LowType& target,
	bool omit_boolean_context, bool materialize_lvalue, bool suppress_bit_field_copy){
	if (result == NULL)
		throw std::runtime_error("PA15 missing conversion result");
	if (conversion.kind == ConversionKind::Identity)
	{
		FundamentalType target_fundamental;
		if (model_.fundamental_of(conversion.target, &target_fundamental) &&
			target_fundamental == FundamentalType::Bool)
		{
			if (result->canonical_truth &&
				result->canonical_truth_policy == CanonicalTruthPolicy::Materialize &&
				!omit_boolean_context && result->physical_type != target)
			{
				if (!result->physical_type.is_integer() ||
					!target.is_integer() ||
					result->physical_type.integer_width() <= target.integer_width())
					throw std::runtime_error(
						"PA15 canonical truth cannot materialize as bool");
				Instruction instruction;
				instruction.kind = Instruction::IK_CONVERT;
				instruction.source_type = result->physical_type;
				instruction.first = result->value;
				instruction.conversion_operator = lowir_model::COP_TRUNC;
				const ValueId value = destination(target, &instruction);
				block().instructions.push_back(instruction);
				*result = LoweredValue(temporary_operand(value,
					instruction.destination_name_id), target, false);
			}
			else if (result->canonical_truth)
				result->type = target;
			else if (result->physical_type.is_integer() &&
				result->physical_type != target)
			{
				const LowType physical = result->physical_type;
				const LoweredValue zero(integer_operand(0, physical),
					physical, false);
				*result = emit_compare_value(lowir_model::CPP_NE, physical,
					*result, zero);
				result->type = target;
				result->physical_type = physical;
			}
		}
		return true;
	}
	if (conversion.kind == ConversionKind::LvalueToRvalue)
	{
		materialize_lvalue_value(result, target, !omit_boolean_context && !suppress_bit_field_copy);
		result->type = target;
		result->physical_type = target;
		result->lvalue = false;
		return true;
	}
	if (conversion.kind == ConversionKind::ClassValue)
	{
		// ClassValue is consumed only by lower_call after it proves the complete
		// canonical constructor signature and creates the opaque object slot.
		// A generic conversion walk has no constructor identity, so it must never
		// materialize a class value on a per-argument or target-type shortcut.
		throw std::runtime_error(
			"PA15 class-value materialization requires validated constructor call");
	}
	if (conversion.kind == ConversionKind::DerivedToBase)
	{
		*result = apply_derived_base_conversion(*result, conversion, target);
		return true;
	}
	if (conversion.kind == ConversionKind::ArrayToPointer)
	{
		// Literal arrays use their typed constant-address lowering.
		if (result->lvalue)
		{
			const LoweredValue address = address_of_storage(*result);
			*result = emit_decay(address);
		}
		else if (!result->type.is_pointer())
			throw std::runtime_error("PA15 array decay lost its lvalue");
		result->type = target;
		result->physical_type = target;
		result->lvalue = false;
		return true;
	}
	if (conversion.kind == ConversionKind::ReferenceBinding)
	{
		if (result->bit_field_lvalue)
			return apply_bit_field_reference_conversion(result, conversion, target);
		const TypeId referred_type = model_.strip_cv_type(
			model_.expression_object_type(conversion.target));
		const bool reference_to_function = referred_type.valid() &&
			model_.type_kind(referred_type) == TypeKind::Function;
		if (result->lvalue)
			*result = address_of_storage(*result);
		else if (!reference_to_function)
		{
			// A converted prvalue owns a typed temporary in the called frame.
			const LowType referred = low_reference_value_type(conversion.target);
			const LoweredValue temporary = generated_slot(referred, "refarg");
			emit_store(referred, result->value, temporary.value);
			*result = address_of_storage(temporary);
		}
		result->type = target;
		result->physical_type = target;
		result->lvalue = false;
		return true;
	}
	if (conversion.kind == ConversionKind::FunctionToPointer)
	{
		if (result->lvalue)
			*result = address_of_storage(*result);
		if (!result->type.is_pointer())
			throw std::runtime_error("PA15 function conversion has no pointer");
		*result = emit_decay(*result);
		result->type = target;
		result->physical_type = target;
		result->lvalue = false;
		return true;
	}
	if (conversion.kind == ConversionKind::Reinterpret)
	{
		*result = apply_reinterpret_conversion(*result, target);
		return true;
	}
	if (conversion.kind == ConversionKind::PointerQualification ||
		conversion.kind == ConversionKind::PointerToVoid ||
		conversion.kind == ConversionKind::NullptrToPointer ||
		conversion.kind == ConversionKind::NullIntegerToPointer ||
		conversion.kind == ConversionKind::NullIntegerToNullptr)
	{
		*result = apply_pointer_conversion(*result, conversion, target);
		return true;
	}
	if (conversion.kind == ConversionKind::PointerToBool ||
		conversion.kind == ConversionKind::NullptrToBool)
	{
		materialize_lvalue_value(result, result->type);
		const LowType source = result->physical_type;
		if (conversion.kind == ConversionKind::NullptrToBool)
		{
			const TypeId source_semantic =
				conversion.source.valid() &&
				conversion.source.value < model_.types_.size()
					? model_.strip_cv_type(
						model_.expression_object_type(conversion.source))
					: TypeId();
			const TypeId target_semantic =
				conversion.target.valid() &&
				conversion.target.value < model_.types_.size()
					? model_.strip_cv_type(conversion.target)
					: TypeId();
			const LowType bool_type =
				low_type(model_.fundamental(FundamentalType::Bool));
			if (!source_semantic.valid() ||
				model_.type_kind(source_semantic) != TypeKind::Fundamental ||
				model_.types_[source_semantic.value].fundamental !=
					FundamentalType::NullptrT ||
				!target_semantic.valid() ||
				model_.type_kind(target_semantic) != TypeKind::Fundamental ||
				model_.types_[target_semantic.value].fundamental !=
					FundamentalType::Bool ||
				source != size_low_type() || target != bool_type)
				throw std::runtime_error(
					"PA15 nullptr-to-bool has invalid typed endpoint or carrier");
		}
		else if (!source.is_pointer())
			throw std::runtime_error("PA15 pointer-to-bool source is not a pointer");
		const LoweredValue zero(integer_operand(0, source), source, false);
		LoweredValue truth = emit_compare_value(lowir_model::CPP_NE, source,
			*result, zero);
		truth.type = target;
		if (conversion.kind == ConversionKind::NullptrToBool &&
			!omit_boolean_context)
		{
			if (!target.is_integer() || source.integer_width() <=
				target.integer_width())
				throw std::runtime_error(
					"PA15 nullptr-to-bool target has invalid width");
			Instruction instruction;
			instruction.kind = Instruction::IK_CONVERT;
			instruction.source_type = source;
			instruction.first = truth.value;
			instruction.conversion_operator = lowir_model::COP_TRUNC;
			const ValueId value = destination(target, &instruction);
			block().instructions.push_back(instruction);
			*result = LoweredValue(temporary_operand(value,
				instruction.destination_name_id), target, false);
		}
		else
			*result = truth;
		return true;
	}
	return false;
}
LoweredValue Pa15Lowerer::apply_conversions(SemanticFactId id, LoweredValue result,
	bool omit_boolean_context, bool materialize_lvalue,
	bool force_integral_literal_conversion, bool suppress_bit_field_copy, std::size_t conversion_first, std::size_t conversion_last){
		validate_conversion_range(id); const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (conversion_first > fact.conversion_count || (conversion_last != InvalidIdentityValue && (conversion_last > fact.conversion_count || conversion_first > conversion_last)))
			throw std::runtime_error("PA15 invalid conversion subrange");
		std::size_t conversion_end = conversion_last == InvalidIdentityValue ? fact.conversion_count : conversion_last;
		if (omit_boolean_context && conversion_first != conversion_end)
		{
			FundamentalType target_fundamental;
			const ConversionFact& last = model_.conversion_facts_[
				fact.conversion_begin + conversion_end - 1];
			if (model_.fundamental_of(model_.expression_object_type(last.target),
				&target_fundamental) && target_fundamental == FundamentalType::Bool &&
				!model_.floating_id(last.source))
				--conversion_end;
		}
		for (std::size_t i = conversion_first; i < conversion_end; ++i)
		{
			const ConversionFact& conversion = model_.conversion_facts_[
				fact.conversion_begin + i];
			const bool source_is_bool = model_.bool_id(conversion.source);
			LowType source_type;
			if (source_is_bool)
				source_type = low_type(conversion.source);
			LowType target = low_type(conversion.target);
			if (force_integral_literal_conversion && target.is_integer() &&
				target.integer_width() == 64)
				target = size_low_type();
			if (conversion.kind == ConversionKind::ToVoid)
			{
				// The source was lowered in discarded context, so no scalar
				// conversion or result is needed for a void target.
				result = LoweredValue(Operand(), target, false);
				continue;
			}
			// Comparisons and short-circuit expressions carry canonical truth as
			// physical i64.  PA12 owns whether this conversion crosses the
			// semantic bool representation; preserve that disposition through
			// LoweredValue instead of inferring it from the current block.
			// A conversion record owns one disposition.  Reset the carried
			// policy for every record, including a later non-bool structural
			// conversion, so an earlier Preserve cannot leak across the range.
			if (result.canonical_truth)
				result.canonical_truth_policy = conversion.canonical_truth_policy;
			if (result.canonical_truth && source_is_bool &&
				conversion.canonical_truth_policy == CanonicalTruthPolicy::Materialize &&
				conversion.kind != ConversionKind::Identity)
			{
				if (!result.physical_type.is_integer() ||
					!source_type.is_integer() ||
					result.physical_type.integer_width() <= source_type.integer_width())
					throw std::runtime_error(
						"PA15 canonical truth cannot feed bool conversion");
				Instruction instruction;
				instruction.kind = Instruction::IK_CONVERT;
				instruction.source_type = result.physical_type;
				instruction.first = result.value;
				instruction.conversion_operator = lowir_model::COP_TRUNC;
				const ValueId value = destination(source_type, &instruction);
				block().instructions.push_back(instruction);
				result = LoweredValue(temporary_operand(value,
					instruction.destination_name_id), source_type, false);
			}
			if (apply_structural_conversion(&result, conversion, target,
				omit_boolean_context, materialize_lvalue, suppress_bit_field_copy))
				continue;
			if (!source_is_bool)
				source_type = low_type(conversion.source);
			FundamentalType target_fundamental;
			const bool target_is_bool = model_.fundamental_of(
				model_.expression_object_type(conversion.target), &target_fundamental) &&
				target_fundamental == FundamentalType::Bool;
			if (source_type.is_float() && target_is_bool)
			{
				// PA13 branches consume integer truth values.  Compare against a
				// zero operand carrying the source float width, including f80.
				materialize_lvalue_value(&result, source_type, !omit_boolean_context && !suppress_bit_field_copy);
				Operand zero_operand = floating_operand(0.0L, source_type);
				zero_operand.presentation_id = intern_spelling(
					source_type.float_kind == LowType::FLOAT_F32 ? "0.0F" :
					source_type.float_kind == LowType::FLOAT_F80 ? "0.0L" : "0.0");
				const LoweredValue zero(zero_operand, source_type, false);
				result = emit_compare_value(lowir_model::CPP_NE, source_type,
					result, zero);
				result.type = target;
				continue;
			}
			if (source_type.is_float() || target.is_float() ||
				conversion.kind == ConversionKind::Floating)
			{
				materialize_lvalue_value(&result, source_type,
					!suppress_bit_field_copy);
				if (source_type == target)
				{
					result.type = target;
					result.physical_type = target;
					continue;
				}
				if ((!source_type.is_integer() && !source_type.is_float()) ||
					(!target.is_integer() && !target.is_float()))
					throw std::runtime_error("PA15 non-scalar floating conversion");
				Instruction instruction;
				instruction.kind = Instruction::IK_CONVERT;
				instruction.source_type = source_type;
				instruction.first = result.value;
				instruction.conversion_operator = conversion_operator(conversion);
				const ValueId value = destination(target, &instruction);
				block().instructions.push_back(instruction);
				result = LoweredValue(temporary_operand(value,
					instruction.destination_name_id), target, false);
				continue;
			}
			const LowType materialization_type = result.bit_field_lvalue &&
				source_type.is_integer() && target.is_integer() &&
				source_type.integer_width() == target.integer_width() ?
				target : source_type;
			materialize_lvalue_value(&result, materialization_type,
				!omit_boolean_context && !suppress_bit_field_copy);
			if (target_is_bool && result.value.kind != Operand::OP_INTEGER &&
				result.physical_type.is_integer())
			{
				const LowType boolean_compare = result.physical_type;
				const LoweredValue zero(integer_operand(0, boolean_compare),
					boolean_compare, false);
				result = emit_compare_value(lowir_model::CPP_NE, boolean_compare,
					result, zero);
				result.type = target;
				continue;
			}
			if (conversion.kind != ConversionKind::Integral ||
				!source_type.is_integer() || !target.is_integer())
				throw std::runtime_error("PA15 unsupported scalar conversion");
			if (source_type == target)
			{
				result.type = target;
				result.physical_type = target;
				continue;
			}
			if (result.value.kind == Operand::OP_INTEGER)
			{
				result = apply_integral_literal_conversion(result, conversion,
					source_type, target, force_integral_literal_conversion);
				continue;
			}
			if (source_type.integer_width() == target.integer_width())
			{
				if (result.physical_type != target)
				{
					Instruction instruction;
					instruction.kind = Instruction::IK_COPY;
					instruction.type = target;
					instruction.first = result.value;
					const ValueId value = destination(target, &instruction);
					block().instructions.push_back(instruction);
					result.value = temporary_operand(value,
						instruction.destination_name_id);
				}
				result.type = target;
				result.physical_type = target;
				continue;
			}
			Instruction instruction;
			instruction.kind = Instruction::IK_CONVERT;
			instruction.source_type = result.physical_type.is_integer() &&
				result.physical_type.integer_width() == source_type.integer_width() ?
				result.physical_type : source_type;
			instruction.first = result.value;
			instruction.conversion_operator = conversion_operator(conversion);
			const ValueId value = destination(target, &instruction);
			block().instructions.push_back(instruction);
			result = LoweredValue(temporary_operand(value,
				instruction.destination_name_id), target, false);
		}
		if (materialize_lvalue && result.lvalue && !result.type.is_object())
		{
			if (omit_boolean_context && result.has_condition_value)
			{
				result.value = result.condition_value;
				result.physical_type = result.type;
				result.lvalue = false;
			}
			else
				materialize_lvalue_value(&result, result.type,
					!omit_boolean_context && !suppress_bit_field_copy);
		}
		return result;
	}
LoweredValue Pa15Lowerer::apply_pointer_conversion(LoweredValue result,
	const ConversionFact& conversion, const LowType& target){
	if (conversion.kind == ConversionKind::NullptrToPointer)
	{
		const TypeId source_semantic =
			conversion.source.valid() &&
			conversion.source.value < model_.types_.size()
				? model_.strip_cv_type(
					model_.expression_object_type(conversion.source))
				: TypeId();
		const TypeId target_semantic =
			conversion.target.valid() &&
			conversion.target.value < model_.types_.size()
				? model_.strip_cv_type(conversion.target)
				: TypeId();
		if (!source_semantic.valid() ||
			model_.type_kind(source_semantic) != TypeKind::Fundamental ||
			model_.types_[source_semantic.value].fundamental !=
				FundamentalType::NullptrT ||
			!target_semantic.valid() ||
			model_.type_kind(target_semantic) != TypeKind::Pointer ||
			!target.is_pointer())
			throw std::runtime_error(
				"PA15 nullptr-to-pointer conversion has invalid typed endpoint");

		// Evaluate the source before replacing its representation.  nullptr_t
		// has one value, so the result is always null after this evaluation,
		// including an lvalue load.
		if (result.lvalue)
			materialize_lvalue_value(&result, result.type);
		if (result.physical_type != size_low_type())
			throw std::runtime_error(
				"PA15 nullptr-to-pointer source has invalid carrier");
		Operand operand = integer_operand(0, target);
		operand.presentation_id = intern_spelling("nullptr");
		Instruction instruction;
		instruction.kind = Instruction::IK_COPY;
		instruction.type = target;
		instruction.first = operand;
		const ValueId value = destination(target, &instruction);
		block().instructions.push_back(instruction);
		return LoweredValue(temporary_operand(value,
			instruction.destination_name_id), target, false);
	}
	if (conversion.kind == ConversionKind::NullIntegerToNullptr)
	{
		const TypeId source_semantic =
			conversion.source.valid() &&
			conversion.source.value < model_.types_.size()
				? model_.strip_cv_type(
					model_.expression_object_type(conversion.source))
				: TypeId();
		const TypeId target_semantic =
			conversion.target.valid() &&
			conversion.target.value < model_.types_.size()
				? model_.strip_cv_type(conversion.target)
				: TypeId();
		FundamentalType source_fundamental = FundamentalType::Void;
		bool source_is_integral = false;
		if (source_semantic.valid() &&
			model_.type_kind(source_semantic) == TypeKind::Fundamental &&
			model_.types_[source_semantic.value].fundamental !=
				FundamentalType::Void)
		{
			source_fundamental =
				model_.types_[source_semantic.value].fundamental;
			source_is_integral = model_.integral_type(source_fundamental);
		}
		if (!source_is_integral || !target_semantic.valid() ||
			model_.type_kind(target_semantic) != TypeKind::Fundamental ||
			model_.types_[target_semantic.value].fundamental !=
				FundamentalType::NullptrT ||
			target != size_low_type() || result.lvalue ||
			result.value.kind != Operand::OP_INTEGER ||
			result.value.int_value != 0)
			throw std::runtime_error(
				"PA15 integer-to-nullptr conversion has invalid typed zero");

		result.value.literal_type = target;
		result.type = target;
		result.physical_type = target;
		result.lvalue = false;
		return result;
	}
	if (result.lvalue && (conversion.kind == ConversionKind::PointerQualification ||
		conversion.kind == ConversionKind::PointerToVoid))
		materialize_lvalue_value(&result, result.type);
	if (result.value.kind == Operand::OP_INTEGER && result.value.int_value == 0)
		result.value.literal_type = target;
	result.type = target;
	result.physical_type = target;
	result.lvalue = false;
	return result;
}
LoweredValue Pa15Lowerer::apply_integral_literal_conversion(
	const LoweredValue& source, const ConversionFact& conversion,
	const LowType& source_type, const LowType& target,
	bool force_integral_literal_conversion){
	LoweredValue result = source;
	const bool unsigned_wide_literal_conversion = target.is_integer() &&
		target.integer_width() == 64 &&
		model_.unsigned_integral_type(conversion.target);
	if ((force_integral_literal_conversion || unsigned_wide_literal_conversion) &&
		source_type.integer_width() != target.integer_width())
	{
		Instruction instruction;
		instruction.kind = Instruction::IK_CONVERT;
		instruction.source_type = source_type;
		instruction.first = result.value;
		instruction.conversion_operator = conversion_operator(conversion);
		const ValueId value = destination(target, &instruction);
		block().instructions.push_back(instruction);
		result.value = temporary_operand(value, instruction.destination_name_id);
	}
	result.type = target;
	result.value.literal_type = target;
	result.physical_type = target;
	return result;
}
lowir_model::ConversionOperator Pa15Lowerer::conversion_operator(const ConversionFact& conversion) const{
		const LowType source = low_type(conversion.source);
		const LowType target = low_type(conversion.target);
		if (source.is_integer() && target.is_integer())
		{
			if (target.integer_width() < source.integer_width())
				return lowir_model::COP_TRUNC;
			if (target.integer_width() > source.integer_width())
			{
				FundamentalType source_fundamental;
				if (model_.fundamental_of(conversion.source, &source_fundamental) &&
					(source_fundamental == FundamentalType::Bool ||
						model_.unsigned_type(source_fundamental)))
					return lowir_model::COP_ZEXT;
				if (model_.unsigned_integral_type(conversion.source))
					return lowir_model::COP_ZEXT;
				return lowir_model::COP_SEXT;
			}
			throw std::runtime_error("PA15 same-width conversion reached instruction emission");
		}
		if (source.is_integer() && target.is_float())
			return model_.unsigned_integral_type(conversion.source) ?
				lowir_model::COP_UITOFP : lowir_model::COP_SITOFP;
		if (source.is_float() && target.is_integer())
			return model_.unsigned_integral_type(conversion.target) ?
				lowir_model::COP_FPTOUI : lowir_model::COP_FPTOSI;
		if (source.is_float() && target.is_float())
		{
			if (target.float_kind > source.float_kind)
				return lowir_model::COP_FPEXT;
			if (target.float_kind < source.float_kind)
				return lowir_model::COP_FPTRUNC;
		}
		throw std::runtime_error("PA15 unsupported scalar conversion");
}

LoweredValue Pa15Lowerer::emit_binary_value(lowir_model::BinaryOperator operation,
		const LowType& type, const LoweredValue& left, const LoweredValue& right){
		Instruction instruction;
		instruction.kind = Instruction::IK_BINARY;
		instruction.binary_operator = operation;
		instruction.type = type;
		instruction.first = left.value;
		instruction.second = right.value;
		const ValueId value = destination(type, &instruction);
		block().instructions.push_back(instruction);
		return LoweredValue(temporary_operand(value, instruction.destination_name_id),
			type, false);
	}

LoweredValue Pa15Lowerer::emit_compare_value(lowir_model::ComparePredicate predicate,
		const LowType& type, const LoweredValue& left, const LoweredValue& right){
		Instruction instruction;
		instruction.kind = Instruction::IK_CMP;
		instruction.compare_predicate = predicate;
		instruction.type = type;
		instruction.first = left.value;
		instruction.second = right.value;
		LowType result_type;
		result_type.kind = LowType::TYPE_INTEGER;
		result_type.integer_kind = LowType::INTEGER_I64;
		instruction.result_type = result_type;
		const ValueId value = destination(result_type, &instruction);
		instruction.type = type;
		block().instructions.push_back(instruction);
		LoweredValue result(temporary_operand(value, instruction.destination_name_id),
		result_type, false);
		result.canonical_truth = true;
		return result;
	}

LoweredValue Pa15Lowerer::integer_i64(const LoweredValue& source,
	TypeId source_type){
		LowType i64;
		i64.kind = LowType::TYPE_INTEGER;
		i64.integer_kind = LowType::INTEGER_I64;
		if (!source.type.is_integer())
			throw std::runtime_error("PA15 pointer offset is not integral");
		if (source.value.kind == Operand::OP_INTEGER)
		{
			Operand value = source.value;
			value.literal_type = i64;
			return LoweredValue(value, i64, false);
		}
		if (source.type == i64)
			return LoweredValue(source.value, i64, false, source.physical_type);
		if (source.type.integer_width() == i64.integer_width())
			return LoweredValue(source.value, i64, false, i64);
		Instruction instruction;
		instruction.kind = Instruction::IK_CONVERT;
		instruction.source_type = source.type;
		instruction.first = source.value;
		instruction.conversion_operator = unsigned_type_for(source_type) ?
			lowir_model::COP_ZEXT : lowir_model::COP_SEXT;
		const ValueId value = destination(i64, &instruction);
		block().instructions.push_back(instruction);
		return LoweredValue(temporary_operand(value, instruction.destination_name_id),
			i64, false);
	}

std::size_t Pa15Lowerer::pointer_element_size(TypeId type) const{
		type = model_.expression_object_type(type);
		while (type.valid() && model_.type_kind(type) == TypeKind::Cv)
			type = model_.types_[type.value].child;
		if (type.valid() && model_.type_kind(type) == TypeKind::Array)
			type = model_.types_[type.value].child;
		else if (type.valid() && model_.type_kind(type) == TypeKind::Pointer)
			type = model_.types_[type.value].child;
		if (!type.valid()) return 1;
		return model_.type_size(type);
	}

LoweredValue Pa15Lowerer::pointer_offset(const LoweredValue& base, TypeId base_type,
		const LoweredValue& amount, TypeId amount_type, bool negative){
		const LowType offset_type = []() {
			LowType result;
			result.kind = LowType::TYPE_INTEGER;
			result.integer_kind = LowType::INTEGER_I64;
			return result;
		}();
		LoweredValue scaled = integer_i64(amount, amount_type);
		const std::size_t element_size = pointer_element_size(base_type);
		if (element_size != 1)
			scaled = emit_binary_value(lowir_model::BOP_MUL, offset_type, scaled,
				LoweredValue(integer_operand(static_cast<long long>(element_size),
					offset_type), offset_type, false));
		if (negative)
			scaled = emit_binary_value(lowir_model::BOP_SUB, offset_type,
				LoweredValue(integer_operand(0, offset_type), offset_type, false), scaled);
		LowType byte;
		byte.kind = LowType::TYPE_INTEGER;
		byte.integer_kind = LowType::INTEGER_I8;
		return emit_index(base, scaled, byte, lowir_model::IPK_NONE);
	}

LoweredValue Pa15Lowerer::lower_incdec(SemanticFactId id, bool postfix){
		const std::vector<SemanticFactId> operands = children(id);
		if (operands.size() != 1)
			throw std::runtime_error("PA15 invalid increment fact");
		const LoweredValue left = lower_lvalue(operands.front());
		LowType target_type = left.type;
		if (left.bit_field_lvalue && fact_token(id) == SimpleTokenType::OP_DEC)
		{
			const BitFieldFact* bit_field = model_.bit_field_fact(
				left.bit_field_binding);
			FundamentalType storage_fundamental;
			if (bit_field != NULL && model_.fundamental_of(bit_field->storage_type,
				&storage_fundamental) && storage_fundamental == FundamentalType::Bool)
				throw std::runtime_error(
					"PA15 decrement of a bool bit-field is not allowed");
		}
		LoweredValue old;
		if (left.bit_field_lvalue)
			old = emit_bit_field_load(left, left.bit_field_binding, target_type);
		else
		{
			const ValueId old_id = emit_load(left, target_type);
			const Instruction& old_instruction = block().instructions.back();
			old = LoweredValue(temporary_operand(old_id,
				old_instruction.destination_name_id), target_type, false);
		}
		LoweredValue amount(integer_operand(1, []() {
			LowType result;
			result.kind = LowType::TYPE_INTEGER;
			result.integer_kind = LowType::INTEGER_I64;
			return result;
		}()), []() {
			LowType result;
			result.kind = LowType::TYPE_INTEGER;
			result.integer_kind = LowType::INTEGER_I64;
			return result;
		}(), false);
		LoweredValue updated;
		if (target_type.is_pointer())
			updated = pointer_offset(old, model_.semantic_facts_[operands.front().value].type,
				amount, model_.fundamental(FundamentalType::Int),
				fact_token(id) == SimpleTokenType::OP_DEC);
		else
		{
			const LowType operation_type = target_type;
			LoweredValue one(integer_operand(1, operation_type), operation_type, false);
			updated = emit_binary_value(fact_token(id) == SimpleTokenType::OP_DEC ?
				lowir_model::BOP_SUB : lowir_model::BOP_ADD, operation_type, old, one);
		}
		if (left.bit_field_lvalue)
		{
			// Prefix and postfix updates both compute their encoded value before
			// the packed-unit RMW.  Prefix returns the updated lvalue boundary,
			// so give its store a fresh projection; postfix can use the already
			// evaluated projection because its result is the old value.
			const LoweredValue operation_storage = postfix ? left :
				reproject_bit_field_address(left);
			const LoweredValue encoded = encode_bit_field_value(
				left.bit_field_binding, updated);
			emit_encoded_bit_field_store(operation_storage,
				left.bit_field_binding, encoded, true);
		}
		else
			emit_store(target_type, updated.value, left.value);
		if (postfix) return old;
		LoweredValue result = left;
		result.type = target_type;
		result.physical_type = target_type;
		result.lvalue = true;
		result.condition_value = updated.value;
		result.has_condition_value = true;
		return result;
	}

SimpleTokenType Pa15Lowerer::fact_token(SemanticFactId id) const{
		return model_.semantic_facts_[id.value].token;
	}

LoweredValue Pa15Lowerer::lower_assignment(SemanticFactId id, bool preserve_lvalue){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const std::vector<SemanticFactId> operands = children(id);
		if (operands.size() != 2)
			throw std::runtime_error("PA15 invalid assignment expression");
		LoweredValue right;
		LoweredValue left;
		LoweredValue old;
		const bool compound = fact.token != SimpleTokenType::OP_ASS;
		if (fact.token == SimpleTokenType::OP_ASS)
		{
			right = lower_expression(operands[1]);
			left = lower_lvalue(operands[0]);
		}
		else
		{
			left = lower_lvalue(operands[0]);
			if (left.bit_field_lvalue)
				old = emit_bit_field_load(left, left.bit_field_binding, left.type);
			else
			{
				const ValueId old_id = emit_load(left, left.type);
				const Instruction& old_instruction = block().instructions.back();
				old = LoweredValue(temporary_operand(old_id,
					old_instruction.destination_name_id), left.type, false);
			}
			right = lower_expression(operands[1]);
		}
		const LowType target = left.type;
		const TypeId target_semantic_type = model_.expression_object_type(
			model_.semantic_facts_[operands[0].value].type);
		if (fact.token == SimpleTokenType::OP_ASS)
		{
			if (left.bit_field_lvalue)
				emit_bit_field_store(left, left.bit_field_binding, right);
			else
				emit_store(target, right.value, left.value);
			if (preserve_lvalue)
			{
				LoweredValue result = left;
				result.type = target;
				result.physical_type = target;
				result.lvalue = true;
				return result;
			}
			return
				LoweredValue(right.value, target, false, right.physical_type);
		}
		const bool pointer_compound = target.is_pointer() &&
			(fact.token == SimpleTokenType::OP_PLUSASS ||
			 fact.token == SimpleTokenType::OP_MINUSASS);
		if (!compound)
			throw std::runtime_error("PA15 compound assignment sequencing mismatch");
		LoweredValue updated;
		if (pointer_compound)
			updated = pointer_offset(old, model_.semantic_facts_[operands[0].value].type,
				right, model_.semantic_facts_[operands[1].value].type,
				fact.token == SimpleTokenType::OP_MINUSASS);
		else
		{
			const lowir_model::BinaryOperator operation = binary_operator(
				fact.token == SimpleTokenType::OP_PLUSASS ? SimpleTokenType::OP_PLUS :
				fact.token == SimpleTokenType::OP_MINUSASS ? SimpleTokenType::OP_MINUS :
				fact.token == SimpleTokenType::OP_STARASS ? SimpleTokenType::OP_STAR :
				fact.token == SimpleTokenType::OP_DIVASS ? SimpleTokenType::OP_DIV :
				fact.token == SimpleTokenType::OP_MODASS ? SimpleTokenType::OP_MOD :
				fact.token, unsigned_type_for(target_semantic_type));
			if (operation == lowir_model::BOP_INVALID)
				throw std::runtime_error("PA15 unsupported compound assignment");
			updated = emit_binary_value(operation, target, old,
				LoweredValue(right.value, target, false));
		}
		if (left.bit_field_lvalue)
			emit_bit_field_store(left, left.bit_field_binding, updated);
		else
			emit_store(target, updated.value, left.value);
		if (preserve_lvalue)
		{
			LoweredValue result = left;
			result.type = target;
			result.physical_type = target;
			result.lvalue = true;
			return result;
		}
		return updated;
	}

bool Pa15Lowerer::conditional_address_result(SemanticFactId id) const{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.category == SemanticValueCategory::Prvalue)
			return false;
		const TypeId result_type = model_.strip_cv_type(
			model_.expression_object_type(fact.type));
		if (result_type.valid() && model_.type_kind(result_type) == TypeKind::Array)
			return true;
		if (fact.conversion_begin == InvalidIdentityValue)
			return false;
		for (std::size_t i = 0; i < fact.conversion_count; ++i)
			if (model_.conversion_facts_[fact.conversion_begin + i].kind ==
				ConversionKind::ReferenceBinding ||
				model_.conversion_facts_[fact.conversion_begin + i].kind ==
				ConversionKind::DerivedToBase)
				return true;
		return false;
}

LoweredValue Pa15Lowerer::lower_conditional_value(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 3)
			throw std::runtime_error("PA15 invalid conditional expression");
		const LowType type = low_type(model_.semantic_facts_[id.value].type);
		const LoweredValue result_slot = generated_slot(type, "cond");
		const BlockId then_block = block_id(new_block("cond_then"));
		const BlockId else_block = block_id(new_block("cond_else"));
		const BlockId join_block = block_id(new_block("cond_end"));
		const LoweredValue condition = lower_condition(facts[0]);
		emit_branch(condition.value, then_block, else_block);
		set_current(then_block);
		emit_store(type, lower_expression(facts[1]).value, result_slot.value);
		if (!terminated(block())) emit_jump(join_block);
		set_current(else_block);
		emit_store(type, lower_expression(facts[2]).value, result_slot.value);
		if (!terminated(block())) emit_jump(join_block);
		set_current(join_block);
		const ValueId value = emit_load(result_slot, type);
		const Instruction& emitted = block().instructions.back();
		return LoweredValue(temporary_operand(value, emitted.destination_name_id),
			type, false);
	}


LoweredValue Pa15Lowerer::lower_expression(SemanticFactId id){
		return lower_expression_impl(id, false);
	}

LoweredValue Pa15Lowerer::lower_condition_expression(SemanticFactId id){
		return lower_expression_impl(id, true);
	}

bool Pa15Lowerer::is_comparison(SimpleTokenType token) const{
		return token == SimpleTokenType::OP_EQ || token == SimpleTokenType::OP_NE ||
			token == SimpleTokenType::OP_LT || token == SimpleTokenType::OP_LE ||
			token == SimpleTokenType::OP_GT || token == SimpleTokenType::OP_GE;
	}

lowir_model::ComparePredicate Pa15Lowerer::compare_predicate(SimpleTokenType token,
		bool is_unsigned) const{
		if (token == SimpleTokenType::OP_EQ) return lowir_model::CPP_EQ;
		if (token == SimpleTokenType::OP_NE) return lowir_model::CPP_NE;
		if (token == SimpleTokenType::OP_LT) return is_unsigned ? lowir_model::CPP_ULT : lowir_model::CPP_LT;
		if (token == SimpleTokenType::OP_LE) return is_unsigned ? lowir_model::CPP_ULE : lowir_model::CPP_LE;
		if (token == SimpleTokenType::OP_GT) return is_unsigned ? lowir_model::CPP_UGT : lowir_model::CPP_GT;
		if (token == SimpleTokenType::OP_GE) return is_unsigned ? lowir_model::CPP_UGE : lowir_model::CPP_GE;
		return lowir_model::CPP_INVALID;
	}

bool Pa15Lowerer::unsigned_type_for(TypeId type) const{
		type = model_.strip_cv_type(model_.expression_object_type(type));
		const NamedRecordId record = model_.named_record_for_type(type);
		if (record.valid() && record.value < model_.named_.size() &&
			model_.named_[record.value].kind == NamedKind::Enum)
		{
			if (!model_.named_[record.value].has_underlying)
				return false;
			return unsigned_type_for(model_.named_[record.value].underlying);
		}
		FundamentalType fundamental_type;
		return model_.fundamental_of(type, &fundamental_type) &&
			model_.integral_type(fundamental_type) &&
			model_.unsigned_type(fundamental_type);
	}

lowir_model::BinaryOperator Pa15Lowerer::binary_operator(SimpleTokenType token,
		bool is_unsigned) const{
		switch (token)
		{
		case SimpleTokenType::OP_PLUS: return lowir_model::BOP_ADD;
		case SimpleTokenType::OP_MINUS: return lowir_model::BOP_SUB;
		case SimpleTokenType::OP_STAR: return lowir_model::BOP_MUL;
		case SimpleTokenType::OP_DIV: return is_unsigned ? lowir_model::BOP_UDIV : lowir_model::BOP_DIV;
		case SimpleTokenType::OP_MOD: return is_unsigned ? lowir_model::BOP_UMOD : lowir_model::BOP_MOD;
		case SimpleTokenType::OP_AMP: return lowir_model::BOP_AND;
		case SimpleTokenType::OP_BOR: return lowir_model::BOP_OR;
		case SimpleTokenType::OP_XOR: return lowir_model::BOP_XOR;
		case SimpleTokenType::OP_LSHIFT: return lowir_model::BOP_SHL;
		case SimpleTokenType::OP_RSHIFT: return is_unsigned ? lowir_model::BOP_USHR : lowir_model::BOP_SHR;
		default: return lowir_model::BOP_INVALID;
		}
	}

void Pa15Lowerer::emit_jump(BlockId target){
		const BlockId source = current_block_id();
		Instruction jump;
		jump.kind = Instruction::IK_JUMP;
		jump.first = block_operand(target);
		block().instructions.push_back(jump);
		propagate_edge(source, target);
	}

void Pa15Lowerer::emit_branch(const Operand& condition, BlockId true_target,
		BlockId false_target){
		const BlockId source = current_block_id();
		Instruction branch;
		branch.kind = Instruction::IK_BRANCH;
		branch.first = condition;
		branch.second = block_operand(true_target);
		branch.third = block_operand(false_target);
		block().instructions.push_back(branch);
		propagate_edge(source, true_target);
		propagate_edge(source, false_target);
	}

bool Pa15Lowerer::condition_is_empty(SemanticFactId id) const{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		return fact.kind == SemanticFactKind::Condition && fact.child_count == 0;
}

bool Pa15Lowerer::has_direct_short_circuit(SemanticFactId id) const{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::Condition)
		{
			if (fact.child_count != 1) return false;
			return has_direct_short_circuit(
				model_.semantic_children_[fact.child_begin]);
		}
		if (fact.kind == SemanticFactKind::ConditionDeclaration)
			return false;
		return fact.kind == SemanticFactKind::BinaryExpression &&
			(fact.token == SimpleTokenType::OP_LAND ||
			 fact.token == SimpleTokenType::OP_LOR);
	}

BlockId Pa15Lowerer::switch_label_target(SemanticFactId id){
		if (switch_stack_.empty())
			throw std::runtime_error("PA15 switch label has no owner");
		SwitchContext& context = switch_stack_.back();
		const std::map<std::size_t, BlockId>::const_iterator found =
			context.labels.find(id.value);
		if (found == context.labels.end())
			throw std::runtime_error("PA15 switch label owner mismatch");
		if (!context.lowered_labels.insert(id.value).second)
			throw std::runtime_error("PA15 switch label was lowered twice");
		return found->second;
	}

bool Pa15Lowerer::switch_label_was_lowered(SemanticFactId id) const{
		if (switch_stack_.empty())
			throw std::runtime_error("PA15 switch label context is missing");
		const SwitchContext& context = switch_stack_.back();
		return context.lowered_labels.find(id.value) !=
			context.lowered_labels.end();
	}

bool Pa15Lowerer::switch_subtree_has_label(SemanticFactId id) const{
		if (switch_stack_.empty())
			throw std::runtime_error("PA15 switch label context is missing");
		return switch_stack_.back().label_subtrees.find(id.value) !=
			switch_stack_.back().label_subtrees.end();
	}


void PA11SemanticModel::lower_pa15(lowir_model::Program& program) const
{
	Pa15Lowerer lowerer(*this, program);
	lowerer.run();
}

}  // namespace pa11_semantic_internal

void emit_pa15_lowir(const PA10Ast& ast, lowir_model::Program& program)
{
	pa11_semantic_internal::PA11SemanticModel model(ast);
	model.analyze();
	model.analyze_pa12();
	model.lower_pa15(program);
}
