#include "pa11_semantic.h"
#include "pa11_semantic_model.h"
#include <algorithm>
#include <limits>
namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

namespace
{

bool has_virtual_member_specifier_impl(const PA10AstNode& node, bool root)
{
	// A nested declaration owns its own members.  Do not let a virtual
	// specifier in one of those declarations become a fact about the enclosing
	// record.  Function bodies and other executable/declarative scopes are
	// boundaries for the same reason.
	if (!root &&
		(node.kind == PA10NodeKind::ClassSpecifier ||
		 node.kind == PA10NodeKind::ClassForwardDeclaration ||
		 node.kind == PA10NodeKind::EnumSpecifier ||
		 node.kind == PA10NodeKind::CompoundStatement ||
		 node.kind == PA10NodeKind::LambdaExpression ||
		 node.kind == PA10NodeKind::FunctionDefinition ||
		 node.kind == PA10NodeKind::NamespaceDefinition ||
		 node.kind == PA10NodeKind::LinkageSpecification))
		return false;

	if ((node.kind == PA10NodeKind::MemberSpecifier && node.has_token &&
			node.token == SimpleTokenType::KW_VIRTUAL) ||
		(node.kind == PA10NodeKind::DeclSpecifier && node.has_token &&
			node.token == SimpleTokenType::KW_VIRTUAL) ||
		node.kind == PA10NodeKind::VirtualSpecifier ||
		node.kind == PA10NodeKind::VirtSpecifier)
		return true;
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (has_virtual_member_specifier_impl(node.children[i], false))
			return true;
	return false;
}

bool has_virtual_member_specifier(const PA10AstNode& node)
{
	// A class/enum declaration appearing as a member is itself a nested
	// declarative boundary, not a virtual member declaration of this record.
	if (node.kind == PA10NodeKind::ClassSpecifier ||
		node.kind == PA10NodeKind::ClassForwardDeclaration ||
		node.kind == PA10NodeKind::EnumSpecifier)
		return false;
	return has_virtual_member_specifier_impl(node, true);
}

}

std::size_t PA11SemanticModel::alignment_specifier_value(
	const PA10AlignmentSpecifier& spec, ScopeId scope)
{
	if (spec.argument_kind == PA10AlignmentArgumentKind::TypeId)
	{
		const TypeId type = type_from_type_id(spec.argument, scope);
		if (!complete_object_type(type))
			throw std::runtime_error("alignas type is not a complete object type");
		const std::size_t alignment = type_alignment(type);
		if (alignment == 0)
			throw std::runtime_error("alignas type has invalid alignment");
		return alignment;
	}
	if (spec.argument_kind != PA10AlignmentArgumentKind::Expression)
		throw std::runtime_error("invalid PA11 alignment argument kind");
	const ConstValue value = eval_constexpr(spec.argument, scope);
	if (!value.valid || !value.type.valid())
		throw std::runtime_error("alignas argument is not an integral constant");
	const TypeId value_type = strip_cv_type(value.type);
	bool integral = false;
	if (type_kind(value_type) == TypeKind::Fundamental)
		integral = integral_type(types_[value_type.value].fundamental);
	else if (type_kind(value_type) == TypeKind::Named)
	{
		const NamedRecordId record = named_record_for_type(value_type);
		integral = record.valid() && record.value < named_.size() &&
			named_[record.value].kind == NamedKind::Enum;
	}
	if (!integral || value.value < 0 ||
		value.value > static_cast<__int128>(
			std::numeric_limits<std::size_t>::max()))
		throw std::runtime_error("invalid alignas constant");
	const std::size_t alignment = static_cast<std::size_t>(value.value);
	if (alignment != 0 && (alignment & (alignment - 1)) != 0)
		throw std::runtime_error("alignas is not a valid alignment");
	return alignment;
}

std::size_t PA11SemanticModel::pack_alignment_at(
	std::size_t source_position) const
{
	const std::vector<PA10PackDirective>& directives = ast_.pack_directives;
	std::size_t first = 0;
	std::size_t last = directives.size();
	while (first < last)
	{
		const std::size_t middle = first + (last - first) / 2;
		if (directives[middle].token_index <= source_position)
			first = middle + 1;
		else
			last = middle;
	}
	if (first == 0)
		return 0;
	const PA10PackDirective& directive = directives[first - 1];
	if (directive.operation == PPPackOperation::Push)
	{
		if (directive.byte_cap == 0 || directive.active_byte_cap == 0 ||
			directive.active_byte_cap != directive.byte_cap)
			throw std::runtime_error("invalid PA11 pack push fact");
	}
	else if (directive.operation == PPPackOperation::Pop)
	{
		if (directive.byte_cap != 0)
			throw std::runtime_error("invalid PA11 pack pop fact");
	}
	else
		throw std::runtime_error("invalid PA11 pack operation fact");
	return directive.active_byte_cap;
}

std::size_t PA11SemanticModel::requested_alignment(const PA10AstNode& node,
	ScopeId scope, bool* has_specifier)
{
	if (node.alignment_specifier_begin > ast_.alignment_specifiers.size() ||
		node.alignment_specifier_count > ast_.alignment_specifiers.size() -
			node.alignment_specifier_begin)
		throw std::runtime_error("invalid PA11 alignment specifier range");
	if (has_specifier != NULL)
		*has_specifier = node.alignment_specifier_count != 0;
	std::size_t result = 0;
	for (std::size_t i = 0; i < node.alignment_specifier_count; ++i)
	{
		const std::size_t alignment = alignment_specifier_value(
			ast_.alignment_specifiers[node.alignment_specifier_begin + i], scope);
		if (alignment > result)
			result = alignment;
	}
	return result;
}

void PA11SemanticModel::apply_record_alignment(const PA10AstNode& node,
	NamedRecordId record_id, ScopeId scope, bool definition,
	const PA10AstNode* additional)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		throw std::runtime_error("alignment has no class owner");
	bool has_specifier = false;
	std::size_t alignment = requested_alignment(node, scope, &has_specifier);
	if (additional != NULL)
	{
		bool additional_has_specifier = false;
		const std::size_t additional_alignment = requested_alignment(
			*additional, scope, &additional_has_specifier);
		has_specifier = has_specifier || additional_has_specifier;
		if (additional_alignment > alignment)
			alignment = additional_alignment;
	}
	NamedRecord& record = named_[record_id.value];
	NamedRecordAlignmentFact alignment_fact;
	const NamedRecordAlignmentFact* existing =
		named_record_alignment_fact(record_id);
	if (existing != NULL)
		alignment_fact = *existing;
	if (!has_specifier)
	{
		if (!definition)
			return;
		if (alignment_fact.has(NamedRecordAlignmentFlag::HasDefinition) ||
			alignment_fact.has(NamedRecordAlignmentFlag::HasSpecifier))
			throw std::runtime_error(
				"class definition alignment does not match declaration");
		// An unaligned definition needs no cold sidecar entry.  The canonical
		// NamedRecord::defined fact rejects a later aligned non-defining
		// declaration below.
		return;
	}
	if (definition)
	{
		if (alignment_fact.has(NamedRecordAlignmentFlag::HasDefinition) ||
			(alignment_fact.has(
					NamedRecordAlignmentFlag::HasNondefiningSpecifier) &&
				(alignment_fact.has(
						NamedRecordAlignmentFlag::NondefiningConflict) ||
					alignment_fact.nondefining_alignment != alignment)))
			throw std::runtime_error(
				"class definition alignment does not match declaration");
	}
	else if (alignment_fact.has(NamedRecordAlignmentFlag::HasDefinition))
	{
		if (!alignment_fact.has(
				NamedRecordAlignmentFlag::DefinitionHasSpecifier) ||
			alignment_fact.definition_alignment != alignment)
			throw std::runtime_error(
				"class declaration alignment does not match definition");
	}
	else if (record.defined)
	{
		throw std::runtime_error(
			"class declaration alignment does not match definition");
	}
	else if (alignment_fact.has(
			NamedRecordAlignmentFlag::HasNondefiningSpecifier) &&
		alignment_fact.nondefining_alignment != alignment)
		alignment_fact.add(NamedRecordAlignmentFlag::NondefiningConflict);
	alignment_fact.add(NamedRecordAlignmentFlag::HasSpecifier);
	if (definition)
	{
		alignment_fact.add(NamedRecordAlignmentFlag::HasDefinition);
		alignment_fact.add(NamedRecordAlignmentFlag::DefinitionHasSpecifier);
		alignment_fact.definition_alignment = alignment;
	}
	else
	{
		if (!alignment_fact.has(
				NamedRecordAlignmentFlag::HasNondefiningSpecifier))
		{
			alignment_fact.add(
				NamedRecordAlignmentFlag::HasNondefiningSpecifier);
			alignment_fact.nondefining_alignment = alignment;
		}
	}
	set_named_record_alignment_fact(record_id, alignment_fact);
	if (alignment == 0)
		return;
	if (record.has_requested_alignment)
	{
		if (record.requested_alignment != alignment)
		{
			if (!definition && !alignment_fact.has(
					NamedRecordAlignmentFlag::HasDefinition))
				return;
			throw std::runtime_error("conflicting class alignment");
		}
		return;
	}
	if (record_id.value >= record_layouts_.size())
		throw std::runtime_error("alignment has no record layout owner");
	if (record_layouts_[record_id.value].state == RecordLayoutState::Complete)
		throw std::runtime_error("class alignment changed after layout completion");
	record.has_requested_alignment = true;
	record.requested_alignment = alignment;
}

void PA11SemanticModel::apply_member_alignment(const PA10AstNode& node,
	BindingId binding_id, ScopeId scope)
{
	const std::size_t alignment = requested_alignment(node, scope);
	if (alignment == 0)
		return;
	BindingSidecar sidecar;
	const BindingSidecar* existing = binding_sidecar(binding_id);
	if (existing != NULL)
		sidecar = *existing;
	if (!sidecar.has_requested_alignment ||
		sidecar.requested_alignment < alignment)
	{
		sidecar.has_requested_alignment = true;
		sidecar.requested_alignment = alignment;
		set_binding_sidecar(binding_id, sidecar);
	}
}

void PA11SemanticModel::record_mutable_member(BindingId id, TypeId type,
	bool record_member)
{
	if (!record_member)
		throw std::runtime_error("PA11 mutable declaration is not a non-static data member");
	if ((cv_qualifiers(type) & 1u) != 0)
		throw std::runtime_error("PA11 mutable member has const-qualified type");
	const TypeKind kind = type_kind(type);
	if (kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference)
		throw std::runtime_error("PA11 mutable member cannot be a reference");
	BindingSidecar sidecar;
	const BindingSidecar* existing = binding_sidecar(id);
	if (existing != NULL)
		sidecar = *existing;
	sidecar.mutable_member = true;
	set_binding_sidecar(id, sidecar);
}

void PA11SemanticModel::process_base_clause(const PA10AstNode& node,
	NamedRecordId record_id, ScopeId scope, ScopeId access_scope)
{
	if (!record_id.valid() || record_id.value >= named_.size() ||
		named_[record_id.value].kind != NamedKind::Class)
		throw std::runtime_error("base clause has no class owner");
	if (named_[record_id.value].class_tag == ClassTag::Union)
		throw std::runtime_error("union cannot have a base class");
	if (node.kind != PA10NodeKind::BaseClause || node.children.empty())
		throw std::runtime_error("empty PA11 base clause");
	if (node.children.size() != 1)
		throw std::runtime_error("multiple inheritance is outside PA16");
	const PA10AstNode& base_specifier = node.children.front();
	if (base_specifier.kind != PA10NodeKind::BaseSpecifier)
		throw std::runtime_error("invalid PA11 base specifier");
	const PA10AstNode* base_name = NULL;
	bool virtual_base = false;
	MemberAccess base_access = named_[record_id.value].class_tag ==
		ClassTag::Class ? MemberAccess::Private : MemberAccess::Public;
	bool access_seen = false;
	for (std::size_t i = 0; i < base_specifier.children.size(); ++i)
	{
		const PA10AstNode& child = base_specifier.children[i];
		if (child.kind == PA10NodeKind::VirtualSpecifier)
			virtual_base = true;
		else if (child.kind == PA10NodeKind::AccessSpecifier)
		{
			if (access_seen)
				throw std::runtime_error("base specifier has multiple access modes");
			access_seen = true;
			switch (child.token)
			{
			case SimpleTokenType::KW_PUBLIC: base_access = MemberAccess::Public; break;
			case SimpleTokenType::KW_PROTECTED: base_access = MemberAccess::Protected; break;
			case SimpleTokenType::KW_PRIVATE: base_access = MemberAccess::Private; break;
			default: throw std::runtime_error("invalid base access specifier");
			}
		}
		else if (child.kind == PA10NodeKind::BaseName)
		{
			if (base_name != NULL)
				throw std::runtime_error("base specifier has multiple names");
			base_name = &child;
		}
	}
	if (virtual_base)
		throw std::runtime_error("virtual inheritance is outside PA16");
	if (base_name == NULL)
		throw std::runtime_error("base specifier has no name");
	const TypeId base_type = lookup_type_path(name_path(*base_name, scope), scope,
		SourcePoint(node.source_begin), NULL, access_scope);
	if (!base_type.valid())
		throw std::runtime_error("unknown PA11 direct base type");
	const TypeId unqualified = strip_cv_type(base_type);
	if (type_kind(unqualified) != TypeKind::Named)
		throw std::runtime_error("direct base is not a class type");
	const NamedRecordId base_record = named_record_for_type(unqualified);
	if (!base_record.valid() || base_record.value >= named_.size() ||
		named_[base_record.value].kind != NamedKind::Class ||
		named_[base_record.value].class_tag == ClassTag::Union)
		throw std::runtime_error("direct base is not a complete class");
	if (base_record == record_id)
		throw std::runtime_error("class cannot directly derive from itself");
	if (!complete_object_type(unqualified))
		throw std::runtime_error("direct base is incomplete");
	NamedRecord& record = named_[record_id.value];
	if (record.has_base && record.direct_base != base_record)
		throw std::runtime_error("multiple inheritance is outside PA16");
	record.has_base = true;
	record.direct_base = base_record;
	record.direct_base_access = base_access;
	record.direct_base_virtual = false;
}

void PA11SemanticModel::process_class_body(const PA10AstNode& node, TypeId type,
	ScopeId owner, bool alignment_applied)
{
	const ScopeId class_scope = class_scope_for_type(type);
	if (!class_scope.valid())
		throw std::runtime_error("class has no class scope");
	const NamedRecordId record_id = named_record_for_type(type);
	if (!record_id.valid() || record_id.value >= named_.size())
		throw std::runtime_error("class has no named record");
	named_[record_id.value].pack_alignment =
		pack_alignment_at(node.source_begin);
	if (!alignment_applied)
		apply_record_alignment(node, record_id, owner, true);
	MemberAccess access = named_[record_id.value].class_tag == ClassTag::Class ?
		MemberAccess::Private : MemberAccess::Public;
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode& child = node.children[i];
		const PA10NodeKind kind = child.kind;
		if (kind == PA10NodeKind::ClassKey || kind == PA10NodeKind::BaseClause)
		{
			if (kind == PA10NodeKind::BaseClause)
				process_base_clause(child, record_id, owner, class_scope);
			continue;
		}
		if (kind == PA10NodeKind::AccessSpecifier)
		{
			switch (child.token)
			{
			case SimpleTokenType::KW_PUBLIC: access = MemberAccess::Public; break;
			case SimpleTokenType::KW_PROTECTED: access = MemberAccess::Protected; break;
			case SimpleTokenType::KW_PRIVATE: access = MemberAccess::Private; break;
			default: throw std::runtime_error("invalid class access specifier");
			}
			continue;
		}
		if (has_virtual_member_specifier(child))
			named_[record_id.value].has_virtual_member = true;
		if (kind == PA10NodeKind::EmptyDeclaration)
			continue;
		const std::size_t binding_begin = scopes_[class_scope.value].bindings.size();
		process_declaration(child, class_scope, access);
		Scope& current = scopes_[class_scope.value];
		for (std::size_t binding_index = binding_begin;
			binding_index < current.bindings.size(); ++binding_index)
			set_member_access(current.bindings[binding_index], access);
	}
	complete_record_layout(named_record_for_type(type));
	(void)owner;
}
static bool is_constant_comparison_token(SimpleTokenType token)
{
	switch (token)
	{
	case SimpleTokenType::OP_EQ:
	case SimpleTokenType::OP_NE:
	case SimpleTokenType::OP_LT:
	case SimpleTokenType::OP_LE:
	case SimpleTokenType::OP_GT:
	case SimpleTokenType::OP_GE:
		return true;
	default:
		return false;
	}
}
TypeId PA11SemanticModel::conditional_common_type(TypeId when_true,
	TypeId when_false) const
{
	when_true = strip_cv_type(expression_object_type(when_true));
	when_false = strip_cv_type(expression_object_type(when_false));
	if (!when_true.valid() || !when_false.valid())
		return TypeId();
	if (when_true == when_false)
		return when_true;
	if (integral_id(when_true) && integral_id(when_false))
		return common_integral_type(when_true, when_false);
	return TypeId();
}
TypeId PA11SemanticModel::constant_expression_type(
	const PA10AstNode& node, ScopeId scope,
	bool allow_scoped_enum_integral_comparison)
{
	if (node.kind == PA10NodeKind::Literal)
		return fundamental(node.literal.type);
	if (node.kind == PA10NodeKind::KeywordLiteral)
	{
		if (node.token == SimpleTokenType::KW_TRUE ||
			node.token == SimpleTokenType::KW_FALSE)
			return fundamental(FundamentalType::Bool);
		return TypeId();
	}
	if (node.kind == PA10NodeKind::ParenthesizedExpression)
	{
		if (node.children.size() != 1)
			return TypeId();
		return constant_expression_type(node.children.front(), scope,
			allow_scoped_enum_integral_comparison);
	}
	if (node.kind == PA10NodeKind::IdExpression)
	{
		const std::vector<ValueRef> values = lookup_value_path(
			name_path(node), scope);
		if (!values.empty())
			return binding(values.front().binding).type;
		return TypeId();
	}
	if (node.kind == PA10NodeKind::CastExpression)
	{
		if (node.children.size() < 2)
			return TypeId();
		return type_from_type_id(node.children.front(), scope);
	}
	if (node.kind == PA10NodeKind::SizeofExpression)
		return fundamental(FundamentalType::UnsignedLongInt);
	if (node.kind == PA10NodeKind::TypeTraitExpression)
		return fundamental(FundamentalType::LongLongInt);
	if (node.kind == PA10NodeKind::UnaryExpression)
	{
		if (node.children.size() != 1)
			return TypeId();
		const TypeId operand = constant_expression_type(
			node.children.front(), scope, allow_scoped_enum_integral_comparison);
		if (node.token == SimpleTokenType::OP_LNOT)
			return fundamental(FundamentalType::Bool);
		if (node.token == SimpleTokenType::OP_PLUS ||
			node.token == SimpleTokenType::OP_MINUS ||
			node.token == SimpleTokenType::OP_COMPL)
			return promote_integral_type(operand);
		return TypeId();
	}
	if (node.kind == PA10NodeKind::BinaryExpression ||
		node.kind == PA10NodeKind::AssignmentExpression)
	{
		if (node.children.size() != 2)
			return TypeId();
		const TypeId left = constant_expression_type(node.children[0], scope,
			allow_scoped_enum_integral_comparison);
		const TypeId right = constant_expression_type(node.children[1], scope,
			allow_scoped_enum_integral_comparison);
		const NamedRecordId left_record = named_record_for_type(left);
		const bool same_scoped_enum = left == right && left_record.valid() &&
			left_record.value < named_.size() &&
			named_[left_record.value].kind == NamedKind::Enum &&
			named_[left_record.value].scoped_enum;
		if (node.token == SimpleTokenType::OP_COMMA)
			return right;
		if (node.token == SimpleTokenType::OP_LAND ||
			node.token == SimpleTokenType::OP_LOR ||
			is_constant_comparison_token(node.token))
			return fundamental(FundamentalType::Bool);
		if (node.token == SimpleTokenType::OP_LSHIFT ||
			node.token == SimpleTokenType::OP_RSHIFT)
			return integral_id(left) && integral_id(right) ?
				promote_integral_type(left) : TypeId();
		if (same_scoped_enum || (integral_id(left) && integral_id(right)))
			return common_integral_type(left, right);
		return TypeId();
	}
	if (node.kind == PA10NodeKind::ConditionalExpression &&
		node.children.size() == 3)
	{
		const TypeId when_true = constant_expression_type(node.children[1],
			scope, allow_scoped_enum_integral_comparison);
		const TypeId when_false = constant_expression_type(node.children[2],
			scope, allow_scoped_enum_integral_comparison);
		return conditional_common_type(when_true, when_false);
	}
	return TypeId();
}
ConstValue PA11SemanticModel::constant_value_as_type(
	const ConstValue& value, TypeId type) const
{
	if (!value.valid)
		return value;
	const TypeId object_type = strip_cv_type(expression_object_type(type));
	type = object_type;
	const NamedRecordId record = named_record_for_type(type);
	if (record.valid() && record.value < named_.size() &&
		named_[record.value].kind == NamedKind::Enum)
	{
		if (!named_[record.value].has_underlying)
			type = fundamental(FundamentalType::Int);
		else
			type = strip_cv_type(named_[record.value].underlying);
	}
	FundamentalType fundamental_type;
	if (!fundamental_of(type, &fundamental_type) ||
		!integral_type(fundamental_type))
		throw NonConstantExpression("constant expression is not integral");
	const std::size_t width = type_size(type) * 8;
	if (width == 0 || width > 64)
		throw NonConstantExpression("constant expression type is too wide");
	if (fundamental_type == FundamentalType::Bool)
		return ConstValue(true, value.value == 0 ? 0 : 1, false, object_type);
	const bool is_unsigned = unsigned_type(fundamental_type);
	if (is_unsigned)
	{
		const __int128 modulus = static_cast<__int128>(1) << width;
		__int128 normalized = value.value % modulus;
		if (normalized < 0) normalized += modulus;
		return ConstValue(true, normalized, true, object_type);
	}
	const __int128 modulus = static_cast<__int128>(1) << width;
	const __int128 minimum = -(modulus >> 1);
	const __int128 maximum = (modulus >> 1) - 1;
	if (value.value < minimum || value.value > maximum)
		throw NonConstantExpression("constant expression overflow");
	return ConstValue(true, value.value, false, object_type);
}
ConstValue PA11SemanticModel::eval_constexpr_unary(const PA10AstNode& node,
	ScopeId scope, bool allow_scoped_enum_integral_comparison)
{
	if (node.children.size() != 1)
		throw std::runtime_error("invalid unary constant expression");
	ConstValue value = eval_constexpr(node.children.front(), scope,
		allow_scoped_enum_integral_comparison);
	const TypeId operation_type = node.token == SimpleTokenType::OP_LNOT ?
		fundamental(FundamentalType::Bool) : promote_integral_type(value.type);
	if (node.token != SimpleTokenType::OP_LNOT && !operation_type.valid())
		throw NonConstantExpression("unsupported unary constant operator");
	if (node.token != SimpleTokenType::OP_LNOT)
		value = constant_value_as_type(value, operation_type);
	switch (node.token)
	{
	case SimpleTokenType::OP_PLUS:
		break;
	case SimpleTokenType::OP_MINUS:
		value.value = -value.value;
		break;
	case SimpleTokenType::OP_LNOT:
		value.value = value.value == 0 ? 1 : 0;
		value.is_unsigned = false;
		break;
	case SimpleTokenType::OP_COMPL:
		value.value = ~value.value;
		break;
	default:
		throw NonConstantExpression("unsupported unary constant operator");
	}
	if (node.token == SimpleTokenType::OP_LNOT)
		return constant_value_as_type(value, fundamental(FundamentalType::Bool));
	return constant_value_as_type(value, operation_type);
}
ConstValue PA11SemanticModel::eval_constexpr_binary(const PA10AstNode& node,
	ScopeId scope, bool allow_scoped_enum_integral_comparison)
{
	if (node.children.size() != 2)
		throw std::runtime_error("invalid binary constant expression");
	const ConstValue left_value = eval_constexpr(node.children[0], scope,
		allow_scoped_enum_integral_comparison);
	if (node.token == SimpleTokenType::OP_LAND && left_value.value == 0)
		return ConstValue(true, 0, false, fundamental(FundamentalType::Bool));
	if (node.token == SimpleTokenType::OP_LOR && left_value.value != 0)
		return ConstValue(true, 1, false, fundamental(FundamentalType::Bool));
	const ConstValue right_value = eval_constexpr(node.children[1], scope,
		allow_scoped_enum_integral_comparison);
	const TypeId left_type = left_value.type;
	const TypeId right_type = right_value.type;
	const bool comparison = node.token == SimpleTokenType::OP_EQ ||
		node.token == SimpleTokenType::OP_NE ||
		node.token == SimpleTokenType::OP_LT ||
		node.token == SimpleTokenType::OP_LE ||
		node.token == SimpleTokenType::OP_GT ||
		node.token == SimpleTokenType::OP_GE;
	const bool logical = node.token == SimpleTokenType::OP_LAND ||
		node.token == SimpleTokenType::OP_LOR;
	TypeId operation_type;
	if (comparison)
	{
		const NamedRecordId record = named_record_for_type(left_type);
		const NamedRecordId right_record = named_record_for_type(right_type);
		const bool same_scoped_enum = left_type == right_type && record.valid() &&
			record.value < named_.size() &&
			named_[record.value].kind == NamedKind::Enum &&
			named_[record.value].scoped_enum;
		operation_type = same_scoped_enum ? left_type :
			(integral_id(left_type) && integral_id(right_type) ?
			common_integral_type(left_type, right_type) : TypeId());
		if (!operation_type.valid() && allow_scoped_enum_integral_comparison)
		{
			const bool left_scoped = record.valid() && record.value < named_.size() &&
				named_[record.value].kind == NamedKind::Enum &&
				named_[record.value].scoped_enum;
			const bool right_scoped = right_record.valid() &&
				right_record.value < named_.size() &&
				named_[right_record.value].kind == NamedKind::Enum &&
				right_record.value < named_.size() &&
				named_[right_record.value].scoped_enum;
			if (left_scoped && integral_id(right_type))
			{
				const TypeId underlying = named_[record.value].has_underlying ?
					named_[record.value].underlying :
					fundamental(FundamentalType::Int);
				operation_type = common_integral_type(underlying, right_type);
			}
			else if (right_scoped && integral_id(left_type))
			{
				const TypeId underlying = named_[right_record.value].has_underlying ?
					named_[right_record.value].underlying :
					fundamental(FundamentalType::Int);
				operation_type = common_integral_type(left_type, underlying);
			}
		}
	}
	else if (node.token == SimpleTokenType::OP_LSHIFT ||
		node.token == SimpleTokenType::OP_RSHIFT)
	{
		if (integral_id(left_type) && integral_id(right_type))
			operation_type = promote_integral_type(left_type);
	}
	else if (!logical && node.token != SimpleTokenType::OP_COMMA)
	{
		const NamedRecordId record = named_record_for_type(left_type);
		const bool same_scoped_enum = left_type == right_type && record.valid() &&
			record.value < named_.size() &&
			named_[record.value].kind == NamedKind::Enum &&
			named_[record.value].scoped_enum;
		if (same_scoped_enum || (integral_id(left_type) && integral_id(right_type)))
			operation_type = common_integral_type(left_type, right_type);
	}
	if (!logical && node.token != SimpleTokenType::OP_COMMA &&
		!operation_type.valid())
		throw NonConstantExpression("invalid integral constant expression");
	ConstValue left = left_value;
	ConstValue right = right_value;
	if (operation_type.valid())
	{
		left = constant_value_as_type(left, operation_type);
		const TypeId right_operation_type =
			node.token == SimpleTokenType::OP_LSHIFT ||
			node.token == SimpleTokenType::OP_RSHIFT ?
			promote_integral_type(right_type) : operation_type;
		right = constant_value_as_type(right, right_operation_type);
	}
	ConstValue result(true, 0, operation_type.valid() &&
		unsigned_integral_type(operation_type));
	switch (node.token)
	{
	case SimpleTokenType::OP_PLUS: result.value = left.value + right.value; break;
	case SimpleTokenType::OP_MINUS: result.value = left.value - right.value; break;
	case SimpleTokenType::OP_STAR:
		result.value = result.is_unsigned ?
			static_cast<__int128>(static_cast<unsigned __int128>(left.value) *
				static_cast<unsigned __int128>(right.value)) :
			left.value * right.value;
		break;
	case SimpleTokenType::OP_DIV:
		if (right.value == 0) throw NonConstantExpression("constant division by zero");
		if (!result.is_unsigned && left.value ==
			static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) &&
			right.value == -1)
			throw NonConstantExpression("constant signed division overflow");
		result.value = left.value / right.value; break;
	case SimpleTokenType::OP_MOD:
		if (right.value == 0) throw NonConstantExpression("constant modulo by zero");
		if (!result.is_unsigned && left.value ==
			static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) &&
			right.value == -1)
			throw NonConstantExpression("constant signed modulo overflow");
		result.value = left.value % right.value; break;
	case SimpleTokenType::OP_LSHIFT:
	case SimpleTokenType::OP_RSHIFT:
	{
		const std::size_t width = type_size(operation_type) * 8;
		if (width == 0 || width > 64 || right.value < 0 ||
			right.value >= static_cast<__int128>(width))
			throw NonConstantExpression("constant shift count out of range");
		const unsigned int shift = static_cast<unsigned int>(right.value);
		if (node.token == SimpleTokenType::OP_LSHIFT)
		{
			if (!left.is_unsigned && left.value < 0)
				throw NonConstantExpression("constant signed shift overflow");
			result.value = static_cast<__int128>(
				static_cast<unsigned __int128>(left.value) << shift);
		}
		else
			result.value = left.is_unsigned ?
				static_cast<__int128>(static_cast<unsigned __int128>(left.value) >> shift) :
				left.value >> shift;
		break;
	}
	case SimpleTokenType::OP_BOR: result.value = left.value | right.value; break;
	case SimpleTokenType::OP_XOR: result.value = left.value ^ right.value; break;
	case SimpleTokenType::OP_AMP: result.value = left.value & right.value; break;
	case SimpleTokenType::OP_EQ: result.value = left.value == right.value; result.is_unsigned = false; break;
	case SimpleTokenType::OP_NE: result.value = left.value != right.value; result.is_unsigned = false; break;
	case SimpleTokenType::OP_LT: result.value = left.value < right.value; result.is_unsigned = false; break;
	case SimpleTokenType::OP_LE: result.value = left.value <= right.value; result.is_unsigned = false; break;
	case SimpleTokenType::OP_GT: result.value = left.value > right.value; result.is_unsigned = false; break;
	case SimpleTokenType::OP_GE: result.value = left.value >= right.value; result.is_unsigned = false; break;
	case SimpleTokenType::OP_LAND: result.value = (left.value != 0 && right.value != 0); result.is_unsigned = false; break;
	case SimpleTokenType::OP_LOR: result.value = (left.value != 0 || right.value != 0); result.is_unsigned = false; break;
	case SimpleTokenType::OP_COMMA: result = right; break;
	default: throw NonConstantExpression("unsupported binary constant operator");
	}
	if (comparison || logical)
		return constant_value_as_type(result, fundamental(FundamentalType::Bool));
	if (node.token == SimpleTokenType::OP_COMMA)
		return right;
	return constant_value_as_type(result, operation_type);
}
ConstValue PA11SemanticModel::eval_constexpr_conditional(
	const PA10AstNode& node, ScopeId scope,
	bool allow_scoped_enum_integral_comparison)
{
	if (node.children.size() != 3)
		throw std::runtime_error("invalid conditional constant expression");
	const ConstValue condition = eval_constexpr(node.children[0], scope,
		allow_scoped_enum_integral_comparison);
	const std::size_t selected = condition.value != 0 ? 1 : 2;
	const ConstValue value = eval_constexpr(node.children[selected], scope,
		allow_scoped_enum_integral_comparison);
	const std::size_t unselected = selected == 1 ? 2 : 1;
	const TypeId unselected_type = constant_expression_type(
		node.children[unselected], scope,
		allow_scoped_enum_integral_comparison);
	const TypeId common_type = conditional_common_type(value.type,
		unselected_type);
	if (!common_type.valid())
		throw NonConstantExpression("invalid conditional constant expression");
	return constant_value_as_type(value, common_type);
}
ConstValue PA11SemanticModel::eval_constexpr(const PA10AstNode& node,
	ScopeId scope, bool allow_scoped_enum_integral_comparison)
{
	if (node.kind == PA10NodeKind::Literal)
	{
		const ConstValue value = literal_constant(node);
		return constant_value_as_type(value, fundamental(node.literal.type));
	}
	if (node.kind == PA10NodeKind::KeywordLiteral)
	{
		const ConstValue value(true, node.has_token &&
			node.token == SimpleTokenType::KW_TRUE ? 1 : 0, false);
		return constant_value_as_type(value, fundamental(FundamentalType::Bool));
	}
	if (node.kind == PA10NodeKind::ParenthesizedExpression)
	{
		if (node.children.empty())
			throw std::runtime_error("empty constant expression");
		return eval_constexpr(node.children.front(), scope,
			allow_scoped_enum_integral_comparison);
	}
	if (node.kind == PA10NodeKind::IdExpression)
	{
		const std::vector<ValueRef> values = lookup_value_path(name_path(node), scope);
		if (values.empty())
			throw std::runtime_error("constant name is not a value");
		const Binding& value_binding = binding(values.front().binding);
		if (!value_binding.has_value)
			throw NonConstantExpression("value is not a constant");
		const bool value_unsigned = value_binding.value_unsigned ||
			unsigned_integral_type(value_binding.type);
		const __int128 value = value_unsigned ?
			static_cast<__int128>(value_binding.value_bits) :
			static_cast<__int128>(value_binding.value);
		const NamedRecordId record = named_record_for_type(value_binding.type);
		const bool enum_value = record.valid() && record.value < named_.size() &&
			named_[record.value].kind == NamedKind::Enum;
		const bool enum_underlying = enum_value && named_[record.value].has_underlying;
		const TypeId value_type = strip_cv_type(expression_object_type(value_binding.type));
		const ConstValue result(true, value, value_unsigned, value_type);
		if (enum_underlying || !enum_value)
			return constant_value_as_type(result, value_binding.type);
		return result;
	}
	if (node.kind == PA10NodeKind::UnaryExpression)
		return eval_constexpr_unary(node, scope,
			allow_scoped_enum_integral_comparison);
	if (node.kind == PA10NodeKind::BinaryExpression ||
		node.kind == PA10NodeKind::AssignmentExpression)
		return eval_constexpr_binary(node, scope,
			allow_scoped_enum_integral_comparison);
	if (node.kind == PA10NodeKind::ConditionalExpression)
		return eval_constexpr_conditional(node, scope,
			allow_scoped_enum_integral_comparison);
	if (node.kind == PA10NodeKind::CastExpression)
	{
		if (node.children.size() < 2)
			throw std::runtime_error("invalid cast constant expression");
		const TypeId target = type_from_type_id(node.children.front(), scope);
		return constant_value_as_type(eval_constexpr(node.children.back(), scope,
			allow_scoped_enum_integral_comparison), target);
	}
	if (node.kind == PA10NodeKind::SizeofExpression ||
		node.kind == PA10NodeKind::TypeTraitExpression)
	{
		const TypeId operand = sizeof_operand_type(node, scope);
		const std::size_t value = node.kind == PA10NodeKind::TypeTraitExpression &&
			node.has_token && node.token == SimpleTokenType::KW_ALIGNOF ?
			type_alignment(operand) : type_size(operand);
		return ConstValue(true, static_cast<__int128>(value), false,
			fundamental(node.kind == PA10NodeKind::SizeofExpression ?
				FundamentalType::UnsignedLongInt : FundamentalType::LongLongInt));
	}
	throw NonConstantExpression("unsupported constant expression");
}
TypeId PA11SemanticModel::process_enum_specifier(const PA10AstNode& node, ScopeId scope,
	NamedRecordId* anonymous_record)
{
	const bool scoped = enum_is_scoped(node);
	const NamePath name = enum_name(node);
	const bool definition = !node.children.empty() &&
		child_of_kind(node, PA10NodeKind::Enumerator) != NULL;
	bool has_underlying = false;
	TypeId underlying = fundamental(FundamentalType::Int);
	FundamentalType underlying_fundamental = FundamentalType::Int;
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (node.children[i].kind == PA10NodeKind::TypeId)
		{
			underlying = type_from_type_id(node.children[i], scope);
			has_underlying = true;
		}
	if (has_underlying &&
		(!fundamental_of(underlying, &underlying_fundamental) ||
			!integral_type(underlying_fundamental)))
		throw std::runtime_error("enum underlying type is not integral");
	if (name.empty() && !scoped && !definition)
		throw std::runtime_error("opaque unscoped enum");
	if (!scoped && !definition && !has_underlying && !name.empty())
	{
		const TypeId existing = lookup_type_path(name, scope);
		const NamedRecordId record = named_record_for_type(existing);
		if (!record.valid() || record.value >= named_.size() ||
			named_[record.value].kind != NamedKind::Enum)
			throw std::runtime_error("undeclared elaborated enum");
		return existing;
	}
	ScopeId owner = scope;
	if (!name.empty())
	{
		owner = declaration_scope(name, scope);
		if (!owner.valid())
			throw std::runtime_error("unresolved enum declaration scope");
	}
	TypeId type;
	if (name.empty())
	{
		type = create_anonymous_enum(owner, scoped, has_underlying,
			underlying, definition);
		if (anonymous_record != NULL)
			*anonymous_record = named_record_for_type(type);
	}
	else
	{
		type = ensure_named_enum(owner, name.last(), scoped, has_underlying,
			underlying, definition);
		add_type_binding(owner, name.last(), type, ClassTag::Struct, false,
			SourcePoint(node.source_begin));
		if (definition && name.components.size() > 1)
			add_qualified_enum_view(scope, named_record_for_type(type), name);
	}
	ScopeId value_scope = owner;
	const NamedRecordId record_id = named_record_for_type(type);
	if (record_id.valid() && named_[record_id.value].scope.valid())
		value_scope = named_[record_id.value].scope;
	__int128 next_value = 0;
	bool have_next = false;
	bool have_value_range = false;
	__int128 minimum_value = 0;
	__int128 maximum_value = 0;
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode& child = node.children[i];
		if (child.kind != PA10NodeKind::Enumerator)
			continue;
		ConstValue value;
		if (!child.children.empty())
			value = eval_constexpr(child.children.front(), value_scope);
		if (!value.valid)
		{
			value = ConstValue(true, have_next ?
				next_value : 0, have_next && next_value >
				static_cast<__int128>(std::numeric_limits<std::int64_t>::max()));
		}
		if (value.value < static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) ||
			value.value > static_cast<__int128>(std::numeric_limits<std::uint64_t>::max()))
			throw std::runtime_error("enumerator value overflow");
		if (scoped && !has_underlying &&
			(value.value < static_cast<__int128>(std::numeric_limits<int>::min()) ||
				value.value > static_cast<__int128>(std::numeric_limits<int>::max())))
			throw std::runtime_error("scoped enum value is not representable by int");
		if (has_underlying)
		{
			// This is the enum declaration rule, not a general integral
			// conversion.  In particular, a bool conversion elsewhere may
			// normalize any nonzero value, but a fixed bool base still accepts
			// only the values 0 and 1 here.
			const std::size_t width = type_size(underlying) * 8;
			const __int128 modulus = static_cast<__int128>(1) << width;
			const bool underlying_unsigned =
				underlying_fundamental == FundamentalType::Bool ||
				unsigned_type(underlying_fundamental);
			const __int128 minimum = underlying_fundamental ==
				FundamentalType::Bool ? 0 :
				(underlying_unsigned ? 0 : -(modulus >> 1));
			const __int128 maximum = underlying_fundamental ==
				FundamentalType::Bool ? 1 :
				(underlying_unsigned ? modulus - 1 : (modulus >> 1) - 1);
			if (value.value < minimum || value.value > maximum)
				throw std::runtime_error("enumerator value is outside underlying type");
		}
		if (!have_value_range)
		{
			minimum_value = value.value;
			maximum_value = value.value;
			have_value_range = true;
		}
		else
		{
			if (value.value < minimum_value) minimum_value = value.value;
			if (value.value > maximum_value) maximum_value = value.value;
		}
		add_enumerator(value_scope, name_from_spelling(child.producer_spelling),
			type, value.value, value.is_unsigned,
			SourcePoint(node.source_begin));
		next_value = value.value + 1;
		have_next = true;
	}
	if (definition && record_id.valid() && record_id.value < named_.size() &&
		!has_underlying)
	{
		// Publish the selected representation once.  All later consumers use
		// this canonical enum fact for promotions, storage, and LowIR types.
		TypeId selected = fundamental(FundamentalType::Int);
		if (scoped)
		{
			if (have_value_range &&
				(minimum_value < static_cast<__int128>(std::numeric_limits<int>::min()) ||
					maximum_value > static_cast<__int128>(std::numeric_limits<int>::max())))
				throw std::runtime_error("scoped enum value is not representable by int");
			selected = fundamental(FundamentalType::Int);
		}
		else if (have_value_range &&
			minimum_value >= static_cast<__int128>(std::numeric_limits<int>::min()) &&
			maximum_value <= static_cast<__int128>(std::numeric_limits<int>::max()))
			selected = fundamental(FundamentalType::Int);
		else if (have_value_range && minimum_value >= 0 &&
			maximum_value <= static_cast<__int128>(std::numeric_limits<unsigned int>::max()))
			selected = fundamental(FundamentalType::UnsignedInt);
		else if (have_value_range &&
			minimum_value >= static_cast<__int128>(std::numeric_limits<long>::min()) &&
			maximum_value <= static_cast<__int128>(std::numeric_limits<long>::max()))
			selected = fundamental(FundamentalType::LongInt);
		else if (have_value_range && minimum_value >= 0 &&
			maximum_value <= static_cast<__int128>(std::numeric_limits<unsigned long>::max()))
			selected = fundamental(FundamentalType::UnsignedLongInt);
		else
			throw std::runtime_error("enum value has no supported underlying type");
		named_[record_id.value].has_underlying = true;
		named_[record_id.value].underlying = selected;
	}
	return type;
}
void PA11SemanticModel::add_enumerator(ScopeId scope, NameId name, TypeId type,
	__int128 value, bool value_unsigned, SourcePoint declaration_point)
{
	Scope& current = scopes_[scope.value];
	if (current.types.find(name) != NULL ||
		direct_namespace_exists(scope, name) || direct_value_exists(scope, name))
		throw std::runtime_error("enumerator conflicts with binding");
	Binding enumerator(BindingKind::Enumerator, name, type);
	enumerator.has_value = true;
	enumerator.value = static_cast<std::int64_t>(value);
	enumerator.value_bits = static_cast<std::uint64_t>(value);
	enumerator.value_unsigned = value_unsigned;
	const BindingId index = store_binding(scope, enumerator);
	append_value_index(scope, name, index, ScopeId(), declaration_point);
}
void PA11SemanticModel::dump(std::ostream& output) const
{
	output << "translation-unit\n";
	dump_scope(output, global_, 1);
}
std::string PA11SemanticModel::render_name_path(const NamePath& path) const
{
	std::ostringstream result;
	if (path.global)
		result << "::";
	for (std::size_t i = 0; i < path.components.size(); ++i)
	{
		if (i != 0)
			result << "::";
		result << name_text(path.components[i]);
	}
	return result.str();
}
std::string PA11SemanticModel::render_generated_name(const GeneratedIdentity& generated) const
{
	std::ostringstream result;
	switch (generated.kind)
	{
	case GeneratedEntityKind::AnonymousUnion:
		result << "__anonymous_union_type__" << generated.source.begin.value << '_'
			<< generated.source.end.value;
		break;
	case GeneratedEntityKind::AnonymousEnum:
		result << "__anonymous_enum" << (generated.ordinal.value + 1);
		break;
	}
	return result.str();
}
std::string PA11SemanticModel::render_record_name(NamedRecordId record_id) const
{
	if (!record_id.valid() || record_id.value >= named_.size())
		throw std::runtime_error("invalid PA11 named record");
	const NamedRecord& record = named_[record_id.value];
	const NamedRecordSidecar* sidecar = named_record_sidecar(record_id);
	if (pa12_render_mode_ && sidecar != NULL && sidecar->local_object_name &&
		record.has_generated_identity)
	{
		std::ostringstream result;
		result << "__local_type" << (record.generated_identity.ordinal.value + 1);
		return result.str();
	}
	if (record.name.valid())
		return name_text(record.name);
	if (record.has_generated_identity)
		return render_generated_name(record.generated_identity);
	return "<anonymous>";
}
std::string PA11SemanticModel::render_named_record(NamedRecordId record_id,
	ClassTag override_tag, bool use_override,
	const NamePath* display_path ) const
{
	if (!record_id.valid() || record_id.value >= named_.size())
		throw std::runtime_error("invalid PA11 named type");
	const NamedRecord& record = named_[record_id.value];
	std::ostringstream result;
	if (record.kind == NamedKind::Enum)
	{
		result << "enum";
		if (record.scoped_enum)
			result << " class";
	}
	else if (record.kind == NamedKind::TemplateParameter)
		result << (record.template_template ? "template-parameter" : "typename");
	else
	{
		const ClassTag tag = use_override ? override_tag : record.class_tag;
		result << (tag == ClassTag::Class ? "class" :
			tag == ClassTag::Union ? "union" : "struct");
	}
	result << ' ' << (display_path == NULL ? render_record_name(record_id) :
		render_name_path(*display_path));
	return result.str();
}
std::string PA11SemanticModel::render_named(TypeId type, ClassTag override_tag,
	bool use_override) const
{
	return render_named_record(named_record_for_type(type), override_tag,
		use_override);
}
std::string PA11SemanticModel::render_type(TypeId type) const
{
	struct Task
	{
		bool text;
		TypeId type;
		std::string value;
		Task(bool text, TypeId type, const char* value)
			: text(text), type(type), value(value == NULL ? "" : value)
		{}
	};
	std::string result;
	std::vector<Task> tasks;
	tasks.push_back(Task(false, type, NULL));
	const std::size_t limit = types_.size() + 1;
	std::size_t steps = 0;
	while (!tasks.empty())
	{
		if (++steps > limit * 8 + 32)
			throw std::runtime_error("PA11 type rendering cycle");
		const Task task = tasks.back();
		tasks.pop_back();
		if (task.text)
		{
			result += task.value;
			continue;
		}
		if (!task.type.valid() || task.type.value >= types_.size())
			throw std::runtime_error("invalid PA11 type for rendering");
		const TypeKey& key = types_[task.type.value];
		switch (key.kind)
		{
		case TypeKind::Fundamental:
			result += fundamental_type_name(key.fundamental);
			break;
		case TypeKind::Named:
		{
			const NamePath* display = pa12_render_mode_ ?
				type_display_path(task.type) : NULL;
			result += render_named_record(named_record_for_type(task.type),
				ClassTag::Struct, false, display);
			break;
		}
		case TypeKind::Cv:
			if ((key.cv & 1u) != 0)
				result += "const ";
			if ((key.cv & 2u) != 0)
				result += "volatile ";
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::Pointer:
			if ((key.cv & 1u) != 0)
				result += "const ";
			if ((key.cv & 2u) != 0)
				result += "volatile ";
			result += "pointer to ";
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::MemberPointer:
			if ((key.cv & 1u) != 0)
				result += "const ";
			if ((key.cv & 2u) != 0)
				result += "volatile ";
			result += "member-pointer of ";
			result += render_named_record(key.named, ClassTag::Struct, false);
			result += " to ";
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::LvalueReference:
			result += "lvalue-reference to ";
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::RvalueReference:
			result += "rvalue-reference to ";
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::Array:
			result += "array of ";
			if (key.unknown_bound)
				result += "unknown bound of ";
			else
			{
				std::ostringstream bound;
				bound << key.bound.value << ' ';
				result += bound.str();
			}
			tasks.push_back(Task(false, key.child, NULL));
			break;
		case TypeKind::Function:
			result += "function of (";
			tasks.push_back(Task(false, key.result, NULL));
			std::string function_suffix = ")";
			if ((key.cv & 1u) != 0)
				function_suffix += " const";
			if ((key.cv & 2u) != 0)
				function_suffix += " volatile";
			function_suffix += " returning ";
			tasks.push_back(Task(true, TypeId(), function_suffix.c_str()));
			if (key.variadic)
			{
				tasks.push_back(Task(true, TypeId(), "..."));
				if (!key.parameters.empty())
					tasks.push_back(Task(true, TypeId(), ", "));
			}
			for (std::size_t i = key.parameters.size(); i != 0; --i)
			{
				tasks.push_back(Task(false, key.parameters[i - 1], NULL));
				if (i > 1)
					tasks.push_back(Task(true, TypeId(), ", "));
			}
			break;
		}
	}
	return result;
}
std::string PA11SemanticModel::render_member_object_parameter(
	TypeId function_type, ScopeId member_scope) const
{
	if (!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class ||
		type_kind(function_type) != TypeKind::Function)
		return render_type(function_type);
	const NamedRecordId owner = scopes_[member_scope.value].record;
	if (!owner.valid())
		throw std::runtime_error("PA12 member function has no object owner");
	const unsigned int qualifiers = types_[function_type.value].cv;
	std::string result = "pointer to ";
	if ((qualifiers & 1u) != 0)
		result += "const ";
	if ((qualifiers & 2u) != 0)
		result += "volatile ";
	result += render_named_record(owner, ClassTag::Struct, false);
	return result;
}
std::string PA11SemanticModel::render_member_function_type(
	TypeId function_type, ScopeId member_scope, BindingId binding_id) const
{
	if (!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class ||
		type_kind(function_type) != TypeKind::Function ||
		is_static_member(binding_id))
		return render_type(function_type);
	const TypeKey& function = types_[function_type.value];
	std::string result = "function of (";
	result += render_member_object_parameter(function_type, member_scope);
	for (std::size_t i = 0; i < function.parameters.size(); ++i)
	{
		result += ", ";
		result += render_type(function.parameters[i]);
	}
	if (function.variadic)
	{
		if (!function.parameters.empty())
			result += ", ";
		result += "...";
	}
	result += ") returning ";
	result += render_type(function.result);
	return result;
}
std::string PA11SemanticModel::render_binding_type(const Binding& binding) const
{
	if (binding.has_tag && type_kind(binding.type) == TypeKind::Named)
		return render_named(binding.type, binding.class_tag, true);
	return render_type(binding.type);
}
std::string PA11SemanticModel::render_template_specialization(
	TemplateSpecializationId id) const
{
	if (!id.valid() || id.value >= template_specialization_facts_.size())
		throw std::runtime_error("invalid PA12 template specialization");
	const TemplateSpecializationFact& specialization =
		template_specialization_facts_[id.value];
	if (specialization.state != TemplateSpecializationState::Complete)
		throw std::runtime_error("incomplete PA12 template specialization");
	std::ostringstream result;
	result << '<';
	for (std::size_t i = 0; i < specialization.arguments.size(); ++i)
	{
		if (i != 0)
			result << ", ";
		result << render_template_argument_type(specialization.arguments[i]);
	}
	result << '>';
	return result.str();
}
std::string PA11SemanticModel::render_template_argument_type(TypeId type) const
{
	if (!type.valid() || type.value >= types_.size())
		throw std::runtime_error("invalid PA12 template argument type");
	const TypeKey& key = types_[type.value];
	if (key.kind == TypeKind::Named && key.named.valid() &&
		key.named.value < named_.size() && named_[key.named.value].name.valid())
		return qualified_binding_name(named_[key.named.value].owner,
			named_[key.named.value].name);
	if (key.kind == TypeKind::Cv)
	{
		std::string result;
		if ((key.cv & 1u) != 0)
			result += "const ";
		if ((key.cv & 2u) != 0)
			result += "volatile ";
		result += render_template_argument_type(key.child);
		return result;
	}
	return render_type(type);
}
std::string PA11SemanticModel::binding_display_name(BindingId binding_id) const
{
	const Binding& value = binding(binding_id);
	if (value.name.valid())
		return name_text(value.name);
	const BindingSidecar* sidecar = binding_sidecar(binding_id);
	if (sidecar != NULL && sidecar->generated_name_record.valid() &&
		sidecar->generated_name_record.value < named_.size())
	{
		const NamedRecord& record =
			named_[sidecar->generated_name_record.value];
		if (record.has_generated_identity &&
			record.generated_identity.kind == GeneratedEntityKind::AnonymousUnion)
		{
			std::ostringstream result;
			result << "__anonymous_union_storage__" <<
				record.generated_identity.source.begin.value << '_' <<
				record.generated_identity.source.end.value;
			return result.str();
		}
	}
	if (sidecar != NULL && sidecar->constructor_record.valid() &&
		sidecar->constructor_record.value < named_.size())
		return render_record_name(sidecar->constructor_record);
	return std::string();
}
std::string PA11SemanticModel::qualified_binding_name(ScopeId owner,
	BindingId binding_id) const
{
	const Binding& value = binding(binding_id);
	const BindingSidecar* sidecar = binding_sidecar(binding_id);
	if (sidecar != NULL && sidecar->constructor_record.valid() &&
		sidecar->constructor_record.value < named_.size())
	{
		const std::string name = render_record_name(sidecar->constructor_record);
		return name + "::" + name;
	}
	if (value.name.valid())
		return qualified_binding_name(owner, value.name);
	return binding_display_name(binding_id);
}
const char* PA11SemanticModel::binding_label(BindingKind kind) const
{
	switch (kind)
	{
	case BindingKind::Type: return "type";
	case BindingKind::TypeAlias: return "type-alias";
	case BindingKind::Function: return "function";
	case BindingKind::Variable: return "variable";
	case BindingKind::Parameter: return "parameter";
	case BindingKind::Enumerator: return "enumerator";
	}
	return "binding";
}
void PA11SemanticModel::dump_binding(std::ostream& output, BindingId binding_id,
	std::size_t depth, const NamePath* display_path ) const
{
	const Binding& value = binding(binding_id);
	const BindingSidecar* sidecar = binding_sidecar(binding_id);
	const std::size_t tag_count = value.kind == BindingKind::Type &&
		display_path == NULL && !value.declaration_tags.empty() ?
		value.declaration_tags.size() : 1;
	for (std::size_t tag_index = 0; tag_index < tag_count; ++tag_index)
	{
		for (std::size_t indent = 0; indent < depth; ++indent)
			output << "  ";
		output << binding_label(value.kind) << ' ';
		output << binding_display_name(binding_id);
		output << ' ';
		if (display_path != NULL &&
			type_kind(value.type) == TypeKind::Named)
			output << render_named_record(named_record_for_type(value.type),
				ClassTag::Struct, false, display_path);
		else if (value.kind == BindingKind::Type &&
			!value.declaration_tags.empty())
			output << render_named(value.type,
				value.declaration_tags[tag_index], true);
		else if (!pa12_render_mode_ && sidecar != NULL &&
			sidecar->unadjusted_type.valid())
			output << render_type(sidecar->unadjusted_type);
		else
			output << render_binding_type(value);
		if (value.kind == BindingKind::Enumerator && value.has_value)
		{
			output << ' ';
			if (value.value_unsigned)
				output << value.value_bits;
			else
				output << value.value;
		}
		output << '\n';
	}
}
bool PA11SemanticModel::has_dump_scope_view(NamedRecordId record) const
{
	return record.valid() && record.value < named_.size() &&
		named_[record.value].dump_scope_view.valid();
}
void PA11SemanticModel::dump_scope_view(std::ostream& output, const DumpScopeView& view,
	std::size_t depth) const
{
	for (std::size_t indent = 0; indent < depth; ++indent)
		output << "  ";
	output << "scope enum " << render_name_path(view.qualified_name) << '\n';
	const NamedRecord& record = named_[view.record.value];
	if (!record.scope.valid())
		throw std::runtime_error("qualified enum has no canonical scope");
	const Scope& source = scopes_[record.scope.value];
	for (std::size_t i = 0; i < source.bindings.size(); ++i)
		dump_binding(output, source.bindings[i], depth + 1,
			&view.qualified_name);
}
void PA11SemanticModel::dump_scope(std::ostream& output, ScopeId scope, std::size_t depth) const
{
	if (!scope.valid() || scope.value >= scopes_.size())
		throw std::runtime_error("invalid PA11 scope identity");
	const Scope& current = scopes_[scope.value];
	for (std::size_t i = 0; i < depth; ++i)
		output << "  ";
	switch (current.kind)
	{
	case ScopeKind::Namespace:
		output << "scope namespace " <<
			(current.name.valid() ? name_text(current.name) :
				(scope == global_ ? "<global>" : "<unnamed>"));
		break;
	case ScopeKind::Class:
		output << "scope class " <<
		render_record_name(current.record);
		break;
	case ScopeKind::Function:
		output << "scope function " << name_text(current.name);
		break;
	case ScopeKind::Block:
		output << "scope block";
		break;
	case ScopeKind::Enum:
		output << "scope enum " <<
		render_record_name(current.record);
		break;
	case ScopeKind::TemplateParameters:
		output << "scope template-parameters";
		break;
	}
	output << '\n';
	std::size_t view_index = 0;
	for (std::size_t binding_index = 0;
		binding_index <= current.bindings.size(); ++binding_index)
	{
		while (view_index < current.binding_views.size() &&
			dump_binding_views_[current.binding_views[view_index].value].position ==
				binding_index)
		{
			const DumpBindingView& view = dump_binding_views_[
				current.binding_views[view_index].value];
			if (view.binding.valid())
				dump_binding(output, view.binding, depth + 1);
			else
			{
				for (std::size_t indent = 0; indent < depth + 1; ++indent)
					output << "  ";
				output << "type " << render_name_path(view.qualified_name) << ' '
					<< render_named_record(view.record, ClassTag::Struct, false,
						&view.qualified_name) << '\n';
			}
			++view_index;
		}
		if (binding_index == current.bindings.size())
			break;
		if (current.kind == ScopeKind::Enum &&
			has_dump_scope_view(current.record))
			continue;
		dump_binding(output, current.bindings[binding_index],
			depth + 1);
	}
	for (int function_pass = 0; function_pass != 2; ++function_pass)
	{
		if (function_pass == 1)
		{
			for (std::size_t i = 0; i < current.children.size(); ++i)
				if (scopes_[current.children[i].value].kind ==
					ScopeKind::Function)
					dump_scope(output, current.children[i], depth + 1);
			continue;
		}
		std::size_t child_index = 0;
		std::size_t scope_view_index = 0;
		while (true)
		{
			while (child_index < current.children.size() &&
				scopes_[current.children[child_index].value].kind ==
					ScopeKind::Function)
				++child_index;
			const bool have_child = child_index < current.children.size();
			const bool have_view = scope_view_index < current.scope_views.size();
			if (!have_child && !have_view)
				break;
			const bool take_view = have_view && (!have_child ||
				dump_scope_views_[current.scope_views[scope_view_index].value].order <
					scopes_[current.children[child_index].value].creation_order);
			if (take_view)
			{
				dump_scope_view(output, dump_scope_views_[
					current.scope_views[scope_view_index].value], depth + 1);
				++scope_view_index;
			}
			else
			{
				dump_scope(output, current.children[child_index], depth + 1);
				++child_index;
			}
		}
	}
}
void PA11SemanticModel::process_function_definition(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 3)
		throw std::runtime_error("invalid PA11 function definition");
	const PA10AstNode& declarator = node.children[1];
	const DeclaratorName name = declarator_name(declarator);
	if (!name.found)
		throw std::runtime_error("unnamed PA11 function definition");
	const bool friend_declaration = has_friend_specifier(node.children[0]);
	const NamedRecordId friend_record = friend_declaration ?
		friend_record_for_scope(scope) : NamedRecordId();
	const bool hidden_friend = friend_record.valid() &&
		!name.path.global && name.path.components.size() == 1;
	const ScopeId target = hidden_friend ? friend_namespace_scope(scope) :
		declaration_scope(name.path, scope);
	if (!target.valid())
		throw std::runtime_error("unresolved PA11 function scope");
	// A friend definition is a namespace member, but its parameter types and
	// body are still lexically formed in the class that introduced the friend.
	// Preserve the namespace owner for the canonical binding while retaining
	// the class scope as the typed lexical parent for lookup.
	const ScopeId friend_type_scope = friend_record.valid() ? scope : target;
	const SpecFact spec = spec_fact(node.children[0], friend_type_scope);
	if (spec.is_mutable)
		throw std::runtime_error("PA11 mutable declaration is not a non-static data member");
	DeclaratorBaseKind base_kind = spec.is_auto ? DeclaratorBaseKind::AutoPlaceholder : DeclaratorBaseKind::Typed;
	const TypeId type = apply_declarator(declarator, spec.base, friend_type_scope, base_kind);
	if (!type.valid()) throw std::runtime_error(spec.is_auto ?
		"PA11 auto definition has no typed result" : "PA11 definition has no typed type");
	if (type_kind(type) != TypeKind::Function)
		throw std::runtime_error("PA11 definition is not a function");
	const bool internal_linkage = spec.is_static && target.value < scopes_.size() &&
		scopes_[target.value].kind == ScopeKind::Namespace;
	const BindingId function_binding = add_value(target, name.path.last(),
		type, true, true, true, BindingId(), SourcePoint(node.source_begin),
		internal_linkage, current_language_linkage_,
		FunctionDeclarationKind::Normal, hidden_friend,
		name.operator_function_kind, name.operator_token,
		literal_operator_suffix(name));
	record_function_declarator(function_binding, name, declarator,
		FunctionDeclarationKind::Normal);
	if (friend_record.valid())
		record_friend_function(function_binding, friend_record, hidden_friend,
		SourcePoint(node.source_begin));
	validate_nonmember_operator(function_binding);
	if (spec.is_static && target.value < scopes_.size() &&
		scopes_[target.value].kind == ScopeKind::Class)
		mark_static_member(function_binding);
	const ScopeId function_scope = create_scope(ScopeKind::Function, target,
		name.path.last());
	function_bindings_.set(function_scope, function_binding);
	if (friend_record.valid())
	{
		if (!friend_type_scope.valid() || friend_type_scope.value >= scopes_.size() ||
			scopes_[friend_type_scope.value].kind != ScopeKind::Class ||
			scopes_[friend_type_scope.value].record != friend_record ||
			friend_record.value >= named_.size() ||
			named_[friend_record.value].kind != NamedKind::Class ||
			named_[friend_record.value].scope != friend_type_scope)
			throw std::runtime_error("PA11 friend lexical owner identity is invalid");
		friend_lexical_scopes_.set(function_scope,
			FriendLexicalScopeRelation(friend_type_scope, friend_record));
	}
	FunctionFact function_fact(&node, target, function_binding,
		function_scope, ScopeId());
	function_definition_points_.set(function_scope,
		SourcePoint(node.source_begin));
	const PA10AstNode* clause = top_parameter_clause(declarator);
	if (clause != NULL)
	{
		bool variadic = false;
		std::vector<ParamFact> facts;
		parameter_types(*clause, friend_type_scope, &variadic, &facts);
		(void)variadic;
		for (std::size_t i = 0; i < facts.size(); ++i)
		{
			Binding parameter(BindingKind::Parameter, facts[i].name,
				facts[i].type);
			const BindingId parameter_id = store_binding(function_scope, parameter);
			if (facts[i].name.valid())
				append_value_index(function_scope, facts[i].name, parameter_id,
					ScopeId(), SourcePoint(node.source_begin));
		}
	}
	const ScopeId body_scope = process_compound_statement(node.children[2],
		function_scope);
	function_fact.body_scope = body_scope;
	const FunctionFactId function_id(function_facts_.size());
	function_facts_.push_back(function_fact);
	function_fact_index_.set(&node, function_id);
	function_binding_fact_index_.set(function_binding, function_id);
}
ScopeId PA11SemanticModel::process_compound_statement(const PA10AstNode& node, ScopeId parent)
{
	if (node.kind != PA10NodeKind::CompoundStatement)
		throw std::runtime_error("invalid PA11 compound statement");
	const ScopeId block = create_scope(ScopeKind::Block, parent, NameId());
	compound_facts_.push_back(CompoundFact(&node, block));
	compound_scope_index_.set(&node, block);
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		const PA10AstNode& child = node.children[i];
		switch (child.kind)
		{
		case PA10NodeKind::SimpleDeclaration:
		case PA10NodeKind::AliasDeclaration:
		case PA10NodeKind::NamespaceAliasDefinition:
		case PA10NodeKind::UsingDirective:
		case PA10NodeKind::UsingDeclaration:
			// Block declarations are formed once, in source order, before
			// PA12 traverses their statement facts.
			process_declaration(child, block);
			break;
		case PA10NodeKind::ClassSpecifier:
			// A standalone block anonymous union is a declaration whose
			// backing storage is synthesized by the typed PA11 owner.
			process_declaration(child, block);
			break;
		case PA10NodeKind::EnumSpecifier:
			// A block enum publishes its typed enumerator bindings before
			// PA12 analyzes the following expressions.
			process_declaration(child, block);
			break;
		case PA10NodeKind::CompoundStatement:
			process_compound_statement(child, block);
			break;
		case PA10NodeKind::EmptyDeclaration:
			break;
		default:
			// PA11's first semantic layer creates block scopes; expression
			// and statement semantics belong to later assignments.
			break;
		}
	}
	return block;
}
void PA11SemanticModel::process_namespace(const PA10AstNode& node, ScopeId parent)
{
	ScopeId namespace_id;
	const SourcePoint declaration_point(node.source_begin);
	if (node.producer_spelling == 0)
	{
		const ScopeId* existing = unnamed_namespace_index_.find(parent);
		if (existing != NULL)
			namespace_id = *existing;
		else
		{
			namespace_id = create_scope(ScopeKind::Namespace, parent,
				NameId());
			unnamed_namespace_index_.set(parent, namespace_id);
			scope_declaration_points_.set(namespace_id, declaration_point);
			// An unnamed namespace is visible through a typed implicit
			// using-directive in its enclosing namespace.  The relation is
			// installed once, even when the syntax is reopened later.
			scopes_[parent.value].using_directives.push_back(
				UsingDirectiveRelation(namespace_id, declaration_point));
			scopes_[parent.value].effective_using_directives.push_back(
				EffectiveUsingDirective(namespace_id, parent,
					declaration_point));
		}
	}
	else
	{
		const NameId name = name_from_spelling(node.producer_spelling);
		namespace_id = named_namespace(parent, name);
		if (scope_declaration_points_.find(namespace_id) == NULL)
			scope_declaration_points_.set(namespace_id, declaration_point);
	}
	const NamespaceFactId namespace_fact_id(namespace_facts_.size());
	namespace_facts_.push_back(NamespaceFact(&node, namespace_id));
	namespace_fact_index_.set(&node, namespace_fact_id);
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (node.children[i].kind == PA10NodeKind::InlineMarker)
		{
			scopes_[namespace_id.value].inline_namespace = true;
			const SourcePoint* existing =
				inline_namespace_declaration_points_.find(namespace_id);
			if (existing == NULL || !existing->valid() ||
				(existing->value > declaration_point.value))
				inline_namespace_declaration_points_.set(namespace_id,
					declaration_point);
		}
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		if (node.children[i].kind == PA10NodeKind::InlineMarker)
			continue;
		process_declaration(node.children[i], namespace_id);
	}
}
template<typename Relation>
bool source_point_relation_visible(const std::vector<Relation>& entries,
	NameId name, SourcePoint point)
{
	for (std::size_t i = 0; i < entries.size(); ++i)
		if (entries[i].name == name)
			return !entries[i].declaration_point.valid() ||
				entries[i].declaration_point.value <= point.value;
	return true;
}
bool namespace_source_point_applicable(const std::vector<Scope>& scopes,
	ScopeId scope, SourcePoint point)
{
	return point.valid() && scope.valid() && scope.value < scopes.size() &&
		scopes[scope.value].kind == ScopeKind::Namespace;
}
void PA11SemanticModel::record_namespace_alias(ScopeId scope, NameId name,
	SourcePoint declaration_point)
{
	NamespaceAliasList* list = namespace_alias_declaration_points_.find(scope);
	if (list == NULL)
	{
		namespace_alias_declaration_points_.set(scope, NamespaceAliasList());
		list = namespace_alias_declaration_points_.find(scope);
	}
	for (std::size_t i = 0; i < list->entries.size(); ++i)
		if (list->entries[i].name == name)
			return;
	list->entries.push_back(NamespaceAliasRelation(name, declaration_point));
}
bool PA11SemanticModel::namespace_alias_visible_at(ScopeId scope,
	NameId name, SourcePoint point) const
{
	if (!namespace_source_point_applicable(scopes_, scope, point))
		return true;
	const NamespaceAliasList* list =
		namespace_alias_declaration_points_.find(scope);
	return list == NULL ? true : source_point_relation_visible(
		list->entries, name, point);
}
void PA11SemanticModel::record_type_declaration(ScopeId scope, NameId name,
	SourcePoint declaration_point, BindingId declaration)
{
	if (!namespace_source_point_applicable(scopes_, scope, declaration_point))
		return;
	TypeDeclarationList* list = type_declaration_points_.find(scope);
	if (list == NULL)
	{
		type_declaration_points_.set(scope, TypeDeclarationList());
		list = type_declaration_points_.find(scope);
	}
	for (std::size_t i = 0; i < list->entries.size(); ++i)
	{
		TypeDeclarationRelation& entry = list->entries[i];
		if (entry.name == name)
		{
			const bool earlier = !entry.declaration_point.valid() ||
				(declaration_point.valid() && declaration_point.value <
					entry.declaration_point.value);
			if (earlier)
			{
				entry.declaration_point = declaration_point;
				if (declaration.valid())
					entry.declaration = declaration;
			}
			else if (!entry.declaration.valid() && declaration.valid())
				entry.declaration = declaration;
			return;
		}
	}
	list->entries.push_back(TypeDeclarationRelation(name, declaration_point,
		declaration));
}
bool PA11SemanticModel::type_visible_at(ScopeId scope, NameId name,
	SourcePoint point) const
{
	if (!namespace_source_point_applicable(scopes_, scope, point))
		return true;
	const TypeDeclarationList* list = type_declaration_points_.find(scope);
	return list == NULL ? true : source_point_relation_visible(
		list->entries, name, point);
}
BindingId PA11SemanticModel::type_declaration_identity(ScopeId scope,
	NameId name) const
{
	const TypeDeclarationList* list = type_declaration_points_.find(scope);
	if (list != NULL)
		for (std::size_t i = 0; i < list->entries.size(); ++i)
			if (list->entries[i].name == name &&
				list->entries[i].declaration.valid())
				return list->entries[i].declaration;
	const TypeId* type = scopes_[scope.value].types.find(name);
	if (type == NULL)
		type = scopes_[scope.value].using_types.find(name);
	if (type == NULL)
		return BindingId();
	const Scope& current = scopes_[scope.value];
	for (std::size_t i = 0; i < current.bindings.size(); ++i)
	{
		const BindingId id = current.bindings[i];
		const Binding& candidate = binding(id);
		if (candidate.name == name && candidate.type == *type &&
			(candidate.kind == BindingKind::Type ||
				candidate.kind == BindingKind::TypeAlias))
			return id;
	}
	return BindingId();
}
bool PA11SemanticModel::inline_namespace_visible_at(ScopeId scope,
	SourcePoint point) const
{
	if (!namespace_source_point_applicable(scopes_, scope, point))
		return true;
	const SourcePoint* marker = inline_namespace_declaration_points_.find(scope);
	return marker == NULL || !marker->valid() || marker->value <= point.value;
}
void PA11SemanticModel::process_namespace_alias(const PA10AstNode& node, ScopeId scope)
{
	if (node.producer_spelling == 0 || node.children.size() != 1)
		throw std::runtime_error("invalid PA11 namespace alias");
	const NameId name = name_from_spelling(node.producer_spelling);
	const NamePath target_name = name_path(node.children.front());
	const ScopeId target = resolve_namespace_path(target_name, scope);
	if (!target.valid())
		throw std::runtime_error("namespace alias target is not a namespace");
	Scope& current = scopes_[scope.value];
	if (current.namespaces.find(name) != NULL ||
		current.types.find(name) != NULL ||
		direct_value_exists(scope, name))
		throw std::runtime_error("namespace alias conflicts with binding");
	const ScopeId* old = current.namespace_aliases.find(name);
	if (old != NULL && *old != target)
		throw std::runtime_error("namespace alias redefinition");
	if (old == NULL)
		record_namespace_alias(scope, name, SourcePoint(node.source_begin));
	current.namespace_aliases.set(name, target);
}
void PA11SemanticModel::process_using_directive(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 1)
		throw std::runtime_error("invalid PA11 using directive");
	const ScopeId target = resolve_namespace_path(name_path(node.children.front()), scope);
	if (!target.valid())
		throw std::runtime_error("using directive target is not a namespace");
	const ScopeId effective = common_ancestor(scope, target);
	if (!effective.valid())
		throw std::runtime_error("using directive has no common ancestor");
	const SourcePoint declaration_point(node.source_begin);
	scopes_[scope.value].using_directives.push_back(
		UsingDirectiveRelation(target, declaration_point));
	scopes_[effective.value].effective_using_directives.push_back(
		EffectiveUsingDirective(target, scope, declaration_point));
}
bool PA11SemanticModel::direct_value_exists(ScopeId scope, NameId name) const
{
	const ValueList* found = scopes_[scope.value].values.find(name);
	if (found == NULL)
		return false;
	for (std::size_t i = 0; i < found->entries.size(); ++i)
	{
		const BindingSidecar* sidecar = binding_sidecar(
			found->entries[i].binding);
		if (sidecar == NULL || sidecar->operator_function_kind !=
			PA10OperatorFunctionKind::Literal)
			return true;
	}
	return false;
}
bool PA11SemanticModel::direct_namespace_exists(ScopeId scope, NameId name) const
{
	const Scope& current = scopes_[scope.value];
	return current.namespaces.find(name) != NULL ||
		current.namespace_aliases.find(name) != NULL;
}
BindingId PA11SemanticModel::direct_variable_binding(ScopeId scope,
	const ValueList& values, bool* direct_other) const
{
	if (!direct_other)
		throw std::runtime_error("missing direct value conflict result");
	*direct_other = false;
	BindingId result;
	for (std::size_t i = 0; i < values.entries.size(); ++i)
	{
		const ValueEntry& entry = values.entries[i];
		if (!entry.origin.valid() || entry.origin.value >= scopes_.size())
			throw std::runtime_error("invalid value origin identity");
		if (!entry.binding.valid() || entry.binding.value >= bindings_.size() ||
			entry.binding.value >= binding_owners_.size() ||
			binding_owners_[entry.binding.value] != entry.origin)
			throw std::runtime_error("value entry owner identity mismatch");
		const Binding& value = binding(entry.binding);
		const BindingSidecar* sidecar = binding_sidecar(entry.binding);
		if (sidecar != NULL && sidecar->operator_function_kind ==
			PA10OperatorFunctionKind::Literal)
			continue;
		if (entry.origin != scope)
		{
			// An imported value is not a redeclaration in this scope.  Keep it as
			// an explicit conflict, so a qualified class definition cannot silently
			// merge a direct member with a using-view or another scope's value.
			*direct_other = true;
			continue;
		}
		if (value.kind == BindingKind::Variable)
		{
			if (result.valid())
				throw std::runtime_error("ambiguous variable redeclaration");
			result = entry.binding;
		}
		else
			*direct_other = true;
	}
	return result;
}
void PA11SemanticModel::validate_qualified_class_static_definition(
	ScopeId target, NameId name) const
{
	if (!target.valid() || target.value >= scopes_.size() ||
		scopes_[target.value].kind != ScopeKind::Class)
		throw std::runtime_error("qualified definition has no class target");
	const ValueList* existing = scopes_[target.value].values.find(name);
	bool direct_other = false;
	const BindingId direct_variable = existing == NULL ? BindingId() :
		direct_variable_binding(target, *existing, &direct_other);
	if (direct_other || !direct_variable.valid() ||
		!is_static_member(direct_variable))
		throw std::runtime_error(
			"qualified class definition is not a direct static member");
}
void PA11SemanticModel::process_using_declaration(const PA10AstNode& node, ScopeId scope, MemberAccess member_access)
{
	if (!scope.valid() || scope.value >= scopes_.size() ||
		node.children.size() != 1)
		throw std::runtime_error("invalid PA11 using declaration");
	const NamePath target_name = name_path(node.children.front());
	const SourcePoint declaration_point(node.source_begin);
	if (!declaration_point.valid()) throw std::runtime_error("invalid PA11 using declaration source point");
	BindingId origin;
	const TypeId type = lookup_type_path(target_name, scope, declaration_point, &origin);
	const NameId introduced = target_name.last();
	Scope& current = scopes_[scope.value]; const bool class_access_view = current.kind == ScopeKind::Class;
	if (record_inheriting_constructor_using(node, scope, target_name, type)) return;
	const auto validate_type = [this](NameId name, TypeId type, BindingId declaration, ScopeId expected_owner) -> bool
	{
		if (!type.valid() || type.value >= types_.size() || !declaration.valid() ||
			declaration.value >= bindings_.size() ||
			declaration.value >= binding_owners_.size())
			throw std::runtime_error("using declaration type identity is invalid");
		const ScopeId owner = binding_owners_[declaration.value];
		if (!owner.valid() || owner.value >= scopes_.size() || (expected_owner.valid() && owner != expected_owner))
			throw std::runtime_error("using declaration type owner is invalid");
		const Binding& candidate = bindings_[declaration.value];
		if (candidate.name != name || candidate.type != type ||
			(candidate.kind != BindingKind::Type && candidate.kind != BindingKind::TypeAlias) ||
			type_declaration_identity(owner, name) != declaration)
			throw std::runtime_error("using declaration type declaration is invalid");
		if (candidate.kind != BindingKind::Type)
			return false;
		const NamedRecordId record = named_record_for_type(type);
		if (!record.valid() || record.value >= named_.size() ||
			(named_[record.value].kind != NamedKind::Class && named_[record.value].kind != NamedKind::Enum))
			throw std::runtime_error("using declaration tag identity is invalid");
		return true;
	};
	const TypeId* existing_type = current.types.find(introduced);
	const bool existing_type_is_tag = existing_type == NULL ||
		validate_type(introduced, *existing_type,
			type_declaration_identity(scope, introduced), scope);
	const bool imported_type_is_tag = !type.valid() ||
		validate_type(introduced, type, origin, ScopeId());
	if (direct_namespace_exists(scope, introduced) ||
		(type.valid() && (existing_type != NULL ||
			(direct_value_exists(scope, introduced) &&
				!imported_type_is_tag))) ||
		(!type.valid() && existing_type != NULL && !existing_type_is_tag))
		throw std::runtime_error("using declaration conflicts with binding");
	if (type.valid())
	{
		current.types.set(introduced, type); current.using_types.set(introduced, type);
		BindingKind kind = BindingKind::TypeAlias; if (origin.valid() && binding(origin).kind == BindingKind::Type) kind = BindingKind::Type;
		const BindingId introduced_binding = store_binding(scope, Binding(kind, introduced, type)); set_member_access(introduced_binding, class_access_view ? member_access : MemberAccess::Public);
		record_type_declaration(scope, introduced, SourcePoint(node.source_begin), introduced_binding);
	}
	const std::vector<ValueRef> values = lookup_value_path(target_name, scope, declaration_point);
	if (values.empty() && type.valid())
		return;
	if (values.empty())
		throw std::runtime_error("using declaration target is not a binding");
	const ValueList* existing = current.values.find(introduced);
	bool have_existing_value = false;
	bool existing_functions = true;
	if (existing != NULL)
	{
		for (std::size_t i = 0; i < existing->entries.size(); ++i)
		{
			const BindingSidecar* sidecar = binding_sidecar(
				existing->entries[i].binding);
			if (sidecar != NULL && sidecar->operator_function_kind ==
				PA10OperatorFunctionKind::Literal)
				continue;
			have_existing_value = true;
			const Binding& old = binding(existing->entries[i].binding);
			if (old.kind != BindingKind::Function ||
				type_kind(old.type) != TypeKind::Function)
				existing_functions = false;
		}
	}
	if (!have_existing_value)
		existing = NULL;
	std::vector<ValueRef> additions;
	std::size_t incoming_function_count = 0;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		const BindingId imported_binding = values[i].binding;
		if (!imported_binding.valid() || imported_binding.value >= bindings_.size() || imported_binding.value >= binding_owners_.size()) throw std::runtime_error("PA11 using declaration binding identity is invalid");
		const ScopeId declared_scope = binding_owners_[imported_binding.value];
		if (!declared_scope.valid() || declared_scope.value >= scopes_.size()) throw std::runtime_error("PA11 using declaration owner is invalid");
		const bool source_member = scopes_[declared_scope.value].kind == ScopeKind::Class;
		if (source_member && !member_accessible(imported_binding, declared_scope, scope, TypeId())) throw std::runtime_error("PA11 using declaration member is inaccessible");
		if (class_access_view && source_member &&
			!base_path_accessible(named_type(current.record), declared_scope, scope))
			throw std::runtime_error("PA11 using declaration member is not a base member");
		const Binding& imported = binding(imported_binding);
		incoming_function_count += imported.kind == BindingKind::Function && type_kind(imported.type) == TypeKind::Function;
		bool duplicate = false;
		for (std::size_t j = 0;
			!duplicate && existing != NULL && j < existing->entries.size(); ++j)
			duplicate = existing->entries[j].binding == imported_binding &&
				existing->entries[j].origin == values[i].scope;
		for (std::size_t j = 0;
			!duplicate && j < additions.size(); ++j)
			duplicate = additions[j].binding == imported_binding &&
				additions[j].scope == values[i].scope;
		if (!duplicate)
			additions.push_back(values[i]);
	}
	if (incoming_function_count != 0 && incoming_function_count != values.size())
		throw std::runtime_error("using declaration mixes value kinds");
	if (existing != NULL && !additions.empty() && (!existing_functions || incoming_function_count != values.size()))
		throw std::runtime_error("using declaration conflicts with binding");
	for (std::size_t i = 0; i < additions.size(); ++i)
	{
			append_value_index(scope, introduced, additions[i].binding, additions[i].scope, SourcePoint(node.source_begin), class_access_view, class_access_view ? member_access : MemberAccess::Public, class_access_view ? scope : ScopeId());
		// Keep one PA11 dump view per imported canonical binding in this
		// scope.  Lookup retains the full (BindingId, origin ScopeId) pair.
		bool have_view = false;
		for (std::size_t j = 0; j < current.binding_views.size(); ++j)
		{
			const DumpBindingViewId view_id = current.binding_views[j];
			if (view_id.valid() && view_id.value < dump_binding_views_.size() &&
				dump_binding_views_[view_id.value].binding == additions[i].binding)
			{
				have_view = true;
				break;
			}
		}
		if (!have_view)
			add_dump_binding_view(scope, additions[i].binding);
	}
}
bool PA11SemanticModel::has_friend_specifier(const PA10AstNode& node) const
{
	if (node.kind != PA10NodeKind::DeclSpecifierSeq)
		return false;
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (node.children[i].kind == PA10NodeKind::DeclSpecifier &&
			node.children[i].has_token &&
			node.children[i].token == SimpleTokenType::KW_FRIEND)
			return true;
	return false;
}
ScopeId PA11SemanticModel::friend_namespace_scope(ScopeId scope) const
{
	for (ScopeId cursor = scope; cursor.valid();)
	{
		if (cursor.value >= scopes_.size())
			return ScopeId();
		if (scopes_[cursor.value].kind == ScopeKind::Namespace)
			return cursor;
		cursor = scopes_[cursor.value].parent;
	}
	return ScopeId();
}
NamedRecordId PA11SemanticModel::friend_record_for_scope(ScopeId scope) const
{
	return scope.valid() && scope.value < scopes_.size() &&
		scopes_[scope.value].kind == ScopeKind::Class ?
		scopes_[scope.value].record : NamedRecordId();
}
void PA11SemanticModel::index_hidden_friend(NameId name,
	ScopeId namespace_scope, BindingId binding_id)
{
	if (!name.valid() || !namespace_scope.valid() ||
		namespace_scope.value >= scopes_.size() ||
		scopes_[namespace_scope.value].kind != ScopeKind::Namespace ||
		!binding_id.valid() || binding_id.value >= bindings_.size() ||
		binding_id.value >= binding_owners_.size() ||
		binding_owners_[binding_id.value] != namespace_scope)
		throw std::runtime_error("invalid PA11 hidden friend index relation");
	std::vector<HiddenFriendBindingRelation>* relations =
		hidden_friend_bindings_.find(HiddenFriendBindingKey(namespace_scope,
			name));
	if (relations == NULL)
	{
		hidden_friend_bindings_.set(HiddenFriendBindingKey(namespace_scope, name),
			std::vector<HiddenFriendBindingRelation>());
		relations = hidden_friend_bindings_.find(HiddenFriendBindingKey(
			namespace_scope, name));
	}
	for (std::size_t i = 0; i < relations->size(); ++i)
		if ((*relations)[i].namespace_scope == namespace_scope &&
			(*relations)[i].binding == binding_id)
			return;
	relations->push_back(HiddenFriendBindingRelation(namespace_scope,
		binding_id));
}
BindingId PA11SemanticModel::add_value(ScopeId scope, NameId name, TypeId type,
	bool function, bool definition, bool lexical_view, BindingId backing_storage,
	SourcePoint declaration_point, bool internal_linkage,
	LanguageLinkage language_linkage, FunctionDeclarationKind declaration_kind,
	bool hidden_friend, PA10OperatorFunctionKind operator_function_kind,
	SimpleTokenType operator_token, NameId operator_literal_suffix)
{
	const bool literal_operator = operator_function_kind ==
		PA10OperatorFunctionKind::Literal;
	if (literal_operator && (language_linkage != LanguageLinkage::Cxx ||
		!scope.valid() || scope.value >= scopes_.size() ||
		scopes_[scope.value].kind != ScopeKind::Namespace))
		throw std::runtime_error(
			"literal operator must have namespace owner and C++ linkage");
	Scope& current = scopes_[scope.value];
	if (!literal_operator && direct_namespace_exists(scope, name))
		throw std::runtime_error("value conflicts with namespace");
	const TypeId* type_found = current.types.find(name);
	if (!literal_operator && type_found != NULL &&
		type_kind(*type_found) != TypeKind::Named)
		throw std::runtime_error("value conflicts with type alias");
	const TypeId unadjusted_type = type;
	type = normalize_embedded_function_types(type);
	if (literal_operator)
	{
		if (!function || !type.valid() || type.value >= types_.size() ||
			type_kind(type) != TypeKind::Function)
			throw std::runtime_error("invalid literal operator declaration");
		const TypeKey& signature = types_[type.value];
		const TypeId size = fundamental(FundamentalType::UnsignedLongInt);
		const TypeId raw = make_pointer(make_cv(fundamental(FundamentalType::Char), 1u));
		const TypeId text[] = {raw, make_pointer(make_cv(
			fundamental(FundamentalType::WcharT), 1u)),
			make_pointer(make_cv(fundamental(FundamentalType::Char16T), 1u)),
			make_pointer(make_cv(fundamental(FundamentalType::Char32T), 1u))};
		const TypeId scalar[] = {fundamental(FundamentalType::UnsignedLongLongInt),
			fundamental(FundamentalType::LongDouble), fundamental(FundamentalType::Char),
			fundamental(FundamentalType::WcharT), fundamental(FundamentalType::Char16T),
			fundamental(FundamentalType::Char32T)};
		bool legal = !signature.variadic && signature.cv == 0 &&
			signature.parameters.size() == 1 && signature.parameters[0] == raw;
		for (std::size_t i = 0; !legal && i < sizeof(scalar) / sizeof(*scalar); ++i)
			legal = !signature.variadic && signature.cv == 0 &&
				signature.parameters.size() == 1 && signature.parameters[0] == scalar[i];
		for (std::size_t i = 0; !legal && i < sizeof(text) / sizeof(*text); ++i)
			legal = !signature.variadic && signature.cv == 0 &&
				signature.parameters.size() == 2 && signature.parameters[1] == size &&
				signature.parameters[0] == text[i];
		if (!legal)
			throw std::runtime_error("invalid literal operator parameter clause");
	}
	const ValueList* existing_values = current.values.find(name);
	const auto operator_identity_matches =
		[operator_function_kind, operator_token, operator_literal_suffix](
			const BindingSidecar* existing_sidecar) -> bool {
		const PA10OperatorFunctionKind existing_kind = existing_sidecar == NULL ?
			PA10OperatorFunctionKind::None :
			existing_sidecar->operator_function_kind;
		if (operator_function_kind == PA10OperatorFunctionKind::None &&
			existing_kind == PA10OperatorFunctionKind::None)
			return true;
		if (existing_sidecar == NULL || existing_kind != operator_function_kind ||
			existing_sidecar->operator_token != operator_token)
			return false;
		return operator_function_kind != PA10OperatorFunctionKind::Literal ||
			existing_sidecar->operator_literal_suffix == operator_literal_suffix;
	};
	const std::vector<HiddenFriendBindingRelation>* hidden_candidates =
		function ? hidden_friend_bindings_.find(HiddenFriendBindingKey(scope,
			name)) : NULL;
	if (function && hidden_candidates != NULL)
	{
		// Hidden friends are deliberately absent from the namespace value index.
		// Redeclarations use only the sparse same-name relation.  A relation may
		// already be visible because a namespace declaration preceded the friend;
		// leave that case to the ordinary value-index merge below so it is not
		// published twice.
		for (std::size_t i = 0; i < hidden_candidates->size(); ++i)
		{
			const HiddenFriendBindingRelation& relation =
				(*hidden_candidates)[i];
			if (relation.namespace_scope != scope || !relation.binding.valid() ||
				relation.binding.value >= bindings_.size() ||
				relation.binding.value >= binding_owners_.size() ||
				binding_owners_[relation.binding.value] != scope)
				continue;
			bool directly_visible = false;
			if (existing_values != NULL)
				for (std::size_t entry = 0; entry < existing_values->entries.size();
					++entry)
					if (existing_values->entries[entry].binding == relation.binding &&
						existing_values->entries[entry].origin == scope)
					{
						directly_visible = true;
						break;
					}
			if (directly_visible)
				continue;
			const Binding& existing = binding(relation.binding);
			const BindingSidecar* existing_sidecar =
				binding_sidecar(relation.binding);
			if (!operator_identity_matches(existing_sidecar))
				continue;
			if (existing.kind != BindingKind::Function ||
				existing.name != name ||
				existing.language_linkage != language_linkage ||
				existing.internal_linkage != internal_linkage ||
				type_kind(existing.type) != TypeKind::Function ||
				type_kind(type) != TypeKind::Function)
				continue;
			const TypeKey& existing_function = types_[existing.type.value];
			const TypeKey& candidate_function = types_[type.value];
			if (existing_function.cv != candidate_function.cv ||
				existing_function.variadic != candidate_function.variadic ||
				existing_function.parameters != candidate_function.parameters)
				continue;
			if (existing_function.result != candidate_function.result)
				throw std::runtime_error("conflicting function return type");
			if (declaration_kind != FunctionDeclarationKind::Normal &&
				!existing.has_definition)
				throw std::runtime_error(
					"deleted/defaulted function must be first declaration");
			if (definition && existing.has_definition)
				throw std::runtime_error("duplicate function definition");
			if (definition)
				binding(relation.binding).has_definition = true;
			if (hidden_friend)
			{
				if (lexical_view)
					add_dump_binding_view(scope, relation.binding);
			}
			else
				append_value_index(scope, name, relation.binding, ScopeId(),
					declaration_point);
			return relation.binding;
		}
	}
	if (existing_values != NULL)
	{
		if (!function)
		{
			bool direct_other = false;
			const BindingId direct_variable = direct_variable_binding(scope,
				*existing_values, &direct_other);
			if (direct_other)
				throw std::runtime_error("ambiguous variable redeclaration");
			if (direct_variable.valid())
			{
				const Binding& existing = binding(direct_variable);
				if (existing.language_linkage != language_linkage ||
					existing.internal_linkage != internal_linkage)
					throw std::runtime_error("incompatible variable redeclaration");
				if (existing.type != type)
					throw std::runtime_error("conflicting variable type");
				if (definition && existing.has_definition)
					throw std::runtime_error("duplicate variable definition");
				if (definition)
					binding(direct_variable).has_definition = true;
				if (lexical_view)
					add_dump_binding_view(scope, direct_variable);
				return direct_variable;
			}
			bool conflicting_value = false;
			for (std::size_t i = 0; i < existing_values->entries.size(); ++i)
			{
				const BindingSidecar* sidecar = binding_sidecar(
					existing_values->entries[i].binding);
				if (sidecar == NULL || sidecar->operator_function_kind !=
					PA10OperatorFunctionKind::Literal)
				{
					conflicting_value = true;
					break;
				}
			}
			if (direct_other || conflicting_value)
				throw std::runtime_error("incompatible value redeclaration");
		}
		else
		{
			for (std::size_t i = 0; i < existing_values->entries.size(); ++i)
			{
				const BindingId existing_id = existing_values->entries[i].binding;
				const Binding& existing = binding(existing_id);
				const BindingSidecar* existing_sidecar =
					binding_sidecar(existing_id);
				if (!operator_identity_matches(existing_sidecar))
					continue;
				if (existing.language_linkage != language_linkage ||
					existing.internal_linkage != internal_linkage)
					continue;
				if (existing.kind != BindingKind::Function)
					throw std::runtime_error("incompatible value redeclaration");
				if (type_kind(existing.type) != TypeKind::Function ||
					type_kind(type) != TypeKind::Function)
					throw std::runtime_error("invalid function redeclaration");
				const TypeKey& existing_function = types_[existing.type.value];
				const TypeKey& candidate_function = types_[type.value];
				if (existing_function.cv != candidate_function.cv ||
					existing_function.variadic != candidate_function.variadic ||
					existing_function.parameters != candidate_function.parameters)
					continue;
				if (existing_function.result != candidate_function.result)
					throw std::runtime_error("conflicting function return type");
				if (declaration_kind != FunctionDeclarationKind::Normal &&
					!existing.has_definition)
					throw std::runtime_error(
						"deleted/defaulted function must be first declaration");
				if (definition && existing.has_definition)
					throw std::runtime_error("duplicate function definition");
				if (definition) binding(existing_id).has_definition = true;
				if (lexical_view) add_dump_binding_view(scope, existing_id);
				return existing_id;
			}
		}
	}
	Binding value(function ? BindingKind::Function : BindingKind::Variable, name, type);
	value.has_definition = definition;
	value.language_linkage = language_linkage;
	value.internal_linkage = internal_linkage;
	const BindingId binding_id = store_binding(scope, value);
	if (backing_storage.valid() || unadjusted_type != type || hidden_friend ||
		operator_function_kind != PA10OperatorFunctionKind::None ||
		operator_literal_suffix.valid())
	{
		BindingSidecar sidecar;
		sidecar.backing_storage = backing_storage;
		if (unadjusted_type != type)
			sidecar.unadjusted_type = unadjusted_type;
		sidecar.hidden_friend = hidden_friend;
		sidecar.operator_function_kind = operator_function_kind;
		sidecar.operator_token = operator_token;
		sidecar.operator_literal_suffix = operator_literal_suffix;
		set_binding_sidecar(binding_id, sidecar);
	}
	if (hidden_friend)
		index_hidden_friend(name, scope, binding_id);
	else
		append_value_index(scope, name, binding_id, ScopeId(), declaration_point);
	return binding_id;
}
void PA11SemanticModel::record_friend_function(BindingId binding_id,
	NamedRecordId record, bool hidden, SourcePoint declaration_point)
{
	if (!binding_id.valid() || binding_id.value >= bindings_.size() ||
		!record.valid() || record.value >= named_.size() ||
		named_[record.value].kind != NamedKind::Class)
		throw std::runtime_error("invalid PA11 friend function relation");
	BindingSidecar binding_sidecar_value;
	const BindingSidecar* existing_binding = binding_sidecar(binding_id);
	if (existing_binding != NULL)
		binding_sidecar_value = *existing_binding;
	if (hidden)
		binding_sidecar_value.hidden_friend = true;
	bool have_record = false;
	for (std::size_t i = 0; i < binding_sidecar_value.friend_records.size(); ++i)
		if (binding_sidecar_value.friend_records[i] == record)
			have_record = true;
	if (!have_record)
		binding_sidecar_value.friend_records.push_back(record);
	set_binding_sidecar(binding_id, binding_sidecar_value);
	if (!hidden)
		return;
	if (binding_id.value >= binding_owners_.size())
		throw std::runtime_error("invalid PA11 hidden friend owner identity");
	index_hidden_friend(binding(binding_id).name,
		binding_owners_[binding_id.value], binding_id);
	NamedRecordSidecar record_sidecar;
	const NamedRecordSidecar* existing_record = named_record_sidecar(record);
	if (existing_record != NULL)
		record_sidecar = *existing_record;
	for (std::size_t i = 0; i < record_sidecar.hidden_friend_functions.size(); ++i)
		if (record_sidecar.hidden_friend_functions[i].binding == binding_id)
			return;
	record_sidecar.hidden_friend_functions.push_back(
		HiddenFriendFunctionRelation(binding_id,
			declaration_point));
	set_named_record_sidecar(record, record_sidecar);
}
void PA11SemanticModel::record_friend_class(NamedRecordId owner, NamedRecordId friend_record) {
	if (!owner.valid() || owner.value >= named_.size() || named_[owner.value].kind != NamedKind::Class || !named_[owner.value].scope.valid() || named_[owner.value].scope.value >= scopes_.size() || scopes_[named_[owner.value].scope.value].kind != ScopeKind::Class || scopes_[named_[owner.value].scope.value].record != owner || !friend_record.valid() || friend_record.value >= named_.size() || named_[friend_record.value].kind != NamedKind::Class || (named_[friend_record.value].scope.valid() && (named_[friend_record.value].scope.value >= scopes_.size() || scopes_[named_[friend_record.value].scope.value].kind != ScopeKind::Class || scopes_[named_[friend_record.value].scope.value].record != friend_record))) throw std::runtime_error("invalid PA11 friend class relation");
	NamedRecordSidecar owner_sidecar; const NamedRecordSidecar* existing_owner = named_record_sidecar(owner); if (existing_owner != NULL) owner_sidecar = *existing_owner;
	bool owner_relation_present = false; for (std::size_t i = 0; i < owner_sidecar.friend_class_records.size(); ++i) if (owner_sidecar.friend_class_records[i] == friend_record) { owner_relation_present = true; break; }
	if (!owner_relation_present) { owner_sidecar.friend_class_records.push_back(friend_record); set_named_record_sidecar(owner, owner_sidecar); }
	std::vector<NamedRecordId>* owners = friend_class_owners_.find(friend_record); if (owners == NULL) { friend_class_owners_.set(friend_record, std::vector<NamedRecordId>()); owners = friend_class_owners_.find(friend_record); }
	if (owners == NULL) throw std::runtime_error("friend class reverse relation is missing");
	for (std::size_t i = 0; i < owners->size(); ++i) if ((*owners)[i] == owner) return;
	owners->push_back(owner);
}
void PA11SemanticModel::validate_nonmember_operator(BindingId binding_id) const
{
	if (!binding_id.valid() || binding_id.value >= bindings_.size() ||
		binding_id.value >= binding_owners_.size())
		throw std::runtime_error("invalid PA11 operator binding");
	const BindingSidecar* sidecar = binding_sidecar(binding_id);
	if (sidecar == NULL ||
		sidecar->operator_function_kind != PA10OperatorFunctionKind::Token)
		return;
	const ScopeId owner = binding_owners_[binding_id.value];
	if (!owner.valid() || owner.value >= scopes_.size() ||
		scopes_[owner.value].kind == ScopeKind::Class)
		return;
	const Binding& value = binding(binding_id);
	if (value.kind != BindingKind::Function ||
		type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA11 nonmember operator is not a function");
	const TypeKey& function = types_[value.type.value];
	for (std::size_t i = 0; i < function.parameters.size(); ++i)
	{
		const TypeId object = strip_cv_type(expression_object_type(
			function.parameters[i]));
		const NamedRecordId record = named_record_for_type(object);
		if (record.valid() && record.value < named_.size() &&
			(named_[record.value].kind == NamedKind::Class ||
				named_[record.value].kind == NamedKind::Enum))
			return;
	}
	throw std::runtime_error("PA11 nonmember operator requires class or enum operand");
}
} // namespace pa11_semantic_internal
void emit_pa11_types(const PA10Ast& ast, std::ostream& output)
{
	pa11_semantic_internal::PA11SemanticModel model(ast);
	model.analyze();
	model.dump(output);
}
namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;
const char* PA11SemanticModel::semantic_category_name(
	SemanticValueCategory category) const
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
	else
	{
		switch (fact.token)
		{
		case SimpleTokenType::OP_ASS: result << '='; break;
		case SimpleTokenType::OP_AMP: result << '&'; break;
		case SimpleTokenType::OP_DOT: result << '.'; break;
		case SimpleTokenType::OP_ARROW: result << "->"; break;
		default: break;
		}
	}
	return result.str();
}
std::string PA11SemanticModel::semantic_literal_token(
	const SemanticFact& fact) const
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
	else if (fact.source->kind == PA10NodeKind::UserDefinedLiteral)
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
	if (fact.child_count != 0 &&
		(fact.child_begin == InvalidIdentityValue ||
			fact.child_begin > semantic_children_.size() ||
			fact.child_count > semantic_children_.size() - fact.child_begin))
		throw std::runtime_error("invalid PA12 semantic child range");
	if (fact.child_count == 0 && fact.child_begin != InvalidIdentityValue &&
		fact.child_begin > semantic_children_.size())
		throw std::runtime_error("invalid PA12 semantic child range");
	for (std::size_t child = 0; child < fact.child_count; ++child)
	{
		const SemanticFactId child_id = semantic_children_[fact.child_begin + child];
		if (!child_id.valid() || child_id.value >= semantic_facts_.size())
			throw std::runtime_error("invalid PA12 semantic child identity");
	}
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
			output << binding_display_name(fact.binding);
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
			std::string callee_type = render_binding_type(callee);
			const FunctionFact* callee_function =
				function_fact_for_binding(fact.selected_binding);
			if (callee_function != NULL && callee_function->is_constructor &&
				callee_function->synthetic)
			{
				if (!callee_function->binding.valid() ||
					callee_function->binding != fact.selected_binding ||
					callee_function->owner != fact.selected_scope ||
					!callee_function->constructor_record.valid() ||
					callee_function->constructor_record.value >= named_.size() ||
					!callee_function->owner.valid() ||
					callee_function->owner.value >= scopes_.size() ||
					scopes_[callee_function->owner.value].kind != ScopeKind::Class ||
					scopes_[callee_function->owner.value].record !=
						callee_function->constructor_record)
					throw std::runtime_error(
						"PA12 synthetic constructor identity is invalid");
				callee_type = render_member_function_type(callee.type,
					callee_function->owner, callee_function->binding);
			}
			for (std::size_t indent = 0; indent < depth + 1; ++indent)
				output << "  ";
			output << "callee " << qualified_binding_name(fact.selected_scope,
				fact.selected_binding) << ' ' << callee_type << '\n';
		}
		for (std::size_t i = 0; i < fact.child_count; ++i)
				dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
					depth + 1);
		return;
	case SemanticFactKind::NewExpression:
		output << "new-expression " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::DestructorCall:
		output << "destructor-call " << semantic_category_name(fact.category) <<
			' ' << type;
		if (fact.selected_binding.valid())
			output << ' ' << qualified_binding_name(fact.selected_scope,
				fact.selected_binding);
		output << '\n';
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
		return;
	case SemanticFactKind::IdExpression:
		output << "id-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ' << (fact.name_count != 0 ? semantic_name(fact) :
				binding_display_name(fact.binding)) << '\n';
		return;
	case SemanticFactKind::MemberExpression:
		output << "member-expression " << semantic_category_name(fact.category) <<
			' ' << type << ' ';
		if (fact.source != NULL &&
			fact.source->kind == PA10NodeKind::MemberExpression)
			output << simple_token_type_name(fact.token) << ':' <<
				semantic_name(fact);
		else
			output << semantic_name(fact);
		output << '\n';
		break;
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
		dump_pa12_aggregate_fact(output, id, depth); return;
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
	case SemanticFactKind::ConstructorAction:
		output << "constructor-action " << qualified_binding_name(
			fact.selected_scope, fact.selected_binding) << '\n';
		break;
	case SemanticFactKind::LabeledStatement:
		output << "labeled-statement " << semantic_name(fact) << '\n';
		break;
	case SemanticFactKind::GotoStatement:
		output << "goto-statement " << semantic_name(fact) << '\n';
		break;
	}
	for (std::size_t i = 0; i < fact.child_count; ++i)
		dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
			depth + 1);
}
void PA11SemanticModel::dump_pa12_function(std::ostream& output,
	const PA10AstNode& node, std::size_t depth) const
{
	const FunctionFact* function = function_fact(node);
	if (function == NULL || !function->body_fact.valid())
		throw std::runtime_error("PA12 function semantic fact is missing");
	const Binding& value = binding(function->binding);
	for (std::size_t indent = 0; indent < depth; ++indent)
		output << "  ";
	output << "function-definition " << qualified_binding_name(function->owner,
		value.name) << ' ' << render_member_function_type(value.type,
		function->owner, function->binding) << '\n';
	if (type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA12 function binding has non-function type");
	const TypeKey& function_type = types_[value.type.value];
	const bool member_function = function->owner.valid() &&
		function->owner.value < scopes_.size() &&
		scopes_[function->owner.value].kind == ScopeKind::Class &&
		!is_static_member(function->binding);
	if (!function->function_scope.valid() ||
		function->function_scope.value >= scopes_.size() ||
		scopes_[function->function_scope.value].kind != ScopeKind::Function)
		throw std::runtime_error("PA12 function scope is invalid");
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
		if (member_function && parameter_index == 0)
			output << ' ' << render_member_object_parameter(value.type,
				function->owner);
		else
		{
			const std::size_t type_index = member_function ?
				parameter_index - 1 : parameter_index;
			output << ' ' << render_type(type_index < function_type.parameters.size() ?
				function_type.parameters[type_index] : parameter.type);
		}
		output << '\n';
		++parameter_index;
	}
	dump_pa12_fact(output, function->body_fact, depth + 1);
}
void PA11SemanticModel::dump_pa12_synthetic_function(
	std::ostream& output, const SyntheticFunctionFact& function,
	std::size_t depth) const
{
	if (!function.record.valid() || function.record.value >= named_.size())
		throw std::runtime_error("PA12 synthetic function record is missing");
	const Binding& value = binding(function.binding);
	if (value.kind != BindingKind::Function ||
		type_kind(value.type) != TypeKind::Function)
		throw std::runtime_error("PA12 synthetic function type is missing");
	const FunctionFact* typed = function_fact_for_binding(function.binding);
	if (typed != NULL)
	{
		if (!typed->is_constructor || !typed->synthetic ||
			typed->binding != function.binding ||
			typed->constructor_record != function.record ||
			!typed->owner.valid() || typed->owner.value >= scopes_.size() ||
			scopes_[typed->owner.value].kind != ScopeKind::Class ||
			scopes_[typed->owner.value].record != function.record ||
			!typed->function_scope.valid() ||
			typed->function_scope.value >= scopes_.size() ||
			scopes_[typed->function_scope.value].kind != ScopeKind::Function ||
			scopes_[typed->function_scope.value].parent != typed->owner)
			throw std::runtime_error(
				"PA12 synthetic constructor fact is invalid");
		for (std::size_t indent = 0; indent < depth; ++indent)
			output << "  ";
		output << "function-definition " << qualified_binding_name(
			typed->owner, function.binding) << ' ' <<
			render_member_function_type(value.type, typed->owner,
				function.binding) << '\n';
		const Scope& function_scope = scopes_[typed->function_scope.value];
		const TypeKey& function_type = types_[value.type.value];
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
			if (parameter_index == 0)
				output << ' ' << render_member_object_parameter(value.type,
					typed->owner);
			else
			{
				const std::size_t type_index = parameter_index - 1;
				output << ' ' << render_type(type_index <
					function_type.parameters.size() ?
					function_type.parameters[type_index] : parameter.type);
			}
			output << '\n';
			++parameter_index;
		}
		if (parameter_index == 0)
			throw std::runtime_error("PA12 synthetic constructor parameter is missing");
		for (std::size_t indent = 0; indent < depth + 1; ++indent)
			output << "  ";
		output << "compound-statement\n";
		return;
	}
	for (std::size_t indent = 0; indent < depth; ++indent)
		output << "  ";
	output << "function-definition " << qualified_binding_name(
		named_[function.record.value].owner, function.binding) << ' ' <<
		render_binding_type(value) << '\n';
	const TypeKey& function_type = types_[value.type.value];
	if (function_type.parameters.size() != 1)
		throw std::runtime_error("PA12 synthetic constructor arity mismatch");
	for (std::size_t indent = 0; indent < depth + 1; ++indent)
		output << "  ";
	output << "parameter this " << render_type(function_type.parameters[0]) << '\n';
	for (std::size_t indent = 0; indent < depth + 1; ++indent)
		output << "  ";
	output << "compound-statement\n";
}
void PA11SemanticModel::dump_pa12_template_specialization(
	std::ostream& output, const TemplateSpecializationFact& specialization,
	std::size_t depth) const
{
	if (specialization.state != TemplateSpecializationState::Complete ||
		!specialization.function.valid() || specialization.function.value >=
		template_function_facts_.size() || !specialization.binding.valid() ||
		specialization.binding.value >= bindings_.size() ||
		type_kind(binding(specialization.binding).type) != TypeKind::Function)
		throw std::runtime_error("PA12 template specialization fact is missing");
	const TemplateFunctionFact& function =
		template_function_facts_[specialization.function.value];
	const Binding& value = binding(specialization.binding);
	for (std::size_t indent = 0; indent < depth; ++indent)
		output << "  ";
	output << "function-declaration " << qualified_binding_name(
		function.visible_scope, specialization.binding) << ' ' <<
		render_binding_type(value) << '\n';
	const TypeKey& function_type = types_[value.type.value];
	for (std::size_t i = 0; i < function_type.parameters.size(); ++i)
	{
		for (std::size_t indent = 0; indent < depth + 1; ++indent)
			output << "  ";
		output << "parameter  " << render_type(function_type.parameters[i]) <<
			'\n';
	}
}
void PA11SemanticModel::dump_pa12_top_node(std::ostream& output,
	const PA10AstNode& node, ScopeId scope, std::size_t depth) const
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
		output << "namespace-definition " << (node.producer_spelling == 0 ?
			"<unnamed>" : ast_.producer_spelling(node.producer_spelling)) << '\n';
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
	case PA10NodeKind::SpecialMemberDefinition:
	{
		const FunctionFact* function = function_fact(node);
		if (function != NULL && function->body_fact.valid())
			dump_pa12_function(output, node, depth);
		return;
	}
	default:
		return;
	}
}
} // namespace pa11_semantic_internal
