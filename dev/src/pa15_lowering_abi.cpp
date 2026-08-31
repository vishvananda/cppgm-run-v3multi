#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

std::string Pa15Lowerer::abi_variable_symbol(BindingId binding_id,
	ScopeId owner) const{
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

std::vector<std::string> Pa15Lowerer::named_type_components(
	NamedRecordId record) const{
	if (!record.valid() || record.value >= model_.named_.size())
		throw std::runtime_error("PA15 ABI named type has no name");
	const NamedRecord& named = model_.named_[record.value];
	if (!named.name.valid())
		throw std::runtime_error("PA15 ABI named type has no name");
	return value_components(named.owner, named.name);
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
	case FundamentalType::NullptrT: result.builtin = abi_mangle::ABI_BUILTIN_NULLPTR; break;
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

std::string Pa15Lowerer::abi_symbol(const FunctionFact& fact,
	abi_mangle::AbiFunctionSpecialTerminalKind terminal) const{
	const Binding& binding = model_.binding(fact.binding);
	if (model_.type_kind(binding.type) != TypeKind::Function)
		throw std::runtime_error("PA15 function binding has non-function type");
	const TypeKey& function = model_.types_[binding.type.value];
	const BindingSidecar* sidecar = model_.binding_sidecar(fact.binding);
	const bool constructor = sidecar != NULL &&
		sidecar->constructor_record.valid();
	const bool destructor = fact.is_destructor || (sidecar != NULL &&
		sidecar->destructor_record.valid());
	abi_mangle::AbiFactCase facts;
	abi_mangle::AbiFactRecord record;
	record.kind = abi_mangle::ABI_FACT_RECORD_TARGET;
	record.target.kind = abi_mangle::ABI_TARGET_FACT_FUNCTION;
	record.target.linkage = (binding.internal_linkage ||
		binding.language_linkage == LanguageLinkage::Cxx) ?
		abi_mangle::ABI_LINKAGE_CXX : abi_mangle::ABI_LINKAGE_C;
	record.target.function.kind = abi_mangle::ABI_FUNCTION_TARGET_PATH;
	record.target.function.name.components = function_abi_components(
		fact.binding, fact.owner);
	if (constructor || destructor)
	{
		if (record.target.function.name.components.empty())
			throw std::runtime_error("PA15 special-member ABI path is empty");
		record.target.function.name.components.pop_back();
		if (constructor)
			record.target.function.special_terminal = terminal ==
				abi_mangle::ABI_SPECIAL_TERMINAL_NONE ?
				abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_COMPLETE : terminal;
		else
			record.target.function.special_terminal = terminal ==
				abi_mangle::ABI_SPECIAL_TERMINAL_NONE ?
				abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_COMPLETE : terminal;
	}
	else
		record.target.function.operator_terminal = operator_terminal(
			fact.binding, function.parameters.size());
	for (std::size_t i = 0; i < function.parameters.size(); ++i)
		record.target.function.signature_parameter_types.push_back(
			abi_type(function.parameters[i]));
	if (fact.owner.valid() && fact.owner.value < model_.scopes_.size() &&
		model_.scopes_[fact.owner.value].kind == ScopeKind::Class &&
		(function.cv & 3u) != 0)
	{
		abi_mangle::AbiFactRecord qualifier;
		qualifier.kind = abi_mangle::ABI_FACT_RECORD_FUNCTION;
		qualifier.function.kind = abi_mangle::ABI_FUNCTION_RECORD_QUALIFIER;
		if ((function.cv & 1u) != 0)
			qualifier.function.qualifiers.push_back(
				abi_mangle::ABI_FUNCTION_QUALIFIER_CONST);
		if ((function.cv & 2u) != 0)
			qualifier.function.qualifiers.push_back(
				abi_mangle::ABI_FUNCTION_QUALIFIER_VOLATILE);
		facts.records.push_back(qualifier);
	}
	if (constructor || destructor)
	{
		abi_mangle::AbiFactRecord terminal_record;
		terminal_record.kind = abi_mangle::ABI_FACT_RECORD_FUNCTION;
		terminal_record.function.kind =
			abi_mangle::ABI_FUNCTION_RECORD_TERMINAL;
		terminal_record.function.special_terminal = record.target.function.
			special_terminal;
		facts.records.push_back(terminal_record);
	}
	facts.records.push_back(record);
	return abi_mangle::mangle_abi_fact_case(facts);
}

std::string Pa15Lowerer::abi_function_symbol(BindingId binding_id,
	ScopeId owner) const{
	const Binding& binding = model_.binding(binding_id);
	if (model_.type_kind(binding.type) != TypeKind::Function)
		throw std::runtime_error("PA15 function binding has non-function type");
	const TypeKey& function = model_.types_[binding.type.value];
	const BindingSidecar* sidecar = model_.binding_sidecar(binding_id);
	const bool constructor = sidecar != NULL &&
		sidecar->constructor_record.valid();
	const bool destructor = sidecar != NULL &&
		sidecar->destructor_record.valid();
	abi_mangle::AbiFactCase facts;
	abi_mangle::AbiFactRecord record;
	record.kind = abi_mangle::ABI_FACT_RECORD_TARGET;
	record.target.kind = abi_mangle::ABI_TARGET_FACT_FUNCTION;
	record.target.linkage = (binding.internal_linkage ||
		binding.language_linkage == LanguageLinkage::Cxx) ?
		abi_mangle::ABI_LINKAGE_CXX : abi_mangle::ABI_LINKAGE_C;
	record.target.function.kind = abi_mangle::ABI_FUNCTION_TARGET_PATH;
	record.target.function.name.components = function_abi_components(
		binding_id, owner);
	if (constructor || destructor)
	{
		if (record.target.function.name.components.empty())
			throw std::runtime_error("PA15 special-member ABI path is empty");
		record.target.function.name.components.pop_back();
		record.target.function.special_terminal = constructor ?
			abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_COMPLETE :
			abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_COMPLETE;
	}
	else
		record.target.function.operator_terminal = operator_terminal(
			binding_id, function.parameters.size());
	for (std::size_t i = 0; i < function.parameters.size(); ++i)
		record.target.function.signature_parameter_types.push_back(
			abi_type(function.parameters[i]));
	if (owner.valid() && owner.value < model_.scopes_.size() &&
		model_.scopes_[owner.value].kind == ScopeKind::Class &&
		(function.cv & 3u) != 0)
	{
		abi_mangle::AbiFactRecord qualifier;
		qualifier.kind = abi_mangle::ABI_FACT_RECORD_FUNCTION;
		qualifier.function.kind = abi_mangle::ABI_FUNCTION_RECORD_QUALIFIER;
		if ((function.cv & 1u) != 0)
			qualifier.function.qualifiers.push_back(
				abi_mangle::ABI_FUNCTION_QUALIFIER_CONST);
		if ((function.cv & 2u) != 0)
			qualifier.function.qualifiers.push_back(
				abi_mangle::ABI_FUNCTION_QUALIFIER_VOLATILE);
		facts.records.push_back(qualifier);
	}
	if (constructor || destructor)
	{
		abi_mangle::AbiFactRecord terminal_record;
		terminal_record.kind = abi_mangle::ABI_FACT_RECORD_FUNCTION;
		terminal_record.function.kind =
			abi_mangle::ABI_FUNCTION_RECORD_TERMINAL;
		terminal_record.function.special_terminal = constructor ?
			abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_COMPLETE :
			abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_COMPLETE;
		facts.records.push_back(terminal_record);
	}
	facts.records.push_back(record);
	return abi_mangle::mangle_abi_fact_case(facts);
}

} // namespace pa11_semantic_internal
