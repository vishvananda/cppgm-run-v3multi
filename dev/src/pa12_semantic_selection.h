#pragma once

#include <vector>

#include "pa11_semantic_storage.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

enum class MemberAccess;

struct SourcePoint
{
	std::size_t value;

	explicit SourcePoint(std::size_t value = InvalidIdentityValue)
		: value(value)
	{}

	bool valid() const { return value != InvalidIdentityValue; }
};

struct HiddenFriendBindingRelation
{
	ScopeId namespace_scope;
	BindingId binding;

	HiddenFriendBindingRelation(ScopeId namespace_scope = ScopeId(),
		BindingId binding = BindingId())
		: namespace_scope(namespace_scope), binding(binding)
	{}
};

struct HiddenFriendBindingKey
{
	ScopeId namespace_scope;
	NameId name;

	HiddenFriendBindingKey(ScopeId namespace_scope = ScopeId(),
		NameId name = NameId())
		: namespace_scope(namespace_scope), name(name)
	{}

	bool operator==(const HiddenFriendBindingKey& other) const
	{
		return namespace_scope == other.namespace_scope && name == other.name;
	}
};

struct HiddenFriendBindingKeyHash
{
	std::size_t operator()(const HiddenFriendBindingKey& key) const
	{
		std::size_t result = key.namespace_scope.value;
		result ^= result >> 17;
		result *= static_cast<std::size_t>(0xed5ad4bbU);
		result ^= key.name.value + static_cast<std::size_t>(0x9e3779b9U) +
			(result << 6) + (result >> 2);
		return result;
	}
};

struct HiddenFriendFunctionRelation
{
	BindingId binding;
	SourcePoint declaration_point;

	HiddenFriendFunctionRelation(BindingId binding = BindingId(),
		SourcePoint declaration_point = SourcePoint())
		: binding(binding), declaration_point(declaration_point)
	{}
};

struct ValueRef
{
	ScopeId scope;
	BindingId binding;
	// A using-declaration may publish a canonical member through a different
	// access view.  Keep that view beside the typed candidate; the binding's
	// declared access remains canonical in its PA11 sidecar.
	bool has_access_override;
	MemberAccess access_override;
	ScopeId access_view_owner;

	ValueRef(ScopeId scope = ScopeId(), BindingId binding = BindingId(),
		bool has_access_override = false,
		MemberAccess access_override = MemberAccess(),
		ScopeId access_view_owner = ScopeId())
		: scope(scope), binding(binding),
		  has_access_override(has_access_override),
		  access_override(access_override), access_view_owner(access_view_owner)
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
	DerivedToBase,
	LvalueToRvalue,
	ClassValue,
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

// PA12 records whether a canonical truth value must first cross the semantic
// bool representation.  The disposition is typed conversion data so PA15
// does not infer it from a block producer or rendered source.
enum class CanonicalTruthPolicy
{
	Materialize,
	Preserve
};

// Keep the standard-conversion rank category separate from the typed
// derived-to-base secondary ordering.  In particular, a base-path length is
// not a rank band: it is only meaningful after the standard category has
// been compared.
enum class ConversionRankCategory
{
	Exact,
	Promotion,
	Conversion,
	UserDefined,
	Ellipsis
};

inline ConversionRankCategory conversion_rank_category(ConversionKind kind,
	unsigned int rank)
{
	switch (kind)
	{
	case ConversionKind::Identity:
	case ConversionKind::LvalueToRvalue:
	case ConversionKind::ClassValue:
	case ConversionKind::PointerQualification:
	case ConversionKind::ArrayToPointer:
	case ConversionKind::FunctionToPointer:
		return ConversionRankCategory::Exact;
	case ConversionKind::Integral:
		return rank <= 1 ? ConversionRankCategory::Promotion :
			ConversionRankCategory::Conversion;
	case ConversionKind::ReferenceBinding:
		return rank == 0 ? ConversionRankCategory::Exact :
			ConversionRankCategory::Conversion;
	default:
		return rank == 0 ? ConversionRankCategory::Exact :
			ConversionRankCategory::Conversion;
	}
}

struct ConversionChoice
{
	bool valid;
	unsigned int rank;
	ConversionKind kind;
	ConversionRankCategory rank_category;
	unsigned int base_distance;
	unsigned int added_cv;
	bool base_access_checked;
	TypeId base_source;
	TypeId base_target;
	ScopeId base_access_scope;

	ConversionChoice(bool valid = false, unsigned int rank = 0,
		ConversionKind kind = ConversionKind::Identity)
		: valid(valid), rank(rank), kind(kind),
		  rank_category(conversion_rank_category(kind, rank)),
		  base_distance(0), added_cv(0), base_access_checked(false),
		  base_source(), base_target(), base_access_scope()
	{}
};

inline ConversionChoice make_derived_base_choice(
	TypeId source, TypeId target, unsigned int base_distance,
	ScopeId access_scope, unsigned int added_cv = 0)
{
	ConversionChoice result(true, 1, ConversionKind::DerivedToBase);
	result.rank_category = ConversionRankCategory::Conversion;
	result.base_distance = base_distance;
	result.added_cv = added_cv;
	result.base_access_checked = access_scope.valid();
	result.base_source = source;
	result.base_target = target;
	result.base_access_scope = access_scope;
	return result;
}

struct ConversionScore
{
	ConversionRankCategory rank_category;
	ConversionKind kind;
	unsigned int legacy_rank;
	unsigned int base_distance;
	unsigned int added_cv;

	ConversionScore()
		: rank_category(ConversionRankCategory::Exact),
		  kind(ConversionKind::Identity), legacy_rank(0), base_distance(0),
		  added_cv(0)
	{}

	explicit ConversionScore(const ConversionChoice& choice)
		: rank_category(choice.rank_category), kind(choice.kind),
		  legacy_rank(choice.rank), base_distance(choice.base_distance),
		  added_cv(choice.added_cv)
	{}

	static ConversionScore ellipsis_score(unsigned int rank)
	{
		ConversionScore result;
		result.rank_category = ConversionRankCategory::Ellipsis;
		result.kind = ConversionKind::Identity;
		result.legacy_rank = rank;
		return result;
	}
};

// Return -1 when left is better, 1 when right is better, and 0 when the
// sequences are indistinguishable (or intentionally incomparable).  The
// implementations live in the PA12 calls translation unit so this shared
// declaration remains a compact ownership boundary.
int compare_conversion_scores(const ConversionScore& left,
	const ConversionScore& right);

int compare_conversion_choices(const ConversionChoice& left,
	const ConversionChoice& right);

struct ConversionFact
{
	TypeId source;
	TypeId target;
	ConversionKind kind;
	unsigned int rank;
	ConversionRankCategory rank_category;
	unsigned int base_distance;
	unsigned int added_cv;
	bool base_access_checked;
	ScopeId base_access_scope;
	std::size_t base_path_begin;
	std::size_t base_path_count;
	CanonicalTruthPolicy canonical_truth_policy;
	ConversionFact(TypeId source = TypeId(), TypeId target = TypeId(),
		ConversionKind kind = ConversionKind::Identity, unsigned int rank = 0)
		: source(source), target(target), kind(kind), rank(rank),
		  rank_category(conversion_rank_category(kind, rank)),
		  base_distance(0), added_cv(0), base_access_checked(false),
		  base_access_scope(),
		  base_path_begin(InvalidIdentityValue), base_path_count(0),
		  canonical_truth_policy(CanonicalTruthPolicy::Materialize)
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

// An overloaded operator has the same typed call boundary as an ordinary
// direct call, with the additional fact that a member candidate consumes the
// left/sole operand as its implicit object.  Keep that distinction explicit
// until the CallExpression is formed; lowering must not infer it from the
// operator's rendered name.
struct TypedOperatorSelection
{
	ValueRef selected;
	TypeId type;
	bool member;
	std::vector<ExprInfo> arguments;

	TypedOperatorSelection(ValueRef selected = ValueRef(),
		TypeId type = TypeId(), bool member = false)
		: selected(selected), type(type), member(member), arguments()
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
