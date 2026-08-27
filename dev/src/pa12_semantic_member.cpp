#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

bool PA11SemanticModel::direct_base_chain(TypeId object,
	std::vector<NamedRecordId>* chain) const
{
	if (chain == NULL)
		return false;
	chain->clear();
	if (!object.valid() || object.value >= types_.size())
		return false;
	const TypeId record_type = strip_cv_type(expression_object_type(object));
	if (!record_type.valid() || type_kind(record_type) != TypeKind::Named)
		return false;
	const NamedRecordId record_id = named_record_for_type(record_type);
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		return false;
	const NamedRecord& initial = named_[record_id.value];
	if (!initial.scope.valid() || initial.scope.value >= scopes_.size() ||
		scopes_[initial.scope.value].kind != ScopeKind::Class ||
		scopes_[initial.scope.value].record != record_id)
		throw std::runtime_error("invalid PA16 class scope metadata");

	// The semantic model permits one direct non-virtual base.  Keep the
	// relation walk typed and validate it before either lookup or lowering uses
	// it.  Floyd's check keeps malformed metadata from turning a bounded walk
	// into an unbounded retry without allocating a whole-program visited set.
	const auto next_base = [this](NamedRecordId current) -> NamedRecordId
	{
		if (!current.valid() || current.value >= named_.size() ||
			named_[current.value].kind != NamedKind::Class)
			throw std::runtime_error("invalid PA16 base record identity");
		const NamedRecord& record = named_[current.value];
		if (!record.scope.valid() || record.scope.value >= scopes_.size() ||
			scopes_[record.scope.value].kind != ScopeKind::Class ||
			scopes_[record.scope.value].record != current)
			throw std::runtime_error("invalid PA16 class scope metadata");
		if (record.direct_base_virtual)
			throw std::runtime_error("virtual inheritance is outside PA16");
		if (!record.has_base)
		{
			if (record.direct_base.valid())
				throw std::runtime_error("invalid PA16 direct base metadata");
			return NamedRecordId();
		}
		if (!record.direct_base.valid() || record.direct_base.value >= named_.size())
			throw std::runtime_error("invalid PA16 direct base metadata");
		const NamedRecord& base = named_[record.direct_base.value];
		if (base.kind != NamedKind::Class || !base.scope.valid() ||
			base.scope.value >= scopes_.size() ||
			scopes_[base.scope.value].kind != ScopeKind::Class ||
			scopes_[base.scope.value].record != record.direct_base ||
			base.class_tag == ClassTag::Union)
			throw std::runtime_error("invalid PA16 direct base class metadata");
		return record.direct_base;
	};

	NamedRecordId slow = record_id;
	NamedRecordId fast = record_id;
	while (slow.valid() && fast.valid())
	{
		slow = next_base(slow);
		if (!slow.valid())
			break;
		fast = next_base(fast);
		if (!fast.valid())
			break;
		fast = next_base(fast);
		if (!fast.valid())
			break;
		if (slow == fast)
			throw std::runtime_error("cyclic PA16 direct base metadata");
	}

	NamedRecordId current = record_id;
	while (true)
	{
		const NamedRecordId base = next_base(current);
		if (!base.valid())
			break;
		chain->push_back(base);
		current = base;
	}
	return true;
}

bool PA11SemanticModel::member_base_path(TypeId object, ScopeId target,
	std::vector<NamedRecordId>* path) const
{
	if (path != NULL)
		path->clear();
	if (!target.valid() || target.value >= scopes_.size() ||
		scopes_[target.value].kind != ScopeKind::Class)
		return false;
	const ScopeId actual = class_scope_for_type(object);
	if (!actual.valid())
		return false;
	if (actual == target)
		return true;
	std::vector<NamedRecordId> chain;
	if (!direct_base_chain(object, &chain))
		return false;
	for (std::size_t i = 0; i < chain.size(); ++i)
	{
		if (path != NULL)
			path->push_back(chain[i]);
		if (named_[chain[i].value].scope == target)
			return true;
	}
	if (path != NULL)
		path->clear();
	return false;
}

bool PA11SemanticModel::member_object_qualification_convertible(TypeId object,
	TypeId required) const
{
	if (!object.valid() || object.value >= types_.size() ||
		!required.valid() || required.value >= types_.size())
		return false;
	const TypeId actual_record = strip_cv_type(expression_object_type(object));
	const TypeId required_record = strip_cv_type(expression_object_type(required));
	if (type_kind(actual_record) != TypeKind::Named ||
		type_kind(required_record) != TypeKind::Named)
		return false;
	return (cv_qualifiers(object) & ~cv_qualifiers(required)) == 0;
}
bool PA11SemanticModel::member_object_convertible(TypeId object,
	TypeId required, ScopeId member_scope,
	std::vector<NamedRecordId>* path) const
{
	if (path != NULL)
		path->clear();
	if (!object.valid() || object.value >= types_.size() ||
		!required.valid() || required.value >= types_.size() ||
		!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class)
		return false;
	const TypeId required_record = strip_cv_type(expression_object_type(required));
	if (type_kind(required_record) != TypeKind::Named ||
		class_scope_for_type(required_record) != member_scope ||
		!member_base_path(object, member_scope, path))
		return false;
	return member_object_qualification_convertible(object, required);
}

TypeId PA11SemanticModel::member_access_type(TypeId object, TypeId member)
{
	const unsigned int qualifiers = cv_qualifiers(expression_object_type(object));
	return qualifiers == 0 ? member : make_cv(member, qualifiers);
}
BindingId PA11SemanticModel::member_binding(TypeId object, NameId name) const
{
	const TypeId record_type = strip_cv_type(expression_object_type(object));
	if (type_kind(record_type) != TypeKind::Named)
		return BindingId();
	const ScopeId scope = class_scope_for_type(record_type);
	if (!scope.valid())
		return BindingId();
	const ValueList* values = scopes_[scope.value].values.find(name);
	if (values == NULL || values->entries.size() != 1)
		return BindingId();
	return values->entries.front().binding;
}
std::vector<ValueRef> PA11SemanticModel::member_function_candidates(
	TypeId object, NameId name) const
{
	const TypeId record_type = strip_cv_type(expression_object_type(object));
	if (type_kind(record_type) != TypeKind::Named)
		return std::vector<ValueRef>();
	const ScopeId member_scope = class_scope_for_type(record_type);
	return member_function_candidates_in_scope(member_scope, name);
}
std::vector<ValueRef> PA11SemanticModel::member_function_candidates_in_scope(
	ScopeId member_scope, NameId name) const
{
	std::vector<ValueRef> result;
	if (!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class)
		return result;
	const ValueList* values = scopes_[member_scope.value].values.find(name);
	if (values == NULL)
		return result;
	for (std::size_t i = 0; i < values->entries.size(); ++i)
	{
		const BindingId candidate_id = values->entries[i].binding;
		const Binding& candidate = binding(candidate_id);
		if (candidate.kind == BindingKind::Function &&
			type_kind(candidate.type) == TypeKind::Function &&
			!is_static_member(candidate_id))
			result.push_back(ValueRef(member_scope, candidate_id));
	}
	return result;
}
PA11SemanticModel::UnqualifiedMemberDeclarationKind
PA11SemanticModel::unqualified_member_scope(TypeId object, NameId name,
	ScopeId start, ScopeId* declaration_scope, TypeId* declaration_type,
	std::vector<NamedRecordId>* base_path) const
{
	if (declaration_scope != NULL)
		*declaration_scope = ScopeId();
	if (declaration_type != NULL)
		*declaration_type = TypeId();
	if (base_path != NULL)
		base_path->clear();
	const ScopeId class_scope = class_scope_for_type(object);
	if (!class_scope.valid() || !start.valid() || start.value >= scopes_.size())
		return UnqualifiedMemberDeclarationKind::None;
	const SourcePoint point = lookup_source_point(start);
	ScopeId cursor = start;
	bool reached_class = false;
	while (cursor.valid() && cursor.value < scopes_.size())
	{
		begin_lookup();
		std::vector<ValueRef> values;
		if (lookup_value_graph(cursor, name, &values, false, point))
		{
			for (std::size_t i = 0; i < values.size(); ++i)
				if (values[i].scope != cursor)
					return UnqualifiedMemberDeclarationKind::None;
			if (declaration_scope != NULL)
				*declaration_scope = cursor;
			return UnqualifiedMemberDeclarationKind::Value;
		}
		// A type-only first declaration set is an owned functional-cast
		// outcome.  Start a fresh graph generation because the value probe
		// above may have marked the same inline-namespace scopes.
		begin_lookup();
		const TypeId type = lookup_type_graph(cursor, name, false, point);
		if (type.valid())
		{
			if (declaration_scope != NULL)
				*declaration_scope = cursor;
			if (declaration_type != NULL)
				*declaration_type = type;
			return UnqualifiedMemberDeclarationKind::Type;
		}
		if (cursor == class_scope)
		{
			reached_class = true;
			break;
		}
		cursor = scopes_[cursor.value].parent;
	}
	if (!reached_class)
		return UnqualifiedMemberDeclarationKind::None;

	std::vector<NamedRecordId> bases;
	if (!direct_base_chain(object, &bases))
		return UnqualifiedMemberDeclarationKind::None;
	for (std::size_t i = 0; i < bases.size(); ++i)
	{
		const ScopeId base_scope = named_[bases[i].value].scope;
		const auto record_base_path = [base_path, &bases, i]()
		{
			if (base_path != NULL)
				base_path->assign(bases.begin(), bases.begin() + i + 1);
		};
		begin_lookup();
		std::vector<ValueRef> values;
		if (lookup_value_graph(base_scope, name, &values, false, point))
		{
			for (std::size_t j = 0; j < values.size(); ++j)
				if (values[j].scope != base_scope)
				{
					if (declaration_scope != NULL)
						*declaration_scope = base_scope;
					record_base_path();
					return UnqualifiedMemberDeclarationKind::Blocked;
				}
			if (declaration_scope != NULL)
				*declaration_scope = base_scope;
			record_base_path();
			return UnqualifiedMemberDeclarationKind::Value;
		}
		begin_lookup();
		const TypeId type = lookup_type_graph(base_scope, name, false, point);
		if (type.valid())
		{
			if (declaration_scope != NULL)
				*declaration_scope = base_scope;
			if (declaration_type != NULL)
				*declaration_type = type;
			record_base_path();
			return UnqualifiedMemberDeclarationKind::Type;
		}
	}
	return UnqualifiedMemberDeclarationKind::None;
}
bool PA11SemanticModel::member_accessible(BindingId binding_id,
	ScopeId member_scope, ScopeId access_scope) const
{
	if (member_access(binding_id) == MemberAccess::Public)
		return true;
	ScopeId cursor = access_scope;
	while (cursor.valid() && cursor.value < scopes_.size())
	{
		if (cursor == member_scope)
			return true;
		cursor = scopes_[cursor.value].parent;
	}
	return false;
}
BindingId PA11SemanticModel::implicit_this_binding(ScopeId scope) const
{
	ScopeId cursor = scope;
	while (cursor.valid() && cursor.value < scopes_.size())
	{
		const Scope& current = scopes_[cursor.value];
		if (current.kind == ScopeKind::Function)
			return current.implicit_object_binding;
		cursor = current.parent;
	}
	return BindingId();
}
ExprInfo PA11SemanticModel::semantic_this_expression(
	const PA10AstNode& node, ScopeId scope)
{
	return semantic_this_expression(node, implicit_this_binding(scope));
}
ExprInfo PA11SemanticModel::semantic_this_expression(
	const PA10AstNode& node, BindingId this_id)
{
	if (!this_id.valid())
		throw std::runtime_error("PA12 this is outside a non-static member function");
	const Binding& this_binding = binding(this_id);
	if (this_binding.kind != BindingKind::Parameter ||
		type_kind(this_binding.type) != TypeKind::Pointer)
		throw std::runtime_error("PA12 implicit this binding is invalid");
	SemanticFact fact(SemanticFactKind::IdExpression, this_binding.type,
		SemanticValueCategory::Prvalue, &node);
	fact.binding = this_id;
	const SemanticFactId result = make_semantic_fact(fact);
	return ExprInfo(result, this_binding.type, SemanticValueCategory::Prvalue,
		false);
}

ExprInfo PA11SemanticModel::semantic_member_expression(
	const PA10AstNode& node, ScopeId scope)
{
	if (node.kind != PA10NodeKind::MemberExpression ||
		node.children.size() != 2 || !node.has_token ||
		(node.token != SimpleTokenType::OP_DOT &&
			node.token != SimpleTokenType::OP_ARROW) ||
		node.children[1].kind != PA10NodeKind::Identifier)
		throw std::runtime_error("PA12 invalid member expression");
	const ExprInfo object = semantic_expression(node.children.front(), scope);
	const NamePath member_name = name_path(node.children.back());
	if (member_name.global || member_name.components.size() != 1)
		throw std::runtime_error("PA12 qualified member is unsupported");
	TypeId record_object = object.type;
	if (node.token == SimpleTokenType::OP_ARROW)
	{
		const TypeId pointer = strip_cv_type(expression_object_type(object.type));
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 arrow operand is not a pointer");
		record_object = types_[pointer.value].child;
		const TypeId pointer_value = strip_top_cv_type(object.type);
		record_builtin_conversion(object, pointer_value);
	}
	else if (type_kind(strip_cv_type(expression_object_type(record_object))) !=
		TypeKind::Named)
		throw std::runtime_error("PA12 dot operand is not a record");
	const BindingId member_id = member_binding(record_object,
		member_name.last());
	if (!member_id.valid())
		throw std::runtime_error("PA12 unknown record member");
	const Binding& member = binding(member_id);
	if (member.kind != BindingKind::Variable)
		throw std::runtime_error("PA12 member function access is unsupported");
	const TypeId type = member_access_type(record_object, member.type);
	SemanticFact fact(SemanticFactKind::MemberExpression, type,
		SemanticValueCategory::Lvalue, &node);
	fact.token = node.token;
	fact.binding = member_id;
	fact.selected_binding = member_id;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, member_name);
	set_semantic_children(result,
		std::vector<SemanticFactId>(1, object.fact));
	return ExprInfo(result, type, SemanticValueCategory::Lvalue, false);
}

ExprInfo PA11SemanticModel::semantic_member_call_expression(
	const PA10AstNode& node, const PA10AstNode& member_node, ScopeId scope)
{
	if (member_node.kind != PA10NodeKind::MemberExpression ||
		member_node.children.size() != 2 || !member_node.has_token ||
		(member_node.token != SimpleTokenType::OP_DOT &&
			member_node.token != SimpleTokenType::OP_ARROW) ||
		member_node.children[1].kind != PA10NodeKind::Identifier)
		throw std::runtime_error("PA12 invalid member call expression");
	const NamePath member_name = name_path(member_node.children.back());
	if (member_name.global || member_name.components.size() != 1)
		throw std::runtime_error("PA12 qualified member call is unsupported");
	const ExprInfo object = semantic_expression(member_node.children.front(),
		scope);
	TypeId record_object = object.type;
	TypeId actual_object = object.type;
	if (member_node.token == SimpleTokenType::OP_ARROW)
	{
		const TypeId pointer = strip_cv_type(expression_object_type(object.type));
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 arrow operand is not a pointer");
		record_object = types_[pointer.value].child;
		actual_object = record_object;
	}
	else
	{
		record_object = strip_cv_type(expression_object_type(record_object));
		actual_object = expression_object_type(object.type);
		if (type_kind(record_object) != TypeKind::Named)
			return ExprInfo();
	}
	return semantic_member_call_with_object(node, scope,
		member_node.token, object, actual_object,
		class_scope_for_type(record_object),
		member_function_candidates(record_object, member_name.last()));
}

ExprInfo PA11SemanticModel::semantic_member_call_with_object(
	const PA10AstNode& node, ScopeId scope,
	SimpleTokenType member_token, const ExprInfo& object,
	TypeId actual_object, ScopeId member_scope,
	const std::vector<ValueRef>& candidates,
	const std::vector<NamedRecordId>* base_path)
{
	if (member_token != SimpleTokenType::OP_DOT &&
		member_token != SimpleTokenType::OP_ARROW)
		throw std::runtime_error("PA12 invalid member call token");
	if (!member_scope.valid())
		return ExprInfo();
	if (candidates.empty())
		return ExprInfo();
	if (member_token == SimpleTokenType::OP_DOT &&
		object.category != SemanticValueCategory::Lvalue)
		throw std::runtime_error("PA12 member call needs an lvalue object");
	if (member_token == SimpleTokenType::OP_ARROW)
	{
		const TypeId pointer = strip_cv_type(expression_object_type(object.type));
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 member call arrow operand is not a pointer");
		const TypeId pointer_value = strip_top_cv_type(object.type);
		record_builtin_conversion(object, pointer_value);
	}
	if (base_path == NULL && !member_base_path(actual_object, member_scope, NULL))
		throw std::runtime_error("PA12 member call object owner mismatch");
	if (base_path != NULL && base_path->empty() &&
		class_scope_for_type(actual_object) != member_scope)
		throw std::runtime_error("PA12 member call object owner mismatch");

	const PA10AstNode& argument_node = node.children.back();
	std::vector<ExprInfo> arguments;
	for (std::size_t i = 0; i < argument_node.children.size(); ++i)
	{
		if (target_function_id(argument_node.children[i], scope) != NULL)
			arguments.push_back(ExprInfo());
		else
			arguments.push_back(semantic_expression(argument_node.children[i], scope));
	}
	struct CandidateScore
	{
		ValueRef value;
		TypeId type;
		unsigned int object_cv;
		std::vector<unsigned int> ranks;
	};
	std::vector<CandidateScore> viable;
	const unsigned int ellipsis_rank = std::numeric_limits<unsigned int>::max() / 4;
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const Binding& candidate = binding(candidates[i].binding);
		const TypeKey& function = types_[candidate.type.value];
		const TypeId required_object = member_object_type(candidate.type,
			member_scope);
		if (!required_object.valid() ||
			class_scope_for_type(required_object) != member_scope ||
			!member_object_qualification_convertible(actual_object,
				required_object))
			continue;
		std::size_t required = function.parameters.size();
		while (required != 0 && function_default_argument(
			candidates[i].binding, required - 1).valid())
			--required;
		if (arguments.size() < required ||
			(!function.variadic && arguments.size() > function.parameters.size()))
			continue;
		CandidateScore score;
		score.value = candidates[i];
		score.type = candidate.type;
		score.object_cv = cv_qualifiers(required_object) &
			~cv_qualifiers(actual_object);
		score.ranks.reserve(arguments.size());
		bool arguments_viable = true;
		for (std::size_t arg = 0; arg < arguments.size(); ++arg)
		{
			if (arg >= function.parameters.size())
			{
				if (!arguments[arg].fact.valid())
				{
					arguments_viable = false;
					break;
				}
				score.ranks.push_back(ellipsis_rank);
				continue;
			}
			ConversionChoice choice;
			if (arguments[arg].fact.valid())
				choice = conversion_for(arguments[arg].type,
					arguments[arg].category, function.parameters[arg],
					semantic_facts_[arguments[arg].fact.value].source,
					arguments[arg].integer_zero);
			else
			{
				const PA10AstNode* function_id = target_function_id(
					argument_node.children[arg], scope);
				if (function_id != NULL)
				{
					const FunctionIdResolution resolution = resolve_function_id_target(
						*function_id, scope, function.parameters[arg]);
					choice = resolution.conversion;
				}
			}
			if (!choice.valid)
			{
				arguments_viable = false;
				break;
			}
			score.ranks.push_back(choice.rank);
		}
		if (arguments_viable)
			viable.push_back(score);
	}
	if (viable.empty())
		throw std::runtime_error("PA12 no viable member call");
	const auto better = [](const CandidateScore& left,
		const CandidateScore& right) -> bool
	{
		bool strict = false;
		// Qualification conversions form a subset ordering.  Thus an exact
		// object match beats any added cv, const beats const volatile, and
		// const and volatile remain incomparable.
		if (left.object_cv != right.object_cv)
		{
			if ((left.object_cv & ~right.object_cv) != 0)
				return false;
			strict = true;
		}
		if (left.ranks.size() != right.ranks.size())
			return false;
		for (std::size_t i = 0; i < left.ranks.size(); ++i)
		{
			if (left.ranks[i] > right.ranks[i])
				return false;
			if (left.ranks[i] < right.ranks[i])
				strict = true;
		}
		return strict;
	};
	std::size_t best_index = 0;
	for (std::size_t i = 1; i < viable.size(); ++i)
		if (better(viable[i], viable[best_index]))
			best_index = i;
	for (std::size_t i = 0; i < viable.size(); ++i)
		if (i != best_index && !better(viable[best_index], viable[i]))
			throw std::runtime_error("PA12 ambiguous member call");
	const ValueRef selected = viable[best_index].value;
	const TypeId selected_type = viable[best_index].type;
	if (!member_accessible(selected.binding, member_scope, scope))
		throw std::runtime_error("PA12 member call is inaccessible");
	if (function_declaration_kind(selected.binding) ==
		FunctionDeclarationKind::Deleted)
		throw std::runtime_error("PA12 member call selects deleted function");
	const TypeKey& function = types_[selected_type.value];
	const std::size_t explicit_count = arguments.size();
	for (std::size_t arg = explicit_count;
		arg < function.parameters.size(); ++arg)
	{
		const SemanticFactId default_fact = function_default_argument(
			selected.binding, arg);
		if (!default_fact.valid())
			throw std::runtime_error("PA12 selected member default is missing");
		const SemanticFact& value = semantic_facts_[default_fact.value];
		arguments.push_back(ExprInfo(default_fact, value.type, value.category,
			false));
	}
	const std::size_t fixed_explicit = explicit_count < function.parameters.size() ?
		explicit_count : function.parameters.size();
	for (std::size_t arg = 0; arg < fixed_explicit; ++arg)
	{
		if (!arguments[arg].fact.valid())
			arguments[arg] = semantic_expression_for_target(
				argument_node.children[arg], scope, function.parameters[arg]);
		arguments[arg] = apply_context_conversion(arguments[arg],
			function.parameters[arg],
			semantic_facts_[arguments[arg].fact.value].source);
	}
	apply_call_argument_conversions(arguments, selected_type, scope);
	const TypeId result_type = function_result_type(selected_type);
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	SemanticFact fact(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	fact.token = member_token;
	fact.has_callee = true;
	fact.has_implicit_object = true;
	fact.selected_binding = selected.binding;
	fact.selected_scope = selected.scope;
	fact.callable_type = member_function_expression_type(selected_type,
		member_scope, selected.binding);
	if (type_kind(fact.callable_type) != TypeKind::Function)
		throw std::runtime_error("PA12 member call has no hidden object signature");
	std::vector<SemanticFactId> children;
	children.push_back(object.fact);
	for (std::size_t i = 0; i < arguments.size(); ++i)
		children.push_back(arguments[i].fact);
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_children(result, children);
	return ExprInfo(result, result_type, result_category, false);
}

ExprInfo PA11SemanticModel::semantic_unqualified_member_call(
	const PA10AstNode& node, const PA10AstNode& callee_node, ScopeId scope)
{
	if (node.children.size() != 2)
		return ExprInfo();
	if (callee_node.kind != PA10NodeKind::IdExpression ||
		callee_node.has_token || has_template_id(callee_node))
		return ExprInfo();
	const NamePath member_name = name_path(callee_node);
	if (member_name.global || member_name.components.size() != 1)
		return ExprInfo();
	const BindingId this_id = implicit_this_binding(scope);
	if (!this_id.valid())
		return ExprInfo();
	const Binding& this_binding = binding(this_id);
	const TypeId this_pointer = strip_cv_type(expression_object_type(
		this_binding.type));
	if (this_binding.kind != BindingKind::Parameter ||
		type_kind(this_pointer) != TypeKind::Pointer)
		throw std::runtime_error("PA12 implicit this binding is invalid");
	const TypeId record_object = types_[this_pointer.value].child;
	const ScopeId member_scope = class_scope_for_type(record_object);
	if (!member_scope.valid())
		return ExprInfo();

	// Search lexical block/function scopes, then the direct class and its typed
	// direct-base chain.  A nearer non-class declaration leaves ownership with
	// the ordinary call resolver; a class/base method set suppresses enclosing
	// namespace candidates.
	std::vector<NamedRecordId> base_path;
	ScopeId selected_scope;
	TypeId selected_type;
	const UnqualifiedMemberDeclarationKind declaration_kind =
		unqualified_member_scope(record_object, member_name.last(), scope,
			&selected_scope, &selected_type, &base_path);
	if (declaration_kind == UnqualifiedMemberDeclarationKind::Type)
	{
		// Keep the first type declaration set typed through the existing cast
		// producer.  Do not reopen ordinary enclosing value/ADL lookup after a
		// class or base has claimed the spelling.
		if (!selected_type.valid())
			throw std::runtime_error("PA12 invalid owned functional cast target");
		return semantic_functional_cast(node, scope, selected_type,
			node.children.back());
	}
	if (declaration_kind == UnqualifiedMemberDeclarationKind::Blocked)
		throw std::runtime_error("PA12 inherited member name is unsupported");
	if (declaration_kind != UnqualifiedMemberDeclarationKind::Value ||
		!selected_scope.valid() || selected_scope.value >= scopes_.size() ||
		scopes_[selected_scope.value].kind != ScopeKind::Class)
		return ExprInfo();
	const std::vector<ValueRef> candidates =
		member_function_candidates_in_scope(selected_scope, member_name.last());
	if (candidates.empty())
	{
		if (!base_path.empty())
			throw std::runtime_error("PA12 inherited member name is not callable");
		return ExprInfo();
	}
	const ExprInfo object = semantic_this_expression(node, this_id);
	return semantic_member_call_with_object(node, scope,
		SimpleTokenType::OP_ARROW, object, record_object, selected_scope,
		candidates, &base_path);
}

ExprInfo PA11SemanticModel::semantic_member_call_probe(
	const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 2)
		return ExprInfo();
	const PA10AstNode* member_node = &node.children.front();
	while (member_node->kind == PA10NodeKind::ParenthesizedExpression &&
		member_node->children.size() == 1)
		member_node = &member_node->children.front();
	if (member_node->kind != PA10NodeKind::MemberExpression &&
		member_node->kind != PA10NodeKind::IdExpression)
		return ExprInfo();
	SemanticTailGuard member_tail(*this);
	const ExprInfo member_call = member_node->kind ==
		PA10NodeKind::MemberExpression ? semantic_member_call_expression(
			node, *member_node, scope) : semantic_unqualified_member_call(node,
			*member_node, scope);
	if (!member_call.fact.valid())
		return ExprInfo();
	member_tail.commit();
	return member_call;
}

} // namespace pa11_semantic_internal
