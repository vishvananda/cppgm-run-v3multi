#pragma once

// ABI fact model for the PA14 standalone abimangle tool.

#include <cstddef>
#include <string>
#include <vector>

namespace abi_mangle {

static const std::size_t ABI_INVALID_DEFINITION_ID =
  static_cast<std::size_t>(-1);

// A qualified name is a semantic sequence of source-name components.  The
// line-oriented adapter may receive one `a::b` token, but the encoder never
// stores or reparses that joined spelling.
struct AbiQualifiedName
{
  std::vector<std::string> components;
};

// Definition identity is a dense, per-case canonical index.  Source spelling
// belongs to the line adapter's cold diagnostic sidecar, not to this reusable
// semantic model.
struct AbiDefinitionId
{
  std::size_t index = ABI_INVALID_DEFINITION_ID;
};

enum AbiFactRecordKind
{
  ABI_FACT_RECORD_DEFINITION,
  ABI_FACT_RECORD_TARGET,
  ABI_FACT_RECORD_FUNCTION
};

enum AbiDefinitionKind
{
  ABI_DEFINITION_TYPE,
  ABI_DEFINITION_TEMPLATE_ARGUMENT,
  ABI_DEFINITION_EXPRESSION,
  ABI_DEFINITION_CONTEXT,
  ABI_DEFINITION_ENTITY
};

enum AbiTypeKind
{
  ABI_TYPE_NAME_OR_REFERENCE,
  ABI_TYPE_NAMED,
  ABI_TYPE_BUILTIN,
  ABI_TYPE_TEMPLATE_PARAMETER,
  ABI_TYPE_POINTER,
  ABI_TYPE_LVALUE_REFERENCE,
  ABI_TYPE_RVALUE_REFERENCE,
  ABI_TYPE_CV,
  ABI_TYPE_PACK_EXPANSION,
  ABI_TYPE_VENDOR_QUALIFIED,
  ABI_TYPE_ARRAY,
  ABI_TYPE_BUILTIN_TRANSFORM,
  ABI_TYPE_FUNCTION,
  ABI_TYPE_MEMBER_POINTER,
  ABI_TYPE_TEMPLATE_SPECIALIZATION,
  ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION,
  ABI_TYPE_STD_TEMPLATE_SPECIALIZATION,
  ABI_TYPE_MEMBER,
  ABI_TYPE_MEMBER_TEMPLATE_SPECIALIZATION,
  ABI_TYPE_DECLTYPE_EXPRESSION,
  ABI_TYPE_LAMBDA_CLOSURE,
  ABI_TYPE_LOCAL_TYPE,
  ABI_TYPE_NAMESPACE_LAMBDA
};

enum AbiBuiltinKind
{
  ABI_BUILTIN_INVALID,
  ABI_BUILTIN_VOID,
  ABI_BUILTIN_WCHAR,
  ABI_BUILTIN_BOOL,
  ABI_BUILTIN_CHAR,
  ABI_BUILTIN_SIGNED_CHAR,
  ABI_BUILTIN_UNSIGNED_CHAR,
  ABI_BUILTIN_SHORT,
  ABI_BUILTIN_UNSIGNED_SHORT,
  ABI_BUILTIN_INT,
  ABI_BUILTIN_UNSIGNED_INT,
  ABI_BUILTIN_LONG,
  ABI_BUILTIN_UNSIGNED_LONG,
  ABI_BUILTIN_LONG_LONG,
  ABI_BUILTIN_UNSIGNED_LONG_LONG,
  ABI_BUILTIN_INT128,
  ABI_BUILTIN_UNSIGNED_INT128,
  ABI_BUILTIN_FLOAT,
  ABI_BUILTIN_DOUBLE,
  ABI_BUILTIN_LONG_DOUBLE,
  ABI_BUILTIN_FLOAT128,
  ABI_BUILTIN_ELLIPSIS,
  ABI_BUILTIN_CHAR16,
  ABI_BUILTIN_CHAR32,
  ABI_BUILTIN_CHAR8,
  ABI_BUILTIN_NULLPTR,
  ABI_BUILTIN_COMPLEX_FLOAT,
  ABI_BUILTIN_COMPLEX_DOUBLE,
  ABI_BUILTIN_COMPLEX_LONG_DOUBLE
};

enum AbiLinkageKind
{
  ABI_LINKAGE_CXX,
  ABI_LINKAGE_C
};

// The Itanium ABI's standard substitutions are a fixed vocabulary.  Keep the
// vocabulary typed so substitution identity does not depend on a rendered
// ``Sx`` fragment.
enum AbiStandardSubstitutionKind
{
  ABI_STANDARD_SUBSTITUTION_NONE,
  ABI_STANDARD_SUBSTITUTION_ALLOCATOR,
  ABI_STANDARD_SUBSTITUTION_BASIC_STRING,
  ABI_STANDARD_SUBSTITUTION_STRING,
  ABI_STANDARD_SUBSTITUTION_ISTREAM,
  ABI_STANDARD_SUBSTITUTION_OSTREAM,
  ABI_STANDARD_SUBSTITUTION_IOSTREAM
};

enum AbiArrayBoundKind
{
  ABI_ARRAY_BOUND_VALUE,
  ABI_ARRAY_BOUND_RAW,
  ABI_ARRAY_BOUND_EXPRESSION
};

enum AbiTemplateArgumentKind
{
  ABI_TEMPLATE_ARGUMENT_TYPE,
  ABI_TEMPLATE_ARGUMENT_VALUE,
  ABI_TEMPLATE_ARGUMENT_DEPENDENT_VALUE,
  ABI_TEMPLATE_ARGUMENT_UNTYPED_VALUE,
  ABI_TEMPLATE_ARGUMENT_EXPRESSION,
  ABI_TEMPLATE_ARGUMENT_TEMPLATE_ENTITY,
  ABI_TEMPLATE_ARGUMENT_MEMBER_TEMPLATE_ENTITY,
  ABI_TEMPLATE_ARGUMENT_TEMPLATE_PARAMETER_TEMPLATE,
  ABI_TEMPLATE_ARGUMENT_EXTERNAL_ENTITY,
  ABI_TEMPLATE_ARGUMENT_MEMBER_EXTERNAL_ENTITY,
  ABI_TEMPLATE_ARGUMENT_ENTITY,
  ABI_TEMPLATE_ARGUMENT_PACK
};

enum AbiExpressionKind
{
  ABI_EXPRESSION_TEMPLATE_PARAMETER,
  ABI_EXPRESSION_FUNCTION_PARAMETER,
  ABI_EXPRESSION_LITERAL,
  ABI_EXPRESSION_INTEGRAL_VALUE,
  ABI_EXPRESSION_UNARY,
  ABI_EXPRESSION_BINARY,
  ABI_EXPRESSION_CONDITIONAL,
  ABI_EXPRESSION_PACK_EXPANSION,
  ABI_EXPRESSION_CALL,
  ABI_EXPRESSION_CONVERSION,
  ABI_EXPRESSION_CAST,
  ABI_EXPRESSION_TEMPLATE_ID,
  ABI_EXPRESSION_TYPE_TRAIT,
  ABI_EXPRESSION_SIZEOF_TYPE,
  ABI_EXPRESSION_MEMBER,
  ABI_EXPRESSION_OBJECT_MEMBER,
  ABI_EXPRESSION_EXTERNAL_ENTITY,
  ABI_EXPRESSION_ENTITY
};

enum AbiContextFactKind
{
  ABI_CONTEXT_RAW,
  ABI_CONTEXT_FUNCTION
};

enum AbiEntityFactKind
{
  ABI_ENTITY_FACT_FUNCTION,
  ABI_ENTITY_FACT_VARIABLE,
  ABI_ENTITY_FACT_SYMBOL
};

enum AbiTargetFactKind
{
  ABI_TARGET_FACT_TYPE,
  ABI_TARGET_FACT_FUNCTION,
  ABI_TARGET_FACT_VARIABLE,
  ABI_TARGET_FACT_TYPEINFO,
  ABI_TARGET_FACT_VTABLE,
  ABI_TARGET_FACT_VTT,
  ABI_TARGET_FACT_CONSTRUCTION_VTABLE,
  ABI_TARGET_FACT_THREAD_LOCAL_WRAPPER,
  ABI_TARGET_FACT_THUNK,
  ABI_TARGET_FACT_VIRTUAL_BASE_THUNK
};

enum AbiFunctionTargetKind
{
  ABI_FUNCTION_TARGET_PATH,
  ABI_FUNCTION_TARGET_ENCODING,
  ABI_FUNCTION_TARGET_LAMBDA,
  ABI_FUNCTION_TARGET_LOCAL,
  ABI_FUNCTION_TARGET_NAMESPACE_LAMBDA
};

// Operator terminals are semantic ABI facts.  The line adapter maps their
// source vocabulary to this enum; the encoder owns the Itanium spellings.
enum AbiOperatorTerminalKind
{
  ABI_OPERATOR_TERMINAL_NONE,
  ABI_OPERATOR_TERMINAL_NEW,
  ABI_OPERATOR_TERMINAL_NEW_ARRAY,
  ABI_OPERATOR_TERMINAL_DELETE,
  ABI_OPERATOR_TERMINAL_DELETE_ARRAY,
  ABI_OPERATOR_TERMINAL_UNARY_PLUS,
  ABI_OPERATOR_TERMINAL_BINARY_PLUS,
  ABI_OPERATOR_TERMINAL_PLUS,
  ABI_OPERATOR_TERMINAL_UNARY_MINUS,
  ABI_OPERATOR_TERMINAL_BINARY_MINUS,
  ABI_OPERATOR_TERMINAL_MINUS,
  ABI_OPERATOR_TERMINAL_ADDRESS_OF,
  ABI_OPERATOR_TERMINAL_DEREF,
  ABI_OPERATOR_TERMINAL_COMPLEMENT,
  ABI_OPERATOR_TERMINAL_MULTIPLY,
  ABI_OPERATOR_TERMINAL_DIVIDE,
  ABI_OPERATOR_TERMINAL_REMAINDER,
  ABI_OPERATOR_TERMINAL_BIT_AND,
  ABI_OPERATOR_TERMINAL_BIT_OR,
  ABI_OPERATOR_TERMINAL_BIT_XOR,
  ABI_OPERATOR_TERMINAL_ASSIGN,
  ABI_OPERATOR_TERMINAL_PLUS_ASSIGN,
  ABI_OPERATOR_TERMINAL_MINUS_ASSIGN,
  ABI_OPERATOR_TERMINAL_MULTIPLY_ASSIGN,
  ABI_OPERATOR_TERMINAL_DIVIDE_ASSIGN,
  ABI_OPERATOR_TERMINAL_REMAINDER_ASSIGN,
  ABI_OPERATOR_TERMINAL_BIT_AND_ASSIGN,
  ABI_OPERATOR_TERMINAL_BIT_OR_ASSIGN,
  ABI_OPERATOR_TERMINAL_BIT_XOR_ASSIGN,
  ABI_OPERATOR_TERMINAL_LEFT_SHIFT,
  ABI_OPERATOR_TERMINAL_RIGHT_SHIFT,
  ABI_OPERATOR_TERMINAL_LEFT_SHIFT_ASSIGN,
  ABI_OPERATOR_TERMINAL_RIGHT_SHIFT_ASSIGN,
  ABI_OPERATOR_TERMINAL_EQUAL,
  ABI_OPERATOR_TERMINAL_NOT_EQUAL,
  ABI_OPERATOR_TERMINAL_LESS,
  ABI_OPERATOR_TERMINAL_GREATER,
  ABI_OPERATOR_TERMINAL_LESS_EQUAL,
  ABI_OPERATOR_TERMINAL_GREATER_EQUAL,
  ABI_OPERATOR_TERMINAL_LOGICAL_NOT,
  ABI_OPERATOR_TERMINAL_LOGICAL_AND,
  ABI_OPERATOR_TERMINAL_LOGICAL_OR,
  ABI_OPERATOR_TERMINAL_INCREMENT,
  ABI_OPERATOR_TERMINAL_DECREMENT,
  ABI_OPERATOR_TERMINAL_COMMA,
  ABI_OPERATOR_TERMINAL_MEMBER_POINTER,
  ABI_OPERATOR_TERMINAL_ARROW,
  ABI_OPERATOR_TERMINAL_CALL,
  ABI_OPERATOR_TERMINAL_INDEX,
  ABI_OPERATOR_TERMINAL_LITERAL
};

enum AbiFunctionPathOperandKind
{
  ABI_FUNCTION_PATH_TYPE,
  ABI_FUNCTION_PATH_TEMPLATE_ARGUMENT,
  ABI_FUNCTION_PATH_RESULT_TYPE,
  ABI_FUNCTION_PATH_VARIADIC
};

enum AbiFunctionRecordKind
{
  ABI_FUNCTION_RECORD_NAME_SOURCE,
  ABI_FUNCTION_RECORD_NAME_STD,
  ABI_FUNCTION_RECORD_NAME_TEMPLATE,
  ABI_FUNCTION_RECORD_FUNCTION_TEMPLATE_ARGUMENT,
  ABI_FUNCTION_RECORD_FUNCTION_TEMPLATE_PREFIX,
  ABI_FUNCTION_RECORD_LOCAL_CONTEXT,
  ABI_FUNCTION_RECORD_LAMBDA_CONTEXT,
  ABI_FUNCTION_RECORD_NAMESPACE_LAMBDA_CONTEXT,
  ABI_FUNCTION_RECORD_TERMINAL_SOURCE,
  ABI_FUNCTION_RECORD_TERMINAL,
  ABI_FUNCTION_RECORD_VARIADIC,
  ABI_FUNCTION_RECORD_ABI_TAG,
  ABI_FUNCTION_RECORD_QUALIFIER,
  ABI_FUNCTION_RECORD_OPERATOR_TERMINAL,
  ABI_FUNCTION_RECORD_CONVERSION_TERMINAL,
  ABI_FUNCTION_RECORD_PARAMETER,
  ABI_FUNCTION_RECORD_RESULT
};

enum AbiFunctionSpecialTerminalKind
{
  ABI_SPECIAL_TERMINAL_NONE,
  ABI_SPECIAL_TERMINAL_CONSTRUCTOR_COMPLETE,
  ABI_SPECIAL_TERMINAL_CONSTRUCTOR_BASE,
  ABI_SPECIAL_TERMINAL_CONSTRUCTOR_ALLOCATING,
  ABI_SPECIAL_TERMINAL_DESTRUCTOR_DELETING,
  ABI_SPECIAL_TERMINAL_DESTRUCTOR_COMPLETE,
  ABI_SPECIAL_TERMINAL_DESTRUCTOR_BASE
};

enum AbiFunctionQualifier
{
  ABI_FUNCTION_QUALIFIER_CONST,
  ABI_FUNCTION_QUALIFIER_VOLATILE,
  ABI_FUNCTION_QUALIFIER_LVALUE_REFERENCE,
  ABI_FUNCTION_QUALIFIER_RVALUE_REFERENCE
};

struct AbiArrayBound
{
  AbiArrayBoundKind kind = ABI_ARRAY_BOUND_VALUE;
  std::size_t value = 0;
  std::string raw;
  AbiDefinitionId expression_ref;
};

struct AbiType
{
  AbiTypeKind kind = ABI_TYPE_NAME_OR_REFERENCE;
  AbiBuiltinKind builtin = ABI_BUILTIN_INVALID;
  AbiQualifiedName name;
  AbiDefinitionId definition_ref;
  std::string substitution;
  std::string standard_substitution;
  AbiStandardSubstitutionKind standard_substitution_kind =
    ABI_STANDARD_SUBSTITUTION_NONE;
  AbiDefinitionId expression_ref;
  AbiDefinitionId context_ref;
  std::string discriminator;
  AbiArrayBound array_bound;
  std::size_t index = 0;
  bool is_const = false;
  bool is_volatile = false;
  bool variadic = false;
  bool lvalue_ref = false;
  bool rvalue_ref = false;
  bool substitutable = false;
  bool standard_substitution_includes_arguments = false;
  std::vector<AbiType> types;
  std::vector<AbiDefinitionId> argument_refs;
  std::vector<std::string> namespace_qualifiers;
  std::vector<std::string> abi_tags;
};

struct AbiTemplateArgument
{
  AbiTemplateArgumentKind kind = ABI_TEMPLATE_ARGUMENT_TYPE;
  AbiType type;
  AbiType value_type;
  AbiType owner_type;
  AbiQualifiedName name;
  std::string substitution;
  AbiDefinitionId entity_ref;
  std::string symbol;
  long long value = 0;
  std::size_t index = 0;
  bool has_value_type = false;
  bool address_of = false;
  bool member_is_function = false;
  bool member_function_const = false;
  bool member_function_volatile = false;
  bool member_function_lvalue_ref = false;
  bool member_function_rvalue_ref = false;
  bool member_function_variadic = false;
  std::vector<AbiType> parameter_types;
  std::vector<AbiDefinitionId> argument_refs;
};

struct AbiDependentExpression
{
  AbiExpressionKind kind = ABI_EXPRESSION_LITERAL;
  AbiType type;
  AbiType value_type;
  std::string text;
  std::string op;
  AbiDefinitionId entity_ref;
  long long value = 0;
  std::size_t index = 0;
  bool close_member_owner = false;
  bool address_of = false;
  std::vector<AbiDefinitionId> expression_refs;
  std::vector<AbiDefinitionId> argument_refs;
  std::vector<AbiType> type_arguments;
};

struct AbiFunctionPathOperand
{
  AbiFunctionPathOperandKind kind = ABI_FUNCTION_PATH_TYPE;
  AbiType type;
  AbiDefinitionId argument_ref;
};

struct AbiFunctionTarget
{
  AbiFunctionTargetKind kind = ABI_FUNCTION_TARGET_PATH;
  AbiQualifiedName name;
  AbiDefinitionId context_ref;
  std::string source_name;
  std::string discriminator;
  // Only an ordinary source terminal is stored here.  Fixed operator and
  // special-member vocabulary is represented by the typed enums below and
  // rendered by the encoder or cold serializer when needed.
  std::string terminal;
  AbiFunctionSpecialTerminalKind special_terminal = ABI_SPECIAL_TERMINAL_NONE;
  AbiOperatorTerminalKind operator_terminal = ABI_OPERATOR_TERMINAL_NONE;
  std::vector<AbiFunctionPathOperand> path_operands;
  std::vector<AbiType> signature_parameter_types;
  std::vector<std::string> namespace_qualifiers;
};

struct AbiLocalContext
{
  AbiContextFactKind kind = ABI_CONTEXT_RAW;
  std::string fragment;
  AbiFunctionTarget function;
};

struct AbiEntityFact
{
  AbiEntityFactKind kind = ABI_ENTITY_FACT_VARIABLE;
  AbiQualifiedName name;
  AbiFunctionTarget function;
  bool internal_linkage = false;
};

struct AbiDefinitionRecord
{
  AbiDefinitionKind kind = ABI_DEFINITION_TYPE;
  AbiDefinitionId id;
  AbiType type;
  AbiTemplateArgument template_argument;
  AbiDependentExpression expression;
  AbiLocalContext context;
  AbiEntityFact entity;
};

struct AbiTargetRecord
{
  AbiTargetFactKind kind = ABI_TARGET_FACT_TYPE;
  AbiLinkageKind linkage = ABI_LINKAGE_CXX;
  AbiType type;
  AbiType base_type;
  AbiFunctionTarget function;
  AbiQualifiedName name;
  unsigned long long base_offset = 0;
  long long this_adjust = 0;
  bool has_result_adjust = false;
  long long result_adjust = 0;
  bool result_adjust_virtual = false;
  long long result_vcall_offset = 0;
  long long vcall_offset = 0;
};

struct AbiFunctionRecord
{
  AbiFunctionRecordKind kind = ABI_FUNCTION_RECORD_PARAMETER;
  std::string name;
  std::string substitution;
  std::string complete_substitution;
  std::string standard_substitution;
  AbiStandardSubstitutionKind standard_substitution_kind =
    ABI_STANDARD_SUBSTITUTION_NONE;
  bool standard_substitution_includes_arguments = false;
  // The function-template-prefix spelling is adapter metadata.  These typed
  // fields preserve its declared terminal shape without making the encoder
  // compare rendered ABI text.
  bool has_function_template_prefix = false;
  AbiQualifiedName function_template_prefix_name;
  AbiOperatorTerminalKind function_template_prefix_operator =
    ABI_OPERATOR_TERMINAL_NONE;
  bool function_template_prefix_conversion = false;
  AbiDefinitionId context_ref;
  AbiQualifiedName source_name;
  std::string discriminator;
  // Ordinary source-terminal text only; typed operator, conversion, and
  // special-member terminals do not duplicate their vocabulary here.
  std::string terminal;
  std::string literal_suffix;
  AbiType type;
  AbiType conversion_type;
  bool has_conversion_type = false;
  AbiOperatorTerminalKind operator_terminal = ABI_OPERATOR_TERMINAL_NONE;
  std::vector<AbiType> types;
  std::vector<AbiDefinitionId> argument_refs;
  std::vector<std::string> namespace_qualifiers;
  std::vector<AbiFunctionQualifier> qualifiers;
  AbiFunctionSpecialTerminalKind special_terminal = ABI_SPECIAL_TERMINAL_NONE;
};

struct AbiFactRecord
{
  AbiFactRecordKind kind = ABI_FACT_RECORD_TARGET;
  AbiDefinitionRecord definition;
  AbiTargetRecord target;
  AbiFunctionRecord function;
};

struct AbiFactCase
{
  std::string label;
  // Adapter-only labels for diagnostics and optional round-tripping.  The
  // encoder uses only AbiDefinitionId::index; labels are never semantic IDs.
  std::vector<std::string> definition_labels;
  std::vector<AbiFactRecord> records;
};

struct AbiFactFile
{
  std::vector<AbiFactCase> cases;
};

AbiFactRecord parse_fact_record_words(const std::vector<std::string> & words);
AbiFactFile parse_fact_text(const std::string & text);
std::string serialize_fact_file(const AbiFactFile & file);
std::string mangle_abi_fact_case(const AbiFactCase & fact_case);
std::string mangle_fact_file(const AbiFactFile & file);
std::string mangle_fact_files(const std::vector<std::string> & input_paths);

}  // namespace abi_mangle
