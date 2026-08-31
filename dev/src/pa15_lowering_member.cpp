#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

LoweredValue Pa15Lowerer::apply_derived_base_conversion(
	LoweredValue result, const ConversionFact& conversion,
	const LowType& target, bool address_context)
{
	if (!conversion.source.valid() || !conversion.target.valid())
		throw std::runtime_error("PA15 derived-base conversion type is invalid");
	if (conversion.kind != ConversionKind::DerivedToBase ||
		!conversion.base_access_checked ||
		!conversion.base_access_scope.valid() ||
		conversion.base_access_scope.value >= model_.scopes_.size() ||
		conversion.base_path_begin == InvalidIdentityValue ||
		conversion.base_path_count == 0 ||
		conversion.base_path_begin > model_.conversion_base_paths_.size() ||
		conversion.base_path_count > model_.conversion_base_paths_.size() -
			conversion.base_path_begin ||
		conversion.base_distance != conversion.base_path_count)
		throw std::runtime_error("PA15 derived-base conversion path is invalid");
	TypeId source_object = model_.expression_object_type(conversion.source);
	TypeId target_object = model_.expression_object_type(conversion.target);
	const bool source_pointer = model_.pointer_id(source_object);
	const bool target_pointer = model_.pointer_id(target_object);
	if (source_pointer != target_pointer)
		throw std::runtime_error("PA15 derived-base conversion category changed");
	if (source_pointer)
	{
		const TypeId source_pointer_type = model_.strip_cv_type(source_object);
		const TypeId target_pointer_type = model_.strip_cv_type(target_object);
		if (model_.type_kind(source_pointer_type) != TypeKind::Pointer ||
			model_.type_kind(target_pointer_type) != TypeKind::Pointer)
			throw std::runtime_error(
				"PA15 derived-base pointer conversion type is invalid");
		source_object = model_.types_[source_pointer_type.value].child;
		target_object = model_.types_[target_pointer_type.value].child;
	}
	source_object = model_.strip_cv_type(source_object);
	target_object = model_.strip_cv_type(target_object);
	LoweredValue base;
	if (source_pointer)
	{
		if (result.lvalue)
			materialize_lvalue_value(&result, result.type);
		if (!result.type.is_pointer() && !result.physical_type.is_pointer())
			throw std::runtime_error(
				"PA15 derived-base pointer conversion has no pointer value");
		base = result;
	}
	else if (address_context)
	{
		if (!result.type.is_pointer() && !result.physical_type.is_pointer())
			throw std::runtime_error(
				"PA15 derived-base address conversion has no address");
		base = result;
	}
	else
	{
		if (!result.lvalue)
			throw std::runtime_error(
				"PA15 class-by-value base conversion is outside checkpoint");
		base = address_of_storage(result);
	}
	if (!base.type.is_pointer() && !base.physical_type.is_pointer())
		throw std::runtime_error("PA15 derived-base projection has no address");
	const bool null_pointer_literal = source_pointer &&
		base.value.kind == Operand::OP_INTEGER && base.value.int_value == 0;

	const NamedRecordId source_record =
		model_.named_record_for_type(source_object);
	const NamedRecordId target_record =
		model_.named_record_for_type(target_object);
	if (!source_record.valid() || !target_record.valid())
		throw std::runtime_error(
			"PA15 derived-base projection record identity is invalid");
	const LowType offset_type = size_low_type();
	LowType byte;
	byte.kind = LowType::TYPE_INTEGER;
	byte.integer_kind = LowType::INTEGER_I8;
	NamedRecordId current_record = source_record;
	const std::size_t path_begin = conversion.base_path_begin;
	for (std::size_t i = 0; i < conversion.base_path_count; ++i)
	{
		const NamedRecordId base_record = model_.conversion_base_paths_[
			path_begin + i];
		if (!current_record.valid() || current_record.value >=
			model_.named_.size() || !base_record.valid() ||
			base_record.value >= model_.named_.size())
			throw std::runtime_error(
				"PA15 derived-base projection path is invalid");
		const NamedRecord& current = model_.named_[current_record.value];
		if (current.kind != NamedKind::Class || !current.has_base ||
			current.direct_base_virtual || current.direct_base != base_record)
			throw std::runtime_error(
				"PA15 derived-base projection relation is invalid");
		const RecordLayout& current_layout =
			model_.record_layout(current_record);
		if (current_layout.state != RecordLayoutState::Complete ||
			!current_layout.has_direct_base ||
			current_layout.direct_base.record != base_record ||
			current_layout.direct_base.offset != 0)
			throw std::runtime_error(
				"PA15 derived-base projection layout is invalid");
		const RecordLayout& base_layout = model_.record_layout(base_record);
		if (base_layout.state != RecordLayoutState::Complete)
			throw std::runtime_error(
				"PA15 derived-base target layout is incomplete");
		// Every supported edge is laid out at offset zero.  A runtime null
		// pointer therefore remains null through the zero projection; a typed
		// null literal skips even that projection so it cannot become a
		// fabricated non-null address.
		current_record = base_record;
	}
	if (current_record != target_record)
		throw std::runtime_error(
			"PA15 derived-base projection ended at wrong record");
	// A DerivedToBase fact has a nonempty typed path; identity conversions are
	// handled by the structural Identity branch.  Keep the count guard here as
	// well so a malformed identity-shaped fact cannot grow LowIR.
	if (conversion.base_path_count != 0 && !null_pointer_literal)
	{
		const LoweredValue zero(integer_operand(0, offset_type),
			offset_type, false);
		base = emit_index(base, zero, byte,
			lowir_model::IPK_BASE_SUBOBJECT);
	}
	if (address_context)
		return base;
	base.type = target;
	base.physical_type = target;
	base.lvalue = false;
	if (null_pointer_literal)
		base.value.literal_type = target;
	return base;
}

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
			current_record = base_record;
		}
		if (current_record != owner)
			throw std::runtime_error("PA15 member base path ended at wrong owner");
		if (!base_path.empty())
		{
			const LoweredValue zero(integer_operand(0, offset_type),
				offset_type, false);
			base = emit_index(base, zero, byte,
				lowir_model::IPK_BASE_SUBOBJECT);
		}
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
		if (model_.bit_field_fact(member_id) == NULL)
			return emit_index(base, member_offset, byte, lowir_model::IPK_FIELD);
		// A simple object member can replay from its canonical storage slot.  A
		// pointer/complex object has already been evaluated into base, so its
		// typed pointer value is the only safe root to replay.
		if (fact.token == SimpleTokenType::OP_DOT &&
			base_path.empty() &&
			(object_fact.kind == SemanticFactKind::IdExpression ||
				object_fact.kind == SemanticFactKind::Variable) &&
			object_fact.binding.valid() &&
			!reference_binding(object_fact.binding))
		{
			const LoweredValue object_storage = storage_for(object_fact.binding);
			return emit_bit_field_index(base, member_offset, byte,
				lowir_model::IPK_FIELD, member_id,
				BitFieldAddressProjection::ROOT_STORAGE_ADDRESS, object_storage);
		}
		return emit_bit_field_index(base, member_offset, byte,
			lowir_model::IPK_FIELD, member_id);
}

void Pa15Lowerer::materialize_lvalue_value(LoweredValue* result,
	const LowType& type, bool publish_bit_field_value)
{
	if (result == NULL || !result->lvalue) return;
	if (result->bit_field_lvalue)
	{
		LoweredValue value = emit_bit_field_load(*result,
			result->bit_field_binding, type);
		// Keep the ordinary value boundary explicit when requested by the
		// enclosing conversion.  Binary operands can consume the extracted
		// temporary directly because no lvalue identity remains there.
		if (publish_bit_field_value)
		{
			Instruction copy;
			copy.kind = Instruction::IK_COPY;
			copy.type = value.type;
			copy.first = value.value;
			const ValueId copied = destination(value.type, &copy);
			block().instructions.push_back(copy);
			value = LoweredValue(temporary_operand(copied,
				copy.destination_name_id), value.type, false);
		}
		*result = value;
		return;
	}
	const ValueId value = emit_load(*result, type);
	const Instruction& emitted = block().instructions.back();
	result->value = temporary_operand(value, emitted.destination_name_id);
	result->physical_type = type;
	result->lvalue = false;
}

bool Pa15Lowerer::apply_bit_field_reference_conversion(
	LoweredValue* result, const ConversionFact& conversion, const LowType& target)
{
	if (result == NULL || !result->bit_field_lvalue)
		throw std::runtime_error("PA15 bit-field reference result is invalid");
	const BitFieldFact* bit_field = model_.bit_field_fact(
		result->bit_field_binding);
	if (bit_field == NULL || !bit_field->named ||
		bit_field->binding != result->bit_field_binding ||
		!conversion.target.valid() || conversion.target.value >= model_.types_.size())
		throw std::runtime_error("PA15 bit-field reference fact is invalid");
	const TypeKind target_kind = model_.type_kind(conversion.target);
	if (target_kind != TypeKind::LvalueReference &&
		target_kind != TypeKind::RvalueReference)
		throw std::runtime_error("PA15 bit-field reference target is invalid");
	const TypeId referred_id = model_.types_[conversion.target.value].child;
	// A non-const reference cannot bind to a bit-field.  A const reference binds
	// to a value temporary, never to the packed unit.
	if ((model_.cv_qualifiers(referred_id) & 1u) == 0)
		throw std::runtime_error("PA15 non-const reference cannot bind to a bit-field");
	materialize_lvalue_value(result, result->type);
	const LowType referred = low_reference_value_type(conversion.target);
	if (result->physical_type != referred)
	{
		if (!result->physical_type.is_integer() || !referred.is_integer())
			throw std::runtime_error("PA15 bit-field reference temporary is not integral");
		Instruction instruction;
		instruction.kind = Instruction::IK_CONVERT;
		instruction.source_type = result->physical_type;
		instruction.first = result->value;
		instruction.conversion_operator = result->physical_type.integer_width() >
			referred.integer_width() ? lowir_model::COP_TRUNC :
			bit_field->is_signed ? lowir_model::COP_SEXT : lowir_model::COP_ZEXT;
		const ValueId value = destination(referred, &instruction);
		block().instructions.push_back(instruction);
		*result = LoweredValue(temporary_operand(value,
			instruction.destination_name_id), referred, false);
	}
	const LoweredValue temporary = generated_slot(referred, "refarg");
	emit_store(referred, result->value, temporary.value);
	*result = address_of_storage(temporary);
	result->type = target;
	result->physical_type = target;
	result->lvalue = false;
	return true;
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

bool Pa15Lowerer::class_value_conversion(SemanticFactId id, TypeId target,
	ConversionFact* conversion) const
{
	if (!id.valid() || id.value >= model_.semantic_facts_.size() ||
		!target.valid() || target.value >= model_.types_.size())
		return false;
	validate_conversion_range(id);
	const SemanticFact& fact = model_.semantic_facts_[id.value];
	bool found = false;
	for (std::size_t i = 0; i < fact.conversion_count; ++i)
	{
		const ConversionFact& candidate = model_.conversion_facts_[
			fact.conversion_begin + i];
		if (candidate.kind != ConversionKind::ClassValue)
			continue;
		if (found || candidate.target != target || !candidate.source.valid() ||
			candidate.source.value >= model_.types_.size() ||
			!model_.class_value_transfer_type(candidate.target) ||
			!model_.class_value_type(candidate.source) ||
			model_.strip_cv_type(model_.expression_object_type(candidate.source)) !=
			model_.strip_cv_type(model_.expression_object_type(candidate.target)))
			throw std::runtime_error("PA15 class-value conversion boundary is invalid");
		found = true;
		if (conversion != NULL)
			*conversion = candidate;
	}
	return found;
}

void Pa15Lowerer::emit_copy_object(TypeId type, LoweredValue source,
	LoweredValue destination)
{
	const LowType object = low_type(type);
	if (!object.is_object() || object.storage_size() == 0 ||
		object.storage_alignment() == 0)
		throw std::runtime_error("PA15 class copy object shape is invalid");
	if (source.lvalue && source.type.is_object())
		source = address_of_storage(source);
	if (destination.lvalue && destination.type.is_object())
		destination = address_of_storage(destination);
	if ((!source.type.is_pointer() && !source.type.is_object()) ||
		!destination.type.is_pointer())
		throw std::runtime_error("PA15 class copy object address is invalid");
	if (source.type.is_object() && source.type != object)
		throw std::runtime_error("PA15 class copy source shape is invalid");
	Instruction instruction;
	instruction.kind = Instruction::IK_COPYOBJ;
	instruction.byte_count = object.storage_size();
	instruction.byte_alignment = object.storage_alignment();
	instruction.first = source.value;
	instruction.second = destination.value;
	block().instructions.push_back(instruction);
}

LoweredValue Pa15Lowerer::mark_bit_field_address(
	const LoweredValue& address, BindingId binding_id,
	BitFieldAddressProjectionId projection) const
{
	const BitFieldFact* fact = model_.bit_field_fact(binding_id);
	if (fact == NULL || !fact->named || fact->binding != binding_id ||
		!fact->operation_type.valid() || !address.type.is_pointer())
		throw std::runtime_error("PA15 bit-field address fact is invalid");
	// PA12 publishes the converted bit-field operation type.  PA15 only
	// consumes that typed fact; it must not reconstruct language promotion from
	// a width or a physical storage rank.
	const LowType value_type = low_type(fact->operation_type);
	if (!value_type.is_integer())
		throw std::runtime_error("PA15 bit-field operation type is not integral");
	LoweredValue result(address.value, value_type, true);
	result.bit_field_lvalue = true;
	result.bit_field_binding = binding_id;
	if (projection.valid())
	{
		if (projection.value >= bit_field_address_projections_.size())
			throw std::runtime_error("PA15 bit-field projection handle is invalid");
		result.bit_field_address_projection = projection;
	}
	else
		result.bit_field_address_projection = address.bit_field_address_projection;
	return result;
}

BitFieldAddressProjectionId
Pa15Lowerer::capture_bit_field_address_projection(
	BitFieldAddressProjection::RootKind root_kind,
	const LoweredValue& root, const LoweredValue& offset,
	const LowType& element, lowir_model::IndexProjectionKind projection)
{
	if (root_kind == BitFieldAddressProjection::ROOT_STORAGE_ADDRESS)
	{
		if (!root.lvalue || !root.type.is_object() ||
			(root.value.kind != Operand::OP_SLOT &&
				root.value.kind != Operand::OP_GLOBAL))
			throw std::runtime_error("PA15 bit-field storage root is invalid");
	}
	else if (root_kind == BitFieldAddressProjection::ROOT_POINTER_LOAD ||
		root_kind == BitFieldAddressProjection::ROOT_POINTER_VALUE)
	{
		if (!root.type.is_pointer())
			throw std::runtime_error("PA15 bit-field pointer root is invalid");
	}
	else
		throw std::runtime_error("PA15 bit-field projection root kind is invalid");
	if (!offset.type.is_integer() || !element.valid())
		throw std::runtime_error("PA15 bit-field projection index is invalid");
	BitFieldAddressProjection descriptor;
	descriptor.root_kind = root_kind;
	descriptor.root = root.value;
	descriptor.root_type = root.type;
	descriptor.offset = offset.value;
	descriptor.offset_type = offset.type;
	descriptor.element_type = element;
	descriptor.index_projection = projection;
	bit_field_address_projections_.push_back(descriptor);
	return BitFieldAddressProjectionId(
		bit_field_address_projections_.size() - 1);
}

LoweredValue Pa15Lowerer::emit_bit_field_index(const LoweredValue& base,
	const LoweredValue& offset, const LowType& element,
	lowir_model::IndexProjectionKind projection, BindingId binding)
{
	return emit_bit_field_index(base, offset, element, projection, binding,
		BitFieldAddressProjection::ROOT_POINTER_VALUE, base);
}

LoweredValue Pa15Lowerer::emit_bit_field_index(const LoweredValue& base,
	const LoweredValue& offset, const LowType& element,
	lowir_model::IndexProjectionKind projection, BindingId binding,
	BitFieldAddressProjection::RootKind root_kind,
	const LoweredValue& root)
{
	if (model_.bit_field_fact(binding) == NULL)
		throw std::runtime_error("PA15 bit-field projection binding is invalid");
	const LoweredValue address = emit_index(base, offset, element, projection);
	const BitFieldAddressProjectionId projection_id =
		capture_bit_field_address_projection(root_kind, root, offset, element,
			projection);
	return mark_bit_field_address(address, binding, projection_id);
}

LoweredValue Pa15Lowerer::reproject_bit_field_address(
	const LoweredValue& storage)
{
	const BitFieldAddressProjectionId projection_id =
		storage.bit_field_address_projection;
	if (!projection_id.valid())
		return storage;
	if (projection_id.value >= bit_field_address_projections_.size())
		throw std::runtime_error("PA15 bit-field projection handle is invalid");
	const BitFieldAddressProjection& projection =
		bit_field_address_projections_[projection_id.value];
	LoweredValue base;
	if (projection.root_kind ==
		BitFieldAddressProjection::ROOT_STORAGE_ADDRESS)
	{
		base = address_of_storage(LoweredValue(projection.root,
			projection.root_type, true));
	}
	else if (projection.root_kind ==
		BitFieldAddressProjection::ROOT_POINTER_LOAD)
	{
		const LoweredValue root(projection.root, projection.root_type, true);
		const ValueId loaded = emit_load(root, projection.root_type);
		const Instruction& instruction = block().instructions.back();
		base = LoweredValue(temporary_operand(loaded,
			instruction.destination_name_id), projection.root_type, false);
	}
	else
		base = LoweredValue(projection.root, projection.root_type, false);
	if (!base.type.is_pointer())
		throw std::runtime_error("PA15 bit-field projection base is not a pointer");
	const LoweredValue offset(projection.offset, projection.offset_type, false);
	const LoweredValue projected = emit_index(base, offset,
		projection.element_type, projection.index_projection);
	return mark_bit_field_address(projected, storage.bit_field_binding,
		projection_id);
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
	const LoweredValue& value, bool force_storage_type,
	BitFieldEncodingOrder order)
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
	const LoweredValue mask(integer_operand(static_cast<long long>(
		fact->value_mask), write_type), write_type, false);
	LoweredValue encoded = order == BitFieldEncodingOrder::ValueFirst ?
		emit_binary_value(lowir_model::BOP_AND, write_type, source, mask) :
		emit_binary_value(lowir_model::BOP_AND, write_type, mask, source);
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
		// The packed-unit store is a later LowIR action than value computation.
		// Replay the saved typed root/index at that boundary so address evaluation
		// remains single-shot while the final destination action is explicit.
		const LoweredValue store_storage = reproject_bit_field_address(storage);
		emit_store(write_type, combined.value, store_storage.value);
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
