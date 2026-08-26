#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{

void PA11SemanticModel::apply_call_argument_conversions(
	std::vector<ExprInfo>& arguments, TypeId selected_type, ScopeId scope)
{
	const TypeKey& function = types_[selected_type.value];
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
			if (fundamental_of(source, &fundamental_type) &&
				fundamental_type == FundamentalType::Float)
				arguments[arg] = apply_context_conversion(arguments[arg],
					fundamental(FundamentalType::Double), fact.source);
		}
		else if (integral_id(source))
		{
			const TypeId promoted = promote_integral_type(source);
			if (promoted != source)
				arguments[arg] = apply_context_conversion(arguments[arg], promoted,
					fact.source);
		}
	}
}

}
