#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

#include <algorithm>

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;
bool PA11SemanticModel::enumeration_id(TypeId type) const
{
	const NamedRecordId record = named_record_for_type(type);
	return record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Enum;
}

TypeId PA11SemanticModel::promote_integral_type(TypeId type) const
{
	type = strip_cv_type(expression_object_type(type));
	const NamedRecordId record = named_record_for_type(type);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Enum &&
		!named_[record.value].scoped_enum)
	{
		return promote_integral_type(named_[record.value].has_underlying ?
			strip_cv_type(expression_object_type(named_[record.value].underlying)) :
			fundamental(FundamentalType::Int));
	}
	FundamentalType fundamental_type;
	if (!fundamental_of(type, &fundamental_type))
		return type;
	switch (fundamental_type)
	{
	case FundamentalType::Bool:
	case FundamentalType::SignedChar:
	case FundamentalType::UnsignedChar:
	case FundamentalType::ShortInt:
	case FundamentalType::UnsignedShortInt:
	case FundamentalType::Char:
		return fundamental(FundamentalType::Int);
	case FundamentalType::Char16T:
	case FundamentalType::WcharT:
		return fundamental(FundamentalType::Int);
	case FundamentalType::Char32T:
		return fundamental(type_size(type) <
			type_size(fundamental(FundamentalType::Int)) ?
			FundamentalType::Int : FundamentalType::UnsignedInt);
	default:
		return type;
	}
}

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
	const TypeId type = apply_declarator(node.children[1], spec.base, target);
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

void PA11SemanticModel::prepare_pa12_node(const PA10AstNode& node,
	ScopeId scope)
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
				prepare_pa12_node(node.children[i], namespace_fact->scope);
		return;
	}
	case PA10NodeKind::LinkageSpecification:
		for (std::size_t i = 0; i < node.children.size(); ++i)
			prepare_pa12_node(node.children[i], scope);
		return;
	case PA10NodeKind::FunctionDefinition:
	{
		const FunctionFact* function = function_fact(node);
		if (function == NULL || node.children.empty())
			throw std::runtime_error("PA12 function fact is missing");
		prepare_pa12_compound(node.children.back(), function->function_scope);
		return;
	}
	default:
		return;
	}
}

void PA11SemanticModel::prepare_pa12_member_parameter(FunctionFact& function)
{
	if (!function.owner.valid() || function.owner.value >= scopes_.size() ||
		scopes_[function.owner.value].kind != ScopeKind::Class ||
		is_static_member(function.binding))
		return;
	const TypeId object_pointer = member_object_pointer_type(
		binding(function.binding).type, function.owner);
	if (!object_pointer.valid())
		throw std::runtime_error("PA12 member function has no object type");
	const NameId this_name = intern_name("this");
	Scope& function_scope = scopes_[function.function_scope.value];
	for (std::size_t i = 0; i < function_scope.bindings.size(); ++i)
	{
		const Binding& parameter = binding(function_scope.bindings[i]);
		if (parameter.kind == BindingKind::Parameter &&
			parameter.name == this_name)
			return;
	}
	store_binding(function.function_scope,
		Binding(BindingKind::Parameter, this_name, object_pointer), 0);
}

void PA11SemanticModel::prepare_pa12()
{
	for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
		prepare_pa12_node(ast_.root.children[i], global_);
}

void PA11SemanticModel::analyze_pa12()
{
	if (ast_.root.kind != PA10NodeKind::TranslationUnit)
		throw std::runtime_error("PA12 root is not a translation unit");
	pa12_render_mode_ = true;
	prepare_pa12();
	for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
		analyze_pa12_node(ast_.root.children[i], global_);
}
BuiltinKind PA11SemanticModel::builtin_kind(const PA10AstNode& node)
{
	if (node.kind != PA10NodeKind::IdExpression || node.has_token ||
		node.global_name || node.name_prefix_count != 0)
		return BuiltinKind::None;
	const NamePath path = name_path(node);
	if (path.components.size() != 1)
		return BuiltinKind::None;
	if (path.last() == builtin_constant_p_name_)
		return BuiltinKind::ConstantP;
	if (path.last() == builtin_abort_name_)
		return BuiltinKind::Abort;
	return BuiltinKind::None;
}
BindingId PA11SemanticModel::builtin_binding(BuiltinKind kind)
{
	if (kind != BuiltinKind::Abort)
		return BindingId();
	if (builtin_abort_binding_.valid())
		return builtin_abort_binding_;
	const TypeId function_type = make_function(std::vector<TypeId>(), false,
		fundamental(FundamentalType::Void));
	const BindingId result(bindings_.size());
	bindings_.push_back(Binding(BindingKind::Function, builtin_abort_name_,
		function_type));
	builtin_abort_binding_ = result;
	return result;
}
PA11SemanticModel::SemanticTailGuard::SemanticTailGuard(PA11SemanticModel& model)
	: model_(model), semantic_begin_(model.semantic_facts_.size()),
	  children_begin_(model.semantic_children_.size()),
	  constant_address_begin_(model.constant_address_facts_.size()),
	  constant_address_bytes_begin_(model.constant_address_literal_bytes_.size()),
	  conversion_begin_(model.conversion_facts_.size()),
	  names_begin_(model.semantic_name_components_.size()), active_(true)
{}
PA11SemanticModel::SemanticTailGuard::~SemanticTailGuard()
{
	discard();
}
void PA11SemanticModel::SemanticTailGuard::discard()
{
	if (!active_)
		return;
	model_.semantic_facts_.resize(semantic_begin_);
	model_.semantic_children_.resize(children_begin_);
	model_.constant_address_facts_.resize(constant_address_begin_);
	model_.constant_address_literal_bytes_.resize(constant_address_bytes_begin_);
	model_.conversion_facts_.resize(conversion_begin_);
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
bool PA11SemanticModel::implicit_default_constructor_supported(
	NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag == ClassTag::Union ||
		!named_[record_id.value].defined ||
		!named_[record_id.value].scope.valid() ||
		named_[record_id.value].scope.value >= scopes_.size())
		return false;
	return scopes_[named_[record_id.value].scope.value].bindings.empty();
}
BindingId PA11SemanticModel::ensure_implicit_default_constructor(
	NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		throw std::runtime_error("invalid implicit constructor record");
	const NamedRecordSidecar* existing = named_record_sidecar(record_id);
	if (existing != NULL && existing->constructor_binding.valid())
		return existing->constructor_binding;
	const TypeId object = named_type(record_id);
	const TypeId constructor_type = make_function(
		std::vector<TypeId>(1, make_pointer(object)), false,
		fundamental(FundamentalType::Void));
	Binding constructor(BindingKind::Function, NameId(), constructor_type);
	const BindingId binding_id(bindings_.size());
	bindings_.push_back(constructor);
	NamedRecordSidecar record_sidecar;
	if (existing != NULL)
		record_sidecar = *existing;
	record_sidecar.constructor_binding = binding_id;
	set_named_record_sidecar(record_id, record_sidecar);
	BindingSidecar binding_sidecar;
	binding_sidecar.constructor_record = record_id;
	set_binding_sidecar(binding_id, binding_sidecar);
	synthetic_function_facts_.push_back(
		SyntheticFunctionFact(record_id, binding_id));
	return binding_id;
}
BindingId PA11SemanticModel::ensure_anonymous_union_constructor(
	NamedRecordId record_id)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class ||
		named_[record_id.value].class_tag != ClassTag::Union)
		throw std::runtime_error("invalid anonymous union constructor record");
	return ensure_implicit_default_constructor(record_id);
}
const AnonymousUnionFact* PA11SemanticModel::anonymous_union_fact(
	const PA10AstNode& node) const
{
	return anonymous_union_fact_index_.find(&node);
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
TypeId PA11SemanticModel::member_access_type(TypeId object, TypeId member)
{
	const unsigned int qualifiers = cv_qualifiers(object);
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
	BindingId storage, const PA10AstNode& source)
{
	const Binding& storage_binding = binding(storage);
	const NamedRecordId record = named_record_for_type(storage_binding.type);
	if (!record.valid() || record.value >= named_.size() ||
		named_[record.value].kind != NamedKind::Class)
		throw std::runtime_error("PA12 constructor action needs a class object");
	const BindingId constructor = named_[record.value].class_tag == ClassTag::Union ?
		ensure_anonymous_union_constructor(record) :
		ensure_implicit_default_constructor(record);
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
	call.selected_scope = named_[record.value].owner;
	const SemanticFactId call_id = make_semantic_fact(call);
	set_semantic_children(call_id,
		std::vector<SemanticFactId>(1, address));
	SemanticFact action(SemanticFactKind::ConstructorAction, TypeId(),
		SemanticValueCategory::Prvalue, &source);
	action.selected_binding = constructor;
	action.selected_scope = named_[record.value].owner;
	const SemanticFactId action_id = make_semantic_fact(action);
	set_semantic_children(action_id,
		std::vector<SemanticFactId>(1, call_id));
	return action_id;
}
ExprInfo PA11SemanticModel::semantic_injected_member(
	const PA10AstNode& node, ScopeId scope, BindingId member_id)
{
	(void)scope;
	const Binding& member = binding(member_id);
	const BindingSidecar* sidecar = binding_sidecar(member_id);
	if (sidecar == NULL || !sidecar->backing_storage.valid())
		throw std::runtime_error("PA12 injected member has no backing storage");
	const ExprInfo object = semantic_storage_id(sidecar->backing_storage);
	const TypeId type = member_access_type(object.type, member.type);
	SemanticFact fact(SemanticFactKind::MemberExpression, type,
		SemanticValueCategory::Lvalue, &node);
	fact.token = SimpleTokenType::OP_DOT;
	fact.binding = member_id;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, name_path(node));
	set_semantic_children(result,
		std::vector<SemanticFactId>(1, object.fact));
	return ExprInfo(result, type, SemanticValueCategory::Lvalue, false);
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
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, member_name);
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
		nullptr_id(type);
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
void PA11SemanticModel::set_semantic_name(SemanticFactId fact, const NamePath& path)
{
	SemanticFact& owner = semantic_facts_[fact.value];
	owner.name_begin = semantic_name_components_.size();
	owner.name_count = path.components.size();
	owner.name_global = path.global;
	semantic_name_components_.insert(semantic_name_components_.end(),
		path.components.begin(), path.components.end());
}
ConversionFactId PA11SemanticModel::add_conversion(TypeId source, TypeId target,
	ConversionKind kind, unsigned int rank)
{
	const ConversionFactId result(conversion_facts_.size());
	conversion_facts_.push_back(ConversionFact(source, target, kind, rank));
	return result;
}
void PA11SemanticModel::set_fact_conversion(SemanticFactId fact, ConversionFactId conversion)
{
	SemanticFact& owner = semantic_facts_[fact.value];
	if (owner.conversion_begin == InvalidIdentityValue)
		owner.conversion_begin = conversion.value;
	else if (owner.conversion_begin + owner.conversion_count != conversion.value)
		throw std::runtime_error("PA12 non-contiguous conversion range");
	++owner.conversion_count;
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
TypeId PA11SemanticModel::callable_function_type(TypeId type) const
{
	type = strip_reference_type(type);
	if (type_kind(strip_cv_type(type)) == TypeKind::Pointer)
		type = types_[strip_cv_type(type).value].child;
	type = strip_cv_type(type);
	return type_kind(type) == TypeKind::Function ? type : TypeId();
}
ConversionChoice PA11SemanticModel::conversion_for(TypeId source,
	SemanticValueCategory category, TypeId target,
	const PA10AstNode* source_node, bool source_integer_zero) const
{
	if (!source.valid() || !target.valid())
		return ConversionChoice();
	const TypeKind target_kind = type_kind(target);
	if (target_kind == TypeKind::LvalueReference ||
		target_kind == TypeKind::RvalueReference)
	{
		const TypeId target_referred = types_[target.value].child;
		const TypeId source_value = expression_object_type(source);
		const bool source_lvalue = category == SemanticValueCategory::Lvalue;
		if (target_kind == TypeKind::LvalueReference && source_lvalue)
		{
			if (!qualification_convertible(source_value, target_referred))
				return ConversionChoice();
			return ConversionChoice(true,
				source_value == target_referred ? 0 : 1,
				ConversionKind::ReferenceBinding);
		}
		if (target_kind == TypeKind::RvalueReference && !source_lvalue)
		{
			if (!qualification_convertible(source_value, target_referred))
				return ConversionChoice();
			return ConversionChoice(true,
				source_value == target_referred ? 0 : 1,
				ConversionKind::ReferenceBinding);
		}
		// A prvalue can bind to a const lvalue reference.  This is the
		// only temporary-binding case in the PA12 foundation.
		if (target_kind == TypeKind::LvalueReference && !source_lvalue &&
			type_kind(target_referred) == TypeKind::Cv &&
			qualification_convertible(source_value, target_referred))
			return ConversionChoice(true, 2, ConversionKind::ReferenceBinding);
		if (type_kind(target_referred) == TypeKind::Cv)
		{
			const ConversionChoice temporary = conversion_for(source, category, target_referred, source_node, source_integer_zero);
			const bool same_lvalue_value = source_lvalue &&
				temporary.kind == ConversionKind::LvalueToRvalue &&
				temporary.rank == 0;
			if (temporary.valid &&
				(target_kind != TypeKind::RvalueReference || !same_lvalue_value))
				return ConversionChoice(true,
					temporary.rank + (target_kind == TypeKind::LvalueReference ? 1 : 0),
					ConversionKind::ReferenceBinding);
		}
		return ConversionChoice();
	}

	const TypeId by_value_source = strip_cv_type(expression_object_type(source));
	const TypeId by_value_target = strip_cv_type(expression_object_type(target));
	const bool null_integer = source_integer_zero || (source_node != NULL && integer_zero(*source_node));
	if (by_value_source == by_value_target)
	{
		return ConversionChoice(true, 0,
			category == SemanticValueCategory::Lvalue ?
			ConversionKind::LvalueToRvalue : ConversionKind::Identity);
	}

	if (type_kind(by_value_source) == TypeKind::Array &&
		type_kind(by_value_target) == TypeKind::Pointer)
	{
		const TypeId element = types_[by_value_source.value].child;
		const TypeId target_element = types_[by_value_target.value].child;
		if (qualification_convertible(element, target_element))
			return ConversionChoice(true, 1, ConversionKind::ArrayToPointer);
	}
	if (type_kind(by_value_source) == TypeKind::Function &&
		type_kind(by_value_target) == TypeKind::Pointer &&
		qualification_convertible(by_value_source,
			types_[by_value_target.value].child))
		return ConversionChoice(true, 1, ConversionKind::FunctionToPointer);
	FundamentalType target_fundamental;
	if (null_integer &&
		fundamental_of(by_value_target, &target_fundamental) &&
		target_fundamental == FundamentalType::NullptrT)
		return ConversionChoice(true, 2, ConversionKind::NullIntegerToNullptr);

	FundamentalType source_fundamental;
	if (fundamental_of(by_value_source, &source_fundamental) &&
		fundamental_of(by_value_target, &target_fundamental) &&
		integral_type(source_fundamental) &&
		integral_type(target_fundamental))
	{
		const unsigned int source_rank = integral_rank(promote_integral_type(by_value_source));
		const unsigned int target_rank = integral_rank(by_value_target);
		return ConversionChoice(true,
			1 + (target_rank > source_rank ? target_rank - source_rank : 0),
			ConversionKind::Integral);
	}
	if (integral_id(by_value_source) && integral_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::Integral);
	if ((floating_id(by_value_source) && floating_id(by_value_target)) ||
		(integral_id(by_value_source) && floating_id(by_value_target)) ||
		(floating_id(by_value_source) && integral_id(by_value_target)))
	{
		const unsigned int source_rank = floating_rank(by_value_source);
		const unsigned int target_rank = floating_rank(by_value_target);
		return ConversionChoice(true,
			1 + (target_rank > source_rank ? target_rank - source_rank : 0),
			floating_id(by_value_target) ? ConversionKind::Floating :
			ConversionKind::Integral);
	}
	if (type_kind(by_value_source) == TypeKind::Fundamental &&
		types_[by_value_source.value].fundamental == FundamentalType::NullptrT &&
		pointer_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::NullptrToPointer);
	if (type_kind(by_value_source) == TypeKind::Fundamental &&
		types_[by_value_source.value].fundamental == FundamentalType::NullptrT &&
		bool_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::NullptrToBool);
	if (null_integer &&
		pointer_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::NullIntegerToPointer);
	// Top-level cv belongs to the pointer object and is discarded by
	// lvalue-to-rvalue conversion; pointee qualification remains typed.
	if (pointer_id(by_value_source) && pointer_id(by_value_target) &&
		types_[by_value_source.value].child ==
			types_[by_value_target.value].child &&
		types_[by_value_source.value].cv !=
		types_[by_value_target.value].cv)
		return ConversionChoice(true, 0,
			category == SemanticValueCategory::Lvalue ?
			ConversionKind::LvalueToRvalue : ConversionKind::Identity);
	if (pointer_id(by_value_source) && pointer_id(by_value_target) &&
		pointer_convertible(by_value_source, by_value_target))
	{
		FundamentalType target_pointee;
		const TypeId target_element = types_[strip_cv_type(by_value_target).value].child;
		const bool to_void = fundamental_of(target_element, &target_pointee) &&
			target_pointee == FundamentalType::Void;
		return ConversionChoice(true, to_void ? 2 : 1,
			to_void ? ConversionKind::PointerToVoid :
			ConversionKind::PointerQualification);
	}
	if (bool_id(by_value_target) && pointer_id(by_value_source))
		return ConversionChoice(true, 3, ConversionKind::PointerToBool);
	return ConversionChoice();
}
ExprInfo PA11SemanticModel::apply_context_conversion(const ExprInfo& expression,
	TypeId target, const PA10AstNode* source_node)
{
	const ConversionChoice choice = conversion_for(expression.type, expression.category, target, source_node, expression.integer_zero);
	if (!choice.valid)
		throw std::runtime_error("PA12 invalid conversion");
	if (choice.kind == ConversionKind::ReferenceBinding &&
		(type_kind(target) == TypeKind::LvalueReference ||
			type_kind(target) == TypeKind::RvalueReference))
	{
		const TypeId referred = types_[target.value].child;
		const TypeId source_value = expression_object_type(expression.type);
		if (type_kind(referred) == TypeKind::Cv &&
			!qualification_convertible(source_value, referred))
		{
				const ConversionChoice temporary = conversion_for(expression.type, expression.category, referred, source_node, expression.integer_zero);
			const PA10AstNode* cast_source = source_node != NULL ? source_node :
				semantic_facts_[expression.fact.value].source;
			if (temporary.valid && cast_source != NULL)
			{
				const SemanticFactId cast = make_expression_fact(
					SemanticFactKind::CastExpression, referred,
					SemanticValueCategory::Prvalue, *cast_source,
					std::vector<SemanticFactId>(1, expression.fact));
				set_fact_conversion(cast, add_conversion(expression.type, referred,
					temporary.kind, temporary.rank));
				set_fact_conversion(cast, add_conversion(referred, target,
					choice.kind, choice.rank));
				return ExprInfo(cast, referred, SemanticValueCategory::Prvalue,
					false);
			}
		}
	}
	const ConversionFactId conversion = add_conversion(expression.type, target,
		choice.kind, choice.rank);
	set_fact_conversion(expression.fact, conversion);
	ExprInfo result = expression;
	if (choice.kind == ConversionKind::NullIntegerToPointer ||
		choice.kind == ConversionKind::NullIntegerToNullptr)
	{
		semantic_facts_[result.fact.value].type = target;
		result.type = target;
	}
	return result;
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
ExprInfo PA11SemanticModel::semantic_builtin_call(const PA10AstNode& node, ScopeId scope, BuiltinKind builtin, const PA10AstNode& argument_node)
{
	if (builtin == BuiltinKind::ConstantP)
	{
		if (argument_node.children.size() != 1)
			throw std::runtime_error("PA12 invalid __builtin_constant_p arity");
		const PA10AstNode& operand_node = argument_node.children.front();
		SemanticTailGuard operand_tail(*this);
		const ExprInfo operand = semantic_expression(operand_node, scope);
		const TypeId operand_type = operand.type;
		operand_tail.discard();
		const bool integral_operand = integral_id(operand_type);
		bool constant = false;
		if (integral_operand)
		{
			// Semantic validation is complete.  Only a typed fold failure is a
			// nonconstant result; malformed or invalid model state must escape.
			try
			{
				constant = eval_constexpr(operand_node, scope).valid;
			}
			catch (const NonConstantExpression&)
			{
				constant = false;
			}
		}
		SemanticFact fact(SemanticFactKind::Literal, fundamental(FundamentalType::Int),
			SemanticValueCategory::Prvalue, &node);
		fact.has_literal_value = true;
		fact.literal_value = constant ? 1 : 0;
		const SemanticFactId result = make_semantic_fact(fact);
		return ExprInfo(result, fact.type, SemanticValueCategory::Prvalue, !constant);
	}
	if (builtin != BuiltinKind::Abort || !argument_node.children.empty())
		throw std::runtime_error("PA12 invalid __builtin_abort arity");
	SemanticFact fact(SemanticFactKind::CallExpression, fundamental(FundamentalType::Void), SemanticValueCategory::Prvalue, &node);
	fact.has_callee = true;
	fact.selected_binding = builtin_binding(builtin);
	fact.selected_scope = global_;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_children(result, std::vector<SemanticFactId>());
	return ExprInfo(result, fact.type, SemanticValueCategory::Prvalue, false);
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
	TypeId target)
{
	const PA10AstNode* source = expression.fact.valid() &&
		expression.fact.value < semantic_facts_.size() ?
		semantic_facts_[expression.fact.value].source : NULL;
	const ConversionChoice choice = conversion_for(expression.type, expression.category, target, source, expression.integer_zero);
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
		choice.kind, choice.rank));
}
SemanticFactId PA11SemanticModel::make_expression_fact(SemanticFactKind kind, TypeId type,
	SemanticValueCategory category, const PA10AstNode& node,
	const std::vector<SemanticFactId>& children)
{
	SemanticFact fact(kind, type, category, &node);
	fact.token = node.token;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_children(result, children);
	return result;
}
ExprInfo PA11SemanticModel::semantic_id_expression(const PA10AstNode& node, ScopeId scope)
{
	if (has_template_id(node))
		throw std::runtime_error("PA12 template-id requires a target");
	const NamePath path = name_path(node);
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
	{
		SemanticFact fact(SemanticFactKind::Literal, value.type,
			SemanticValueCategory::Prvalue, &node);
		fact.has_literal_value = true;
		fact.literal_value_negative = value.value < 0;
		fact.literal_value = value.value < 0 ?
			static_cast<std::uint64_t>(-(value.value + 1)) + 1 :
			static_cast<std::uint64_t>(value.value);
		return ExprInfo(make_semantic_fact(fact), value.type,
			SemanticValueCategory::Prvalue, false);
	}
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
	if (operand.category != SemanticValueCategory::Lvalue ||
		!modifiable_lvalue(operand.type) ||
		(!integral_id(operand.type) && !floating_id(operand.type) &&
			!pointer_id(operand.type)))
		throw std::runtime_error("PA12 postfix requires modifiable lvalue");
	const TypeId type = strip_top_cv_type(operand.type);
	record_builtin_conversion(operand, integral_id(operand.type) ?
		promote_integral_type(operand.type) : type);
	return ExprInfo(make_expression_fact(SemanticFactKind::PostfixExpression,
		type, SemanticValueCategory::Prvalue, node,
		std::vector<SemanticFactId>(1, operand.fact)), type,
		SemanticValueCategory::Prvalue, false);
}
ExprInfo PA11SemanticModel::semantic_binary_expression(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 2 || !node.has_token)
		throw std::runtime_error("PA12 invalid binary expression");
	const ExprInfo left = semantic_expression(node.children[0], scope);
	const ExprInfo right = semantic_expression(node.children[1], scope);
	TypeId type;
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	const TypeId left_object = strip_cv_type(expression_object_type(left.type));
	const TypeId right_object = strip_cv_type(expression_object_type(right.type));
	const bool left_array = type_kind(left_object) == TypeKind::Array;
	const bool right_array = type_kind(right_object) == TypeKind::Array;
	const bool left_pointer = pointer_id(left.type) || left_array;
	const bool right_pointer = pointer_id(right.type) || right_array;
	const bool left_arithmetic = integral_id(left.type) || floating_id(left.type);
	const bool right_arithmetic = integral_id(right.type) || floating_id(right.type);
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
	switch (node.token)
	{
	case SimpleTokenType::OP_COMMA:
		type = right.type;
		category = right.category;
		break;
	case SimpleTokenType::OP_PLUS:
	case SimpleTokenType::OP_MINUS:
		if (left_pointer && integral_id(right.type))
		{
			type = left_pointer_type;
			record_builtin_conversion(left, left_pointer_type);
			record_builtin_conversion(right, promote_integral_type(right.type));
		}
		else if (node.token == SimpleTokenType::OP_PLUS &&
			right_pointer && integral_id(left.type))
		{
			type = right_pointer_type;
			record_builtin_conversion(right, right_pointer_type);
			record_builtin_conversion(left, promote_integral_type(left.type));
		}
		else if (left_pointer && right_pointer &&
			node.token == SimpleTokenType::OP_MINUS)
		{
			const TypeId common_pointer = pointer_subtraction_common_type(
				left_pointer_type, right_pointer_type);
			if (!common_pointer.valid())
				throw std::runtime_error("PA12 incompatible pointer subtraction");
			record_builtin_conversion(left, common_pointer);
			record_builtin_conversion(right, common_pointer);
			type = fundamental(FundamentalType::LongInt);
		}
		else if (left_arithmetic && right_arithmetic)
		{
			type = common_arithmetic_type(left.type, right.type);
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
			type = common_arithmetic_type(left.type, right.type);
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
		type = common_integral_type(left.type, right.type);
		record_builtin_conversion(left, type);
		record_builtin_conversion(right, type);
		break;
	case SimpleTokenType::OP_LSHIFT:
	case SimpleTokenType::OP_RSHIFT:
	{
		if (!integral_id(left.type) || !integral_id(right.type))
			throw std::runtime_error("PA12 invalid shift operands");
		const TypeId promoted_left = promote_integral_type(left.type);
		const TypeId promoted_right = promote_integral_type(right.type);
		record_builtin_conversion(left, promoted_left);
		record_builtin_conversion(right, promoted_right);
		type = promoted_left;
		break;
	}
	case SimpleTokenType::OP_LAND:
	case SimpleTokenType::OP_LOR:
		if (!scalar_id(left.type) || !scalar_id(right.type))
			throw std::runtime_error("PA12 invalid logical operands");
		record_builtin_conversion(left, fundamental(FundamentalType::Bool));
		record_builtin_conversion(right, fundamental(FundamentalType::Bool));
		type = fundamental(FundamentalType::Bool);
		break;
	case SimpleTokenType::OP_EQ:
	case SimpleTokenType::OP_NE:
	case SimpleTokenType::OP_LT:
	case SimpleTokenType::OP_LE:
	case SimpleTokenType::OP_GT:
	case SimpleTokenType::OP_GE:
		if (left_pointer && right_pointer)
		{
			if (!pointer_convertible(left_pointer_type, right_pointer_type) &&
				!pointer_convertible(right_pointer_type, left_pointer_type))
				throw std::runtime_error("PA12 incompatible pointer comparison");
			const TypeId common_pointer = pointer_convertible(
				left_pointer_type, right_pointer_type) ? right_pointer_type :
				left_pointer_type;
			record_builtin_conversion(left, common_pointer);
			record_builtin_conversion(right, common_pointer);
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
			record_builtin_conversion(left_pointer ? left : right, pointer_type);
			record_builtin_conversion(other, pointer_type);
		}
		else if (nullptr_id(left.type) && nullptr_id(right.type))
		{
			if (node.token != SimpleTokenType::OP_EQ &&
				node.token != SimpleTokenType::OP_NE)
				throw std::runtime_error("PA12 invalid nullptr comparison");
		}
		else if (left_arithmetic && right_arithmetic)
		{
			type = common_arithmetic_type(left.type, right.type);
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
	semantic_facts_[result.value].size_type_derived = size_type_derived;
	return ExprInfo(result, type, category, false);
}
ExprInfo PA11SemanticModel::semantic_assignment_expression(const PA10AstNode& node,
	ScopeId scope)
{
	if (node.children.size() != 2 || !node.has_token)
		throw std::runtime_error("PA12 invalid assignment expression");
	const ExprInfo left = semantic_expression(node.children[0], scope);
	const ExprInfo right = semantic_expression(node.children[1], scope);
	if (left.category != SemanticValueCategory::Lvalue)
		throw std::runtime_error("PA12 assignment requires lvalue");
	if (!modifiable_lvalue(left.type))
		throw std::runtime_error("PA12 assignment requires modifiable lvalue");
	const TypeId target = expression_object_type(left.type);
	if (node.token == SimpleTokenType::OP_ASS)
		apply_context_conversion(right, target, semantic_facts_[right.fact.value].source);
	else
	{
		const bool pointer_plus = pointer_id(target) &&
			(node.token == SimpleTokenType::OP_PLUSASS || node.token == SimpleTokenType::OP_MINUSASS);
		if (pointer_plus)
		{
			if (!integral_id(right.type))
				throw std::runtime_error("PA12 pointer compound assignment requires integral");
			record_builtin_conversion(left, target);
			record_builtin_conversion(right, promote_integral_type(right.type));
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
				record_builtin_conversion(left, promote_integral_type(target));
				record_builtin_conversion(right,
					promote_integral_type(right.type));
			}
			else
			{
				const TypeId operation_type = integral_operator ?
					common_integral_type(target, right.type) :
					common_arithmetic_type(target, right.type);
				record_builtin_conversion(left, operation_type);
				record_builtin_conversion(right, operation_type);
			}
		}
	}
	std::vector<SemanticFactId> children;
	children.push_back(left.fact);
	children.push_back(right.fact);
	return ExprInfo(make_expression_fact(SemanticFactKind::AssignmentExpression,
		target, SemanticValueCategory::Lvalue, node, children), target,
		SemanticValueCategory::Lvalue, false);
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
	if (!true_array && !false_array &&
		strip_top_cv_type(true_object) == strip_top_cv_type(false_object))
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
		type = common_arithmetic_type(when_true.type, when_false.type);
		record_builtin_conversion(when_true, type);
		record_builtin_conversion(when_false, type);
	}
	else if (true_pointer && false_pointer)
	{
		type = conditional_pointer_common_type(true_pointer_type,
			false_pointer_type);
		if (!type.valid())
			throw std::runtime_error("PA12 incompatible conditional pointers");
		record_builtin_conversion(when_true, type);
		record_builtin_conversion(when_false, type);
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
	return semantic_cast_to_target(node, target, operand);
}
ExprInfo PA11SemanticModel::semantic_call_expression(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 2)
		throw std::runtime_error("PA12 invalid call expression");
	const PA10AstNode& callee_node = node.children.front();
	const PA10AstNode& argument_node = node.children.back();
	if (argument_node.kind != PA10NodeKind::ArgumentList &&
		argument_node.kind != PA10NodeKind::ParenArgumentList)
		throw std::runtime_error("PA12 invalid argument list");
	const BuiltinKind builtin = builtin_kind(callee_node);
	if (builtin != BuiltinKind::None)
		return semantic_builtin_call(node, scope, builtin, argument_node);

	// PA10 retains function-style casts as call-shaped syntax.  Resolve the
	// typed target here; aliases are considered only after value lookup so a
	// real function or hiding value keeps ordinary call semantics.
	TypeId functional_target;
	if (functional_cast_target(callee_node, scope, &functional_target))
		return semantic_functional_cast(node, scope, functional_target,
			argument_node);
	if (callee_node.kind == PA10NodeKind::IdExpression &&
		!has_template_id(callee_node))
	{
		const NamePath path = name_path(callee_node);
		if (lookup_value_path(path, scope).empty())
		{
			const TemplateFunctionList* templates = template_functions(path, scope);
			if (templates != NULL && !templates->entries.empty())
				return semantic_template_call(node, scope, *templates,
					argument_node);
		}
	}

	std::vector<ValueRef> candidates;
	bool direct = false;
	ExprInfo indirect_callee;
	TypeId indirect_type;
	if (callee_node.kind == PA10NodeKind::IdExpression)
	{
		const NamePath path = name_path(callee_node);
		candidates = lookup_value_path(path, scope);
		direct = !candidates.empty();
		for (std::size_t i = 0; direct && i < candidates.size(); ++i)
		{
			const Binding& candidate = binding(candidates[i].binding);
			if (candidate.kind != BindingKind::Function ||
				type_kind(candidate.type) != TypeKind::Function)
				direct = false;
		}
	}
	if (!direct)
	{
		indirect_callee = semantic_expression(callee_node, scope);
		indirect_type = callable_function_type(indirect_callee.type);
		if (!indirect_type.valid())
			throw std::runtime_error("PA12 call target is not callable");
		const TypeKey& function = types_[indirect_type.value];
		if ((!function.variadic && argument_node.children.size() !=
			function.parameters.size()) ||
			(function.variadic && argument_node.children.size() <
			function.parameters.size()))
			throw std::runtime_error("PA12 indirect call arity mismatch");
	}

	std::vector<ExprInfo> arguments;
	if (!direct)
	{
		const TypeKey& function = types_[indirect_type.value];
		for (std::size_t i = 0; i < argument_node.children.size(); ++i)
		{
			if (i < function.parameters.size())
				arguments.push_back(semantic_expression_for_target(
					argument_node.children[i], scope, function.parameters[i]));
			else
				arguments.push_back(semantic_expression(argument_node.children[i],
					scope));
		}
	}
	else
	{
		// An overloaded function ID has no expression type until a target
		// function pointer/reference parameter is selected.  Keep it deferred;
		// all ordinary arguments are still analyzed exactly once here.
		for (std::size_t i = 0; i < argument_node.children.size(); ++i)
		{
			if (target_function_id(argument_node.children[i], scope) != NULL)
				arguments.push_back(ExprInfo());
			else
				arguments.push_back(semantic_expression(argument_node.children[i],
					scope));
		}
	}

	ValueRef selected;
	TypeId selected_type;
	if (direct)
	{
		struct CandidateScore
		{
			ValueRef value;
			TypeId type;
			bool variadic;
			std::vector<unsigned int> ranks;
		};
		std::vector<CandidateScore> viable_candidates;
		const unsigned int ellipsis_rank = std::numeric_limits<unsigned int>::max() / 4;
		for (std::size_t i = 0; i < candidates.size(); ++i)
		{
			const Binding& candidate = binding(candidates[i].binding);
			if (candidate.kind != BindingKind::Function ||
				type_kind(candidate.type) != TypeKind::Function)
				continue;
			const TypeKey& function = types_[candidate.type.value];
			if ((!function.variadic && arguments.size() !=
				function.parameters.size()) ||
				(function.variadic && arguments.size() <
				function.parameters.size()))
				continue;
			CandidateScore score = {candidates[i], candidate.type,
				function.variadic, std::vector<unsigned int>()};
			score.ranks.reserve(arguments.size());
			for (std::size_t arg = 0; arg < arguments.size(); ++arg)
			{
				if (arg >= function.parameters.size())
				{
					if (!arguments[arg].fact.valid())
						break;
					score.ranks.push_back(ellipsis_rank);
					continue;
				}
				ConversionChoice choice;
				if (arguments[arg].fact.valid())
					choice = conversion_for(arguments[arg].type, arguments[arg].category, function.parameters[arg], semantic_facts_[arguments[arg].fact.value].source, arguments[arg].integer_zero);
				else
				{
					const PA10AstNode* function_id = target_function_id(
						argument_node.children[arg], scope);
					if (function_id != NULL)
					{
						const FunctionIdResolution resolution =
							resolve_function_id_target(*function_id, scope,
								function.parameters[arg]);
						choice = resolution.conversion;
					}
				}
				if (!choice.valid)
					break;
				score.ranks.push_back(choice.rank);
			}
			if (score.ranks.size() != arguments.size())
				continue;
			viable_candidates.push_back(score);
		}
		if (viable_candidates.empty())
			throw std::runtime_error("PA12 no viable call");
		const auto better = [](const CandidateScore& left,
			const CandidateScore& right) -> bool
		{
			bool strict = false;
			for (std::size_t i = 0; i < left.ranks.size(); ++i)
			{
				if (left.ranks[i] > right.ranks[i])
					return false;
				if (left.ranks[i] < right.ranks[i])
					strict = true;
			}
			return strict || (left.variadic != right.variadic && !left.variadic);
		};
		std::size_t best_index = 0;
		for (std::size_t i = 1; i < viable_candidates.size(); ++i)
			if (better(viable_candidates[i], viable_candidates[best_index]))
				best_index = i;
		for (std::size_t i = 0; i < viable_candidates.size(); ++i)
			if (i != best_index && !better(viable_candidates[best_index],
				viable_candidates[i]))
				throw std::runtime_error("PA12 ambiguous call");
		selected = viable_candidates[best_index].value;
		selected_type = viable_candidates[best_index].type;
		const TypeKey& function = types_[selected_type.value];
		for (std::size_t arg = 0; arg < function.parameters.size(); ++arg)
		{
			if (!arguments[arg].fact.valid())
				arguments[arg] = semantic_expression_for_target(
					argument_node.children[arg], scope, function.parameters[arg]);
			arguments[arg] = apply_context_conversion(arguments[arg],
				function.parameters[arg],
				semantic_facts_[arguments[arg].fact.value].source);
		}
	}
	else
	{
		selected_type = indirect_type;
		const TypeKey& function = types_[selected_type.value];
		for (std::size_t arg = 0; arg < function.parameters.size(); ++arg)
			arguments[arg] = apply_context_conversion(arguments[arg],
				function.parameters[arg],
				semantic_facts_[arguments[arg].fact.value].source);
	}

	const TypeId result_type = function_result_type(selected_type);
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	std::vector<SemanticFactId> children;
	if (!direct)
		children.push_back(indirect_callee.fact);
	for (std::size_t i = 0; i < arguments.size(); ++i)
		children.push_back(arguments[i].fact);
	SemanticFact fact(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	fact.has_callee = direct;
	if (direct)
	{
		fact.selected_binding = selected.binding;
		fact.selected_scope = selected.scope;
	}
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_children(result, children);
	return ExprInfo(result, result_type, result_category, false);
}
ExprInfo PA11SemanticModel::semantic_braced_init_list(
	const PA10AstNode& node, TypeId target, ScopeId scope)
{
	if (node.kind != PA10NodeKind::BracedInitList)
		throw std::runtime_error("PA12 expected braced initializer");
	const TypeId object = strip_top_cv_type(target);
	if (type_kind(object) != TypeKind::Array)
		throw std::runtime_error("PA12 braced initializer needs array target");
	const TypeKey& array = types_[object.value];
	if (array.unknown_bound || node.children.size() > array.bound.value)
		throw std::runtime_error("PA12 braced initializer bound mismatch");
	std::vector<SemanticFactId> children;
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		if (node.children[i].kind == PA10NodeKind::BracedInitList)
		{
			children.push_back(semantic_braced_init_list(
				node.children[i], array.child, scope).fact);
			continue;
		}
		const ExprInfo expression = semantic_expression_for_target(
			node.children[i], scope, array.child);
		apply_context_conversion(expression, array.child,
			semantic_facts_[expression.fact.value].source);
		children.push_back(expression.fact);
	}
	return ExprInfo(make_expression_fact(SemanticFactKind::BracedInitList,
		object, SemanticValueCategory::Lvalue, node, children), object,
		SemanticValueCategory::Lvalue, false);
}
ExprInfo PA11SemanticModel::semantic_expression(const PA10AstNode& node, ScopeId scope)
{
	switch (node.kind)
	{
	case PA10NodeKind::Literal:
	case PA10NodeKind::KeywordLiteral:
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
	case PA10NodeKind::SubscriptExpression:
	{
		if (node.children.size() != 2)
			throw std::runtime_error("PA12 invalid subscript expression");
		ExprInfo sequence_expression = semantic_expression(node.children[0], scope);
		ExprInfo index_expression = semantic_expression(node.children[1], scope);
		TypeId sequence = strip_cv_type(expression_object_type(sequence_expression.type));
		if (type_kind(sequence) != TypeKind::Array &&
			type_kind(sequence) != TypeKind::Pointer)
		{
			std::swap(sequence_expression, index_expression);
			sequence = strip_cv_type(expression_object_type(
				sequence_expression.type));
		}
		if (type_kind(sequence) == TypeKind::Array)
			sequence = make_pointer(types_[sequence.value].child);
		if (type_kind(sequence) != TypeKind::Pointer ||
			!integral_id(index_expression.type))
			throw std::runtime_error("PA12 invalid subscript operands");
		record_builtin_conversion(sequence_expression, sequence);
		record_builtin_conversion(index_expression,
			promote_integral_type(index_expression.type));
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
		const Binding& value = binding(binding_id);
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
		apply_context_conversion(expression, value.type,
			semantic_facts_[expression.fact.value].source);
		if (declaration->is_constexpr)
			retarget_constexpr_literal(expression.fact, value.type);
		set_semantic_children(variable,
			std::vector<SemanticFactId>(1, expression.fact));
		declaration->semantic_begin = declaration_semantic_ids_.size();
		declaration->semantic_count = 1;
		declaration_semantic_ids_.push_back(variable);
		return variable;
	}
	if (node.children.size() != 2 ||
		node.children[1].kind != PA10NodeKind::InitDeclaratorList)
		throw std::runtime_error("PA12 invalid declaration fact");
	const PA10AstNode& list = node.children[1];
	if (list.children.size() != declaration->binding_count)
		throw std::runtime_error("PA12 declaration binding mismatch");
	declaration->semantic_begin = declaration_semantic_ids_.size();
	for (std::size_t i = 0; i < list.children.size(); ++i)
	{
		const PA10AstNode& init = list.children[i];
		const BindingId binding_id = declaration_bindings_[
			declaration->binding_begin + i];
		const Binding& value = binding(binding_id);
		SemanticFact fact(SemanticFactKind::Variable, value.type,
			SemanticValueCategory::Prvalue, &init);
		fact.binding = binding_id;
		fact.selected_scope = declaration->scope;
		SemanticFactId variable = make_semantic_fact(fact);
		const NamedRecordId record = named_record_for_type(value.type);
		const NamedRecordSidecar* record_sidecar =
			named_record_sidecar(record);
		const bool anonymous_union_object = record.valid() &&
			record.value < named_.size() &&
			named_[record.value].kind == NamedKind::Class &&
			named_[record.value].class_tag == ClassTag::Union &&
			record_sidecar != NULL && record_sidecar->local_object_name;
		const bool local_object_scope = declaration->scope.valid() &&
			declaration->scope.value < scopes_.size() &&
			scopes_[declaration->scope.value].kind == ScopeKind::Block;
		const bool implicit_default_object = init.children.size() == 1 &&
			local_object_scope && record.valid() &&
			implicit_default_constructor_supported(record);
		const PA10AstNode* direct_operand = NULL;
		if (direct_initializer_operand(init, declaration->scope, &direct_operand))
		{
			const ExprInfo expression = semantic_expression_for_target(
				*direct_operand, declaration->scope, value.type);
			apply_context_conversion(expression, value.type,
				semantic_facts_[expression.fact.value].source);
			set_semantic_children(variable,
				std::vector<SemanticFactId>(1, expression.fact));
		}
		else if (init.children.size() > 1)
		{
			const PA10AstNode& initializer = init.children[1];
			if (initializer.kind != PA10NodeKind::Initializer &&
				initializer.kind != PA10NodeKind::ParenInitializer)
				throw std::runtime_error("PA12 unsupported initializer");
			if (initializer.children.size() != 1)
				throw std::runtime_error("PA12 initializer arity mismatch");
			const PA10AstNode* clause = &initializer.children.front();
			if (clause->kind == PA10NodeKind::ParenInitializer)
			{
				if (clause->children.size() != 1)
					throw std::runtime_error("PA12 invalid parenthesized initializer");
				clause = &clause->children.front();
			}
			const ExprInfo expression = semantic_expression_for_target(
				*clause, declaration->scope, value.type);
			if (clause->kind != PA10NodeKind::BracedInitList)
				apply_context_conversion(expression, value.type,
					semantic_facts_[expression.fact.value].source);
			if (declaration->is_constexpr)
				retarget_constexpr_literal(expression.fact, value.type);
			set_semantic_children(variable,
				std::vector<SemanticFactId>(1, expression.fact));
		}
		else if (anonymous_union_object || implicit_default_object)
		{
			set_semantic_children(variable,
				std::vector<SemanticFactId>(1,
					semantic_constructor_action(binding_id, init)));
		}
		// Namespace initializers are the one PA15 constant boundary.  Persist
		// the typed PA12 fold result (including nested braced elements) once,
		// while the semantic owner still has its validated scope and AST facts.
		record_constant_initializer(variable, declaration->scope);
		declaration_semantic_ids_.push_back(variable);
	}
	declaration->semantic_count = list.children.size();
	return declaration_semantic_ids_[declaration->semantic_begin];
}
FunctionIdResolution PA11SemanticModel::resolve_single_argument_function(
	const NamePath& path, ScopeId scope, const ExprInfo& argument) const
{
	const std::vector<ValueRef> candidates = lookup_value_path(path, scope);
	ValueRef selected;
	ConversionChoice selected_conversion;
	bool have_selected = false, ambiguous_best = false;
	for (std::size_t i = 0; i < candidates.size(); ++i)
	{
		const Binding& candidate = binding(candidates[i].binding);
		if (candidate.kind != BindingKind::Function ||
			type_kind(candidate.type) != TypeKind::Function)
			continue;
		const TypeKey& function = types_[candidate.type.value];
		if (function.parameters.size() != 1)
			continue;
		const ConversionChoice choice = conversion_for(argument.type, argument.category, function.parameters.front(), semantic_facts_[argument.fact.value].source, argument.integer_zero);
		if (!choice.valid)
			continue;
		if (!have_selected || choice.rank < selected_conversion.rank)
		{
			have_selected = true;
			ambiguous_best = false;
			selected = candidates[i];
			selected_conversion = choice;
		}
		else if (choice.rank == selected_conversion.rank)
			ambiguous_best = true;
	}
	if (ambiguous_best)
		throw std::runtime_error("PA12 ambiguous call");
	return have_selected ? FunctionIdResolution(true, selected,
		selected_conversion) : FunctionIdResolution();
}
ExprInfo PA11SemanticModel::semantic_single_argument_call(
	const PA10AstNode& node, const FunctionIdResolution& resolution,
	const ExprInfo& argument)
{
	const ValueRef selected = resolution.selected;
	const TypeKey& function = types_[binding(selected.binding).type.value];
	const ExprInfo converted = apply_context_conversion(argument,
		function.parameters.front(),
		semantic_facts_[argument.fact.value].source);
	const TypeId result_type = function.result;
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	SemanticFact fact(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	fact.has_callee = true;
	fact.selected_binding = selected.binding;
	fact.selected_scope = selected.scope;
	const SemanticFactId call = make_semantic_fact(fact);
	set_semantic_children(call, std::vector<SemanticFactId>(1, converted.fact));
	return ExprInfo(call, result_type, result_category, false);
}
SemanticFactId PA11SemanticModel::semantic_ambiguous_call_statement(
	const PA10AstNode& node, ScopeId scope)
{
	NamePath function_name;
	const PA10AstNode* argument_node = NULL;
	const PA10AstNode* right_node = NULL;
	if (ambiguous_assignment_statement(node, scope, &function_name,
		&argument_node, &right_node))
	{
		const ExprInfo argument = semantic_id_expression(*argument_node, scope);
		const FunctionIdResolution resolution =
			resolve_single_argument_function(function_name, scope, argument);
		if (!resolution.valid)
			throw std::runtime_error("PA12 no viable call");
		const ExprInfo left = semantic_single_argument_call(node, resolution, argument);
		if (left.category != SemanticValueCategory::Lvalue)
			throw std::runtime_error("PA12 assignment requires lvalue");
		if (!modifiable_lvalue(left.type))
			throw std::runtime_error("PA12 assignment requires modifiable lvalue");
		const ExprInfo right_expression = semantic_expression(*right_node, scope);
		const TypeId target = expression_object_type(left.type);
		apply_context_conversion(right_expression, target,
			semantic_facts_[right_expression.fact.value].source);
		SemanticFact assignment_fact(SemanticFactKind::AssignmentExpression, target,
			SemanticValueCategory::Lvalue, &node);
		assignment_fact.token = SimpleTokenType::OP_ASS;
		const SemanticFactId assignment = make_semantic_fact(assignment_fact);
		set_semantic_children(assignment,
			std::vector<SemanticFactId>{left.fact, right_expression.fact});
		return make_expression_fact(SemanticFactKind::ExpressionStatement,
			TypeId(), SemanticValueCategory::Prvalue, node,
			std::vector<SemanticFactId>(1, assignment));
	}
	if (!ambiguous_call_statement(node, scope, &function_name, &argument_node) ||
		argument_node == NULL)
		throw std::runtime_error("PA12 unsupported declaration statement");
	const ExprInfo argument = semantic_id_expression(*argument_node, scope);
	const FunctionIdResolution resolution =
		resolve_single_argument_function(function_name, scope, argument);
	if (!resolution.valid)
		throw std::runtime_error("PA12 no viable call");
	const ExprInfo call = semantic_single_argument_call(node, resolution, argument);
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
			apply_context_conversion(condition, condition_target,
				semantic_facts_[variable.value].source);
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
			apply_context_conversion(expression, condition_target,
				semantic_facts_[expression.fact.value].source);
			children.push_back(expression.fact);
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
		add_conversion(label_type, switch_context.conversion_type,
			choice.kind, choice.rank));
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

SemanticFactId PA11SemanticModel::semantic_jump_statement(
	const PA10AstNode& node, unsigned int loop_depth, unsigned int switch_depth)
{
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
	{
		const Binding& function_binding = binding(function.binding);
		const TypeKey& function_type = types_[function_binding.type.value];
		if (type_kind(function_binding.type) != TypeKind::Function)
			throw std::runtime_error("PA12 function fact has non-function type");
		const TypeId result_type = function_type.result;
		std::vector<SemanticFactId> children;
		if (!node.children.empty())
		{
			if (void_id(result_type))
				throw std::runtime_error("PA12 value returned from void function");
			const ExprInfo expression = semantic_expression(node.children.front(),
				scope);
			apply_context_conversion(expression, result_type,
				semantic_facts_[expression.fact.value].source);
			children.push_back(expression.fact);
		}
		else if (!void_id(result_type))
			throw std::runtime_error("PA12 missing return value");
		return make_expression_fact(SemanticFactKind::ReturnStatement,
			TypeId(), SemanticValueCategory::Prvalue, node, children);
	}
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
	case PA10NodeKind::BreakStatement:
	case PA10NodeKind::ContinueStatement:
		return semantic_jump_statement(node, loop_depth, switch_depth);
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
		for (std::size_t i = 0; i < node.children.size(); ++i)
		{
			if (node.children[i].kind != PA10NodeKind::FunctionDefinition)
				continue;
			analyze_pa12_node(node.children[i], class_scope);
			const FunctionFactId* id = function_fact_index_.find(
				&node.children[i]);
			if (id == NULL || !id->valid())
				throw std::runtime_error("PA12 class function identity is missing");
			class_function_facts_.push_back(*id);
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
		FunctionFact* function = function_fact(node);
		if (function == NULL || function->body_fact.valid())
			throw std::runtime_error("PA12 function fact is missing");
		prepare_pa12_member_parameter(*function);
		function->body_fact = semantic_compound(node.children.back(),
			function->function_scope, *function, 0, 0, NULL);
		break;
	}
	default:
		break;
	}
}
} // namespace pa11_semantic_internal

void emit_pa12_semantics(const PA10Ast& ast, std::ostream& output)
{
	pa11_semantic_internal::PA11SemanticModel model(ast);
	model.analyze();
	model.analyze_pa12();
	model.dump_pa12(output);
}
