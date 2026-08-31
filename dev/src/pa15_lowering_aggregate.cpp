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

	// Keep a single omitted scalar slot typed, while coalescing longer runs into
	// one zero item.  This preserves the structural boundary without allocating
	// one data item per bound-sized tail.
	void append_zero_elements(TypeId value, std::size_t count,
		std::size_t element_bytes)
	{
		if (count == 0)
			return;
		if (count == 1)
			append_typed_zero(value);
		else
			append_zero(count * element_bytes);
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
		return lowerer_.resolve_constructor_parameter(id, function, arguments,
			result);
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
					append_zero_elements(array.child, gap, element_bytes);
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
				append_zero_elements(array.child, tail, element_bytes);
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

bool Pa15Lowerer::resolve_constructor_parameter(SemanticFactId id,
	const FunctionFact* function, const std::vector<SemanticFactId>& arguments,
	SemanticFactId* result) const
{
	if (result == NULL || function == NULL || !id.valid() ||
		id.value >= model_.semantic_facts_.size() ||
		!function->function_scope.valid() ||
		function->function_scope.value >= model_.scopes_.size())
		return false;
	const Scope& scope = model_.scopes_[function->function_scope.value];
	if (scope.kind != ScopeKind::Function || scope.parent != function->owner)
		return false;
	std::set<std::size_t> visited;
	SemanticFactId current = id;
	while (current.valid() && current.value < model_.semantic_facts_.size() &&
		visited.insert(current.value).second)
	{
		const SemanticFact& fact = model_.semantic_facts_[current.value];
		if (fact.kind == SemanticFactKind::IdExpression)
		{
			if (!fact.binding.valid() || fact.binding.value >=
				model_.bindings_.size() || fact.binding.value >=
				model_.binding_owners_.size() ||
				model_.binding_owners_[fact.binding.value] !=
				function->function_scope || model_.binding(fact.binding).kind !=
				BindingKind::Parameter)
				return false;
			std::size_t argument_index = 0;
			for (std::size_t i = 0; i < scope.bindings.size(); ++i)
			{
				const BindingId parameter = scope.bindings[i];
				if (!parameter.valid() || parameter.value >= model_.bindings_.size() ||
					parameter.value >= model_.binding_owners_.size() ||
					model_.binding_owners_[parameter.value] !=
					function->function_scope)
					return false;
				if (model_.binding(parameter).kind != BindingKind::Parameter)
					continue;
				if (parameter == scope.implicit_object_binding)
					continue;
				if (parameter == fact.binding)
				{
					if (argument_index >= arguments.size() ||
						!arguments[argument_index].valid() ||
						arguments[argument_index].value >=
						model_.semantic_facts_.size())
						return false;
					*result = arguments[argument_index];
					return true;
				}
				++argument_index;
			}
			return false;
		}
		if (fact.kind != SemanticFactKind::CastExpression ||
			fact.child_count != 1 || fact.child_begin == InvalidIdentityValue ||
			fact.child_begin >= model_.semantic_children_.size())
			return false;
		current = model_.semantic_children_[fact.child_begin];
	}
	return false;
}

bool Pa15Lowerer::global_aggregate_constructor_inline_eligible(
	const SemanticFact& fact) const
{
	if (fact.kind != SemanticFactKind::ConstructorAction || !fact.has_callee ||
		!fact.selected_binding.valid() || fact.selected_binding.value >=
		model_.bindings_.size())
		return false;
	const BindingSidecar* binding_sidecar =
		model_.binding_sidecar(fact.selected_binding);
	if (binding_sidecar == NULL || !binding_sidecar->constructor_record.valid() ||
		binding_sidecar->constructor_record.value >= model_.named_.size())
		return false;
	const NamedRecordId record_id = binding_sidecar->constructor_record;
	const NamedRecord& record = model_.named_[record_id.value];
	if (record.kind != NamedKind::Class || record.class_tag == ClassTag::Union ||
		!record.name.valid() || record.has_base ||
		!record.scope.valid() || record.scope.value >= model_.scopes_.size() ||
		model_.scopes_[record.scope.value].kind != ScopeKind::Class ||
		model_.scopes_[record.scope.value].record != record_id)
		return false;
	const NamedRecordSidecar* record_sidecar =
		model_.named_record_sidecar(record_id);
	if (record_sidecar == NULL ||
		record_sidecar->aggregate_constructor_binding != fact.selected_binding)
		return false;
	const Binding& selected = model_.binding(fact.selected_binding);
	if (selected.kind != BindingKind::Function || !selected.type.valid() ||
		selected.type.value >= model_.types_.size() ||
		model_.type_kind(selected.type) != TypeKind::Function)
		return false;
	const FunctionFact* function = model_.function_fact_for_binding(
		fact.selected_binding);
	if (function == NULL || function->binding != fact.selected_binding ||
		function->owner != record.scope || !function->is_constructor ||
		!function->synthetic ||
		function->constructor_action_begin == InvalidIdentityValue ||
		function->constructor_action_begin > model_.constructor_actions_.size() ||
		function->constructor_action_count > model_.constructor_actions_.size() -
		function->constructor_action_begin)
		return false;
	const RecordLayout& layout = model_.record_layout(record_id);
	if (layout.state != RecordLayoutState::Complete ||
		function->constructor_action_count != layout.members.size())
		return false;
	if (!function->function_scope.valid() || function->function_scope.value >=
		model_.scopes_.size() || model_.scopes_[function->function_scope.value].kind !=
		ScopeKind::Function || model_.scopes_[function->function_scope.value].parent !=
		record.scope)
		return false;
	const Scope& function_scope = model_.scopes_[function->function_scope.value];
	if (!function_scope.implicit_object_binding.valid() ||
		function_scope.implicit_object_binding.value >= model_.bindings_.size() ||
		function_scope.implicit_object_binding.value >= model_.binding_owners_.size() ||
		model_.binding_owners_[function_scope.implicit_object_binding.value] !=
			function->function_scope ||
		model_.binding(function_scope.implicit_object_binding).kind !=
			BindingKind::Parameter)
		return false;
	const TypeKey& signature = model_.types_[
		model_.binding(fact.selected_binding).type.value];
	if (signature.variadic || signature.parameters.size() != layout.members.size() ||
		fact.child_count != signature.parameters.size() ||
		(fact.child_count != 0 &&
			(fact.child_begin == InvalidIdentityValue ||
				fact.child_begin > model_.semantic_children_.size() ||
				fact.child_count > model_.semantic_children_.size() -
					fact.child_begin)) ||
		(fact.child_count == 0 && fact.child_begin != InvalidIdentityValue &&
			fact.child_begin > model_.semantic_children_.size()))
		return false;
	std::size_t non_object_parameters = 0;
	for (std::size_t i = 0; i < function_scope.bindings.size(); ++i)
	{
		const BindingId parameter = function_scope.bindings[i];
		if (!parameter.valid() || parameter.value >= model_.bindings_.size() ||
			parameter.value >= model_.binding_owners_.size() ||
			model_.binding_owners_[parameter.value] != function->function_scope)
			return false;
		if (model_.binding(parameter).kind != BindingKind::Parameter)
			continue;
		if (parameter != function_scope.implicit_object_binding)
			++non_object_parameters;
	}
	if (non_object_parameters != signature.parameters.size())
		return false;
	for (std::size_t i = 0; i < function->constructor_action_count; ++i)
	{
		const ConstructorActionFact& action = model_.constructor_actions_[
			function->constructor_action_begin + i];
		if (action.target != ConstructorActionTarget::Member ||
			action.member != layout.members[i].binding || action.constructor.valid() ||
			!action.initializer.valid() || action.argument_count != 0 ||
			(action.argument_begin != InvalidIdentityValue &&
				action.argument_begin > model_.constructor_arguments_.size()) ||
			action.initializer.value >= model_.semantic_facts_.size())
			return false;
		const BindingId member = action.member;
		if (!member.valid() || member.value >= model_.bindings_.size() ||
			member.value >= model_.binding_owners_.size() ||
			model_.binding_owners_[member.value] != record.scope ||
			model_.binding(member).kind != BindingKind::Variable ||
			model_.is_static_member(member) ||
			action.object_type != model_.binding(member).type ||
			signature.parameters[i] != model_.binding(member).type)
			return false;
		const LowType member_type = low_type(model_.binding(action.member).type);
		if (!member_type.is_integer() && !member_type.is_float() &&
			!member_type.is_pointer())
			return false;
		const std::size_t* member_offset = layout.member_offsets.find(member);
		if (member_offset == NULL || *member_offset > layout.size ||
			member_type.storage_size() == 0 || *member_offset > layout.size -
				member_type.storage_size())
			return false;
		SemanticFactId current = action.initializer;
		std::set<std::size_t> visited;
		bool parameter = false;
		while (current.valid() && current.value < model_.semantic_facts_.size() &&
			visited.insert(current.value).second)
		{
			const SemanticFact& initializer = model_.semantic_facts_[current.value];
			if (initializer.kind == SemanticFactKind::IdExpression)
			{
				if (!initializer.binding.valid() || initializer.binding.value >=
					model_.bindings_.size() || initializer.binding.value >=
					model_.binding_owners_.size() ||
					model_.binding_owners_[initializer.binding.value] !=
					function->function_scope || model_.binding(initializer.binding).kind !=
					BindingKind::Parameter || initializer.binding ==
					function_scope.implicit_object_binding)
					return false;
				bool listed = false;
				for (std::size_t parameter_index = 0;
					parameter_index < function_scope.bindings.size(); ++parameter_index)
					if (function_scope.bindings[parameter_index] == initializer.binding)
					{
						listed = true;
						break;
					}
				if (!listed)
					return false;
				parameter = true;
				break;
			}
			if (initializer.kind != SemanticFactKind::CastExpression ||
				initializer.child_count != 1 || initializer.child_begin ==
				InvalidIdentityValue || initializer.child_begin >=
				model_.semantic_children_.size())
				return false;
			current = model_.semantic_children_[initializer.child_begin];
		}
		if (!parameter)
			return false;
	}
	return true;
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

bool Pa15Lowerer::aggregate_element_is_noop(TypeId type,
	const AggregateElementFact* element) const
{
	if (element == NULL)
		return zero_initialization_is_noop(type);
	if (!element->initializer.valid() ||
		element->initializer.value >= model_.semantic_facts_.size())
		return false;
	const SemanticFact& initializer =
		model_.semantic_facts_[element->initializer.value];
	return initializer.kind == SemanticFactKind::ConstructorAction &&
		(constructor_action_is_noop(initializer) ||
			aggregate_constructor_action_is_noop(type, initializer));
}

bool Pa15Lowerer::semantic_fact_is_side_effect_free(
	SemanticFactId fact_id, std::set<std::size_t>& visiting,
	std::map<std::size_t, bool>& memo) const
{
	if (!fact_id.valid() || fact_id.value >= model_.semantic_facts_.size())
		return false;
	const std::map<std::size_t, bool>::const_iterator known =
		memo.find(fact_id.value);
	if (known != memo.end())
		return known->second;
	if (!visiting.insert(fact_id.value).second)
		return false;
	const SemanticFact& fact = model_.semantic_facts_[fact_id.value];
	const bool child_range_valid = fact.child_begin == InvalidIdentityValue ?
		fact.child_count == 0 :
		(fact.child_begin <= model_.semantic_children_.size() &&
			fact.child_count <= model_.semantic_children_.size() -
				fact.child_begin);
	bool result = false;
	if (child_range_valid && fact.type.valid() &&
		fact.type.value < model_.types_.size() &&
		(model_.cv_qualifiers(fact.type) & 2u) == 0)
		switch (fact.kind)
		{
		case SemanticFactKind::Literal:
		case SemanticFactKind::SizeofExpression:
			result = fact.child_count == 0;
			break;
		case SemanticFactKind::UnaryExpression:
			if (fact.token == SimpleTokenType::OP_INC ||
				fact.token == SimpleTokenType::OP_DEC)
			{
				result = false;
				break;
			}
			// Fall through: the remaining unary operators only evaluate their
			// operand, so their typed child edges carry the complete effect set.
		case SemanticFactKind::CastExpression:
		case SemanticFactKind::MemberExpression:
		case SemanticFactKind::SubscriptExpression:
		case SemanticFactKind::BinaryExpression:
		case SemanticFactKind::ConditionalExpression:
			result = true;
			for (std::size_t i = 0; i < fact.child_count; ++i)
			{
				const SemanticFactId child = model_.semantic_children_[
					fact.child_begin + i];
				if (!semantic_fact_is_side_effect_free(child, visiting, memo))
				{
					result = false;
					break;
				}
			}
			break;
		default:
			break;
		}
	visiting.erase(fact_id.value);
	memo[fact_id.value] = result;
	return result;
}

bool Pa15Lowerer::aggregate_constructor_action_is_noop(
	TypeId type, const SemanticFact& action) const
{
	if (action.kind != SemanticFactKind::ConstructorAction ||
		action.type != type ||
		!action.has_callee || action.temporary_object || action.value_initialize ||
		!action.selected_binding.valid() || !action.selected_scope.valid() ||
		action.selected_binding.value >= model_.bindings_.size() ||
		action.selected_binding.value >= model_.binding_owners_.size() ||
		action.selected_scope.value >= model_.scopes_.size() ||
		!action.callable_type.valid() || action.callable_type.value >=
			model_.types_.size() || model_.type_kind(action.callable_type) !=
			TypeKind::Function)
		return false;
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(type));
	if (!object.valid() || object.value >= model_.types_.size() ||
		model_.type_kind(object) != TypeKind::Named)
		return false;
	const NamedRecordId record = model_.named_record_for_type(object);
	if (!record.valid() || record.value >= model_.named_.size() ||
		model_.named_[record.value].kind != NamedKind::Class ||
		model_.named_[record.value].class_tag == ClassTag::Union ||
		model_.named_[record.value].has_base)
		return false;
	if (record.value >= model_.record_layouts_.size())
		return false;
	const ScopeId record_scope = model_.named_[record.value].scope;
	if (!record_scope.valid() || record_scope.value >= model_.scopes_.size() ||
		model_.scopes_[record_scope.value].kind != ScopeKind::Class ||
		model_.scopes_[record_scope.value].record != record ||
		action.selected_scope != record_scope ||
		model_.binding_owners_[action.selected_binding.value] != record_scope)
		return false;
	const RecordLayout& layout = model_.record_layout(record);
	if (layout.state != RecordLayoutState::Complete || !layout.members.empty())
		return false;
	const BindingSidecar* sidecar =
		model_.binding_sidecar(action.selected_binding);
	if (sidecar == NULL || sidecar->constructor_record != record ||
		action.selected_scope != model_.named_[record.value].scope)
		return false;
	const FunctionFactId* function_id =
		model_.function_binding_fact_index_.find(action.selected_binding);
	if (function_id == NULL || !function_id->valid() || function_id->value >=
		model_.function_facts_.size())
		return false;
	const FunctionFact& function = model_.function_facts_[function_id->value];
	if (function.binding != action.selected_binding ||
		function.constructor_record != record ||
		function.owner != record_scope || function.synthetic ||
		!constructor_function_is_noop(*function_id, false))
		return false;
	const Binding& binding = model_.binding(action.selected_binding);
	if (binding.kind != BindingKind::Function || !binding.type.valid() ||
		binding.type.value >= model_.types_.size() ||
		model_.type_kind(binding.type) != TypeKind::Function)
		return false;
	const TypeKey& signature = model_.types_[binding.type.value];
	const TypeKey& callable_signature =
		model_.types_[action.callable_type.value];
	if (callable_signature.result != signature.result ||
		callable_signature.variadic != signature.variadic ||
		callable_signature.parameters.size() != signature.parameters.size() + 1)
		return false;
	const TypeId implicit_object = callable_signature.parameters.front();
	if (!implicit_object.valid() ||
		implicit_object.value >= model_.types_.size() ||
		model_.type_kind(implicit_object) != TypeKind::Pointer ||
		!model_.types_[implicit_object.value].child.valid() ||
		model_.types_[implicit_object.value].child.value >= model_.types_.size() ||
		model_.type_kind(model_.types_[implicit_object.value].child) !=
			TypeKind::Named ||
		model_.named_record_for_type(
			model_.types_[implicit_object.value].child) != record)
		return false;
	for (std::size_t parameter = 0;
		parameter < signature.parameters.size(); ++parameter)
		if (callable_signature.parameters[parameter + 1] !=
			signature.parameters[parameter])
			return false;
	if (signature.variadic || signature.parameters.size() != action.child_count)
		return false;
	if (action.child_count != 0 &&
		(action.child_begin == InvalidIdentityValue ||
			action.child_begin > model_.semantic_children_.size() ||
			action.child_count > model_.semantic_children_.size() -
				action.child_begin))
		return false;
	if (action.child_count == 0 && action.child_begin != InvalidIdentityValue &&
		action.child_begin > model_.semantic_children_.size())
		return false;
	std::set<std::size_t> visiting;
	std::map<std::size_t, bool> memo;
	for (std::size_t i = 0; i < action.child_count; ++i)
		if (!semantic_fact_is_side_effect_free(model_.semantic_children_[
			action.child_begin + i], visiting, memo))
			return false;
	return true;
}

bool Pa15Lowerer::aggregate_direct_scalar(const SemanticFact* initializer,
	TypeId type) const
{
	if (initializer == NULL || initializer->kind == SemanticFactKind::BracedInitList ||
		initializer->kind == SemanticFactKind::ConstructorAction)
		return false;
	if (initializer->kind != SemanticFactKind::Literal ||
		initializer->literal_element_count == 0)
		return true;
	const TypeId object = model_.strip_cv_type(
		model_.expression_object_type(type));
	return object.valid() && object.value < model_.types_.size() &&
		model_.type_kind(object) != TypeKind::Array;
}

void Pa15Lowerer::initialize_aggregate_value(
	TypeId target, SemanticFactId initializer,
	const LoweredValue& destination_value,
	const ConstructorActionFact* root_action, const std::vector<ConstructorAddressStep>* path,
	BitFieldInitializationContext* context, const LoweredValue* aggregate_root_storage, TypeId aggregate_root_type)
{
	BitFieldInitializationContext local_context;
	if (context == NULL)
		context = &local_context;
	if (!initializer.valid() || initializer.value >= model_.semantic_facts_.size())
		throw std::runtime_error("PA15 aggregate initializer is invalid");
	const TypeId object = model_.strip_cv_type(model_.expression_object_type(target));
	if (!object.valid() || object.value >= model_.types_.size())
		throw std::runtime_error("PA15 aggregate constructor target is invalid");
	std::size_t element_count = 0;
	std::size_t total_count = 0;
	const AggregateElementFact* elements = aggregate_elements(initializer, &element_count, &total_count);
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
		const bool lazy_storage_context = aggregate_root_storage != NULL &&
			destination_value.lvalue && destination_value.type.is_object();
		const bool direct_root_address = recompute_aggregate_path &&
			path->empty() && !lazy_storage_context && !recompute_path;
		LoweredValue sequence;
		if (!lazy_storage_context && !direct_root_address)
			sequence = emit_decay(destination_value);
		const TypeId child_object = model_.strip_cv_type(model_.expression_object_type(array.child));
		const bool byte_projection = (child_object.valid() &&
			model_.type_kind(child_object) == TypeKind::Array) ||
			class_object_type(array.child);
		std::size_t next_element = 0;
		for (std::size_t i = 0; i < array.bound.value; ++i)
		{
			const LowType element_type = array_element_instruction_type(array.child);
			std::vector<ConstructorAddressStep> element_path;
			if (path != NULL)
			{
				element_path = *path;
				element_path.push_back(ConstructorAddressStep(BindingId(), i, true));
			}
			const AggregateElementFact* element_fact = next_element < element_count &&
				elements[next_element].index == i ? &elements[next_element++] : NULL;
			const SemanticFact* initializer_fact = element_fact != NULL ?
				&model_.semantic_facts_[element_fact->initializer.value] : NULL;
			if (aggregate_element_is_noop(array.child, element_fact))
				continue;
			const bool direct_scalar = initializer_fact != NULL &&
				initializer_fact->kind != SemanticFactKind::BracedInitList &&
				initializer_fact->kind != SemanticFactKind::ConstructorAction &&
				!(initializer_fact->kind == SemanticFactKind::Literal &&
					initializer_fact->literal_element_count != 0);
			if (lazy_storage_context && element_fact != NULL && direct_scalar)
			{
				const LoweredValue value = lower_expression(
					element_fact->initializer);
				const LoweredValue element = aggregate_path_address(
					*aggregate_root_storage, aggregate_root_type, element_path);
				emit_store(low_type(array.child), value.value, element.value);
				continue;
			}
			if (lazy_storage_context && element_fact != NULL &&
				(initializer_fact->kind == SemanticFactKind::BracedInitList ||
					(initializer_fact->kind == SemanticFactKind::ConstructorAction &&
						global_aggregate_constructor_inline_eligible(*initializer_fact))))
			{
				initialize_constructor_value(array.child,
					element_fact->initializer, destination_value, root_action,
					&element_path, NULL, aggregate_root_storage,
					aggregate_root_type);
				continue;
			}
			LoweredValue element;
			if (recompute_path && i != 0)
			{
				element = constructor_path_address(*root_action, element_path);
			}
			else if (direct_root_address && i == 0)
			{
				element = destination_value;
			}
			else if (direct_root_address)
			{
				const std::size_t offset = checked_array_element_offset(object, i);
				if (offset > static_cast<std::size_t>(
					std::numeric_limits<long long>::max()))
					throw std::runtime_error(
						"PA15 direct aggregate array offset is invalid");
				element = emit_index(destination_value,
					LoweredValue(integer_operand(static_cast<long long>(offset),
						size_low_type()), size_low_type(), false), element_type,
					lowir_model::IPK_NONE);
			}
			else if (lazy_storage_context || (recompute_aggregate_path &&
				(path->empty() || i != 0)))
			{
				element = aggregate_path_address(*aggregate_root_storage,
					aggregate_root_type, element_path);
			}
			else
				element = emit_index(sequence,
					byte_projection ? emit_array_element_offset(object, i) :
					LoweredValue(integer_operand(static_cast<long long>(i),
						size_low_type()), size_low_type(), false), element_type,
					lowir_model::IPK_ARRAY_ELEMENT);
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
		const SemanticFact* element_initializer = element_fact != NULL ?
			&model_.semantic_facts_[element_fact->initializer.value] : NULL;
		const bool aggregate_action_noop = element_initializer != NULL &&
			element_initializer->kind == SemanticFactKind::ConstructorAction &&
			aggregate_constructor_action_is_noop(
				model_.binding(member).type, *element_initializer);
		if (aggregate_element_is_noop(model_.binding(member).type, element_fact) &&
			!aggregate_action_noop)
			continue;
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
		const SemanticFact* initializer_fact = element_initializer;
		const bool direct_scalar = aggregate_direct_scalar(initializer_fact,
			model_.binding(member).type);
		LoweredValue direct_value = direct_scalar ?
			lower_expression(element_fact->initializer) : LoweredValue();
		LoweredValue encoded;
		const bool encoded_bit_field = element_fact != NULL &&
			element_fact->initializer.valid() &&
			model_.bit_field_fact(member) != NULL;
		const LoweredValue bit_field_value = encoded_bit_field && !direct_scalar ?
			lower_expression(element_fact->initializer) : direct_value;
		const bool encode_before_address = encoded_bit_field &&
			!bit_field_initialization_preserves_existing(member, *context);
		if (encode_before_address)
			encoded = encode_bit_field_value(member, bit_field_value, true);
		LoweredValue member_value;
		if (root_action != NULL && path != NULL && i != 0)
			member_value = constructor_path_address(*root_action, member_path);
		else if (aggregate_root_storage != NULL && path != NULL &&
			(path->empty() || i != 0))
			member_value = aggregate_path_address(*aggregate_root_storage,
				aggregate_root_type, member_path);
		else
		{
			const LoweredValue member_offset(integer_operand(
				static_cast<long long>(*offset), size_low_type()), size_low_type(),
				false);
			member_value = model_.bit_field_fact(member) != NULL ?
				emit_bit_field_index(destination_value, member_offset, byte,
					lowir_model::IPK_FIELD, member) :
				emit_index(destination_value, member_offset, byte,
					lowir_model::IPK_FIELD);
		}
		if (aggregate_action_noop)
			continue;
		if (encoded_bit_field)
		{
			if (encode_before_address)
				initialize_encoded_bit_field(member_value, member, encoded,
					*context);
			else
				initialize_bit_field(member_value, member, bit_field_value,
					*context);
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
				member_value, root_action, path != NULL ? &member_path : NULL,
				member_context,
				aggregate_root_storage, aggregate_root_type);
		}
	}
	if (next_element != element_count) throw std::runtime_error("PA15 aggregate member sparse index is invalid");
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

void Pa15Lowerer::emit_constructor_elements(TypeId target,
	const LoweredValue& destination, BindingId constructor,
	std::size_t argument_begin, std::size_t argument_count,
	const std::vector<SemanticFactId>* semantic_arguments,
	ArrayCleanupChain* cleanup,
	std::vector<ConstructedElement>* completed,
	bool value_initialize,
	const ArrayAddressRoot& root,
	const std::vector<ConstructorAddressStep>& path,
	bool destination_deferred, bool constructor_may_throw)
{
	if (!destination_deferred && !destination.type.is_pointer())
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
		LoweredValue sequence;
		if (!destination_deferred)
			sequence = emit_decay(destination);
		for (std::size_t i = 0; i < array.bound.value; ++i)
		{
			const TypeId child = array.child;
			std::vector<ConstructorAddressStep> element_path = path;
			element_path.push_back(ConstructorAddressStep(BindingId(), i, true));
			const bool defer_element = !completed->empty() &&
				constructor_may_throw;
			LoweredValue element;
			if (defer_element)
			{
				// The terminal constructor emits its own address after installing
				// the matching handler.  Do not let an address producer from this
				// normal block become an exception-edge input.
			}
			else if (destination_deferred)
			{
				const ConstructedElement address_root(root, element_path);
				element = recompute_constructed_element_address(address_root);
			}
			else if (i != 0)
			{
				const ConstructedElement address_root(root, element_path);
				element = recompute_constructed_element_address(address_root);
			}
			else
				element = emit_index(sequence, emit_array_element_offset(object, i),
					array_element_instruction_type(child),
					lowir_model::IPK_ARRAY_ELEMENT);
			emit_constructor_elements(child, element, constructor,
				argument_begin, argument_count, semantic_arguments,
				cleanup, completed, value_initialize, root, element_path,
				defer_element, constructor_may_throw);
		}
		return;
	}
	const NamedRecordId record = model_.class_record_for_object_type(object);
	if (!record.valid() || record.value >= model_.named_.size() ||
		model_.named_[record.value].class_tag == ClassTag::Union)
		throw std::runtime_error("PA15 array constructor element is not a class");
	const FunctionFact& constructor_function =
		checked_constructor_function(constructor, record);
	const ConstructedElement completed_element(root, path,
		model_.destructor_binding(record), record);
	LoweredValue call_destination = destination;
	if (destination_deferred && value_initialize &&
		constructor_function.synthetic)
		call_destination = recompute_constructed_element_address(
			completed_element);
	if (value_initialize && constructor_function.synthetic)
		zero_initialize_value_initialized_object(object, call_destination);
	const FunctionFactId* constructor_id =
		model_.function_binding_fact_index_.find(constructor);
	const bool no_op = constructor_function.synthetic && constructor_id != NULL &&
		constructor_id->valid() && constructor_id->value <
		model_.function_facts_.size() && constructor_function_is_noop(*constructor_id);
	const bool throwing = constructor_may_throw;
	bool emitted = false;
	if (!no_op && throwing && !completed->empty())
	{
		materialize_constructor_cleanup(cleanup, *completed);
		if (cleanup->head.valid())
		{
			const BlockId dispatch = cleanup->head;
			const BlockId continuation =
				block_id(new_block("array_ctor_continue"));
			Instruction eh;
			eh.kind = Instruction::IK_EH_TRY;
			eh.first = block_operand(dispatch);
			block().instructions.push_back(eh);
			const LoweredValue eh_destination = destination_deferred ?
				recompute_constructed_element_address(completed_element) : destination;
			if (semantic_arguments != NULL)
				emit_constructor_call(constructor, eh_destination,
					*semantic_arguments);
			else
				emit_constructor_call(constructor, eh_destination,
					argument_begin, argument_count);
			Instruction end;
			end.kind = Instruction::IK_EH_END;
			block().instructions.push_back(end);
			emit_jump(continuation);
			set_current(continuation);
			emitted = true;
		}
	}
	if (!no_op && !emitted)
	{
		if (destination_deferred && !call_destination.type.is_pointer())
			call_destination = recompute_constructed_element_address(
				completed_element);
		if (semantic_arguments != NULL)
			emit_constructor_call(constructor, call_destination,
				*semantic_arguments);
		else
			emit_constructor_call(constructor, call_destination,
				argument_begin, argument_count);
	}
	completed->push_back(completed_element);
}

} // namespace pa11_semantic_internal
