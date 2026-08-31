#include "pa11_semantic_model.h"

#include <limits>

namespace pa11_semantic_internal
{

namespace
{

bool align_up_checked(std::size_t value, std::size_t alignment,
	std::size_t* result)
{
	if (alignment == 0)
		return false;
	const std::size_t remainder = value % alignment;
	if (remainder == 0)
	{
		*result = value;
		return true;
	}
	const std::size_t padding = alignment - remainder;
	if (value > std::numeric_limits<std::size_t>::max() - padding)
		return false;
	*result = value + padding;
	return true;
}

}

const BitFieldFact* PA11SemanticModel::bit_field_fact(BindingId binding_id) const
{
	return binding_id.valid() ? bit_field_facts_.find(binding_id) : NULL;
}

const BitFieldFact* PA11SemanticModel::bit_field_fact_for_expression(
	const ExprInfo& expression) const
{
	if (!expression.fact.valid() || expression.fact.value >= semantic_facts_.size())
		return NULL;
	const SemanticFact& fact = semantic_facts_[expression.fact.value];
	if (fact.category != SemanticValueCategory::Lvalue ||
		(fact.kind != SemanticFactKind::IdExpression &&
			fact.kind != SemanticFactKind::MemberExpression))
		return NULL;
	const BindingId binding_id = fact.binding.valid() ? fact.binding :
		fact.selected_binding;
	const BitFieldFact* result = bit_field_fact(binding_id);
	if (result == NULL)
		return NULL;
	if (!result->named || result->binding != binding_id ||
		!result->operation_type.valid())
		throw std::runtime_error("invalid PA11 bit-field expression fact");
	return result;
}

void PA11SemanticModel::set_bit_field_fact(BindingId binding_id,
	const BitFieldFact& fact)
{
	if (!binding_id.valid() || binding_id.value >= bindings_.size() ||
		fact.binding != binding_id)
		throw std::runtime_error("invalid PA11 bit-field identity");
	bit_field_facts_.set(binding_id, fact);
}

void PA11SemanticModel::append_record_member(NamedRecordId record_id,
	BindingId binding_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		record_id.value >= record_member_declarations_.size() ||
		!binding_id.valid() || binding_id.value >= bindings_.size())
		throw std::runtime_error("invalid PA11 record member identity");
	if (record_member_event_owners_.find(binding_id) != NULL)
		throw std::runtime_error("duplicate PA11 record member declaration");
	record_member_event_owners_.set(binding_id, record_id);
	record_member_declarations_[record_id.value].push_back(
		RecordMemberDeclaration(record_id, binding_id));
}

void PA11SemanticModel::append_record_bit_field(NamedRecordId record_id,
	const BitFieldFact& fact)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		record_id.value >= record_member_declarations_.size() ||
		fact.owner_record != record_id ||
		(fact.named && !fact.binding.valid()))
		throw std::runtime_error("invalid PA11 record bit-field identity");
	if (fact.named)
	{
		if (record_member_event_owners_.find(fact.binding) != NULL)
			throw std::runtime_error("duplicate PA11 record member declaration");
		record_member_event_owners_.set(fact.binding, record_id);
	}
	record_member_declarations_[record_id.value].push_back(
		RecordMemberDeclaration::make_bit_field(fact));
}

const RecordLayout& PA11SemanticModel::record_layout(
	NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		record_id.value >= record_layouts_.size())
		throw std::runtime_error("invalid PA11 record layout identity");
	return record_layouts_[record_id.value];
}

bool PA11SemanticModel::pa17_transfer_field_type(TypeId type) const
{
	// This deliberately walks only the canonical type wrappers.  A named class
	// is a hard boundary for this checkpoint: without a typed proof of its
	// special members, an outer object must not be lowered as a raw copy.
	TypeId current = type;
	while (current.valid() && current.value < types_.size())
	{
		const TypeKey& key = types_[current.value];
		switch (key.kind)
		{
		case TypeKind::Cv:
		case TypeKind::Array:
			if (key.kind == TypeKind::Array && key.unknown_bound)
				return false;
			current = key.child;
			continue;
		case TypeKind::Fundamental:
			return key.fundamental != FundamentalType::Void;
		case TypeKind::Pointer:
			return true;
		case TypeKind::Named:
			return key.named.valid() && key.named.value < named_.size() &&
				named_[key.named.value].kind == NamedKind::Enum;
		case TypeKind::MemberPointer:
		case TypeKind::LvalueReference:
		case TypeKind::RvalueReference:
		case TypeKind::Function:
			return false;
		}
	}
	return false;
}

bool PA11SemanticModel::pa17_class_value_transfer_eligible(
	NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		record_id.value >= record_layouts_.size())
		return false;
	const RecordLayout& layout = record_layouts_[record_id.value];
	ClassValueTransferFact& fact = layout.pa17_class_value_transfer;
	if (fact.state == ClassValueTransferState::Complete)
		return fact.eligible;
	if (fact.state == ClassValueTransferState::Failed)
		return false;
	if (fact.state == ClassValueTransferState::Computing)
	{
		// The current policy rejects class subobjects before recursion, but keep
		// the state transition explicit so a future typed recursive proof is
		// cycle-safe and conservative by construction.
		fact.state = ClassValueTransferState::Failed;
		fact.eligible = false;
		return false;
	}
	fact.state = ClassValueTransferState::Computing;
	fact.eligible = false;
	const NamedRecord& record = named_[record_id.value];
	bool eligible = layout.state == RecordLayoutState::Complete &&
		record.kind == NamedKind::Class && record.class_tag != ClassTag::Union &&
		record.defined && !record.has_base && !record.direct_base.valid() &&
		!record.direct_base_virtual && !record.has_virtual_member &&
		!layout.has_direct_base && !layout.direct_base.record.valid() &&
		record.scope.valid() && record.scope.value < scopes_.size() &&
		scopes_[record.scope.value].kind == ScopeKind::Class &&
		scopes_[record.scope.value].record == record_id;
	const NamedRecordSidecar* sidecar = named_record_sidecar(record_id);
	if (eligible && sidecar != NULL &&
		(sidecar->has_constructor_declaration ||
		 sidecar->has_destructor_declaration ||
		 sidecar->has_default_member_initializer))
		eligible = false;
	if (eligible)
	{
		for (std::size_t i = 0; i < layout.members.size(); ++i)
		{
			const BindingId member_id = layout.members[i].binding;
			if (!member_id.valid() || member_id.value >= bindings_.size() ||
				member_id.value >= binding_owners_.size() ||
				binding_owners_[member_id.value] != record.scope)
			{
				eligible = false;
				break;
			}
			const Binding& member = binding(member_id);
			if (member.kind != BindingKind::Variable ||
				is_static_member(member_id) ||
				!pa17_transfer_field_type(member.type))
			{
				eligible = false;
				break;
			}
		}
	}
	fact.eligible = eligible;
	fact.state = ClassValueTransferState::Complete;
	return fact.eligible;
}

TypeLayout PA11SemanticModel::type_layout(TypeId type) const
{
	if (!type.valid() || type.value >= types_.size())
		throw std::runtime_error("invalid PA11 layout type");
	const TypeKey& key = types_[type.value];
	switch (key.kind)
	{
	case TypeKind::Cv:
		return type_layout(key.child);
	case TypeKind::Fundamental:
		switch (key.fundamental)
		{
		case FundamentalType::SignedChar:
		case FundamentalType::UnsignedChar:
		case FundamentalType::Char:
		case FundamentalType::Bool:
			return TypeLayout(1, 1);
		case FundamentalType::ShortInt:
		case FundamentalType::UnsignedShortInt:
		case FundamentalType::Char16T:
			return TypeLayout(2, 2);
		case FundamentalType::Int:
		case FundamentalType::UnsignedInt:
		case FundamentalType::WcharT:
		case FundamentalType::Char32T:
		case FundamentalType::Float:
			return TypeLayout(4, 4);
		case FundamentalType::LongInt:
		case FundamentalType::LongLongInt:
		case FundamentalType::UnsignedLongInt:
		case FundamentalType::UnsignedLongLongInt:
		case FundamentalType::Double:
			return TypeLayout(8, 8);
		case FundamentalType::LongDouble:
			return TypeLayout(16, 16);
		case FundamentalType::NullptrT:
			return TypeLayout(8, 8);
		case FundamentalType::Void:
			throw std::runtime_error("void has no record layout");
		}
		break;
	case TypeKind::Pointer:
	case TypeKind::MemberPointer:
	case TypeKind::LvalueReference:
	case TypeKind::RvalueReference:
		return TypeLayout(8, 8);
	case TypeKind::Array:
	{
		if (key.unknown_bound)
			throw std::runtime_error("incomplete array has no record layout");
		const TypeLayout element = type_layout(key.child);
		if (element.size == 0 || element.alignment == 0 ||
			key.bound.value > std::numeric_limits<std::size_t>::max() /
				element.size)
			throw std::runtime_error("record layout size overflow");
		return TypeLayout(key.bound.value * element.size,
			element.alignment);
	}
	case TypeKind::Named:
	{
		const NamedRecordId record_id = key.named;
		if (!record_id.valid() || record_id.value >= named_.size())
			throw std::runtime_error("invalid named record layout type");
		const NamedRecord& record = named_[record_id.value];
		if (record.kind == NamedKind::Enum)
			return type_layout(record.has_underlying ? record.underlying :
				fundamental(FundamentalType::Int));
		if (record.kind == NamedKind::TemplateParameter)
			throw std::runtime_error("template parameter has no record layout");
		const RecordLayout& layout = record_layout(record_id);
		if (layout.state == RecordLayoutState::Computing)
			throw std::runtime_error("cyclic by-value record layout");
		if (layout.state == RecordLayoutState::Failed)
			throw std::runtime_error("record layout previously failed");
		if (layout.state != RecordLayoutState::Complete)
			throw std::runtime_error("incomplete class has no record layout");
		return TypeLayout(layout.size, layout.alignment);
	}
	case TypeKind::Function:
		throw std::runtime_error("function has no record layout");
	}
	throw std::runtime_error("unhandled PA11 record layout type");
}

bool PA11SemanticModel::type_has_zero_offset_record(TypeId type,
	const RecordTypeSet& records) const
{
	RecordTypeSet visited;
	return type_has_zero_offset_record(type, records, visited);
}

bool PA11SemanticModel::type_has_zero_offset_record(TypeId type,
	const RecordTypeSet& records, RecordTypeSet& visited) const
{
	if (!type.valid() || type.value >= types_.size())
		return false;
	while (type.valid() && type.value < types_.size() &&
		type_kind(type) == TypeKind::Cv)
		type = types_[type.value].child;
	if (!type.valid() || type.value >= types_.size())
		return false;
	const TypeKey& key = types_[type.value];
	if (key.kind == TypeKind::Array)
		return !key.unknown_bound &&
			type_has_zero_offset_record(key.child, records, visited);
	if (key.kind != TypeKind::Named || !key.named.valid() ||
		key.named.value >= named_.size())
		return false;
	if (records.find(key.named) != NULL)
		return true;
	const NamedRecord& record = named_[key.named.value];
	if (record.kind != NamedKind::Class)
		return false;
	if (visited.find(key.named) != NULL)
		return false;
	visited.set(key.named, true);
	const RecordLayout& layout = record_layout(key.named);
	if (layout.state != RecordLayoutState::Complete || layout.size == 0 ||
		layout.alignment == 0 || !record.scope.valid() ||
		record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != key.named ||
		layout.has_direct_base != record.has_base ||
		(!layout.has_direct_base && (layout.direct_base.record.valid() ||
			layout.direct_base.zero_size || layout.direct_base.offset != 0)))
		throw std::runtime_error(
			"zero-offset record query has an inconsistent layout owner");
	if (layout.has_direct_base)
	{
		if (!layout.direct_base.record.valid() ||
			layout.direct_base.record.value >= named_.size() ||
			record.direct_base != layout.direct_base.record ||
			record.direct_base_virtual || layout.direct_base.offset != 0 ||
			named_[layout.direct_base.record.value].kind != NamedKind::Class)
			throw std::runtime_error("zero-offset direct base identity is invalid");
		const RecordLayout& base_layout = record_layout(
			layout.direct_base.record);
		if (base_layout.state != RecordLayoutState::Complete ||
			base_layout.size == 0 || base_layout.alignment == 0 ||
			(layout.direct_base.zero_size && !base_layout.empty))
			throw std::runtime_error("zero-offset direct base layout is invalid");
		if (type_has_zero_offset_record(
			named_type(layout.direct_base.record), records, visited))
			return true;
	}
	for (std::size_t i = 0; i < layout.members.size(); ++i)
	{
		const BindingId member_id = layout.members[i].binding;
		if (!member_id.valid() || member_id.value >= bindings_.size() ||
			member_id.value >= binding_owners_.size() ||
			binding_owners_[member_id.value] != record.scope)
			throw std::runtime_error("zero-offset member identity is invalid");
		const Binding& member = binding(member_id);
		if (member.kind != BindingKind::Variable ||
			is_static_member(member_id))
			throw std::runtime_error("zero-offset layout member is not a field");
		const std::size_t* member_offset = layout.member_offsets.find(member_id);
		if (member_offset == NULL || *member_offset != layout.members[i].offset)
			throw std::runtime_error("zero-offset member layout is inconsistent");
		const BitFieldFact* bit_field = bit_field_fact(member_id);
		if (bit_field != NULL && (bit_field->binding != member_id ||
			bit_field->owner_record != key.named ||
			bit_field->owner_scope != record.scope))
			throw std::runtime_error("zero-offset bit-field identity is invalid");
		if (layout.members[i].offset != 0)
			continue;
		if (type_has_zero_offset_record(member.type, records, visited))
			return true;
	}
	return false;
}

void PA11SemanticModel::append_direct_base_records(NamedRecordId base,
	RecordTypeSet& records) const
{
	// Per-query scratch ancestry; no transitive closure is retained in RecordLayout.
	if (!base.valid() || base.value >= named_.size())
		throw std::runtime_error("zero-offset base identity is invalid");
	NamedRecordId current = base;
	while (current.valid())
	{
		if (current.value >= named_.size() ||
			named_[current.value].kind != NamedKind::Class)
			throw std::runtime_error("zero-offset base ancestry is invalid");
		if (records.find(current) != NULL)
			throw std::runtime_error("cyclic zero-offset base ancestry");
		const NamedRecord& record = named_[current.value];
		const RecordLayout& layout = record_layout(current);
		if (layout.state != RecordLayoutState::Complete)
			throw std::runtime_error("zero-offset base ancestry fact is invalid");
		records.set(current, true);
		if (!record.has_base)
		{
			if (layout.has_direct_base)
				throw std::runtime_error("zero-offset base ancestry fact is invalid");
			break;
		}
		if (!layout.has_direct_base || layout.direct_base.offset != 0 ||
			record.direct_base != layout.direct_base.record ||
			!layout.direct_base.record.valid() ||
			layout.direct_base.record.value >= named_.size())
			throw std::runtime_error("zero-offset base ancestry fact is invalid");
		current = layout.direct_base.record;
	}
}

void PA11SemanticModel::complete_record_members(NamedRecordId record_id,
	const Scope& scope, RecordLayout& layout, bool is_union,
	bool& checkpoint_zero_storage_eligible, std::size_t& offset,
	std::size_t& largest_member, std::size_t& record_alignment)
{
	bool bit_unit_active = false;
	std::size_t bit_unit_offset = 0;
	std::size_t bit_unit_size = 0;
	std::size_t bit_unit_alignment = 0;
	std::size_t bit_unit_width = 0;
	std::size_t bit_cursor = 0;
	const auto flush_bit_unit = [&]() {
		if (!bit_unit_active)
			return;
		if (bit_unit_offset > std::numeric_limits<std::size_t>::max() -
			bit_unit_size)
			throw std::runtime_error("record layout size overflow");
		offset = bit_unit_offset + bit_unit_size;
		bit_unit_active = false;
		bit_unit_offset = 0;
		bit_unit_size = 0;
		bit_unit_alignment = 0;
		bit_unit_width = 0;
		bit_cursor = 0;
	};
	if (record_id.value >= record_member_declarations_.size())
		throw std::runtime_error("record member declaration index is invalid");
	const std::vector<RecordMemberDeclaration>& declarations =
		record_member_declarations_[record_id.value];
	// Filtered scope order is checked against the owner-stable event stream so
	// nested definitions and anonymous aggregate aliases cannot mix records.
	std::size_t expected_event = 0;
	for (std::size_t i = 0; i < scope.bindings.size(); ++i)
	{
		const BindingId member_id = scope.bindings[i];
		const Binding& member = binding(member_id);
		if (member.kind != BindingKind::Variable ||
			is_static_member(member_id))
			continue;
		const BindingSidecar* sidecar = binding_sidecar(member_id);
		if (sidecar != NULL && (sidecar->backing_storage.valid() ||
			sidecar->generated_name_record.valid()))
			continue;
		while (expected_event < declarations.size() &&
			declarations[expected_event].bit_field &&
			!declarations[expected_event].bit.named)
			++expected_event;
		if (expected_event == declarations.size() ||
			declarations[expected_event].binding != member_id)
			throw std::runtime_error("record member declaration order is invalid");
		++expected_event;
	}
	while (expected_event < declarations.size() &&
		declarations[expected_event].bit_field &&
		!declarations[expected_event].bit.named)
		++expected_event;
	if (expected_event != declarations.size())
		throw std::runtime_error("record member declaration is omitted");
	for (std::size_t event = 0; event < declarations.size(); ++event)
	{
		const RecordMemberDeclaration& declaration = declarations[event];
		if (declaration.owner_record != record_id)
			throw std::runtime_error("record member declaration owner is invalid");
		if (declaration.bit_field)
		{
			BitFieldFact fact = declaration.bit;
			if (fact.owner_record != record_id || !fact.declared_type.valid() ||
				!fact.storage_type.valid() || !fact.operation_type.valid() ||
				fact.storage_unit_size == 0 ||
				fact.storage_unit_size > 8 || fact.storage_width == 0 ||
				fact.storage_width > 64 ||
				fact.storage_width != fact.storage_unit_size * 8 ||
				fact.value_width > fact.storage_width ||
				(fact.width != 0 && fact.value_width == 0) ||
				(fact.width != 0 && fact.zero_width) ||
				(fact.width == 0 && !fact.zero_width) ||
				(fact.width == 0 && fact.allocation_size != 0) ||
				(fact.width != 0 && (fact.allocation_size <
					fact.storage_unit_size || fact.allocation_size %
					fact.storage_unit_size != 0)))
				throw std::runtime_error("record bit-field fact is invalid");
			if (fact.width != 0)
			{
				const std::size_t unit_count = fact.width /
					fact.storage_width + (fact.width % fact.storage_width == 0 ? 0 : 1);
				if (unit_count == 0 || unit_count >
					std::numeric_limits<std::size_t>::max() /
					fact.storage_unit_size || fact.allocation_size !=
					unit_count * fact.storage_unit_size)
					throw std::runtime_error("record bit-field allocation is invalid");
			}
			if (fact.named)
			{
				if (!fact.binding.valid())
					throw std::runtime_error("named bit-field binding is missing");
				const BitFieldFact* canonical = bit_field_fact(fact.binding);
				if (canonical == NULL || canonical->owner_record != record_id ||
					canonical->declared_type != fact.declared_type ||
					canonical->width != fact.width)
					throw std::runtime_error("record bit-field binding fact is invalid");
				fact = *canonical;
			}
			if (!type_checkpoint_zero_storage_eligible(fact.declared_type))
				checkpoint_zero_storage_eligible = false;
			const TypeLayout field_layout = type_layout(fact.storage_type);
			if (field_layout.size != fact.storage_unit_size ||
				field_layout.alignment == 0)
				throw std::runtime_error("bit-field storage layout is invalid");
			const std::size_t field_alignment =
				record_id.value < named_.size() &&
				named_[record_id.value].pack_alignment != 0 &&
				field_layout.alignment >
					named_[record_id.value].pack_alignment ?
					named_[record_id.value].pack_alignment : field_layout.alignment;
			if (field_alignment > record_alignment)
				record_alignment = field_alignment;
			if (is_union)
			{
				flush_bit_unit();
				fact.storage_offset = 0;
				fact.bit_offset = 0;
				fact.storage_mask = fact.zero_width ? 0 : fact.value_mask;
				if (!fact.zero_width && fact.allocation_size > largest_member)
					largest_member = fact.allocation_size;
			}
			else if (fact.zero_width)
			{
				flush_bit_unit();
				if (!align_up_checked(offset, field_alignment,
					&fact.storage_offset))
					throw std::runtime_error("record layout size overflow");
				fact.bit_offset = 0;
				fact.storage_mask = 0;
			}
			else if (fact.allocation_size > fact.storage_unit_size)
			{
				// A declaration wider than the physical scalar occupies a private
				// sequence of physical units.  Its value projection remains in the
				// first unit; the remaining bits are padding and cannot be packed
				// with a following declaration.
				flush_bit_unit();
				if (!align_up_checked(offset, field_alignment,
					&fact.storage_offset))
					throw std::runtime_error("record layout size overflow");
				fact.bit_offset = 0;
				if (fact.value_width > fact.storage_width)
					throw std::runtime_error("wide bit-field value projection is invalid");
				fact.storage_mask = fact.value_mask;
				if (fact.storage_offset > std::numeric_limits<std::size_t>::max() -
					fact.allocation_size)
					throw std::runtime_error("record layout size overflow");
				offset = fact.storage_offset + fact.allocation_size;
			}
			else
			{
				if (!bit_unit_active || bit_unit_size != fact.storage_unit_size ||
					bit_unit_alignment != field_alignment ||
					bit_cursor > bit_unit_width ||
					fact.width > bit_unit_width - bit_cursor)
				{
					flush_bit_unit();
					if (!align_up_checked(offset, field_alignment,
						&bit_unit_offset))
						throw std::runtime_error("record layout size overflow");
					bit_unit_active = true;
					bit_unit_size = fact.storage_unit_size;
					bit_unit_alignment = field_alignment;
					bit_unit_width = fact.storage_width;
					bit_cursor = 0;
				}
				fact.storage_offset = bit_unit_offset;
				fact.bit_offset = bit_cursor;
				if (fact.bit_offset > 64 || fact.value_width >
					64 - fact.bit_offset)
					throw std::runtime_error("bit-field mask is too wide");
				fact.storage_mask = fact.value_mask << fact.bit_offset;
				bit_cursor += fact.width;
			}
			if (fact.named)
			{
				set_bit_field_fact(fact.binding, fact);
				layout.members.push_back(RecordLayoutMember(fact.binding,
					fact.storage_offset));
				layout.member_offsets.set(fact.binding, fact.storage_offset);
			}
			continue;
		}

		flush_bit_unit();
		const BindingId member_id = declaration.binding;
		if (!member_id.valid() || member_id.value >= bindings_.size() ||
			member_id.value >= binding_owners_.size() ||
			binding_owners_[member_id.value] != named_[record_id.value].scope)
			throw std::runtime_error("record member binding is invalid");
		const Binding& member = binding(member_id);
		if (member.kind != BindingKind::Variable ||
			is_static_member(member_id))
			throw std::runtime_error("record member declaration is not a field");
		const BindingSidecar* sidecar = binding_sidecar(member_id);
		if (sidecar != NULL && sidecar->has_default_member_initializer)
			checkpoint_zero_storage_eligible = false;
		if (!type_checkpoint_zero_storage_eligible(member.type))
			checkpoint_zero_storage_eligible = false;
		const TypeLayout member_layout = type_layout(member.type);
		if (member_layout.size == 0 || member_layout.alignment == 0)
			throw std::runtime_error("record member has invalid layout");
		std::size_t member_alignment = member_layout.alignment;
		bool explicit_member_alignment = false;
		if (sidecar != NULL && sidecar->has_requested_alignment)
		{
			if (sidecar->requested_alignment == 0 ||
				sidecar->requested_alignment < member_alignment)
				throw std::runtime_error("member alignment is weaker than natural alignment");
			member_alignment = sidecar->requested_alignment;
			explicit_member_alignment = true;
		}
		if (!explicit_member_alignment &&
			named_[record_id.value].pack_alignment != 0 &&
			member_alignment > named_[record_id.value].pack_alignment)
			member_alignment = named_[record_id.value].pack_alignment;
		if (member_alignment > record_alignment)
			record_alignment = member_alignment;
		std::size_t member_offset = 0;
		if (is_union)
		{
			if (member_layout.size > largest_member)
				largest_member = member_layout.size;
		}
		else
		{
			if (!align_up_checked(offset, member_alignment, &member_offset) ||
				member_offset > std::numeric_limits<std::size_t>::max() -
				member_layout.size)
				throw std::runtime_error("record layout size overflow");
			offset = member_offset + member_layout.size;
		}
		layout.members.push_back(RecordLayoutMember(member_id, member_offset));
		layout.member_offsets.set(member_id, member_offset);
	}
	flush_bit_unit();
}

void PA11SemanticModel::complete_record_layout(NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		record_id.value >= record_layouts_.size())
		throw std::runtime_error("invalid PA11 record layout identity");
	NamedRecord& record = named_[record_id.value];
	RecordLayout& layout = record_layouts_[record_id.value];
	if (layout.state == RecordLayoutState::Complete)
		return;
	if (layout.state == RecordLayoutState::Failed)
	{
		layout.empty = false;
		layout.checkpoint_zero_storage_eligible = false;
		layout.pa17_class_value_transfer = ClassValueTransferFact(
			ClassValueTransferState::Failed);
		throw std::runtime_error("record layout previously failed");
	}
	if (layout.state == RecordLayoutState::Computing)
	{
		layout.state = RecordLayoutState::Failed;
		layout.empty = false;
		layout.checkpoint_zero_storage_eligible = false;
		layout.pa17_class_value_transfer = ClassValueTransferFact(
			ClassValueTransferState::Failed);
		throw std::runtime_error("cyclic record layout computation");
	}
	layout.empty = false;
	layout.checkpoint_zero_storage_eligible = false;
	layout.pa17_class_value_transfer = ClassValueTransferFact();
	if (record.kind != NamedKind::Class || !record.defined ||
		!record.scope.valid() || record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id)
	{
		layout.state = RecordLayoutState::Failed;
		layout.pa17_class_value_transfer = ClassValueTransferFact(
			ClassValueTransferState::Failed);
		throw std::runtime_error("record definition is incomplete");
	}
	if (record.has_virtual_member || record.direct_base_virtual ||
		record.has_base != record.direct_base.valid() ||
		(record.has_base && (!record.direct_base.valid() ||
			record.direct_base.value >= named_.size() ||
			record.direct_base == record_id ||
			named_[record.direct_base.value].kind != NamedKind::Class ||
			named_[record.direct_base.value].class_tag == ClassTag::Union)))
	{
		layout.state = RecordLayoutState::Failed;
		layout.empty = false;
		layout.size = 0;
		layout.alignment = 0;
		layout.pa17_class_value_transfer = ClassValueTransferFact(
			ClassValueTransferState::Failed);
		layout.has_direct_base = false;
		layout.direct_base = RecordLayoutBase();
		layout.members.clear();
		layout.member_offsets = FlatIndex<BindingId, std::size_t,
			IdentityHash<BindingId> >();
		return;
	}

	layout.state = RecordLayoutState::Computing;
	layout.size = 0;
	layout.alignment = 0;
	layout.has_direct_base = false;
	layout.direct_base = RecordLayoutBase();
	layout.empty = false;
	layout.members.clear();
	layout.member_offsets = FlatIndex<BindingId, std::size_t,
		IdentityHash<BindingId> >();
	try
	{
		const Scope& scope = scopes_[record.scope.value];
		for (std::size_t i = 0; i < scope.bindings.size(); ++i)
		{
			const BindingId member_id = scope.bindings[i];
			const Binding& member = binding(member_id);
			if (member.kind == BindingKind::Variable &&
				!is_static_member(member_id) &&
				layout_depends_on_template_parameter(member.type))
			{
				layout.state = RecordLayoutState::Incomplete;
				return;
			}
		}
		const bool is_union = record.class_tag == ClassTag::Union;
		bool checkpoint_zero_storage_eligible = !is_union;
		bool direct_base_zero_size = false;
		std::size_t offset = 0;
		std::size_t largest_member = 0;
		std::size_t record_alignment = 1;
		if (record.has_base)
		{
			const RecordLayout& base_layout = record_layout(record.direct_base);
			if (base_layout.state != RecordLayoutState::Complete ||
				base_layout.size == 0 || base_layout.alignment == 0)
				throw std::runtime_error("direct base has no complete layout");
			if (!base_layout.checkpoint_zero_storage_eligible)
				checkpoint_zero_storage_eligible = false;
			direct_base_zero_size = base_layout.empty;
			layout.has_direct_base = true;
			layout.direct_base = RecordLayoutBase(record.direct_base, 0, false);
			if (direct_base_zero_size)
			{
				if (record_id.value >= record_member_declarations_.size())
					throw std::runtime_error(
						"record member declaration index is invalid");
				const std::vector<RecordMemberDeclaration>& declarations =
					record_member_declarations_[record_id.value];
				for (std::size_t i = 0; i < declarations.size(); ++i)
				{
					const RecordMemberDeclaration& declaration = declarations[i];
					if (declaration.owner_record != record_id)
						throw std::runtime_error(
							"record member declaration owner is invalid");
					if (declaration.bit_field)
					{
						// An unnamed or named zero-width bit-field does not
						// introduce an addressable object or storage.  The first
						// nonzero bit-field consumes the offset-zero unit, so no
						// later member can overlap the base.
						if (declaration.bit.zero_width)
							continue;
						break;
					}
					const BindingId member_id = declaration.binding;
					if (!member_id.valid() || member_id.value >= bindings_.size())
						throw std::runtime_error("record member binding is invalid");
					const Binding& member = binding(member_id);
					if (member.kind != BindingKind::Variable ||
						is_static_member(member_id))
						throw std::runtime_error(
							"record member declaration is not a field");
					// This declaration stream is the typed layout owner.  Synthetic
					// names must not hide its first storage event: a backed injected
					// view is checked through its canonical backing type below, and a
					// generated storage binding is checked through member.type.
					const BindingId first_member_id = member_id;
					const TypeId first_member_type = member.type;
					RecordTypeSet base_records;
					append_direct_base_records(record.direct_base, base_records);
					bool same_type_subobject =
						type_has_zero_offset_record(first_member_type,
							base_records);
					const BindingSidecar* sidecar =
						binding_sidecar(first_member_id);
					if (!same_type_subobject && sidecar != NULL &&
						sidecar->backing_storage.valid())
					{
						const BindingId storage = sidecar->backing_storage;
						if (storage.value >= bindings_.size())
							throw std::runtime_error(
								"anonymous backing storage identity is invalid");
						same_type_subobject =
							type_has_zero_offset_record(binding(storage).type,
								base_records);
					}
					if (!same_type_subobject && sidecar != NULL &&
						sidecar->generated_name_record.valid())
					{
						if (sidecar->generated_name_record.value >= named_.size())
							throw std::runtime_error(
								"generated record identity is invalid");
						same_type_subobject =
							type_has_zero_offset_record(
								named_type(sidecar->generated_name_record),
								base_records);
					}
					if (same_type_subobject)
						direct_base_zero_size = false;
					// Every complete non-bit-field member has nonzero size,
					// therefore later members are necessarily after it.
					break;
				}
			}
			offset = direct_base_zero_size ? 0 : base_layout.size;
			record_alignment = base_layout.alignment;
			if (record.pack_alignment != 0 &&
				record_alignment > record.pack_alignment)
				record_alignment = record.pack_alignment;
		}
		complete_record_members(record_id, scope, layout, is_union,
			checkpoint_zero_storage_eligible, offset, largest_member,
			record_alignment);
		if (layout.has_direct_base)
			layout.direct_base.zero_size = direct_base_zero_size;
		if (record.has_requested_alignment)
		{
			if (record.requested_alignment == 0 ||
				record.requested_alignment < record_alignment)
				throw std::runtime_error("class alignment is weaker than natural alignment");
			record_alignment = record.requested_alignment;
		}
		std::size_t unrounded_size = is_union ? largest_member : offset;
		if (unrounded_size == 0)
			unrounded_size = 1;
		if (!align_up_checked(unrounded_size, record_alignment,
			&layout.size))
			throw std::runtime_error("record layout size overflow");
		layout.alignment = record_alignment;
		layout.empty = !is_union && layout.members.empty() && offset == 0 &&
			(!layout.has_direct_base || layout.direct_base.zero_size);
		layout.checkpoint_zero_storage_eligible =
			checkpoint_zero_storage_eligible;
		layout.pa17_class_value_transfer = ClassValueTransferFact();
		layout.state = RecordLayoutState::Complete;
	}
	catch (...)
	{
		layout.state = RecordLayoutState::Failed;
		layout.empty = false;
		layout.checkpoint_zero_storage_eligible = false;
		layout.pa17_class_value_transfer = ClassValueTransferFact(
			ClassValueTransferState::Failed);
		layout.size = 0;
		layout.alignment = 0;
		layout.has_direct_base = false;
		layout.direct_base = RecordLayoutBase();
		layout.members.clear();
		layout.member_offsets = FlatIndex<BindingId, std::size_t,
			IdentityHash<BindingId> >();
		throw;
	}
}

std::size_t PA11SemanticModel::type_size(TypeId type) const
{
	return type_layout(type).size;
}

std::size_t PA11SemanticModel::type_alignment(TypeId type) const
{
	return type_layout(type).alignment;
}

}
