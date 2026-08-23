#pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "posttoken.h"

// PA10 receives one typed posttoken stream per source file.  The source field
// is a cold presentation spelling, while spelling is the producer-owned
// identity copied into the AST's producer spelling snapshot.
enum class PA10TokenKind
{
	Fixed,
	Identifier,
	Literal,
	UserDefinedLiteral,
	End
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
	New,
	Delete,
	NewArray,
	DeleteArray
};

enum class PA10LinkageKind
{
	Unknown,
	C,
	Cxx
};

struct PA10Token
{
	PA10TokenKind kind;
	SimpleTokenType fixed;
	PPSpellingId spelling;
	std::string source;
	LiteralData literal;
	UserDefinedLiteralData user_defined;

	PA10Token(PA10TokenKind kind = PA10TokenKind::End,
		SimpleTokenType fixed = SimpleTokenType::OP_SEMICOLON,
		PPSpellingId spelling = 0,
		const std::string& source = std::string())
		: kind(kind), fixed(fixed), spelling(spelling), source(source),
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
	ClassSpecifier,
	ClassForwardDeclaration,
	SpecialMemberDeclaration,
	SpecialMemberDefinition,
	ClassKey,
	AccessSpecifier,
	VirtualSpecifier,
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
	std::vector<PPSpellingId> name_parts;
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
	// Linkage labels are classified from the posttoken-decoded literal.  The
	// literal itself remains on this node as the typed payload owner.
	PA10LinkageKind linkage_kind;
	bool has_literal;
	LiteralData literal;
	std::vector<PA10AstNode> children;

	PA10AstNode(PA10NodeKind kind = PA10NodeKind::TranslationUnit)
		: kind(kind), has_token(false),
		  token(SimpleTokenType::OP_SEMICOLON), token_spelling(0),
		  identifier_declspecifier(false), text(0),
		  global_name(false), name_parts(), producer_spelling(0),
		  unqualified_id_kind(PA10UnqualifiedIdKind::None),
		  unqualified_id_token(SimpleTokenType::OP_SEMICOLON),
		  unqualified_id_token_spelling(0), unqualified_id_spelling(0),
		  operator_function_kind(PA10OperatorFunctionKind::None),
		  operator_token(SimpleTokenType::OP_SEMICOLON),
		  operator_presentation_begin(0), operator_presentation_count(0),
		  semantic_child_begin(0), semantic_child_count(0),
		  linkage_kind(PA10LinkageKind::Unknown),
		  has_literal(false), literal(),
		  children()
	{}
};

struct PA10Ast
{
	// This snapshot is the only producer-name storage needed after the
	// PPTokenBuffer/session lifetime ends.  It is not a presentation intern
	// table and its IDs are never reused for synthetic text.
	std::vector<std::string> producer_spellings;
	// Cold renderer text (fixed-token spellings, labels, literal source, and
	// derived operator/destructor labels) has its own expected-O(1) index.
	std::vector<std::string> presentation_spellings;
	std::unordered_map<std::string, PA10StringId> presentation_ids;
	// Cold operator labels are ranges of already-interned presentation IDs;
	// conversion type-ids are sparse semantic children owned by the AST.
	std::vector<PA10StringId> operator_presentation_spellings;
	std::vector<PA10AstNode> semantic_child_nodes;
	PA10AstNode root;

	PA10Ast()
		: producer_spellings(1, std::string()),
		  presentation_spellings(1, std::string()), presentation_ids(),
		  operator_presentation_spellings(), semantic_child_nodes(),
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

	PA10StringId intern_presentation(const std::string& value)
	{
		std::unordered_map<std::string, PA10StringId>::const_iterator found =
			presentation_ids.find(value);
		if (found != presentation_ids.end())
			return found->second;
		const PA10StringId id = presentation_spellings.size();
		presentation_spellings.push_back(value);
		presentation_ids[value] = id;
		return id;
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
