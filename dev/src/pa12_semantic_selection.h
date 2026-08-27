#pragma once

#include <vector>

#include "pa11_semantic_storage.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

struct ValueRef
{
	ScopeId scope;
	BindingId binding;

	ValueRef(ScopeId scope = ScopeId(), BindingId binding = BindingId())
		: scope(scope), binding(binding)
	{}
};

enum class ConstructorInitializationContext
{
	Direct,
	Copy,
	CopyList
};

enum class SemanticValueCategory
{
	Lvalue,
	Prvalue,
	Xvalue
};

enum class ConversionKind
{
	Identity,
	LvalueToRvalue,
	Integral,
	PointerQualification,
	PointerToVoid,
	NullptrToPointer,
	NullIntegerToPointer,
	NullptrToBool,
	NullIntegerToNullptr,
	ArrayToPointer,
	FunctionToPointer,
	ReferenceBinding,
	PointerToBool,
	Floating,
	Reinterpret,
	ToVoid
};

struct ConversionChoice
{
	bool valid;
	unsigned int rank;
	ConversionKind kind;

	ConversionChoice(bool valid = false, unsigned int rank = 0,
		ConversionKind kind = ConversionKind::Identity)
		: valid(valid), rank(rank), kind(kind)
	{}
};

struct FunctionIdResolution
{
	bool valid;
	ValueRef selected;
	ConversionChoice conversion;

	FunctionIdResolution(bool valid = false, ValueRef selected = ValueRef(),
		ConversionChoice conversion = ConversionChoice())
		: valid(valid), selected(selected), conversion(conversion)
	{}
};

struct ExprInfo
{
	SemanticFactId fact;
	TypeId type;
	SemanticValueCategory category;
	bool integer_zero;

	ExprInfo(SemanticFactId fact = SemanticFactId(), TypeId type = TypeId(),
		SemanticValueCategory category = SemanticValueCategory::Prvalue,
		bool integer_zero = false)
		: fact(fact), type(type), category(category), integer_zero(integer_zero)
	{}
};

struct TypedFunctionSelection
{
	ValueRef selected;
	TypeId type;
	std::vector<ExprInfo> arguments;

	TypedFunctionSelection(ValueRef selected = ValueRef(),
		TypeId type = TypeId())
		: selected(selected), type(type), arguments()
	{}

	bool valid() const { return selected.binding.valid() && type.valid(); }
};

struct ConstructorSelection
{
	BindingId binding;
	ScopeId scope;
	TypeId callable_type;
	std::vector<SemanticFactId> arguments;

	ConstructorSelection(BindingId binding = BindingId(),
		ScopeId scope = ScopeId(), TypeId callable_type = TypeId())
		: binding(binding), scope(scope), callable_type(callable_type),
		  arguments()
	{}

	bool valid() const
	{
		return binding.valid() && scope.valid() && callable_type.valid();
	}
};

}
