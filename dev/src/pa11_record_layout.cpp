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

const RecordLayout& PA11SemanticModel::record_layout(
	NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		record_id.value >= record_layouts_.size())
		throw std::runtime_error("invalid PA11 record layout identity");
	return record_layouts_[record_id.value];
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
		layout.checkpoint_zero_storage_eligible = false;
		throw std::runtime_error("record layout previously failed");
	}
	if (layout.state == RecordLayoutState::Computing)
	{
		layout.state = RecordLayoutState::Failed;
		layout.checkpoint_zero_storage_eligible = false;
		throw std::runtime_error("cyclic record layout computation");
	}
	layout.checkpoint_zero_storage_eligible = false;
	if (record.kind != NamedKind::Class || !record.defined ||
		!record.scope.valid() || record.scope.value >= scopes_.size())
	{
		layout.state = RecordLayoutState::Failed;
		throw std::runtime_error("record definition is incomplete");
	}
	if (record.has_virtual_member || record.direct_base_virtual ||
		(record.has_base && (!record.direct_base.valid() ||
			record.direct_base.value >= named_.size())))
	{
		layout.state = RecordLayoutState::Failed;
		layout.size = 0;
		layout.alignment = 0;
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
		bool checkpoint_zero_storage_eligible = !is_union && !record.has_base;
		std::size_t offset = 0;
		std::size_t largest_member = 0;
		std::size_t record_alignment = 1;
		if (record.has_base)
		{
			const RecordLayout& base_layout = record_layout(record.direct_base);
			if (base_layout.state != RecordLayoutState::Complete ||
				base_layout.size == 0 || base_layout.alignment == 0)
				throw std::runtime_error("direct base has no complete layout");
			layout.has_direct_base = true;
			layout.direct_base = RecordLayoutBase(record.direct_base, 0);
			offset = base_layout.size;
			record_alignment = base_layout.alignment;
		}
		for (std::size_t i = 0; i < scope.bindings.size(); ++i)
		{
			const BindingId member_id = scope.bindings[i];
			const Binding& member = binding(member_id);
			if (member.kind != BindingKind::Variable ||
				is_static_member(member_id))
				continue;
			const BindingSidecar* sidecar = binding_sidecar(member_id);
			if (sidecar != NULL && sidecar->has_default_member_initializer)
				checkpoint_zero_storage_eligible = false;
			if (!type_checkpoint_zero_storage_eligible(member.type))
				checkpoint_zero_storage_eligible = false;
			const TypeLayout member_layout = type_layout(member.type);
			if (member_layout.size == 0 || member_layout.alignment == 0)
				throw std::runtime_error("record member has invalid layout");
			std::size_t member_alignment = member_layout.alignment;
			if (sidecar != NULL && sidecar->has_requested_alignment)
			{
				if (sidecar->requested_alignment == 0 ||
					sidecar->requested_alignment < member_alignment)
					throw std::runtime_error("member alignment is weaker than natural alignment");
				member_alignment = sidecar->requested_alignment;
			}
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
				if (!align_up_checked(offset, member_alignment,
					&member_offset) ||
					member_offset > std::numeric_limits<std::size_t>::max() -
					member_layout.size)
					throw std::runtime_error("record layout size overflow");
				offset = member_offset + member_layout.size;
			}
			layout.members.push_back(RecordLayoutMember(member_id,
				member_offset));
			layout.member_offsets.set(member_id, member_offset);
		}

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
		layout.checkpoint_zero_storage_eligible =
			checkpoint_zero_storage_eligible;
		layout.state = RecordLayoutState::Complete;
	}
	catch (...)
	{
		layout.state = RecordLayoutState::Failed;
		layout.checkpoint_zero_storage_eligible = false;
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
