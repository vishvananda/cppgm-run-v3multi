#include "pa15_lowering.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

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
	label_generation_ = 0;
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
		label_generation_ = 1;
	}
	else
		++label_generation_;
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

void Pa15Lowerer::initialize_label_flow(SemanticFactId body){
	begin_label_flow();
	collect_label_flow(body);
	compute_label_subtree(body);
}

void Pa15Lowerer::collect_label_flow(SemanticFactId id){
		if (!id.valid() || id.value >= model_.semantic_facts_.size())
			throw std::runtime_error("PA15 label prepass fact is invalid");
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
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::GotoStatement)
		{
			if (!fact.label.valid() ||
				fact.label.value >= label_referenced_.size())
				throw std::runtime_error("PA15 goto has invalid label identity");
			if (label_referenced_generations_[fact.label.value] !=
				label_generation_)
			{
				label_referenced_generations_[fact.label.value] = label_generation_;
				label_referenced_[fact.label.value] = 0;
			}
			label_referenced_[fact.label.value] = 1;
		}
		else if (fact.kind == SemanticFactKind::LabeledStatement)
		{
			if (!fact.label.valid() ||
				fact.label.value >= label_referenced_.size())
				throw std::runtime_error("PA15 label has invalid label identity");
		}
		if (fact.child_count != 0)
		{
			if (fact.child_begin == InvalidIdentityValue ||
				fact.child_begin + fact.child_count >
					model_.semantic_children_.size())
				throw std::runtime_error("PA15 invalid label prepass child range");
			for (std::size_t i = 0; i < fact.child_count; ++i)
				collect_label_flow(model_.semantic_children_[fact.child_begin + i]);
		}
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
			if (!current_label_target(fact.label)) return;
			lower_statement(id);
			return;
		}
		const std::vector<SemanticFactId> facts = children(id);
		if (fact.kind == SemanticFactKind::CompoundStatement)
		{
			for (std::size_t i = 0; i < facts.size(); ++i)
			{
				if (current_block_ == InvalidIdentityValue)
				{
					if (referenced_label_subtree(facts[i]))
						lower_referenced_label_subtree(facts[i]);
					continue;
				}
				lower_statement(facts[i]);
			}
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
