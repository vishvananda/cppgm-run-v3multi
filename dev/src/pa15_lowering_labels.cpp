#include "pa15_lowering.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

BlockId Pa15Lowerer::label_target(LabelId label){
		if (!label.valid())
			throw std::runtime_error("PA15 label has no semantic identity");
		if (label.value >= label_blocks_.size())
			throw std::runtime_error("PA15 label identity is out of range");
		BlockId& target = label_blocks_[label.value];
		if (target.valid()) return target;
		target = block_id(new_block("goto"));
		return target;
	}

void Pa15Lowerer::initialize_label_flow(SemanticFactId body){
		label_blocks_.assign(model_.label_facts_.size(), BlockId());
		label_referenced_.assign(model_.label_facts_.size(), 0);
		label_subtrees_.assign(model_.semantic_facts_.size(), 0);
		label_index_states_.assign(model_.semantic_facts_.size(), 0);
		label_subtree_states_.assign(model_.semantic_facts_.size(), 0);
		label_lowered_.assign(model_.label_facts_.size(), 0);
		collect_label_flow(body);
		compute_label_subtree(body);
	}

void Pa15Lowerer::collect_label_flow(SemanticFactId id){
		if (!id.valid() || id.value >= model_.semantic_facts_.size())
			throw std::runtime_error("PA15 label prepass fact is invalid");
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
			found = label_referenced_[fact.label.value] != 0;
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
		return label_subtrees_[id.value] != 0;
	}

void Pa15Lowerer::lower_referenced_label_subtree(SemanticFactId id){
		if (!referenced_label_subtree(id)) return;
		const SemanticFact& fact = model_.semantic_facts_[id.value];
		if (fact.kind == SemanticFactKind::LabeledStatement)
		{
			if (!fact.label.valid() || fact.label.value >= label_blocks_.size())
				throw std::runtime_error("PA15 referenced label is invalid");
			if (!label_blocks_[fact.label.value].valid()) return;
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
