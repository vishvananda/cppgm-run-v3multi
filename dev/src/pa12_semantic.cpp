#include "pa11_semantic.h"
#include "pa11_semantic_model.h"
#include <algorithm>
namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;
TypeId PA11SemanticModel::switch_condition_type(TypeId type) const
{
	type = strip_cv_type(expression_object_type(type));
	const NamedRecordId record = named_record_for_type(type);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Enum)
	{
		if (named_[record.value].scoped_enum)
			return type;
		const TypeId underlying = named_[record.value].has_underlying ?
			strip_cv_type(named_[record.value].underlying) :
			fundamental(FundamentalType::Int);
		return promote_integral_type(underlying);
	}
	return promote_integral_type(type);
}
bool PA11SemanticModel::case_label_convertible(TypeId source, TypeId target) const
{
	source = strip_cv_type(expression_object_type(source));
	target = strip_cv_type(expression_object_type(target));
	const NamedRecordId source_record = named_record_for_type(source);
	const NamedRecordId target_record = named_record_for_type(target);
	if (target_record.valid())
		return source == target;
	if (source_record.valid() && source_record.value < named_.size() &&
		named_[source_record.value].scoped_enum)
		return false;
	return conversion_for(source, SemanticValueCategory::Prvalue, target, NULL).valid;
}
ScopeId PA11SemanticModel::create_internal_scope(ScopeId parent)
{
	if (!parent.valid() || parent.value >= scopes_.size())
		throw std::runtime_error("PA12 internal scope has no valid parent");
	const ScopeId result(scopes_.size());
	const std::size_t depth = scopes_[parent.value].depth + 1;
	scopes_.push_back(Scope(ScopeKind::Block, parent, NameId(),
		NamedRecordId(), false, creation_order_++, depth));
	return result;
}
void PA11SemanticModel::process_condition_declaration(
	const PA10AstNode& node, ScopeId scope)
{
	if (declaration_fact(node) != NULL)
		return;
	if (node.kind != PA10NodeKind::ConditionDeclaration ||
		node.children.size() != 3 ||
		node.children[0].kind != PA10NodeKind::DeclSpecifierSeq ||
		node.children[1].kind != PA10NodeKind::Declarator)
		throw std::runtime_error("invalid PA12 condition declaration");
	const SpecFact spec = spec_fact(node.children[0], scope);
	if (spec.is_typedef)
		throw std::runtime_error("condition declaration cannot be a typedef");
	const DeclaratorName name = declarator_name(node.children[1]);
	if (!name.found || name.path.components.size() != 1)
		throw std::runtime_error("invalid PA12 condition declarator");
	const ScopeId target = declaration_scope(name.path, scope);
	if (!target.valid())
		throw std::runtime_error("unresolved PA12 condition scope");
	DeclaratorBaseKind base_kind = spec.is_auto ?
		DeclaratorBaseKind::AutoPlaceholder : DeclaratorBaseKind::Typed;
	const TypeId type = apply_declarator(node.children[1], spec.base, target,
		base_kind);
	if (!type.valid())
		throw std::runtime_error("PA12 condition has no typed type");
	const BindingId binding_id = add_value(target, name.path.last(), type, false,
		false, true, BindingId(), SourcePoint(node.source_begin));
	DeclarationFact declaration(&node, target);
	declaration.is_extern = spec.is_extern;
	declaration.is_static = spec.is_static;
	declaration.is_thread_local = spec.is_thread_local;
	declaration.automatic_storage = target.value < scopes_.size() &&
		scopes_[target.value].kind == ScopeKind::Block &&
		!spec.is_static && !spec.is_extern && !spec.is_thread_local;
	declaration.binding_begin = declaration_bindings_.size();
	declaration_bindings_.push_back(binding_id);
	declaration_definition_flags_.push_back(0);
	declaration.binding_count = 1;
	const DeclarationFactId declaration_id(declaration_facts_.size());
	declaration_facts_.push_back(declaration);
	declaration_fact_index_.set(&node, declaration_id);
}
StatementFactId PA11SemanticModel::add_statement_fact(
	const StatementFact& fact)
{
	if (fact.node == NULL)
		throw std::runtime_error("PA12 statement fact has no node");
	const StatementFactId result(statement_facts_.size());
	statement_facts_.push_back(fact);
	statement_fact_index_.set(fact.node, result);
	return result;
}
const StatementFact* PA11SemanticModel::statement_fact(
	const PA10AstNode& node) const
{
	const StatementFactId* found = statement_fact_index_.find(&node);
	if (found == NULL || !found->valid() ||
		found->value >= statement_facts_.size())
		return NULL;
	return &statement_facts_[found->value];
}
ScopeId PA11SemanticModel::substatement_scope(const PA10AstNode& node) const
{
	const ScopeId* found = substatement_scope_index_.find(&node);
	return found == NULL ? ScopeId() : *found;
}
void PA11SemanticModel::prepare_pa12_condition(const PA10AstNode& node,
	ScopeId scope)
{
	if (node.kind != PA10NodeKind::Condition || node.children.size() > 1)
		throw std::runtime_error("invalid PA12 condition");
	if (!node.children.empty() &&
		node.children.front().kind == PA10NodeKind::ConditionDeclaration)
		process_condition_declaration(node.children.front(), scope);
}
void PA11SemanticModel::prepare_pa12_substatement(
	const PA10AstNode& node, ScopeId parent)
{
	if (node.kind == PA10NodeKind::CompoundStatement)
	{
		prepare_pa12_compound(node, parent);
		return;
	}
	const ScopeId* old = substatement_scope_index_.find(&node);
	ScopeId substatement;
	if (old != NULL)
		substatement = *old;
	else
	{
		substatement = create_internal_scope(parent);
		substatement_scope_index_.set(&node, substatement);
	}
	if (!substatement.valid())
		throw std::runtime_error("PA12 substatement scope is missing");
	prepare_pa12_statement(node, substatement);
}

void PA11SemanticModel::prepare_pa12_compound(const PA10AstNode& node,
	ScopeId parent)
{
	if (node.kind != PA10NodeKind::CompoundStatement)
		throw std::runtime_error("PA12 expected compound statement");
	const ScopeId* old = compound_scope_index_.find(&node);
	ScopeId block;
	if (old == NULL)
	{
		block = process_compound_statement(node, parent);
	}
	else
	{
		block = *old;
		if (!block.valid() || !parent.valid() || block == parent ||
			scopes_[block.value].parent != parent)
			throw std::runtime_error("PA12 compound scope is missing");
	}
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		// process_compound_statement already formed direct declaration
		// children in source order.  Do not feed them back through the
		// substatement declaration path, which is reserved for declarations
		// reached through an unbraced or label/case statement edge.
		switch (node.children[i].kind)
		{
		case PA10NodeKind::SimpleDeclaration:
		case PA10NodeKind::AliasDeclaration:
		case PA10NodeKind::NamespaceAliasDefinition:
		case PA10NodeKind::UsingDirective:
		case PA10NodeKind::UsingDeclaration:
			continue;
		default:
			prepare_pa12_statement(node.children[i], block);
			break;
		}
	}
}
ScopeId PA11SemanticModel::prepare_pa12_control(
	const PA10AstNode& node, ScopeId parent, StatementFactKind kind)
{
	const StatementFact* old = statement_fact(node);
	if (old != NULL)
		return old->scope;
	const ScopeId result = create_internal_scope(parent);
	add_statement_fact(StatementFact(&node, kind, result));
	return result;
}

void PA11SemanticModel::prepare_pa12_statement(
	const PA10AstNode& node, ScopeId scope)
{
	switch (node.kind)
	{
	case PA10NodeKind::CompoundStatement:
		prepare_pa12_compound(node, scope);
		return;
	case PA10NodeKind::SimpleDeclaration:
		if (declaration_fact(node) == NULL)
			process_simple_declaration(node, scope);
		return;
	case PA10NodeKind::AliasDeclaration:
		if (declaration_fact(node) == NULL)
			process_declaration(node, scope);
		return;
	case PA10NodeKind::NamespaceAliasDefinition:
	case PA10NodeKind::UsingDirective:
	case PA10NodeKind::UsingDeclaration:
		// These declarations are not direct children of a compound scope
		// here; they were reached through an implicit substatement or a
		// label/case edge and must be formed in that edge's scope.
		process_declaration(node, scope);
		return;
	case PA10NodeKind::IfStatement:
	{
		if (node.children.size() < 2 || node.children[0].kind !=
			PA10NodeKind::Condition)
			throw std::runtime_error("invalid PA12 if statement");
		const ScopeId control = prepare_pa12_control(node, scope,
			StatementFactKind::If);
		prepare_pa12_condition(node.children[0], control);
		for (std::size_t i = 1; i < node.children.size(); ++i)
		{
			const PA10AstNode& branch = node.children[i];
			if ((branch.kind != PA10NodeKind::ThenBranch &&
				branch.kind != PA10NodeKind::ElseBranch) ||
				branch.children.size() != 1)
				throw std::runtime_error("invalid PA12 if branch");
			prepare_pa12_substatement(branch.children.front(), control);
		}
		return;
	}
	case PA10NodeKind::SwitchStatement:
	{
		if (node.children.size() != 2 || node.children[0].kind !=
			PA10NodeKind::Condition)
			throw std::runtime_error("invalid PA12 switch statement");
		const ScopeId control = prepare_pa12_control(node, scope,
			StatementFactKind::Switch);
		prepare_pa12_condition(node.children[0], control);
		prepare_pa12_substatement(node.children[1], control);
		return;
	}
	case PA10NodeKind::WhileStatement:
	{
		if (node.children.size() != 2 || node.children[0].kind !=
			PA10NodeKind::Condition)
			throw std::runtime_error("invalid PA12 while statement");
		const ScopeId control = prepare_pa12_control(node, scope,
			StatementFactKind::While);
		prepare_pa12_condition(node.children[0], control);
		prepare_pa12_substatement(node.children[1], control);
		return;
	}
	case PA10NodeKind::DoStatement:
	{
		if (node.children.size() != 2 || node.children[1].kind !=
			PA10NodeKind::Condition)
			throw std::runtime_error("invalid PA12 do statement");
		const ScopeId control = prepare_pa12_control(node, scope,
			StatementFactKind::Do);
		prepare_pa12_substatement(node.children[0], control);
		prepare_pa12_condition(node.children[1], control);
		return;
	}
	case PA10NodeKind::ForStatement:
	{
		if (node.children.size() < 3 || node.children[0].kind !=
			PA10NodeKind::ForInitStatement)
			throw std::runtime_error("invalid PA12 for statement");
		const ScopeId control = prepare_pa12_control(node, scope,
			StatementFactKind::For);
		const PA10AstNode& init = node.children[0];
		if (!init.children.empty())
		{
			const PA10AstNode& init_child = init.children.front();
			if (init_child.kind == PA10NodeKind::SimpleDeclaration)
			{
				if (declaration_fact(init_child) == NULL)
					process_simple_declaration(init_child, control);
			}
		}
		for (std::size_t i = 1; i + 1 < node.children.size(); ++i)
		{
			if (node.children[i].kind == PA10NodeKind::Condition)
				prepare_pa12_condition(node.children[i], control);
		}
		prepare_pa12_substatement(node.children.back(), control);
		return;
	}
	case PA10NodeKind::CaseStatement:
		if (node.children.size() != 2)
			throw std::runtime_error("invalid PA12 case statement");
		prepare_pa12_statement(node.children.back(), scope);
		return;
	case PA10NodeKind::DefaultStatement:
		if (node.children.size() != 1)
			throw std::runtime_error("invalid PA12 default statement");
		prepare_pa12_statement(node.children.front(), scope);
		return;
	case PA10NodeKind::LabeledStatement:
		for (std::size_t i = 0; i < node.children.size(); ++i)
			prepare_pa12_statement(node.children[i], scope);
		return;
	default:
		return;
	}
}

void PA11SemanticModel::validate_switch_initialization(
	const PA10AstNode& body, ScopeId scope) const
{
	SwitchInitializationState state;
	collect_switch_transfer_points(body, scope, &state);
}

PA11SemanticModel::LookupPointGuard::LookupPointGuard(
	PA11SemanticModel& model, SourcePoint point)
	: model_(model), previous_(model.active_lookup_point_)
{ model_.active_lookup_point_ = point; }
PA11SemanticModel::LookupPointGuard::~LookupPointGuard()
{ model_.active_lookup_point_ = previous_; }

PA11SemanticModel::SemanticTailGuard::SemanticTailGuard(PA11SemanticModel& model)
	: model_(model), semantic_begin_(model.semantic_facts_.size()),
	  children_begin_(model.semantic_children_.size()),
	  aggregate_begin_(model.aggregate_elements_.size()),
	  aggregate_range_entries_begin_(model.aggregate_ranges_.entry_count()),
	  floating_literal_begin_(model.floating_literal_facts_.size()),
	  floating_literal_bytes_begin_(model.floating_literal_bytes_.size()),
	  constant_address_begin_(model.constant_address_facts_.size()),
	  constant_address_bytes_begin_(model.constant_address_literal_bytes_.size()),
	  conversion_begin_(model.conversion_facts_.size()),
	  conversion_base_path_begin_(model.conversion_base_paths_.size()),
	  semantic_base_path_begin_(model.semantic_base_paths_.size()),
	  names_begin_(model.semantic_name_components_.size()), active_(true)
{}
PA11SemanticModel::SemanticTailGuard::~SemanticTailGuard()
{
	discard();
}
void PA11SemanticModel::SemanticTailGuard::commit()
{
	active_ = false;
}
void PA11SemanticModel::SemanticTailGuard::discard()
{
	if (!active_)
		return;
	model_.aggregate_ranges_.truncate(aggregate_range_entries_begin_);
	model_.semantic_facts_.resize(semantic_begin_);
	model_.semantic_children_.resize(children_begin_);
	model_.aggregate_elements_.resize(aggregate_begin_);
	model_.floating_literal_facts_.resize(floating_literal_begin_);
	model_.floating_literal_bytes_.resize(floating_literal_bytes_begin_);
	model_.constant_address_facts_.resize(constant_address_begin_);
	model_.constant_address_literal_bytes_.resize(constant_address_bytes_begin_);
	model_.conversion_facts_.resize(conversion_begin_);
	model_.conversion_base_paths_.resize(conversion_base_path_begin_);
	model_.semantic_base_paths_.resize(semantic_base_path_begin_);
	model_.semantic_name_components_.resize(names_begin_);
	active_ = false;
}
bool PA11SemanticModel::builtin_cast_target(const PA10AstNode& node,
	TypeId* target) const
{
	if (node.kind != PA10NodeKind::IdExpression || !node.has_token)
		return false;
	FundamentalType fundamental_type;
	switch (node.token)
	{
	case SimpleTokenType::KW_BOOL: fundamental_type = FundamentalType::Bool; break;
	case SimpleTokenType::KW_CHAR: fundamental_type = FundamentalType::Char; break;
	case SimpleTokenType::KW_CHAR16_T: fundamental_type = FundamentalType::Char16T; break;
	case SimpleTokenType::KW_CHAR32_T: fundamental_type = FundamentalType::Char32T; break;
	case SimpleTokenType::KW_DOUBLE: fundamental_type = FundamentalType::Double; break;
	case SimpleTokenType::KW_FLOAT: fundamental_type = FundamentalType::Float; break;
	case SimpleTokenType::KW_INT: fundamental_type = FundamentalType::Int; break;
	case SimpleTokenType::KW_LONG: fundamental_type = FundamentalType::LongInt; break;
	case SimpleTokenType::KW_SHORT: fundamental_type = FundamentalType::ShortInt; break;
	case SimpleTokenType::KW_SIGNED: fundamental_type = FundamentalType::Int; break;
	case SimpleTokenType::KW_UNSIGNED: fundamental_type = FundamentalType::UnsignedInt; break;
	case SimpleTokenType::KW_VOID: fundamental_type = FundamentalType::Void; break;
	case SimpleTokenType::KW_WCHAR_T: fundamental_type = FundamentalType::WcharT; break;
	default: return false;
	}
	*target = fundamental(fundamental_type);
	return true;
}
void PA11SemanticModel::dump_pa12(std::ostream& output) const
{
	output << "translation-unit\n";
	for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
		dump_pa12_top_node(output, ast_.root.children[i], global_, 1);
	for (std::size_t i = 0; i < template_specialization_facts_.size(); ++i)
	{
		if (template_specialization_facts_[i].state !=
			TemplateSpecializationState::Complete)
			continue;
		dump_pa12_template_specialization(output,
			template_specialization_facts_[i], 1);
	}
	for (std::size_t i = 0; i < class_function_facts_.size(); ++i)
	{
		const FunctionFactId id = class_function_facts_[i];
		if (!id.valid() || id.value >= function_facts_.size())
			throw std::runtime_error("PA12 class function fact is missing");
		const FunctionFact& function = function_facts_[id.value];
		if (function.node == NULL)
			throw std::runtime_error("PA12 class function node is missing");
		dump_pa12_function(output, *function.node, 1);
	}
	for (std::size_t i = 0; i < synthetic_function_facts_.size(); ++i)
		dump_pa12_synthetic_function(output, synthetic_function_facts_[i], 1);
}
const DeclarationFact* PA11SemanticModel::declaration_fact(const PA10AstNode& node) const
{
	const DeclarationFactId* found = declaration_fact_index_.find(&node);
	if (found == NULL || !found->valid() || found->value >= declaration_facts_.size())
		return NULL;
	return &declaration_facts_[found->value];
}
DeclarationFact* PA11SemanticModel::declaration_fact(const PA10AstNode& node)
{
	return const_cast<DeclarationFact*>(
		static_cast<const PA11SemanticModel*>(this)->declaration_fact(node));
}
const FunctionFact* PA11SemanticModel::function_fact(const PA10AstNode& node) const
{
	const FunctionFactId* found = function_fact_index_.find(&node);
	if (found == NULL || !found->valid() || found->value >= function_facts_.size())
		return NULL;
	return &function_facts_[found->value];
}
FunctionFact* PA11SemanticModel::function_fact(const PA10AstNode& node)
{
	return const_cast<FunctionFact*>(
		static_cast<const PA11SemanticModel*>(this)->function_fact(node));
}
const NamespaceFact* PA11SemanticModel::namespace_fact(const PA10AstNode& node) const
{
	const NamespaceFactId* found = namespace_fact_index_.find(&node);
	if (found == NULL || !found->valid() || found->value >= namespace_facts_.size())
		return NULL;
	return &namespace_facts_[found->value];
}

ScopeId PA11SemanticModel::compound_scope(const PA10AstNode& node) const
{
	const ScopeId* found = compound_scope_index_.find(&node);
	return found == NULL ? ScopeId() : *found;
}
bool PA11SemanticModel::modifiable_lvalue(TypeId type) const
{
	type = strip_reference_type(type);
	while (type_kind(type) == TypeKind::Cv)
	{
		if ((types_[type.value].cv & 1u) != 0)
			return false;
		type = types_[type.value].child;
	}
	return type_kind(type) != TypeKind::Pointer ||
		(types_[type.value].cv & 1u) == 0;
}
TypeId PA11SemanticModel::expression_object_type(TypeId type) const
{
	return strip_reference_type(type);
}
bool PA11SemanticModel::direct_initializer_operand(const PA10AstNode& node,
	ScopeId scope, const PA10AstNode** operand)
{
	if (node.kind != PA10NodeKind::InitDeclarator || node.children.size() != 1)
		return false;
	const PA10AstNode& declarator = node.children.front();
	if (declarator.kind != PA10NodeKind::Declarator ||
		declarator.children.size() != 2 ||
		declarator.children[0].kind != PA10NodeKind::Identifier ||
		declarator.children[1].kind != PA10NodeKind::ParameterClause)
		return false;
	const PA10AstNode& clause = declarator.children[1];
	if (clause.children.size() != 1 ||
		clause.children.front().kind != PA10NodeKind::ParameterDeclaration)
		return false;
	const PA10AstNode& parameter = clause.children.front();
	if (parameter.children.size() != 1 ||
		parameter.children.front().kind != PA10NodeKind::DeclSpecifierSeq)
		return false;
	const PA10AstNode& spec = parameter.children.front();
	if (spec.children.size() != 1 ||
		spec.children.front().kind != PA10NodeKind::DeclSpecifier)
		return false;
	const PA10AstNode& name_node = spec.children.front();
	if (!name_node.identifier_declspecifier || name_node.has_token ||
		name_node.name_parts.empty())
		return false;
	const NamePath name = name_path(name_node);
	const std::vector<ValueRef> values = lookup_value_path(name, scope);
	if (values.empty())
		return false;
	if (lookup_type_path(name, scope).valid() &&
		(values.size() != 1 || values.front().binding.value >= bindings_.size() ||
			(binding(values.front().binding).kind != BindingKind::Variable &&
				binding(values.front().binding).kind != BindingKind::Parameter)))
		return false;
	if (operand != NULL)
		*operand = &name_node;
	return true;
}
ExprInfo PA11SemanticModel::semantic_storage_id(BindingId storage,
	const PA10AstNode* source)
{
	const Binding& value = binding(storage);
	if (value.kind != BindingKind::Variable)
		throw std::runtime_error("PA12 anonymous union storage is not an object");
	SemanticFact fact(SemanticFactKind::IdExpression, value.type,
		SemanticValueCategory::Lvalue, source);
	fact.binding = storage;
	const SemanticFactId result = make_semantic_fact(fact);
	if (value.name.valid())
	{
		NamePath path;
		path.components.push_back(value.name);
		set_semantic_name(result, path);
	}
	return ExprInfo(result, value.type, SemanticValueCategory::Lvalue, false);
}
SemanticFactId PA11SemanticModel::semantic_constructor_action(
	BindingId storage, const PA10AstNode& source,
	const std::vector<const PA10AstNode*>& argument_nodes,
	ScopeId access_scope, ConstructorInitializationContext context)
{
	if (!storage.valid() || storage.value >= binding_owners_.size())
		throw std::runtime_error("PA12 constructor storage is invalid");
	if (!access_scope.valid())
		access_scope = binding_owners_[storage.value];
	const Binding& storage_binding = binding(storage);
	const NamedRecordId record = class_record_for_object_type(storage_binding.type);
	if (!record.valid() || record.value >= named_.size() ||
		named_[record.value].kind != NamedKind::Class)
		throw std::runtime_error("PA12 constructor action needs a class object");
	if (named_[record.value].class_tag == ClassTag::Union &&
		!argument_nodes.empty())
		throw std::runtime_error(
			"PA16 union constructor arguments are outside checkpoint");
	ConstructorSelection selection;
	if (named_[record.value].class_tag == ClassTag::Union)
	{
		const BindingId constructor = ensure_anonymous_union_constructor(record);
		selection = ConstructorSelection(constructor, named_[record.value].scope,
			constructor_callable_type(constructor));
	}
	else
		selection = select_constructor(record, access_scope,
			argument_nodes, true, context);
	if (!selection.valid())
		throw std::runtime_error("PA12 constructor selection is incomplete");
	const BindingId constructor = selection.binding;
	if (function_declaration_kind(constructor) ==
		FunctionDeclarationKind::Deleted)
		throw std::runtime_error("PA12 default construction selects deleted constructor");
	const ExprInfo object = semantic_storage_id(storage, &source);
	const TypeId pointer = make_pointer(object.type);
	SemanticFact unary(SemanticFactKind::UnaryExpression, pointer,
		SemanticValueCategory::Prvalue, &source);
	unary.token = SimpleTokenType::OP_AMP;
	const SemanticFactId address = make_semantic_fact(unary);
	set_semantic_children(address,
		std::vector<SemanticFactId>(1, object.fact));
	SemanticFact call(SemanticFactKind::CallExpression,
		fundamental(FundamentalType::Void), SemanticValueCategory::Prvalue,
		&source);
	call.has_callee = true;
	call.selected_binding = constructor;
	call.selected_scope = selection.scope;
	call.callable_type = selection.callable_type;
	const SemanticFactId call_id = make_semantic_fact(call);
	std::vector<SemanticFactId> call_children;
	call_children.reserve(1 + selection.arguments.size());
	call_children.push_back(address);
	call_children.insert(call_children.end(), selection.arguments.begin(),
		selection.arguments.end());
	set_semantic_children(call_id, call_children);
	SemanticFact action(SemanticFactKind::ConstructorAction, TypeId(),
		SemanticValueCategory::Prvalue, &source);
	action.selected_binding = constructor;
	action.selected_scope = selection.scope;
	action.callable_type = selection.callable_type;
	const SemanticFactId action_id = make_semantic_fact(action);
	set_semantic_children(action_id,
		std::vector<SemanticFactId>(1, call_id));
	return action_id;
}
ExprInfo PA11SemanticModel::semantic_injected_member(
	const PA10AstNode& node, ScopeId scope, BindingId member_id)
{
	(void)scope;
	// This is a storage-backed anonymous-union view rather than a
	// class-owned member selection.  It deliberately has no selected owner;
	// PA15 checks the backing-storage marker before applying the typed
	// class-member projection invariant and preserves its existing unsupported
	// boundary.
	const Binding& member = binding(member_id);
	const BindingSidecar* sidecar = binding_sidecar(member_id);
	if (sidecar == NULL || !sidecar->backing_storage.valid())
		throw std::runtime_error("PA12 injected member has no backing storage");
	const ExprInfo object = semantic_storage_id(sidecar->backing_storage);
	const TypeId type = member_access_type(object.type, member.type, member_id);
	SemanticFact fact(SemanticFactKind::MemberExpression, type,
		SemanticValueCategory::Lvalue, &node);
	fact.token = SimpleTokenType::OP_DOT;
	fact.contains_member_value = true;
	fact.binding = member_id;
	fact.selected_binding = member_id;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, name_path(node));
	set_semantic_children(result,
		std::vector<SemanticFactId>(1, object.fact));
	return ExprInfo(result, type, SemanticValueCategory::Lvalue, false);
}
bool PA11SemanticModel::fundamental_of(TypeId type, FundamentalType* result) const
{
	type = strip_cv_type(expression_object_type(type));
	if (type_kind(type) != TypeKind::Fundamental)
		return false;
	if (result != NULL)
		*result = types_[type.value].fundamental;
	return true;
}
bool PA11SemanticModel::integral_id(TypeId type) const
{
	type = strip_cv_type(expression_object_type(type));
	FundamentalType fundamental_type;
	if (fundamental_of(type, &fundamental_type))
		return integral_type(fundamental_type);
	const NamedRecordId record = named_record_for_type(type);
	return record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Enum &&
		!named_[record.value].scoped_enum;
}
bool PA11SemanticModel::bool_id(TypeId type) const
{
	FundamentalType fundamental_type;
	return fundamental_of(type, &fundamental_type) &&
		fundamental_type == FundamentalType::Bool;
}
bool PA11SemanticModel::floating_id(TypeId type) const
{
	FundamentalType fundamental_type;
	if (!fundamental_of(type, &fundamental_type))
		return false;
	return fundamental_type == FundamentalType::Float ||
		fundamental_type == FundamentalType::Double ||
		fundamental_type == FundamentalType::LongDouble;
}
bool PA11SemanticModel::void_id(TypeId type) const
{
	FundamentalType fundamental_type;
	return fundamental_of(type, &fundamental_type) &&
		fundamental_type == FundamentalType::Void;
}
bool PA11SemanticModel::pointer_id(TypeId type) const
{
	return type_kind(strip_cv_type(expression_object_type(type))) == TypeKind::Pointer;
}
bool PA11SemanticModel::scalar_id(TypeId type) const
{
	return integral_id(type) || pointer_id(type) ||
		type_kind(strip_cv_type(expression_object_type(type))) ==
			TypeKind::MemberPointer ||
		bool_id(type) || floating_id(type) ||
		nullptr_id(type) || enumeration_id(type);
}
bool PA11SemanticModel::nullptr_id(TypeId type) const
{
	FundamentalType fundamental_type;
	return fundamental_of(type, &fundamental_type) &&
		fundamental_type == FundamentalType::NullptrT;
}
unsigned int PA11SemanticModel::integral_rank(TypeId type) const
{
	type = strip_cv_type(expression_object_type(type));
	const NamedRecordId record = named_record_for_type(type);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Enum &&
		!named_[record.value].scoped_enum)
		return integral_rank(promote_integral_type(type));
	FundamentalType fundamental_type;
	if (!fundamental_of(type, &fundamental_type))
		return 0;
	switch (fundamental_type)
	{
	case FundamentalType::Bool: return 0;
	case FundamentalType::SignedChar:
	case FundamentalType::UnsignedChar: return 1;
	case FundamentalType::ShortInt:
	case FundamentalType::UnsignedShortInt:
	case FundamentalType::Char16T: return 2;
	case FundamentalType::Int:
	case FundamentalType::UnsignedInt:
	case FundamentalType::Char:
	case FundamentalType::WcharT:
	case FundamentalType::Char32T: return 3;
	case FundamentalType::LongInt:
	case FundamentalType::UnsignedLongInt: return 4;
	case FundamentalType::LongLongInt:
	case FundamentalType::UnsignedLongLongInt: return 5;
	default: return 0;
	}
}
bool PA11SemanticModel::signed_integral_represents(TypeId signed_type,
	TypeId unsigned_value) const
{
	FundamentalType signed_fundamental, unsigned_fundamental;
	if (!fundamental_of(signed_type, &signed_fundamental) ||
		!fundamental_of(unsigned_value, &unsigned_fundamental) ||
		unsigned_type(signed_fundamental) ||
		!unsigned_type(unsigned_fundamental))
		return false;
	return type_size(signed_type) > type_size(unsigned_value);
}
FundamentalType PA11SemanticModel::unsigned_counterpart(
	FundamentalType type) const
{
	switch (type)
	{
	case FundamentalType::SignedChar: return FundamentalType::UnsignedChar;
	case FundamentalType::ShortInt: return FundamentalType::UnsignedShortInt;
	case FundamentalType::Int: return FundamentalType::UnsignedInt;
	case FundamentalType::LongInt: return FundamentalType::UnsignedLongInt;
	case FundamentalType::LongLongInt: return FundamentalType::UnsignedLongLongInt;
	default: return type;
	}
}
SemanticFactId PA11SemanticModel::make_semantic_fact(const SemanticFact& fact)
{
	const SemanticFactId result(semantic_facts_.size());
	semantic_facts_.push_back(fact);
	return result;
}
void PA11SemanticModel::set_semantic_children(SemanticFactId fact,
	const std::vector<SemanticFactId>& children)
{
	SemanticFact& owner = semantic_facts_[fact.value];
	owner.child_begin = semantic_children_.size();
	owner.child_count = children.size();
	semantic_children_.insert(semantic_children_.end(), children.begin(),
		children.end());
}
void PA11SemanticModel::set_semantic_base_path(SemanticFactId fact,
	const std::vector<NamedRecordId>& path)
{
	if (!fact.valid() || fact.value >= semantic_facts_.size())
		throw std::runtime_error("PA12 base-path fact identity is invalid");
	SemanticFact& owner = semantic_facts_[fact.value];
	if (owner.base_path_begin != InvalidIdentityValue ||
		owner.base_path_count != 0)
		throw std::runtime_error("PA12 base-path fact is already published");
	if (path.empty())
		return;
	if (path.size() > std::numeric_limits<std::size_t>::max() -
		semantic_base_paths_.size())
		throw std::runtime_error("PA12 base-path range overflow");
	for (std::size_t i = 0; i < path.size(); ++i)
		if (!path[i].valid() || path[i].value >= named_.size())
			throw std::runtime_error("PA12 base-path identity is invalid");
	owner.base_path_begin = semantic_base_paths_.size();
	owner.base_path_count = path.size();
	semantic_base_paths_.insert(semantic_base_paths_.end(), path.begin(),
		path.end());
}
void PA11SemanticModel::set_semantic_aggregate_elements(SemanticFactId fact,
	const std::vector<AggregateElementFact>& elements,
	std::size_t total_count)
{
	if (!fact.valid() || fact.value >= semantic_facts_.size())
		throw std::runtime_error("PA12 aggregate fact identity is invalid");
	if (elements.size() > total_count)
		throw std::runtime_error("PA12 aggregate element count exceeds destination");
	if (aggregate_ranges_.find(fact) != NULL)
		throw std::runtime_error("PA12 aggregate fact range is already published");
	const std::size_t begin = aggregate_elements_.size();
	aggregate_elements_.insert(aggregate_elements_.end(), elements.begin(),
		elements.end());
	aggregate_ranges_.set(fact, AggregateFactRange(begin, elements.size(),
		total_count));
}
void PA11SemanticModel::set_semantic_name(SemanticFactId fact, const NamePath& path)
{
	SemanticFact& owner = semantic_facts_[fact.value];
	owner.name_begin = semantic_name_components_.size();
	owner.name_count = path.components.size();
	owner.name_global = path.global;
	semantic_name_components_.insert(semantic_name_components_.end(),
		path.components.begin(), path.components.end());
}
std::string PA11SemanticModel::semantic_name(const SemanticFact& fact) const
{
	NamePath path;
	path.global = fact.name_global;
	for (std::size_t i = 0; i < fact.name_count; ++i)
		path.components.push_back(semantic_name_components_[
			fact.name_begin + i]);
	std::string result = render_name_path(path);
	const BindingSidecar* sidecar = fact.binding.valid() ?
		binding_sidecar(fact.binding) : NULL;
	if (sidecar != NULL && sidecar->template_specialization.valid())
		result += render_template_specialization(sidecar->template_specialization);
	return result;
}
std::string PA11SemanticModel::qualified_binding_name(ScopeId owner, NameId name) const
{
	NamePath path;
	std::vector<NameId> parents;
	ScopeId cursor = owner;
	while (cursor.valid() && cursor != global_)
	{
		const Scope& current = scopes_[cursor.value];
		if (current.kind == ScopeKind::Namespace && current.name.valid())
			parents.push_back(current.name);
		else if (current.kind == ScopeKind::Class && current.record.valid() &&
			current.record.value < named_.size() &&
			named_[current.record.value].name.valid())
			parents.push_back(named_[current.record.value].name);
		cursor = current.parent;
	}
	for (std::size_t i = parents.size(); i != 0; --i)
		path.components.push_back(parents[i - 1]);
	path.components.push_back(name);
	return render_name_path(path);
}
TypeId PA11SemanticModel::function_result_type(TypeId type) const
{
	if (type_kind(type) != TypeKind::Function)
		throw std::runtime_error("PA12 callable is not a function");
	return types_[type.value].result;
}
bool PA11SemanticModel::class_value_type(TypeId type) const
{
	if (!type.valid() || type.value >= types_.size())
		return false;
	const TypeKind kind = type_kind(type);
	if (kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference)
		return false;
	const TypeId object = strip_cv_type(expression_object_type(type));
	return object.valid() && class_scope_for_type(object).valid();
}
bool PA11SemanticModel::empty_class_value_type(TypeId type) const
{
	if (!class_value_type(type))
		return false;
	const TypeId object = strip_cv_type(expression_object_type(type));
	const NamedRecordId record_id = named_record_for_type(object);
	if (!record_id.valid() || record_id.value >= named_.size() ||
		record_id.value >= record_layouts_.size())
		return false;
	const NamedRecord& record = named_[record_id.value];
	if (record.kind != NamedKind::Class || record.class_tag == ClassTag::Union ||
		!record.defined || record.has_base || record.direct_base.valid() ||
		record.direct_base_virtual || record.has_virtual_member ||
		record.has_requested_alignment || !record.scope.valid() ||
		record.scope.value >= scopes_.size() ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != record_id)
		return false;
	const RecordLayout& layout = record_layout(record_id);
	if (layout.state != RecordLayoutState::Complete || layout.size != 1 ||
		layout.alignment != 1 || layout.has_direct_base ||
		!layout.checkpoint_zero_storage_eligible || !layout.members.empty())
		return false;
	const NamedRecordSidecar* sidecar = named_record_sidecar(record_id);
	return sidecar == NULL || (!sidecar->has_constructor_declaration &&
		!sidecar->has_destructor_declaration &&
		!sidecar->has_default_member_initializer);
}
bool PA11SemanticModel::narrow_class_value_constructor(
	const FunctionFact& function) const
{
	const FunctionFact* canonical = &function;
	if (function.constructor_base_entry)
	{
		if (!function.constructor_entry_source.valid() ||
			function.inherited_base_record.valid() ||
			function.inherited_base_constructor.valid() ||
			function.destructor_base_entry ||
			function.destructor_entry_source.valid())
			return false;
		canonical = function_fact_for_binding(function.constructor_entry_source);
		if (canonical == NULL)
			return false;
	}
	else if (function.constructor_entry_source.valid() ||
		function.inherited_base_record.valid() ||
		function.inherited_base_constructor.valid() ||
		function.destructor_base_entry ||
		function.destructor_entry_source.valid())
		return false;
	if (canonical->constructor_base_entry ||
		canonical->constructor_entry_source.valid() ||
		canonical->inherited_base_record.valid() ||
		canonical->inherited_base_constructor.valid() ||
		canonical->destructor_base_entry ||
		canonical->destructor_entry_source.valid())
		return false;
	if (!canonical->is_constructor || !canonical->out_of_class_definition ||
		canonical->node == NULL ||
		!canonical->binding.valid() || canonical->binding.value >= bindings_.size() ||
		canonical->binding.value >= binding_owners_.size() ||
		!canonical->constructor_record.valid() ||
		canonical->constructor_record.value >= named_.size())
		return false;
	const NamedRecord& record = named_[canonical->constructor_record.value];
	if (record.kind != NamedKind::Class || record.class_tag == ClassTag::Union ||
		!record.name.valid() || !record.scope.valid() ||
		record.scope.value >= scopes_.size() || canonical->owner != record.scope ||
		scopes_[record.scope.value].kind != ScopeKind::Class ||
		scopes_[record.scope.value].record != canonical->constructor_record ||
		binding_owners_[canonical->binding.value] != canonical->owner)
		return false;
	const Binding& value = binding(canonical->binding);
	if (value.kind != BindingKind::Function || value.name != record.name ||
		!value.type.valid() || value.type.value >= types_.size() ||
		type_kind(value.type) != TypeKind::Function || !value.has_definition)
		return false;
	const BindingSidecar* sidecar = binding_sidecar(canonical->binding);
	if (sidecar == NULL || sidecar->constructor_record !=
		canonical->constructor_record)
		return false;
	const ValueList* values = scopes_[record.scope.value].values.find(record.name);
	bool canonical_indexed = false;
	if (values != NULL)
		for (std::size_t i = 0; i < values->entries.size(); ++i)
			if (values->entries[i].binding == canonical->binding &&
				values->entries[i].origin == record.scope)
			{
				canonical_indexed = true;
				break;
			}
	if (!canonical_indexed)
		return false;
	const TypeKey& signature = types_[value.type.value];
	return void_id(signature.result) && !signature.variadic &&
		signature.parameters.size() == 1 &&
		empty_class_value_type(signature.parameters.front());
}
TypeId PA11SemanticModel::callable_function_type(TypeId type) const
{
	type = strip_reference_type(type);
	if (type_kind(strip_cv_type(type)) == TypeKind::Pointer)
		type = types_[strip_cv_type(type).value].child;
	type = strip_cv_type(type);
	return type_kind(type) == TypeKind::Function ? type : TypeId();
}
ConversionChoice PA11SemanticModel::conversion_for(const ExprInfo& expression,
	TypeId target, const PA10AstNode* source_node, ScopeId access_scope) const
{
	const BitFieldFact* bit_field = bit_field_fact_for_expression(expression);
	return conversion_for(expression.type, expression.category, target,
		source_node, expression.integer_zero, access_scope,
		bit_field == NULL ? TypeId() : bit_field->operation_type,
		bit_field == NULL ? BindingId() : bit_field->binding);
}
ExprInfo PA11SemanticModel::make_bit_field_reference_temporary(
	const ExprInfo& expression, TypeId target, const PA10AstNode* source_node,
	ScopeId access_scope, const ConversionChoice& binding)
{
	if (!binding.valid || !expression.fact.valid() ||
		expression.fact.value >= semantic_facts_.size() || !target.valid() ||
		(target.value >= types_.size()) ||
		(type_kind(target) != TypeKind::LvalueReference &&
			type_kind(target) != TypeKind::RvalueReference))
		throw std::runtime_error("PA12 bit-field reference temporary is invalid");
	const BitFieldFact* bit_field = bit_field_fact_for_expression(expression);
	if (bit_field == NULL)
		throw std::runtime_error("PA12 bit-field reference owner is missing");
	const TypeId referred = types_[target.value].child;
	const ConversionChoice value = conversion_for(expression, referred,
		source_node, access_scope);
	const PA10AstNode* cast_source = source_node != NULL ? source_node :
		semantic_facts_[expression.fact.value].source;
	if (!value.valid || cast_source == NULL)
		throw std::runtime_error("PA12 bit-field reference value is invalid");
	const SemanticValueCategory category = type_kind(target) ==
		TypeKind::RvalueReference ? SemanticValueCategory::Xvalue :
		SemanticValueCategory::Lvalue;
	const SemanticFactId cast = make_expression_fact(
		SemanticFactKind::CastExpression, target, category, *cast_source,
		std::vector<SemanticFactId>(1, expression.fact));
	set_fact_conversion(cast, add_conversion(expression.type, referred, value));
	set_fact_conversion(cast, add_conversion(referred, target, binding));
	return ExprInfo(cast, target, category, false);
}
void PA11SemanticModel::retarget_constexpr_literal(SemanticFactId fact_id, TypeId target)
{
	if (!fact_id.valid() || fact_id.value >= semantic_facts_.size() || !target.valid() ||
		!complete_object_type(target))
		return;
	SemanticFact& fact = semantic_facts_[fact_id.value];
	if (fact.kind != SemanticFactKind::Literal || fact.source == NULL)
		return;
	const PA10AstNode& source = *fact.source;
	if (pointer_id(target))
	{
		if (integer_zero(source) || (source.kind == PA10NodeKind::KeywordLiteral &&
			source.token == SimpleTokenType::KW_NULLPTR))
			fact.type = strip_top_cv_type(target);
		return;
	}
	if ((source.kind == PA10NodeKind::Literal || source.kind == PA10NodeKind::KeywordLiteral) &&
		integral_id(target) && integral_id(fact.type))
		fact.type = target;
}
TypeId PA11SemanticModel::common_integral_type(TypeId left, TypeId right) const
{
	left = promote_integral_type(left);
	right = promote_integral_type(right);
	const unsigned int left_rank = integral_rank(left);
	const unsigned int right_rank = integral_rank(right);
	FundamentalType left_fundamental, right_fundamental;
	if (fundamental_of(left, &left_fundamental) &&
		fundamental_of(right, &right_fundamental))
	{
		const bool left_unsigned = unsigned_type(left_fundamental);
		const bool right_unsigned = unsigned_type(right_fundamental);
		if (left_unsigned == right_unsigned)
			return left_rank >= right_rank ? left : right;
		if (left_unsigned)
			return left_rank >= right_rank ? left :
				signed_integral_represents(right, left) ? right :
				fundamental(unsigned_counterpart(right_fundamental));
		if (right_rank >= left_rank)
			return right;
		return signed_integral_represents(left, right) ? left :
			fundamental(unsigned_counterpart(left_fundamental));
	}
	return left_rank > right_rank ? left : right;
}
TypeId PA11SemanticModel::integral_operation_type(
	const ExprInfo& expression) const
{
	const BitFieldFact* bit_field = bit_field_fact_for_expression(expression);
	return bit_field == NULL ? promote_integral_type(expression.type) :
		bit_field->operation_type;
}
unsigned int PA11SemanticModel::floating_rank(TypeId type) const
{
	FundamentalType fundamental_type;
	if (!fundamental_of(type, &fundamental_type))
		return 0;
	switch (fundamental_type)
	{
	case FundamentalType::Float: return 1;
	case FundamentalType::Double: return 2;
	case FundamentalType::LongDouble: return 3;
	default: return 0;
	}
}
TypeId PA11SemanticModel::common_arithmetic_type(TypeId left, TypeId right) const
{
	left = strip_cv_type(expression_object_type(left));
	right = strip_cv_type(expression_object_type(right));
	if (!floating_id(left) && !floating_id(right))
		return common_integral_type(left, right);
	if (floating_id(left) && floating_id(right))
	{
		const unsigned int rank = std::max(floating_rank(left), floating_rank(right));
		return fundamental(rank == 3 ? FundamentalType::LongDouble :
			rank == 2 ? FundamentalType::Double : FundamentalType::Float);
	}
	return floating_id(left) ? left : right;
}
void PA11SemanticModel::record_builtin_conversion(const ExprInfo& expression,
	TypeId target, ScopeId access_scope)
{
	const PA10AstNode* source = expression.fact.valid() &&
		expression.fact.value < semantic_facts_.size() ?
		semantic_facts_[expression.fact.value].source : NULL;
	const ConversionChoice choice = conversion_for(expression, target, source,
		access_scope);
	if (!choice.valid)
		throw std::runtime_error("PA12 invalid built-in conversion");
	// A cast owns its selected source-to-target conversion.  An exact-target
	// prvalue needs no later contextual identity conversion.
	if (choice.kind == ConversionKind::Identity && expression.fact.valid() &&
		expression.fact.value < semantic_facts_.size() &&
		semantic_facts_[expression.fact.value].kind ==
			SemanticFactKind::CastExpression && expression.type == target)
		return;
	set_fact_conversion(expression.fact, add_conversion(expression.type, target,
		choice));
}
SemanticFactId PA11SemanticModel::make_expression_fact(SemanticFactKind kind, TypeId type,
	SemanticValueCategory category, const PA10AstNode& node,
	const std::vector<SemanticFactId>& children)
{
	SemanticFact fact(kind, type, category, &node);
	fact.token = node.token;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_children(result, children);
	// A cast is a typed value boundary.  Carry only the direct canonical-truth
	// marker across that boundary so a bool-to-nonbool conversion owned by the
	// cast can retain its PA15 disposition.  Member provenance is finalized from
	// the explicit typed result edges after PA12 construction.
	if (kind == SemanticFactKind::CastExpression && children.size() == 1 &&
		children.front().valid() && children.front().value < semantic_facts_.size())
	{
		const SemanticFact& source = semantic_facts_[children.front().value];
		if (source.canonical_truth && source.direct_bool_boundary)
		{
			semantic_facts_[result.value].canonical_truth = true;
			semantic_facts_[result.value].direct_bool_boundary = true;
		}
	}
	return result;
}
ExprInfo PA11SemanticModel::semantic_enumerator_expression(
	const PA10AstNode& node, BindingId binding_id, ScopeId owner)
{
	if (!binding_id.valid() || binding_id.value >= bindings_.size() ||
		binding_id.value >= binding_owners_.size())
		throw std::runtime_error("PA12 enumerator binding is invalid");
	const ScopeId canonical_owner = binding_owners_[binding_id.value];
	if (!canonical_owner.valid() || canonical_owner.value >= scopes_.size() ||
		(owner.valid() && (owner.value >= scopes_.size() ||
			owner != canonical_owner)))
		throw std::runtime_error("PA12 enumerator owner is invalid");
	const Binding& value = binding(binding_id);
	if (value.kind != BindingKind::Enumerator || !value.type.valid() ||
		value.type.value >= types_.size() || !value.has_value)
		throw std::runtime_error("PA12 enumerator fact is invalid");
	const NamedRecordId record = named_record_for_type(value.type);
	if (!record.valid() || record.value >= named_.size() ||
		named_[record.value].kind != NamedKind::Enum ||
		!named_[record.value].has_underlying ||
		!named_[record.value].underlying.valid())
		throw std::runtime_error("PA12 enumerator type is invalid");
	const bool value_unsigned = unsigned_integral_type(value.type);
	const __int128 raw_value = value.value_unsigned ?
		static_cast<__int128>(value.value_bits) :
		static_cast<__int128>(value.value);
	SemanticFact fact(SemanticFactKind::Literal, value.type,
		SemanticValueCategory::Prvalue, &node);
	fact.binding = binding_id;
	fact.selected_binding = binding_id;
	fact.selected_scope = owner.valid() ? owner : canonical_owner;
	fact.has_literal_value = true;
	fact.literal_value_unsigned = value_unsigned;
	if (value_unsigned)
	{
		const std::size_t bytes = type_size(value.type);
		if (bytes == 0 || bytes > sizeof(std::uint64_t))
			throw std::runtime_error("PA12 enumerator width is invalid");
		const std::size_t width = bytes * 8;
		if (width >= sizeof(__int128) * 8)
			throw std::runtime_error("PA12 enumerator shift width is invalid");
		const __int128 modulus = static_cast<__int128>(1) << width;
		__int128 normalized = raw_value % modulus;
		if (normalized < 0)
			normalized += modulus;
		fact.literal_value = static_cast<std::uint64_t>(normalized);
	}
	else
	{
		fact.literal_value_negative = raw_value < 0;
		fact.literal_value = raw_value < 0 ?
			static_cast<std::uint64_t>(-(raw_value + 1)) + 1 :
			static_cast<std::uint64_t>(raw_value);
	}
	return ExprInfo(make_semantic_fact(fact), value.type,
		SemanticValueCategory::Prvalue, false);
}

ExprInfo PA11SemanticModel::semantic_id_expression(const PA10AstNode& node, ScopeId scope)
{
	// Each identifier refines an enclosing control/call source point.
	LookupPointGuard lookup_point(*this, SourcePoint(node.source_begin));
	if (has_template_id(node))
		throw std::runtime_error("PA12 template-id requires a target");
	const NamePath path = name_path(node);
	const BindingId this_id = implicit_this_binding(scope);
	if (this_id.valid() && !path.components.empty())
	{
		const Binding& this_binding = binding(this_id);
		const TypeId this_pointer = strip_cv_type(expression_object_type(
			this_binding.type));
		if (this_binding.kind != BindingKind::Parameter ||
			type_kind(this_pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 implicit this binding is invalid");
		const TypeId this_record = types_[this_pointer.value].child;
		MemberLookup selection;
		bool member_candidate = false;
		if (!path.global && path.components.size() == 1)
		{
			selection = unqualified_member_lookup(this_record, path.last(),
				scope);
			member_candidate = selection.owner.valid() &&
				selection.owner.value < scopes_.size() &&
				scopes_[selection.owner.value].kind == ScopeKind::Class;
		}
		else if (path.components.size() > 1)
		{
			NamePath qualifier;
			qualifier.global = path.global;
			qualifier.components.assign(path.components.begin(),
				path.components.end() - 1);
			const TypeId qualifier_type = lookup_type_path(qualifier, scope);
			const ScopeId qualifier_scope = class_scope_for_type(qualifier_type);
			if (qualifier_scope.valid() && member_base_path(this_record,
				qualifier_scope, NULL))
			{
				selection = member_lookup(qualifier_type, path.last());
				member_candidate = selection.owner.valid() &&
					selection.owner.value < scopes_.size() &&
					scopes_[selection.owner.value].kind == ScopeKind::Class;
			}
		}
		if (member_candidate && (selection.kind == MemberLookupKind::Type ||
			selection.kind == MemberLookupKind::Blocked))
			throw std::runtime_error("PA12 inherited member name is unsupported");
		if (member_candidate && selection.kind == MemberLookupKind::Value &&
			selection.binding.valid())
		{
			const Binding& member = binding(selection.binding);
			bool static_member_claimed = false;
			const ExprInfo static_member = semantic_static_data_member(node, scope, path, selection, &static_member_claimed);
			if (static_member.fact.valid()) return static_member;
			const BindingSidecar* sidecar = binding_sidecar(selection.binding);
			if (member.kind == BindingKind::Variable &&
				!is_static_member(selection.binding) &&
				(sidecar == NULL || !sidecar->backing_storage.valid()))
			{
				if (!member_accessible(selection.binding, selection.owner, scope,
					this_record, selection.has_access_override,
					selection.access_override, selection.access_view_owner))
					throw std::runtime_error("PA12 record member is inaccessible");
				const ExprInfo object = semantic_this_expression(node, this_id);
				const TypeId member_type = member_access_type(this_record,
					member.type, selection.binding);
				SemanticFact fact(SemanticFactKind::MemberExpression,
					member_type, SemanticValueCategory::Lvalue, &node);
				fact.token = SimpleTokenType::OP_ARROW;
				fact.contains_member_value = true;
				fact.binding = selection.binding;
				fact.selected_binding = selection.binding;
				fact.selected_scope = selection.owner;
				const SemanticFactId result = make_semantic_fact(fact);
				set_semantic_name(result, path);
				set_semantic_children(result,
					std::vector<SemanticFactId>(1, object.fact));
				return ExprInfo(result, member_type,
					SemanticValueCategory::Lvalue, false);
			}
			if (static_member_claimed) throw std::runtime_error("PA12 class member requires an object");
		}
	}
	const ExprInfo static_member = this_id.valid() ? ExprInfo() : semantic_static_data(node, scope, path);
	if (static_member.fact.valid()) return static_member;
	const std::vector<ValueRef> values = lookup_value_path(path, scope);
	if (values.empty())
		throw std::runtime_error("PA12 unknown expression name");
	if (values.size() != 1)
		throw std::runtime_error("PA12 overloaded id requires a target");
	const Binding& value = binding(values.front().binding);
	TypeId type = value.type;
	if (value.kind == BindingKind::Function)
		type = member_function_expression_type(type, values.front().scope,
			values.front().binding);
	SemanticValueCategory category = SemanticValueCategory::Lvalue;
	if (value.kind == BindingKind::Enumerator)
		return semantic_enumerator_expression(node, values.front().binding,
			values.front().scope);
	const BindingSidecar* sidecar = binding_sidecar(values.front().binding);
	if (sidecar != NULL && sidecar->backing_storage.valid())
		return semantic_injected_member(node, scope, values.front().binding);
	else if (type_kind(type) == TypeKind::LvalueReference ||
		type_kind(type) == TypeKind::RvalueReference)
		type = types_[type.value].child;
	SemanticFact fact(SemanticFactKind::IdExpression, type, category, &node);
	fact.binding = values.front().binding;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, path);
	return ExprInfo(result, type, category, false);
}
ExprInfo PA11SemanticModel::semantic_postfix_expression(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 1 || !node.has_token ||
		(node.token != SimpleTokenType::OP_INC &&
			 node.token != SimpleTokenType::OP_DEC))
		throw std::runtime_error("PA12 invalid postfix expression");
	const ExprInfo operand = semantic_expression(node.children.front(), scope);
	const TypeId operand_object = strip_cv_type(expression_object_type(operand.type));
	const NamedRecordId operand_record = named_record_for_type(operand_object);
	if (operand_record.valid() && operand_record.value < named_.size() &&
		(named_[operand_record.value].kind == NamedKind::Class ||
			named_[operand_record.value].kind == NamedKind::Enum))
	{
		// The postfix discriminator is a typed synthetic int argument.  It is
		// owned by this bounded overload probe and is committed only if the
		// selected operator call consumes it.
		SemanticTailGuard overload_tail(*this);
		SemanticFact tag(SemanticFactKind::Literal,
			fundamental(FundamentalType::Int),
			SemanticValueCategory::Prvalue, &node);
		tag.has_literal_value = true;
		const SemanticFactId tag_fact = make_semantic_fact(tag);
		const ExprInfo tag_info(tag_fact, tag.type,
			SemanticValueCategory::Prvalue, true);
		std::vector<TypeId> associated_objects;
		associated_objects.push_back(operand.type);
		std::vector<const PA10AstNode*> member_nodes;
		member_nodes.push_back(&node);
		std::vector<ExprInfo> member_arguments;
		member_arguments.push_back(tag_info);
		std::vector<const PA10AstNode*> nonmember_nodes;
		nonmember_nodes.push_back(&node.children.front());
		nonmember_nodes.push_back(&node);
		std::vector<ExprInfo> nonmember_arguments;
		nonmember_arguments.push_back(operand);
		nonmember_arguments.push_back(tag_info);
		const ExprInfo overloaded = semantic_operator_call(node, scope,
			PA10OperatorFunctionKind::Token, node.token, operand,
			associated_objects, member_nodes, member_arguments,
			nonmember_nodes, nonmember_arguments);
		if (overloaded.fact.valid())
		{
			overload_tail.commit();
			return overloaded;
		}
		overload_tail.discard();
	}
	if (operand.category != SemanticValueCategory::Lvalue ||
		!modifiable_lvalue(operand.type) ||
		(!integral_id(operand.type) && !floating_id(operand.type) &&
			!pointer_id(operand.type)))
		throw std::runtime_error("PA12 postfix requires modifiable lvalue");
	if (node.token == SimpleTokenType::OP_DEC)
	{
		const BitFieldFact* bit_field = bit_field_fact_for_expression(operand);
		FundamentalType storage_fundamental;
		if (bit_field != NULL && fundamental_of(bit_field->storage_type,
			&storage_fundamental) && storage_fundamental == FundamentalType::Bool)
			throw std::runtime_error(
				"PA12 decrement of a bool bit-field is not allowed");
	}
	const TypeId type = strip_top_cv_type(operand.type);
	record_builtin_conversion(operand, integral_id(operand.type) ?
		integral_operation_type(operand) : type);
	return ExprInfo(make_expression_fact(SemanticFactKind::PostfixExpression,
		type, SemanticValueCategory::Prvalue, node,
		std::vector<SemanticFactId>(1, operand.fact)), type,
		SemanticValueCategory::Prvalue, false);
}
bool PA11SemanticModel::has_implicit_this_result(
	ScopeId scope, SimpleTokenType token,
	const std::vector<SemanticFactId>& children) const
{
	const BindingId this_id = implicit_this_binding(scope);
	if (!this_id.valid())
		return false;
	const std::size_t begin = token == SimpleTokenType::OP_COMMA ? 1 : 0;
	for (std::size_t i = begin; i < children.size(); ++i)
		if (semantic_facts_[children[i].value].binding == this_id)
			return true;
	return false;
}
ExprInfo PA11SemanticModel::semantic_binary_expression(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 2 || !node.has_token)
		throw std::runtime_error("PA12 invalid binary expression");
	const ExprInfo left = semantic_expression(node.children[0], scope);
	const ExprInfo right = semantic_expression(node.children[1], scope);
	TypeId type;
	TypeId operation_type;
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	const TypeId left_object = strip_cv_type(expression_object_type(left.type));
	const TypeId right_object = strip_cv_type(expression_object_type(right.type));
	const NamedRecordId left_record = named_record_for_type(left_object);
	const bool same_enum = left_object == right_object &&
		left_record.valid() && left_record.value < named_.size() &&
		named_[left_record.value].kind == NamedKind::Enum;
	const bool left_array = type_kind(left_object) == TypeKind::Array;
	const bool right_array = type_kind(right_object) == TypeKind::Array;
	const bool left_pointer = pointer_id(left.type) || left_array;
	const bool right_pointer = pointer_id(right.type) || right_array;
	const bool left_arithmetic = integral_id(left.type) || floating_id(left.type);
	const bool right_arithmetic = integral_id(right.type) || floating_id(right.type);
	const TypeId left_integral_operation = integral_id(left.type) ?
		integral_operation_type(left) : left.type;
	const TypeId right_integral_operation = integral_id(right.type) ?
		integral_operation_type(right) : right.type;
	const TypeId left_arithmetic_operation = integral_id(left.type) ?
		left_integral_operation : left.type;
	const TypeId right_arithmetic_operation = integral_id(right.type) ?
		right_integral_operation : right.type;
	const bool left_size_type = semantic_facts_[left.fact.value].kind ==
		SemanticFactKind::SizeofExpression ||
		semantic_facts_[left.fact.value].size_type_derived;
	const bool right_size_type = semantic_facts_[right.fact.value].kind ==
		SemanticFactKind::SizeofExpression ||
		semantic_facts_[right.fact.value].size_type_derived;
	const bool size_type_derived = left_size_type || right_size_type;
	const TypeId left_pointer_type = left_array ? make_pointer(
		types_[left_object.value].child) : strip_top_cv_type(left.type);
	const TypeId right_pointer_type = right_array ? make_pointer(
		types_[right_object.value].child) : strip_top_cv_type(right.type);
	std::vector<TypeId> associated_objects;
	associated_objects.push_back(left.type);
	associated_objects.push_back(right.type);
	std::vector<const PA10AstNode*> member_nodes;
	member_nodes.push_back(&node.children[1]);
	std::vector<ExprInfo> member_arguments;
	member_arguments.push_back(right);
	std::vector<const PA10AstNode*> nonmember_nodes;
	nonmember_nodes.push_back(&node.children[0]);
	nonmember_nodes.push_back(&node.children[1]);
	std::vector<ExprInfo> nonmember_arguments;
	nonmember_arguments.push_back(left);
	nonmember_arguments.push_back(right);
	const ExprInfo overloaded = semantic_operator_call(node, scope,
		PA10OperatorFunctionKind::Token, node.token, left,
		associated_objects, member_nodes, member_arguments,
		nonmember_nodes, nonmember_arguments);
	if (overloaded.fact.valid())
		return overloaded;
	switch (node.token)
	{
	case SimpleTokenType::OP_COMMA:
		type = right.type;
		operation_type = type;
		category = right.category;
		break;
	case SimpleTokenType::OP_PLUS:
	case SimpleTokenType::OP_MINUS:
		if (left_pointer && integral_id(right.type))
		{
			type = left_pointer_type;
			operation_type = type;
			record_builtin_conversion(left, left_pointer_type);
			record_builtin_conversion(right, right_integral_operation);
		}
		else if (node.token == SimpleTokenType::OP_PLUS &&
			right_pointer && integral_id(left.type))
		{
			type = right_pointer_type;
			operation_type = type;
			record_builtin_conversion(right, right_pointer_type);
			record_builtin_conversion(left, left_integral_operation);
		}
		else if (left_pointer && right_pointer &&
			node.token == SimpleTokenType::OP_MINUS)
		{
			const TypeId common_pointer = pointer_subtraction_common_type(
				left_pointer_type, right_pointer_type);
			if (!common_pointer.valid())
				throw std::runtime_error("PA12 incompatible pointer subtraction");
			record_builtin_conversion(left, common_pointer, scope);
			record_builtin_conversion(right, common_pointer, scope);
			type = fundamental(FundamentalType::LongInt);
			operation_type = type;
		}
		else if (left_arithmetic && right_arithmetic)
		{
			type = common_arithmetic_type(left_arithmetic_operation,
				right_arithmetic_operation);
			operation_type = type;
			record_builtin_conversion(left, type);
			record_builtin_conversion(right, type);
		}
		else
			throw std::runtime_error("PA12 invalid addition operands");
		break;
	case SimpleTokenType::OP_STAR:
	case SimpleTokenType::OP_DIV:
		if (left_arithmetic && right_arithmetic)
		{
			type = common_arithmetic_type(left_arithmetic_operation,
				right_arithmetic_operation);
			operation_type = type;
			record_builtin_conversion(left, type);
			record_builtin_conversion(right, type);
		}
		else
			throw std::runtime_error("PA12 invalid arithmetic operands");
		break;
	case SimpleTokenType::OP_MOD:
	case SimpleTokenType::OP_BOR:
	case SimpleTokenType::OP_XOR:
	case SimpleTokenType::OP_AMP:
		if (!integral_id(left.type) || !integral_id(right.type))
			throw std::runtime_error("PA12 invalid integral operands");
		type = common_integral_type(left_integral_operation,
			right_integral_operation);
		operation_type = type;
		record_builtin_conversion(left, type);
		record_builtin_conversion(right, type);
		break;
	case SimpleTokenType::OP_LSHIFT:
	case SimpleTokenType::OP_RSHIFT:
	{
		if (!integral_id(left.type) || !integral_id(right.type))
			throw std::runtime_error("PA12 invalid shift operands");
		const TypeId promoted_left = left_integral_operation;
		const TypeId promoted_right = right_integral_operation;
		record_builtin_conversion(left, promoted_left);
		record_builtin_conversion(right, promoted_right);
		type = promoted_left;
		operation_type = type;
		break;
	}
	case SimpleTokenType::OP_LAND:
	case SimpleTokenType::OP_LOR:
		if (!scalar_id(left.type) || !scalar_id(right.type))
			throw std::runtime_error("PA12 invalid logical operands");
		record_builtin_conversion(left, fundamental(FundamentalType::Bool));
		record_builtin_conversion(right, fundamental(FundamentalType::Bool));
		type = fundamental(FundamentalType::Bool);
		operation_type = type;
		break;
	case SimpleTokenType::OP_EQ:
	case SimpleTokenType::OP_NE:
	case SimpleTokenType::OP_LT:
	case SimpleTokenType::OP_LE:
	case SimpleTokenType::OP_GT:
	case SimpleTokenType::OP_GE:
		if (left_pointer && right_pointer)
		{
			if (!pointer_common_type_convertible(left_pointer_type,
				right_pointer_type) &&
				!pointer_common_type_convertible(right_pointer_type,
					left_pointer_type))
				throw std::runtime_error("PA12 incompatible pointer comparison");
			const TypeId common_pointer = pointer_common_type_convertible(
				left_pointer_type, right_pointer_type) ? right_pointer_type :
				left_pointer_type;
			operation_type = common_pointer;
			record_builtin_conversion(left, common_pointer, scope);
			record_builtin_conversion(right, common_pointer, scope);
		}
		else if (left_pointer || right_pointer)
		{
			const ExprInfo& other = left_pointer ? right : left;
			const bool null_value = nullptr_id(other.type) || other.integer_zero;
			if (!null_value)
				throw std::runtime_error("PA12 invalid pointer comparison");
			if (node.token != SimpleTokenType::OP_EQ &&
				node.token != SimpleTokenType::OP_NE)
				throw std::runtime_error("PA12 invalid pointer relational comparison");
			const TypeId pointer_type = left_pointer ? left_pointer_type :
				right_pointer_type;
			operation_type = pointer_type;
			record_builtin_conversion(left_pointer ? left : right, pointer_type);
			record_builtin_conversion(other, pointer_type);
		}
		else if (nullptr_id(left.type) && nullptr_id(right.type))
		{
			if (node.token != SimpleTokenType::OP_EQ &&
				node.token != SimpleTokenType::OP_NE)
				throw std::runtime_error("PA12 invalid nullptr comparison");
		}
		else if (same_enum)
		{
			operation_type = left_object;
			record_builtin_conversion(left, left_object);
			record_builtin_conversion(right, right_object);
		}
		else if (left_arithmetic && right_arithmetic)
		{
			type = common_arithmetic_type(left_arithmetic_operation,
				right_arithmetic_operation);
			operation_type = type;
			record_builtin_conversion(left, type);
			record_builtin_conversion(right, type);
		}
		else
			throw std::runtime_error("PA12 invalid comparison operands");
		type = fundamental(FundamentalType::Bool);
		break;
	default:
		throw std::runtime_error("PA12 unsupported binary operator");
	}
	std::vector<SemanticFactId> children;
	children.push_back(left.fact);
	children.push_back(right.fact);
	const SemanticFactId result = make_expression_fact(
		SemanticFactKind::BinaryExpression, type, category, node, children);
	SemanticFact& result_fact = semantic_facts_[result.value];
	result_fact.operation_type = operation_type;
	result_fact.size_type_derived = size_type_derived;
	const bool comparison = node.token == SimpleTokenType::OP_EQ ||
		node.token == SimpleTokenType::OP_NE ||
		node.token == SimpleTokenType::OP_LT ||
		node.token == SimpleTokenType::OP_LE ||
		node.token == SimpleTokenType::OP_GT ||
		node.token == SimpleTokenType::OP_GE;
	result_fact.canonical_truth = bool_id(type) &&
		(comparison || node.token == SimpleTokenType::OP_LAND ||
			node.token == SimpleTokenType::OP_LOR);
	result_fact.contains_member_value = has_implicit_this_result(
		scope, node.token, children);
	result_fact.direct_bool_boundary = result_fact.canonical_truth &&
		result_fact.contains_member_value;
	return ExprInfo(result, type, category, false);
}
ExprInfo PA11SemanticModel::semantic_assignment_expression(const PA10AstNode& node,
	ScopeId scope)
{
	if (node.children.size() != 2 || !node.has_token)
		throw std::runtime_error("PA12 invalid assignment expression");
	const ExprInfo left = semantic_expression(node.children[0], scope);
	if (left.category != SemanticValueCategory::Lvalue)
		throw std::runtime_error("PA12 assignment requires lvalue");
	if (!modifiable_lvalue(left.type))
		throw std::runtime_error("PA12 assignment requires modifiable lvalue");
	const TypeId target = expression_object_type(left.type);
	const ExprInfo right = node.token == SimpleTokenType::OP_ASS ?
		semantic_expression_for_target(node.children[1], scope, target) :
		semantic_expression(node.children[1], scope);
	const TypeId left_operation = integral_id(target) ?
		integral_operation_type(left) : target;
	const TypeId right_operation = integral_id(right.type) ?
		integral_operation_type(right) : right.type;
	if (node.token == SimpleTokenType::OP_ASS)
	{
		// Simple assignment has the same typed operator boundary as compound
		// assignment.  Probe it before the builtin conversion so an lvalue
		// result such as Iter& from operator* remains the member object and
		// selects Iter::operator=(int).
		std::vector<TypeId> associated_objects;
		associated_objects.push_back(left.type);
		associated_objects.push_back(right.type);
		std::vector<const PA10AstNode*> member_nodes(1, &node.children[1]);
		std::vector<ExprInfo> member_arguments(1, right);
		std::vector<const PA10AstNode*> nonmember_nodes;
		nonmember_nodes.push_back(&node.children[0]);
		nonmember_nodes.push_back(&node.children[1]);
		std::vector<ExprInfo> nonmember_arguments;
		nonmember_arguments.push_back(left);
		nonmember_arguments.push_back(right);
		const ExprInfo overloaded = semantic_operator_call(node, scope,
			PA10OperatorFunctionKind::Token, node.token, left,
			associated_objects, member_nodes, member_arguments,
			nonmember_nodes, nonmember_arguments);
		if (overloaded.fact.valid())
			return overloaded;
		apply_context_conversion(right, target,
			semantic_facts_[right.fact.value].source, scope);
	}
	else
	{
		// Compound assignment operators have the same ordinary overloaded
		// operator boundary as their corresponding binary token.  Keep the
		// assignment target as the implicit object for a member candidate and
		// retain the builtin compound path only when no typed overload is viable.
		std::vector<TypeId> associated_objects;
		associated_objects.push_back(left.type);
		associated_objects.push_back(right.type);
		std::vector<const PA10AstNode*> member_nodes(1, &node.children[1]);
		std::vector<ExprInfo> member_arguments(1, right);
		std::vector<const PA10AstNode*> nonmember_nodes;
		nonmember_nodes.push_back(&node.children[0]);
		nonmember_nodes.push_back(&node.children[1]);
		std::vector<ExprInfo> nonmember_arguments;
		nonmember_arguments.push_back(left);
		nonmember_arguments.push_back(right);
		const ExprInfo overloaded = semantic_operator_call(node, scope,
			PA10OperatorFunctionKind::Token, node.token, left,
			associated_objects, member_nodes, member_arguments,
			nonmember_nodes, nonmember_arguments);
		if (overloaded.fact.valid())
			return overloaded;
		const bool pointer_plus = pointer_id(target) &&
			(node.token == SimpleTokenType::OP_PLUSASS || node.token == SimpleTokenType::OP_MINUSASS);
		if (pointer_plus)
		{
			if (!integral_id(right.type))
				throw std::runtime_error("PA12 pointer compound assignment requires integral");
			record_builtin_conversion(left, target);
			record_builtin_conversion(right, right_operation);
		}
		else
		{
			const bool arithmetic_operator = node.token == SimpleTokenType::OP_STARASS ||
				node.token == SimpleTokenType::OP_DIVASS;
			const bool integral_operator =
				node.token == SimpleTokenType::OP_MODASS || node.token == SimpleTokenType::OP_BORASS ||
				node.token == SimpleTokenType::OP_XORASS || node.token == SimpleTokenType::OP_BANDASS ||
				node.token == SimpleTokenType::OP_LSHIFTASS || node.token == SimpleTokenType::OP_RSHIFTASS;
			if (arithmetic_operator &&
				(!floating_id(target) && !integral_id(target)))
				throw std::runtime_error("PA12 invalid arithmetic compound assignment");
			if (integral_operator && !integral_id(target))
				throw std::runtime_error("PA12 invalid integral compound assignment");
			if (!arithmetic_operator && !integral_operator &&
				(!integral_id(target) && !floating_id(target)))
				throw std::runtime_error("PA12 invalid compound assignment target");
			if ((arithmetic_operator &&
				(!integral_id(right.type) && !floating_id(right.type))) ||
				(integral_operator && !integral_id(right.type)) ||
				(!arithmetic_operator && !integral_operator &&
					(!integral_id(right.type) && !floating_id(right.type))))
				throw std::runtime_error("PA12 invalid compound assignment operands");
			if (integral_operator &&
				(node.token == SimpleTokenType::OP_LSHIFTASS ||
					node.token == SimpleTokenType::OP_RSHIFTASS))
			{
				record_builtin_conversion(left, left_operation);
				record_builtin_conversion(right,
					right_operation);
			}
			else
			{
				const TypeId operation_type = integral_operator ?
					common_integral_type(left_operation, right_operation) :
					common_arithmetic_type(left_operation, right_operation);
				record_builtin_conversion(left, operation_type);
				record_builtin_conversion(right, operation_type);
			}
		}
	}
	std::vector<SemanticFactId> children;
	children.push_back(left.fact);
	children.push_back(right.fact);
	const SemanticFactId result = make_expression_fact(
		SemanticFactKind::AssignmentExpression,
		target, SemanticValueCategory::Lvalue, node, children);
	if (left.fact.valid() && left.fact.value < semantic_facts_.size() &&
		semantic_facts_[left.fact.value].kind == SemanticFactKind::IdExpression &&
		semantic_facts_[left.fact.value].binding.valid() &&
		semantic_facts_[left.fact.value].binding.value < bindings_.size() &&
		binding(semantic_facts_[left.fact.value].binding).kind ==
			BindingKind::Variable)
		semantic_facts_[result.value].binding =
			semantic_facts_[left.fact.value].binding;
	return ExprInfo(result, target, SemanticValueCategory::Lvalue, false);
}
ExprInfo PA11SemanticModel::semantic_conditional_expression(const PA10AstNode& node,
	ScopeId scope)
{
	if (node.children.size() != 3)
		throw std::runtime_error("PA12 invalid conditional expression");
	const ExprInfo condition = semantic_expression(node.children[0], scope);
	if (!scalar_id(condition.type))
		throw std::runtime_error("PA12 conditional requires scalar condition");
	record_builtin_conversion(condition, fundamental(FundamentalType::Bool));
	const ExprInfo when_true = semantic_expression(node.children[1], scope);
	const ExprInfo when_false = semantic_expression(node.children[2], scope);
	const TypeId true_arithmetic_operation = integral_id(when_true.type) ?
		integral_operation_type(when_true) : when_true.type;
	const TypeId false_arithmetic_operation = integral_id(when_false.type) ?
		integral_operation_type(when_false) : when_false.type;
	TypeId type;
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	const TypeId true_object = expression_object_type(when_true.type);
	const TypeId false_object = expression_object_type(when_false.type);
	const TypeId true_unqualified = strip_cv_type(true_object);
	const TypeId false_unqualified = strip_cv_type(false_object);
	const bool true_array = type_kind(true_unqualified) == TypeKind::Array;
	const bool false_array = type_kind(false_unqualified) == TypeKind::Array;
	const bool true_pointer = pointer_id(when_true.type) || true_array;
	const bool false_pointer = pointer_id(when_false.type) || false_array;
	const TypeId true_pointer_type = true_array ? make_pointer(
		types_[true_unqualified.value].child) : strip_top_cv_type(when_true.type);
	const TypeId false_pointer_type = false_array ? make_pointer(
		types_[false_unqualified.value].child) : strip_top_cv_type(when_false.type);
	const bool same_value_category = when_true.category == when_false.category &&
		when_true.category != SemanticValueCategory::Prvalue;
	const bool same_glvalue_category =
		when_true.category == SemanticValueCategory::Lvalue ||
		when_true.category == SemanticValueCategory::Xvalue;
	bool class_glvalue_base = false;
	if (same_value_category && same_glvalue_category &&
		!true_array && !false_array &&
		class_scope_for_type(true_unqualified).valid() &&
		class_scope_for_type(false_unqualified).valid())
	{
		unsigned int base_distance = 0;
		TypeId base_object;
		TypeId derived_object;
		ExprInfo base_expression;
		if (derived_base_relation(true_object, false_object, &base_distance,
			NULL, scope) && base_distance != 0)
		{
			derived_object = true_object;
			base_object = false_object;
			base_expression = when_true;
		}
		else
		{
			base_distance = 0;
			if (derived_base_relation(false_object, true_object,
				&base_distance, NULL, scope) && base_distance != 0)
			{
				derived_object = false_object;
				base_object = true_object;
				base_expression = when_false;
			}
		}
		if (base_object.valid() && derived_object.valid() && base_distance != 0)
		{
			const unsigned int qualifiers = cv_qualifiers(true_object) |
				cv_qualifiers(false_object);
			type = make_cv(strip_cv_type(base_object), qualifiers);
			const ConversionChoice choice = make_derived_base_choice(derived_object,
				base_object, base_distance, scope, cv_qualifiers(type) &
				~cv_qualifiers(base_expression.type));
			set_fact_conversion(base_expression.fact,
				add_conversion(base_expression.type, type, choice));
			category = when_true.category;
			class_glvalue_base = true;
		}
	}
	if (class_glvalue_base)
	{
		// The branch conversion above projects an lvalue/xvalue onto its base
		// subobject; no class object is copied at this boundary.
	}
	else if (strip_top_cv_type(true_object) == strip_top_cv_type(false_object) &&
		((!true_array && !false_array) || same_value_category))
	{
		type = (when_true.category == SemanticValueCategory::Lvalue ||
			when_true.category == SemanticValueCategory::Xvalue) ?
			true_object : strip_top_cv_type(true_object);
		if (when_true.category == SemanticValueCategory::Lvalue &&
			when_false.category == SemanticValueCategory::Lvalue)
			category = SemanticValueCategory::Lvalue;
		else if (when_true.category == SemanticValueCategory::Xvalue &&
			when_false.category == SemanticValueCategory::Xvalue)
			category = SemanticValueCategory::Xvalue;
		if (category == SemanticValueCategory::Prvalue)
		{
			record_builtin_conversion(when_true, type);
			record_builtin_conversion(when_false, type);
		}
	}
	else if ((integral_id(when_true.type) || floating_id(when_true.type)) &&
		(integral_id(when_false.type) || floating_id(when_false.type)))
	{
		type = common_arithmetic_type(true_arithmetic_operation,
			false_arithmetic_operation);
		record_builtin_conversion(when_true, type);
		record_builtin_conversion(when_false, type);
	}
	else if (true_pointer && false_pointer)
	{
		type = conditional_pointer_common_type(true_pointer_type,
			false_pointer_type);
		if (!type.valid())
			throw std::runtime_error("PA12 incompatible conditional pointers");
		record_builtin_conversion(when_true, type, scope);
		record_builtin_conversion(when_false, type, scope);
	}
	else if (true_pointer &&
		(nullptr_id(when_false.type) || when_false.integer_zero))
	{
		type = true_pointer_type;
		record_builtin_conversion(when_true, type);
		record_builtin_conversion(when_false, type);
	}
	else if (false_pointer &&
		(nullptr_id(when_true.type) || when_true.integer_zero))
	{
		type = false_pointer_type;
		record_builtin_conversion(when_true, type);
		record_builtin_conversion(when_false, type);
	}
	else
		throw std::runtime_error("PA12 incompatible conditional operands");
	std::vector<SemanticFactId> children;
	children.push_back(condition.fact);
	children.push_back(when_true.fact);
	children.push_back(when_false.fact);
	return ExprInfo(make_expression_fact(
		SemanticFactKind::ConditionalExpression, type, category, node, children),
		type, category, false);
}
ExprInfo PA11SemanticModel::semantic_cast_expression(
	const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() < 2)
		throw std::runtime_error("PA12 invalid cast expression");
	const TypeId target = type_from_type_id(node.children.front(), scope);
	const PA10AstNode& operand_node = node.children.back();
	ExprInfo operand;
	// A template-id has no standalone overload-set expression type.  When the
	// cast supplies a function-pointer target, select the typed specialization
	// before forming the address-of fact.  This is deliberately limited to the
	// explicit function-template target shape; ordinary unary expressions keep
	// their existing semantic path.
	if (operand_node.kind == PA10NodeKind::UnaryExpression &&
		operand_node.has_token && operand_node.token == SimpleTokenType::OP_AMP &&
		operand_node.children.size() == 1 &&
		has_template_id(operand_node.children.front()))
	{
		const TypeId target_object = strip_cv_type(expression_object_type(target));
		if (type_kind(target_object) == TypeKind::Pointer)
		{
			const TypeId function_target = strip_cv_type(
				types_[target_object.value].child);
			if (type_kind(function_target) == TypeKind::Function)
			{
				const ExprInfo selected = semantic_expression_for_target(
					operand_node.children.front(), scope, function_target);
				const TypeId pointer = make_pointer(selected.type);
				SemanticFact unary(SemanticFactKind::UnaryExpression, pointer,
					SemanticValueCategory::Prvalue, &operand_node);
				unary.token = SimpleTokenType::OP_AMP;
				const SemanticFactId unary_id = make_semantic_fact(unary);
				set_semantic_children(unary_id,
					std::vector<SemanticFactId>(1, selected.fact));
				operand = ExprInfo(unary_id, pointer,
					SemanticValueCategory::Prvalue, false);
			}
			else
				operand = semantic_expression(operand_node, scope);
		}
		else
			operand = semantic_expression(operand_node, scope);
	}
	else
		operand = semantic_expression(operand_node, scope);
	return semantic_cast_to_target(node, scope, target, operand);
}
ExprInfo PA11SemanticModel::semantic_expression(const PA10AstNode& node, ScopeId scope)
{
	switch (node.kind)
	{
	case PA10NodeKind::Literal:
	{
		const SemanticFactId fact = semantic_literal(node);
		return ExprInfo(fact, semantic_facts_[fact.value].type,
			semantic_facts_[fact.value].category, integer_zero(node));
	}
	case PA10NodeKind::UserDefinedLiteral:
		return semantic_user_defined_literal(node, scope);
	case PA10NodeKind::KeywordLiteral:
		if (node.has_token && node.token == SimpleTokenType::KW_THIS)
			return semantic_this_expression(node, scope);
		{
			const SemanticFactId fact = semantic_literal(node);
			return ExprInfo(fact, semantic_facts_[fact.value].type,
				semantic_facts_[fact.value].category, integer_zero(node));
		}
	case PA10NodeKind::IdExpression:
		return semantic_id_expression(node, scope);
	case PA10NodeKind::MemberExpression:
		return semantic_member_expression(node, scope);
	case PA10NodeKind::ParenthesizedExpression:
		if (node.children.size() != 1)
			throw std::runtime_error("PA12 invalid parenthesized expression");
		return semantic_expression(node.children.front(), scope);
	case PA10NodeKind::UnaryExpression:
		return semantic_unary_expression(node, scope);
	case PA10NodeKind::PostfixExpression:
		return semantic_postfix_expression(node, scope);
	case PA10NodeKind::BinaryExpression:
		return semantic_binary_expression(node, scope);
	case PA10NodeKind::AssignmentExpression:
		return semantic_assignment_expression(node, scope);
	case PA10NodeKind::ConditionalExpression:
		return semantic_conditional_expression(node, scope);
	case PA10NodeKind::CastExpression:
		return semantic_cast_expression(node, scope);
	case PA10NodeKind::CallExpression:
		return semantic_call_expression(node, scope);
	case PA10NodeKind::NewExpression:
		return semantic_new_expression(node, scope);
	case PA10NodeKind::SubscriptExpression:
	{
		if (node.children.size() != 2)
			throw std::runtime_error("PA12 invalid subscript expression");
		ExprInfo sequence_expression = semantic_expression(node.children[0], scope);
		ExprInfo index_expression = semantic_expression(node.children[1], scope);
		std::vector<TypeId> associated_objects;
		associated_objects.push_back(sequence_expression.type);
		associated_objects.push_back(index_expression.type);
		std::vector<const PA10AstNode*> member_nodes;
		member_nodes.push_back(&node.children[1]);
		std::vector<ExprInfo> member_arguments;
		member_arguments.push_back(index_expression);
		std::vector<const PA10AstNode*> no_nonmember_nodes;
		std::vector<ExprInfo> no_nonmember_arguments;
		const ExprInfo overloaded = semantic_operator_call(node, scope,
			PA10OperatorFunctionKind::Subscript, node.token,
			sequence_expression, associated_objects, member_nodes,
			member_arguments, no_nonmember_nodes, no_nonmember_arguments);
		if (overloaded.fact.valid())
			return overloaded;
		TypeId sequence = strip_cv_type(expression_object_type(sequence_expression.type));
		if (type_kind(sequence) != TypeKind::Array &&
			type_kind(sequence) != TypeKind::Pointer)
		{
			std::swap(sequence_expression, index_expression);
			sequence = strip_cv_type(expression_object_type(
				sequence_expression.type));
		}
		const bool sequence_array = type_kind(sequence) == TypeKind::Array;
		if (sequence_array)
			sequence = make_pointer(types_[sequence.value].child);
		if (type_kind(sequence) != TypeKind::Pointer ||
			!integral_id(index_expression.type))
			throw std::runtime_error("PA12 invalid subscript operands");
		record_builtin_conversion(sequence_expression, sequence);
		record_builtin_conversion(index_expression,
			integral_operation_type(index_expression));
		// The array-to-pointer conversion above gives PA12 enough typed
		// context to publish a literal address fact.  PA15 can then consume
		// the decoded payload through the normal address path without looking
		// back at the source spelling.
		if (sequence_array)
			record_constant_address(sequence_expression.fact, scope);
		const TypeId element = types_[sequence.value].child;
		std::vector<SemanticFactId> children;
		children.push_back(sequence_expression.fact);
		children.push_back(index_expression.fact);
		const SemanticFactId fact = make_expression_fact(
			SemanticFactKind::SubscriptExpression, element,
			SemanticValueCategory::Lvalue, node, children);
		return ExprInfo(fact, element, SemanticValueCategory::Lvalue, false);
	}
	case PA10NodeKind::SizeofExpression:
	case PA10NodeKind::TypeTraitExpression:
	{
		const TypeId operand_type = sizeof_operand_type(node, scope);
		const TypeId result = fundamental(FundamentalType::UnsignedLongInt);
		const SemanticFactId fact = make_expression_fact(
			SemanticFactKind::SizeofExpression, result,
			SemanticValueCategory::Prvalue, node,
			std::vector<SemanticFactId>());
		SemanticFact& record = semantic_facts_[fact.value];
		record.size_type_derived = true;
		record.has_literal_value = true;
		record.literal_value = type_size(operand_type);
		record.literal_value_unsigned = true;
		return ExprInfo(fact, result, SemanticValueCategory::Prvalue, false);
	}
	case PA10NodeKind::BracedInitList:
		throw std::runtime_error("PA12 braced initializer has no target");
	default:
		throw std::runtime_error("PA12 unsupported expression form");
	}
}
SemanticFactId PA11SemanticModel::semantic_declaration(const PA10AstNode& node, ScopeId scope)
{
	if (declaration_definition_flags_.size() != declaration_bindings_.size())
		throw std::runtime_error("PA12 declaration definition arena is discontinuous");
	DeclarationFact* declaration = declaration_fact(node);
	if (declaration == NULL)
		throw std::runtime_error("PA12 declaration fact is missing");
	if (declaration->semantic_begin != InvalidIdentityValue)
		return declaration_semantic_ids_[declaration->semantic_begin];
	if (node.kind == PA10NodeKind::AliasDeclaration)
	{
		if (declaration->binding_count != 1)
			throw std::runtime_error("PA12 alias declaration binding mismatch");
		const BindingId binding_id = declaration_bindings_[
			declaration->binding_begin];
		// Copy before initializer analysis can grow the binding arena.
		const Binding value = binding(binding_id);
		SemanticFact fact(SemanticFactKind::TypeAlias, value.type,
			SemanticValueCategory::Prvalue, &node);
		fact.binding = binding_id;
		const SemanticFactId result = make_semantic_fact(fact);
		declaration->semantic_begin = declaration_semantic_ids_.size();
		declaration->semantic_count = 1;
		declaration_semantic_ids_.push_back(result);
		return result;
	}
	if (node.kind == PA10NodeKind::ConditionDeclaration)
	{
		if (node.children.size() != 3 || declaration->binding_count != 1)
			throw std::runtime_error("PA12 invalid condition declaration fact");
		const BindingId binding_id = declaration_bindings_[
			declaration->binding_begin];
		const Binding& value = binding(binding_id);
		SemanticFact fact(SemanticFactKind::Variable, value.type,
			SemanticValueCategory::Prvalue, &node);
		fact.binding = binding_id;
		fact.selected_scope = declaration->scope;
		const SemanticFactId variable = make_semantic_fact(fact);
		const PA10AstNode& initializer = node.children[2];
		if ((initializer.kind != PA10NodeKind::Initializer &&
			initializer.kind != PA10NodeKind::ParenInitializer) ||
			initializer.children.size() != 1)
			throw std::runtime_error("PA12 invalid condition initializer");
		const PA10AstNode* clause = &initializer.children.front();
		if (clause->kind == PA10NodeKind::ParenInitializer)
		{
			if (clause->children.size() != 1)
				throw std::runtime_error("PA12 invalid parenthesized initializer");
			clause = &clause->children.front();
		}
		const ExprInfo expression = semantic_expression_for_target(
			*clause, declaration->scope, value.type);
		const ExprInfo converted = apply_context_conversion(expression, value.type,
			semantic_facts_[expression.fact.value].source, declaration->scope);
		if (declaration->is_constexpr)
			retarget_constexpr_literal(converted.fact, value.type);
		set_semantic_children(variable,
			std::vector<SemanticFactId>(1, converted.fact));
		declaration->semantic_begin = declaration_semantic_ids_.size();
		declaration->semantic_count = 1;
		declaration_semantic_ids_.push_back(variable);
		return variable;
	}
	if (node.children.size() != 2 || node.children[1].kind != PA10NodeKind::InitDeclaratorList)
		throw std::runtime_error("PA12 invalid declaration fact");
	if (declaration->binding_begin == InvalidIdentityValue ||
		declaration->binding_begin > declaration_definition_flags_.size() ||
		declaration->binding_count > declaration_definition_flags_.size() -
			declaration->binding_begin)
		throw std::runtime_error("PA12 declaration definition range is invalid");
	const PA10AstNode& list = node.children[1];
	if (list.children.size() != declaration->binding_count)
		throw std::runtime_error("PA12 declaration binding mismatch");
	declaration->semantic_begin = declaration_semantic_ids_.size(); declaration->lifetime_begin = lifetime_facts_.size();
	for (std::size_t i = 0; i < list.children.size(); ++i)
	{
		const PA10AstNode& init = list.children[i];
		const BindingId binding_id = declaration_bindings_[
			declaration->binding_begin + i];
		const unsigned char definition_flag = declaration_definition_flags_[
			declaration->binding_begin + i];
		if (definition_flag > 1)
			throw std::runtime_error("PA12 declaration definition flag is invalid");
		// Copy before initializer analysis can grow the binding arena.
		const Binding value = binding(binding_id);
		if (value.kind == BindingKind::Function && has_function_default_argument(node, i))
		{
			FunctionFactId function_id;
			const FunctionFactId* found = function_binding_fact_index_.find(
				binding_id);
			if (found != NULL)
			{
				if (!found->valid() || found->value >= function_facts_.size())
					throw std::runtime_error("PA12 default function fact is invalid");
				function_id = *found;
			}
			else
			{
				if (init.children.empty())
					throw std::runtime_error("PA12 default function declarator is missing");
				const DeclaratorName name = declarator_name(init.children.front());
				const ScopeId owner = name.found ?
					declaration_scope(name.path, declaration->scope) : ScopeId();
				if (!owner.valid())
					throw std::runtime_error("PA12 default function scope is missing");
				const FunctionFact declaration_function(&node, owner, binding_id);
				function_id = FunctionFactId(function_facts_.size());
				function_facts_.push_back(declaration_function);
				function_binding_fact_index_.set(binding_id, function_id);
			}
			record_function_default_arguments(function_id, node, i);
		}
		SemanticFact fact(SemanticFactKind::Variable, value.type,
			SemanticValueCategory::Prvalue, &init);
		fact.binding = binding_id;
		fact.selected_scope = declaration->scope;
		SemanticFactId variable = make_semantic_fact(fact);
		const NamedRecordId record = class_record_for_object_type(value.type);
		const PA10AstNode* direct_operand = NULL;
		const bool direct_operand_initializer = direct_initializer_operand(init,
			declaration->scope, &direct_operand);
		const PA10AstNode* clause = NULL;
		ConstructorInitializationContext initialization_context =
			ConstructorInitializationContext::Direct;
		if (!direct_operand_initializer && init.children.size() > 1)
		{
			const PA10AstNode& initializer = init.children[1];
			if (initializer.kind != PA10NodeKind::Initializer &&
				initializer.kind != PA10NodeKind::ParenInitializer)
				throw std::runtime_error("PA12 unsupported initializer");
			if (initializer.kind == PA10NodeKind::Initializer)
			{
				if (initializer.children.size() != 1)
					throw std::runtime_error("PA12 initializer arity mismatch");
				clause = &initializer.children.front();
				if (initializer.has_token)
					initialization_context = clause->kind ==
						PA10NodeKind::BracedInitList ?
						ConstructorInitializationContext::CopyList :
						ConstructorInitializationContext::Copy;
			}
			else
				clause = &initializer;
		}
		SemanticFactId initializer_fact;
		semantic_variable_initializer(binding_id, variable, init, value, *declaration,
			definition_flag != 0, record, direct_operand, clause,
			initialization_context, &initializer_fact);
		if (initializer_fact.valid() &&
			value.kind == BindingKind::Variable && !is_static_member(binding_id) &&
			declaration->scope.valid() &&
			declaration->scope.value < scopes_.size() &&
			scopes_[declaration->scope.value].kind == ScopeKind::Class)
		{
			BindingSidecar sidecar;
			const BindingSidecar* existing = binding_sidecar(binding_id);
			if (existing != NULL)
				sidecar = *existing;
			sidecar.has_default_member_initializer = true;
			sidecar.default_member_initializer = initializer_fact;
			set_binding_sidecar(binding_id, sidecar);
			mark_default_member_initializer(declaration->scope);
		}
		if (value.kind == BindingKind::Variable && declaration->automatic_storage &&
			!declaration->is_static && !declaration->is_thread_local)
			record_automatic_lifetime(binding_id, value.type, declaration->scope);
		record_constant_initializer(variable, declaration->scope);
		declaration_semantic_ids_.push_back(variable);
	}
	declaration->semantic_count = list.children.size(); declaration->lifetime_count = lifetime_facts_.size() - declaration->lifetime_begin;
	return declaration_semantic_ids_[declaration->semantic_begin];
}
FunctionIdResolution PA11SemanticModel::resolve_single_argument_function(const NamePath& path, ScopeId scope, const ExprInfo& argument) const
{
	const std::vector<ValueRef> candidates = lookup_value_path(path, scope);
	ValueRef selected;
	ConversionChoice selected_conversion;
	bool have_selected = false, ambiguous_best = false;
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const Binding& candidate = binding(candidates[i].binding);
		if (candidate.kind != BindingKind::Function || type_kind(candidate.type) != TypeKind::Function)
			continue;
		const TypeKey& function = types_[candidate.type.value];
		if (function.parameters.size() != 1)
			continue;
		const ConversionChoice choice = conversion_for(argument, function.parameters.front(), semantic_facts_[argument.fact.value].source, scope);
		ConversionChoice selected_choice = choice;
		if (choice.valid && supports_pa16_class_value_parameter(candidates[i], 0, argument, function.parameters.front()))
			selected_choice = ConversionChoice(true, 0, ConversionKind::ClassValue);
		if (!selected_choice.valid)
			continue;
		const int comparison = have_selected ? compare_conversion_choices(selected_choice, selected_conversion) : -1;
		if (!have_selected || comparison < 0)
		{
			have_selected = true;
			ambiguous_best = false;
			selected = candidates[i];
			selected_conversion = selected_choice;
		}
		else if (comparison == 0)
			ambiguous_best = true;
	}
	if (ambiguous_best)
		throw std::runtime_error("PA12 ambiguous call");
	return have_selected ? FunctionIdResolution(true, selected, selected_conversion) : FunctionIdResolution();
}
ExprInfo PA11SemanticModel::semantic_single_argument_call(
	const PA10AstNode& node, const FunctionIdResolution& resolution,
	const ExprInfo& argument, ScopeId scope)
{
	const ValueRef selected = resolution.selected;
	if (function_declaration_kind(selected.binding) ==
		FunctionDeclarationKind::Deleted)
		throw std::runtime_error("PA12 call selects deleted function");
	const TypeKey& function = types_[binding(selected.binding).type.value];
	ExprInfo converted = argument;
	if (resolution.conversion.kind == ConversionKind::ClassValue)
	{
		set_fact_conversion(argument.fact, add_conversion(argument.type, function.parameters.front(), resolution.conversion));
	}
	else
		converted = apply_context_conversion(argument, function.parameters.front(),
			semantic_facts_[argument.fact.value].source, scope);
	const TypeId result_type = function.result;
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	SemanticFact fact(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	fact.has_callee = true;
	fact.bool_context_operand = bool_id(result_type);
	fact.direct_bool_boundary = bool_id(result_type);
	fact.selected_binding = selected.binding;
	fact.selected_scope = selected.scope;
	fact.callable_type = selected.binding.valid() ?
		binding(selected.binding).type : TypeId();
	const SemanticFactId call = make_semantic_fact(fact);
	set_semantic_children(call,
		std::vector<SemanticFactId>(1, converted.fact));
	return ExprInfo(call, result_type, result_category, false);
}
SemanticFactId PA11SemanticModel::semantic_ambiguous_call_statement(
	const PA10AstNode& node, ScopeId scope)
{
	LookupPointGuard lookup_point(*this, SourcePoint(node.source_begin));
	NamePath function_name;
	const PA10AstNode* argument_node = NULL;
	const PA10AstNode* right_node = NULL;
	if (ambiguous_assignment_statement(node, scope, &function_name,
		&argument_node, &right_node))
	{
		const ExprInfo argument = semantic_id_expression(*argument_node, scope);
		bool member_claimed = false;
		ExprInfo left = semantic_ambiguous_member_call(node, scope, function_name,
			*argument_node, argument, &member_claimed);
		if (!member_claimed)
		{
			const FunctionIdResolution resolution =
				resolve_single_argument_function(function_name, scope, argument);
			if (!resolution.valid)
				throw std::runtime_error("PA12 no viable call");
			left = semantic_single_argument_call(node, resolution, argument,
				scope);
		}
		else if (!left.fact.valid())
			throw std::runtime_error("PA12 no viable member call");
		if (left.category != SemanticValueCategory::Lvalue)
			throw std::runtime_error("PA12 assignment requires lvalue");
		if (!modifiable_lvalue(left.type))
			throw std::runtime_error("PA12 assignment requires modifiable lvalue");
		const ExprInfo right_expression = semantic_expression(*right_node, scope);
		const TypeId target = expression_object_type(left.type);
		apply_context_conversion(right_expression, target,
			semantic_facts_[right_expression.fact.value].source, scope);
		SemanticFact assignment_fact(SemanticFactKind::AssignmentExpression, target,
			SemanticValueCategory::Lvalue, &node);
		assignment_fact.token = SimpleTokenType::OP_ASS;
		const SemanticFactId assignment = make_semantic_fact(assignment_fact);
		set_semantic_children(assignment,
			std::vector<SemanticFactId>{left.fact, right_expression.fact});
		if (left.fact.valid() && left.fact.value < semantic_facts_.size() &&
			semantic_facts_[left.fact.value].kind == SemanticFactKind::IdExpression &&
			semantic_facts_[left.fact.value].binding.valid() &&
			semantic_facts_[left.fact.value].binding.value < bindings_.size() &&
			binding(semantic_facts_[left.fact.value].binding).kind ==
				BindingKind::Variable)
			semantic_facts_[assignment.value].binding =
				semantic_facts_[left.fact.value].binding;
		return make_expression_fact(SemanticFactKind::ExpressionStatement,
			TypeId(), SemanticValueCategory::Prvalue, node,
			std::vector<SemanticFactId>(1, assignment));
	}
	if (!ambiguous_call_statement(node, scope, &function_name, &argument_node) ||
		argument_node == NULL)
		throw std::runtime_error("PA12 unsupported declaration statement");
	const ExprInfo argument = semantic_id_expression(*argument_node, scope);
	bool member_claimed = false;
	ExprInfo call = semantic_ambiguous_member_call(node, scope, function_name,
		*argument_node, argument, &member_claimed);
	if (!member_claimed)
	{
		const FunctionIdResolution resolution =
			resolve_single_argument_function(function_name, scope, argument);
		if (!resolution.valid)
			throw std::runtime_error("PA12 no viable call");
		call = semantic_single_argument_call(node, resolution, argument, scope);
	}
	else if (!call.fact.valid())
		throw std::runtime_error("PA12 no viable member call");
	return make_expression_fact(SemanticFactKind::ExpressionStatement,
		TypeId(), SemanticValueCategory::Prvalue, node,
		std::vector<SemanticFactId>(1, call.fact));
}
SemanticFactId PA11SemanticModel::semantic_condition(const PA10AstNode& node,
	ScopeId scope, bool switch_condition)
{
	if (node.kind != PA10NodeKind::Condition || node.children.size() > 1)
		throw std::runtime_error("invalid PA12 condition");
	std::vector<SemanticFactId> children;
	if (!node.children.empty())
	{
		const PA10AstNode& child = node.children.front();
		if (child.kind == PA10NodeKind::ConditionDeclaration)
		{
			const SemanticFactId variable = semantic_declaration(child, scope);
			const DeclarationFact* declaration = declaration_fact(child);
			if (declaration == NULL || declaration->binding_count != 1)
				throw std::runtime_error("invalid PA12 condition binding");
			const BindingId id = declaration_bindings_[declaration->binding_begin];
			const TypeId type = binding(id).type;
			if (switch_condition && !integral_id(type) && !enumeration_id(type))
				throw std::runtime_error("PA12 switch condition is not integral");
			const ExprInfo condition(variable, type,
				SemanticValueCategory::Lvalue, false);
			const TypeId condition_target = switch_condition ?
				switch_condition_type(type) : fundamental(FundamentalType::Bool);
			const ExprInfo converted = apply_context_conversion(condition,
				condition_target, semantic_facts_[variable.value].source);
			(void)converted;
			children.push_back(make_expression_fact(
				SemanticFactKind::ConditionDeclaration, TypeId(),
				SemanticValueCategory::Prvalue, child,
				std::vector<SemanticFactId>(1, variable)));
		}
		else
		{
			const ExprInfo expression = semantic_expression(child, scope);
			const TypeId type = expression_object_type(expression.type);
			if (switch_condition && !integral_id(type) && !enumeration_id(type))
				throw std::runtime_error("PA12 switch condition is not integral");
			const TypeId condition_target = switch_condition ?
				switch_condition_type(type) : fundamental(FundamentalType::Bool);
			const ExprInfo converted = apply_context_conversion(expression,
				condition_target, semantic_facts_[expression.fact.value].source);
			children.push_back(converted.fact);
		}
	}
	return make_expression_fact(SemanticFactKind::Condition, TypeId(),
		SemanticValueCategory::Prvalue, node, children);
}

bool PA11SemanticModel::convert_case_value(TypeId switch_type, __int128 value,
	SwitchCaseKey* result) const
{
	if (result == NULL)
		throw std::runtime_error("PA12 case conversion has no result");
	TypeId target = strip_cv_type(switch_type);
	const NamedRecordId record = named_record_for_type(target);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Enum)
		target = named_[record.value].has_underlying ?
			strip_cv_type(named_[record.value].underlying) :
			fundamental(FundamentalType::Int);
	FundamentalType target_fundamental;
	if (!fundamental_of(target, &target_fundamental) ||
		!integral_type(target_fundamental))
		throw std::runtime_error("PA12 switch case type is not integral");
	const std::size_t byte_count = type_size(target);
	if (byte_count == 0 || byte_count > sizeof(std::uint64_t))
		throw std::runtime_error("PA12 switch case type is too wide");
	const unsigned int width = static_cast<unsigned int>(byte_count * 8);
	const __int128 modulus = static_cast<__int128>(1) << width;
	const bool is_unsigned = unsigned_type(target_fundamental);
	const __int128 minimum = target_fundamental == FundamentalType::Bool ?
		0 : (is_unsigned ? 0 : -(modulus >> 1));
	const __int128 maximum = target_fundamental == FundamentalType::Bool ?
		1 : (is_unsigned ? modulus - 1 : (modulus >> 1) - 1);
	if (value < minimum || value > maximum)
		return false;
	const std::uint64_t bits = value < 0 ?
		static_cast<std::uint64_t>(value + modulus) :
		static_cast<std::uint64_t>(value);
	*result = SwitchCaseKey(bits, width, is_unsigned);
	return true;
}

SemanticFactId PA11SemanticModel::semantic_case_label(const PA10AstNode& node,
	ScopeId scope, SwitchValidationContext& switch_context)
{
	if (node.kind != PA10NodeKind::CaseStatement || node.children.size() != 2)
		throw std::runtime_error("invalid PA12 case label");
	const PA10AstNode& expression = node.children.front();
	const ConstValue value = eval_constexpr(expression, scope);
	if (!value.valid || (!integral_id(switch_context.conversion_type) &&
		!enumeration_id(switch_context.conversion_type)))
		throw std::runtime_error("PA12 case label is not integral constant");
	const TypeId label_type = expression_object_type(
		expression_type(expression, scope));
	if (!integral_id(label_type) && !enumeration_id(label_type))
		throw std::runtime_error("PA12 case label has invalid type");
	if (!case_label_convertible(label_type, switch_context.conversion_type))
		throw std::runtime_error("PA12 case label cannot convert to switch type");
	SwitchCaseKey key;
	if (!convert_case_value(switch_context.conversion_type, value.value, &key))
		throw std::runtime_error("PA12 case label is not representable");
	if (switch_context.case_values.find(key) != NULL)
		throw std::runtime_error("PA12 duplicate case label");
	switch_context.case_values.set(key, true);
	SemanticFact fact(SemanticFactKind::Literal, switch_context.type,
		SemanticValueCategory::Prvalue, &expression);
	fact.has_literal_value = true;
	fact.literal_value_unsigned = key.is_unsigned;
	fact.literal_value_negative = false;
	if (key.is_unsigned)
		fact.literal_value = key.bits;
	else
	{
		const __int128 modulus = static_cast<__int128>(1) << key.width;
		__int128 normalized = static_cast<__int128>(key.bits);
		if (normalized >= (modulus >> 1))
			normalized -= modulus;
		if (normalized < 0)
		{
			fact.literal_value_negative = true;
			normalized = -normalized;
		}
		fact.literal_value = static_cast<std::uint64_t>(normalized);
	}
	const ConversionChoice choice = conversion_for(label_type,
		SemanticValueCategory::Prvalue, switch_context.conversion_type, NULL);
	if (!choice.valid)
		throw std::runtime_error("PA12 case label conversion is invalid");
	const SemanticFactId result = make_semantic_fact(fact);
	set_fact_conversion(result,
		add_conversion(label_type, switch_context.conversion_type, choice));
	return result;
}

SemanticFactId PA11SemanticModel::semantic_for_init(const PA10AstNode& node,
	ScopeId scope)
{
	if (node.kind != PA10NodeKind::ForInitStatement)
		throw std::runtime_error("invalid PA12 for-init statement");
	std::vector<SemanticFactId> children;
	if (!node.children.empty())
	{
		const PA10AstNode& child = node.children.front();
		/* The parser preserves an empty for-init as an empty declaration. */
		if (child.kind == PA10NodeKind::SimpleDeclaration)
		{
			const DeclarationFact* declaration = declaration_fact(child);
			if (declaration == NULL)
				throw std::runtime_error("PA12 for-init declaration is missing");
			semantic_declaration(child, scope);
			std::vector<SemanticFactId> variables;
			for (std::size_t i = 0; i < declaration->semantic_count; ++i)
				variables.push_back(declaration_semantic_ids_[
					declaration->semantic_begin + i]);
			children.push_back(make_expression_fact(
				SemanticFactKind::SimpleDeclaration, TypeId(),
				SemanticValueCategory::Prvalue, child, variables));
		}
		else if (child.kind != PA10NodeKind::EmptyDeclaration)
		{
			const ExprInfo expression = semantic_expression(child, scope);
			children.push_back(expression.fact);
		}
	}
	return make_expression_fact(SemanticFactKind::ForInitStatement, TypeId(),
		SemanticValueCategory::Prvalue, node, children);
}

SemanticFactId PA11SemanticModel::semantic_substatement(
	const PA10AstNode& wrapper, ScopeId parent, const FunctionFact& function,
	unsigned int loop_depth, unsigned int switch_depth,
	SwitchValidationContext* switch_context)
{
	if ((wrapper.kind != PA10NodeKind::ThenBranch &&
		wrapper.kind != PA10NodeKind::ElseBranch) || wrapper.children.size() != 1)
		throw std::runtime_error("invalid PA12 substatement wrapper");
	const PA10AstNode& child = wrapper.children.front();
	const ScopeId body = child.kind == PA10NodeKind::CompoundStatement ?
		compound_scope(child) : substatement_scope(child);
	if (!body.valid())
		throw std::runtime_error("PA12 substatement scope is missing");
	const SemanticFactId statement = semantic_statement(child, body, function,
		loop_depth, switch_depth, switch_context);
	std::vector<SemanticFactId> children;
	if (statement.valid())
		children.push_back(statement);
	return make_expression_fact(wrapper.kind == PA10NodeKind::ThenBranch ?
		SemanticFactKind::ThenBranch : SemanticFactKind::ElseBranch,
		TypeId(), SemanticValueCategory::Prvalue, wrapper,
		children);
}

SemanticFactId PA11SemanticModel::semantic_compound(const PA10AstNode& node,
	ScopeId parent, const FunctionFact& function, unsigned int loop_depth,
	unsigned int switch_depth, SwitchValidationContext* switch_context)
{
	if (node.kind != PA10NodeKind::CompoundStatement)
		throw std::runtime_error("PA12 expected compound statement");
	const ScopeId block = compound_scope(node);
	if (!block.valid())
		throw std::runtime_error("PA12 compound scope is missing");
	std::vector<SemanticFactId> children;
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode& child = node.children[i];
		if (child.kind == PA10NodeKind::EmptyDeclaration)
			continue;
		const SemanticFactId fact = semantic_statement(child, block, function,
			loop_depth, switch_depth, switch_context);
		if (fact.valid())
			children.push_back(fact);
	}
	const SemanticFactId result = make_expression_fact(
		SemanticFactKind::CompoundStatement, TypeId(),
		SemanticValueCategory::Prvalue, node, children);
	(void)parent;
	return result;
}

SemanticFactId PA11SemanticModel::semantic_declaration_statement(
	const PA10AstNode& node, ScopeId scope)
{
	switch (node.kind)
	{
	case PA10NodeKind::NamespaceAliasDefinition:
	case PA10NodeKind::UsingDirective:
	case PA10NodeKind::UsingDeclaration:
		// Lookup-only declarations have no PA12 statement line.
		return SemanticFactId();
	case PA10NodeKind::AliasDeclaration:
	{
		return semantic_declaration(node, scope);
	}
	case PA10NodeKind::SimpleDeclaration:
	{
		const DeclarationFact* declaration = declaration_fact(node);
		if (declaration == NULL)
			return semantic_ambiguous_call_statement(node, scope);
		semantic_declaration(node, scope);
		std::vector<SemanticFactId> variables;
		for (std::size_t i = 0; i < declaration->semantic_count; ++i)
			variables.push_back(declaration_semantic_ids_[
				declaration->semantic_begin + i]);
		return make_expression_fact(SemanticFactKind::SimpleDeclaration,
			TypeId(), SemanticValueCategory::Prvalue, node, variables);
	}
	default:
		throw std::runtime_error("PA12 unsupported declaration statement");
	}
}
SemanticFactId PA11SemanticModel::semantic_anonymous_union_statement(
	const PA10AstNode& node)
{
	const AnonymousUnionFact* anonymous = anonymous_union_fact(node);
	if (anonymous == NULL || !anonymous->storage.valid())
		throw std::runtime_error("PA12 unsupported class statement");
	const Binding& storage = binding(anonymous->storage);
	SemanticFact variable(SemanticFactKind::Variable, storage.type,
		SemanticValueCategory::Prvalue, &node);
	variable.binding = anonymous->storage;
	variable.selected_scope = anonymous->owner;
	const SemanticFactId variable_id = make_semantic_fact(variable);
	set_semantic_children(variable_id,
		std::vector<SemanticFactId>(1,
			semantic_constructor_action(anonymous->storage, node)));
	return make_expression_fact(SemanticFactKind::SimpleDeclaration,
		TypeId(), SemanticValueCategory::Prvalue, node,
		std::vector<SemanticFactId>(1, variable_id));
}
SemanticFactId PA11SemanticModel::semantic_statement(const PA10AstNode& node,
	ScopeId scope, const FunctionFact& function, unsigned int loop_depth,
	unsigned int switch_depth, SwitchValidationContext* switch_context)
{
	LookupPointGuard lookup_point(*this, SourcePoint(node.source_begin));
	switch (node.kind)
	{
	case PA10NodeKind::EmptyDeclaration:
		return SemanticFactId();
	case PA10NodeKind::ClassSpecifier:
		return semantic_anonymous_union_statement(node);
	case PA10NodeKind::EnumSpecifier:
		return make_expression_fact(SemanticFactKind::SimpleDeclaration,
			TypeId(), SemanticValueCategory::Prvalue, node,
			std::vector<SemanticFactId>());
	case PA10NodeKind::AliasDeclaration:
	case PA10NodeKind::NamespaceAliasDefinition:
	case PA10NodeKind::UsingDirective:
	case PA10NodeKind::UsingDeclaration:
	case PA10NodeKind::SimpleDeclaration:
		return semantic_declaration_statement(node, scope);
	case PA10NodeKind::ReturnStatement:
		return semantic_return_statement(node, scope, function);
	case PA10NodeKind::ExpressionStatement:
	{
		if (node.children.empty())
			return SemanticFactId();
		const ExprInfo expression = semantic_expression(node.children.front(),
			scope);
		return make_expression_fact(SemanticFactKind::ExpressionStatement,
			TypeId(), SemanticValueCategory::Prvalue, node,
			std::vector<SemanticFactId>(1, expression.fact));
	}
	case PA10NodeKind::CompoundStatement:
		return semantic_compound(node, scope, function, loop_depth, switch_depth,
			switch_context);
	case PA10NodeKind::IfStatement:
	{
		const StatementFact* statement = statement_fact(node);
		if (statement == NULL || statement->kind != StatementFactKind::If ||
			node.children.size() < 2)
			throw std::runtime_error("PA12 if statement fact is missing");
		std::vector<SemanticFactId> children;
		children.push_back(semantic_condition(node.children[0],
			statement->scope, false));
		children.push_back(semantic_substatement(node.children[1],
			statement->scope, function, loop_depth, switch_depth, switch_context));
		if (node.children.size() > 2)
			children.push_back(semantic_substatement(node.children[2],
				statement->scope, function, loop_depth, switch_depth,
				switch_context));
		return make_expression_fact(SemanticFactKind::IfStatement, TypeId(),
			SemanticValueCategory::Prvalue, node, children);
	}
	case PA10NodeKind::SwitchStatement:
	{
		const StatementFact* statement = statement_fact(node);
		if (statement == NULL || statement->kind != StatementFactKind::Switch ||
			node.children.size() != 2)
			throw std::runtime_error("PA12 switch statement fact is missing");
		const SemanticFactId condition = semantic_condition(node.children[0],
			statement->scope, true);
		const SemanticFact& condition_fact = semantic_facts_[condition.value];
		if (condition_fact.child_count != 1)
			throw std::runtime_error("PA12 switch condition is empty");
		const SemanticFact& condition_value = semantic_facts_[
			semantic_children_[condition_fact.child_begin].value];
		SemanticFactId condition_value_id = semantic_children_[
			condition_fact.child_begin];
		if (condition_value.kind == SemanticFactKind::ConditionDeclaration)
		{
			if (condition_value.child_count != 1)
				throw std::runtime_error("PA12 switch condition declaration is empty");
			condition_value_id = semantic_children_[condition_value.child_begin];
		}
		const SemanticFact& resolved_condition_value = semantic_facts_[
			condition_value_id.value];
		const TypeId switch_type = strip_cv_type(
			expression_object_type(resolved_condition_value.type));
		SwitchValidationContext current_switch(switch_type,
			switch_condition_type(switch_type));
		const PA10AstNode& body_node = node.children[1];
		const ScopeId body = body_node.kind == PA10NodeKind::CompoundStatement ?
			compound_scope(body_node) : substatement_scope(body_node);
		if (!body.valid())
			throw std::runtime_error("PA12 switch body scope is missing");
		validate_switch_initialization(body_node, body);
		std::vector<SemanticFactId> children;
		children.push_back(condition);
		const SemanticFactId body_fact = semantic_statement(body_node, body,
			function, loop_depth, switch_depth + 1, &current_switch);
		if (body_fact.valid())
			children.push_back(body_fact);
		return make_expression_fact(SemanticFactKind::SwitchStatement, TypeId(),
			SemanticValueCategory::Prvalue, node, children);
	}
	case PA10NodeKind::WhileStatement:
	case PA10NodeKind::DoStatement:
	{
		const StatementFact* statement = statement_fact(node);
		const bool is_while = node.kind == PA10NodeKind::WhileStatement;
		if (statement == NULL ||
			(is_while && statement->kind != StatementFactKind::While) ||
			(!is_while && statement->kind != StatementFactKind::Do) ||
			node.children.size() != 2)
			throw std::runtime_error("PA12 iteration statement fact is missing");
		const std::size_t body_index = is_while ? 1 : 0;
		const std::size_t condition_index = is_while ? 0 : 1;
		const PA10AstNode& body_node = node.children[body_index];
		const ScopeId body = body_node.kind == PA10NodeKind::CompoundStatement ?
			compound_scope(body_node) : substatement_scope(body_node);
		if (!body.valid())
			throw std::runtime_error("PA12 iteration body scope is missing");
		const SemanticFactId condition = semantic_condition(
			node.children[condition_index], statement->scope, false);
		const SemanticFactId body_fact = semantic_statement(body_node, body,
			function, loop_depth + 1, switch_depth, switch_context);
		std::vector<SemanticFactId> children;
		if (is_while)
		{
			children.push_back(condition);
			if (body_fact.valid())
				children.push_back(body_fact);
		}
		else
		{
			if (body_fact.valid())
				children.push_back(body_fact);
			children.push_back(condition);
		}
		return make_expression_fact(is_while ? SemanticFactKind::WhileStatement :
			SemanticFactKind::DoStatement, TypeId(),
			SemanticValueCategory::Prvalue, node, children);
	}
	case PA10NodeKind::ForStatement:
	{
		const StatementFact* statement = statement_fact(node);
		if (statement == NULL || statement->kind != StatementFactKind::For ||
			node.children.size() < 3)
			throw std::runtime_error("PA12 for statement fact is missing");
		std::vector<SemanticFactId> children;
		children.push_back(semantic_for_init(node.children[0],
			statement->scope));
		std::size_t body_index = node.children.size() - 1;
		for (std::size_t i = 1; i < body_index; ++i)
		{
			if (node.children[i].kind == PA10NodeKind::Condition)
				children.push_back(semantic_condition(node.children[i],
					statement->scope, false));
			else if (node.children[i].kind == PA10NodeKind::Iteration)
			{
				if (node.children[i].children.size() != 1)
					throw std::runtime_error("invalid PA12 for iteration");
				const ExprInfo iteration = semantic_expression(
					node.children[i].children.front(), statement->scope);
				children.push_back(make_expression_fact(SemanticFactKind::Iteration,
					TypeId(), SemanticValueCategory::Prvalue, node.children[i],
					std::vector<SemanticFactId>(1, iteration.fact)));
			}
		}
		const PA10AstNode& body_node = node.children[body_index];
		const ScopeId body = body_node.kind == PA10NodeKind::CompoundStatement ?
			compound_scope(body_node) : substatement_scope(body_node);
		if (!body.valid())
			throw std::runtime_error("PA12 for body scope is missing");
		const SemanticFactId body_fact = semantic_statement(body_node, body,
			function, loop_depth + 1, switch_depth, switch_context);
		if (body_fact.valid())
			children.push_back(body_fact);
		return make_expression_fact(SemanticFactKind::ForStatement, TypeId(),
			SemanticValueCategory::Prvalue, node, children);
	}
	case PA10NodeKind::CaseStatement:
	{
		if (switch_depth == 0 || switch_context == NULL ||
			node.children.size() != 2)
			throw std::runtime_error("PA12 case outside switch");
		const PA10AstNode& body_node = node.children.back();
		const ScopeId body = body_node.kind == PA10NodeKind::CompoundStatement ?
			compound_scope(body_node) : scope;
		if (!body.valid())
			throw std::runtime_error("PA12 case body scope is missing");
		std::vector<SemanticFactId> children;
		children.push_back(semantic_case_label(node, scope, *switch_context));
		const SemanticFactId body_fact = semantic_statement(body_node, body,
			function, loop_depth, switch_depth, switch_context);
		if (body_fact.valid())
			children.push_back(body_fact);
		return make_expression_fact(SemanticFactKind::CaseStatement, TypeId(),
			SemanticValueCategory::Prvalue, node, children);
	}
	case PA10NodeKind::DefaultStatement:
		if (switch_depth == 0 || switch_context == NULL ||
			node.children.size() != 1)
			throw std::runtime_error("PA12 default outside switch");
	{
		if (switch_context->has_default)
			throw std::runtime_error("PA12 duplicate default label");
		switch_context->has_default = true;
		const PA10AstNode& body_node = node.children.front();
		const ScopeId body = body_node.kind == PA10NodeKind::CompoundStatement ?
			compound_scope(body_node) : scope;
		if (!body.valid())
			throw std::runtime_error("PA12 default body scope is missing");
		const SemanticFactId body_fact = semantic_statement(body_node, body,
			function, loop_depth, switch_depth, switch_context);
		std::vector<SemanticFactId> children;
		if (body_fact.valid())
			children.push_back(body_fact);
		return make_expression_fact(SemanticFactKind::DefaultStatement,
			TypeId(), SemanticValueCategory::Prvalue, node, children);
	}
	case PA10NodeKind::LabeledStatement:
		return semantic_label_statement(node, scope, function, loop_depth,
			switch_depth, switch_context);
	case PA10NodeKind::GotoStatement:
	case PA10NodeKind::BreakStatement:
	case PA10NodeKind::ContinueStatement:
		return semantic_jump_statement(node, function, loop_depth, switch_depth);
	default:
		throw std::runtime_error("PA12 unsupported statement form");
	}
}
void PA11SemanticModel::analyze_pa12_node(const PA10AstNode& node, ScopeId scope)
{
	switch (node.kind)
	{
	case PA10NodeKind::NamespaceDefinition:
	{
		const NamespaceFact* namespace_fact = this->namespace_fact(node);
		if (namespace_fact == NULL)
			throw std::runtime_error("PA12 namespace fact is missing");
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind != PA10NodeKind::InlineMarker)
				analyze_pa12_node(node.children[i], namespace_fact->scope);
		break;
	}
	case PA10NodeKind::LinkageSpecification:
		for (std::size_t i = 0; i < node.children.size(); ++i)
			analyze_pa12_node(node.children[i], scope);
		break;
	case PA10NodeKind::ClassSpecifier:
	{
		const NamePath name = class_name(node);
		if (name.empty())
			break;
		const ScopeId class_scope = class_scope_for_type(
			lookup_type_path(name, scope));
		if (!class_scope.valid())
			throw std::runtime_error("PA12 class semantic scope is missing");
		publish_class_constructor_defaults(node, class_scope);
		// Form every field's initializer fact before any constructor consumes
		// the class-owned DMI sidecars.  This also makes declaration order in
		// the source independent of which constructor appears first.
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind == PA10NodeKind::SimpleDeclaration &&
				declaration_fact(node.children[i]) != NULL)
				semantic_declaration(node.children[i], class_scope);
		for (std::size_t i = 0; i < node.children.size(); ++i)
		{
			if (node.children[i].kind == PA10NodeKind::ClassSpecifier)
			{
				analyze_pa12_node(node.children[i], class_scope);
				continue;
			}
			if (node.children[i].kind == PA10NodeKind::FunctionDefinition)
			{
				analyze_pa12_node(node.children[i], class_scope);
				const FunctionFactId* id = function_fact_index_.find(
					&node.children[i]);
				if (id == NULL || !id->valid())
					throw std::runtime_error("PA12 class function identity is missing");
				class_function_facts_.push_back(*id);
			}
			else if (node.children[i].kind == PA10NodeKind::SpecialMemberDefinition ||
				node.children[i].kind == PA10NodeKind::SpecialMemberDeclaration)
				analyze_special_member(node.children[i], class_scope);
		}
		break;
	}
	case PA10NodeKind::SimpleDeclaration:
		if (declaration_fact(node) != NULL)
			semantic_declaration(node, scope);
		break;
	case PA10NodeKind::AliasDeclaration:
		semantic_declaration(node, scope);
		break;
	case PA10NodeKind::FunctionDefinition:
	{
		const FunctionFactId* found = function_fact_index_.find(&node);
		if (found == NULL || !found->valid() ||
			found->value >= function_facts_.size())
			throw std::runtime_error("PA12 function fact is missing");
		const FunctionFactId function_id = *found;
		if (function_facts_[function_id.value].body_fact.valid() ||
			node.children.empty())
			throw std::runtime_error("PA12 function fact is missing");
		prepare_pa12_member_parameter(function_facts_[function_id.value]);
		record_function_default_arguments(function_id, node, 0);
		prepare_pa12_labels(node.children.back(),
			function_facts_[function_id.value]);
		const ScopeId function_scope =
			function_facts_[function_id.value].function_scope;
		const FunctionFact semantic_function = function_facts_[function_id.value];
		const SemanticFactId body_fact = semantic_compound(node.children.back(),
			function_scope, semantic_function, 0, 0, NULL);
		function_facts_[function_id.value].body_fact = body_fact;
		break;
	}
	case PA10NodeKind::SpecialMemberDefinition:
	case PA10NodeKind::SpecialMemberDeclaration:
		analyze_special_member(node, scope);
		break;
	default:
		break;
	}
}
} // namespace pa11_semantic_internal
