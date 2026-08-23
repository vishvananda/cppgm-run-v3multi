#include "pa8_semantic.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cpp_declaration_syntax.h"
#include "cpp_syntax_tokens.h"

#include "cpp_semantic_core.h"

using namespace CppSemantic;

namespace
{
struct PA8Value
{
	TypeId type;
	bool constant;
	bool lvalue;
	bool null_pointer;
	EntityId entity;
	std::size_t element_count;
	std::vector<std::uint8_t> bytes;

	PA8Value()
		: type(), constant(false), lvalue(false), null_pointer(false),
		  entity(), element_count(0), bytes()
	{}
};

struct PA8BaseSpec
{
	bool is_typedef;
	bool is_static;
	bool is_thread_local;
	bool is_extern;
	bool is_constexpr;
	bool is_inline;
	bool has_named_type;
	QualifiedName named_type;
	unsigned int cv;
	bool has_char;
	bool has_short;
	bool has_int;
	unsigned int long_count;
	bool has_signed;
	bool has_unsigned;
	bool has_bool;
	bool has_wchar;
	bool has_char16;
	bool has_char32;
	bool has_float;
	bool has_double;
	bool has_void;
	TypeId resolved_type;

	PA8BaseSpec()
		: is_typedef(false), is_static(false), is_thread_local(false),
		  is_extern(false), is_constexpr(false), is_inline(false),
		  has_named_type(false), named_type(), cv(0), has_char(false),
		  has_short(false), has_int(false), long_count(0), has_signed(false),
		  has_unsigned(false), has_bool(false), has_wchar(false),
		  has_char16(false), has_char32(false), has_float(false),
		  has_double(false), has_void(false), resolved_type()
	{}
};

struct Layout
{
	std::size_t size;
	std::size_t alignment;

	Layout(std::size_t size = 0, std::size_t alignment = 1)
		: size(size), alignment(alignment)
	{}
};
}

struct PA8ProgramModel::Impl : CppSemantic::SemanticCore
{
	std::size_t next_translation_unit;

	Impl();

	QualifiedName qualified_name(const CppSyntaxQualifiedName& source);

	Layout layout(TypeId type) const;
	PA8BaseSpec declaration_spec(const CppSyntaxDeclSpec& source,
		NamespaceId scope);
	TypeId fundamental_from_spec(const PA8BaseSpec& spec);
	std::vector<TypeId> parameter_types(const CppSyntaxDeclaratorOp& source,
		NamespaceId scope);
	DeclaratorShape declarator(const CppSyntaxDeclarator& source,
		NamespaceId scope);
	TypeId apply_shape(TypeId base, const DeclaratorShape& shape);
	TypeId type_id(const CppSyntaxTypeId& source, NamespaceId scope);

	PA8Value evaluate(const CppSyntaxExpression& expression,
		NamespaceId scope) const;
	PA8Value literal_value(const LiteralData& literal) const;
	bool convert_value(TypeId destination, const PA8Value& source,
		std::vector<std::uint8_t>* bytes, EntityId* relocation,
		std::ptrdiff_t* addend) const;
	bool convert_numeric(TypeId destination, const PA8Value& source,
		std::vector<std::uint8_t>* bytes) const;
	long double numeric_value(const PA8Value& source) const;
	std::uint64_t unsigned_numeric_value(const PA8Value& source) const;
	bool is_integral(TypeId type) const;
	bool is_floating(TypeId type) const;

	EntityId declare_entity(NamespaceId scope, NameId name, TypeId type,
		const PA8BaseSpec& spec, bool function_definition,
		const CppSyntaxDeclarator& source);
	void analyze_static_assert(const CppSyntaxExpression& expression,
		NamespaceId scope);
	void analyze_translation_unit(const PPTokenBuffer& tokens,
		std::size_t translation_unit);
	void build_image(std::vector<char>& image) const;
};

PA8ProgramModel::Impl::Impl()
	: SemanticCore(), next_translation_unit(0)
{}

QualifiedName PA8ProgramModel::Impl::qualified_name(
	const CppSyntaxQualifiedName& source)
{
	QualifiedName result;
	result.global = source.global;
	result.components.reserve(source.components.size());
	for (std::size_t i = 0; i < source.components.size(); ++i)
		result.components.push_back(intern_name(source.components[i]));
	return result;
}

Layout PA8ProgramModel::Impl::layout(TypeId type) const
{
	if (!type.valid() || type.value >= types.size())
		throw std::runtime_error("invalid PA8 object type");
	const TypeKey& key = types[type.value].key;
	if (key.kind == TypeKind::Fundamental)
	{
		std::size_t size = 0;
		switch (key.fundamental)
		{
		case FundamentalType::SignedChar:
		case FundamentalType::Char:
		case FundamentalType::UnsignedChar:
		case FundamentalType::Bool:
			size = 1;
			break;
		case FundamentalType::ShortInt:
		case FundamentalType::UnsignedShortInt:
		case FundamentalType::Char16T:
			size = 2;
			break;
		case FundamentalType::Int:
		case FundamentalType::UnsignedInt:
		case FundamentalType::WcharT:
		case FundamentalType::Char32T:
		case FundamentalType::Float:
			size = 4;
			break;
		case FundamentalType::LongInt:
		case FundamentalType::LongLongInt:
		case FundamentalType::UnsignedLongInt:
		case FundamentalType::UnsignedLongLongInt:
		case FundamentalType::Double:
			size = 8;
			break;
		case FundamentalType::LongDouble:
			size = 16;
			break;
		case FundamentalType::NullptrT:
			size = 8;
			break;
		case FundamentalType::Void:
			throw std::runtime_error("void has no PA8 object layout");
		}
		return Layout(size, size);
	}
	if (key.kind == TypeKind::Cv)
		return layout(key.child);
	if (key.kind == TypeKind::Pointer ||
		key.kind == TypeKind::LvalueReference ||
		key.kind == TypeKind::RvalueReference)
		return Layout(8, 8);
	if (key.kind == TypeKind::Function)
		return Layout(4, 4);
	if (key.unknown_bound)
		throw std::runtime_error("incomplete PA8 array object");
	const Layout child = layout(key.child);
	if (key.bound != 0 && child.size >
		std::numeric_limits<std::size_t>::max() / key.bound)
		throw std::runtime_error("PA8 array size overflow");
	return Layout(child.size * key.bound, child.alignment);
}

TypeId PA8ProgramModel::Impl::fundamental_from_spec(
	const PA8BaseSpec& spec)
{
	FundamentalType type = FundamentalType::Int;
	if (spec.has_char)
	{
		if (spec.has_short || spec.has_int || spec.long_count != 0 ||
			spec.has_bool || spec.has_wchar || spec.has_char16 ||
			spec.has_char32 || spec.has_float || spec.has_double ||
			spec.has_void || (spec.has_signed && spec.has_unsigned))
			throw std::runtime_error("invalid PA8 character specifiers");
		if (spec.has_signed)
			type = FundamentalType::SignedChar;
		else if (spec.has_unsigned)
			type = FundamentalType::UnsignedChar;
		else
			type = FundamentalType::Char;
	}
	else if (spec.has_char16)
	{
		if (spec.has_short || spec.has_int || spec.long_count != 0 ||
			spec.has_signed || spec.has_unsigned || spec.has_bool ||
			spec.has_wchar || spec.has_char32 || spec.has_float ||
			spec.has_double || spec.has_void)
			throw std::runtime_error("invalid PA8 char16_t specifiers");
		type = FundamentalType::Char16T;
	}
	else if (spec.has_char32)
	{
		if (spec.has_short || spec.has_int || spec.long_count != 0 ||
			spec.has_signed || spec.has_unsigned || spec.has_bool ||
			spec.has_wchar || spec.has_char16 || spec.has_float ||
			spec.has_double || spec.has_void)
			throw std::runtime_error("invalid PA8 char32_t specifiers");
		type = FundamentalType::Char32T;
	}
	else if (spec.has_wchar)
	{
		if (spec.has_short || spec.has_int || spec.long_count != 0 ||
			spec.has_signed || spec.has_unsigned || spec.has_bool ||
			spec.has_char16 || spec.has_char32 || spec.has_float ||
			spec.has_double || spec.has_void)
			throw std::runtime_error("invalid PA8 wchar_t specifiers");
		type = FundamentalType::WcharT;
	}
	else if (spec.has_bool)
	{
		if (spec.has_short || spec.has_int || spec.long_count != 0 ||
			spec.has_signed || spec.has_unsigned || spec.has_float ||
			spec.has_double || spec.has_void)
			throw std::runtime_error("invalid PA8 bool specifiers");
		type = FundamentalType::Bool;
	}
	else if (spec.has_float)
	{
		if (spec.has_short || spec.has_int || spec.long_count != 0 ||
			spec.has_signed || spec.has_unsigned || spec.has_double ||
			spec.has_void)
			throw std::runtime_error("invalid PA8 float specifiers");
		type = FundamentalType::Float;
	}
	else if (spec.has_double)
	{
		if (spec.has_short || spec.has_int || spec.has_signed ||
			spec.has_unsigned || spec.long_count > 1 || spec.has_void)
			throw std::runtime_error("invalid PA8 double specifiers");
		type = spec.long_count == 0 ? FundamentalType::Double :
			FundamentalType::LongDouble;
	}
	else if (spec.has_void)
	{
		if (spec.has_short || spec.has_int || spec.long_count != 0 ||
			spec.has_signed || spec.has_unsigned)
			throw std::runtime_error("invalid PA8 void specifiers");
		type = FundamentalType::Void;
	}
	else
	{
		if (spec.long_count > 2 || (spec.has_signed && spec.has_unsigned))
			throw std::runtime_error("invalid PA8 integer specifiers");
		if (spec.long_count == 2)
			type = spec.has_unsigned ? FundamentalType::UnsignedLongLongInt :
				FundamentalType::LongLongInt;
		else if (spec.long_count == 1)
			type = spec.has_unsigned ? FundamentalType::UnsignedLongInt :
				FundamentalType::LongInt;
		else if (spec.has_short)
			type = spec.has_unsigned ? FundamentalType::UnsignedShortInt :
				FundamentalType::ShortInt;
		else if (spec.has_unsigned)
			type = FundamentalType::UnsignedInt;
		else
			type = FundamentalType::Int;
	}
	return cv(fundamental(type), spec.cv);
}

PA8BaseSpec PA8ProgramModel::Impl::declaration_spec(
	const CppSyntaxDeclSpec& source, NamespaceId scope)
{
	PA8BaseSpec result;
	result.is_typedef = source.is_typedef;
	result.is_static = source.is_static;
	result.is_thread_local = source.is_thread_local;
	result.is_extern = source.is_extern;
	result.is_constexpr = source.is_constexpr;
	result.is_inline = source.is_inline;
	result.has_named_type = source.has_named_type;
	result.named_type = qualified_name(source.named_type);
	result.cv = source.cv;
	result.has_char = source.has_char;
	result.has_short = source.has_short;
	result.has_int = source.has_int;
	result.long_count = source.long_count;
	result.has_signed = source.has_signed;
	result.has_unsigned = source.has_unsigned;
	result.has_bool = source.has_bool;
	result.has_wchar = source.has_wchar;
	result.has_char16 = source.has_char16;
	result.has_char32 = source.has_char32;
	result.has_float = source.has_float;
	result.has_double = source.has_double;
	result.has_void = source.has_void;
	if (result.has_named_type)
	{
		if (result.has_char || result.has_short || result.has_int ||
			result.long_count != 0 || result.has_signed ||
			result.has_unsigned || result.has_bool || result.has_wchar ||
			result.has_char16 || result.has_char32 || result.has_float ||
			result.has_double || result.has_void)
			throw std::runtime_error("mixed PA8 type specifiers");
		if (!lookup_type(result.named_type, scope, &result.resolved_type))
			throw std::runtime_error("unresolved PA8 typedef name");
		result.resolved_type = cv(result.resolved_type, result.cv);
	}
	else
		result.resolved_type = fundamental_from_spec(result);
	if (result.is_constexpr)
		result.resolved_type = cv(result.resolved_type, 1u);
	return result;
}

std::vector<TypeId> PA8ProgramModel::Impl::parameter_types(
	const CppSyntaxDeclaratorOp& source, NamespaceId scope)
{
	std::vector<TypeId> result;
	if (source.parameters.size() == 1 &&
		!source.parameters[0].has_declarator)
	{
		PA8BaseSpec only = declaration_spec(source.parameters[0].spec, scope);
		if (type_kind(remove_top_cv(only.resolved_type)) ==
			TypeKind::Fundamental &&
			types[remove_top_cv(only.resolved_type).value].key.fundamental ==
				FundamentalType::Void)
			return result;
	}
	result.reserve(source.parameters.size());
	for (std::size_t i = 0; i < source.parameters.size(); ++i)
	{
		const CppSyntaxParameter& parameter = source.parameters[i];
		PA8BaseSpec spec = declaration_spec(parameter.spec, scope);
		DeclaratorShape shape;
		if (parameter.has_declarator)
			shape = declarator(parameter.declarator, scope);
		TypeId type = apply_shape(spec.resolved_type, shape);
		type = remove_top_cv(type);
		if (type_kind(type) == TypeKind::Array)
			type = pointer(types[type.value].key.child);
		else if (type_kind(type) == TypeKind::Function)
			type = pointer(type);
		result.push_back(type);
	}
	return result;
}

DeclaratorShape PA8ProgramModel::Impl::declarator(
	const CppSyntaxDeclarator& source, NamespaceId scope)
{
	DeclaratorShape result;
	result.has_name = source.has_name;
	result.name = qualified_name(source.name);
	result.operations.reserve(source.operations.size());
	for (std::size_t i = 0; i < source.operations.size(); ++i)
	{
		const CppSyntaxDeclaratorOp& source_operation = source.operations[i];
		DeclaratorOp operation;
		switch (source_operation.kind)
		{
		case CppSyntaxDeclaratorOpKind::Pointer:
			operation.kind = DeclaratorOpKind::Pointer;
			break;
		case CppSyntaxDeclaratorOpKind::LvalueReference:
			operation.kind = DeclaratorOpKind::LvalueReference;
			break;
		case CppSyntaxDeclaratorOpKind::RvalueReference:
			operation.kind = DeclaratorOpKind::RvalueReference;
			break;
		case CppSyntaxDeclaratorOpKind::Array:
			operation.kind = DeclaratorOpKind::Array;
			break;
		case CppSyntaxDeclaratorOpKind::Function:
			operation.kind = DeclaratorOpKind::Function;
			break;
		}
		operation.cv = source_operation.cv;
		operation.unknown_bound = source_operation.unknown_bound;
		operation.bound = source_operation.bound;
		operation.variadic = source_operation.variadic;
		if (source_operation.kind == CppSyntaxDeclaratorOpKind::Function)
			operation.parameters = parameter_types(source_operation, scope);
		result.operations.push_back(operation);
	}
	return result;
}

TypeId PA8ProgramModel::Impl::apply_shape(TypeId base,
	const DeclaratorShape& shape)
{
	TypeId result = base;
	for (std::vector<DeclaratorOp>::const_reverse_iterator it =
		shape.operations.rbegin(); it != shape.operations.rend(); ++it)
	{
		const DeclaratorOp& operation = *it;
		switch (operation.kind)
		{
		case DeclaratorOpKind::Pointer:
			result = pointer(result, operation.cv);
			break;
		case DeclaratorOpKind::LvalueReference:
			result = reference(result, false);
			break;
		case DeclaratorOpKind::RvalueReference:
			result = reference(result, true);
			break;
		case DeclaratorOpKind::Array:
			result = array(result, operation.unknown_bound, operation.bound);
			break;
		case DeclaratorOpKind::Function:
			result = function(operation.parameters, operation.variadic, result);
			break;
		}
	}
	return result;
}

TypeId PA8ProgramModel::Impl::type_id(const CppSyntaxTypeId& source,
	NamespaceId scope)
{
	PA8BaseSpec spec = declaration_spec(source.spec, scope);
	if (!source.has_declarator)
		return spec.resolved_type;
	return apply_shape(spec.resolved_type,
		declarator(source.declarator, scope));
}

bool PA8ProgramModel::Impl::is_integral(TypeId type) const
{
	type = remove_top_cv(type);
	if (type_kind(type) != TypeKind::Fundamental)
		return false;
	switch (types[type.value].key.fundamental)
	{
	case FundamentalType::SignedChar:
	case FundamentalType::ShortInt:
	case FundamentalType::Int:
	case FundamentalType::LongInt:
	case FundamentalType::LongLongInt:
	case FundamentalType::UnsignedChar:
	case FundamentalType::UnsignedShortInt:
	case FundamentalType::UnsignedInt:
	case FundamentalType::UnsignedLongInt:
	case FundamentalType::UnsignedLongLongInt:
	case FundamentalType::WcharT:
	case FundamentalType::Char:
	case FundamentalType::Char16T:
	case FundamentalType::Char32T:
	case FundamentalType::Bool:
		return true;
	default:
		return false;
	}
}

bool PA8ProgramModel::Impl::is_floating(TypeId type) const
{
	type = remove_top_cv(type);
	if (type_kind(type) != TypeKind::Fundamental)
		return false;
	const FundamentalType fundamental_type = types[type.value].key.fundamental;
	return fundamental_type == FundamentalType::Float ||
		fundamental_type == FundamentalType::Double ||
		fundamental_type == FundamentalType::LongDouble;
}

std::uint64_t PA8ProgramModel::Impl::unsigned_numeric_value(
	const PA8Value& source) const
{
	TypeId type = remove_top_cv(source.type);
	if (type_kind(type) != TypeKind::Fundamental)
		throw std::runtime_error("PA8 value is not scalar");
	const FundamentalType fundamental_type = types[type.value].key.fundamental;
	if (is_floating(type))
		return static_cast<std::uint64_t>(numeric_value(source));
	std::uint64_t value = 0;
	const std::size_t count = std::min<std::size_t>(source.bytes.size(), 8);
	for (std::size_t i = 0; i < count; ++i)
		value |= static_cast<std::uint64_t>(source.bytes[i]) << (i * 8);
	if (fundamental_type == FundamentalType::Bool)
		return value == 0 ? 0 : 1;
	return value;
}

long double PA8ProgramModel::Impl::numeric_value(const PA8Value& source) const
{
	TypeId type = remove_top_cv(source.type);
	if (type_kind(type) != TypeKind::Fundamental)
		throw std::runtime_error("PA8 value is not numeric");
	const FundamentalType fundamental_type = types[type.value].key.fundamental;
	if (fundamental_type == FundamentalType::Float)
	{
		float value = 0.0f;
		if (source.bytes.size() != sizeof(value))
			throw std::runtime_error("invalid PA8 float payload");
		std::memcpy(&value, &source.bytes[0], sizeof(value));
		return static_cast<long double>(value);
	}
	if (fundamental_type == FundamentalType::Double)
	{
		double value = 0.0;
		if (source.bytes.size() != sizeof(value))
			throw std::runtime_error("invalid PA8 double payload");
		std::memcpy(&value, &source.bytes[0], sizeof(value));
		return static_cast<long double>(value);
	}
	if (fundamental_type == FundamentalType::LongDouble)
	{
		long double value = 0.0L;
		if (source.bytes.size() != sizeof(value))
			throw std::runtime_error("invalid PA8 long double payload");
		std::memcpy(&value, &source.bytes[0], sizeof(value));
		return value;
	}
	const std::uint64_t value = unsigned_numeric_value(source);
	const bool signed_type = fundamental_type == FundamentalType::SignedChar ||
		fundamental_type == FundamentalType::ShortInt ||
		fundamental_type == FundamentalType::Int ||
		fundamental_type == FundamentalType::LongInt ||
		fundamental_type == FundamentalType::LongLongInt;
	if (!signed_type)
		return static_cast<long double>(value);
	std::size_t width = source.bytes.size();
	if (width > 8)
		width = 8;
	if (width != 0 && width < 8 &&
		(value & (static_cast<std::uint64_t>(1) << (width * 8 - 1))) != 0)
	{
		const std::uint64_t mask = ~static_cast<std::uint64_t>(0) <<
			(width * 8);
		return static_cast<long double>(static_cast<std::int64_t>(value | mask));
	}
	return static_cast<long double>(static_cast<std::int64_t>(value));
}

PA8Value PA8ProgramModel::Impl::literal_value(const LiteralData& literal) const
{
	PA8Value result;
	result.type = fundamental(literal.type);
	result.constant = true;
	result.lvalue = literal.element_count != 0;
	result.element_count = literal.element_count;
	result.bytes = literal.bytes;
	return result;
}

PA8Value PA8ProgramModel::Impl::evaluate(
	const CppSyntaxExpression& expression, NamespaceId scope) const
{
	PA8Value result;
	switch (expression.kind)
	{
	case CppSyntaxExpressionKind::True:
		result.type = fundamental(FundamentalType::Bool);
		result.constant = true;
		result.bytes.push_back(1);
		return result;
	case CppSyntaxExpressionKind::False:
		result.type = fundamental(FundamentalType::Bool);
		result.constant = true;
		result.bytes.push_back(0);
		return result;
	case CppSyntaxExpressionKind::Nullptr:
		result.type = fundamental(FundamentalType::NullptrT);
		result.constant = true;
		result.null_pointer = true;
		result.bytes.assign(8, 0);
		return result;
	case CppSyntaxExpressionKind::Literal:
		return literal_value(expression.literal);
	case CppSyntaxExpressionKind::Id:
		break;
	}
	QualifiedName path = const_cast<PA8ProgramModel::Impl*>(this)->
		qualified_name(expression.id);
	const std::vector<EntityId> found = lookup_entities(path, scope);
	if (found.size() != 1)
		throw std::runtime_error("ambiguous or unresolved PA8 expression id");
	const EntityId entity = found[0];
	const EntityRecord& record = entities[entity.value];
	result.entity = entity;
	result.type = record.type;
	if (record.kind == EntityKind::Function)
	{
		result.lvalue = true;
		return result;
	}
	result.lvalue = true;
	TypeId natural = record.type;
	if (type_kind(natural) == TypeKind::LvalueReference ||
		type_kind(natural) == TypeKind::RvalueReference)
		result.type = types[natural.value].key.child;
	if (record.has_constant)
	{
		result.constant = true;
		result.bytes = record.constant_bytes;
	}
	return result;
}

bool PA8ProgramModel::Impl::convert_numeric(TypeId destination,
	const PA8Value& source, std::vector<std::uint8_t>* bytes) const
{
	if (!source.constant || source.element_count != 0 ||
		(!is_integral(source.type) && !is_floating(source.type)))
		return false;
	destination = remove_top_cv(destination);
	if (type_kind(destination) != TypeKind::Fundamental ||
		(!is_integral(destination) && !is_floating(destination)))
		return false;
	const Layout destination_layout = layout(destination);
	bytes->assign(destination_layout.size, 0);
	const FundamentalType destination_type =
		types[destination.value].key.fundamental;
	if (destination_type == FundamentalType::Bool)
	{
		(*bytes)[0] = numeric_value(source) == 0.0L ? 0 : 1;
		return true;
	}
	if (is_floating(destination))
	{
		const long double value = numeric_value(source);
		if (destination_type == FundamentalType::Float)
		{
			const float converted = static_cast<float>(value);
			std::memcpy(&(*bytes)[0], &converted, sizeof(converted));
		}
		else if (destination_type == FundamentalType::Double)
		{
			const double converted = static_cast<double>(value);
			std::memcpy(&(*bytes)[0], &converted, sizeof(converted));
		}
		else
		{
			const long double converted = value;
			if (bytes->size() != sizeof(converted))
				return false;
			std::memcpy(&(*bytes)[0], &converted, sizeof(converted));
		}
		return true;
	}
	std::uint64_t value;
	if (is_floating(source.type))
		value = static_cast<std::uint64_t>(numeric_value(source));
	else
		value = unsigned_numeric_value(source);
	for (std::size_t i = 0; i < bytes->size(); ++i)
		(*bytes)[i] = static_cast<std::uint8_t>(value >> (i * 8));
	return true;
}

bool PA8ProgramModel::Impl::convert_value(TypeId destination,
	const PA8Value& source, std::vector<std::uint8_t>* bytes,
	EntityId* relocation, std::ptrdiff_t* addend) const
{
	*relocation = EntityId();
	*addend = 0;
	destination = remove_top_cv(destination);
	if (type_kind(destination) == TypeKind::Array)
	{
		if (!source.constant || source.element_count == 0)
			return false;
		const TypeKey& array_key = types[destination.value].key;
		TypeId element_type = remove_top_cv(array_key.child);
		if (type_kind(element_type) != TypeKind::Fundamental)
			return false;
		const Layout element_layout = layout(element_type);
		if (!array_key.unknown_bound && array_key.bound < source.element_count)
			return false;
		const std::size_t count = array_key.unknown_bound ? source.element_count :
			array_key.bound;
		if (element_layout.size != 0 && count >
			std::numeric_limits<std::size_t>::max() / element_layout.size)
			return false;
		bytes->assign(count * element_layout.size, 0);
		const Layout source_layout = layout(source.type);
		if (source_layout.size == 0)
			return false;
		for (std::size_t i = 0; i < source.element_count; ++i)
		{
			PA8Value element = source;
			element.element_count = 0;
			element.bytes.assign(source.bytes.begin() +
				i * source_layout.size, source.bytes.begin() +
				(i + 1) * source_layout.size);
			std::vector<std::uint8_t> converted;
			if (!convert_numeric(element_type, element, &converted) ||
				converted.size() != element_layout.size)
				return false;
			std::copy(converted.begin(), converted.end(), bytes->begin() +
				i * element_layout.size);
		}
		return true;
	}
	if (type_kind(destination) == TypeKind::Pointer)
	{
		const TypeKey& pointer_key = types[destination.value].key;
		if (source.null_pointer)
		{
			bytes->assign(8, 0);
			return true;
		}
		if (source.constant && is_integral(source.type) &&
			unsigned_numeric_value(source) == 0)
		{
			bytes->assign(8, 0);
			return true;
		}
		if (source.entity.valid() && source.entity.value < entities.size() &&
			entities[source.entity.value].kind == EntityKind::Function &&
			type_kind(source.type) == TypeKind::Function &&
			type_kind(pointer_key.child) == TypeKind::Function)
		{
			bytes->assign(8, 0);
			*relocation = source.entity;
			return true;
		}
		if (type_kind(source.type) == TypeKind::Pointer && source.constant)
		{
			bytes->assign(8, 0);
			if (source.bytes.size() == 8)
				*bytes = source.bytes;
			return source.bytes.size() == 8;
		}
		return false;
	}
	if (type_kind(destination) == TypeKind::LvalueReference ||
		type_kind(destination) == TypeKind::RvalueReference)
	{
		if (!source.lvalue || !source.entity.valid())
			return false;
		bytes->assign(8, 0);
		*relocation = source.entity;
		return true;
	}
	return convert_numeric(destination, source, bytes);
}

EntityId PA8ProgramModel::Impl::declare_entity(NamespaceId scope,
	NameId name, TypeId type, const PA8BaseSpec& spec,
	bool function_definition, const CppSyntaxDeclarator& source)
{
	if (!scope.valid() || scope.value >= namespaces.size())
		throw std::runtime_error("invalid PA8 declaration scope");
	const SourceNameKey source_key(name, current_translation_unit);
	const NamespaceId* named_namespace =
		namespaces[scope.value].named_children.find(name);
	if ((named_namespace != NULL && namespace_visible(*named_namespace)) ||
		namespaces[scope.value].namespace_aliases.find(source_key) != NULL ||
		namespaces[scope.value].aliases.find(source_key) != NULL)
		throw std::runtime_error("PA8 entity conflicts with namespace or type");

	const TypeKind raw_kind = type_kind(type);
	const bool is_function = raw_kind == TypeKind::Function;
	if (function_definition && !is_function)
		throw std::runtime_error("PA8 function definition has non-function type");
	if (spec.is_inline && !is_function)
		throw std::runtime_error("PA8 inline specifier on non-function");
	if (spec.is_thread_local && is_function)
		throw std::runtime_error("PA8 thread-local function");
	if (is_function && source.has_initializer)
		throw std::runtime_error("PA8 function initializer");

	const bool const_object = spec.is_constexpr || is_const_qualified(type);
	bool internal = namespaces[scope.value].internal_scope ||
		spec.is_static || (!spec.is_extern && !is_function && const_object);
	std::vector<EntityId> same_name = link_candidates(scope, name);
	for (std::vector<EntityId>::const_iterator it = same_name.begin();
		it != same_name.end(); ++it)
	{
		const EntityRecord& candidate = entities[it->value];
		if (candidate.last_declaration_translation_unit ==
			current_translation_unit &&
			candidate.kind == (is_function ? EntityKind::Function :
				EntityKind::Variable) && candidate.type.value == type.value)
		{
			if (spec.is_static && !candidate.internal_linkage)
				throw std::runtime_error("inconsistent PA8 entity linkage");
			// A declaration owner supplies the linkage for later declarations
			// of the same entity.  This includes a no-storage-class function
			// declaration following static, and avoids inventing a second
			// cross-linkage candidate for the valid direction.
			internal = candidate.internal_linkage;
			break;
		}
	}
	EntityId matching;
	for (std::vector<EntityId>::const_iterator it = same_name.begin();
		it != same_name.end(); ++it)
	{
		const EntityRecord& candidate = entities[it->value];
		const bool same_linkage = candidate.internal_linkage == internal &&
			(!internal || candidate.translation_unit == current_translation_unit);
		if (!same_linkage)
		{
			if (candidate.kind != (is_function ? EntityKind::Function :
				EntityKind::Variable) &&
				(candidate.last_declaration_translation_unit ==
					current_translation_unit ||
					(!candidate.internal_linkage && !internal)))
				throw std::runtime_error("PA8 declaration kind conflict");
			continue;
		}
		if (candidate.kind != (is_function ? EntityKind::Function :
			EntityKind::Variable))
			throw std::runtime_error("PA8 declaration kind conflict");
		if (candidate.type.value != type.value)
		{
			if (!is_function)
				throw std::runtime_error("incompatible PA8 variable redeclaration");
			continue;
		}
		matching = *it;
		break;
	}
	if (!matching.valid())
	{
		for (std::vector<EntityId>::const_iterator it = same_name.begin();
			it != same_name.end(); ++it)
		{
			const EntityRecord& candidate = entities[it->value];
			if (candidate.kind != (is_function ? EntityKind::Function :
				EntityKind::Variable))
				throw std::runtime_error("PA8 declaration kind conflict");
			if (!is_function && candidate.last_declaration_translation_unit ==
				current_translation_unit)
				throw std::runtime_error("incompatible PA8 variable linkage");
		}
	}

	EntityRecord* record = NULL;
	EntityId entity;
	if (matching.valid())
	{
		entity = matching;
		record = &entities[entity.value];
		if (record->is_thread_local != spec.is_thread_local)
			throw std::runtime_error("incompatible PA8 thread storage duration");
		if (function_definition && record->has_definition)
		{
			if (!(record->is_inline && spec.is_inline &&
				record->definition_translation_unit !=
					current_translation_unit))
				throw std::runtime_error("duplicate PA8 function definition");
		}
		if (!is_function && record->has_definition &&
			(!spec.is_extern || source.has_initializer))
			throw std::runtime_error("duplicate PA8 variable definition");
	}
	else
	{
		entity = EntityId(entities.size());
		EntityRecord fresh;
		fresh.kind = is_function ? EntityKind::Function : EntityKind::Variable;
		fresh.name = name;
		fresh.owner = scope;
		fresh.type = type;
		fresh.translation_unit = current_translation_unit;
		fresh.internal_linkage = internal;
		fresh.is_static = spec.is_static;
		fresh.is_thread_local = spec.is_thread_local;
		fresh.is_extern = spec.is_extern;
		fresh.is_const = const_object;
		fresh.is_constexpr = spec.is_constexpr;
		fresh.is_inline = spec.is_inline;
		entities.push_back(fresh);
		const EntityBucketId link_bucket = internal ?
			ensure_internal_entity_bucket(scope, name,
				current_translation_unit) :
			ensure_external_entity_bucket(scope, name);
		entity_buckets[link_bucket.value].push_back(entity);
		if (is_function)
			namespaces[scope.value].functions.push_back(entity);
		else
			namespaces[scope.value].variables.push_back(entity);
		record = &entities.back();
	}
	mark_entity_declaration(entity);
	invalidate(LookupCategory::Entity);

	if (is_function)
	{
		record->is_inline = record->is_inline || spec.is_inline;
		if (function_definition)
		{
			record->has_definition = true;
			record->definition_translation_unit = current_translation_unit;
		}
		return entity;
	}

	bool definition = !spec.is_extern || source.has_initializer;
	if (spec.is_constexpr && !source.has_initializer)
		throw std::runtime_error("constexpr PA8 variable needs initializer");
	if (definition && const_object && !source.has_initializer)
		throw std::runtime_error("const PA8 variable needs initializer");
	if (definition)
	{
		try
		{
			(void)layout(type);
		}
		catch (const std::exception&)
		{
			throw std::runtime_error("incomplete PA8 variable definition");
		}
	}

	if (source.has_initializer)
	{
		PA8Value value = evaluate(source.initializer, scope);
		// An unknown-bound array obtains its bound from a string literal before
		// conversion and layout.  This is still a typed LiteralData path.
		if (type_kind(type) == TypeKind::Array && value.element_count != 0 &&
			type_kind(type) == TypeKind::Array &&
			types[type.value].key.unknown_bound)
		{
			const TypeKey& key = types[type.value].key;
			type = array(key.child, false, value.element_count);
			record->type = type;
		}
		if (spec.is_constexpr && !value.constant)
			throw std::runtime_error("nonconstant constexpr PA8 initializer");
		std::vector<std::uint8_t> bytes;
		EntityId relocation;
		std::ptrdiff_t addend = 0;
		if (convert_value(type, value, &bytes, &relocation, &addend))
		{
			record->has_constant = value.constant;
			if (record->has_constant)
				record->constant_bytes = bytes;
			record->has_relocation = relocation.valid();
			record->relocation = relocation;
			record->relocation_addend = addend;
		}
		else if (value.constant || spec.is_constexpr)
			throw std::runtime_error("invalid PA8 initializer conversion");
	}
	record->has_definition = record->has_definition || definition;
	record->is_const = record->is_const || const_object;
	record->is_constexpr = record->is_constexpr || spec.is_constexpr;
	return entity;
}

void PA8ProgramModel::Impl::analyze_static_assert(
	const CppSyntaxExpression& expression, NamespaceId scope)
{
	const PA8Value value = evaluate(expression, scope);
	if (!value.constant || value.element_count != 0 ||
		(!is_integral(value.type) && !is_floating(value.type) &&
		!value.null_pointer))
		throw std::runtime_error("PA8 static_assert is not constant");
	if (value.null_pointer || value.bytes.empty())
		throw std::runtime_error("PA8 static_assert failed");
	bool nonzero = false;
	for (std::vector<std::uint8_t>::const_iterator it = value.bytes.begin();
		it != value.bytes.end(); ++it)
		if (*it != 0)
			nonzero = true;
	if (!nonzero)
		throw std::runtime_error("PA8 static_assert failed");
}

class PA8SemanticActions : public CppDeclarationSyntaxConsumer
{
public:
	PA8SemanticActions(PA8ProgramModel::Impl& model, std::size_t translation_unit)
		: model_(model), current_(model.global), namespace_parents_(),
		  active_spec_(), declaration_active_(false),
		  translation_unit_(translation_unit)
	{}

	bool accept_type_name(const CppSyntaxQualifiedName& name) const
	{
		return model_.accepts_type_name(name, current_);
	}

	bool accept_namespace_name(const CppSyntaxQualifiedName& name) const
	{
		return model_.accepts_namespace_name(name, current_);
	}

	bool accept_nested_name_specifier(
		const CppSyntaxQualifiedName& name) const
	{
		return model_.accepts_nested_name_specifier(name, current_);
	}

	void on_empty_declaration()
	{
	}

	void on_namespace_begin(bool inline_namespace, bool anonymous_namespace,
		PPSpellingId name)
	{
		NamespaceId child;
		if (anonymous_namespace)
			child = model_.anonymous_namespace(current_, inline_namespace);
		else
			child = model_.named_namespace(current_, model_.intern_name(name),
				inline_namespace);
		namespace_parents_.push_back(current_);
		current_ = child;
	}

	void on_namespace_end()
	{
		if (namespace_parents_.empty())
			throw std::runtime_error("PA8 namespace action underflow");
		current_ = namespace_parents_.back();
		namespace_parents_.pop_back();
	}

	void on_namespace_alias(PPSpellingId name,
		const CppSyntaxQualifiedName& target)
	{
		model_.declare_namespace_alias(current_, model_.intern_name(name),
			model_.resolve_namespace_path(model_.qualified_name(target), current_));
	}

	void on_using_directive(const CppSyntaxQualifiedName& target)
	{
		model_.add_using_directive(current_,
			model_.resolve_namespace_path(model_.qualified_name(target), current_));
	}

	void on_using_declaration(const CppSyntaxQualifiedName& source)
	{
		QualifiedName path = model_.qualified_name(source);
		TypeId type;
		if (model_.lookup_type(path, current_, &type))
		{
			model_.add_using_type(current_, path.last(), type);
			return;
		}
		const std::vector<EntityId> entities =
			model_.lookup_entities(path, current_);
		if (entities.empty())
		{
			NamespaceId namespace_id;
			if (model_.lookup_namespace(path, current_, &namespace_id))
				throw std::runtime_error("using declaration names a namespace");
			throw std::runtime_error("unresolved PA8 using declaration");
		}
		for (std::vector<EntityId>::const_iterator it = entities.begin();
			it != entities.end(); ++it)
			model_.add_using_entity(current_, path.last(), *it);
	}

	void on_alias_declaration(PPSpellingId name,
		const CppSyntaxTypeId& source)
	{
		model_.declare_type_alias(current_, model_.intern_name(name),
			model_.type_id(source, current_));
	}

	void on_simple_declaration_begin(const CppSyntaxDeclSpec& source)
	{
		if (declaration_active_)
			throw std::runtime_error("nested PA8 declaration action");
		active_spec_ = model_.declaration_spec(source, current_);
		declaration_active_ = true;
	}

	void on_simple_declarator(const CppSyntaxDeclarator& source)
	{
		if (!declaration_active_)
			throw std::runtime_error("PA8 declaration action without begin");
		process_declarator(source, false);
	}

	void on_simple_declaration_end()
	{
		if (!declaration_active_)
			throw std::runtime_error("PA8 declaration action without end");
		declaration_active_ = false;
	}

	void on_function_definition(const CppSyntaxDeclSpec& spec,
		const CppSyntaxDeclarator& source)
	{
		if (declaration_active_)
			throw std::runtime_error("PA8 function definition in declaration");
		active_spec_ = model_.declaration_spec(spec, current_);
		if (active_spec_.is_typedef)
			throw std::runtime_error("PA8 typedef function definition");
		process_declarator(source, true);
	}

	void on_static_assert_declaration(
		const CppSyntaxExpression& expression, const LiteralData& message)
	{
		(void)message;
		model_.analyze_static_assert(expression, current_);
	}

private:
	PA8ProgramModel::Impl& model_;
	NamespaceId current_;
	std::vector<NamespaceId> namespace_parents_;
	PA8BaseSpec active_spec_;
	bool declaration_active_;
	std::size_t translation_unit_;

	void process_declarator(const CppSyntaxDeclarator& source,
		bool function_definition)
	{
		DeclaratorShape shape = model_.declarator(source, current_);
		if (!shape.has_name || shape.name.components.empty())
			throw std::runtime_error("unnamed PA8 declaration");
		const TypeId type = model_.apply_shape(active_spec_.resolved_type,
			shape);
		NamespaceId target = current_;
		const bool qualified = shape.name.global ||
			shape.name.components.size() > 1;
		if (qualified)
		{
			QualifiedName prefix = shape.name;
			prefix.components.pop_back();
			if (prefix.components.empty() && prefix.global)
				target = model_.global;
			else
				target = model_.resolve_namespace_path(prefix, current_);
			if (!model_.is_enclosing_namespace(current_, target))
				throw std::runtime_error(
					"PA8 qualified declaration is not in an enclosing namespace");
		}
		const NameId name = shape.name.last();
		if (active_spec_.is_typedef)
		{
			if (source.has_initializer || function_definition)
				throw std::runtime_error("invalid PA8 typedef declaration");
			model_.declare_type_alias(target, name, type);
		}
		else
			model_.declare_entity(target, name, type, active_spec_,
				function_definition, source);
	}
};

void PA8ProgramModel::Impl::analyze_translation_unit(
	const PPTokenBuffer& tokens, std::size_t translation_unit)
{
	begin_translation_unit(&tokens, translation_unit);
	CppSyntaxTokenCollector collector;
	posttokenize_cpp_tokens(tokens, collector);
	if (collector.invalid || collector.tokens.empty() ||
		collector.tokens.back().kind != CppSyntaxTokenKind::End)
		throw std::runtime_error("invalid PA8 posttoken stream");
	PA8SemanticActions actions(*this, translation_unit);
	CppDeclarationSyntaxParser parser(collector.tokens, actions);
	parser.parse();
	end_translation_unit();
}

void PA8ProgramModel::Impl::build_image(std::vector<char>& image) const
{
	image.clear();
	image.push_back('P');
	image.push_back('A');
	image.push_back('8');
	image.push_back('\0');
	std::vector<std::size_t> offsets(entities.size(),
		std::numeric_limits<std::size_t>::max());
	for (std::size_t i = 0; i < entities.size(); ++i)
	{
		const EntityRecord& entity = entities[i];
		const bool emit = entity.kind == EntityKind::Function ||
			entity.has_definition;
		if (!emit)
			continue;
		const Layout object = layout(entity.type);
		if (object.alignment == 0)
			throw std::runtime_error("invalid PA8 alignment");
		const std::size_t remainder = image.size() % object.alignment;
		if (remainder != 0)
			image.resize(image.size() + object.alignment - remainder, 0);
		offsets[i] = image.size();
		if (entity.kind == EntityKind::Function)
		{
			image.push_back('f');
			image.push_back('u');
			image.push_back('n');
			image.push_back('\0');
			continue;
		}
		std::vector<std::uint8_t> bytes(object.size, 0);
		if (entity.has_constant)
		{
			if (entity.constant_bytes.size() != object.size)
				throw std::runtime_error("PA8 constant size mismatch");
			bytes = entity.constant_bytes;
		}
		for (std::vector<std::uint8_t>::const_iterator it = bytes.begin();
			it != bytes.end(); ++it)
			image.push_back(static_cast<char>(*it));
	}
	for (std::size_t i = 0; i < entities.size(); ++i)
	{
		const EntityRecord& entity = entities[i];
		if (!entity.has_relocation || offsets[i] ==
			std::numeric_limits<std::size_t>::max())
			continue;
		if (!entity.relocation.valid() || entity.relocation.value >= offsets.size() ||
			offsets[entity.relocation.value] ==
				std::numeric_limits<std::size_t>::max())
			throw std::runtime_error("unresolved PA8 relocation");
		const std::ptrdiff_t target = static_cast<std::ptrdiff_t>(
			offsets[entity.relocation.value]) + entity.relocation_addend;
		if (target < 0 || static_cast<std::uint64_t>(target) >
			std::numeric_limits<std::uint64_t>::max())
			throw std::runtime_error("PA8 relocation overflow");
		std::uint64_t value = static_cast<std::uint64_t>(target);
		const Layout object = layout(entity.type);
		if (object.size < 8 || offsets[i] + 8 > image.size())
			throw std::runtime_error("invalid PA8 relocation target");
		for (std::size_t byte = 0; byte < 8; ++byte)
		{
			image[offsets[i] + byte] =
				static_cast<char>(value & static_cast<std::uint64_t>(0xff));
			value >>= 8;
		}
	}
}

PA8ProgramModel::PA8ProgramModel() : impl_(new Impl())
{}

PA8ProgramModel::~PA8ProgramModel()
{
	delete impl_;
}

void PA8ProgramModel::add_translation_unit(const PPTokenBuffer& tokens)
{
	impl_->analyze_translation_unit(tokens, impl_->next_translation_unit);
	++impl_->next_translation_unit;
}

void PA8ProgramModel::build_image(std::vector<char>& image) const
{
	impl_->build_image(image);
}
