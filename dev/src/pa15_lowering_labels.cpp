#include "pa15_lowering.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

static bool label_recovery_loop_kind(SemanticFactKind kind){
	return kind == SemanticFactKind::WhileStatement ||
		kind == SemanticFactKind::DoStatement ||
		kind == SemanticFactKind::ForStatement;
}

static bool label_recovery_switch_kind(SemanticFactKind kind){
	return kind == SemanticFactKind::SwitchStatement;
}

void Pa15Lowerer::store_loop_flow(SemanticFactId id, const LoopFlow& flow){
		if (!id.valid() || id.value >= loop_flow_indexes_.size() ||
			id.value >= fact_index_generations_.size() ||
			fact_index_generations_[id.value] != label_generation_)
			throw std::runtime_error("PA15 loop flow identity is invalid");
		LoopFlowIndex& index = loop_flow_indexes_[id.value];
		if (!index.valid())
		{
			loop_flow_arena_.push_back(flow);
			index = LoopFlowIndex(loop_flow_arena_.size() - 1);
		}
		else
		{
			if (index.value >= loop_flow_arena_.size())
				throw std::runtime_error("PA15 loop flow index is invalid");
			loop_flow_arena_[index.value] = flow;
		}
}

const LoopFlow* Pa15Lowerer::remembered_loop_flow(SemanticFactId id) const{
		if (!id.valid() || id.value >= loop_flow_indexes_.size() ||
			id.value >= fact_index_generations_.size() ||
			fact_index_generations_[id.value] != label_generation_)
			return NULL;
		const LoopFlowIndex index = loop_flow_indexes_[id.value];
		if (!index.valid()) return NULL;
		if (index.value >= loop_flow_arena_.size())
			throw std::runtime_error("PA15 loop flow index is invalid");
		return loop_flow_arena_[index.value].valid ?
			&loop_flow_arena_[index.value] : NULL;
}

void Pa15Lowerer::store_if_flow(SemanticFactId id, const IfFlow& flow){
		if (!id.valid() || id.value >= if_flow_indexes_.size() ||
			id.value >= fact_index_generations_.size() ||
			fact_index_generations_[id.value] != label_generation_)
			throw std::runtime_error("PA15 if flow identity is invalid");
		IfFlowIndex& index = if_flow_indexes_[id.value];
		if (!index.valid())
		{
			if_flow_arena_.push_back(flow);
			index = IfFlowIndex(if_flow_arena_.size() - 1);
		}
		else
		{
			if (index.value >= if_flow_arena_.size())
				throw std::runtime_error("PA15 if flow index is invalid");
			if_flow_arena_[index.value] = flow;
		}
}

const IfFlow* Pa15Lowerer::remembered_if_flow(SemanticFactId id) const{
		if (!id.valid() || id.value >= if_flow_indexes_.size() ||
			id.value >= fact_index_generations_.size() ||
			fact_index_generations_[id.value] != label_generation_)
			return NULL;
		const IfFlowIndex index = if_flow_indexes_[id.value];
		if (!index.valid()) return NULL;
		if (index.value >= if_flow_arena_.size())
			throw std::runtime_error("PA15 if flow index is invalid");
		return if_flow_arena_[index.value].valid ?
			&if_flow_arena_[index.value] : NULL;
}

void Pa15Lowerer::store_switch_flow(SemanticFactId id,
	const SwitchContext& flow){
		if (!id.valid() || id.value >= switch_flow_indexes_.size() ||
			id.value >= fact_index_generations_.size() ||
			fact_index_generations_[id.value] != label_generation_)
			throw std::runtime_error("PA15 switch flow identity is invalid");
		SwitchFlowIndex& index = switch_flow_indexes_[id.value];
		if (!index.valid())
		{
			switch_flow_arena_.push_back(flow);
			index = SwitchFlowIndex(switch_flow_arena_.size() - 1);
		}
		else
		{
			if (index.value >= switch_flow_arena_.size())
				throw std::runtime_error("PA15 switch flow index is invalid");
			switch_flow_arena_[index.value] = flow;
		}
}

const SwitchContext* Pa15Lowerer::remembered_switch_flow(
	SemanticFactId id) const{
		if (!id.valid() || id.value >= switch_flow_indexes_.size() ||
			id.value >= fact_index_generations_.size() ||
			fact_index_generations_[id.value] != label_generation_)
			return NULL;
		const SwitchFlowIndex index = switch_flow_indexes_[id.value];
		if (!index.valid()) return NULL;
		if (index.value >= switch_flow_arena_.size())
			throw std::runtime_error("PA15 switch flow index is invalid");
		const SwitchContext& flow = switch_flow_arena_[index.value];
		if (!flow.dispatch_target.valid() || !flow.end_target.valid())
			return NULL;
	return &flow;
}

bool Pa15Lowerer::enter_shared_compound_cursor(SemanticFactId parent,
	std::size_t child_index){
	if (!parent.valid() || parent.value >= model_.semantic_facts_.size() ||
		parent.value >= fact_index_generations_.size() ||
		fact_index_generations_[parent.value] != label_generation_)
		throw std::runtime_error("PA15 shared continuation parent is invalid");
	const SemanticFact& parent_fact = model_.semantic_facts_[parent.value];
	if (parent_fact.kind != SemanticFactKind::CompoundStatement ||
		parent_fact.child_count == 0 ||
		parent_fact.child_begin == InvalidIdentityValue ||
		parent_fact.child_begin + parent_fact.child_count >
			model_.semantic_children_.size() ||
		child_index >= parent_fact.child_count)
		throw std::runtime_error("PA15 shared continuation cursor is invalid");
	const SemanticFactId first =
		model_.semantic_children_[parent_fact.child_begin + child_index];
	if (!first.valid() || first.value >= continuation_indexes_.size() ||
		first.value >= fact_index_generations_.size() ||
		fact_index_generations_[first.value] != label_generation_)
		throw std::runtime_error("PA15 shared continuation fact is stale");
	ContinuationIndex& index = continuation_indexes_[first.value];
	if (index.valid())
	{
		if (index.value >= continuation_arena_.size() ||
			continuation_arena_[index.value].state == 0)
			throw std::runtime_error("PA15 shared continuation index is invalid");
		const BlockId entry = continuation_arena_[index.value].entry;
		if (!entry.valid())
			throw std::runtime_error("PA15 shared continuation entry is invalid");
		if (current_block_ != InvalidIdentityValue)
		{
			if (!terminated(block()) && current_block_id() != entry)
				emit_jump(entry);
			current_block_ = InvalidIdentityValue;
		}
		return false;
	}
	if (current_block_ == InvalidIdentityValue)
		return false;
	if (terminated(block()))
	{
		current_block_ = InvalidIdentityValue;
		return false;
	}
	BlockId entry;
	if (child_index == 0)
		entry = current_block_id();
	else
	{
		entry = block_id(new_block("label_cont"));
		emit_jump(entry);
		set_current(entry);
	}
	continuation_arena_.push_back(CompoundContinuation(entry, 1));
	index = ContinuationIndex(continuation_arena_.size() - 1);
	return true;
}

bool Pa15Lowerer::enter_recovery_frame_continuation(
	SemanticFactId frame){
	if (!frame.valid() || frame.value >= model_.semantic_facts_.size() ||
		frame.value >= fact_recovery_exit_indexes_.size() ||
		frame.value >= fact_index_generations_.size() ||
		fact_index_generations_[frame.value] != label_generation_)
		throw std::runtime_error("PA15 recovery frame is invalid");
	const SemanticFactKind kind = model_.semantic_facts_[frame.value].kind;
	if (!label_recovery_loop_kind(kind) &&
		kind != SemanticFactKind::IfStatement &&
		kind != SemanticFactKind::CaseStatement &&
		kind != SemanticFactKind::DefaultStatement &&
		kind != SemanticFactKind::SwitchStatement)
		throw std::runtime_error("PA15 recovery frame kind is invalid");
	ContinuationIndex& index = fact_recovery_exit_indexes_[frame.value];
	if (index.valid())
	{
		if (index.value >= continuation_arena_.size() ||
			continuation_arena_[index.value].state == 0 ||
			!continuation_arena_[index.value].entry.valid())
			throw std::runtime_error("PA15 recovery frame continuation is invalid");
		const BlockId entry = continuation_arena_[index.value].entry;
		if (current_block_ != InvalidIdentityValue)
		{
			if (!terminated(block()) && current_block_id() != entry)
				emit_jump(entry);
			current_block_ = InvalidIdentityValue;
		}
		return false;
	}
	if (current_block_ == InvalidIdentityValue)
		return false;
	if (terminated(block()))
	{
		current_block_ = InvalidIdentityValue;
		return false;
	}
	const BlockId entry = block_id(new_block("label_exit"));
	emit_jump(entry);
	set_current(entry);
	// The frame exit is a complete canonical entry as soon as it is
	// installed.  Its enclosing frame is walked by the caller, not by a
	// second visit to this exit node.
	continuation_arena_.push_back(CompoundContinuation(entry, 2));
	index = ContinuationIndex(continuation_arena_.size() - 1);
	return true;
}

void Pa15Lowerer::finish_shared_compound_cursor(SemanticFactId parent,
	std::size_t child_index){
	if (!parent.valid() || parent.value >= model_.semantic_facts_.size() ||
		parent.value >= fact_index_generations_.size() ||
		fact_index_generations_[parent.value] != label_generation_)
		throw std::runtime_error("PA15 shared continuation parent is invalid");
	const SemanticFact& parent_fact = model_.semantic_facts_[parent.value];
	if (parent_fact.kind != SemanticFactKind::CompoundStatement ||
		parent_fact.child_count == 0 ||
		parent_fact.child_begin == InvalidIdentityValue ||
		parent_fact.child_begin + parent_fact.child_count >
			model_.semantic_children_.size() ||
		child_index >= parent_fact.child_count)
		throw std::runtime_error("PA15 shared continuation cursor is invalid");
	const SemanticFactId first =
		model_.semantic_children_[parent_fact.child_begin + child_index];
	if (!first.valid() || first.value >= continuation_indexes_.size() ||
		first.value >= fact_index_generations_.size() ||
		fact_index_generations_[first.value] != label_generation_)
		throw std::runtime_error("PA15 shared continuation fact is invalid");
	const ContinuationIndex index = continuation_indexes_[first.value];
	if (!index.valid() || index.value >= continuation_arena_.size() ||
		continuation_arena_[index.value].state != 1)
		throw std::runtime_error("PA15 shared continuation was not entered");
	continuation_arena_[index.value].state = 2;
}

bool Pa15Lowerer::jump_to_cached_label_continuation(
	SemanticFactId label_fact){
	if (!label_fact.valid() || label_fact.value >= fact_recovery_frames_.size() ||
		label_fact.value >= fact_recovery_frame_indexes_.size() ||
		label_fact.value >= fact_index_generations_.size() ||
		fact_index_generations_[label_fact.value] != label_generation_)
		throw std::runtime_error("PA15 cached label continuation fact is stale");
	const SemanticFactId frame = fact_recovery_frames_[label_fact.value];
	if (!frame.valid()) return false;
	if (frame.value >= model_.semantic_facts_.size() ||
		frame.value >= fact_recovery_frame_children_.size() ||
		frame.value >= fact_recovery_frame_indexes_.size() ||
		frame.value >= fact_recovery_exit_indexes_.size() ||
		frame.value >= fact_index_generations_.size() ||
		fact_index_generations_[frame.value] != label_generation_)
		throw std::runtime_error("PA15 cached label continuation frame is stale");
	const std::size_t child_index =
		fact_recovery_frame_indexes_[label_fact.value];
	if (child_index == InvalidIdentityValue)
		throw std::runtime_error("PA15 cached label continuation index is invalid");
	const SemanticFact& frame_fact = model_.semantic_facts_[frame.value];
	if (frame_fact.kind == SemanticFactKind::CompoundStatement)
	{
		if (frame_fact.child_count == 0 ||
			frame_fact.child_begin == InvalidIdentityValue ||
			frame_fact.child_begin + frame_fact.child_count >
				model_.semantic_children_.size() ||
			child_index >= frame_fact.child_count)
			throw std::runtime_error("PA15 cached compound continuation is invalid");
		const std::size_t next_index = child_index + 1;
		if (next_index >= frame_fact.child_count) return false;
		const SemanticFactId next =
			model_.semantic_children_[frame_fact.child_begin + next_index];
		if (!next.valid() || next.value >= continuation_indexes_.size() ||
			next.value >= fact_index_generations_.size() ||
			fact_index_generations_[next.value] != label_generation_)
			throw std::runtime_error("PA15 cached compound child is stale");
		if (!continuation_indexes_[next.value].valid() ||
			continuation_indexes_[next.value].value >= continuation_arena_.size() ||
			continuation_arena_[continuation_indexes_[next.value].value].state != 2)
			return false;
		(void)enter_shared_compound_cursor(frame, next_index);
		return true;
	}
	if (!label_recovery_loop_kind(frame_fact.kind) &&
		frame_fact.kind != SemanticFactKind::IfStatement &&
		frame_fact.kind != SemanticFactKind::CaseStatement &&
		frame_fact.kind != SemanticFactKind::DefaultStatement &&
		frame_fact.kind != SemanticFactKind::SwitchStatement)
		return false;
	if (!fact_recovery_exit_indexes_[frame.value].valid() ||
		fact_recovery_exit_indexes_[frame.value].value >= continuation_arena_.size() ||
		continuation_arena_[fact_recovery_exit_indexes_[frame.value].value].state != 2)
		return false;
	(void)enter_recovery_frame_continuation(frame);
	return true;
}

void Pa15Lowerer::initialize_label_storage(){
	const std::size_t label_count = model_.label_facts_.size();
	const std::size_t fact_count = model_.semantic_facts_.size();
	label_blocks_.resize(label_count);
	label_referenced_.resize(label_count);
	label_lowered_.resize(label_count);
	label_subtrees_.resize(fact_count);
	label_index_states_.resize(fact_count);
	label_subtree_states_.resize(fact_count);
	label_block_generations_.assign(label_count, 0);
	label_referenced_generations_.assign(label_count, 0);
	fact_index_generations_.assign(fact_count, 0);
	fact_subtree_generations_.assign(fact_count, 0);
	label_lowered_generations_.assign(label_count, 0);
	label_recovery_waiting_generations_.assign(label_count, 0);
	label_recovery_queued_generations_.assign(label_count, 0);
	label_statement_facts_.assign(label_count, SemanticFactId());
	fact_parents_.assign(fact_count, SemanticFactId());
	fact_parent_indexes_.assign(fact_count, InvalidIdentityValue);
	fact_recovery_frames_.assign(fact_count, SemanticFactId());
	fact_recovery_frame_children_.assign(fact_count, SemanticFactId());
	fact_recovery_frame_indexes_.assign(fact_count, InvalidIdentityValue);
	fact_recovery_orders_.assign(fact_count, InvalidIdentityValue);
	fact_recovery_ends_.assign(fact_count, InvalidIdentityValue);
	fact_recovery_exit_indexes_.assign(fact_count, ContinuationIndex());
	fact_switch_ancestors_.assign(fact_count, SemanticFactId());
	fact_recovery_control_heads_.assign(fact_count,
		RecoveryControlIndex());
	recovery_control_arena_.clear();
	label_recovery_root_ = SemanticFactId();
	label_recovery_order_ = 0;
	label_recovery_boundaries_.clear();
	label_recovery_queue_.clear();
	label_generation_ = 0;
	recovery_control_head_ = RecoveryControlIndex();
	recovery_control_base_depth_ = 0;
	recovery_control_active_ = false;
}

void Pa15Lowerer::begin_label_flow(){
	if (label_generation_ ==
		std::numeric_limits<std::uint32_t>::max())
	{
		std::fill(label_block_generations_.begin(),
			label_block_generations_.end(), 0);
		std::fill(label_referenced_generations_.begin(),
			label_referenced_generations_.end(), 0);
		std::fill(fact_index_generations_.begin(),
			fact_index_generations_.end(), 0);
		std::fill(fact_subtree_generations_.begin(),
			fact_subtree_generations_.end(), 0);
		std::fill(label_lowered_generations_.begin(),
			label_lowered_generations_.end(), 0);
		std::fill(label_recovery_waiting_generations_.begin(),
			label_recovery_waiting_generations_.end(), 0);
		std::fill(label_recovery_queued_generations_.begin(),
			label_recovery_queued_generations_.end(), 0);
		// Fact-keyed continuation indexes are normally never reused because
		// semantic facts are translation-unit owned.  Clear them at the epoch
		// reset as well, so a wrapped generation cannot observe an old cursor
		// even if a future model reuses a fact identity.
		std::fill(continuation_indexes_.begin(),
			continuation_indexes_.end(), ContinuationIndex());
		std::fill(fact_recovery_exit_indexes_.begin(),
			fact_recovery_exit_indexes_.end(), ContinuationIndex());
		continuation_arena_.clear();
		std::fill(fact_recovery_control_heads_.begin(),
			fact_recovery_control_heads_.end(), RecoveryControlIndex());
		recovery_control_arena_.clear();
		label_generation_ = 1;
	}
	else
		++label_generation_;
	label_recovery_queue_.clear();
	label_recovery_boundaries_.clear();
	label_recovery_order_ = 0;
	recovery_control_head_ = RecoveryControlIndex();
	recovery_control_base_depth_ = 0;
	recovery_control_active_ = false;
}

BlockId Pa15Lowerer::label_target(LabelId label){
		if (!label.valid())
			throw std::runtime_error("PA15 label has no semantic identity");
		if (label.value >= label_blocks_.size())
			throw std::runtime_error("PA15 label identity is out of range");
		if (label_block_generations_[label.value] != label_generation_)
		{
			label_block_generations_[label.value] = label_generation_;
			label_blocks_[label.value] = BlockId();
		}
		BlockId& target = label_blocks_[label.value];
		if (target.valid()) return target;
		target = block_id(new_block("goto"));
		if (label_recovery_waiting_generations_[label.value] ==
			label_generation_)
			queue_label_recovery(label);
		return target;
	}

bool Pa15Lowerer::current_label_target(LabelId label) const{
		return label.valid() && label.value < label_blocks_.size() &&
			label_block_generations_[label.value] == label_generation_ &&
			label_blocks_[label.value].valid();
}

bool Pa15Lowerer::current_label_referenced(LabelId label) const{
		return label.valid() && label.value < label_referenced_.size() &&
			label_referenced_generations_[label.value] == label_generation_ &&
			label_referenced_[label.value] != 0;
}

void Pa15Lowerer::push_label_recovery_boundary(SemanticFactId boundary){
	if (!boundary.valid() || boundary.value >= model_.semantic_facts_.size() ||
		model_.semantic_facts_[boundary.value].kind !=
			SemanticFactKind::CompoundStatement)
		throw std::runtime_error("PA15 label recovery boundary is invalid");
	label_recovery_boundaries_.push_back(boundary);
}

void Pa15Lowerer::pop_label_recovery_boundary(){
	if (label_recovery_boundaries_.empty())
		throw std::runtime_error("PA15 label recovery boundary stack is empty");
	label_recovery_boundaries_.pop_back();
}

SemanticFactId Pa15Lowerer::label_recovery_boundary() const{
	return label_recovery_boundaries_.empty() ? label_recovery_root_ :
		label_recovery_boundaries_.back();
}

void Pa15Lowerer::push_label_recovery_controls(
	const LabelRecoveryWork& work, std::size_t* saved_depth){
	if (saved_depth == NULL)
		throw std::runtime_error("PA15 label recovery control depth is missing");
	*saved_depth = control_stack_.size();
	if (!work.label_fact.valid() || work.label_fact.value >=
		fact_index_generations_.size() ||
		fact_index_generations_[work.label_fact.value] != label_generation_)
		throw std::runtime_error("PA15 label recovery control fact is stale");
	if (work.label_fact.value >= fact_recovery_control_heads_.size())
		throw std::runtime_error("PA15 label recovery control index is invalid");
	const RecoveryControlIndex head =
		fact_recovery_control_heads_[work.label_fact.value];
	if (head.valid() && head.value >= recovery_control_arena_.size())
		throw std::runtime_error("PA15 label recovery control chain is invalid");
	// The chain is a persistent typed context.  Keep it out of
	// control_stack_: materializing root-to-label targets for every queued
	// label would reintroduce an O(labels * depth) path.  control_target()
	// reads this context between the saved outer stack and any controls
	// lowered inside the recovered label body.
	recovery_control_head_ = head;
	recovery_control_base_depth_ = *saved_depth;
	recovery_control_active_ = true;
}

void Pa15Lowerer::pop_label_recovery_control(SemanticFactId frame){
	if (!recovery_control_active_)
		throw std::runtime_error("PA15 label recovery control context is inactive");
	if (!frame.valid() || frame.value >= model_.semantic_facts_.size() ||
		frame.value >= fact_index_generations_.size() ||
		fact_index_generations_[frame.value] != label_generation_ ||
		(!label_recovery_loop_kind(model_.semantic_facts_[frame.value].kind) &&
			!label_recovery_switch_kind(model_.semantic_facts_[frame.value].kind)))
		throw std::runtime_error("PA15 label recovery control frame is invalid");
	if (!recovery_control_head_.valid() ||
		recovery_control_head_.value >= recovery_control_arena_.size())
		throw std::runtime_error("PA15 label recovery control chain is exhausted");
	const RecoveryControlNode& node =
		recovery_control_arena_[recovery_control_head_.value];
	if (node.fact != frame)
		throw std::runtime_error("PA15 label recovery control order is invalid");
	recovery_control_head_ = node.parent;
}

BlockId Pa15Lowerer::control_target(bool continue_target) const{
	const std::size_t saved_depth = recovery_control_active_ ?
		std::min(recovery_control_base_depth_, control_stack_.size()) : 0;
	// Controls lowered inside the recovered label body are the innermost
	// targets and therefore precede its persistent structural context.
	for (std::size_t i = control_stack_.size(); i > saved_depth; --i)
	{
		const ControlTarget& target = control_stack_[i - 1];
		if (continue_target && !target.loop) continue;
		const BlockId result = continue_target ? target.continue_target :
			target.break_target;
		if (result.valid()) return result;
	}
	if (recovery_control_active_ && recovery_control_head_.valid())
	{
		if (recovery_control_head_.value >= recovery_control_arena_.size())
			throw std::runtime_error(
				"PA15 recovered control context index is invalid");
		const RecoveryControlNode& head =
			recovery_control_arena_[recovery_control_head_.value];
		const RecoveryControlIndex selected = continue_target ?
			head.continue_frame : recovery_control_head_;
		if (!selected.valid() || selected.value >= recovery_control_arena_.size())
			throw std::runtime_error(
				"PA15 recovered control target frame is missing");
		const SemanticFactId frame =
			recovery_control_arena_[selected.value].fact;
		if (!frame.valid() || frame.value >= model_.semantic_facts_.size() ||
			frame.value >= fact_index_generations_.size() ||
			fact_index_generations_[frame.value] != label_generation_)
			throw std::runtime_error(
				"PA15 recovered control context fact is stale");
		const SemanticFactKind kind = model_.semantic_facts_[frame.value].kind;
		const bool loop_kind = kind == SemanticFactKind::WhileStatement ||
			kind == SemanticFactKind::DoStatement ||
			kind == SemanticFactKind::ForStatement;
		if (loop_kind)
		{
			const LoopFlow* flow = remembered_loop_flow(frame);
			if (flow == NULL)
				throw std::runtime_error(
					"PA15 recovered control loop flow is missing");
			if (continue_target)
				return kind == SemanticFactKind::ForStatement ?
					flow->iteration : flow->condition;
			return flow->end;
		}
		if (kind == SemanticFactKind::SwitchStatement && !continue_target)
		{
			const SwitchContext* flow = remembered_switch_flow(frame);
			if (flow == NULL)
				throw std::runtime_error(
					"PA15 recovered control switch flow is missing");
			return flow->end_target;
		}
		throw std::runtime_error(
			"PA15 recovered control target kind is invalid");
	}
	// Once the recovered path has popped all of its own frames, the
	// caller's saved outer controls remain the fallback context.
	for (std::size_t i = saved_depth; i != 0; --i)
	{
		const ControlTarget& target = control_stack_[i - 1];
		if (continue_target && !target.loop) continue;
		const BlockId result = continue_target ? target.continue_target :
			target.break_target;
		if (result.valid()) return result;
	}
	throw std::runtime_error(continue_target ?
		"PA15 continue target is missing" : "PA15 break target is missing");
}

void Pa15Lowerer::queue_label_recovery(LabelId label){
	if (!label.valid() || label.value >= label_statement_facts_.size())
		throw std::runtime_error("PA15 queued label identity is invalid");
	if (label_recovery_queued_generations_[label.value] == label_generation_)
		return;
	const SemanticFactId fact = label_statement_facts_[label.value];
	if (!fact.valid() || fact.value >= fact_parents_.size() ||
		fact_index_generations_[fact.value] != label_generation_)
		throw std::runtime_error("PA15 queued label fact is missing");
	const SemanticFactId boundary = label_recovery_boundary();
	if (!boundary.valid())
		throw std::runtime_error("PA15 queued label boundary is missing");
	label_recovery_queue_[boundary].push_back(LabelRecoveryWork(label, fact,
		boundary));
	label_recovery_queued_generations_[label.value] = label_generation_;
}

void Pa15Lowerer::defer_label_recovery(LabelId label){
	if (!label.valid() || label.value >= label_recovery_waiting_generations_.size())
		throw std::runtime_error("PA15 deferred label identity is invalid");
	if (label_recovery_waiting_generations_[label.value] != label_generation_)
		label_recovery_waiting_generations_[label.value] = label_generation_;
	if (current_label_target(label))
		queue_label_recovery(label);
}

void Pa15Lowerer::lower_label_recovery_continuation(
	const LabelRecoveryWork& work, SemanticFactId boundary,
	std::size_t resume_child){
	if (jump_to_cached_label_continuation(work.label_fact))
		return;
	SemanticFactId child = work.label_fact;
	while (child.valid() && child.value < fact_parents_.size())
	{
		if (child.value >= model_.semantic_facts_.size() ||
			fact_index_generations_[child.value] != label_generation_)
			throw std::runtime_error("PA15 label continuation fact is stale");
		if (boundary.valid() &&
			(boundary.value >= model_.semantic_facts_.size() ||
				boundary.value >= fact_recovery_orders_.size() ||
				boundary.value >= fact_recovery_ends_.size() ||
				fact_recovery_orders_[boundary.value] == InvalidIdentityValue ||
				fact_recovery_ends_[boundary.value] == InvalidIdentityValue))
			throw std::runtime_error("PA15 label continuation boundary is stale");
		const SemanticFactId parent = fact_recovery_frames_[child.value];
		const bool boundary_contains_child = boundary.valid() &&
			fact_recovery_orders_[boundary.value] <=
				fact_recovery_orders_[child.value] &&
			fact_recovery_orders_[child.value] <
				fact_recovery_ends_[boundary.value];
		if (!parent.valid())
		{
			if (boundary_contains_child)
				return;
			child = label_recovery_root_;
			break;
		}
		if (parent.value >= model_.semantic_facts_.size() ||
			parent.value >= fact_recovery_orders_.size() ||
			parent.value >= fact_recovery_ends_.size() ||
			fact_index_generations_[parent.value] != label_generation_)
			throw std::runtime_error("PA15 label continuation frame is stale");
		if (boundary_contains_child &&
			!(fact_recovery_orders_[boundary.value] <=
				fact_recovery_orders_[parent.value] &&
				fact_recovery_orders_[parent.value] <
				fact_recovery_ends_[boundary.value]))
			return;
		const SemanticFactId child_fact =
			fact_recovery_frame_children_[child.value];
		const std::size_t child_index =
			fact_recovery_frame_indexes_[child.value];
		if (!child_fact.valid() || child_fact.value >= fact_parents_.size() ||
			child_fact.value >= fact_index_generations_.size() ||
			fact_index_generations_[child_fact.value] != label_generation_)
			throw std::runtime_error("PA15 label continuation frame child is invalid");
		if (parent.value >= model_.semantic_facts_.size() ||
			child_index == InvalidIdentityValue)
			throw std::runtime_error("PA15 label continuation frame is invalid");
		const SemanticFact& parent_fact = model_.semantic_facts_[parent.value];
		if (parent_fact.child_count != 0 &&
			(parent_fact.child_begin == InvalidIdentityValue ||
			 parent_fact.child_begin + parent_fact.child_count >
				model_.semantic_children_.size()))
			throw std::runtime_error("PA15 label continuation child range is invalid");
		if (child_index >= parent_fact.child_count ||
			model_.semantic_children_[parent_fact.child_begin + child_index] !=
				child_fact)
			throw std::runtime_error("PA15 label continuation child is stale");
		child = parent;
		if (parent_fact.kind == SemanticFactKind::CompoundStatement)
		{
			const std::size_t begin = child_index + 1;
			if (begin > parent_fact.child_count)
				throw std::runtime_error("PA15 label continuation cursor is invalid");
			const bool stop_here = boundary.valid() && parent == boundary;
			const std::size_t end = stop_here && resume_child != InvalidIdentityValue ?
				std::min(resume_child, parent_fact.child_count) :
				parent_fact.child_count;
			if (stop_here && resume_child > parent_fact.child_count)
				throw std::runtime_error("PA15 label continuation boundary is invalid");
			push_label_recovery_boundary(parent);
			bool shared_path = true;
			for (std::size_t i = begin; i < end; ++i)
			{
				const SemanticFactId sibling =
					model_.semantic_children_[parent_fact.child_begin + i];
				bool entered_shared = false;
				if (current_block_ != InvalidIdentityValue && shared_path &&
					!enter_shared_compound_cursor(parent, i))
				{
					pop_label_recovery_boundary();
					return;
				}
				if (current_block_ != InvalidIdentityValue && shared_path)
					entered_shared = true;
				if (current_block_ == InvalidIdentityValue)
				{
					if (referenced_label_subtree(sibling))
						lower_referenced_label_subtree(sibling);
				}
				else
				{
					const SemanticFactKind kind =
						model_.semantic_facts_[sibling.value].kind;
					if (kind == SemanticFactKind::CaseStatement ||
						kind == SemanticFactKind::DefaultStatement)
					{
						lower_existing_switch_label(sibling);
					}
					else
					{
						const bool existing_control =
						(label_recovery_loop_kind(kind) &&
							remembered_loop_flow(sibling) != NULL) ||
						(kind == SemanticFactKind::IfStatement &&
							remembered_if_flow(sibling) != NULL) ||
						(kind == SemanticFactKind::SwitchStatement &&
							remembered_switch_flow(sibling) != NULL);
						if (existing_control)
							lower_existing_label_control(sibling);
						else
							lower_statement(sibling);
					}
				}
				if (entered_shared)
					finish_shared_compound_cursor(parent, i);
				drain_label_recovery_queue(parent,
					stop_here && resume_child != InvalidIdentityValue ? i + 1 :
						InvalidIdentityValue);
				if (current_block_ == InvalidIdentityValue) break;
			}
				pop_label_recovery_boundary();
			if (current_block_ == InvalidIdentityValue || stop_here)
				return;
			continue;
		}
		if (parent_fact.kind == SemanticFactKind::WhileStatement ||
			parent_fact.kind == SemanticFactKind::DoStatement ||
			parent_fact.kind == SemanticFactKind::ForStatement)
		{
			if (parent_fact.child_count == 0)
				throw std::runtime_error("PA15 label continuation loop is empty");
			const std::size_t body_index =
				parent_fact.kind == SemanticFactKind::WhileStatement ? 1 :
				parent_fact.kind == SemanticFactKind::DoStatement ? 0 :
				parent_fact.child_count - 1;
			if (child_index != body_index)
				throw std::runtime_error("PA15 label continuation is not a loop body");
			const LoopFlow* flow = remembered_loop_flow(parent);
			if (flow == NULL)
				throw std::runtime_error("PA15 label continuation loop flow is missing");
			const BlockId continue_target = parent_fact.kind == SemanticFactKind::ForStatement ?
				flow->iteration : flow->condition;
			if (current_block_ != InvalidIdentityValue &&
				current_block_id() != continue_target && !terminated(block()))
				emit_jump(continue_target);
			set_current(flow->end);
			if (!enter_recovery_frame_continuation(parent))
				return;
			pop_label_recovery_control(parent);
			continue;
		}
		if (parent_fact.kind == SemanticFactKind::IfStatement)
		{
			if (child_index == 0 || child_index > 2)
				throw std::runtime_error("PA15 label continuation is not an if branch");
			const IfFlow* flow = remembered_if_flow(parent);
			if (flow == NULL)
				throw std::runtime_error("PA15 label continuation if flow is missing");
			if (current_block_ != InvalidIdentityValue &&
				current_block_id() != flow->join && !terminated(block()))
				emit_jump(flow->join);
			set_current(flow->join);
			if (!enter_recovery_frame_continuation(parent))
				return;
			continue;
		}
		if (parent_fact.kind == SemanticFactKind::CaseStatement ||
			parent_fact.kind == SemanticFactKind::DefaultStatement)
		{
			const SemanticFactId switch_fact = enclosing_switch_fact(parent);
			const SwitchContext* flow = remembered_switch_flow(switch_fact);
			if (flow == NULL)
				throw std::runtime_error("PA15 label continuation switch flow is missing");
			if (flow->labels.find(parent.value) == flow->labels.end())
				throw std::runtime_error("PA15 label continuation switch arm is missing");
			// A case fact owns only its first substatement.  The following
			// break/case facts are siblings in the switch body, so leave the
			// current block live and let the enclosing compound frame continue
			// the source-ordered fallthrough.
			if (!enter_recovery_frame_continuation(parent))
				return;
			continue;
		}
		if (parent_fact.kind == SemanticFactKind::SwitchStatement)
		{
			if (child_index != 1)
				throw std::runtime_error("PA15 label continuation is not a switch body");
			const SwitchContext* flow = remembered_switch_flow(parent);
			if (flow == NULL)
				throw std::runtime_error("PA15 label continuation switch flow is missing");
			if (current_block_ != InvalidIdentityValue &&
				current_block_id() != flow->end_target && !terminated(block()))
				emit_jump(flow->end_target);
			set_current(flow->end_target);
			if (!enter_recovery_frame_continuation(parent))
				return;
			pop_label_recovery_control(parent);
			continue;
		}
		if (parent_fact.kind == SemanticFactKind::ThenBranch ||
			parent_fact.kind == SemanticFactKind::ElseBranch ||
			parent_fact.kind == SemanticFactKind::Condition ||
			parent_fact.kind == SemanticFactKind::ConditionDeclaration ||
			parent_fact.kind == SemanticFactKind::Iteration ||
			parent_fact.kind == SemanticFactKind::ForInitStatement)
			continue;
		throw std::runtime_error("PA15 unsupported label continuation frame");
	}
	if (!child.valid() || child != label_recovery_root_)
		throw std::runtime_error("PA15 label continuation ancestry is incomplete");
}

void Pa15Lowerer::lower_existing_switch_label(SemanticFactId id){
	if (!id.valid() || id.value >= model_.semantic_facts_.size())
		throw std::runtime_error("PA15 existing switch label fact is invalid");
	const SemanticFactKind kind = model_.semantic_facts_[id.value].kind;
	if (kind != SemanticFactKind::CaseStatement &&
		kind != SemanticFactKind::DefaultStatement)
		throw std::runtime_error("PA15 existing switch label kind is invalid");
	if (!switch_stack_.empty())
	{
		const SwitchContext& active = switch_stack_.back();
		if (active.labels.find(id.value) == active.labels.end())
			throw std::runtime_error("PA15 active switch label arm is missing");
		if (active.lowered_labels.find(id.value) == active.lowered_labels.end())
		{
			lower_switch_label_recovery(id);
			return;
		}
	}
	const SemanticFactId switch_fact = enclosing_switch_fact(id);
	const SwitchContext* flow = remembered_switch_flow(switch_fact);
	if (flow == NULL)
		throw std::runtime_error("PA15 existing switch label flow is missing");
	// The persistent fact-keyed map is the canonical typed owner even when
	// the original switch_stack_ has already been popped by recovery.  Do not
	// rescan all arms for every recovered label.
	const std::map<std::size_t, BlockId>::const_iterator arm =
		flow->labels.find(id.value);
	if (arm == flow->labels.end() || !arm->second.valid())
		throw std::runtime_error("PA15 existing switch label arm is missing");
	const BlockId arm_target = arm->second;
	if (current_block_ != InvalidIdentityValue &&
		current_block_id() != arm_target && !terminated(block()))
		emit_jump(arm_target);
	set_current(arm_target);
	if (terminated(arm_target)) current_block_ = InvalidIdentityValue;
}

void Pa15Lowerer::drain_label_recovery_queue(SemanticFactId boundary,
	std::size_t resume_child){
	if (!boundary.valid())
	{
		while (!label_recovery_queue_.empty())
			drain_label_recovery_queue(label_recovery_queue_.begin()->first,
				InvalidIdentityValue);
		return;
	}
	while (true)
	{
		std::map<SemanticFactId, std::vector<LabelRecoveryWork> >::iterator found =
			label_recovery_queue_.find(boundary);
		if (found == label_recovery_queue_.end() || found->second.empty())
		{
			if (found != label_recovery_queue_.end())
				label_recovery_queue_.erase(found);
			return;
		}
		const LabelRecoveryWork work = found->second.back();
		found->second.pop_back();
		if (found->second.empty()) label_recovery_queue_.erase(found);
		if (!work.label.valid() || work.label.value >= label_statement_facts_.size() ||
			work.label_fact != label_statement_facts_[work.label.value] ||
			work.queue_boundary != boundary)
			throw std::runtime_error("PA15 deferred label work is invalid");
		if (label_lowered_generations_[work.label.value] == label_generation_ &&
			label_lowered_[work.label.value] != 0)
			continue;
		if (!current_label_target(work.label))
			throw std::runtime_error("PA15 deferred label target is missing");
		const BlockId saved_current = current_block_id();
		std::size_t saved_control_depth = 0;
		const RecoveryControlIndex saved_recovery_control_head =
			recovery_control_head_;
		const std::size_t saved_recovery_control_base_depth =
			recovery_control_base_depth_;
		const bool saved_recovery_control_active = recovery_control_active_;
		push_label_recovery_controls(work, &saved_control_depth);
		set_current(label_blocks_[work.label.value]);
		lower_statement(work.label_fact);
		if (current_block_ != InvalidIdentityValue)
			lower_label_recovery_continuation(work, boundary, resume_child);
		control_stack_.resize(saved_control_depth);
		recovery_control_head_ = saved_recovery_control_head;
		recovery_control_base_depth_ = saved_recovery_control_base_depth;
		recovery_control_active_ = saved_recovery_control_active;
		if (saved_current.valid()) set_current(saved_current);
	}
}

void Pa15Lowerer::lower_existing_label_control(SemanticFactId id){
	if (!id.valid() || id.value >= model_.semantic_facts_.size())
		throw std::runtime_error("PA15 existing label control fact is invalid");
	const SemanticFact& fact = model_.semantic_facts_[id.value];
	const std::vector<SemanticFactId> facts = children(id);
	if (fact.kind == SemanticFactKind::WhileStatement ||
		fact.kind == SemanticFactKind::DoStatement ||
		fact.kind == SemanticFactKind::ForStatement)
	{
		const LoopFlow* flow = remembered_loop_flow(id);
		if (flow == NULL)
			throw std::runtime_error("PA15 existing label loop flow is missing");
		if (fact.kind == SemanticFactKind::ForStatement)
		{
			if (facts.empty())
				throw std::runtime_error("PA15 existing label for fact is empty");
			lower_statement(facts.front());
		}
		const BlockId entry = fact.kind == SemanticFactKind::DoStatement ?
			flow->body : flow->condition;
		if (current_block_ != InvalidIdentityValue &&
			current_block_id() != entry && !terminated(block()))
			emit_jump(entry);
		set_current(flow->end);
		return;
	}
	if (fact.kind == SemanticFactKind::SwitchStatement)
	{
		if (facts.empty())
			throw std::runtime_error("PA15 existing label switch fact is empty");
		const SwitchContext* flow = remembered_switch_flow(id);
		if (flow == NULL)
			throw std::runtime_error("PA15 existing label switch flow is missing");
		const LoweredValue selector = lower_condition(facts.front());
		const BlockId dispatch = block_id(new_block("switch_resume"));
		if (current_block_ != InvalidIdentityValue &&
			current_block_id() != dispatch && !terminated(block()))
			emit_jump(dispatch);
		set_current(dispatch);
		Instruction instruction;
		instruction.kind = Instruction::IK_SWITCH;
		instruction.first = selector.value;
		instruction.second = block_operand(flow->default_target);
		for (std::size_t i = 0; i < flow->arms.size(); ++i)
		{
			if (flow->arms[i].is_default) continue;
			instruction.args.push_back(flow->arms[i].value);
			instruction.args.push_back(block_operand(flow->arms[i].target));
		}
		block().instructions.push_back(instruction);
		const BlockId source = current_block_id();
		propagate_edge(source, flow->default_target);
		for (std::size_t i = 0; i < flow->arms.size(); ++i)
			if (!flow->arms[i].is_default)
				propagate_edge(source, flow->arms[i].target);
		set_current(flow->end_target);
		return;
	}
	if (fact.kind == SemanticFactKind::IfStatement)
	{
		if (facts.size() < 2 || facts.size() > 3)
			throw std::runtime_error("PA15 existing label if fact is invalid");
		const IfFlow* flow = remembered_if_flow(id);
		if (flow == NULL)
			throw std::runtime_error("PA15 existing label if flow is missing");
		if (has_direct_short_circuit(facts.front()))
			lower_condition_branch(facts.front(), flow->then_target,
				flow->else_target);
		else
		{
			const LoweredValue condition = lower_condition(facts.front());
			emit_branch(condition.value, flow->then_target, flow->else_target);
		}
		set_current(flow->join);
		return;
	}
	throw std::runtime_error("PA15 unsupported existing label control");
}

void Pa15Lowerer::initialize_label_flow(SemanticFactId body){
	begin_label_flow();
	label_recovery_root_ = body;
	collect_label_flow(body);
	compute_label_subtree(body);
}

void Pa15Lowerer::collect_label_flow(SemanticFactId id,
	SemanticFactId parent, std::size_t parent_index){
	if (!id.valid() || id.value >= model_.semantic_facts_.size())
		throw std::runtime_error("PA15 label prepass fact is invalid");
	// A default argument or another typed expression may be shared by several
	// call sites.  Its first structural path is sufficient for label recovery;
	// a second completed occurrence must not turn ordinary DAG sharing into a
	// parent conflict.  An active occurrence, however, is a real cycle.
	if (fact_index_generations_[id.value] == label_generation_)
	{
		if (label_index_states_[id.value] == 2)
			return;
		if (label_index_states_[id.value] == 1)
			throw std::runtime_error("PA15 semantic fact cycle in label prepass");
	}
	fact_parents_[id.value] = parent;
	fact_parent_indexes_[id.value] = parent_index;
	fact_switch_ancestors_[id.value] = SemanticFactId();
	fact_recovery_control_heads_[id.value] = RecoveryControlIndex();
	if (parent.valid())
	{
		if (parent.value >= model_.semantic_facts_.size() ||
			parent.value >= fact_switch_ancestors_.size() ||
			parent.value >= fact_recovery_control_heads_.size() ||
			fact_index_generations_[parent.value] != label_generation_)
			throw std::runtime_error("PA15 label prepass parent is invalid");
		fact_recovery_control_heads_[id.value] =
			fact_recovery_control_heads_[parent.value];
		fact_switch_ancestors_[id.value] =
			fact_switch_ancestors_[parent.value];
		const SemanticFactKind parent_kind =
			model_.semantic_facts_[parent.value].kind;
		if (label_recovery_loop_kind(parent_kind) ||
			label_recovery_switch_kind(parent_kind))
		{
			const RecoveryControlIndex inherited_control =
				fact_recovery_control_heads_[id.value];
			RecoveryControlIndex continue_frame;
			if (label_recovery_switch_kind(parent_kind) && inherited_control.valid())
			{
				if (inherited_control.value >= recovery_control_arena_.size())
					throw std::runtime_error(
						"PA15 label prepass control chain is invalid");
				continue_frame =
					recovery_control_arena_[inherited_control.value].continue_frame;
			}
			recovery_control_arena_.push_back(RecoveryControlNode(parent,
				inherited_control, continue_frame));
			fact_recovery_control_heads_[id.value] =
				RecoveryControlIndex(recovery_control_arena_.size() - 1);
			if (label_recovery_loop_kind(parent_kind))
				recovery_control_arena_.back().continue_frame =
					fact_recovery_control_heads_[id.value];
			if (label_recovery_switch_kind(parent_kind))
				fact_switch_ancestors_[id.value] = parent;
		}
	}
	if (fact_index_generations_[id.value] != label_generation_)
	{
		fact_index_generations_[id.value] = label_generation_;
		label_index_states_[id.value] = 0;
	}
	unsigned char& state = label_index_states_[id.value];
	if (state == 2) return;
	if (state == 1)
		throw std::runtime_error("PA15 semantic fact cycle in label prepass");
	state = 1;
	fact_recovery_orders_[id.value] = label_recovery_order_++;
	if (parent.valid())
	{
		const SemanticFact& parent_fact = model_.semantic_facts_[parent.value];
		if (parent_index >= parent_fact.child_count)
			throw std::runtime_error("PA15 label prepass parent index is invalid");
		const bool structural_parent =
			label_recovery_loop_kind(parent_fact.kind) ||
			parent_fact.kind == SemanticFactKind::IfStatement ||
			parent_fact.kind == SemanticFactKind::CaseStatement ||
			parent_fact.kind == SemanticFactKind::DefaultStatement ||
			parent_fact.kind == SemanticFactKind::SwitchStatement ||
			(parent_fact.kind == SemanticFactKind::CompoundStatement &&
				parent_index + 1 < parent_fact.child_count);
		if (structural_parent)
		{
			fact_recovery_frames_[id.value] = parent;
			fact_recovery_frame_children_[id.value] = id;
			fact_recovery_frame_indexes_[id.value] = parent_index;
		}
		else if (parent.value < fact_recovery_frames_.size())
		{
			fact_recovery_frames_[id.value] =
				fact_recovery_frames_[parent.value];
			fact_recovery_frame_children_[id.value] =
				fact_recovery_frame_children_[parent.value];
			fact_recovery_frame_indexes_[id.value] =
				fact_recovery_frame_indexes_[parent.value];
		}
	}
	const SemanticFact& fact = model_.semantic_facts_[id.value];
	if (fact.kind == SemanticFactKind::GotoStatement)
	{
		if (!fact.label.valid() || fact.label.value >= label_referenced_.size())
			throw std::runtime_error("PA15 goto has invalid label identity");
		if (label_referenced_generations_[fact.label.value] != label_generation_)
		{
			label_referenced_generations_[fact.label.value] = label_generation_;
			label_referenced_[fact.label.value] = 0;
		}
		label_referenced_[fact.label.value] = 1;
	}
	else if (fact.kind == SemanticFactKind::LabeledStatement)
	{
		if (!fact.label.valid() || fact.label.value >= label_referenced_.size())
			throw std::runtime_error("PA15 label has invalid label identity");
		if (label_statement_facts_[fact.label.value].valid() &&
			label_statement_facts_[fact.label.value] != id)
			throw std::runtime_error("PA15 label fact identity is duplicated");
		label_statement_facts_[fact.label.value] = id;
	}
	if (fact.child_count != 0)
	{
		if (fact.child_begin == InvalidIdentityValue ||
			fact.child_begin + fact.child_count >
				model_.semantic_children_.size())
			throw std::runtime_error("PA15 invalid label prepass child range");
		for (std::size_t i = 0; i < fact.child_count; ++i)
			collect_label_flow(
				model_.semantic_children_[fact.child_begin + i], id, i);
	}
	fact_recovery_ends_[id.value] = label_recovery_order_;
	state = 2;
}

bool Pa15Lowerer::compute_label_subtree(SemanticFactId id){
		if (!id.valid() || id.value >= model_.semantic_facts_.size())
			throw std::runtime_error("PA15 label subtree fact is invalid");
		if (fact_subtree_generations_[id.value] != label_generation_)
		{
			fact_subtree_generations_[id.value] = label_generation_;
			label_subtree_states_[id.value] = 0;
		}
		unsigned char& state = label_subtree_states_[id.value];
		if (state == 2) return label_subtrees_[id.value] != 0;
		if (state == 1)
			throw std::runtime_error("PA15 semantic fact cycle in label subtree");
		state = 1;
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		bool found = false;
		if (fact.kind == SemanticFactKind::LabeledStatement)
		{
			if (!fact.label.valid() ||
				fact.label.value >= label_referenced_.size())
				throw std::runtime_error("PA15 label has invalid subtree identity");
			found = current_label_referenced(fact.label);
		}
		if (fact.child_count != 0)
		{
			if (fact.child_begin == InvalidIdentityValue ||
				fact.child_begin + fact.child_count >
					model_.semantic_children_.size())
				throw std::runtime_error("PA15 invalid label subtree child range");
			for (std::size_t i = 0; i < fact.child_count; ++i)
				if (compute_label_subtree(
					model_.semantic_children_[fact.child_begin + i]))
					found = true;
		}
		label_subtrees_[id.value] = found ? 1 : 0;
		state = 2;
		return found;
	}

bool Pa15Lowerer::referenced_label_subtree(SemanticFactId id) const{
	if (!id.valid() || id.value >= label_subtrees_.size())
			throw std::runtime_error("PA15 label subtree identity is invalid");
		return fact_subtree_generations_[id.value] == label_generation_ &&
			label_subtrees_[id.value] != 0;
	}

void Pa15Lowerer::lower_referenced_label_subtree(SemanticFactId id){
		if (!referenced_label_subtree(id)) return;
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::LabeledStatement)
		{
			if (!fact.label.valid() || fact.label.value >= label_blocks_.size())
				throw std::runtime_error("PA15 referenced label is invalid");
			if (!current_label_target(fact.label))
			{
				defer_label_recovery(fact.label);
				return;
			}
			lower_statement(id);
			return;
		}
		if (current_block_ == InvalidIdentityValue &&
			(fact.kind == SemanticFactKind::WhileStatement ||
			 fact.kind == SemanticFactKind::DoStatement ||
			 fact.kind == SemanticFactKind::ForStatement))
		{
			if (fact.kind == SemanticFactKind::WhileStatement)
				lower_while(id);
			else if (fact.kind == SemanticFactKind::DoStatement)
				lower_do(id);
			else
				lower_for(id);
			return;
		}
		if (current_block_ == InvalidIdentityValue &&
			fact.kind == SemanticFactKind::SwitchStatement)
		{
			lower_switch(id, children(id));
			return;
		}
		if (current_block_ == InvalidIdentityValue &&
			fact.kind == SemanticFactKind::IfStatement)
		{
			lower_if(id, children(id));
			return;
		}
	const std::vector<SemanticFactId> facts = children(id);
	if (fact.kind == SemanticFactKind::CompoundStatement)
	{
		push_label_recovery_boundary(id);
		for (std::size_t i = 0; i < facts.size(); ++i)
		{
			if (current_block_ == InvalidIdentityValue)
			{
				if (referenced_label_subtree(facts[i]))
					lower_referenced_label_subtree(facts[i]);
			}
			else
				lower_statement(facts[i]);
			drain_label_recovery_queue(id, i + 1);
		}
		pop_label_recovery_boundary();
		return;
		}
		if (fact.kind == SemanticFactKind::CaseStatement)
		{
			if (facts.size() != 2) throw std::runtime_error("PA15 invalid case recovery");
			lower_referenced_label_subtree(facts.back());
			return;
		}
		if (fact.kind == SemanticFactKind::DefaultStatement)
		{
			if (facts.size() != 1) throw std::runtime_error("PA15 invalid default recovery");
			lower_referenced_label_subtree(facts.front());
			return;
		}
		if (fact.kind == SemanticFactKind::SwitchStatement && facts.size() == 2)
		{
			lower_referenced_label_subtree(facts.back());
			return;
		}
		for (std::size_t i = 0; i < facts.size(); ++i)
		{
			if (!referenced_label_subtree(facts[i])) continue;
			if (current_block_ == InvalidIdentityValue)
				lower_referenced_label_subtree(facts[i]);
			else
				break;
		}
	}

} // namespace pa11_semantic_internal
