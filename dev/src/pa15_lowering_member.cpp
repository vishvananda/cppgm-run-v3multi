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
			throw std::runtime_error("PA15 static member projection is unsupported");
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

}
