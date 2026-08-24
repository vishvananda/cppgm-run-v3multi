#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

TypeId PA11SemanticModel::make_cv(TypeId child, unsigned int qualifiers)
{
	if (qualifiers == 0)
		return child;
	if (type_kind(child) == TypeKind::Cv)
	{
		const TypeKey& old = types_[child.value];
		qualifiers |= old.cv;
		child = old.child;
	}
	if (type_kind(child) == TypeKind::Array)
	{
		const TypeKey& array = types_[child.value];
		return make_array(make_cv(array.child, qualifiers),
			array.unknown_bound, array.bound);
	}
	TypeKey key;
	key.kind = TypeKind::Cv;
	key.child = child;
	key.cv = qualifiers;
	return intern_type(key);
}
TypeId PA11SemanticModel::make_pointer(TypeId child, unsigned int qualifiers)
{
	if (type_kind(child) == TypeKind::LvalueReference ||
		type_kind(child) == TypeKind::RvalueReference)
		throw std::runtime_error("pointer to reference type");
	TypeKey key;
	key.kind = TypeKind::Pointer;
	key.child = child;
	key.cv = qualifiers;
	return intern_type(key);
}
TypeId PA11SemanticModel::make_member_pointer(NamedRecordId owner,
	TypeId child, unsigned int qualifiers)
{
	if (!owner.valid() || owner.value >= named_.size())
		throw std::runtime_error("member pointer has invalid class owner");
	if (named_[owner.value].kind != NamedKind::Class)
		throw std::runtime_error("member pointer owner is not a class");
	if (type_kind(child) == TypeKind::LvalueReference ||
		type_kind(child) == TypeKind::RvalueReference)
		throw std::runtime_error("member pointer to reference type");
	TypeKey key;
	key.kind = TypeKind::MemberPointer;
	key.child = child;
	key.cv = qualifiers;
	key.named = owner;
	return intern_type(key);
}
TypeId PA11SemanticModel::make_reference(TypeId child, bool rvalue)
{
	TypeId unqualified = child;
	if (type_kind(unqualified) == TypeKind::Cv)
		unqualified = types_[unqualified.value].child;
	if (type_kind(unqualified) == TypeKind::Fundamental &&
		types_[unqualified.value].fundamental == FundamentalType::Void)
		throw std::runtime_error("reference to void type");
	if (!rvalue && type_kind(child) == TypeKind::LvalueReference)
		return child;
	TypeKey key;
	key.kind = rvalue ? TypeKind::RvalueReference :
		TypeKind::LvalueReference;
	key.child = child;
	return intern_type(key);
}
TypeId PA11SemanticModel::make_array(TypeId child, bool unknown_bound,
	ArrayBound bound)
{
	const TypeKind kind = type_kind(child);
	if (kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference)
		throw std::runtime_error("array of reference type");
	TypeKey key;
	key.kind = TypeKind::Array;
	key.child = child;
	key.unknown_bound = unknown_bound;
	key.bound = bound;
	return intern_type(key);
}
TypeId PA11SemanticModel::make_function(const std::vector<TypeId>& parameters,
	bool variadic, TypeId result, unsigned int qualifiers)
{
	TypeKey key;
	key.kind = TypeKind::Function;
	key.parameters = parameters;
	key.variadic = variadic;
	key.result = result;
	key.cv = qualifiers;
	return intern_type(key);
}
TypeId PA11SemanticModel::member_object_type(TypeId function_type,
	ScopeId member_scope)
{
	if (!member_scope.valid() || member_scope.value >= scopes_.size() ||
		scopes_[member_scope.value].kind != ScopeKind::Class ||
		type_kind(function_type) != TypeKind::Function)
		return TypeId();
	const NamedRecordId owner = scopes_[member_scope.value].record;
	if (!owner.valid())
		return TypeId();
	TypeId object = named_type(owner);
	const unsigned int qualifiers = types_[function_type.value].cv;
	return qualifiers == 0 ? object : make_cv(object, qualifiers);
}
TypeId PA11SemanticModel::member_object_pointer_type(TypeId function_type,
	ScopeId member_scope)
{
	const TypeId object = member_object_type(function_type, member_scope);
	return object.valid() ? make_pointer(object) : TypeId();
}
TypeId PA11SemanticModel::strip_top_cv_type(TypeId type)
{
	type = strip_reference_type(type);
	while (type_kind(type) == TypeKind::Cv)
		type = types_[type.value].child;
	if (type_kind(type) == TypeKind::Pointer && types_[type.value].cv != 0)
		return make_pointer(types_[type.value].child);
	if (type_kind(type) == TypeKind::MemberPointer && types_[type.value].cv != 0)
		return make_member_pointer(types_[type.value].named,
			types_[type.value].child);
	return type;
}
unsigned int PA11SemanticModel::cv_qualifiers(TypeId type) const
{
	return type_kind(type) == TypeKind::Cv ? types_[type.value].cv : 0;
}
void PA11SemanticModel::qualification_decomposition(TypeId type,
	std::vector<unsigned int>& qualifiers, TypeId* unqualified) const
{
	unsigned int current_cv = 0;
	TypeId cursor = type;
	while (type_kind(cursor) == TypeKind::Cv)
	{
		current_cv |= types_[cursor.value].cv;
		cursor = types_[cursor.value].child;
	}
	if (type_kind(cursor) == TypeKind::Pointer)
	{
		current_cv |= types_[cursor.value].cv;
		qualifiers.push_back(current_cv);
		qualification_decomposition(types_[cursor.value].child,
			qualifiers, unqualified);
		return;
	}
	qualifiers.push_back(current_cv);
	*unqualified = cursor;
}
bool PA11SemanticModel::qualification_convertible_impl(TypeId source,
	TypeId target, bool outer_pointer_consumed) const
{
	if (source == target)
		return true;
	const TypeId source_core = strip_cv_type(source);
	const TypeId target_core = strip_cv_type(target);
	if (type_kind(source_core) == TypeKind::Array &&
		type_kind(target_core) == TypeKind::Array)
	{
		const TypeKey& source_array = types_[source_core.value];
		const TypeKey& target_array = types_[target_core.value];
		if (source_array.unknown_bound != target_array.unknown_bound ||
			(!source_array.unknown_bound &&
				!(source_array.bound == target_array.bound)))
			return false;
		return qualification_convertible_impl(source_array.child,
			target_array.child, outer_pointer_consumed);
	}
	std::vector<unsigned int> source_qualifiers;
	std::vector<unsigned int> target_qualifiers;
	TypeId source_unqualified;
	TypeId target_unqualified;
	qualification_decomposition(source, source_qualifiers,
		&source_unqualified);
	qualification_decomposition(target, target_qualifiers,
		&target_unqualified);
	if (source_unqualified != target_unqualified ||
		source_qualifiers.size() != target_qualifiers.size())
		return false;
	for (std::size_t i = 0; i < source_qualifiers.size(); ++i)
	{
		if ((source_qualifiers[i] & ~target_qualifiers[i]) != 0)
			return false;
		if (source_qualifiers[i] == target_qualifiers[i])
			continue;
		const std::size_t first_intermediate = outer_pointer_consumed ? 0 : 1;
		for (std::size_t j = first_intermediate; j < i; ++j)
			if ((target_qualifiers[j] & 1u) == 0)
				return false;
	}
	return true;
}
bool PA11SemanticModel::qualification_convertible(TypeId source,
	TypeId target) const
{
	return qualification_convertible_impl(source, target, false);
}
TypeId PA11SemanticModel::normalize_parameter_type(TypeId type)
{
	while (type_kind(type) == TypeKind::Cv)
		type = types_[type.value].child;
	if ((type_kind(type) == TypeKind::Pointer ||
		type_kind(type) == TypeKind::MemberPointer) &&
		types_[type.value].cv != 0)
	{
		TypeKey normalized = types_[type.value];
		normalized.cv = 0;
		return intern_type(normalized);
	}
	return type;
}
TypeId PA11SemanticModel::type_from_type_id(const PA10AstNode& node,
	ScopeId scope)
{
	if (node.kind != PA10NodeKind::TypeId || node.children.empty())
		throw std::runtime_error("invalid PA11 type-id");
	SpecFact spec = spec_fact(node.children.front(), scope);
	TypeId result = spec.base;
	if (node.children.size() > 1)
		result = apply_declarator(node.children[1], result, scope);
	return result;
}
DeclaratorOp PA11SemanticModel::pointer_op(const PA10AstNode& node,
	ScopeId scope)
{
	if (!node.name_parts.empty() || node.global_name)
	{
		const TypeId owner_type = lookup_type_path(name_path(node), scope);
		const NamedRecordId owner = named_record_for_type(owner_type);
		if (!owner.valid() || owner.value >= named_.size() ||
			named_[owner.value].kind != NamedKind::Class)
			throw std::runtime_error("member pointer owner is not a class");
		DeclaratorOp result;
		result.member_owner = owner;
		return result;
	}
	if (!node.has_token)
		unsupported("member-pointer declarators");
	DeclaratorOp result;
	if (node.token == SimpleTokenType::OP_STAR)
		result.kind = DeclaratorOp::Pointer;
	else if (node.token == SimpleTokenType::OP_AMP)
		result.kind = DeclaratorOp::LvalueReference;
	else if (node.token == SimpleTokenType::OP_LAND)
		result.kind = DeclaratorOp::RvalueReference;
	else
		unsupported("unknown pointer operator");
	return result;
}
bool PA11SemanticModel::contains_parameter_pack(const PA10AstNode& node) const
{
	if (node.kind == PA10NodeKind::ParameterPack)
		return true;
	if (node.kind == PA10NodeKind::ParameterClause)
		return false;
	for (std::size_t i = 0; i < node.children.size(); ++i)
		if (contains_parameter_pack(node.children[i]))
			return true;
	return false;
}
std::vector<TypeId> PA11SemanticModel::parameter_types(
	const PA10AstNode& clause, ScopeId scope, bool* variadic,
	std::vector<ParamFact>* facts)
{
	if (clause.kind != PA10NodeKind::ParameterClause)
		throw std::runtime_error("invalid PA11 parameter clause");
	std::vector<TypeId> result;
	*variadic = false;
	for (std::size_t i = 0; i < clause.children.size(); ++i)
	{
		const PA10AstNode& child = clause.children[i];
		if (child.kind == PA10NodeKind::ParameterPack)
		{
			*variadic = true;
			continue;
		}
		if (child.kind != PA10NodeKind::ParameterDeclaration ||
			child.children.empty())
			throw std::runtime_error("invalid PA11 parameter declaration");
		SpecFact spec = spec_fact(child.children.front(), scope);
		TypeId type = spec.base;
		DeclaratorName name;
		if (child.children.size() > 1)
		{
			name = declarator_name(child.children[1]);
			type = apply_declarator(child.children[1], type, scope);
			if (contains_parameter_pack(child.children[1]))
				*variadic = true;
		}
		const bool unnamed_void = type_kind(type) == TypeKind::Fundamental &&
			types_[type.value].fundamental == FundamentalType::Void && !name.found;
		if (unnamed_void && clause.children.size() == 1)
			continue;
		result.push_back(type);
		if (facts != NULL)
			facts->push_back(ParamFact(name.found ? name.path.last() : NameId(),
				type));
	}
	return result;
}
TypeId PA11SemanticModel::apply_prefix(const std::vector<DeclaratorOp>& ops,
	TypeId base)
{
	TypeId result = base;
	for (std::size_t i = 0; i < ops.size(); ++i)
	{
		switch (ops[i].kind)
		{
		case DeclaratorOp::Pointer:
			result = ops[i].member_owner.valid() ?
				make_member_pointer(ops[i].member_owner, result, ops[i].cv) :
				make_pointer(result, ops[i].cv);
			break;
		case DeclaratorOp::LvalueReference:
			result = make_reference(result, false);
			break;
		case DeclaratorOp::RvalueReference:
			result = make_reference(result, true);
			break;
		case DeclaratorOp::Array:
		case DeclaratorOp::Function:
			throw std::runtime_error("invalid PA11 prefix declarator operation");
		}
	}
	return result;
}
TypeId PA11SemanticModel::apply_suffix(const std::vector<DeclaratorOp>& ops,
	TypeId base, ScopeId scope)
{
	TypeId result = base;
	for (std::size_t i = ops.size(); i != 0; --i)
	{
		const DeclaratorOp& op = ops[i - 1];
		if (op.kind == DeclaratorOp::Array)
		{
			result = make_array(result, op.unknown_bound, op.bound);
			continue;
		}
		if (op.kind == DeclaratorOp::Function)
		{
			bool variadic = false;
			const std::vector<TypeId> parameters = parameter_types(
				*op.parameter_clause, scope, &variadic, NULL);
			result = make_function(parameters, variadic, result, op.cv);
			continue;
		}
		throw std::runtime_error("invalid PA11 suffix declarator operation");
	}
	return result;
}
TypeId PA11SemanticModel::apply_declarator(const PA10AstNode& node,
	TypeId base, ScopeId scope)
{
	if (node.kind != PA10NodeKind::Declarator &&
		node.kind != PA10NodeKind::AbstractDeclarator)
		throw std::runtime_error("invalid PA11 declarator node");
	std::size_t direct = node.children.size();
	for (std::size_t i = 0; i < node.children.size(); ++i)
	{
		if (node.children[i].kind == PA10NodeKind::Identifier ||
			node.children[i].kind == PA10NodeKind::NestedDeclarator)
		{
			direct = i;
			break;
		}
	}
	std::vector<DeclaratorOp> prefix;
	for (std::size_t i = 0; i < direct; ++i)
	{
		if (node.children[i].kind == PA10NodeKind::PtrOperator)
		{
			DeclaratorOp op = pointer_op(node.children[i], scope);
			std::size_t at = i + 1;
			while (at < direct && is_cv_node(node.children[at]))
			{
				op.cv |= cv_bit(node.children[at]);
				++at;
			}
			prefix.push_back(op);
			i = at - 1;
		}
		else if (node.children[i].kind != PA10NodeKind::ParameterPack &&
			node.children[i].kind != PA10NodeKind::CvQualifier)
			throw std::runtime_error("invalid PA11 declarator prefix");
	}
	std::vector<DeclaratorOp> suffix;
	if (direct < node.children.size())
	{
		for (std::size_t i = direct + 1; i < node.children.size(); ++i)
		{
			const PA10AstNode& child = node.children[i];
			if (child.kind == PA10NodeKind::ArraySuffix)
			{
				DeclaratorOp op(DeclaratorOp::Array);
				if (child.children.empty())
					op.unknown_bound = true;
				else if (child.children.size() == 1)
				{
					const ConstValue bound = eval_constexpr(
						child.children.front(), scope);
					if (!bound.valid || bound.value <= 0 ||
						bound.value > static_cast<__int128>(InvalidIdentityValue))
						throw std::runtime_error("invalid PA11 array bound");
					op.bound = ArrayBound(static_cast<std::size_t>(bound.value));
				}
				else
					throw std::runtime_error("unsupported PA11 array bound expression");
				suffix.push_back(op);
			}
			else if (child.kind == PA10NodeKind::ParameterClause)
			{
				DeclaratorOp op(DeclaratorOp::Function);
				op.parameter_clause = &child;
				suffix.push_back(op);
			}
			else if (child.kind == PA10NodeKind::FunctionQualifier ||
				child.kind == PA10NodeKind::RefQualifier ||
				child.kind == PA10NodeKind::VirtSpecifier)
			{
			}
			else if (is_cv_node(child) && !suffix.empty() &&
				suffix.back().kind == DeclaratorOp::Function)
				suffix.back().cv |= cv_bit(child);
			else
				throw std::runtime_error("invalid PA11 declarator suffix");
		}
	}
	TypeId result = base;
	if (direct < node.children.size() &&
		node.children[direct].kind == PA10NodeKind::NestedDeclarator)
	{
		const TypeId with_prefix = apply_prefix(prefix, base);
		const TypeId with_suffix = apply_suffix(suffix, with_prefix, scope);
		result = apply_declarator(node.children[direct].children.front(),
			with_suffix, scope);
	}
	else
	{
		result = apply_prefix(prefix, base);
		result = apply_suffix(suffix, result, scope);
	}
	return result;
}
} // namespace pa11_semantic_internal
