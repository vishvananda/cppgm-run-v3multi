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
		case SemanticFactKind::MemberExpression: return lower_member_address(id);
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
			const LowType element = low_type(fact.type);
			return emit_index(sequence, index, element, array ? lowir_model::IPK_ARRAY_ELEMENT : lowir_model::IPK_NONE);
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
				destination_address = emit_index(base, offset, byte, lowir_model::IPK_NONE);
			}
			if (i < values.size() && model_.semantic_facts_[values[i].value].kind ==
				SemanticFactKind::BracedInitList)
				throw std::runtime_error("PA15 nested array initializer is outside checkpoint");
			const Operand value = i < values.size() ?
				lower_expression(values[i]).value : integer_operand(0, element_type);
			emit_store(element_type, value, destination_address.value);
		}
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
			if (!constructor_action_is_noop(initializer_fact))
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
				initialize_constructor_value(declared_type, initializer.front(),
					address_of_storage(storage));
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
		active_constructor_record_.value >= model_.named_.size())
		throw std::runtime_error("PA15 constructor has no active object");
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
	return emit_index(object, LoweredValue(integer_operand(
		static_cast<long long>(*offset), offset_type), offset_type, false),
		byte, lowir_model::IPK_FIELD);
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
		if (!object.valid())
			throw std::runtime_error("PA15 constructor path type is invalid");
		if (path[i].array_element)
		{
			if (model_.type_kind(object) != TypeKind::Array)
				throw std::runtime_error("PA15 constructor path array is invalid");
			result = emit_index(emit_decay(result),
				LoweredValue(integer_operand(static_cast<long long>(path[i].index),
					offset_type), offset_type, false), byte,
				lowir_model::IPK_ARRAY_ELEMENT);
			current_type = model_.types_[object.value].child;
			continue;
		}
		if (model_.type_kind(object) != TypeKind::Named ||
			!path[i].member.valid())
			throw std::runtime_error("PA15 constructor path member is invalid");
		const NamedRecordId record = model_.named_record_for_type(object);
		if (!record.valid() || record.value >= model_.named_.size() ||
			model_.named_[record.value].kind != NamedKind::Class)
			throw std::runtime_error("PA15 constructor path class is invalid");
		const RecordLayout& layout = model_.record_layout(record);
		if (layout.state != RecordLayoutState::Complete)
			throw std::runtime_error("PA15 constructor path layout is invalid");
		const std::size_t* offset = layout.member_offsets.find(path[i].member);
		if (offset == NULL || *offset > static_cast<std::size_t>(
			std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 constructor path member offset is invalid");
		result = emit_index(result, LoweredValue(integer_operand(
			static_cast<long long>(*offset), offset_type), offset_type, false),
			byte, lowir_model::IPK_FIELD);
		current_type = model_.binding(path[i].member).type;
	}
	return result;
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

void Pa15Lowerer::zero_initialize_constructor_value(TypeId target,
	const LoweredValue& destination_value,
	const ConstructorActionFact* root_action,
	const std::vector<ConstructorAddressStep>* path)
{
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(target));
	if (!object.valid())
		throw std::runtime_error("PA15 zero constructor target is invalid");
	const TypeKind kind = model_.type_kind(object);
	if (kind == TypeKind::Array)
	{
		const TypeKey& array = model_.types_[object.value];
		if (array.unknown_bound)
			throw std::runtime_error("PA15 zero constructor array is incomplete");
		const LoweredValue sequence = emit_decay(destination_value);
		for (std::size_t i = 0; i < array.bound.value; ++i)
		{
			const LowType element_type = low_type(array.child);
			LoweredValue element;
			std::vector<ConstructorAddressStep> element_path;
			if (root_action != NULL && path != NULL)
			{
				element_path = *path;
				element_path.push_back(ConstructorAddressStep(BindingId(), i, true));
			}
			if (root_action != NULL && path != NULL && i != 0)
			{
				element = constructor_path_address(*root_action, element_path);
			}
			else
				element = emit_index(sequence,
					LoweredValue(integer_operand(static_cast<long long>(i),
						size_low_type()), size_low_type(), false), element_type,
					lowir_model::IPK_ARRAY_ELEMENT);
			zero_initialize_constructor_value(array.child, element,
				root_action, root_action != NULL && path != NULL ?
				&element_path : NULL);
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
			layout.state != RecordLayoutState::Complete)
			throw std::runtime_error("PA15 zero constructor class layout is invalid");
		LowType byte;
		byte.kind = LowType::TYPE_INTEGER;
		byte.integer_kind = LowType::INTEGER_I8;
		const std::vector<BindingId> class_members =
			model_.scopes_[scope.value].bindings;
		for (std::size_t i = 0; i < class_members.size(); ++i)
		{
			const BindingId member = class_members[i];
			if (model_.binding(member).kind != BindingKind::Variable ||
				model_.is_static_member(member))
				continue;
			const std::size_t* offset = layout.member_offsets.find(member);
			if (offset == NULL || *offset > static_cast<std::size_t>(
				std::numeric_limits<long long>::max()))
				throw std::runtime_error("PA15 zero constructor member offset is invalid");
			std::vector<ConstructorAddressStep> member_path;
			if (root_action != NULL && path != NULL)
			{
				member_path = *path;
				member_path.push_back(ConstructorAddressStep(member));
			}
			const LoweredValue member_value = root_action != NULL && path != NULL &&
				i != 0 ? constructor_path_address(*root_action, member_path) :
				emit_index(destination_value, LoweredValue(integer_operand(
					static_cast<long long>(*offset), size_low_type()), size_low_type(),
					false), byte, lowir_model::IPK_FIELD);
			zero_initialize_constructor_value(model_.binding(member).type,
				member_value, root_action, root_action != NULL && path != NULL ?
				&member_path : NULL);
		}
		return;
	}
	const LowType value_type = low_type(target);
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
	const std::vector<ConstructorAddressStep>* path)
{
	if (!initializer.valid() || initializer.value >= model_.semantic_facts_.size())
		throw std::runtime_error("PA15 constructor initializer is invalid");
	const SemanticFact& fact = model_.semantic_facts_[initializer.value];
	if (fact.kind == SemanticFactKind::ConstructorAction)
	{
		if (!fact.has_callee || !fact.selected_binding.valid() ||
			fact.child_count != 0)
			throw std::runtime_error("PA15 nested constructor initializer is unsupported");
		emit_constructor_call(fact.selected_binding, destination_value,
			InvalidIdentityValue, 0);
		return;
	}
	if (fact.kind != SemanticFactKind::BracedInitList)
	{
		const LoweredValue value = lower_expression(initializer);
		emit_store(low_type(target), value.value, destination_value.value);
		return;
	}
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(target));
	if (!object.valid())
		throw std::runtime_error("PA15 braced constructor target is invalid");
	const std::vector<SemanticFactId> values = children(initializer);
	if (model_.type_kind(object) == TypeKind::Array)
	{
		const TypeKey& array = model_.types_[object.value];
		if (array.unknown_bound || values.size() > array.bound.value)
			throw std::runtime_error("PA15 braced constructor array bound is invalid");
		const bool recompute_path = root_action != NULL && path != NULL;
		const LoweredValue sequence = emit_decay(destination_value);
		for (std::size_t i = 0; i < array.bound.value; ++i)
		{
			const LowType element_type = low_type(array.child);
			std::vector<ConstructorAddressStep> element_path;
			LoweredValue element;
			if (recompute_path)
			{
				element_path = *path;
				element_path.push_back(ConstructorAddressStep(BindingId(), i, true));
			}
			if (recompute_path && i != 0)
			{
				element = constructor_path_address(*root_action, element_path);
			}
			else
				element = emit_index(sequence,
					LoweredValue(integer_operand(static_cast<long long>(i),
						size_low_type()), size_low_type(), false), element_type,
					lowir_model::IPK_ARRAY_ELEMENT);
			if (i < values.size())
			{
				const SemanticFact& value = model_.semantic_facts_[values[i].value];
				if (value.kind == SemanticFactKind::BracedInitList)
					initialize_constructor_value(array.child, values[i], element,
						root_action, recompute_path ? &element_path : NULL);
				else
				{
					const LoweredValue lowered = lower_expression(values[i]);
					emit_store(element_type, lowered.value, element.value);
				}
			}
			else
				zero_initialize_constructor_value(array.child, element, root_action,
					recompute_path ? &element_path : NULL);
		}
		return;
	}
	if (model_.type_kind(object) != TypeKind::Named)
		throw std::runtime_error("PA15 braced constructor target is not aggregate");
	const NamedRecordId record = model_.named_record_for_type(object);
	if (!record.valid() || record.value >= model_.named_.size() ||
		model_.named_[record.value].kind != NamedKind::Class ||
		model_.named_[record.value].class_tag == ClassTag::Union ||
		model_.named_[record.value].has_base)
		throw std::runtime_error("PA15 braced constructor class is unsupported");
	const ScopeId scope = model_.named_[record.value].scope;
	const RecordLayout& layout = model_.record_layout(record);
	if (!scope.valid() || scope.value >= model_.scopes_.size() ||
		layout.state != RecordLayoutState::Complete || values.size() >
		model_.scopes_[scope.value].bindings.size())
		throw std::runtime_error("PA15 braced constructor class layout is invalid");
	LowType byte;
	byte.kind = LowType::TYPE_INTEGER;
	byte.integer_kind = LowType::INTEGER_I8;
	std::vector<BindingId> members;
	const std::vector<BindingId> class_members =
		model_.scopes_[scope.value].bindings;
	for (std::size_t i = 0; i < class_members.size(); ++i)
	{
		const BindingId member = class_members[i];
		if (model_.binding(member).kind == BindingKind::Variable &&
			!model_.is_static_member(member))
			members.push_back(member);
	}
	if (values.size() > members.size())
		throw std::runtime_error("PA15 braced constructor has too many members");
	for (std::size_t i = 0; i < members.size(); ++i)
	{
		const BindingId member = members[i];
		const std::size_t* offset = layout.member_offsets.find(member);
		if (offset == NULL || *offset > static_cast<std::size_t>(
			std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 braced constructor member offset is invalid");
		std::vector<ConstructorAddressStep> member_path;
		if (root_action != NULL && path != NULL)
		{
			member_path = *path;
			member_path.push_back(ConstructorAddressStep(member));
		}
		const LoweredValue member_value = root_action != NULL && path != NULL &&
			i != 0 ? constructor_path_address(*root_action, member_path) :
			emit_index(destination_value, LoweredValue(integer_operand(
				static_cast<long long>(*offset), size_low_type()), size_low_type(),
				false), byte, lowir_model::IPK_FIELD);
		if (i < values.size())
			initialize_constructor_value(model_.binding(member).type, values[i],
				member_value, root_action, root_action != NULL && path != NULL ?
				&member_path : NULL);
		else
			zero_initialize_constructor_value(model_.binding(member).type,
				member_value, root_action, root_action != NULL && path != NULL ?
				&member_path : NULL);
	}
}

void Pa15Lowerer::lower_constructor_action(const ConstructorActionFact& action)
{
	if (action.constructor.valid())
	{
		const LoweredValue destination_value = constructor_subobject_address(action);
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
			const LoweredValue destination_value =
				constructor_subobject_address(action);
			emit_store(low_type(target), value.value, destination_value.value);
			return;
		}
		const LoweredValue destination_value = constructor_subobject_address(action);
		const std::vector<ConstructorAddressStep> empty_path;
		initialize_constructor_value(target, action.initializer,
			destination_value, &action, &empty_path);
		return;
	}
	throw std::runtime_error("PA15 constructor action has no operation");
}

bool Pa15Lowerer::constructor_action_is_noop(const SemanticFact& action) const
{
	if (!action.selected_binding.valid())
		return false;
	const FunctionFactId* function_id =
		model_.function_binding_fact_index_.find(action.selected_binding);
	if (function_id != NULL && function_id->valid() &&
		function_id->value < model_.function_facts_.size())
	{
		const FunctionFact& function = model_.function_facts_[function_id->value];
		if (function.is_constructor && function.synthetic &&
			function.constructor_action_count == 0)
			return true;
	}
	const BindingSidecar* binding_sidecar =
		model_.binding_sidecar(action.selected_binding);
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
	const std::vector<SemanticFactId> action_facts = children(id);
	if (action_facts.size() != 1)
		throw std::runtime_error("PA15 constructor expression has no call fact");
	return lower_expression(action_facts.front());
}

} // namespace pa11_semantic_internal
