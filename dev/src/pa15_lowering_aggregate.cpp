#include "pa15_lowering.h"

#include <functional>

namespace pa11_semantic_internal
{

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
			const std::uint8_t* bytes =
				model_.constant_address_literal_bytes_.data() +
				value.literal_byte_begin;
			std::uint64_t byte_hash = 1469598103934665603ULL;
			for (std::size_t byte = 0; byte < value.literal_byte_count; ++byte)
			{
				byte_hash ^= static_cast<std::uint64_t>(bytes[byte]);
				byte_hash *= 1099511628211ULL;
			}
			const LiteralContentKey content_key(value.element_type,
				value.literal_element_count, value.literal_byte_count, byte_hash);
			std::vector<LiteralContentIdentity>& content_bucket =
				literal_content_symbols_[content_key];
			for (std::size_t bucket_index = 0;
				bucket_index < content_bucket.size(); ++bucket_index)
			{
				const LiteralContentIdentity& content = content_bucket[bucket_index];
				if (!content.fact.valid() || content.fact.value >=
					model_.constant_address_facts_.size() ||
					!content.symbol.valid())
					throw std::runtime_error(
						"PA15 literal content identity is invalid");
				const ConstantAddressFact& prior =
					model_.constant_address_facts_[content.fact.value];
				if (!prior.evaluated || !prior.valid ||
					prior.kind != ConstantAddressKind::Literal ||
					prior.element_type != value.element_type ||
					prior.literal_element_count != value.literal_element_count ||
					prior.literal_byte_count != value.literal_byte_count ||
					prior.literal_byte_begin == InvalidIdentityValue ||
					prior.literal_byte_begin >
					model_.constant_address_literal_bytes_.size() ||
					prior.literal_byte_count >
					model_.constant_address_literal_bytes_.size() -
					prior.literal_byte_begin)
					throw std::runtime_error(
						"PA15 literal content payload identity is invalid");
				const std::uint8_t* prior_bytes =
					model_.constant_address_literal_bytes_.data() +
					prior.literal_byte_begin;
				bool same_payload = true;
				for (std::size_t byte = 0; byte < value.literal_byte_count; ++byte)
					if (prior_bytes[byte] != bytes[byte])
					{
						same_payload = false;
						break;
					}
				if (same_payload)
				{
					literal_address_symbols_[fact.constant_address.value] =
						content.symbol;
					*target = content.symbol;
					*addend = value.byte_addend;
					if (relocation != NULL) *relocation = &value;
					return true;
				}
			}
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
			content_bucket.push_back(LiteralContentIdentity(
				fact.constant_address, literal_global.symbol_id));
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
			demand_function_declaration(BindingId(value.target.value));
			*target = function->second;
		}
		*addend = value.byte_addend;
		if (relocation != NULL) *relocation = &value;
		return true;
	}

struct Pa15Lowerer::TypedGlobalDataAppender
{
	Pa15Lowerer& lowerer_;
	GlobalDefinition* global_;

	TypedGlobalDataAppender(Pa15Lowerer& lowerer, GlobalDefinition* global)
		: lowerer_(lowerer), global_(global) {}

	void append_zero(std::size_t bytes)
	{
		if (bytes == 0)
			return;
		if (!global_->data_items.empty() && global_->data_items.back().kind ==
			GlobalDefinition::DataItem::ITEM_ZERO)
		{
			global_->data_items.back().zero_bytes += bytes;
			return;
		}
		GlobalDefinition::DataItem item;
		item.kind = GlobalDefinition::DataItem::ITEM_ZERO;
		item.zero_bytes = bytes;
		global_->data_items.push_back(item);
	}

	void append_typed_zero(TypeId value)
	{
		const LowType type = lowerer_.low_type(value);
		if (type.is_integer())
		{
			GlobalDefinition::DataItem item;
			item.kind = GlobalDefinition::DataItem::ITEM_INTEGER;
			item.type = type;
			item.literal_operand = lowerer_.integer_operand(0, type);
			global_->data_items.push_back(item);
		}
		else
			append_zero(type.storage_size());
	}

	std::size_t object_bytes(TypeId value) const
	{
		const LowType type = lowerer_.low_type(value);
		if (!type.valid() || type.storage_size() == 0)
			throw std::runtime_error("PA15 typed global object size is invalid");
		return type.storage_size();
	}

	bool can_zero(TypeId value) const
	{
		const TypeId object = lowerer_.model_.strip_cv_type(
			lowerer_.model_.expression_object_type(value));
		if (!object.valid() || object.value >= lowerer_.model_.types_.size())
			return false;
		const TypeKind kind = lowerer_.model_.type_kind(object);
		if (kind == TypeKind::Array)
			return !lowerer_.model_.types_[object.value].unknown_bound &&
				can_zero(lowerer_.model_.types_[object.value].child);
		if (kind == TypeKind::Named)
		{
			const NamedRecordId record =
				lowerer_.model_.named_record_for_type(object);
			return record.valid() && record.value < lowerer_.model_.named_.size() &&
				lowerer_.model_.named_[record.value].kind == NamedKind::Class &&
				lowerer_.checkpoint_zero_storage_eligible(object);
		}
		const LowType type = lowerer_.low_type(value);
		return type.is_integer() || type.is_float() || type.is_pointer();
	}

	bool resolve_parameter(SemanticFactId id, const FunctionFact* function,
		const std::vector<SemanticFactId>& arguments, SemanticFactId* result) const
	{
		if (result == NULL || function == NULL || !id.valid() ||
			id.value >= lowerer_.model_.semantic_facts_.size())
			return false;
		const SemanticFact& fact = lowerer_.model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::IdExpression && fact.binding.valid() &&
			function->function_scope.valid() && function->function_scope.value <
			lowerer_.model_.scopes_.size())
		{
			const Scope& scope = lowerer_.model_.scopes_[function->function_scope.value];
			std::size_t argument_index = 0;
			for (std::size_t i = 0; i < scope.bindings.size(); ++i)
			{
				const BindingId parameter = scope.bindings[i];
				if (parameter.value >= lowerer_.model_.bindings_.size() ||
					lowerer_.model_.binding(parameter).kind != BindingKind::Parameter)
					continue;
				if (parameter == scope.implicit_object_binding)
					continue;
				if (parameter == fact.binding)
				{
					if (argument_index >= arguments.size())
						return false;
					*result = arguments[argument_index];
					return result->valid() && result->value <
						lowerer_.model_.semantic_facts_.size();
				}
				++argument_index;
			}
		}
		if (fact.kind == SemanticFactKind::CastExpression &&
			fact.child_count == 1 && fact.child_begin != InvalidIdentityValue &&
			fact.child_begin < lowerer_.model_.semantic_children_.size())
			return resolve_parameter(lowerer_.model_.semantic_children_[fact.child_begin],
				function, arguments, result);
		return false;
	}

	bool append_string_literal(TypeId destination, const SemanticFact& fact)
	{
		if (fact.source == NULL || fact.source->kind != PA10NodeKind::Literal ||
			fact.source->literal.type != FundamentalType::Char)
			throw std::runtime_error("PA15 typed global string literal is invalid");
		const TypeKey& array = lowerer_.model_.types_[destination.value];
		if (array.unknown_bound || fact.literal_element_count > array.bound.value)
			throw std::runtime_error("PA15 typed global string literal is too large");
		const LowType element_type = lowerer_.low_type(array.child);
		if (!element_type.is_integer() || element_type.storage_size() != 1)
			throw std::runtime_error("PA15 typed global string element is invalid");
		for (std::size_t i = 0; i < array.bound.value; ++i)
		{
			if (i < fact.literal_element_count)
			{
				GlobalDefinition::DataItem item;
				item.kind = GlobalDefinition::DataItem::ITEM_INTEGER;
				item.type = element_type;
				item.literal_operand = lowerer_.integer_operand(
					static_cast<long long>(fact.source->literal.bytes[i]), element_type);
				global_->data_items.push_back(item);
			}
			else
				append_zero(element_type.storage_size());
		}
		return true;
	}

	bool append_constructor(TypeId object, SemanticFactId fact_id,
		const SemanticFact& fact)
	{
		if (!fact.has_callee || !fact.selected_binding.valid())
			return false;
		const FunctionFact* function = lowerer_.model_.function_fact_for_binding(
			fact.selected_binding);
		if (function == NULL || !function->is_constructor ||
			function->constructor_action_begin == InvalidIdentityValue ||
			function->constructor_action_begin >
			lowerer_.model_.constructor_actions_.size() ||
			function->constructor_action_count >
			lowerer_.model_.constructor_actions_.size() -
			function->constructor_action_begin)
			return false;
		const NamedRecordId record =
			lowerer_.model_.class_record_for_object_type(object);
		if (!record.valid() || record.value >= lowerer_.model_.named_.size() ||
			lowerer_.model_.named_[record.value].kind != NamedKind::Class ||
			lowerer_.model_.named_[record.value].class_tag == ClassTag::Union)
			return false;
		const RecordLayout& layout = lowerer_.model_.record_layout(record);
		if (layout.state != RecordLayoutState::Complete)
			return false;
		const std::vector<SemanticFactId> arguments = lowerer_.children(fact_id);
		std::map<std::size_t, const ConstructorActionFact*> actions;
		for (std::size_t i = 0; i < function->constructor_action_count; ++i)
		{
			const ConstructorActionFact& action = lowerer_.model_.constructor_actions_[
				function->constructor_action_begin + i];
			if (action.target != ConstructorActionTarget::Member ||
				!action.member.valid())
				return false;
			if (!actions.insert(std::make_pair(action.member.value, &action)).second)
				throw std::runtime_error("PA15 duplicate typed global constructor member");
		}
		std::size_t offset = 0;
		for (std::size_t i = 0; i < layout.members.size(); ++i)
		{
			const BindingId member = layout.members[i].binding;
			const std::size_t* member_offset = layout.member_offsets.find(member);
			if (member_offset == NULL || *member_offset < offset ||
				*member_offset > layout.size)
				throw std::runtime_error("PA15 typed global constructor offset is invalid");
			append_zero(*member_offset - offset);
			const std::map<std::size_t, const ConstructorActionFact*>::const_iterator
				action = actions.find(member.value);
			if (action == actions.end())
			{
				if (!can_zero(lowerer_.model_.binding(member).type))
					return false;
				append_zero(object_bytes(lowerer_.model_.binding(member).type));
			}
			else
			{
				const ConstructorActionFact& value = *action->second;
				if (value.constructor.valid() || !value.initializer.valid())
					return false;
				SemanticFactId resolved;
				if (!resolve_parameter(value.initializer, function, arguments, &resolved) ||
					!append(lowerer_.model_.binding(member).type, resolved))
					return false;
			}
			offset = *member_offset + object_bytes(lowerer_.model_.binding(member).type);
		}
		if (offset > layout.size)
			throw std::runtime_error("PA15 typed global constructor size is invalid");
		append_zero(layout.size - offset);
		return true;
	}

	bool append_braced(TypeId object, SemanticFactId fact_id)
	{
		std::size_t element_count = 0;
		std::size_t total_count = 0;
		const AggregateElementFact* elements = lowerer_.aggregate_elements(
			fact_id, &element_count, &total_count);
		if (lowerer_.model_.type_kind(object) == TypeKind::Array)
		{
			const TypeKey& array = lowerer_.model_.types_[object.value];
			if (array.unknown_bound || total_count != array.bound.value)
				throw std::runtime_error("PA15 typed global array bound is invalid");
			const std::size_t element_bytes = object_bytes(array.child);
			const std::size_t max_size = std::numeric_limits<std::size_t>::max();
			std::size_t next_index = 0;
			for (std::size_t i = 0; i < element_count; ++i)
			{
				const std::size_t index = elements[i].index;
				if (index < next_index || index >= array.bound.value)
					throw std::runtime_error("PA15 typed global sparse array is invalid");
				const std::size_t gap = index - next_index;
				if (gap != 0)
				{
					if (!can_zero(array.child) || gap > max_size / element_bytes)
						return false;
					append_zero(gap * element_bytes);
				}
				if (!append(array.child, elements[i].initializer))
					return false;
				next_index = index + 1;
			}
			const std::size_t tail = array.bound.value - next_index;
			if (tail != 0)
			{
				if (!can_zero(array.child) || tail > max_size / element_bytes)
					return false;
				append_zero(tail * element_bytes);
			}
			return true;
		}
		if (lowerer_.model_.type_kind(object) != TypeKind::Named)
			throw std::runtime_error("PA15 typed global list is not an object");
		const NamedRecordId record = lowerer_.model_.named_record_for_type(object);
		if (!record.valid() || record.value >= lowerer_.model_.named_.size() ||
			lowerer_.model_.named_[record.value].kind != NamedKind::Class ||
			lowerer_.model_.named_[record.value].class_tag == ClassTag::Union ||
			lowerer_.model_.named_[record.value].has_base)
			throw std::runtime_error("PA15 typed global class is unsupported");
		const RecordLayout& layout = lowerer_.model_.record_layout(record);
		if (layout.state != RecordLayoutState::Complete ||
			total_count != layout.members.size())
			throw std::runtime_error("PA15 typed global class layout is invalid");
		std::size_t next = 0;
		std::size_t offset = 0;
		for (std::size_t i = 0; i < layout.members.size(); ++i)
		{
			const BindingId member = layout.members[i].binding;
			const std::size_t* member_offset = layout.member_offsets.find(member);
			if (member_offset == NULL || *member_offset < offset ||
				*member_offset > layout.size)
				throw std::runtime_error("PA15 typed global member offset is invalid");
			append_zero(*member_offset - offset);
			const TypeId member_type = lowerer_.model_.binding(member).type;
			if (next < element_count && elements[next].index == i)
			{
				if (elements[next].member != member ||
					!append(member_type, elements[next++].initializer))
					return false;
			}
			else if (!can_zero(member_type))
				return false;
			else
				append_typed_zero(member_type);
			offset = *member_offset + object_bytes(member_type);
		}
		if (next != element_count || offset > layout.size)
			throw std::runtime_error("PA15 typed global sparse class is invalid");
		append_zero(layout.size - offset);
		return true;
	}

	bool append(TypeId destination, SemanticFactId fact_id)
	{
		if (!destination.valid() || destination.value >= lowerer_.model_.types_.size() ||
			!fact_id.valid() || fact_id.value >= lowerer_.model_.semantic_facts_.size())
			throw std::runtime_error("PA15 typed global child identity is invalid");
		const TypeId object = lowerer_.model_.strip_cv_type(
			lowerer_.model_.expression_object_type(destination));
		if (!object.valid() || object.value >= lowerer_.model_.types_.size())
			throw std::runtime_error("PA15 typed global child type is invalid");
		const SemanticFact& fact = lowerer_.model_.semantic_facts_[fact_id.value];
		if (fact.kind == SemanticFactKind::BracedInitList)
			return append_braced(object, fact_id);
		if (fact.kind == SemanticFactKind::Literal &&
			fact.literal_element_count != 0 &&
			lowerer_.model_.type_kind(object) == TypeKind::Array)
			return append_string_literal(object, fact);
		if (fact.kind == SemanticFactKind::ConstructorAction)
			return append_constructor(object, fact_id, fact);
		const LowType type = lowerer_.low_type(destination);
		if (type.is_pointer())
		{
			if (lowerer_.typed_pointer_zero(fact_id, destination))
			{
				append_zero(type.storage_size());
				return true;
			}
			SymbolId target;
			long long addend = 0;
			if (!lowerer_.map_constant_address(fact_id, &target, &addend, NULL))
				return false;
			GlobalDefinition::DataItem item;
			item.kind = GlobalDefinition::DataItem::ITEM_ADDR;
			item.type = type;
			item.symbol_id = target;
			item.addr_addend = addend;
			item.symbol_name_id = lowerer_.symbol_name_for(target);
			if (!item.symbol_name_id.valid())
				throw std::runtime_error("PA15 typed global address has no name");
			global_->data_items.push_back(item);
			return true;
		}
		if (type.is_integer())
		{
			Operand value;
			if (!lowerer_.constant_integer(fact_id, type, &value))
				return false;
			GlobalDefinition::DataItem item;
			item.kind = GlobalDefinition::DataItem::ITEM_INTEGER;
			item.type = type;
			item.literal_operand = value;
			global_->data_items.push_back(item);
			return true;
		}
		return false;
	}

	bool run(TypeId type, SemanticFactId initializer)
	{
		if (global_ == NULL || !type.valid() || type.value >=
			lowerer_.model_.types_.size() || !initializer.valid() ||
			initializer.value >= lowerer_.model_.semantic_facts_.size())
			throw std::runtime_error("PA15 typed global initializer identity is invalid");
		return append(type, initializer);
	}
};

bool Pa15Lowerer::append_typed_global_data(GlobalDefinition* global, TypeId type,
	SemanticFactId initializer)
{
	return TypedGlobalDataAppender(*this, global).run(type, initializer);
}

const AggregateElementFact* Pa15Lowerer::aggregate_elements(
	SemanticFactId id, std::size_t* count, std::size_t* total_count) const
{
	if (count == NULL || total_count == NULL)
		throw std::runtime_error("PA15 aggregate range output is missing");
	if (!id.valid() || id.value >= model_.semantic_facts_.size())
		throw std::runtime_error("PA15 invalid aggregate fact identity");
	const SemanticFact& fact = model_.semantic_facts_[id.value];
	if (fact.kind != SemanticFactKind::BracedInitList)
		throw std::runtime_error("PA15 aggregate range belongs to non-list fact");
	if (fact.child_count != 0)
		throw std::runtime_error("PA15 aggregate list has duplicate semantic edges");
	const AggregateFactRange* range = model_.aggregate_ranges_.find(id);
	if (range == NULL)
		throw std::runtime_error("PA15 aggregate list has no typed range");
	if (range->count > range->total_count ||
		range->begin == InvalidIdentityValue ||
		range->begin > model_.aggregate_elements_.size() ||
		range->count > model_.aggregate_elements_.size() - range->begin)
		throw std::runtime_error("PA15 invalid aggregate element range");
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(fact.type));
	if (!object.valid() || object.value >= model_.types_.size())
		throw std::runtime_error("PA15 aggregate owner type is invalid");
	if (model_.type_kind(object) != TypeKind::Array &&
		model_.type_kind(object) != TypeKind::Named)
		throw std::runtime_error("PA15 aggregate owner is not an object");
	const TypeKey* array = model_.type_kind(object) == TypeKind::Array ?
		&model_.types_[object.value] : NULL;
	const NamedRecordId record = model_.type_kind(object) == TypeKind::Named ?
		model_.named_record_for_type(object) : NamedRecordId();
	const RecordLayout* layout = NULL;
	ScopeId scope;
	if (array != NULL)
	{
		if (array->unknown_bound || range->total_count != array->bound.value)
			throw std::runtime_error("PA15 aggregate array bound is invalid");
	}
	else
	{
		if (!record.valid() || record.value >= model_.named_.size() ||
			model_.named_[record.value].kind != NamedKind::Class ||
			model_.named_[record.value].class_tag == ClassTag::Union ||
			model_.named_[record.value].has_base)
			throw std::runtime_error("PA15 aggregate class element owner is invalid");
		scope = model_.named_[record.value].scope;
		if (!scope.valid() || scope.value >= model_.scopes_.size() ||
			model_.scopes_[scope.value].kind != ScopeKind::Class ||
			model_.scopes_[scope.value].record != record)
			throw std::runtime_error("PA15 aggregate class scope is invalid");
		layout = &model_.record_layout(record);
		if (layout->state != RecordLayoutState::Complete ||
			range->total_count != layout->members.size())
			throw std::runtime_error("PA15 aggregate class layout is invalid");
	}
	std::size_t previous_index = 0;
	for (std::size_t i = 0; i < range->count; ++i)
	{
		const AggregateElementFact& element = model_.aggregate_elements_[
			range->begin + i];
		if (element.kind != AggregateElementKind::ArrayElement &&
			element.kind != AggregateElementKind::Member)
			throw std::runtime_error("PA15 invalid aggregate element kind");
		if (!element.type.valid() || element.type.value >= model_.types_.size())
			throw std::runtime_error("PA15 invalid aggregate element type");
		if (element.index >= range->total_count ||
			(i != 0 && element.index <= previous_index))
			throw std::runtime_error("PA15 aggregate element indices are not sparse");
		previous_index = element.index;
		if (!element.initializer.valid() || element.initializer.value >=
			model_.semantic_facts_.size())
			throw std::runtime_error("PA15 invalid aggregate element initializer");
		if (array != NULL)
		{
			if (element.kind !=
				AggregateElementKind::ArrayElement || element.member.valid() ||
				element.type != array->child)
				throw std::runtime_error(
					"PA15 aggregate array element owner is invalid");
		}
		else
		{
			if (element.kind != AggregateElementKind::Member ||
				!element.member.valid() || element.member.value >=
					model_.bindings_.size() || element.member.value >=
					model_.binding_owners_.size() ||
				model_.binding_owners_[element.member.value] != scope ||
				model_.binding(element.member).kind != BindingKind::Variable ||
				model_.is_static_member(element.member) ||
				element.type != model_.binding(element.member).type)
				throw std::runtime_error(
					"PA15 aggregate class element owner is invalid");
			if (element.index >= layout->members.size() ||
				layout->members[element.index].binding != element.member)
				throw std::runtime_error(
					"PA15 aggregate class declaration order is invalid");
		}
	}
	*count = range->count;
	*total_count = range->total_count;
	return range->count == 0 ? NULL : &model_.aggregate_elements_[range->begin];
}

void Pa15Lowerer::initialize_aggregate_value(TypeId target,
	SemanticFactId initializer, const LoweredValue& destination_value,
	const ConstructorActionFact* root_action,
	const std::vector<ConstructorAddressStep>* path,
	BitFieldInitializationContext* context,
	const LoweredValue* aggregate_root_storage, TypeId aggregate_root_type)
{
	BitFieldInitializationContext local_context;
	if (context == NULL)
		context = &local_context;
	if (!initializer.valid() || initializer.value >= model_.semantic_facts_.size())
		throw std::runtime_error("PA15 aggregate initializer is invalid");
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(target));
	if (!object.valid() || object.value >= model_.types_.size())
		throw std::runtime_error("PA15 aggregate constructor target is invalid");
	std::size_t element_count = 0;
	std::size_t total_count = 0;
	const AggregateElementFact* elements = aggregate_elements(initializer,
		&element_count, &total_count);
	if (model_.type_kind(object) == TypeKind::Array)
	{
		const TypeKey& array = model_.types_[object.value];
		if (array.unknown_bound || array.bound.value > static_cast<std::size_t>(
			std::numeric_limits<long long>::max()) ||
			total_count != array.bound.value)
			throw std::runtime_error("PA15 braced constructor array bound is invalid");
		const bool recompute_path = root_action != NULL && path != NULL;
		const bool recompute_aggregate_path = aggregate_root_storage != NULL &&
			path != NULL;
		const LoweredValue sequence = emit_decay(destination_value);
		const TypeId child_object = model_.strip_cv_type(
			model_.expression_object_type(array.child));
		const bool byte_projection = (child_object.valid() &&
			model_.type_kind(child_object) == TypeKind::Array) ||
			class_object_type(array.child);
		std::size_t next_element = 0;
		for (std::size_t i = 0; i < array.bound.value; ++i)
		{
			const LowType element_type = array_element_instruction_type(array.child);
			std::vector<ConstructorAddressStep> element_path;
			LoweredValue element;
			if (path != NULL)
			{
				element_path = *path;
				element_path.push_back(ConstructorAddressStep(BindingId(), i, true));
			}
			if (recompute_path && i != 0)
			{
				element = constructor_path_address(*root_action, element_path);
			}
			else if (recompute_aggregate_path &&
				(path->empty() || i != 0))
				element = aggregate_path_address(*aggregate_root_storage,
					aggregate_root_type, element_path);
			else
				element = emit_index(sequence,
					byte_projection ? emit_array_element_offset(object, i) :
					LoweredValue(integer_operand(static_cast<long long>(i),
						size_low_type()), size_low_type(), false), element_type,
					lowir_model::IPK_ARRAY_ELEMENT);
			const AggregateElementFact* element_fact = next_element < element_count &&
				elements[next_element].index == i ? &elements[next_element++] : NULL;
			if (element_fact != NULL)
				initialize_constructor_value(array.child,
					element_fact->initializer, element, root_action,
					path != NULL ? &element_path : NULL, NULL,
					aggregate_root_storage, aggregate_root_type);
			else
				zero_initialize_constructor_value(array.child, element, root_action,
					path != NULL ? &element_path : NULL, NULL,
					aggregate_root_storage, aggregate_root_type);
		}
		if (next_element != element_count)
			throw std::runtime_error("PA15 aggregate array sparse index is invalid");
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
		model_.scopes_[scope.value].kind != ScopeKind::Class ||
		model_.scopes_[scope.value].record != record ||
		layout.state != RecordLayoutState::Complete)
		throw std::runtime_error("PA15 braced constructor class layout is invalid");
	LowType byte;
	byte.kind = LowType::TYPE_INTEGER;
	byte.integer_kind = LowType::INTEGER_I8;
	if (total_count != layout.members.size())
		throw std::runtime_error("PA15 braced constructor member count is invalid");
	std::size_t next_element = 0;
	for (std::size_t i = 0; i < total_count; ++i)
	{
		const AggregateElementFact* element_fact = next_element < element_count &&
			elements[next_element].index == i ? &elements[next_element++] : NULL;
		const BindingId member = element_fact != NULL ? element_fact->member :
			layout.members[i].binding;
		if (member.value >= model_.bindings_.size() ||
			member.value >= model_.binding_owners_.size() ||
			model_.binding_owners_[member.value] != scope ||
			model_.binding(member).kind != BindingKind::Variable ||
			model_.is_static_member(member) ||
			(element_fact != NULL && element_fact->type !=
				model_.binding(member).type))
			throw std::runtime_error("PA15 aggregate member fact owner is invalid");
		const std::size_t* offset = layout.member_offsets.find(member);
		if (offset == NULL || *offset > static_cast<std::size_t>(
			std::numeric_limits<long long>::max()))
			throw std::runtime_error("PA15 braced constructor member offset is invalid");
		std::vector<ConstructorAddressStep> member_path;
		if (path != NULL)
		{
			member_path = *path;
			member_path.push_back(ConstructorAddressStep(member));
		}
		const SemanticFact* initializer_fact = element_fact != NULL ?
			&model_.semantic_facts_[element_fact->initializer.value] : NULL;
		const bool direct_scalar = initializer_fact != NULL &&
			initializer_fact->kind != SemanticFactKind::BracedInitList &&
			initializer_fact->kind != SemanticFactKind::ConstructorAction &&
			!(initializer_fact->kind == SemanticFactKind::Literal &&
				initializer_fact->literal_element_count != 0 &&
				model_.type_kind(model_.strip_cv_type(
					model_.expression_object_type(model_.binding(member).type))) ==
					TypeKind::Array);
		LoweredValue direct_value;
		if (direct_scalar)
			direct_value = lower_expression(element_fact->initializer);
		LoweredValue encoded;
		const bool encoded_bit_field = element_fact != NULL &&
			element_fact->initializer.valid() &&
			model_.bit_field_fact(member) != NULL;
		const bool encode_before_address = encoded_bit_field &&
			!bit_field_initialization_preserves_existing(member, *context);
		if (encode_before_address)
			encoded = encode_bit_field_value(member,
				lower_expression(element_fact->initializer),
				true);
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
		if (encoded_bit_field)
		{
			if (encode_before_address)
				initialize_encoded_bit_field(member_value, member, encoded,
					*context);
			else
				initialize_bit_field(member_value, member,
					lower_expression(element_fact->initializer), *context);
		}
		else if (element_fact != NULL)
		{
			if (direct_scalar)
				emit_store(low_type(model_.binding(member).type),
					direct_value.value, member_value.value);
			else
				initialize_constructor_value(model_.binding(member).type,
					element_fact->initializer,
					member_value, root_action, path != NULL ? &member_path : NULL,
					NULL, aggregate_root_storage, aggregate_root_type);
		}
		else
		{
			BitFieldInitializationContext* member_context =
				model_.bit_field_fact(member) != NULL ? context : NULL;
			zero_initialize_constructor_value(model_.binding(member).type,
				member_value, root_action, root_action != NULL && path != NULL ?
				&member_path : NULL, member_context);
		}
	}
	if (next_element != element_count)
		throw std::runtime_error("PA15 aggregate member sparse index is invalid");
}

} // namespace pa11_semantic_internal
