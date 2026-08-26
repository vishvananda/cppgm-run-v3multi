#include "lowir_model.h"

#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace lowir_model {

int LowType::integer_width() const
{
  switch (integer_kind) {
  case INTEGER_I1: return 1;
  case INTEGER_I8:
  case INTEGER_U8: return 8;
  case INTEGER_I16:
  case INTEGER_U16: return 16;
  case INTEGER_I32:
  case INTEGER_U32: return 32;
  case INTEGER_I64:
  case INTEGER_U64: return 64;
  default: return 0;
  }
}

std::size_t LowType::storage_size() const
{
  if (kind == TYPE_OBJECT) return object_bytes;
  if (kind == TYPE_FLOAT) {
    if (float_kind == FLOAT_F32) return 4;
    if (float_kind == FLOAT_F64) return 8;
    if (float_kind == FLOAT_F80) return 16;
  }
  if (kind == TYPE_POINTER) return 8;
  if (kind == TYPE_INTEGER) {
    const int width = integer_width();
    return width <= 8 ? 1 : width <= 16 ? 2 : width <= 32 ? 4 : width == 64 ? 8 : 0;
  }
  return 0;
}

std::size_t LowType::storage_alignment() const
{
  if (kind == TYPE_OBJECT) return object_alignment;
  if (kind == TYPE_FLOAT && float_kind == FLOAT_F80) return 8;
  const std::size_t size = storage_size();
  return size >= 8 ? 8 : size;
}

bool operator==(const LowType &left, const LowType &right)
{
  if (left.kind != right.kind) return false;
  if (left.kind == LowType::TYPE_INTEGER) return left.integer_kind == right.integer_kind;
  if (left.kind == LowType::TYPE_FLOAT) return left.float_kind == right.float_kind;
  if (left.kind == LowType::TYPE_OBJECT)
    return left.object_bytes == right.object_bytes && left.object_alignment == right.object_alignment;
  return true;
}

bool operator!=(const LowType &left, const LowType &right) { return !(left == right); }

namespace {

const std::string &spelling(const Program &program, SpellingId id)
{
  if (!id.valid() || id.index >= program.presentation.size())
    throw std::runtime_error("LowIR serializer: invalid presentation identity");
  return program.presentation[id.index];
}

std::string type_text(const LowType &type)
{
  if (type.kind == LowType::TYPE_VOID) return "void";
  if (type.kind == LowType::TYPE_POINTER) return "ptr";
  if (type.kind == LowType::TYPE_OBJECT) {
    if (type.object_bytes == 0 || type.object_alignment == 0)
      throw std::runtime_error("LowIR serializer: invalid object type");
    std::ostringstream out;
    out << "obj<" << type.object_bytes << "x" << type.object_alignment << ">";
    return out.str();
  }
  if (type.kind == LowType::TYPE_FLOAT) {
    switch (type.float_kind) {
    case LowType::FLOAT_F32: return "f32";
    case LowType::FLOAT_F64: return "f64";
    case LowType::FLOAT_F80: return "f80";
    default: throw std::runtime_error("LowIR serializer: invalid floating type");
    }
  }
  if (type.kind == LowType::TYPE_INTEGER) {
    switch (type.integer_kind) {
    case LowType::INTEGER_I1: return "i1";
    case LowType::INTEGER_I8: return "i8";
    case LowType::INTEGER_U8: return "u8";
    case LowType::INTEGER_I16: return "i16";
    case LowType::INTEGER_U16: return "u16";
    case LowType::INTEGER_I32: return "i32";
    case LowType::INTEGER_U32: return "u32";
    case LowType::INTEGER_I64: return "i64";
    case LowType::INTEGER_U64: return "u64";
    default: throw std::runtime_error("LowIR serializer: invalid integer type");
  }
  }
  throw std::runtime_error("LowIR serializer: invalid type state");
}

std::string operand_text(const Program &program, const Operand &operand)
{
  if (operand.presentation_id.valid()) return spelling(program, operand.presentation_id);
  std::ostringstream out;
  switch (operand.kind) {
  case Operand::OP_INTEGER: out << operand.int_value; break;
  case Operand::OP_FLOAT:
    out << std::setprecision(20) << static_cast<long double>(operand.float_value);
    break;
  default: throw std::runtime_error("LowIR serializer: named operand lacks presentation identity");
  }
  return out.str();
}

const char *role_text(SymbolRole role)
{
  switch (role) {
  case SR_ENTRY: return "entry";
  case SR_INIT: return "init";
  case SR_FINI: return "fini";
  case SR_EH_TOP: return "eh_top";
  case SR_EH_VALUE: return "eh_value";
  case SR_EH_TYPE: return "eh_type";
  case SR_EH_UNHANDLED: return "eh_unhandled";
  case SR_EH_ALLOCATE_EXCEPTION: return "eh_allocate_exception";
  case SR_EH_BEGIN_CATCH: return "eh_begin_catch";
  case SR_EH_CALL_UNEXPECTED: return "eh_call_unexpected";
  case SR_EH_CURRENT_EXCEPTION_TYPE: return "eh_current_exception_type";
  case SR_EH_END_CATCH: return "eh_end_catch";
  case SR_EH_RETHROW: return "eh_rethrow";
  case SR_EH_THROW: return "eh_throw";
  case SR_EH_PERSONALITY: return "eh_personality";
  case SR_EH_RESUME: return "eh_resume";
  default: throw std::runtime_error("LowIR serializer: invalid symbol role");
  }
}

void add_symbol_metadata(std::vector<std::string> *items,
                         const Program &program,
                         const SymbolMetadata &metadata)
{
  items->clear();
  if (metadata.section_segment_id.valid() || metadata.section_name_id.valid() ||
      metadata.object_output_root || metadata.tls_for_id.valid())
    throw std::runtime_error("LowIR serializer: unsupported symbol metadata state");
  if (metadata.role != SR_NONE) {
    std::ostringstream item;
    item << "role=" << role_text(metadata.role);
    items->push_back(item.str());
  }
  switch (metadata.linkage) {
  case LLM_DEFAULT: break;
  case LLM_C: items->push_back("linkage=c"); break;
  case LLM_CPP: items->push_back("linkage=cpp"); break;
  default: throw std::runtime_error("LowIR serializer: invalid linkage state");
  }
  switch (metadata.binding) {
  case SBM_DEFAULT: break;
  case SBM_INTERNAL: items->push_back("binding=internal"); break;
  case SBM_STRONG: items->push_back("binding=strong"); break;
  case SBM_WEAK: items->push_back("binding=weak"); break;
  default: throw std::runtime_error("LowIR serializer: invalid binding state");
  }
  if (metadata.object_symbol_id.valid())
    items->push_back("object=" + spelling(program, metadata.object_symbol_id));
  if (metadata.tls_for_name_id.valid())
    items->push_back("tls_for=" + spelling(program, metadata.tls_for_name_id));
  if (metadata.keep_internal_alias) items->push_back("keep_alias=yes");
  if (metadata.prefer_local_object_binding) items->push_back("prefer_local=yes");
  if (metadata.object_trivial_lifecycle) items->push_back("trivial_lifecycle=yes");
  if (metadata.force_inline) items->push_back("force_inline=yes");
}

void add_boundary_metadata(std::vector<std::string> *items,
                           const FunctionBoundaryMetadata &boundary)
{
  switch (boundary.arity) {
  case CAM_FIXED: break;
  case CAM_VARIADIC: items->push_back("arity=variadic"); break;
  case CAM_PROTOTYPE_RELAXED: items->push_back("arity=prototype_relaxed"); break;
  default: throw std::runtime_error("LowIR serializer: invalid call arity state");
  }
  switch (boundary.effects) {
  case CFXM_DEFAULT: break;
  case CFXM_READNONE: items->push_back("effects=readnone"); break;
  case CFXM_READONLY: items->push_back("effects=readonly"); break;
  case CFXM_READWRITE: items->push_back("effects=readwrite"); break;
  default: throw std::runtime_error("LowIR serializer: invalid call effects state");
  }
  switch (boundary.unwind) {
  case CUM_DEFAULT: break;
  case CUM_MAY: items->push_back("unwind=may"); break;
  case CUM_NO: items->push_back("unwind=no"); break;
  default: throw std::runtime_error("LowIR serializer: invalid call unwind state");
  }
  switch (boundary.returns) {
  case CRM_DEFAULT: break;
  case CRM_RETURNS: items->push_back("return=returns"); break;
  case CRM_NORETURN: items->push_back("return=noreturn"); break;
  default: throw std::runtime_error("LowIR serializer: invalid call return state");
  }
}

void add_metadata(std::ostream &out, const std::vector<std::string> &items)
{
  if (items.empty()) return;
  out << " [";
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i != 0) out << ", ";
    out << items[i];
  }
  out << "]";
}

void add_parameter_metadata(std::vector<std::string> *items,
                            const ParameterMetadata &metadata)
{
  switch (metadata.passing) {
  case PPM_DIRECT: break;
  case PPM_INDIRECT_RESULT: items->push_back("pass=indirect_result"); break;
  case PPM_BY_ADDRESS: items->push_back("pass=by_address"); break;
  case PPM_REFERENCE: items->push_back("pass=reference"); break;
  case PPM_DECAY: items->push_back("pass=decay"); break;
  default: throw std::runtime_error("LowIR serializer: invalid parameter passing state");
  }
  switch (metadata.capture) {
  case PCM_DEFAULT: break;
  case PCM_NOCAPTURE: items->push_back("capture=nocapture"); break;
  case PCM_MAYCAPTURE: items->push_back("capture=maycapture"); break;
  default: throw std::runtime_error("LowIR serializer: invalid parameter capture state");
  }
  switch (metadata.access) {
  case PAM_DEFAULT: break;
  case PAM_NONE: items->push_back("access=none"); break;
  case PAM_READ: items->push_back("access=read"); break;
  case PAM_WRITE: items->push_back("access=write"); break;
  case PAM_READWRITE: items->push_back("access=readwrite"); break;
  default: throw std::runtime_error("LowIR serializer: invalid parameter access state");
  }
  switch (metadata.alias) {
  case PALM_DEFAULT: break;
  case PALM_NOALIAS: items->push_back("alias=noalias"); break;
  default: throw std::runtime_error("LowIR serializer: invalid parameter alias state");
  }
}

std::string unary_text(UnaryOperator op)
{
  switch (op) {
  case UOP_NEG: return "neg";
  case UOP_NOT: return "not";
  case UOP_BITNOT: return "bitnot";
  case UOP_DECAY: return "decay";
  case UOP_BSWAP: return "bswap";
  default: throw std::runtime_error("LowIR serializer: invalid unary operator");
  }
}

std::string binary_text(BinaryOperator op)
{
  switch (op) {
  case BOP_ADD: return "add";
  case BOP_SUB: return "sub";
  case BOP_MUL: return "mul";
  case BOP_DIV: return "div";
  case BOP_MOD: return "mod";
  case BOP_UDIV: return "udiv";
  case BOP_UMOD: return "umod";
  case BOP_AND: return "and";
  case BOP_OR: return "or";
  case BOP_XOR: return "xor";
  case BOP_SHL: return "shl";
  case BOP_SHR: return "shr";
  case BOP_USHR: return "ushr";
  default: throw std::runtime_error("LowIR serializer: invalid binary operator");
  }
}

std::string compare_text(ComparePredicate predicate)
{
  switch (predicate) {
  case CPP_EQ: return "eq";
  case CPP_NE: return "ne";
  case CPP_LT: return "lt";
  case CPP_LE: return "le";
  case CPP_GT: return "gt";
  case CPP_GE: return "ge";
  case CPP_ULT: return "ult";
  case CPP_ULE: return "ule";
  case CPP_UGT: return "ugt";
  case CPP_UGE: return "uge";
  default: throw std::runtime_error("LowIR serializer: invalid comparison predicate");
  }
}

std::string conversion_text(ConversionOperator op)
{
  switch (op) {
  case COP_SEXT: return "sext";
  case COP_ZEXT: return "zext";
  case COP_TRUNC: return "trunc";
  case COP_SITOFP: return "sitofp";
  case COP_UITOFP: return "uitofp";
  case COP_FPTOSI: return "fptosi";
  case COP_FPTOUI: return "fptoui";
  case COP_FPEXT: return "fpext";
  case COP_FPTRUNC: return "fptrunc";
  default: throw std::runtime_error("LowIR serializer: invalid conversion operator");
  }
}

void emit_dest(std::ostream &out, const Program &program,
               const Instruction &instruction)
{
  if (instruction.dest_id.valid() && !instruction.destination_name_id.valid())
    throw std::runtime_error("LowIR serializer: value destination lacks presentation identity");
  if (instruction.destination_name_id.valid())
    out << spelling(program, instruction.destination_name_id) << " = ";
}

void emit_instruction(std::ostream &out, const Program &program,
                      const Instruction &instruction)
{
  emit_dest(out, program, instruction);
  switch (instruction.kind) {
  case Instruction::IK_CONST:
    out << "const " << type_text(instruction.type) << " "
        << operand_text(program, instruction.first);
    break;
  case Instruction::IK_COPY:
    out << "copy " << type_text(instruction.type) << " "
        << operand_text(program, instruction.first);
    break;
  case Instruction::IK_ADDR:
    out << "addr " << operand_text(program, instruction.first);
    break;
  case Instruction::IK_LOAD:
    out << "load " << type_text(instruction.type) << " "
        << operand_text(program, instruction.first);
    break;
  case Instruction::IK_INDEX:
    out << "index " << type_text(instruction.type);
    switch (instruction.index_projection) {
    case IPK_NONE: break;
    case IPK_ARRAY_ELEMENT: out << " [projection=array_element]"; break;
    case IPK_FIELD: out << " [projection=field]"; break;
    case IPK_BASE_SUBOBJECT: out << " [projection=base_subobject]"; break;
    case IPK_REFERENCE_FIELD: out << " [projection=reference_field]"; break;
    default: throw std::runtime_error("LowIR serializer: invalid index projection");
    }
    out << " " << operand_text(program, instruction.first) << ", "
        << operand_text(program, instruction.second);
    break;
  case Instruction::IK_STORE:
    out << "store " << type_text(instruction.type) << " "
        << operand_text(program, instruction.first) << ", "
        << operand_text(program, instruction.second);
    break;
  case Instruction::IK_UNARY:
    out << "unary " << unary_text(instruction.unary_operator) << " "
        << type_text(instruction.type) << " "
        << operand_text(program, instruction.first);
    break;
  case Instruction::IK_BINARY:
    out << "binary " << binary_text(instruction.binary_operator) << " "
        << type_text(instruction.type) << " "
        << operand_text(program, instruction.first) << ", "
        << operand_text(program, instruction.second);
    break;
  case Instruction::IK_CMP:
    out << "cmp " << compare_text(instruction.compare_predicate) << " "
        << type_text(instruction.type) << " "
        << operand_text(program, instruction.first) << ", "
        << operand_text(program, instruction.second);
    break;
  case Instruction::IK_CONVERT:
    out << "convert " << conversion_text(instruction.conversion_operator) << " "
        << type_text(instruction.type) << " " << type_text(instruction.source_type)
        << " " << operand_text(program, instruction.first);
    break;
  case Instruction::IK_CALL:
    out << "call " << (instruction.call_returns_void ? "void" :
        type_text(instruction.call_return_type)) << " "
        << operand_text(program, instruction.first) << "(";
    for (std::size_t i = 0; i < instruction.args.size(); ++i) {
      if (i != 0) out << ", ";
      out << operand_text(program, instruction.args[i]);
    }
    out << ")";
    if (instruction.has_call_signature) {
      out << " as (";
      for (std::size_t i = 0; i < instruction.call_params.size(); ++i) {
        if (i != 0) out << ", ";
        out << spelling(program, instruction.call_params[i].name_id) << " : "
            << type_text(instruction.call_params[i].type);
        std::vector<std::string> parameter_items;
        add_parameter_metadata(&parameter_items,
                               instruction.call_params[i].metadata);
        add_metadata(out, parameter_items);
      }
      out << ") -> " << type_text(instruction.call_return_type);
      std::vector<std::string> boundary_items;
      add_boundary_metadata(&boundary_items, instruction.call_boundary);
      add_metadata(out, boundary_items);
    }
    break;
  case Instruction::IK_JUMP:
    out << "jump " << operand_text(program, instruction.first);
    break;
  case Instruction::IK_BRANCH:
    out << "branch " << operand_text(program, instruction.first) << ", "
        << operand_text(program, instruction.second) << ", "
        << operand_text(program, instruction.third);
    break;
  case Instruction::IK_SWITCH:
    if (instruction.args.size() % 2 != 0)
      throw std::runtime_error("LowIR serializer: switch case list is not paired");
    out << "switch " << operand_text(program, instruction.first) << ", "
        << operand_text(program, instruction.second);
    for (std::size_t i = 0; i < instruction.args.size(); i += 2)
      out << ", " << operand_text(program, instruction.args[i]) << ":"
          << operand_text(program, instruction.args[i + 1]);
    break;
  case Instruction::IK_RETURN:
    out << "return " << type_text(instruction.type);
    if (!instruction.type.is_void())
      out << " " << operand_text(program, instruction.first);
    break;
  default:
    throw std::runtime_error("LowIR serializer: unsupported instruction kind");
  }
  if (instruction.debug_location.present())
    throw std::runtime_error("LowIR serializer: debug locations are not supported by this serializer");
}

void emit_parameters(std::ostream &out, const Program &program,
                     const std::vector<Parameter> &parameters)
{
  out << "(";
  for (std::size_t i = 0; i < parameters.size(); ++i) {
    if (i != 0) out << ", ";
    out << spelling(program, parameters[i].name_id) << " : "
        << type_text(parameters[i].type);
    std::vector<std::string> items;
    add_parameter_metadata(&items, parameters[i].metadata);
    add_metadata(out, items);
  }
  out << ")";
}

void add_global_storage(std::vector<std::string> *items, GlobalStorageMode storage)
{
  switch (storage) {
  case GSM_DEFAULT:
  case GSM_WRITABLE:
    return;
  case GSM_READONLY:
    items->push_back("storage=readonly");
    return;
  case GSM_THREAD_LOCAL:
    items->push_back("storage=thread_local");
    return;
  default:
    throw std::runtime_error("LowIR serializer: invalid global storage state");
  }
}

void emit_function(std::ostream &out, const Program &program,
                   const Function &function)
{
  out << "function " << spelling(program, function.name_id);
  emit_parameters(out, program, function.params);
  out << " -> " << type_text(function.return_type);
  std::vector<std::string> items;
  add_boundary_metadata(&items, function.boundary);
  std::vector<std::string> symbols;
  add_symbol_metadata(&symbols, program, function.metadata);
  items.insert(items.end(), symbols.begin(), symbols.end());
  add_metadata(out, items);
  if (function.debug_location.present())
    throw std::runtime_error("LowIR serializer: debug locations are not supported by this serializer");
  out << " {\n";
  for (std::size_t i = 0; i < function.slots.size(); ++i) {
    out << "  slot " << spelling(program, function.slots[i].name_id) << " : "
        << type_text(function.slots[i].type) << "\n";
  }
  if (!function.slots.empty()) out << "\n";
  for (std::size_t i = 0; i < function.blocks.size(); ++i) {
    if (i != 0) out << "\n";
    out << "  block " << spelling(program, function.blocks[i].label_id) << ":\n";
    for (std::size_t j = 0; j < function.blocks[i].instructions.size(); ++j) {
      out << "    ";
      emit_instruction(out, program, function.blocks[i].instructions[j]);
      out << "\n";
    }
  }
  out << "}\n";
}

void emit_function_declaration(std::ostream &out, const Program &program,
                               const FunctionDeclaration &declaration)
{
  out << "declare function " << spelling(program, declaration.name_id);
  emit_parameters(out, program, declaration.params);
  out << " -> " << type_text(declaration.return_type);
  std::vector<std::string> items;
  add_boundary_metadata(&items, declaration.boundary);
  std::vector<std::string> symbols;
  add_symbol_metadata(&symbols, program, declaration.metadata);
  items.insert(items.end(), symbols.begin(), symbols.end());
  add_metadata(out, items);
  out << "\n";
}

}  // namespace

std::string serialize_lowir_program(const LowirProgram &program)
{
  std::ostringstream out;
  for (std::size_t i = 0; i < program.global_declarations.size(); ++i) {
    const GlobalDeclaration &declaration = program.global_declarations[i];
    out << "declare global " << spelling(program, declaration.name_id);
    if (declaration.has_type) out << " : " << type_text(declaration.type);
    std::vector<std::string> items;
    add_symbol_metadata(&items, program, declaration.metadata);
    add_global_storage(&items, declaration.storage);
    add_metadata(out, items);
    out << "\n";
  }
  for (std::size_t i = 0; i < program.function_declarations.size(); ++i)
    emit_function_declaration(out, program, program.function_declarations[i]);
  for (std::size_t i = 0; i < program.globals.size(); ++i) {
    const GlobalDefinition &global = program.globals[i];
    std::vector<std::string> items;
    add_symbol_metadata(&items, program, global.metadata);
    add_global_storage(&items, global.storage);
    out << "global " << spelling(program, global.name_id);
    if (global.structured) {
      if (global.data_items.empty())
        throw std::runtime_error("LowIR serializer: structured global has no data");
      add_metadata(out, items);
      out << " = {\n";
      for (std::size_t j = 0; j < global.data_items.size(); ++j) {
        const GlobalDefinition::DataItem &item = global.data_items[j];
        out << "  ";
        if (item.kind == GlobalDefinition::DataItem::ITEM_ADDR) {
          if (!item.symbol_name_id.valid())
            throw std::runtime_error("LowIR serializer: structured address lacks symbol");
          out << "ptr addr " << spelling(program, item.symbol_name_id);
          if (item.addr_addend > 0) out << " + " << item.addr_addend;
          else if (item.addr_addend < 0) out << " - " << -item.addr_addend;
        } else if (item.kind == GlobalDefinition::DataItem::ITEM_ZERO) {
          out << "zero " << item.zero_bytes;
        } else if (item.kind == GlobalDefinition::DataItem::ITEM_INTEGER) {
          if (item.literal_operand.kind != Operand::OP_INTEGER &&
              item.literal_operand.kind != Operand::OP_FLOAT)
            throw std::runtime_error("LowIR serializer: invalid structured scalar");
          out << type_text(item.type) << " "
              << operand_text(program, item.literal_operand);
        } else {
          throw std::runtime_error("LowIR serializer: invalid structured data item");
        }
        out << "\n";
      }
      out << "}\n";
      continue;
    }
    out << " : " << type_text(global.type);
    add_metadata(out, items);
    out << " = ";
    if (global.init_kind == GlobalDefinition::INIT_ZERO) out << "zero";
    else if (global.init_kind == GlobalDefinition::INIT_ADDR) {
      if (global.init_operand.kind != Operand::OP_GLOBAL)
        throw std::runtime_error("LowIR serializer: invalid global address initializer");
      out << "addr " << operand_text(program, global.init_operand);
      if (global.addr_addend > 0) out << " + " << global.addr_addend;
      else if (global.addr_addend < 0) out << " - " << -global.addr_addend;
    } else if (global.init_kind == GlobalDefinition::INIT_INTEGER) {
      if (global.init_operand.kind != Operand::OP_INTEGER &&
          global.init_operand.kind != Operand::OP_FLOAT)
        throw std::runtime_error("LowIR serializer: invalid scalar global initializer");
      out << operand_text(program, global.init_operand);
    } else {
      throw std::runtime_error("LowIR serializer: invalid global initializer kind");
    }
    out << "\n";
  }
  for (std::size_t i = 0; i < program.functions.size(); ++i) {
    if (out.tellp() > std::streampos(0)) out << "\n";
    emit_function(out, program, program.functions[i]);
  }
  for (std::size_t i = 0; i < program.object_aliases.size(); ++i) {
    out << "alias object " << spelling(program, program.object_aliases[i].object_name_id)
        << " = " << spelling(program, program.object_aliases[i].target_name_id)
        << "\n";
  }
  return out.str();
}

}  // namespace lowir_model
