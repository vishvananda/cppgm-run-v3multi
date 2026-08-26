#pragma once
#include "abi_mangle.h"
#include "lowir_model.h"
#include "pa11_semantic_model.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pa11_semantic_internal
{

using lowir_model::Block;
using lowir_model::BlockId;
using lowir_model::Function;
using lowir_model::GlobalDeclaration;
using lowir_model::GlobalDefinition;
using lowir_model::Instruction;
using lowir_model::LowType;
using lowir_model::Operand;
using lowir_model::FunctionDeclaration;
using lowir_model::Parameter;
using lowir_model::Program;
using lowir_model::SpellingId;
using lowir_model::SymbolId;
using lowir_model::ValueId;

struct LoweredValue
{
	Operand value;
	// The semantic/value type may differ from the type physically carried by
	// the LowIR operand after a compare or a conversion.  Keep both explicitly
	// so consumers never need to rescan the current block for a producer.
	LowType type;
	LowType physical_type;
	bool lvalue;
	bool canonical_truth;
	Operand condition_value;
	bool has_condition_value;

	LoweredValue() : value(), type(), physical_type(), lvalue(false),
		canonical_truth(false), condition_value(), has_condition_value(false) {}
	LoweredValue(const Operand& value, const LowType& type, bool lvalue)
		: value(value), type(type), physical_type(type), lvalue(lvalue),
		canonical_truth(false), condition_value(), has_condition_value(false) {}
	LoweredValue(const Operand& value, const LowType& type, bool lvalue,
		const LowType& physical_type)
		: value(value), type(type), physical_type(physical_type), lvalue(lvalue),
		canonical_truth(false), condition_value(), has_condition_value(false) {}
};

struct FunctionPlan
{
	std::size_t fact_index;
	std::size_t program_index;

	FunctionPlan(std::size_t fact_index = 0, std::size_t program_index = 0)
		: fact_index(fact_index), program_index(program_index) {}
};

struct PendingGlobalInitializer
{
	SymbolId global;
	SymbolId target;
	Operand index;
	LowType element_type;

	PendingGlobalInitializer(SymbolId global = SymbolId(),
		SymbolId target = SymbolId(), const Operand& index = Operand(),
		const LowType& element_type = LowType())
		: global(global), target(target), index(index), element_type(element_type) {}
};

struct LabelRecoveryWork
{
	// The parent/index ancestry is shared by fact identity.  Keeping only
	// typed cursors here avoids copying one vector for every deferred label.
	LabelId label;
	SemanticFactId label_fact;
	SemanticFactId queue_boundary;

	LabelRecoveryWork(LabelId label = LabelId(),
		SemanticFactId label_fact = SemanticFactId(),
		SemanticFactId queue_boundary = SemanticFactId())
		: label(label), label_fact(label_fact), queue_boundary(queue_boundary) {}
};

struct ControlTarget
{
	bool loop;
	BlockId break_target;
	BlockId continue_target;

	ControlTarget(bool loop = false, BlockId break_target = BlockId(),
		BlockId continue_target = BlockId())
		: loop(loop), break_target(break_target), continue_target(continue_target) {}
};

struct LoopFlowIndexTag {};
struct IfFlowIndexTag {};
struct SwitchFlowIndexTag {};
struct ContinuationIndexTag {};
struct RecoveryControlIndexTag {};

template <typename Tag>
struct FlowArenaIndex
{
	std::size_t value;

	explicit FlowArenaIndex(
		std::size_t value = pa11_semantic_storage::InvalidIdentityValue)
		: value(value) {}
	bool valid() const{
		return value != pa11_semantic_storage::InvalidIdentityValue;
	}
};

typedef FlowArenaIndex<LoopFlowIndexTag> LoopFlowIndex;
typedef FlowArenaIndex<IfFlowIndexTag> IfFlowIndex;
typedef FlowArenaIndex<SwitchFlowIndexTag> SwitchFlowIndex;
typedef FlowArenaIndex<ContinuationIndexTag> ContinuationIndex;
typedef FlowArenaIndex<RecoveryControlIndexTag> RecoveryControlIndex;

// Persistent typed control ancestry for a recovered label.  The parent link
// points toward the enclosing control, so labels in the same structural
// region share the already-built suffix instead of copying an ancestry path.
struct RecoveryControlNode
{
	SemanticFactId fact;
	RecoveryControlIndex parent;
	RecoveryControlIndex continue_frame;

	RecoveryControlNode(SemanticFactId fact = SemanticFactId(),
		RecoveryControlIndex parent = RecoveryControlIndex(),
		RecoveryControlIndex continue_frame = RecoveryControlIndex())
		: fact(fact), parent(parent), continue_frame(continue_frame) {}
};

struct CompoundContinuation
{
	BlockId entry;
	unsigned char state;

	CompoundContinuation(BlockId entry = BlockId(), unsigned char state = 0)
		: entry(entry), state(state) {}
};

struct LoopFlow
{
	bool valid;
	SemanticFactKind kind;
	BlockId condition;
	BlockId body;
	BlockId iteration;
	BlockId end;

	LoopFlow(SemanticFactKind kind = SemanticFactKind::WhileStatement,
		BlockId condition = BlockId(), BlockId body = BlockId(),
		BlockId iteration = BlockId(), BlockId end = BlockId())
		: valid(condition.valid() && body.valid() && end.valid()), kind(kind),
		  condition(condition), body(body), iteration(iteration), end(end) {}
};

struct IfFlow
{
	bool valid;
	BlockId then_target;
	BlockId else_target;
	BlockId join;

	IfFlow(BlockId then_target = BlockId(), BlockId else_target = BlockId(),
		BlockId join = BlockId())
		: valid(then_target.valid() && else_target.valid() && join.valid()),
		  then_target(then_target), else_target(else_target), join(join) {}
};

struct SwitchArm
{
	SemanticFactId fact;
	BlockId target;
	bool is_default;
	Operand value;

	SwitchArm(SemanticFactId fact = SemanticFactId(),
		BlockId target = BlockId(), bool is_default = false,
		const Operand& value = Operand())
		: fact(fact), target(target), is_default(is_default), value(value) {}
};

struct SwitchContext
{
	BlockId dispatch_target;
	BlockId end_target;
	BlockId default_target;
	std::vector<SwitchArm> arms;
	std::map<std::size_t, BlockId> labels;
	std::set<std::size_t> lowered_labels;
	std::set<std::size_t> label_subtrees;

	SwitchContext(BlockId end_target = BlockId(), BlockId dispatch_target = BlockId())
		: dispatch_target(dispatch_target), end_target(end_target),
		  default_target(end_target), arms(), labels(),
		  lowered_labels(), label_subtrees() {}
};

class Pa15Lowerer
{
public:
	Pa15Lowerer(const PA11SemanticModel& model, Program& program);
	void run();
private:
	const PA11SemanticModel& model_;
	Program& program_;
	std::map<std::string, SpellingId> spelling_ids_;
	std::set<std::string> used_symbols_;
	std::set<std::string> used_slot_names_;
	std::set<std::string> used_value_names_;
	std::map<std::string, std::size_t> symbol_collision_counters_;
	std::map<std::string, std::size_t> slot_collision_counters_;
	std::map<std::size_t, SymbolId> function_symbols_;
	std::map<std::size_t, SpellingId> function_name_ids_;
	std::map<std::size_t, FunctionDeclaration> function_declaration_plans_;
	std::set<std::size_t> demanded_function_declarations_;
	std::map<std::size_t, SymbolId> global_symbols_;
	std::map<std::size_t, SpellingId> global_name_ids_;
	std::map<std::size_t, SpellingId> symbol_name_ids_;
	std::map<std::size_t, SymbolId> literal_address_symbols_;
	std::vector<BlockId> label_blocks_;
	std::vector<unsigned char> label_referenced_;
	std::vector<unsigned char> label_subtrees_;
	std::vector<unsigned char> label_index_states_;
	std::vector<unsigned char> label_subtree_states_;
	std::vector<unsigned char> label_lowered_;
	std::vector<std::uint32_t> label_block_generations_;
	std::vector<std::uint32_t> label_referenced_generations_;
	std::vector<std::uint32_t> fact_index_generations_;
	std::vector<std::uint32_t> fact_subtree_generations_;
	std::vector<std::uint32_t> label_lowered_generations_;
	std::vector<std::uint32_t> label_recovery_waiting_generations_;
	std::vector<std::uint32_t> label_recovery_queued_generations_;
	std::vector<SemanticFactId> label_statement_facts_;
	std::vector<SemanticFactId> fact_parents_;
	std::vector<std::size_t> fact_parent_indexes_;
	std::vector<SemanticFactId> fact_recovery_frames_;
	std::vector<SemanticFactId> fact_recovery_frame_children_;
	std::vector<std::size_t> fact_recovery_frame_indexes_;
	std::vector<std::size_t> fact_recovery_orders_;
	std::vector<std::size_t> fact_recovery_ends_;
	std::vector<SemanticFactId> fact_switch_ancestors_;
	std::vector<RecoveryControlIndex> fact_recovery_control_heads_;
	std::vector<RecoveryControlNode> recovery_control_arena_;
	SemanticFactId label_recovery_root_;
	std::size_t label_recovery_order_;
	std::vector<SemanticFactId> label_recovery_boundaries_;
	std::map<SemanticFactId, std::vector<LabelRecoveryWork> >
		label_recovery_queue_;
	std::uint32_t label_generation_;
	RecoveryControlIndex recovery_control_head_;
	std::size_t recovery_control_base_depth_;
	bool recovery_control_active_;
	std::map<std::size_t, SemanticFactId> variable_facts_;
	std::map<std::size_t, const DeclarationFact*> declaration_by_binding_;
	std::map<std::size_t, lowir_model::SlotId> slot_by_binding_;
	std::vector<SpellingId> slot_spellings_;
	std::vector<FunctionPlan> function_plans_;
	std::vector<PendingGlobalInitializer> pending_global_initializers_;
	bool needs_trivial_namespace_object_init_;
	std::vector<std::vector<BindingId> > function_scope_variables_;
	std::size_t next_symbol_;
	std::size_t literal_backing_ordinal_;
	std::size_t next_value_;
	std::size_t next_slot_;
	std::size_t next_block_;
	std::size_t current_function_;
	std::size_t current_block_;
	std::size_t temp_ordinal_;
	std::size_t block_ordinal_;
	std::size_t generated_slot_ordinal_;
	std::map<std::size_t, std::size_t> block_indexes_;
	std::vector<ControlTarget> control_stack_;
	std::vector<SwitchContext> switch_stack_;
	// Fact-domain indexes stay compact; heavyweight flow records exist only
	// for facts of the corresponding kind in their typed sparse arenas.
	std::vector<LoopFlowIndex> loop_flow_indexes_;
	std::vector<LoopFlow> loop_flow_arena_;
	std::vector<IfFlowIndex> if_flow_indexes_;
	std::vector<IfFlow> if_flow_arena_;
	std::vector<SwitchFlowIndex> switch_flow_indexes_;
	std::vector<SwitchContext> switch_flow_arena_;
	// A continuation is keyed by the typed fact at the cursor's first child;
	// no source spelling or per-label ancestry copy participates in lookup.
	std::vector<ContinuationIndex> continuation_indexes_;
	std::vector<CompoundContinuation> continuation_arena_;
	// Structural exits use the same typed continuation arena.  A frame's
	// entry is installed once, then later label paths jump to it directly.
	std::vector<ContinuationIndex> fact_recovery_exit_indexes_;
	std::vector<BlockId> block_order_;
	std::set<std::size_t> ordered_block_ids_;
	std::size_t reachability_base_;
	std::vector<unsigned char> reachable_blocks_;
	std::vector<BlockId> reachability_work_;
	std::vector<unsigned char> constant_truth_cache_;
	void initialize_spelling_ids();
	void initialize_identity_counters();
	const std::string& spelling(SpellingId id) const;
	SpellingId intern_spelling(const std::string& value);
	SpellingId symbol_spelling(const std::string& name);
	std::vector<std::string> function_components(const FunctionFact& fact) const;
	std::vector<std::string> value_components(ScopeId owner, NameId name) const;
	std::vector<std::string> function_abi_components(BindingId binding_id,
		ScopeId owner) const;
	abi_mangle::AbiOperatorTerminalKind operator_terminal(
		BindingId binding_id, std::size_t parameter_count) const;
	std::string abi_variable_symbol(BindingId binding_id, ScopeId owner) const;
	std::vector<std::string> named_type_components(NamedRecordId record) const;
	abi_mangle::AbiType abi_type_nested(TypeId type) const;
	abi_mangle::AbiType abi_type(TypeId type) const;
	std::string abi_symbol(const FunctionFact& fact) const;
	std::string abi_function_symbol(BindingId binding_id, ScopeId owner) const;
	LowType low_type(TypeId type) const;
	LowType function_result_low_type(TypeId type) const;
	bool class_object_type(TypeId type) const;
	LowType low_reference_value_type(TypeId type) const;
	void index_binding_facts();
	bool constant_integer(SemanticFactId id, const LowType& type, Operand* result);
	bool typed_pointer_zero(SemanticFactId id, TypeId destination) const;
	bool map_constant_address(SemanticFactId id, SymbolId* target,
		long long* addend, const ConstantAddressFact** relocation);
	std::string internal_value_name(ScopeId owner, NameId name) const;
	SpellingId symbol_name_for(SymbolId target) const;
	void append_array_data(GlobalDefinition* global, TypeId array_type,
		const std::vector<SemanticFactId>& initializers, ScopeId scope);
	void collect_globals();
	void materialize_pending_global_initializers();
	void index_function_scope_variables();
	void collect_functions();
	void collect_function_declarations();
	void demand_function_declaration(BindingId binding);
	void materialize_function_declarations();
	SpellingId parameter_value_name(NameId name, std::size_t ordinal);
	SpellingId slot_name(NameId name, std::size_t ordinal, bool shadowed);
	void collect_local_slots(Function& function, ScopeId scope,
		std::map<std::size_t, std::size_t>* active_names);
	void add_slot(Function& function, BindingId binding, TypeId semantic_type,
	              SpellingId name_id, const LowType& type);
	ValueId allocate_value();
	void clear_value_records();
	void claim_value(ValueId id, SymbolId owner,
		const lowir_model::Parameter* parameter,
		const lowir_model::Instruction* instruction,
		lowir_model::ValueRecord::ProducerKind producer);
	void finalize_value_records();
	Function& function();
	const Function& function() const;
	Block& block();
	bool terminated(const Block& current) const;
	bool terminated(BlockId id) const;
	void reorder_condition_blocks(std::size_t begin, std::size_t destination_count,
		BlockId saved_current);
	SpellingId temporary_name();
	std::size_t new_block(const std::string& base);
	void rebuild_block_indexes();
	std::size_t reachability_index(BlockId id) const;
	bool is_reachable(BlockId id) const;
	BlockId edge_target(const Operand& operand) const;
	void enqueue_reachable(BlockId id);
	void propagate_existing_terminator_edges(BlockId source);
	void mark_reachable(BlockId start);
	void propagate_edge(BlockId source, BlockId target);
	void store_loop_flow(SemanticFactId id, const LoopFlow& flow);
	const LoopFlow* remembered_loop_flow(SemanticFactId id) const;
	void store_if_flow(SemanticFactId id, const IfFlow& flow);
	const IfFlow* remembered_if_flow(SemanticFactId id) const;
	void store_switch_flow(SemanticFactId id, const SwitchContext& flow);
	const SwitchContext* remembered_switch_flow(SemanticFactId id) const;
	bool enter_shared_compound_cursor(SemanticFactId parent,
		std::size_t child_index);
	bool enter_recovery_frame_continuation(SemanticFactId frame);
	void finish_shared_compound_cursor(SemanticFactId parent,
		std::size_t child_index);
	bool jump_to_cached_label_continuation(SemanticFactId label_fact);
	SemanticFactId enclosing_switch_fact(SemanticFactId id) const;
	BlockId block_id(std::size_t index) const;
	std::size_t block_index(BlockId id) const;
	void set_current(BlockId id);
	BlockId current_block_id() const;
	void reorder_function_blocks();
	Operand temporary_operand(ValueId id, SpellingId name) const;
	Operand slot_operand(lowir_model::SlotId id) const;
	Operand global_operand(SymbolId id, SpellingId name) const;
	Operand block_operand(std::size_t index) const;
	Operand block_operand(BlockId id) const;
	Operand integer_operand(long long value, const LowType& type) const;
	Operand floating_operand(long double value, const LowType& type) const;
	ValueId destination(const LowType& type, Instruction* instruction);
	ValueId emit_load(const LoweredValue& storage, const LowType& type);
	void materialize_lvalue_value(LoweredValue* result, const LowType& type);
	void emit_store(const LowType& type, const Operand& value, const Operand& storage);
	LoweredValue address_of_storage(const LoweredValue& storage);
	LoweredValue emit_index(const LoweredValue& base, const LoweredValue& offset,
		const LowType& element, bool array_projection);
	LoweredValue emit_decay(const LoweredValue& address);
	LoweredValue storage_for(BindingId binding) const;
	std::vector<SemanticFactId> children(SemanticFactId id) const;
	LowType lvalue_type(SemanticFactId id) const;
	bool reference_binding(BindingId binding) const;
	LoweredValue generated_slot(const LowType& type, const std::string& prefix);
	LoweredValue lower_lvalue(SemanticFactId id);
	LowType size_low_type() const;
	LoweredValue lower_sizeof(const SemanticFact& fact);
	LoweredValue literal(const SemanticFact& fact);
	LoweredValue apply_reinterpret_conversion(LoweredValue result,
		const LowType& target);
	LoweredValue apply_conversions(SemanticFactId id, LoweredValue result,
		bool omit_boolean_context = false, bool materialize_lvalue = true,
		bool force_integral_literal_conversion = false);
	bool apply_structural_conversion(LoweredValue* result,
		const ConversionFact& conversion, const LowType& target,
		bool omit_boolean_context, bool materialize_lvalue);
	LoweredValue apply_pointer_conversion(LoweredValue result,
		const ConversionFact& conversion, const LowType& target);
	LoweredValue apply_integral_literal_conversion(
		const LoweredValue& result, const ConversionFact& conversion,
		const LowType& source_type, const LowType& target,
		bool force_integral_literal_conversion);
	lowir_model::ConversionOperator conversion_operator(const ConversionFact& conversion) const;
	LoweredValue emit_binary_value(lowir_model::BinaryOperator operation,
		const LowType& type, const LoweredValue& left, const LoweredValue& right);
	LoweredValue emit_compare_value(lowir_model::ComparePredicate predicate,
		const LowType& type, const LoweredValue& left, const LoweredValue& right);
	LoweredValue integer_i64(const LoweredValue& source, TypeId source_type);
	std::size_t pointer_element_size(TypeId type) const;
	LoweredValue pointer_offset(const LoweredValue& base, TypeId base_type,
		const LoweredValue& amount, TypeId amount_type, bool negative);
	LoweredValue lower_incdec(SemanticFactId id, bool postfix);
	SimpleTokenType fact_token(SemanticFactId id) const;
	LoweredValue lower_assignment(SemanticFactId id, bool preserve_lvalue = false);
	LoweredValue lower_call(SemanticFactId id);
	bool conditional_address_result(SemanticFactId id) const;
	LoweredValue lower_conditional_address(SemanticFactId id);
	LoweredValue lower_conditional_value(SemanticFactId id);
	LoweredValue lower_address(SemanticFactId id);
	bool pointer_like(TypeId type) const;
	LoweredValue lower_logical(SemanticFactId id);
	void initialize_array(BindingId binding, SemanticFactId initializer,
		const LoweredValue& storage);
	LoweredValue lower_expression(SemanticFactId id);
	LoweredValue lower_condition_expression(SemanticFactId id);
	void lower_discarded_expression(SemanticFactId id);
	LoweredValue lower_binary_expression(SemanticFactId id);
	LoweredValue lower_expression_impl(SemanticFactId id,
		bool omit_boolean_context, bool materialize_lvalue = true,
		bool force_integral_literal_conversion = false,
		bool defer_conversions = false);
	bool is_comparison(SimpleTokenType token) const;
	lowir_model::ComparePredicate compare_predicate(SimpleTokenType token,
		bool is_unsigned) const;
	bool unsigned_type_for(TypeId type) const;
	lowir_model::BinaryOperator binary_operator(SimpleTokenType token,
		bool is_unsigned) const;
	void emit_jump(BlockId target);
	void emit_branch(const Operand& condition, BlockId true_target,
		BlockId false_target);
	void initialize_label_storage();
	void begin_label_flow();
	BlockId label_target(LabelId label);
	bool current_label_target(LabelId label) const;
	bool current_label_referenced(LabelId label) const;
	void push_label_recovery_boundary(SemanticFactId boundary);
	void pop_label_recovery_boundary();
	SemanticFactId label_recovery_boundary() const;
	void push_label_recovery_controls(const LabelRecoveryWork& work,
		std::size_t* saved_depth);
	void pop_label_recovery_control(SemanticFactId frame);
	void queue_label_recovery(LabelId label);
	void defer_label_recovery(LabelId label);
	void drain_label_recovery_queue(SemanticFactId boundary,
		std::size_t resume_child = InvalidIdentityValue);
	void lower_label_recovery_continuation(const LabelRecoveryWork& work,
		SemanticFactId boundary, std::size_t resume_child);
	void lower_existing_switch_label(SemanticFactId id);
	void lower_existing_label_control(SemanticFactId id);
	void initialize_label_flow(SemanticFactId body);
	void collect_label_flow(SemanticFactId id,
		SemanticFactId parent = SemanticFactId(),
		std::size_t parent_index = InvalidIdentityValue);
	bool compute_label_subtree(SemanticFactId id);
	bool referenced_label_subtree(SemanticFactId id) const;
	void lower_referenced_label_subtree(SemanticFactId id);
	bool condition_is_empty(SemanticFactId id) const;
	bool constant_truth(SemanticFactId id, bool* value);
	bool has_direct_short_circuit(SemanticFactId id) const;
	void lower_condition_branch(SemanticFactId id, BlockId true_target,
		BlockId false_target);
	BlockId control_target(bool continue_target) const;
	BlockId switch_label_target(SemanticFactId id);
	bool switch_label_was_lowered(SemanticFactId id) const;
	BlockId switch_label_existing_target(SemanticFactId id) const;
	void terminate_unreachable_block(BlockId id);
	bool lower_switch_label_recovery(SemanticFactId id);
	bool collect_switch_labels(SemanticFactId id, SwitchContext* context);
	bool switch_subtree_has_label(SemanticFactId id) const;
	void finish_switch_loop(const LoopFlow& target);
	void recover_existing_switch_loop(SemanticFactId body_fact,
		const LoopFlow& target);
	void lower_switch_while(SemanticFactId id);
	void lower_switch_do(SemanticFactId id);
	void lower_switch_for(SemanticFactId id);
	bool lower_switch_body(SemanticFactId id);
	void finish_switch_labels();
	void lower_switch(SemanticFactId id,
		const std::vector<SemanticFactId>& facts);
	void lower_while(SemanticFactId id);
	void lower_do(SemanticFactId id);
	void lower_for(SemanticFactId id);
	void lower_statement(SemanticFactId id);
	LoweredValue lower_condition(SemanticFactId id);
	void lower_if(SemanticFactId id,
		const std::vector<SemanticFactId>& facts);
	void lower_function(const FunctionPlan& plan);
};
}
