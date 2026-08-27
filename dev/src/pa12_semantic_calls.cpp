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
			bindings_.size() || !candidate_ref.scope.valid() ||
			candidate_ref.scope.value >= scopes_.size())
			throw std::runtime_error("PA12 function candidate identity is invalid");
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
