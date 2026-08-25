#include "abi_mangle.h"
#include "lowir_model.h"
#include "pa11_semantic_model.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pa11_semantic_internal
{

using lowir_model::Block;
using lowir_model::BlockId;
using lowir_model::Function;
using lowir_model::Instruction;
using lowir_model::LowType;
using lowir_model::Operand;
using lowir_model::Program;
using lowir_model::SpellingId;
using lowir_model::SymbolId;
using lowir_model::ValueId;

struct LoweredValue
{
	Operand value;
	LowType type;
	bool lvalue;

	LoweredValue() : value(), type(), lvalue(false) {}
	LoweredValue(const Operand& value, const LowType& type, bool lvalue)
		: value(value), type(type), lvalue(lvalue) {}
};

struct FunctionPlan
{
	std::size_t fact_index;
	std::size_t program_index;

	FunctionPlan(std::size_t fact_index = 0, std::size_t program_index = 0)
		: fact_index(fact_index), program_index(program_index) {}
};

struct ControlTarget
{
	bool loop;
	BlockId break_target;
	BlockId continue_target;

	ControlTarget(bool loop = false, BlockId break_target = BlockId(),
		BlockId continue_target = BlockId())
		: loop(loop), break_target(break_target), continue_target(continue_target) {}
};

struct LoopTarget
{
	bool valid;
	BlockId break_target;
	BlockId continue_target;

	LoopTarget(BlockId break_target = BlockId(),
		BlockId continue_target = BlockId())
		: valid(break_target.valid() && continue_target.valid()),
		  break_target(break_target), continue_target(continue_target) {}
};

struct SwitchArm
{
	SemanticFactId fact;
	BlockId target;
	bool is_default;
	Operand value;

	SwitchArm(SemanticFactId fact = SemanticFactId(),
		BlockId target = BlockId(), bool is_default = false,
		const Operand& value = Operand())
		: fact(fact), target(target), is_default(is_default), value(value) {}
};

struct SwitchContext
{
	BlockId end_target;
	BlockId default_target;
	std::vector<SwitchArm> arms;
	std::map<std::size_t, BlockId> labels;
	std::set<std::size_t> lowered_labels;
	std::set<std::size_t> label_subtrees;

	SwitchContext(BlockId end_target = BlockId())
		: end_target(end_target), default_target(end_target), arms(), labels(),
		  lowered_labels(), label_subtrees() {}
};

class Pa15Lowerer
{
public:
	Pa15Lowerer(const PA11SemanticModel& model, Program& program)
		: model_(model), program_(program), spelling_ids_(), used_symbols_(),
		  used_slot_names_(), used_value_names_(), symbol_collision_counters_(),
		  slot_collision_counters_(), function_symbols_(),
		  function_program_indexes_(), slot_by_binding_(), slot_spellings_(),
		  function_plans_(), function_scope_variables_(), next_symbol_(0),
		  next_value_(program.values.size()),
		  next_slot_(0), next_block_(0), current_function_(0),
		  current_block_(InvalidIdentityValue), temp_ordinal_(0),
		  block_ordinal_(0), block_indexes_(), control_stack_(),
		  switch_stack_(), block_order_(), ordered_block_ids_(),
		  loop_targets_(), reachability_base_(0), reachable_blocks_(),
		  reachability_work_() {}

	void run()
	{
		initialize_spelling_ids();
		initialize_identity_counters();
		clear_value_records();
		collect_functions();
		// SemanticFactId values are translation-unit identities.  Allocate this
		// dense table once, then retain targets across all function lowerings.
		loop_targets_.resize(model_.semantic_facts_.size());
		for (std::size_t i = 0; i < function_plans_.size(); ++i)
			lower_function(function_plans_[i]);
		finalize_value_records();
	}

private:
	const PA11SemanticModel& model_;
	Program& program_;
	std::map<std::string, SpellingId> spelling_ids_;
	std::set<std::string> used_symbols_;
	std::set<std::string> used_slot_names_;
	std::set<std::string> used_value_names_;
	std::map<std::string, std::size_t> symbol_collision_counters_;
	std::map<std::string, std::size_t> slot_collision_counters_;
	std::map<std::size_t, SymbolId> function_symbols_;
	std::map<std::size_t, std::size_t> function_program_indexes_;
	std::map<std::size_t, lowir_model::SlotId> slot_by_binding_;
	std::vector<SpellingId> slot_spellings_;
	std::vector<FunctionPlan> function_plans_;
	// Indexed once from the PA12 scope arena. Entries are in scope and
	// binding creation order, matching the old fallback's deterministic order.
	std::vector<std::vector<BindingId> > function_scope_variables_;
	std::size_t next_symbol_;
	std::size_t next_value_;
	std::size_t next_slot_;
	std::size_t next_block_;
	std::size_t current_function_;
	std::size_t current_block_;
	std::size_t temp_ordinal_;
	std::size_t block_ordinal_;
	std::map<std::size_t, std::size_t> block_indexes_;
	std::vector<ControlTarget> control_stack_;
	std::vector<SwitchContext> switch_stack_;
	std::vector<BlockId> block_order_;
	std::set<std::size_t> ordered_block_ids_;
	std::vector<LoopTarget> loop_targets_;
	std::size_t reachability_base_;
	std::vector<unsigned char> reachable_blocks_;
	std::vector<BlockId> reachability_work_;

	void initialize_spelling_ids()
	{
		for (std::size_t i = 0; i < program_.presentation.size(); ++i)
			spelling_ids_[program_.presentation[i]] = SpellingId(i);
	}

	void initialize_identity_counters()
	{
		for (std::size_t i = 0; i < program_.global_declarations.size(); ++i)
			if (program_.global_declarations[i].symbol_id.valid())
				next_symbol_ = std::max(next_symbol_,
					program_.global_declarations[i].symbol_id.index + 1);
		for (std::size_t i = 0; i < program_.globals.size(); ++i)
			if (program_.globals[i].symbol_id.valid())
				next_symbol_ = std::max(next_symbol_,
					program_.globals[i].symbol_id.index + 1);
		for (std::size_t i = 0; i < program_.functions.size(); ++i)
		{
			const Function& function = program_.functions[i];
			if (function.symbol_id.valid())
				next_symbol_ = std::max(next_symbol_,
					function.symbol_id.index + 1);
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
		for (std::size_t i = 0; i < program_.global_declarations.size(); ++i)
			used_symbols_.insert(spelling(program_.global_declarations[i].name_id));
		for (std::size_t i = 0; i < program_.globals.size(); ++i)
			used_symbols_.insert(spelling(program_.globals[i].name_id));
		for (std::size_t i = 0; i < program_.functions.size(); ++i)
			used_symbols_.insert(spelling(program_.functions[i].name_id));
	}

	const std::string& spelling(SpellingId id) const
	{
		if (!id.valid() || id.index >= program_.presentation.size())
			throw std::runtime_error("PA15 invalid presentation identity");
		return program_.presentation[id.index];
	}

	SpellingId intern_spelling(const std::string& value)
	{
		std::map<std::string, SpellingId>::const_iterator found =
			spelling_ids_.find(value);
		if (found != spelling_ids_.end())
			return found->second;
		const SpellingId result(program_.presentation.size());
		program_.presentation.push_back(value);
		spelling_ids_[value] = result;
		return result;
	}

	SpellingId symbol_spelling(const std::string& name)
	{
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

	std::vector<std::string> function_components(const FunctionFact& fact) const
	{
		std::vector<std::string> reversed;
		ScopeId scope = fact.owner;
		while (scope.valid())
		{
			const Scope& current = model_.scopes_[scope.value];
			if (current.kind == ScopeKind::Namespace && current.name.valid())
				reversed.push_back(model_.name_text(current.name));
			scope = current.parent;
		}
		std::reverse(reversed.begin(), reversed.end());
		reversed.push_back(model_.name_text(
			model_.binding(fact.binding).name));
		return reversed;
	}

	abi_mangle::AbiType abi_type(TypeId type) const
	{
		while (type.valid() && model_.type_kind(type) == TypeKind::Cv)
			type = model_.types_[type.value].child;
		if (!type.valid())
			throw std::runtime_error("PA15 invalid ABI type");
		const TypeKind kind = model_.type_kind(type);
		if (kind == TypeKind::Pointer)
		{
			abi_mangle::AbiType result;
			result.kind = abi_mangle::ABI_TYPE_POINTER;
			result.types.push_back(abi_type(model_.types_[type.value].child));
			return result;
		}
		if (kind != TypeKind::Fundamental)
			throw std::runtime_error("PA15 unsupported ABI parameter type");
		abi_mangle::AbiType result;
		result.kind = abi_mangle::ABI_TYPE_BUILTIN;
		switch (model_.types_[type.value].fundamental)
		{
		case FundamentalType::Void: result.builtin = abi_mangle::ABI_BUILTIN_VOID; break;
		case FundamentalType::WcharT: result.builtin = abi_mangle::ABI_BUILTIN_WCHAR; break;
		case FundamentalType::Bool: result.builtin = abi_mangle::ABI_BUILTIN_BOOL; break;
		case FundamentalType::Char: result.builtin = abi_mangle::ABI_BUILTIN_CHAR; break;
		case FundamentalType::SignedChar: result.builtin = abi_mangle::ABI_BUILTIN_SIGNED_CHAR; break;
		case FundamentalType::UnsignedChar: result.builtin = abi_mangle::ABI_BUILTIN_UNSIGNED_CHAR; break;
		case FundamentalType::ShortInt: result.builtin = abi_mangle::ABI_BUILTIN_SHORT; break;
		case FundamentalType::UnsignedShortInt: result.builtin = abi_mangle::ABI_BUILTIN_UNSIGNED_SHORT; break;
		case FundamentalType::Int: result.builtin = abi_mangle::ABI_BUILTIN_INT; break;
		case FundamentalType::UnsignedInt: result.builtin = abi_mangle::ABI_BUILTIN_UNSIGNED_INT; break;
		case FundamentalType::LongInt: result.builtin = abi_mangle::ABI_BUILTIN_LONG; break;
		case FundamentalType::UnsignedLongInt: result.builtin = abi_mangle::ABI_BUILTIN_UNSIGNED_LONG; break;
		case FundamentalType::LongLongInt: result.builtin = abi_mangle::ABI_BUILTIN_LONG_LONG; break;
		case FundamentalType::UnsignedLongLongInt: result.builtin = abi_mangle::ABI_BUILTIN_UNSIGNED_LONG_LONG; break;
		case FundamentalType::Char16T: result.builtin = abi_mangle::ABI_BUILTIN_CHAR16; break;
		case FundamentalType::Char32T: result.builtin = abi_mangle::ABI_BUILTIN_CHAR32; break;
		case FundamentalType::Float: result.builtin = abi_mangle::ABI_BUILTIN_FLOAT; break;
		case FundamentalType::Double: result.builtin = abi_mangle::ABI_BUILTIN_DOUBLE; break;
		case FundamentalType::LongDouble: result.builtin = abi_mangle::ABI_BUILTIN_LONG_DOUBLE; break;
		default: throw std::runtime_error("PA15 unsupported ABI fundamental type");
		}
		return result;
	}

	std::string abi_symbol(const FunctionFact& fact) const
	{
		const Binding& binding = model_.binding(fact.binding);
		if (model_.type_kind(binding.type) != TypeKind::Function)
			throw std::runtime_error("PA15 function binding has non-function type");
		const TypeKey& function = model_.types_[binding.type.value];
		abi_mangle::AbiFactCase facts;
		abi_mangle::AbiFactRecord record;
		record.kind = abi_mangle::ABI_FACT_RECORD_TARGET;
		record.target.kind = abi_mangle::ABI_TARGET_FACT_FUNCTION;
		record.target.linkage = (binding.internal_linkage ||
			binding.language_linkage == LanguageLinkage::Cxx) ?
			abi_mangle::ABI_LINKAGE_CXX : abi_mangle::ABI_LINKAGE_C;
		record.target.function.kind = abi_mangle::ABI_FUNCTION_TARGET_PATH;
		record.target.function.name.components = function_components(fact);
		for (std::size_t i = 0; i < function.parameters.size(); ++i)
			record.target.function.signature_parameter_types.push_back(
				abi_type(function.parameters[i]));
		facts.records.push_back(record);
		return abi_mangle::mangle_abi_fact_case(facts);
	}

	LowType low_type(TypeId type) const
	{
		while (type.valid() && model_.type_kind(type) == TypeKind::Cv)
			type = model_.types_[type.value].child;
		if (!type.valid())
			throw std::runtime_error("PA15 invalid semantic type");
		const TypeKind kind = model_.type_kind(type);
		if (kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference)
			return low_type(model_.types_[type.value].child);
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
		case FundamentalType::UnsignedLongInt:
		case FundamentalType::UnsignedLongLongInt:
			result.kind = LowType::TYPE_INTEGER; result.integer_kind = LowType::INTEGER_U64; return result;
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

	void index_function_scope_variables()
	{
		function_scope_variables_.assign(model_.scopes_.size(),
			std::vector<BindingId>());
		std::vector<bool> collected_function_scope(model_.scopes_.size(), false);
		for (std::size_t i = 0; i < model_.function_facts_.size(); ++i)
		{
			const FunctionFact& fact = model_.function_facts_[i];
			if (!fact.owner.valid() || fact.owner.value >= model_.scopes_.size() ||
				model_.scopes_[fact.owner.value].kind != ScopeKind::Namespace)
				continue;
			if (!fact.function_scope.valid() ||
				fact.function_scope.value >= model_.scopes_.size())
				throw std::runtime_error("PA15 function scope is missing");
			collected_function_scope[fact.function_scope.value] = true;
		}

		// Scope creation is parent-before-child. Propagate the collected
		// function-scope owner once, then append each variable binding once.
		// This partitions the arena in O(S + B + F), where S is the number of
		// scopes, B the number of bindings, and F the function count.
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

	void collect_functions()
	{
		index_function_scope_variables();
		for (std::size_t i = 0; i < model_.function_facts_.size(); ++i)
		{
			const FunctionFact& fact = model_.function_facts_[i];
			if (!fact.owner.valid() || fact.owner.value >= model_.scopes_.size())
				throw std::runtime_error("PA15 function owner is missing");
			if (model_.scopes_[fact.owner.value].kind != ScopeKind::Namespace)
				continue;
			const Binding& binding = model_.binding(fact.binding);
			if (binding.kind != BindingKind::Function ||
				model_.type_kind(binding.type) != TypeKind::Function)
				throw std::runtime_error("PA15 invalid procedural function fact");

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
			function.return_type = low_type(model_.types_[binding.type.value].result);
			function.metadata.binding = binding.internal_linkage ?
				lowir_model::SBM_INTERNAL : lowir_model::SBM_STRONG;
			if (binding.language_linkage == LanguageLinkage::C)
				function.metadata.linkage = lowir_model::LLM_C;
			const bool is_main = components.size() == 1 && components.front() == "main";
			if (is_main)
			{
				function.metadata.role = lowir_model::SR_ENTRY;
				function.metadata.keep_internal_alias = true;
			}
			else
				function.metadata.object_symbol_id = intern_spelling(abi_symbol(fact));

			const std::size_t function_index = program_.functions.size();
			program_.functions.push_back(function);
			function_plans_.push_back(FunctionPlan(i, function_index));
			function_symbols_[fact.binding.value] = function.symbol_id;
			function_program_indexes_[fact.binding.value] = function_index;
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
					parameter_binding.name, id.value);
				const LowType parameter_type = low_type(parameter_binding.type);
				lowir_model::Parameter parameter_record;
				parameter_record.name_id = intern_spelling(
					parameter_name.valid() ? spelling(parameter_name) : "%__pa15_param");
				parameter_record.type = parameter_type;
				stored.params.push_back(parameter_record);
				add_slot(stored, id, parameter_binding.type,
					slot_name(parameter_binding.name, id.value, false),
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
			// The function scope owns control-statement scopes as well as the
			// compound body.  Starting at the body would miss condition
			// declarations and for-init bindings, whose scopes are parents of
			// their substatements.
			collect_local_slots(stored, fact.function_scope, &active_names);
			// PA12 internal control scopes intentionally have no presentation
			// tree edge. Add the already-indexed missing variables in the same
			// creation order as the former fallback, without rescanning the arena.
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
		}
	}

	SpellingId parameter_value_name(NameId name, std::size_t ordinal)
	{
		if (name.valid()) return intern_spelling("%" + model_.name_text(name));
		std::ostringstream generated;
		generated << "%__pa15_param_" << ordinal;
		return intern_spelling(generated.str());
	}

	SpellingId slot_name(NameId name, std::size_t ordinal, bool shadowed)
	{
		std::string result;
		if (name.valid()) result = "$" + model_.name_text(name);
		else
		{
			std::ostringstream generated;
			generated << "$__pa15_slot_" << ordinal;
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

	void collect_local_slots(Function& function, ScopeId scope,
		std::map<std::size_t, std::size_t>* active_names)
	{
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

	void add_slot(Function& function, BindingId binding, TypeId semantic_type,
	              SpellingId name_id, const LowType& type)
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
		slot_by_binding_[binding.value] = slot.slot_id;
	}

	ValueId allocate_value()
	{
		const ValueId id(next_value_++);
		lowir_model::ValueRecord record;
		record.id = id;
		program_.values.push_back(record);
		return id;
	}

	void clear_value_records()
	{
		for (std::size_t i = 0; i < program_.values.size(); ++i)
		{
			program_.values[i].id = ValueId(i);
			program_.values[i].parameter = 0;
			program_.values[i].instruction = 0;
			program_.values[i].owner_function_id = SymbolId();
			program_.values[i].producer = lowir_model::ValueRecord::VALUE_UNDEFINED;
		}
	}

	void claim_value(ValueId id, SymbolId owner,
		const lowir_model::Parameter* parameter,
		const lowir_model::Instruction* instruction,
		lowir_model::ValueRecord::ProducerKind producer)
	{
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

	void finalize_value_records()
	{
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

	Function& function()
	{
		return program_.functions[current_function_];
	}

	const Function& function() const
	{
		return program_.functions[current_function_];
	}

	Block& block()
	{
		return function().blocks[current_block_];
	}

	bool terminated(const Block& current) const
	{
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

	bool terminated(BlockId id) const
	{
		return terminated(function().blocks[block_index(id)]);
	}

	void reorder_condition_blocks(std::size_t begin, std::size_t destination_count,
		BlockId saved_current)
	{
		Function& target = function();
		if (begin > target.blocks.size() ||
			destination_count > target.blocks.size() - begin)
			throw std::runtime_error("PA15 invalid condition block range");
		std::vector<Block> suffix(target.blocks.begin() + begin,
			target.blocks.end());
		std::vector<Block> reordered;
		// Logical-condition blocks are allocated while recursively lowering the
		// left operand.  Reverse their creation order to present the
		// source-to-target path order used by normalized LowIR.
		for (std::size_t i = suffix.size(); i > destination_count; --i)
			reordered.push_back(suffix[i - 1]);
		for (std::size_t i = 0; i < destination_count; ++i)
			reordered.push_back(suffix[i]);
		target.blocks.erase(target.blocks.begin() + begin, target.blocks.end());
		target.blocks.insert(target.blocks.end(), reordered.begin(), reordered.end());
		rebuild_block_indexes();
		set_current(saved_current);
	}

	SpellingId temporary_name()
	{
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

	std::size_t new_block(const std::string& base)
	{
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

	void rebuild_block_indexes()
	{
		block_indexes_.clear();
		for (std::size_t i = 0; i < function().blocks.size(); ++i)
			block_indexes_[function().blocks[i].block_id.index] = i;
	}

	std::size_t reachability_index(BlockId id) const
	{
		if (!id.valid() || id.index < reachability_base_ ||
			id.index - reachability_base_ >= reachable_blocks_.size())
			throw std::runtime_error("PA15 reachability identity is not owned");
		return id.index - reachability_base_;
	}

	bool is_reachable(BlockId id) const
	{
		return reachable_blocks_[reachability_index(id)] != 0;
	}

	BlockId edge_target(const Operand& operand) const
	{
		if (operand.kind != Operand::OP_LABEL || !operand.block_id.valid())
			throw std::runtime_error("PA15 terminator edge has no block target");
		return operand.block_id;
	}

	void enqueue_reachable(BlockId id)
	{
		const std::size_t index = reachability_index(id);
		if (reachable_blocks_[index] == 0)
		{
			reachable_blocks_[index] = 1;
			reachability_work_.push_back(id);
		}
	}

	void propagate_existing_terminator_edges(BlockId source)
	{
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

	void mark_reachable(BlockId start)
	{
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

	void propagate_edge(BlockId source, BlockId target)
	{
		if (is_reachable(source)) mark_reachable(target);
	}

	void remember_loop_target(SemanticFactId id, BlockId break_target,
		BlockId continue_target)
	{
		if (!id.valid())
			throw std::runtime_error("PA15 loop fact has no identity");
		if (id.value >= loop_targets_.size())
			throw std::runtime_error("PA15 loop fact is outside target table");
		if (loop_targets_[id.value].valid)
			throw std::runtime_error("PA15 loop was lowered twice");
		loop_targets_[id.value] = LoopTarget(break_target, continue_target);
	}

	const LoopTarget* remembered_loop_target(SemanticFactId id) const
	{
		if (!id.valid() || id.value >= loop_targets_.size() ||
			!loop_targets_[id.value].valid)
			return NULL;
		return &loop_targets_[id.value];
	}

	BlockId block_id(std::size_t index) const
	{
		if (index >= function().blocks.size())
			throw std::runtime_error("PA15 block index is out of range");
		return function().blocks[index].block_id;
	}

	std::size_t block_index(BlockId id) const
	{
		const std::map<std::size_t, std::size_t>::const_iterator found =
			block_indexes_.find(id.index);
		if (!id.valid() || found == block_indexes_.end())
			throw std::runtime_error("PA15 block identity is not owned by function");
		return found->second;
	}

	void set_current(BlockId id)
	{
		if (!id.valid())
		{
			current_block_ = InvalidIdentityValue;
			return;
		}
		current_block_ = block_index(id);
		if (ordered_block_ids_.insert(id.index).second)
			block_order_.push_back(id);
	}

	BlockId current_block_id() const
	{
		return current_block_ == InvalidIdentityValue ? BlockId() :
			block_id(current_block_);
	}

	void reorder_function_blocks()
	{
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
		// A preallocated join or an otherwise unreachable structured target is
		// still part of the typed CFG. Retain such blocks in allocation order
		// after the reached CFG order instead of silently dropping them.
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

	Operand temporary_operand(ValueId id, SpellingId name) const
	{
		Operand operand;
		operand.kind = Operand::OP_TEMP;
		operand.value_id = id;
		operand.presentation_id = name;
		return operand;
	}

	Operand slot_operand(lowir_model::SlotId id) const
	{
		Operand operand;
		operand.kind = Operand::OP_SLOT;
		operand.slot_id = id;
		if (!id.valid() || id.index >= slot_spellings_.size() ||
			!slot_spellings_[id.index].valid())
			throw std::runtime_error("PA15 slot identity has no presentation record");
		operand.presentation_id = slot_spellings_[id.index];
		return operand;
	}

	Operand global_operand(SymbolId id, SpellingId name) const
	{
		Operand operand;
		operand.kind = Operand::OP_GLOBAL;
		operand.symbol_id = id;
		operand.presentation_id = name;
		return operand;
	}

	Operand block_operand(std::size_t index) const
	{
		return block_operand(block_id(index));
	}

	Operand block_operand(BlockId id) const
	{
		Operand operand;
		operand.kind = Operand::OP_LABEL;
		operand.block_id = id;
		operand.presentation_id = function().blocks[block_index(id)].label_id;
		return operand;
	}

	Operand integer_operand(long long value, const LowType& type) const
	{
		Operand operand;
		operand.kind = Operand::OP_INTEGER;
		operand.int_value = value;
		operand.literal_type = type;
		return operand;
	}

	ValueId destination(const LowType& type, Instruction* instruction)
	{
		const ValueId id = allocate_value();
		instruction->dest_id = id;
		instruction->destination_name_id = temporary_name();
		instruction->type = type;
		instruction->result_type = type;
		return id;
	}

	ValueId emit_load(const LoweredValue& storage, const LowType& type)
	{
		Instruction instruction;
		instruction.kind = Instruction::IK_LOAD;
		instruction.first = storage.value;
		const ValueId id = destination(type, &instruction);
		block().instructions.push_back(instruction);
		return id;
	}

	void emit_store(const LowType& type, const Operand& value, const Operand& storage)
	{
		Instruction instruction;
		instruction.kind = Instruction::IK_STORE;
		instruction.type = type;
		instruction.first = value;
		instruction.second = storage;
		block().instructions.push_back(instruction);
	}

	LoweredValue storage_for(BindingId binding) const
	{
		std::map<std::size_t, lowir_model::SlotId>::const_iterator found =
			slot_by_binding_.find(binding.value);
		if (found == slot_by_binding_.end())
			throw std::runtime_error("PA15 scalar binding has no slot");
		const LowType type = low_type(model_.binding(binding).type);
		return LoweredValue(slot_operand(found->second), type, true);
	}

	std::vector<SemanticFactId> children(SemanticFactId id) const
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		std::vector<SemanticFactId> result;
		if (fact.child_count == 0) return result;
		if (fact.child_begin == InvalidIdentityValue ||
			fact.child_begin + fact.child_count > model_.semantic_children_.size())
			throw std::runtime_error("PA15 invalid semantic child range");
		for (std::size_t i = 0; i < fact.child_count; ++i)
			result.push_back(model_.semantic_children_[fact.child_begin + i]);
		return result;
	}

	LoweredValue lower_lvalue(SemanticFactId id)
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::IdExpression ||
			fact.kind == SemanticFactKind::Variable)
			return storage_for(fact.binding);
		throw std::runtime_error("PA15 unsupported lvalue expression");
	}

	LoweredValue literal(const SemanticFact& fact)
	{
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
		else if (fact.source != NULL && fact.source->kind == PA10NodeKind::Literal)
		{
			const ConstValue decoded = model_.literal_constant(*fact.source);
			if (!decoded.valid) throw std::runtime_error("PA15 invalid literal value");
			value = static_cast<long long>(decoded.value);
		}
		else
			throw std::runtime_error("PA15 unsupported literal value");
		const LowType type = low_type(fact.type);
		return LoweredValue(integer_operand(value, type), type, false);
	}

	LoweredValue apply_conversions(SemanticFactId id, LoweredValue result,
		bool omit_boolean_context = false)
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.conversion_count != 0 && fact.conversion_begin == InvalidIdentityValue)
			throw std::runtime_error("PA15 invalid semantic conversion range");
		std::size_t conversion_count = fact.conversion_count;
		if (omit_boolean_context && conversion_count != 0)
		{
			FundamentalType target_fundamental;
			const ConversionFact& last = model_.conversion_facts_[
				fact.conversion_begin + conversion_count - 1];
			if (model_.fundamental_of(last.target, &target_fundamental) &&
				target_fundamental == FundamentalType::Bool)
				--conversion_count;
		}
		for (std::size_t i = 0; i < conversion_count; ++i)
		{
			const ConversionFact& conversion = model_.conversion_facts_[
				fact.conversion_begin + i];
			const LowType target = low_type(conversion.target);
			if (conversion.kind == ConversionKind::Identity)
				continue;
			if (conversion.kind == ConversionKind::LvalueToRvalue)
			{
				if (result.lvalue)
				{
					const ValueId value = emit_load(result, target);
					const Instruction& emitted = block().instructions.back();
					result.value = temporary_operand(value, emitted.destination_name_id);
				}
				result.type = target;
				result.lvalue = false;
				continue;
			}
			if (conversion.kind == ConversionKind::Floating ||
				result.type.is_float() || target.is_float())
				throw std::runtime_error("PA15 floating conversion is outside checkpoint");
			if (result.lvalue)
			{
				const ValueId value = emit_load(result, result.type);
				const Instruction& emitted = block().instructions.back();
				result.value = temporary_operand(value, emitted.destination_name_id);
				result.lvalue = false;
			}
			if (conversion.kind != ConversionKind::Integral)
				throw std::runtime_error("PA15 unsupported scalar conversion");
			if (!result.type.is_integer() || !target.is_integer())
				throw std::runtime_error("PA15 non-integral conversion in scalar spine");
			const LowType source_type = low_type(conversion.source);
			if (!source_type.is_integer())
				throw std::runtime_error("PA15 non-integral conversion source");
			if (result.value.kind == Operand::OP_INTEGER)
			{
				result.type = target;
				result.value.literal_type = target;
				continue;
			}
			if (source_type == target) continue;
			if (source_type.integer_width() == target.integer_width())
			{
				// LowIR integer signedness is a representation annotation.  A
				// same-width signed/unsigned change must not be expressed as an
				// extend or truncate.
				result.type = target;
				continue;
			}
			Instruction instruction;
			instruction.kind = Instruction::IK_CONVERT;
			// Semantic conversions describe the source value, while a compare
			// destination is physically the LowIR canonical i64 truth value.
			// Keep that typed distinction: the operand remains the compare
			// temporary, but the conversion source annotation is the semantic
			// bool/integer source used to choose its operator.
			instruction.source_type = low_type(conversion.source);
			instruction.first = result.value;
			instruction.conversion_operator = conversion_operator(conversion);
			const ValueId value = destination(target, &instruction);
			block().instructions.push_back(instruction);
			result.value = temporary_operand(value, instruction.destination_name_id);
			result.type = target;
		}
		if (result.lvalue)
		{
			const ValueId value = emit_load(result, result.type);
			const Instruction& emitted = block().instructions.back();
			result.value = temporary_operand(value, emitted.destination_name_id);
			result.lvalue = false;
		}
		return result;
	}

	lowir_model::ConversionOperator conversion_operator(const ConversionFact& conversion) const
	{
		if (conversion.kind != ConversionKind::Integral)
			throw std::runtime_error("PA15 unsupported scalar conversion");
		const LowType source = low_type(conversion.source);
		const LowType target = low_type(conversion.target);
		if (!source.is_integer() || !target.is_integer())
			throw std::runtime_error("PA15 non-integral conversion in scalar spine");
		if (target.integer_width() < source.integer_width())
			return lowir_model::COP_TRUNC;
		if (target.integer_width() > source.integer_width())
		{
			FundamentalType source_fundamental;
			if (model_.fundamental_of(conversion.source, &source_fundamental) &&
				(source_fundamental == FundamentalType::Bool ||
				 model_.unsigned_type(source_fundamental)))
				return lowir_model::COP_ZEXT;
			return lowir_model::COP_SEXT;
		}
		throw std::runtime_error("PA15 same-width conversion reached instruction emission");
	}

	LoweredValue lower_expression(SemanticFactId id)
	{
		return lower_expression_impl(id, false);
	}

	LoweredValue lower_condition_expression(SemanticFactId id)
	{
		return lower_expression_impl(id, true);
	}

	LoweredValue lower_expression_impl(SemanticFactId id,
		bool omit_boolean_context)
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		LoweredValue result;
		switch (fact.kind)
		{
		case SemanticFactKind::Variable:
		{
			const std::vector<SemanticFactId> initializer = children(id);
			if (initializer.size() > 1)
				throw std::runtime_error("PA15 invalid condition initializer");
			const LoweredValue storage = storage_for(fact.binding);
			if (initializer.size() == 1)
			{
				const LoweredValue value = lower_expression(initializer.front());
				emit_store(storage.type, value.value, storage.value);
			}
			result = storage;
			break;
		}
		case SemanticFactKind::Literal:
			result = literal(fact);
			break;
		case SemanticFactKind::IdExpression:
			result = lower_lvalue(id);
			break;
		case SemanticFactKind::UnaryExpression:
		{
			const std::vector<SemanticFactId> operands = children(id);
			if (operands.size() != 1) throw std::runtime_error("PA15 invalid unary fact");
			const LoweredValue operand = lower_expression(operands.front());
			if (fact.token == SimpleTokenType::OP_PLUS)
				result = operand;
			else if (fact.token == SimpleTokenType::OP_AMP ||
				fact.token == SimpleTokenType::OP_STAR)
				throw std::runtime_error("PA15 unsupported pointer unary expression");
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
				result = LoweredValue(temporary_operand(value, instruction.destination_name_id),
					instruction.type, false);
			}
			break;
		}
		case SemanticFactKind::BinaryExpression:
		{
			const std::vector<SemanticFactId> operands = children(id);
			if (operands.size() != 2) throw std::runtime_error("PA15 invalid binary fact");
			const LoweredValue left = lower_expression(operands[0]);
			const LoweredValue right = lower_expression(operands[1]);
			Instruction instruction;
			instruction.first = left.value;
			instruction.second = right.value;
			instruction.type = left.type;
			if (is_comparison(fact.token))
			{
				instruction.kind = Instruction::IK_CMP;
				instruction.compare_predicate = compare_predicate(fact.token,
					unsigned_type_for(left.type));
				LowType result_type;
				result_type.kind = LowType::TYPE_INTEGER;
				result_type.integer_kind = LowType::INTEGER_I64;
				instruction.result_type = result_type;
				const ValueId value = destination(result_type, &instruction);
				instruction.type = left.type;
				block().instructions.push_back(instruction);
				result = LoweredValue(temporary_operand(value, instruction.destination_name_id),
					result_type, false);
			}
			else
			{
				instruction.kind = Instruction::IK_BINARY;
				instruction.binary_operator = binary_operator(fact.token,
					unsigned_type_for(left.type));
				if (instruction.binary_operator == lowir_model::BOP_INVALID)
					throw std::runtime_error("PA15 unsupported binary operator");
				instruction.type = low_type(fact.type);
				const ValueId value = destination(instruction.type, &instruction);
				block().instructions.push_back(instruction);
				result = LoweredValue(temporary_operand(value, instruction.destination_name_id),
					instruction.type, false);
			}
			break;
		}
		case SemanticFactKind::AssignmentExpression:
		{
			const std::vector<SemanticFactId> operands = children(id);
			if (operands.size() != 2 || fact.token != SimpleTokenType::OP_ASS)
				throw std::runtime_error("PA15 unsupported assignment expression");
			const LoweredValue left = lower_lvalue(operands[0]);
			const LoweredValue right = lower_expression(operands[1]);
			emit_store(low_type(fact.type), right.value, left.value);
			result = LoweredValue(right.value, low_type(fact.type), false);
			break;
		}
		case SemanticFactKind::CallExpression:
		{
			if (!fact.has_callee || !fact.selected_binding.valid())
				throw std::runtime_error("PA15 indirect call is outside checkpoint");
			const std::map<std::size_t, SymbolId>::const_iterator found =
				function_symbols_.find(fact.selected_binding.value);
			const std::map<std::size_t, std::size_t>::const_iterator program_found =
				function_program_indexes_.find(fact.selected_binding.value);
			if (found == function_symbols_.end() ||
				program_found == function_program_indexes_.end())
				throw std::runtime_error("PA15 direct call target was not emitted");
			const Binding& callee = model_.binding(fact.selected_binding);
			if (model_.type_kind(callee.type) != TypeKind::Function)
				throw std::runtime_error("PA15 call target is not a function");
			const TypeKey& function_type = model_.types_[callee.type.value];
			const std::vector<SemanticFactId> arguments = children(id);
			if (arguments.size() != function_type.parameters.size())
				throw std::runtime_error("PA15 direct call arity mismatch");
			Instruction instruction;
			instruction.kind = Instruction::IK_CALL;
			instruction.direct_callee_id = found->second;
			instruction.first = global_operand(found->second,
				program_.functions[program_found->second].name_id);
			instruction.call_return_type = low_type(function_type.result);
			instruction.call_returns_void = instruction.call_return_type.is_void();
			for (std::size_t i = 0; i < arguments.size(); ++i)
				instruction.args.push_back(lower_expression(arguments[i]).value);
			if (instruction.call_returns_void)
				block().instructions.push_back(instruction);
			else
			{
				const ValueId value = destination(instruction.call_return_type, &instruction);
				block().instructions.push_back(instruction);
				result = LoweredValue(temporary_operand(value, instruction.destination_name_id),
					instruction.call_return_type, false);
			}
			if (instruction.call_returns_void)
				result = LoweredValue(Operand(), instruction.call_return_type, false);
			break;
		}
		default:
			throw std::runtime_error("PA15 unsupported scalar expression fact");
		}
		return apply_conversions(id, result, omit_boolean_context);
	}

	bool is_comparison(SimpleTokenType token) const
	{
		return token == SimpleTokenType::OP_EQ || token == SimpleTokenType::OP_NE ||
			token == SimpleTokenType::OP_LT || token == SimpleTokenType::OP_LE ||
			token == SimpleTokenType::OP_GT || token == SimpleTokenType::OP_GE;
	}

	lowir_model::ComparePredicate compare_predicate(SimpleTokenType token,
		bool is_unsigned) const
	{
		if (token == SimpleTokenType::OP_EQ) return lowir_model::CPP_EQ;
		if (token == SimpleTokenType::OP_NE) return lowir_model::CPP_NE;
		if (token == SimpleTokenType::OP_LT) return is_unsigned ? lowir_model::CPP_ULT : lowir_model::CPP_LT;
		if (token == SimpleTokenType::OP_LE) return is_unsigned ? lowir_model::CPP_ULE : lowir_model::CPP_LE;
		if (token == SimpleTokenType::OP_GT) return is_unsigned ? lowir_model::CPP_UGT : lowir_model::CPP_GT;
		if (token == SimpleTokenType::OP_GE) return is_unsigned ? lowir_model::CPP_UGE : lowir_model::CPP_GE;
		return lowir_model::CPP_INVALID;
	}

	bool unsigned_type_for(const LowType& type) const
	{
		return type.kind == LowType::TYPE_INTEGER &&
			(type.integer_kind == LowType::INTEGER_U8 ||
			 type.integer_kind == LowType::INTEGER_U16 ||
			 type.integer_kind == LowType::INTEGER_U32 ||
			 type.integer_kind == LowType::INTEGER_U64);
	}

	lowir_model::BinaryOperator binary_operator(SimpleTokenType token,
		bool is_unsigned) const
	{
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

	void emit_jump(BlockId target)
	{
		const BlockId source = current_block_id();
		Instruction jump;
		jump.kind = Instruction::IK_JUMP;
		jump.first = block_operand(target);
		block().instructions.push_back(jump);
		propagate_edge(source, target);
	}

	void emit_branch(const Operand& condition, BlockId true_target,
		BlockId false_target)
	{
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

	bool condition_is_empty(SemanticFactId id) const
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		return fact.kind == SemanticFactKind::Condition && fact.child_count == 0;
	}

	bool has_direct_short_circuit(SemanticFactId id) const
	{
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

	void lower_condition_branch(SemanticFactId id, BlockId true_target,
		BlockId false_target)
	{
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

	BlockId control_target(bool continue_target) const
	{
		for (std::size_t i = control_stack_.size(); i != 0; --i)
		{
			const ControlTarget& target = control_stack_[i - 1];
			if (continue_target && !target.loop) continue;
			const BlockId result = continue_target ? target.continue_target :
				target.break_target;
			if (result.valid()) return result;
		}
		throw std::runtime_error(continue_target ?
			"PA15 continue target is missing" : "PA15 break target is missing");
	}

	BlockId switch_label_target(SemanticFactId id)
	{
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

	bool switch_label_was_lowered(SemanticFactId id) const
	{
		if (switch_stack_.empty())
			throw std::runtime_error("PA15 switch label context is missing");
		const SwitchContext& context = switch_stack_.back();
		return context.lowered_labels.find(id.value) !=
			context.lowered_labels.end();
	}

	BlockId switch_label_existing_target(SemanticFactId id) const
	{
		if (switch_stack_.empty())
			throw std::runtime_error("PA15 switch label context is missing");
		const SwitchContext& context = switch_stack_.back();
		const std::map<std::size_t, BlockId>::const_iterator found =
			context.labels.find(id.value);
		if (found == context.labels.end())
			throw std::runtime_error("PA15 switch label owner mismatch");
		return found->second;
	}

	void terminate_unreachable_block(BlockId id)
	{
		if (terminated(id)) return;
		if (is_reachable(id))
			throw std::runtime_error("PA15 reachable block cannot be a sink");
		const BlockId saved_current = current_block_id();
		set_current(id);
		// A proven-unreachable retained tail may already contain lowered source
		// instructions.  Terminate both empty and populated tails structurally;
		// reachable non-void fallthrough is rejected by the caller.
		emit_jump(id);
		if (saved_current.valid()) set_current(saved_current);
		else current_block_ = InvalidIdentityValue;
	}

	bool lower_switch_label_recovery(SemanticFactId id)
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const bool already_lowered = switch_label_was_lowered(id);
		const BlockId target = already_lowered ?
			switch_label_existing_target(id) : switch_label_target(id);
		if (already_lowered)
		{
			// A recovery path may fall through to a label whose body was already
			// emitted by the ordinary entry path.  Join it to that typed block,
			// then stop this recovery walk so the existing body is not duplicated.
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

	bool collect_switch_labels(SemanticFactId id, SwitchContext* context)
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		// A nested switch owns all labels below it.  Skipping it here makes
		// each owned-switch traversal linear in its own fact subtree rather
		// than repeatedly collecting nested labels for every enclosing switch.
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

	bool switch_subtree_has_label(SemanticFactId id) const
	{
		if (switch_stack_.empty())
			throw std::runtime_error("PA15 switch label context is missing");
		return switch_stack_.back().label_subtrees.find(id.value) !=
			switch_stack_.back().label_subtrees.end();
	}

	void finish_switch_loop(const LoopTarget& target)
	{
		control_stack_.pop_back();
		// Keep the typed lexical continuation available so source statements
		// after an unreachable loop end are still serialized.  The final
		// function check sinks this block only if it remains both unreachable
		// and empty.
		set_current(target.break_target);
	}

	void recover_existing_switch_loop(SemanticFactId body_fact,
		const LoopTarget& target)
	{
		control_stack_.push_back(ControlTarget(true, target.break_target,
			target.continue_target));
		current_block_ = InvalidIdentityValue;
		lower_switch_body(body_fact);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(target.continue_target);
		finish_switch_loop(target);
	}

	void lower_switch_while(SemanticFactId id)
	{
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid recovered while fact");
		const LoopTarget* existing = remembered_loop_target(id);
		if (existing != NULL)
		{
			recover_existing_switch_loop(facts[1], *existing);
			return;
		}
		const BlockId condition = block_id(new_block("while_cond"));
		const BlockId body = block_id(new_block("while_body"));
		const BlockId end = block_id(new_block("while_end"));
		remember_loop_target(id, end, condition);
		set_current(condition);
		if (has_direct_short_circuit(facts[0]))
			lower_condition_branch(facts[0], body, end);
		else
		{
			const LoweredValue value = lower_condition(facts[0]);
			emit_branch(value.value, body, end);
		}
		control_stack_.push_back(ControlTarget(true, end, condition));
		set_current(body);
		lower_statement(facts[1]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		current_block_ = InvalidIdentityValue;
		// The first pass lowers the ordinary loop once.  This recovery pass only
		// visits labels not reached by that pass; nested loops reuse their saved
		// typed targets instead of duplicating their ordinary lowering.
		lower_switch_body(facts[1]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		finish_switch_loop(loop_targets_[id.value]);
	}

	void lower_switch_do(SemanticFactId id)
	{
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid recovered do fact");
		const LoopTarget* existing = remembered_loop_target(id);
		if (existing != NULL)
		{
			recover_existing_switch_loop(facts[0], *existing);
			return;
		}
		const BlockId body = block_id(new_block("do_body"));
		const BlockId condition = block_id(new_block("do_cond"));
		const BlockId end = block_id(new_block("do_end"));
		remember_loop_target(id, end, condition);
		control_stack_.push_back(ControlTarget(true, end, condition));
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
		finish_switch_loop(loop_targets_[id.value]);
	}

	void lower_switch_for(SemanticFactId id)
	{
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
		const LoopTarget* existing = remembered_loop_target(id);
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
		// This synthetic entry is intentionally not connected to the switch
		// dispatch.  A dispatch directly to a nested case bypasses for-init;
		// later loop iterations enter through condition/body as usual.
		remember_loop_target(id, end, iteration);
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
		control_stack_.push_back(ControlTarget(true, end, iteration));
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
			(void)lower_expression(expression.front());
		}
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		finish_switch_loop(loop_targets_[id.value]);
	}

	bool lower_switch_body(SemanticFactId id)
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		// Nested switches own their labels.  They are lowered only when reached
		// as statements, never as part of the enclosing label search.
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
			bool entered_label = false;
			for (std::size_t i = 0; i < branches.size(); ++i)
			{
				const SemanticFactKind kind =
					model_.semantic_facts_[branches[i].value].kind;
				if (kind != SemanticFactKind::ThenBranch &&
					kind != SemanticFactKind::ElseBranch)
					continue;
				if (!switch_subtree_has_label(branches[i])) continue;
				// Each branch is a mutually exclusive lexical container.  Once
				// one branch has supplied a recovered case path, search another
				// label-bearing sibling from an invalid current block instead of
				// treating its ordinary statements as fallthrough.
				if (entered_label) current_block_ = InvalidIdentityValue;
				if (!lower_switch_body(branches[i])) continue;
				entered_label = true;
				if (current_block_ != InvalidIdentityValue)
					fallthroughs.push_back(current_block_id());
			}
			if (fallthroughs.empty())
			{
				current_block_ = InvalidIdentityValue;
			}
			else if (fallthroughs.size() == 1)
			{
				set_current(fallthroughs.front());
			}
			else
			{
				const BlockId join = block_id(new_block("switch_if_end"));
				for (std::size_t i = 0; i < fallthroughs.size(); ++i)
				{
					set_current(fallthroughs[i]);
					if (!terminated(block())) emit_jump(join);
				}
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
			!switch_subtree_has_label(id)) return false;
		if (fact.kind == SemanticFactKind::CompoundStatement)
		{
			const std::vector<SemanticFactId> facts = children(id);
			bool entered_label = false;
			for (std::size_t i = 0; i < facts.size(); ++i)
				if (lower_switch_body(facts[i])) entered_label = true;
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

		// The lexical predecessor may have terminated, but a case/default below
		// an unreachable statement is still a dispatch entry.  Search the typed
		// statement graph once, skipping no subtree, until every owned label is
		// entered.  Expression facts are harmlessly traversed and nested switches
		// are cut off by the guard above.
		const std::vector<SemanticFactId> facts = children(id);
		bool entered_label = false;
		for (std::size_t i = 0; i < facts.size(); ++i)
			if (lower_switch_body(facts[i])) entered_label = true;
		return entered_label;
	}

	void finish_switch_labels()
	{
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

	void lower_switch(const std::vector<SemanticFactId>& facts)
	{
		if (facts.size() < 1 || facts.size() > 2)
			throw std::runtime_error("PA15 invalid switch fact");
		const LoweredValue selector = lower_condition(facts.front());
		const BlockId dispatch = block_id(new_block("switch_dispatch"));
		const BlockId end = block_id(new_block("switch_end"));
		SwitchContext context(end);
		if (facts.size() == 2)
			collect_switch_labels(facts[1], &context);
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
		control_stack_.push_back(ControlTarget(false, end));
		if (facts.size() == 2)
		{
			// Dispatch enters a case/default target directly.  There is no
			// lexical fallthrough into statements before the first label, so the
			// recovery walk must begin with no current block and skip that prefix.
			current_block_ = InvalidIdentityValue;
			lower_switch_body(facts[1]);
			finish_switch_labels();
		}
		if (current_block_ != InvalidIdentityValue &&
			current_block_id() != end && !terminated(block()))
			emit_jump(end);
		control_stack_.pop_back();
		switch_stack_.pop_back();
		// Preserve the typed continuation for source statements after an
		// unreachable switch end; an empty one is sunk only at function exit.
		set_current(end);
	}

	void lower_while(SemanticFactId id)
	{
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid while fact");
		const BlockId condition = block_id(new_block("while_cond"));
		const BlockId body = block_id(new_block("while_body"));
		const BlockId end = block_id(new_block("while_end"));
		remember_loop_target(id, end, condition);
		emit_jump(condition);
		set_current(condition);
		if (has_direct_short_circuit(facts[0]))
			lower_condition_branch(facts[0], body, end);
		else
		{
			const LoweredValue value = lower_condition(facts[0]);
			emit_branch(value.value, body, end);
		}
		control_stack_.push_back(ControlTarget(true, end, condition));
		set_current(body);
		lower_statement(facts[1]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		control_stack_.pop_back();
		set_current(end);
	}

	void lower_do(SemanticFactId id)
	{
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid do fact");
		const BlockId body = block_id(new_block("do_body"));
		const BlockId condition = block_id(new_block("do_cond"));
		const BlockId end = block_id(new_block("do_end"));
		remember_loop_target(id, end, condition);
		emit_jump(body);
		control_stack_.push_back(ControlTarget(true, end, condition));
		set_current(body);
		lower_statement(facts[0]);
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
		control_stack_.pop_back();
		set_current(end);
	}

	void lower_for(SemanticFactId id)
	{
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() < 2)
			throw std::runtime_error("PA15 invalid for fact");
		lower_statement(facts[0]);
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
		remember_loop_target(id, end, iteration);
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
		control_stack_.push_back(ControlTarget(true, end, iteration));
		set_current(body);
		lower_statement(facts.back());
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(iteration);
		set_current(iteration);
		if (iteration_fact.valid())
		{
			const std::vector<SemanticFactId> expression = children(iteration_fact);
			if (expression.size() != 1)
				throw std::runtime_error("PA15 invalid for iteration fact");
			(void)lower_expression(expression.front());
		}
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		control_stack_.pop_back();
		set_current(end);
	}

	void lower_statement(SemanticFactId id)
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (current_block_ == InvalidIdentityValue &&
			fact.kind != SemanticFactKind::CaseStatement &&
			fact.kind != SemanticFactKind::DefaultStatement)
			return;
		const std::vector<SemanticFactId> facts = children(id);
		switch (fact.kind)
		{
		case SemanticFactKind::CompoundStatement:
			for (std::size_t i = 0; i < facts.size(); ++i)
			{
				if (current_block_ == InvalidIdentityValue) break;
				lower_statement(facts[i]);
			}
			break;
		case SemanticFactKind::SimpleDeclaration:
			for (std::size_t i = 0; i < facts.size(); ++i)
				lower_statement(facts[i]);
			break;
		case SemanticFactKind::Variable:
			if (facts.size() > 1) throw std::runtime_error("PA15 invalid local initializer");
			if (facts.size() == 1)
			{
				const LoweredValue value = lower_expression(facts.front());
				const LoweredValue storage = storage_for(fact.binding);
				emit_store(storage.type, value.value, storage.value);
			}
			break;
		case SemanticFactKind::ExpressionStatement:
			if (facts.size() == 1) (void)lower_expression(facts.front());
			break;
		case SemanticFactKind::ReturnStatement:
		{
			Instruction instruction;
			instruction.kind = Instruction::IK_RETURN;
			instruction.type = function().return_type;
			if (facts.size() == 1) instruction.first = lower_expression(facts.front()).value;
			else if (!instruction.type.is_void())
				throw std::runtime_error("PA15 missing return operand");
			block().instructions.push_back(instruction);
			current_block_ = InvalidIdentityValue;
			break;
		}
		case SemanticFactKind::IfStatement:
			lower_if(facts);
			break;
		case SemanticFactKind::SwitchStatement:
			lower_switch(facts);
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
					(void)lower_expression(facts[i]);
			}
			break;
		case SemanticFactKind::Iteration:
			if (facts.size() != 1)
				throw std::runtime_error("PA15 invalid iteration fact");
			(void)lower_expression(facts.front());
			break;
		case SemanticFactKind::BreakStatement:
			emit_jump(control_target(false));
			current_block_ = InvalidIdentityValue;
			break;
		case SemanticFactKind::ContinueStatement:
			emit_jump(control_target(true));
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
		case SemanticFactKind::ThenBranch:
		case SemanticFactKind::ElseBranch:
			if (facts.size() == 1) lower_statement(facts.front());
			break;
		default:
			throw std::runtime_error("PA15 unsupported scalar statement fact");
		}
	}

	LoweredValue lower_condition(SemanticFactId id)
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const std::vector<SemanticFactId> facts = children(id);
		if (fact.kind == SemanticFactKind::Condition && facts.size() == 1)
			return lower_condition(facts.front());
		if (fact.kind == SemanticFactKind::ConditionDeclaration && facts.size() == 1)
			return lower_condition_expression(facts.front());
		return lower_condition_expression(id);
	}

	void lower_if(const std::vector<SemanticFactId>& facts)
	{
		if (facts.size() < 2 || facts.size() > 3)
			throw std::runtime_error("PA15 invalid if fact");
		const bool direct = has_direct_short_circuit(facts[0]);
		if (direct)
		{
			// Allocate branch destinations first so their typed names retain the
			// same ordinal relationship as ordinary if lowering.  The RHS blocks
			// are then placed before those destinations in presentation order.
			const std::size_t begin = function().blocks.size();
			const BlockId then_block = block_id(new_block("if_then"));
			const BlockId else_block = block_id(new_block("if_else"));
			const bool implicit_else = facts.size() == 2;
			BlockId join_block;
			if (implicit_else)
				join_block = block_id(new_block("if_end"));
			lower_condition_branch(facts[0], then_block, else_block);
			const BlockId saved_current = current_block_id();
			reorder_condition_blocks(begin, implicit_else ? 3 : 2,
				saved_current);

			set_current(then_block);
			lower_statement(facts[1]);
			const bool then_terminated = terminated(then_block);
			set_current(else_block);
			if (!implicit_else) lower_statement(facts[2]);
			const bool else_terminated = terminated(else_block);
			if (then_terminated && else_terminated)
			{
				current_block_ = InvalidIdentityValue;
				return;
			}
			if (!join_block.valid())
				join_block = block_id(new_block("if_end"));
			if (!then_terminated)
			{
				set_current(then_block);
				emit_jump(join_block);
			}
			if (!else_terminated)
			{
				set_current(else_block);
				emit_jump(join_block);
			}
			set_current(join_block);
			return;
		}

		const LoweredValue condition = lower_condition(facts[0]);
		const BlockId then_block = block_id(new_block("if_then"));
		const BlockId else_block = block_id(new_block("if_else"));
		emit_branch(condition.value, then_block, else_block);

		set_current(then_block);
		lower_statement(facts[1]);
		const bool then_terminated = terminated(then_block);
		set_current(else_block);
		if (facts.size() == 3) lower_statement(facts[2]);
		const bool else_terminated = terminated(else_block);
		if (then_terminated && else_terminated)
		{
			current_block_ = InvalidIdentityValue;
			return;
		}
		const BlockId join_block = block_id(new_block("if_end"));
		if (!then_terminated)
		{
			set_current(then_block);
			emit_jump(join_block);
		}
		if (!else_terminated)
		{
			set_current(else_block);
			emit_jump(join_block);
		}
		set_current(join_block);
	}

	void lower_function(const FunctionPlan& plan)
	{
		current_function_ = plan.program_index;
		current_block_ = InvalidIdentityValue;
		temp_ordinal_ = 0;
		block_ordinal_ = 0;
		block_indexes_.clear();
		control_stack_.clear();
		switch_stack_.clear();
		block_order_.clear();
		ordered_block_ids_.clear();
		reachability_base_ = next_block_;
		reachable_blocks_.clear();
		reachability_work_.clear();
		Function& target = function();
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
		if (!fact.body_fact.valid())
			throw std::runtime_error("PA15 function body fact is missing");
		lower_statement(fact.body_fact);
		if (current_block_ != InvalidIdentityValue &&
			!terminated(block()))
		{
			const BlockId continuation = current_block_id();
			if (!is_reachable(continuation))
			{
				// Only a proven-unreachable continuation may become a structural
				// sink.  Its retained source tail may be empty or populated.
				terminate_unreachable_block(continuation);
				current_block_ = InvalidIdentityValue;
			}
			else
			{
				if (!target.return_type.is_void())
					throw std::runtime_error("PA15 function falls through without return");
				Instruction instruction;
				instruction.kind = Instruction::IK_RETURN;
				instruction.type = target.return_type;
				block().instructions.push_back(instruction);
			}
		}
		target.value_count = next_value_ - value_begin;
		reorder_function_blocks();
	}
};

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
