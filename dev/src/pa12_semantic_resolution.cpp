#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

bool PA11SemanticModel::pointer_common_type_convertible(TypeId source,
	TypeId target) const
{
	if (pointer_convertible(source, target))
		return true;
	source = strip_cv_type(source);
	target = strip_cv_type(target);
	if (type_kind(source) != TypeKind::Pointer ||
		type_kind(target) != TypeKind::Pointer)
		return false;
	const TypeId source_pointee = types_[source.value].child;
	const TypeId target_pointee = types_[target.value].child;
	if (!object_type(source_pointee) || !object_type(target_pointee) ||
		(cv_qualifiers(source_pointee) & ~cv_qualifiers(target_pointee)) != 0)
		return false;
	// This no-scope walk is a common-type discovery fact only.  The caller must
	// still commit each branch through conversion_for with its access scope.
	return derived_base_path(source_pointee, target_pointee, NULL);
}

ConversionChoice PA11SemanticModel::conversion_for(TypeId source,
	SemanticValueCategory category, TypeId target,
	const PA10AstNode* source_node, bool source_integer_zero,
	ScopeId access_scope, TypeId source_operation_type,
	BindingId source_binding) const
{
	if (!source.valid() || !target.valid())
		return ConversionChoice();
	const BitFieldFact* source_bit_field = source_binding.valid() ?
		bit_field_fact(source_binding) : NULL;
	if (source_binding.valid() && (source_bit_field == NULL ||
		!source_bit_field->named || source_bit_field->binding != source_binding ||
		!source_bit_field->operation_type.valid() ||
		source_operation_type != source_bit_field->operation_type))
		throw std::runtime_error("PA12 bit-field conversion fact is invalid");
	const TypeKind target_kind = type_kind(target);
	if (target_kind == TypeKind::LvalueReference ||
		target_kind == TypeKind::RvalueReference)
	{
		const TypeId target_referred = types_[target.value].child;
		const TypeId source_value = expression_object_type(source);
		const bool source_lvalue = category == SemanticValueCategory::Lvalue;
		const auto derived_base_choice = [this, access_scope](TypeId actual,
			TypeId required, ConversionChoice* choice) -> bool
		{
			if (choice == NULL || !access_scope.valid() ||
				(cv_qualifiers(actual) & ~cv_qualifiers(required)) != 0)
				return false;
			unsigned int base_distance = 0;
			if (!derived_base_relation(actual, required, &base_distance, NULL,
				access_scope) || base_distance == 0)
				return false;
			*choice = make_derived_base_choice(actual, required, base_distance,
				access_scope,
				cv_qualifiers(required) & ~cv_qualifiers(actual));
			return true;
		};
		const auto reference_object_convertible =
			[this](TypeId actual, TypeId required) -> bool
		{
			return qualification_convertible(actual, required);
		};
		if (target_kind == TypeKind::LvalueReference && source_lvalue)
		{
			if (source_bit_field != NULL)
			{
				// [class.bit] forbids a non-const reference to a bit-field.  A
				// const reference is a value temporary, even when the declared
				// type is otherwise qualification-compatible with the target.
				if ((cv_qualifiers(target_referred) & 1u) == 0)
					return ConversionChoice();
				const ConversionChoice temporary = conversion_for(source, category,
					target_referred, source_node, source_integer_zero, access_scope,
					source_operation_type, source_binding);
				if (!temporary.valid)
					return ConversionChoice();
				return ConversionChoice(true, 2,
					ConversionKind::ReferenceBinding);
			}
			ConversionChoice base_choice;
			if (derived_base_choice(source_value, target_referred, &base_choice))
				return base_choice;
			if (reference_object_convertible(source_value, target_referred))
				return ConversionChoice(true,
					strip_cv_type(source_value) == strip_cv_type(target_referred) ?
						0 : 1,
					ConversionKind::ReferenceBinding);
		}
		if (target_kind == TypeKind::RvalueReference && !source_lvalue)
		{
			ConversionChoice base_choice;
			if (derived_base_choice(source_value, target_referred, &base_choice))
				return base_choice;
			if (reference_object_convertible(source_value, target_referred))
				return ConversionChoice(true,
					strip_cv_type(source_value) == strip_cv_type(target_referred) ?
						0 : 1,
					ConversionKind::ReferenceBinding);
		}
		if (target_kind == TypeKind::LvalueReference && !source_lvalue &&
			cv_qualifiers(target_referred) != 0 &&
			reference_object_convertible(source_value, target_referred))
			return ConversionChoice(true, 2, ConversionKind::ReferenceBinding);
		if (cv_qualifiers(target_referred) != 0)
		{
			const ConversionChoice temporary = conversion_for(source, category,
				target_referred, source_node, source_integer_zero, access_scope,
				source_operation_type, source_binding);
			const bool same_lvalue_value = source_lvalue &&
				temporary.kind == ConversionKind::LvalueToRvalue &&
				temporary.rank == 0;
			if (temporary.valid &&
				(target_kind != TypeKind::RvalueReference || !same_lvalue_value))
				return ConversionChoice(true,
					temporary.rank + (target_kind == TypeKind::LvalueReference ? 1 : 0),
					ConversionKind::ReferenceBinding);
		}
		return ConversionChoice();
	}
	const TypeId by_value_target = strip_cv_type(expression_object_type(target));
	const TypeId declared_source = strip_cv_type(expression_object_type(source));
	TypeId by_value_source = declared_source;
	if (source_bit_field != NULL && source_operation_type.valid())
	{
		const TypeId operation_source = strip_cv_type(
			expression_object_type(source_operation_type));
		// A bit-field retains its declared identity for an exact typed target
		// (notably an enum overload); promotion is a value conversion only when
		// the target requires a different operation type.
		if (by_value_target != declared_source)
			by_value_source = operation_source;
	}
	const bool bit_field_promotion = source_bit_field != NULL &&
		source_operation_type.valid() && by_value_source != declared_source;
	const bool null_integer = source_integer_zero || (source_node != NULL && integer_zero(*source_node));
	if (by_value_source == by_value_target)
	{
		if (bit_field_promotion)
			return ConversionChoice(true, 1, ConversionKind::Integral);
		return ConversionChoice(true, 0,
			category == SemanticValueCategory::Lvalue ?
			ConversionKind::LvalueToRvalue : ConversionKind::Identity);
	}
	// A target enum accepts only its own named enum identity.
	const NamedRecordId target_record = named_record_for_type(by_value_target);
	if (target_record.valid() && target_record.value < named_.size() &&
		named_[target_record.value].kind == NamedKind::Enum)
	{
		const NamedRecordId source_record =
			named_record_for_type(by_value_source);
		if (source_record != target_record)
			return ConversionChoice();
	}
	// Rank unscoped-enum promotion from its typed PA11 representation.
	if (integral_id(by_value_source) && enumeration_id(by_value_source))
	{
		const TypeId promoted = promote_integral_type(by_value_source);
		FundamentalType target_fundamental;
		if (fundamental_of(by_value_target, &target_fundamental) &&
			integral_type(target_fundamental))
		{
			const unsigned int source_rank = integral_rank(promoted);
			const unsigned int target_rank = integral_rank(by_value_target);
			return ConversionChoice(true,
				by_value_target == promoted ? 1 :
				2 + (target_rank > source_rank ? target_rank - source_rank : 0),
				ConversionKind::Integral);
		}
	}
	if (type_kind(by_value_source) == TypeKind::Array &&
		type_kind(by_value_target) == TypeKind::Pointer)
	{
		const TypeId element = types_[by_value_source.value].child;
		const TypeId target_element = types_[by_value_target.value].child;
		if (qualification_convertible(element, target_element))
			return ConversionChoice(true, 1, ConversionKind::ArrayToPointer);
	}
	if (type_kind(by_value_source) == TypeKind::Function &&
		type_kind(by_value_target) == TypeKind::Pointer &&
		qualification_convertible(by_value_source,
			types_[by_value_target.value].child))
		return ConversionChoice(true, 1, ConversionKind::FunctionToPointer);
	FundamentalType target_fundamental;
	if (null_integer &&
		fundamental_of(by_value_target, &target_fundamental) &&
		target_fundamental == FundamentalType::NullptrT)
		return ConversionChoice(true, 2, ConversionKind::NullIntegerToNullptr);
	FundamentalType source_fundamental;
	if (fundamental_of(by_value_source, &source_fundamental) &&
		fundamental_of(by_value_target, &target_fundamental) &&
		integral_type(source_fundamental) &&
		integral_type(target_fundamental))
	{
		const unsigned int source_rank = integral_rank(promote_integral_type(by_value_source));
		const unsigned int target_rank = integral_rank(by_value_target);
		unsigned int rank = 1 + (target_rank > source_rank ?
			target_rank - source_rank : 0);
		// Once a bit-field has crossed its PA11 promotion boundary, a target
		// other than that promoted type is at least an ordinary conversion.  In
		// particular, promotion to int must beat conversion to same-rank
		// unsigned int for enum/bool/narrow unsigned fields.
		if (bit_field_promotion && by_value_target != by_value_source && rank < 2)
			rank = 2;
		return ConversionChoice(true, rank,
			ConversionKind::Integral);
	}
	if (integral_id(by_value_source) && integral_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::Integral);
	if ((floating_id(by_value_source) && floating_id(by_value_target)) ||
		(integral_id(by_value_source) && floating_id(by_value_target)) ||
		(floating_id(by_value_source) && integral_id(by_value_target)))
	{
		const unsigned int source_rank = floating_rank(by_value_source);
		const unsigned int target_rank = floating_rank(by_value_target);
		unsigned int rank = 1 + (target_rank > source_rank ?
			target_rank - source_rank : 0);
		if (bit_field_promotion && by_value_target != by_value_source && rank < 2)
			rank = 2;
		return ConversionChoice(true, rank,
			floating_id(by_value_target) ? ConversionKind::Floating :
			ConversionKind::Integral);
	}
	if (type_kind(by_value_source) == TypeKind::Fundamental &&
		types_[by_value_source.value].fundamental == FundamentalType::NullptrT &&
		pointer_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::NullptrToPointer);
	if (type_kind(by_value_source) == TypeKind::Fundamental &&
		types_[by_value_source.value].fundamental == FundamentalType::NullptrT &&
		bool_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::NullptrToBool);
	if (null_integer &&
		pointer_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::NullIntegerToPointer);
	// Top-level cv belongs to the pointer object and is discarded by
	// lvalue-to-rvalue conversion; pointee qualification remains typed.
	if (pointer_id(by_value_source) && pointer_id(by_value_target) &&
		types_[by_value_source.value].child ==
			types_[by_value_target.value].child &&
			types_[by_value_source.value].cv !=
			types_[by_value_target.value].cv)
		return ConversionChoice(true, 0,
			category == SemanticValueCategory::Lvalue ?
			ConversionKind::LvalueToRvalue : ConversionKind::Identity);
	if (pointer_id(by_value_source) && pointer_id(by_value_target) &&
		access_scope.valid() &&
		(cv_qualifiers(types_[strip_cv_type(by_value_source).value].child) &
			~cv_qualifiers(types_[strip_cv_type(by_value_target).value].child)) == 0)
	{
		const TypeId source_element =
			types_[strip_cv_type(by_value_source).value].child;
		const TypeId target_element =
			types_[strip_cv_type(by_value_target).value].child;
		unsigned int base_distance = 0;
		if (derived_base_relation(source_element, target_element,
			&base_distance, NULL, access_scope) && base_distance != 0)
			return make_derived_base_choice(source_element, target_element,
				base_distance, access_scope,
				cv_qualifiers(target_element) & ~cv_qualifiers(source_element));
	}
	if (pointer_id(by_value_source) && pointer_id(by_value_target) &&
		pointer_convertible(by_value_source, by_value_target))
	{
		FundamentalType target_pointee;
		const TypeId target_element = types_[strip_cv_type(by_value_target).value].child;
		const bool to_void = fundamental_of(target_element, &target_pointee) &&
			target_pointee == FundamentalType::Void;
		return ConversionChoice(true, to_void ? 2 : 1,
			to_void ? ConversionKind::PointerToVoid :
			ConversionKind::PointerQualification);
	}
	if (bool_id(by_value_target) && pointer_id(by_value_source))
		return ConversionChoice(true, 3, ConversionKind::PointerToBool);
	return ConversionChoice();
}

bool PA11SemanticModel::has_template_id(const PA10AstNode& node) const
{
	for (std::size_t i = 0; i < node.name_parts.size(); ++i)
		if (node.name_parts[i].has_template_id)
			return true;
	return false;
}
NamePath PA11SemanticModel::template_name_path(const PA10AstNode& node)
{
	if (node.name_prefix_count != 0)
		throw std::runtime_error("template-id decltype qualifier unsupported");
	NamePath result;
	result.global = node.global_name;
	for (std::size_t i = 0; i < node.name_parts.size(); ++i)
		result.components.push_back(name_from_spelling(node.name_parts[i].spelling));
	if (result.components.empty() && node.producer_spelling != 0)
		result.components.push_back(name_from_spelling(node.producer_spelling));
	if (result.components.empty())
		throw std::runtime_error("template-id has no semantic component");
	return result;
}
const TemplateFunctionList* PA11SemanticModel::template_functions(
	const NamePath& path, ScopeId scope) const
{
	(void)scope;
	if (path.components.size() != 1)
		return NULL;
	return template_function_index_.find(path.last());
}
bool PA11SemanticModel::template_argument_types(const PA10AstNode& node,
	ScopeId scope, std::vector<TypeId>* arguments)
{
	if (arguments == NULL || node.name_parts.empty())
		return false;
	const PA10NameComponent* component = NULL;
	for (std::size_t i = 0; i < node.name_parts.size(); ++i)
	{
		if (!node.name_parts[i].has_template_id)
			continue;
		if (component != NULL || i + 1 != node.name_parts.size())
			return false;
		component = &node.name_parts[i];
	}
	if (component == NULL || component->template_argument_begin >
		ast_.template_arguments.size() || component->template_argument_count >
		ast_.template_arguments.size() - component->template_argument_begin)
		return false;
	arguments->clear();
	for (std::size_t i = 0; i < component->template_argument_count; ++i)
	{
		const PA10TemplateArgument& argument = ast_.template_arguments[
			component->template_argument_begin + i];
		TypeId type;
		if (argument.kind == PA10TemplateArgumentKind::TypeId)
			type = type_from_type_id(argument.syntax, scope);
		else if (argument.syntax.kind == PA10NodeKind::IdExpression &&
			!has_template_id(argument.syntax))
			type = lookup_type_path(name_path(argument.syntax), scope);
		else
			return false;
		if (!type.valid())
			return false;
		arguments->push_back(type);
	}
	return true;
}
TypeId PA11SemanticModel::substitute_template_type(TypeId type,
	const TemplateFunctionFact& function, const std::vector<TypeId>& arguments)
{
	if (!type.valid() || type.value >= types_.size())
		return TypeId();
	const TypeKey& key = types_[type.value];
	if (key.kind == TypeKind::Named)
	{
		for (std::size_t i = 0; i < function.parameters.size(); ++i)
			if (key.named == function.parameters[i])
				return i < arguments.size() ? arguments[i] : TypeId();
		return type;
	}
	if (key.kind == TypeKind::Function)
	{
		std::vector<TypeId> parameters;
		parameters.reserve(key.parameters.size());
		for (std::size_t i = 0; i < key.parameters.size(); ++i)
			parameters.push_back(substitute_template_type(key.parameters[i],
				function, arguments));
		return make_function(parameters, key.variadic,
			substitute_template_type(key.result, function, arguments), key.cv);
	}
	if (key.kind == TypeKind::Fundamental)
		return type;
	return TypeId();
}
bool PA11SemanticModel::deduce_template_type(TypeId pattern, TypeId actual,
	const TemplateFunctionFact& function, std::vector<TypeId>* arguments) const
{
	if (arguments == NULL || !pattern.valid() || !actual.valid() ||
		pattern.value >= types_.size() || actual.value >= types_.size())
		return false;
	const TypeKey& pattern_key = types_[pattern.value];
	if (pattern_key.kind == TypeKind::Named)
	{
		for (std::size_t i = 0; i < function.parameters.size(); ++i)
		{
			if (pattern_key.named != function.parameters[i])
				continue;
			if (i >= arguments->size())
				return false;
			if ((*arguments)[i].valid())
				return (*arguments)[i] == actual;
			(*arguments)[i] = actual;
			return true;
		}
	}
	return pattern == actual;
}
TemplateSpecializationId PA11SemanticModel::specialize_template_function(
	TemplateFunctionId function_id, const std::vector<TypeId>& arguments)
{
	if (!function_id.valid() || function_id.value >= template_function_facts_.size())
		return TemplateSpecializationId();
	const TemplateFunctionFact& function = template_function_facts_[
		function_id.value];
	const TemplateSpecializationKey key(function_id, arguments);
	TemplateSpecializationId specialization;
	TemplateSpecializationId* indexed = template_specialization_index_.find(key);
	if (indexed != NULL)
	{
		specialization = *indexed;
		if (!specialization.valid() || specialization.value >=
			template_specialization_facts_.size())
			return TemplateSpecializationId();
		TemplateSpecializationFact& existing =
			template_specialization_facts_[specialization.value];
		if (existing.state == TemplateSpecializationState::Complete)
			return specialization;
		if (existing.state == TemplateSpecializationState::InProgress ||
			existing.state == TemplateSpecializationState::Failed)
			return TemplateSpecializationId();
	}
	else
	{
		specialization = TemplateSpecializationId(
			template_specialization_facts_.size());
		TemplateSpecializationFact fact(function_id, BindingId());
		fact.arguments = arguments;
		template_specialization_facts_.push_back(fact);
		template_specialization_index_.set(key, specialization);
	}
	TemplateSpecializationFact& fact =
		template_specialization_facts_[specialization.value];
	if (fact.state != TemplateSpecializationState::NotStarted)
		return TemplateSpecializationId();
	fact.state = TemplateSpecializationState::InProgress;
	if (arguments.size() != function.parameters.size())
	{
		fact.state = TemplateSpecializationState::Failed;
		return TemplateSpecializationId();
	}
	const Binding& source = binding(function.binding);
	const TypeId specialized_type = substitute_template_type(source.type,
		function, arguments);
	if (!specialized_type.valid())
	{
		fact.state = TemplateSpecializationState::Failed;
		return TemplateSpecializationId();
	}
	if (!function.binding.valid() || function.binding.value >= binding_owners_.size())
	{
		fact.state = TemplateSpecializationState::Failed;
		return TemplateSpecializationId();
	}
	const BindingId binding_id(bindings_.size());
	bindings_.push_back(Binding(BindingKind::Function, source.name,
		specialized_type));
	binding_owners_.push_back(binding_owners_[function.binding.value]);
	fact.binding = binding_id;
	BindingSidecar sidecar;
	sidecar.template_specialization = specialization;
	set_binding_sidecar(binding_id, sidecar);
	fact.state = TemplateSpecializationState::Complete;
	return specialization;
}
FunctionIdResolution PA11SemanticModel::resolve_template_function_id_target(
	const PA10AstNode& node, ScopeId scope, TypeId target)
{
	if (!has_template_id(node))
		return FunctionIdResolution();
	const NamePath path = template_name_path(node);
	const TemplateFunctionList* list = template_functions(path, scope);
	if (list == NULL)
		return FunctionIdResolution();
	std::vector<TypeId> arguments;
	if (!template_argument_types(node, scope, &arguments))
		return FunctionIdResolution();
	bool have_selected = false;
	bool ambiguous = false;
	TemplateFunctionId selected_function;
	std::vector<TypeId> selected_arguments;
	ConversionChoice selected_conversion;
	for (std::size_t i = 0; i < list->entries.size(); ++i)
	{
		const TemplateFunctionId id = list->entries[i];
		if (!id.valid() || id.value >= template_function_facts_.size())
			continue;
		const TemplateFunctionFact& function = template_function_facts_[id.value];
		ScopeId cursor = scope;
		bool visible = false;
		while (cursor.valid())
		{
			if (cursor == function.visible_scope)
			{
				visible = true;
				break;
			}
			if (cursor.value >= scopes_.size())
				break;
			cursor = scopes_[cursor.value].parent;
		}
		if (!visible || arguments.size() != function.parameters.size())
			continue;
		const TypeId specialized_type = substitute_template_type(
			binding(function.binding).type, function, arguments);
		if (!specialized_type.valid())
			continue;
		const ConversionChoice conversion = conversion_for(specialized_type,
			SemanticValueCategory::Lvalue, target, &node, false, scope);
		if (!conversion.valid)
			continue;
		const int comparison = have_selected ? compare_conversion_choices(
			conversion, selected_conversion) : -1;
		if (!have_selected || comparison < 0)
		{
			have_selected = true;
			ambiguous = false;
			selected_function = id;
			selected_arguments = arguments;
			selected_conversion = conversion;
		}
		else if (comparison == 0)
			ambiguous = true;
	}
	if (!have_selected || ambiguous)
		return FunctionIdResolution();
	const TemplateSpecializationId specialization = specialize_template_function(
		selected_function, selected_arguments);
	if (!specialization.valid() || specialization.value >=
		template_specialization_facts_.size() ||
		template_specialization_facts_[specialization.value].state !=
			TemplateSpecializationState::Complete)
		return FunctionIdResolution();
	const TemplateSpecializationFact& fact =
		template_specialization_facts_[specialization.value];
	return FunctionIdResolution(true,
		ValueRef(template_function_facts_[selected_function.value].visible_scope,
			fact.binding), selected_conversion);
}
ExprInfo PA11SemanticModel::semantic_template_call(
	const PA10AstNode& node, ScopeId scope,
	const TemplateFunctionList& candidates,
	const PA10AstNode& argument_node)
{
	if (argument_node.kind != PA10NodeKind::ArgumentList &&
		argument_node.kind != PA10NodeKind::ParenArgumentList)
		throw std::runtime_error("PA12 invalid template argument list");
	std::vector<ExprInfo> arguments;
	for (std::size_t i = 0; i < argument_node.children.size(); ++i)
		arguments.push_back(semantic_expression(argument_node.children[i], scope));
	struct Candidate
	{
		TemplateFunctionId function;
		std::vector<TypeId> arguments;
		TypeId type;
		std::vector<ConversionScore> ranks;
		Candidate() : function(), arguments(), type(), ranks() {}
	};
	std::vector<Candidate> viable;
	for (std::size_t i = 0; i < candidates.entries.size(); ++i)
	{
		const TemplateFunctionId id = candidates.entries[i];
		if (!id.valid() || id.value >= template_function_facts_.size())
			continue;
		const TemplateFunctionFact& function = template_function_facts_[id.value];
		ScopeId cursor = scope;
		bool visible = false;
		while (cursor.valid())
		{
			if (cursor == function.visible_scope)
			{
				visible = true;
				break;
			}
			if (cursor.value >= scopes_.size())
				break;
			cursor = scopes_[cursor.value].parent;
		}
		if (!visible || function.binding.value >= bindings_.size())
			continue;
		const TypeId source_type = binding(function.binding).type;
		if (type_kind(source_type) != TypeKind::Function)
			continue;
		const TypeKey& source_function = types_[source_type.value];
		if (arguments.size() != source_function.parameters.size())
			continue;
		std::vector<TypeId> template_arguments(function.parameters.size());
		bool deduced = true;
		for (std::size_t arg = 0; arg < source_function.parameters.size(); ++arg)
		{
			if (!deduce_template_type(source_function.parameters[arg],
				expression_object_type(arguments[arg].type), function,
				&template_arguments))
			{
				deduced = false;
				break;
			}
		}
		for (std::size_t arg = 0; deduced && arg < template_arguments.size(); ++arg)
			if (!template_arguments[arg].valid())
				deduced = false;
		if (!deduced)
			continue;
		const TypeId specialized_type = substitute_template_type(source_type,
			function, template_arguments);
		if (!specialized_type.valid() || type_kind(specialized_type) !=
			TypeKind::Function)
			continue;
		const TypeKey& specialized_function = types_[specialized_type.value];
		Candidate candidate;
		candidate.function = id;
		candidate.arguments = template_arguments;
		candidate.type = specialized_type;
		candidate.ranks.reserve(arguments.size());
		for (std::size_t arg = 0; arg < arguments.size(); ++arg)
		{
			const ConversionChoice conversion = conversion_for(arguments[arg],
				specialized_function.parameters[arg],
				semantic_facts_[arguments[arg].fact.value].source, scope);
			if (!conversion.valid)
			{
				candidate.ranks.clear();
				break;
			}
			candidate.ranks.push_back(ConversionScore(conversion));
		}
		if (candidate.ranks.size() == arguments.size())
			viable.push_back(candidate);
	}
	if (viable.empty())
		throw std::runtime_error("PA12 no viable function template");
	const auto better = [](const Candidate& left, const Candidate& right) -> bool
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
		return strict;
	};
	std::size_t best = 0;
	for (std::size_t i = 1; i < viable.size(); ++i)
		if (better(viable[i], viable[best]))
			best = i;
	for (std::size_t i = 0; i < viable.size(); ++i)
		if (i != best && !better(viable[best], viable[i]))
			throw std::runtime_error("PA12 ambiguous function template call");
	const Candidate& selected = viable[best];
	const TemplateSpecializationId specialization = specialize_template_function(
		selected.function, selected.arguments);
	if (!specialization.valid() || specialization.value >=
		template_specialization_facts_.size() ||
		template_specialization_facts_[specialization.value].state !=
			TemplateSpecializationState::Complete)
		throw std::runtime_error("PA12 template specialization failed");
	const TemplateSpecializationFact& specialization_fact =
		template_specialization_facts_[specialization.value];
	const TypeKey& selected_function = types_[selected.type.value];
	for (std::size_t i = 0; i < selected_function.parameters.size(); ++i)
		arguments[i] = apply_context_conversion(arguments[i],
			selected_function.parameters[i],
			semantic_facts_[arguments[i].fact.value].source, scope);
	std::vector<SemanticFactId> children;
	for (std::size_t i = 0; i < arguments.size(); ++i)
		children.push_back(arguments[i].fact);
	SemanticFact call(SemanticFactKind::CallExpression,
		function_result_type(selected.type), SemanticValueCategory::Prvalue,
		&node);
	call.has_callee = true;
	call.bool_context_operand = bool_id(function_result_type(selected.type));
	call.direct_bool_boundary = bool_id(function_result_type(selected.type));
	call.selected_binding = specialization_fact.binding;
	call.selected_scope = template_function_facts_[selected.function.value].visible_scope;
	const SemanticFactId call_id = make_semantic_fact(call);
	set_semantic_children(call_id, children);
	return ExprInfo(call_id, function_result_type(selected.type),
		SemanticValueCategory::Prvalue, false);
}

TypeId PA11SemanticModel::member_function_expression_type(TypeId type,
	ScopeId member_scope, BindingId binding_id)
{
	if (!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class ||
		type_kind(type) != TypeKind::Function || is_static_member(binding_id))
		return type;
	const TypeId object_pointer = member_object_pointer_type(type, member_scope);
	if (!object_pointer.valid())
		return type;
	const TypeKey& function = types_[type.value];
	std::vector<TypeId> parameters;
	parameters.push_back(object_pointer);
	parameters.insert(parameters.end(), function.parameters.begin(),
		function.parameters.end());
	return make_function(parameters, function.variadic, function.result);
}
const PA10AstNode* PA11SemanticModel::target_function_id(
	const PA10AstNode& node, ScopeId scope)
{
	if (node.kind == PA10NodeKind::ParenthesizedExpression)
	{
		if (node.children.size() != 1)
			return NULL;
		return target_function_id(node.children.front(), scope);
	}
	if (node.kind != PA10NodeKind::IdExpression &&
		!(node.kind == PA10NodeKind::DeclSpecifier &&
			node.identifier_declspecifier))
		return NULL;
	if (has_template_id(node))
		return &node;
	const std::vector<ValueRef> values = lookup_value_path(name_path(node), scope);
	if (values.size() <= 1)
		return NULL;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		const Binding& value = binding(values[i].binding);
		if (value.kind != BindingKind::Function ||
			type_kind(value.type) != TypeKind::Function)
			return NULL;
	}
	return &node;
}
FunctionIdResolution PA11SemanticModel::resolve_function_id_target(
	const PA10AstNode& node, ScopeId scope, TypeId target)
{
	if (node.kind != PA10NodeKind::IdExpression &&
		!(node.kind == PA10NodeKind::DeclSpecifier &&
			node.identifier_declspecifier))
		return FunctionIdResolution();
	if (has_template_id(node))
		return resolve_template_function_id_target(node, scope, target);
	const std::vector<ValueRef> values = lookup_value_path(name_path(node), scope);
	ValueRef selected;
	ConversionChoice selected_conversion;
	bool have_selected = false;
	bool ambiguous = false;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		const Binding& value = binding(values[i].binding);
		if (value.kind != BindingKind::Function ||
			type_kind(value.type) != TypeKind::Function)
			continue;
		ConversionChoice conversion;
		const TypeId target_type = strip_cv_type(target);
		if (type_kind(target_type) == TypeKind::MemberPointer)
		{
			const TypeKey& member_pointer = types_[target_type.value];
			const bool owner_match = values[i].scope.valid() &&
				values[i].scope.value < scopes_.size() &&
				scopes_[values[i].scope.value].kind == ScopeKind::Class &&
				!is_static_member(values[i].binding) &&
				scopes_[values[i].scope.value].record ==
				member_pointer.named;
			conversion = owner_match && member_pointer.child == value.type ?
				ConversionChoice(true, 0, ConversionKind::Identity) :
				ConversionChoice();
		}
		else
			conversion = conversion_for(value.type,
				SemanticValueCategory::Lvalue, target, &node, false, scope);
		if (!conversion.valid)
			continue;
		const int comparison = have_selected ? compare_conversion_choices(
			conversion, selected_conversion) : -1;
		if (!have_selected || comparison < 0)
		{
			have_selected = true;
			selected = values[i];
			selected_conversion = conversion;
			ambiguous = false;
		}
		else if (comparison == 0)
			ambiguous = true;
	}
	return have_selected && !ambiguous ? FunctionIdResolution(true, selected,
		selected_conversion) : FunctionIdResolution();
}
bool PA11SemanticModel::functional_cast_target(const PA10AstNode& node,
	ScopeId scope, TypeId* target)
{
	if (builtin_cast_target(node, target))
		return true;
	if (node.kind == PA10NodeKind::TypeId)
	{
		*target = type_from_type_id(node, scope);
		return target->valid();
	}
	const PA10AstNode* target_node = &node;
	while (target_node->kind == PA10NodeKind::ParenthesizedExpression &&
		target_node->children.size() == 1)
		target_node = &target_node->children.front();
	if (target_node->kind != PA10NodeKind::IdExpression ||
		target_node->has_token)
		return false;
	const NamePath path = name_path(*target_node, scope);
	if (!path.decltype_root.valid() && !lookup_value_path(path, scope).empty())
		return false;
	*target = lookup_type_path(path, scope);
	return target->valid();
}
bool PA11SemanticModel::functional_cast_target_supported(TypeId target) const
{
	const TypeId unqualified = strip_cv_type(target);
	const TypeKind direct_kind = unqualified.valid() ? type_kind(unqualified) :
		TypeKind::Fundamental;
	if (direct_kind == TypeKind::LvalueReference ||
		direct_kind == TypeKind::RvalueReference)
		return true;
	const TypeId object = strip_cv_type(expression_object_type(target));
	const TypeKind kind = object.valid() ? type_kind(object) : TypeKind::Fundamental;
	if (kind == TypeKind::Named)
	{
		const NamedRecordId record = named_record_for_type(object);
		if (record.valid() && record.value < named_.size() &&
			named_[record.value].kind == NamedKind::Class)
			return true;
	}
	return void_id(target) || scalar_id(target) || enumeration_id(target) ||
		kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference;
}
bool PA11SemanticModel::cv_cast_compatible(TypeId source, TypeId target) const
{
	return cv_cast_compatible_impl(source, target);
}
bool PA11SemanticModel::cv_cast_compatible_impl(TypeId source,
	TypeId target) const
{
	while (source.valid() && type_kind(source) == TypeKind::Cv)
		source = types_[source.value].child;
	while (target.valid() && type_kind(target) == TypeKind::Cv)
		target = types_[target.value].child;
	if (!source.valid() || !target.valid() || type_kind(source) != type_kind(target))
		return false;
	const TypeKind kind = type_kind(source);
	if (kind == TypeKind::LvalueReference ||
		kind == TypeKind::RvalueReference)
		// Reference identity is part of a function signature.  This helper
		// strips cv wrappers only; unlike expression_object_type it never
		// erases a nested reference while walking that signature.
		return cv_cast_compatible_impl(types_[source.value].child,
			types_[target.value].child);
	if (kind == TypeKind::Pointer)
		return cv_cast_compatible_impl(types_[source.value].child,
			types_[target.value].child);
	if (kind == TypeKind::MemberPointer)
		return types_[source.value].named == types_[target.value].named &&
			cv_cast_compatible_impl(types_[source.value].child,
				types_[target.value].child);
	if (kind == TypeKind::Array)
		return types_[source.value].unknown_bound ==
			types_[target.value].unknown_bound &&
			(types_[source.value].unknown_bound ||
				types_[source.value].bound == types_[target.value].bound) &&
			cv_cast_compatible_impl(types_[source.value].child,
				types_[target.value].child);
	if (kind == TypeKind::Function)
	{
		const TypeKey& left = types_[source.value];
		const TypeKey& right = types_[target.value];
		if (left.variadic != right.variadic ||
			left.parameters.size() != right.parameters.size() ||
			!cv_cast_compatible_impl(left.result, right.result))
			return false;
		for (std::size_t i = 0; i < left.parameters.size(); ++i)
			if (!cv_cast_compatible_impl(left.parameters[i], right.parameters[i]))
				return false;
		return true;
	}
	return source == target;
}
bool PA11SemanticModel::reinterpret_reference_compatible(TypeId source,
	TypeId target) const
{
	if (cv_cast_compatible(source, target))
		return true;
	const TypeId source_object = strip_cv_type(source);
	const TypeId target_object = strip_cv_type(target);
	const bool source_supported = integral_id(source_object) ||
		floating_id(source_object) || enumeration_id(source_object) ||
		pointer_id(source_object);
	const bool target_supported = integral_id(target_object) ||
		floating_id(target_object) || enumeration_id(target_object) ||
		pointer_id(target_object);
	// The PA15 reinterpret-reference subset is defined by the typed scalar
	// owner, not by an incidental equal-size fixture.  Reference formation is
	// accepted for the supported scalar object kinds; unsupported class,
	// array, function, and member-pointer objects remain rejected here.
	return source_supported && target_supported;
}
ExplicitCastKind PA11SemanticModel::explicit_cast_kind(
	const PA10AstNode& node) const
{
	if (node.kind == PA10NodeKind::CallExpression)
		return ExplicitCastKind::Functional;
	switch (node.token)
	{
	case SimpleTokenType::OP_LPAREN: return ExplicitCastKind::CStyle;
	case SimpleTokenType::KW_STATIC_CAST: return ExplicitCastKind::Static;
	case SimpleTokenType::KW_CONST_CAST: return ExplicitCastKind::Const;
	case SimpleTokenType::KW_REINTERPET_CAST:
		return ExplicitCastKind::Reinterpret;
	default: return ExplicitCastKind::None;
	}
}
ExprInfo PA11SemanticModel::semantic_id_expression_selected(
	const PA10AstNode& node, ScopeId scope,
	const FunctionIdResolution& resolution)
{
	(void)scope;
	if (!resolution.valid)
		throw std::runtime_error("PA12 target does not select a function");
	const Binding& value = binding(resolution.selected.binding);
	if (value.kind != BindingKind::Function ||
		type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA12 target selected a non-function");
	const TypeId expression_type = member_function_expression_type(
		value.type, resolution.selected.scope, resolution.selected.binding);
	SemanticFact fact(SemanticFactKind::IdExpression, expression_type,
		SemanticValueCategory::Lvalue, &node);
	fact.binding = resolution.selected.binding;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, has_template_id(node) ? template_name_path(node) :
		name_path(node));
	return ExprInfo(result, expression_type, SemanticValueCategory::Lvalue, false);
}
ExprInfo PA11SemanticModel::semantic_expression_for_target(
	const PA10AstNode& node, ScopeId scope, TypeId target)
{
	if (node.kind == PA10NodeKind::BracedInitList)
		return semantic_braced_init_list(node, target, scope);
	const PA10AstNode* function_id = target_function_id(node, scope);
	if (function_id == NULL)
	{
		if (node.kind == PA10NodeKind::DeclSpecifier &&
			node.identifier_declspecifier)
			return semantic_id_expression(node, scope);
		const ExprInfo expression = semantic_expression(node, scope);
		const TypeKind target_kind = type_kind(target);
		if ((target_kind == TypeKind::LvalueReference ||
			target_kind == TypeKind::RvalueReference) && expression.fact.valid() &&
			target.value < types_.size())
		{
			const TypeId referred = types_[target.value].child;
			if (target_kind == TypeKind::RvalueReference ||
				type_kind(referred) == TypeKind::Cv)
			{
				const TypeId object = strip_cv_type(
					expression_object_type(referred));
				const NamedRecordId record = named_record_for_type(object);
				const ConversionChoice direct = conversion_for(expression, target,
					semantic_facts_[expression.fact.value].source, scope);
				if (record.valid() && record.value < named_.size() &&
					named_[record.value].kind == NamedKind::Class && !direct.valid)
				{
					std::vector<const PA10AstNode*> arguments(1, &node);
					const ConstructorSelection selection = select_constructor(record,
						scope, arguments, false,
						ConstructorInitializationContext::Copy);
					if (!selection.valid())
						throw std::runtime_error(
							"PA12 implicit constructor selection is incomplete");
					SemanticFact call(SemanticFactKind::CallExpression,
						fundamental(FundamentalType::Void),
						SemanticValueCategory::Prvalue, &node);
					call.has_callee = true;
					call.temporary_object = true;
					call.selected_binding = selection.binding;
					call.selected_scope = selection.scope;
					call.callable_type = selection.callable_type;
					const SemanticFactId call_id = make_semantic_fact(call);
					set_semantic_children(call_id, selection.arguments);
					SemanticFact temporary(SemanticFactKind::ConstructorAction,
						object, SemanticValueCategory::Prvalue, &node);
					temporary.has_callee = true;
					temporary.temporary_object = true;
					temporary.selected_binding = selection.binding;
					temporary.selected_scope = selection.scope;
					temporary.callable_type = selection.callable_type;
					const SemanticFactId result = make_semantic_fact(temporary);
					set_semantic_children(result,
						std::vector<SemanticFactId>(1, call_id));
					return ExprInfo(result, object,
						SemanticValueCategory::Prvalue, false);
				}
			}
		}
		return expression;
	}
	const FunctionIdResolution resolution = resolve_function_id_target(
		*function_id, scope, target);
	if (!resolution.valid)
		throw std::runtime_error("PA12 no function matches target type");
	return semantic_id_expression_selected(*function_id, scope, resolution);
}

ExprInfo PA11SemanticModel::semantic_default_member_initializer(
	const PA10AstNode& node, ScopeId scope, TypeId target)
{
	const TypeId object = strip_top_cv_type(target);
	const NamedRecordId record = named_record_for_type(object);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Class)
	{
		if (node.kind == PA10NodeKind::BracedInitList && !node.children.empty())
			return semantic_braced_init_list(node, target, scope);
		if (node.kind == PA10NodeKind::BracedInitList ||
			node.kind == PA10NodeKind::CallExpression)
		{
			const PA10AstNode* callee = NULL;
			if (node.kind == PA10NodeKind::CallExpression)
			{
				if (node.children.size() != 2)
					throw std::runtime_error(
						"PA12 class member initializer call is invalid");
				callee = &node.children.front();
				const PA10AstNode& arguments = node.children.back();
				if (arguments.kind != PA10NodeKind::ArgumentList &&
					arguments.kind != PA10NodeKind::ParenArgumentList)
					throw std::runtime_error(
						"PA12 class member initializer arguments are invalid");
				if (!arguments.children.empty())
					throw std::runtime_error(
						"PA12 class member initializer arguments are outside checkpoint");
			}
			if (node.kind == PA10NodeKind::BracedInitList &&
				!node.children.empty())
				throw std::runtime_error(
					"PA12 class member initializer is not an aggregate");
			if (callee != NULL)
			{
				const TypeId callee_type = lookup_type_path(name_path(*callee),
					scope);
				if (!callee_type.valid() || strip_cv_type(callee_type) != object)
					throw std::runtime_error(
						"PA12 class member initializer names another type");
			}
			BindingId constructor = default_constructor_binding(record);
			if (!constructor.valid())
				constructor = ensure_implicit_default_constructor(record);
			if (function_declaration_kind(constructor) ==
				FunctionDeclarationKind::Deleted)
				throw std::runtime_error(
					"PA12 class member initializer selects deleted constructor");
			SemanticFact fact(SemanticFactKind::ConstructorAction, target,
				SemanticValueCategory::Lvalue, &node);
			fact.has_callee = true;
			fact.value_initialize = true;
			fact.selected_binding = constructor;
			fact.selected_scope = named_[record.value].scope;
			fact.callable_type = constructor_callable_type(constructor);
			const SemanticFactId result = make_semantic_fact(fact);
			set_semantic_children(result, std::vector<SemanticFactId>());
			return ExprInfo(result, target, SemanticValueCategory::Lvalue, false);
		}
	}
	return semantic_expression_for_target(node, scope, target);
}

SemanticFactId PA11SemanticModel::semantic_return_statement(
	const PA10AstNode& node, ScopeId scope, const FunctionFact& function)
{
	const Binding& function_binding = binding(function.binding);
	const TypeKey& function_type = types_[function_binding.type.value];
	if (type_kind(function_binding.type) != TypeKind::Function)
		throw std::runtime_error("PA12 function fact has non-function type");
	const TypeId result_type = function_type.result;
	std::vector<SemanticFactId> children;
	if (!node.children.empty())
	{
		if (void_id(result_type))
		{
			const ExprInfo expression = semantic_expression(
				node.children.front(), scope);
			if (!void_id(expression.type))
				throw std::runtime_error("PA12 non-void expression returned from void function");
			children.push_back(expression.fact);
		}
		else
		{
			const bool braced = node.children.front().kind ==
				PA10NodeKind::BracedInitList;
			const ExprInfo expression = semantic_expression_for_target(
				node.children.front(), scope, result_type);
			const SemanticFact& expression_fact =
				semantic_facts_[expression.fact.value];
			const bool exact_prvalue_bool = !braced && bool_id(result_type) &&
				expression.category == SemanticValueCategory::Prvalue &&
				expression.type == result_type;
			const bool needs_canonical_bool_materialization = exact_prvalue_bool &&
				expression_fact.canonical_truth &&
				!expression_fact.direct_bool_boundary;
			if (needs_canonical_bool_materialization)
				set_fact_conversion(expression.fact, add_conversion(
					expression.type, result_type, ConversionKind::Identity, 0));
			if (!needs_canonical_bool_materialization && !exact_prvalue_bool)
				apply_context_conversion(expression, result_type,
					semantic_facts_[expression.fact.value].source, scope);
			// Keep the return expression rooted at its expression-owned fact.  The
			// conversion range is still attached above, while replacing this root
			// with a generic conversion temporary changes established reference
			// alias lowering (notably const_cast pointer aliases).
			children.push_back(expression.fact);
		}
	}
	else if (!void_id(result_type))
		throw std::runtime_error("PA12 missing return value");
	const SemanticFactId result = make_expression_fact(
		SemanticFactKind::ReturnStatement,
		TypeId(), SemanticValueCategory::Prvalue, node, children);
	// Retained function bodies pass their typed owner into statement analysis.
	// Keep only this compact pair; the finalizer resolves declaration owners to
	// the canonical definition after all PA12 construction is complete.
	if (function.node != NULL)
	{
		const FunctionFactId* owner = function_fact_index_.find(function.node);
		if (owner != NULL && owner->valid() && owner->value < function_facts_.size())
			function_return_owners_.push_back(
				std::make_pair(result, *owner));
	}
	return result;
}
ExprInfo PA11SemanticModel::semantic_cast_to_target(
	const PA10AstNode& node, ScopeId scope, TypeId target,
	const ExprInfo& operand)
{
	const TypeId source = expression_object_type(operand.type);
	const TypeKind target_kind = type_kind(target);
	const ExplicitCastKind cast_kind = explicit_cast_kind(node);
	if (cast_kind == ExplicitCastKind::CStyle && pointer_id(target) &&
		type_kind(strip_cv_type(source)) == TypeKind::Array)
	{
		const TypeId array = strip_cv_type(source);
		const TypeId decayed = make_pointer(types_[array.value].child);
		const ConversionChoice decay = conversion_for(operand, decayed,
			semantic_facts_[operand.fact.value].source, scope);
		const ConversionChoice to_void = conversion_for(decayed,
			SemanticValueCategory::Prvalue, target, &node, false, scope);
		if (!decay.valid || decay.kind != ConversionKind::ArrayToPointer ||
			!to_void.valid || to_void.kind != ConversionKind::PointerToVoid)
			throw std::runtime_error("PA12 invalid array pointer cast");
		const SemanticFactId result = make_expression_fact(
			SemanticFactKind::CastExpression, target,
			SemanticValueCategory::Prvalue, node,
			std::vector<SemanticFactId>(1, operand.fact));
		set_fact_conversion(result, add_conversion(operand.type, decayed, decay));
		set_fact_conversion(result, add_conversion(decayed, target, to_void));
		return ExprInfo(result, target, SemanticValueCategory::Prvalue, false);
	}
	if (target_kind == TypeKind::LvalueReference ||
		target_kind == TypeKind::RvalueReference)
	{
		const TypeId referred = types_[target.value].child;
		const BitFieldFact* bit_field = bit_field_fact_for_expression(operand);
		if (bit_field != NULL && (cv_qualifiers(referred) & 1u) == 0)
			throw std::runtime_error(
				"PA12 non-const reference cannot bind to a bit-field");
		bool valid = qualification_convertible(source, referred);
		ConversionKind reference_kind = ConversionKind::ReferenceBinding;
		ConversionChoice reference_choice(true, 0, reference_kind);
		if (cast_kind == ExplicitCastKind::Const ||
			cast_kind == ExplicitCastKind::Functional)
			valid = cv_cast_compatible(source, referred);
		else if (cast_kind == ExplicitCastKind::Reinterpret)
			valid = reinterpret_reference_compatible(source, referred);
		else if (cast_kind == ExplicitCastKind::Static)
		{
			if (cv_cast_compatible(source, referred))
				valid = true;
			else
			{
				const ConversionChoice choice = conversion_for(operand, target,
					semantic_facts_[operand.fact.value].source, scope);
				valid = choice.valid && choice.kind == ConversionKind::DerivedToBase;
				if (valid)
					reference_choice = choice;
			}
		}
		if (!valid)
			throw std::runtime_error("PA12 invalid reference cast");
		if (target_kind == TypeKind::LvalueReference &&
			operand.category != SemanticValueCategory::Lvalue)
			throw std::runtime_error("PA12 invalid reference cast category");
		if (bit_field != NULL)
		{
			const ConversionChoice binding = conversion_for(operand, target,
				semantic_facts_[operand.fact.value].source, scope);
			if (!binding.valid)
				throw std::runtime_error(
					"PA12 bit-field reference cast is not bindable");
			return make_bit_field_reference_temporary(operand, target,
				semantic_facts_[operand.fact.value].source, ScopeId(), binding);
		}
		const SemanticValueCategory category = target_kind ==
			TypeKind::RvalueReference ? SemanticValueCategory::Xvalue :
			SemanticValueCategory::Lvalue;
		// A cv-compatible static reference cast preserves the historical PA12
		// identity fact.  This keeps reference result ownership stable for
		// earlier semantic dumps while the non-identity reference families
		// retain an explicit typed cast boundary for PA15 lowering.
		if (cast_kind == ExplicitCastKind::Static &&
			cv_cast_compatible(source, referred))
		{
			semantic_facts_[operand.fact.value].type = target;
			semantic_facts_[operand.fact.value].category = category;
			return ExprInfo(operand.fact, target, category, false);
		}
		SemanticFact fact(SemanticFactKind::CastExpression, target,
			category, &node);
		fact.token = node.token;
		const SemanticFactId result = make_semantic_fact(fact);
		set_semantic_children(result,
			std::vector<SemanticFactId>(1, operand.fact));
		if (reference_choice.kind == ConversionKind::DerivedToBase)
			set_fact_conversion(result,
				add_conversion(operand.type, target, reference_choice));
		return ExprInfo(result, target, category, false);
	}
	bool valid = false;
	ConversionKind kind = ConversionKind::Integral;
	ConversionChoice selected_choice;
	if (cast_kind == ExplicitCastKind::Const)
	{
		valid = pointer_id(source) && pointer_id(target) &&
			cv_cast_compatible(source, target);
		kind = ConversionKind::PointerQualification;
		selected_choice = ConversionChoice(true, 0, kind);
	}
	else if (cast_kind == ExplicitCastKind::Reinterpret)
	{
		const bool source_pointer = pointer_id(source);
		const bool target_pointer = pointer_id(target);
		valid = (target_pointer && (source_pointer || integral_id(source) ||
			enumeration_id(source) || nullptr_id(source))) ||
			(integral_id(target) && source_pointer);
		kind = ConversionKind::Reinterpret;
		selected_choice = ConversionChoice(true, 0, kind);
	}
	else if (void_id(target))
	{
		valid = void_id(source) || scalar_id(source) ||
			type_kind(source) == TypeKind::Function || object_type(source);
		kind = ConversionKind::ToVoid;
		selected_choice = ConversionChoice(true, 0, kind);
	}
	else if (integral_id(target))
	{
		if (bool_id(target))
		{
			const ConversionChoice choice = conversion_for(operand, target,
				semantic_facts_[operand.fact.value].source, scope);
			valid = choice.valid;
			kind = choice.kind;
			selected_choice = choice;
		}
		else
		{
			valid = integral_id(source) || enumeration_id(source) ||
				nullptr_id(source);
			kind = ConversionKind::Integral;
			selected_choice = ConversionChoice(true, 0, kind);
		}
	}
	else if (pointer_id(target) && cast_kind == ExplicitCastKind::CStyle &&
		(integral_id(source) || enumeration_id(source) || nullptr_id(source)))
	{
		// The supported C-style scalar-to-pointer form is represented at
		// PA12 as the same typed reinterpret boundary used by reinterpret_cast.
		valid = true;
		kind = ConversionKind::Reinterpret;
		selected_choice = ConversionChoice(true, 0, kind);
	}
	else if (floating_id(target) || pointer_id(target))
	{
		const ConversionChoice choice = conversion_for(operand, target,
			semantic_facts_[operand.fact.value].source, scope);
		valid = choice.valid;
		kind = choice.kind;
		selected_choice = choice;
	}
	else if (type_kind(strip_cv_type(target)) == TypeKind::MemberPointer)
	{
		valid = type_kind(strip_cv_type(source)) == TypeKind::MemberPointer &&
			strip_cv_type(source) == strip_cv_type(target);
		kind = ConversionKind::Identity;
		selected_choice = ConversionChoice(true, 0, kind);
	}
	else if (type_kind(strip_cv_type(target)) == TypeKind::Named)
	{
		valid = integral_id(source) || source == target;
		selected_choice = ConversionChoice(true, 0, kind);
	}
	if (!valid)
		throw std::runtime_error("PA12 invalid explicit cast");
	if (node.token == SimpleTokenType::KW_STATIC_CAST &&
		type_kind(strip_cv_type(target)) == TypeKind::MemberPointer &&
		kind == ConversionKind::Identity &&
		strip_cv_type(source) == strip_cv_type(target))
		return operand;
	const SemanticFactId result = make_expression_fact(
		SemanticFactKind::CastExpression, target,
		SemanticValueCategory::Prvalue, node,
		std::vector<SemanticFactId>(1, operand.fact));
	if (!selected_choice.valid)
		selected_choice = ConversionChoice(true, 0, kind);
	set_fact_conversion(result, add_conversion(source, target, selected_choice));
	return ExprInfo(result, target, SemanticValueCategory::Prvalue, false);
}
ExprInfo PA11SemanticModel::semantic_functional_cast(
	const PA10AstNode& node, ScopeId scope, TypeId target,
	const PA10AstNode& argument_node)
{
	if (!functional_cast_target_supported(target))
		throw std::runtime_error("PA12 unsupported functional cast target");
	if (argument_node.children.empty())
	{
		if (void_id(target))
			throw std::runtime_error("PA12 invalid zero-argument functional cast");
		const TypeId object = strip_cv_type(expression_object_type(target));
		const NamedRecordId record = named_record_for_type(object);
		if (record.valid() && record.value < named_.size() &&
			named_[record.value].kind == NamedKind::Class)
		{
			const std::vector<const PA10AstNode*> no_arguments;
			const ConstructorSelection selection = select_constructor(record, scope,
				no_arguments, true, ConstructorInitializationContext::Direct);
			if (!selection.valid())
				throw std::runtime_error("PA12 functional constructor selection is incomplete");
			SemanticFact call(SemanticFactKind::CallExpression,
				fundamental(FundamentalType::Void), SemanticValueCategory::Prvalue,
				&node);
			call.has_callee = true;
			call.temporary_object = true;
			call.selected_binding = selection.binding;
			call.selected_scope = selection.scope;
			call.callable_type = selection.callable_type;
			call.value_initialize = true;
			const SemanticFactId call_id = make_semantic_fact(call);
			set_semantic_children(call_id, selection.arguments);
			SemanticFact temporary(SemanticFactKind::ConstructorAction, target,
				SemanticValueCategory::Prvalue, &node);
			temporary.has_callee = true;
			temporary.temporary_object = true;
			// A zero-argument functional construction is value-initialization,
			// even when the selected implicit constructor has an empty action
			// list.  The destination owner performs the zero-initialization;
			// PA15 must not materialize a throwaway constructor temporary.
			temporary.value_initialize = true;
			temporary.selected_binding = selection.binding;
			temporary.selected_scope = selection.scope;
			temporary.callable_type = selection.callable_type;
			const SemanticFactId result = make_semantic_fact(temporary);
			set_semantic_children(result,
				std::vector<SemanticFactId>(1, call_id));
			return ExprInfo(result, target, SemanticValueCategory::Prvalue, false);
		}
		SemanticFact fact(SemanticFactKind::Literal, target,
			SemanticValueCategory::Prvalue, &node);
		fact.has_literal_value = true;
		fact.literal_value = 0;
		fact.literal_value_unsigned = false;
		fact.literal_value_negative = false;
		const SemanticFactId result = make_semantic_fact(fact);
		return ExprInfo(result, target, SemanticValueCategory::Prvalue,
			integral_id(target));
	}
	if (argument_node.children.size() != 1)
		throw std::runtime_error("PA12 invalid functional cast arity");
	const TypeId object = strip_cv_type(expression_object_type(target));
	const NamedRecordId record = named_record_for_type(object);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Class)
	{
		std::vector<const PA10AstNode*> arguments(1,
			&argument_node.children.front());
		const ConstructorSelection selection = select_constructor(record, scope,
			arguments, true, ConstructorInitializationContext::Direct);
		if (!selection.valid())
			throw std::runtime_error("PA12 functional constructor selection is incomplete");
		SemanticFact call(SemanticFactKind::CallExpression,
			fundamental(FundamentalType::Void), SemanticValueCategory::Prvalue,
			&node);
		call.has_callee = true;
		call.temporary_object = true;
		call.selected_binding = selection.binding;
		call.selected_scope = selection.scope;
		call.callable_type = selection.callable_type;
		const SemanticFactId call_id = make_semantic_fact(call);
		set_semantic_children(call_id, selection.arguments);
		SemanticFact temporary(SemanticFactKind::ConstructorAction, target,
			SemanticValueCategory::Prvalue, &node);
		temporary.has_callee = true;
		temporary.temporary_object = true;
		temporary.selected_binding = selection.binding;
		temporary.selected_scope = selection.scope;
		temporary.callable_type = selection.callable_type;
		const SemanticFactId result = make_semantic_fact(temporary);
		set_semantic_children(result,
			std::vector<SemanticFactId>(1, call_id));
		return ExprInfo(result, target, SemanticValueCategory::Prvalue, false);
	}
	const ExprInfo operand = semantic_expression(
		argument_node.children.front(), scope);
	return semantic_cast_to_target(node, scope, target, operand);
}
} // namespace pa11_semantic_internal
