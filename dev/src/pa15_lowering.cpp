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

class Pa15Lowerer
{
public:
	Pa15Lowerer(const PA11SemanticModel& model, Program& program)
		: model_(model), program_(program), spelling_ids_(), used_symbols_(),
			  used_slot_names_(), used_value_names_(), symbol_collision_counters_(),
		  slot_collision_counters_(), function_symbols_(),
		  function_program_indexes_(), slot_by_binding_(), slot_spellings_(),
		  function_plans_(), next_symbol_(0), next_value_(program.values.size()),
		  next_slot_(0), next_block_(0), current_function_(0),
		  current_block_(InvalidIdentityValue), temp_ordinal_(0),
		  block_ordinal_(0) {}

	void run()
	{
		initialize_spelling_ids();
		initialize_identity_counters();
		clear_value_records();
		collect_functions();
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
	std::size_t next_symbol_;
	std::size_t next_value_;
	std::size_t next_slot_;
	std::size_t next_block_;
	std::size_t current_function_;
	std::size_t current_block_;
	std::size_t temp_ordinal_;
	std::size_t block_ordinal_;

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

	void collect_functions()
	{
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
			collect_local_slots(stored, fact.body_scope, &active_names);
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
		return function().blocks.size() - 1;
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
		Operand operand;
		operand.kind = Operand::OP_LABEL;
		operand.block_id = function().blocks[index].block_id;
		operand.presentation_id = function().blocks[index].label_id;
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

	LoweredValue apply_conversions(SemanticFactId id, LoweredValue result)
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.conversion_count != 0 && fact.conversion_begin == InvalidIdentityValue)
			throw std::runtime_error("PA15 invalid semantic conversion range");
		for (std::size_t i = 0; i < fact.conversion_count; ++i)
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
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		LoweredValue result;
		switch (fact.kind)
		{
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
		return apply_conversions(id, result);
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

	void lower_statement(SemanticFactId id)
	{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
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
			return lower_expression(facts.front());
		return lower_expression(id);
	}

	void lower_if(const std::vector<SemanticFactId>& facts)
	{
		if (facts.size() < 2 || facts.size() > 3)
			throw std::runtime_error("PA15 invalid if fact");
		const LoweredValue condition = lower_condition(facts[0]);
		const std::size_t then_block = new_block("if_then");
		const std::size_t else_block = new_block("if_else");
		Instruction branch;
		branch.kind = Instruction::IK_BRANCH;
		branch.first = condition.value;
		branch.second = block_operand(then_block);
		branch.third = block_operand(else_block);
		block().instructions.push_back(branch);

		current_block_ = then_block;
		lower_statement(facts[1]);
		const bool then_terminated = current_block_ == InvalidIdentityValue ||
			terminated(function().blocks[then_block]);
		current_block_ = else_block;
		if (facts.size() == 3) lower_statement(facts[2]);
		const bool else_terminated = current_block_ == InvalidIdentityValue ||
			terminated(function().blocks[else_block]);
		if (then_terminated && else_terminated)
		{
			current_block_ = InvalidIdentityValue;
			return;
		}
		const std::size_t join_block = new_block("if_join");
		if (!then_terminated)
		{
			current_block_ = then_block;
			Instruction jump;
			jump.kind = Instruction::IK_JUMP;
			jump.first = block_operand(join_block);
			block().instructions.push_back(jump);
		}
		if (!else_terminated)
		{
			current_block_ = else_block;
			Instruction jump;
			jump.kind = Instruction::IK_JUMP;
			jump.first = block_operand(join_block);
			block().instructions.push_back(jump);
		}
		current_block_ = join_block;
	}

	void lower_function(const FunctionPlan& plan)
	{
		current_function_ = plan.program_index;
		current_block_ = InvalidIdentityValue;
		temp_ordinal_ = 0;
		block_ordinal_ = 0;
		Function& target = function();
		used_value_names_.clear();
		for (std::size_t i = 0; i < target.params.size(); ++i)
			used_value_names_.insert(spelling(target.params[i].name_id));
		target.value_begin = ValueId(next_value_);
		for (std::size_t i = 0; i < target.params.size(); ++i)
			target.params[i].value_id = allocate_value();
		const std::size_t value_begin = target.value_begin.index;
		current_block_ = new_block("entry");
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
			if (!target.return_type.is_void())
				throw std::runtime_error("PA15 function falls through without return");
			Instruction instruction;
			instruction.kind = Instruction::IK_RETURN;
			instruction.type = target.return_type;
			block().instructions.push_back(instruction);
		}
		target.value_count = next_value_ - value_begin;
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
