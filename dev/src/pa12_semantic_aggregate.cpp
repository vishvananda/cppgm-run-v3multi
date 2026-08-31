#include "pa11_semantic_model.h"

#include <algorithm>
#include <stdexcept>

namespace pa11_semantic_internal
{

struct PA11SemanticModel::AggregateAppertainer
{
	PA11SemanticModel& model_;
	ScopeId scope_;

	AggregateAppertainer(PA11SemanticModel& model, ScopeId scope)
		: model_(model), scope_(scope) {}

	bool aggregate_target(TypeId candidate) const
	{
		const TypeId object = model_.strip_top_cv_type(candidate);
		if (!object.valid() || object.value >= model_.types_.size())
			return false;
		if (model_.type_kind(object) == TypeKind::Array)
			return !model_.types_[object.value].unknown_bound;
		if (model_.type_kind(object) != TypeKind::Named)
			return false;
		const NamedRecordId record = model_.named_record_for_type(object);
		return record.valid() && record.value < model_.named_.size() &&
			model_.named_[record.value].kind == NamedKind::Class &&
			model_.named_[record.value].class_tag != ClassTag::Union &&
			!model_.named_[record.value].has_base &&
			model_.aggregate_class_initialization_supported(record);
	}

	bool string_array_clause(const PA10AstNode& clause, TypeId destination) const
	{
		if (clause.kind != PA10NodeKind::Literal ||
			clause.literal.element_count == 0)
			return false;
		const TypeId object = model_.strip_top_cv_type(destination);
		if (!object.valid() || object.value >= model_.types_.size() ||
			model_.type_kind(object) != TypeKind::Array)
			return false;
		const TypeKey& array = model_.types_[object.value];
		FundamentalType element;
		return !array.unknown_bound &&
			model_.fundamental_of(array.child, &element) &&
			element == FundamentalType::Char &&
			clause.literal.element_count <= array.bound.value;
	}

	ExprInfo apply_list_conversion(const ExprInfo& expression, TypeId destination)
	{
		const TypeId source_object = model_.strip_top_cv_type(
			model_.expression_object_type(expression.type));
		const TypeId target_object = model_.strip_top_cv_type(
			model_.expression_object_type(destination));
		// This checkpoint covers the tested [dcl.init.list] floating-to-integral
		// narrowing category.  Other scalar categories remain on the existing
		// typed conversion path until their PA16 fixtures are supported.
		if (model_.floating_id(source_object) &&
			model_.integral_id(target_object))
			throw std::runtime_error("PA12 list initialization narrows floating value");
		const PA10AstNode* source = expression.fact.valid() &&
			expression.fact.value < model_.semantic_facts_.size() ?
			model_.semantic_facts_[expression.fact.value].source : NULL;
		const ExprInfo converted = model_.apply_context_conversion(expression,
			destination, source, scope_);
		if (converted.fact.valid() && converted.fact.value <
			model_.semantic_facts_.size() &&
			model_.semantic_facts_[converted.fact.value].literal_element_count != 0)
			model_.record_constant_address(converted.fact, scope_);
		return converted;
	}

	ExprInfo consume_value(const PA10AstNode& clause, TypeId destination)
	{
		const TypeId object = model_.strip_top_cv_type(destination);
		if (string_array_clause(clause, destination))
		{
			// The decoded literal fact is retargeted as an array payload.  PA15
			// consumes the typed bytes and never recovers the source spelling.
			const SemanticFactId source_fact = model_.semantic_literal(clause);
			SemanticFact typed = model_.semantic_facts_[source_fact.value];
			typed.type = destination;
			const SemanticFactId result = model_.make_semantic_fact(typed);
			return ExprInfo(result, destination, SemanticValueCategory::Lvalue, false);
		}
		const NamedRecordId record = model_.named_record_for_type(object);
		if (record.valid() && record.value < model_.named_.size() &&
			model_.named_[record.value].kind == NamedKind::Class &&
			clause.kind == PA10NodeKind::CallExpression &&
			clause.children.size() == 2 &&
			clause.children.back().kind == PA10NodeKind::BracedInitList)
		{
			TypeId functional_target;
			if (model_.functional_cast_target(clause.children.front(), scope_,
				&functional_target) &&
				model_.class_record_for_object_type(functional_target) == record)
			{
				const PA10AstNode& list = clause.children.back();
				std::vector<const PA10AstNode*> arguments;
				arguments.reserve(list.children.size());
				for (std::size_t i = 0; i < list.children.size(); ++i)
					arguments.push_back(&list.children[i]);
				// The list expression names exactly this aggregate member.  Its
				// constructor selection can therefore initialize the subobject
				// directly, preserving copy-elision and avoiding an untyped class
				// value transfer through the enclosing aggregate.
				return model_.semantic_aggregate_constructor_value(
					list, destination, scope_, arguments, list.children.empty());
			}
		}
		if (record.valid() && record.value < model_.named_.size() &&
			model_.named_[record.value].kind == NamedKind::Class &&
			!aggregate_target(destination))
		{
			std::vector<const PA10AstNode*> arguments;
			if (clause.kind == PA10NodeKind::BracedInitList)
			{
				arguments.reserve(clause.children.size());
				for (std::size_t i = 0; i < clause.children.size(); ++i)
					arguments.push_back(&clause.children[i]);
			}
			else
				arguments.push_back(&clause);
			return model_.semantic_aggregate_constructor_value(clause, destination,
				scope_, arguments, clause.kind == PA10NodeKind::BracedInitList &&
				arguments.empty());
		}
		if (clause.kind == PA10NodeKind::BracedInitList)
			return consume_braced(clause, destination);
		const ExprInfo expression = model_.semantic_expression_for_target(clause,
			scope_, destination);
		return apply_list_conversion(expression, destination);
	}

	bool direct_member_clause(TypeId slot_type, const PA10AstNode& clause) const
	{
		if (clause.kind != PA10NodeKind::BracedInitList ||
			clause.children.empty() || !aggregate_target(slot_type))
			return false;
		const TypeId object = model_.strip_top_cv_type(slot_type);
		const NamedRecordId record = model_.named_record_for_type(object);
		if (!record.valid() || !model_.aggregate_class_initialization_supported(record))
			return false;
		const RecordLayout& layout = model_.record_layout(record);
		if (layout.state != RecordLayoutState::Complete ||
			layout.members.size() != clause.children.size())
			return false;
		for (std::size_t i = 0; i < clause.children.size(); ++i)
			if (clause.children[i].kind == PA10NodeKind::BracedInitList)
				return false;
		return true;
	}

	void consume_slot(TypeId slot_type, AggregateElementFact& element,
		const std::vector<PA10AstNode>& clauses, std::size_t& cursor,
		bool prefer_aggregate_constructor)
	{
		if (cursor >= clauses.size())
			return;
		const PA10AstNode& clause = clauses[cursor];
		if (prefer_aggregate_constructor && direct_member_clause(slot_type, clause))
		{
			std::vector<const PA10AstNode*> arguments;
			arguments.reserve(clause.children.size());
			for (std::size_t i = 0; i < clause.children.size(); ++i)
				arguments.push_back(&clause.children[i]);
			const ExprInfo value = model_.semantic_aggregate_constructor_value(
				clause, slot_type, scope_, arguments, false);
			++cursor;
			element.initializer = value.fact;
			return;
		}
		if (aggregate_target(slot_type) &&
			clause.kind != PA10NodeKind::BracedInitList &&
			!string_array_clause(clause, slot_type))
		{
			const std::size_t original = cursor;
			element.initializer = consume_aggregate(slot_type, clause, clauses,
				cursor).fact;
			if (cursor == original)
				throw std::runtime_error("PA12 brace-elided aggregate consumed no clause");
		}
		else
		{
			element.initializer = consume_value(clause, slot_type).fact;
			++cursor;
		}
	}

	ExprInfo consume_aggregate(TypeId destination, const PA10AstNode& source,
		const std::vector<PA10AstNode>& clauses, std::size_t& cursor)
	{
		const TypeId object = model_.strip_top_cv_type(destination);
		if (!object.valid() || object.value >= model_.types_.size())
			throw std::runtime_error("PA12 aggregate target is invalid");
		std::vector<AggregateElementFact> elements;
		std::size_t total_count = 0;
		if (model_.type_kind(object) == TypeKind::Array)
		{
			// Semanticizing a child can intern new types, so do not retain a
			// reference into the reallocated type arena across consume_slot.
			const TypeKey array = model_.types_[object.value];
			if (array.unknown_bound)
				throw std::runtime_error("PA12 aggregate array bound is incomplete");
			total_count = array.bound.value;
			elements.reserve(std::min(array.bound.value, clauses.size()));
			const TypeId child_object = model_.strip_top_cv_type(array.child);
			const NamedRecordId child_record =
				model_.class_record_for_object_type(array.child);
			const bool runtime_tail = model_.type_kind(child_object) == TypeKind::Named &&
				child_record.valid() && model_.constructor_requires_runtime(child_record);
			// Omitted scalar tails stay implicit; runtime class elements are material
			// because each needs a typed constructor selection.
			for (std::size_t i = 0; i < array.bound.value &&
				(cursor < clauses.size() || runtime_tail); ++i)
			{
				AggregateElementFact element(AggregateElementKind::ArrayElement,
					array.child, BindingId(), i);
				consume_slot(array.child, element, clauses, cursor, true);
				if (!element.initializer.valid())
				{
					const TypeId child_object = model_.strip_cv_type(array.child);
					const NamedRecordId child_record =
						model_.class_record_for_object_type(array.child);
					if (model_.type_kind(child_object) == TypeKind::Named &&
						child_record.valid() &&
						model_.constructor_requires_runtime(child_record))
						element.initializer =
							model_.semantic_aggregate_constructor_value(source, array.child,
								scope_, std::vector<const PA10AstNode*>(), false).fact;
				}
				if (element.initializer.valid())
					elements.push_back(element);
			}
		}
		else if (model_.type_kind(object) == TypeKind::Named)
		{
			const NamedRecordId record = model_.named_record_for_type(object);
			if (!record.valid() || record.value >= model_.named_.size() ||
				model_.named_[record.value].kind != NamedKind::Class ||
				model_.named_[record.value].class_tag == ClassTag::Union ||
				model_.named_[record.value].has_base)
				throw std::runtime_error("PA12 aggregate class target is unsupported");
			const ScopeId class_scope = model_.named_[record.value].scope;
			const RecordLayout& layout = model_.record_layout(record);
			if (!class_scope.valid() || class_scope.value >= model_.scopes_.size() ||
				model_.scopes_[class_scope.value].kind != ScopeKind::Class ||
				model_.scopes_[class_scope.value].record != record ||
				layout.state != RecordLayoutState::Complete)
				throw std::runtime_error("PA12 aggregate class scope is missing");
			std::vector<BindingId> members;
			members.reserve(layout.members.size());
			for (std::size_t i = 0; i < layout.members.size(); ++i)
			{
				const BindingId member_id = layout.members[i].binding;
				if (!member_id.valid() || member_id.value >= model_.bindings_.size() ||
					member_id.value >= model_.binding_owners_.size() ||
					model_.binding_owners_[member_id.value] != class_scope ||
					model_.binding(member_id).kind != BindingKind::Variable ||
					model_.is_static_member(member_id))
					throw std::runtime_error(
						"PA12 aggregate class member identity is invalid");
				members.push_back(member_id);
			}
			total_count = members.size();
			elements.reserve(std::min(members.size(), clauses.size()));
			for (std::size_t i = 0; i < members.size(); ++i)
			{
				const BindingId member = members[i];
				AggregateElementFact element(AggregateElementKind::Member,
					model_.binding(member).type, member, i);
				consume_slot(element.type, element, clauses, cursor, false);
				if (!element.initializer.valid())
				{
					const BindingSidecar* sidecar = model_.binding_sidecar(member);
					if (sidecar != NULL && sidecar->default_member_initializer.valid())
						element.initializer = sidecar->default_member_initializer;
					else
					{
						const TypeId member_object = model_.strip_cv_type(element.type);
						if (model_.type_kind(member_object) == TypeKind::LvalueReference ||
							model_.type_kind(member_object) == TypeKind::RvalueReference)
							throw std::runtime_error(
								"PA12 omitted aggregate reference has no initializer");
						const NamedRecordId member_record =
							model_.class_record_for_object_type(element.type);
						if (model_.type_kind(member_object) == TypeKind::Named &&
							member_record.valid() &&
							model_.constructor_requires_runtime(member_record))
							element.initializer =
								model_.semantic_aggregate_constructor_value(source, element.type,
									scope_, std::vector<const PA10AstNode*>(), false).fact;
					}
				}
				if (element.initializer.valid())
					elements.push_back(element);
			}
		}
		else
			throw std::runtime_error("PA12 aggregate target is not an object");
		const SemanticFactId result = model_.make_expression_fact(
			SemanticFactKind::BracedInitList, object,
			SemanticValueCategory::Lvalue, source,
			std::vector<SemanticFactId>());
		model_.set_semantic_aggregate_elements(result, elements, total_count);
		return ExprInfo(result, object, SemanticValueCategory::Lvalue, false);
	}

	ExprInfo consume_braced(const PA10AstNode& list, TypeId destination)
	{
		const TypeId object = model_.strip_top_cv_type(destination);
		if (aggregate_target(destination))
		{
			std::size_t cursor = 0;
			const ExprInfo result = consume_aggregate(destination, list,
				list.children, cursor);
			if (cursor != list.children.size())
				throw std::runtime_error("PA12 aggregate initializer has too many elements");
			return result;
		}
		if (model_.type_kind(object) != TypeKind::Array)
		{
			if (list.children.empty())
			{
				const NamedRecordId record = model_.named_record_for_type(object);
				if (record.valid() && record.value < model_.named_.size() &&
					model_.named_[record.value].kind == NamedKind::Class)
					return model_.semantic_aggregate_constructor_value(list, destination,
						scope_, std::vector<const PA10AstNode*>(), true);
				return model_.semantic_empty_braced_init_list(list, destination);
			}
			if (list.children.size() != 1)
				throw std::runtime_error("PA12 scalar braced initializer needs one value");
			return consume_value(list.children.front(), destination);
		}
		throw std::runtime_error("PA12 braced initializer target is unsupported");
	}

	ExprInfo consume(const PA10AstNode& node, TypeId target)
	{
		if (node.kind != PA10NodeKind::BracedInitList)
			throw std::runtime_error("PA12 expected braced initializer");
		return consume_braced(node, target);
	}
};

ExprInfo PA11SemanticModel::semantic_braced_init_list(
	const PA10AstNode& node, TypeId target, ScopeId scope)
{
	return AggregateAppertainer(*this, scope).consume(node, target);
}

void PA11SemanticModel::dump_pa12_aggregate_fact(std::ostream& output,
	SemanticFactId id, std::size_t depth) const
{
	const AggregateFactRange* range = aggregate_ranges_.find(id);
	if (range == NULL)
		return;
	const SemanticFact& fact = semantic_facts_[id.value];
	if (fact.child_count != 0 || range->count > range->total_count ||
		(range->count != 0 && range->begin == InvalidIdentityValue) ||
		range->begin > aggregate_elements_.size() ||
		range->count > aggregate_elements_.size() - range->begin)
		throw std::runtime_error("invalid PA12 aggregate fact range");
	for (std::size_t i = 0; i < range->count; ++i)
	{
		const AggregateElementFact& element =
			aggregate_elements_[range->begin + i];
		if (!element.initializer.valid() ||
			element.initializer.value >= semantic_facts_.size())
			throw std::runtime_error("invalid PA12 aggregate initializer identity");
		dump_pa12_fact(output, element.initializer, depth + 1);
	}
}

} // namespace pa11_semantic_internal
