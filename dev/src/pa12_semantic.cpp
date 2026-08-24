#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

void PA11SemanticModel::analyze_pa12()
{
	if (ast_.root.kind != PA10NodeKind::TranslationUnit)
		throw std::runtime_error("PA12 root is not a translation unit");
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
TypeId PA11SemanticModel::strip_cv_type(TypeId type) const
{
	return type_kind(type) == TypeKind::Cv ? types_[type.value].child : type;
}
TypeId PA11SemanticModel::strip_reference_type(TypeId type) const
{
	const TypeKind kind = type_kind(type);
	if (kind == TypeKind::LvalueReference ||
		kind == TypeKind::RvalueReference)
		return types_[type.value].child;
	return type;
}
TypeId PA11SemanticModel::expression_object_type(TypeId type) const
{
	return strip_reference_type(type);
}
bool PA11SemanticModel::fundamental_of(TypeId type, FundamentalType* result) const
{
	type = strip_cv_type(type);
	if (type_kind(type) != TypeKind::Fundamental)
		return false;
	if (result != NULL)
		*result = types_[type.value].fundamental;
	return true;
}
bool PA11SemanticModel::integral_id(TypeId type) const
{
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
	return type_kind(strip_cv_type(type)) == TypeKind::Pointer;
}
bool PA11SemanticModel::scalar_id(TypeId type) const
{
	return integral_id(type) || pointer_id(type) ||
		bool_id(type) || floating_id(type) ||
		type_kind(strip_cv_type(type)) == TypeKind::Named;
}
unsigned int PA11SemanticModel::integral_rank(TypeId type) const
{
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
bool PA11SemanticModel::qualification_convertible(TypeId source, TypeId target) const
{
	return qualification_convertible_impl(source, target, false);
}
bool PA11SemanticModel::pointer_convertible(TypeId source, TypeId target) const
{
	source = strip_cv_type(source);
	target = strip_cv_type(target);
	if (type_kind(source) != TypeKind::Pointer ||
		type_kind(target) != TypeKind::Pointer)
		return false;
	const TypeKey& source_key = types_[source.value];
	const TypeKey& target_key = types_[target.value];
	if ((source_key.cv & ~target_key.cv) != 0)
		return false;
	TypeId source_pointee = source_key.child;
	TypeId target_pointee = target_key.child;
	FundamentalType target_fundamental;
	if (fundamental_of(target_pointee, &target_fundamental) &&
		target_fundamental == FundamentalType::Void)
	{
		return (cv_qualifiers(source_pointee) &
			~cv_qualifiers(target_pointee)) == 0;
	}
	return qualification_convertible_impl(source_pointee, target_pointee, true);
}
bool PA11SemanticModel::integer_zero(const PA10AstNode& node) const
{
	if (node.kind == PA10NodeKind::ParenthesizedExpression)
		return node.children.size() == 1 && integer_zero(node.children.front());
	if (node.kind != PA10NodeKind::Literal || !node.has_literal ||
		node.literal.element_count != 0 || !integral_type(node.literal.type))
		return false;
	return literal_constant(node).valid && literal_constant(node).value == 0;
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
		return ConversionChoice();
	}

	const TypeId source_value = expression_object_type(source);
	const TypeId by_value_source = strip_cv_type(source_value);
	const TypeId by_value_target = strip_cv_type(target);
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

	FundamentalType source_fundamental;
	FundamentalType target_fundamental;
	if (fundamental_of(by_value_source, &source_fundamental) &&
		fundamental_of(by_value_target, &target_fundamental) &&
		integral_type(source_fundamental) &&
		integral_type(target_fundamental))
	{
		const unsigned int source_rank = integral_rank(by_value_source);
		const unsigned int target_rank = integral_rank(by_value_target);
		return ConversionChoice(true,
			1 + (target_rank > source_rank ? target_rank - source_rank : 0),
			ConversionKind::Integral);
	}
	if (integral_id(by_value_source) && integral_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::Integral);

	if (type_kind(by_value_source) == TypeKind::Fundamental &&
		types_[by_value_source.value].fundamental == FundamentalType::NullptrT &&
		pointer_id(by_value_target))
		return ConversionChoice(true, 1, ConversionKind::NullptrToPointer);
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
		return ConversionChoice(true, 2, ConversionKind::PointerToBool);
	return ConversionChoice();
}
ExprInfo PA11SemanticModel::apply_context_conversion(const ExprInfo& expression,
	TypeId target, const PA10AstNode* source_node)
{
	const ConversionChoice choice = conversion_for(expression.type,
		expression.category, target, source_node);
	if (!choice.valid)
		throw std::runtime_error("PA12 invalid conversion");
	const ConversionFactId conversion = add_conversion(expression.type, target,
		choice.kind, choice.rank);
	set_fact_conversion(expression.fact, conversion);
	ExprInfo result = expression;
	if (choice.kind == ConversionKind::NullIntegerToPointer)
	{
		semantic_facts_[result.fact.value].type = target;
		result.type = target;
	}
	return result;
}
TypeId PA11SemanticModel::common_integral_type(TypeId left, TypeId right) const
{
	left = strip_cv_type(expression_object_type(left));
	right = strip_cv_type(expression_object_type(right));
	if (left == right)
		return left;
	const unsigned int left_rank = integral_rank(left);
	const unsigned int right_rank = integral_rank(right);
	if (left_rank > right_rank)
		return left;
	if (right_rank > left_rank)
		return right;
	FundamentalType left_fundamental;
	FundamentalType right_fundamental;
	if (fundamental_of(left, &left_fundamental) &&
		fundamental_of(right, &right_fundamental))
	{
		if (unsigned_type(left_fundamental))
			return left;
		return right;
	}
	return left;
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
		category = SemanticValueCategory::Prvalue;
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
		const TypeId pointer = strip_cv_type(operand.type);
		if (type_kind(pointer) != TypeKind::Pointer)
			throw std::runtime_error("PA12 dereference requires pointer");
		type = types_[pointer.value].child;
		category = SemanticValueCategory::Lvalue;
		break;
	}
	case SimpleTokenType::OP_INC:
	case SimpleTokenType::OP_DEC:
		if (operand.category != SemanticValueCategory::Lvalue ||
			(!integral_id(operand.type) && !pointer_id(operand.type)))
			throw std::runtime_error("PA12 increment requires modifiable lvalue");
		type = expression_object_type(operand.type);
		category = SemanticValueCategory::Lvalue;
		break;
	case SimpleTokenType::OP_PLUS:
	case SimpleTokenType::OP_MINUS:
	case SimpleTokenType::OP_COMPL:
		if (!integral_id(operand.type))
			throw std::runtime_error("PA12 unary arithmetic requires integral");
		type = common_integral_type(operand.type, operand.type);
		break;
	case SimpleTokenType::OP_LNOT:
		if (!scalar_id(operand.type))
			throw std::runtime_error("PA12 logical negation requires scalar");
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
		(!integral_id(operand.type) && !pointer_id(operand.type)))
		throw std::runtime_error("PA12 postfix requires modifiable lvalue");
	const TypeId type = expression_object_type(operand.type);
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
	const bool left_pointer = pointer_id(left.type);
	const bool right_pointer = pointer_id(right.type);
	switch (node.token)
	{
	case SimpleTokenType::OP_COMMA:
		type = right.type;
		category = right.category;
		break;
	case SimpleTokenType::OP_PLUS:
	case SimpleTokenType::OP_MINUS:
		if (left_pointer && integral_id(right.type))
			type = expression_object_type(left.type);
		else if (node.token == SimpleTokenType::OP_PLUS &&
			right_pointer && integral_id(left.type))
			type = expression_object_type(right.type);
		else if (integral_id(left.type) && integral_id(right.type))
			type = common_integral_type(left.type, right.type);
		else
			throw std::runtime_error("PA12 invalid addition operands");
		break;
	case SimpleTokenType::OP_STAR:
	case SimpleTokenType::OP_DIV:
	case SimpleTokenType::OP_MOD:
	case SimpleTokenType::OP_BOR:
	case SimpleTokenType::OP_XOR:
	case SimpleTokenType::OP_AMP:
	case SimpleTokenType::OP_LSHIFT:
	case SimpleTokenType::OP_RSHIFT:
		if (!integral_id(left.type) || !integral_id(right.type))
			throw std::runtime_error("PA12 invalid integral operands");
		type = common_integral_type(left.type, right.type);
		break;
	case SimpleTokenType::OP_LAND:
	case SimpleTokenType::OP_LOR:
		if (!scalar_id(left.type) || !scalar_id(right.type))
			throw std::runtime_error("PA12 invalid logical operands");
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
			if (!pointer_convertible(left.type, right.type) &&
				!pointer_convertible(right.type, left.type))
				throw std::runtime_error("PA12 incompatible pointer comparison");
		}
		else if (left_pointer || right_pointer)
		{
			const ExprInfo& other = left_pointer ? right : left;
			const PA10AstNode* other_source =
				other.fact.valid() ? semantic_facts_[other.fact.value].source : NULL;
			const bool null_integer = other_source != NULL &&
				integer_zero(*other_source);
			if (!null_integer)
			{
				if (type_kind(expression_object_type(other.type)) !=
					TypeKind::Fundamental ||
					types_[expression_object_type(other.type).value].fundamental !=
						FundamentalType::NullptrT)
					throw std::runtime_error("PA12 invalid pointer comparison");
			}
		}
		else if (!integral_id(left.type) || !integral_id(right.type))
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
	const TypeId target = expression_object_type(left.type);
	if (node.token == SimpleTokenType::OP_ASS)
		apply_context_conversion(right, target, semantic_facts_[right.fact.value].source);
	else
	{
		const bool pointer_plus =
			(node.token == SimpleTokenType::OP_PLUSASS ||
			 node.token == SimpleTokenType::OP_MINUSASS) &&
			pointer_id(target);
		if (pointer_plus)
		{
			if (!integral_id(right.type))
				throw std::runtime_error("PA12 pointer compound assignment requires integral");
		}
		else if (!integral_id(target) || !integral_id(right.type))
			throw std::runtime_error("PA12 invalid compound assignment operands");
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
	const ExprInfo when_true = semantic_expression(node.children[1], scope);
	const ExprInfo when_false = semantic_expression(node.children[2], scope);
	TypeId type;
	SemanticValueCategory category = SemanticValueCategory::Prvalue;
	if (expression_object_type(when_true.type) ==
		expression_object_type(when_false.type))
	{
		type = expression_object_type(when_true.type);
		if (when_true.category == SemanticValueCategory::Lvalue &&
			when_false.category == SemanticValueCategory::Lvalue)
			category = SemanticValueCategory::Lvalue;
	}
	else if (integral_id(when_true.type) && integral_id(when_false.type))
		type = common_integral_type(when_true.type, when_false.type);
	else if (pointer_id(when_true.type) && pointer_id(when_false.type))
	{
		if (pointer_convertible(when_true.type, when_false.type))
			type = expression_object_type(when_false.type);
		else if (pointer_convertible(when_false.type, when_true.type))
			type = expression_object_type(when_true.type);
		else
			throw std::runtime_error("PA12 incompatible conditional pointers");
	}
	else if (pointer_id(when_true.type) &&
		(type_kind(expression_object_type(when_false.type)) == TypeKind::Fundamental &&
		 types_[expression_object_type(when_false.type).value].fundamental ==
			FundamentalType::NullptrT))
		type = expression_object_type(when_true.type);
	else if (pointer_id(when_false.type) &&
		(type_kind(expression_object_type(when_true.type)) == TypeKind::Fundamental &&
		 types_[expression_object_type(when_true.type).value].fundamental ==
			FundamentalType::NullptrT))
		type = expression_object_type(when_false.type);
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
bool PA11SemanticModel::builtin_cast_target(const PA10AstNode& node, TypeId* target) const
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
	case SimpleTokenType::KW_UNSIGNED: fundamental_type = FundamentalType::UnsignedInt; break;
	case SimpleTokenType::KW_VOID: fundamental_type = FundamentalType::Void; break;
	case SimpleTokenType::KW_WCHAR_T: fundamental_type = FundamentalType::WcharT; break;
	default: return false;
	}
	*target = fundamental(fundamental_type);
	return true;
}
ExprInfo PA11SemanticModel::semantic_cast_expression(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() < 2)
		throw std::runtime_error("PA12 invalid cast expression");
	const TypeId target = type_from_type_id(node.children.front(), scope);
	const ExprInfo operand = semantic_expression(node.children.back(), scope);
	const TypeId source = expression_object_type(operand.type);
	bool valid = false;
	ConversionKind kind = ConversionKind::Integral;
	if (void_id(target))
		valid = scalar_id(source) || type_kind(source) == TypeKind::Function;
	else if (integral_id(target))
	{
		valid = integral_id(source) ||
			(type_kind(strip_cv_type(source)) == TypeKind::Named &&
			 named_record_for_type(source).valid()) ||
			type_kind(source) == TypeKind::Fundamental &&
			types_[source.value].fundamental == FundamentalType::NullptrT;
		kind = ConversionKind::Integral;
	}
	else if (pointer_id(target))
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

	std::vector<ExprInfo> arguments;
	for (std::size_t i = 0; i < argument_node.children.size(); ++i)
		arguments.push_back(semantic_expression(argument_node.children[i], scope));

	std::vector<ValueRef> candidates;
	bool direct = callee_node.kind == PA10NodeKind::IdExpression;
	ExprInfo indirect_callee;
	if (direct)
	{
		const NamePath path = name_path(callee_node);
		candidates = lookup_value_path(path, scope);
		if (candidates.empty())
			throw std::runtime_error("PA12 call target not found");
	}
	else
		indirect_callee = semantic_expression(callee_node, scope);

	ValueRef selected;
	TypeId selected_type;
	bool have_selected = false;
	unsigned int selected_worst = std::numeric_limits<unsigned int>::max();
	unsigned int selected_sum = std::numeric_limits<unsigned int>::max();
	if (direct)
	{
		for (std::size_t i = 0; i < candidates.size(); ++i)
		{
			const Binding& candidate = binding(candidates[i].binding);
			if (candidate.kind != BindingKind::Function ||
				type_kind(candidate.type) != TypeKind::Function)
				continue;
			const TypeKey& function = types_[candidate.type.value];
			if ((!function.variadic && arguments.size() != function.parameters.size()) ||
				(function.variadic && arguments.size() < function.parameters.size()))
				continue;
			unsigned int worst = 0;
			unsigned int sum = 0;
			bool viable = true;
			for (std::size_t arg = 0; arg < function.parameters.size(); ++arg)
			{
				const ConversionChoice choice = conversion_for(arguments[arg].type,
					arguments[arg].category, function.parameters[arg],
					semantic_facts_[arguments[arg].fact.value].source);
				if (!choice.valid)
				{
					viable = false;
					break;
				}
				worst = std::max(worst, choice.rank);
				sum += choice.rank;
			}
			if (!viable)
				continue;
			if (!have_selected || worst < selected_worst ||
				(worst == selected_worst && sum < selected_sum))
			{
				have_selected = true;
				selected = candidates[i];
				selected_type = candidate.type;
				selected_worst = worst;
				selected_sum = sum;
			}
			else if (worst == selected_worst && sum == selected_sum)
				throw std::runtime_error("PA12 ambiguous call");
		}
		if (!have_selected)
			throw std::runtime_error("PA12 no viable call");
		const TypeKey& function = types_[selected_type.value];
		for (std::size_t arg = 0; arg < function.parameters.size(); ++arg)
			arguments[arg] = apply_context_conversion(arguments[arg],
				function.parameters[arg],
				semantic_facts_[arguments[arg].fact.value].source);
	}
	else
	{
		selected_type = callable_function_type(indirect_callee.type);
		if (!selected_type.valid())
			throw std::runtime_error("PA12 call target is not callable");
		const TypeKey& function = types_[selected_type.value];
		if ((!function.variadic && arguments.size() != function.parameters.size()) ||
			(function.variadic && arguments.size() < function.parameters.size()))
			throw std::runtime_error("PA12 indirect call arity mismatch");
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
		const ExprInfo left = semantic_expression(node.children[0], scope);
		const ExprInfo right = semantic_expression(node.children[1], scope);
		TypeId sequence = strip_cv_type(left.type);
		if (type_kind(sequence) == TypeKind::Array)
			sequence = make_pointer(types_[sequence.value].child);
		if (type_kind(sequence) != TypeKind::Pointer ||
			!integral_id(right.type))
			throw std::runtime_error("PA12 invalid subscript operands");
		const TypeId element = types_[sequence.value].child;
		std::vector<SemanticFactId> children;
		children.push_back(left.fact);
		children.push_back(right.fact);
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
		SemanticFactId variable = make_semantic_fact(fact);
		if (init.children.size() > 1)
		{
			const PA10AstNode& initializer = init.children[1];
			if (initializer.kind != PA10NodeKind::Initializer &&
				initializer.kind != PA10NodeKind::ParenInitializer)
				throw std::runtime_error("PA12 unsupported initializer");
			if (initializer.children.size() != 1)
				throw std::runtime_error("PA12 initializer arity mismatch");
			const ExprInfo expression = semantic_expression(
				initializer.children.front(), declaration->scope);
			apply_context_conversion(expression, value.type,
				semantic_facts_[expression.fact.value].source);
			set_semantic_children(variable,
				std::vector<SemanticFactId>(1, expression.fact));
		}
		declaration_semantic_ids_.push_back(variable);
	}
	declaration->semantic_count = list.children.size();
	return declaration_semantic_ids_[declaration->semantic_begin];
}
SemanticFactId PA11SemanticModel::semantic_ambiguous_call_statement(const PA10AstNode& node,
	ScopeId scope)
{
	NamePath function_name;
	const PA10AstNode* argument_node = NULL;
	if (!ambiguous_call_statement(node, scope, &function_name, &argument_node) ||
		argument_node == NULL)
		throw std::runtime_error("PA12 unsupported declaration statement");
	const std::vector<ValueRef> candidates = lookup_value_path(function_name,
		scope);
	const ExprInfo argument = semantic_id_expression(*argument_node, scope);
	ValueRef selected;
	TypeId selected_type;
	unsigned int selected_rank = std::numeric_limits<unsigned int>::max();
	bool have_selected = false;
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
		if (have_selected && choice.rank == selected_rank)
			throw std::runtime_error("PA12 ambiguous call");
		if (!have_selected || choice.rank < selected_rank)
		{
			have_selected = true;
			selected = candidates[i];
			selected_type = candidate.type;
			selected_rank = choice.rank;
		}
	}
	if (!have_selected)
		throw std::runtime_error("PA12 no viable call");
	const TypeKey& function = types_[selected_type.value];
	const ExprInfo converted = apply_context_conversion(argument,
		function.parameters.front(),
		semantic_facts_[argument.fact.value].source);
	const TypeId result_type = function.result;
	SemanticValueCategory result_category = SemanticValueCategory::Prvalue;
	if (type_kind(result_type) == TypeKind::LvalueReference)
		result_category = SemanticValueCategory::Lvalue;
	else if (type_kind(result_type) == TypeKind::RvalueReference)
		result_category = SemanticValueCategory::Xvalue;
	SemanticFact call_fact(SemanticFactKind::CallExpression, result_type,
		result_category, &node);
	call_fact.has_callee = true;
	call_fact.selected_binding = selected.binding;
	call_fact.selected_scope = selected.scope;
	const SemanticFactId call = make_semantic_fact(call_fact);
	set_semantic_children(call, std::vector<SemanticFactId>(1, converted.fact));
	return make_expression_fact(SemanticFactKind::ExpressionStatement,
		TypeId(), SemanticValueCategory::Prvalue, node,
		std::vector<SemanticFactId>(1, call));
}
SemanticFactId PA11SemanticModel::semantic_compound(const PA10AstNode& node, ScopeId parent,
	const FunctionFact& function)
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
		const SemanticFactId fact = semantic_statement(child, block, function);
		if (fact.valid())
			children.push_back(fact);
	}
	const SemanticFactId result = make_expression_fact(
		SemanticFactKind::CompoundStatement, TypeId(),
		SemanticValueCategory::Prvalue, node, children);
	(void)parent;
	return result;
}
SemanticFactId PA11SemanticModel::semantic_statement(const PA10AstNode& node, ScopeId scope,
	const FunctionFact& function)
{
	switch (node.kind)
	{
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
		return semantic_compound(node, scope, function);
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
	case PA10NodeKind::FunctionDefinition:
	{
		FunctionFact* function = function_fact(node);
		if (function == NULL || function->body_fact.valid())
			throw std::runtime_error("PA12 function fact is missing");
		function->body_fact = semantic_compound(node.children.back(),
			function->function_scope, *function);
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
	if (fact.source == NULL || !fact.source->has_token)
		return std::string();
	std::ostringstream result;
	result << simple_token_type_name(fact.source->token) << ':';
	if (fact.source->token_spelling != 0)
		result << ast_.spelling(fact.source->token_spelling);
	return result.str();
}
std::string PA11SemanticModel::semantic_literal_token(const SemanticFact& fact) const
{
	if (fact.source == NULL)
		return std::string();
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
		output << name_text(value.name) << ' ' << render_binding_type(value) << '\n';
		for (std::size_t i = 0; i < fact.child_count; ++i)
			dump_pa12_fact(output, semantic_children_[fact.child_begin + i],
				depth + 1);
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
	case SemanticFactKind::SizeofExpression:
		output << "sizeof-expression " << semantic_category_name(fact.category) <<
			' ' << type << '\n';
		return;
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
		output << "parameter " << name_text(parameter.name) << ' ' <<
			render_type(parameter_index < function_type.parameters.size() ?
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
		if (node.children.size() != 1 || node.producer_spelling == 0)
			throw std::runtime_error("PA12 invalid alias dump fact");
		const TypeId type = const_cast<PA11SemanticModel*>(this)->
			type_from_type_id(node.children.front(), scope);
		for (std::size_t indent = 0; indent < depth; ++indent)
			output << "  ";
		output << "type-alias " << ast_.producer_spelling(
			node.producer_spelling) << ' ' << render_type(type) << '\n';
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
