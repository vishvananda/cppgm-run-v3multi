#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

LoweredValue Pa15Lowerer::lower_member_address(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 1 || (fact.token != SimpleTokenType::OP_DOT &&
			fact.token != SimpleTokenType::OP_ARROW))
			throw std::runtime_error("PA15 invalid member address fact");
		if (!fact.selected_binding.valid())
			throw std::runtime_error("PA15 member address has no selected binding");
		const BindingId member_id = fact.selected_binding;
		const Binding& member = model_.binding(member_id);
		if (member.kind != BindingKind::Variable)
			throw std::runtime_error("PA15 member address is not a data member");
		if (model_.is_static_member(member_id))
		{
			// The object operand is still an evaluated source expression, but a
			// static data member has no subobject projection.  Its storage is the
			// canonical class-owned global selected by PA11/PA12.
			lower_discarded_expression(facts.front());
			return address_of_storage(storage_for(member_id));
		}
		const BindingSidecar* sidecar = model_.binding_sidecar(member_id);
		if (sidecar != NULL && sidecar->backing_storage.valid())
			// Anonymous-union injected members are storage-backed facts, not
			// class-owned member projections.  Keep their prior unsupported
			// boundary before requiring the typed owner invariant below.
			throw std::runtime_error("PA15 injected member projection is unsupported");
		if (!fact.selected_scope.valid() || fact.selected_scope.value >=
			model_.scopes_.size() || model_.scopes_[fact.selected_scope.value].kind !=
			ScopeKind::Class)
			throw std::runtime_error("PA15 member address has no selected owner");

		const SemanticFact& object_fact = model_.semantic_facts_[facts.front().value];
		TypeId record_type = TypeId();
		LoweredValue base;
		if (fact.token == SimpleTokenType::OP_ARROW)
		{
			const TypeId pointer = model_.strip_cv_type(
				model_.expression_object_type(object_fact.type));
			if (!pointer.valid() || model_.type_kind(pointer) != TypeKind::Pointer)
				throw std::runtime_error("PA15 arrow member base is not a pointer");
			record_type = model_.strip_cv_type(model_.expression_object_type(
				model_.types_[pointer.value].child));
			base = lower_expression(facts.front());
		}
		else
		{
			record_type = model_.strip_cv_type(
				model_.expression_object_type(object_fact.type));
			base = lower_address(facts.front());
		}
		if (!record_type.valid() || model_.type_kind(record_type) != TypeKind::Named)
			throw std::runtime_error("PA15 member base is not a named record");
		if (!base.type.is_pointer())
			throw std::runtime_error("PA15 member base has no address");

		const NamedRecordId record = model_.named_record_for_type(record_type);
		if (!record.valid() || record.value >= model_.named_.size() ||
			model_.named_[record.value].kind != NamedKind::Class)
			throw std::runtime_error("PA15 member base is not a class");
		const NamedRecordId owner = model_.scopes_[fact.selected_scope.value].record;
		if (!owner.valid() || owner.value >= model_.named_.size() ||
			model_.named_[owner.value].kind != NamedKind::Class ||
			model_.named_[owner.value].scope != fact.selected_scope)
			throw std::runtime_error("PA15 member owner metadata is invalid");
		std::vector<NamedRecordId> base_path;
		if (!model_.member_base_path(record_type, fact.selected_scope,
			&base_path))
			throw std::runtime_error("PA15 member base owner mismatch");
		NamedRecordId current_record = record;
		const LowType offset_type = size_low_type();
		LowType byte;
		byte.kind = LowType::TYPE_INTEGER;
		byte.integer_kind = LowType::INTEGER_I8;
		for (std::size_t i = 0; i < base_path.size(); ++i)
		{
			const NamedRecordId base_record = base_path[i];
			if (!current_record.valid() || current_record.value >=
				model_.named_.size() || !base_record.valid() ||
				base_record.value >= model_.named_.size())
				throw std::runtime_error("PA15 member base path is invalid");
			const NamedRecord& current = model_.named_[current_record.value];
			if (current.kind != NamedKind::Class || !current.has_base ||
				current.direct_base_virtual || current.direct_base != base_record)
				throw std::runtime_error("PA15 member base relation is invalid");
			const RecordLayout& current_layout = model_.record_layout(
				current_record);
			if (current_layout.state != RecordLayoutState::Complete ||
				!current_layout.has_direct_base ||
				current_layout.direct_base.record != base_record ||
				current_layout.direct_base.offset != 0)
				throw std::runtime_error("PA15 member base layout is invalid");
			const RecordLayout& base_layout = model_.record_layout(base_record);
			if (base_layout.state != RecordLayoutState::Complete)
				throw std::runtime_error("PA15 member base target has no complete layout");
			const LoweredValue zero(integer_operand(0, offset_type),
				offset_type, false);
			base = emit_index(base, zero, byte,
				lowir_model::IPK_BASE_SUBOBJECT);
			current_record = base_record;
		}
		if (current_record != owner)
			throw std::runtime_error("PA15 member base path ended at wrong owner");
		const RecordLayout& layout = model_.record_layout(owner);
		if (layout.state != RecordLayoutState::Complete)
			throw std::runtime_error("PA15 member owner has no complete layout");
		const std::size_t* offset = layout.member_offsets.find(member_id);
		if (offset == NULL)
			throw std::runtime_error("PA15 member is not in the owner record layout");
		if (*offset > static_cast<std::size_t>(std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 member offset exceeds LowIR range");
		const LoweredValue member_offset(integer_operand(
			static_cast<long long>(*offset), offset_type), offset_type, false);
		return emit_index(base, member_offset, byte, lowir_model::IPK_FIELD);
}

void Pa15Lowerer::materialize_lvalue_value(LoweredValue* result,
	const LowType& type)
{
	if (result == NULL || !result->lvalue) return;
	if (result->bit_field_lvalue)
	{
		const LoweredValue value = emit_bit_field_load(*result,
			result->bit_field_binding, type);
		*result = value;
		return;
	}
	const ValueId value = emit_load(*result, type);
	const Instruction& emitted = block().instructions.back();
	result->value = temporary_operand(value, emitted.destination_name_id);
	result->physical_type = type;
	result->lvalue = false;
}

void Pa15Lowerer::emit_store(const LowType& type, const Operand& value,
	const Operand& storage)
{
	Instruction instruction;
	instruction.kind = Instruction::IK_STORE;
	instruction.type = type;
	instruction.first = value;
	instruction.second = storage;
	block().instructions.push_back(instruction);
}

LoweredValue Pa15Lowerer::mark_bit_field_address(
	const LoweredValue& address, BindingId binding_id) const
{
	const BitFieldFact* fact = model_.bit_field_fact(binding_id);
	if (fact == NULL || !fact->named || fact->binding != binding_id ||
		!address.type.is_pointer())
		throw std::runtime_error("PA15 bit-field address fact is invalid");
	LowType value_type = low_type(fact->declared_type);
	const TypeId int_type = model_.fundamental(FundamentalType::Int);
	const LowType promoted_type = low_type(int_type);
	// Integral bit-fields narrower than int undergo the ordinary integral
	// promotion at a value boundary.  Keep the declared type in BitFieldFact,
	// but expose the promoted operation type to PA15 so a same-width storage
	// load can preserve the glvalue's projection without a text-based guess.
	if (promoted_type.is_integer() && value_type.is_integer() &&
		fact->value_width < static_cast<std::size_t>(
			promoted_type.integer_width()) &&
		model_.integral_rank(fact->storage_type) <=
		model_.integral_rank(int_type))
		value_type = promoted_type;
	LoweredValue result(address.value, value_type, true);
	result.bit_field_lvalue = true;
	result.bit_field_binding = binding_id;
	return result;
}

LoweredValue Pa15Lowerer::emit_bit_field_load(
	const LoweredValue& storage, BindingId binding_id,
	const LowType& result_type)
{
	const BitFieldFact* fact = model_.bit_field_fact(binding_id);
	if (fact == NULL || !fact->named || fact->binding != binding_id ||
		!storage.lvalue || !result_type.is_integer())
		throw std::runtime_error("PA15 bit-field load fact is invalid");
	const LowType unit_type = low_type(fact->storage_type);
	if (!unit_type.is_integer() || fact->storage_width == 0 ||
		fact->storage_width > 64 || fact->value_width == 0 ||
		fact->value_width > fact->storage_width ||
		fact->bit_offset > fact->storage_width - fact->value_width)
		throw std::runtime_error("PA15 bit-field load projection is invalid");
	const LowType extraction_type = unit_type.integer_width() ==
		result_type.integer_width() ? result_type : unit_type;
	const LoweredValue unit_storage(storage.value, unit_type, true);
	const ValueId loaded_id = emit_load(unit_storage, extraction_type);
	const Instruction& loaded_instruction = block().instructions.back();
	LoweredValue value(temporary_operand(loaded_id,
		loaded_instruction.destination_name_id), extraction_type, false,
		extraction_type);
	if (fact->bit_offset != 0)
	{
		const lowir_model::BinaryOperator shift =
			extraction_type == unit_type && !fact->is_signed ?
				lowir_model::BOP_USHR : lowir_model::BOP_SHR;
		value = emit_binary_value(shift, extraction_type, value,
			LoweredValue(integer_operand(static_cast<long long>(fact->bit_offset),
				extraction_type), extraction_type, false));
	}
	value = emit_binary_value(lowir_model::BOP_AND, extraction_type, value,
		LoweredValue(integer_operand(static_cast<long long>(fact->value_mask),
			extraction_type), extraction_type, false));
	if (fact->is_signed && fact->value_width < fact->storage_width)
	{
		const std::size_t shift = fact->storage_width - fact->value_width;
		const LoweredValue amount(integer_operand(static_cast<long long>(shift),
			extraction_type), extraction_type, false);
		value = emit_binary_value(lowir_model::BOP_SHL, extraction_type, value,
			amount);
		value = emit_binary_value(lowir_model::BOP_SHR, extraction_type, value,
			amount);
	}
	if (extraction_type != result_type)
	{
		if (!result_type.is_integer())
			throw std::runtime_error("PA15 bit-field result is not integral");
		if (extraction_type.integer_width() == result_type.integer_width())
		{
			Instruction copy;
			copy.kind = Instruction::IK_COPY;
			copy.type = result_type;
			copy.first = value.value;
			const ValueId copied = destination(result_type, &copy);
			block().instructions.push_back(copy);
			value = LoweredValue(temporary_operand(copied,
				copy.destination_name_id), result_type, false);
		}
		else
		{
			Instruction conversion;
			conversion.kind = Instruction::IK_CONVERT;
			conversion.source_type = extraction_type;
			conversion.first = value.value;
			conversion.conversion_operator = extraction_type.integer_width() >
				result_type.integer_width() ? lowir_model::COP_TRUNC :
				fact->is_signed ? lowir_model::COP_SEXT : lowir_model::COP_ZEXT;
			const ValueId converted = destination(result_type, &conversion);
			block().instructions.push_back(conversion);
			value = LoweredValue(temporary_operand(converted,
				conversion.destination_name_id), result_type, false);
		}
	}
	return value;
}

LoweredValue Pa15Lowerer::encode_bit_field_value(BindingId binding_id,
	const LoweredValue& value, bool force_storage_type)
{
	const BitFieldFact* fact = model_.bit_field_fact(binding_id);
	if (fact == NULL || !fact->named || fact->binding != binding_id ||
		fact->value_width == 0 || fact->storage_width == 0 ||
		fact->storage_width > 64 || fact->bit_offset >
		fact->storage_width - fact->value_width)
		throw std::runtime_error("PA15 bit-field value fact is invalid");
	const LowType unit_type = low_type(fact->storage_type);
	if (!unit_type.is_integer())
		throw std::runtime_error("PA15 bit-field storage is not integral");
	LoweredValue source = value;
	if (source.lvalue)
		materialize_lvalue_value(&source, source.type);
	if (!source.type.is_integer() || !source.physical_type.is_integer())
		throw std::runtime_error("PA15 bit-field value is not integral");
	LowType write_type = unit_type;
	if (!force_storage_type && source.physical_type.integer_width() ==
		unit_type.integer_width())
		write_type = source.physical_type;
	if (source.physical_type != write_type)
	{
		if (source.value.kind == Operand::OP_INTEGER)
		{
			source.value.literal_type = write_type;
			source.type = write_type;
			source.physical_type = write_type;
		}
		else
		{
			Instruction conversion;
			conversion.kind = Instruction::IK_CONVERT;
			conversion.source_type = source.physical_type;
			conversion.first = source.value;
			conversion.conversion_operator = source.physical_type.integer_width() >
				write_type.integer_width() ? lowir_model::COP_TRUNC :
				!fact->is_signed ? lowir_model::COP_ZEXT : lowir_model::COP_SEXT;
			const ValueId converted = destination(write_type, &conversion);
			block().instructions.push_back(conversion);
			source = LoweredValue(temporary_operand(converted,
				conversion.destination_name_id), write_type, false);
		}
	}
	LoweredValue encoded = emit_binary_value(lowir_model::BOP_AND, write_type,
		LoweredValue(integer_operand(static_cast<long long>(fact->value_mask),
			write_type), write_type, false), source);
	if (fact->bit_offset != 0)
		encoded = emit_binary_value(lowir_model::BOP_SHL, write_type, encoded,
			LoweredValue(integer_operand(static_cast<long long>(fact->bit_offset),
				write_type), write_type, false));
	return encoded;
}

void Pa15Lowerer::emit_encoded_bit_field_store(
	const LoweredValue& storage, BindingId binding_id,
	const LoweredValue& encoded, bool preserve_existing)
{
	const BitFieldFact* fact = model_.bit_field_fact(binding_id);
	if (fact == NULL || !fact->named || fact->binding != binding_id ||
		!storage.lvalue || !encoded.type.is_integer() ||
		!encoded.physical_type.is_integer() || fact->value_width == 0 ||
		fact->storage_width == 0 || fact->storage_width > 64 ||
		fact->bit_offset > fact->storage_width - fact->value_width)
		throw std::runtime_error("PA15 encoded bit-field store fact is invalid");
	const LowType unit_type = low_type(fact->storage_type);
	if (!unit_type.is_integer() || encoded.type.integer_width() !=
		unit_type.integer_width() || encoded.physical_type != encoded.type)
		throw std::runtime_error("PA15 encoded bit-field storage is invalid");
	const LowType write_type = encoded.type;
	if (preserve_existing)
	{
		const LoweredValue unit_storage(storage.value, write_type, true);
		const ValueId old_id = emit_load(unit_storage, write_type);
		const Instruction& old_instruction = block().instructions.back();
		const LoweredValue old(temporary_operand(old_id,
			old_instruction.destination_name_id), write_type, false);
		const std::uint64_t full_mask = fact->storage_width >= 64 ?
			std::numeric_limits<std::uint64_t>::max() :
			((static_cast<std::uint64_t>(1) << fact->storage_width) - 1);
		const std::uint64_t clear_mask = full_mask & ~fact->storage_mask;
		const LoweredValue cleared = emit_binary_value(lowir_model::BOP_AND,
			write_type, old, LoweredValue(integer_operand(static_cast<long long>(
				clear_mask), write_type), write_type, false));
		const LoweredValue combined = emit_binary_value(lowir_model::BOP_OR,
			write_type, cleared, encoded);
		emit_store(write_type, combined.value, storage.value);
		return;
	}
	emit_store(write_type, encoded.value, storage.value);
}

void Pa15Lowerer::emit_bit_field_store(const LoweredValue& storage,
	BindingId binding_id, const LoweredValue& value, bool preserve_existing)
{
	if (preserve_existing)
	{
		const BitFieldFact* fact = model_.bit_field_fact(binding_id);
		if (fact == NULL || !fact->named || fact->binding != binding_id ||
			!storage.lvalue)
			throw std::runtime_error("PA15 preserving bit-field store is invalid");
		LoweredValue source = value;
		if (source.lvalue)
			materialize_lvalue_value(&source, source.type);
		const LowType unit_type = low_type(fact->storage_type);
		if (!unit_type.is_integer() || !source.physical_type.is_integer() ||
			!source.type.is_integer())
			throw std::runtime_error("PA15 preserving bit-field value is invalid");
		const LowType write_type = source.physical_type.integer_width() ==
			unit_type.integer_width() ? source.physical_type : unit_type;
		const LoweredValue unit_storage(storage.value, write_type, true);
		const ValueId old_id = emit_load(unit_storage, write_type);
		const Instruction& old_instruction = block().instructions.back();
		const LoweredValue old(temporary_operand(old_id,
			old_instruction.destination_name_id), write_type, false);
		const std::uint64_t full_mask = fact->storage_width >= 64 ?
			std::numeric_limits<std::uint64_t>::max() :
			((static_cast<std::uint64_t>(1) << fact->storage_width) - 1);
		const std::uint64_t clear_mask = full_mask & ~fact->storage_mask;
		const LoweredValue cleared = emit_binary_value(lowir_model::BOP_AND,
			write_type, old, LoweredValue(integer_operand(static_cast<long long>(
				clear_mask), write_type), write_type, false));
		const LoweredValue encoded = encode_bit_field_value(binding_id, source);
		if (encoded.type != write_type)
			throw std::runtime_error("PA15 preserving bit-field encoding changed type");
		const LoweredValue combined = emit_binary_value(lowir_model::BOP_OR,
			write_type, cleared, encoded);
		emit_store(write_type, combined.value, storage.value);
		return;
	}
	const LoweredValue encoded = encode_bit_field_value(binding_id, value);
	emit_encoded_bit_field_store(storage, binding_id, encoded,
		preserve_existing);
}

bool Pa15Lowerer::bit_field_initialization_preserves_existing(
	BindingId binding_id, const BitFieldInitializationContext& context) const
{
	const BitFieldFact* fact = model_.bit_field_fact(binding_id);
	if (fact == NULL || !fact->named || !fact->owner_record.valid())
		throw std::runtime_error("PA15 bit-field initialization fact is invalid");
	return context.has_initialized_unit &&
		context.owner_record == fact->owner_record &&
		context.storage_offset == fact->storage_offset;
}

void Pa15Lowerer::initialize_bit_field(const LoweredValue& storage,
	BindingId binding_id, const LoweredValue& value,
	BitFieldInitializationContext& context)
{
	const BitFieldFact* fact = model_.bit_field_fact(binding_id);
	if (fact == NULL || !fact->named)
		throw std::runtime_error("PA15 bit-field initializer fact is invalid");
	const bool preserve_existing =
		bit_field_initialization_preserves_existing(binding_id, context);
	if (context.has_initialized_unit &&
		(context.owner_record != fact->owner_record ||
			fact->storage_offset < context.storage_offset))
		throw std::runtime_error(
			"PA15 bit-field initializer left its declaration-order root");
	if (!context.has_initialized_unit)
	{
		context.owner_record = fact->owner_record;
		context.storage_offset = fact->storage_offset;
		context.has_initialized_unit = true;
	}
	else if (fact->storage_offset > context.storage_offset)
		context.storage_offset = fact->storage_offset;
	if (preserve_existing)
	{
		emit_bit_field_store(storage, binding_id, value, true);
		return;
	}
	const LoweredValue encoded = encode_bit_field_value(binding_id, value,
		true);
	emit_encoded_bit_field_store(storage, binding_id, encoded, false);
}

void Pa15Lowerer::initialize_encoded_bit_field(
	const LoweredValue& storage, BindingId binding_id,
	const LoweredValue& encoded, BitFieldInitializationContext& context)
{
	const BitFieldFact* fact = model_.bit_field_fact(binding_id);
	if (fact == NULL || !fact->named)
		throw std::runtime_error("PA15 encoded bit-field initializer fact is invalid");
	if (bit_field_initialization_preserves_existing(binding_id, context))
		throw std::runtime_error("PA15 encoded bit-field initializer is not first");
	if (context.has_initialized_unit &&
		(context.owner_record != fact->owner_record ||
			fact->storage_offset < context.storage_offset))
		throw std::runtime_error(
			"PA15 encoded bit-field initializer left its declaration-order root");
	context.owner_record = fact->owner_record;
	context.storage_offset = fact->storage_offset;
	context.has_initialized_unit = true;
	emit_encoded_bit_field_store(storage, binding_id, encoded, false);
}

LoweredValue Pa15Lowerer::address_of_storage(const LoweredValue& storage)
{
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

}
