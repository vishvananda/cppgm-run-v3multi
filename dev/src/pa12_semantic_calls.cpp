#include "pa11_semantic_model.h"

#include <limits>

namespace pa11_semantic_internal
{

TypedFunctionSelection PA11SemanticModel::select_typed_function(
	const std::vector<ValueRef>& candidates,
	const std::vector<const PA10AstNode*>& argument_nodes,
	const std::vector<ExprInfo>& initial_arguments, ScopeId scope,
	bool reject_class_by_value)
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
		std::vector<unsigned int> ranks;
	};
	std::vector<CandidateScore> viable;
	const unsigned int ellipsis_rank = std::numeric_limits<unsigned int>::max() / 4;
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
		if (reject_class_by_value)
		{
			bool supported = true;
			for (std::size_t parameter = 0;
				parameter < function.parameters.size(); ++parameter)
			{
				const TypeId parameter_type = function.parameters[parameter];
				const TypeId parameter_object = strip_cv_type(
					expression_object_type(parameter_type));
				const NamedRecordId parameter_record =
					named_record_for_type(parameter_object);
				if (type_kind(parameter_type) != TypeKind::LvalueReference &&
					type_kind(parameter_type) != TypeKind::RvalueReference &&
					parameter_record.valid() && parameter_record.value < named_.size() &&
					named_[parameter_record.value].kind == NamedKind::Class)
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
			function.variadic, std::vector<unsigned int>()};
		score.ranks.reserve(arguments.size());
		for (std::size_t argument = 0; argument < arguments.size(); ++argument)
		{
			if (argument >= function.parameters.size())
			{
				if (!arguments[argument].fact.valid())
					break;
				score.ranks.push_back(ellipsis_rank);
				continue;
			}
			ConversionChoice choice;
			if (arguments[argument].fact.valid())
			{
				if (arguments[argument].fact.value >= semantic_facts_.size())
					throw std::runtime_error("PA12 function argument fact is invalid");
				const SemanticFact& fact =
					semantic_facts_[arguments[argument].fact.value];
				choice = conversion_for(arguments[argument].type,
					arguments[argument].category, function.parameters[argument],
					fact.source, arguments[argument].integer_zero);
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
				break;
			score.ranks.push_back(choice.rank);
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
			if (left.ranks[i] > right.ranks[i])
				return false;
			if (left.ranks[i] < right.ranks[i])
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
		if (!arguments[argument].fact.valid())
			arguments[argument] = semantic_expression_for_target(
				*argument_nodes[argument], scope,
				selected_function.parameters[argument]);
		if (!arguments[argument].fact.valid() || arguments[argument].fact.value >=
			semantic_facts_.size())
			throw std::runtime_error("PA12 selected function argument is invalid");
		const PA10AstNode* source = semantic_facts_[
			arguments[argument].fact.value].source;
		arguments[argument] = apply_context_conversion(arguments[argument],
			selected_function.parameters[argument], source);
	}
	apply_call_argument_conversions(arguments, selected_type, scope);
	TypedFunctionSelection result(selected, selected_type);
	result.arguments.swap(arguments);
	return result;
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

	// Form the associated class/enum set from the operand types.  The walk is
	// bounded by the already-formed direct-base chains and enclosing class
	// scopes; it never scans unrelated program declarations.
	std::vector<NamedRecordId> associated_records;
	for (std::size_t object_index = 0; object_index < associated_objects.size();
		++object_index)
	{
		TypeId object = strip_cv_type(expression_object_type(
			associated_objects[object_index]));
		if (!object.valid() || type_kind(object) != TypeKind::Named)
			continue;
		std::vector<NamedRecordId> pending;
		const NamedRecordId initial = named_record_for_type(object);
		if (initial.valid())
			pending.push_back(initial);
		while (!pending.empty())
		{
			const NamedRecordId record_id = pending.back();
			pending.pop_back();
			if (!record_id.valid() || record_id.value >= named_.size())
				continue;
			bool seen = false;
			for (std::size_t i = 0; i < associated_records.size(); ++i)
				if (associated_records[i] == record_id)
				{
					seen = true;
					break;
				}
			if (seen)
				continue;
			const NamedRecord& record = named_[record_id.value];
			if (record.kind != NamedKind::Class && record.kind != NamedKind::Enum)
				continue;
			associated_records.push_back(record_id);
			if (record.kind == NamedKind::Class)
			{
				std::vector<NamedRecordId> bases;
				if (!direct_base_chain(named_type(record_id), &bases))
					throw std::runtime_error("PA12 operator base relation is invalid");
				for (std::size_t i = 0; i < bases.size(); ++i)
					pending.push_back(bases[i]);
			}
			if (record.owner.valid() && record.owner.value < scopes_.size() &&
				scopes_[record.owner.value].kind == ScopeKind::Class &&
				scopes_[record.owner.value].record.valid())
				pending.push_back(scopes_[record.owner.value].record);
		}
	}
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
		for (std::size_t i = 0; i < associated_records.size(); ++i)
		{
			ScopeId cursor = named_[associated_records[i].value].owner;
			while (cursor.valid() && cursor.value < scopes_.size() &&
				scopes_[cursor.value].kind != ScopeKind::Namespace)
				cursor = scopes_[cursor.value].parent;
			if (!cursor.valid())
				continue;
			bool seen = false;
			for (std::size_t j = 0; j < associated_namespaces.size(); ++j)
				if (associated_namespaces[j] == cursor)
				{
					seen = true;
					break;
				}
			if (!seen)
				associated_namespaces.push_back(cursor);
		}
		for (std::size_t i = 0; i < associated_namespaces.size(); ++i)
		{
			begin_lookup();
			std::vector<ValueRef> candidates;
			lookup_value_graph(associated_namespaces[i], name, &candidates, true,
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
	const std::vector<ExprInfo>& nonmember_arguments, ScopeId scope,
	bool reject_class_by_value)
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
		std::size_t object_base_distance;
		unsigned int object_cv;
		std::vector<unsigned int> ranks;
	};
	std::vector<CandidateScore> viable;
	const unsigned int ellipsis_rank = std::numeric_limits<unsigned int>::max() / 4;
	// An implicit conversion to a class reference may use one non-explicit
	// converting constructor.  Keep this probe at the typed operator boundary:
	// it checks the canonical constructor index, access, and the constructor's
	// single parameter conversion without publishing speculative semantic facts.
	const auto implicit_constructor_conversion = [this, scope] (
		const ExprInfo& argument, const PA10AstNode* argument_node,
		TypeId target) -> ConversionChoice
	{
		const TypeKind target_kind = type_kind(target);
		if ((target_kind != TypeKind::LvalueReference &&
			target_kind != TypeKind::RvalueReference) ||
			!argument.fact.valid() || argument.fact.value >= semantic_facts_.size())
			return ConversionChoice();
		const TypeId referred = types_[target.value].child;
		if (target_kind == TypeKind::LvalueReference &&
			type_kind(referred) != TypeKind::Cv)
			return ConversionChoice();
		const TypeId object = strip_cv_type(expression_object_type(referred));
		const NamedRecordId record_id = named_record_for_type(object);
		if (!record_id.valid() || record_id.value >= named_.size() ||
			named_[record_id.value].kind != NamedKind::Class ||
			named_[record_id.value].class_tag == ClassTag::Union)
			return ConversionChoice();
		const NamedRecord& record = named_[record_id.value];
		if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
			scopes_[record.scope.value].kind != ScopeKind::Class ||
			!record.name.valid())
			throw std::runtime_error("PA12 implicit constructor owner is invalid");
		const ValueList* values = scopes_[record.scope.value].values.find(record.name);
		if (values == NULL)
			return ConversionChoice();
		const SemanticFact& source_fact = semantic_facts_[argument.fact.value];
		bool found = false;
		bool ambiguous = false;
		unsigned int best_rank = std::numeric_limits<unsigned int>::max();
		std::vector<BindingId> seen;
		for (std::size_t i = 0; i < values->entries.size(); ++i)
		{
			const ValueEntry& entry = values->entries[i];
			const BindingId candidate_id = entry.binding;
			if (!candidate_id.valid() || candidate_id.value >= bindings_.size() ||
				candidate_id.value >= binding_owners_.size() ||
				binding_owners_[candidate_id.value] != record.scope ||
				entry.origin != record.scope)
				throw std::runtime_error(
					"PA12 implicit constructor index identity is invalid");
			for (std::size_t prior = 0; prior < seen.size(); ++prior)
				if (seen[prior] == candidate_id)
					throw std::runtime_error(
						"PA12 duplicate implicit constructor index entry");
			seen.push_back(candidate_id);
			const Binding& candidate = binding(candidate_id);
			if (candidate.kind != BindingKind::Function || !candidate.type.valid() ||
				candidate.type.value >= types_.size() ||
				type_kind(candidate.type) != TypeKind::Function)
				continue;
			const BindingSidecar* sidecar = binding_sidecar(candidate_id);
			if (sidecar == NULL || sidecar->constructor_record != record_id ||
				sidecar->explicit_constructor ||
				function_declaration_kind(candidate_id) ==
					FunctionDeclarationKind::Deleted)
				continue;
			const TypeKey function = types_[candidate.type.value];
			if (function.variadic || function.parameters.size() != 1)
				continue;
			const TypeId parameter = function.parameters.front();
			const NamedRecordId parameter_record = named_record_for_type(
				strip_cv_type(expression_object_type(parameter)));
			if (parameter_record.valid() && parameter_record.value < named_.size() &&
				named_[parameter_record.value].kind == NamedKind::Class &&
				type_kind(parameter) != TypeKind::LvalueReference &&
				type_kind(parameter) != TypeKind::RvalueReference)
				continue;
			if (!member_accessible(candidate_id, record.scope, scope, object))
				continue;
			const ConversionChoice conversion = conversion_for(argument.type,
				argument.category, parameter, source_fact.source, argument.integer_zero);
			if (!conversion.valid)
				continue;
			const unsigned int rank = conversion.rank <
				std::numeric_limits<unsigned int>::max() - 3 ?
				3 + conversion.rank : std::numeric_limits<unsigned int>::max();
			if (!found || rank < best_rank)
			{
				found = true;
				ambiguous = false;
				best_rank = rank;
			}
			else if (rank == best_rank)
				ambiguous = true;
		}
		(void)argument_node;
		return found && !ambiguous ? ConversionChoice(true, best_rank,
			ConversionKind::ReferenceBinding) : ConversionChoice();
	};
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
						candidate_ref.scope, NULL))
					continue;
				if (!member_accessible(candidate_ref.binding, candidate_ref.scope,
					scope, expression_object_type(object.type)))
					continue;
			}
			else if (scopes_[candidate_ref.scope.value].kind != ScopeKind::Namespace)
				continue;
			const TypeKey function = types_[candidate.type.value];
			if (reject_class_by_value)
			{
				bool supported = true;
				for (std::size_t parameter = 0;
					parameter < function.parameters.size(); ++parameter)
				{
					const TypeId parameter_type = function.parameters[parameter];
					const TypeId parameter_object = strip_cv_type(
						expression_object_type(parameter_type));
					const NamedRecordId parameter_record =
						named_record_for_type(parameter_object);
					if (type_kind(parameter_type) != TypeKind::LvalueReference &&
						type_kind(parameter_type) != TypeKind::RvalueReference &&
						parameter_record.valid() && parameter_record.value < named_.size() &&
						named_[parameter_record.value].kind == NamedKind::Class)
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
			if (initial_arguments.size() < required ||
				(!function.variadic && initial_arguments.size() >
				function.parameters.size()))
				continue;
			CandidateScore score;
			score.value = candidate_ref;
			score.type = candidate.type;
			score.member = member;
			score.variadic = function.variadic;
			score.object_base_distance = 0;
			score.object_cv = 0;
			score.ranks.reserve(initial_arguments.size() + (member ? 1 : 0));
			if (member)
			{
				const TypeId required_object = member_object_type(candidate.type,
					candidate_ref.scope);
				std::vector<NamedRecordId> object_path;
				if (!member_object_convertible(expression_object_type(object.type),
					required_object, candidate_ref.scope, &object_path))
					throw std::runtime_error(
						"PA12 operator object conversion changed during scoring");
				score.object_base_distance = object_path.size();
				score.object_cv = cv_qualifiers(required_object) &
					~cv_qualifiers(expression_object_type(object.type));
				const unsigned int object_rank = object_path.empty() ?
					0 :
					1 + (score.object_cv == 0 ? 0 : 1);
				score.ranks.push_back(object_rank);
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
					score.ranks.push_back(ellipsis_rank);
					continue;
				}
				ConversionChoice choice;
				if (initial_arguments[argument].fact.valid())
					choice = conversion_for(initial_arguments[argument].type,
						initial_arguments[argument].category,
						function.parameters[argument],
						semantic_facts_[initial_arguments[argument].fact.value].source,
						initial_arguments[argument].integer_zero);
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
				if (!choice.valid)
					choice = implicit_constructor_conversion(initial_arguments[argument],
						argument_nodes[argument], function.parameters[argument]);
				if (!choice.valid)
				{
					arguments_viable = false;
					break;
				}
				if (!member && argument == 0)
				{
					const TypeId required_object = expression_object_type(
						function.parameters.front());
					const TypeId actual_object = expression_object_type(object.type);
					if (required_object.valid() && actual_object.valid())
						score.object_cv = cv_qualifiers(required_object) &
							~cv_qualifiers(actual_object);
				}
				score.ranks.push_back(choice.rank);
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
			if (left.ranks[i] > right.ranks[i])
				return false;
			if (left.ranks[i] < right.ranks[i])
				strict = true;
		}
		if (left.member && right.member &&
			left.object_base_distance != right.object_base_distance)
		{
			if (left.object_base_distance > right.object_base_distance)
				return false;
			strict = true;
		}
		// Qualification conversions are a subset order, not a bit-count order:
		// const and volatile remain incomparable while an exact object beats one
		// that adds either qualifier.
		if (left.object_cv != right.object_cv)
		{
			if ((left.object_cv & ~right.object_cv) != 0)
				return false;
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
			!conversion_for(arguments[argument].type, arguments[argument].category,
				parameter,
				semantic_facts_[arguments[argument].fact.value].source,
				arguments[argument].integer_zero).valid)
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
			semantic_facts_[arguments[argument].fact.value].source);
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
	const std::vector<ExprInfo>& nonmember_arguments,
	bool reject_class_by_value)
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
		nonmember_arguments, scope, reject_class_by_value);
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
					fact.source);
			}
			record_constant_address(fact_id, scope);
			continue;
		}
		if (!variadic) continue;
		if (source.valid() && type_kind(source) == TypeKind::Array)
		{
			const TypeId pointer = make_pointer(types_[source.value].child);
			arguments[arg] = apply_context_conversion(arguments[arg], pointer,
				fact.source);
		}
		else if (source.valid() && type_kind(source) == TypeKind::Function)
		{
			const TypeId pointer = make_pointer(source);
			arguments[arg] = apply_context_conversion(arguments[arg], pointer,
				fact.source);
		}
		else if (floating_id(source))
		{
			FundamentalType fundamental_type;
			TypeId target = source;
			if (fundamental_of(source, &fundamental_type) &&
				fundamental_type == FundamentalType::Float)
				target = fundamental(FundamentalType::Double);
			arguments[arg] = apply_context_conversion(arguments[arg], target,
				fact.source);
		}
		else if (integral_id(source))
		{
			const TypeId promoted = promote_integral_type(source);
			arguments[arg] = apply_context_conversion(arguments[arg], promoted,
				fact.source);
		}
		else if (pointer_id(source))
		{
			// The default argument conversion of a pointer lvalue is still
			// lvalue-to-rvalue even when its pointer type is unchanged.
			arguments[arg] = apply_context_conversion(arguments[arg], source,
				fact.source);
		}
	}
}

}

namespace pa11_semantic_internal
{
ExprInfo PA11SemanticModel::semantic_call_expression(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 2)
		throw std::runtime_error("PA12 invalid call expression");
	const PA10AstNode& callee_node = node.children.front();
	const PA10AstNode& argument_node = node.children.back();
	if (argument_node.kind != PA10NodeKind::ArgumentList &&
		argument_node.kind != PA10NodeKind::ParenArgumentList)
		throw std::runtime_error("PA12 invalid argument list");
	const BuiltinKind builtin = builtin_kind(callee_node);
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
	const std::vector<ValueRef> candidates = direct_call_candidates(callee_node,
		scope, qualified_class_member, qualified_static_candidates);
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
				no_nonmember_nodes, no_nonmember_arguments, true);
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
	std::vector<ExprInfo> arguments;
	if (!direct)
	{
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
	else
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
			candidates, argument_nodes, arguments, scope);
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
				semantic_facts_[arguments[arg].fact.value].source);
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
