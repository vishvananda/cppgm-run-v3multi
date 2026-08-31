#include "pa11_semantic_model.h"

#include <limits>

namespace pa11_semantic_internal
{

int compare_standard_conversion_components(
	ConversionRankCategory left_category, ConversionKind left_kind,
	unsigned int left_rank, unsigned int left_base_distance,
	unsigned int left_added_cv, bool left_rvalue_reference,
	ConversionRankCategory right_category, ConversionKind right_kind,
	unsigned int right_rank, unsigned int right_base_distance,
	unsigned int right_added_cv, bool right_rvalue_reference)
{
	const bool left_derived = left_kind == ConversionKind::DerivedToBase;
	const bool right_derived = right_kind == ConversionKind::DerivedToBase;
	// Preserve the established numeric ordering for pairs that contain no
	// class adjustment, but retain the typed qualification subset when the
	// score carries cv metadata (notably an implicit member object).  The
	// category/path ordering below is for a candidate whose standard sequence
	// includes DerivedToBase.
	if (left_category != right_category)
		return static_cast<int>(left_category) <
			static_cast<int>(right_category) ? -1 : 1;
	if (!left_derived && !right_derived)
	{
		if (left_rank == right_rank)
		{
			if (left_kind == ConversionKind::ReferenceBinding &&
				right_kind == ConversionKind::ReferenceBinding &&
				left_rvalue_reference != right_rvalue_reference)
				return left_rvalue_reference ? -1 : 1;
			const unsigned int left_extra = left_added_cv & ~right_added_cv;
			const unsigned int right_extra = right_added_cv & ~left_added_cv;
			if (left_extra != 0 && right_extra == 0)
				return 1;
			if (right_extra != 0 && left_extra == 0)
				return -1;
			return 0;
		}
		return left_rank < right_rank ? -1 : 1;
	}
	if (left_derived || right_derived)
	{
		// [over.ics.rank] gives a derived-to-base pointer/reference conversion
		// precedence over the competing base/void conversion at this category.
		if (left_derived != right_derived)
			return left_derived ? -1 : 1;
		if (left_base_distance != right_base_distance)
			return left_base_distance < right_base_distance ? -1 : 1;
		// Qualification is a subset ordering, not a bit-count ordering.
		const unsigned int left_extra = left_added_cv & ~right_added_cv;
		const unsigned int right_extra = right_added_cv & ~left_added_cv;
		if (left_extra != 0 && right_extra == 0)
			return 1;
		if (right_extra != 0 && left_extra == 0)
			return -1;
		return 0;
	}
	return 0;
}

// [over.ics.rank] compares the standard/user-defined/ellipsis sequence
// boundary before any legacy numeric rank.  A user-defined score's legacy
// rank is the first standard sequence, but [over.ics.rank] 13.3.3.2 p3 may
// compare the second standard sequence when the same typed constructor is
// used on both sides.
int compare_conversion_scores(const ConversionScore& left,
	const ConversionScore& right)
{
	const bool left_standard =
		left.rank_category == ConversionRankCategory::Exact ||
		left.rank_category == ConversionRankCategory::Promotion ||
		left.rank_category == ConversionRankCategory::Conversion;
	const bool right_standard =
		right.rank_category == ConversionRankCategory::Exact ||
		right.rank_category == ConversionRankCategory::Promotion ||
		right.rank_category == ConversionRankCategory::Conversion;
	if (left_standard != right_standard)
		return left_standard ? -1 : 1;
	if (!left_standard)
	{
		if (left.rank_category != right.rank_category)
			return left.rank_category == ConversionRankCategory::UserDefined ?
				-1 : 1;
		if (left.rank_category == ConversionRankCategory::UserDefined &&
				left.user_defined_constructor.valid() &&
				left.user_defined_constructor == right.user_defined_constructor &&
				left.user_defined_target.valid() &&
				left.user_defined_target == right.user_defined_target &&
				!left.user_defined_ambiguous &&
				!right.user_defined_ambiguous)
			return compare_standard_conversion_components(
				conversion_rank_category(left.kind, left.legacy_rank),
				left.kind, left.legacy_rank, left.base_distance,
				left.added_cv,
				left.user_defined_second_rvalue_reference,
				conversion_rank_category(right.kind, right.legacy_rank),
				right.kind, right.legacy_rank, right.base_distance,
				right.added_cv,
				right.user_defined_second_rvalue_reference);
		return 0;
	}
	return compare_standard_conversion_components(left.rank_category, left.kind,
		left.legacy_rank, left.base_distance, left.added_cv, false,
		right.rank_category, right.kind, right.legacy_rank,
		right.base_distance, right.added_cv, false);
}

int compare_conversion_choices(const ConversionChoice& left,
	const ConversionChoice& right)
{
	return compare_conversion_scores(ConversionScore(left),
		ConversionScore(right));
}

bool PA11SemanticModel::supports_class_value_parameter(
	const ValueRef& candidate_ref, std::size_t parameter,
	const ExprInfo& argument, TypeId parameter_type) const
{
	const FunctionFact* function = function_fact_for_binding(
		candidate_ref.binding);
	if (function == NULL || !function->binding.valid() ||
		function->binding != candidate_ref.binding)
		return false;
	if (function->is_constructor)
	{
		if (!narrow_class_value_constructor(*function))
			return false;
	}
	else if (!function->owner.valid() || function->owner.value >= scopes_.size() ||
		scopes_[function->owner.value].kind != ScopeKind::Namespace)
		return false;
	if (candidate_ref.binding.value >= bindings_.size())
		return false;
	const TypeId function_type = binding(candidate_ref.binding).type;
	if (!function_type.valid() || function_type.value >= types_.size() ||
		type_kind(function_type) != TypeKind::Function ||
		types_[function_type.value].variadic ||
		parameter >= types_[function_type.value].parameters.size() ||
		types_[function_type.value].parameters[parameter] != parameter_type)
		return false;
	if (!class_value_transfer_type(parameter_type) ||
		type_kind(parameter_type) == TypeKind::LvalueReference ||
		type_kind(parameter_type) == TypeKind::RvalueReference)
		return false;
	const TypeId parameter_object = strip_cv_type(
		expression_object_type(parameter_type));
	return argument.fact.valid() &&
		argument.fact.value < semantic_facts_.size() &&
		strip_cv_type(expression_object_type(argument.type)) == parameter_object;
}

void PA11SemanticModel::collect_implicit_constructor_candidates(
	NamedRecordId record_id, ConstructorInitializationContext context,
	std::vector<ValueRef>& candidates,
	std::vector<NamedRecordId>& active) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag == ClassTag::Union)
		throw std::runtime_error(
			"PA12 implicit constructor candidate record is invalid");
	const NamedRecord& record = named_[record_id.value];
	if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id ||
		!record.name.valid())
		throw std::runtime_error(
			"PA12 implicit constructor candidate owner is invalid");
	for (std::size_t i = 0; i < active.size(); ++i)
		if (active[i] == record_id)
			throw std::runtime_error(
				"PA12 implicit constructor candidate relation cycle");
	if (active.size() >= named_.size())
		throw std::runtime_error(
			"PA12 implicit constructor candidate depth is invalid");
	active.push_back(record_id);

	// Keep the pure viability view separate from expand_inheriting_constructor_candidates:
	// the latter publishes wrapper bindings and constructor actions for selected
	// materialization, while this view must not make speculative overload probes
	// source-order dependent or demand unused helpers.
	std::vector<ValueRef> direct;
	FlatIndex<BindingId, bool, IdentityHash<BindingId> > seen;
	FlatIndex<ConstructorSignatureKey, bool, ConstructorSignatureHash>
		direct_signatures;
	const ValueList* values = scopes_[record.scope.value].values.find(
		record.name);
	if (values != NULL)
	{
		for (std::size_t i = 0; i < values->entries.size(); ++i)
		{
			const ValueEntry& entry = values->entries[i];
			const BindingId candidate_id = entry.binding;
			if (!candidate_id.valid() || candidate_id.value >= bindings_.size() ||
				candidate_id.value >= binding_owners_.size() ||
				binding_owners_[candidate_id.value] != record.scope ||
				entry.origin != record.scope)
				throw std::runtime_error(
					"PA12 implicit constructor candidate identity is invalid");
			if (seen.find(candidate_id) != NULL)
				throw std::runtime_error(
					"PA12 duplicate implicit constructor candidate identity");
			seen.set(candidate_id, true);
			const Binding& candidate = binding(candidate_id);
			const FunctionFact* function = function_fact_for_binding(candidate_id);
			if (function != NULL && function->inheriting_constructor)
				continue;
			if (candidate.kind != BindingKind::Function ||
				!candidate.type.valid() || candidate.type.value >= types_.size() ||
				type_kind(candidate.type) != TypeKind::Function)
				continue;
			const BindingSidecar* sidecar = binding_sidecar(candidate_id);
			if (sidecar == NULL || sidecar->constructor_record != record_id)
				continue;
			if (function == NULL || !function->is_constructor ||
				function->binding != candidate_id || function->owner != record.scope ||
				function->constructor_record != record_id)
				throw std::runtime_error(
					"PA12 implicit constructor candidate fact is missing");
			const TypeKey& signature = types_[candidate.type.value];
			if (!signature.variadic && signature.parameters.size() == 1)
				direct_signatures.set(ConstructorSignatureKey(
					signature.parameters.front(), signature.result, signature.cv), true);
			if (context == ConstructorInitializationContext::Copy &&
				sidecar->explicit_constructor)
				continue;
			direct.push_back(ValueRef(record.scope, candidate_id));
		}
	}

	FlatIndex<BindingId, bool, IdentityHash<BindingId> > output_seen;
	const auto append_one_argument_candidate =
		[this, context, &candidates, &output_seen](const ValueRef& candidate_ref) {
			if (!candidate_ref.binding.valid() ||
				candidate_ref.binding.value >= bindings_.size() ||
				candidate_ref.binding.value >= binding_owners_.size() ||
				!candidate_ref.scope.valid() ||
				candidate_ref.scope.value >= scopes_.size() ||
				binding_owners_[candidate_ref.binding.value] != candidate_ref.scope ||
				scopes_[candidate_ref.scope.value].kind != ScopeKind::Class)
				throw std::runtime_error(
					"PA12 implicit constructor candidate owner is invalid");
			const NamedRecordId candidate_record =
				scopes_[candidate_ref.scope.value].record;
			if (!candidate_record.valid() || candidate_record.value >= named_.size() ||
				named_[candidate_record.value].kind != NamedKind::Class)
				throw std::runtime_error(
					"PA12 implicit constructor candidate record is invalid");
			const Binding& candidate = binding(candidate_ref.binding);
			if (candidate.kind != BindingKind::Function ||
				!candidate.type.valid() || candidate.type.value >= types_.size() ||
				type_kind(candidate.type) != TypeKind::Function)
				throw std::runtime_error(
					"PA12 implicit constructor candidate function is invalid");
			const BindingSidecar* sidecar = binding_sidecar(candidate_ref.binding);
			const FunctionFact* function = function_fact_for_binding(
				candidate_ref.binding);
			if (sidecar == NULL || sidecar->constructor_record != candidate_record ||
				function == NULL || !function->is_constructor ||
				function->binding != candidate_ref.binding ||
				function->owner != candidate_ref.scope ||
				function->constructor_record != candidate_record ||
				(context == ConstructorInitializationContext::Copy &&
					sidecar->explicit_constructor))
				throw std::runtime_error(
					"PA12 implicit constructor candidate fact is invalid");
			const TypeKey& signature = types_[candidate.type.value];
			if (signature.variadic || signature.parameters.empty())
				return;
			std::size_t required = signature.parameters.size();
			while (required > 1 && function_default_argument(
				candidate_ref.binding, required - 1).valid())
				--required;
			if (required != 1)
				return;
			if (output_seen.find(candidate_ref.binding) != NULL)
				throw std::runtime_error(
					"PA12 duplicate implicit constructor candidate");
			output_seen.set(candidate_ref.binding, true);
			candidates.push_back(candidate_ref);
		};
	for (std::size_t i = 0; i < direct.size(); ++i)
		append_one_argument_candidate(direct[i]);

	const NamedRecordSidecar* record_sidecar = named_record_sidecar(record_id);
	if (record_sidecar != NULL)
	{
		FlatIndex<NamedRecordId, bool, IdentityHash<NamedRecordId> > relation_seen;
		for (std::size_t relation_index = 0;
			relation_index < record_sidecar->inheriting_constructors.size();
			++relation_index)
		{
			const NamedRecordId base_record =
				record_sidecar->inheriting_constructors[relation_index].base_record;
			if (!base_record.valid() || base_record.value >= named_.size() ||
				named_[base_record.value].kind != NamedKind::Class ||
				base_record != record.direct_base)
				throw std::runtime_error(
					"PA12 implicit inherited constructor relation is invalid");
			if (relation_seen.find(base_record) != NULL)
				throw std::runtime_error(
					"PA12 duplicate implicit inherited constructor relation");
			relation_seen.set(base_record, true);
			std::vector<ValueRef> base_candidates;
			collect_implicit_constructor_candidates(base_record, context,
				base_candidates, active);
			for (std::size_t base_index = 0;
				base_index < base_candidates.size(); ++base_index)
			{
				const ValueRef& base_ref = base_candidates[base_index];
				const Binding& base_binding = binding(base_ref.binding);
				const TypeKey& base_signature = types_[base_binding.type.value];
				if (base_signature.variadic || base_signature.parameters.empty() ||
					inherited_constructor_minimum_arity(base_ref.binding,
						base_signature) > 1)
					continue;
				if (direct_signatures.find(ConstructorSignatureKey(
					base_signature.parameters.front(), base_signature.result,
					base_signature.cv)) == NULL)
					append_one_argument_candidate(base_ref);
			}
		}
	}
	active.pop_back();
}

ImplicitConstructorConversion PA11SemanticModel::implicit_constructor_conversion(
	const ExprInfo& argument, TypeId target, ScopeId scope) const
{
	if (!target.valid() || target.value >= types_.size() ||
		!argument.fact.valid() || argument.fact.value >= semantic_facts_.size())
		return ImplicitConstructorConversion();
	const TypeKind target_kind = type_kind(target);
	if ((target_kind != TypeKind::LvalueReference &&
		target_kind != TypeKind::RvalueReference))
		return ImplicitConstructorConversion();
	const TypeId referred = types_[target.value].child;
	if (!referred.valid() || referred.value >= types_.size())
		return ImplicitConstructorConversion();
	if (target_kind == TypeKind::LvalueReference &&
		(type_kind(referred) != TypeKind::Cv ||
			(cv_qualifiers(referred) & 2u) != 0))
		return ImplicitConstructorConversion();
	const TypeId object = strip_cv_type(expression_object_type(referred));
	const NamedRecordId record_id = named_record_for_type(object);
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag == ClassTag::Union)
		return ImplicitConstructorConversion();
	const NamedRecord& record = named_[record_id.value];
	if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		!record.name.valid())
		throw std::runtime_error("PA12 implicit constructor owner is invalid");
	const SemanticFact& source_fact = semantic_facts_[argument.fact.value];
	const ConversionChoice second = conversion_for(object,
		SemanticValueCategory::Prvalue, target, source_fact.source, false, scope);
	if (!second.valid)
		return ImplicitConstructorConversion();
	bool found = false;
	bool ambiguous = false;
	unsigned int best_rank = std::numeric_limits<unsigned int>::max();
	ConversionScore best_score;
	BindingId best_constructor;
	std::vector<ValueRef> candidates;
	std::vector<NamedRecordId> active;
	collect_implicit_constructor_candidates(record_id,
		ConstructorInitializationContext::Copy, candidates, active);
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const ValueRef& candidate_ref = candidates[i];
		const BindingId candidate_id = candidate_ref.binding;
		const Binding& candidate = binding(candidate_id);
		if (candidate.kind != BindingKind::Function || !candidate.type.valid() ||
			candidate.type.value >= types_.size() ||
			type_kind(candidate.type) != TypeKind::Function)
			continue;
		const BindingSidecar* sidecar = binding_sidecar(candidate_id);
		const FunctionFact* candidate_function =
			function_fact_for_binding(candidate_id);
		if (sidecar == NULL || candidate_function == NULL ||
			!candidate_function->is_constructor)
			throw std::runtime_error(
				"PA12 implicit constructor candidate fact is invalid");
		if (sidecar->explicit_constructor)
			continue;
		const TypeKey& signature = types_[candidate.type.value];
		if (signature.variadic || signature.parameters.empty())
			throw std::runtime_error(
				"PA12 implicit constructor candidate signature is invalid");
		const TypeId parameter = signature.parameters.front();
		const NamedRecordId parameter_record = named_record_for_type(
			strip_cv_type(expression_object_type(parameter)));
		if (parameter_record.valid() && parameter_record.value < named_.size() &&
			named_[parameter_record.value].kind == NamedKind::Class &&
			type_kind(parameter) != TypeKind::LvalueReference &&
			type_kind(parameter) != TypeKind::RvalueReference)
			continue;
		const ConversionChoice conversion = conversion_for(argument, parameter,
			source_fact.source, scope);
		if (!conversion.valid)
			continue;
		const ConversionScore score(conversion);
		const int comparison = found ? compare_conversion_scores(score,
			best_score) : -1;
		if (!found || comparison < 0)
		{
			found = true;
			ambiguous = false;
			best_rank = conversion.rank;
			best_score = score;
			best_constructor = candidate_id;
		}
		else if (comparison == 0)
			ambiguous = true;
	}
	if (!found)
		return ImplicitConstructorConversion();
	// Even an ambiguous constructor-level conversion is a viable, indistinguish-
	// able user-defined sequence for the enclosing overload set.  Keep the
	// first standard rank as metadata only.  A unique constructor identity and
	// its second standard sequence are retained solely for outer ranking; if
	// this candidate wins, target-aware materialization re-enters
	// select_constructor and diagnoses the ambiguous constructor.
	ImplicitConstructorConversion result;
	result.choice = ConversionChoice(true, best_rank,
		ConversionKind::ReferenceBinding);
	result.choice.rank_category = ConversionRankCategory::UserDefined;
	result.ambiguous = ambiguous;
	result.second = second;
	result.second_rvalue_reference =
		type_kind(target) == TypeKind::RvalueReference;
	if (!ambiguous)
	{
		result.constructor = best_constructor;
		result.target = record_id;
	}
	return result;
}

TypedFunctionSelection PA11SemanticModel::select_typed_function(
	const std::vector<ValueRef>& candidates,
	const std::vector<const PA10AstNode*>& argument_nodes,
	const std::vector<ExprInfo>& initial_arguments, ScopeId scope,
	bool allow_class_value)
{
	if (argument_nodes.size() != initial_arguments.size())
		throw std::runtime_error("PA12 function argument boundary mismatch");
	for (std::size_t i = 0; i < argument_nodes.size(); ++i)
		if (argument_nodes[i] == NULL)
			throw std::runtime_error("PA12 function argument node is missing");
	std::vector<ExprInfo> arguments = initial_arguments;
	struct CandidateScore
	{
		ValueRef value;
		TypeId type;
		bool variadic;
		std::vector<ConversionScore> ranks;
	};
	std::vector<CandidateScore> viable;
	const unsigned int ellipsis_rank = std::numeric_limits<unsigned int>::max() / 4;
	const auto supported_class_value_parameter = [this, &arguments,
		allow_class_value](const ValueRef& candidate_ref,
		std::size_t parameter, TypeId parameter_type) -> bool
	{
		if (!allow_class_value || parameter >= arguments.size())
			return false;
		return supports_class_value_parameter(candidate_ref, parameter,
			arguments[parameter], parameter_type);
	};
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const ValueRef& candidate_ref = candidates[i];
		if (!candidate_ref.binding.valid() || candidate_ref.binding.value >=
			bindings_.size() || candidate_ref.binding.value >=
			binding_owners_.size() || !candidate_ref.scope.valid() ||
			candidate_ref.scope.value >= scopes_.size())
			throw std::runtime_error("PA12 function candidate identity is invalid");
		if (binding_owners_[candidate_ref.binding.value] != candidate_ref.scope)
			throw std::runtime_error(
				"PA12 function candidate owner identity is invalid");
		const Binding& candidate = binding(candidate_ref.binding);
		if (candidate.kind != BindingKind::Function || !candidate.type.valid() ||
			candidate.type.value >= types_.size() ||
			type_kind(candidate.type) != TypeKind::Function)
			continue;
		const TypeId candidate_type = candidate.type;
		// Copy the signature before resolving a deferred function-id argument:
		// specialization may publish more typed facts and invalidate a vector
		// reference into types_.
		const TypeKey function = types_[candidate_type.value];
		if (allow_class_value)
		{
			bool supported = true;
			for (std::size_t parameter = 0;
				parameter < function.parameters.size(); ++parameter)
			{
				const TypeId parameter_type = function.parameters[parameter];
				if (class_value_type(parameter_type) &&
					!supported_class_value_parameter(candidate_ref, parameter,
						parameter_type))
				{
					supported = false;
					break;
				}
			}
			if (!supported)
				continue;
		}
		std::size_t required = function.parameters.size();
		while (required != 0 && function_default_argument(
			candidate_ref.binding, required - 1).valid())
			--required;
		if (arguments.size() < required ||
			(!function.variadic && arguments.size() > function.parameters.size()))
			continue;
		CandidateScore score = {candidate_ref, candidate_type,
			function.variadic, std::vector<ConversionScore>()};
		score.ranks.reserve(arguments.size());
		for (std::size_t argument = 0; argument < arguments.size(); ++argument)
		{
			if (argument >= function.parameters.size())
			{
				if (!arguments[argument].fact.valid())
					break;
				score.ranks.push_back(ConversionScore::ellipsis_score(ellipsis_rank));
				continue;
			}
			ConversionChoice choice;
			if (arguments[argument].fact.valid())
			{
				if (arguments[argument].fact.value >= semantic_facts_.size())
					throw std::runtime_error("PA12 function argument fact is invalid");
				const SemanticFact& fact =
					semantic_facts_[arguments[argument].fact.value];
				choice = conversion_for(arguments[argument],
					function.parameters[argument], fact.source, scope);
				if (choice.valid &&
					type_kind(function.parameters[argument]) !=
						TypeKind::LvalueReference &&
						type_kind(function.parameters[argument]) !=
						TypeKind::RvalueReference &&
						supported_class_value_parameter(candidate_ref, argument,
							function.parameters[argument]))
				{
					choice.kind = ConversionKind::ClassValue;
					choice.rank_category = ConversionRankCategory::Exact;
				}
			}
			else
			{
				const PA10AstNode* function_id = target_function_id(
					*argument_nodes[argument], scope);
				if (function_id != NULL)
				{
					const FunctionIdResolution resolution =
						resolve_function_id_target(*function_id, scope,
							function.parameters[argument]);
					choice = resolution.conversion;
				}
			}
			if (!choice.valid)
			{
				const ImplicitConstructorConversion implicit =
					implicit_constructor_conversion(arguments[argument],
						function.parameters[argument], scope);
				if (!implicit.choice.valid)
					break;
				score.ranks.push_back(ConversionScore(implicit));
			}
			else
				score.ranks.push_back(ConversionScore(choice));
		}
		if (score.ranks.size() == arguments.size())
			viable.push_back(score);
	}
	if (viable.empty())
		throw std::runtime_error("PA12 no viable function");
	const auto better = [](const CandidateScore& left,
		const CandidateScore& right) -> bool
	{
		bool strict = false;
		for (std::size_t i = 0; i < left.ranks.size(); ++i)
		{
			const int comparison = compare_conversion_scores(left.ranks[i],
				right.ranks[i]);
			if (comparison > 0)
				return false;
			if (comparison < 0)
				strict = true;
		}
		return strict || (left.variadic != right.variadic && !left.variadic);
	};
	std::size_t best_index = 0;
	for (std::size_t i = 1; i < viable.size(); ++i)
		if (better(viable[i], viable[best_index]))
			best_index = i;
	for (std::size_t i = 0; i < viable.size(); ++i)
		if (i != best_index && !better(viable[best_index], viable[i]))
			throw std::runtime_error("PA12 ambiguous function call");

	const ValueRef selected = viable[best_index].value;
	const TypeId selected_type = viable[best_index].type;
	if (function_declaration_kind(selected.binding) ==
		FunctionDeclarationKind::Deleted)
		throw std::runtime_error("PA12 call selects deleted function");
	// Copy the selected signature before default publication and contextual
	// conversion can grow the semantic/type arenas.
	const TypeKey selected_function = types_[selected_type.value];
	const std::size_t explicit_count = arguments.size();
	for (std::size_t argument = explicit_count;
		argument < selected_function.parameters.size(); ++argument)
	{
		const SemanticFactId default_fact = function_default_argument(
			selected.binding, argument);
		if (!default_fact.valid() || default_fact.value >= semantic_facts_.size())
			throw std::runtime_error("PA12 selected function default is missing");
		const SemanticFact value = semantic_facts_[default_fact.value];
		arguments.push_back(ExprInfo(default_fact, value.type, value.category,
			false));
	}
	const std::size_t fixed_explicit = explicit_count <
		selected_function.parameters.size() ? explicit_count :
		selected_function.parameters.size();
	for (std::size_t argument = 0; argument < fixed_explicit; ++argument)
	{
		const TypeId parameter = selected_function.parameters[argument];
		if (!parameter.valid() || parameter.value >= types_.size())
			throw std::runtime_error("PA12 selected function parameter is invalid");
		if (!arguments[argument].fact.valid())
			arguments[argument] = semantic_expression_for_target(
				*argument_nodes[argument], scope,
				parameter);
		else
		{
			if (arguments[argument].fact.value >= semantic_facts_.size())
				throw std::runtime_error(
					"PA12 selected function argument fact is invalid");
			const TypeKind parameter_kind = type_kind(parameter);
			const bool class_reference =
				(parameter_kind == TypeKind::LvalueReference ||
					parameter_kind == TypeKind::RvalueReference) &&
				class_scope_for_type(types_[parameter.value].child).valid();
			if (class_reference && !conversion_for(arguments[argument], parameter,
				semantic_facts_[arguments[argument].fact.value].source,
				scope).valid)
				arguments[argument] = semantic_expression_for_target(
					*argument_nodes[argument], scope, parameter);
		}
		if (!arguments[argument].fact.valid() || arguments[argument].fact.value >=
			semantic_facts_.size())
			throw std::runtime_error("PA12 selected function argument is invalid");
		const bool class_value = supported_class_value_parameter(selected, argument,
			parameter);
		const PA10AstNode* source = semantic_facts_[
			arguments[argument].fact.value].source;
		if (class_value)
		{
			set_fact_conversion(arguments[argument].fact,
				add_conversion(arguments[argument].type,
					parameter,
					ConversionChoice(true, 0, ConversionKind::ClassValue)));
		}
		else
			arguments[argument] = apply_context_conversion(arguments[argument],
				parameter, source, scope);
	}
	apply_call_argument_conversions(arguments, selected_type, scope);
	TypedFunctionSelection result(selected, selected_type);
	result.arguments.swap(arguments);
	return result;
}

void PA11SemanticModel::collect_associated_adl_records(
	const std::vector<TypeId>& associated_objects,
	std::vector<NamedRecordId>* associated_records) const
{
	if (associated_records == NULL)
		throw std::runtime_error("PA12 associated ADL record output is missing");
	associated_records->clear();

	// Form the associated class/enum set from the operand types.  The walk is
	// bounded by the typed argument wrappers, already-formed direct-base chains,
	// and enclosing class scopes; it never scans unrelated program declarations.
	std::vector<TypeId> seen_types;
	const auto enqueue_type = [this, &seen_types](TypeId type,
		std::vector<TypeId>* pending) {
		if (pending == NULL || !type.valid() || type.value >= types_.size())
			return;
		for (std::size_t i = 0; i < seen_types.size(); ++i)
			if (seen_types[i] == type)
				return;
		seen_types.push_back(type);
		pending->push_back(type);
	};
	for (std::size_t object_index = 0; object_index < associated_objects.size();
		++object_index)
	{
		std::vector<TypeId> pending_types;
		enqueue_type(associated_objects[object_index], &pending_types);
		while (!pending_types.empty())
		{
			const TypeId object = pending_types.back();
			pending_types.pop_back();
			const TypeKey& object_key = types_[object.value];
			switch (object_key.kind)
			{
			case TypeKind::Cv:
			case TypeKind::LvalueReference:
			case TypeKind::RvalueReference:
			case TypeKind::Pointer:
			case TypeKind::Array:
				enqueue_type(object_key.child, &pending_types);
				continue;
			case TypeKind::Function:
				// Push in reverse so the typed function result is visited first,
				// followed by parameters in declaration order.
				for (std::size_t i = object_key.parameters.size(); i != 0; --i)
					enqueue_type(object_key.parameters[i - 1], &pending_types);
				enqueue_type(object_key.result, &pending_types);
				continue;
			case TypeKind::Named:
			case TypeKind::Fundamental:
			case TypeKind::MemberPointer:
				break;
			}
			if (object_key.kind != TypeKind::Named)
				continue;
			const NamedRecordId initial = named_record_for_type(object);
			if (!initial.valid())
				continue;
			std::vector<NamedRecordId> pending_records;
			pending_records.push_back(initial);
			while (!pending_records.empty())
			{
				const NamedRecordId record_id = pending_records.back();
				pending_records.pop_back();
				if (!record_id.valid() || record_id.value >= named_.size())
					continue;
				bool seen = false;
				for (std::size_t i = 0; i < associated_records->size(); ++i)
					if ((*associated_records)[i] == record_id)
					{
						seen = true;
						break;
					}
				if (seen)
					continue;
				const NamedRecord& record = named_[record_id.value];
				if (record.kind != NamedKind::Class && record.kind != NamedKind::Enum)
					continue;
				associated_records->push_back(record_id);
				if (record.kind == NamedKind::Class)
				{
					// A forward-declared class has no class scope yet, but its
					// enclosing namespace is still an ADL-associated namespace.
					// Complete classes retain the validated base walk; malformed
					// incomplete metadata does not silently acquire a base relation.
					if (record.defined)
					{
						std::vector<NamedRecordId> bases;
						if (!direct_base_chain(named_type(record_id), &bases))
							throw std::runtime_error("PA12 associated ADL base relation is invalid");
						for (std::size_t i = 0; i < bases.size(); ++i)
							pending_records.push_back(bases[i]);
					}
					else if (record.has_base || record.direct_base.valid() ||
						record.direct_base_virtual)
						throw std::runtime_error(
							"PA12 incomplete associated ADL base metadata is invalid");
				}
				if (record.owner.valid() && record.owner.value < scopes_.size() &&
					scopes_[record.owner.value].kind == ScopeKind::Class &&
					scopes_[record.owner.value].record.valid())
					pending_records.push_back(scopes_[record.owner.value].record);
			}
		}
	}
}

void PA11SemanticModel::collect_associated_adl_namespaces(
	const std::vector<NamedRecordId>& associated_records,
	std::vector<ScopeId>* associated_namespaces) const
{
	if (associated_namespaces == NULL)
		throw std::runtime_error("PA12 associated ADL namespace output is missing");
	associated_namespaces->clear();
	std::vector<ScopeId> pending;
	const auto enqueue_namespace = [this, associated_namespaces, &pending](
		ScopeId candidate) {
		if (!candidate.valid() || candidate.value >= scopes_.size() ||
			scopes_[candidate.value].kind != ScopeKind::Namespace)
			return;
		for (std::size_t i = 0; i < associated_namespaces->size(); ++i)
			if ((*associated_namespaces)[i] == candidate)
				return;
		associated_namespaces->push_back(candidate);
		pending.push_back(candidate);
	};
	for (std::size_t i = 0; i < associated_records.size(); ++i)
	{
		if (!associated_records[i].valid() ||
			associated_records[i].value >= named_.size())
			throw std::runtime_error("PA12 associated ADL record identity is invalid");
		ScopeId cursor = named_[associated_records[i].value].owner;
		// A class record can be nested in another class, but ADL stops at the
		// first enclosing namespace.  In particular, do not climb namespace
		// parents after this point.
		while (cursor.valid() && cursor.value < scopes_.size() &&
			scopes_[cursor.value].kind != ScopeKind::Namespace)
			cursor = scopes_[cursor.value].parent;
		if (cursor.valid() && cursor.value < scopes_.size())
			enqueue_namespace(cursor);
	}
	// Apply only the standard inline-namespace closure to each first
	// enclosing namespace.  An inline namespace contributes its enclosing
	// namespace, and a namespace contributes directly contained inline
	// namespaces; ordinary namespace parents are never climbed.
	for (std::size_t i = 0; i < pending.size(); ++i)
	{
		const ScopeId current = pending[i];
		const Scope& scope = scopes_[current.value];
		if (scope.inline_namespace)
			enqueue_namespace(scope.parent);
		for (std::size_t j = 0; j < scope.children.size(); ++j)
		{
			const ScopeId child = scope.children[j];
			if (child.valid() && child.value < scopes_.size() &&
				scopes_[child.value].kind == ScopeKind::Namespace &&
				scopes_[child.value].inline_namespace)
				enqueue_namespace(child);
		}
	}
}

void PA11SemanticModel::append_adl_function_candidates(
	NameId name, const std::vector<TypeId>& associated_objects,
	SourcePoint point, std::vector<ValueRef>* candidates) const
{
	if (!name.valid() || candidates == NULL)
		throw std::runtime_error("PA12 ADL candidate inputs are missing");
	std::vector<NamedRecordId> associated_records;
	collect_associated_adl_records(associated_objects, &associated_records);
	if (associated_records.empty())
		return;
	std::vector<ScopeId> associated_namespaces;
	collect_associated_adl_namespaces(associated_records,
		&associated_namespaces);
	const auto append_candidate = [this, candidates](const ValueRef& candidate) {
		if (!candidate.binding.valid() || candidate.binding.value >= bindings_.size() ||
			candidate.binding.value >= binding_owners_.size() ||
			!candidate.scope.valid() || candidate.scope.value >= scopes_.size() ||
			binding_owners_[candidate.binding.value] != candidate.scope)
			throw std::runtime_error("PA12 ADL candidate identity is invalid");
		const Binding& value = binding(candidate.binding);
		const BindingSidecar* sidecar = binding_sidecar(candidate.binding);
		if (value.kind != BindingKind::Function ||
			type_kind(value.type) != TypeKind::Function ||
			(sidecar != NULL && sidecar->template_specialization.valid()))
			return;
		for (std::size_t i = 0; i < candidates->size(); ++i)
			if ((*candidates)[i].binding == candidate.binding &&
				(*candidates)[i].scope == candidate.scope)
				return;
		candidates->push_back(candidate);
	};
	for (std::size_t i = 0; i < associated_namespaces.size(); ++i)
	{
		begin_lookup();
		std::vector<ValueRef> found;
		lookup_value_graph(associated_namespaces[i], name, &found, false, point);
		for (std::size_t j = 0; j < found.size(); ++j)
			if (found[j].scope.valid() && found[j].scope.value < scopes_.size() &&
				scopes_[found[j].scope.value].kind == ScopeKind::Namespace)
				append_candidate(found[j]);
	}
	for (std::size_t i = 0; i < associated_records.size(); ++i)
	{
		const NamedRecordSidecar* sidecar = named_record_sidecar(
			associated_records[i]);
		if (sidecar == NULL)
			continue;
		for (std::size_t j = 0; j < sidecar->hidden_friend_functions.size(); ++j)
		{
			const HiddenFriendFunctionRelation& relation =
				sidecar->hidden_friend_functions[j];
			if (point.valid() && relation.declaration_point.valid() &&
				relation.declaration_point.value > point.value)
				continue;
			if (!relation.binding.valid() ||
				relation.binding.value >= binding_owners_.size())
				throw std::runtime_error("PA12 hidden friend identity is invalid");
			const ScopeId owner = binding_owners_[relation.binding.value];
			if (!owner.valid() || owner.value >= scopes_.size() ||
				scopes_[owner.value].kind != ScopeKind::Namespace)
				throw std::runtime_error("PA12 hidden friend owner is invalid");
			append_candidate(ValueRef(owner, relation.binding));
		}
	}
}

void PA11SemanticModel::collect_operator_candidates(
	PA10OperatorFunctionKind kind, SimpleTokenType token, TypeId member_object,
	const std::vector<TypeId>& associated_objects, ScopeId scope,
	std::vector<ValueRef>* member_candidates,
	std::vector<ValueRef>* nonmember_candidates) const
{
	if (member_candidates == NULL || nonmember_candidates == NULL)
		throw std::runtime_error("PA12 operator candidate outputs are missing");
	member_candidates->clear();
	nonmember_candidates->clear();
	const NameId name = const_cast<PA11SemanticModel*>(this)->operator_name(
		kind, token);
	const auto matches_operator = [this, kind, token](BindingId binding_id)
		-> bool
	{
		const BindingSidecar* sidecar = binding_sidecar(binding_id);
		if (sidecar == NULL || sidecar->template_specialization.valid() ||
			sidecar->operator_function_kind != kind)
			return false;
		return kind != PA10OperatorFunctionKind::Token ||
			sidecar->operator_token == token;
	};
	const auto append_candidate = [this, &matches_operator](
		std::vector<ValueRef>* output, const ValueRef& candidate) {
		if (!candidate.binding.valid() || candidate.binding.value >= bindings_.size() ||
			candidate.binding.value >= binding_owners_.size() ||
			!candidate.scope.valid() || candidate.scope.value >= scopes_.size() ||
			binding_owners_[candidate.binding.value] != candidate.scope)
			throw std::runtime_error("PA12 operator candidate identity is invalid");
		if (!matches_operator(candidate.binding))
			return;
		const Binding& value = binding(candidate.binding);
		if (value.kind != BindingKind::Function ||
			type_kind(value.type) != TypeKind::Function)
			return;
		for (std::size_t i = 0; i < output->size(); ++i)
			if ((*output)[i].binding == candidate.binding &&
				(*output)[i].scope == candidate.scope)
				return;
		output->push_back(candidate);
	};

	std::vector<NamedRecordId> associated_records;
	collect_associated_adl_records(associated_objects, &associated_records);
	if (associated_records.empty())
		return;

	if (member_object.valid())
	{
		const TypeId object = strip_cv_type(expression_object_type(
			member_object));
		if (class_scope_for_type(object).valid())
		{
			const MemberLookup selection = member_lookup(object, name);
			if (selection.kind == MemberLookupKind::Value &&
				selection.owner.valid() && selection.owner.value < scopes_.size() &&
				scopes_[selection.owner.value].kind == ScopeKind::Class)
			{
				const std::vector<ValueRef> candidates =
					member_function_candidates_in_scope(selection.owner, name);
				for (std::size_t i = 0; i < candidates.size(); ++i)
					append_candidate(member_candidates, candidates[i]);
			}
		}
	}

	if (kind == PA10OperatorFunctionKind::Token)
	{
		NamePath path;
		path.components.push_back(name);
		const SourcePoint point = lookup_source_point(scope);
		const std::vector<ValueRef> ordinary = lookup_value_path(path, scope, point);
		for (std::size_t i = 0; i < ordinary.size(); ++i)
		{
			const ValueRef& candidate = ordinary[i];
			if (!candidate.scope.valid() || candidate.scope.value >= scopes_.size() ||
				scopes_[candidate.scope.value].kind != ScopeKind::Namespace)
				continue;
			append_candidate(nonmember_candidates, candidate);
		}

		std::vector<ScopeId> associated_namespaces;
		collect_associated_adl_namespaces(associated_records,
			&associated_namespaces);
		for (std::size_t i = 0; i < associated_namespaces.size(); ++i)
		{
			begin_lookup();
			std::vector<ValueRef> candidates;
			lookup_value_graph(associated_namespaces[i], name, &candidates, false,
				point);
			for (std::size_t j = 0; j < candidates.size(); ++j)
				if (candidates[j].scope.valid() &&
					candidates[j].scope.value < scopes_.size() &&
					scopes_[candidates[j].scope.value].kind == ScopeKind::Namespace)
					append_candidate(nonmember_candidates, candidates[j]);
		}
		for (std::size_t i = 0; i < associated_records.size(); ++i)
		{
			const NamedRecordSidecar* sidecar = named_record_sidecar(
				associated_records[i]);
			if (sidecar == NULL)
				continue;
			for (std::size_t j = 0; j < sidecar->hidden_friend_functions.size(); ++j)
			{
				const HiddenFriendFunctionRelation& relation =
					sidecar->hidden_friend_functions[j];
				if (point.valid() && relation.declaration_point.valid() &&
					relation.declaration_point.value > point.value)
					continue;
				if (!relation.binding.valid() ||
					relation.binding.value >= binding_owners_.size())
					throw std::runtime_error("PA12 hidden friend identity is invalid");
				const ScopeId owner = binding_owners_[relation.binding.value];
				if (!owner.valid() || owner.value >= scopes_.size() ||
					scopes_[owner.value].kind != ScopeKind::Namespace)
					throw std::runtime_error("PA12 hidden friend owner is invalid");
				append_candidate(nonmember_candidates, ValueRef(owner, relation.binding));
			}
		}
	}
}

TypedOperatorSelection PA11SemanticModel::select_typed_operator(
	const std::vector<ValueRef>& member_candidates,
	const std::vector<ValueRef>& nonmember_candidates,
	const ExprInfo& member_object,
	const std::vector<const PA10AstNode*>& member_argument_nodes,
	const std::vector<ExprInfo>& member_arguments,
	const std::vector<const PA10AstNode*>& nonmember_argument_nodes,
	const std::vector<ExprInfo>& nonmember_arguments, ScopeId scope)
{
	if (member_argument_nodes.size() != member_arguments.size() ||
		nonmember_argument_nodes.size() != nonmember_arguments.size())
		throw std::runtime_error("PA12 operator argument boundary mismatch");
	for (std::size_t i = 0; i < member_argument_nodes.size(); ++i)
		if (member_argument_nodes[i] == NULL)
			throw std::runtime_error("PA12 member operator argument node is missing");
	for (std::size_t i = 0; i < nonmember_argument_nodes.size(); ++i)
		if (nonmember_argument_nodes[i] == NULL)
			throw std::runtime_error("PA12 nonmember operator argument node is missing");
	struct CandidateScore
	{
		ValueRef value;
		TypeId type;
		bool member;
		bool variadic;
		unsigned int object_cv;
		ConversionScore object_score;
		std::vector<ConversionScore> ranks;
	};
	std::vector<CandidateScore> viable;
	const unsigned int ellipsis_rank = std::numeric_limits<unsigned int>::max() / 4;
	const auto append_scores = [&](const std::vector<ValueRef>& candidates,
		bool member, const ExprInfo& object,
		const std::vector<const PA10AstNode*>& argument_nodes,
		const std::vector<ExprInfo>& initial_arguments) {
		for (std::size_t i = 0; i < candidates.size(); ++i)
		{
			const ValueRef& candidate_ref = candidates[i];
			if (!candidate_ref.binding.valid() || candidate_ref.binding.value >=
				bindings_.size() || candidate_ref.binding.value >=
				binding_owners_.size() || !candidate_ref.scope.valid() ||
				candidate_ref.scope.value >= scopes_.size() ||
				binding_owners_[candidate_ref.binding.value] != candidate_ref.scope)
				throw std::runtime_error("PA12 operator candidate owner is invalid");
			const Binding& candidate = binding(candidate_ref.binding);
			if (candidate.kind != BindingKind::Function || !candidate.type.valid() ||
				candidate.type.value >= types_.size() ||
				type_kind(candidate.type) != TypeKind::Function)
				continue;
			if (member)
			{
				if (scopes_[candidate_ref.scope.value].kind != ScopeKind::Class ||
					is_static_member(candidate_ref.binding) ||
					!object.fact.valid())
					continue;
				const TypeId required_object = member_object_type(candidate.type,
					candidate_ref.scope);
				if (!required_object.valid() ||
					!member_object_convertible(expression_object_type(object.type),
						required_object,
						candidate_ref.scope, NULL, scope))
					continue;
				if (!member_accessible(candidate_ref.binding, candidate_ref.scope,
					scope, expression_object_type(object.type),
					candidate_ref.has_access_override,
					candidate_ref.access_override,
					candidate_ref.access_view_owner))
					continue;
			}
			else if (scopes_[candidate_ref.scope.value].kind != ScopeKind::Namespace)
				continue;
			const TypeKey function = types_[candidate.type.value];
			std::size_t required = function.parameters.size();
			while (required != 0 && function_default_argument(
				candidate_ref.binding, required - 1).valid())
				--required;
			if (initial_arguments.size() < required ||
				(!function.variadic && initial_arguments.size() >
				function.parameters.size()))
				continue;
			CandidateScore score;
			score.value = candidate_ref;
			score.type = candidate.type;
			score.member = member;
			score.variadic = function.variadic;
			score.object_cv = 0;
			score.object_score = ConversionScore();
			score.ranks.reserve(initial_arguments.size() + (member ? 1 : 0));
			if (member)
			{
				const TypeId required_object = member_object_type(candidate.type,
					candidate_ref.scope);
				unsigned int object_distance = 0;
				const TypeId actual_object = expression_object_type(object.type);
				if (!member_object_convertible(actual_object, required_object,
					candidate_ref.scope, NULL, scope, &object_distance))
					throw std::runtime_error(
						"PA12 operator object conversion changed during scoring");
				score.object_cv = cv_qualifiers(required_object) &
					~cv_qualifiers(actual_object);
				const unsigned int object_rank = object_distance == 0 ?
					0 :
					1 + (score.object_cv == 0 ? 0 : 1);
				ConversionChoice object_choice(object_distance == 0, object_rank,
					ConversionKind::Identity);
				if (object_distance != 0)
					object_choice = make_derived_base_choice(actual_object,
						required_object, object_distance, scope, score.object_cv);
				else if (score.object_cv != 0)
				{
					object_choice = ConversionChoice(true, object_rank,
						ConversionKind::ReferenceBinding);
					object_choice.added_cv = score.object_cv;
				}
				score.object_score = ConversionScore(object_choice);
				score.ranks.push_back(score.object_score);
			}
			bool arguments_viable = true;
			for (std::size_t argument = 0; argument < initial_arguments.size();
				++argument)
			{
				if (argument >= function.parameters.size())
				{
					if (!initial_arguments[argument].fact.valid())
					{
						arguments_viable = false;
						break;
					}
					score.ranks.push_back(ConversionScore::ellipsis_score(
						ellipsis_rank));
					continue;
				}
				ConversionChoice choice;
				if (initial_arguments[argument].fact.valid())
					choice = conversion_for(initial_arguments[argument],
						function.parameters[argument],
						semantic_facts_[initial_arguments[argument].fact.value].source,
						scope);
				else
				{
					const PA10AstNode* function_id = target_function_id(
						*argument_nodes[argument], scope);
					if (function_id != NULL)
					{
						const FunctionIdResolution resolution = resolve_function_id_target(
							*function_id, scope, function.parameters[argument]);
						choice = resolution.conversion;
					}
				}
				ImplicitConstructorConversion implicit;
				bool used_implicit = false;
				if (!choice.valid)
				{
					implicit = implicit_constructor_conversion(
						initial_arguments[argument], function.parameters[argument], scope);
					if (!implicit.choice.valid)
					{
						arguments_viable = false;
						break;
					}
					used_implicit = true;
				}
				ConversionChoice& selected_choice = used_implicit ?
					implicit.choice : choice;
				if (!selected_choice.valid)
				{
					arguments_viable = false;
					break;
				}
				if (!member && argument == 0)
				{
					// Parameter zero is the explicit nonmember operand being
					// compared with a member operator's implicit object.
					const TypeId required_object = expression_object_type(
						function.parameters.front());
					const TypeId actual_object = expression_object_type(object.type);
					if (required_object.valid() && actual_object.valid())
					{
						score.object_cv = cv_qualifiers(required_object) &
							~cv_qualifiers(actual_object);
					}
					if (score.object_cv != 0 &&
						strip_cv_type(required_object) == strip_cv_type(actual_object) &&
						selected_choice.kind == ConversionKind::ReferenceBinding)
						selected_choice.added_cv = score.object_cv;
				}
				if (used_implicit)
					score.ranks.push_back(ConversionScore(implicit));
				else
					score.ranks.push_back(ConversionScore(choice));
			}
			if (arguments_viable)
			{
				viable.push_back(score);
			}
		}
	};
	append_scores(member_candidates, true, member_object,
		member_argument_nodes, member_arguments);
	append_scores(nonmember_candidates, false, member_object,
		nonmember_argument_nodes, nonmember_arguments);
	if (viable.empty())
		return TypedOperatorSelection();
	const auto better = [](const CandidateScore& left,
		const CandidateScore& right) -> bool {
		bool strict = false;
		if (left.ranks.size() != right.ranks.size())
			return false;
		for (std::size_t i = 0; i < left.ranks.size(); ++i)
		{
			const int comparison = compare_conversion_scores(left.ranks[i],
				right.ranks[i]);
			if (comparison > 0)
				return false;
			if (comparison < 0)
				strict = true;
		}
		return strict || (left.variadic != right.variadic && !left.variadic);
	};
	std::size_t best_index = 0;
	for (std::size_t i = 1; i < viable.size(); ++i)
		if (better(viable[i], viable[best_index]))
			best_index = i;
	for (std::size_t i = 0; i < viable.size(); ++i)
		if (i != best_index && !better(viable[best_index], viable[i]))
			throw std::runtime_error("PA12 ambiguous operator call");
	const CandidateScore& best = viable[best_index];
	if (function_declaration_kind(best.value.binding) ==
		FunctionDeclarationKind::Deleted)
		throw std::runtime_error("PA12 operator call selects deleted function");
	std::vector<ExprInfo> arguments = best.member ? member_arguments :
		nonmember_arguments;
	const std::vector<const PA10AstNode*>& argument_nodes = best.member ?
		member_argument_nodes : nonmember_argument_nodes;
	const TypeKey selected_function = types_[best.type.value];
	const std::size_t explicit_count = arguments.size();
	for (std::size_t argument = explicit_count;
		argument < selected_function.parameters.size(); ++argument)
	{
		const SemanticFactId default_fact = function_default_argument(
			best.value.binding, argument);
		if (!default_fact.valid() || default_fact.value >= semantic_facts_.size())
			throw std::runtime_error("PA12 selected operator default is missing");
		const SemanticFact value = semantic_facts_[default_fact.value];
		arguments.push_back(ExprInfo(default_fact, value.type, value.category,
			false));
	}
	const std::size_t fixed_explicit = explicit_count <
		selected_function.parameters.size() ? explicit_count :
		selected_function.parameters.size();
	for (std::size_t argument = 0; argument < fixed_explicit; ++argument)
	{
		const TypeId parameter = selected_function.parameters[argument];
		if (arguments[argument].fact.valid() &&
			(type_kind(parameter) == TypeKind::LvalueReference ||
				type_kind(parameter) == TypeKind::RvalueReference) &&
			class_scope_for_type(types_[parameter.value].child).valid() &&
			!conversion_for(arguments[argument], parameter,
				semantic_facts_[arguments[argument].fact.value].source,
				scope).valid)
			arguments[argument] = semantic_expression_for_target(
				*argument_nodes[argument], scope, parameter);
		if (!arguments[argument].fact.valid())
			arguments[argument] = semantic_expression_for_target(
				*argument_nodes[argument], scope,
				parameter);
		if (!arguments[argument].fact.valid() || arguments[argument].fact.value >=
			semantic_facts_.size())
			throw std::runtime_error("PA12 selected operator argument is invalid");
		arguments[argument] = apply_context_conversion(arguments[argument],
			parameter,
			semantic_facts_[arguments[argument].fact.value].source, scope);
	}
	apply_call_argument_conversions(arguments, best.type, scope);
	TypedOperatorSelection result(best.value, best.type, best.member);
	result.arguments.swap(arguments);
	return result;
}

ExprInfo PA11SemanticModel::semantic_operator_call(
	const PA10AstNode& node, ScopeId scope, PA10OperatorFunctionKind kind,
	SimpleTokenType token, const ExprInfo& member_object,
	const std::vector<TypeId>& associated_objects,
	const std::vector<const PA10AstNode*>& member_argument_nodes,
	const std::vector<ExprInfo>& member_arguments,
	const std::vector<const PA10AstNode*>& nonmember_argument_nodes,
	const std::vector<ExprInfo>& nonmember_arguments)
{
	std::vector<ValueRef> member_candidates;
	std::vector<ValueRef> nonmember_candidates;
	collect_operator_candidates(kind, token, member_object.type,
		associated_objects, scope, &member_candidates, &nonmember_candidates);
	if (member_candidates.empty() && nonmember_candidates.empty())
		return ExprInfo();
	const TypedOperatorSelection selection = select_typed_operator(
		member_candidates, nonmember_candidates, member_object,
		member_argument_nodes, member_arguments, nonmember_argument_nodes,
		nonmember_arguments, scope);
	if (!selection.valid())
		return ExprInfo();
	const TypeId result_type = function_result_type(selection.type);
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	SemanticFact fact(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	fact.has_callee = true;
	fact.bool_context_operand = bool_id(result_type);
	fact.direct_bool_boundary = bool_id(result_type);
	fact.operator_result = true;
	fact.has_implicit_object = selection.member;
	fact.selected_binding = selection.selected.binding;
	fact.selected_scope = selection.selected.scope;
	fact.callable_type = selection.member ? member_function_expression_type(
		selection.type, selection.selected.scope, selection.selected.binding) :
		selection.type;
	if (selection.member)
	{
		if (!member_object.fact.valid())
			throw std::runtime_error("PA12 operator member object is missing");
		fact.token = SimpleTokenType::OP_DOT;
	}
	if (!fact.callable_type.valid() || type_kind(fact.callable_type) !=
		TypeKind::Function)
		throw std::runtime_error("PA12 operator call signature is invalid");
	std::vector<SemanticFactId> children;
	if (selection.member)
		children.push_back(member_object.fact);
	for (std::size_t i = 0; i < selection.arguments.size(); ++i)
		children.push_back(selection.arguments[i].fact);
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_children(result, children);
	return ExprInfo(result, result_type, result_category, false);
}

const UserDefinedLiteralData* PA11SemanticModel::user_defined_literal_data(
	const PA10AstNode& node) const
{
	if (node.kind != PA10NodeKind::UserDefinedLiteral)
		return NULL;
	if (node.user_defined_literal_count != 1 ||
		node.user_defined_literal_begin > ast_.user_defined_literals.size() ||
		node.user_defined_literal_count > ast_.user_defined_literals.size() -
		node.user_defined_literal_begin)
		throw std::runtime_error("invalid PA10 user-defined literal sidecar");
	const UserDefinedLiteralData& data = ast_.user_defined_literals[
		node.user_defined_literal_begin];
	if (data.source.empty() || data.suffix.empty() || node.text == 0 ||
		ast_.spelling(node.text) != data.source)
		throw std::runtime_error("invalid PA10 user-defined literal data");
	return &data;
}
NameId PA11SemanticModel::literal_operator_suffix(const DeclaratorName& name)
{
	if (name.operator_function_kind != PA10OperatorFunctionKind::Literal)
		return NameId();
	if (name.operator_literal_data_count != 1 ||
		name.operator_literal_data_begin > ast_.user_defined_literals.size() ||
		name.operator_literal_data_count > ast_.user_defined_literals.size() -
		name.operator_literal_data_begin)
		throw std::runtime_error("literal operator has no typed suffix");
	const UserDefinedLiteralData& data = ast_.user_defined_literals[
		name.operator_literal_data_begin];
	if (data.source.empty() || data.suffix.empty() ||
		data.kind != UserDefinedLiteralKind::String)
		throw std::runtime_error("literal operator has invalid typed suffix");
	return intern_name(data.suffix);
}

ExprInfo PA11SemanticModel::semantic_user_defined_literal(
	const PA10AstNode& node, ScopeId scope)
{
	const UserDefinedLiteralData* data = user_defined_literal_data(node);
	if (data == NULL || data->kind != UserDefinedLiteralKind::String)
		throw std::runtime_error(
			"PA12 unsupported non-string user-defined literal");
	switch (data->value.type)
	{
	case FundamentalType::Char:
	case FundamentalType::WcharT:
	case FundamentalType::Char16T:
	case FundamentalType::Char32T:
		break;
	default:
		throw std::runtime_error(
			"PA12 invalid cooked string element type");
	}
	const TypeId element = fundamental(data->value.type);
	const std::size_t element_size = type_size(element);
	const std::size_t count = data->value.element_count;
	if (!element.valid() || element_size == 0 || count == 0 ||
		count > std::numeric_limits<std::size_t>::max() / element_size ||
		data->value.bytes.size() != count * element_size)
		throw std::runtime_error("PA12 invalid user-defined string payload");
	const TypeId array = make_array(make_cv(element, 1u), false,
		ArrayBound(count));
	SemanticFact text_fact(SemanticFactKind::Literal, array,
		SemanticValueCategory::Lvalue, &node);
	text_fact.literal_element_count = count;
	const SemanticFactId text_id = make_semantic_fact(text_fact);
	const ExprInfo text(text_id, array, SemanticValueCategory::Lvalue, false);

	// The cooked string form supplies the source array and a typed size_t
	// argument.  Keep the size as a synthetic scalar fact, as existing
	// selection/lowering machinery already handles its integral conversion.
	const bool size_unsigned = count - 1 > static_cast<std::size_t>(
		std::numeric_limits<int>::max());
	const TypeId size_type = fundamental(size_unsigned ?
		FundamentalType::UnsignedLongInt : FundamentalType::Int);
	const std::size_t size = count - 1;
	SemanticFact size_fact(SemanticFactKind::Literal, size_type,
		SemanticValueCategory::Prvalue, &node);
	size_fact.has_literal_value = true;
	size_fact.literal_value = static_cast<std::uint64_t>(size);
	size_fact.literal_value_unsigned = size_unsigned;
	size_fact.has_constant_value = true;
	size_fact.constant_value = static_cast<__int128>(size);
	size_fact.constant_value_unsigned = size_unsigned;
	size_fact.constant_value_evaluated = true;
	const SemanticFactId size_id = make_semantic_fact(size_fact);
	const ExprInfo size_expression(size_id, size_type,
		SemanticValueCategory::Prvalue, false);

	NamePath path;
	path.components.push_back(operator_name(
		PA10OperatorFunctionKind::Literal, SimpleTokenType::OP_SEMICOLON));
	const NameId suffix = intern_name(data->suffix);
	const std::vector<ValueRef> visible = lookup_value_path(path, scope,
		SourcePoint(node.source_begin), suffix);
	std::vector<ValueRef> candidates;
	FlatIndex<BindingId, bool, IdentityHash<BindingId> > candidate_seen;
	for (std::size_t i = 0; i < visible.size(); ++i)
	{
		const ValueRef& candidate = visible[i];
		if (!candidate.binding.valid() || candidate.binding.value >=
			bindings_.size())
			throw std::runtime_error(
				"PA12 literal operator candidate binding is invalid");
		if (candidate.binding.value >= binding_owners_.size())
			throw std::runtime_error(
				"PA12 literal operator candidate owner is missing");
		if (!candidate.scope.valid() || candidate.scope.value >= scopes_.size())
			throw std::runtime_error(
				"PA12 literal operator candidate scope is invalid");
		const ScopeId owner = binding_owners_[candidate.binding.value];
		if (!owner.valid() || owner.value >= scopes_.size() || owner !=
			candidate.scope)
			throw std::runtime_error(
				"PA12 literal operator candidate owner mismatch");
		const Binding& binding_value = binding(candidate.binding);
		if (binding_value.name != path.components.back() ||
			binding_value.kind != BindingKind::Function ||
			!binding_value.type.valid() || binding_value.type.value >= types_.size() ||
			type_kind(binding_value.type) != TypeKind::Function)
			throw std::runtime_error(
				"PA12 literal operator candidate function/type is invalid");
		if (scopes_[candidate.scope.value].kind != ScopeKind::Namespace)
			throw std::runtime_error(
				"PA12 literal operator owner is not a namespace");
		const BindingSidecar* sidecar = binding_sidecar(candidate.binding);
		if (sidecar == NULL || sidecar->operator_function_kind !=
			PA10OperatorFunctionKind::Literal ||
			sidecar->operator_literal_suffix != suffix)
			throw std::runtime_error(
				"PA12 literal operator candidate suffix is invalid");
		const TypeKey& function = types_[binding_value.type.value];
		const TypeId expected_text = make_pointer(make_cv(element, 1u));
		const TypeId expected_size = fundamental(
			FundamentalType::UnsignedLongInt);
		if (function.variadic || function.parameters.size() != 2 ||
			normalize_parameter_type(function.parameters[0]) != expected_text ||
			normalize_parameter_type(function.parameters[1]) != expected_size)
			continue;
		if (candidate_seen.find(candidate.binding) != NULL)
			continue;
		candidate_seen.set(candidate.binding, true);
		candidates.push_back(candidate);
	}
	if (candidates.empty())
		throw std::runtime_error("PA12 no literal operator candidate");

	std::vector<const PA10AstNode*> no_member_nodes;
	std::vector<ExprInfo> no_member_arguments;
	std::vector<const PA10AstNode*> argument_nodes;
	argument_nodes.push_back(&node);
	argument_nodes.push_back(&node);
	std::vector<ExprInfo> arguments;
	arguments.push_back(text);
	arguments.push_back(size_expression);
	const TypedOperatorSelection selection = select_typed_operator(
		std::vector<ValueRef>(), candidates, text, no_member_nodes,
		no_member_arguments, argument_nodes, arguments, scope);
	if (!selection.valid() || selection.member)
		throw std::runtime_error("PA12 literal operator selection failed");
	const TypeId result_type = function_result_type(selection.type);
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	SemanticFact call(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	call.has_callee = true;
	call.bool_context_operand = bool_id(result_type);
	call.direct_bool_boundary = bool_id(result_type);
	call.selected_binding = selection.selected.binding;
	call.selected_scope = selection.selected.scope;
	call.callable_type = selection.type;
	if (!call.callable_type.valid() || type_kind(call.callable_type) !=
		TypeKind::Function)
		throw std::runtime_error("PA12 literal operator signature is invalid");
	std::vector<SemanticFactId> children;
	for (std::size_t i = 0; i < selection.arguments.size(); ++i)
		children.push_back(selection.arguments[i].fact);
	const SemanticFactId result = make_semantic_fact(call);
	set_semantic_children(result, children);
	return ExprInfo(result, result_type, result_category, false);
}

void PA11SemanticModel::apply_call_argument_conversions(
	std::vector<ExprInfo>& arguments, TypeId selected_type, ScopeId scope)
{
	const TypeKey function = types_[selected_type.value];
	const std::size_t fixed_count = function.parameters.size();
	for (std::size_t arg = 0; arg < arguments.size(); ++arg)
	{
		if (!arguments[arg].fact.valid())
		{
			if (arg >= fixed_count)
				throw std::runtime_error("PA12 variadic argument fact is missing");
			continue;
		}
		const SemanticFactId fact_id = arguments[arg].fact;
		const SemanticFact& fact = semantic_facts_[fact_id.value];
		const bool variadic = arg >= fixed_count;
		const TypeId source = strip_cv_type(expression_object_type(
			arguments[arg].type));
		if (fact.literal_element_count != 0)
		{
			if (variadic && source.valid() && type_kind(source) == TypeKind::Array)
			{
				const TypeId pointer = make_pointer(types_[source.value].child);
				arguments[arg] = apply_context_conversion(arguments[arg], pointer,
					fact.source, scope);
			}
			record_constant_address(fact_id, scope);
			continue;
		}
		if (!variadic) continue;
		if (source.valid() && type_kind(source) == TypeKind::Array)
		{
			const TypeId pointer = make_pointer(types_[source.value].child);
			arguments[arg] = apply_context_conversion(arguments[arg], pointer,
				fact.source, scope);
		}
		else if (source.valid() && type_kind(source) == TypeKind::Function)
		{
			const TypeId pointer = make_pointer(source);
			arguments[arg] = apply_context_conversion(arguments[arg], pointer,
				fact.source, scope);
		}
		else if (floating_id(source))
		{
			FundamentalType fundamental_type;
			TypeId target = source;
			if (fundamental_of(source, &fundamental_type) &&
				fundamental_type == FundamentalType::Float)
				target = fundamental(FundamentalType::Double);
			arguments[arg] = apply_context_conversion(arguments[arg], target,
				fact.source, scope);
		}
		else if (integral_id(source))
		{
			const TypeId promoted = integral_operation_type(arguments[arg]);
			arguments[arg] = apply_context_conversion(arguments[arg], promoted,
				fact.source, scope);
		}
		else if (pointer_id(source))
		{
			// The default argument conversion of a pointer lvalue is still
			// lvalue-to-rvalue even when its pointer type is unchanged.
			arguments[arg] = apply_context_conversion(arguments[arg], source,
				fact.source, scope);
		}
	}
}

}

namespace pa11_semantic_internal
{
ExprInfo PA11SemanticModel::semantic_call_expression(const PA10AstNode& node, ScopeId scope)
{
	LookupPointGuard lookup_point(*this, SourcePoint(node.source_begin));
	if (node.children.size() != 2)
		throw std::runtime_error("PA12 invalid call expression");
	const PA10AstNode& callee_node = node.children.front();
	const PA10AstNode& argument_node = node.children.back();
	if (argument_node.kind != PA10NodeKind::ArgumentList &&
		argument_node.kind != PA10NodeKind::ParenArgumentList)
		throw std::runtime_error("PA12 invalid argument list");
	const BuiltinKind builtin = builtin_kind(callee_node, scope);
	if (builtin != BuiltinKind::None)
		return semantic_builtin_call(node, scope, builtin, argument_node);
	std::vector<ValueRef> qualified_static_candidates;
	ScopeId qualified_static_scope;
	const bool qualified_class_member = qualified_static_member_candidates(
		callee_node, scope,
		&qualified_static_candidates, &qualified_static_scope);
	const ExprInfo member_call = !qualified_class_member ?
		semantic_member_call_probe(node, scope) : ExprInfo();
	if (member_call.fact.valid())
		return member_call;
	if (!qualified_class_member && qualified_static_scope.valid())
		throw std::runtime_error("PA12 class-qualified static call has no target");
	TypeId functional_target;
	if (functional_cast_target(callee_node, scope, &functional_target))
		return semantic_functional_cast(node, scope, functional_target,
			argument_node);
	if (callee_node.kind == PA10NodeKind::IdExpression &&
		!has_template_id(callee_node))
	{
		const NamePath path = name_path(callee_node);
		if (lookup_value_path(path, scope).empty())
		{
			const TemplateFunctionList* templates = template_functions(path, scope);
			if (templates != NULL && !templates->entries.empty())
				return semantic_template_call(node, scope, *templates,
					argument_node);
		}
	}
	if (qualified_class_member && qualified_static_candidates.empty())
		throw std::runtime_error("PA12 class-qualified static call has no target");
	std::vector<ValueRef> candidates = direct_call_candidates(callee_node,
		scope, qualified_class_member, qualified_static_candidates);
	std::vector<ExprInfo> arguments;
	bool arguments_ready = false;
	bool allow_class_value = false;
	// ADL is formed only for an unqualified-id.  Ordinary lookup remains the
	// first candidate source; an empty ordinary set is also the point at which
	// an otherwise unknown unqualified name can be recovered through ADL.
	if (!qualified_class_member && callee_node.kind == PA10NodeKind::IdExpression &&
		!has_template_id(callee_node))
	{
		const NamePath path = name_path(callee_node);
		const SourcePoint point = lookup_source_point(scope);
		const std::vector<ValueRef> ordinary = lookup_value_path(path, scope, point);
		const bool unqualified = !path.global && path.components.size() == 1;
		bool ordinary_lookup_suppresses_adl = false;
		for (std::size_t i = 0; i < ordinary.size(); ++i)
		{
			if (!ordinary[i].binding.valid() || ordinary[i].binding.value >=
				bindings_.size() || !ordinary[i].scope.valid() ||
				ordinary[i].scope.value >= scopes_.size())
				throw std::runtime_error("PA12 ordinary call candidate identity is invalid");
			const Binding& value = binding(ordinary[i].binding);
			if (value.kind != BindingKind::Function ||
				scopes_[ordinary[i].scope.value].kind != ScopeKind::Namespace)
				ordinary_lookup_suppresses_adl = true;
		}
		const bool allow_adl = unqualified && !ordinary_lookup_suppresses_adl &&
			(candidates.empty() ? ordinary.empty() : !ordinary.empty());
		if (allow_adl)
		{
			allow_class_value = true;
			arguments_ready = true;
			arguments.reserve(argument_node.children.size());
			for (std::size_t i = 0; i < argument_node.children.size(); ++i)
			{
				if (target_function_id(argument_node.children[i], scope) != NULL)
					arguments.push_back(ExprInfo());
				else
					arguments.push_back(semantic_expression(
						argument_node.children[i], scope));
			}
			std::vector<TypeId> associated_objects;
			associated_objects.reserve(arguments.size());
			for (std::size_t i = 0; i < arguments.size(); ++i)
				if (arguments[i].fact.valid())
					associated_objects.push_back(arguments[i].type);
			append_adl_function_candidates(path.last(), associated_objects, point,
				&candidates);
		}
	}
	const bool direct = !candidates.empty();
	ExprInfo indirect_callee;
	TypeId indirect_type;
	if (!direct)
	{
		indirect_callee = semantic_expression(callee_node, scope);
		const TypeId callee_object = strip_cv_type(expression_object_type(
			indirect_callee.type));
		const NamedRecordId callee_record = named_record_for_type(callee_object);
		if (callee_record.valid() && callee_record.value < named_.size() &&
			named_[callee_record.value].kind == NamedKind::Class)
		{
			std::vector<const PA10AstNode*> member_nodes;
			std::vector<ExprInfo> member_arguments;
			for (std::size_t i = 0; i < argument_node.children.size(); ++i)
			{
				member_nodes.push_back(&argument_node.children[i]);
				if (target_function_id(argument_node.children[i], scope) != NULL)
					member_arguments.push_back(ExprInfo());
				else
					member_arguments.push_back(semantic_expression(
						argument_node.children[i], scope));
			}
			std::vector<TypeId> associated_objects;
			associated_objects.push_back(indirect_callee.type);
			const std::vector<const PA10AstNode*> no_nonmember_nodes;
			const std::vector<ExprInfo> no_nonmember_arguments;
			const ExprInfo overloaded = semantic_operator_call(node, scope,
				PA10OperatorFunctionKind::Call, node.token, indirect_callee,
				associated_objects, member_nodes, member_arguments,
				no_nonmember_nodes, no_nonmember_arguments);
			if (overloaded.fact.valid())
				return overloaded;
		}
		indirect_type = callable_function_type(indirect_callee.type);
		if (!indirect_type.valid())
			throw std::runtime_error("PA12 call target is not callable");
		const TypeKey& function = types_[indirect_type.value];
		if ((!function.variadic && argument_node.children.size() !=
			function.parameters.size()) ||
			(function.variadic && argument_node.children.size() <
				function.parameters.size()))
			throw std::runtime_error("PA12 indirect call arity mismatch");
	}
	if (!direct)
	{
		arguments.clear();
		const TypeKey& function = types_[indirect_type.value];
		record_builtin_conversion(indirect_callee, make_pointer(indirect_type));
		for (std::size_t i = 0; i < argument_node.children.size(); ++i)
		{
			if (i < function.parameters.size())
				arguments.push_back(semantic_expression_for_target(
					argument_node.children[i], scope, function.parameters[i]));
			else
				arguments.push_back(semantic_expression(argument_node.children[i],
					scope));
		}
	}
	else if (!arguments_ready)
	{
		// An overloaded function ID has no expression type until a target
		// function pointer/reference parameter is selected.  Keep it deferred;
		// all ordinary arguments are still analyzed exactly once here.
		for (std::size_t i = 0; i < argument_node.children.size(); ++i)
		{
			if (target_function_id(argument_node.children[i], scope) != NULL)
				arguments.push_back(ExprInfo());
			else
				arguments.push_back(semantic_expression(argument_node.children[i],
					scope));
		}
	}
	ValueRef selected;
	TypeId selected_type;
	if (direct)
	{
		std::vector<const PA10AstNode*> argument_nodes;
		argument_nodes.reserve(argument_node.children.size());
		for (std::size_t i = 0; i < argument_node.children.size(); ++i)
			argument_nodes.push_back(&argument_node.children[i]);
		const TypedFunctionSelection selection = select_typed_function(
			candidates, argument_nodes, arguments, scope,
			allow_class_value);
		selected = selection.selected;
		selected_type = selection.type;
		arguments = selection.arguments;
		validate_direct_static_member_call(selected, qualified_class_member,
			qualified_static_scope, scope);
	}
	else
	{
		selected_type = indirect_type;
		const TypeKey function = types_[selected_type.value];
		for (std::size_t arg = 0; arg < function.parameters.size(); ++arg)
				arguments[arg] = apply_context_conversion(arguments[arg],
					function.parameters[arg],
					semantic_facts_[arguments[arg].fact.value].source, scope);
	}
	if (!direct)
		apply_call_argument_conversions(arguments, selected_type, scope);
	const TypeId result_type = function_result_type(selected_type);
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	std::vector<SemanticFactId> children;
	if (!direct)
		children.push_back(indirect_callee.fact);
	for (std::size_t i = 0; i < arguments.size(); ++i)
		children.push_back(arguments[i].fact);
	SemanticFact fact(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	fact.has_callee = direct;
	fact.bool_context_operand = bool_id(result_type);
	fact.direct_bool_boundary = bool_id(result_type);
	fact.callable_type = selected_type;
	if (direct)
	{
		fact.selected_binding = selected.binding;
		fact.selected_scope = selected.scope;
	}
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_children(result, children);
	return ExprInfo(result, result_type, result_category, false);
}
}
