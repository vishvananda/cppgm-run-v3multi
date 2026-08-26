#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

void PA11SemanticModel::collect_pa12_labels(const PA10AstNode& node,
	FunctionLabelTable& table)
{
	// Labels belong to the innermost function.  Nested function-like AST
	// regions are analyzed separately and must not contribute to this table.
	if (node.kind == PA10NodeKind::FunctionDefinition ||
		node.kind == PA10NodeKind::ClassSpecifier ||
		node.kind == PA10NodeKind::LambdaExpression)
		return;
	if (node.kind == PA10NodeKind::LabeledStatement)
	{
		if (node.producer_spelling == 0 || node.children.size() != 1)
			throw std::runtime_error("invalid PA12 labeled statement");
		const NameId name = name_from_spelling(node.producer_spelling);
		if (table.by_name.find(name) != NULL)
			throw std::runtime_error("PA12 duplicate label");
		const LabelId label(label_facts_.size());
		label_facts_.push_back(LabelFact(name, &node));
		table.by_name.set(name, label);
	}
	for (std::size_t i = 0; i < node.children.size(); ++i)
		collect_pa12_labels(node.children[i], table);
}

void PA11SemanticModel::prepare_pa12_labels(const PA10AstNode& body,
	FunctionFact& function)
{
	if (body.kind != PA10NodeKind::CompoundStatement)
		throw std::runtime_error("PA12 function body is not a compound");
	FunctionLabelTable table;
	collect_pa12_labels(body, table);
	function.label_table = LabelTableId(label_tables_.size());
	label_tables_.push_back(table);
}

LabelId PA11SemanticModel::label_for_name(const FunctionFact& function,
	NameId name) const
{
	if (!function.label_table.valid() ||
		function.label_table.value >= label_tables_.size())
		throw std::runtime_error("PA12 function label table is missing");
	const LabelId* found = label_tables_[function.label_table.value].by_name.find(
		name);
	return found == NULL ? LabelId() : *found;
}

SemanticFactId PA11SemanticModel::semantic_jump_statement(
	const PA10AstNode& node, const FunctionFact& function,
	unsigned int loop_depth, unsigned int switch_depth)
{
	if (node.kind == PA10NodeKind::GotoStatement)
	{
		if (node.producer_spelling == 0 || !node.children.empty())
			throw std::runtime_error("invalid PA12 goto statement");
		const NameId name = name_from_spelling(node.producer_spelling);
		const LabelId label = label_for_name(function, name);
		if (!label.valid())
			throw std::runtime_error("PA12 unresolved label");
		SemanticFact fact(SemanticFactKind::GotoStatement, TypeId(),
			SemanticValueCategory::Prvalue, &node);
		fact.label = label;
		const SemanticFactId result = make_semantic_fact(fact);
		NamePath path;
		path.components.push_back(name);
		set_semantic_name(result, path);
		return result;
	}
	if (node.kind == PA10NodeKind::BreakStatement)
	{
		if (loop_depth == 0 && switch_depth == 0)
			throw std::runtime_error("PA12 break outside loop or switch");
		return make_expression_fact(SemanticFactKind::BreakStatement, TypeId(),
			SemanticValueCategory::Prvalue, node,
			std::vector<SemanticFactId>());
	}
	if (loop_depth == 0)
		throw std::runtime_error("PA12 continue outside loop");
	return make_expression_fact(SemanticFactKind::ContinueStatement, TypeId(),
		SemanticValueCategory::Prvalue, node,
		std::vector<SemanticFactId>());
}

SemanticFactId PA11SemanticModel::semantic_label_statement(
	const PA10AstNode& node, ScopeId scope, const FunctionFact& function,
	unsigned int loop_depth, unsigned int switch_depth,
	SwitchValidationContext* switch_context)
{
	if (node.kind != PA10NodeKind::LabeledStatement ||
		node.producer_spelling == 0 || node.children.size() != 1)
		throw std::runtime_error("invalid PA12 labeled statement");
	const NameId name = name_from_spelling(node.producer_spelling);
	const LabelId label = label_for_name(function, name);
	if (!label.valid())
		throw std::runtime_error("PA12 unresolved label");
	const PA10AstNode& body_node = node.children.front();
	const ScopeId body = body_node.kind == PA10NodeKind::CompoundStatement ?
		compound_scope(body_node) : scope;
	if (!body.valid())
		throw std::runtime_error("PA12 labeled body scope is missing");
	const SemanticFactId body_fact = semantic_statement(body_node, body,
		function, loop_depth, switch_depth, switch_context);
	std::vector<SemanticFactId> children;
	if (body_fact.valid())
		children.push_back(body_fact);
	SemanticFact fact(SemanticFactKind::LabeledStatement, TypeId(),
		SemanticValueCategory::Prvalue, &node);
	fact.label = label;
	const SemanticFactId result = make_semantic_fact(fact);
	NamePath path;
	path.components.push_back(name);
	set_semantic_name(result, path);
	set_semantic_children(result, children);
	return result;
}

} // namespace pa11_semantic_internal
