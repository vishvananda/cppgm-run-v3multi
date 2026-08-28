#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "pa10_ast.h"
#include "posttoken.h"
#include "pa11_semantic_storage.h"
#include "pa12_semantic_selection.h"

namespace lowir_model
{
struct Program;
}

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

class Pa15Lowerer;

enum class TypeKind
{
	Fundamental,
	Named,
	Cv,
	Pointer,
	MemberPointer,
	LvalueReference,
	RvalueReference,
	Array,
	Function
};

enum class NamedKind
{
	Class,
	Enum,
	TemplateParameter
};

enum class ClassTag
{
	Struct,
	Class,
	Union
};

enum class MemberAccess
{
	Public,
	Protected,
	Private
};

struct TypeKey
{
	TypeKind kind;
	FundamentalType fundamental;
	TypeId child;
	unsigned int cv;
	bool unknown_bound;
	ArrayBound bound;
	NamedRecordId named;
	TypeId result;
	std::vector<TypeId> parameters;
	bool variadic;

	TypeKey()
		: kind(TypeKind::Fundamental), fundamental(FundamentalType::Int),
		  child(), cv(0), unknown_bound(false), bound(),
		  named(), result(), parameters(), variadic(false)
	{}

	bool operator==(const TypeKey& other) const
	{
		return kind == other.kind && fundamental == other.fundamental &&
			child == other.child && cv == other.cv &&
			unknown_bound == other.unknown_bound && bound == other.bound &&
			named == other.named && result == other.result &&
			parameters == other.parameters && variadic == other.variadic;
	}
};

struct TypeKeyHash
{
	static std::size_t combine(std::size_t seed, std::size_t value)
	{
		value += static_cast<std::size_t>(0x9e3779b9U) +
			(seed << 6) + (seed >> 2);
		return seed ^ value;
	}

	std::size_t operator()(const TypeKey& key) const
	{
		std::size_t result = static_cast<std::size_t>(key.kind);
		result = combine(result, static_cast<std::size_t>(key.fundamental));
		result = combine(result, key.child.value);
		result = combine(result, key.cv);
		result = combine(result, key.unknown_bound ? 1 : 0);
		result = combine(result, key.bound.value);
		result = combine(result, key.named.value);
		result = combine(result, key.result.value);
		result = combine(result, key.variadic ? 1 : 0);
		result = combine(result, key.parameters.size());
		for (std::size_t i = 0; i < key.parameters.size(); ++i)
			result = combine(result, key.parameters[i].value);
		return result;
	}
};

struct NamePath
{
	bool global;
	std::vector<NameId> components;

	NamePath() : global(false), components() {}

	bool empty() const { return components.empty(); }
	NameId last() const
	{
		return components.empty() ? NameId() : components.back();
	}
};

struct TemplateFunctionFact
{
	ScopeId visible_scope;
	BindingId binding;
	NameId name;
	std::vector<NamedRecordId> parameters;

	TemplateFunctionFact(ScopeId visible_scope = ScopeId(),
		BindingId binding = BindingId(), NameId name = NameId())
		: visible_scope(visible_scope), binding(binding), name(name), parameters()
	{}
};

struct TemplateFunctionList
{
	std::vector<TemplateFunctionId> entries;
};

enum class TemplateSpecializationState
{
	NotStarted,
	InProgress,
	Complete,
	Failed
};

struct TemplateSpecializationKey
{
	TemplateFunctionId function;
	std::vector<TypeId> arguments;

	TemplateSpecializationKey(TemplateFunctionId function =
		TemplateFunctionId(), const std::vector<TypeId>& arguments =
		std::vector<TypeId>())
		: function(function), arguments(arguments)
	{}

	bool operator==(const TemplateSpecializationKey& other) const
	{
		return function == other.function && arguments == other.arguments;
	}
};

struct TemplateSpecializationKeyHash
{
	static std::size_t combine(std::size_t seed, std::size_t value)
	{
		value += static_cast<std::size_t>(0x9e3779b9U) +
			(seed << 6) + (seed >> 2);
		return seed ^ value;
	}

	std::size_t operator()(const TemplateSpecializationKey& key) const
	{
		std::size_t result = key.function.value;
		result = combine(result, key.arguments.size());
		for (std::size_t i = 0; i < key.arguments.size(); ++i)
			result = combine(result, key.arguments[i].value);
		return result;
	}
};

struct TemplateSpecializationFact
{
	TemplateFunctionId function;
	BindingId binding;
	std::vector<TypeId> arguments;
	TemplateSpecializationState state;

	TemplateSpecializationFact(TemplateFunctionId function =
		TemplateFunctionId(), BindingId binding = BindingId())
		: function(function), binding(binding), arguments(),
		  state(TemplateSpecializationState::NotStarted)
	{}
};

enum class BindingKind
{
	Type,
	TypeAlias,
	Function,
	Variable,
	Parameter,
	Enumerator
};

// These are source-owned linkage/storage facts.  They are kept on the
// canonical binding rather than recovered from a rendered declaration when
// PA15 chooses LowIR metadata.
enum class LanguageLinkage
{
	Cxx,
	C
};

struct Binding
{
	BindingKind kind;
	NameId name;
	TypeId type;
	bool has_tag;
	ClassTag class_tag;
	std::vector<ClassTag> declaration_tags;
	bool has_value;
	std::int64_t value;
	std::uint64_t value_bits;
	bool value_unsigned;
	bool has_definition;
	LanguageLinkage language_linkage;
	bool internal_linkage;

	Binding(BindingKind kind = BindingKind::Variable, NameId name = NameId(),
		TypeId type = TypeId())
		: kind(kind), name(name), type(type), has_tag(false),
		  class_tag(ClassTag::Struct), declaration_tags(),
		  has_value(false), value(0), value_bits(0), value_unsigned(false),
		  has_definition(false),
		  language_linkage(LanguageLinkage::Cxx), internal_linkage(false)
	{}
};

// A function's deleted/defaulted status is a declaration fact, not a
// property of its rendered name.  PA11 merges this fact onto the canonical
// binding; PA12 consumes it when it builds a call boundary.
enum class FunctionDeclarationKind
{
	Normal,
	Defaulted,
	Deleted
};

// Rare anonymous-union and synthetic-function relations live beside the
// canonical binding table instead of enlarging every Binding.
struct BindingSidecar
{
	BindingId backing_storage;
	NamedRecordId constructor_record; NamedRecordId destructor_record;
	NamedRecordId generated_name_record;
	bool hidden_friend; std::vector<NamedRecordId> friend_records;
	MemberAccess member_access;
	bool static_member;
	bool has_default_member_initializer;
	SemanticFactId default_member_initializer;
	// Non-static member alignment is retained on the canonical binding until
	// RecordLayout consumes it; static members never participate in layout.
	bool has_requested_alignment;
	std::size_t requested_alignment;
	// Operator identity and exception behavior are declaration facts.  Keep
	// them beside the canonical binding so ABI/lowering consumers do not have
	// to recover either fact from a rendered name or a declarator body.
	PA10OperatorFunctionKind operator_function_kind;
	SimpleTokenType operator_token;
	bool nonthrowing;
	bool explicit_constructor;
	FunctionDeclarationKind declaration_kind;
	TemplateSpecializationId template_specialization;
	TypeId unadjusted_type;

	BindingSidecar(BindingId backing_storage = BindingId(),
		NamedRecordId constructor_record = NamedRecordId(),
		NamedRecordId generated_name_record = NamedRecordId())
		: backing_storage(backing_storage), constructor_record(constructor_record), destructor_record(),
		  generated_name_record(generated_name_record),
		  hidden_friend(false), friend_records(),
		  member_access(MemberAccess::Public), static_member(false),
		  has_default_member_initializer(false),
		  default_member_initializer(),
		  has_requested_alignment(false), requested_alignment(0),
		  operator_function_kind(PA10OperatorFunctionKind::None),
		  operator_token(SimpleTokenType::OP_SEMICOLON), nonthrowing(false),
		  explicit_constructor(false),
		  declaration_kind(FunctionDeclarationKind::Normal),
		  template_specialization(), unadjusted_type()
	{}
};

struct FriendLexicalScopeRelation
{
	ScopeId class_scope; NamedRecordId class_record;
	FriendLexicalScopeRelation(ScopeId s = ScopeId(), NamedRecordId r = NamedRecordId()) : class_scope(s), class_record(r) {}
};
struct ValueEntry
{
	BindingId binding;
	// A using-declaration retains the canonical source scope beside the
	// binding identity; the pair cannot become length-mismatched.
	ScopeId origin;
	SourcePoint declaration_point;

	ValueEntry(BindingId binding = BindingId(), ScopeId origin = ScopeId(),
		SourcePoint declaration_point = SourcePoint())
		: binding(binding), origin(origin), declaration_point(declaration_point)
	{}
};

struct ValueList
{
	std::vector<ValueEntry> entries;
};

struct UsingDirectiveRelation
{
	ScopeId target;
	SourcePoint declaration_point;

	UsingDirectiveRelation(ScopeId target = ScopeId(),
		SourcePoint declaration_point = SourcePoint())
		: target(target), declaration_point(declaration_point)
	{}
};

struct EffectiveUsingDirective
{
	ScopeId target;
	ScopeId lexical_scope;
	SourcePoint declaration_point;

	EffectiveUsingDirective(ScopeId target = ScopeId(),
		ScopeId lexical_scope = ScopeId(),
		SourcePoint declaration_point = SourcePoint())
		: target(target), lexical_scope(lexical_scope),
		  declaration_point(declaration_point)
	{}
};

// Namespace aliases are rare declaration-point relations.  Keep their
// source points in a sparse side index rather than enlarging every Scope or
// replacing the canonical NameId -> ScopeId alias map.
struct NamespaceAliasRelation
{
	NameId name;
	SourcePoint declaration_point;

	NamespaceAliasRelation(NameId name = NameId(),
		SourcePoint declaration_point = SourcePoint())
		: name(name), declaration_point(declaration_point)
	{}
};

struct NamespaceAliasList
{
	std::vector<NamespaceAliasRelation> entries;
};

// Namespace-owned type declarations are formed before deferred function-body
// semantics.  Retain their declaration points sparsely so a later typedef,
// alias, class, enum, or using-declaration cannot enter an earlier lookup.
struct TypeDeclarationRelation
{
	NameId name;
	SourcePoint declaration_point;
	BindingId declaration;

	TypeDeclarationRelation(NameId name = NameId(),
		SourcePoint declaration_point = SourcePoint(),
		BindingId declaration = BindingId())
		: name(name), declaration_point(declaration_point),
		  declaration(declaration)
	{}
};

struct TypeDeclarationList
{
	std::vector<TypeDeclarationRelation> entries;
};

// Lookup returns TypeId at the consumer boundary, but ambiguity is decided
// by the canonical declaration identity when one is available.
struct TypeLookupCandidate
{
	TypeId type;
	BindingId declaration;

	TypeLookupCandidate(TypeId type = TypeId(),
		BindingId declaration = BindingId())
		: type(type), declaration(declaration)
	{}

	bool operator==(const TypeLookupCandidate& other) const
	{
		if (declaration.valid() || other.declaration.valid())
			return declaration.valid() && other.declaration.valid() &&
				declaration == other.declaration;
		return type == other.type;
	}
};

enum class ScopeKind
{
	Namespace,
	Class,
	Function,
	Block,
	Enum,
	TemplateParameters
};

struct Scope
{
	ScopeKind kind;
	ScopeId parent;
	NameId name;
	NamedRecordId record;
	// PA12 owns the exact synthetic object parameter for a member function.
	// Consumers must not rediscover it from names or parameter ordering.
	BindingId implicit_object_binding;
	bool inline_namespace;
	std::vector<ScopeId> children;
	std::vector<BindingId> bindings;
	FlatIndex<NameId, TypeId, IdentityHash<NameId> > types;
	FlatIndex<NameId, ScopeId, IdentityHash<NameId> > namespaces;
	FlatIndex<NameId, ScopeId, IdentityHash<NameId> > namespace_aliases;
	FlatIndex<NameId, ValueList, IdentityHash<NameId> > values;
	FlatIndex<NameId, TypeId, IdentityHash<NameId> > using_types;
	std::vector<UsingDirectiveRelation> using_directives;
	// Entries are placed at their common ancestor once, then filtered by the
	// typed lexical owner during an unqualified lookup.
	std::vector<EffectiveUsingDirective> effective_using_directives;
	std::vector<DumpBindingViewId> binding_views;
	std::vector<DumpScopeViewId> scope_views;
	std::size_t creation_order;
	std::size_t depth;

	Scope(ScopeKind kind = ScopeKind::Namespace, ScopeId parent = ScopeId(),
		NameId name = NameId(), NamedRecordId record = NamedRecordId(),
		bool inline_namespace = false, std::size_t creation_order = 0,
		std::size_t depth = 0)
		: kind(kind), parent(parent), name(name), record(record),
		  implicit_object_binding(),
		  inline_namespace(inline_namespace),
		  children(), bindings(),
		  types(), namespaces(), namespace_aliases(), values(), using_types(),
		  using_directives(), effective_using_directives(), binding_views(),
		  scope_views(), creation_order(creation_order), depth(depth)
	{}
};

struct SourceInterval
{
	struct SourceIndex
	{
		std::size_t value;

		explicit SourceIndex(std::size_t value = 0) : value(value) {}
	};

	SourceIndex begin;
	SourceIndex end;

	SourceInterval(std::size_t begin = 0, std::size_t end = 0)
		: begin(begin), end(end)
	{}
};

struct GeneratedOrdinal
{
	std::size_t value;

	explicit GeneratedOrdinal(std::size_t value = 0) : value(value) {}
};

enum class GeneratedEntityKind
{
	AnonymousUnion,
	AnonymousEnum
};

struct GeneratedIdentity
{
	GeneratedEntityKind kind;
	ScopeId owner;
	SourceInterval source;
	GeneratedOrdinal ordinal;

	GeneratedIdentity(GeneratedEntityKind kind =
		GeneratedEntityKind::AnonymousUnion, ScopeId owner = ScopeId(),
		SourceInterval source = SourceInterval(),
		GeneratedOrdinal ordinal = GeneratedOrdinal())
		: kind(kind), owner(owner), source(source), ordinal(ordinal)
	{}
};

struct NamedRecord
{
	NamedKind kind;
	NameId name;
	ScopeId owner;
	bool defined;
	ClassTag class_tag;
	// These typed boundary facts keep the natural-layout checkpoint from
	// silently presenting an inheritance or polymorphic record as flat.
	bool has_base;
	NamedRecordId direct_base; MemberAccess direct_base_access;
	bool direct_base_virtual;
	bool has_virtual_member;
	// The effective non-zero alignment for the canonical record.  This is the
	// strictest value within one declaration; equivalent redeclarations are
	// checked separately below rather than merged by strength.
	bool has_requested_alignment;
	std::size_t requested_alignment;
	bool scoped_enum;
	bool has_underlying;
	TypeId underlying;
	bool template_template;
	ScopeId scope;
	bool has_generated_identity;
	GeneratedIdentity generated_identity;
	DumpScopeViewId dump_scope_view;

	NamedRecord(NamedKind kind = NamedKind::Class, NameId name = NameId(),
		ScopeId owner = ScopeId())
		: kind(kind), name(name), owner(owner), defined(false),
		  class_tag(ClassTag::Struct), has_base(false),
		  direct_base(), direct_base_access(MemberAccess::Public), direct_base_virtual(false), has_virtual_member(false),
		  has_requested_alignment(false), requested_alignment(0),
		  scoped_enum(false),
		  has_underlying(false), underlying(), template_template(false), scope(),
		  has_generated_identity(false), generated_identity(), dump_scope_view()
	{}
};

// Only canonical records that need alignment redeclaration checking receive
// this sparse typed fact.  The effective non-zero request remains on
// NamedRecord because RecordLayout consumes it directly.
enum class NamedRecordAlignmentFlag : std::uint8_t
{
	HasSpecifier = 1,
	HasNondefiningSpecifier = 2,
	NondefiningConflict = 4,
	HasDefinition = 8,
	DefinitionHasSpecifier = 16
};

struct NamedRecordAlignmentFact
{
	std::uint8_t flags;
	std::size_t nondefining_alignment;
	std::size_t definition_alignment;

	NamedRecordAlignmentFact()
		: flags(0), nondefining_alignment(0), definition_alignment(0)
	{}

	bool has(NamedRecordAlignmentFlag flag) const
	{
		return (flags & static_cast<std::uint8_t>(flag)) != 0;
	}

	void add(NamedRecordAlignmentFlag flag)
	{
		flags = static_cast<std::uint8_t>(flags |
			static_cast<std::uint8_t>(flag));
	}
};

// A record's object layout is a PA11 semantic fact keyed by NamedRecordId.
// The state is explicit so a by-value cycle cannot turn into an accidental
// retry loop, and completed consumers can read size/alignment without
// reconstructing the class body.
enum class RecordLayoutState
{
	Incomplete,
	Computing,
	Complete,
	Failed
};

struct RecordLayoutMember
{
	BindingId binding;
	std::size_t offset;

	RecordLayoutMember(BindingId binding = BindingId(),
		std::size_t offset = 0)
		: binding(binding), offset(offset)
	{}
};

struct RecordLayoutBase
{
	NamedRecordId record;
	std::size_t offset;

	RecordLayoutBase(NamedRecordId record = NamedRecordId(),
		std::size_t offset = 0)
		: record(record), offset(offset)
	{}
};

struct BitFieldFact
{
	BindingId binding; ScopeId owner_scope; NamedRecordId owner_record;
	TypeId declared_type; TypeId storage_type; TypeId operation_type;
	std::size_t width, value_width, storage_offset, storage_unit_size, allocation_size;
	std::size_t bit_offset, storage_width;
	std::uint64_t value_mask, storage_mask;
	bool named, zero_width, is_signed;
	BitFieldFact() : binding(), owner_scope(), owner_record(), declared_type(),
		storage_type(), operation_type(), width(0), value_width(0), storage_offset(0),
		storage_unit_size(0), allocation_size(0), bit_offset(0), storage_width(0),
		value_mask(0), storage_mask(0), named(false), zero_width(false), is_signed(false) {}
};

struct RecordMemberDeclaration
{
	NamedRecordId owner_record; bool bit_field; BindingId binding;
	BitFieldFact bit;
	RecordMemberDeclaration(NamedRecordId owner_record = NamedRecordId(),
		BindingId binding = BindingId())
		: owner_record(owner_record), bit_field(false), binding(binding), bit()
	{}
	static RecordMemberDeclaration make_bit_field(const BitFieldFact& value) {
		RecordMemberDeclaration result(value.owner_record, value.binding);
		result.bit_field = true; result.bit = value; return result;
	}
};

struct RecordLayout
{
	RecordLayoutState state;
	std::size_t size;
	std::size_t alignment;
	// A direct base is a distinct subobject, not an ordinary field.  PA16
	// supports exactly one non-virtual base at offset zero.
	bool has_direct_base;
	RecordLayoutBase direct_base;
	// Narrow PA15 checkpoint fact; this is not a full C++ triviality claim.
	// It is meaningful only for a Complete layout and is reset on failure.
	bool checkpoint_zero_storage_eligible;
	std::vector<RecordLayoutMember> members;
	FlatIndex<BindingId, std::size_t, IdentityHash<BindingId> > member_offsets;

	RecordLayout()
		: state(RecordLayoutState::Incomplete), size(0), alignment(0),
		  has_direct_base(false), direct_base(),
		  checkpoint_zero_storage_eligible(false), members(), member_offsets()
	{}
};

struct TypeLayout
{
	std::size_t size;
	std::size_t alignment;

	TypeLayout(std::size_t size = 0, std::size_t alignment = 0)
		: size(size), alignment(alignment)
	{}
};

// Named records receive sparse typed relation entries for canonical
// constructor identity and the narrow anonymous-union storage checkpoint.
struct NamedRecordSidecar
{
	bool local_object_name;
	BindingId backing_storage;
	BindingId constructor_binding; BindingId default_constructor_binding;
	BindingId destructor_binding;
	bool has_constructor_declaration; bool has_destructor_declaration;
	bool has_display_path;
	NamePath display_path;
	std::vector<HiddenFriendFunctionRelation> hidden_friend_functions;

	NamedRecordSidecar(bool local_object_name = false,
		BindingId backing_storage = BindingId(),
		BindingId constructor_binding = BindingId(),
		BindingId default_constructor_binding = BindingId(),
		bool has_constructor_declaration = false)
		: local_object_name(local_object_name), backing_storage(backing_storage),
		  constructor_binding(constructor_binding), default_constructor_binding(default_constructor_binding),
		  destructor_binding(), has_constructor_declaration(has_constructor_declaration),
		  has_destructor_declaration(false),
		  has_display_path(false),
		  display_path(), hidden_friend_functions()
	{}
};
struct AnonymousUnionFact
{
	NamedRecordId record;
	ScopeId owner;
	BindingId storage;
	AnonymousUnionFact(NamedRecordId record = NamedRecordId(),
		ScopeId owner = ScopeId(), BindingId storage = BindingId())
		: record(record), owner(owner), storage(storage)
	{}
};
struct DumpBindingView
{
	ScopeId parent;
	std::size_t position;
	NamedRecordId record;
	NamePath qualified_name;
	BindingId binding;
};
struct DumpScopeView
{
	ScopeId parent;
	std::size_t order;
	NamedRecordId record;
	NamePath qualified_name;
};
struct ParamFact
{
	NameId name;
	TypeId type;
	ParamFact(NameId name = NameId(), TypeId type = TypeId())
		: name(name), type(type)
	{}
};
struct SpecFact
{
	TypeId base;
	bool has_base;
	NamedRecordId anonymous_record;
	unsigned int cv;
	bool is_typedef;
	bool is_constexpr;
	bool is_static;
	bool is_extern;
	bool is_thread_local; bool is_auto;
	SpecFact()
		: base(), has_base(false), anonymous_record(), cv(0),
		  is_typedef(false), is_constexpr(false), is_static(false),
		  is_extern(false), is_thread_local(false), is_auto(false)
	{}
};
struct ConstValue
{
	bool valid;
	bool is_unsigned;
	__int128 value;
	TypeId type;
	ConstValue(bool valid = false, __int128 value = 0,
		bool is_unsigned = false, TypeId type = TypeId())
		: valid(valid), is_unsigned(is_unsigned), value(value), type(type)
	{}
};
struct NonConstantExpression : std::runtime_error
{
	explicit NonConstantExpression(const char* message)
		: std::runtime_error(message)
	{}
};
struct DeclaratorName
{
	bool found;
	NamePath path;
	bool operator_function;
	PA10OperatorFunctionKind operator_function_kind;
	SimpleTokenType operator_token;
	DeclaratorName()
		: found(false), path(), operator_function(false),
		  operator_function_kind(PA10OperatorFunctionKind::None),
		  operator_token(SimpleTokenType::OP_SEMICOLON) {}
};
enum class DeclaratorBaseKind { Typed, AutoPlaceholder };
struct DeclaratorOp
{
	enum Kind
	{
		Pointer,
		LvalueReference,
		RvalueReference,
		Array,
		Function, TrailingReturn
	};
	Kind kind;
	unsigned int cv;
	bool unknown_bound;
	ArrayBound bound;
	const PA10AstNode* parameter_clause; const PA10AstNode* trailing_type_id;
	NamedRecordId member_owner;
	DeclaratorOp(Kind kind = Pointer)
		: kind(kind), cv(0), unknown_bound(false), bound(),
		  parameter_clause(NULL), trailing_type_id(NULL), member_owner()
	{}
};
// PA12 facts are owned by the PA11 model.  The hot representation contains
// only typed IDs, enums, and arena ranges; source text is retained through
// the PA10 node pointer and rendered once at the requested dump boundary.
enum class SemanticFactKind
{
	TypeAlias,
	Variable,
	SimpleDeclaration,
	CompoundStatement,
	ReturnStatement,
	ExpressionStatement,
	CallExpression,
	IdExpression,
	MemberExpression,
	Literal,
	UnaryExpression,
	PostfixExpression,
	BinaryExpression,
	AssignmentExpression,
	ConditionalExpression,
	CastExpression,
	SubscriptExpression,
	BracedInitList,
	SizeofExpression,
	IfStatement,
	ThenBranch,
	ElseBranch,
	SwitchStatement,
	WhileStatement,
	DoStatement,
	ForStatement,
	ForInitStatement,
	Condition,
	ConditionDeclaration,
	Iteration,
	CaseStatement,
	DefaultStatement,
	BreakStatement,
	ContinueStatement,
	ConstructorAction,
	LabeledStatement,
	GotoStatement
};
enum class BuiltinKind
{
	None,
	ConstantP,
	Abort
};
// PA12 uses this local discriminator while validating the source-level cast
// family.  It is not stored in SemanticFact and has no PA15 presentation role.
enum class ExplicitCastKind
{
	None,
	CStyle,
	Static,
	Const,
	Reinterpret,
	Functional
};
struct ConstantAddressFactId
{
	std::size_t value;
	explicit ConstantAddressFactId(std::size_t value = InvalidIdentityValue)
		: value(value)
	{}
	bool valid() const { return value != InvalidIdentityValue; }
};
enum class ConstantAddressKind
{
	None,
	SymbolAddend,
	ArrayElement,
	Literal
};
enum class ConstantAddressContext
{
	Value,
	ObjectAddress,
	ArrayDecay
};
// PA12 owns the one-time typed relocation decision for a namespace-scope
// initializer.  The target remains a semantic BindingId until PA15 allocates
// the LowIR symbol identity.  ArrayElement retains the typed projection that
// PA15 must materialize when a direct relocation is not the checked-in form.
struct ConstantAddressFact
{
	bool evaluated;
	bool valid;
	ConstantAddressKind kind;
	BindingId target;
	long long byte_addend;
	TypeId element_type;
	TypeId index_type;
	SemanticFactId index_fact;
	__int128 index_value;
	bool index_unsigned;
	std::size_t literal_element_count;
	std::size_t literal_byte_begin;
	std::size_t literal_byte_count;
	ConstantAddressFact()
		: evaluated(false), valid(false), kind(ConstantAddressKind::None),
		  target(), byte_addend(0), element_type(), index_type(), index_fact(),
		  index_value(0), index_unsigned(false), literal_element_count(0),
		  literal_byte_begin(InvalidIdentityValue), literal_byte_count(0)
	{}
};
// Floating literal bytes are already typed by PA2.  Keep them in a sparse
// sidecar because only floating literal facts need the payload and the
// payload must retain the source f32/f64/f80 representation exactly.
struct FloatingLiteralFact
{
	FundamentalType type;
	std::size_t byte_begin;
	std::size_t byte_count;
	FloatingLiteralFact(FundamentalType type = FundamentalType::Float,
		std::size_t byte_begin = InvalidIdentityValue,
		std::size_t byte_count = 0)
		: type(type), byte_begin(byte_begin), byte_count(byte_count)
	{}
};
struct SemanticFact
{
	SemanticFactKind kind;
	SemanticValueCategory category;
	TypeId type;
	BindingId binding;
	BindingId selected_binding;
	ScopeId selected_scope;
	LabelId label;
	// A call publishes the canonical function TypeId selected for its boundary
	// so lowering never rediscovers a signature from a child fact.
	TypeId callable_type;
	SimpleTokenType token;
	const PA10AstNode* source;
	std::size_t name_begin;
	std::size_t name_count;
	bool name_global;
	std::size_t child_begin;
	std::size_t child_count;
	std::size_t conversion_begin;
	std::size_t conversion_count;
	std::size_t literal_element_count;
	std::uint64_t literal_value;
	bool has_literal_value;
	bool literal_value_unsigned;
	bool literal_value_negative;
	std::size_t literal_float;
	// PA12-owned result for a constant initializer expression.  PA15 consumes
	// this typed fact rather than recomputing semantic state while lowering.
	bool has_constant_value;
	__int128 constant_value;
	bool constant_value_unsigned;
	bool constant_value_evaluated;
	ConstantAddressFactId constant_address;
	TypeId operation_type;
	// Typed bool-boundary provenance; no source spelling or function category.
	bool canonical_truth;
	bool direct_bool_boundary;
	bool contains_member_value;
	bool bool_context_operand, operator_result;
	bool size_type_derived;
	bool has_callee;
	bool value_initialize;
	// A direct member call stores its typed implicit object as child zero.
	bool has_implicit_object;
	bool temporary_object;
	SemanticFact(SemanticFactKind kind = SemanticFactKind::Variable,
		TypeId type = TypeId(),
		SemanticValueCategory category = SemanticValueCategory::Prvalue,
		const PA10AstNode* source = NULL)
		: kind(kind), category(category), type(type), binding(),
		  selected_binding(), selected_scope(),
		  label(),
		  callable_type(),
		  token(SimpleTokenType::OP_SEMICOLON), source(source),
		  name_begin(0), name_count(0), name_global(false),
		  child_begin(InvalidIdentityValue), child_count(0),
		  conversion_begin(InvalidIdentityValue), conversion_count(0),
			literal_element_count(0), literal_value(0), has_literal_value(false),
			literal_value_unsigned(false), literal_value_negative(false),
			literal_float(InvalidIdentityValue),
			has_constant_value(false), constant_value(0),
			constant_value_unsigned(false), constant_value_evaluated(false),
			constant_address(),
			operation_type(),
			canonical_truth(false), direct_bool_boundary(false),
			contains_member_value(false), bool_context_operand(false), operator_result(false),
			size_type_derived(false),
			has_callee(false),
			value_initialize(false),
			has_implicit_object(false), temporary_object(false)
	{}
};
struct DeclarationFact
{
	const PA10AstNode* node;
	ScopeId scope;
	std::size_t binding_begin;
	std::size_t binding_count;
	std::size_t semantic_begin;
	std::size_t semantic_count;
	bool is_constexpr;
	bool automatic_storage;
	bool is_extern;
	bool is_static;
	bool is_thread_local;
	std::size_t lifetime_begin, lifetime_count;
	DeclarationFact(const PA10AstNode* node = NULL, ScopeId scope = ScopeId())
		: node(node), scope(scope), binding_begin(InvalidIdentityValue),
		  binding_count(0), semantic_begin(InvalidIdentityValue),
		  semantic_count(0), is_constexpr(false), automatic_storage(false),
		  is_extern(false), is_static(false), is_thread_local(false),
		  lifetime_begin(InvalidIdentityValue), lifetime_count(0)
	{}
};
enum class ConstructorActionTarget { Base, Member };
struct ConstructorActionFact
{
	ConstructorActionTarget target; NamedRecordId base_record; BindingId member;
	BindingId constructor; SemanticFactId initializer;
	std::size_t argument_begin; std::size_t argument_count; bool value_initialize;
	TypeId object_type;
	ConstructorActionFact(ConstructorActionTarget target = ConstructorActionTarget::Member,
		NamedRecordId base_record = NamedRecordId(), BindingId member = BindingId(),
		BindingId constructor = BindingId(), SemanticFactId initializer = SemanticFactId())
		: target(target), base_record(base_record), member(member),
		  constructor(constructor), initializer(initializer),
		  argument_begin(InvalidIdentityValue), argument_count(0),
		  value_initialize(false), object_type()
	{}
};
struct DestructorActionFact
{
	ConstructorActionTarget target; NamedRecordId base_record; BindingId member;
	BindingId destructor; TypeId object_type;
	DestructorActionFact(ConstructorActionTarget target = ConstructorActionTarget::Member,
		NamedRecordId base_record = NamedRecordId(), BindingId member = BindingId(),
		BindingId destructor = BindingId(), TypeId object_type = TypeId())
		: target(target), base_record(base_record), member(member), destructor(destructor),
		  object_type(object_type)
	{}
};

struct LifetimeFact
{
	BindingId object; TypeId object_type; BindingId destructor; ScopeId scope;
	LifetimeFact(BindingId object = BindingId(), TypeId object_type = TypeId(),
		BindingId destructor = BindingId(), ScopeId scope = ScopeId())
		: object(object), object_type(object_type), destructor(destructor), scope(scope)
	{}
};

enum class ConstructorRuntimeCacheState : std::uint8_t { Unseen, InProgress, Complete };

struct FunctionFact
{
	const PA10AstNode* node;
	ScopeId owner;
	BindingId binding;
	ScopeId function_scope;
	ScopeId body_scope;
	SemanticFactId body_fact;
	LabelTableId label_table;
	std::size_t default_argument_begin;
	std::size_t default_argument_count;
	bool is_constructor;
	bool is_destructor;
	bool synthetic;
	NamedRecordId constructor_record;
	NamedRecordId destructor_record;
	std::size_t constructor_action_begin, constructor_action_count;
	std::size_t destructor_action_begin, destructor_action_count;

	FunctionFact(const PA10AstNode* node = NULL, ScopeId owner = ScopeId(),
		BindingId binding = BindingId(), ScopeId function_scope = ScopeId(),
		ScopeId body_scope = ScopeId())
		: node(node), owner(owner), binding(binding),
		  function_scope(function_scope), body_scope(body_scope), body_fact(),
		  label_table(),
		  default_argument_begin(InvalidIdentityValue),
		  default_argument_count(0), is_constructor(false), is_destructor(false),
		  synthetic(false), constructor_record(), destructor_record(),
		  constructor_action_begin(InvalidIdentityValue), constructor_action_count(0),
		  destructor_action_begin(InvalidIdentityValue),
		  destructor_action_count(0)
	{}
};

struct LabelFact
{
	NameId name;
	const PA10AstNode* node;

	LabelFact(NameId name = NameId(), const PA10AstNode* node = NULL)
		: name(name), node(node)
	{}
};

struct FunctionLabelTable
{
	FlatIndex<NameId, LabelId, IdentityHash<NameId> > by_name;

	FunctionLabelTable() : by_name() {}
};

struct SyntheticFunctionFact
{
	NamedRecordId record;
	BindingId binding;

	SyntheticFunctionFact(NamedRecordId record = NamedRecordId(),
		BindingId binding = BindingId())
		: record(record), binding(binding)
	{}
};

struct NamespaceFact
{
	const PA10AstNode* node;
	ScopeId scope;

	NamespaceFact(const PA10AstNode* node = NULL, ScopeId scope = ScopeId())
		: node(node), scope(scope)
	{}
};

struct CompoundFact
{
	const PA10AstNode* node;
	ScopeId scope;

	CompoundFact(const PA10AstNode* node = NULL, ScopeId scope = ScopeId())
		: node(node), scope(scope)
	{}
};

enum class StatementFactKind
{
	If,
	Switch,
	While,
	Do,
	For
};

struct StatementFact
{
	const PA10AstNode* node;
	StatementFactKind kind;
	ScopeId scope;

	StatementFact(const PA10AstNode* node = NULL,
		StatementFactKind kind = StatementFactKind::If,
		ScopeId scope = ScopeId())
		: node(node), kind(kind), scope(scope)
	{}
};

struct SwitchCaseKey
{
	std::uint64_t bits;
	unsigned int width;
	bool is_unsigned;

	SwitchCaseKey(std::uint64_t bits = 0, unsigned int width = 0,
		bool is_unsigned = false)
		: bits(bits), width(width), is_unsigned(is_unsigned)
	{}

	bool operator==(const SwitchCaseKey& other) const
	{
		return bits == other.bits && width == other.width &&
			is_unsigned == other.is_unsigned;
	}
};

struct SwitchCaseKeyHash
{
	std::size_t operator()(const SwitchCaseKey& key) const
	{
		std::size_t result = static_cast<std::size_t>(key.bits);
		result ^= result >> 17;
		result *= static_cast<std::size_t>(0xed5ad4bbU);
		result ^= static_cast<std::size_t>(key.width) *
			static_cast<std::size_t>(0x9e3779b9U);
		result ^= key.is_unsigned ? static_cast<std::size_t>(0x85ebca6bU) :
			static_cast<std::size_t>(0xc2b2ae35U);
		return result;
	}
};

struct SwitchValidationContext
{
	TypeId type;
	TypeId conversion_type;
	FlatIndex<SwitchCaseKey, bool, SwitchCaseKeyHash> case_values;
	bool has_default;

	explicit SwitchValidationContext(TypeId type = TypeId(), TypeId conversion_type = TypeId())
		: type(type), conversion_type(conversion_type.valid() ? conversion_type : type), case_values(), has_default(false) {}
};

struct SwitchInitializationFrame
{
	ScopeId scope;
	std::size_t initialized;

	SwitchInitializationFrame(ScopeId scope = ScopeId())
		: scope(scope), initialized(0)
	{}
};

struct SwitchInitializationState
{
	std::vector<SwitchInitializationFrame> lexical_frames;
	std::size_t active;

	SwitchInitializationState() : lexical_frames(), active(0) {}
};

class PA11SemanticModel
{
public:
	explicit PA11SemanticModel(const PA10Ast& ast)
	;
	void analyze()
	;
	void dump(std::ostream& output) const
	;
	void analyze_pa12()
	;
	void dump_pa12(std::ostream& output) const
	;
	void lower_pa15(lowir_model::Program& program) const
	;

private:
	friend class Pa15Lowerer;

	const PA10Ast& ast_;
	std::vector<std::string> names_;
	FlatIndex<std::string, NameId, StringHash> name_ids_;
	std::vector<TypeKey> types_;
	FlatIndex<TypeKey, TypeId, TypeKeyHash> type_ids_;
	std::vector<NamedRecord> named_;
	// Parallel to named_: NamedRecordId is the sole key for this semantic
	// fact.  Non-class records retain the default incomplete entry.
	std::vector<RecordLayout> record_layouts_;
	std::vector<std::vector<RecordMemberDeclaration> > record_member_declarations_;
	FlatIndex<BindingId, NamedRecordId, IdentityHash<BindingId> > record_member_event_owners_;
	FlatIndex<BindingId, BitFieldFact, IdentityHash<BindingId> > bit_field_facts_;
	FlatIndex<NamedRecordId, NamedRecordSidecar, IdentityHash<NamedRecordId> >
		named_record_sidecars_;
	FlatIndex<NamedRecordId, NamedRecordAlignmentFact,
		IdentityHash<NamedRecordId> > named_record_alignment_facts_;
	std::vector<TemplateFunctionFact> template_function_facts_;
	FlatIndex<NameId, TemplateFunctionList, IdentityHash<NameId> >
		template_function_index_;
	std::vector<TemplateSpecializationFact> template_specialization_facts_;
	FlatIndex<TemplateSpecializationKey, TemplateSpecializationId,
		TemplateSpecializationKeyHash> template_specialization_index_;
	std::vector<Scope> scopes_;
	FlatIndex<ScopeId, ScopeId, IdentityHash<ScopeId> >
		unnamed_namespace_index_;
	FlatIndex<ScopeId, NamespaceAliasList, IdentityHash<ScopeId> >
		namespace_alias_declaration_points_;
	FlatIndex<ScopeId, TypeDeclarationList, IdentityHash<ScopeId> >
		type_declaration_points_;
	FlatIndex<ScopeId, SourcePoint, IdentityHash<ScopeId> >
		inline_namespace_declaration_points_;
	FlatIndex<ScopeId, SourcePoint, IdentityHash<ScopeId> >
		scope_declaration_points_;
	FlatIndex<ScopeId, SourcePoint, IdentityHash<ScopeId> >
		function_definition_points_;
	FlatIndex<ScopeId, BindingId, IdentityHash<ScopeId> > function_bindings_;
	FlatIndex<ScopeId, FriendLexicalScopeRelation, IdentityHash<ScopeId> > friend_lexical_scopes_;
	std::vector<Binding> bindings_;
	// BindingId is the canonical identity; retain its typed owning scope so
	// ValueEntry ownership checks do not rescan a whole scope on redeclaration.
	std::vector<ScopeId> binding_owners_;
	FlatIndex<BindingId, BindingSidecar, IdentityHash<BindingId> >
		binding_sidecars_;
	FlatIndex<HiddenFriendBindingKey,
		std::vector<HiddenFriendBindingRelation>, HiddenFriendBindingKeyHash>
		hidden_friend_bindings_;
	ScopeId global_;
	std::vector<ScopeId> deferred_scopes_;
	std::vector<DumpBindingView> dump_binding_views_;
	std::vector<DumpScopeView> dump_scope_views_;
	std::size_t anonymous_union_count_;
	std::size_t anonymous_enum_count_;
	std::size_t creation_order_;
	mutable std::vector<std::uint32_t> lookup_marks_;
	mutable std::uint32_t lookup_generation_;
	mutable std::vector<std::uint32_t> lexical_marks_;
	mutable std::uint32_t lexical_generation_;
	mutable std::vector<LookupFrame> lookup_frames_;
	std::vector<DeclarationFact> declaration_facts_;
	FlatIndex<const PA10AstNode*, DeclarationFactId, PointerHash>
		declaration_fact_index_;
	std::vector<BindingId> declaration_bindings_;
	std::vector<FunctionFact> function_facts_;
	FlatIndex<const PA10AstNode*, FunctionFactId, PointerHash>
		function_fact_index_;
	FlatIndex<BindingId, FunctionFactId, IdentityHash<BindingId> >
		function_binding_fact_index_;
	std::vector<SemanticFactId> function_default_arguments_;
	std::vector<LabelFact> label_facts_;
	std::vector<FunctionLabelTable> label_tables_;
	std::vector<FunctionFactId> class_function_facts_;
	std::vector<ConstructorActionFact> constructor_actions_;
	std::vector<SemanticFactId> constructor_arguments_;
	std::vector<DestructorActionFact> destructor_actions_;
	std::vector<LifetimeFact> lifetime_facts_;
	std::vector<ConstructorRuntimeCacheState> constructor_runtime_states_;
	std::vector<unsigned char> constructor_runtime_results_;
	std::vector<unsigned char> constructor_runtime_invalid_;
	std::vector<ConstructorRuntimeCacheState> destructor_runtime_states_;
	std::vector<unsigned char> destructor_runtime_results_;
	std::vector<unsigned char> destructor_runtime_invalid_;
	std::vector<SyntheticFunctionFact> synthetic_function_facts_;
	std::vector<NamespaceFact> namespace_facts_;
	FlatIndex<const PA10AstNode*, NamespaceFactId, PointerHash>
		namespace_fact_index_;
	std::vector<CompoundFact> compound_facts_;
	FlatIndex<const PA10AstNode*, ScopeId, PointerHash>
		compound_scope_index_;
	std::vector<StatementFact> statement_facts_;
	FlatIndex<const PA10AstNode*, StatementFactId, PointerHash>
		statement_fact_index_;
	FlatIndex<const PA10AstNode*, ScopeId, PointerHash>
		substatement_scope_index_;
	std::vector<SemanticFact> semantic_facts_;
	std::vector<SemanticFactId> semantic_children_;
	std::vector<FloatingLiteralFact> floating_literal_facts_;
	std::vector<std::uint8_t> floating_literal_bytes_;
	std::vector<ConstantAddressFact> constant_address_facts_;
	std::vector<std::uint8_t> constant_address_literal_bytes_;
	std::vector<ConversionFact> conversion_facts_;
	std::vector<NamedRecordId> conversion_base_paths_;
	std::vector<SemanticFactId> declaration_semantic_ids_;
	std::vector<NameId> semantic_name_components_;
	FlatIndex<const PA10AstNode*, AnonymousUnionFact, PointerHash>
		anonymous_union_fact_index_;
	NameId builtin_constant_p_name_;
	NameId builtin_abort_name_;
	BindingId builtin_abort_binding_;
	bool pa12_render_mode_;
	LanguageLinkage current_language_linkage_;
	static void unsupported(const char* feature)
	;
	NameId intern_name(const std::string& name)
	;
	const std::string& name_text(NameId name) const
	;
	NameId name_from_spelling(PPSpellingId spelling)
	;
	TypeId intern_type(const TypeKey& key)
	;
	TypeId fundamental(FundamentalType type) const
	;
	TypeKind type_kind(TypeId type) const
	;
	NamedRecordId append_named_record(const NamedRecord& record)
	;
	const BitFieldFact* bit_field_fact(BindingId binding) const;
	const BitFieldFact* bit_field_fact_for_expression(const ExprInfo& expression) const;
	void set_bit_field_fact(BindingId binding, const BitFieldFact& fact);
	void append_record_member(NamedRecordId record, BindingId binding);
	void append_record_bit_field(NamedRecordId record, const BitFieldFact& fact);
	TypeId make_cv(TypeId child, unsigned int qualifiers)
	;
	TypeId make_pointer(TypeId child, unsigned int qualifiers = 0)
	;
	TypeId make_member_pointer(NamedRecordId owner, TypeId child,
	unsigned int qualifiers = 0)
	;
	TypeId member_object_type(TypeId function_type, ScopeId member_scope)
	;
	TypeId member_object_pointer_type(TypeId function_type,
	ScopeId member_scope)
	;
	TypeId make_reference(TypeId child, bool rvalue)
	;
	TypeId make_array(TypeId child, bool unknown_bound, ArrayBound bound)
	;
	TypeId make_function(const std::vector<TypeId>& parameters, bool variadic,
	TypeId result, unsigned int qualifiers = 0)
	;
	ScopeId create_scope(ScopeKind kind, ScopeId parent, NameId name,
	NamedRecordId record = NamedRecordId(),
	bool inline_namespace = false, bool attach = true)
	;
	const PA10AstNode* child_of_kind(const PA10AstNode& node,
	PA10NodeKind kind) const
	;
	bool is_cv_node(const PA10AstNode& node) const
	;
	unsigned int cv_bit(const PA10AstNode& node) const
	;
	NameId operator_name(PA10OperatorFunctionKind kind,
		SimpleTokenType token)
	;
	NamePath name_path(const PA10AstNode& node)
	;
	bool find_declarator_name(const PA10AstNode& node, DeclaratorName* result)
	;
	DeclaratorName declarator_name(const PA10AstNode& node)
	;
	FunctionDeclarationKind function_declaration_kind(BindingId binding) const
	;
	void record_function_declarator(BindingId binding,
		const DeclaratorName& name, const PA10AstNode& declarator,
		FunctionDeclarationKind declaration_kind)
	;
	ClassTag class_tag(const PA10AstNode& node) const
	;
	TypeId named_type(NamedRecordId named)
	;
	TypeId named_type(NamedRecordId named) const
	;
	NamedRecordId named_record_for_type(TypeId type) const
	;
	ScopeId class_scope_for_type(TypeId type) const
	;
	bool validated_direct_base(NamedRecordId record, NamedRecordId* base) const;
	bool direct_base_chain(TypeId object,
		std::vector<NamedRecordId>* chain) const
	;
	bool member_base_path(TypeId object, ScopeId target,
		std::vector<NamedRecordId>* path) const
	;
	bool derived_base_relation(TypeId object, TypeId target, unsigned int* distance,
		std::vector<NamedRecordId>* path, ScopeId access_scope = ScopeId()) const;
	bool derived_base_path(TypeId object, TypeId target,
		std::vector<NamedRecordId>* path, ScopeId access_scope = ScopeId()) const;
	bool base_path_accessible(TypeId object, ScopeId target, ScopeId access_scope) const;
	bool member_object_qualification_convertible(TypeId object, TypeId required) const;
	bool member_object_convertible(TypeId object, TypeId required, ScopeId member_scope, std::vector<NamedRecordId>* path = NULL, ScopeId access_scope = ScopeId(), unsigned int* distance = NULL) const;
	ScopeId scope_for_type(TypeId type) const
	;
	bool direct_value_exists(ScopeId scope, NameId name) const
	;
	BindingId direct_variable_binding(ScopeId scope,
		const ValueList& values, bool* direct_other) const
	;
	void validate_qualified_class_static_definition(ScopeId target,
		NameId name) const
	;
	bool direct_namespace_exists(ScopeId scope, NameId name) const
	;
	ScopeId named_namespace(ScopeId parent, NameId name)
	;
	ScopeId create_named_namespace(ScopeId parent, NameId name)
	;
	ScopeId lookup_namespace_here(ScopeId scope, NameId name,
		SourcePoint point = SourcePoint()) const
	;
	void record_namespace_alias(ScopeId scope, NameId name,
		SourcePoint declaration_point)
	;
	bool namespace_alias_visible_at(ScopeId scope, NameId name,
		SourcePoint point) const
	;
	void record_type_declaration(ScopeId scope, NameId name,
		SourcePoint declaration_point,
		BindingId declaration = BindingId())
	;
	bool type_visible_at(ScopeId scope, NameId name,
		SourcePoint point) const
	;
	BindingId type_declaration_identity(ScopeId scope, NameId name) const
	;
	bool inline_namespace_visible_at(ScopeId scope,
		SourcePoint point) const
	;
	SourcePoint lookup_source_point(ScopeId start) const
	;
	bool scope_visible_at(ScopeId scope, SourcePoint point) const
	;
	bool relation_visible_at(ScopeId owner, SourcePoint declaration_point,
		SourcePoint point) const
	;
	void begin_lookup() const
	;
	bool mark_lookup_scope(ScopeId scope) const
	;
	void prepare_unqualified_lookup(ScopeId start) const
	;
	bool lexical_scope_is_applicable(ScopeId scope) const
	;
	ScopeId common_ancestor(ScopeId left, ScopeId right) const
	;
	void append_effective_using_targets(ScopeId level,
		std::vector<ScopeId>* targets, SourcePoint point = SourcePoint()) const
	;
	void reset_lookup_frames(LookupGraphKind kind, ScopeId start) const
	;
	ScopeId lookup_namespace_graph(ScopeId start, NameId name,
		bool include_using = true, SourcePoint point = SourcePoint()) const
	;
	ScopeId lookup_namespace_unqualified(ScopeId start, NameId name,
		SourcePoint point = SourcePoint()) const
	;
	TypeId lookup_type_graph(ScopeId start, NameId name,
		bool include_using = true, SourcePoint point = SourcePoint(),
		BindingId* declaration = NULL) const
	;
	TypeId lookup_type_unqualified(ScopeId start, NameId name,
		SourcePoint point = SourcePoint(), BindingId* declaration = NULL) const
	;
	TypeId lookup_type_qualified(ScopeId scope, NameId name,
		SourcePoint point = SourcePoint(), BindingId* declaration = NULL) const
	;
	bool lookup_value_graph(ScopeId start, NameId name,
	std::vector<ValueRef>* result, bool include_using = true,
		SourcePoint point = SourcePoint()) const
	;
	std::vector<ValueRef> lookup_value_unqualified(ScopeId start, NameId name,
		SourcePoint point = SourcePoint()) const
	;
	std::vector<ValueRef> lookup_value_path(const NamePath& path, ScopeId start,
		SourcePoint point = SourcePoint()) const
	;
	ScopeId resolve_qualifier_scope(const std::vector<NameId>& components,
		ScopeId start, SourcePoint point = SourcePoint()) const
	;
	TypeId lookup_type_path(const NamePath& path, ScopeId start,
		SourcePoint point = SourcePoint(), BindingId* declaration = NULL) const
	;
	ScopeId resolve_global_qualifier_scope(
		const std::vector<NameId>& components,
		SourcePoint point = SourcePoint()) const
	;
	ScopeId resolve_namespace_path(const NamePath& path, ScopeId start,
		SourcePoint point = SourcePoint()) const
	;
	BindingId store_binding(ScopeId scope, const Binding& binding,
		std::size_t position = InvalidIdentityValue)
	;
	const BindingSidecar* binding_sidecar(BindingId id) const
	;
	void set_binding_sidecar(BindingId id, const BindingSidecar& sidecar)
	;
	bool is_static_member(BindingId id) const
	;
	void mark_static_member(BindingId id)
	;
	MemberAccess member_access(BindingId id) const
	;
	void set_member_access(BindingId id, MemberAccess access)
	;
	const NamedRecordSidecar* named_record_sidecar(NamedRecordId id) const
	;
	void set_named_record_sidecar(NamedRecordId id,
		const NamedRecordSidecar& sidecar)
	;
	const NamedRecordAlignmentFact* named_record_alignment_fact(
		NamedRecordId id) const
	;
	void set_named_record_alignment_fact(NamedRecordId id,
		const NamedRecordAlignmentFact& fact)
	;
	void remember_type_display_path(TypeId type, const NamePath& path)
	;
	bool canonical_type_display_path(TypeId type, const NamePath& path) const
	;
	const NamePath* type_display_path(TypeId type) const
	;
	void add_dump_binding_view(ScopeId scope, BindingId binding)
	;
	const Binding& binding(BindingId id) const
	;
	Binding& binding(BindingId id)
	;
	void append_value_index(ScopeId scope, NameId name, BindingId id,
		ScopeId origin = ScopeId(),
		SourcePoint declaration_point = SourcePoint())
	;
	TypeId ensure_named_class(ScopeId owner, NameId name, ClassTag tag,
	bool definition)
	;
	TypeId create_anonymous_class(ScopeId owner, ClassTag tag,
	const PA10AstNode& origin)
	;
	TypeId ensure_named_enum(ScopeId owner, NameId name, bool scoped,
	bool has_underlying, TypeId underlying, bool definition)
	;
	TypeId create_anonymous_enum(ScopeId owner, bool scoped, bool has_underlying,
	TypeId underlying, bool definition)
	;
	void finalize_anonymous_record(TypeId type, NameId name, ScopeId owner,
		SourcePoint declaration_point = SourcePoint())
	;
	void inject_anonymous_union(TypeId type, ScopeId owner,
		bool create_storage = false, const PA10AstNode* origin = NULL)
	;
	bool implicit_default_type_empty(TypeId type,
		std::vector<NamedRecordId>& active) const
	;
	bool implicit_default_record_empty(NamedRecordId record,
		std::vector<NamedRecordId>& active) const
	;
	bool implicit_default_constructor_supported(NamedRecordId record) const
	;
	NamedRecordId class_record_for_object_type(TypeId type) const
	;
	bool implicit_default_type_supported(TypeId type,
		std::vector<NamedRecordId>& active) const
	;
	BindingId default_constructor_binding(NamedRecordId record) const
	;
	bool has_constructor_declaration(NamedRecordId record) const
	;
	BindingId destructor_binding(NamedRecordId record) const
	;
	bool has_destructor_declaration(NamedRecordId record) const
	;
	bool aggregate_class_initialization_supported(NamedRecordId record) const
	;
	bool classify_constructor_runtime(NamedRecordId record)
	;
	bool constructor_requires_runtime(NamedRecordId record)
	;
	TypeId constructor_callable_type(BindingId constructor)
	;
	BindingId ensure_implicit_default_constructor(NamedRecordId record)
	;
	bool classify_destructor_runtime(NamedRecordId record)
	;
	bool destructor_requires_runtime(NamedRecordId record)
	;
	BindingId ensure_implicit_destructor(NamedRecordId record)
	;
	void build_destructor_actions(FunctionFactId function)
	;
	void record_automatic_lifetime(BindingId object, TypeId object_type,
		ScopeId scope)
	;
	ConstructorSelection select_constructor(NamedRecordId record,
		ScopeId access_scope,
		const std::vector<const PA10AstNode*>& argument_nodes,
		bool allow_implicit_default,
		ConstructorInitializationContext context)
	;
	BindingId ensure_anonymous_union_constructor(NamedRecordId record)
	;
	const AnonymousUnionFact* anonymous_union_fact(
		const PA10AstNode& node) const
	;
	bool enum_is_scoped(const PA10AstNode& node) const
	;
	NamePath enum_name(const PA10AstNode& node)
	;
	void add_qualified_enum_view(ScopeId parent, NamedRecordId record,
	const NamePath& qualified_name)
	;
	TypeId process_enum_specifier(const PA10AstNode& node, ScopeId scope,
	NamedRecordId* anonymous_record)
	;
	void add_enumerator(ScopeId scope, NameId name, TypeId type,
		__int128 value, bool value_unsigned = false,
		SourcePoint declaration_point = SourcePoint())
	;
	bool integral_type(FundamentalType type) const
	;
	bool unsigned_type(FundamentalType type) const
	;
	ConstValue literal_constant(const PA10AstNode& node) const
	;
	void check_constant_range(const ConstValue& value) const
	;
	const RecordLayout& record_layout(NamedRecordId record) const
	;
	bool layout_depends_on_template_parameter(TypeId type) const
	;
	bool type_checkpoint_zero_storage_eligible(TypeId type) const
	;
	TypeLayout type_layout(TypeId type) const
	;
	void complete_record_members(NamedRecordId record, const Scope& scope, RecordLayout& layout, bool is_union, bool& zero_storage_eligible, std::size_t& offset, std::size_t& largest_member, std::size_t& record_alignment);
	void complete_record_layout(NamedRecordId record)
	;
	std::size_t type_size(TypeId type) const
	;
	std::size_t type_alignment(TypeId type) const
	;
	TypeId expression_type(const PA10AstNode& node, ScopeId scope)
	;
	TypeId constant_expression_type(const PA10AstNode& node, ScopeId scope,
		bool allow_scoped_enum_integral_comparison = false)
	;
	TypeId conditional_common_type(TypeId when_true, TypeId when_false) const
	;
	bool enumeration_id(TypeId type) const
	;
	TypeId sizeof_operand_type(const PA10AstNode& node, ScopeId scope)
	;
	ConstValue eval_constexpr(const PA10AstNode& node, ScopeId scope,
		bool allow_scoped_enum_integral_comparison = false)
	;
	ConstValue eval_constexpr_unary(const PA10AstNode& node, ScopeId scope,
		bool allow_scoped_enum_integral_comparison)
	;
	ConstValue eval_constexpr_binary(const PA10AstNode& node, ScopeId scope,
		bool allow_scoped_enum_integral_comparison)
	;
	ConstValue eval_constexpr_conditional(const PA10AstNode& node,
		ScopeId scope, bool allow_scoped_enum_integral_comparison)
	;
	ConstValue constant_value_as_type(const ConstValue& value, TypeId type) const
	;
	void record_constant_expression_value(SemanticFactId fact, ScopeId scope)
	;
	void record_constant_initializer(SemanticFactId fact, ScopeId scope)
	;
	ConstantAddressFactId make_constant_address_fact(
		const ConstantAddressFact& fact)
	;
	void record_constant_address(SemanticFactId fact, ScopeId scope)
	;
	bool resolve_constant_address_impl(SemanticFactId fact, ScopeId scope,
		ConstantAddressContext context, ConstantAddressFact* result)
	;
	bool resolve_constant_address_literal(const SemanticFact& fact,
		ConstantAddressContext context, ConstantAddressFact* result)
	;
	bool constant_address_fact_well_formed(
		const ConstantAddressFact& fact) const
	;
	bool constant_integer_value(SemanticFactId fact, __int128* value,
		bool* is_unsigned) const
	;
	bool resolve_constant_address(SemanticFactId fact, ScopeId scope,
		ConstantAddressContext context, ConstantAddressFact* result)
	;
	TypeId decltype_type(const PA10AstNode& node, ScopeId scope)
	;
	void add_type_binding(ScopeId scope, NameId name, TypeId type, ClassTag tag,
	bool has_tag, SourcePoint declaration_point = SourcePoint())
	;
	BindingId add_type_alias(ScopeId scope, NameId name, TypeId type,
		SourcePoint declaration_point = SourcePoint())
	;
	TypeId normalize_parameter_type(TypeId type)
	;
	TypeId normalize_embedded_function_types(TypeId type)
	;
	TypeId normalize_function_type(TypeId type)
	;
	BindingId add_value(ScopeId scope, NameId name, TypeId type, bool function,
		bool definition = false, bool lexical_view = false, BindingId backing_storage = BindingId(),
		SourcePoint declaration_point = SourcePoint(), bool internal_linkage = false,
		LanguageLinkage language_linkage = LanguageLinkage::Cxx,
		FunctionDeclarationKind declaration_kind = FunctionDeclarationKind::Normal,
		bool hidden_friend = false, PA10OperatorFunctionKind operator_function_kind = PA10OperatorFunctionKind::None,
		SimpleTokenType operator_token = SimpleTokenType::OP_SEMICOLON);
	void index_hidden_friend(NameId name, ScopeId namespace_scope, BindingId binding);
	bool has_friend_specifier(const PA10AstNode& node) const;
	ScopeId friend_namespace_scope(ScopeId scope) const;
	NamedRecordId friend_record_for_scope(ScopeId scope) const;
	void record_friend_function(BindingId binding, NamedRecordId record, bool hidden, SourcePoint declaration_point);
	void validate_nonmember_operator(BindingId binding) const;
	ScopeId declaration_scope(const NamePath& path, ScopeId current) const
	;
	SpecFact spec_fact(const PA10AstNode& node, ScopeId scope)
	;
	NamePath class_name(const PA10AstNode& node)
	;
	std::size_t alignment_specifier_value(
		const PA10AlignmentSpecifier& spec, ScopeId scope)
	;
	std::size_t requested_alignment(const PA10AstNode& node, ScopeId scope,
		bool* has_specifier = NULL)
	;
	void apply_record_alignment(const PA10AstNode& node,
		NamedRecordId record, ScopeId scope, bool definition,
		const PA10AstNode* additional = NULL)
	;
	void apply_member_alignment(const PA10AstNode& node,
		BindingId binding, ScopeId scope)
	;
	void process_base_clause(const PA10AstNode& node,
		NamedRecordId record, ScopeId scope)
	;
	void process_class_body(const PA10AstNode& node, TypeId type, ScopeId owner,
		bool alignment_applied)
	;
	void process_special_member(const PA10AstNode& node, ScopeId scope)
	;
	TypeId type_from_type_id(const PA10AstNode& node, ScopeId scope)
	;
	DeclaratorOp pointer_op(const PA10AstNode& node, ScopeId scope)
	;
	ArrayBound literal_bound(const PA10AstNode& node) const
	;
	bool contains_parameter_pack(const PA10AstNode& node) const
	;
	std::vector<TypeId> parameter_types(const PA10AstNode& clause, ScopeId scope,
	bool* variadic, std::vector<ParamFact>* facts)
	;
	TypeId apply_prefix(const std::vector<DeclaratorOp>& ops, TypeId base)
	;
	TypeId apply_suffix(const std::vector<DeclaratorOp>& ops, TypeId base,
	ScopeId scope)
	;
	TypeId apply_declarator(const PA10AstNode& node, TypeId base, ScopeId scope, DeclaratorBaseKind& base_kind);
	TypeId member_function_expression_type(TypeId type, ScopeId scope,
		BindingId binding)
	;
	bool ambiguous_call_statement(const PA10AstNode& node, ScopeId scope,
	NamePath* callee, const PA10AstNode** argument)
	;
	bool direct_initializer_operand(const PA10AstNode& node, ScopeId scope,
		const PA10AstNode** operand)
	;
	const PA10AstNode* top_parameter_clause(const PA10AstNode& node) const
	;
	void process_simple_declaration(const PA10AstNode& node, ScopeId scope)
	;
	void process_bit_field_declaration(const PA10AstNode& node, ScopeId scope);
	void process_function_definition(const PA10AstNode& node, ScopeId scope)
	;
	ScopeId process_compound_statement(const PA10AstNode& node, ScopeId parent)
	;
	void process_namespace(const PA10AstNode& node, ScopeId parent)
	;
	void process_namespace_alias(const PA10AstNode& node, ScopeId scope)
	;
	void process_using_directive(const PA10AstNode& node, ScopeId scope)
	;
	void process_using_declaration(const PA10AstNode& node, ScopeId scope)
	;
	NameId template_parameter_name(const PA10AstNode& node)
	;
	void process_template_parameter(const PA10AstNode& node, ScopeId scope)
	;
	void record_template_function(const PA10AstNode& node, ScopeId visible_scope,
		ScopeId parameter_scope)
	;
	void process_template_declaration(const PA10AstNode& node, ScopeId parent)
	;
	void process_declaration(const PA10AstNode& node, ScopeId scope)
	;
	const DeclarationFact* declaration_fact(const PA10AstNode& node) const
	;
	DeclarationFact* declaration_fact(const PA10AstNode& node)
	;
	const FunctionFact* function_fact(const PA10AstNode& node) const
	;
	FunctionFact* function_fact(const PA10AstNode& node)
	;
	const FunctionFact* function_fact_for_binding(BindingId binding) const
	;
	FunctionFact* function_fact_for_binding(BindingId binding)
	;
	SemanticFactId function_default_argument(BindingId binding,
		std::size_t parameter) const
	;
	const NamespaceFact* namespace_fact(const PA10AstNode& node) const
	;
	ScopeId compound_scope(const PA10AstNode& node) const
	;
	ScopeId create_internal_scope(ScopeId parent)
	;
	void process_condition_declaration(const PA10AstNode& node, ScopeId scope)
	;
	void prepare_pa12()
	;
	void prepare_pa12_member_parameter(FunctionFact& function)
	;
	bool has_function_default_argument(const PA10AstNode& declaration,
		std::size_t declarator_index) const
	;
	void record_function_default_arguments(FunctionFactId function,
		const PA10AstNode& declaration, std::size_t declarator_index)
	;
	void publish_class_constructor_defaults(const PA10AstNode& node,
		ScopeId class_scope)
	;
	void prepare_pa12_node(const PA10AstNode& node, ScopeId scope)
	;
	void analyze_special_member(const PA10AstNode& node, ScopeId scope)
	;
	void build_constructor_actions(FunctionFactId function)
	;
	void prepare_pa12_compound(const PA10AstNode& node, ScopeId parent)
	;
	void prepare_pa12_statement(const PA10AstNode& node, ScopeId scope)
	;
	void prepare_pa12_labels(const PA10AstNode& body, FunctionFact& function)
	;
	void collect_pa12_labels(const PA10AstNode& node,
		FunctionLabelTable& table)
	;
	LabelId label_for_name(const FunctionFact& function, NameId name) const
	;
	bool simple_declaration_has_initializer(const PA10AstNode& node) const
	;
	void collect_switch_transfer_points(const PA10AstNode& node,
		ScopeId scope, SwitchInitializationState* state) const
	;
	void validate_switch_initialization(const PA10AstNode& body,
		ScopeId scope) const
	;
	ScopeId prepare_pa12_control(const PA10AstNode& node, ScopeId parent,
		StatementFactKind kind)
	;
	void prepare_pa12_condition(const PA10AstNode& node, ScopeId scope)
	;
	void prepare_pa12_substatement(const PA10AstNode& node, ScopeId parent)
	;
	StatementFactId add_statement_fact(const StatementFact& fact)
	;
	const StatementFact* statement_fact(const PA10AstNode& node) const
	;
	ScopeId substatement_scope(const PA10AstNode& node) const
	;
	TypeId strip_cv_type(TypeId type) const
	;
	TypeId strip_reference_type(TypeId type) const
	;
	TypeId strip_top_cv_type(TypeId type)
	;
	bool modifiable_lvalue(TypeId type) const
	;
	TypeId expression_object_type(TypeId type) const
	;
	TypeId member_access_type(TypeId object, TypeId member)
	;
	enum class MemberLookupKind
	{
		None,
		Value,
		Type,
		Blocked
	};
	struct MemberLookup
	{
		MemberLookupKind kind;
		BindingId binding;
		ScopeId owner;
		TypeId type;
		std::vector<NamedRecordId> base_path;

		MemberLookup(MemberLookupKind kind = MemberLookupKind::None)
			: kind(kind), binding(), owner(), type(), base_path()
		{}
	};
	MemberLookup member_lookup(TypeId object, NameId name) const
	;
	MemberLookup unqualified_member_lookup(TypeId object, NameId name,
		ScopeId start) const
	;
	ExprInfo semantic_static_data_member(const PA10AstNode& node, ScopeId scope,
		const NamePath& path, const MemberLookup& selection, bool* claimed)
	;
	ExprInfo semantic_unqualified_static_data(const PA10AstNode& node,
		ScopeId scope, const NamePath& path, bool* claimed)
	;
	ExprInfo semantic_qualified_static_data(const PA10AstNode& node,
		ScopeId scope, const NamePath& path, bool* claimed)
	;
	ExprInfo semantic_static_data(const PA10AstNode& node, ScopeId scope,
		const NamePath& path)
	;
	std::vector<ValueRef> member_function_candidates_in_scope(ScopeId scope,
		NameId name) const
	;
	std::vector<ValueRef> static_member_function_candidates_in_scope(
		ScopeId scope, NameId name) const
	;
	bool qualified_static_member_candidates(const PA10AstNode& node,
		ScopeId scope, std::vector<ValueRef>* candidates, ScopeId* owner)
	;
	std::vector<ValueRef> direct_call_candidates(const PA10AstNode& node,
		ScopeId scope, bool qualified_class_member,
		const std::vector<ValueRef>& qualified_static_candidates)
	;
	void validate_direct_static_member_call(const ValueRef& selected,
		bool qualified_class_member, ScopeId qualified_static_scope,
		ScopeId access_scope) const
	;
	TypedFunctionSelection select_typed_function(
		const std::vector<ValueRef>& candidates,
		const std::vector<const PA10AstNode*>& argument_nodes,
		const std::vector<ExprInfo>& initial_arguments, ScopeId scope,
		bool reject_class_by_value = false)
	;
	TypedOperatorSelection select_typed_operator(const std::vector<ValueRef>& member_candidates, const std::vector<ValueRef>& nonmember_candidates, const ExprInfo& member_object, const std::vector<const PA10AstNode*>& member_argument_nodes, const std::vector<ExprInfo>& member_arguments, const std::vector<const PA10AstNode*>& nonmember_argument_nodes, const std::vector<ExprInfo>& nonmember_arguments, ScopeId scope, bool reject_class_by_value = false);
	void collect_operator_candidates(PA10OperatorFunctionKind kind, SimpleTokenType token, TypeId member_object, const std::vector<TypeId>& associated_objects, ScopeId scope, std::vector<ValueRef>* member_candidates, std::vector<ValueRef>* nonmember_candidates) const;
	ExprInfo semantic_operator_call(const PA10AstNode& node, ScopeId scope, PA10OperatorFunctionKind kind, SimpleTokenType token, const ExprInfo& member_object, const std::vector<TypeId>& associated_objects, const std::vector<const PA10AstNode*>& member_argument_nodes, const std::vector<ExprInfo>& member_arguments, const std::vector<const PA10AstNode*>& nonmember_argument_nodes, const std::vector<ExprInfo>& nonmember_arguments, bool reject_class_by_value = false);
	bool member_accessible(BindingId binding, ScopeId member_scope,
		ScopeId access_scope, TypeId object) const
	;
	BindingId implicit_this_binding(ScopeId scope) const
	;
	ExprInfo semantic_this_expression(const PA10AstNode& node,
		ScopeId scope)
	;
	ExprInfo semantic_this_expression(const PA10AstNode& node,
		BindingId binding)
	;
	ExprInfo semantic_member_expression(const PA10AstNode& node,
		ScopeId scope)
	;
	ExprInfo semantic_member_call_expression(const PA10AstNode& node,
		const PA10AstNode& member_node, ScopeId scope)
	;
	ExprInfo semantic_member_call_with_object(const PA10AstNode& node,
		ScopeId scope, SimpleTokenType member_token,
		const ExprInfo& object, TypeId actual_object, ScopeId member_scope,
		const std::vector<ValueRef>& candidates,
		const std::vector<NamedRecordId>* base_path = NULL,
		bool allow_static = false, BindingId implicit_this = BindingId())
	;
	ExprInfo finish_member_call(const PA10AstNode& node, ScopeId scope, SimpleTokenType member_token, const ExprInfo& object, TypeId actual_object, std::vector<ExprInfo>& arguments, ValueRef selected, TypeId selected_type, bool selected_static, BindingId implicit_this);
	ExprInfo semantic_member_call_with_implicit_object(
		const PA10AstNode& node, ScopeId scope, BindingId this_binding,
		TypeId actual_object, ScopeId member_scope,
		const std::vector<ValueRef>& candidates)
	;
	ExprInfo semantic_unqualified_member_call(const PA10AstNode& node,
		const PA10AstNode& callee_node, ScopeId scope)
	;
	ExprInfo semantic_member_call_probe(const PA10AstNode& node,
		ScopeId scope)
	;
	ExprInfo semantic_injected_member(const PA10AstNode& node,
		ScopeId scope, BindingId member_id)
	;
	ExprInfo semantic_storage_id(BindingId storage,
		const PA10AstNode* source = NULL)
	;
	bool semantic_local_class_initializer(
		BindingId storage, SemanticFactId variable,
		const PA10AstNode& source, const PA10AstNode* direct_operand,
		const PA10AstNode* clause, NamedRecordId record, ScopeId access_scope,
		ConstructorInitializationContext context)
	;
	SemanticFactId semantic_constructor_action(BindingId storage,
		const PA10AstNode& source,
		const std::vector<const PA10AstNode*>& argument_nodes =
			std::vector<const PA10AstNode*>(), ScopeId access_scope = ScopeId(),
		ConstructorInitializationContext context =
			ConstructorInitializationContext::Direct)
	;
	bool fundamental_of(TypeId type, FundamentalType* result) const
	;
	bool unsigned_integral_type(TypeId type) const
	;
	bool integral_id(TypeId type) const
	;
	bool bool_id(TypeId type) const
	;
	bool floating_id(TypeId type) const
	;
	bool void_id(TypeId type) const
	;
	bool pointer_id(TypeId type) const
	;
	bool scalar_id(TypeId type) const
	;
	bool nullptr_id(TypeId type) const
	;
	unsigned int integral_rank(TypeId type) const
	;
	bool signed_integral_represents(TypeId signed_type,
		TypeId unsigned_value) const
	;
	FundamentalType unsigned_counterpart(FundamentalType type) const
	;
	unsigned int cv_qualifiers(TypeId type) const
	;
	void qualification_decomposition(TypeId type,
	std::vector<unsigned int>& qualifiers, TypeId* unqualified) const
	;
	bool qualification_convertible_impl(TypeId source, TypeId target,
	bool outer_pointer_consumed) const
	;
	bool qualification_convertible(TypeId source, TypeId target) const
	;
	bool object_type(TypeId type) const
	;
	bool complete_object_type(TypeId type) const
	;
	bool pointer_convertible(TypeId source, TypeId target) const
	;
	bool pointer_common_type_convertible(TypeId source, TypeId target) const;
	TypeId pointer_subtraction_common_type(TypeId left, TypeId right)
	;
	TypeId conditional_pointer_common_type(TypeId left, TypeId right)
	;
	bool integer_zero(const PA10AstNode& node) const
	;
	BuiltinKind builtin_kind(const PA10AstNode& node)
	;
	BindingId builtin_binding(BuiltinKind kind)
	;
	class SemanticTailGuard
	{
	public:
		explicit SemanticTailGuard(PA11SemanticModel& model)
		;
		~SemanticTailGuard()
		;
		void commit()
		;
		void discard()
		;
	private:
		PA11SemanticModel& model_;
		std::size_t semantic_begin_;
		std::size_t children_begin_;
		std::size_t floating_literal_begin_;
		std::size_t floating_literal_bytes_begin_;
		std::size_t constant_address_begin_;
		std::size_t constant_address_bytes_begin_;
		std::size_t conversion_begin_;
		std::size_t conversion_base_path_begin_;
		std::size_t names_begin_;
		bool active_;
	};
	SemanticFactId make_semantic_fact(const SemanticFact& fact)
	;
	void set_semantic_children(SemanticFactId fact,
	const std::vector<SemanticFactId>& children)
	;
	void set_semantic_name(SemanticFactId fact, const NamePath& path)
	;
	bool has_template_id(const PA10AstNode& node) const
	;
	NamePath template_name_path(const PA10AstNode& node)
	;
	std::string render_template_specialization(
		TemplateSpecializationId id) const
	;
	std::string render_template_argument_type(TypeId type) const
	;
	const TemplateFunctionList* template_functions(const NamePath& path,
		ScopeId scope) const
	;
	bool template_argument_types(const PA10AstNode& node, ScopeId scope,
		std::vector<TypeId>* arguments)
	;
	TypeId substitute_template_type(TypeId type,
		const TemplateFunctionFact& function,
		const std::vector<TypeId>& arguments)
	;
	bool deduce_template_type(TypeId pattern, TypeId actual,
		const TemplateFunctionFact& function,
		std::vector<TypeId>* arguments) const
	;
	TemplateSpecializationId specialize_template_function(
		TemplateFunctionId function, const std::vector<TypeId>& arguments)
	;
	FunctionIdResolution resolve_template_function_id_target(
		const PA10AstNode& node, ScopeId scope, TypeId target)
	;
	ExprInfo semantic_template_call(const PA10AstNode& node, ScopeId scope,
		const TemplateFunctionList& candidates,
		const PA10AstNode& argument_node)
	;
	ConversionFactId add_conversion(TypeId source, TypeId target,
		ConversionKind kind, unsigned int rank);
	ConversionFactId add_conversion(TypeId source, TypeId target,
		const ConversionChoice& choice);
	void set_fact_conversion(SemanticFactId fact, ConversionFactId conversion)
	;
	std::string semantic_name(const SemanticFact& fact) const
	;
	std::string binding_display_name(BindingId binding_id) const
	;
	std::string qualified_binding_name(ScopeId owner, NameId name) const
	;
	std::string qualified_binding_name(ScopeId owner, BindingId binding_id) const
	;
	TypeId function_result_type(TypeId type) const
	;
	TypeId callable_function_type(TypeId type) const
	;
	ConversionChoice conversion_for(TypeId source, SemanticValueCategory category, TypeId target, const PA10AstNode* source_node, bool source_integer_zero = false, ScopeId access_scope = ScopeId(), TypeId source_operation_type = TypeId(), BindingId source_binding = BindingId()) const;
	ConversionChoice conversion_for(const ExprInfo& expression, TypeId target, const PA10AstNode* source_node, ScopeId access_scope = ScopeId()) const;
	ExprInfo make_bit_field_reference_temporary(const ExprInfo& expression, TypeId target, const PA10AstNode* source_node, ScopeId access_scope, const ConversionChoice& binding);
	const PA10AstNode* target_function_id(const PA10AstNode& node, ScopeId scope)
	;
	FunctionIdResolution resolve_function_id_target(const PA10AstNode& node,
		ScopeId scope, TypeId target)
	;
	FunctionIdResolution resolve_single_argument_function(const NamePath& path,
		ScopeId scope, const ExprInfo& argument) const
	;
	ExprInfo semantic_single_argument_call(const PA10AstNode& node,
		const FunctionIdResolution& resolution, const ExprInfo& argument,
		ScopeId scope);
	ExprInfo semantic_id_expression_selected(const PA10AstNode& node,
	ScopeId scope, const FunctionIdResolution& resolution)
	;
	ExprInfo semantic_expression_for_target(const PA10AstNode& node,
	ScopeId scope, TypeId target)
	;
	ExprInfo semantic_default_member_initializer(const PA10AstNode& node,
		ScopeId scope, TypeId target)
	;
	void retarget_constexpr_literal(SemanticFactId fact, TypeId target)
	;
	ExprInfo apply_context_conversion(const ExprInfo& expression, TypeId target, const PA10AstNode* source_node, ScopeId access_scope = ScopeId());
	TypeId common_integral_type(TypeId left, TypeId right) const;
	TypeId integral_operation_type(const ExprInfo& expression) const;
	TypeId common_arithmetic_type(TypeId left, TypeId right) const;
	unsigned int floating_rank(TypeId type) const;
	void record_builtin_conversion(const ExprInfo& expression, TypeId target,
		ScopeId access_scope = ScopeId());
	SemanticFactId make_expression_fact(SemanticFactKind kind, TypeId type,
	SemanticValueCategory category, const PA10AstNode& node,
	const std::vector<SemanticFactId>& children)
	;
	SemanticFactId semantic_literal(const PA10AstNode& node)
	;
	std::size_t add_floating_literal(const LiteralData& literal)
	;
	ExprInfo semantic_id_expression(const PA10AstNode& node, ScopeId scope)
	;
	ExprInfo semantic_unary_expression(const PA10AstNode& node, ScopeId scope)
	;
	ExprInfo semantic_postfix_expression(const PA10AstNode& node, ScopeId scope)
	;
	ExprInfo semantic_binary_expression(const PA10AstNode& node, ScopeId scope)
	;
	ExprInfo semantic_assignment_expression(const PA10AstNode& node,
	ScopeId scope)
	;
	ExprInfo semantic_conditional_expression(const PA10AstNode& node,
	ScopeId scope)
	;
	bool builtin_cast_target(const PA10AstNode& node, TypeId* target) const
	;
	bool functional_cast_target(const PA10AstNode& node, ScopeId scope,
	TypeId* target)
	;
	bool functional_cast_target_supported(TypeId target) const
	;
	bool cv_cast_compatible(TypeId source, TypeId target) const
	;
	bool cv_cast_compatible_impl(TypeId source, TypeId target) const
	;
	bool reinterpret_reference_compatible(TypeId source, TypeId target) const
	;
	ExplicitCastKind explicit_cast_kind(const PA10AstNode& node) const
	;
	ExprInfo semantic_cast_to_target(const PA10AstNode& node, ScopeId scope,
	TypeId target, const ExprInfo& operand);
	ExprInfo semantic_functional_cast(const PA10AstNode& node, ScopeId scope,
	TypeId target, const PA10AstNode& argument_node)
	;
	ExprInfo semantic_cast_expression(const PA10AstNode& node, ScopeId scope)
	;
	ExprInfo semantic_call_expression(const PA10AstNode& node, ScopeId scope)
	;
	void apply_call_argument_conversions(std::vector<ExprInfo>& arguments,
		TypeId selected_type, ScopeId scope)
	;
	ExprInfo semantic_builtin_call(const PA10AstNode& node, ScopeId scope,
	BuiltinKind builtin, const PA10AstNode& argument_node)
	;
	ExprInfo semantic_expression(const PA10AstNode& node, ScopeId scope)
	;
	ExprInfo semantic_braced_init_list(const PA10AstNode& node,
		TypeId target, ScopeId scope)
	;
	ExprInfo semantic_empty_braced_init_list(const PA10AstNode& node,
		TypeId target)
	;
	SemanticFactId semantic_declaration(const PA10AstNode& node, ScopeId scope)
	;
	SemanticFactId semantic_declaration_statement(const PA10AstNode& node,
		ScopeId scope)
	;
	SemanticFactId semantic_anonymous_union_statement(const PA10AstNode& node)
	;
	SemanticFactId semantic_ambiguous_call_statement(const PA10AstNode& node,
	ScopeId scope)
	;
	bool ambiguous_assignment_statement(const PA10AstNode& node,
		ScopeId scope, NamePath* callee, const PA10AstNode** argument,
		const PA10AstNode** right)
	;
	SemanticFactId semantic_compound(const PA10AstNode& node, ScopeId parent,
	const FunctionFact& function, unsigned int loop_depth,
	unsigned int switch_depth, SwitchValidationContext* switch_context)
	;
	SemanticFactId semantic_condition(const PA10AstNode& node, ScopeId scope,
		bool switch_condition)
	;
	SemanticFactId semantic_case_label(const PA10AstNode& node, ScopeId scope,
		SwitchValidationContext& switch_context)
	;
	TypeId promote_integral_type(TypeId type) const
	;
	TypeId switch_condition_type(TypeId type) const
	;
	bool case_label_convertible(TypeId source, TypeId target) const
	;
	bool convert_case_value(TypeId switch_type, __int128 value,
		SwitchCaseKey* result) const
	;
	SemanticFactId semantic_for_init(const PA10AstNode& node, ScopeId scope)
	;
	SemanticFactId semantic_substatement(const PA10AstNode& wrapper,
		ScopeId parent, const FunctionFact& function, unsigned int loop_depth,
		unsigned int switch_depth, SwitchValidationContext* switch_context)
	;
	SemanticFactId semantic_jump_statement(const PA10AstNode& node,
		const FunctionFact& function, unsigned int loop_depth,
		unsigned int switch_depth)
	;
	SemanticFactId semantic_label_statement(const PA10AstNode& node,
		ScopeId scope, const FunctionFact& function, unsigned int loop_depth,
		unsigned int switch_depth, SwitchValidationContext* switch_context)
	;
	SemanticFactId semantic_statement(const PA10AstNode& node, ScopeId scope,
	const FunctionFact& function, unsigned int loop_depth,
	unsigned int switch_depth, SwitchValidationContext* switch_context)
	;
	SemanticFactId semantic_return_statement(const PA10AstNode& node,
		ScopeId scope, const FunctionFact& function)
	;
	void analyze_pa12_node(const PA10AstNode& node, ScopeId scope)
	;
	const char* semantic_category_name(SemanticValueCategory category) const
	;
	std::string semantic_operator(const SemanticFact& fact) const
	;
	std::string semantic_literal_token(const SemanticFact& fact) const
	;
	void dump_pa12_fact(std::ostream& output, SemanticFactId id,
	std::size_t depth) const
	;
	void dump_pa12_function(std::ostream& output, const PA10AstNode& node,
	std::size_t depth) const
	;
	void dump_pa12_synthetic_function(std::ostream& output,
		const SyntheticFunctionFact& function, std::size_t depth) const
	;
	void dump_pa12_template_specialization(std::ostream& output,
		const TemplateSpecializationFact& specialization, std::size_t depth) const
	;
	void dump_pa12_top_node(std::ostream& output, const PA10AstNode& node,
	ScopeId scope, std::size_t depth) const
	;
	std::string render_name_path(const NamePath& path) const
	;
	std::string render_generated_name(const GeneratedIdentity& generated) const
	;
	std::string render_record_name(NamedRecordId record_id) const
	;
	std::string render_named_record(NamedRecordId record_id,
	ClassTag override_tag, bool use_override,
	const NamePath* display_path = NULL) const
	;
	std::string render_named(TypeId type, ClassTag override_tag,
	bool use_override) const
	;
	std::string render_type(TypeId type) const
	;
	std::string render_member_object_parameter(TypeId function_type,
	ScopeId member_scope) const
	;
	std::string render_member_function_type(TypeId function_type,
	ScopeId member_scope, BindingId binding) const
	;
	std::string render_binding_type(const Binding& binding) const
	;
	const char* binding_label(BindingKind kind) const
	;
	void dump_binding(std::ostream& output, BindingId binding_id,
	std::size_t depth, const NamePath* display_path = NULL) const
	;
	bool has_dump_scope_view(NamedRecordId record) const
	;
	void dump_scope_view(std::ostream& output, const DumpScopeView& view,
	std::size_t depth) const
	;
	void dump_scope(std::ostream& output, ScopeId scope, std::size_t depth) const
	;
};

}
