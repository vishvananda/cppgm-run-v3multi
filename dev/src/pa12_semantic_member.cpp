#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

NamedRecordId PA11SemanticModel::class_record_for_object_type(TypeId type) const
{
	if (!type.valid() || type.value >= types_.size())
		return NamedRecordId();
	type = strip_cv_type(type);
	while (type.valid() && type.value < types_.size() &&
		type_kind(type) == TypeKind::Array)
	{
		const TypeKey& array = types_[type.value];
		if (array.unknown_bound)
			return NamedRecordId();
		type = strip_cv_type(array.child);
	}
	if (!type.valid() || type.value >= types_.size() ||
		type_kind(type) != TypeKind::Named)
		return NamedRecordId();
	const NamedRecordId record = types_[type.value].named;
	return record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Class ? record : NamedRecordId();
}

bool PA11SemanticModel::implicit_default_type_empty(TypeId type,
	std::vector<NamedRecordId>& active) const
{
	if (!type.valid() || type.value >= types_.size())
		return false;
	const TypeKey& key = types_[type.value];
	switch (key.kind)
	{
	case TypeKind::Cv:
		return implicit_default_type_empty(key.child, active);
	case TypeKind::Fundamental:
		return key.fundamental != FundamentalType::Void;
	case TypeKind::Pointer:
	case TypeKind::MemberPointer:
		return true;
	case TypeKind::Array:
		return !key.unknown_bound &&
			implicit_default_type_empty(key.child, active);
	case TypeKind::Named:
	{
		if (!key.named.valid() || key.named.value >= named_.size())
			return false;
		const NamedRecord& record = named_[key.named.value];
		if (record.kind == NamedKind::Enum)
			return true;
		if (record.kind != NamedKind::Class)
			return false;
		return implicit_default_record_empty(key.named, active);
	}
	case TypeKind::LvalueReference:
	case TypeKind::RvalueReference:
	case TypeKind::Function:
		return false;
	}
	return false;
}

bool PA11SemanticModel::implicit_default_record_empty(
	NamedRecordId record_id, std::vector<NamedRecordId>& active) const
{
	if (!record_id.valid() || record_id.value >= named_.size())
		return false;
	const NamedRecord& record = named_[record_id.value];
	if (record.kind != NamedKind::Class || !record.defined ||
		record.has_virtual_member ||
		!record.scope.valid() || record.scope.value >= scopes_.size())
		return false;
	for (std::size_t i = 0; i < active.size(); ++i)
		if (active[i] == record_id)
			return false;
	active.push_back(record_id);
	if (record.has_base &&
		(record.direct_base_virtual || !record.direct_base.valid() ||
			record.direct_base.value >= named_.size() ||
			!implicit_default_record_empty(record.direct_base, active)))
	{
		active.pop_back();
		return false;
	}
	const Scope& scope = scopes_[record.scope.value];
	for (std::size_t i = 0; i < scope.bindings.size(); ++i)
	{
		const BindingId member_id = scope.bindings[i];
		const Binding& member = binding(member_id);
		if (member.kind != BindingKind::Variable ||
			is_static_member(member_id))
			continue;
		const BindingSidecar* sidecar = binding_sidecar(member_id);
		if (sidecar != NULL && sidecar->has_default_member_initializer)
		{
			active.pop_back();
			return false;
		}
		if (!implicit_default_type_empty(member.type, active))
		{
			active.pop_back();
			return false;
		}
	}
	active.pop_back();
	return true;
}

bool PA11SemanticModel::implicit_default_constructor_supported(
	NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag == ClassTag::Union)
		return false;
	if (!named_[record_id.value].defined ||
		!named_[record_id.value].scope.valid() ||
		named_[record_id.value].scope.value >= scopes_.size())
		throw std::runtime_error(
			"PA12 implicit default construction requires a complete class");
	std::vector<NamedRecordId> active;
	if (!implicit_default_type_supported(named_type(record_id), active))
		throw std::runtime_error(
			"PA12 implicit default construction is unavailable");
	return true;
}

bool PA11SemanticModel::implicit_default_type_supported(TypeId type,
	std::vector<NamedRecordId>& active) const
{
	if (!type.valid() || type.value >= types_.size())
		return false;
	type = strip_cv_type(type);
	const TypeKey& key = types_[type.value];
	switch (key.kind)
	{
	case TypeKind::Cv:
		return implicit_default_type_supported(key.child, active);
	case TypeKind::Fundamental:
		return key.fundamental != FundamentalType::Void;
	case TypeKind::Pointer:
	case TypeKind::MemberPointer:
		return true;
	case TypeKind::Array:
		return !key.unknown_bound &&
			implicit_default_type_supported(key.child, active);
	case TypeKind::Named:
	{
		if (!key.named.valid() || key.named.value >= named_.size())
			return false;
		const NamedRecord& record = named_[key.named.value];
		if (record.kind == NamedKind::Enum)
			return true;
		if (record.kind != NamedKind::Class ||
			record.class_tag == ClassTag::Union || !record.defined ||
			!record.scope.valid() || record.scope.value >= scopes_.size())
			return false;
		if (scopes_[record.scope.value].kind != ScopeKind::Class ||
			scopes_[record.scope.value].record != key.named ||
			record.direct_base_virtual ||
			(!record.has_base && record.direct_base.valid()) ||
			(record.has_base && (!record.direct_base.valid() ||
				record.direct_base.value >= named_.size() ||
				named_[record.direct_base.value].kind != NamedKind::Class)))
			throw std::runtime_error("PA12 implicit constructor owner is invalid");
		for (std::size_t i = 0; i < active.size(); ++i)
			if (active[i] == key.named)
				return false;
		active.push_back(key.named);
		const BindingId default_ctor = default_constructor_binding(key.named);
		bool result = default_ctor.valid() ?
			function_declaration_kind(default_ctor) !=
			FunctionDeclarationKind::Deleted :
			!has_constructor_declaration(key.named);
		const NamedRecord& current = named_[key.named.value];
		if (result && current.has_base)
			result = implicit_default_type_supported(
				named_type(current.direct_base), active);
		if (result)
		{
			const Scope& scope = scopes_[current.scope.value];
			for (std::size_t i = 0; i < scope.bindings.size() && result; ++i)
			{
				const BindingId member_id = scope.bindings[i];
				if (!member_id.valid() || member_id.value >= bindings_.size() ||
					member_id.value >= binding_owners_.size() ||
					binding_owners_[member_id.value] != record.scope)
					throw std::runtime_error(
						"PA12 implicit constructor member identity is invalid");
				const Binding& member = binding(member_id);
				if (member.kind != BindingKind::Variable ||
					is_static_member(member_id))
					continue;
				const BindingSidecar* sidecar = binding_sidecar(member_id);
				if (sidecar != NULL && sidecar->has_default_member_initializer)
					continue;
				result = implicit_default_type_supported(member.type, active);
			}
		}
		active.pop_back();
		return result;
	}
	case TypeKind::LvalueReference:
	case TypeKind::RvalueReference:
	case TypeKind::Function:
		return false;
	}
	return false;
}

BindingId PA11SemanticModel::default_constructor_binding(
	NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		return BindingId();
	const NamedRecordSidecar* sidecar = named_record_sidecar(record_id);
	if (sidecar == NULL || !sidecar->default_constructor_binding.valid())
		return BindingId();
	const BindingId result = sidecar->default_constructor_binding;
	if (result.value >= bindings_.size() || result.value >= binding_owners_.size() ||
		!named_[record_id.value].scope.valid() ||
		binding_owners_[result.value] != named_[record_id.value].scope)
		throw std::runtime_error("PA12 default constructor identity is invalid");
	const Binding& candidate = binding(result);
	if (candidate.kind != BindingKind::Function ||
		type_kind(candidate.type) != TypeKind::Function)
		throw std::runtime_error("PA12 default constructor binding is invalid");
	const BindingSidecar* binding_fact = binding_sidecar(result);
	if (binding_fact == NULL || binding_fact->constructor_record != record_id)
		throw std::runtime_error("PA12 default constructor owner is invalid");
	return result;
}

bool PA11SemanticModel::has_constructor_declaration(
	NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		return false;
	const NamedRecordSidecar* sidecar = named_record_sidecar(record_id);
	return sidecar != NULL && sidecar->has_constructor_declaration;
}

BindingId PA11SemanticModel::destructor_binding(NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		return BindingId();
	const NamedRecordSidecar* sidecar = named_record_sidecar(record_id);
	if (sidecar == NULL || !sidecar->destructor_binding.valid())
		return BindingId();
	const BindingId result = sidecar->destructor_binding;
	if (result.value >= bindings_.size() || result.value >= binding_owners_.size() ||
		!named_[record_id.value].scope.valid() ||
		binding_owners_[result.value] != named_[record_id.value].scope)
		throw std::runtime_error("PA16 destructor identity is invalid");
	const Binding& candidate = binding(result);
	if (candidate.kind != BindingKind::Function ||
		type_kind(candidate.type) != TypeKind::Function)
		throw std::runtime_error("PA16 destructor binding is invalid");
	const BindingSidecar* binding_fact = binding_sidecar(result);
	if (binding_fact == NULL || binding_fact->destructor_record != record_id)
		throw std::runtime_error("PA16 destructor owner is invalid");
	return result;
}

bool PA11SemanticModel::has_destructor_declaration(
	NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		return false;
	const NamedRecordSidecar* sidecar = named_record_sidecar(record_id);
	return sidecar != NULL && sidecar->has_destructor_declaration;
}

bool PA11SemanticModel::aggregate_class_initialization_supported(
	NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag == ClassTag::Union)
		return false;
	const NamedRecord& record = named_[record_id.value];
	if (!record.name.valid() || !record.scope.valid() ||
		record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id)
		throw std::runtime_error("PA12 aggregate constructor owner is invalid");
	// An implicitly generated constructor is not a user-declared constructor
	// and must not change aggregate eligibility after it is materialized.
	const bool declared = has_constructor_declaration(record_id);
	if (!declared)
		return true;
	const ValueList* constructor_values =
		scopes_[record.scope.value].values.find(record.name);
	FlatIndex<BindingId, bool, IdentityHash<BindingId> > seen;
	if (constructor_values == NULL)
		throw std::runtime_error(
			"PA12 aggregate constructor value index is missing");
	bool found_constructor = false;
	for (std::size_t i = 0; i < constructor_values->entries.size(); ++i)
	{
		const ValueEntry& entry = constructor_values->entries[i];
		const BindingId candidate_id = entry.binding;
		if (!candidate_id.valid() || candidate_id.value >= bindings_.size() ||
			candidate_id.value >= binding_owners_.size() ||
			binding_owners_[candidate_id.value] != record.scope ||
			entry.origin != record.scope)
			throw std::runtime_error(
				"PA12 aggregate constructor value index identity is invalid");
		if (seen.find(candidate_id) != NULL)
			throw std::runtime_error(
				"PA12 duplicate aggregate constructor value index entry");
		seen.set(candidate_id, true);
		const Binding& candidate = binding(candidate_id);
		const BindingSidecar* sidecar = binding_sidecar(candidate_id);
		if (candidate.kind != BindingKind::Function ||
			!candidate.type.valid() || candidate.type.value >= types_.size() ||
			type_kind(candidate.type) != TypeKind::Function || sidecar == NULL ||
			sidecar->constructor_record != record_id)
			continue;
		found_constructor = true;
		// Explicitly defaulted and deleted constructors are not user-provided
		// in the C++11 aggregate rule.  A normal declaration, including a
		// declaration without an in-class body, is user-provided and therefore
		// routes braced construction through constructor selection.
		if (function_declaration_kind(candidate_id) ==
			FunctionDeclarationKind::Normal)
			return false;
	}
	if (!found_constructor)
		throw std::runtime_error(
			"PA12 aggregate constructor identity is missing");
	return true;
}

bool PA11SemanticModel::classify_constructor_runtime(NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		return false;
	if (constructor_runtime_states_.size() != named_.size() ||
		constructor_runtime_results_.size() != named_.size() ||
		constructor_runtime_invalid_.size() != named_.size())
		throw std::runtime_error("PA12 constructor runtime cache is missing");
	const std::size_t index = record_id.value;
	if (constructor_runtime_states_[index] ==
		ConstructorRuntimeCacheState::Complete)
		return constructor_runtime_results_[index] != 0;
	if (constructor_runtime_states_[index] ==
		ConstructorRuntimeCacheState::InProgress)
	{
		constructor_runtime_invalid_[index] = 1;
		return false;
	}
	constructor_runtime_states_[index] =
		ConstructorRuntimeCacheState::InProgress;
	const NamedRecord& record = named_[index];
	if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id ||
		record.direct_base_virtual || (!record.has_base && record.direct_base.valid()) ||
		(record.has_base && (!record.direct_base.valid() ||
			record.direct_base.value >= named_.size() ||
			named_[record.direct_base.value].kind != NamedKind::Class)))
		throw std::runtime_error("PA12 constructor runtime owner is invalid");
	bool result = false;
	const BindingId default_ctor = default_constructor_binding(record_id);
	if (default_ctor.valid())
	{
		const FunctionFact* function = function_fact_for_binding(default_ctor);
		// A non-synthetic default constructor is an emitted user body (or a
		// deleted declaration, which must remain a demanded/error boundary).
		if (function == NULL || !function->synthetic)
			result = true;
	}
	if (!result && record.scope.valid() && record.scope.value < scopes_.size())
	{
		// A constructor with trailing defaults is still a runtime constructor
		// for a no-argument initialization.  Keep this probe typed and local to
		// the owning class; the full selector publishes the chosen conversion
		// and default facts when the action is formed.
		const ScopeId class_scope = record.scope;
		const ValueList* constructor_values = record.name.valid() ?
			scopes_[class_scope.value].values.find(record.name) : NULL;
		FlatIndex<BindingId, bool, IdentityHash<BindingId> > seen;
		if (constructor_values != NULL)
		{
			for (std::size_t i = 0; i < constructor_values->entries.size() &&
				!result; ++i)
			{
				const ValueEntry& entry = constructor_values->entries[i];
				const BindingId candidate_id = entry.binding;
				if (!candidate_id.valid() || candidate_id.value >= bindings_.size() ||
					candidate_id.value >= binding_owners_.size() ||
					binding_owners_[candidate_id.value] != class_scope ||
					entry.origin != class_scope)
					throw std::runtime_error(
						"PA12 constructor runtime value index identity is invalid");
				if (seen.find(candidate_id) != NULL)
					throw std::runtime_error(
						"PA12 duplicate constructor runtime value index entry");
				seen.set(candidate_id, true);
				const Binding& candidate = binding(candidate_id);
				const BindingSidecar* sidecar = binding_sidecar(candidate_id);
				if (candidate.kind != BindingKind::Function ||
					!candidate.type.valid() || candidate.type.value >= types_.size() ||
					type_kind(candidate.type) != TypeKind::Function || sidecar == NULL ||
					sidecar->constructor_record != record_id)
					continue;
				const TypeKey signature = types_[candidate.type.value];
				std::size_t required = signature.parameters.size();
				while (required != 0 && function_default_argument(candidate_id,
					required - 1).valid())
					--required;
				if (required == 0)
					result = true;
			}
		}
	}
	if (!result && record.has_base)
	{
		result = classify_constructor_runtime(record.direct_base);
		if (record.direct_base.valid() && record.direct_base.value <
			constructor_runtime_invalid_.size() &&
			constructor_runtime_invalid_[record.direct_base.value] != 0)
			constructor_runtime_invalid_[index] = 1;
	}
	if (!result && record.scope.valid() && record.scope.value < scopes_.size())
	{
		const Scope& scope = scopes_[record.scope.value];
		for (std::size_t i = 0; i < scope.bindings.size() && !result; ++i)
		{
			const BindingId member_id = scope.bindings[i];
			if (!member_id.valid() || member_id.value >= bindings_.size() ||
				member_id.value >= binding_owners_.size() ||
				binding_owners_[member_id.value] != record.scope)
				throw std::runtime_error("PA12 constructor runtime member identity is invalid");
			const Binding& member = binding(member_id);
			if (member.kind != BindingKind::Variable ||
				is_static_member(member_id))
				continue;
			const BindingSidecar* sidecar = binding_sidecar(member_id);
			if (sidecar != NULL && sidecar->has_default_member_initializer)
			{
				result = true;
				break;
			}
			const NamedRecordId member_record =
				class_record_for_object_type(member.type);
			if (member_record.valid())
			{
				result = classify_constructor_runtime(member_record);
				if (member_record.value < constructor_runtime_invalid_.size() &&
					constructor_runtime_invalid_[member_record.value] != 0)
					constructor_runtime_invalid_[index] = 1;
			}
		}
	}
	constructor_runtime_states_[index] =
		ConstructorRuntimeCacheState::Complete;
	constructor_runtime_results_[index] = result ? 1 : 0;
	return result;
}

bool PA11SemanticModel::constructor_requires_runtime(NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		return false;
	const bool result = classify_constructor_runtime(record_id);
	if (record_id.value >= constructor_runtime_invalid_.size())
		throw std::runtime_error("PA12 constructor runtime cache is invalid");
	if (constructor_runtime_invalid_[record_id.value] != 0)
		throw std::runtime_error("PA12 constructor runtime dependency cycle");
	return result;
}

bool PA11SemanticModel::classify_destructor_runtime(NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		return false;
	if (destructor_runtime_states_.size() != named_.size() ||
		destructor_runtime_results_.size() != named_.size() ||
		destructor_runtime_invalid_.size() != named_.size())
		throw std::runtime_error("PA16 destructor runtime cache is missing");
	const std::size_t index = record_id.value;
	if (destructor_runtime_states_[index] ==
		ConstructorRuntimeCacheState::Complete)
		return destructor_runtime_results_[index] != 0;
	if (destructor_runtime_states_[index] ==
		ConstructorRuntimeCacheState::InProgress)
	{
		destructor_runtime_invalid_[index] = 1;
		return false;
	}
	destructor_runtime_states_[index] =
		ConstructorRuntimeCacheState::InProgress;
	const NamedRecord& record = named_[index];
	if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id)
		throw std::runtime_error("PA16 destructor runtime owner is invalid");
	bool result = destructor_binding(record_id).valid();
	if (!result && record.has_base)
	{
		if (!record.direct_base.valid() || record.direct_base.value >= named_.size() ||
			named_[record.direct_base.value].kind != NamedKind::Class)
			throw std::runtime_error("PA16 destructor base identity is invalid");
		result = classify_destructor_runtime(record.direct_base);
		if (destructor_runtime_invalid_[record.direct_base.value] != 0)
			destructor_runtime_invalid_[index] = 1;
	}
	if (!result)
	{
		const Scope& scope = scopes_[record.scope.value];
		for (std::size_t i = 0; i < scope.bindings.size(); ++i)
		{
			const BindingId member_id = scope.bindings[i];
			if (!member_id.valid() || member_id.value >= bindings_.size() ||
				member_id.value >= binding_owners_.size() ||
				binding_owners_[member_id.value] != record.scope)
				throw std::runtime_error("PA16 destructor member identity is invalid");
			const Binding& member = binding(member_id);
			if (member.kind != BindingKind::Variable || is_static_member(member_id))
				continue;
			const NamedRecordId member_record =
				class_record_for_object_type(member.type);
			if (!member_record.valid())
				continue;
			result = classify_destructor_runtime(member_record);
			if (destructor_runtime_invalid_[member_record.value] != 0)
				destructor_runtime_invalid_[index] = 1;
			if (result)
				break;
		}
	}
	destructor_runtime_states_[index] =
		ConstructorRuntimeCacheState::Complete;
	destructor_runtime_results_[index] = result ? 1 : 0;
	return result;
}

bool PA11SemanticModel::destructor_requires_runtime(NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		return false;
	const bool result = classify_destructor_runtime(record_id);
	if (record_id.value >= destructor_runtime_invalid_.size())
		throw std::runtime_error("PA16 destructor runtime cache is invalid");
	if (destructor_runtime_invalid_[record_id.value] != 0)
		throw std::runtime_error("PA16 destructor runtime dependency cycle");
	return result;
}

bool PA11SemanticModel::direct_base_chain(TypeId object,
	std::vector<NamedRecordId>* chain) const
{
	if (chain == NULL)
		return false;
	chain->clear();
	if (!object.valid() || object.value >= types_.size())
		return false;
	const TypeId record_type = strip_cv_type(expression_object_type(object));
	if (!record_type.valid() || type_kind(record_type) != TypeKind::Named)
		return false;
	const NamedRecordId record_id = named_record_for_type(record_type);
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		return false;
	const NamedRecord& initial = named_[record_id.value];
	if (!initial.scope.valid() || initial.scope.value >= scopes_.size() ||
		scopes_[initial.scope.value].kind != ScopeKind::Class ||
		scopes_[initial.scope.value].record != record_id)
		throw std::runtime_error("invalid PA16 class scope metadata");

	// The semantic model permits one direct non-virtual base.  Keep the
	// relation walk typed and validate it before either lookup or lowering uses
	// it.  Floyd's check keeps malformed metadata from turning a bounded walk
	// into an unbounded retry without allocating a whole-program visited set.
	const auto next_base = [this](NamedRecordId current) -> NamedRecordId
	{
		if (!current.valid() || current.value >= named_.size() ||
			named_[current.value].kind != NamedKind::Class)
			throw std::runtime_error("invalid PA16 base record identity");
		const NamedRecord& record = named_[current.value];
		if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
			scopes_[record.scope.value].kind != ScopeKind::Class ||
			scopes_[record.scope.value].record != current)
			throw std::runtime_error("invalid PA16 class scope metadata");
		if (record.direct_base_virtual)
			throw std::runtime_error("virtual inheritance is outside PA16");
		if (!record.has_base)
		{
			if (record.direct_base.valid())
				throw std::runtime_error("invalid PA16 direct base metadata");
			return NamedRecordId();
		}
		if (!record.direct_base.valid() || record.direct_base.value >= named_.size())
			throw std::runtime_error("invalid PA16 direct base metadata");
		const NamedRecord& base = named_[record.direct_base.value];
		if (base.kind != NamedKind::Class || !base.scope.valid() ||
			base.scope.value >= scopes_.size() ||
			scopes_[base.scope.value].kind != ScopeKind::Class ||
			scopes_[base.scope.value].record != record.direct_base ||
			base.class_tag == ClassTag::Union)
			throw std::runtime_error("invalid PA16 direct base class metadata");
		return record.direct_base;
	};

	NamedRecordId slow = record_id;
	NamedRecordId fast = record_id;
	while (slow.valid() && fast.valid())
	{
		slow = next_base(slow);
		if (!slow.valid())
			break;
		fast = next_base(fast);
		if (!fast.valid())
			break;
		fast = next_base(fast);
		if (!fast.valid())
			break;
		if (slow == fast)
			throw std::runtime_error("cyclic PA16 direct base metadata");
	}

	NamedRecordId current = record_id;
	while (true)
	{
		const NamedRecordId base = next_base(current);
		if (!base.valid())
			break;
		chain->push_back(base);
		current = base;
	}
	return true;
}

bool PA11SemanticModel::member_base_path(TypeId object, ScopeId target,
	std::vector<NamedRecordId>* path) const
{
	if (path != NULL)
		path->clear();
	if (!target.valid() || target.value >= scopes_.size() ||
		scopes_[target.value].kind != ScopeKind::Class)
		return false;
	const ScopeId actual = class_scope_for_type(object);
	if (!actual.valid())
		return false;
	if (actual == target)
		return true;
	std::vector<NamedRecordId> chain;
	if (!direct_base_chain(object, &chain))
		return false;
	for (std::size_t i = 0; i < chain.size(); ++i)
	{
		if (path != NULL)
			path->push_back(chain[i]);
		if (named_[chain[i].value].scope == target)
			return true;
	}
	if (path != NULL)
		path->clear();
	return false;
}

bool PA11SemanticModel::base_path_accessible(TypeId object, ScopeId target,
	ScopeId access_scope) const
{
	if (!access_scope.valid() || !target.valid() ||
		target.value >= scopes_.size() ||
		scopes_[target.value].kind != ScopeKind::Class)
		return false;
	const ScopeId actual = class_scope_for_type(object);
	if (!actual.valid() || actual.value >= scopes_.size() ||
		scopes_[actual.value].kind != ScopeKind::Class)
		return false;
	if (actual == target)
		return true;
	const auto class_is_derived_from = [this](ScopeId derived,
		ScopeId owner) -> bool
	{
		if (!derived.valid() || derived.value >= scopes_.size() ||
			scopes_[derived.value].kind != ScopeKind::Class || !owner.valid())
			return false;
		NamedRecordId current = scopes_[derived.value].record;
		for (std::size_t steps = 0; current.valid() &&
			steps < named_.size(); ++steps)
		{
			if (current.value >= named_.size() ||
				named_[current.value].kind != NamedKind::Class)
				return false;
			const NamedRecord& record = named_[current.value];
			if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
				scopes_[record.scope.value].kind != ScopeKind::Class ||
				scopes_[record.scope.value].record != current)
				return false;
			if (record.scope == owner)
				return true;
			if (!record.has_base || !record.direct_base.valid() ||
				record.direct_base.value >= named_.size())
				return false;
			current = record.direct_base;
		}
		return false;
	};
	const auto scope_can_access = [this, &class_is_derived_from](
		ScopeId owner, ScopeId access, MemberAccess edge_access) -> bool
	{
		ScopeId cursor = access;
		for (std::size_t steps = 0; cursor.valid() &&
			steps < scopes_.size(); ++steps)
		{
			if (cursor.value >= scopes_.size())
				return false;
			if (cursor == owner)
				return true;
			if (edge_access == MemberAccess::Protected &&
				scopes_[cursor.value].kind == ScopeKind::Class &&
				class_is_derived_from(cursor, owner))
				return true;
			if (scopes_[cursor.value].kind == ScopeKind::Function)
			{
				const BindingId* function_binding =
					function_bindings_.find(cursor);
				if (function_binding != NULL)
				{
					const BindingSidecar* sidecar =
						binding_sidecar(*function_binding);
					if (sidecar != NULL)
						for (std::size_t i = 0;
							i < sidecar->friend_records.size(); ++i)
						{
							const NamedRecordId friend_record =
								sidecar->friend_records[i];
							if (friend_record.valid() &&
								friend_record.value < named_.size() &&
								named_[friend_record.value].kind == NamedKind::Class &&
								(named_[friend_record.value].scope == owner ||
									(edge_access == MemberAccess::Protected &&
									class_is_derived_from(
										named_[friend_record.value].scope, owner))))
								return true;
						}
				}
			}
			cursor = scopes_[cursor.value].parent;
		}
		return false;
	};
	NamedRecordId current = named_record_for_type(
		strip_cv_type(expression_object_type(object)));
	for (std::size_t steps = 0; current.valid() &&
		steps < named_.size(); ++steps)
	{
		if (current.value >= named_.size() ||
			named_[current.value].kind != NamedKind::Class)
			return false;
		const NamedRecord& record = named_[current.value];
		if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
			scopes_[record.scope.value].kind != ScopeKind::Class ||
			scopes_[record.scope.value].record != current || !record.has_base ||
			!record.direct_base.valid() ||
			record.direct_base.value >= named_.size())
			return false;
		if (record.direct_base_access != MemberAccess::Public &&
			!scope_can_access(record.scope, access_scope,
				record.direct_base_access))
			return false;
		current = record.direct_base;
		if (named_[current.value].scope == target)
			return true;
	}
	return false;
}

bool PA11SemanticModel::member_object_qualification_convertible(TypeId object,
	TypeId required) const
{
	if (!object.valid() || object.value >= types_.size() ||
		!required.valid() || required.value >= types_.size())
		return false;
	const TypeId actual_record = strip_cv_type(expression_object_type(object));
	const TypeId required_record = strip_cv_type(expression_object_type(required));
	if (type_kind(actual_record) != TypeKind::Named ||
		type_kind(required_record) != TypeKind::Named)
		return false;
	return (cv_qualifiers(object) & ~cv_qualifiers(required)) == 0;
}
bool PA11SemanticModel::member_object_convertible(TypeId object,
	TypeId required, ScopeId member_scope,
	std::vector<NamedRecordId>* path, ScopeId access_scope) const
{
	if (path != NULL)
		path->clear();
	if (!object.valid() || object.value >= types_.size() ||
		!required.valid() || required.value >= types_.size() ||
		!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class)
		return false;
	const TypeId required_record = strip_cv_type(expression_object_type(required));
	if (type_kind(required_record) != TypeKind::Named ||
		class_scope_for_type(required_record) != member_scope ||
		!member_base_path(object, member_scope, path))
		return false;
	if (class_scope_for_type(object) != member_scope && access_scope.valid() &&
		!base_path_accessible(object, member_scope, access_scope))
		return false;
	return member_object_qualification_convertible(object, required);
}

TypeId PA11SemanticModel::member_access_type(TypeId object, TypeId member)
{
	const unsigned int qualifiers = cv_qualifiers(expression_object_type(object));
	return qualifiers == 0 ? member : make_cv(member, qualifiers);
}
PA11SemanticModel::MemberLookup PA11SemanticModel::member_lookup(
	TypeId object, NameId name) const
{
	MemberLookup result;
	const TypeId record_type = strip_cv_type(expression_object_type(object));
	if (type_kind(record_type) != TypeKind::Named)
		return result;
	const ScopeId class_scope = class_scope_for_type(record_type);
	if (!class_scope.valid() || class_scope.value >= scopes_.size() ||
		scopes_[class_scope.value].kind != ScopeKind::Class)
		return result;

	// A member lookup is deliberately direct at each class scope.  This keeps
	// the declaration set at the first owning class authoritative: a derived
	// declaration hides every base declaration, while a using-import or an
	// ambiguous set is not guessed at from the canonical binding alone.
	const auto inspect_scope = [this, name](ScopeId scope) -> MemberLookup
	{
		MemberLookup found;
		if (!scope.valid() || scope.value >= scopes_.size() ||
			scopes_[scope.value].kind != ScopeKind::Class)
			return found;
		const Scope& current = scopes_[scope.value];
		const ValueList* values = current.values.find(name);
		if (values != NULL && !values->entries.empty())
		{
			bool imported = false;
			bool all_functions = true;
			for (std::size_t i = 0; i < values->entries.size(); ++i)
			{
				const ValueEntry& entry = values->entries[i];
				if (!entry.binding.valid() || entry.binding.value >=
					bindings_.size() || entry.binding.value >= binding_owners_.size())
					throw std::runtime_error("PA12 member value owner is invalid");
				if (entry.origin != scope)
					imported = true;
				const Binding& candidate = binding(entry.binding);
				all_functions = all_functions &&
					candidate.kind == BindingKind::Function &&
					type_kind(candidate.type) == TypeKind::Function;
			}
			if (!all_functions && (imported || values->entries.size() != 1))
			{
				found.kind = MemberLookupKind::Blocked;
				found.owner = scope;
				return found;
			}
			found.kind = MemberLookupKind::Value;
			found.owner = scope;
			if (values->entries.size() == 1)
			{
				found.binding = values->entries.front().binding;
				if (found.binding.valid())
					found.type = binding(found.binding).type;
			}
			return found;
		}
		if (current.using_types.find(name) != NULL)
		{
			found.kind = MemberLookupKind::Blocked;
			found.owner = scope;
			return found;
		}
		const TypeId* type = current.types.find(name);
		if (type != NULL)
		{
			found.kind = type->valid() ? MemberLookupKind::Type :
				MemberLookupKind::Blocked;
			found.owner = scope;
			found.type = *type;
		}
		return found;
	};

	result = inspect_scope(class_scope);
	if (result.kind != MemberLookupKind::None)
		return result;
	std::vector<NamedRecordId> bases;
	if (!direct_base_chain(record_type, &bases))
		return result;
	for (std::size_t i = 0; i < bases.size(); ++i)
	{
		const ScopeId base_scope = named_[bases[i].value].scope;
		MemberLookup found = inspect_scope(base_scope);
		if (found.kind == MemberLookupKind::None)
			continue;
		found.base_path.assign(bases.begin(), bases.begin() + i + 1);
		return found;
	}
	return result;
}

PA11SemanticModel::MemberLookup PA11SemanticModel::unqualified_member_lookup(
	TypeId object, NameId name, ScopeId start) const
{
	MemberLookup result;
	const ScopeId class_scope = class_scope_for_type(object);
	if (!class_scope.valid() || !start.valid() || start.value >= scopes_.size())
		return result;
	const SourcePoint point = lookup_source_point(start);
	ScopeId cursor = start;
	while (cursor.valid() && cursor.value < scopes_.size() &&
		cursor != class_scope)
	{
		begin_lookup();
		std::vector<ValueRef> values;
		if (lookup_value_graph(cursor, name, &values, false, point))
		{
			for (std::size_t i = 0; i < values.size(); ++i)
				if (values[i].scope != cursor)
					return result;
			result.kind = MemberLookupKind::Value;
			result.owner = cursor;
			if (values.size() == 1)
			{
				result.binding = values.front().binding;
				if (result.binding.valid())
					result.type = binding(result.binding).type;
			}
			return result;
		}
		begin_lookup();
		const TypeId type = lookup_type_graph(cursor, name, false, point);
		if (type.valid())
		{
			result.kind = MemberLookupKind::Type;
			result.owner = cursor;
			result.type = type;
			return result;
		}
		cursor = scopes_[cursor.value].parent;
	}
	if (cursor != class_scope)
		return result;
	return member_lookup(object, name);
}

ExprInfo PA11SemanticModel::semantic_static_data_member(
	const PA10AstNode& node, ScopeId scope, const NamePath& path,
	const MemberLookup& selection, bool* claimed)
{
	if (claimed == NULL)
		throw std::runtime_error("PA12 static member claim output is missing");
	*claimed = false;
	if (!selection.owner.valid() || selection.owner.value >= scopes_.size() ||
		scopes_[selection.owner.value].kind != ScopeKind::Class)
		return ExprInfo();
	// Types and blocked names are authoritative class lookup results.  Function
	// overload sets are intentionally left to the function-id/call path.
	if (selection.kind == MemberLookupKind::Type ||
		selection.kind == MemberLookupKind::Blocked)
	{
		*claimed = true;
		return ExprInfo();
	}
	if (selection.kind != MemberLookupKind::Value ||
		!selection.binding.valid())
		return ExprInfo();
	if (selection.binding.value >= bindings_.size())
		throw std::runtime_error("PA12 static member binding is invalid");
	const Binding& member = binding(selection.binding);
	if (member.kind != BindingKind::Variable)
		return ExprInfo();
	*claimed = true;
	if (!is_static_member(selection.binding))
		return ExprInfo();
	if (!member_accessible(selection.binding, selection.owner, scope, TypeId()))
		throw std::runtime_error("PA12 record member is inaccessible");
	TypeId member_type = member.type;
	if (type_kind(member_type) == TypeKind::LvalueReference ||
		type_kind(member_type) == TypeKind::RvalueReference)
		member_type = types_[member_type.value].child;
	SemanticFact fact(SemanticFactKind::IdExpression, member_type,
		SemanticValueCategory::Lvalue, &node);
	fact.binding = selection.binding;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, path);
	return ExprInfo(result, member_type, SemanticValueCategory::Lvalue, false);
}

ExprInfo PA11SemanticModel::semantic_unqualified_static_data(
	const PA10AstNode& node, ScopeId scope, const NamePath& path, bool* claimed)
{
	if (claimed == NULL)
		throw std::runtime_error("PA12 static member claim output is missing");
	*claimed = false;
	if (path.global || path.components.size() != 1)
		return ExprInfo();
	ScopeId cursor = scope;
	for (std::size_t steps = 0; cursor.valid() &&
		cursor.value < scopes_.size() && steps < scopes_.size(); ++steps)
	{
		const Scope& current = scopes_[cursor.value];
		if (current.kind == ScopeKind::Function)
		{
			if (current.parent.valid() && current.parent.value < scopes_.size() &&
				scopes_[current.parent.value].kind == ScopeKind::Class)
			{
				const ScopeId member_scope = current.parent;
				const Scope& owner = scopes_[member_scope.value];
				const MemberLookup selection = unqualified_member_lookup(
					named_type(owner.record), path.last(), scope);
				return semantic_static_data_member(node, scope, path, selection,
					claimed);
			}
			return ExprInfo();
		}
		cursor = current.parent;
	}
	return ExprInfo();
}

ExprInfo PA11SemanticModel::semantic_qualified_static_data(
	const PA10AstNode& node, ScopeId scope, const NamePath& path, bool* claimed)
{
	if (claimed == NULL)
		throw std::runtime_error("PA12 static member claim output is missing");
	*claimed = false;
	if (path.components.size() <= 1)
		return ExprInfo();
	NamePath qualifier;
	qualifier.global = path.global;
	qualifier.components.assign(path.components.begin(), path.components.end() - 1);
	const TypeId qualifier_type = lookup_type_path(qualifier, scope);
	if (!qualifier_type.valid() || !class_scope_for_type(qualifier_type).valid())
		return ExprInfo();
	const MemberLookup selection = member_lookup(qualifier_type, path.last());
	return semantic_static_data_member(node, scope, path, selection, claimed);
}

ExprInfo PA11SemanticModel::semantic_static_data(const PA10AstNode& node,
	ScopeId scope, const NamePath& path)
{
	bool claimed = false;
	ExprInfo result = semantic_unqualified_static_data(node, scope, path,
		&claimed);
	if (result.fact.valid())
		return result;
	if (claimed)
		throw std::runtime_error("PA12 class member requires an object");
	result = semantic_qualified_static_data(node, scope, path, &claimed);
	if (result.fact.valid())
		return result;
	if (claimed)
		throw std::runtime_error("PA12 class member requires an object");
	return result;
}

std::vector<ValueRef> PA11SemanticModel::member_function_candidates_in_scope(
	ScopeId member_scope, NameId name) const
{
	std::vector<ValueRef> result;
	if (!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class)
		return result;
	const ValueList* values = scopes_[member_scope.value].values.find(name);
	if (values == NULL)
		return result;
	for (std::size_t i = 0; i < values->entries.size(); ++i)
	{
		const BindingId candidate_id = values->entries[i].binding;
		const Binding& candidate = binding(candidate_id);
		if (candidate.kind == BindingKind::Function &&
			type_kind(candidate.type) == TypeKind::Function &&
			!is_static_member(candidate_id))
		{
			const ScopeId owner = values->entries[i].origin.valid() ?
				values->entries[i].origin : member_scope;
			if (!owner.valid() || owner.value >= scopes_.size() ||
				scopes_[owner.value].kind != ScopeKind::Class ||
				candidate_id.value >= binding_owners_.size() ||
				binding_owners_[candidate_id.value] != owner)
				throw std::runtime_error("PA12 member candidate owner is invalid");
			result.push_back(ValueRef(owner, candidate_id));
		}
	}
	return result;
}
std::vector<ValueRef> PA11SemanticModel::static_member_function_candidates_in_scope(
	ScopeId member_scope, NameId name) const
{
	std::vector<ValueRef> result;
	if (!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class)
		return result;
	const ValueList* values = scopes_[member_scope.value].values.find(name);
	if (values == NULL)
		return result;
	for (std::size_t i = 0; i < values->entries.size(); ++i)
	{
		const BindingId candidate_id = values->entries[i].binding;
		const Binding& candidate = binding(candidate_id);
		if (candidate.kind == BindingKind::Function &&
			type_kind(candidate.type) == TypeKind::Function &&
			is_static_member(candidate_id))
		{
			const ScopeId owner = values->entries[i].origin.valid() ?
				values->entries[i].origin : member_scope;
			if (!owner.valid() || owner.value >= scopes_.size() ||
				scopes_[owner.value].kind != ScopeKind::Class ||
				candidate_id.value >= binding_owners_.size() ||
				binding_owners_[candidate_id.value] != owner)
				throw std::runtime_error("PA12 static member candidate owner is invalid");
			result.push_back(ValueRef(owner, candidate_id));
		}
	}
	return result;
}
bool PA11SemanticModel::qualified_static_member_candidates(
	const PA10AstNode& node, ScopeId scope, std::vector<ValueRef>* candidates,
	ScopeId* owner)
{
	if (candidates == NULL || owner == NULL)
		throw std::runtime_error("PA12 static member lookup outputs are missing");
	candidates->clear();
	*owner = ScopeId();
	const PA10AstNode* callee = &node;
	while (callee->kind == PA10NodeKind::ParenthesizedExpression &&
		callee->children.size() == 1)
		callee = &callee->children.front();
	if (callee->kind != PA10NodeKind::IdExpression || callee->has_token ||
		has_template_id(*callee))
		return false;
	const NamePath path = name_path(*callee);
	if (path.components.size() <= 1)
		return false;
	NamePath qualifier;
	qualifier.global = path.global;
	qualifier.components.assign(path.components.begin(), path.components.end() - 1);
	const TypeId qualifier_type = lookup_type_path(qualifier, scope);
	if (!qualifier_type.valid())
		return false;
	const ScopeId class_scope = class_scope_for_type(qualifier_type);
	if (!class_scope.valid())
		return false;
	const MemberLookup selection = member_lookup(qualifier_type, path.last());
	*owner = selection.owner.valid() ? selection.owner : class_scope;
	const BindingId this_id = implicit_this_binding(scope);
	const bool has_implicit_object = this_id.valid();
	bool qualifier_is_implicit_object_base = false;
	if (has_implicit_object)
	{
		const Binding& this_binding = binding(this_id);
		const TypeId this_pointer = strip_cv_type(expression_object_type(
			this_binding.type));
		if (this_binding.kind == BindingKind::Parameter &&
			type_kind(this_pointer) == TypeKind::Pointer)
			qualifier_is_implicit_object_base = member_base_path(
				types_[this_pointer.value].child, class_scope, NULL);
	}
	if (selection.kind != MemberLookupKind::Value || !selection.owner.valid())
		return !qualifier_is_implicit_object_base;
	*candidates = static_member_function_candidates_in_scope(selection.owner,
		path.last());
	if (qualifier_is_implicit_object_base)
	{
		// A current/base class qualifier is still an implicit-object member
		// call in a non-static member body.  Keep the class claim out of the
		// direct static path so the member probe can rank the complete owning
		// declaration set, including non-static candidates.
		return false;
	}
	if (!candidates->empty())
		return true;
	// The boolean result claims the class-qualified spelling even when the
	// first owning declaration set contains no static callable.  The caller
	// must not reopen namespace/value lookup in that case: a non-static member,
	// type, blocked name, or malformed overload set is a failed class lookup.
	return true;
}
std::vector<ValueRef> PA11SemanticModel::direct_call_candidates(
	const PA10AstNode& node, ScopeId scope,
	bool qualified_class_member,
	const std::vector<ValueRef>& qualified_static_candidates)
{
	if (qualified_class_member)
		return qualified_static_candidates;
	const PA10AstNode* callee = &node;
	while (callee->kind == PA10NodeKind::ParenthesizedExpression &&
		callee->children.size() == 1)
		callee = &callee->children.front();
	if (callee->kind != PA10NodeKind::IdExpression)
		return std::vector<ValueRef>();
	const NamePath path = name_path(*callee);
	std::vector<ValueRef> candidates;
	if (path.components.size() == 1 && !path.global &&
		!implicit_this_binding(scope).valid())
	{
		ScopeId static_member_scope;
		bool function_scope_seen = false;
		ScopeId cursor = scope;
		for (std::size_t steps = 0; cursor.valid() &&
			cursor.value < scopes_.size() && steps < scopes_.size(); ++steps)
		{
			const Scope& current = scopes_[cursor.value];
			if (current.kind == ScopeKind::Function)
			{
				function_scope_seen = true;
				if (!current.implicit_object_binding.valid() && current.parent.valid() &&
					current.parent.value < scopes_.size() &&
					scopes_[current.parent.value].kind == ScopeKind::Class)
					static_member_scope = current.parent;
				break;
			}
			cursor = current.parent;
		}
		if (!function_scope_seen && cursor.valid())
			throw std::runtime_error("PA12 static call scope ancestry is invalid");
		if (static_member_scope.valid())
		{
			// Search block/function lexical declarations up to, but not past,
			// the owning class.  An outer namespace function must not reopen
			// lookup after a class member (including an inherited one) claims
			// the unqualified spelling.
			const SourcePoint point = lookup_source_point(scope);
			prepare_unqualified_lookup(scope);
			bool lexical_found = false;
			cursor = scope;
			for (std::size_t steps = 0; cursor.valid() &&
				cursor.value < scopes_.size() && steps < scopes_.size() &&
				cursor != static_member_scope; ++steps)
			{
				begin_lookup();
				std::vector<ValueRef> found;
				if (lookup_value_graph(cursor, path.last(), &found, false, point))
				{
					candidates.swap(found);
					lexical_found = true;
					break;
				}
				std::vector<ScopeId> targets;
				append_effective_using_targets(cursor, &targets, point);
				for (std::size_t i = 0; i < targets.size(); ++i)
				{
					begin_lookup();
					std::vector<ValueRef> nominated;
					if (lookup_value_graph(targets[i], path.last(), &nominated,
						true, point))
					{
						candidates.insert(candidates.end(), nominated.begin(),
							nominated.end());
						lexical_found = true;
					}
				}
				if (lexical_found)
					break;
				cursor = scopes_[cursor.value].parent;
			}
			if (!lexical_found)
			{
				const TypeId member_type = named_type(
					scopes_[static_member_scope.value].record);
				const MemberLookup selection = member_lookup(member_type,
					path.last());
				if (selection.kind == MemberLookupKind::Value &&
					selection.owner.valid())
					candidates = static_member_function_candidates_in_scope(
						selection.owner, path.last());
				else if (selection.kind == MemberLookupKind::None)
					candidates = lookup_value_path(path, scope);
			}
		}
		else
		{
			candidates = lookup_value_path(path, scope);
			std::vector<ValueRef> static_candidates;
			for (std::size_t i = 0; i < candidates.size(); ++i)
			{
				const ValueRef& candidate = candidates[i];
				if (candidate.scope.valid() && candidate.scope.value < scopes_.size() &&
					scopes_[candidate.scope.value].kind == ScopeKind::Class &&
					!is_static_member(candidate.binding))
					continue;
				static_candidates.push_back(candidate);
			}
			candidates.swap(static_candidates);
		}
	}
	else
		candidates = lookup_value_path(path, scope);
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const Binding& candidate = binding(candidates[i].binding);
		if (candidate.kind != BindingKind::Function ||
			type_kind(candidate.type) != TypeKind::Function)
			return std::vector<ValueRef>();
	}
	return candidates;
}
void PA11SemanticModel::validate_direct_static_member_call(
	const ValueRef& selected, bool qualified_class_member,
	ScopeId qualified_static_scope, ScopeId access_scope) const
{
	const bool selected_class_static = selected.scope.valid() &&
		selected.scope.value < scopes_.size() &&
		scopes_[selected.scope.value].kind == ScopeKind::Class &&
		is_static_member(selected.binding);
	if (qualified_class_member)
	{
		if (!selected_class_static || selected.scope != qualified_static_scope ||
			!member_accessible(selected.binding, selected.scope, access_scope,
				TypeId()))
			throw std::runtime_error("PA12 static member call is inaccessible");
	}
	else if (selected_class_static && !member_accessible(selected.binding,
		selected.scope, access_scope, TypeId()))
		throw std::runtime_error("PA12 static member call is inaccessible");
}
bool PA11SemanticModel::member_accessible(BindingId binding_id,
	ScopeId member_scope, ScopeId access_scope, TypeId object) const
{
	const MemberAccess access = member_access(binding_id);
	if (access == MemberAccess::Public)
		return true;
	ScopeId cursor = access_scope;
	std::vector<ScopeId> access_classes;
	for (std::size_t lexical_steps = 0;
		cursor.valid() && cursor.value < scopes_.size() &&
		lexical_steps < scopes_.size(); ++lexical_steps)
	{
		if (cursor == member_scope)
			return true;
		if (scopes_[cursor.value].kind == ScopeKind::Function)
		{
			const BindingId* function_binding = function_bindings_.find(cursor);
			if (function_binding != NULL)
			{
				const BindingSidecar* function_sidecar =
					binding_sidecar(*function_binding);
				if (function_sidecar != NULL)
					for (std::size_t i = 0;
						i < function_sidecar->friend_records.size(); ++i)
					{
						const NamedRecordId friend_record =
							function_sidecar->friend_records[i];
						if (friend_record.valid() && friend_record.value < named_.size() &&
							named_[friend_record.value].kind == NamedKind::Class &&
							named_[friend_record.value].scope == member_scope)
							return true;
					}
			}
		}
		if (scopes_[cursor.value].kind == ScopeKind::Class)
			access_classes.push_back(cursor);
		cursor = scopes_[cursor.value].parent;
	}
	// A valid but out-of-range cursor, or a valid cursor left after the
	// scope-vector bound was exhausted, is malformed ancestry.  Do not let
	// classes collected before that point grant protected access.
	if (cursor.valid())
		return false;
	// Protected members are also accessible from a member body of a derived
	// class.  A nested class is itself a member, so its enclosing class scopes
	// are eligible access classes as well.  Keep this narrow: private members
	// still require the owning class, and an unrelated/non-class access scope
	// cannot acquire protected access.
	if (access != MemberAccess::Protected || access_classes.empty())
		return false;
	// Protected access has two independent typed requirements.  First, an
	// eligible access class must derive from the declaring owner.  The
	// canonical named type is already interned; looking it up by TypeKey keeps
	// this const path O(1) without manufacturing a parallel type identity.
	// The object-expression restriction is specific to protected non-static
	// members.  Protected static members still require the derived access
	// class proof above, but their spelling through an object does not impose a
	// second object-type relation.
	const bool static_member = is_static_member(binding_id);
	const BindingSidecar* sidecar = binding_sidecar(binding_id);
	const bool constructor = sidecar != NULL &&
		sidecar->constructor_record.valid() &&
		sidecar->constructor_record.value < named_.size() &&
		named_[sidecar->constructor_record.value].kind == NamedKind::Class &&
		named_[sidecar->constructor_record.value].scope == member_scope;
	// C++ additionally restricts the object expression: it must have the
	// accessing class type (or a further-derived type), not merely the
	// declaring base type.  This prevents Derived::f(Base&) from acquiring
	// Base's protected member through an arbitrary Base object.
	const TypeId object_record = (static_member || constructor) ? TypeId() :
		strip_cv_type(expression_object_type(object));
	if (!static_member && !constructor && (!object_record.valid() ||
		type_kind(object_record) != TypeKind::Named ||
		!class_scope_for_type(object_record).valid()))
		return false;
	for (std::size_t i = 0; i < access_classes.size(); ++i)
	{
		const ScopeId access_class = access_classes[i];
		if (access_class.value >= scopes_.size() ||
			!scopes_[access_class.value].record.valid())
			continue;
		const NamedRecordId access_record = scopes_[access_class.value].record;
		if (access_record.value >= named_.size() ||
			named_[access_record.value].kind != NamedKind::Class ||
			!named_[access_record.value].scope.valid() ||
			named_[access_record.value].scope != access_class)
			continue;
		TypeKey access_key;
		access_key.kind = TypeKind::Named;
		access_key.named = access_record;
		const TypeId* access_type = type_ids_.find(access_key);
		if (access_type == NULL ||
			!member_base_path(*access_type, member_scope, NULL))
			continue;
		if (static_member || constructor ||
			member_base_path(object_record, access_class, NULL))
			return true;
	}
	return false;
}
BindingId PA11SemanticModel::implicit_this_binding(ScopeId scope) const
{
	ScopeId cursor = scope;
	while (cursor.valid() && cursor.value < scopes_.size())
	{
		const Scope& current = scopes_[cursor.value];
		if (current.kind == ScopeKind::Function)
			return current.implicit_object_binding;
		cursor = current.parent;
	}
	return BindingId();
}
ExprInfo PA11SemanticModel::semantic_this_expression(
	const PA10AstNode& node, ScopeId scope)
{
	return semantic_this_expression(node, implicit_this_binding(scope));
}
ExprInfo PA11SemanticModel::semantic_this_expression(
	const PA10AstNode& node, BindingId this_id)
{
	if (!this_id.valid())
		throw std::runtime_error("PA12 this is outside a non-static member function");
	const Binding& this_binding = binding(this_id);
	if (this_binding.kind != BindingKind::Parameter ||
		type_kind(this_binding.type) != TypeKind::Pointer)
		throw std::runtime_error("PA12 implicit this binding is invalid");
	SemanticFact fact(SemanticFactKind::IdExpression, this_binding.type,
		SemanticValueCategory::Prvalue, &node);
	fact.binding = this_id;
	const SemanticFactId result = make_semantic_fact(fact);
	return ExprInfo(result, this_binding.type, SemanticValueCategory::Prvalue,
		false);
}

ExprInfo PA11SemanticModel::semantic_member_expression(
	const PA10AstNode& node, ScopeId scope)
{
	if (node.kind != PA10NodeKind::MemberExpression ||
		node.children.size() != 2 || !node.has_token ||
		(node.token != SimpleTokenType::OP_DOT &&
			node.token != SimpleTokenType::OP_ARROW) ||
		node.children[1].kind != PA10NodeKind::Identifier)
		throw std::runtime_error("PA12 invalid member expression");
	const ExprInfo object = semantic_expression(node.children.front(), scope);
	const NamePath member_name = name_path(node.children.back());
	if (member_name.global || member_name.components.size() != 1)
		throw std::runtime_error("PA12 qualified member is unsupported");
	TypeId record_object = object.type;
	if (node.token == SimpleTokenType::OP_ARROW)
	{
		const TypeId pointer = strip_cv_type(expression_object_type(object.type));
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 arrow operand is not a pointer");
		record_object = types_[pointer.value].child;
		const TypeId pointer_value = strip_top_cv_type(object.type);
		record_builtin_conversion(object, pointer_value);
	}
	else if (type_kind(strip_cv_type(expression_object_type(record_object))) !=
		TypeKind::Named)
		throw std::runtime_error("PA12 dot operand is not a record");
	const MemberLookup selection = member_lookup(record_object,
		member_name.last());
	if (selection.kind != MemberLookupKind::Value ||
		!selection.binding.valid() || !selection.owner.valid() ||
		selection.owner.value >= scopes_.size() ||
		scopes_[selection.owner.value].kind != ScopeKind::Class)
		throw std::runtime_error("PA12 unknown or ambiguous record member");
	const BindingId member_id = selection.binding;
	const Binding& member = binding(member_id);
	if (member.kind != BindingKind::Variable)
		throw std::runtime_error("PA12 member function access is unsupported");
	if (!member_accessible(member_id, selection.owner, scope, record_object))
		throw std::runtime_error("PA12 record member is inaccessible");
	const TypeId type = member_access_type(record_object, member.type);
	SemanticFact fact(SemanticFactKind::MemberExpression, type,
		SemanticValueCategory::Lvalue, &node);
	fact.token = node.token;
	fact.contains_member_value = true;
	fact.binding = member_id;
	fact.selected_binding = member_id;
	fact.selected_scope = selection.owner;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, member_name);
	set_semantic_children(result,
		std::vector<SemanticFactId>(1, object.fact));
	return ExprInfo(result, type, SemanticValueCategory::Lvalue, false);
}

ExprInfo PA11SemanticModel::semantic_member_call_expression(
	const PA10AstNode& node, const PA10AstNode& member_node, ScopeId scope)
{
	if (member_node.kind != PA10NodeKind::MemberExpression ||
		member_node.children.size() != 2 || !member_node.has_token ||
		(member_node.token != SimpleTokenType::OP_DOT &&
			member_node.token != SimpleTokenType::OP_ARROW) ||
		member_node.children[1].kind != PA10NodeKind::Identifier)
		throw std::runtime_error("PA12 invalid member call expression");
	const NamePath member_name = name_path(member_node.children.back());
	if (member_name.global || member_name.components.size() != 1)
		throw std::runtime_error("PA12 qualified member call is unsupported");
	const ExprInfo object = semantic_expression(member_node.children.front(),
		scope);
	TypeId record_object = object.type;
	TypeId actual_object = object.type;
	if (member_node.token == SimpleTokenType::OP_ARROW)
	{
		const TypeId pointer = strip_cv_type(expression_object_type(object.type));
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 arrow operand is not a pointer");
		record_object = types_[pointer.value].child;
		actual_object = record_object;
	}
	else
	{
		record_object = strip_cv_type(expression_object_type(record_object));
		actual_object = expression_object_type(object.type);
		if (type_kind(record_object) != TypeKind::Named)
			return ExprInfo();
	}
	const MemberLookup selection = member_lookup(record_object,
		member_name.last());
	if (selection.kind == MemberLookupKind::Type ||
		selection.kind == MemberLookupKind::Blocked)
		throw std::runtime_error("PA12 member call name is unsupported");
	if (selection.kind != MemberLookupKind::Value ||
		!selection.owner.valid() || selection.owner.value >= scopes_.size() ||
		scopes_[selection.owner.value].kind != ScopeKind::Class)
		return ExprInfo();
	const std::vector<ValueRef> candidates =
		member_function_candidates_in_scope(selection.owner, member_name.last());
	if (candidates.empty())
		// Keep the existing indirect-call path for a selected data member
		// (including function-pointer fields).  The callee is still the typed
		// member expression, so it cannot fall back to an unrelated free name;
		// a non-callable field is rejected by that path.
		return ExprInfo();
	return semantic_member_call_with_object(node, scope,
		member_node.token, object, actual_object,
		selection.owner, candidates, &selection.base_path);
}

ExprInfo PA11SemanticModel::semantic_member_call_with_object(
	const PA10AstNode& node, ScopeId scope,
	SimpleTokenType member_token, const ExprInfo& object,
	TypeId actual_object, ScopeId member_scope,
	const std::vector<ValueRef>& candidates,
	const std::vector<NamedRecordId>* base_path, bool allow_static,
	BindingId implicit_this)
{
	if (member_token != SimpleTokenType::OP_DOT &&
		member_token != SimpleTokenType::OP_ARROW)
		throw std::runtime_error("PA12 invalid member call token");
	if (!member_scope.valid())
		return ExprInfo();
	if (candidates.empty())
		return ExprInfo();
	if (allow_static && !implicit_this.valid())
		throw std::runtime_error("PA12 implicit member object is missing");
	if (!allow_static && !object.fact.valid())
		throw std::runtime_error("PA12 member call object is missing");
	if (!allow_static && member_token == SimpleTokenType::OP_ARROW)
	{
		const TypeId pointer = strip_cv_type(expression_object_type(object.type));
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 member call arrow operand is not a pointer");
		const TypeId pointer_value = strip_top_cv_type(object.type);
		record_builtin_conversion(object, pointer_value);
	}
	if (base_path == NULL && !member_base_path(actual_object, member_scope, NULL))
		throw std::runtime_error("PA12 member call object owner mismatch");
	if (base_path != NULL && base_path->empty() &&
		class_scope_for_type(actual_object) != member_scope)
		throw std::runtime_error("PA12 member call object owner mismatch");

	const PA10AstNode& argument_node = node.children.back();
	std::vector<ExprInfo> arguments;
	for (std::size_t i = 0; i < argument_node.children.size(); ++i)
	{
		if (target_function_id(argument_node.children[i], scope) != NULL)
			arguments.push_back(ExprInfo());
		else
			arguments.push_back(semantic_expression(argument_node.children[i], scope));
	}
	struct CandidateScore
	{
		ValueRef value;
		TypeId type;
		bool static_member;
		std::size_t object_base_distance;
		unsigned int object_cv;
		std::vector<unsigned int> ranks;
	};
	std::vector<CandidateScore> viable;
	const unsigned int ellipsis_rank = std::numeric_limits<unsigned int>::max() / 4;
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const ValueRef& candidate_ref = candidates[i];
		if (!candidate_ref.binding.valid() || candidate_ref.binding.value >=
			bindings_.size() || candidate_ref.binding.value >= binding_owners_.size() ||
			!candidate_ref.scope.valid() || candidate_ref.scope.value >= scopes_.size() ||
			scopes_[candidate_ref.scope.value].kind != ScopeKind::Class ||
			binding_owners_[candidate_ref.binding.value] != candidate_ref.scope)
			throw std::runtime_error("PA12 member candidate ownership is invalid");
		const Binding& candidate = binding(candidate_ref.binding);
		if (candidate.kind != BindingKind::Function || !candidate.type.valid() ||
			candidate.type.value >= types_.size() ||
			type_kind(candidate.type) != TypeKind::Function)
			throw std::runtime_error("PA12 member candidate type is invalid");
		const TypeKey& function = types_[candidate.type.value];
		const bool static_member = is_static_member(candidate_ref.binding);
		if (static_member && !allow_static)
			continue;
		TypeId required_object;
		std::vector<NamedRecordId> object_path;
		if (!static_member)
		{
			required_object = member_object_type(candidate.type,
				candidate_ref.scope);
			if (!required_object.valid() ||
				class_scope_for_type(required_object) != candidate_ref.scope ||
				!member_object_convertible(actual_object, required_object,
					candidate_ref.scope, &object_path))
				continue;
		}
		std::size_t required = function.parameters.size();
		while (required != 0 && function_default_argument(
			candidate_ref.binding, required - 1).valid())
			--required;
		if (arguments.size() < required ||
			(!function.variadic && arguments.size() > function.parameters.size()))
			continue;
		CandidateScore score;
		score.value = candidate_ref;
		score.type = candidate.type;
		score.static_member = static_member;
		score.object_base_distance = static_member ? 0 : object_path.size();
		score.object_cv = static_member ? 0 : cv_qualifiers(required_object) &
			~cv_qualifiers(actual_object);
		score.ranks.reserve(arguments.size());
		bool arguments_viable = true;
		for (std::size_t arg = 0; arg < arguments.size(); ++arg)
		{
			if (arg >= function.parameters.size())
			{
				if (!arguments[arg].fact.valid())
				{
					arguments_viable = false;
					break;
				}
				score.ranks.push_back(ellipsis_rank);
				continue;
			}
			ConversionChoice choice;
			if (arguments[arg].fact.valid())
				choice = conversion_for(arguments[arg], function.parameters[arg],
					semantic_facts_[arguments[arg].fact.value].source, scope);
			else
			{
				const PA10AstNode* function_id = target_function_id(
					argument_node.children[arg], scope);
				if (function_id != NULL)
				{
					const FunctionIdResolution resolution = resolve_function_id_target(
						*function_id, scope, function.parameters[arg]);
					choice = resolution.conversion;
				}
			}
			if (!choice.valid)
			{
				arguments_viable = false;
				break;
			}
			score.ranks.push_back(choice.rank);
		}
		if (arguments_viable)
			viable.push_back(score);
	}
	if (viable.empty())
		throw std::runtime_error("PA12 no viable member call");
	const auto better = [](const CandidateScore& left,
		const CandidateScore& right) -> bool
	{
		bool strict = false;
		// N3485 [over.match.best] gives a static member no implicit-object
		// conversion sequence: its ICS1 is neither better nor worse than
		// another candidate's.  Compare qualification only between two
		// non-static candidates; explicit argument ranks still compare for all
		// candidates.  For non-static candidates, qualification conversions form
		// a subset ordering: an exact object match beats added cv, const beats
		// const volatile, and const and volatile remain incomparable.
		if (!left.static_member && !right.static_member &&
			left.object_base_distance != right.object_base_distance)
		{
			if (left.object_base_distance > right.object_base_distance)
				return false;
			strict = true;
		}
		if (!left.static_member && !right.static_member &&
			left.object_cv != right.object_cv)
		{
			if ((left.object_cv & ~right.object_cv) != 0)
				return false;
			strict = true;
		}
		if (left.ranks.size() != right.ranks.size())
			return false;
		for (std::size_t i = 0; i < left.ranks.size(); ++i)
		{
			if (left.ranks[i] > right.ranks[i])
				return false;
			if (left.ranks[i] < right.ranks[i])
				strict = true;
		}
		return strict;
	};
	std::size_t best_index = 0;
	for (std::size_t i = 1; i < viable.size(); ++i)
		if (better(viable[i], viable[best_index]))
			best_index = i;
	for (std::size_t i = 0; i < viable.size(); ++i)
		if (i != best_index && !better(viable[best_index], viable[i]))
			throw std::runtime_error("PA12 ambiguous member call");
	const ValueRef selected = viable[best_index].value;
	const TypeId selected_type = viable[best_index].type;
	const bool selected_static = viable[best_index].static_member;
	if (!member_accessible(selected.binding, selected.scope, scope,
		selected_static ? TypeId() : actual_object))
		throw std::runtime_error("PA12 member call is inaccessible");
	if (function_declaration_kind(selected.binding) ==
		FunctionDeclarationKind::Deleted)
		throw std::runtime_error("PA12 member call selects deleted function");
	const TypeKey& function = types_[selected_type.value];
	const std::size_t explicit_count = arguments.size();
	for (std::size_t arg = explicit_count;
		arg < function.parameters.size(); ++arg)
	{
		const SemanticFactId default_fact = function_default_argument(
			selected.binding, arg);
		if (!default_fact.valid())
			throw std::runtime_error("PA12 selected member default is missing");
		const SemanticFact& value = semantic_facts_[default_fact.value];
		arguments.push_back(ExprInfo(default_fact, value.type, value.category,
			false));
	}
	const std::size_t fixed_explicit = explicit_count < function.parameters.size() ?
		explicit_count : function.parameters.size();
	for (std::size_t arg = 0; arg < fixed_explicit; ++arg)
	{
		if (!arguments[arg].fact.valid())
			arguments[arg] = semantic_expression_for_target(
				argument_node.children[arg], scope, function.parameters[arg]);
		arguments[arg] = apply_context_conversion(arguments[arg],
			function.parameters[arg],
			semantic_facts_[arguments[arg].fact.value].source, scope);
	}
	apply_call_argument_conversions(arguments, selected_type, scope);
	const TypeId result_type = function_result_type(selected_type);
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	SemanticFact fact(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	fact.has_callee = true;
	fact.has_implicit_object = !selected_static;
	fact.selected_binding = selected.binding;
	fact.selected_scope = selected.scope;
	fact.callable_type = selected_static ? selected_type :
		member_function_expression_type(selected_type, selected.scope,
			selected.binding);
	if (type_kind(fact.callable_type) != TypeKind::Function)
		throw std::runtime_error("PA12 member call signature is invalid");
	std::vector<SemanticFactId> children;
	if (!selected_static)
	{
		fact.token = member_token;
		children.push_back(object.fact.valid() ? object.fact :
			semantic_this_expression(node, implicit_this).fact);
	}
	for (std::size_t i = 0; i < arguments.size(); ++i)
		children.push_back(arguments[i].fact);
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_children(result, children);
	return ExprInfo(result, result_type, result_category, false);
}

ExprInfo PA11SemanticModel::semantic_member_call_with_implicit_object(
	const PA10AstNode& node, ScopeId scope, BindingId this_binding,
	TypeId actual_object, ScopeId member_scope,
	const std::vector<ValueRef>& candidates)
{
	if (node.children.size() != 2 || !this_binding.valid() ||
		!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class)
		return ExprInfo();
	const Binding& this_value = binding(this_binding);
	const TypeId this_pointer = strip_cv_type(expression_object_type(
		this_value.type));
	if (this_value.kind != BindingKind::Parameter ||
		type_kind(this_pointer) != TypeKind::Pointer)
		throw std::runtime_error("PA12 implicit member object is invalid");
	return semantic_member_call_with_object(node, scope,
		SimpleTokenType::OP_ARROW, ExprInfo(), actual_object, member_scope,
		candidates, NULL, true, this_binding);
}

ExprInfo PA11SemanticModel::semantic_unqualified_member_call(
	const PA10AstNode& node, const PA10AstNode& callee_node, ScopeId scope)
{
	if (node.children.size() != 2)
		return ExprInfo();
	if (callee_node.kind != PA10NodeKind::IdExpression ||
		callee_node.has_token || has_template_id(callee_node))
		return ExprInfo();
	const NamePath member_name = name_path(callee_node);
	if (member_name.global || member_name.components.empty())
		return ExprInfo();
	const BindingId this_id = implicit_this_binding(scope);
	if (!this_id.valid())
		return ExprInfo();
	const Binding& this_binding = binding(this_id);
	const TypeId this_pointer = strip_cv_type(expression_object_type(
		this_binding.type));
	if (this_binding.kind != BindingKind::Parameter ||
		type_kind(this_pointer) != TypeKind::Pointer)
		throw std::runtime_error("PA12 implicit this binding is invalid");
	const TypeId record_object = types_[this_pointer.value].child;
	const ScopeId member_scope = class_scope_for_type(record_object);
	if (!member_scope.valid())
		return ExprInfo();

	MemberLookup selection;
	std::vector<NamedRecordId> base_path;
	if (member_name.components.size() == 1)
	{
		// Search lexical block/function scopes, then the direct class and its
		// typed direct-base chain.  A nearer non-class declaration leaves
		// ownership with the ordinary call resolver; a class/base method set
		// suppresses enclosing namespace candidates.
		selection = unqualified_member_lookup(record_object,
			member_name.last(), scope);
		base_path = selection.base_path;
	}
	else
	{
		// A parser-supported qualified implicit call such as YA::f() names a
		// class scope, not a free function.  Resolve only the qualifier, prove
		// it is a base subobject of this, and then use the same typed selector
		// as dot/arrow and unqualified calls.
		NamePath qualifier;
		qualifier.global = member_name.global;
		qualifier.components.assign(member_name.components.begin(),
			member_name.components.end() - 1);
		const TypeId qualifier_type = lookup_type_path(qualifier, scope);
		const ScopeId qualifier_scope = class_scope_for_type(qualifier_type);
		if (!qualifier_scope.valid() || !member_base_path(record_object,
			qualifier_scope, &base_path))
			return ExprInfo();
		selection = member_lookup(qualifier_type, member_name.last());
		base_path.insert(base_path.end(), selection.base_path.begin(),
			selection.base_path.end());
	}
	if (selection.kind == MemberLookupKind::Type)
	{
		// Keep the first type declaration set typed through the existing cast
		// producer.  Do not reopen ordinary enclosing value/ADL lookup after a
		// class or base has claimed the spelling.
		if (!selection.type.valid())
			throw std::runtime_error("PA12 invalid owned functional cast target");
		return semantic_functional_cast(node, scope, selection.type,
			node.children.back());
	}
	if (selection.kind == MemberLookupKind::Blocked)
		throw std::runtime_error("PA12 inherited member name is unsupported");
	if (selection.kind != MemberLookupKind::Value ||
		!selection.owner.valid() || selection.owner.value >= scopes_.size() ||
		scopes_[selection.owner.value].kind != ScopeKind::Class)
		return ExprInfo();
	std::vector<ValueRef> candidates = member_function_candidates_in_scope(
		selection.owner, member_name.last());
	const std::vector<ValueRef> static_candidates =
		static_member_function_candidates_in_scope(selection.owner,
			member_name.last());
	candidates.insert(candidates.end(), static_candidates.begin(),
		static_candidates.end());
	if (candidates.empty())
	{
		if (!base_path.empty())
			throw std::runtime_error("PA12 inherited member name is not callable");
		return ExprInfo();
	}
	return semantic_member_call_with_implicit_object(node, scope, this_id,
		record_object, selection.owner, candidates);
}

ExprInfo PA11SemanticModel::semantic_member_call_probe(
	const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 2)
		return ExprInfo();
	const PA10AstNode* member_node = &node.children.front();
	while (member_node->kind == PA10NodeKind::ParenthesizedExpression &&
		member_node->children.size() == 1)
		member_node = &member_node->children.front();
	if (member_node->kind != PA10NodeKind::MemberExpression &&
		member_node->kind != PA10NodeKind::IdExpression)
		return ExprInfo();
	SemanticTailGuard member_tail(*this);
	const ExprInfo member_call = member_node->kind ==
		PA10NodeKind::MemberExpression ? semantic_member_call_expression(
			node, *member_node, scope) : semantic_unqualified_member_call(node,
			*member_node, scope);
	if (!member_call.fact.valid())
		return ExprInfo();
	member_tail.commit();
	return member_call;
}

} // namespace pa11_semantic_internal
