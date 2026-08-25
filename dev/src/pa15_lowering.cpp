#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

Pa15Lowerer::Pa15Lowerer(const PA11SemanticModel& model, Program& program)
		: model_(model), program_(program), spelling_ids_(), used_symbols_(),
		  used_slot_names_(), used_value_names_(), symbol_collision_counters_(),
		  slot_collision_counters_(), function_symbols_(),
		  function_name_ids_(), global_symbols_(), global_name_ids_(),
		  symbol_name_ids_(), literal_address_symbols_(), variable_facts_(),
		  declaration_by_binding_(), slot_by_binding_(), slot_spellings_(),
		  function_plans_(), pending_global_initializers_(),
		  function_scope_variables_(), next_symbol_(0),
		  literal_backing_ordinal_(0), next_value_(program.values.size()),
		  next_slot_(0), next_block_(0), current_function_(0),
		  current_block_(InvalidIdentityValue), temp_ordinal_(0),
		  block_ordinal_(0), generated_slot_ordinal_(0), block_indexes_(), control_stack_(),
		  switch_stack_(), block_order_(), ordered_block_ids_(),
		  loop_targets_(), reachability_base_(0), reachable_blocks_(),
		  reachability_work_(){}

void Pa15Lowerer::run(){
		initialize_spelling_ids();
		initialize_identity_counters();
		clear_value_records();
		index_binding_facts();
		collect_functions();
		collect_function_declarations();
		collect_globals();
		materialize_pending_global_initializers();


		loop_targets_.resize(model_.semantic_facts_.size());
		for (std::size_t i = 0; i < function_plans_.size(); ++i)
			lower_function(function_plans_[i]);
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

std::vector<std::string> Pa15Lowerer::function_components(const FunctionFact& fact) const{
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

std::vector<std::string> Pa15Lowerer::value_components(ScopeId owner, NameId name) const{
		std::vector<std::string> reversed;
		ScopeId scope = owner;
		while (scope.valid())
		{
			const Scope& current = model_.scopes_[scope.value];
			if (current.kind == ScopeKind::Namespace && current.name.valid())
				reversed.push_back(model_.name_text(current.name));
			scope = current.parent;
		}
		std::reverse(reversed.begin(), reversed.end());
		reversed.push_back(model_.name_text(name));
		return reversed;
	}

std::string Pa15Lowerer::abi_variable_symbol(BindingId binding_id, ScopeId owner) const{
		const Binding& binding = model_.binding(binding_id);
		abi_mangle::AbiFactCase facts;
		abi_mangle::AbiFactRecord record;
		record.kind = abi_mangle::ABI_FACT_RECORD_TARGET;
		record.target.kind = abi_mangle::ABI_TARGET_FACT_VARIABLE;
		record.target.linkage = (binding.internal_linkage ||
			binding.language_linkage == LanguageLinkage::Cxx) ?
			abi_mangle::ABI_LINKAGE_CXX : abi_mangle::ABI_LINKAGE_C;
		record.target.name.components = value_components(owner, binding.name);
		facts.records.push_back(record);
		return abi_mangle::mangle_abi_fact_case(facts);
	}

std::vector<std::string> Pa15Lowerer::named_type_components(NamedRecordId record) const{
		if (!record.valid() || record.value >= model_.named_.size() ||
			!model_.named_[record.value].name.valid())
			throw std::runtime_error("PA15 ABI named type has no name");
		std::vector<std::string> reversed;
		ScopeId scope = model_.named_[record.value].owner;
		while (scope.valid())
		{
			const Scope& current = model_.scopes_[scope.value];
			if (current.kind == ScopeKind::Namespace && current.name.valid())
				reversed.push_back(model_.name_text(current.name));
			scope = current.parent;
		}
		std::reverse(reversed.begin(), reversed.end());
		reversed.push_back(model_.name_text(model_.named_[record.value].name));
		return reversed;
	}

abi_mangle::AbiType Pa15Lowerer::abi_type_nested(TypeId type) const{
		if (!type.valid())
			throw std::runtime_error("PA15 invalid ABI type");
		const TypeKind kind = model_.type_kind(type);
		if (kind == TypeKind::Cv)
		{
			abi_mangle::AbiType result;
			result.kind = abi_mangle::ABI_TYPE_CV;
			result.is_const = (model_.types_[type.value].cv & 1u) != 0;
			result.is_volatile = (model_.types_[type.value].cv & 2u) != 0;
			result.types.push_back(abi_type_nested(model_.types_[type.value].child));
			return result;
		}
		if (kind == TypeKind::Pointer || kind == TypeKind::LvalueReference ||
			kind == TypeKind::RvalueReference)
		{
			abi_mangle::AbiType result;
			result.kind = kind == TypeKind::Pointer ? abi_mangle::ABI_TYPE_POINTER :
				kind == TypeKind::LvalueReference ? abi_mangle::ABI_TYPE_LVALUE_REFERENCE :
				abi_mangle::ABI_TYPE_RVALUE_REFERENCE;
			result.types.push_back(abi_type_nested(model_.types_[type.value].child));
			return result;
		}
		if (kind == TypeKind::Array)
		{
			abi_mangle::AbiType result;
			result.kind = abi_mangle::ABI_TYPE_ARRAY;
			result.array_bound.kind = abi_mangle::ABI_ARRAY_BOUND_VALUE;
			result.array_bound.value = model_.types_[type.value].unknown_bound ? 0 :
				model_.types_[type.value].bound.value;
			result.types.push_back(abi_type_nested(model_.types_[type.value].child));
			return result;
		}
		if (kind == TypeKind::Function)
		{
			abi_mangle::AbiType result;
			result.kind = abi_mangle::ABI_TYPE_FUNCTION;
			result.types.push_back(abi_type_nested(model_.types_[type.value].result));
			for (std::size_t i = 0; i < model_.types_[type.value].parameters.size(); ++i)
				result.types.push_back(abi_type_nested(
					model_.types_[type.value].parameters[i]));
			result.variadic = model_.types_[type.value].variadic;
			return result;
		}
		if (kind == TypeKind::Named)
		{
			abi_mangle::AbiType result;
			result.kind = abi_mangle::ABI_TYPE_NAMED;
			result.name.components = named_type_components(model_.types_[type.value].named);
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

abi_mangle::AbiType Pa15Lowerer::abi_type(TypeId type) const{
		while (type.valid() && model_.type_kind(type) == TypeKind::Cv)
			type = model_.types_[type.value].child;
		return abi_type_nested(type);
	}

std::string Pa15Lowerer::abi_symbol(const FunctionFact& fact) const{
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

std::string Pa15Lowerer::abi_function_symbol(BindingId binding_id, ScopeId owner) const{
		const Binding& binding = model_.binding(binding_id);
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
		record.target.function.name.components = value_components(owner, binding.name);
		for (std::size_t i = 0; i < function.parameters.size(); ++i)
			record.target.function.signature_parameter_types.push_back(
				abi_type(function.parameters[i]));
		facts.records.push_back(record);
		return abi_mangle::mangle_abi_fact_case(facts);
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
			result.object_alignment = element.storage_alignment();
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
		for (std::size_t i = 0; i < model_.declaration_facts_.size(); ++i)
		{
			const DeclarationFact& declaration = model_.declaration_facts_[i];
			if (declaration.binding_begin == InvalidIdentityValue)
				continue;
			for (std::size_t j = 0; j < declaration.binding_count; ++j)
			{
				const BindingId binding = model_.declaration_bindings_[
					declaration.binding_begin + j];
				declaration_by_binding_[binding.value] = &declaration;
				if (declaration.semantic_begin != InvalidIdentityValue &&
					j < declaration.semantic_count)
					variable_facts_[binding.value] = SemanticFactId(
						model_.declaration_semantic_ids_[declaration.semantic_begin + j].value);
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

bool Pa15Lowerer::map_constant_address(SemanticFactId id, SymbolId* target,
		long long* addend, const ConstantAddressFact** relocation){
		if (relocation != NULL) *relocation = NULL;
		if (!id.valid() || id.value >= model_.semantic_facts_.size() ||
			target == NULL || addend == NULL)
			return false;
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (!fact.constant_address.valid() ||
			fact.constant_address.value >= model_.constant_address_facts_.size())
			return false;
		const ConstantAddressFact& value =
			model_.constant_address_facts_[fact.constant_address.value];
		if (!value.evaluated || !value.valid ||
			value.kind == ConstantAddressKind::None)
			return false;
		if (value.kind == ConstantAddressKind::Literal)
		{
			std::map<std::size_t, SymbolId>::const_iterator found =
				literal_address_symbols_.find(fact.constant_address.value);
			if (found != literal_address_symbols_.end())
			{
				*target = found->second;
				*addend = value.byte_addend;
				if (relocation != NULL) *relocation = &value;
				return true;
			}
			if (!value.element_type.valid() || value.literal_element_count == 0 ||
				value.literal_byte_begin == InvalidIdentityValue ||
				value.literal_byte_begin >
				model_.constant_address_literal_bytes_.size() ||
				value.literal_byte_count >
				model_.constant_address_literal_bytes_.size() -
				value.literal_byte_begin)
				return false;
			const LowType element_type = low_type(value.element_type);
			const std::size_t element_size = element_type.storage_size();
			if (element_size == 0 || value.literal_element_count >
				std::numeric_limits<std::size_t>::max() / element_size ||
			value.literal_byte_count != value.literal_element_count *
				element_size)
				return false;
			GlobalDefinition literal_global;
			literal_global.symbol_id = SymbolId(next_symbol_++);
			std::ostringstream name;
			name << "__strlit__" << ++literal_backing_ordinal_;
			literal_global.name_id = symbol_spelling(name.str());
			literal_global.structured = true;
			literal_global.metadata.binding = lowir_model::SBM_INTERNAL;
			for (std::size_t i = 0; i < value.literal_element_count; ++i)
			{
				std::uint64_t bits = 0;
				for (std::size_t byte = 0; byte < element_size; ++byte)
					bits |= static_cast<std::uint64_t>(
						model_.constant_address_literal_bytes_[
							value.literal_byte_begin + i * element_size + byte]) <<
						(byte * 8);
				GlobalDefinition::DataItem item;
				item.kind = GlobalDefinition::DataItem::ITEM_INTEGER;
				item.type = element_type;
				item.literal_operand = integer_operand(
					static_cast<long long>(bits), element_type);
				literal_global.data_items.push_back(item);
			}
			literal_address_symbols_[fact.constant_address.value] =
				literal_global.symbol_id;
			symbol_name_ids_[literal_global.symbol_id.index] =
				literal_global.name_id;
			program_.globals.push_back(literal_global);
			*target = literal_global.symbol_id;
			*addend = value.byte_addend;
			if (relocation != NULL) *relocation = &value;
			return true;
		}
		if (!value.target.valid())
			return false;
		std::map<std::size_t, SymbolId>::const_iterator global =
			global_symbols_.find(value.target.value);
		if (global != global_symbols_.end())
			*target = global->second;
		else
		{
			std::map<std::size_t, SymbolId>::const_iterator function =
				function_symbols_.find(value.target.value);
			if (function == function_symbols_.end()) return false;
			*target = function->second;
		}
		*addend = value.byte_addend;
		if (relocation != NULL) *relocation = &value;
		return true;
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

void Pa15Lowerer::append_array_data(GlobalDefinition* global, TypeId array_type,
		const std::vector<SemanticFactId>& initializers, ScopeId scope){
		TypeId element = model_.types_[array_type.value].child;
		const LowType element_type = low_type(element);
		const std::size_t bound = model_.types_[array_type.value].bound.value;
		std::size_t pending_zero_bytes = 0;
		const auto flush_zeroes = [&]() {
			if (pending_zero_bytes == 0) return;
			GlobalDefinition::DataItem zero;
			zero.kind = GlobalDefinition::DataItem::ITEM_ZERO;
			zero.zero_bytes = pending_zero_bytes;
			global->data_items.push_back(zero);
			pending_zero_bytes = 0;
		};
		for (std::size_t i = 0; i < bound; ++i)
		{
			if (i >= initializers.size())
			{
				pending_zero_bytes += element_type.storage_size();
				continue;
			}
			const SemanticFactId initializer = initializers[i];
			if (element_type.is_pointer() && typed_pointer_zero(initializer,
				element))
			{
				pending_zero_bytes += element_type.storage_size();
				continue;
			}
			flush_zeroes();
			SymbolId target;
			long long addend = 0;
			if (element_type.is_pointer() && map_constant_address(
				initializer, &target, &addend, NULL))
			{
				GlobalDefinition::DataItem item;
				item.kind = GlobalDefinition::DataItem::ITEM_ADDR;
				item.type = element_type;
				item.symbol_id = target;
				item.addr_addend = addend;
				item.symbol_name_id = symbol_name_for(target);
				if (!item.symbol_name_id.valid())
					throw std::runtime_error("PA15 global address target has no name");
				global->data_items.push_back(item);
				continue;
			}
			Operand value;
			if (!constant_integer(initializer, element_type, &value))
				throw std::runtime_error("PA15 nonconstant global array initializer");
			GlobalDefinition::DataItem item;
			item.kind = GlobalDefinition::DataItem::ITEM_INTEGER;
			item.type = element_type;
			item.literal_operand = value;
			global->data_items.push_back(item);
		}
		flush_zeroes();
	}

void Pa15Lowerer::collect_globals(){




		for (std::size_t scope_index = 0; scope_index < model_.scopes_.size(); ++scope_index)
		{
			const Scope& scope = model_.scopes_[scope_index];
			if (scope.kind != ScopeKind::Namespace) continue;
			for (std::size_t i = 0; i < scope.bindings.size(); ++i)
			{
				const BindingId binding_id = scope.bindings[i];
				const Binding& binding = model_.binding(binding_id);
				if (binding.kind != BindingKind::Variable) continue;
				const std::string internal_name = internal_value_name(ScopeId(scope_index), binding.name);
				global_symbols_[binding_id.value] = SymbolId(next_symbol_++);
				global_name_ids_[binding_id.value] = symbol_spelling(internal_name);
				symbol_name_ids_[global_symbols_[binding_id.value].index] =
					global_name_ids_[binding_id.value];
			}
		}

		for (std::size_t scope_index = 0; scope_index < model_.scopes_.size(); ++scope_index)
		{
			const Scope& scope = model_.scopes_[scope_index];
			if (scope.kind != ScopeKind::Namespace) continue;
			for (std::size_t i = 0; i < scope.bindings.size(); ++i)
			{
				const BindingId binding_id = scope.bindings[i];
				const Binding& binding = model_.binding(binding_id);
				if (binding.kind != BindingKind::Variable) continue;
				const DeclarationFact* declaration = NULL;
				std::map<std::size_t, const DeclarationFact*>::const_iterator declaration_it =
					declaration_by_binding_.find(binding_id.value);
				if (declaration_it != declaration_by_binding_.end()) declaration = declaration_it->second;
				const SymbolId symbol = global_symbols_.find(binding_id.value)->second;
				const SpellingId name_id = global_name_ids_.find(binding_id.value)->second;
				const SpellingId object_name = intern_spelling(abi_variable_symbol(
					binding_id, ScopeId(scope_index)));
				const bool unknown_array = model_.type_kind(model_.strip_cv_type(binding.type)) ==
					TypeKind::Array && model_.types_[model_.strip_cv_type(binding.type).value].unknown_bound;
				const std::vector<SemanticFactId> initializers =
					variable_facts_.find(binding_id.value) != variable_facts_.end() ?
					children(variable_facts_.find(binding_id.value)->second) :
					std::vector<SemanticFactId>();




				const bool declaration_only = declaration != NULL && declaration->is_extern &&
					initializers.empty();
				if (declaration_only)
				{
					GlobalDeclaration entry;
					entry.symbol_id = symbol;
					entry.name_id = name_id;
					entry.has_type = !unknown_array;
					if (entry.has_type) entry.type = low_type(binding.type);
					if (declaration->is_thread_local)
						entry.storage = lowir_model::GSM_THREAD_LOCAL;
					entry.metadata.binding = binding.internal_linkage ? lowir_model::SBM_INTERNAL : lowir_model::SBM_STRONG;
					entry.metadata.object_symbol_id = object_name;
					if (binding.language_linkage == LanguageLinkage::C)
						entry.metadata.linkage = lowir_model::LLM_C;
					program_.global_declarations.push_back(entry);
					continue;
				}
				GlobalDefinition entry;
				entry.symbol_id = symbol;
				entry.name_id = name_id;
				entry.metadata.binding = binding.internal_linkage ? lowir_model::SBM_INTERNAL : lowir_model::SBM_STRONG;
				entry.metadata.object_symbol_id = object_name;
				if (binding.language_linkage == LanguageLinkage::C)
					entry.metadata.linkage = lowir_model::LLM_C;
				if (declaration != NULL && declaration->is_thread_local)
					entry.storage = lowir_model::GSM_THREAD_LOCAL;
				TypeId object_type = model_.strip_cv_type(binding.type);
				if (model_.type_kind(object_type) == TypeKind::Array)
				{
					if (model_.types_[object_type.value].unknown_bound)
						throw std::runtime_error("PA15 unknown-bound array needs an extern declaration");
					entry.structured = true;
					if (initializers.size() == 1 && model_.semantic_facts_[initializers.front().value].kind ==
						SemanticFactKind::BracedInitList)
						append_array_data(&entry, object_type, children(initializers.front()),
							ScopeId(scope_index));
					else
						append_array_data(&entry, object_type, std::vector<SemanticFactId>(),
							ScopeId(scope_index));
				}
				else
				{
					entry.type = low_type(binding.type);
					if (initializers.empty())
						entry.init_kind = GlobalDefinition::INIT_ZERO;
					else if (entry.type.is_pointer() && typed_pointer_zero(
						initializers.front(), binding.type))
						entry.init_kind = GlobalDefinition::INIT_ZERO;
					else
					{
						const ConstantAddressFact* relocation = NULL;
						if (entry.type.is_pointer() && map_constant_address(
							initializers.front(), &entry.init_operand.symbol_id,
							&entry.addr_addend, &relocation))
						{
							if (relocation->kind == ConstantAddressKind::ArrayElement &&
								relocation->byte_addend != 0)
							{
								if (!relocation->index_fact.valid() ||
									!relocation->index_type.valid() ||
									!relocation->element_type.valid())
									throw std::runtime_error(
										"PA15 incomplete typed global projection");
								const LowType index_type =
									low_type(relocation->index_type);
								if (!index_type.is_integer())
									throw std::runtime_error(
										"PA15 noninteger global projection index");
								const Operand index = integer_operand(
									static_cast<long long>(relocation->index_value),
									index_type);
								pending_global_initializers_.push_back(
									PendingGlobalInitializer(symbol,
										entry.init_operand.symbol_id, index,
										low_type(relocation->element_type)));
								entry.init_kind = GlobalDefinition::INIT_ZERO;
							}
							else
							{
								entry.init_kind = GlobalDefinition::INIT_ADDR;
								entry.init_operand.kind = Operand::OP_GLOBAL;
								if (entry.init_operand.symbol_id == symbol)
									throw std::runtime_error(
										"PA15 global initializer points at itself");
								entry.init_operand.presentation_id = symbol_name_for(
									entry.init_operand.symbol_id);
								if (!entry.init_operand.presentation_id.valid())
									throw std::runtime_error(
										"PA15 global initializer target has no name");
							}
						}
						else if (!constant_integer(initializers.front(), entry.type,
							&entry.init_operand))
							throw std::runtime_error(
								"PA15 nonconstant global initializer");
						else
							entry.init_kind = GlobalDefinition::INIT_INTEGER;
					}
				}
				program_.globals.push_back(entry);
			}
		}
	}

void Pa15Lowerer::materialize_pending_global_initializers(){
		if (pending_global_initializers_.empty()) return;
		Function init;
		init.symbol_id = SymbolId(next_symbol_++);
		init.name_id = symbol_spelling("__cppgm_init");
		init.return_type.kind = LowType::TYPE_VOID;
		init.metadata.role = lowir_model::SR_INIT;
		init.metadata.binding = lowir_model::SBM_INTERNAL;
		init.value_begin = ValueId(next_value_);
		const std::size_t function_index = program_.functions.size();
		program_.functions.push_back(init);
		symbol_name_ids_[program_.functions[function_index].symbol_id.index] =
			program_.functions[function_index].name_id;
		current_function_ = function_index;
		current_block_ = InvalidIdentityValue;
		temp_ordinal_ = 0;
		used_value_names_.clear();
		block_indexes_.clear();
		Block entry;
		entry.block_id = BlockId(next_block_++);
		entry.label_id = intern_spelling("^entry");
		program_.functions[function_index].blocks.push_back(entry);
		block_indexes_[entry.block_id.index] = 0;
		current_block_ = 0;
		for (std::size_t i = 0; i < pending_global_initializers_.size(); ++i)
		{
			const PendingGlobalInitializer& pending = pending_global_initializers_[i];
			const SpellingId target_name = symbol_name_for(pending.target);
			const SpellingId global_name = symbol_name_for(pending.global);
			if (!target_name.valid() || !global_name.valid())
				throw std::runtime_error("PA15 init target has no symbol name");
			LowType object_type = pending.element_type;
			LoweredValue storage(global_operand(pending.target, target_name),
				object_type, true);
			const LoweredValue address = address_of_storage(storage);
			const LoweredValue sequence = emit_decay(address);
			const LoweredValue index(pending.index, pending.index.literal_type, false);
			const LoweredValue element = emit_index(sequence, index,
				pending.element_type, true);
			LowType pointer;
			pointer.kind = LowType::TYPE_POINTER;
			emit_store(pointer, element.value,
				global_operand(pending.global, global_name));
		}
		Instruction ret;
		ret.kind = Instruction::IK_RETURN;
		ret.type.kind = LowType::TYPE_VOID;
		block().instructions.push_back(ret);
		program_.functions[function_index].value_count =
			next_value_ - program_.functions[function_index].value_begin.index;
	}

void Pa15Lowerer::index_function_scope_variables(){
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
			function_name_ids_[fact.binding.value] = name_id;
			symbol_name_ids_[function.symbol_id.index] = name_id;
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
		}
	}

void Pa15Lowerer::collect_function_declarations(){
		for (std::size_t scope_index = 0; scope_index < model_.scopes_.size(); ++scope_index)
		{
			const Scope& scope = model_.scopes_[scope_index];
			if (scope.kind != ScopeKind::Namespace) continue;
			for (std::size_t i = 0; i < scope.bindings.size(); ++i)
			{
				const BindingId binding_id = scope.bindings[i];
				const Binding& binding = model_.binding(binding_id);
				if (binding.kind != BindingKind::Function ||
					model_.type_kind(binding.type) != TypeKind::Function ||
					function_symbols_.find(binding_id.value) != function_symbols_.end())
					continue;
				const TypeKey& type = model_.types_[binding.type.value];
				FunctionDeclaration declaration;
				declaration.symbol_id = SymbolId(next_symbol_++);
				declaration.name_id = symbol_spelling(internal_value_name(
					ScopeId(scope_index), binding.name));
				declaration.return_type = low_type(type.result);
				declaration.metadata.binding = binding.internal_linkage ?
					lowir_model::SBM_INTERNAL : lowir_model::SBM_STRONG;
				if (binding.language_linkage != LanguageLinkage::C ||
					binding.internal_linkage)
					declaration.metadata.object_symbol_id = intern_spelling(
						abi_function_symbol(binding_id, ScopeId(scope_index)));
				if (binding.language_linkage == LanguageLinkage::C)
					declaration.metadata.linkage = lowir_model::LLM_C;
				declaration.boundary.arity = type.variadic ?
					lowir_model::CAM_VARIADIC : lowir_model::CAM_FIXED;
				for (std::size_t parameter = 0; parameter < type.parameters.size(); ++parameter)
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
			program_.function_declarations.push_back(declaration);
			function_symbols_[binding_id.value] = declaration.symbol_id;
			function_name_ids_[binding_id.value] = declaration.name_id;
			symbol_name_ids_[declaration.symbol_id.index] = declaration.name_id;
			}
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

void Pa15Lowerer::add_slot(Function& function, BindingId binding, TypeId semantic_type,
	              SpellingId name_id, const LowType& type){
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

void Pa15Lowerer::remember_loop_target(SemanticFactId id, BlockId break_target,
		BlockId continue_target){
		if (!id.valid())
			throw std::runtime_error("PA15 loop fact has no identity");
		if (id.value >= loop_targets_.size())
			throw std::runtime_error("PA15 loop fact is outside target table");
		if (loop_targets_[id.value].valid)
			throw std::runtime_error("PA15 loop was lowered twice");
		loop_targets_[id.value] = LoopTarget(break_target, continue_target);
	}

const LoopTarget* Pa15Lowerer::remembered_loop_target(SemanticFactId id) const{
		if (!id.valid() || id.value >= loop_targets_.size() ||
			!loop_targets_[id.value].valid)
			return NULL;
		return &loop_targets_[id.value];
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

void Pa15Lowerer::materialize_lvalue_value(LoweredValue* result, const LowType& type){
	if (result == NULL || !result->lvalue) return;
	const ValueId value = emit_load(*result, type);
	const Instruction& emitted = block().instructions.back();
	result->value = temporary_operand(value, emitted.destination_name_id);
	result->physical_type = type;
	result->lvalue = false;
}

void Pa15Lowerer::emit_store(const LowType& type, const Operand& value, const Operand& storage){
		Instruction instruction;
		instruction.kind = Instruction::IK_STORE;
		instruction.type = type;
		instruction.first = value;
		instruction.second = storage;
		block().instructions.push_back(instruction);
	}

LoweredValue Pa15Lowerer::address_of_storage(const LoweredValue& storage){
		if (storage.value.kind == Operand::OP_TEMP && storage.lvalue)
		{
			LowType pointer;
			pointer.kind = LowType::TYPE_POINTER;
			return LoweredValue(storage.value, pointer, false);
		}
		if (storage.value.kind == Operand::OP_TEMP && storage.type.is_pointer())
			return LoweredValue(storage.value, storage.type, false);
		if (storage.value.kind != Operand::OP_SLOT &&
			storage.value.kind != Operand::OP_GLOBAL)
			throw std::runtime_error("PA15 address requires addressable storage");
		LowType pointer;
		pointer.kind = LowType::TYPE_POINTER;
		Instruction instruction;
		instruction.kind = Instruction::IK_ADDR;
		instruction.first = storage.value;
		const ValueId value = destination(pointer, &instruction);
		block().instructions.push_back(instruction);
		return LoweredValue(temporary_operand(value, instruction.destination_name_id),
			pointer, false);
	}

LoweredValue Pa15Lowerer::emit_index(const LoweredValue& base, const LoweredValue& offset,
		const LowType& element, bool array_projection){
		if (!base.type.is_pointer() && !base.type.is_object())
			throw std::runtime_error("PA15 index base is not addressable");
		Instruction instruction;
		instruction.kind = Instruction::IK_INDEX;
		instruction.type = element;
		instruction.first = base.value;
		instruction.second = offset.value;
		instruction.index_projection = array_projection ?
			lowir_model::IPK_ARRAY_ELEMENT : lowir_model::IPK_NONE;
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

std::vector<SemanticFactId> Pa15Lowerer::children(SemanticFactId id) const{
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

LowType Pa15Lowerer::lvalue_type(SemanticFactId id) const{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		return low_type(model_.expression_object_type(fact.type));
	}

bool Pa15Lowerer::reference_binding(BindingId binding) const{
		if (!binding.valid()) return false;
		const TypeKind kind = model_.type_kind(
			model_.strip_cv_type(model_.binding(binding).type));
		return kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference;
	}

LoweredValue Pa15Lowerer::generated_slot(const LowType& type, const std::string& prefix){
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
		function().slots.push_back(slot);
		if (slot_spellings_.size() <= slot.slot_id.index)
			slot_spellings_.resize(slot.slot_id.index + 1);
		slot_spellings_[slot.slot_id.index] = name_id;
		return LoweredValue(slot_operand(slot.slot_id), type, true);
	}

LoweredValue Pa15Lowerer::lower_lvalue(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::IdExpression ||
			fact.kind == SemanticFactKind::Variable)
		{
			const LoweredValue storage = storage_for(fact.binding);
			if (!reference_binding(fact.binding)) return storage;
			const LowType object = lvalue_type(id);
			const ValueId value = emit_load(storage, storage.type);
			const Instruction& emitted = block().instructions.back();
			return LoweredValue(temporary_operand(value, emitted.destination_name_id),
				object, true, storage.type);
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
			LowType pointer;
			pointer.kind = LowType::TYPE_POINTER;
			Operand operand = integer_operand(0, pointer);
			operand.presentation_id = intern_spelling("nullptr");
			Instruction instruction;
			instruction.kind = Instruction::IK_COPY;
			instruction.type = pointer;
			instruction.first = operand;
			const ValueId result = destination(pointer, &instruction);
			block().instructions.push_back(instruction);
			return LoweredValue(temporary_operand(result,
				instruction.destination_name_id), pointer, false);
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
		return LoweredValue(integer_operand(value, type), type, false);
	}

LoweredValue Pa15Lowerer::apply_conversions(SemanticFactId id, LoweredValue result,
		bool omit_boolean_context, bool materialize_lvalue,
		bool force_integral_literal_conversion){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.conversion_count != 0 && fact.conversion_begin == InvalidIdentityValue)
			throw std::runtime_error("PA15 invalid semantic conversion range");
		std::size_t conversion_count = fact.conversion_count;
		if (omit_boolean_context && conversion_count != 0)
		{
			FundamentalType target_fundamental;
			const ConversionFact& last = model_.conversion_facts_[
				fact.conversion_begin + conversion_count - 1];
			if (model_.fundamental_of(model_.expression_object_type(last.target),
				&target_fundamental) &&
				target_fundamental == FundamentalType::Bool)
				--conversion_count;
		}
		for (std::size_t i = 0; i < conversion_count; ++i)
		{
			const ConversionFact& conversion = model_.conversion_facts_[
				fact.conversion_begin + i];
			LowType target = low_type(conversion.target);
			if (force_integral_literal_conversion && target.is_integer() &&
				target.integer_width() == 64)
				target = size_low_type();
			if (conversion.kind == ConversionKind::Identity)
			{
				FundamentalType target_fundamental;
				if (model_.fundamental_of(conversion.target, &target_fundamental) &&
					target_fundamental == FundamentalType::Bool &&
					result.physical_type.is_integer() &&
					result.physical_type != target)
				{
					const LowType physical = result.physical_type;
					const LoweredValue zero(integer_operand(0, physical),
						physical, false);
					result = emit_compare_value(lowir_model::CPP_NE, physical,
						result, zero);
					result.type = target;
					result.physical_type = physical;
				}
				continue;
			}
			if (conversion.kind == ConversionKind::LvalueToRvalue)
			{
				materialize_lvalue_value(&result, target);
				result.type = target;
				result.physical_type = target;
				result.lvalue = false;
				continue;
			}
			if (conversion.kind == ConversionKind::ArrayToPointer)
			{
				if (!result.lvalue)
					throw std::runtime_error("PA15 array decay lost its lvalue");
				const LoweredValue address = address_of_storage(result);
				result = emit_decay(address);
				result.type = target;
				result.physical_type = target;
				result.lvalue = false;
				continue;
			}
			if (conversion.kind == ConversionKind::ReferenceBinding)
			{
				if (result.lvalue)
					result = address_of_storage(result);
				else if (result.value.kind == Operand::OP_INTEGER &&
					result.value.int_value == 0)
					result.value.literal_type = target;
					else if (!result.type.is_pointer())
						throw std::runtime_error("PA15 reference binding has no address");
				result.type = target;
				result.physical_type = target;
				result.lvalue = false;
				continue;
			}
			if (conversion.kind == ConversionKind::FunctionToPointer)
			{
				if (result.lvalue)
					result = address_of_storage(result);
				result.type = target;
				result.physical_type = target;
				result.lvalue = false;
				continue;
			}
			if (conversion.kind == ConversionKind::PointerQualification ||
				conversion.kind == ConversionKind::PointerToVoid ||
				conversion.kind == ConversionKind::NullptrToPointer ||
				conversion.kind == ConversionKind::NullIntegerToPointer ||
				conversion.kind == ConversionKind::NullIntegerToNullptr)
			{
				if (result.lvalue && (conversion.kind ==
					ConversionKind::PointerQualification || conversion.kind ==
					ConversionKind::PointerToVoid))
					materialize_lvalue_value(&result, result.type);
				if (result.value.kind == Operand::OP_INTEGER &&
					result.value.int_value == 0)
					result.value.literal_type = target;
				result.type = target;
				result.physical_type = target;
				result.lvalue = false;
				continue;
			}
			if (conversion.kind == ConversionKind::PointerToBool ||
				conversion.kind == ConversionKind::NullptrToBool)
			{
				materialize_lvalue_value(&result, result.type);
				LowType pointer = result.type;
				if (!pointer.is_pointer())
					throw std::runtime_error("PA15 pointer-to-bool source is not a pointer");
				LowType bool_type = target;
				LoweredValue zero(integer_operand(0, pointer), pointer, false);
				result = emit_compare_value(lowir_model::CPP_NE, pointer, result, zero);
				if (result.type != bool_type)
				{
					Instruction instruction;
					instruction.kind = Instruction::IK_CONVERT;
					instruction.source_type = result.type;
					instruction.first = result.value;
					instruction.conversion_operator = lowir_model::COP_TRUNC;
					const ValueId value = destination(bool_type, &instruction);
					block().instructions.push_back(instruction);
					result = LoweredValue(temporary_operand(value,
							instruction.destination_name_id), bool_type, false,
							bool_type);
				}
				continue;
			}
			if (conversion.kind == ConversionKind::Floating ||
				result.type.is_float() || target.is_float())
				throw std::runtime_error("PA15 floating conversion is outside checkpoint");
			materialize_lvalue_value(&result, result.type);
			FundamentalType target_fundamental;
			if (model_.fundamental_of(conversion.target, &target_fundamental) &&
				target_fundamental == FundamentalType::Bool &&
				result.value.kind != Operand::OP_INTEGER &&
				result.physical_type.is_integer())
			{
				LowType boolean_compare;
				boolean_compare.kind = LowType::TYPE_INTEGER;
				boolean_compare.integer_kind = LowType::INTEGER_I64;
				const LoweredValue zero(integer_operand(0, boolean_compare),
					boolean_compare, false);
				result = emit_compare_value(lowir_model::CPP_NE, boolean_compare,
					result, zero);
				result.type = target;
				result.physical_type = boolean_compare;
				continue;
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
				if (force_integral_literal_conversion &&
					source_type.integer_width() != target.integer_width())
				{
					Instruction instruction;
					instruction.kind = Instruction::IK_CONVERT;
					instruction.source_type = source_type;
					instruction.first = result.value;
					instruction.conversion_operator = conversion_operator(conversion);
					const ValueId value = destination(target, &instruction);
					block().instructions.push_back(instruction);
					result.value = temporary_operand(value,
						instruction.destination_name_id);
					result.type = target;
					result.physical_type = target;
					continue;
				}
					result.type = target;
					result.value.literal_type = target;
					result.physical_type = target;
					continue;
			}
			if (source_type == target)
			{
				result.physical_type = target;
				continue;
			}
		if (source_type.integer_width() == target.integer_width())
		{
			if (result.value.kind != Operand::OP_INTEGER &&
					result.physical_type != target)
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
			instruction.source_type = result.physical_type.valid() &&
				result.physical_type.is_integer() && source_type.is_integer() && result.physical_type.integer_width() == source_type.integer_width() ?
				result.physical_type : source_type;
			instruction.first = result.value;
			instruction.conversion_operator = conversion_operator(conversion);
			const ValueId value = destination(target, &instruction);
			block().instructions.push_back(instruction);
				result.value = temporary_operand(value, instruction.destination_name_id);
				result.type = target;
				result.physical_type = target;
		}
		if (materialize_lvalue && result.lvalue && !result.type.is_object())
		{
			const ValueId value = emit_load(result, result.type);
			const Instruction& emitted = block().instructions.back();
				result.value = temporary_operand(value, emitted.destination_name_id);
				result.physical_type = result.type;
				result.lvalue = false;
		}
		return result;
	}
lowir_model::ConversionOperator Pa15Lowerer::conversion_operator(const ConversionFact& conversion) const{
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
			if (model_.unsigned_integral_type(conversion.source))
				return lowir_model::COP_ZEXT;
			return lowir_model::COP_SEXT;
		}
		throw std::runtime_error("PA15 same-width conversion reached instruction emission");
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
		return LoweredValue(temporary_operand(value, instruction.destination_name_id),
			result_type, false);
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
		return emit_index(base, scaled, byte, false);
	}

LoweredValue Pa15Lowerer::lower_incdec(SemanticFactId id, bool postfix){
		const std::vector<SemanticFactId> operands = children(id);
		if (operands.size() != 1)
			throw std::runtime_error("PA15 invalid increment fact");
		const LoweredValue left = lower_lvalue(operands.front());
		const LowType target_type = left.type;
		const ValueId old_id = emit_load(left, target_type);
		const Instruction& old_instruction = block().instructions.back();
		LoweredValue old(temporary_operand(old_id, old_instruction.destination_name_id),
			target_type, false);
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
		emit_store(target_type, updated.value, left.value);
		if (postfix) return old;
		return LoweredValue(left.value, target_type, true);
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



			FundamentalType left_fundamental;
			const bool bool_target = model_.fundamental_of(
				model_.expression_object_type(
					model_.semantic_facts_[operands[0].value].type),
				&left_fundamental) && left_fundamental == FundamentalType::Bool;
			FundamentalType right_fundamental;
			const bool bool_value = model_.fundamental_of(
				model_.expression_object_type(
					model_.semantic_facts_[operands[1].value].type),
				&right_fundamental) && right_fundamental == FundamentalType::Bool;
			right = bool_target && bool_value ?
				lower_condition_expression(operands[1]) :
				lower_expression(operands[1]);
			left = lower_lvalue(operands[0]);
		}
		else
		{
			left = lower_lvalue(operands[0]);
			const ValueId old_id = emit_load(left, left.type);
			const Instruction& old_instruction = block().instructions.back();
			old = LoweredValue(temporary_operand(old_id,
				old_instruction.destination_name_id), left.type, false);
			right = lower_expression(operands[1]);
		}
		const LowType target = left.type;
		const TypeId target_semantic_type = model_.expression_object_type(
			model_.semantic_facts_[operands[0].value].type);
		if (fact.token == SimpleTokenType::OP_ASS)
		{
			emit_store(target, right.value, left.value);
			return preserve_lvalue ? LoweredValue(left.value, target, true) :
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
		emit_store(target, updated.value, left.value);
		return preserve_lvalue ? LoweredValue(left.value, target, true) : updated;
	}

LoweredValue Pa15Lowerer::lower_call(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const std::vector<SemanticFactId> facts = children(id);
		const Binding* callee_binding = NULL;
		const TypeKey* function_type = NULL;
		std::size_t argument_begin = 0;
		Instruction instruction;
		instruction.kind = Instruction::IK_CALL;
		if (fact.has_callee)
		{
			if (!fact.selected_binding.valid())
				throw std::runtime_error("PA15 direct call has no selected binding");
			std::map<std::size_t, SymbolId>::const_iterator symbol =
				function_symbols_.find(fact.selected_binding.value);
			if (symbol == function_symbols_.end())
				throw std::runtime_error("PA15 direct call target was not emitted");
			callee_binding = &model_.binding(fact.selected_binding);
			if (model_.type_kind(callee_binding->type) != TypeKind::Function)
				throw std::runtime_error("PA15 direct call target is not a function");
			function_type = &model_.types_[callee_binding->type.value];
			instruction.direct_callee_id = symbol->second;
			instruction.first = global_operand(symbol->second,
				function_name_ids_.find(fact.selected_binding.value)->second);
		}
		else
		{
			if (facts.empty()) throw std::runtime_error("PA15 indirect call has no callee");
			argument_begin = 1;
			TypeId function_id = model_.strip_cv_type(
				model_.expression_object_type(model_.semantic_facts_[facts.front().value].type));
			if (model_.type_kind(function_id) == TypeKind::Pointer)
				function_id = model_.strip_cv_type(model_.types_[function_id.value].child);
			if (!function_id.valid() || model_.type_kind(function_id) != TypeKind::Function)
				throw std::runtime_error("PA15 indirect call type is not a function");
			function_type = &model_.types_[function_id.value];
			instruction.has_call_signature = true;
		}
		const std::size_t argument_count = facts.size() - argument_begin;
		if (!function_type || (!function_type->variadic &&
			argument_count != function_type->parameters.size()) ||
			(function_type->variadic && argument_count < function_type->parameters.size()))
			throw std::runtime_error("PA15 call arity mismatch");
		instruction.call_return_type = low_type(function_type->result);
		instruction.call_returns_void = instruction.call_return_type.is_void();
		instruction.call_boundary.arity = function_type->variadic ?
			lowir_model::CAM_VARIADIC : lowir_model::CAM_FIXED;
		for (std::size_t i = 0; i < argument_count; ++i)
		{
			const SemanticFactId argument = facts[argument_begin + i];
			if (i < function_type->parameters.size())
			{
				const TypeKind kind = model_.type_kind(
					model_.strip_cv_type(function_type->parameters[i]));
				if (kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference)
					instruction.args.push_back(lower_address(argument).value);
				else
					instruction.args.push_back(lower_expression(argument).value);
			}
			else
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
				const TypeKind kind = model_.type_kind(
					model_.strip_cv_type(function_type->parameters[i]));
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
				low_type(model_.expression_object_type(function_type->result)), true,
				instruction.call_return_type);
		return LoweredValue(temporary_operand(value, instruction.destination_name_id),
			instruction.call_return_type, false);
	}

bool Pa15Lowerer::conditional_address_result(SemanticFactId id) const{
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.category != SemanticValueCategory::Prvalue)
		{
			if (fact.conversion_begin != InvalidIdentityValue)
				for (std::size_t i = 0; i < fact.conversion_count; ++i)
					if (model_.conversion_facts_[fact.conversion_begin + i].kind ==
						ConversionKind::ReferenceBinding)
						return true;
			return false;
		}
		if (fact.child_count != 3) return false;
		const std::vector<SemanticFactId> facts = children(id);
		bool all_arrays = true;
		for (std::size_t i = 1; i < facts.size(); ++i)
		{
			TypeId type = model_.expression_object_type(
				model_.semantic_facts_[facts[i].value].type);
			type = model_.strip_cv_type(type);
			if (!type.valid() || model_.type_kind(type) != TypeKind::Array)
				all_arrays = false;
		}
		return all_arrays;
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
		set_current(then_block);
		const LoweredValue when_true = lower_address(facts[1]);
		emit_store(pointer, when_true.value, result_slot.value);
		if (!terminated(block())) emit_jump(join_block);
		set_current(else_block);
		const LoweredValue when_false = lower_address(facts[2]);
		emit_store(pointer, when_false.value, result_slot.value);
		if (!terminated(block())) emit_jump(join_block);
		set_current(join_block);
		const ValueId value = emit_load(result_slot, pointer);
		const Instruction& emitted = block().instructions.back();
		return LoweredValue(temporary_operand(value, emitted.destination_name_id),
			pointer, false);
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

LoweredValue Pa15Lowerer::lower_address(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const std::vector<SemanticFactId> facts = children(id);
		switch (fact.kind)
		{
		case SemanticFactKind::IdExpression:
		case SemanticFactKind::Variable:
			if (model_.binding(fact.binding).kind == BindingKind::Function)
			{
				const std::map<std::size_t, SymbolId>::const_iterator found =
					function_symbols_.find(fact.binding.value);
				if (found == function_symbols_.end())
					throw std::runtime_error("PA15 function address has no symbol");
				LowType pointer;
				pointer.kind = LowType::TYPE_POINTER;
				return address_of_storage(LoweredValue(global_operand(found->second,
					function_name_ids_.find(fact.binding.value)->second), pointer, false));
			}
			return address_of_storage(lower_lvalue(id));
		case SemanticFactKind::UnaryExpression:
			if (facts.size() != 1) throw std::runtime_error("PA15 invalid address unary fact");
			if (fact.token == SimpleTokenType::OP_AMP)
				return lower_address(facts.front());
			if (fact.token == SimpleTokenType::OP_STAR)
			{
				const LoweredValue pointer = lower_expression(facts.front());
				if (!pointer.type.is_pointer())
					throw std::runtime_error("PA15 dereference address is not a pointer");
				return pointer;
			}
			if (fact.token == SimpleTokenType::OP_INC ||
				fact.token == SimpleTokenType::OP_DEC)
				return address_of_storage(lower_incdec(id, false));
			break;
		case SemanticFactKind::SubscriptExpression:
		{
			if (facts.size() != 2) throw std::runtime_error("PA15 invalid subscript fact");
			const LoweredValue sequence = lower_expression(facts.front());
			const LoweredValue index = lower_expression(facts.back());
			TypeId sequence_type = model_.expression_object_type(
				model_.semantic_facts_[facts.front().value].type);
			sequence_type = model_.strip_cv_type(sequence_type);
			const bool array = sequence_type.valid() &&
				model_.type_kind(sequence_type) == TypeKind::Array;
			const LowType element = low_type(fact.type);
			return emit_index(sequence, index, element, array);
		}
		case SemanticFactKind::AssignmentExpression:
				return address_of_storage(lower_assignment(id, true));
		case SemanticFactKind::BinaryExpression:
			if (fact.token == SimpleTokenType::OP_COMMA && facts.size() == 2)
			{
				lower_discarded_expression(facts.front());
				return lower_address(facts.back());
			}
			break;
		case SemanticFactKind::ConditionalExpression:
			return lower_conditional_address(id);
		case SemanticFactKind::CallExpression:
		{
			const LoweredValue call = lower_call(id);
			if (!call.type.is_pointer() && !call.lvalue)
				throw std::runtime_error("PA15 reference call has no address");
			return call.lvalue ? address_of_storage(call) : call;
		}
		case SemanticFactKind::CastExpression:
			if (facts.size() == 1) return lower_address(facts.front());
			break;
		default:
			break;
		}
		throw std::runtime_error("PA15 unsupported address expression");
	}

bool Pa15Lowerer::pointer_like(TypeId type) const{
		type = model_.strip_cv_type(model_.expression_object_type(type));
		return type.valid() && (model_.type_kind(type) == TypeKind::Pointer ||
			model_.type_kind(type) == TypeKind::Array);
	}

LoweredValue Pa15Lowerer::lower_logical(SemanticFactId id){
		const std::vector<SemanticFactId> operands = children(id);
		if (operands.size() != 2)
			throw std::runtime_error("PA15 invalid logical expression");
		LowType result_type;
		result_type.kind = LowType::TYPE_INTEGER;
		result_type.integer_kind = LowType::INTEGER_I64;
		const LoweredValue slot = generated_slot(result_type,
			model_.semantic_facts_[id.value].token == SimpleTokenType::OP_LAND ?
			"land" : "lor");
		const bool conjunction = model_.semantic_facts_[id.value].token ==
			SimpleTokenType::OP_LAND;
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
		Operand compare_value = right.value;
		if (compare_type.is_integer())
		{
			compare_type = result_type;
			if (compare_value.kind == Operand::OP_INTEGER)
				compare_value.literal_type = compare_type;
		}
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
		return LoweredValue(temporary_operand(value, emitted.destination_name_id),
			result_type, false);
	}

void Pa15Lowerer::initialize_array(BindingId binding, SemanticFactId initializer,
		const LoweredValue& storage){
		const SemanticFact& init_fact = model_.semantic_facts_[initializer.value];
		if (init_fact.kind != SemanticFactKind::BracedInitList)
			throw std::runtime_error("PA15 unsupported array initializer");
		TypeId array_type = model_.strip_cv_type(model_.binding(binding).type);
		if (!array_type.valid() || model_.type_kind(array_type) != TypeKind::Array)
			throw std::runtime_error("PA15 array initializer target is not an array");
		const LowType element_type = low_type(model_.types_[array_type.value].child);
		const LoweredValue base = address_of_storage(storage);
		const std::vector<SemanticFactId> values = children(initializer);
		const std::size_t bound = model_.types_[array_type.value].bound.value;
		if (values.size() > bound)
			throw std::runtime_error("PA15 array initializer exceeds bound");
		for (std::size_t i = 0; i < bound; ++i)
		{
			LoweredValue destination_address = base;
			if (i != 0)
			{
				LowType i64;
				i64.kind = LowType::TYPE_INTEGER;
				i64.integer_kind = LowType::INTEGER_I64;
				LoweredValue offset(integer_operand(static_cast<long long>(i) *
					static_cast<long long>(element_type.storage_size()), i64), i64, false);
				LowType byte;
				byte.kind = LowType::TYPE_INTEGER;
				byte.integer_kind = LowType::INTEGER_I8;
				destination_address = emit_index(base, offset, byte, false);
			}
			if (i < values.size() && model_.semantic_facts_[values[i].value].kind ==
				SemanticFactKind::BracedInitList)
				throw std::runtime_error("PA15 nested array initializer is outside checkpoint");
			const Operand value = i < values.size() ?
				lower_expression(values[i]).value : integer_operand(0, element_type);
			emit_store(element_type, value, destination_address.value);
		}
	}

LoweredValue Pa15Lowerer::lower_expression(SemanticFactId id){
		return lower_expression_impl(id, false);
	}

LoweredValue Pa15Lowerer::lower_condition_expression(SemanticFactId id){
		return lower_expression_impl(id, true);
	}

void Pa15Lowerer::lower_discarded_expression(SemanticFactId id){
		(void)lower_expression_impl(id, false, false);
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

BlockId Pa15Lowerer::control_target(bool continue_target) const{
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
