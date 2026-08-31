#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

void PA11SemanticModel::expand_inheriting_constructor_candidates(
	NamedRecordId record_id, ConstructorInitializationContext context,
	std::vector<ValueRef>& candidates, std::vector<NamedRecordId>& active)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag == ClassTag::Union)
		throw std::runtime_error("PA12 inherited candidate record is invalid");
	const NamedRecord record = named_[record_id.value];
	if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id || !record.name.valid())
		throw std::runtime_error("PA12 inherited candidate owner is invalid");
	const ScopeId record_scope = record.scope;
	const NamedRecordId record_base = record.direct_base;
	for (std::size_t i = 0; i < active.size(); ++i)
		if (active[i] == record_id)
			throw std::runtime_error("PA12 inherited candidate relation cycle");
	if (active.size() >= named_.size())
		throw std::runtime_error("PA12 inherited candidate depth is invalid");
	active.push_back(record_id);
	FlatIndex<BindingId, bool, IdentityHash<BindingId> > seen;
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		if (!candidates[i].binding.valid() ||
			candidates[i].binding.value >= bindings_.size() ||
			candidates[i].binding.value >= binding_owners_.size() ||
			candidates[i].scope != record.scope ||
			binding_owners_[candidates[i].binding.value] != record.scope)
			throw std::runtime_error("PA12 inherited candidate identity is invalid");
		if (seen.find(candidates[i].binding) != NULL)
			throw std::runtime_error("PA12 duplicate inherited candidate identity");
		seen.set(candidates[i].binding, true);
	}
	std::vector<InheritingConstructorRelation> relations;
	const NamedRecordSidecar* sidecar = named_record_sidecar(record_id);
	if (sidecar != NULL)
		relations = sidecar->inheriting_constructors;
	FlatIndex<TypeId, bool, IdentityHash<TypeId> > direct_signatures;
	if (!relations.empty())
	{
		const ValueList* direct_values = scopes_[record_scope.value].values.find(
			record.name);
		if (direct_values != NULL)
		{
			FlatIndex<BindingId, bool, IdentityHash<BindingId> > direct_seen;
			for (std::size_t i = 0; i < direct_values->entries.size(); ++i)
			{
				const ValueEntry& entry = direct_values->entries[i];
				const BindingId candidate_id = entry.binding;
				if (!candidate_id.valid() || candidate_id.value >= bindings_.size() ||
					candidate_id.value >= binding_owners_.size() ||
					binding_owners_[candidate_id.value] != record_scope ||
					entry.origin != record_scope)
					throw std::runtime_error(
						"PA12 inherited direct constructor identity is invalid");
				if (direct_seen.find(candidate_id) != NULL)
					throw std::runtime_error(
						"PA12 duplicate inherited direct constructor identity");
				direct_seen.set(candidate_id, true);
				const Binding& candidate = binding(candidate_id);
				const FunctionFact* function = function_fact_for_binding(candidate_id);
				if (function != NULL && function->inheriting_constructor)
					continue;
				if (candidate.kind != BindingKind::Function ||
					!candidate.type.valid() || candidate.type.value >= types_.size() ||
					type_kind(candidate.type) != TypeKind::Function)
					continue;
				const BindingSidecar* candidate_sidecar =
					binding_sidecar(candidate_id);
				if (candidate_sidecar == NULL ||
					candidate_sidecar->constructor_record != record_id)
					continue;
				if (function == NULL || !function->is_constructor ||
					function->binding != candidate_id || function->owner != record_scope ||
					function->constructor_record != record_id)
					throw std::runtime_error(
						"PA12 inherited direct constructor fact is missing");
				direct_signatures.set(candidate.type, true);
			}
		}
	}
	for (std::size_t relation_index = 0;
		relation_index < relations.size(); ++relation_index)
	{
		const NamedRecordId base_record = relations[relation_index].base_record;
		if (!base_record.valid() || base_record.value >= named_.size() ||
			base_record != record_base ||
			named_[base_record.value].kind != NamedKind::Class)
			throw std::runtime_error(
				"PA12 inherited candidate relation is invalid");
		const NamedRecord base = named_[base_record.value];
		if (!base.scope.valid() || base.scope.value >= scopes_.size() ||
			scopes_[base.scope.value].kind != ScopeKind::Class ||
			scopes_[base.scope.value].record != base_record || !base.name.valid())
			throw std::runtime_error(
				"PA12 inherited candidate base owner is invalid");
		const ScopeId base_scope = base.scope;
		const NameId base_name = base.name;

		std::vector<ValueRef> base_direct_candidates;
		const ValueList* base_values =
			scopes_[base_scope.value].values.find(base_name);
		if (base_values != NULL)
		{
			FlatIndex<BindingId, bool, IdentityHash<BindingId> > base_seen;
			for (std::size_t i = 0; i < base_values->entries.size(); ++i)
			{
				const ValueEntry& entry = base_values->entries[i];
				const BindingId candidate_id = entry.binding;
				if (!candidate_id.valid() ||
					candidate_id.value >= bindings_.size() ||
					candidate_id.value >= binding_owners_.size() ||
					binding_owners_[candidate_id.value] != base_scope ||
					entry.origin != base_scope)
					throw std::runtime_error(
						"PA12 inherited candidate value index is invalid");
				if (base_seen.find(candidate_id) != NULL)
					throw std::runtime_error(
						"PA12 duplicate inherited candidate value index");
				base_seen.set(candidate_id, true);
				const Binding& candidate = binding(candidate_id);
				const FunctionFact* function =
					function_fact_for_binding(candidate_id);
				if (function != NULL && function->inheriting_constructor)
					continue;
				if (candidate.kind != BindingKind::Function ||
					!candidate.type.valid() ||
					candidate.type.value >= types_.size() ||
					type_kind(candidate.type) != TypeKind::Function)
					continue;
				const BindingSidecar* candidate_sidecar =
					binding_sidecar(candidate_id);
				if (candidate_sidecar == NULL ||
					candidate_sidecar->constructor_record != base_record ||
					(context == ConstructorInitializationContext::Copy &&
						candidate_sidecar->explicit_constructor))
					continue;
				base_direct_candidates.push_back(
					ValueRef(base_scope, candidate_id));
			}
		}
		expand_inheriting_constructor_candidates(base_record, context,
			base_direct_candidates, active);
		// Recursive publication may grow the base value arena.  Copy the
		// expanded entries before wrapper publication can demand more facts.
		std::vector<ValueEntry> expanded_entries;
		const ValueList* expanded_values =
			scopes_[base_scope.value].values.find(base_name);
		if (expanded_values != NULL)
			expanded_entries = expanded_values->entries;
		for (std::size_t base_index = 0;
			base_index < expanded_entries.size(); ++base_index)
		{
			const ValueEntry& entry = expanded_entries[base_index];
			if (!entry.binding.valid() ||
				entry.binding.value >= bindings_.size() ||
				entry.binding.value >= binding_owners_.size() ||
				binding_owners_[entry.binding.value] != base_scope ||
				entry.origin != base_scope)
				throw std::runtime_error(
					"PA12 inherited candidate value index is invalid");
			const Binding& base_candidate = binding(entry.binding);
			const BindingSidecar* base_sidecar_pointer =
				binding_sidecar(entry.binding);
			if (base_candidate.kind != BindingKind::Function ||
				!base_candidate.type.valid() ||
				base_candidate.type.value >= types_.size() ||
				type_kind(base_candidate.type) != TypeKind::Function ||
				base_sidecar_pointer == NULL ||
				base_sidecar_pointer->constructor_record != base_record)
				continue;
			const BindingSidecar base_sidecar = *base_sidecar_pointer;
			const TypeKey base_signature = types_[base_candidate.type.value];
			if (base_signature.variadic)
				throw std::runtime_error(
					"PA16 variadic inheriting constructors are outside checkpoint");
			if (base_signature.parameters.empty())
				continue;
			const std::size_t minimum_arity =
				inherited_constructor_minimum_arity(entry.binding,
					base_signature);
			for (std::size_t arity = base_signature.parameters.size();
				arity != 0 && arity >= minimum_arity; --arity)
			{
				std::vector<TypeId> wrapper_parameters(
					base_signature.parameters.begin(),
					base_signature.parameters.begin() + arity);
				const TypeId wrapper_type = make_function(wrapper_parameters,
					false, base_signature.result, base_signature.cv);
				if (direct_signatures.find(wrapper_type) != NULL)
					continue;
				if (context == ConstructorInitializationContext::Copy &&
					base_sidecar.explicit_constructor)
					continue;
				const BindingId wrapper = ensure_inheriting_constructor(record_id,
					base_record, entry.binding, arity);
				if (seen.find(wrapper) != NULL)
					continue;
				seen.set(wrapper, true);
				candidates.push_back(ValueRef(record_scope, wrapper));
			}
		}
	}
	active.pop_back();
}

}
