#pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

#include "posttoken.h"

// PA10 receives one typed posttoken stream per source file.  The source field
// is a cold presentation spelling, while spelling is the producer-owned
// identity copied into the AST's producer spelling snapshot.
enum class PA10TokenKind
{
	Fixed,
	// OP_RSHIFT is split at the posttoken -> PA10 boundary so a template
	// parser can consume either one close angle or both nested close angles
	// without losing the shift operator seen by expression parsing.
	RShiftPiece1,
	RShiftPiece2,
	Identifier,
	Literal,
	UserDefinedLiteral,
	End
};

// Contextual fixed vocabulary is classified once at the posttoken -> PA10
// boundary.  The original spelling remains available only for presentation.
enum class PA10ContextualIdentifierKind
{
	None,
	Override,
	Final,
	AttributeIntroducer
};

// These facts describe the grammar's unqualified-id alternatives without
// flattening them into a source spelling.  Presentation pieces remain cold
// sidecar IDs; the typed fields are the semantic owner used by later stages.
enum class PA10UnqualifiedIdKind
{
	None,
	Destructor,
	OperatorFunction
};

enum class PA10OperatorFunctionKind
{
	None,
	Token,
	Subscript,
	Call,
	Conversion,
	Literal,
	New,
	Delete,
	NewArray,
	DeleteArray
};

enum class PA10TemplateArgumentKind
{
	TypeId,
	Expression,
	// Identifier-starting syntax can be either a type-id or an expression;
	// PA10 retains the parsed syntax without making a semantic choice.
	Unresolved
};

// One component of a qualified name. Template arguments are owned by the
// PA10Ast sidecar and addressed by this range; the component itself retains
// only producer identity and fixed syntax facts.
struct PA10NameComponent
{
	PPSpellingId spelling;
	bool template_disambiguator;
	bool has_template_id;
	std::size_t template_argument_begin;
	std::size_t template_argument_count;

	PA10NameComponent()
		: spelling(0), template_disambiguator(false), has_template_id(false),
		  template_argument_begin(0), template_argument_count(0)
	{}
};

struct PA10Token
{
	PA10TokenKind kind;
	PA10ContextualIdentifierKind contextual_identifier;
	SimpleTokenType fixed;
	PPSpellingId spelling;
	std::string source;
	LiteralData literal;
	UserDefinedLiteralData user_defined;

	PA10Token(PA10TokenKind kind = PA10TokenKind::End,
		SimpleTokenType fixed = SimpleTokenType::OP_SEMICOLON,
		PPSpellingId spelling = 0,
		const std::string& source = std::string())
		: kind(kind), contextual_identifier(
			PA10ContextualIdentifierKind::None), fixed(fixed), spelling(spelling),
			source(source),
		  literal(), user_defined()
	{}
};

enum class PA10NodeKind
{
	TranslationUnit,
	EmptyDeclaration,
	SimpleDeclaration,
	NamespaceDefinition,
	InlineMarker,
	NamespaceAliasDefinition,
	LinkageSpecification,
	UsingDirective,
	UsingDeclaration,
	AliasDeclaration,
	Target,
	StaticAssertDeclaration,
	Message,
	DeclSpecifierSeq,
	DeclSpecifier,
	TypeSpecifierSeq,
	TypeSpecifier,
	TypeName,
	DecltypeSpecifier,
	CvQualifier,
	TypeId,
	AbstractDeclarator,
	InitDeclaratorList,
	InitDeclarator,
	Declarator,
	NestedDeclarator,
	Identifier,
	PtrOperator,
	ParameterClause,
	ParameterDeclaration,
	ParameterPack,
	DefaultArgument,
	DefaultTemplateArgument,
	FunctionQualifier,
	NoexceptSpecification,
	RefQualifier,
	TrailingReturnType,
	ArraySuffix,
	Initializer,
	ParenInitializer,
	BracedInitList,
	FunctionDefinition,
	CompoundStatement,
	ReturnStatement,
	ExpressionStatement,
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
	LabeledStatement,
	BreakStatement,
	ContinueStatement,
	GotoStatement,
	ThrowStatement,
	TryBlock,
	Handler,
	ExceptionDeclaration,
	IdExpression,
	Literal,
	KeywordLiteral,
	ParenthesizedExpression,
	CallExpression,
	ArgumentList,
	MemberExpression,
	SubscriptExpression,
	UnaryExpression,
	PostfixExpression,
	BinaryExpression,
	AssignmentExpression,
	ConditionalExpression,
	CastExpression,
	SizeofExpression,
	TypeTraitExpression,
	NewExpression,
	DeleteExpression,
	ArrayDeleteMarker,
	LambdaExpression,
	LambdaIntroducer,
	LambdaDeclarator,
	LambdaSpecifier,
	ClassSpecifier,
	ClassForwardDeclaration,
	SpecialMemberDeclaration,
	SpecialMemberDefinition,
	MemberSpecifiers,
	MemberSpecifier,
	ClassKey,
	AccessSpecifier,
	VirtualSpecifier,
	VirtSpecifier,
	BaseClause,
	BaseSpecifier,
	BaseName,
	BitFieldDeclaration,
	BitFieldDeclarator,
	EnumSpecifier,
	EnumKey,
	Enumerator,
	TemplateDeclaration,
	TemplateParameterClause,
	TemplateParameterList,
	TypeParameter,
	NonTypeTemplateParameter,
	TemplateTemplateParameter,
	ParameterKey,
	ExplicitInstantiationDeclaration,
	ExplicitSpecializationDeclaration,
	CtorInitializer,
	MemInitializer,
	MemInitializerId,
	ParenArgumentList,
	SpecialInitializer,
	LeafFixed
};

typedef std::size_t PA10StringId;

// Shared parser/renderer structural recursion ceiling.  The parser also has
// a separate linear token-work budget; this ceiling protects call-stack paths
// that cannot be characterized by token consumption alone.
static const std::size_t PA10_MAX_AST_NESTING = 1024;

// This is the canonical PA10 syntax tree.  Names are component IDs, fixed
// syntax is an enum plus a cold source spelling, and literals retain decoded
// PA2 data alongside their source spelling.  Children describe grammar
// structure rather than an opaque source span.
struct PA10AstNode
{
	PA10NodeKind kind;
	bool has_token;
	SimpleTokenType token;
	PA10StringId token_spelling;
	bool identifier_declspecifier;
	PA10StringId text;
	bool global_name;
	// Qualified-name components retain producer identity.  The renderer
	// resolves these IDs through PA10Ast::producer_spellings on demand.
	std::vector<PA10NameComponent> name_parts;
	// A nested-name-specifier may be rooted in one typed decltype-specifier;
	// the syntax node lives in PA10Ast's sidecar and is never flattened.
	std::size_t name_prefix_begin;
	std::size_t name_prefix_count;
	// A single source identifier attached to a presentation-labelled node.
	// Synthetic labels leave this zero and use text only.
	PPSpellingId producer_spelling;
	PA10UnqualifiedIdKind unqualified_id_kind;
	SimpleTokenType unqualified_id_token;
	PA10StringId unqualified_id_token_spelling;
	PPSpellingId unqualified_id_spelling;
	PA10OperatorFunctionKind operator_function_kind;
	SimpleTokenType operator_token;
	// Ranges into PA10Ast's cold/operator and semantic sidecars.  The ranges
	// keep presentation and conversion payload out of every hot node while
	// preserving one owner for each fact.
	std::size_t operator_presentation_begin;
	std::size_t operator_presentation_count;
	std::size_t semantic_child_begin;
	std::size_t semantic_child_count;
	bool has_literal;
	LiteralData literal;
	std::vector<PA10AstNode> children;

	PA10AstNode(PA10NodeKind kind = PA10NodeKind::TranslationUnit)
		: kind(kind), has_token(false),
		  token(SimpleTokenType::OP_SEMICOLON), token_spelling(0),
		  identifier_declspecifier(false), text(0),
		  global_name(false), name_parts(), name_prefix_begin(0),
		  name_prefix_count(0),
		  producer_spelling(0),
		  unqualified_id_kind(PA10UnqualifiedIdKind::None),
		  unqualified_id_token(SimpleTokenType::OP_SEMICOLON),
		  unqualified_id_token_spelling(0), unqualified_id_spelling(0),
		  operator_function_kind(PA10OperatorFunctionKind::None),
		  operator_token(SimpleTokenType::OP_SEMICOLON),
		  operator_presentation_begin(0), operator_presentation_count(0),
		  semantic_child_begin(0), semantic_child_count(0),
		  has_literal(false), literal(),
		  children()
	{}
};

struct PA10TemplateArgument
{
	PA10TemplateArgumentKind kind;
	PA10AstNode syntax;

	PA10TemplateArgument(PA10TemplateArgumentKind kind =
		PA10TemplateArgumentKind::Expression,
		const PA10AstNode& syntax = PA10AstNode())
		: kind(kind), syntax(syntax)
	{}
};

struct PA10Ast
{
	// This snapshot is the only producer-name storage needed after the
	// PPTokenBuffer/session lifetime ends.  It is not a presentation intern
	// table and its IDs are never reused for synthetic text.
	std::vector<std::string> producer_spellings;
	// Cold renderer text (fixed-token spellings, labels, literal source, and
	// derived operator/destructor labels) survives as a deduplicated vector.
	std::vector<std::string> presentation_spellings;
	// Cold operator labels are ranges of already-interned presentation IDs;
	// conversion type-ids are sparse semantic children owned by the AST.
	std::vector<PA10StringId> operator_presentation_spellings;
	std::vector<PA10AstNode> semantic_child_nodes;
	// Template arguments are structured syntax owners. Name components refer
	// to this vector by range instead of retaining a flattened spelling.
	std::vector<PA10TemplateArgument> template_arguments;
	std::vector<PA10AstNode> name_prefix_nodes;
	PA10AstNode root;

	PA10Ast()
		: producer_spellings(1, std::string()),
		  presentation_spellings(1, std::string()),
		  operator_presentation_spellings(), semantic_child_nodes(),
		  template_arguments(),
		  name_prefix_nodes(),
		  root(PA10NodeKind::TranslationUnit)
	{}

	const std::string& spelling(PA10StringId id) const
	{
		return presentation_spellings.at(id);
	}

	const std::string& producer_spelling(PPSpellingId id) const
	{
		return producer_spellings.at(id);
	}

	void snapshot_producer_spellings(const PPSpellingTable& source)
	{
		producer_spellings = source.values;
		if (producer_spellings.empty())
			producer_spellings.push_back(std::string());
	}
};

// Parse the canonical phase-3 buffer through posttoken facts and build one
// structured PA10 syntax owner.  Syntax errors and bounded-work violations
// are reported as std::runtime_error.
PA10Ast parse_pa10_ast(const PPTokenBuffer& input);

// Render the requested PA10 cold text boundary.  The renderer never feeds its
// output back into parsing or semantic ownership.
void render_pa10_ast(const PA10Ast& ast, std::ostream& output);
