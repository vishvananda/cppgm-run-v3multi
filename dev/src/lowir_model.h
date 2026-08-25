#pragma once

// Typed LowIR model for the PA13 text adapter.
//
// LowIR text is the durable compiler boundary introduced in PA13. This model
// owns the backend-visible facts recovered from that text; source spellings
// remain presentation sidecars and are never semantic lookup keys after
// validation.

#include <stdexcept>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace lowir_model {

static const std::size_t INVALID_ID = std::numeric_limits<std::size_t>::max();

template <typename Tag>
struct Identity
{
  std::size_t index;

  Identity() : index(INVALID_ID) {}
  explicit Identity(std::size_t value) : index(value) {}

  bool valid() const { return index != INVALID_ID; }
  bool operator==(const Identity & other) const { return index == other.index; }
  bool operator!=(const Identity & other) const { return !(*this == other); }
  bool operator<(const Identity & other) const { return index < other.index; }
};

struct SymbolIdentityTag;
struct ValueIdentityTag;
struct SlotIdentityTag;
struct BlockIdentityTag;
struct SpellingIdentityTag;

typedef Identity<SymbolIdentityTag> SymbolId;
typedef Identity<ValueIdentityTag> ValueId;
typedef Identity<SlotIdentityTag> SlotId;
typedef Identity<BlockIdentityTag> BlockId;
typedef Identity<SpellingIdentityTag> SpellingId;

struct ParseError : std::runtime_error
{
  explicit ParseError(const std::string & message)
    : std::runtime_error(message)
  {}
};

struct LowType
{
  enum Kind
  {
    TYPE_INVALID,
    TYPE_VOID,
    TYPE_INTEGER,
    TYPE_FLOAT,
    TYPE_POINTER,
    TYPE_OBJECT
  } kind = TYPE_INVALID;

  enum IntegerKind
  {
    INTEGER_NONE,
    INTEGER_I1,
    INTEGER_I8,
    INTEGER_U8,
    INTEGER_I16,
    INTEGER_U16,
    INTEGER_I32,
    INTEGER_U32,
    INTEGER_I64,
    INTEGER_U64
  } integer_kind = INTEGER_NONE;

  enum FloatKind
  {
    FLOAT_NONE,
    FLOAT_F32,
    FLOAT_F64,
    FLOAT_F80
  } float_kind = FLOAT_NONE;

  std::size_t object_bytes = 0;
  std::size_t object_alignment = 0;

  bool valid() const { return kind != TYPE_INVALID; }
  bool is_void() const { return kind == TYPE_VOID; }
  bool is_integer() const { return kind == TYPE_INTEGER; }
  bool is_float() const { return kind == TYPE_FLOAT; }
  bool is_pointer() const { return kind == TYPE_POINTER; }
  bool is_object() const { return kind == TYPE_OBJECT; }
  bool is_scalar() const { return is_integer() || is_float() || is_pointer(); }
  int integer_width() const;
  std::size_t storage_size() const;
  std::size_t storage_alignment() const;
};

bool operator==(const LowType & left, const LowType & right);
bool operator!=(const LowType & left, const LowType & right);

struct Operand
{
  enum Kind
  {
    OP_TEMP,
    OP_SLOT,
    OP_GLOBAL,
    OP_LABEL,
    OP_INTEGER,
    OP_FLOAT
  } kind = OP_INTEGER;

  // Boundary text is centralized in Program::presentation. Validation fills
  // exactly one typed identity for each named operand.
  SpellingId presentation_id;
  SymbolId symbol_id;
  ValueId value_id;
  SlotId slot_id;
  BlockId block_id;
  long long int_value = 0;
  long double float_value = 0.0L;
  LowType literal_type;
};

enum SymbolRole
{
  SR_NONE,
  SR_ENTRY,
  SR_INIT,
  SR_FINI,
  SR_EH_TOP,
  SR_EH_VALUE,
  SR_EH_TYPE,
  SR_EH_UNHANDLED,
  SR_EH_ALLOCATE_EXCEPTION,
  SR_EH_BEGIN_CATCH,
  SR_EH_CALL_UNEXPECTED,
  SR_EH_CURRENT_EXCEPTION_TYPE,
  SR_EH_END_CATCH,
  SR_EH_RETHROW,
  SR_EH_THROW,
  SR_EH_PERSONALITY,
  SR_EH_RESUME
};

enum LanguageLinkageMode
{
  LLM_DEFAULT,
  LLM_C,
  LLM_CPP
};

enum SymbolBindingMode
{
  SBM_DEFAULT,
  SBM_INTERNAL,
  SBM_STRONG,
  SBM_WEAK
};

enum ParamPassingMode
{
  PPM_DIRECT,
  PPM_INDIRECT_RESULT,
  PPM_BY_ADDRESS,
  PPM_REFERENCE,
  PPM_DECAY
};

enum ParamCaptureMode
{
  PCM_DEFAULT,
  PCM_NOCAPTURE,
  PCM_MAYCAPTURE
};

enum ParamAccessMode
{
  PAM_DEFAULT,
  PAM_NONE,
  PAM_READ,
  PAM_WRITE,
  PAM_READWRITE
};

enum ParamAliasMode
{
  PALM_DEFAULT,
  PALM_NOALIAS
};

enum CallArityMode
{
  CAM_FIXED,
  CAM_VARIADIC,
  CAM_PROTOTYPE_RELAXED
};

enum CallEffectsMode
{
  CFXM_DEFAULT,
  CFXM_READNONE,
  CFXM_READONLY,
  CFXM_READWRITE
};

enum CallUnwindMode
{
  CUM_DEFAULT,
  CUM_MAY,
  CUM_NO
};

enum CallReturnMode
{
  CRM_DEFAULT,
  CRM_RETURNS,
  CRM_NORETURN
};

enum GlobalStorageMode
{
  GSM_DEFAULT,
  GSM_WRITABLE,
  GSM_READONLY,
  GSM_THREAD_LOCAL
};

enum IndexProjectionKind
{
  IPK_NONE,
  IPK_ARRAY_ELEMENT,
  IPK_FIELD,
  IPK_BASE_SUBOBJECT,
  IPK_REFERENCE_FIELD
};

enum UnaryOperator
{
  UOP_INVALID,
  UOP_NEG,
  UOP_NOT,
  UOP_BITNOT,
  UOP_DECAY,
  UOP_BSWAP
};

enum BinaryOperator
{
  BOP_INVALID,
  BOP_ADD,
  BOP_SUB,
  BOP_MUL,
  BOP_DIV,
  BOP_MOD,
  BOP_UDIV,
  BOP_UMOD,
  BOP_AND,
  BOP_OR,
  BOP_XOR,
  BOP_SHL,
  BOP_SHR,
  BOP_USHR
};

enum ComparePredicate
{
  CPP_INVALID,
  CPP_EQ,
  CPP_NE,
  CPP_LT,
  CPP_LE,
  CPP_GT,
  CPP_GE,
  CPP_ULT,
  CPP_ULE,
  CPP_UGT,
  CPP_UGE
};

enum ConversionOperator
{
  COP_INVALID,
  COP_SEXT,
  COP_ZEXT,
  COP_TRUNC,
  COP_SITOFP,
  COP_UITOFP,
  COP_FPTOSI,
  COP_FPTOUI,
  COP_FPEXT,
  COP_FPTRUNC
};

enum AtomicOrder
{
  AO_RELAXED = 0,
  AO_CONSUME = 1,
  AO_ACQUIRE = 2,
  AO_RELEASE = 3,
  AO_ACQ_REL = 4,
  AO_SEQ_CST = 5,
  AO_INVALID = 6
};

struct SymbolMetadata
{
  SymbolRole role = SR_NONE;
  LanguageLinkageMode linkage = LLM_DEFAULT;
  SymbolBindingMode binding = SBM_DEFAULT;
  SpellingId object_symbol_id;
  SpellingId tls_for_name_id;
  SymbolId tls_for_id;
  SpellingId section_segment_id;
  SpellingId section_name_id;
  bool keep_internal_alias = false;
  bool prefer_local_object_binding = false;
  bool object_output_root = false;
  bool object_trivial_lifecycle = false;
  bool force_inline = false;
};

struct FunctionBoundaryMetadata
{
  CallArityMode arity = CAM_FIXED;
  CallEffectsMode effects = CFXM_DEFAULT;
  CallUnwindMode unwind = CUM_DEFAULT;
  CallReturnMode returns = CRM_DEFAULT;
};

struct ParameterMetadata
{
  ParamPassingMode passing = PPM_DIRECT;
  ParamCaptureMode capture = PCM_DEFAULT;
  ParamAccessMode access = PAM_DEFAULT;
  ParamAliasMode alias = PALM_DEFAULT;
};

struct Parameter
{
  ValueId value_id;
  SpellingId name_id;
  LowType type;
  ParameterMetadata metadata;
};

struct InstructionDebugLocation
{
  SpellingId file_id;
  std::size_t line = 0;
  std::size_t column = 0;

  bool present() const
  {
    return file_id.valid() && line != 0 && column != 0;
  }
};

struct GlobalDeclaration
{
  SymbolId symbol_id;
  SpellingId name_id;
  bool has_type = false;
  LowType type;
  GlobalStorageMode storage = GSM_DEFAULT;
  SymbolMetadata metadata;
};

struct GlobalDefinition
{
  struct DataItem
  {
    enum Kind
    {
      ITEM_INTEGER,
      ITEM_ADDR,
      ITEM_ZERO
    } kind = ITEM_INTEGER;

    LowType type;
    Operand literal_operand;
    SpellingId symbol_name_id;
    SymbolId symbol_id;
    long long addr_addend = 0;
    std::size_t zero_bytes = 0;
  };

  SymbolId symbol_id;
  SpellingId name_id;
  bool structured = false;
  GlobalStorageMode storage = GSM_DEFAULT;
  LowType type;
  enum InitKind
  {
    INIT_ZERO,
    INIT_INTEGER,
    INIT_ADDR
  } init_kind = INIT_ZERO;
  Operand init_operand;
  long long addr_addend = 0;
  std::vector<DataItem> data_items;
  SymbolMetadata metadata;
};

struct Instruction
{
  enum Kind
  {
    IK_CONST,
    IK_COPY,
    IK_ADDR,
    IK_LOAD,
    IK_ATOMIC_LOAD,
    IK_STORE,
    IK_ATOMIC_STORE,
    IK_ATOMIC_EXCHANGE,
    IK_INDEX,
    IK_UNARY,
    IK_BINARY,
    IK_CMP,
    IK_CONVERT,
    IK_ATOMIC_ADD_FETCH,
    IK_ATOMIC_COMPARE_EXCHANGE,
    IK_ATOMIC_THREAD_FENCE,
    IK_ATOMIC_SIGNAL_FENCE,
    IK_VA_START,
    IK_VA_ARG,
    IK_STACK_ALLOC,
    IK_CALL,
    IK_COPYOBJ,
    IK_ZEROINIT,
    IK_EH_TRY,
    IK_EH_CLEANUP,
    IK_EH_CLEANUP_CLAUSE,
    IK_EH_CATCH,
    IK_EH_FILTER,
    IK_EH_CATCH_ALL,
    IK_EH_END,
    IK_THROW,
    IK_EXCEPTION,
    IK_EXCEPTION_SELECTOR,
    IK_RESUME,
    IK_JUMP,
    IK_BRANCH,
    IK_SWITCH,
    IK_RETURN
  } kind = IK_CONST;

  SpellingId destination_name_id;
  ValueId dest_id;
  LowType type;
  // Validation assigns the canonical type owned by dest_id. This avoids
  // reconstructing result types from the instruction kind in later passes.
  LowType result_type;
  LowType source_type;
  AtomicOrder atomic_order = AO_INVALID;
  AtomicOrder atomic_failure_order = AO_INVALID;
  UnaryOperator unary_operator = UOP_INVALID;
  BinaryOperator binary_operator = BOP_INVALID;
  ComparePredicate compare_predicate = CPP_INVALID;
  ConversionOperator conversion_operator = COP_INVALID;
  std::size_t byte_count = 0;
  std::size_t byte_alignment = 1;
  IndexProjectionKind index_projection = IPK_NONE;
  Operand first;
  Operand second;
  Operand third;
  std::vector<Operand> args;
  bool call_returns_void = false;
  bool has_call_signature = false;
  SymbolId direct_callee_id;
  std::vector<Parameter> call_params;
  LowType call_return_type;
  FunctionBoundaryMetadata call_boundary;
  InstructionDebugLocation debug_location;
};

struct Block
{
  BlockId block_id;
  SpellingId label_id;
  std::vector<Instruction> instructions;
};

struct Function
{
  SymbolId symbol_id;
  SpellingId name_id;
  std::vector<Parameter> params;
  LowType return_type;
  struct Slot
  {
    SlotId slot_id;
    SpellingId name_id;
    LowType type;
  };
  std::vector<Slot> slots;
  std::vector<Block> blocks;
  ValueId value_begin;
  std::size_t value_count = 0;
  SlotId slot_begin;
  std::size_t slot_count = 0;
  InstructionDebugLocation debug_location;
  FunctionBoundaryMetadata boundary;
  SymbolMetadata metadata;
};

struct FunctionDeclaration
{
  SymbolId symbol_id;
  SpellingId name_id;
  std::vector<Parameter> params;
  LowType return_type;
  FunctionBoundaryMetadata boundary;
  SymbolMetadata metadata;
};

struct ObjectAlias
{
  SpellingId object_name_id;
  SpellingId target_name_id;
  SymbolId target_id;
};

struct ValueRecord
{
  enum ProducerKind
  {
    VALUE_UNDEFINED,
    VALUE_PARAMETER,
    VALUE_INSTRUCTION
  };

  ValueId id;
  const Parameter *parameter = 0;
  const Instruction *instruction = 0;
  SymbolId owner_function_id;
  ProducerKind producer = VALUE_UNDEFINED;
};

struct Program
{
  // One cold table owns all source, literal, external and debug spellings.
  // Hot records carry only SpellingId values into this table.
  std::vector<std::string> presentation;
  std::vector<GlobalDeclaration> global_declarations;
  std::vector<GlobalDefinition> globals;
  std::vector<FunctionDeclaration> function_declarations;
  std::vector<Function> functions;
  std::vector<ObjectAlias> object_aliases;
  std::vector<ValueRecord> values;
};

using LowirType = LowType;
using LowirOperand = Operand;
using LowirParameter = Parameter;
using LowirInstruction = Instruction;
using LowirBlock = Block;
using LowirFunction = Function;
using LowirFunctionDeclaration = FunctionDeclaration;
using LowirGlobalDeclaration = GlobalDeclaration;
using LowirGlobalDefinition = GlobalDefinition;
using LowirObjectAlias = ObjectAlias;
using LowirProgram = Program;

LowirProgram parse_lowir_program_text(const std::string & text,
                                      const std::string & source_name = std::string("<memory>"));
LowirProgram parse_lowir_program_files(const std::vector<std::string> & paths);
std::string serialize_lowir_program(const LowirProgram & program);

}  // namespace lowir_model
