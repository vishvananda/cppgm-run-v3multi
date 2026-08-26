#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

LoweredValue Pa15Lowerer::lower_call(SemanticFactId id)
{
	const SemanticFact& fact = model_.semantic_facts_[id.value];
	const std::vector<SemanticFactId> facts = children(id);
	const Binding* callee_binding = NULL;
	const TypeKey* function_type = NULL;
	std::size_t argument_begin = 0;
	Instruction instruction;
	instruction.kind = Instruction::IK_CALL;
	if (!fact.callable_type.valid() ||
		model_.type_kind(fact.callable_type) != TypeKind::Function)
		throw std::runtime_error("PA15 call has no typed callable signature");
	function_type = &model_.types_[fact.callable_type.value];
	if (fact.has_implicit_object && !fact.has_callee)
		throw std::runtime_error("PA15 member call is not direct");
	if (fact.has_callee)
	{
		if (!fact.selected_binding.valid())
			throw std::runtime_error("PA15 direct call has no selected binding");
		std::map<std::size_t, SymbolId>::const_iterator symbol =
			function_symbols_.find(fact.selected_binding.value);
		if (symbol == function_symbols_.end())
			throw std::runtime_error("PA15 direct call target was not emitted");
		demand_function_declaration(fact.selected_binding);
		callee_binding = &model_.binding(fact.selected_binding);
		if (model_.type_kind(callee_binding->type) != TypeKind::Function)
			throw std::runtime_error("PA15 direct call target is not a function");
		instruction.direct_callee_id = symbol->second;
		instruction.first = global_operand(symbol->second,
			function_name_ids_.find(fact.selected_binding.value)->second);
		if (fact.has_implicit_object)
		{
			if (facts.empty() || (fact.token != SimpleTokenType::OP_DOT &&
				fact.token != SimpleTokenType::OP_ARROW))
				throw std::runtime_error("PA15 member call object is missing");
			const FunctionFact* member_fact =
				model_.function_fact_for_binding(fact.selected_binding);
			if (member_fact == NULL || !member_fact->owner.valid() ||
				member_fact->owner.value >= model_.scopes_.size() ||
				model_.scopes_[member_fact->owner.value].kind != ScopeKind::Class ||
				fact.selected_scope != member_fact->owner)
				throw std::runtime_error("PA15 member call owner is invalid");
			const Binding& member = model_.binding(fact.selected_binding);
			if (member.kind != BindingKind::Function ||
				model_.type_kind(member.type) != TypeKind::Function ||
				model_.is_static_member(fact.selected_binding))
				throw std::runtime_error("PA15 member call target is not an ordinary method");
			const TypeKey& member_signature = model_.types_[member.type.value];
			if (function_type->cv != 0 || function_type->variadic !=
				member_signature.variadic || function_type->result !=
				member_signature.result || function_type->parameters.size() !=
				member_signature.parameters.size() + 1)
				throw std::runtime_error("PA15 member call signature mismatch");
			for (std::size_t parameter = 0;
				parameter < member_signature.parameters.size(); ++parameter)
				if (function_type->parameters[parameter + 1] !=
					member_signature.parameters[parameter])
					throw std::runtime_error("PA15 member call parameter mismatch");
			if (!member_fact->function_scope.valid() ||
				member_fact->function_scope.value >= model_.scopes_.size())
				throw std::runtime_error("PA15 member call function scope is missing");
			const Scope& function_scope = model_.scopes_[
				member_fact->function_scope.value];
			const BindingId hidden_this = function_scope.implicit_object_binding;
			if (!hidden_this.valid() || model_.binding(hidden_this).kind !=
				BindingKind::Parameter || model_.binding(hidden_this).type !=
				function_type->parameters.front())
				throw std::runtime_error("PA15 member call hidden object mismatch");
			const TypeId hidden_pointer = model_.strip_cv_type(
				model_.expression_object_type(function_type->parameters.front()));
			if (model_.type_kind(hidden_pointer) != TypeKind::Pointer)
				throw std::runtime_error("PA15 member call hidden object is not a pointer");
			const TypeId required_object = model_.types_[hidden_pointer.value].child;
			const SemanticFact& object_fact = model_.semantic_facts_[
				facts.front().value];
			TypeId actual_object;
			LoweredValue object;
			if (fact.token == SimpleTokenType::OP_ARROW)
			{
				const TypeId pointer = model_.strip_cv_type(
					model_.expression_object_type(object_fact.type));
				if (model_.type_kind(pointer) != TypeKind::Pointer)
					throw std::runtime_error("PA15 member call arrow object is not a pointer");
				actual_object = model_.types_[pointer.value].child;
				object = lower_expression(facts.front());
			}
			else
			{
				actual_object = model_.expression_object_type(object_fact.type);
				object = lower_address(facts.front());
			}
			if (model_.class_scope_for_type(model_.strip_cv_type(actual_object)) !=
				member_fact->owner || !model_.qualification_convertible(
				actual_object, required_object) || !object.type.is_pointer())
				throw std::runtime_error("PA15 member call object is incompatible");
			instruction.args.push_back(object.value);
			argument_begin = 1;
		}
	}
	else
	{
		if (facts.empty()) throw std::runtime_error("PA15 indirect call has no callee");
		argument_begin = 1;
		instruction.has_call_signature = true;
	}
	const std::size_t explicit_argument_count = facts.size() - argument_begin;
	const std::size_t argument_count = explicit_argument_count +
		(fact.has_implicit_object ? 1 : 0);
	if (!function_type || (!function_type->variadic &&
		argument_count != function_type->parameters.size()) ||
		(function_type->variadic && argument_count < function_type->parameters.size()))
		throw std::runtime_error("PA15 call arity mismatch");
	instruction.call_return_type = low_type(function_type->result);
	instruction.call_returns_void = instruction.call_return_type.is_void();
	instruction.call_boundary.arity = function_type->variadic ?
		lowir_model::CAM_VARIADIC : lowir_model::CAM_FIXED;
	for (std::size_t i = 0; i < explicit_argument_count; ++i)
	{
		const SemanticFactId argument = facts[argument_begin + i];
		instruction.args.push_back(lower_expression(argument).value);
	}
	if (!fact.has_callee)
		instruction.first = lower_expression(facts.front()).value;
	if (instruction.has_call_signature)
		for (std::size_t i = 0; i < function_type->parameters.size(); ++i)
		{
			Parameter parameter;
			std::ostringstream name;
			name << "%arg" << i;
			parameter.name_id = intern_spelling(name.str());
			parameter.type = low_type(function_type->parameters[i]);
			TypeId parameter_type = function_type->parameters[i];
			while (model_.type_kind(parameter_type) == TypeKind::Cv)
				parameter_type = model_.types_[parameter_type.value].child;
			const TypeKind kind = model_.type_kind(parameter_type);
			if (kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference)
				parameter.metadata.passing = lowir_model::PPM_REFERENCE;
			instruction.call_params.push_back(parameter);
		}
	if (instruction.call_returns_void)
	{
		block().instructions.push_back(instruction);
		return LoweredValue(Operand(), instruction.call_return_type, false);
	}
	const ValueId value = destination(instruction.call_return_type, &instruction);
	block().instructions.push_back(instruction);
	const bool reference_result = model_.type_kind(model_.strip_cv_type(
		function_type->result)) == TypeKind::LvalueReference ||
		model_.type_kind(model_.strip_cv_type(function_type->result)) ==
		TypeKind::RvalueReference;
	if (reference_result)
		return LoweredValue(temporary_operand(value, instruction.destination_name_id),
			low_reference_value_type(function_type->result), true,
			instruction.call_return_type);
	return LoweredValue(temporary_operand(value, instruction.destination_name_id),
		instruction.call_return_type, false);
}

} // namespace pa11_semantic_internal
