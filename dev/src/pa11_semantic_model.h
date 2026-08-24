#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "pa10_ast.h"
#include "posttoken.h"
#include "pa11_semantic_storage.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;


enum class TypeKind
{
	Fundamental,
	Named,
	Cv,
	Pointer,
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

enum class BindingKind
{
	Type,
	TypeAlias,
	Function,
	Variable,
	Parameter,
	Enumerator
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
	bool has_definition;

	Binding(BindingKind kind = BindingKind::Variable, NameId name = NameId(),
		TypeId type = TypeId())
		: kind(kind), name(name), type(type), has_tag(false),
		  class_tag(ClassTag::Struct), declaration_tags(),
		  has_value(false), value(0), has_definition(false)
	{}
};

struct ValueList
{
	std::vector<BindingId> bindings;
};

struct ValueRef
{
	ScopeId scope;
	BindingId binding;

	ValueRef(ScopeId scope = ScopeId(), BindingId binding = BindingId())
		: scope(scope), binding(binding)
	{}
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
	bool inline_namespace;
	std::vector<ScopeId> children;
	std::vector<BindingId> bindings;
	FlatIndex<NameId, TypeId, IdentityHash<NameId> > types;
	FlatIndex<NameId, ScopeId, IdentityHash<NameId> > namespaces;
	FlatIndex<NameId, ScopeId, IdentityHash<NameId> > namespace_aliases;
	FlatIndex<NameId, ValueList, IdentityHash<NameId> > values;
	FlatIndex<NameId, TypeId, IdentityHash<NameId> > using_types;
	std::vector<ScopeId> using_directives;
	std::vector<DumpBindingViewId> binding_views;
	std::vector<DumpScopeViewId> scope_views;
	std::size_t creation_order;

	Scope(ScopeKind kind = ScopeKind::Namespace, ScopeId parent = ScopeId(),
		NameId name = NameId(), NamedRecordId record = NamedRecordId(),
		bool inline_namespace = false, std::size_t creation_order = 0)
		: kind(kind), parent(parent), name(name), record(record),
		  inline_namespace(inline_namespace), children(), bindings(),
		  types(), namespaces(), namespace_aliases(), values(), using_types(),
		  using_directives(), binding_views(), scope_views(),
		  creation_order(creation_order)
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
	AnonymousUnion
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
		  class_tag(ClassTag::Struct), scoped_enum(false),
		  has_underlying(false), underlying(), template_template(false), scope(),
		  has_generated_identity(false), generated_identity(), dump_scope_view()
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

	SpecFact()
		: base(), has_base(false), anonymous_record(), cv(0),
		  is_typedef(false), is_constexpr(false)
	{}
};

struct ConstValue
{
	bool valid;
	bool is_unsigned;
	__int128 value;

	ConstValue(bool valid = false, __int128 value = 0,
		bool is_unsigned = false)
		: valid(valid), is_unsigned(is_unsigned), value(value)
	{}
};

struct DeclaratorName
{
	bool found;
	NamePath path;

	DeclaratorName() : found(false), path() {}
};

struct DeclaratorOp
{
	enum Kind
	{
		Pointer,
		LvalueReference,
		RvalueReference,
		Array,
		Function
	};

	Kind kind;
	unsigned int cv;
	bool unknown_bound;
	ArrayBound bound;
	const PA10AstNode* parameter_clause;

	DeclaratorOp(Kind kind = Pointer)
		: kind(kind), cv(0), unknown_bound(false), bound(),
		  parameter_clause(NULL)
	{}
};

// PA12 facts are owned by the PA11 model.  The hot representation contains
// only typed IDs, enums, and arena ranges; source text is retained through
// the PA10 node pointer and rendered once at the requested dump boundary.
enum class SemanticFactKind
{
	Variable,
	SimpleDeclaration,
	CompoundStatement,
	ReturnStatement,
	ExpressionStatement,
	CallExpression,
	IdExpression,
	Literal,
	UnaryExpression,
	PostfixExpression,
	BinaryExpression,
	AssignmentExpression,
	ConditionalExpression,
	CastExpression,
	SubscriptExpression,
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
	ContinueStatement
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
	PointerToBool
};

struct ConversionFact
{
	TypeId source;
	TypeId target;
	ConversionKind kind;
	unsigned int rank;

	ConversionFact(TypeId source = TypeId(), TypeId target = TypeId(),
		ConversionKind kind = ConversionKind::Identity, unsigned int rank = 0)
		: source(source), target(target), kind(kind), rank(rank)
	{}
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

struct SemanticFact
{
	SemanticFactKind kind;
	SemanticValueCategory category;
	TypeId type;
	BindingId binding;
	BindingId selected_binding;
	ScopeId selected_scope;
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
	bool has_callee;

	SemanticFact(SemanticFactKind kind = SemanticFactKind::Variable,
		TypeId type = TypeId(),
		SemanticValueCategory category = SemanticValueCategory::Prvalue,
		const PA10AstNode* source = NULL)
		: kind(kind), category(category), type(type), binding(),
		  selected_binding(), selected_scope(),
		  token(SimpleTokenType::OP_SEMICOLON), source(source),
		  name_begin(0), name_count(0), name_global(false),
		  child_begin(InvalidIdentityValue), child_count(0),
		  conversion_begin(InvalidIdentityValue), conversion_count(0),
		  literal_element_count(0), literal_value(0), has_literal_value(false),
		  literal_value_unsigned(false), literal_value_negative(false),
		  has_callee(false)
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

	DeclarationFact(const PA10AstNode* node = NULL, ScopeId scope = ScopeId())
		: node(node), scope(scope), binding_begin(InvalidIdentityValue),
		  binding_count(0), semantic_begin(InvalidIdentityValue),
		  semantic_count(0)
	{}
};

struct FunctionFact
{
	const PA10AstNode* node;
	ScopeId owner;
	BindingId binding;
	ScopeId function_scope;
	ScopeId body_scope;
	SemanticFactId body_fact;

	FunctionFact(const PA10AstNode* node = NULL, ScopeId owner = ScopeId(),
		BindingId binding = BindingId(), ScopeId function_scope = ScopeId(),
		ScopeId body_scope = ScopeId())
		: node(node), owner(owner), binding(binding),
		  function_scope(function_scope), body_scope(body_scope), body_fact()
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

private:

	const PA10Ast& ast_;
	std::vector<std::string> names_;
	FlatIndex<std::string, NameId, StringHash> name_ids_;
	std::vector<TypeKey> types_;
	FlatIndex<TypeKey, TypeId, TypeKeyHash> type_ids_;
	std::vector<NamedRecord> named_;
	std::vector<Scope> scopes_;
	std::vector<Binding> bindings_;
	ScopeId global_;
	std::vector<ScopeId> deferred_scopes_;
	std::vector<DumpBindingView> dump_binding_views_;
	std::vector<DumpScopeView> dump_scope_views_;
	std::size_t anonymous_union_count_;
	std::size_t creation_order_;
	mutable std::vector<std::uint32_t> lookup_marks_;
	mutable std::uint32_t lookup_generation_;
	mutable std::vector<LookupFrame> lookup_frames_;
	std::vector<DeclarationFact> declaration_facts_;
	FlatIndex<const PA10AstNode*, DeclarationFactId, PointerHash>
		declaration_fact_index_;
	std::vector<BindingId> declaration_bindings_;
	std::vector<FunctionFact> function_facts_;
	FlatIndex<const PA10AstNode*, FunctionFactId, PointerHash>
		function_fact_index_;
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
	std::vector<ConversionFact> conversion_facts_;
	std::vector<SemanticFactId> declaration_semantic_ids_;
	std::vector<NameId> semantic_name_components_;
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
	TypeId make_cv(TypeId child, unsigned int qualifiers)
	;
	TypeId make_pointer(TypeId child, unsigned int qualifiers = 0)
	;
	TypeId make_reference(TypeId child, bool rvalue)
	;
	TypeId make_array(TypeId child, bool unknown_bound, ArrayBound bound)
	;
	TypeId make_function(const std::vector<TypeId>& parameters, bool variadic,
	TypeId result)
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
	NamePath name_path(const PA10AstNode& node)
	;
	bool find_declarator_name(const PA10AstNode& node, NamePath* result)
	;
	DeclaratorName declarator_name(const PA10AstNode& node)
	;
	ClassTag class_tag(const PA10AstNode& node) const
	;
	TypeId named_type(NamedRecordId named)
	;
	NamedRecordId named_record_for_type(TypeId type) const
	;
	ScopeId class_scope_for_type(TypeId type) const
	;
	ScopeId scope_for_type(TypeId type) const
	;
	bool direct_value_exists(ScopeId scope, NameId name) const
	;
	bool direct_namespace_exists(ScopeId scope, NameId name) const
	;
	ScopeId named_namespace(ScopeId parent, NameId name)
	;
	ScopeId create_named_namespace(ScopeId parent, NameId name)
	;
	ScopeId lookup_namespace_here(ScopeId scope, NameId name) const
	;
	void begin_lookup() const
	;
	bool mark_lookup_scope(ScopeId scope) const
	;
	void reset_lookup_frames(LookupGraphKind kind, ScopeId start) const
	;
	ScopeId lookup_namespace_graph(ScopeId start, NameId name) const
	;
	ScopeId lookup_namespace_unqualified(ScopeId start, NameId name) const
	;
	TypeId lookup_type_graph(ScopeId start, NameId name) const
	;
	TypeId lookup_type_unqualified(ScopeId start, NameId name) const
	;
	TypeId lookup_type_qualified(ScopeId scope, NameId name) const
	;
	bool lookup_value_graph(ScopeId start, NameId name,
	std::vector<ValueRef>* result) const
	;
	std::vector<ValueRef> lookup_value_unqualified(ScopeId start, NameId name) const
	;
	std::vector<ValueRef> lookup_value_path(const NamePath& path, ScopeId start) const
	;
	ScopeId resolve_qualifier_scope(const std::vector<NameId>& components,
	ScopeId start) const
	;
	TypeId lookup_type_path(const NamePath& path, ScopeId start) const
	;
	ScopeId resolve_global_qualifier_scope(const std::vector<NameId>& components) const
	;
	ScopeId resolve_namespace_path(const NamePath& path, ScopeId start) const
	;
	BindingId store_binding(ScopeId scope, const Binding& binding,
	std::size_t position = InvalidIdentityValue)
	;
	void add_dump_binding_view(ScopeId scope, BindingId binding)
	;
	const Binding& binding(BindingId id) const
	;
	Binding& binding(BindingId id)
	;
	void append_value_index(ScopeId scope, NameId name, BindingId id)
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
	void finalize_anonymous_record(TypeId type, NameId name, ScopeId owner)
	;
	void inject_anonymous_union(TypeId type, ScopeId owner)
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
	std::int64_t value)
	;
	bool integral_type(FundamentalType type) const
	;
	bool unsigned_type(FundamentalType type) const
	;
	ConstValue literal_constant(const PA10AstNode& node) const
	;
	void check_constant_range(const ConstValue& value) const
	;
	std::size_t type_size(TypeId type) const
	;
	TypeId expression_type(const PA10AstNode& node, ScopeId scope)
	;
	bool enumeration_id(TypeId type) const
	;
	TypeId sizeof_operand_type(const PA10AstNode& node, ScopeId scope)
	;
	ConstValue eval_constexpr(const PA10AstNode& node, ScopeId scope)
	;
	TypeId decltype_type(const PA10AstNode& node, ScopeId scope)
	;
	void add_type_binding(ScopeId scope, NameId name, TypeId type, ClassTag tag,
	bool has_tag)
	;
	BindingId add_type_alias(ScopeId scope, NameId name, TypeId type)
	;
	TypeId normalize_parameter_type(TypeId type)
	;
	TypeId normalize_function_type(TypeId type)
	;
	BindingId add_value(ScopeId scope, NameId name, TypeId type, bool function,
	bool definition = false, bool lexical_view = false)
	;
	ScopeId declaration_scope(const NamePath& path, ScopeId current) const
	;
	SpecFact spec_fact(const PA10AstNode& node, ScopeId scope)
	;
	NamePath class_name(const PA10AstNode& node)
	;
	void process_class_body(const PA10AstNode& node, TypeId type, ScopeId owner)
	;
	TypeId type_from_type_id(const PA10AstNode& node, ScopeId scope)
	;
	DeclaratorOp pointer_op(const PA10AstNode& node)
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
	TypeId apply_declarator(const PA10AstNode& node, TypeId base, ScopeId scope)
	;
	bool ambiguous_call_statement(const PA10AstNode& node, ScopeId scope,
	NamePath* callee, const PA10AstNode** argument)
	;
	const PA10AstNode* top_parameter_clause(const PA10AstNode& node) const
	;
	void process_simple_declaration(const PA10AstNode& node, ScopeId scope)
	;
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
	void prepare_pa12_node(const PA10AstNode& node, ScopeId scope)
	;
	void prepare_pa12_compound(const PA10AstNode& node, ScopeId parent)
	;
	void prepare_pa12_statement(const PA10AstNode& node, ScopeId scope)
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
	TypeId expression_object_type(TypeId type) const
	;
	bool fundamental_of(TypeId type, FundamentalType* result) const
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
	unsigned int integral_rank(TypeId type) const
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
	bool pointer_convertible(TypeId source, TypeId target) const
	;
	bool integer_zero(const PA10AstNode& node) const
	;
	SemanticFactId make_semantic_fact(const SemanticFact& fact)
	;
	void set_semantic_children(SemanticFactId fact,
	const std::vector<SemanticFactId>& children)
	;
	void set_semantic_name(SemanticFactId fact, const NamePath& path)
	;
	ConversionFactId add_conversion(TypeId source, TypeId target,
	ConversionKind kind, unsigned int rank)
	;
	void set_fact_conversion(SemanticFactId fact, ConversionFactId conversion)
	;
	std::string semantic_name(const SemanticFact& fact) const
	;
	std::string qualified_binding_name(ScopeId owner, NameId name) const
	;
	TypeId function_result_type(TypeId type) const
	;
	TypeId callable_function_type(TypeId type) const
	;
	ConversionChoice conversion_for(TypeId source,
	SemanticValueCategory category, TypeId target,
	const PA10AstNode* source_node) const
	;
	const PA10AstNode* target_function_id(const PA10AstNode& node,
	ScopeId scope)
	;
	FunctionIdResolution resolve_function_id_target(const PA10AstNode& node,
	ScopeId scope, TypeId target)
	;
	ExprInfo semantic_id_expression_selected(const PA10AstNode& node,
	ScopeId scope, const FunctionIdResolution& resolution)
	;
	ExprInfo semantic_expression_for_target(const PA10AstNode& node,
	ScopeId scope, TypeId target)
	;
	ExprInfo apply_context_conversion(const ExprInfo& expression,
	TypeId target, const PA10AstNode* source_node)
	;
	TypeId common_integral_type(TypeId left, TypeId right) const
	;
	SemanticFactId make_expression_fact(SemanticFactKind kind, TypeId type,
	SemanticValueCategory category, const PA10AstNode& node,
	const std::vector<SemanticFactId>& children)
	;
	SemanticFactId semantic_literal(const PA10AstNode& node)
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
	ExprInfo semantic_cast_expression(const PA10AstNode& node, ScopeId scope)
	;
	ExprInfo semantic_call_expression(const PA10AstNode& node, ScopeId scope)
	;
	ExprInfo semantic_expression(const PA10AstNode& node, ScopeId scope)
	;
	SemanticFactId semantic_declaration(const PA10AstNode& node, ScopeId scope)
	;
	SemanticFactId semantic_ambiguous_call_statement(const PA10AstNode& node,
	ScopeId scope)
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
		unsigned int loop_depth, unsigned int switch_depth)
	;
	SemanticFactId semantic_statement(const PA10AstNode& node, ScopeId scope,
	const FunctionFact& function, unsigned int loop_depth,
	unsigned int switch_depth, SwitchValidationContext* switch_context)
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
	void dump_pa12_top_node(std::ostream& output, const PA10AstNode& node,
	ScopeId scope, std::size_t depth) const
	;
	std::string render_name_path(const NamePath& path) const
	;
	std::string render_generated_name(const GeneratedIdentity& generated) const
	;
	std::string render_record_name(const NamedRecord& record) const
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
	std::string render_binding_type(const Binding& binding) const
	;
	const char* binding_label(BindingKind kind) const
	;
	void dump_binding(std::ostream& output, const Binding& value,
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
