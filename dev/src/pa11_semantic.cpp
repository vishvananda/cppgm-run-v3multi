#include "pa11_semantic.h"
#include "pa11_semantic_model.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

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
		return ConstValue(true, static_cast<__int128>(type_size(
			sizeof_operand_type(node, scope))), false,
			fundamental(node.kind == PA10NodeKind::SizeofExpression ?
				FundamentalType::UnsignedLongInt : FundamentalType::LongLongInt));
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
	const ScopeId target = declaration_scope(name.path, scope);
	if (!target.valid())
		throw std::runtime_error("unresolved PA11 function scope");
	const SpecFact spec = spec_fact(node.children[0], target);
	const TypeId type = apply_declarator(declarator, spec.base, target);
	if (type_kind(type) != TypeKind::Function)
		throw std::runtime_error("PA11 definition is not a function");
	const bool internal_linkage = spec.is_static && target.value < scopes_.size() &&
		scopes_[target.value].kind == ScopeKind::Namespace;
	const BindingId function_binding = add_value(target, name.path.last(),
		type, true, true, true, BindingId(), SourcePoint(node.source_begin),
		internal_linkage, current_language_linkage_);
	if (spec.is_static && target.value < scopes_.size() &&
		scopes_[target.value].kind == ScopeKind::Class)
		mark_static_member(function_binding);
	const ScopeId function_scope = create_scope(ScopeKind::Function, target,
		name.path.last());
	FunctionFact function_fact(&node, target, function_binding,
		function_scope, ScopeId());
	function_definition_points_.set(function_scope,
		SourcePoint(node.source_begin));
	const PA10AstNode* clause = top_parameter_clause(declarator);
	if (clause != NULL)
	{
		bool variadic = false;
		std::vector<ParamFact> facts;
		parameter_types(*clause, target, &variadic, &facts);
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
void PA11SemanticModel::process_using_declaration(const PA10AstNode& node, ScopeId scope)
{
	if (node.children.size() != 1)
		throw std::runtime_error("invalid PA11 using declaration");
	const NamePath target_name = name_path(node.children.front());
	BindingId origin;
	const TypeId type = lookup_type_path(target_name, scope, SourcePoint(),
		&origin);
	const NameId introduced = target_name.last();
	Scope& current = scopes_[scope.value];
	if (current.types.find(introduced) != NULL ||
		direct_namespace_exists(scope, introduced))
		throw std::runtime_error("using declaration conflicts with binding");
	if (type.valid())
	{
		if (direct_value_exists(scope, introduced))
			throw std::runtime_error("using declaration conflicts with binding");
		current.types.set(introduced, type);
		current.using_types.set(introduced, type);
		BindingKind kind = BindingKind::TypeAlias;
		if (origin.valid() && binding(origin).kind == BindingKind::Type)
			kind = BindingKind::Type;
		const BindingId introduced_binding = store_binding(scope,
			Binding(kind, introduced, type));
		record_type_declaration(scope, introduced,
			SourcePoint(node.source_begin), origin.valid() ? origin :
			introduced_binding);
		return;
	}
	const std::vector<ValueRef> values = lookup_value_path(target_name, scope);
	if (values.empty())
		throw std::runtime_error("using declaration target is not a binding");
	const ValueList* existing = current.values.find(introduced);
	bool existing_functions = existing != NULL && !existing->entries.empty();
	if (existing_functions)
	{
		for (std::size_t i = 0; i < existing->entries.size(); ++i)
		{
			const Binding& old = binding(existing->entries[i].binding);
			if (old.kind != BindingKind::Function ||
				type_kind(old.type) != TypeKind::Function)
			{
				existing_functions = false;
				break;
			}
		}
	}
	bool incoming_functions = true;
	bool incoming_nonfunctions = true;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		const Binding& imported = binding(values[i].binding);
		const bool is_function = imported.kind == BindingKind::Function &&
			type_kind(imported.type) == TypeKind::Function;
		incoming_functions = incoming_functions && is_function;
		incoming_nonfunctions = incoming_nonfunctions && !is_function;
	}
	if (!incoming_functions && !incoming_nonfunctions)
		throw std::runtime_error("using declaration mixes value kinds");
	std::vector<ValueRef> additions;
	for (std::size_t i = 0; i < values.size(); ++i)
	{
		bool duplicate = false;
		if (existing != NULL)
			for (std::size_t j = 0; j < existing->entries.size(); ++j)
				if (existing->entries[j].binding == values[i].binding &&
					existing->entries[j].origin == values[i].scope)
				{
					duplicate = true;
					break;
				}
		if (!duplicate)
			for (std::size_t j = 0; j < additions.size(); ++j)
				if (additions[j].binding == values[i].binding &&
					additions[j].scope == values[i].scope)
				{
					duplicate = true;
					break;
				}
		if (duplicate)
			continue;
		const Binding& imported = binding(values[i].binding);
		const bool is_function = imported.kind == BindingKind::Function &&
			type_kind(imported.type) == TypeKind::Function;
		if (existing != NULL && (!existing_functions || !is_function))
			throw std::runtime_error("using declaration conflicts with binding");
		additions.push_back(values[i]);
	}
	for (std::size_t i = 0; i < additions.size(); ++i)
	{
		append_value_index(scope, introduced, additions[i].binding,
			additions[i].scope, SourcePoint(node.source_begin));
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
			for (std::size_t indent = 0; indent < depth + 1; ++indent)
				output << "  ";
			output << "callee " << qualified_binding_name(fact.selected_scope,
				fact.selected_binding) << ' ' << render_binding_type(callee) << '\n';
		}
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
	case SemanticFactKind::ConstructorAction:
		output << "constructor-action " << qualified_binding_name(
			fact.selected_scope, fact.selected_binding) << '\n';
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
	default:
		return;
	}
}
} // namespace pa11_semantic_internal
