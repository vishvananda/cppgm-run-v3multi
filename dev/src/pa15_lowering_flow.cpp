#include "pa15_lowering.h"

namespace pa11_semantic_internal
{

LoweredValue Pa15Lowerer::lower_expression_impl(SemanticFactId id, bool omit_boolean_context,
		bool materialize_lvalue, bool force_integral_literal_conversion, bool defer_conversions){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		LoweredValue result;
		switch (fact.kind)
		{
		case SemanticFactKind::Variable:
		{
			const std::vector<SemanticFactId> initializer = children(id);
			if (initializer.size() > 1)
				throw std::runtime_error("PA15 invalid condition initializer");
			const LoweredValue storage = storage_for(fact.binding);
			if (initializer.size() == 1)
			{
				if (storage.type.is_object() &&
					model_.semantic_facts_[initializer.front().value].kind ==
					SemanticFactKind::BracedInitList)
					initialize_array(fact.binding, initializer.front(), storage);
				else
				{
					FundamentalType storage_fundamental;
					FundamentalType initializer_fundamental;
					const bool bool_storage = model_.fundamental_of(
						model_.expression_object_type(model_.binding(fact.binding).type),
						&storage_fundamental) && storage_fundamental == FundamentalType::Bool;
					const bool bool_initializer = model_.fundamental_of(
						model_.expression_object_type(
							model_.semantic_facts_[initializer.front().value].type),
						&initializer_fundamental) && initializer_fundamental == FundamentalType::Bool;
					const LoweredValue value = bool_storage && bool_initializer ?
						lower_condition_expression(initializer.front()) :
						lower_expression(initializer.front());
					emit_store(storage.type, value.value, storage.value);
				}
			}
			result = storage;
			break;
		}
		case SemanticFactKind::Literal:
			result = literal(fact);
			break;
		case SemanticFactKind::SizeofExpression:
			result = lower_sizeof(fact);
			break;
		case SemanticFactKind::IdExpression:
			if (model_.binding(fact.binding).kind == BindingKind::Function)
				result = lower_address(id);
			else
				result = lower_lvalue(id);
			break;
		case SemanticFactKind::UnaryExpression:
		{
			const std::vector<SemanticFactId> operands = children(id);
			if (operands.size() != 1) throw std::runtime_error("PA15 invalid unary fact");
			if (fact.token == SimpleTokenType::OP_AMP)
				result = lower_address(operands.front());
			else if (fact.token == SimpleTokenType::OP_STAR)
			{
				const LoweredValue operand = lower_expression(operands.front());
				if (!operand.type.is_pointer())
					throw std::runtime_error("PA15 dereference operand is not a pointer");
				result = LoweredValue(operand.value, lvalue_type(id), true,
					operand.physical_type);
			}
			else if (fact.token == SimpleTokenType::OP_INC ||
				fact.token == SimpleTokenType::OP_DEC)
				result = lower_incdec(id, false);
			else
			{
				const LoweredValue operand = fact.token == SimpleTokenType::OP_LNOT ?
					lower_condition_expression(operands.front()) :
					lower_expression(operands.front());
				if (fact.token == SimpleTokenType::OP_PLUS)
					result = operand;
				else if (fact.token == SimpleTokenType::OP_LNOT)
				{
					const LowType compare_type = operand.physical_type;
					if (!compare_type.is_integer() && !compare_type.is_pointer())
						throw std::runtime_error("PA15 logical negation operand is not scalar");
					result = emit_compare_value(lowir_model::CPP_EQ, compare_type,
						operand, LoweredValue(integer_operand(0, compare_type),
							compare_type, false));
				}
				else
				{
					Instruction instruction;
					instruction.kind = Instruction::IK_UNARY;
					instruction.type = low_type(fact.type);
					instruction.first = operand.value;
					instruction.unary_operator = fact.token == SimpleTokenType::OP_MINUS ?
						lowir_model::UOP_NEG : fact.token == SimpleTokenType::OP_LNOT ?
						lowir_model::UOP_NOT : fact.token == SimpleTokenType::OP_COMPL ?
						lowir_model::UOP_BITNOT : lowir_model::UOP_INVALID;
					if (instruction.unary_operator == lowir_model::UOP_INVALID)
						throw std::runtime_error("PA15 unsupported unary operator");
					const ValueId value = destination(instruction.type, &instruction);
					block().instructions.push_back(instruction);
					result = LoweredValue(temporary_operand(value,
						instruction.destination_name_id), instruction.type, false);
				}
			}
			break;
		}
		case SemanticFactKind::PostfixExpression:
			result = lower_incdec(id, true);
			break;
		case SemanticFactKind::SubscriptExpression:
			result = lower_lvalue(id);
			break;
		case SemanticFactKind::BinaryExpression:
		{
			const std::vector<SemanticFactId> operands = children(id);
			if (operands.size() != 2) throw std::runtime_error("PA15 invalid binary fact");
				if (fact.token == SimpleTokenType::OP_COMMA)
				{
					lower_discarded_expression(operands.front());
				if (model_.semantic_facts_[operands.back().value].category !=
					SemanticValueCategory::Prvalue)
					result = lower_lvalue(operands.back());
				else
					result = lower_expression(operands.back());
				break;
			}
			if (fact.token == SimpleTokenType::OP_LAND ||
				fact.token == SimpleTokenType::OP_LOR)
			{
				result = lower_logical(id);
				break;
			}
			const bool left_pointer = pointer_like(
				model_.semantic_facts_[operands[0].value].type);
			const bool right_pointer = pointer_like(
				model_.semantic_facts_[operands[1].value].type);
			const bool force_size_operands = fact.size_type_derived;
			LoweredValue left;
			LoweredValue right;
			if (force_size_operands)
			{
				left = lower_expression_impl(operands[0], false, true,
					force_size_operands, true); right = lower_expression_impl(operands[1],
					false, true, force_size_operands, true);
				left = apply_conversions(operands[0], left, false, true, force_size_operands);
				right = apply_conversions(operands[1], right, false, true, force_size_operands);
			}
			else
			{
				left = lower_expression_impl(operands[0], false, true,
					force_size_operands);
					right = lower_expression_impl(operands[1], false, true,
					force_size_operands);
			}
			if ((fact.token == SimpleTokenType::OP_PLUS ||
				fact.token == SimpleTokenType::OP_MINUS) && left_pointer &&
				!right_pointer && left.type.is_pointer())
			{
				result = pointer_offset(left,
					model_.semantic_facts_[operands[0].value].type, right,
					model_.semantic_facts_[operands[1].value].type,
					fact.token == SimpleTokenType::OP_MINUS);
			}
			else if (fact.token == SimpleTokenType::OP_PLUS && right_pointer &&
				!left_pointer && right.type.is_pointer())
			{
				result = pointer_offset(right,
					model_.semantic_facts_[operands[1].value].type, left,
					model_.semantic_facts_[operands[0].value].type, false);
			}
			else if (fact.token == SimpleTokenType::OP_MINUS && left_pointer &&
				right_pointer && left.type.is_pointer() && right.type.is_pointer())
			{
				const LowType pointer = left.type;
				const LoweredValue difference = emit_binary_value(
					lowir_model::BOP_SUB, pointer, left, right);
				const LowType i64 = []() {
					LowType value;
					value.kind = LowType::TYPE_INTEGER;
					value.integer_kind = LowType::INTEGER_I64;
					return value;
				}();
				const std::size_t element_size = pointer_element_size(
					model_.semantic_facts_[operands[0].value].type);
				LoweredValue quotient = difference;
				if (element_size != 1)
					quotient = emit_binary_value(lowir_model::BOP_DIV, i64,
						difference, LoweredValue(integer_operand(
							static_cast<long long>(element_size), i64), i64, false));
				result = quotient;
			}
			else if (is_comparison(fact.token))
			{
				const TypeId operation_type = fact.operation_type.valid() ?
					fact.operation_type :
					model_.expression_object_type(
						model_.semantic_facts_[operands[0].value].type);
				const NamedRecordId operation_record =
					model_.named_record_for_type(operation_type);
				const bool scoped_enum_operation = operation_record.valid() &&
					operation_record.value < model_.named_.size() &&
					model_.named_[operation_record.value].kind == NamedKind::Enum &&
					model_.named_[operation_record.value].scoped_enum;
				LowType compare_type = operation_type.valid() ?
					low_type(operation_type) : left.physical_type;
				if (!compare_type.valid())
				{
					compare_type = low_type(model_.expression_object_type(
						model_.semantic_facts_[operands[0].value].type));
				}
				if (!scoped_enum_operation && !compare_type.is_pointer() &&
					compare_type.is_integer() &&
					compare_type.integer_width() < 32)
				{
					compare_type.kind = LowType::TYPE_INTEGER;
					compare_type.integer_kind = LowType::INTEGER_I32;
				}
				result = emit_compare_value(compare_predicate(fact.token,
					operation_type.valid() && unsigned_type_for(operation_type)),
					compare_type,
					LoweredValue(left.value, compare_type, false, compare_type),
					LoweredValue(right.value, compare_type, false, compare_type));
			}
			else
			{
				const TypeId operation_type = fact.operation_type.valid() ?
					fact.operation_type : fact.type;
				LowType type = low_type(operation_type);
				if (fact.size_type_derived && type.is_integer() &&
					type.integer_width() == 64)
					type = size_low_type();
				const lowir_model::BinaryOperator operation = binary_operator(
					fact.token, unsigned_type_for(operation_type));
				if (operation == lowir_model::BOP_INVALID)
					throw std::runtime_error("PA15 unsupported binary operator");
				result = emit_binary_value(operation, type, left,
					LoweredValue(right.value, type, false));
			}
			break;
		}
		case SemanticFactKind::AssignmentExpression:
			result = lower_assignment(id);
			break;
		case SemanticFactKind::ConditionalExpression:
			if (conditional_address_result(id))
			{
				const LoweredValue address = lower_conditional_address(id);
				if (fact.category == SemanticValueCategory::Prvalue)
					result = address;
				else
					result = LoweredValue(address.value, lvalue_type(id), true,
						address.physical_type);
			}
			else
				result = lower_conditional_value(id);
			break;
		case SemanticFactKind::CallExpression:
			result = lower_call(id);
			break;
		case SemanticFactKind::CastExpression:
			if (children(id).size() != 1)
				throw std::runtime_error("PA15 unsupported cast expression");
			result = lower_expression(children(id).front());
			break;
		default:
			throw std::runtime_error("PA15 unsupported scalar expression fact");
		}
		if (defer_conversions)
		{
			if (materialize_lvalue && result.lvalue && !result.type.is_object())
			{
				const ValueId value = emit_load(result, result.type);
				const Instruction& emitted = block().instructions.back();
				result.value = temporary_operand(value,
					emitted.destination_name_id);
				result.physical_type = result.type;
				result.lvalue = false;
			}
			return result;
		}
		return apply_conversions(id, result, omit_boolean_context,
			materialize_lvalue, force_integral_literal_conversion);
	}

void Pa15Lowerer::lower_condition_branch(SemanticFactId id, BlockId true_target,
		BlockId false_target){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::Condition)
		{
			if (fact.child_count != 1)
				throw std::runtime_error("PA15 empty branching condition");
			lower_condition_branch(
				model_.semantic_children_[fact.child_begin], true_target,
				false_target);
			return;
		}
		if (fact.kind == SemanticFactKind::ConditionDeclaration)
		{
			const std::vector<SemanticFactId> values = children(id);
			if (values.size() != 1)
				throw std::runtime_error("PA15 invalid branching condition declaration");
			const LoweredValue condition = lower_condition_expression(values.front());
			emit_branch(condition.value, true_target, false_target);
			return;
		}
		if (fact.kind == SemanticFactKind::BinaryExpression &&
			(fact.token == SimpleTokenType::OP_LAND ||
			 fact.token == SimpleTokenType::OP_LOR))
		{
			const std::vector<SemanticFactId> operands = children(id);
			if (operands.size() != 2)
				throw std::runtime_error("PA15 invalid logical condition fact");
			const std::size_t rhs_index = new_block(fact.token ==
				SimpleTokenType::OP_LAND ? "land_rhs" : "lor_rhs");
			const BlockId rhs_target = block_id(rhs_index);
			if (fact.token == SimpleTokenType::OP_LAND)
			{
				lower_condition_branch(operands[0], rhs_target, false_target);
				set_current(rhs_target);
				lower_condition_branch(operands[1], true_target, false_target);
			}
			else
			{
				lower_condition_branch(operands[0], true_target, rhs_target);
				set_current(rhs_target);
				lower_condition_branch(operands[1], true_target, false_target);
			}
			return;
		}
		const LoweredValue condition = lower_condition_expression(id);
		emit_branch(condition.value, true_target, false_target);
	}

BlockId Pa15Lowerer::switch_label_existing_target(SemanticFactId id) const{
		if (switch_stack_.empty())
			throw std::runtime_error("PA15 switch label context is missing");
		const SwitchContext& context = switch_stack_.back();
		const std::map<std::size_t, BlockId>::const_iterator found =
			context.labels.find(id.value);
		if (found == context.labels.end())
			throw std::runtime_error("PA15 switch label owner mismatch");
		return found->second;
	}

void Pa15Lowerer::terminate_unreachable_block(BlockId id){
		if (terminated(id)) return;
		if (is_reachable(id))
			throw std::runtime_error("PA15 reachable block cannot be a sink");
		const BlockId saved_current = current_block_id();
		set_current(id);



		emit_jump(id);
		if (saved_current.valid()) set_current(saved_current);
		else current_block_ = InvalidIdentityValue;
	}

bool Pa15Lowerer::lower_switch_label_recovery(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const bool already_lowered = switch_label_was_lowered(id);
		const BlockId target = already_lowered ?
			switch_label_existing_target(id) : switch_label_target(id);
		if (already_lowered)
		{



			if (current_block_ != InvalidIdentityValue &&
				current_block_id() != target && !terminated(block()))
				emit_jump(target);
			current_block_ = InvalidIdentityValue;
			return true;
		}
		if (current_block_ != InvalidIdentityValue &&
			current_block_id() != target && !terminated(block()))
			emit_jump(target);
		set_current(target);
		const std::vector<SemanticFactId> facts = children(id);
		if (fact.kind == SemanticFactKind::CaseStatement)
		{
			if (facts.size() != 2)
				throw std::runtime_error("PA15 invalid case statement");
			lower_statement(facts.back());
		}
		else
		{
			if (facts.size() != 1)
				throw std::runtime_error("PA15 invalid default statement");
			lower_statement(facts.front());
		}
		return true;
	}

bool Pa15Lowerer::collect_switch_labels(SemanticFactId id, SwitchContext* context){
		const SemanticFact& fact = model_.semantic_facts_[id.value];



		if (fact.kind == SemanticFactKind::SwitchStatement) return false;
		bool found_label = false;
		if (fact.kind == SemanticFactKind::CaseStatement)
		{
			const std::vector<SemanticFactId> label_children = children(id);
			if (label_children.size() != 2)
				throw std::runtime_error("PA15 invalid case fact");
			const LoweredValue value = literal(
				model_.semantic_facts_[label_children.front().value]);
			const BlockId target = block_id(new_block("switch_case"));
			if (!context->labels.insert(std::make_pair(id.value, target)).second)
				throw std::runtime_error("PA15 duplicate switch label fact");
			context->arms.push_back(SwitchArm(id, target, false, value.value));
			found_label = true;
			if (collect_switch_labels(label_children.back(), context))
				found_label = true;
		}
		else if (fact.kind == SemanticFactKind::DefaultStatement)
		{
			const std::vector<SemanticFactId> label_children = children(id);
			if (label_children.size() != 1)
				throw std::runtime_error("PA15 invalid default fact");
			const BlockId target = block_id(new_block("switch_default"));
			if (!context->labels.insert(std::make_pair(id.value, target)).second)
				throw std::runtime_error("PA15 duplicate switch label fact");
			context->default_target = target;
			context->arms.push_back(SwitchArm(id, target, true, Operand()));
			found_label = true;
			if (collect_switch_labels(label_children.front(), context))
				found_label = true;
		}
		else
		{
			const std::vector<SemanticFactId> facts = children(id);
			for (std::size_t i = 0; i < facts.size(); ++i)
				if (collect_switch_labels(facts[i], context))
					found_label = true;
		}
		if (found_label) context->label_subtrees.insert(id.value);
		return found_label;
	}

void Pa15Lowerer::finish_switch_loop(const LoopTarget& target){
		control_stack_.pop_back();




		set_current(target.break_target);
	}

void Pa15Lowerer::recover_existing_switch_loop(SemanticFactId body_fact,
		const LoopTarget& target){
		control_stack_.push_back(ControlTarget(true, target.break_target,
			target.continue_target));
		current_block_ = InvalidIdentityValue;
		lower_switch_body(body_fact);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(target.continue_target);
		finish_switch_loop(target);
	}

void Pa15Lowerer::lower_switch_while(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid recovered while fact");
		const LoopTarget* existing = remembered_loop_target(id);
		if (existing != NULL)
		{
			recover_existing_switch_loop(facts[1], *existing);
			return;
		}
		const BlockId condition = block_id(new_block("while_cond"));
		const BlockId body = block_id(new_block("while_body"));
		const BlockId end = block_id(new_block("while_end"));
		remember_loop_target(id, end, condition);
		set_current(condition);
		if (has_direct_short_circuit(facts[0]))
			lower_condition_branch(facts[0], body, end);
		else
		{
			const LoweredValue value = lower_condition(facts[0]);
			emit_branch(value.value, body, end);
		}
		control_stack_.push_back(ControlTarget(true, end, condition));
		set_current(body);
		lower_statement(facts[1]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		current_block_ = InvalidIdentityValue;



		lower_switch_body(facts[1]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		finish_switch_loop(loop_targets_[id.value]);
	}

void Pa15Lowerer::lower_switch_do(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid recovered do fact");
		const LoopTarget* existing = remembered_loop_target(id);
		if (existing != NULL)
		{
			recover_existing_switch_loop(facts[0], *existing);
			return;
		}
		const BlockId body = block_id(new_block("do_body"));
		const BlockId condition = block_id(new_block("do_cond"));
		const BlockId end = block_id(new_block("do_end"));
		remember_loop_target(id, end, condition);
		control_stack_.push_back(ControlTarget(true, end, condition));
		set_current(body);
		lower_statement(facts[0]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		current_block_ = InvalidIdentityValue;
		lower_switch_body(facts[0]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		set_current(condition);
		if (has_direct_short_circuit(facts[1]))
			lower_condition_branch(facts[1], body, end);
		else
		{
			const LoweredValue value = lower_condition(facts[1]);
			emit_branch(value.value, body, end);
		}
		finish_switch_loop(loop_targets_[id.value]);
	}

void Pa15Lowerer::lower_switch_for(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() < 2)
			throw std::runtime_error("PA15 invalid recovered for fact");
		SemanticFactId condition_fact;
		SemanticFactId iteration_fact;
		for (std::size_t i = 1; i + 1 < facts.size(); ++i)
		{
			const SemanticFactKind kind = model_.semantic_facts_[facts[i].value].kind;
			if (kind == SemanticFactKind::Condition) condition_fact = facts[i];
			else if (kind == SemanticFactKind::Iteration) iteration_fact = facts[i];
		}
		const LoopTarget* existing = remembered_loop_target(id);
		if (existing != NULL)
		{
			recover_existing_switch_loop(facts.back(), *existing);
			return;
		}
		const BlockId initialization = block_id(new_block("for_init"));
		const BlockId condition = block_id(new_block("for_cond"));
		const BlockId body = block_id(new_block("for_body"));
		const BlockId iteration = block_id(new_block("for_iter"));
		const BlockId end = block_id(new_block("for_end"));



		remember_loop_target(id, end, iteration);
		set_current(initialization);
		lower_statement(facts[0]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		set_current(condition);
		if (!condition_fact.valid() || condition_is_empty(condition_fact))
			emit_jump(body);
		else if (has_direct_short_circuit(condition_fact))
			lower_condition_branch(condition_fact, body, end);
		else
		{
			const LoweredValue value = lower_condition(condition_fact);
			emit_branch(value.value, body, end);
		}
		control_stack_.push_back(ControlTarget(true, end, iteration));
		set_current(body);
		lower_statement(facts.back());
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(iteration);
		current_block_ = InvalidIdentityValue;
		lower_switch_body(facts.back());
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(iteration);
		set_current(iteration);
		if (iteration_fact.valid())
		{
			const std::vector<SemanticFactId> expression = children(iteration_fact);
			if (expression.size() != 1)
				throw std::runtime_error("PA15 invalid recovered for iteration fact");
			lower_discarded_expression(expression.front());
		}
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		finish_switch_loop(loop_targets_[id.value]);
	}

bool Pa15Lowerer::lower_switch_body(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];


		if (fact.kind == SemanticFactKind::SwitchStatement)
		{
			if (current_block_ != InvalidIdentityValue) lower_statement(id);
			return false;
		}
		if (fact.kind == SemanticFactKind::IfStatement &&
			current_block_ == InvalidIdentityValue)
		{
			const std::vector<SemanticFactId> branches = children(id);
			std::vector<BlockId> fallthroughs;
			bool entered_label = false;
			for (std::size_t i = 0; i < branches.size(); ++i)
			{
				const SemanticFactKind kind =
					model_.semantic_facts_[branches[i].value].kind;
				if (kind != SemanticFactKind::ThenBranch &&
					kind != SemanticFactKind::ElseBranch)
					continue;
				if (!switch_subtree_has_label(branches[i])) continue;




				if (entered_label) current_block_ = InvalidIdentityValue;
				if (!lower_switch_body(branches[i])) continue;
				entered_label = true;
				if (current_block_ != InvalidIdentityValue)
					fallthroughs.push_back(current_block_id());
			}
			if (fallthroughs.empty())
			{
				current_block_ = InvalidIdentityValue;
			}
			else if (fallthroughs.size() == 1)
			{
				set_current(fallthroughs.front());
			}
			else
			{
				const BlockId join = block_id(new_block("switch_if_end"));
				for (std::size_t i = 0; i < fallthroughs.size(); ++i)
				{
					set_current(fallthroughs[i]);
					if (!terminated(block())) emit_jump(join);
				}
				set_current(join);
			}
			return entered_label;
		}
		if (current_block_ == InvalidIdentityValue &&
			switch_subtree_has_label(id))
		{
			if (fact.kind == SemanticFactKind::WhileStatement)
			{
				lower_switch_while(id);
				return true;
			}
			if (fact.kind == SemanticFactKind::DoStatement)
			{
				lower_switch_do(id);
				return true;
			}
			if (fact.kind == SemanticFactKind::ForStatement)
			{
				lower_switch_for(id);
				return true;
			}
		}
		if (current_block_ == InvalidIdentityValue &&
			!switch_subtree_has_label(id)) return false;
		if (fact.kind == SemanticFactKind::CompoundStatement)
		{
			const std::vector<SemanticFactId> facts = children(id);
			bool entered_label = false;
			for (std::size_t i = 0; i < facts.size(); ++i)
				if (lower_switch_body(facts[i])) entered_label = true;
			return entered_label;
		}
		if (fact.kind == SemanticFactKind::CaseStatement ||
			fact.kind == SemanticFactKind::DefaultStatement)
		{
			return lower_switch_label_recovery(id);
		}
		if (current_block_ != InvalidIdentityValue)
		{
			lower_statement(id);
			return false;
		}






		const std::vector<SemanticFactId> facts = children(id);
		bool entered_label = false;
		for (std::size_t i = 0; i < facts.size(); ++i)
			if (lower_switch_body(facts[i])) entered_label = true;
		return entered_label;
	}

void Pa15Lowerer::finish_switch_labels(){
		if (switch_stack_.empty())
			throw std::runtime_error("PA15 switch label context is missing");
		const SwitchContext& context = switch_stack_.back();
		if (context.lowered_labels.size() != context.labels.size())
			throw std::runtime_error("PA15 switch label was not lowered");
		for (std::map<std::size_t, BlockId>::const_iterator label =
			context.labels.begin(); label != context.labels.end(); ++label)
			if (context.lowered_labels.find(label->first) ==
				context.lowered_labels.end())
				throw std::runtime_error("PA15 switch label was not visited");
		for (std::size_t i = 0; i < context.arms.size(); ++i)
		{
			const BlockId target = context.arms[i].target;
			if (!terminated(target))
			{
				set_current(target);
				emit_jump(context.end_target);
			}
		}
	}

void Pa15Lowerer::lower_switch(const std::vector<SemanticFactId>& facts){
		if (facts.size() < 1 || facts.size() > 2)
			throw std::runtime_error("PA15 invalid switch fact");
		const LoweredValue selector = lower_condition(facts.front());
		const BlockId dispatch = block_id(new_block("switch_dispatch"));
		const BlockId end = block_id(new_block("switch_end"));
		SwitchContext context(end);
		if (facts.size() == 2)
			collect_switch_labels(facts[1], &context);
		emit_jump(dispatch);
		set_current(dispatch);
		Instruction instruction;
		instruction.kind = Instruction::IK_SWITCH;
		instruction.first = selector.value;
		instruction.second = block_operand(context.default_target);
		for (std::size_t i = 0; i < context.arms.size(); ++i)
		{
			if (context.arms[i].is_default) continue;
			instruction.args.push_back(context.arms[i].value);
			instruction.args.push_back(block_operand(context.arms[i].target));
		}
		block().instructions.push_back(instruction);
		const BlockId dispatch_source = current_block_id();
		propagate_edge(dispatch_source, context.default_target);
		for (std::size_t i = 0; i < context.arms.size(); ++i)
			if (!context.arms[i].is_default)
				propagate_edge(dispatch_source, context.arms[i].target);

		switch_stack_.push_back(context);
		control_stack_.push_back(ControlTarget(false, end));
		if (facts.size() == 2)
		{



			current_block_ = InvalidIdentityValue;
			lower_switch_body(facts[1]);
			finish_switch_labels();
		}
		if (current_block_ != InvalidIdentityValue &&
			current_block_id() != end && !terminated(block()))
			emit_jump(end);
		control_stack_.pop_back();
		switch_stack_.pop_back();


		set_current(end);
	}

void Pa15Lowerer::lower_while(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid while fact");
		const BlockId condition = block_id(new_block("while_cond"));
		const BlockId body = block_id(new_block("while_body"));
		const BlockId end = block_id(new_block("while_end"));
		remember_loop_target(id, end, condition);
		emit_jump(condition);
		set_current(condition);
		if (has_direct_short_circuit(facts[0]))
			lower_condition_branch(facts[0], body, end);
		else
		{
			const LoweredValue value = lower_condition(facts[0]);
			emit_branch(value.value, body, end);
		}
		control_stack_.push_back(ControlTarget(true, end, condition));
		set_current(body);
		lower_statement(facts[1]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		control_stack_.pop_back();
		set_current(end);
	}

void Pa15Lowerer::lower_do(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() != 2)
			throw std::runtime_error("PA15 invalid do fact");
		const BlockId body = block_id(new_block("do_body"));
		const BlockId condition = block_id(new_block("do_cond"));
		const BlockId end = block_id(new_block("do_end"));
		remember_loop_target(id, end, condition);
		emit_jump(body);
		control_stack_.push_back(ControlTarget(true, end, condition));
		set_current(body);
		lower_statement(facts[0]);
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		set_current(condition);
		if (has_direct_short_circuit(facts[1]))
			lower_condition_branch(facts[1], body, end);
		else
		{
			const LoweredValue value = lower_condition(facts[1]);
			emit_branch(value.value, body, end);
		}
		control_stack_.pop_back();
		set_current(end);
	}

void Pa15Lowerer::lower_for(SemanticFactId id){
		const std::vector<SemanticFactId> facts = children(id);
		if (facts.size() < 2)
			throw std::runtime_error("PA15 invalid for fact");
		lower_statement(facts[0]);
		SemanticFactId condition_fact;
		SemanticFactId iteration_fact;
		for (std::size_t i = 1; i + 1 < facts.size(); ++i)
		{
			const SemanticFactKind kind = model_.semantic_facts_[facts[i].value].kind;
			if (kind == SemanticFactKind::Condition) condition_fact = facts[i];
			else if (kind == SemanticFactKind::Iteration) iteration_fact = facts[i];
		}
		const BlockId condition = block_id(new_block("for_cond"));
		const BlockId body = block_id(new_block("for_body"));
		const BlockId iteration = block_id(new_block("for_iter"));
		const BlockId end = block_id(new_block("for_end"));
		remember_loop_target(id, end, iteration);
		emit_jump(condition);
		set_current(condition);
		if (!condition_fact.valid() || condition_is_empty(condition_fact))
			emit_jump(body);
		else if (has_direct_short_circuit(condition_fact))
			lower_condition_branch(condition_fact, body, end);
		else
		{
			const LoweredValue value = lower_condition(condition_fact);
			emit_branch(value.value, body, end);
		}
		control_stack_.push_back(ControlTarget(true, end, iteration));
		set_current(body);
		lower_statement(facts.back());
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(iteration);
		set_current(iteration);
		if (iteration_fact.valid())
		{
			const std::vector<SemanticFactId> expression = children(iteration_fact);
			if (expression.size() != 1)
				throw std::runtime_error("PA15 invalid for iteration fact");
			lower_discarded_expression(expression.front());
		}
		if (current_block_ != InvalidIdentityValue && !terminated(block()))
			emit_jump(condition);
		control_stack_.pop_back();
		set_current(end);
	}

void Pa15Lowerer::lower_statement(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (current_block_ == InvalidIdentityValue &&
			fact.kind != SemanticFactKind::CaseStatement &&
			fact.kind != SemanticFactKind::DefaultStatement)
			return;
		const std::vector<SemanticFactId> facts = children(id);
		switch (fact.kind)
		{
		case SemanticFactKind::CompoundStatement:
			for (std::size_t i = 0; i < facts.size(); ++i)
			{
				if (current_block_ == InvalidIdentityValue) break;
				lower_statement(facts[i]);
			}
			break;
		case SemanticFactKind::SimpleDeclaration:
			for (std::size_t i = 0; i < facts.size(); ++i)
				lower_statement(facts[i]);
			break;
		case SemanticFactKind::Variable:
			if (facts.size() > 1) throw std::runtime_error("PA15 invalid local initializer");
			if (facts.size() == 1)
			{
				const LoweredValue storage = storage_for(fact.binding);
				if (storage.type.is_object() &&
					model_.semantic_facts_[facts.front().value].kind ==
					SemanticFactKind::BracedInitList)
						initialize_array(fact.binding, facts.front(), storage);
				else
				{
					FundamentalType storage_fundamental;
					FundamentalType initializer_fundamental;
					const bool bool_storage = model_.fundamental_of(
						model_.expression_object_type(model_.binding(fact.binding).type),
						&storage_fundamental) && storage_fundamental == FundamentalType::Bool;
					const bool bool_initializer = model_.fundamental_of(
						model_.expression_object_type(
							model_.semantic_facts_[facts.front().value].type),
						&initializer_fundamental) && initializer_fundamental == FundamentalType::Bool;
					const LoweredValue value = bool_storage && bool_initializer ?
						lower_condition_expression(facts.front()) :
						lower_expression(facts.front());
					emit_store(storage.type, value.value, storage.value);
				}
			}
			break;
		case SemanticFactKind::ExpressionStatement:
			if (facts.size() == 1) lower_discarded_expression(facts.front());
			break;
		case SemanticFactKind::ReturnStatement:
		{
			Instruction instruction;
			instruction.kind = Instruction::IK_RETURN;
			instruction.type = function().return_type;
			if (facts.size() == 1)
			{
				const SemanticFact& value_fact = model_.semantic_facts_[
					facts.front().value];
				const bool direct_boolean_compare =
					value_fact.kind == SemanticFactKind::BinaryExpression &&
					is_comparison(value_fact.token);
				instruction.first = (direct_boolean_compare ?
					lower_condition_expression(facts.front()) :
					lower_expression(facts.front())).value;
			}
			else if (!instruction.type.is_void())
				throw std::runtime_error("PA15 missing return operand");
			block().instructions.push_back(instruction);
			current_block_ = InvalidIdentityValue;
			break;
		}
		case SemanticFactKind::IfStatement:
			lower_if(facts);
			break;
		case SemanticFactKind::SwitchStatement:
			lower_switch(facts);
			break;
		case SemanticFactKind::WhileStatement:
			lower_while(id);
			break;
		case SemanticFactKind::DoStatement:
			lower_do(id);
			break;
		case SemanticFactKind::ForStatement:
			lower_for(id);
			break;
		case SemanticFactKind::ForInitStatement:
			for (std::size_t i = 0; i < facts.size(); ++i)
			{
				if (model_.semantic_facts_[facts[i].value].kind ==
					SemanticFactKind::SimpleDeclaration)
					lower_statement(facts[i]);
				else
					(void)lower_expression(facts[i]);
			}
			break;
		case SemanticFactKind::Iteration:
			if (facts.size() != 1)
				throw std::runtime_error("PA15 invalid iteration fact");
			lower_discarded_expression(facts.front());
			break;
		case SemanticFactKind::BreakStatement:
			emit_jump(control_target(false));
			current_block_ = InvalidIdentityValue;
			break;
		case SemanticFactKind::ContinueStatement:
			emit_jump(control_target(true));
			current_block_ = InvalidIdentityValue;
			break;
		case SemanticFactKind::CaseStatement:
		{
			const BlockId target = switch_label_target(id);
			if (current_block_ != InvalidIdentityValue &&
				current_block_id() != target && !terminated(block()))
				emit_jump(target);
			set_current(target);
			if (facts.size() != 2)
				throw std::runtime_error("PA15 invalid case statement");
			lower_statement(facts.back());
			break;
		}
		case SemanticFactKind::DefaultStatement:
		{
			const BlockId target = switch_label_target(id);
			if (current_block_ != InvalidIdentityValue &&
				current_block_id() != target && !terminated(block()))
				emit_jump(target);
			set_current(target);
			if (facts.size() != 1)
				throw std::runtime_error("PA15 invalid default statement");
			lower_statement(facts.front());
			break;
		}
		case SemanticFactKind::ThenBranch:
		case SemanticFactKind::ElseBranch:
			if (facts.size() == 1) lower_statement(facts.front());
			break;
		default:
			throw std::runtime_error("PA15 unsupported scalar statement fact");
		}
	}

LoweredValue Pa15Lowerer::lower_condition(SemanticFactId id){
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		const std::vector<SemanticFactId> facts = children(id);
		if (fact.kind == SemanticFactKind::Condition && facts.size() == 1)
			return lower_condition(facts.front());
		if (fact.kind == SemanticFactKind::ConditionDeclaration && facts.size() == 1)
			return lower_condition_expression(facts.front());
		return lower_condition_expression(id);
	}

void Pa15Lowerer::lower_if(const std::vector<SemanticFactId>& facts){
		if (facts.size() < 2 || facts.size() > 3)
			throw std::runtime_error("PA15 invalid if fact");
		const bool direct = has_direct_short_circuit(facts[0]);
		if (direct)
		{



			const std::size_t begin = function().blocks.size();
			const BlockId then_block = block_id(new_block("if_then"));
			const BlockId else_block = block_id(new_block("if_else"));
			const bool implicit_else = facts.size() == 2;
			BlockId join_block;
			if (implicit_else)
				join_block = block_id(new_block("if_end"));
			lower_condition_branch(facts[0], then_block, else_block);
			const BlockId saved_current = current_block_id();
			reorder_condition_blocks(begin, implicit_else ? 3 : 2,
				saved_current);

			set_current(then_block);
			lower_statement(facts[1]);
			const bool then_terminated = terminated(then_block);
			set_current(else_block);
			if (!implicit_else) lower_statement(facts[2]);
			const bool else_terminated = terminated(else_block);
			if (then_terminated && else_terminated)
			{
				current_block_ = InvalidIdentityValue;
				return;
			}
			if (!join_block.valid())
				join_block = block_id(new_block("if_end"));
			if (!then_terminated)
			{
				set_current(then_block);
				emit_jump(join_block);
			}
			if (!else_terminated)
			{
				set_current(else_block);
				emit_jump(join_block);
			}
			set_current(join_block);
			return;
		}

		const LoweredValue condition = lower_condition(facts[0]);
		const BlockId then_block = block_id(new_block("if_then"));
		const BlockId else_block = block_id(new_block("if_else"));
		emit_branch(condition.value, then_block, else_block);

		set_current(then_block);
		lower_statement(facts[1]);
		const bool then_terminated = terminated(then_block);
		set_current(else_block);
		if (facts.size() == 3) lower_statement(facts[2]);
		const bool else_terminated = terminated(else_block);
		if (then_terminated && else_terminated)
		{
			current_block_ = InvalidIdentityValue;
			return;
		}
		const BlockId join_block = block_id(new_block("if_end"));
		if (!then_terminated)
		{
			set_current(then_block);
			emit_jump(join_block);
		}
		if (!else_terminated)
		{
			set_current(else_block);
			emit_jump(join_block);
		}
		set_current(join_block);
	}

void Pa15Lowerer::lower_function(const FunctionPlan& plan){
		current_function_ = plan.program_index;
		current_block_ = InvalidIdentityValue;
		temp_ordinal_ = 0;
		block_ordinal_ = 0;
		generated_slot_ordinal_ = 0;
		block_indexes_.clear();
		control_stack_.clear();
		switch_stack_.clear();
		block_order_.clear();
		ordered_block_ids_.clear();
		reachability_base_ = next_block_;
		reachable_blocks_.clear();
		reachability_work_.clear();
		Function& target = function();
		used_value_names_.clear();
		for (std::size_t i = 0; i < target.params.size(); ++i)
			used_value_names_.insert(spelling(target.params[i].name_id));
		target.value_begin = ValueId(next_value_);
		for (std::size_t i = 0; i < target.params.size(); ++i)
			target.params[i].value_id = allocate_value();
		const std::size_t value_begin = target.value_begin.index;
		const BlockId entry = block_id(new_block("entry"));
		set_current(entry);
		mark_reachable(entry);
		for (std::size_t i = 0; i < target.params.size(); ++i)
		{
			const lowir_model::Parameter& parameter = target.params[i];
			if (i < target.slots.size())
			{
				Instruction store;
				store.kind = Instruction::IK_STORE;
				store.type = parameter.type;
				store.first = temporary_operand(parameter.value_id, parameter.name_id);
				store.second = slot_operand(target.slots[i].slot_id);
				block().instructions.push_back(store);
			}
		}
		const FunctionFact& fact = model_.function_facts_[plan.fact_index];
		if (!fact.body_fact.valid())
			throw std::runtime_error("PA15 function body fact is missing");
		lower_statement(fact.body_fact);
		if (current_block_ != InvalidIdentityValue &&
			!terminated(block()))
		{
			const BlockId continuation = current_block_id();
			if (!is_reachable(continuation))
			{


				terminate_unreachable_block(continuation);
				current_block_ = InvalidIdentityValue;
			}
			else
			{
				if (!target.return_type.is_void())
					throw std::runtime_error("PA15 function falls through without return");
				Instruction instruction;
				instruction.kind = Instruction::IK_RETURN;
				instruction.type = target.return_type;
				block().instructions.push_back(instruction);
			}
		}
		target.value_count = next_value_ - value_begin;
		target.slot_count = next_slot_ - target.slot_begin.index;
		reorder_function_blocks();
	}
}
