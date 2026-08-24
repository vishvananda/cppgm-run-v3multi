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
		false, true);
	DeclarationFact declaration(&node, target);
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

void PA11SemanticModel::prepare_pa12()
{
	for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
		prepare_pa12_node(ast_.root.children[i], global_);
}

void PA11SemanticModel::analyze_pa12()
{
	if (ast_.root.kind != PA10NodeKind::TranslationUnit)
		throw std::runtime_error("PA12 root is not a translation unit");
	prepare_pa12();
	for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
		analyze_pa12_node(ast_.root.children[i], global_);
}
void PA11SemanticModel::dump_pa12(std::ostream& output) const
{
	output << "translation-unit\n";
	for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
		dump_pa12_top_node(output, ast_.root.children[i], global_, 1);
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
	return render_name_path(path);
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
	const PA10AstNode* source_node) const
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
			const ConversionChoice temporary = conversion_for(source, category,
				target_referred, source_node);
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
	if (source_node != NULL && integer_zero(*source_node) &&
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
	if (source_node != NULL && integer_zero(*source_node) &&
		pointer_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::NullIntegerToPointer);
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
	const ConversionChoice choice = conversion_for(expression.type,
		expression.category, target, source_node);
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
			const ConversionChoice temporary = conversion_for(expression.type,
				expression.category, referred, source_node);
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
const PA10AstNode* PA11SemanticModel::target_function_id(
	const PA10AstNode& node, ScopeId scope)
{
	if (node.kind == PA10NodeKind::ParenthesizedExpression)
	{
		if (node.children.size() != 1)
			return NULL;
		return target_function_id(node.children.front(), scope);
	}
	if (node.kind != PA10NodeKind::IdExpression &&
		!(node.kind == PA10NodeKind::DeclSpecifier &&
			node.identifier_declspecifier))
		return NULL;
	const std::vector<ValueRef> values = lookup_value_path(name_path(node), scope);
	if (values.size() <= 1)
		return NULL;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		const Binding& value = binding(values[i].binding);
		if (value.kind != BindingKind::Function ||
			type_kind(value.type) != TypeKind::Function)
			return NULL;
	}
	return &node;
}
FunctionIdResolution PA11SemanticModel::resolve_function_id_target(
	const PA10AstNode& node, ScopeId scope, TypeId target)
{
	if (node.kind != PA10NodeKind::IdExpression &&
		!(node.kind == PA10NodeKind::DeclSpecifier &&
			node.identifier_declspecifier))
		return FunctionIdResolution();
	const std::vector<ValueRef> values = lookup_value_path(name_path(node), scope);
	ValueRef selected;
	ConversionChoice selected_conversion;
	bool have_selected = false;
	bool ambiguous = false;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		const Binding& value = binding(values[i].binding);
		if (value.kind != BindingKind::Function ||
			type_kind(value.type) != TypeKind::Function)
			continue;
		const ConversionChoice conversion = conversion_for(value.type,
			SemanticValueCategory::Lvalue, target, &node);
		if (!conversion.valid)
			continue;
		if (!have_selected || conversion.rank < selected_conversion.rank)
		{
			have_selected = true;
			selected = values[i];
			selected_conversion = conversion;
			ambiguous = false;
		}
		else if (conversion.rank == selected_conversion.rank)
		{
			ambiguous = true;
		}
	}
	return have_selected && !ambiguous ? FunctionIdResolution(true, selected,
		selected_conversion) : FunctionIdResolution();
}
ExprInfo PA11SemanticModel::semantic_id_expression_selected(
	const PA10AstNode& node, ScopeId scope,
	const FunctionIdResolution& resolution)
{
	(void)scope;
	if (!resolution.valid)
		throw std::runtime_error("PA12 target does not select a function");
	const Binding& value = binding(resolution.selected.binding);
	if (value.kind != BindingKind::Function ||
		type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA12 target selected a non-function");
	SemanticFact fact(SemanticFactKind::IdExpression, value.type,
		SemanticValueCategory::Lvalue, &node);
	fact.binding = resolution.selected.binding;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, name_path(node));
	return ExprInfo(result, value.type, SemanticValueCategory::Lvalue, false);
}
ExprInfo PA11SemanticModel::semantic_expression_for_target(
	const PA10AstNode& node, ScopeId scope, TypeId target)
{
	if (node.kind == PA10NodeKind::BracedInitList)
		return semantic_braced_init_list(node, target, scope);
	const PA10AstNode* function_id = target_function_id(node, scope);
	if (function_id == NULL)
	{
		if (node.kind == PA10NodeKind::DeclSpecifier &&
			node.identifier_declspecifier)
			return semantic_id_expression(node, scope);
		return semantic_expression(node, scope);
	}
	const FunctionIdResolution resolution = resolve_function_id_target(
		*function_id, scope, target);
	if (!resolution.valid)
		throw std::runtime_error("PA12 no function matches target type");
	return semantic_id_expression_selected(*function_id, scope, resolution);
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
		const ExprInfo operand = semantic_expression(operand_node, scope);
		bool constant = false;
		if (integral_id(operand.type))
		{
			// Validation above leaves only supported integral queries; folding
			// failure means this valid expression is not a propagated constant.
			try
			{
				constant = eval_constexpr(operand_node, scope).valid;
			}
			catch (const std::runtime_error&)
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
	const ConversionChoice choice = conversion_for(expression.type,
		expression.category, target, source);
	if (!choice.valid)
		throw std::runtime_error("PA12 invalid built-in conversion");
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
SemanticFactId PA11SemanticModel::semantic_literal(const PA10AstNode& node)
{
	TypeId type;
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	std::size_t element_count = 0;
	if (node.kind == PA10NodeKind::Literal)
	{
		type = fundamental(node.literal.type);
		element_count = node.literal.element_count;
		if (element_count != 0)
		{
			type = make_array(make_cv(type, 1u), false,
				ArrayBound(element_count));
			category = SemanticValueCategory::Lvalue;
		}
	}
	else if (node.kind == PA10NodeKind::KeywordLiteral)
	{
		if (node.token == SimpleTokenType::KW_TRUE ||
			node.token == SimpleTokenType::KW_FALSE)
			type = fundamental(FundamentalType::Bool);
		else if (node.token == SimpleTokenType::KW_NULLPTR)
			type = fundamental(FundamentalType::NullptrT);
		else
			throw std::runtime_error("PA12 unsupported keyword literal");
	}
	else
		throw std::runtime_error("PA12 expected literal");
	SemanticFact fact(SemanticFactKind::Literal, type, category, &node);
	fact.token = node.token;
	fact.literal_element_count = element_count;
	return make_semantic_fact(fact);
}
ExprInfo PA11SemanticModel::semantic_id_expression(const PA10AstNode& node, ScopeId scope)
{
	const NamePath path = name_path(node);
	const std::vector<ValueRef> values = lookup_value_path(path, scope);
	if (values.empty())
		throw std::runtime_error("PA12 unknown expression name");
	if (values.size() != 1)
		throw std::runtime_error("PA12 overloaded id requires a target");
	const Binding& value = binding(values.front().binding);
	TypeId type = value.type;
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
	else if (type_kind(type) == TypeKind::LvalueReference ||
		type_kind(type) == TypeKind::RvalueReference)
		type = types_[type.value].child;
	SemanticFact fact(SemanticFactKind::IdExpression, type, category, &node);
	fact.binding = values.front().binding;
	const SemanticFactId result = make_semantic_fact(fact);
	set_semantic_name(result, path);
	return ExprInfo(result, type, category, false);
}
ExprInfo PA11SemanticModel::semantic_unary_expression(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 1 || !node.has_token)
		throw std::runtime_error("PA12 invalid unary expression");
	const ExprInfo operand = semantic_expression(node.children.front(), scope);
	TypeId type = expression_object_type(operand.type);
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	switch (node.token)
	{
	case SimpleTokenType::OP_AMP:
		if (operand.category != SemanticValueCategory::Lvalue)
			throw std::runtime_error("PA12 address-of requires lvalue");
		type = make_pointer(operand.type);
		break;
	case SimpleTokenType::OP_STAR:
	{
		TypeId pointer = strip_cv_type(expression_object_type(operand.type));
		if (type_kind(pointer) == TypeKind::Array)
		{
			pointer = make_pointer(types_[pointer.value].child);
			record_builtin_conversion(operand, pointer);
		}
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 dereference requires pointer");
		if (type_kind(strip_cv_type(expression_object_type(operand.type))) ==
			TypeKind::Pointer)
			record_builtin_conversion(operand, pointer);
		type = types_[pointer.value].child;
		category = SemanticValueCategory::Lvalue;
		break;
	}
	case SimpleTokenType::OP_INC:
	case SimpleTokenType::OP_DEC:
		if (operand.category != SemanticValueCategory::Lvalue ||
			!modifiable_lvalue(operand.type) ||
			(!integral_id(operand.type) && !floating_id(operand.type) && !pointer_id(operand.type)))
			throw std::runtime_error("PA12 increment requires modifiable lvalue");
		type = strip_top_cv_type(operand.type);
		record_builtin_conversion(operand, integral_id(operand.type) ?
			promote_integral_type(operand.type) : type);
		category = SemanticValueCategory::Lvalue;
		break;
	case SimpleTokenType::OP_PLUS:
	case SimpleTokenType::OP_MINUS:
	case SimpleTokenType::OP_COMPL:
		if (!integral_id(operand.type) &&
			(node.token == SimpleTokenType::OP_COMPL || !floating_id(operand.type)))
			throw std::runtime_error("PA12 unary arithmetic requires integral");
		type = node.token == SimpleTokenType::OP_COMPL ?
			common_integral_type(operand.type, operand.type) :
			common_arithmetic_type(operand.type, operand.type);
		record_builtin_conversion(operand, type);
		break;
	case SimpleTokenType::OP_LNOT:
		if (!scalar_id(operand.type))
			throw std::runtime_error("PA12 logical negation requires scalar");
		record_builtin_conversion(operand, fundamental(FundamentalType::Bool));
		type = fundamental(FundamentalType::Bool);
		break;
	default:
		throw std::runtime_error("PA12 unsupported unary operator");
	}
	return ExprInfo(make_expression_fact(SemanticFactKind::UnaryExpression,
		type, category, node, std::vector<SemanticFactId>(1, operand.fact)),
		type, category, false);
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
	return ExprInfo(make_expression_fact(SemanticFactKind::BinaryExpression,
		type, category, node, children), type, category, false);
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
ExprInfo PA11SemanticModel::semantic_cast_expression(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() < 2)
		throw std::runtime_error("PA12 invalid cast expression");
	const TypeId target = type_from_type_id(node.children.front(), scope);
	const ExprInfo operand = semantic_expression(node.children.back(), scope);
	const TypeId source = expression_object_type(operand.type);
	const TypeKind target_kind = type_kind(target);
	if (target_kind == TypeKind::LvalueReference ||
		target_kind == TypeKind::RvalueReference)
	{
		const TypeId referred = types_[target.value].child;
		if (!qualification_convertible(source, referred))
			throw std::runtime_error("PA12 invalid reference cast");
		if (operand.category == SemanticValueCategory::Lvalue)
		{
			const SemanticValueCategory category =
				target_kind == TypeKind::RvalueReference ?
				SemanticValueCategory::Xvalue : SemanticValueCategory::Lvalue;
			semantic_facts_[operand.fact.value].type = target;
			semantic_facts_[operand.fact.value].category = category;
			return ExprInfo(operand.fact, target, category, false);
		}
		throw std::runtime_error("PA12 invalid reference cast category");
	}
	bool valid = false;
	ConversionKind kind = ConversionKind::Integral;
	if (void_id(target))
		valid = scalar_id(source) || type_kind(source) == TypeKind::Function;
	else if (integral_id(target))
	{
		if (bool_id(target))
		{
			const ConversionChoice choice = conversion_for(source,
				operand.category, target,
				semantic_facts_[operand.fact.value].source);
			valid = choice.valid;
			kind = choice.kind;
		}
		else
		{
			valid = integral_id(source) ||
				(type_kind(strip_cv_type(source)) == TypeKind::Named &&
				 named_record_for_type(source).valid()) ||
				nullptr_id(source);
			kind = ConversionKind::Integral;
		}
	}
	else if (floating_id(target) || pointer_id(target))
	{
		const ConversionChoice choice = conversion_for(source,
			operand.category, target, semantic_facts_[operand.fact.value].source);
		valid = choice.valid;
		kind = choice.kind;
	}
	else if (type_kind(strip_cv_type(target)) == TypeKind::Named)
		valid = integral_id(source) || source == target;
	if (!valid)
		throw std::runtime_error("PA12 invalid explicit cast");
	const ConversionFactId conversion = add_conversion(source, target, kind, 0);
	const SemanticFactId result = make_expression_fact(
		SemanticFactKind::CastExpression, target,
		SemanticValueCategory::Prvalue, node,
		std::vector<SemanticFactId>(1, operand.fact));
	set_fact_conversion(result, conversion);
	return ExprInfo(result, target, SemanticValueCategory::Prvalue, false);
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

	// PA10 intentionally retains built-in function-style casts as a
	// call-shaped syntax node.  Resolve that typed vocabulary here without
	// turning the rendered AST back into a semantic input.
	TypeId builtin_target;
	if (argument_node.kind == PA10NodeKind::ParenArgumentList &&
		builtin_cast_target(callee_node, &builtin_target))
	{
		if (argument_node.children.size() != 1)
			throw std::runtime_error("PA12 invalid functional cast arity");
		const ExprInfo operand = semantic_expression(
			argument_node.children.front(), scope);
		const TypeId source = expression_object_type(operand.type);
		if (!void_id(builtin_target) && !scalar_id(source) &&
			type_kind(source) != TypeKind::Function)
			throw std::runtime_error("PA12 invalid functional cast");
		const SemanticFactId result = make_expression_fact(
			SemanticFactKind::CastExpression, builtin_target,
			SemanticValueCategory::Prvalue, node,
			std::vector<SemanticFactId>(1, operand.fact));
		return ExprInfo(result, builtin_target,
			SemanticValueCategory::Prvalue, false);
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
					choice = conversion_for(arguments[arg].type,
						arguments[arg].category, function.parameters[arg],
						semantic_facts_[arguments[arg].fact.value].source);
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
		(void)sizeof_operand_type(node, scope);
		const TypeId result = fundamental(FundamentalType::UnsignedLongInt);
		const SemanticFactId fact = make_expression_fact(
			SemanticFactKind::SizeofExpression, result,
			SemanticValueCategory::Prvalue, node,
			std::vector<SemanticFactId>());
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
		const ExprInfo expression = semantic_expression_for_target(
			initializer.children.front(), declaration->scope, value.type);
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
			const PA10AstNode& clause = initializer.children.front();
			const ExprInfo expression = semantic_expression_for_target(
				clause, declaration->scope, value.type);
			if (clause.kind != PA10NodeKind::BracedInitList)
				apply_context_conversion(expression, value.type,
					semantic_facts_[expression.fact.value].source);
			if (declaration->is_constexpr)
				retarget_constexpr_literal(expression.fact, value.type);
			set_semantic_children(variable,
				std::vector<SemanticFactId>(1, expression.fact));
		}
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
		const ConversionChoice choice = conversion_for(argument.type,
			argument.category, function.parameters.front(),
			semantic_facts_[argument.fact.value].source);
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
SemanticFactId PA11SemanticModel::semantic_statement(const PA10AstNode& node,
	ScopeId scope, const FunctionFact& function, unsigned int loop_depth,
	unsigned int switch_depth, SwitchValidationContext* switch_context)
{
	switch (node.kind)
	{
	case PA10NodeKind::EmptyDeclaration:
		return SemanticFactId();
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
		function->body_fact = semantic_compound(node.children.back(),
			function->function_scope, *function, 0, 0, NULL);
		break;
	}
	default:
		break;
	}
}
const char* PA11SemanticModel::semantic_category_name(SemanticValueCategory category) const
{
	switch (category)
	{
	case SemanticValueCategory::Lvalue: return "lvalue";
	case SemanticValueCategory::Prvalue: return "prvalue";
	case SemanticValueCategory::Xvalue: return "xvalue";
	}
	return "prvalue";
}
std::string PA11SemanticModel::semantic_operator(const SemanticFact& fact) const
{
	std::ostringstream result;
	result << simple_token_type_name(fact.token) << ':';
	if (fact.source != NULL && fact.source->has_token &&
		fact.source->token == fact.token && fact.source->token_spelling != 0)
		result << ast_.spelling(fact.source->token_spelling);
	else if (fact.token == SimpleTokenType::OP_ASS)
		result << '=';
	return result.str();
}
std::string PA11SemanticModel::semantic_literal_token(const SemanticFact& fact) const
{
	if (fact.source == NULL)
		return std::string();
	if (fact.has_literal_value)
	{
		std::ostringstream result;
		if (fact.literal_value_unsigned)
			result << fact.literal_value;
		else
		{
			if (fact.literal_value_negative)
				result << '-';
			result << fact.literal_value;
		}
		return result.str();
	}
	std::ostringstream result;
	if (fact.source->kind == PA10NodeKind::KeywordLiteral)
		result << simple_token_type_name(fact.source->token) << ':';
	if (fact.source->kind == PA10NodeKind::Literal)
	{
		if (fact.source->text != 0)
			result << ast_.spelling(fact.source->text);
	}
	else if (fact.source->token_spelling != 0)
		result << ast_.spelling(fact.source->token_spelling);
	return result.str();
}
void PA11SemanticModel::dump_pa12_fact(std::ostream& output, SemanticFactId id,
	std::size_t depth) const
{
	if (!id.valid() || id.value >= semantic_facts_.size())
		throw std::runtime_error("invalid PA12 semantic fact");
	const SemanticFact& fact = semantic_facts_[id.value];
	const std::string type = fact.type.valid() ? render_type(fact.type) :
		std::string();
	const std::string op = semantic_operator(fact);
	for (std::size_t indent = 0; indent < depth; ++indent)
		output << "  ";
	switch (fact.kind)
	{
	case SemanticFactKind::Variable:
	{
		const Binding& value = binding(fact.binding);
		if (value.kind == BindingKind::TypeAlias)
			output << "type-alias ";
		else if (value.kind == BindingKind::Function)
			output << "function-declaration ";
		else
			output << "variable ";
		if (value.kind == BindingKind::Function && fact.selected_scope.valid())
			output << qualified_binding_name(fact.selected_scope, value.name);
		else
			output << name_text(value.name);
		output << ' ' << render_binding_type(value) << '\n';
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	}
	case SemanticFactKind::TypeAlias:
	{
		const Binding& value = binding(fact.binding);
		output << "type-alias " << name_text(value.name) << ' ' <<
			render_binding_type(value) << '\n';
		return;
	}
	case SemanticFactKind::SimpleDeclaration:
		output << "simple-declaration\n";
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::CompoundStatement:
		output << "compound-statement\n";
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::ReturnStatement:
		output << "return-statement\n";
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::ExpressionStatement:
		output << "expression-statement\n";
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::CallExpression:
		output << "call-expression " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		if (fact.has_callee)
		{
			const Binding& callee = binding(fact.selected_binding);
			for (std::size_t indent = 0; indent < depth + 1; ++indent)
				output << "  ";
			output << "callee " << qualified_binding_name(fact.selected_scope,
				callee.name) << ' ' << render_binding_type(callee) << '\n';
		}
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::IdExpression:
		output << "id-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << semantic_name(fact) << '\n';
		return;
	case SemanticFactKind::Literal:
		output << "literal " << semantic_category_name(fact.category) << ' ' <<
			type << ' ' << semantic_literal_token(fact) << '\n';
		return;
	case SemanticFactKind::UnaryExpression:
		output << "unary-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << op << '\n';
		break;
	case SemanticFactKind::PostfixExpression:
		output << "postfix-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << op << '\n';
		break;
	case SemanticFactKind::BinaryExpression:
		output << "binary-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << op << '\n';
		break;
	case SemanticFactKind::AssignmentExpression:
		output << "assignment-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << op << '\n';
		break;
	case SemanticFactKind::ConditionalExpression:
		output << "conditional-expression " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		break;
	case SemanticFactKind::CastExpression:
		output << "cast-expression " << semantic_category_name(fact.category) <<
			' ' << type;
		if (fact.source != NULL && fact.source->has_token)
			output << ' ' << op;
		output << '\n';
		break;
	case SemanticFactKind::SubscriptExpression:
		output << "subscript-expression " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		break;
	case SemanticFactKind::BracedInitList:
		output << "braced-init-list " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		break;
	case SemanticFactKind::SizeofExpression:
		output << "sizeof-expression " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		return;
	case SemanticFactKind::IfStatement:
		output << "if-statement\n";
		break;
	case SemanticFactKind::ThenBranch:
		output << "then\n";
		break;
	case SemanticFactKind::ElseBranch:
		output << "else\n";
		break;
	case SemanticFactKind::SwitchStatement:
		output << "switch-statement\n";
		break;
	case SemanticFactKind::WhileStatement:
		output << "while-statement\n";
		break;
	case SemanticFactKind::DoStatement:
		output << "do-statement\n";
		break;
	case SemanticFactKind::ForStatement:
		output << "for-statement\n";
		break;
	case SemanticFactKind::ForInitStatement:
		output << "for-init-statement\n";
		break;
	case SemanticFactKind::Condition:
		output << "condition\n";
		break;
	case SemanticFactKind::ConditionDeclaration:
		output << "condition-declaration\n";
		break;
	case SemanticFactKind::Iteration:
		output << "iteration\n";
		break;
	case SemanticFactKind::CaseStatement:
		output << "case-statement\n";
		break;
	case SemanticFactKind::DefaultStatement:
		output << "default-statement\n";
		break;
	case SemanticFactKind::BreakStatement:
		output << "break-statement\n";
		break;
	case SemanticFactKind::ContinueStatement:
		output << "continue-statement\n";
		break;
	}
	for (std::size_t i = 0; i < fact.child_count; ++i)
		dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
			depth + 1);
}
void PA11SemanticModel::dump_pa12_function(std::ostream& output, const PA10AstNode& node,
	std::size_t depth) const
{
	const FunctionFact* function = function_fact(node);
	if (function == NULL || !function->body_fact.valid())
		throw std::runtime_error("PA12 function semantic fact is missing");
	const Binding& value = binding(function->binding);
	for (std::size_t indent = 0; indent < depth; ++indent)
		output << "  ";
	output << "function-definition " << qualified_binding_name(function->owner,
		value.name) << ' ' << render_binding_type(value) << '\n';
	if (type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA12 function binding has non-function type");
	const TypeKey& function_type = types_[value.type.value];
	const Scope& function_scope = scopes_[function->function_scope.value];
	std::size_t parameter_index = 0;
	for (std::size_t i = 0; i < function_scope.bindings.size(); ++i)
	{
		const Binding& parameter = binding(function_scope.bindings[i]);
		if (parameter.kind != BindingKind::Parameter)
			continue;
		for (std::size_t indent = 0; indent < depth + 1; ++indent)
			output << "  ";
		output << "parameter ";
		if (parameter.name.valid())
			output << name_text(parameter.name);
		output << ' ' << render_type(parameter_index < function_type.parameters.size() ?
			function_type.parameters[parameter_index] : parameter.type) << '\n';
		++parameter_index;
	}
	dump_pa12_fact(output, function->body_fact, depth + 1);
}
void PA11SemanticModel::dump_pa12_top_node(std::ostream& output, const PA10AstNode& node,
	ScopeId scope, std::size_t depth) const
{
	switch (node.kind)
	{
	case PA10NodeKind::NamespaceDefinition:
	{
		const NamespaceFact* namespace_fact = this->namespace_fact(node);
		const ScopeId namespace_scope = namespace_fact == NULL ?
			ScopeId() : namespace_fact->scope;
		if (!namespace_scope.valid())
			throw std::runtime_error("PA12 namespace dump fact is missing");
		for (std::size_t indent = 0; indent < depth; ++indent)
			output << "  ";
		output << "namespace-definition " << ast_.producer_spelling(
			node.producer_spelling) << '\n';
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind != PA10NodeKind::InlineMarker)
				dump_pa12_top_node(output, node.children[i], namespace_scope,
					depth + 1);
		return;
	}
	case PA10NodeKind::LinkageSpecification:
		for (std::size_t i = 0; i < node.children.size(); ++i)
			dump_pa12_top_node(output, node.children[i], scope, depth);
		return;
	case PA10NodeKind::SimpleDeclaration:
	{
		const DeclarationFact* declaration = declaration_fact(node);
		if (declaration == NULL)
			return;
		for (std::size_t i = 0; i < declaration->semantic_count; ++i)
			dump_pa12_fact(output, declaration_semantic_ids_[
				declaration->semantic_begin + i], depth);
		return;
	}
	case PA10NodeKind::AliasDeclaration:
	{
		const DeclarationFact* declaration = declaration_fact(node);
		if (declaration == NULL || declaration->semantic_count != 1 ||
			declaration->semantic_begin == InvalidIdentityValue)
			throw std::runtime_error("PA12 alias semantic fact is missing");
		dump_pa12_fact(output, declaration_semantic_ids_[
			declaration->semantic_begin], depth);
		return;
	}
	case PA10NodeKind::FunctionDefinition:
		dump_pa12_function(output, node, depth);
		return;
	default:
		return;
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
