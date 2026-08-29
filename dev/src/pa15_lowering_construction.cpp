#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

LoweredValue Pa15Lowerer::lower_address(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const std::vector<SemanticFactId> facts = children(id);
		switch (fact.kind)
		{
		case SemanticFactKind::Literal:
		{
			if (fact.literal_element_count == 0 ||
				!fact.constant_address.valid())
				break;
			SymbolId target;
			long long addend = 0;
			if (!map_constant_address(id, &target, &addend, NULL) || addend != 0)
				break;
			LowType pointer;
			pointer.kind = LowType::TYPE_POINTER;
			const LoweredValue storage = LoweredValue(global_operand(target,
				symbol_name_for(target)), pointer, true);
			return address_of_storage(storage);
		}
		case SemanticFactKind::IdExpression:
		case SemanticFactKind::Variable:
			if (reference_binding(fact.binding) &&
				model_.callable_function_type(model_.binding(fact.binding).type).valid())
				return lower_lvalue(id);
			if (model_.binding(fact.binding).kind == BindingKind::Function)
			{
				const std::map<std::size_t, SymbolId>::const_iterator found =
					function_symbols_.find(fact.binding.value);
				if (found == function_symbols_.end())
					throw std::runtime_error("PA15 function address has no symbol");
				demand_function_declaration(fact.binding);
				LowType pointer;
				pointer.kind = LowType::TYPE_POINTER;
				return address_of_storage(LoweredValue(global_operand(found->second,
					function_name_ids_.find(fact.binding.value)->second), pointer, false));
			}
			return address_of_storage(lower_lvalue(id));
		case SemanticFactKind::MemberExpression:
			if (model_.bit_field_fact(fact.selected_binding) != NULL)
				throw std::runtime_error("PA15 address-of bit-field is not allowed");
		{
			const LoweredValue member_address = lower_member_address(id);
			if (!reference_binding(fact.selected_binding))
				return member_address;
			const ValueId referent = emit_load(member_address, member_address.type);
			const Instruction& load = block().instructions.back();
			return LoweredValue(temporary_operand(referent,
				load.destination_name_id), member_address.type, false);
		}
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
			const SemanticFact& sequence_fact =
				model_.semantic_facts_[facts.front().value];
			const TypeId sequence_object = model_.strip_cv_type(
				model_.expression_object_type(sequence_fact.type));
			const bool literal_array = sequence_fact.kind ==
				SemanticFactKind::Literal && sequence_fact.literal_element_count != 0 &&
				sequence_object.valid() && model_.type_kind(sequence_object) ==
				TypeKind::Array;
			// PA12's literal fact owns the decoded bytes and the constant-address
			// relation.  Consume that address directly for an array subscript so
			// the backing global is materialized once; no source-text decoding or
			// rendered-name reconstruction is needed here.
			const LoweredValue sequence = literal_array ?
				lower_address(facts.front()) : lower_expression(facts.front());
			const LoweredValue index = lower_expression(facts.back());
			TypeId sequence_type = model_.expression_object_type(sequence_fact.type);
			sequence_type = model_.strip_cv_type(sequence_type);
			const bool array = sequence_type.valid() &&
				model_.type_kind(sequence_type) == TypeKind::Array;
			if (array)
			{
				const TypeId child = model_.types_[sequence_type.value].child;
				const TypeId child_object = model_.strip_cv_type(
					model_.expression_object_type(child));
				const bool byte_projection = (child_object.valid() &&
					model_.type_kind(child_object) == TypeKind::Array) ||
					class_object_type(child);
				if (!byte_projection)
					return emit_index(sequence, index, low_type(child),
						lowir_model::IPK_ARRAY_ELEMENT);
				const LowType element = array_element_instruction_type(child);
				const std::size_t stride = model_.type_size(child);
				LoweredValue offset = index;
				if (stride != 1)
				{
					if (stride > static_cast<std::size_t>(
						std::numeric_limits<long long>::max()))
						throw std::runtime_error(
							"PA15 subscript element stride is invalid");
					const TypeId index_type = model_.semantic_facts_[
						facts.back().value].type;
					offset = emit_binary_value(lowir_model::BOP_MUL,
						size_low_type(), integer_i64(index, index_type),
						LoweredValue(integer_operand(static_cast<long long>(stride),
							size_low_type()), size_low_type(), false));
				}
				return emit_index(sequence, offset, element,
					lowir_model::IPK_ARRAY_ELEMENT);
			}
			if (sequence_type.valid() && sequence_type.value < model_.types_.size() &&
				model_.type_kind(sequence_type) == TypeKind::Pointer)
			{
				const TypeId child = model_.types_[sequence_type.value].child;
				const TypeId child_object = model_.strip_cv_type(
					model_.expression_object_type(child));
				const bool byte_projection = (child_object.valid() &&
					model_.type_kind(child_object) == TypeKind::Array) ||
					class_object_type(child);
				if (byte_projection)
				{
					const std::size_t stride = model_.type_size(child);
					LoweredValue offset = index;
					if (stride != 1)
					{
						if (stride > static_cast<std::size_t>(
							std::numeric_limits<long long>::max()))
							throw std::runtime_error(
								"PA15 pointer subscript element stride is invalid");
						const TypeId index_type = model_.semantic_facts_[
							facts.back().value].type;
						offset = emit_binary_value(lowir_model::BOP_MUL,
								size_low_type(), integer_i64(index, index_type),
								LoweredValue(integer_operand(static_cast<long long>(stride),
									size_low_type()), size_low_type(), false));
					}
					return emit_index(sequence, offset,
						array_element_instruction_type(child),
						lowir_model::IPK_ARRAY_ELEMENT);
				}
			}
			const LowType element = low_type(fact.type);
			return emit_index(sequence, index, element, lowir_model::IPK_NONE);
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
		case SemanticFactKind::ConstructorAction:
			if (fact.temporary_object)
				return address_of_storage(lower_constructor_expression(id));
			break;
		case SemanticFactKind::DestructorCall:
			throw std::runtime_error("PA15 destructor call has no address");
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

void Pa15Lowerer::initialize_array(BindingId binding, SemanticFactId initializer,
		const LoweredValue& storage){
	if (!binding.valid() || binding.value >= model_.bindings_.size() ||
		model_.binding(binding).kind != BindingKind::Variable)
		throw std::runtime_error("PA15 array initializer storage is invalid");
	const TypeId array_type = model_.strip_cv_type(
		model_.binding(binding).type);
	if (!array_type.valid() || model_.type_kind(array_type) != TypeKind::Array)
		throw std::runtime_error("PA15 array initializer target is not an array");
	const TypeId element = model_.types_[array_type.value].child;
	const TypeId element_object = model_.strip_cv_type(
		model_.expression_object_type(element));
	if (!element_object.valid() || element_object.value >= model_.types_.size())
		throw std::runtime_error("PA15 array initializer element is invalid");
	// Aggregate facts own the typed clause edges.  Scalar arrays retain the
	// established compact store/address sequence while deriving omitted values
	// from the destination type and sparse range.
	if (model_.type_kind(element_object) != TypeKind::Array &&
		!class_object_type(element))
	{
		std::size_t element_count = 0;
		std::size_t total_count = 0;
		const AggregateElementFact* elements = aggregate_elements(initializer,
			&element_count, &total_count);
		const TypeKey& array = model_.types_[array_type.value];
		const LowType element_type = low_type(element);
		const std::size_t element_size = element_type.storage_size();
		const std::size_t max_offset = static_cast<std::size_t>(
			std::numeric_limits<long long>::max());
		if (array.unknown_bound || total_count != array.bound.value ||
			element_size == 0 || element_size > max_offset ||
			array.bound.value > max_offset)
			throw std::runtime_error("PA15 scalar array initializer range is invalid");
		const LoweredValue base = address_of_storage(storage);
		std::size_t next_element = 0;
		for (std::size_t i = 0; i < array.bound.value; ++i)
		{
			if (i > max_offset / element_size)
				throw std::runtime_error("PA15 scalar array initializer offset is invalid");
			LoweredValue destination = base;
			if (i != 0)
			{
				LowType i64;
				i64.kind = LowType::TYPE_INTEGER;
				i64.integer_kind = LowType::INTEGER_I64;
				const LoweredValue offset(integer_operand(
					static_cast<long long>(i) * static_cast<long long>(element_size),
					i64), i64, false);
				LowType byte;
				byte.kind = LowType::TYPE_INTEGER;
				byte.integer_kind = LowType::INTEGER_I8;
				destination = emit_index(base, offset, byte,
					lowir_model::IPK_NONE);
			}
			if (next_element < element_count && elements[next_element].index == i)
			{
				const SemanticFactId value_id = elements[next_element++].initializer;
				if (!value_id.valid() || value_id.value >= model_.semantic_facts_.size())
					throw std::runtime_error(
						"PA15 scalar array initializer fact is invalid");
				const Operand value = lower_expression(value_id).value;
				emit_store(element_type, value, destination.value);
			}
			else
				emit_store(element_type, integer_operand(0, element_type),
					destination.value);
		}
		if (next_element != element_count)
			throw std::runtime_error("PA15 scalar array sparse index is invalid");
		return;
	}
	const LoweredValue address = address_of_storage(storage);
	const std::vector<ConstructorAddressStep> empty_path;
	initialize_constructor_value(model_.binding(binding).type, initializer,
		address, NULL, &empty_path, NULL, &storage,
		model_.binding(binding).type);
}

LoweredValue Pa15Lowerer::lower_variable_expression(SemanticFactId id)
{
	const SemanticFact& fact = model_.semantic_facts_[id.value];
	const std::vector<SemanticFactId> initializer = children(id);
	if (initializer.size() > 1)
		throw std::runtime_error("PA15 invalid condition initializer");
	const LoweredValue storage = storage_for(fact.binding);
	if (initializer.size() == 1)
	{
		const SemanticFact& initializer_fact =
			model_.semantic_facts_[initializer.front().value];
		if (initializer_fact.kind == SemanticFactKind::ConstructorAction)
		{
			const TypeId declared_type = model_.binding(fact.binding).type;
			if (initializer_fact.value_initialize)
				initialize_constructor_value(declared_type, initializer.front(),
					address_of_storage(storage));
			else if (!constructor_action_is_noop(initializer_fact))
			{
				const std::vector<SemanticFactId> action_facts =
					children(initializer.front());
				if (action_facts.size() == 1)
					(void)lower_expression(action_facts.front());
				else if (initializer_fact.has_callee &&
					initializer_fact.selected_binding.valid())
					emit_constructor_call(initializer_fact.selected_binding,
						address_of_storage(storage), InvalidIdentityValue, 0);
				else
					throw std::runtime_error(
						"PA15 local constructor action is incomplete");
			}
		}
		else if (storage.type.is_object() &&
			initializer_fact.kind == SemanticFactKind::BracedInitList)
		{
			const TypeId declared_type = model_.binding(fact.binding).type;
			const TypeId object_type = model_.strip_cv_type(
				model_.expression_object_type(declared_type));
			if (model_.type_kind(object_type) == TypeKind::Array)
				initialize_array(fact.binding, initializer.front(), storage);
			else
			{
				const LoweredValue address = address_of_storage(storage);
				const std::vector<ConstructorAddressStep> empty_path;
				initialize_constructor_value(declared_type, initializer.front(),
					address, NULL, &empty_path, NULL, &storage, declared_type);
			}
		}
		else
		{
			const LoweredValue value = lower_expression(initializer.front());
			emit_store(storage.type, value.value, storage.value);
		}
	}
	return storage;
}

LoweredValue Pa15Lowerer::constructor_subobject_address(
	const ConstructorActionFact& action)
{
	if (!active_constructor_record_.valid() ||
		!active_constructor_this_.valid() ||
		active_constructor_record_.value >= model_.named_.size() ||
		active_constructor_record_.value >= model_.record_layouts_.size())
		throw std::runtime_error("PA15 constructor has no active object");
	const NamedRecord& active_record = model_.named_[
		active_constructor_record_.value];
	if (active_record.kind != NamedKind::Class || !active_record.scope.valid() ||
		active_record.scope.value >= model_.scopes_.size() ||
		model_.scopes_[active_record.scope.value].kind != ScopeKind::Class ||
		model_.scopes_[active_record.scope.value].record !=
			active_constructor_record_)
		throw std::runtime_error("PA15 constructor owner is invalid");
	const LoweredValue this_storage = storage_for(active_constructor_this_);
	const ValueId this_value = emit_load(this_storage, this_storage.type);
	const Instruction& this_load = block().instructions.back();
	const LoweredValue object(temporary_operand(this_value,
		this_load.destination_name_id), this_storage.type, false);
	const LowType offset_type = size_low_type();
	LowType byte;
	byte.kind = LowType::TYPE_INTEGER;
	byte.integer_kind = LowType::INTEGER_I8;
	if (action.target == ConstructorActionTarget::Base)
	{
		const NamedRecord& record = model_.named_[
			active_constructor_record_.value];
		if (!record.has_base || record.direct_base_virtual ||
			record.direct_base != action.base_record ||
			!action.base_record.valid() || action.base_record.value >=
			model_.named_.size())
			throw std::runtime_error("PA15 constructor base action is invalid");
		const RecordLayout& layout = model_.record_layout(
			active_constructor_record_);
		if (layout.state != RecordLayoutState::Complete ||
			!layout.has_direct_base || layout.direct_base.record !=
			action.base_record || layout.direct_base.offset != 0)
			throw std::runtime_error("PA15 constructor base layout is invalid");
		return emit_index(object, LoweredValue(integer_operand(0, offset_type),
			offset_type, false), byte, lowir_model::IPK_BASE_SUBOBJECT);
	}
	if (!action.member.valid() || action.member.value >= model_.bindings_.size())
		throw std::runtime_error("PA15 constructor member action is invalid");
	if (action.member.value >= model_.binding_owners_.size() ||
		model_.binding_owners_[action.member.value] != active_record.scope)
		throw std::runtime_error("PA15 constructor member owner is invalid");
	if (model_.binding(action.member).kind != BindingKind::Variable ||
		model_.is_static_member(action.member))
		throw std::runtime_error("PA15 constructor member is not direct");
	const RecordLayout& layout = model_.record_layout(
		active_constructor_record_);
	if (layout.state != RecordLayoutState::Complete)
		throw std::runtime_error("PA15 constructor member layout is invalid");
	const std::size_t* offset = layout.member_offsets.find(action.member);
	if (offset == NULL || *offset > static_cast<std::size_t>(
		std::numeric_limits<long long>::max()))
		throw std::runtime_error("PA15 constructor member offset is invalid");
	const LoweredValue address = emit_index(object, LoweredValue(integer_operand(
		static_cast<long long>(*offset), offset_type), offset_type, false),
		byte, lowir_model::IPK_FIELD);
	return model_.bit_field_fact(action.member) != NULL ?
		mark_bit_field_address(address, action.member) : address;
}

std::size_t Pa15Lowerer::checked_array_element_offset(TypeId array,
	std::size_t index) const
{
	array = model_.strip_cv_type(model_.expression_object_type(array));
	if (!array.valid() || array.value >= model_.types_.size() ||
		model_.type_kind(array) != TypeKind::Array)
		throw std::runtime_error("PA15 array element type is invalid");
	const TypeKey& key = model_.types_[array.value];
	if (key.unknown_bound || index >= key.bound.value)
		throw std::runtime_error("PA15 array element index is out of bounds");
	const std::size_t stride = model_.type_size(key.child);
	const std::size_t max_offset = static_cast<std::size_t>(
		std::numeric_limits<long long>::max());
	if (stride == 0 || stride > max_offset || index > max_offset / stride)
		throw std::runtime_error("PA15 array element stride overflows offset");
	return index * stride;
}

LowType Pa15Lowerer::array_element_instruction_type(TypeId element) const
{
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(element));
	bool byte_projection = false;
	if (object.valid() && object.value < model_.types_.size())
	{
		const TypeKind kind = model_.type_kind(object);
		byte_projection = kind == TypeKind::Array;
		if (kind == TypeKind::Named)
		{
			const NamedRecordId record = model_.named_record_for_type(object);
			byte_projection = record.valid() && record.value < model_.named_.size() &&
				model_.named_[record.value].kind == NamedKind::Class;
		}
		if (byte_projection)
		{
			LowType byte;
			byte.kind = LowType::TYPE_INTEGER;
			byte.integer_kind = LowType::INTEGER_I8;
			return byte;
		}
	}
	return low_type(element);
}

LoweredValue Pa15Lowerer::emit_array_element_offset(TypeId array,
	std::size_t index)
{
	array = model_.strip_cv_type(model_.expression_object_type(array));
	if (!array.valid() || array.value >= model_.types_.size() ||
		model_.type_kind(array) != TypeKind::Array)
		throw std::runtime_error("PA15 array element offset type is invalid");
	const TypeId child = model_.types_[array.value].child;
	(void)checked_array_element_offset(array, index);
	const std::size_t stride = model_.type_size(child);
	const LowType integer = size_low_type();
	if (stride == 1)
		return LoweredValue(integer_operand(static_cast<long long>(index),
			integer), integer, false);
	return emit_binary_value(lowir_model::BOP_MUL, integer,
		LoweredValue(integer_operand(static_cast<long long>(index), integer),
			integer, false),
		LoweredValue(integer_operand(static_cast<long long>(stride), integer),
			integer, false));
}

LoweredValue Pa15Lowerer::constructor_path_address(
	const ConstructorActionFact& action,
	const std::vector<ConstructorAddressStep>& path)
{
	LoweredValue result = constructor_subobject_address(action);
	TypeId current_type = action.target == ConstructorActionTarget::Base ?
		model_.named_type(action.base_record) : model_.binding(action.member).type;
	const LowType offset_type = size_low_type();
	LowType byte;
	byte.kind = LowType::TYPE_INTEGER;
	byte.integer_kind = LowType::INTEGER_I8;
	for (std::size_t i = 0; i < path.size(); ++i)
	{
		const TypeId object = model_.strip_cv_type(
			model_.expression_object_type(current_type));
		if (!object.valid() || object.value >= model_.types_.size())
			throw std::runtime_error("PA15 constructor path type is invalid");
		if (path[i].array_element)
		{
			if (model_.type_kind(object) != TypeKind::Array ||
				model_.types_[object.value].unknown_bound ||
				path[i].index >= model_.types_[object.value].bound.value)
				throw std::runtime_error("PA15 constructor path array is invalid");
			(void)checked_array_element_offset(object, path[i].index);
			const TypeId child = model_.types_[object.value].child;
			const TypeId child_object = model_.strip_cv_type(
				model_.expression_object_type(child));
			const bool byte_projection = (child_object.valid() &&
				model_.type_kind(child_object) == TypeKind::Array) ||
				class_object_type(child);
			const LoweredValue array_offset = byte_projection ?
				emit_array_element_offset(object, path[i].index) :
				LoweredValue(integer_operand(static_cast<long long>(path[i].index),
					size_low_type()), size_low_type(), false);
			const LowType element_type = array_element_instruction_type(child);
			result = emit_index(emit_decay(result),
				array_offset, element_type,
				lowir_model::IPK_ARRAY_ELEMENT);
			current_type = child;
			continue;
		}
		if (model_.type_kind(object) != TypeKind::Named ||
			!path[i].member.valid() || path[i].member.value >=
			model_.bindings_.size())
			throw std::runtime_error("PA15 constructor path member is invalid");
		const NamedRecordId record = model_.named_record_for_type(object);
		if (!record.valid() || record.value >= model_.named_.size() ||
			model_.named_[record.value].kind != NamedKind::Class)
			throw std::runtime_error("PA15 constructor path class is invalid");
		const RecordLayout& layout = model_.record_layout(record);
		if (layout.state != RecordLayoutState::Complete)
			throw std::runtime_error("PA15 constructor path layout is invalid");
		if (path[i].member.value >= model_.binding_owners_.size() ||
			model_.binding_owners_[path[i].member.value] !=
				model_.named_[record.value].scope ||
			model_.binding(path[i].member).kind != BindingKind::Variable ||
			model_.is_static_member(path[i].member))
			throw std::runtime_error("PA15 constructor path member owner is invalid");
		const std::size_t* offset = layout.member_offsets.find(path[i].member);
		if (offset == NULL || *offset > static_cast<std::size_t>(
			std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 constructor path member offset is invalid");
		result = emit_index(result, LoweredValue(integer_operand(
			static_cast<long long>(*offset), offset_type), offset_type, false),
			byte, lowir_model::IPK_FIELD);
		if (model_.bit_field_fact(path[i].member) != NULL)
			result = mark_bit_field_address(result, path[i].member);
		current_type = model_.binding(path[i].member).type;
	}
	return result;
}

LoweredValue Pa15Lowerer::aggregate_path_address(const LoweredValue& storage,
	TypeId root_type, const std::vector<ConstructorAddressStep>& path)
{
	LoweredValue result = address_of_storage(storage);
	TypeId current_type = root_type;
	const LowType offset_type = size_low_type();
	LowType byte;
	byte.kind = LowType::TYPE_INTEGER;
	byte.integer_kind = LowType::INTEGER_I8;
	for (std::size_t i = 0; i < path.size(); ++i)
	{
		const TypeId object = model_.strip_cv_type(
			model_.expression_object_type(current_type));
		if (!object.valid() || object.value >= model_.types_.size())
			throw std::runtime_error("PA15 aggregate path type is invalid");
		if (path[i].array_element)
		{
			if (model_.type_kind(object) != TypeKind::Array ||
				model_.types_[object.value].unknown_bound ||
				path[i].index >= model_.types_[object.value].bound.value)
				throw std::runtime_error("PA15 aggregate path array is invalid");
			(void)checked_array_element_offset(object, path[i].index);
			const TypeId child = model_.types_[object.value].child;
			const TypeId child_object = model_.strip_cv_type(
				model_.expression_object_type(child));
			const bool byte_projection = (child_object.valid() &&
				model_.type_kind(child_object) == TypeKind::Array) ||
				class_object_type(child);
			const LoweredValue sequence = emit_decay(result);
			const LoweredValue array_offset = byte_projection ?
				emit_array_element_offset(object, path[i].index) :
				LoweredValue(integer_operand(static_cast<long long>(path[i].index),
					size_low_type()), size_low_type(), false);
			result = emit_index(sequence, array_offset,
				array_element_instruction_type(child),
				lowir_model::IPK_ARRAY_ELEMENT);
			current_type = child;
			continue;
		}
		if (model_.type_kind(object) != TypeKind::Named ||
			!path[i].member.valid() || path[i].member.value >=
			model_.bindings_.size())
			throw std::runtime_error("PA15 aggregate path member is invalid");
		const NamedRecordId record = model_.named_record_for_type(object);
		if (!record.valid() || record.value >= model_.named_.size() ||
			model_.named_[record.value].kind != NamedKind::Class)
			throw std::runtime_error("PA15 aggregate path class is invalid");
		const RecordLayout& layout = model_.record_layout(record);
		if (layout.state != RecordLayoutState::Complete ||
			path[i].member.value >= model_.binding_owners_.size() ||
			model_.binding_owners_[path[i].member.value] !=
				model_.named_[record.value].scope ||
			model_.binding(path[i].member).kind != BindingKind::Variable ||
			model_.is_static_member(path[i].member))
			throw std::runtime_error("PA15 aggregate path member owner is invalid");
		const std::size_t* offset = layout.member_offsets.find(path[i].member);
		if (offset == NULL || *offset > static_cast<std::size_t>(
			std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 aggregate path member offset is invalid");
		result = emit_index(result, LoweredValue(integer_operand(
			static_cast<long long>(*offset), offset_type), offset_type, false),
			byte, lowir_model::IPK_FIELD);
		current_type = model_.binding(path[i].member).type;
	}
	return result;
}

void Pa15Lowerer::initialize_global_aggregate_constructor(TypeId target,
	SemanticFactId initializer, const std::vector<ConstructorAddressStep>& path,
	BitFieldInitializationContext& context,
	const LoweredValue& aggregate_root_storage, TypeId aggregate_root_type)
{
	if (!aggregate_root_storage.lvalue ||
		!aggregate_root_storage.type.is_object())
		throw std::runtime_error("PA15 global aggregate root is not storage");
	if (!initializer.valid() || initializer.value >= model_.semantic_facts_.size())
		throw std::runtime_error("PA15 global aggregate constructor fact is invalid");
	const SemanticFact& fact = model_.semantic_facts_[initializer.value];
	if (!global_aggregate_constructor_inline_eligible(fact))
		throw std::runtime_error("PA15 global aggregate constructor is not inlineable");
	if (!aggregate_root_type.valid() || aggregate_root_type.value >=
		model_.types_.size() || low_type(aggregate_root_type) !=
		aggregate_root_storage.type)
		throw std::runtime_error("PA15 global aggregate root type is invalid");
	const BindingSidecar* binding_sidecar =
		model_.binding_sidecar(fact.selected_binding);
	if (binding_sidecar == NULL || !binding_sidecar->constructor_record.valid())
		throw std::runtime_error("PA15 global aggregate constructor owner is invalid");
	const NamedRecordId record_id = binding_sidecar->constructor_record;
	const TypeId target_object = model_.strip_cv_type(
		model_.expression_object_type(target));
	if (!target_object.valid() || target_object.value >= model_.types_.size() ||
		model_.type_kind(target_object) != TypeKind::Named ||
		model_.named_record_for_type(target_object) != record_id)
		throw std::runtime_error("PA15 global aggregate constructor target is invalid");
	const FunctionFact& constructor = checked_constructor_function(
		fact.selected_binding, record_id);
	const RecordLayout& layout = model_.record_layout(record_id);
	const std::vector<SemanticFactId> arguments = children(initializer);
	const TypeKey& signature = model_.types_[
		model_.binding(fact.selected_binding).type.value];
	if (signature.variadic || arguments.size() != signature.parameters.size())
		throw std::runtime_error("PA15 global aggregate constructor arity is invalid");
	if (constructor.constructor_action_begin == InvalidIdentityValue ||
		constructor.constructor_action_begin > model_.constructor_actions_.size() ||
		constructor.constructor_action_count != layout.members.size() ||
		constructor.constructor_action_count > model_.constructor_actions_.size() -
		constructor.constructor_action_begin)
		throw std::runtime_error("PA15 global aggregate constructor range is invalid");
	for (std::size_t i = 0; i < constructor.constructor_action_count; ++i)
	{
		const ConstructorActionFact& action = model_.constructor_actions_[
			constructor.constructor_action_begin + i];
		if (action.target != ConstructorActionTarget::Member ||
			action.member != layout.members[i].binding || action.constructor.valid() ||
			!action.initializer.valid() || action.argument_count != 0)
			throw std::runtime_error("PA15 global aggregate constructor action is invalid");
		const BindingId member = action.member;
		const ScopeId owner = model_.named_[record_id.value].scope;
		if (!member.valid() || member.value >= model_.bindings_.size() ||
			member.value >= model_.binding_owners_.size() ||
			model_.binding_owners_[member.value] != owner ||
			model_.binding(member).kind != BindingKind::Variable ||
			model_.is_static_member(member) ||
			action.object_type != model_.binding(member).type ||
			signature.parameters[i] != model_.binding(member).type)
			throw std::runtime_error("PA15 global aggregate constructor member is invalid");
		SemanticFactId resolved;
		if (!resolve_constructor_parameter(action.initializer, &constructor,
			arguments, &resolved) || !resolved.valid() || resolved.value >=
			model_.semantic_facts_.size())
			throw std::runtime_error("PA15 global aggregate constructor argument is invalid");
		const TypeId member_type = model_.binding(action.member).type;
		const SemanticFact& resolved_fact = model_.semantic_facts_[resolved.value];
		const bool direct_scalar = resolved_fact.kind !=
			SemanticFactKind::BracedInitList && resolved_fact.kind !=
			SemanticFactKind::ConstructorAction;
		if (!direct_scalar)
			throw std::runtime_error("PA15 global aggregate constructor child is unsupported");
		const LoweredValue value = lower_expression(resolved);
		std::vector<ConstructorAddressStep> member_path = path;
		member_path.push_back(ConstructorAddressStep(action.member));
		LoweredValue destination = aggregate_path_address(aggregate_root_storage,
			aggregate_root_type, member_path);
		if (model_.bit_field_fact(action.member) != NULL)
			destination = mark_bit_field_address(destination, action.member);
		if (destination.bit_field_lvalue)
			initialize_bit_field(destination, action.member, value, context);
		else
			emit_store(low_type(member_type), value.value, destination.value);
	}
}

void Pa15Lowerer::zero_initialize_value_initialized_object(TypeId target,
	const LoweredValue& destination)
{
	if (!destination.type.is_pointer())
		throw std::runtime_error("PA15 value-initialization target is not addressable");
	const LowType target_type = low_type(target);
	if (!target_type.valid())
		throw std::runtime_error("PA15 value-initialization type is invalid");
	if (target_type.is_object())
	{
		const std::size_t bytes = target_type.object_bytes;
		const std::size_t alignment = target_type.object_alignment;
		if (bytes == 0 || alignment == 0 || bytes > static_cast<std::size_t>(
			std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 value-initialization object size is invalid");
		LowType byte;
		byte.kind = LowType::TYPE_INTEGER;
		byte.integer_kind = LowType::INTEGER_I8;
		std::size_t offset = 0;
		while (offset < bytes)
		{
			const std::size_t remaining = bytes - offset;
			std::size_t width;
			LowType zero;
			zero.kind = LowType::TYPE_INTEGER;
			if (remaining >= 8 && alignment % 8 == 0 && offset % 8 == 0)
			{
				width = 8;
				zero.integer_kind = LowType::INTEGER_I64;
			}
			else if (remaining >= 4 && alignment % 4 == 0 && offset % 4 == 0)
			{
				width = 4;
				zero.integer_kind = LowType::INTEGER_I32;
			}
			else if (remaining >= 2 && alignment % 2 == 0 && offset % 2 == 0)
			{
				width = 2;
				zero.integer_kind = LowType::INTEGER_I16;
			}
			else
			{
				width = 1;
				zero.integer_kind = LowType::INTEGER_I8;
			}
			LoweredValue storage = destination;
			if (offset != 0)
				storage = emit_index(destination, LoweredValue(integer_operand(
					static_cast<long long>(offset), size_low_type()), size_low_type(),
					false), byte, lowir_model::IPK_NONE);
			emit_store(zero, integer_operand(0, zero), storage.value);
			offset += width;
		}
		return;
	}
	if (target_type.is_float())
	{
		emit_store(target_type, floating_operand(0.0L, target_type),
			destination.value);
		return;
	}
	if (target_type.is_integer() || target_type.is_pointer())
	{
		emit_store(target_type, integer_operand(0, target_type),
			destination.value);
		return;
	}
	throw std::runtime_error("PA15 value-initialization target is unsupported");
}

void Pa15Lowerer::index_lifetime_facts()
{
		lifetime_by_binding_.clear();
		// Build the scope-indexed ownership flags once.  Each lifetime walks its
		// validated ancestry at indexing time; lower_function then performs one
		// dense O(1) lookup instead of rescanning all lifetime facts.
		lifetime_function_scope_flags_.assign(model_.scopes_.size(), 0);
		for (std::size_t i = 0; i < model_.lifetime_facts_.size(); ++i)
		{
			const LifetimeFact& lifetime = model_.lifetime_facts_[i];
			const NamedRecordId record = lifetime.object_type.valid() &&
				lifetime.object_type.value < model_.types_.size() ?
				model_.class_record_for_object_type(lifetime.object_type) :
				NamedRecordId();
			if (!lifetime.object.valid() || lifetime.object.value >= model_.bindings_.size() ||
				lifetime.object.value >= model_.binding_owners_.size() ||
				!lifetime.object_type.valid() ||
				lifetime.object_type.value >= model_.types_.size() ||
				!lifetime.destructor.valid() ||
				lifetime.destructor.value >= model_.bindings_.size() ||
				!lifetime.scope.valid() || lifetime.scope.value >= model_.scopes_.size() ||
				model_.binding_owners_[lifetime.object.value] != lifetime.scope ||
				model_.binding(lifetime.object).kind != BindingKind::Variable ||
				model_.binding(lifetime.object).type != lifetime.object_type ||
				!record.valid() || record.value >= model_.named_.size() ||
				model_.named_[record.value].kind != NamedKind::Class)
				throw std::runtime_error("PA15 lifetime fact identity is invalid");
			if (model_.destructor_binding(record) != lifetime.destructor)
				throw std::runtime_error("PA15 lifetime destructor identity is invalid");
			(void)checked_destructor_function(lifetime.destructor, record);
			if (lifetime.storage != LifetimeStorageKind::Automatic)
			{
				const Scope& owner = model_.scopes_[lifetime.scope.value];
				if ((owner.kind != ScopeKind::Namespace &&
					owner.kind != ScopeKind::Class) ||
					(owner.kind == ScopeKind::Class &&
						!model_.is_static_member(lifetime.object)) ||
					(owner.kind == ScopeKind::Namespace &&
						model_.is_static_member(lifetime.object)))
					throw std::runtime_error(
						"PA15 namespace lifetime scope is invalid");
				if (lifetime_by_binding_.find(lifetime.object.value) !=
					lifetime_by_binding_.end())
					throw std::runtime_error("PA15 duplicate lifetime fact identity");
				lifetime_by_binding_[lifetime.object.value] = &lifetime;
				continue;
			}
			ScopeId function_scope;
			ScopeId scope = lifetime.scope;
			for (std::size_t depth = 0; depth < model_.scopes_.size(); ++depth)
			{
				if (!scope.valid() || scope.value >= model_.scopes_.size())
					throw std::runtime_error("PA15 lifetime scope ancestry is invalid");
				const Scope& current = model_.scopes_[scope.value];
				if (current.kind == ScopeKind::Function)
				{
					if (!current.parent.valid() || current.parent.value >=
						model_.scopes_.size() || current.parent == scope)
						throw std::runtime_error(
							"PA15 lifetime function scope owner is invalid");
					function_scope = scope;
					break;
				}
				scope = current.parent;
			}
			if (!function_scope.valid())
				throw std::runtime_error("PA15 lifetime scope ancestry is cyclic");
			lifetime_function_scope_flags_[function_scope.value] = 1;
			if (lifetime_by_binding_.find(lifetime.object.value) !=
				lifetime_by_binding_.end())
				throw std::runtime_error("PA15 duplicate lifetime fact identity");
			lifetime_by_binding_[lifetime.object.value] = &lifetime;
		}
	}

const FunctionFact& Pa15Lowerer::checked_constructor_function(
	BindingId constructor, NamedRecordId record) const
{
	if (!record.valid() || record.value >= model_.named_.size() ||
		model_.named_[record.value].kind != NamedKind::Class ||
		model_.named_[record.value].class_tag == ClassTag::Union)
		throw std::runtime_error("PA15 constructor fact record is invalid");
	const NamedRecord& named = model_.named_[record.value];
	if (!named.scope.valid() || named.scope.value >= model_.scopes_.size() ||
		model_.scopes_[named.scope.value].kind != ScopeKind::Class ||
		model_.scopes_[named.scope.value].record != record)
		throw std::runtime_error("PA15 constructor fact owner is invalid");
	if (!constructor.valid() || constructor.value >= model_.bindings_.size() ||
		constructor.value >= model_.binding_owners_.size() ||
		model_.binding_owners_[constructor.value] != named.scope)
		throw std::runtime_error("PA15 constructor fact binding owner is invalid");
	const Binding& binding = model_.binding(constructor);
	if (binding.kind != BindingKind::Function ||
		model_.type_kind(binding.type) != TypeKind::Function)
		throw std::runtime_error("PA15 constructor fact binding is invalid");
	const BindingSidecar* binding_sidecar = model_.binding_sidecar(constructor);
	if (binding_sidecar == NULL || binding_sidecar->constructor_record != record)
		throw std::runtime_error("PA15 constructor fact record owner is invalid");
	const FunctionFactId* function_id =
		model_.function_binding_fact_index_.find(constructor);
	if (function_id == NULL || !function_id->valid() ||
		function_id->value >= model_.function_facts_.size())
		throw std::runtime_error("PA15 constructor fact is missing");
	const FunctionFact& function = model_.function_facts_[function_id->value];
	if (!function.is_constructor || function.binding != constructor ||
		function.owner != named.scope || function.constructor_record != record ||
		!function.function_scope.valid() ||
		function.function_scope.value >= model_.scopes_.size() ||
		model_.scopes_[function.function_scope.value].kind != ScopeKind::Function ||
		model_.scopes_[function.function_scope.value].parent != function.owner)
		throw std::runtime_error("PA15 constructor fact identity is invalid");
	const BindingId this_binding =
		model_.scopes_[function.function_scope.value].implicit_object_binding;
	if (!this_binding.valid() || this_binding.value >= model_.bindings_.size() ||
		this_binding.value >= model_.binding_owners_.size() ||
		model_.binding_owners_[this_binding.value] != function.function_scope ||
		model_.binding(this_binding).kind != BindingKind::Parameter)
		throw std::runtime_error("PA15 constructor fact object parameter is invalid");
	if (function.constructor_action_begin == InvalidIdentityValue ||
		function.constructor_action_begin > model_.constructor_actions_.size() ||
		function.constructor_action_count > model_.constructor_actions_.size() -
		function.constructor_action_begin)
		throw std::runtime_error("PA15 constructor fact action range is invalid");
	return function;
}

const FunctionFact& Pa15Lowerer::checked_destructor_function(
	BindingId destructor, NamedRecordId record) const
{
	if (!record.valid() || record.value >= model_.named_.size() ||
		model_.named_[record.value].kind != NamedKind::Class ||
		model_.named_[record.value].class_tag == ClassTag::Union)
		throw std::runtime_error("PA15 destructor fact record is invalid");
	const NamedRecord& named = model_.named_[record.value];
	if (!named.scope.valid() || named.scope.value >= model_.scopes_.size() ||
		model_.scopes_[named.scope.value].kind != ScopeKind::Class ||
		model_.scopes_[named.scope.value].record != record)
		throw std::runtime_error("PA15 destructor fact owner is invalid");
	if (!destructor.valid() || destructor.value >= model_.bindings_.size() ||
		destructor.value >= model_.binding_owners_.size() ||
		model_.binding_owners_[destructor.value] != named.scope)
		throw std::runtime_error("PA15 destructor fact binding owner is invalid");
	const Binding& binding = model_.binding(destructor);
	if (binding.kind != BindingKind::Function ||
		model_.type_kind(binding.type) != TypeKind::Function)
		throw std::runtime_error("PA15 destructor fact binding is invalid");
	const BindingSidecar* sidecar = model_.binding_sidecar(destructor);
	if (sidecar == NULL || sidecar->destructor_record != record)
		throw std::runtime_error("PA15 destructor fact record owner is invalid");
	const FunctionFactId* function_id =
		model_.function_binding_fact_index_.find(destructor);
	if (function_id == NULL || !function_id->valid() ||
		function_id->value >= model_.function_facts_.size())
		throw std::runtime_error("PA15 destructor fact is missing");
	const FunctionFact& function = model_.function_facts_[function_id->value];
	if (!function.is_destructor || function.binding != destructor ||
		function.owner != named.scope || function.destructor_record != record ||
		!function.function_scope.valid() ||
		function.function_scope.value >= model_.scopes_.size() ||
		model_.scopes_[function.function_scope.value].kind != ScopeKind::Function ||
		model_.scopes_[function.function_scope.value].parent != function.owner)
		throw std::runtime_error("PA15 destructor fact identity is invalid");
	const BindingId this_binding = model_.scopes_[function.function_scope.value].
		implicit_object_binding;
	if (!this_binding.valid() || this_binding.value >= model_.bindings_.size() ||
		this_binding.value >= model_.binding_owners_.size() ||
		model_.binding_owners_[this_binding.value] != function.function_scope ||
		model_.binding(this_binding).kind != BindingKind::Parameter)
		throw std::runtime_error("PA15 destructor fact object parameter is invalid");
	if (function.destructor_action_begin == InvalidIdentityValue ||
		function.destructor_action_begin > model_.destructor_actions_.size() ||
		function.destructor_action_count > model_.destructor_actions_.size() -
		function.destructor_action_begin)
		throw std::runtime_error("PA15 destructor fact action range is invalid");
	return function;
}

LoweredValue Pa15Lowerer::destructor_subobject_address(
	const DestructorActionFact& action)
{
	if (!active_destructor_record_.valid() || !active_destructor_this_.valid() ||
		active_destructor_record_.value >= model_.named_.size())
		throw std::runtime_error("PA15 destructor has no active object");
	const NamedRecord& active = model_.named_[active_destructor_record_.value];
	if (active.kind != NamedKind::Class || !active.scope.valid() ||
		active.scope.value >= model_.scopes_.size() ||
		model_.scopes_[active.scope.value].kind != ScopeKind::Class ||
		model_.scopes_[active.scope.value].record != active_destructor_record_)
		throw std::runtime_error("PA15 destructor owner is invalid");
	const LoweredValue this_storage = storage_for(active_destructor_this_);
	const ValueId this_value = emit_load(this_storage, this_storage.type);
	const Instruction& this_load = block().instructions.back();
	const LoweredValue object(temporary_operand(this_value,
		this_load.destination_name_id), this_storage.type, false);
	const LowType offset_type = size_low_type();
	LowType byte;
	byte.kind = LowType::TYPE_INTEGER;
	byte.integer_kind = LowType::INTEGER_I8;
	if (action.target == ConstructorActionTarget::Base)
	{
		if (!active.has_base || active.direct_base_virtual ||
			active.direct_base != action.base_record ||
			!action.base_record.valid())
			throw std::runtime_error("PA15 destructor base action is invalid");
		const RecordLayout& layout = model_.record_layout(active_destructor_record_);
		if (layout.state != RecordLayoutState::Complete || !layout.has_direct_base ||
			layout.direct_base.record != action.base_record ||
			layout.direct_base.offset != 0)
			throw std::runtime_error("PA15 destructor base layout is invalid");
		return emit_index(object, LoweredValue(integer_operand(0, offset_type),
			offset_type, false), byte, lowir_model::IPK_BASE_SUBOBJECT);
	}
	if (action.target != ConstructorActionTarget::Member ||
		!action.member.valid() || action.member.value >= model_.bindings_.size() ||
		action.member.value >= model_.binding_owners_.size() ||
		model_.binding_owners_[action.member.value] != active.scope ||
		model_.binding(action.member).kind != BindingKind::Variable ||
		model_.is_static_member(action.member))
		throw std::runtime_error("PA15 destructor member action is invalid");
	const RecordLayout& layout = model_.record_layout(active_destructor_record_);
	if (layout.state != RecordLayoutState::Complete)
		throw std::runtime_error("PA15 destructor member layout is invalid");
	const std::size_t* offset = layout.member_offsets.find(action.member);
	if (offset == NULL || *offset > static_cast<std::size_t>(
		std::numeric_limits<long long>::max()))
		throw std::runtime_error("PA15 destructor member offset is invalid");
	return emit_index(object, LoweredValue(integer_operand(
		static_cast<long long>(*offset), offset_type), offset_type, false), byte,
		lowir_model::IPK_FIELD);
}

LoweredValue Pa15Lowerer::recompute_constructed_element_address(
	const ConstructedElement& element)
{
	if (!element.root.type.valid())
		throw std::runtime_error("PA15 completed element root type is invalid");
	LoweredValue result;
	TypeId current_type = element.root.type;
	if (element.root.action_based)
	{
		if (!element.root.owner.valid() ||
			element.root.owner != active_constructor_record_)
			throw std::runtime_error("PA15 completed element constructor owner is invalid");
		result = constructor_subobject_address(element.root.action);
	}
	else
	{
		if (!element.root.storage.valid() ||
			element.root.storage.value >= model_.bindings_.size() ||
			model_.binding(element.root.storage).kind != BindingKind::Variable)
			throw std::runtime_error("PA15 completed element storage root is invalid");
		result = address_of_storage(storage_for(element.root.storage));
	}
	for (std::size_t i = 0; i < element.path.size(); ++i)
	{
		if (!element.path[i].array_element)
			throw std::runtime_error("PA15 completed element path is not an array path");
		const TypeId object = model_.strip_cv_type(
			model_.expression_object_type(current_type));
			if (!object.valid() || object.value >= model_.types_.size() ||
			model_.type_kind(object) != TypeKind::Array)
			throw std::runtime_error("PA15 completed element array path is invalid");
		const TypeId child = model_.types_[object.value].child;
		const LoweredValue array_offset = emit_array_element_offset(object,
			element.path[i].index);
		result = emit_index(emit_decay(result), array_offset,
			array_element_instruction_type(child),
			lowir_model::IPK_ARRAY_ELEMENT);
		current_type = child;
	}
	return result;
}

void Pa15Lowerer::emit_destructor_call(BindingId destructor,
	const LoweredValue& destination_value)
{
	if (!destructor.valid() || destructor.value >= model_.bindings_.size() ||
		model_.binding(destructor).kind != BindingKind::Function ||
		model_.type_kind(model_.binding(destructor).type) != TypeKind::Function)
		throw std::runtime_error("PA15 destructor call target is invalid");
	const std::map<std::size_t, SymbolId>::const_iterator symbol =
		function_symbols_.find(destructor.value);
	const std::map<std::size_t, SpellingId>::const_iterator name =
		function_name_ids_.find(destructor.value);
	if (symbol == function_symbols_.end() || name == function_name_ids_.end())
		throw std::runtime_error("PA15 destructor call target was not emitted");
	const TypeKey& signature = model_.types_[model_.binding(destructor).type.value];
	if (signature.variadic || !signature.parameters.empty())
		throw std::runtime_error("PA15 destructor call signature is invalid");
	Instruction instruction;
	instruction.kind = Instruction::IK_CALL;
	instruction.first = global_operand(symbol->second, name->second);
	instruction.direct_callee_id = symbol->second;
	instruction.call_return_type.kind = LowType::TYPE_VOID;
	instruction.call_returns_void = true;
	instruction.call_boundary.arity = lowir_model::CAM_FIXED;
	instruction.args.push_back(destination_value.value);
	block().instructions.push_back(instruction);
}

LoweredValue Pa15Lowerer::lower_destructor_call(SemanticFactId id)
{
	const SemanticFact& fact = model_.semantic_facts_[id.value];
	const std::vector<SemanticFactId> facts = children(id);
	if (facts.size() != 1 || !model_.void_id(fact.type) ||
		(fact.token != SimpleTokenType::OP_DOT &&
			fact.token != SimpleTokenType::OP_ARROW))
		throw std::runtime_error("PA15 invalid destructor call fact");
	if (!fact.selected_binding.valid())
	{
		if (fact.has_callee || fact.has_implicit_object ||
			fact.callable_type.valid() || fact.selected_scope.valid() ||
			fact.base_path_begin != InvalidIdentityValue ||
			fact.base_path_count != 0)
			throw std::runtime_error("PA15 pseudo-destructor fact is not scalar");
		// A scalar pseudo-destructor has no runtime callee.  Its only semantic
		// effect is the one evaluation represented by the child fact.
		if (fact.token == SimpleTokenType::OP_ARROW)
			(void)lower_expression(facts.front());
		else
			lower_discarded_expression(facts.front());
		return LoweredValue(Operand(), low_type(fact.type), false);
	}
	if (!fact.has_callee || !fact.has_implicit_object ||
		!fact.selected_scope.valid() || fact.selected_scope.value >=
		model_.scopes_.size() || model_.scopes_[fact.selected_scope.value].kind !=
		ScopeKind::Class || !fact.callable_type.valid() ||
		model_.type_kind(fact.callable_type) != TypeKind::Function)
		throw std::runtime_error("PA15 destructor call target is incomplete");
	const NamedRecordId record = model_.scopes_[fact.selected_scope.value].record;
	const FunctionFact& function = checked_destructor_function(
		fact.selected_binding, record);
	const Binding& destructor = model_.binding(fact.selected_binding);
	const TypeKey& raw_signature = model_.types_[destructor.type.value];
	const TypeKey& callable_signature = model_.types_[fact.callable_type.value];
	if (raw_signature.variadic || !raw_signature.parameters.empty() ||
		raw_signature.result != fact.type || callable_signature.variadic ||
		callable_signature.parameters.size() != 1 ||
		callable_signature.result != fact.type ||
		function.owner != fact.selected_scope)
		throw std::runtime_error("PA15 destructor call signature is invalid");
	const TypeId hidden_pointer = model_.strip_cv_type(
		model_.expression_object_type(callable_signature.parameters.front()));
	if (!hidden_pointer.valid() || model_.type_kind(hidden_pointer) !=
		TypeKind::Pointer)
		throw std::runtime_error("PA15 destructor hidden object is not a pointer");
	const TypeId required_object = model_.types_[hidden_pointer.value].child;
	const SemanticFact& object_fact = model_.semantic_facts_[facts.front().value];
	TypeId actual_object;
	LoweredValue object;
	if (fact.token == SimpleTokenType::OP_ARROW)
	{
		const TypeId pointer = model_.strip_cv_type(
			model_.expression_object_type(object_fact.type));
		if (!pointer.valid() || model_.type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA15 destructor arrow object is not a pointer");
		actual_object = model_.types_[pointer.value].child;
		object = lower_expression(facts.front());
	}
	else
	{
		actual_object = model_.expression_object_type(object_fact.type);
		object = lower_address(facts.front());
	}
	if (!object.type.is_pointer())
		throw std::runtime_error("PA15 destructor object has no address");
	actual_object = model_.strip_cv_type(actual_object);
	const TypeId required_unqualified = model_.strip_cv_type(
		model_.expression_object_type(required_object));
	if (!validate_typed_base_path(actual_object, required_unqualified,
		fact.selected_scope, fact.base_path_begin, fact.base_path_count))
		throw std::runtime_error("PA15 destructor object is incompatible");
	NamedRecordId current_record = model_.named_record_for_type(actual_object);
	for (std::size_t i = 0; i < fact.base_path_count; ++i)
	{
		if (!current_record.valid() || current_record.value >= model_.named_.size())
			throw std::runtime_error("PA15 destructor base record is invalid");
		const NamedRecord& current = model_.named_[current_record.value];
		const NamedRecordId base_record = model_.semantic_base_paths_[
			fact.base_path_begin + i];
		if (current.kind != NamedKind::Class || !current.has_base ||
			current.direct_base_virtual || current.direct_base != base_record)
			throw std::runtime_error("PA15 destructor base relation is invalid");
		const RecordLayout& layout = model_.record_layout(current_record);
		if (layout.state != RecordLayoutState::Complete ||
			!layout.has_direct_base || layout.direct_base.record != base_record ||
			layout.direct_base.offset != 0)
			throw std::runtime_error("PA15 destructor base layout is invalid");
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
	if (current_record != record)
		throw std::runtime_error("PA15 destructor base path ended at wrong record");
	emit_destructor_call(fact.selected_binding, object);
	return LoweredValue(Operand(), low_type(fact.type), false);
}

void Pa15Lowerer::emit_destructor_elements(TypeId target,
	const LoweredValue& destination, BindingId destructor)
{
	if (!destination.type.is_pointer())
		throw std::runtime_error("PA15 array destructor target is not addressable");
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(target));
	if (!object.valid() || object.value >= model_.types_.size())
		throw std::runtime_error("PA15 array destructor target is invalid");
	if (model_.type_kind(object) == TypeKind::Array)
	{
		const TypeKey& array = model_.types_[object.value];
		if (array.unknown_bound || array.bound.value > static_cast<std::size_t>(
			std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 array destructor bound is invalid");
		const LoweredValue sequence = emit_decay(destination);
		for (std::size_t i = array.bound.value; i != 0; --i)
		{
			const std::size_t index = i - 1;
			const TypeId child = array.child;
			const LoweredValue element = emit_index(sequence,
				emit_array_element_offset(object, index),
				array_element_instruction_type(child),
				lowir_model::IPK_ARRAY_ELEMENT);
			emit_destructor_elements(child, element, destructor);
		}
		return;
	}
	const NamedRecordId record = model_.class_record_for_object_type(object);
	const BindingSidecar* sidecar = model_.binding_sidecar(destructor);
	if (!record.valid() || sidecar == NULL || sidecar->destructor_record != record)
		throw std::runtime_error("PA15 array destructor element owner is invalid");
	(void)checked_destructor_function(destructor, record);
	emit_destructor_call(destructor, destination);
}

void Pa15Lowerer::append_constructor_cleanup(ArrayCleanupChain* cleanup,
	const ConstructedElement& element)
{
	if (cleanup == NULL)
		throw std::runtime_error("PA15 constructor cleanup chain is missing");
	if (!element.destructor.valid()) return;
	const BlockId saved_current = current_block_id();
	if (!cleanup->base.valid())
	{
		cleanup->base = block_id(new_block("array_ctor_cleanup_base"));
		set_current(cleanup->base);
		Instruction resume;
		resume.kind = Instruction::IK_RESUME;
		block().instructions.push_back(resume);
	}
	const BlockId prior = cleanup->head.valid() ? cleanup->head : cleanup->base;
	const BlockId node = block_id(new_block("array_ctor_cleanup"));
	set_current(node);
	emit_destructor_call(element.destructor,
		recompute_constructed_element_address(element));
	emit_jump(prior);
	cleanup->head = node;
	if (saved_current.valid()) set_current(saved_current);
	else current_block_ = InvalidIdentityValue;
}

void Pa15Lowerer::materialize_constructor_cleanup(ArrayCleanupChain* cleanup,
	const std::vector<ConstructedElement>& completed)
{
	if (cleanup == NULL || cleanup->materialized > completed.size())
		throw std::runtime_error("PA15 constructor cleanup prefix is invalid");
	for (std::size_t i = cleanup->materialized; i < completed.size(); ++i)
	{
		append_constructor_cleanup(cleanup, completed[i]);
		++cleanup->materialized;
	}
}

void Pa15Lowerer::lower_destructor_action(const DestructorActionFact& action)
{
	if ((action.target == ConstructorActionTarget::Base &&
		(!action.base_record.valid() || action.base_record.value >=
			model_.named_.size() || action.member.valid())) ||
		(action.target == ConstructorActionTarget::Member &&
		(!action.member.valid() || action.base_record.valid())) ||
		!action.destructor.valid() || action.destructor.value >=
			model_.bindings_.size() || !action.object_type.valid() ||
			action.object_type.value >= model_.types_.size())
		throw std::runtime_error("PA15 destructor action identity is invalid");
	const LoweredValue destination = destructor_subobject_address(action);
	emit_destructor_elements(action.object_type, destination, action.destructor);
}

void Pa15Lowerer::emit_active_destructor_actions()
{
	if (!active_destructor_record_.valid()) return;
	const BindingId destructor = model_.destructor_binding(
		active_destructor_record_);
	const FunctionFact& function = checked_destructor_function(destructor,
		active_destructor_record_);
	for (std::size_t action = 0; action < function.destructor_action_count;
		++action)
		lower_destructor_action(model_.destructor_actions_[
			function.destructor_action_begin + action]);
}

void Pa15Lowerer::activate_lifetime(BindingId object)
{
	if (!object.valid() || object.value >= model_.bindings_.size())
		throw std::runtime_error("PA15 lifetime activation binding is invalid");
	const std::map<std::size_t, const LifetimeFact*>::const_iterator found =
		lifetime_by_binding_.find(object.value);
	if (found == lifetime_by_binding_.end()) return;
	if (found->second->object != object || !found->second->scope.valid())
		throw std::runtime_error("PA15 lifetime activation fact is invalid");
	if (std::find(active_lifetimes_.begin(), active_lifetimes_.end(), object) !=
		active_lifetimes_.end())
		throw std::runtime_error("PA15 lifetime activation is duplicated");
	active_lifetimes_.push_back(object);
}

void Pa15Lowerer::lower_scoped_statement(SemanticFactId id)
{
	if (!id.valid() || id.value >= model_.semantic_facts_.size())
		throw std::runtime_error("PA15 scoped statement identity is invalid");
	const SemanticFact& fact = model_.semantic_facts_[id.value];
	const ScopeId scope = fact.source == NULL ? ScopeId() :
		model_.substatement_scope(*fact.source);
	if (!scope.valid())
	{
		lower_statement(id);
		return;
	}
	if (scope.value >= model_.scopes_.size())
		throw std::runtime_error("PA15 scoped statement scope is invalid");
	const std::size_t depth = active_lifetimes_.size();
	lifetime_scope_stack_.push_back(scope);
	lifetime_scope_depths_.push_back(depth);
	lower_statement(id);
	if (current_block_ != InvalidIdentityValue && !terminated(block()))
		emit_scope_destructors(scope, depth);
	else
		restore_lifetime_depth(depth);
	lifetime_scope_stack_.pop_back();
	lifetime_scope_depths_.pop_back();
}

void Pa15Lowerer::emit_lifetime_destructors(std::size_t depth)
{
	if (depth > active_lifetimes_.size())
		throw std::runtime_error("PA15 lifetime stack depth is invalid");
	for (std::size_t i = active_lifetimes_.size(); i != depth; --i)
	{
		const BindingId object = active_lifetimes_[i - 1];
		const std::map<std::size_t, const LifetimeFact*>::const_iterator found =
			lifetime_by_binding_.find(object.value);
		if (found == lifetime_by_binding_.end() || found->second->object != object)
			throw std::runtime_error("PA15 active lifetime fact is missing");
		const LifetimeFact& lifetime = *found->second;
		const LoweredValue storage = storage_for(lifetime.object);
		emit_destructor_elements(lifetime.object_type,
			address_of_storage(storage), lifetime.destructor);
	}
	active_lifetimes_.resize(depth);
}

void Pa15Lowerer::restore_lifetime_depth(std::size_t depth)
{
	// A structural exit may already have popped farther than this lexical
	// marker.  Never resurrect an object on the continuation path.
	if (depth < active_lifetimes_.size())
		active_lifetimes_.resize(depth);
}

void Pa15Lowerer::emit_control_lifetime_destructors(bool continue_target)
{
	std::size_t depth = InvalidIdentityValue;
	const BlockId target = control_target(continue_target, &depth);
	if (depth == InvalidIdentityValue)
	{
		if (!active_lifetimes_.empty())
			throw std::runtime_error(
				"PA15 control exit crosses an unrepresentable lifetime");
		emit_jump(target);
		return;
	}
	emit_lifetime_destructors(depth);
	emit_jump(target);
}

void Pa15Lowerer::emit_scope_destructors(ScopeId scope, std::size_t depth)
{
	if (!scope.valid() || scope.value >= model_.scopes_.size() ||
		depth > active_lifetimes_.size() || lifetime_scope_stack_.empty() ||
		lifetime_scope_depths_.empty() || lifetime_scope_stack_.back() != scope ||
		lifetime_scope_depths_.back() != depth)
		throw std::runtime_error("PA15 lifetime scope is invalid");
	for (std::size_t i = active_lifetimes_.size(); i != depth; --i)
	{
		const BindingId object = active_lifetimes_[i - 1];
		const std::map<std::size_t, const LifetimeFact*>::const_iterator found =
			lifetime_by_binding_.find(object.value);
		if (found == lifetime_by_binding_.end() || found->second->scope != scope)
			throw std::runtime_error("PA15 lifetime scope stack is out of order");
		const LifetimeFact& lifetime = *found->second;
		const LoweredValue storage = storage_for(lifetime.object);
		emit_destructor_elements(lifetime.object_type,
			address_of_storage(storage), lifetime.destructor);
	}
	active_lifetimes_.resize(depth);
}

void Pa15Lowerer::emit_active_scope_destructors()
{
	emit_lifetime_destructors(0);
}

void Pa15Lowerer::emit_constructor_call(BindingId constructor,
	const LoweredValue& destination_value, std::size_t argument_begin,
	std::size_t argument_count)
{
	if (!constructor.valid() || constructor.value >= model_.bindings_.size() ||
		model_.binding(constructor).kind != BindingKind::Function ||
		model_.type_kind(model_.binding(constructor).type) != TypeKind::Function)
		throw std::runtime_error("PA15 constructor call target is invalid");
	const std::map<std::size_t, SymbolId>::const_iterator symbol =
		function_symbols_.find(constructor.value);
	const std::map<std::size_t, SpellingId>::const_iterator name =
		function_name_ids_.find(constructor.value);
	if (symbol == function_symbols_.end() || name == function_name_ids_.end())
		throw std::runtime_error("PA15 constructor call target was not emitted");
	const TypeKey& signature = model_.types_[
		model_.binding(constructor).type.value];
	if (argument_begin == InvalidIdentityValue && argument_count != 0)
		throw std::runtime_error("PA15 constructor argument range is invalid");
	if (argument_begin != InvalidIdentityValue &&
		(argument_begin > model_.constructor_arguments_.size() ||
		 argument_count > model_.constructor_arguments_.size() - argument_begin))
		throw std::runtime_error("PA15 constructor argument range is invalid");
	if (!signature.variadic && argument_count != signature.parameters.size())
		throw std::runtime_error("PA15 constructor call arity mismatch");
	if (signature.variadic && argument_count < signature.parameters.size())
		throw std::runtime_error("PA15 constructor call arity mismatch");
	Instruction instruction;
	instruction.kind = Instruction::IK_CALL;
	instruction.first = global_operand(symbol->second, name->second);
	instruction.direct_callee_id = symbol->second;
	instruction.call_return_type.kind = LowType::TYPE_VOID;
	instruction.call_returns_void = true;
	instruction.call_boundary.arity = signature.variadic ?
		lowir_model::CAM_VARIADIC : lowir_model::CAM_FIXED;
	instruction.args.push_back(destination_value.value);
	for (std::size_t i = 0; i < argument_count; ++i)
		instruction.args.push_back(lower_expression(
			model_.constructor_arguments_[argument_begin + i]).value);
	block().instructions.push_back(instruction);
}

void Pa15Lowerer::emit_constructor_call(BindingId constructor,
	const LoweredValue& destination_value,
	const std::vector<SemanticFactId>& arguments)
{
	if (!constructor.valid() || constructor.value >= model_.bindings_.size() ||
		model_.binding(constructor).kind != BindingKind::Function ||
		model_.type_kind(model_.binding(constructor).type) != TypeKind::Function)
		throw std::runtime_error("PA15 constructor call target is invalid");
	const std::map<std::size_t, SymbolId>::const_iterator symbol =
		function_symbols_.find(constructor.value);
	const std::map<std::size_t, SpellingId>::const_iterator name =
		function_name_ids_.find(constructor.value);
	if (symbol == function_symbols_.end() || name == function_name_ids_.end())
		throw std::runtime_error("PA15 constructor call target was not emitted");
	const TypeKey& signature = model_.types_[model_.binding(constructor).type.value];
	if ((!signature.variadic && arguments.size() != signature.parameters.size()) ||
		(signature.variadic && arguments.size() < signature.parameters.size()))
		throw std::runtime_error("PA15 constructor call arity mismatch");
	Instruction instruction;
	instruction.kind = Instruction::IK_CALL;
	instruction.first = global_operand(symbol->second, name->second);
	instruction.direct_callee_id = symbol->second;
	instruction.call_return_type.kind = LowType::TYPE_VOID;
	instruction.call_returns_void = true;
	instruction.call_boundary.arity = signature.variadic ?
		lowir_model::CAM_VARIADIC : lowir_model::CAM_FIXED;
	instruction.args.push_back(destination_value.value);
	for (std::size_t i = 0; i < arguments.size(); ++i)
		instruction.args.push_back(lower_expression(arguments[i]).value);
	block().instructions.push_back(instruction);
}

void Pa15Lowerer::emit_constructor_elements(TypeId target,
	const LoweredValue& destination, BindingId constructor,
	std::size_t argument_begin, std::size_t argument_count,
	const std::vector<SemanticFactId>* semantic_arguments,
	ArrayCleanupChain* cleanup, std::vector<ConstructedElement>* completed,
	bool value_initialize,
	const ArrayAddressRoot& root,
	const std::vector<ConstructorAddressStep>& path)
{
	if (!destination.type.is_pointer())
		throw std::runtime_error("PA15 array constructor target is not addressable");
	if (cleanup == NULL)
		throw std::runtime_error("PA15 array constructor cleanup chain is missing");
	std::vector<ConstructedElement> local_completed;
	if (completed == NULL)
	{
		local_completed.reserve(1);
		completed = &local_completed;
	}
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(target));
	if (!object.valid() || object.value >= model_.types_.size())
		throw std::runtime_error("PA15 array constructor target is invalid");
	if (model_.type_kind(object) == TypeKind::Array)
	{
		const TypeKey& array = model_.types_[object.value];
		if (array.unknown_bound || array.bound.value > static_cast<std::size_t>(
			std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 array constructor bound is invalid");
		const LoweredValue sequence = emit_decay(destination);
		for (std::size_t i = 0; i < array.bound.value; ++i)
		{
			const TypeId child = array.child;
			const LoweredValue element = emit_index(sequence,
				emit_array_element_offset(object, i),
				array_element_instruction_type(child),
				lowir_model::IPK_ARRAY_ELEMENT);
			std::vector<ConstructorAddressStep> element_path = path;
			element_path.push_back(ConstructorAddressStep(BindingId(), i, true));
			emit_constructor_elements(child, element, constructor,
				argument_begin, argument_count, semantic_arguments, cleanup,
				completed,
				value_initialize, root, element_path);
		}
		return;
	}
	const NamedRecordId record = model_.class_record_for_object_type(object);
	if (!record.valid() || record.value >= model_.named_.size() ||
		model_.named_[record.value].class_tag == ClassTag::Union)
		throw std::runtime_error("PA15 array constructor element is not a class");
	const FunctionFact& constructor_function =
		checked_constructor_function(constructor, record);
	if (value_initialize && constructor_function.synthetic)
		zero_initialize_value_initialized_object(object, destination);
	const bool no_op = constructor_function.synthetic &&
		constructor_function.constructor_action_count == 0;
	const BindingSidecar* constructor_sidecar =
		model_.binding_sidecar(constructor);
	const FunctionFactId* constructor_id =
		model_.function_binding_fact_index_.find(constructor);
	bool throwing = constructor_sidecar == NULL ||
		!constructor_sidecar->nonthrowing;
	if (constructor_function.synthetic && constructor_id != NULL &&
		constructor_is_nothrow(*constructor_id))
		throwing = false;
	bool emitted = false;
	if (!no_op && throwing && !completed->empty())
	{
		materialize_constructor_cleanup(cleanup, *completed);
	}
	if (!no_op && throwing && cleanup->head.valid())
	{
		const BlockId dispatch = cleanup->head;
		const BlockId continuation = block_id(new_block("array_ctor_continue"));
		Instruction eh;
		eh.kind = Instruction::IK_EH_TRY;
		eh.first = block_operand(dispatch);
		block().instructions.push_back(eh);
		if (semantic_arguments != NULL)
			emit_constructor_call(constructor, destination, *semantic_arguments);
		else
			emit_constructor_call(constructor, destination, argument_begin,
				argument_count);
		emitted = true;
		Instruction end;
		end.kind = Instruction::IK_EH_END;
		block().instructions.push_back(end);
		emit_jump(continuation);
		set_current(continuation);
	}
	if (!no_op && !emitted)
	{
		if (semantic_arguments != NULL)
			emit_constructor_call(constructor, destination, *semantic_arguments);
		else
			emit_constructor_call(constructor, destination, argument_begin,
				argument_count);
	}
	const ConstructedElement completed_element(root, path,
		model_.destructor_binding(record), record);
	completed->push_back(completed_element);
}

void Pa15Lowerer::zero_initialize_constructor_value(TypeId target,
	const LoweredValue& destination_value,
	const ConstructorActionFact* root_action,
	const std::vector<ConstructorAddressStep>* path,
	BitFieldInitializationContext* context,
	const LoweredValue* aggregate_root_storage,
	TypeId aggregate_root_type)
{
	BitFieldInitializationContext local_context;
	if (context == NULL)
		context = &local_context;
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(target));
	if (!object.valid() || object.value >= model_.types_.size())
		throw std::runtime_error("PA15 zero constructor target is invalid");
	const TypeKind kind = model_.type_kind(object);
	if (kind == TypeKind::Array)
	{
		const TypeKey& array = model_.types_[object.value];
		if (array.unknown_bound || array.bound.value > static_cast<std::size_t>(
			std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 zero constructor array is incomplete");
		const LoweredValue sequence = emit_decay(destination_value);
		const TypeId child_object = model_.strip_cv_type(
			model_.expression_object_type(array.child));
		const bool byte_projection = (child_object.valid() &&
			model_.type_kind(child_object) == TypeKind::Array) ||
			class_object_type(array.child);
		for (std::size_t i = 0; i < array.bound.value; ++i)
		{
			const LowType element_type = array_element_instruction_type(array.child);
			LoweredValue element;
			std::vector<ConstructorAddressStep> element_path;
			if (path != NULL)
			{
				element_path = *path;
				element_path.push_back(ConstructorAddressStep(BindingId(), i, true));
			}
			if (root_action != NULL && path != NULL && i != 0)
			{
				element = constructor_path_address(*root_action, element_path);
			}
			else if (aggregate_root_storage != NULL && path != NULL &&
				(path->empty() || i != 0))
				element = aggregate_path_address(*aggregate_root_storage,
					aggregate_root_type, element_path);
			else
				element = emit_index(sequence,
					byte_projection ? emit_array_element_offset(object, i) :
					LoweredValue(integer_operand(static_cast<long long>(i),
						size_low_type()), size_low_type(), false), element_type,
					lowir_model::IPK_ARRAY_ELEMENT);
			zero_initialize_constructor_value(array.child, element,
				root_action, path != NULL ? &element_path : NULL, NULL,
				aggregate_root_storage, aggregate_root_type);
		}
		return;
	}
	if (kind == TypeKind::Named)
	{
		const NamedRecordId record = model_.named_record_for_type(object);
		if (!record.valid() || record.value >= model_.named_.size() ||
			model_.named_[record.value].kind != NamedKind::Class ||
			model_.named_[record.value].class_tag == ClassTag::Union ||
			model_.named_[record.value].has_base)
			throw std::runtime_error("PA15 zero constructor class is unsupported");
		const ScopeId scope = model_.named_[record.value].scope;
		const RecordLayout& layout = model_.record_layout(record);
		if (!scope.valid() || scope.value >= model_.scopes_.size() ||
			model_.scopes_[scope.value].kind != ScopeKind::Class ||
			model_.scopes_[scope.value].record != record ||
			layout.state != RecordLayoutState::Complete)
			throw std::runtime_error("PA15 zero constructor class layout is invalid");
		LowType byte;
		byte.kind = LowType::TYPE_INTEGER;
		byte.integer_kind = LowType::INTEGER_I8;
		for (std::size_t i = 0; i < layout.members.size(); ++i)
		{
			const BindingId member = layout.members[i].binding;
			if (!member.valid() || member.value >= model_.bindings_.size() ||
				member.value >= model_.binding_owners_.size() ||
				model_.binding_owners_[member.value] != scope)
				throw std::runtime_error("PA15 zero constructor member identity is invalid");
			if (model_.binding(member).kind != BindingKind::Variable ||
				model_.is_static_member(member))
				continue;
			const std::size_t* offset = layout.member_offsets.find(member);
			if (offset == NULL || *offset > static_cast<std::size_t>(
				std::numeric_limits<long long>::max()))
				throw std::runtime_error("PA15 zero constructor member offset is invalid");
			std::vector<ConstructorAddressStep> member_path;
			if (path != NULL)
			{
				member_path = *path;
				member_path.push_back(ConstructorAddressStep(member));
			}
			LoweredValue member_value = root_action != NULL && path != NULL &&
				i != 0 ? constructor_path_address(*root_action, member_path) :
				aggregate_root_storage != NULL && path != NULL &&
				(path->empty() || i != 0) ?
				aggregate_path_address(*aggregate_root_storage,
					aggregate_root_type, member_path) :
				emit_index(destination_value, LoweredValue(integer_operand(
					static_cast<long long>(*offset), size_low_type()), size_low_type(),
					false), byte, lowir_model::IPK_FIELD);
			if (model_.bit_field_fact(member) != NULL)
				member_value = mark_bit_field_address(member_value, member);
			BitFieldInitializationContext* member_context =
				model_.bit_field_fact(member) != NULL ? context : NULL;
			const BindingSidecar* sidecar = model_.binding_sidecar(member);
			if (sidecar != NULL && sidecar->default_member_initializer.valid())
			{
				if (sidecar->default_member_initializer.value >=
					model_.semantic_facts_.size())
					throw std::runtime_error(
						"PA15 zero constructor default member initializer is invalid");
				initialize_constructor_value(model_.binding(member).type,
					sidecar->default_member_initializer, member_value,
					root_action, path != NULL ? &member_path : NULL,
					member_context, aggregate_root_storage, aggregate_root_type);
			}
			else
				zero_initialize_constructor_value(model_.binding(member).type,
					member_value, root_action, path != NULL ? &member_path : NULL,
					member_context, aggregate_root_storage, aggregate_root_type);
		}
		return;
	}
	const LowType value_type = low_type(target);
	if (destination_value.bit_field_lvalue)
	{
		initialize_bit_field(destination_value,
			destination_value.bit_field_binding,
			LoweredValue(integer_operand(0, value_type), value_type, false),
			*context);
		return;
	}
	if (value_type.is_float())
		emit_store(value_type, floating_operand(0.0L, value_type),
			destination_value.value);
	else if (value_type.is_integer() || value_type.is_pointer())
		emit_store(value_type, integer_operand(0, value_type),
			destination_value.value);
	else
		throw std::runtime_error("PA15 zero constructor scalar is unsupported");
}

void Pa15Lowerer::initialize_constructor_value(TypeId target,
	SemanticFactId initializer, const LoweredValue& destination_value,
	const ConstructorActionFact* root_action,
	const std::vector<ConstructorAddressStep>* path,
	BitFieldInitializationContext* context,
	const LoweredValue* aggregate_root_storage,
	TypeId aggregate_root_type)
{
	BitFieldInitializationContext local_context;
	if (context == NULL)
		context = &local_context;
	if (!initializer.valid() || initializer.value >= model_.semantic_facts_.size())
		throw std::runtime_error("PA15 constructor initializer is invalid");
	const SemanticFact& fact = model_.semantic_facts_[initializer.value];
	if (fact.kind == SemanticFactKind::ConstructorAction)
	{
		if (!fact.has_callee || !fact.selected_binding.valid())
			throw std::runtime_error("PA15 nested constructor initializer is unsupported");
		const TypeId object = model_.strip_cv_type(
			model_.expression_object_type(target));
		const NamedRecordId record = model_.named_record_for_type(object);
		if (!record.valid() || record.value >= model_.named_.size() ||
			model_.named_[record.value].kind != NamedKind::Class)
			throw std::runtime_error("PA15 constructor initializer record is invalid");
		const NamedRecord& named = model_.named_[record.value];
		if (named.class_tag == ClassTag::Union || !named.name.valid())
		{
			// Anonymous/union construction retains its pre-checkpoint legacy
			// representation.  Named non-union classes must use the checked
			// FunctionFact path below.
			const FunctionFact* legacy_constructor =
				model_.function_fact_for_binding(fact.selected_binding);
			if (fact.value_initialize && (legacy_constructor == NULL ||
				legacy_constructor->synthetic))
				zero_initialize_value_initialized_object(target, destination_value);
			emit_constructor_call(fact.selected_binding, destination_value,
				InvalidIdentityValue, 0);
			return;
		}
		if (!fact.selected_scope.valid() || fact.selected_scope.value >=
			model_.scopes_.size() || fact.selected_scope != named.scope ||
			!fact.callable_type.valid() || model_.type_kind(fact.callable_type) !=
			TypeKind::Function)
			throw std::runtime_error(
				"PA15 constructor initializer owner or type is invalid");
		const FunctionFact& constructor = checked_constructor_function(
			fact.selected_binding, record);
		const bool global_initializer = current_function_ != InvalidIdentityValue &&
			current_function_ < program_.functions.size() &&
			program_.functions[current_function_].metadata.role == lowir_model::SR_INIT;
		const NamedRecordSidecar* record_sidecar =
			model_.named_record_sidecar(record);
		if (global_initializer && aggregate_root_storage != NULL && path != NULL &&
			record_sidecar != NULL &&
			record_sidecar->aggregate_constructor_binding ==
			fact.selected_binding &&
			global_aggregate_constructor_inline_eligible(fact))
		{
			initialize_global_aggregate_constructor(target, initializer, *path,
				*context, *aggregate_root_storage, aggregate_root_type);
			return;
		}
		LoweredValue constructor_destination = destination_value;
		if (constructor_destination.lvalue &&
			constructor_destination.type.is_object())
			constructor_destination = address_of_storage(constructor_destination);
		if (fact.value_initialize && constructor.synthetic)
			zero_initialize_value_initialized_object(target, constructor_destination);
		if (constructor.synthetic && constructor.constructor_action_count == 0)
			return;
		emit_constructor_call(fact.selected_binding, constructor_destination,
			children(initializer));
		return;
	}
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(target));
	if (!object.valid() || object.value >= model_.types_.size())
		throw std::runtime_error("PA15 constructor target is invalid");
	if (fact.kind != SemanticFactKind::BracedInitList)
	{
		if (fact.kind == SemanticFactKind::Literal &&
			fact.literal_element_count != 0 &&
			model_.type_kind(object) == TypeKind::Array)
		{
			if (fact.source == NULL || fact.source->kind != PA10NodeKind::Literal ||
				fact.source->literal.type != FundamentalType::Char ||
				fact.source->literal.bytes.size() != fact.literal_element_count)
				throw std::runtime_error("PA15 string literal payload is invalid");
			const TypeKey& array = model_.types_[object.value];
			if (array.unknown_bound || fact.literal_element_count > array.bound.value)
				throw std::runtime_error("PA15 string literal does not fit array");
			const LowType element_type = low_type(array.child);
			if (element_type.storage_size() != 1)
				throw std::runtime_error("PA15 string literal element type is invalid");
			const LoweredValue sequence = emit_decay(destination_value);
			for (std::size_t i = 0; i < array.bound.value; ++i)
			{
				LoweredValue element;
				if (aggregate_root_storage != NULL && path != NULL &&
					(path->empty() || i != 0))
				{
					std::vector<ConstructorAddressStep> byte_path = *path;
					byte_path.push_back(ConstructorAddressStep(BindingId(), i, true));
					element = aggregate_path_address(*aggregate_root_storage,
						aggregate_root_type, byte_path);
				}
				else
					element = emit_index(sequence,
						LoweredValue(integer_operand(static_cast<long long>(i),
							size_low_type()), size_low_type(), false), element_type,
						lowir_model::IPK_ARRAY_ELEMENT);
				const long long value = i < fact.literal_element_count ?
					static_cast<long long>(fact.source->literal.bytes[i]) : 0;
				emit_store(element_type, integer_operand(value, element_type),
					element.value);
			}
			return;
		}
		const LoweredValue value = lower_expression(initializer);
		if (destination_value.bit_field_lvalue)
			initialize_bit_field(destination_value,
				destination_value.bit_field_binding, value, *context);
		else
			emit_store(low_type(target), value.value, destination_value.value);
		return;
	}
	if (fact.kind == SemanticFactKind::BracedInitList)
	{
		initialize_aggregate_value(target, initializer, destination_value,
			root_action, path, context, aggregate_root_storage,
			aggregate_root_type);
		return;
	}
}
void Pa15Lowerer::lower_constructor_action(
	const ConstructorActionFact& action,
	BitFieldInitializationContext& context)
{
	if (action.target != ConstructorActionTarget::Base &&
		action.target != ConstructorActionTarget::Member)
		throw std::runtime_error("PA15 constructor action target is invalid");
	if ((action.constructor.valid() && action.initializer.valid()) ||
		(!action.constructor.valid() && !action.initializer.valid()))
		throw std::runtime_error("PA15 constructor action operation is invalid");
	if (action.value_initialize && !action.constructor.valid())
		throw std::runtime_error("PA15 value-initialization action is invalid");
	if (action.constructor.valid())
	{
		const LoweredValue destination_value = constructor_subobject_address(action);
		const TypeId target = action.target == ConstructorActionTarget::Base ?
			(action.object_type.valid() ? action.object_type :
				model_.named_type(action.base_record)) :
			(action.object_type.valid() ? action.object_type :
				model_.binding(action.member).type);
		const TypeId object = model_.strip_cv_type(
			model_.expression_object_type(target));
		const NamedRecordId record = model_.class_record_for_object_type(object);
		if (!record.valid() || record.value >= model_.named_.size() ||
			model_.named_[record.value].kind != NamedKind::Class)
			throw std::runtime_error("PA15 constructor action record is invalid");
		if (model_.type_kind(object) == TypeKind::Array)
		{
			ArrayAddressRoot root;
			root.type = target;
			root.owner = active_constructor_record_;
			root.action = action;
			root.action_based = true;
			const std::vector<ConstructorAddressStep> empty_path;
			ArrayCleanupChain cleanup;
			emit_constructor_elements(target, destination_value,
				action.constructor, action.argument_begin, action.argument_count,
				NULL, &cleanup, NULL, action.value_initialize, root, empty_path);
			return;
		}
		const NamedRecord& named = model_.named_[record.value];
		if (named.class_tag == ClassTag::Union || !named.name.valid())
		{
			// Preserve the pre-checkpoint anonymous/union constructor path.
			const FunctionFact* legacy_constructor =
				model_.function_fact_for_binding(action.constructor);
			if (action.value_initialize && (legacy_constructor == NULL ||
				legacy_constructor->synthetic))
			{
				zero_initialize_value_initialized_object(target, destination_value);
			}
			emit_constructor_call(action.constructor, destination_value,
				action.argument_begin, action.argument_count);
			return;
		}
		const FunctionFact& constructor = checked_constructor_function(
			action.constructor, record);
		if (action.value_initialize && constructor.synthetic)
			zero_initialize_value_initialized_object(target, destination_value);
		if (constructor.synthetic && constructor.constructor_action_count == 0)
			return;
		emit_constructor_call(action.constructor, destination_value,
			action.argument_begin, action.argument_count);
		return;
	}
	if (action.initializer.valid())
	{
		if (action.initializer.value >= model_.semantic_facts_.size())
			throw std::runtime_error("PA15 constructor initializer is invalid");
		const TypeId target = action.target == ConstructorActionTarget::Base ?
			model_.named_type(action.base_record) : model_.binding(action.member).type;
		const SemanticFact& initializer = model_.semantic_facts_[
			action.initializer.value];
		// Evaluate scalar mem-initializers before publishing their destination
		// address.  This preserves the source-order LowIR boundary for a
		// constructor expression such as b(a + 3).
		if (initializer.kind != SemanticFactKind::BracedInitList &&
			initializer.kind != SemanticFactKind::ConstructorAction)
		{
			const LoweredValue value = lower_expression(action.initializer);
			const BitFieldFact* bit_field = model_.bit_field_fact(action.member);
			const bool encode_before_address = bit_field != NULL &&
				!bit_field_initialization_preserves_existing(action.member, context);
			LoweredValue encoded;
			if (encode_before_address)
				encoded = encode_bit_field_value(action.member, value, true);
			const LoweredValue destination_value =
				constructor_subobject_address(action);
			if (destination_value.bit_field_lvalue)
			{
				if (bit_field == NULL)
					throw std::runtime_error(
						"PA15 constructor bit-field identity is inconsistent");
				if (encode_before_address)
					initialize_encoded_bit_field(destination_value,
						destination_value.bit_field_binding, encoded, context);
				else
					initialize_bit_field(destination_value,
						destination_value.bit_field_binding, value, context);
			}
			else
				emit_store(low_type(target), value.value, destination_value.value);
			return;
		}
		const LoweredValue destination_value = constructor_subobject_address(action);
		const std::vector<ConstructorAddressStep> empty_path;
		initialize_constructor_value(target, action.initializer,
			destination_value, &action, &empty_path,
			model_.bit_field_fact(action.member) != NULL ? &context : NULL);
		return;
	}
	throw std::runtime_error("PA15 constructor action has no operation");
}

bool Pa15Lowerer::constructor_action_is_noop(const SemanticFact& action) const
{
	if (!action.selected_binding.valid())
		return false;
	const BindingSidecar* binding_sidecar =
		model_.binding_sidecar(action.selected_binding);
	if (binding_sidecar != NULL && binding_sidecar->constructor_record.valid() &&
		binding_sidecar->constructor_record.value < model_.named_.size())
	{
		const NamedRecord& named = model_.named_[
			binding_sidecar->constructor_record.value];
		if (named.kind == NamedKind::Class && named.class_tag != ClassTag::Union &&
			named.name.valid())
		{
			if (!action.selected_scope.valid() || action.selected_scope.value >=
				model_.scopes_.size() || action.selected_scope != named.scope)
				throw std::runtime_error(
					"PA15 constructor action selected owner is invalid");
			const FunctionFact& function = checked_constructor_function(
				action.selected_binding, binding_sidecar->constructor_record);
			return !action.value_initialize && function.synthetic &&
				function.constructor_action_count == 0;
		}
	}
	const FunctionFactId* function_id =
		model_.function_binding_fact_index_.find(action.selected_binding);
	if (function_id != NULL && function_id->valid() &&
		function_id->value < model_.function_facts_.size())
	{
		const FunctionFact& function = model_.function_facts_[function_id->value];
		if (!action.value_initialize && function.is_constructor && function.synthetic &&
			function.constructor_action_count == 0)
			return true;
	}
	if (binding_sidecar == NULL || !binding_sidecar->constructor_record.valid() ||
		binding_sidecar->constructor_record.value >= model_.named_.size())
		return false;
	const NamedRecord& record =
		model_.named_[binding_sidecar->constructor_record.value];
	if (record.kind != NamedKind::Class || record.class_tag == ClassTag::Union)
		return false;
	const NamedRecordSidecar* record_sidecar =
		model_.named_record_sidecar(binding_sidecar->constructor_record);
	return record_sidecar != NULL &&
		record_sidecar->constructor_binding == action.selected_binding &&
		!record_sidecar->default_constructor_binding.valid() &&
		record.scope.valid() && record.scope.value < model_.scopes_.size() &&
		model_.scopes_[record.scope.value].bindings.empty();
}

LoweredValue Pa15Lowerer::lower_constructor_expression(SemanticFactId id)
{
	const SemanticFact& action = model_.semantic_facts_[id.value];
	const std::vector<SemanticFactId> action_facts = children(id);
	if (action.temporary_object)
	{
		if (!action.has_callee || !action.selected_binding.valid() ||
			!action.callable_type.valid() || action_facts.size() != 1)
			throw std::runtime_error("PA15 temporary constructor fact is incomplete");
		const SemanticFact& call = model_.semantic_facts_[action_facts.front().value];
		if (call.kind != SemanticFactKind::CallExpression || !call.has_callee ||
			call.selected_binding != action.selected_binding ||
			call.callable_type != action.callable_type)
			throw std::runtime_error("PA15 temporary constructor call is invalid");
		const std::vector<SemanticFactId> constructor_arguments = children(
			action_facts.front());
		const char* storage_prefix = constructor_arguments.empty() ?
			"tmpobj" : "arg";
		const LoweredValue storage = generated_slot(low_type(action.type),
			storage_prefix);
		const LoweredValue address = address_of_storage(storage);
		emit_constructor_call(action.selected_binding, address,
			constructor_arguments);
		// Preserve the already-materialized address as an lvalue object result.
		// address_of_storage recognizes this typed temporary and does not emit a
		// second address operation when the object is immediately used as a
		// member-call receiver.
		return LoweredValue(address.value, low_type(action.type), true,
			address.physical_type);
	}
	if (action_facts.size() != 1)
		throw std::runtime_error("PA15 constructor expression has no call fact");
	const SemanticFact& call = model_.semantic_facts_[action_facts.front().value];
	if (call.kind == SemanticFactKind::CallExpression)
	{
		const std::vector<SemanticFactId> call_children =
			children(action_facts.front());
		if (!call_children.empty())
		{
			const SemanticFact& address =
				model_.semantic_facts_[call_children.front().value];
			if (address.kind == SemanticFactKind::UnaryExpression &&
				address.token == SimpleTokenType::OP_AMP)
			{
				const std::vector<SemanticFactId> address_children =
					children(call_children.front());
				if (address_children.size() == 1)
				{
					const SemanticFact& storage_fact =
						model_.semantic_facts_[address_children.front().value];
					if (storage_fact.binding.valid() &&
						storage_fact.binding.value < model_.bindings_.size())
					{
						const TypeId target = model_.binding(
							storage_fact.binding).type;
						const TypeId object = model_.strip_cv_type(
							model_.expression_object_type(target));
			if (object.valid() && object.value < model_.types_.size() &&
				model_.type_kind(object) == TypeKind::Array)
			{
							std::vector<SemanticFactId> arguments;
							for (std::size_t i = 1; i < call_children.size(); ++i)
								arguments.push_back(call_children[i]);
				ArrayAddressRoot root;
				root.type = target;
				root.storage = storage_fact.binding;
				const std::vector<ConstructorAddressStep> empty_path;
				ArrayCleanupChain cleanup;
				emit_constructor_elements(target,
					address_of_storage(storage_for(storage_fact.binding)),
				call.selected_binding, InvalidIdentityValue, 0,
					&arguments, &cleanup, NULL, false, root, empty_path);
							return storage_for(storage_fact.binding);
						}
					}
				}
			}
		}
	}
	return lower_expression(action_facts.front());
}

} // namespace pa11_semantic_internal
