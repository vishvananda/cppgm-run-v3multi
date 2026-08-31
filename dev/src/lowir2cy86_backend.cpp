#include "lowir2cy86_backend.h"

#include "lowir_model.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace lowir2cy86 {

namespace {

using lowir_model::Block;
using lowir_model::Function;
using lowir_model::FunctionBoundaryMetadata;
using lowir_model::FunctionDeclaration;
using lowir_model::GlobalDeclaration;
using lowir_model::GlobalDefinition;
using lowir_model::GlobalStorageMode;
using lowir_model::IndexProjectionKind;
using lowir_model::Instruction;
using lowir_model::InstructionDebugLocation;
using lowir_model::LowType;
using lowir_model::ObjectAlias;
using lowir_model::Operand;
using lowir_model::Parameter;
using lowir_model::ParameterMetadata;
using lowir_model::Program;
using lowir_model::SymbolMetadata;
using lowir_model::BinaryOperator;
using lowir_model::AtomicOrder;
using lowir_model::ComparePredicate;
using lowir_model::ConversionOperator;
using lowir_model::UnaryOperator;

class LowirError : public std::runtime_error {
public:
  explicit LowirError(const std::string &message) : std::runtime_error(message) {}
};

std::string without_prefix(const std::string &text) { return text.size() > 0 ? text.substr(1) : text; }

std::string cy_function(const std::string &name) { return "fn__" + without_prefix(name); }

std::string cy_global(const std::string &name) { return "g__" + without_prefix(name); }

std::string cy_block_label(const std::string &function_label, const std::string &block_label) {
  return function_label + "__" + without_prefix(block_label);
}

bool is_terminator(Instruction::Kind kind) {
  return kind == Instruction::IK_JUMP || kind == Instruction::IK_BRANCH || kind == Instruction::IK_SWITCH || kind == Instruction::IK_RETURN ||
         kind == Instruction::IK_THROW || kind == Instruction::IK_RESUME;
}

int integer_width(const LowType &type) {
  return type.integer_width();
}

bool is_float_type(const LowType &type) {
  return type.is_float();
}

bool is_integer_type(const LowType &type) {
  return type.is_integer();
}

std::size_t type_storage_size(const LowType &type) {
  return type.storage_size();
}

std::size_t type_storage_alignment(const LowType &type) {
  return type.storage_alignment();
}

LowType i64_type() {
  LowType type;
  type.kind = LowType::TYPE_INTEGER;
  type.integer_kind = LowType::INTEGER_I64;
  return type;
}

LowType pointer_type() {
  LowType type;
  type.kind = LowType::TYPE_POINTER;
  return type;
}

} // namespace

class Validator {
private:
  enum SymbolKind { SYMBOL_GLOBAL, SYMBOL_FUNCTION };

  struct Symbol {
    lowir_model::SymbolId id;
    SymbolKind kind;
    lowir_model::SpellingId output_name_id;
    SymbolMetadata *metadata;
    const GlobalDeclaration *global_declaration;
    const GlobalDefinition *global_definition;
    const Function *function;
    const FunctionDeclaration *function_declaration;

    Symbol()
        : kind(SYMBOL_GLOBAL), metadata(0), global_declaration(0), global_definition(0), function(0),
          function_declaration(0) {}
  };

  struct Value {
    LowType type;
    bool known;
    Value() : known(false) {}
    explicit Value(const LowType &t) : type(t), known(t.valid()) {}
  };

public:
  explicit Validator(Program &program) : program_(program), entry_id_(), init_id_(), fini_id_() {}

  void validate() {
    collect_symbols();
    resolve_top_level_references();
    validate_aliases();
    validate_roles_and_tls();
    select_runtime_functions();
    for (std::size_t i = 0; i < program_.global_declarations.size(); ++i) {
      validate_global_declaration(program_.global_declarations[i]);
    }
    for (std::size_t i = 0; i < program_.globals.size(); ++i) {
      validate_global(program_.globals[i]);
    }
    for (std::size_t i = 0; i < program_.function_declarations.size(); ++i) {
      validate_function_header(program_.function_declarations[i].params, program_.function_declarations[i].return_type);
    }
    for (std::size_t i = 0; i < program_.functions.size(); ++i) {
      validate_function(program_.functions[i]);
    }
    if (!entry_id_.valid()) {
      throw LowirError("program has no entry function");
    }
  }

  const Function *entry_function() const { return function_for(entry_id_); }

  const Function *init_function() const { return function_for(init_id_); }

  const Function *fini_function() const { return function_for(fini_id_); }

  const Function *function_for(lowir_model::SymbolId id) const {
    if (!id.valid() || id.index >= symbols_.size()) return 0;
    return symbols_[id.index].function;
  }

  const FunctionDeclaration *function_declaration_for(lowir_model::SymbolId id) const {
    if (!id.valid() || id.index >= symbols_.size()) return 0;
    return symbols_[id.index].function_declaration;
  }

  bool is_function_symbol(lowir_model::SymbolId id) const {
    return id.valid() && id.index < symbols_.size() && symbols_[id.index].kind == SYMBOL_FUNCTION;
  }

  const std::string &presentation(lowir_model::SpellingId id) const {
    if (!id.valid() || id.index >= program_.presentation.size()) throw LowirError("invalid presentation identity");
    return program_.presentation[id.index];
  }

  const std::string &symbol_label(lowir_model::SymbolId id) const { return presentation(symbol_record(id).output_name_id); }

  const std::string &block_label(lowir_model::BlockId id) const {
    if (!id.valid() || id.index >= block_owners_.size()) throw LowirError("invalid resolved block identity");
    return presentation(block_owners_[id.index]->label_id);
  }

  LowType value_type(lowir_model::ValueId id) const {
    if (!id.valid() || id.index >= program_.values.size()) throw LowirError("invalid resolved value identity");
    const lowir_model::ValueRecord &record = program_.values[id.index];
    if (record.parameter != 0) return record.parameter->type;
    if (record.instruction != 0) return record.instruction->result_type;
    throw LowirError("value has no typed owner");
  }

  LowType slot_type(lowir_model::SlotId id) const {
    if (!id.valid() || id.index >= slot_owners_.size()) throw LowirError("invalid resolved slot identity");
    return slot_owners_[id.index]->type;
  }

private:
  const Symbol &symbol_record(lowir_model::SymbolId id) const {
    if (!id.valid() || id.index >= symbols_.size()) throw LowirError("invalid resolved symbol identity");
    return symbols_[id.index];
  }

  LowType symbol_type(const Symbol &symbol) const {
    if (symbol.global_declaration != 0) return symbol.global_declaration->has_type ? symbol.global_declaration->type : LowType();
    if (symbol.global_definition != 0) return symbol.global_definition->structured ? LowType() : symbol.global_definition->type;
    if (symbol.function != 0) return symbol.function->return_type;
    if (symbol.function_declaration != 0) return symbol.function_declaration->return_type;
    return LowType();
  }

  GlobalStorageMode symbol_storage(const Symbol &symbol) const {
    if (symbol.global_declaration != 0) return symbol.global_declaration->storage;
    if (symbol.global_definition != 0) return symbol.global_definition->storage;
    return lowir_model::GSM_DEFAULT;
  }

  lowir_model::SymbolId lookup_symbol(const std::string &name) const {
    std::map<std::string, lowir_model::SymbolId>::const_iterator it = symbol_names_.find(name);
    if (it == symbol_names_.end()) throw LowirError("use of undefined global or function");
    return it->second;
  }

  lowir_model::SymbolId add_symbol(lowir_model::SpellingId name_id, Symbol symbol) {
    const std::string name = presentation(name_id);
    if (symbol_names_.find(name) != symbol_names_.end()) throw LowirError("duplicate top-level symbol");
    const lowir_model::SymbolId id(symbols_.size());
    symbol.id = id;
    const std::string output_name = symbol.kind == SYMBOL_FUNCTION ? cy_function(name) : cy_global(name);
    symbol.output_name_id = lowir_model::SpellingId(program_.presentation.size());
    program_.presentation.push_back(output_name);
    symbol_names_[name] = id;
    symbols_.push_back(symbol);
    return id;
  }

  void collect_symbols() {
    for (std::size_t i = 0; i < program_.global_declarations.size(); ++i) {
      GlobalDeclaration &global = program_.global_declarations[i];
      Symbol symbol;
      symbol.kind = SYMBOL_GLOBAL;
      symbol.metadata = &global.metadata;
      symbol.global_declaration = &global;
      global.symbol_id = add_symbol(global.name_id, symbol);
    }
    for (std::size_t i = 0; i < program_.globals.size(); ++i) {
      GlobalDefinition &global = program_.globals[i];
      Symbol symbol;
      symbol.kind = SYMBOL_GLOBAL;
      symbol.metadata = &global.metadata;
      symbol.global_definition = &global;
      global.symbol_id = add_symbol(global.name_id, symbol);
    }
    for (std::size_t i = 0; i < program_.function_declarations.size(); ++i) {
      FunctionDeclaration &function = program_.function_declarations[i];
      Symbol symbol;
      symbol.kind = SYMBOL_FUNCTION;
      symbol.metadata = &function.metadata;
      symbol.function = 0;
      symbol.function_declaration = &function;
      function.symbol_id = add_symbol(function.name_id, symbol);
    }
    for (std::size_t i = 0; i < program_.functions.size(); ++i) {
      Function &function = program_.functions[i];
      Symbol symbol;
      symbol.kind = SYMBOL_FUNCTION;
      symbol.metadata = &function.metadata;
      symbol.function = &function;
      symbol.function_declaration = 0;
      function.symbol_id = add_symbol(function.name_id, symbol);
    }
  }

  void resolve_top_level_references() {
    for (std::size_t i = 0; i < program_.globals.size(); ++i) {
      GlobalDefinition &global = program_.globals[i];
      if (global.structured) {
        for (std::size_t j = 0; j < global.data_items.size(); ++j) {
          if (global.data_items[j].kind == GlobalDefinition::DataItem::ITEM_ADDR)
            global.data_items[j].symbol_id = lookup_symbol(presentation(global.data_items[j].symbol_name_id));
        }
      } else if (global.init_kind == GlobalDefinition::INIT_ADDR) {
        global.init_operand.symbol_id = lookup_symbol(presentation(global.init_operand.presentation_id));
      }
    }
  }

  void validate_aliases() {
    std::set<lowir_model::SpellingId> aliases;
    for (std::size_t i = 0; i < program_.object_aliases.size(); ++i) {
      ObjectAlias &alias = program_.object_aliases[i];
      if (!aliases.insert(alias.object_name_id).second) {
        throw LowirError("duplicate object alias");
      }
      alias.target_id = lookup_symbol(presentation(alias.target_name_id));
    }
  }

  void validate_roles_and_tls() {
    std::set<int> singleton_roles;
    for (std::size_t i = 0; i < symbols_.size(); ++i) {
      Symbol &symbol = symbols_[i];
      SymbolMetadata *metadata = symbol.metadata;
      if (metadata == 0) continue;
      if (symbol.kind == SYMBOL_GLOBAL) {
        if (metadata->role != lowir_model::SR_NONE && metadata->role != lowir_model::SR_EH_TOP && metadata->role != lowir_model::SR_EH_VALUE &&
            metadata->role != lowir_model::SR_EH_TYPE)
          throw LowirError("function role attached to global");
      } else {
        if (metadata->role == lowir_model::SR_EH_TOP || metadata->role == lowir_model::SR_EH_VALUE || metadata->role == lowir_model::SR_EH_TYPE)
          throw LowirError("global role attached to function");
        if (metadata->tls_for_name_id.valid()) {
          const lowir_model::SymbolId target_id = lookup_symbol(presentation(metadata->tls_for_name_id));
          const Symbol &target = symbol_record(target_id);
          if (target.kind != SYMBOL_GLOBAL || symbol_storage(target) != lowir_model::GSM_THREAD_LOCAL)
            throw LowirError("tls_for target is not thread local");
          metadata->tls_for_id = target_id;
          if (!tls_wrappers_.insert(target_id).second) throw LowirError("duplicate tls wrapper");
        }
      }
      if (metadata->role != lowir_model::SR_NONE && !singleton_roles.insert(static_cast<int>(metadata->role)).second)
        throw LowirError("duplicate singleton role");
    }
  }

  void select_runtime_functions() {
    const std::map<std::string, lowir_model::SymbolId>::const_iterator legacy_entry = symbol_names_.find("@main");
    const std::map<std::string, lowir_model::SymbolId>::const_iterator legacy_init = symbol_names_.find("@__cppgm_init");
    const std::map<std::string, lowir_model::SymbolId>::const_iterator legacy_fini = symbol_names_.find("@__cppgm_fini");
    for (std::size_t i = 0; i < program_.functions.size(); ++i) {
      Function &function = program_.functions[i];
      const lowir_model::SymbolRole role = function.metadata.role;
      if (role == lowir_model::SR_ENTRY ||
          (role == lowir_model::SR_NONE && legacy_entry != symbol_names_.end() && function.symbol_id == legacy_entry->second)) {
        if (entry_id_.valid()) throw LowirError("multiple entry functions");
        entry_id_ = function.symbol_id;
      }
      if (role == lowir_model::SR_INIT ||
          (role == lowir_model::SR_NONE && legacy_init != symbol_names_.end() && function.symbol_id == legacy_init->second)) {
        if (init_id_.valid()) throw LowirError("multiple runtime hook functions");
        init_id_ = function.symbol_id;
      }
      if (role == lowir_model::SR_FINI ||
          (role == lowir_model::SR_NONE && legacy_fini != symbol_names_.end() && function.symbol_id == legacy_fini->second)) {
        if (fini_id_.valid()) throw LowirError("multiple runtime hook functions");
        fini_id_ = function.symbol_id;
      }
    }
  }

  void validate_global_declaration(const GlobalDeclaration &global) const {
    if (global.has_type && !global.type.valid()) {
      throw LowirError("invalid global declaration type");
    }
  }

  void validate_global(const GlobalDefinition &global) const {
    if (global.structured) {
      if (global.data_items.empty()) throw LowirError("empty structured global");
      for (std::size_t i = 0; i < global.data_items.size(); ++i) {
        const GlobalDefinition::DataItem &item = global.data_items[i];
        if (item.kind == GlobalDefinition::DataItem::ITEM_ZERO) {
          if (item.zero_bytes == 0) throw LowirError("zero data item has no size");
        } else if (item.kind == GlobalDefinition::DataItem::ITEM_INTEGER) {
          if (!item.type.is_scalar()) throw LowirError("invalid structured global item type");
          if (item.literal_operand.kind != Operand::OP_INTEGER && item.literal_operand.kind != Operand::OP_FLOAT)
            throw LowirError("invalid structured global literal");
          if (!operand_matches(item.literal_operand, item.type)) throw LowirError("structured global literal type mismatch");
        } else if (!item.symbol_id.valid()) {
          throw LowirError("undefined structured global address");
        }
      }
    } else if (global.init_kind == GlobalDefinition::INIT_ADDR) {
      if (!global.type.is_pointer()) throw LowirError("address global must have ptr type");
      if (!global.init_operand.symbol_id.valid()) throw LowirError("undefined global address initializer");
    } else if (global.init_kind == GlobalDefinition::INIT_ZERO) {
      if (!global.type.is_scalar()) throw LowirError("invalid zero global type");
    } else {
      if (!global.type.is_scalar()) throw LowirError("invalid scalar global type");
      if (global.init_operand.kind != Operand::OP_INTEGER && global.init_operand.kind != Operand::OP_FLOAT)
        throw LowirError("invalid scalar global initializer");
      if (!operand_matches(global.init_operand, global.type)) throw LowirError("scalar global initializer type mismatch");
    }
  }

  void validate_function_header(const std::vector<Parameter> &parameters, const LowType &return_type) const {
    if (!return_type.valid()) throw LowirError("invalid function return type");
    std::set<std::string> names;
    bool saw_indirect_result = false;
    for (std::size_t i = 0; i < parameters.size(); ++i) {
      const Parameter &parameter = parameters[i];
      if (!names.insert(presentation(parameter.name_id)).second) throw LowirError("duplicate parameter");
      if (!parameter.type.valid() || parameter.type.is_void()) throw LowirError("invalid parameter type");
      const ParameterMetadata &metadata = parameter.metadata;
      const bool pointer_metadata = metadata.passing != lowir_model::PPM_DIRECT || metadata.capture != lowir_model::PCM_DEFAULT ||
                                    metadata.access != lowir_model::PAM_DEFAULT || metadata.alias != lowir_model::PALM_DEFAULT;
      if (pointer_metadata && !parameter.type.is_pointer()) throw LowirError("pointer parameter metadata on non-pointer");
      if (metadata.passing == lowir_model::PPM_INDIRECT_RESULT) {
        if (i != 0) throw LowirError("indirect result is not first parameter");
        saw_indirect_result = true;
      }
    }
    if (saw_indirect_result && !return_type.is_void()) throw LowirError("indirect result on non-void function");
  }

  Value operand_value(const Operand &operand) const {
    if (operand.kind == Operand::OP_TEMP) {
      return Value(value_type(operand.value_id));
    }
    if (operand.kind == Operand::OP_SLOT) {
      if (!operand.slot_id.valid() || operand.slot_id.index >= slot_owners_.size()) throw LowirError("use of undefined slot");
      return Value(slot_owners_[operand.slot_id.index]->type);
    }
    if (operand.kind == Operand::OP_GLOBAL) {
      const Symbol &symbol = symbol_record(operand.symbol_id);
      return Value(symbol.kind == SYMBOL_FUNCTION ? pointer_type() : symbol_type(symbol));
    }
    if (operand.kind == Operand::OP_INTEGER || operand.kind == Operand::OP_FLOAT) return Value(operand.literal_type);
    return Value();
  }

  void require_value(const Operand &operand) const { (void)operand_value(operand); }

  bool operand_matches(const Operand &operand, const LowType &expected) const {
    if (!expected.valid() || expected.is_void()) return false;
    const Value actual = operand_value(operand);
    if (!actual.known) return false;
    if (operand.kind == Operand::OP_INTEGER) {
      if (expected.is_integer()) return true;
      return expected.is_pointer() && operand.int_value == 0;
    }
    if (operand.kind == Operand::OP_FLOAT) return expected.is_float();
    return actual.type == expected;
  }

  bool copy_operand_matches(const Operand &operand, const LowType &expected) const {
    if (operand_matches(operand, expected)) return true;
    const Value actual = operand_value(operand);
    // LowIR's signed and unsigned integer spellings share the same bit
    // representation.  PA15 uses copy for a same-width semantic signedness
    // retag when no widening or truncation is required; keep real width and
    // kind mismatches rejected by limiting this relaxation to integers with
    // equal storage width.
    return actual.known && actual.type.is_integer() && expected.is_integer() &&
           actual.type.integer_width() == expected.integer_width();
  }

  bool comparison_operand_matches(const Operand &operand,
                                  const LowType &expected,
                                  ComparePredicate predicate) const {
    if (operand_matches(operand, expected)) return true;
    // Equality has no signed ordering.  Permit the same-width signedness
    // carrier used by PA15 for a narrow unsigned bit-field; relational
    // predicates must continue to match their exact typed operation carrier.
    if (predicate != lowir_model::CPP_EQ && predicate != lowir_model::CPP_NE)
      return false;
    return copy_operand_matches(operand, expected);
  }

  void require_operand_type(const Operand &operand, const LowType &expected, const std::string &what) const {
    if (!operand_matches(operand, expected)) throw LowirError(what + " type mismatch");
  }

  bool is_memory_operand(const Operand &operand) const {
    if (operand.kind == Operand::OP_SLOT) return true;
    if (operand.kind == Operand::OP_GLOBAL) return !is_function_symbol(operand.symbol_id);
    if (operand.kind == Operand::OP_TEMP) {
      const LowType type = operand_value(operand).type;
      return type.is_pointer() || type.is_object();
    }
    return false;
  }

  void require_memory_operand(const Operand &operand, const std::string &what) const {
    if (!is_memory_operand(operand)) throw LowirError(what + " is not addressable storage");
  }

  void require_pointer_operand(const Operand &operand, const std::string &what) const {
    if (!operand_matches(operand, pointer_type())) throw LowirError(what + " is not a pointer");
  }

  lowir_model::ValueId allocate_value(Parameter *parameter) {
    const lowir_model::ValueId id(program_.values.size());
    lowir_model::ValueRecord record;
    record.id = id;
    record.parameter = parameter;
    program_.values.push_back(record);
    return id;
  }

  lowir_model::ValueId allocate_value(Instruction *instruction) {
    const lowir_model::ValueId id(program_.values.size());
    lowir_model::ValueRecord record;
    record.id = id;
    record.instruction = instruction;
    program_.values.push_back(record);
    return id;
  }

  lowir_model::SlotId allocate_slot(Function::Slot *slot) {
    const lowir_model::SlotId id(slot_owners_.size());
    slot_owners_.push_back(slot);
    return id;
  }

  lowir_model::BlockId allocate_block(Block *block) {
    const lowir_model::BlockId id(block_owners_.size());
    block_owners_.push_back(block);
    return id;
  }

  void resolve_operand(Operand *operand, const std::map<std::string, lowir_model::ValueId> &values,
                      const std::map<std::string, lowir_model::SlotId> &slots, const std::map<std::string, lowir_model::BlockId> &blocks) {
    if (operand->kind == Operand::OP_TEMP) {
      const std::string &name = presentation(operand->presentation_id);
      std::map<std::string, lowir_model::ValueId>::const_iterator it = values.find(name);
      if (it == values.end()) throw LowirError("use of undefined temporary");
      operand->value_id = it->second;
    } else if (operand->kind == Operand::OP_SLOT) {
      const std::string &name = presentation(operand->presentation_id);
      std::map<std::string, lowir_model::SlotId>::const_iterator it = slots.find(name);
      if (it == slots.end()) throw LowirError("use of undefined slot");
      operand->slot_id = it->second;
    } else if (operand->kind == Operand::OP_GLOBAL) {
      operand->symbol_id = lookup_symbol(presentation(operand->presentation_id));
    } else if (operand->kind == Operand::OP_LABEL) {
      const std::string &name = presentation(operand->presentation_id);
      std::map<std::string, lowir_model::BlockId>::const_iterator it = blocks.find(name);
      if (it == blocks.end()) throw LowirError("undefined block target");
      operand->block_id = it->second;
    }
  }

  void resolve_instruction_operands(Instruction *instruction, const std::map<std::string, lowir_model::ValueId> &values,
                                    const std::map<std::string, lowir_model::SlotId> &slots,
                                    const std::map<std::string, lowir_model::BlockId> &blocks) {
    resolve_operand(&instruction->first, values, slots, blocks);
    resolve_operand(&instruction->second, values, slots, blocks);
    resolve_operand(&instruction->third, values, slots, blocks);
    for (std::size_t i = 0; i < instruction->args.size(); ++i) resolve_operand(&instruction->args[i], values, slots, blocks);
    instruction->direct_callee_id = lowir_model::SymbolId();
    if (instruction->kind == Instruction::IK_CALL && instruction->first.kind == Operand::OP_GLOBAL && is_function_symbol(instruction->first.symbol_id))
      instruction->direct_callee_id = instruction->first.symbol_id;
  }

  void validate_function(Function &function) {
    validate_function_header(function.params, function.return_type);
    if (function.blocks.empty()) throw LowirError("function has no blocks");
    function.value_begin = lowir_model::ValueId(program_.values.size());
    function.slot_begin = lowir_model::SlotId(slot_owners_.size());
    std::map<std::string, lowir_model::SlotId> slots;
    for (std::size_t i = 0; i < function.slots.size(); ++i) {
      Function::Slot &slot = function.slots[i];
      if (!slot.type.valid() || slot.type.is_void()) throw LowirError("invalid slot type");
      const lowir_model::SlotId id = allocate_slot(&slot);
      if (!slots.insert(std::make_pair(presentation(slot.name_id), id)).second) throw LowirError("duplicate slot");
      slot.slot_id = id;
    }
    std::map<std::string, lowir_model::BlockId> blocks;
    for (std::size_t i = 0; i < function.blocks.size(); ++i) {
      const lowir_model::BlockId id = allocate_block(&function.blocks[i]);
      if (!blocks.insert(std::make_pair(presentation(function.blocks[i].label_id), id)).second) throw LowirError("duplicate block");
      function.blocks[i].block_id = id;
    }
    std::map<std::string, lowir_model::ValueId> values;
    for (std::size_t i = 0; i < function.params.size(); ++i) {
      const lowir_model::ValueId id = allocate_value(&function.params[i]);
      if (!values.insert(std::make_pair(presentation(function.params[i].name_id), id)).second) throw LowirError("duplicate parameter");
      function.params[i].value_id = id;
    }
    for (std::size_t b = 0; b < function.blocks.size(); ++b) {
      Block &block = function.blocks[b];
      if (block.instructions.empty()) throw LowirError("block has no terminator");
      bool terminated = false;
      for (std::size_t i = 0; i < block.instructions.size(); ++i) {
        Instruction &instruction = block.instructions[i];
        if (terminated) throw LowirError("instruction after terminator");
        resolve_instruction_operands(&instruction, values, slots, blocks);
        validate_instruction(function, instruction, blocks, values, slots);
        if (is_terminator(instruction.kind)) terminated = true;
      }
      if (!terminated) throw LowirError("block has no terminator");
    }
    function.value_count = program_.values.size() - function.value_begin.index;
    function.slot_count = slot_owners_.size() - function.slot_begin.index;
  }

  void require_block(const Operand &operand, const std::map<std::string, lowir_model::BlockId> &blocks) const {
    (void)blocks;
    if (operand.kind != Operand::OP_LABEL || !operand.block_id.valid()) throw LowirError("undefined block target");
  }

  struct CallTarget {
    const std::vector<Parameter> *parameters;
    FunctionBoundaryMetadata boundary;
    LowType return_type;
    bool direct;
    CallTarget() : parameters(0), direct(false) {}
  };

  CallTarget call_target(const Instruction &instruction) const {
    CallTarget result;
    if (instruction.direct_callee_id.valid()) {
      result.direct = true;
      const Function *function = function_for(instruction.direct_callee_id);
      const FunctionDeclaration *declaration = function_declaration_for(instruction.direct_callee_id);
      if (function != 0) {
        result.parameters = &function->params;
        result.boundary = function->boundary;
        result.return_type = function->return_type;
      } else if (declaration != 0) {
        result.parameters = &declaration->params;
        result.boundary = declaration->boundary;
        result.return_type = declaration->return_type;
      }
    } else {
      result.parameters = &instruction.call_params;
      result.boundary = instruction.call_boundary;
      result.return_type = instruction.call_return_type;
    }
    return result;
  }

  void validate_call(const Instruction &instruction) const {
    const Value callee_value = operand_value(instruction.first);
    if (!callee_value.known) throw LowirError("call has invalid callee");
    if (instruction.first.kind == Operand::OP_GLOBAL && !is_function_symbol(instruction.first.symbol_id))
      throw LowirError("direct call target is not a function");
    if (instruction.first.kind != Operand::OP_GLOBAL && !callee_value.type.is_pointer())
      throw LowirError("indirect call target is not a pointer");
    const CallTarget target = call_target(instruction);
    if (!target.direct && !instruction.has_call_signature) throw LowirError("indirect call is missing signature");
    if (instruction.has_call_signature) validate_function_header(instruction.call_params, instruction.call_return_type);
    const std::size_t required = target.parameters->size();
    if (target.boundary.arity == lowir_model::CAM_FIXED && instruction.args.size() != required) throw LowirError("fixed call arity mismatch");
    if (target.boundary.arity != lowir_model::CAM_FIXED && instruction.args.size() < required) throw LowirError("call has too few arguments");
    for (std::size_t i = 0; i < required && i < instruction.args.size(); ++i) {
      const bool addressable_slot = (*target.parameters)[i].type.is_pointer() && instruction.args[i].kind == Operand::OP_SLOT &&
                                    !slot_owners_[instruction.args[i].slot_id.index]->type.is_pointer();
      if (!addressable_slot && !operand_matches(instruction.args[i], (*target.parameters)[i].type)) throw LowirError("call argument type mismatch");
    }
    if (target.direct && !instruction.call_returns_void && instruction.call_return_type != target.return_type)
      throw LowirError("direct call result type mismatch");
    if (instruction.call_returns_void) {
      if (!target.return_type.is_void()) throw LowirError("void call has non-void target");
    } else if (target.return_type.is_void()) {
      throw LowirError("value call has void target");
    } else if (!instruction.destination_name_id.valid()) {
      // Calls returning a value always have a destination in the grammar.
      throw LowirError("missing call destination");
    }
  }

  void define_value(Instruction *instruction, const LowType &type, std::map<std::string, lowir_model::ValueId> *values) {
    if (!instruction->destination_name_id.valid()) return;
    const std::string &name = presentation(instruction->destination_name_id);
    if (values->find(name) != values->end()) throw LowirError("duplicate temporary definition");
    instruction->result_type = type;
    const lowir_model::ValueId id = allocate_value(instruction);
    (*values)[name] = id;
    instruction->dest_id = id;
  }

  void validate_instruction(Function &function, Instruction &instruction, const std::map<std::string, lowir_model::BlockId> &blocks,
                            std::map<std::string, lowir_model::ValueId> &values, const std::map<std::string, lowir_model::SlotId> &slots) {
    const auto require = [&](const Operand &operand) { require_value(operand); };
    switch (instruction.kind) {
    case Instruction::IK_CONST:
      if (instruction.type.is_void()) throw LowirError("void constant");
      if (instruction.first.kind != Operand::OP_INTEGER && instruction.first.kind != Operand::OP_FLOAT) throw LowirError("constant is not scalar");
      if (!operand_matches(instruction.first, instruction.type)) throw LowirError("constant type mismatch");
      define_value(&instruction, instruction.type, &values);
      break;
    case Instruction::IK_COPY:
      require(instruction.first);
      if (!copy_operand_matches(instruction.first, instruction.type))
        throw LowirError("copy source type mismatch");
      define_value(&instruction, instruction.type, &values);
      break;
    case Instruction::IK_ADDR:
      if (instruction.first.kind != Operand::OP_SLOT && instruction.first.kind != Operand::OP_GLOBAL) throw LowirError("addr requires addressable operand");
      if (instruction.first.kind == Operand::OP_GLOBAL && !instruction.first.symbol_id.valid()) throw LowirError("addr of undefined symbol");
      define_value(&instruction, pointer_type(), &values);
      break;
    case Instruction::IK_LOAD:
    case Instruction::IK_ATOMIC_LOAD:
      if (!instruction.type.valid() || instruction.type.is_void()) throw LowirError("invalid load type");
      require_memory_operand(instruction.first, "load source");
      if (instruction.kind == Instruction::IK_ATOMIC_LOAD && instruction.atomic_order == lowir_model::AO_INVALID) throw LowirError("invalid atomic order");
      define_value(&instruction, instruction.type, &values);
      break;
    case Instruction::IK_STORE:
    case Instruction::IK_ATOMIC_STORE:
      require(instruction.first);
      require_operand_type(instruction.first, instruction.type, "store value");
      require_memory_operand(instruction.second, "store destination");
      if (instruction.kind == Instruction::IK_ATOMIC_STORE && instruction.atomic_order == lowir_model::AO_INVALID) throw LowirError("invalid atomic order");
      break;
    case Instruction::IK_INDEX:
      require_memory_operand(instruction.first, "index base");
      if (!operand_value(instruction.second).known || !operand_value(instruction.second).type.is_integer())
        throw LowirError("index offset is not integer");
      if (!instruction.type.valid() || instruction.type.is_void()) throw LowirError("invalid index element type");
      define_value(&instruction, pointer_type(), &values);
      break;
    case Instruction::IK_UNARY:
      if (instruction.unary_operator == lowir_model::UOP_INVALID) throw LowirError("unknown unary operator");
      require_operand_type(instruction.first, instruction.type, "unary operand");
      if (instruction.unary_operator == lowir_model::UOP_DECAY && !instruction.type.is_pointer()) throw LowirError("decay requires ptr type");
      if (instruction.unary_operator == lowir_model::UOP_NEG && !instruction.type.is_integer() && !instruction.type.is_float())
        throw LowirError("invalid negation type");
      if ((instruction.unary_operator == lowir_model::UOP_NOT || instruction.unary_operator == lowir_model::UOP_BITNOT) && !instruction.type.is_integer())
        throw LowirError("invalid integer unary type");
      if (instruction.unary_operator == lowir_model::UOP_BSWAP &&
          (instruction.type.integer_kind != LowType::INTEGER_I16 && instruction.type.integer_kind != LowType::INTEGER_I32 &&
           instruction.type.integer_kind != LowType::INTEGER_I64))
        throw LowirError("invalid bswap type");
      define_value(&instruction, instruction.type, &values);
      break;
    case Instruction::IK_BINARY:
      if (instruction.binary_operator == lowir_model::BOP_INVALID) throw LowirError("unknown binary operator");
      if (instruction.type.is_pointer() && instruction.binary_operator == lowir_model::BOP_SUB) {
        require_operand_type(instruction.first, instruction.type, "left pointer-difference operand");
        require_operand_type(instruction.second, instruction.type, "right pointer-difference operand");
        // PA15 represents pointer difference as a 64-bit byte distance while
        // retaining ptr as the operation type for the typed pointer operands.
        define_value(&instruction, i64_type(), &values);
        break;
      }
      require_operand_type(instruction.first, instruction.type, "left binary operand");
      require_operand_type(instruction.second, instruction.type, "right binary operand");
      if (!instruction.type.is_integer() && !instruction.type.is_float()) throw LowirError("invalid binary type");
      if (is_float_type(instruction.type)) {
        if (instruction.binary_operator != lowir_model::BOP_ADD && instruction.binary_operator != lowir_model::BOP_SUB &&
            instruction.binary_operator != lowir_model::BOP_MUL && instruction.binary_operator != lowir_model::BOP_DIV)
          throw LowirError("invalid floating binary operator");
      }
      define_value(&instruction, instruction.type, &values);
      break;
    case Instruction::IK_CMP:
      if (instruction.compare_predicate == lowir_model::CPP_INVALID) throw LowirError("unknown comparison predicate");
      if (!comparison_operand_matches(instruction.first, instruction.type,
                                       instruction.compare_predicate))
        throw LowirError("left comparison operand type mismatch");
      if (!comparison_operand_matches(instruction.second, instruction.type,
                                      instruction.compare_predicate))
        throw LowirError("right comparison operand type mismatch");
      if (!instruction.type.is_integer() && !instruction.type.is_float() && !instruction.type.is_pointer()) throw LowirError("invalid comparison type");
      define_value(&instruction, i64_type(), &values);
      break;
    case Instruction::IK_CONVERT: {
      require_operand_type(instruction.first, instruction.source_type, "conversion operand");
      const int dst_width = integer_width(instruction.type);
      const int src_width = integer_width(instruction.source_type);
      if (instruction.conversion_operator == lowir_model::COP_INVALID) throw LowirError("unknown conversion operator");
      if ((instruction.conversion_operator == lowir_model::COP_SEXT || instruction.conversion_operator == lowir_model::COP_ZEXT) &&
          (dst_width == 0 || src_width == 0 || dst_width <= src_width))
        throw LowirError("invalid integer widening conversion");
      if (instruction.conversion_operator == lowir_model::COP_TRUNC && (dst_width == 0 || src_width == 0 || dst_width >= src_width))
        throw LowirError("invalid integer truncation conversion");
      if ((instruction.conversion_operator == lowir_model::COP_SITOFP || instruction.conversion_operator == lowir_model::COP_UITOFP) &&
          (!is_integer_type(instruction.source_type) || !is_float_type(instruction.type)))
        throw LowirError("invalid integer to float conversion");
      if ((instruction.conversion_operator == lowir_model::COP_FPTOSI || instruction.conversion_operator == lowir_model::COP_FPTOUI) &&
          (!is_float_type(instruction.source_type) || !is_integer_type(instruction.type)))
        throw LowirError("invalid float to integer conversion");
      if ((instruction.conversion_operator == lowir_model::COP_FPEXT || instruction.conversion_operator == lowir_model::COP_FPTRUNC) &&
          (!is_float_type(instruction.source_type) || !is_float_type(instruction.type)))
        throw LowirError("invalid floating conversion");
      if (instruction.conversion_operator == lowir_model::COP_FPEXT && instruction.type.float_kind <= instruction.source_type.float_kind)
        throw LowirError("fpext does not widen floating type");
      if (instruction.conversion_operator == lowir_model::COP_FPTRUNC && instruction.type.float_kind >= instruction.source_type.float_kind)
        throw LowirError("fptrunc does not narrow floating type");
      define_value(&instruction, instruction.type, &values);
      break;
    }
    case Instruction::IK_ATOMIC_ADD_FETCH:
      require_pointer_operand(instruction.first, "atomic add pointer");
      require_operand_type(instruction.second, instruction.type, "atomic add value");
      if (instruction.atomic_order == lowir_model::AO_INVALID) throw LowirError("invalid atomic order");
      define_value(&instruction, instruction.type, &values);
      break;
    case Instruction::IK_ATOMIC_EXCHANGE:
      require_pointer_operand(instruction.first, "atomic exchange pointer");
      require_operand_type(instruction.second, instruction.type, "atomic exchange value");
      if (instruction.atomic_order == lowir_model::AO_INVALID) throw LowirError("invalid atomic order");
      define_value(&instruction, instruction.type, &values);
      break;
    case Instruction::IK_ATOMIC_COMPARE_EXCHANGE:
      require_pointer_operand(instruction.first, "atomic compare pointer");
      require_pointer_operand(instruction.second, "atomic expected pointer");
      require_operand_type(instruction.third, instruction.type, "atomic desired value");
      if (instruction.atomic_order == lowir_model::AO_INVALID || instruction.atomic_failure_order == lowir_model::AO_INVALID)
        throw LowirError("invalid atomic order");
      define_value(&instruction, i64_type(), &values);
      break;
    case Instruction::IK_ATOMIC_THREAD_FENCE:
    case Instruction::IK_ATOMIC_SIGNAL_FENCE:
      if (instruction.atomic_order == lowir_model::AO_INVALID) throw LowirError("invalid atomic order");
      break;
    case Instruction::IK_CALL:
      validate_call(instruction);
      if (!instruction.call_returns_void) define_value(&instruction, instruction.call_return_type, &values);
      break;
    case Instruction::IK_COPYOBJ: {
      if (instruction.byte_count == 0 || instruction.byte_alignment == 0 || (instruction.byte_alignment & (instruction.byte_alignment - 1)) != 0)
        throw LowirError("invalid storage operation alignment");
      require(instruction.first);
      if (!is_memory_operand(instruction.second)) throw LowirError("copy destination is not pointer");
      const Value source = operand_value(instruction.first);
      if (!source.known || (!source.type.is_pointer() && !source.type.is_object())) throw LowirError("copy source is not pointer or object");
      if (source.type.is_object() && (source.type.storage_size() != instruction.byte_count || source.type.storage_alignment() != instruction.byte_alignment))
        throw LowirError("copy object shape mismatch");
      break;
    }
    case Instruction::IK_ZEROINIT:
      if (instruction.byte_count == 0 || instruction.byte_alignment == 0 || (instruction.byte_alignment & (instruction.byte_alignment - 1)) != 0)
        throw LowirError("invalid storage operation alignment");
      require(instruction.first);
      if (!is_memory_operand(instruction.first)) throw LowirError("zero destination is not pointer");
      break;
    case Instruction::IK_EH_TRY:
    case Instruction::IK_EH_CLEANUP:
      require_block(instruction.first, blocks);
      break;
    case Instruction::IK_EH_CATCH:
      if (instruction.first.kind != Operand::OP_GLOBAL || !instruction.first.symbol_id.valid())
        throw LowirError("invalid exception catch symbol");
      break;
    case Instruction::IK_EH_FILTER:
      for (std::size_t i = 0; i < instruction.args.size(); ++i)
        if (instruction.args[i].kind != Operand::OP_GLOBAL || !instruction.args[i].symbol_id.valid())
          throw LowirError("invalid exception filter symbol");
      break;
    case Instruction::IK_EH_CATCH_ALL:
    case Instruction::IK_EH_END:
      break;
    case Instruction::IK_THROW:
      require(instruction.first);
      break;
    case Instruction::IK_EXCEPTION:
      if (instruction.type.is_void()) throw LowirError("void exception value");
      define_value(&instruction, instruction.type, &values);
      break;
    case Instruction::IK_EXCEPTION_SELECTOR:
      define_value(&instruction, instruction.type, &values);
      break;
    case Instruction::IK_RESUME:
      break;
    case Instruction::IK_JUMP:
      require_block(instruction.first, blocks);
      break;
    case Instruction::IK_BRANCH:
      require(instruction.first);
      require_block(instruction.second, blocks);
      require_block(instruction.third, blocks);
      break;
    case Instruction::IK_SWITCH:
      require(instruction.first);
      require_block(instruction.second, blocks);
      if (instruction.args.size() % 2 != 0) throw LowirError("invalid switch arms");
      for (std::size_t i = 0; i < instruction.args.size(); i += 2) {
        require(instruction.args[i]);
        require_block(instruction.args[i + 1], blocks);
      }
      break;
    case Instruction::IK_RETURN:
      if (instruction.type != function.return_type) throw LowirError("return type mismatch");
      if (!instruction.type.is_void()) require_operand_type(instruction.first, function.return_type, "return value");
      break;
    default:
      throw LowirError("unsupported instruction");
    }
  }

  Program &program_;
  std::map<std::string, lowir_model::SymbolId> symbol_names_;
  std::vector<Symbol> symbols_;
  std::vector<const Function::Slot *> slot_owners_;
  std::vector<const Block *> block_owners_;
  std::set<lowir_model::SymbolId> tls_wrappers_;
  lowir_model::SymbolId entry_id_;
  lowir_model::SymbolId init_id_;
  lowir_model::SymbolId fini_id_;
};

struct Location {
  std::size_t offset;
  bool valid;
  Location() : offset(0), valid(false) {}
  explicit Location(std::size_t o) : offset(o), valid(true) {}
};

class FunctionLayout {
public:
  FunctionLayout(const Function &function, const Validator &validator)
      : function_(function), validator_(validator), frame_size_(0), f80_scratch_offset_(0), has_f80_(false),
        value_locations_(function.value_count), slot_locations_(function.slot_count) {
    build();
  }

  const Location &find(const lowir_model::ValueId id) const {
    if (!id.valid() || !function_.value_begin.valid() || id.index < function_.value_begin.index ||
        id.index >= function_.value_begin.index + function_.value_count)
      throw LowirError("value identity is outside function range");
    const Location &location = value_locations_[id.index - function_.value_begin.index];
    if (!location.valid) throw LowirError("missing emitted value location");
    return location;
  }

  const Location &find(const lowir_model::SlotId id) const {
    if (!id.valid() || !function_.slot_begin.valid() || id.index < function_.slot_begin.index ||
        id.index >= function_.slot_begin.index + function_.slot_count)
      throw LowirError("slot identity is outside function range");
    const Location &location = slot_locations_[id.index - function_.slot_begin.index];
    if (!location.valid) throw LowirError("missing emitted slot location");
    return location;
  }

  const Location &find(const Operand &operand) const {
    if (operand.kind == Operand::OP_TEMP) return find(operand.value_id);
    if (operand.kind == Operand::OP_SLOT) return find(operand.slot_id);
    throw LowirError("missing emitted local location");
  }

  LowType type(const Operand &operand) const {
    if (operand.kind == Operand::OP_TEMP) return validator_.value_type(operand.value_id);
    if (operand.kind == Operand::OP_SLOT) return validator_.slot_type(operand.slot_id);
    throw LowirError("missing emitted local type");
  }

  std::size_t frame_size() const { return frame_size_; }

  bool has_f80() const { return has_f80_; }

  std::size_t f80_scratch_offset() const {
    if (f80_scratch_offset_ == 0) throw LowirError("f80 scratch requested without scratch storage");
    return f80_scratch_offset_;
  }

private:
  Location allocate_frame(const LowType &type) {
    const std::size_t size = type.is_float() && type.float_kind == LowType::FLOAT_F80 ? 16 : (type.is_object() ? type_storage_size(type) : 8);
    const std::size_t alignment = type.is_float() && type.float_kind == LowType::FLOAT_F80 ? 8 : (type.is_object() ? type_storage_alignment(type) : 8);
    if (alignment > 1) {
      const std::size_t remainder = frame_size_ % alignment;
      if (remainder != 0) frame_size_ += alignment - remainder;
    }
    frame_size_ += size;
    return Location(frame_size_);
  }

  void allocate(const lowir_model::ValueId id, const LowType &type) {
    if (!id.valid() || id.index < function_.value_begin.index || id.index >= function_.value_begin.index + function_.value_count)
      throw LowirError("value identity is outside function range");
    Location &location = value_locations_[id.index - function_.value_begin.index];
    if (location.valid) throw LowirError("duplicate emitted location");
    location = allocate_frame(type);
  }

  void allocate(const lowir_model::SlotId id, const LowType &type) {
    if (!id.valid() || id.index < function_.slot_begin.index || id.index >= function_.slot_begin.index + function_.slot_count)
      throw LowirError("slot identity is outside function range");
    Location &location = slot_locations_[id.index - function_.slot_begin.index];
    if (location.valid) throw LowirError("duplicate emitted location");
    location = allocate_frame(type);
  }

  void build() {
    const bool hidden_return = function_.return_type.is_object() || (function_.return_type.is_float() && function_.return_type.float_kind == LowType::FLOAT_F80);
    if (hidden_return) frame_size_ += 8;
    for (std::size_t i = 0; i < function_.params.size(); ++i) allocate(function_.params[i].value_id, function_.params[i].type);
    for (std::size_t i = 0; i < function_.slots.size(); ++i) allocate(function_.slots[i].slot_id, function_.slots[i].type);
    bool needs_scratch = false;
    for (std::size_t b = 0; b < function_.blocks.size(); ++b) {
      for (std::size_t i = 0; i < function_.blocks[b].instructions.size(); ++i) {
        const Instruction &instruction = function_.blocks[b].instructions[i];
        if (instruction.dest_id.valid()) {
          const LowType type = validator_.value_type(instruction.dest_id);
          if (type.valid()) allocate(instruction.dest_id, type);
        }
        if (instruction.kind == Instruction::IK_CONVERT) needs_scratch = true;
        if ((instruction.type.is_float() && instruction.type.float_kind == LowType::FLOAT_F80) ||
            (instruction.source_type.is_float() && instruction.source_type.float_kind == LowType::FLOAT_F80) ||
            (instruction.call_return_type.is_float() && instruction.call_return_type.float_kind == LowType::FLOAT_F80))
          has_f80_ = true;
        if (instruction.kind == Instruction::IK_CALL) {
          const std::vector<Parameter> *parameters = &instruction.call_params;
          if (instruction.direct_callee_id.valid()) {
            const Function *target = validator_.function_for(instruction.direct_callee_id);
            const FunctionDeclaration *declaration = validator_.function_declaration_for(instruction.direct_callee_id);
            if (target != 0) parameters = &target->params;
            else if (declaration != 0) parameters = &declaration->params;
          }
          for (std::size_t p = 0; p < parameters->size(); ++p)
            if ((*parameters)[p].type.is_float() && (*parameters)[p].type.float_kind == LowType::FLOAT_F80) has_f80_ = true;
        }
      }
    }
    for (std::size_t i = 0; i < function_.params.size(); ++i)
      if (function_.params[i].type.is_float() && function_.params[i].type.float_kind == LowType::FLOAT_F80) has_f80_ = true;
    if (function_.return_type.is_float() && function_.return_type.float_kind == LowType::FLOAT_F80) has_f80_ = true;
    for (std::size_t i = 0; i < function_.slots.size(); ++i)
      if (function_.slots[i].type.is_float() && function_.slots[i].type.float_kind == LowType::FLOAT_F80) has_f80_ = true;
    if (has_f80_ || needs_scratch) {
      f80_scratch_offset_ = frame_size_ + 16;
      frame_size_ += 64;
    }
  }

  const Function &function_;
  const Validator &validator_;
  std::size_t frame_size_;
  std::size_t f80_scratch_offset_;
  bool has_f80_;
  std::vector<Location> value_locations_;
  std::vector<Location> slot_locations_;
};

class Emitter {
public:
  Emitter(const Program &program, const Validator &validator) : program_(program), validator_(validator), out_() {}

  std::string emit() {
    const Function *entry = validator_.entry_function();
    if (entry == 0) throw LowirError("missing entry function");
    collect_emission_facts();
    emit_start(entry);
    for (std::size_t i = 0; i < program_.functions.size(); ++i) {
      emit_function(program_.functions[i]);
      if (i + 1 < program_.functions.size() || !program_.globals.empty() || has_eh_) blank();
    }
    if (has_eh_) {
      emit_eh_unhandled_function();
      blank();
    }
    for (std::size_t i = 0; i < program_.globals.size(); ++i) {
      emit_global(program_.globals[i]);
    }
    if (has_eh_ && !program_.globals.empty()) blank();
    if (has_eh_) {
      emit_eh_runtime_global("@__cppgm_eh_top");
      blank();
      emit_eh_runtime_global("@__cppgm_eh_value");
    }
    return out_.str();
  }

private:
  void line(const std::string &text) { out_ << text << '\n'; }

  void blank() { out_ << '\n'; }

  static std::string width_name(const LowType &type) {
    if (type.is_float() && type.float_kind == LowType::FLOAT_F32) return "32";
    if (type.is_float() && type.float_kind == LowType::FLOAT_F64) return "64";
    if (type.is_float() && type.float_kind == LowType::FLOAT_F80) return "80";
    const int width = integer_width(type);
    if (width == 0) return "64";
    std::ostringstream result;
    result << (width == 1 ? 8 : width);
    return result.str();
  }

  static std::string reg64(const std::string &reg) {
    if (reg == "x" || reg == "y" || reg == "z" || reg == "t") return reg + "64";
    if (reg == "x8" || reg == "x16" || reg == "x32" || reg == "x64") return "x64";
    if (reg == "y8" || reg == "y16" || reg == "y32" || reg == "y64") return "y64";
    if (reg == "z8" || reg == "z16" || reg == "z32" || reg == "z64") return "z64";
    if (reg == "t8" || reg == "t16" || reg == "t32" || reg == "t64") return "t64";
    return reg;
  }

  static std::string address_at(std::size_t offset) {
    std::ostringstream result;
    result << "[bp-" << offset << "]";
    return result.str();
  }

  static std::string plus_address(std::size_t offset, std::size_t addend) {
    std::ostringstream result;
    result << "[bp-" << offset;
    if (addend != 0) result << "+" << addend;
    result << "]";
    return result.str();
  }

  static std::string immediate_signed(long long value) {
    std::ostringstream result;
    result << value;
    return result.str();
  }

  void collect_emission_facts() {
    has_eh_ = false;
    for (std::size_t i = 0; i < program_.functions.size(); ++i) {
      const Function &function = program_.functions[i];
      for (std::size_t b = 0; b < function.blocks.size(); ++b) {
        for (std::size_t j = 0; j < function.blocks[b].instructions.size(); ++j) {
          const Instruction::Kind kind = function.blocks[b].instructions[j].kind;
          if (kind == Instruction::IK_EH_TRY || kind == Instruction::IK_EH_CLEANUP || kind == Instruction::IK_EH_END || kind == Instruction::IK_THROW ||
              kind == Instruction::IK_EXCEPTION || kind == Instruction::IK_RESUME) {
            has_eh_ = true;
          }
        }
      }
    }
  }

  void emit_start(const Function *entry) {
    line("start:");
    line("\tmove64 bp sp;");
    const Function *init = validator_.init_function();
    const Function *fini = validator_.fini_function();
    if (init != 0) {
      line("\tcall " + validator_.symbol_label(init->symbol_id) + ";");
    }
    line("\tcall " + validator_.symbol_label(entry->symbol_id) + ";");
    if (fini != 0) {
      line("\tisub64 sp sp 8;");
      line("\tmove64 [sp] x64;");
      line("\tcall " + validator_.symbol_label(fini->symbol_id) + ";");
      line("\tmove64 x64 [sp];");
      line("\tiadd64 sp sp 8;");
    }
    line("\tsyscall1 t64 60 x64;");
    blank();
  }

  std::string stack_operand(const Location &location) const { return address_at(location.offset); }

  void emit_function(const Function &function) {
    FunctionLayout layout(function, validator_);
    current_function_ = &function;
    current_layout_ = &layout;
    line(validator_.symbol_label(function.symbol_id) + ":");
    line("\tisub64 sp sp 8;");
    line("\tmove64 [sp] bp;");
    line("\tmove64 bp sp;");
    if (layout.frame_size() != 0) {
      line("\tisub64 sp sp " + immediate_signed(static_cast<long long>(layout.frame_size())) + ";");
    }
    emit_parameter_spills(function);
    for (std::size_t b = 0; b < function.blocks.size(); ++b) {
      emit_block(function, function.blocks[b]);
    }
    line(validator_.symbol_label(function.symbol_id) + "__epilogue:");
    line("\tmove64 sp bp;");
    line("\tmove64 bp [sp];");
    line("\tiadd64 sp sp 8;");
    line("\tret;");
    current_function_ = 0;
    current_layout_ = 0;
  }

  std::string parameter_register(std::size_t index) const {
    static const char *const registers[] = {"x", "y", "z", "t"};
    if (index < 4) return registers[index];
    return "x";
  }

  void emit_parameter_spills(const Function &function) {
    const std::size_t hidden = function.return_type.is_object() || (function.return_type.is_float() && function.return_type.float_kind == LowType::FLOAT_F80) ? 1 : 0;
    if (hidden) line("\tmove64 [bp-8] x64;");
    for (std::size_t i = 0; i < function.params.size(); ++i) {
      const Parameter &parameter = function.params[i];
      const Location &location = current_layout_->find(parameter.value_id);
      const std::size_t register_index = i + hidden;
      const std::string reg = parameter_register(register_index);
      if (parameter.type.is_float() && parameter.type.float_kind == LowType::FLOAT_F80) {
        if (register_index < 4)
          line("\tmove64 x64 " + reg + "64;");
        else
          line("\tmove64 x64 [bp+" + immediate_signed(16 + (register_index - 4) * 8) + "];");
        line("\tmove64 z64 [x64];");
        line("\tmove64 " + stack_operand(location) + " z64;");
        line("\tmove64 z64 [x64+8];");
        line("\tmove64 " + address_at(location.offset - 8) + " z64;");
        continue;
      }
      if (parameter.type.is_object()) {
        if (register_index < 4)
          line("\tmove64 x64 " + reg + "64;");
        else
          line("\tmove64 x64 [bp+" + immediate_signed(16 + (register_index - 4) * 8) + "];");
        const std::size_t bytes = type_storage_size(parameter.type);
        std::size_t copied = 0;
        while (copied + 8 <= bytes) {
          line("\tmove64 z64 [x64" + (copied == 0 ? std::string() : "+" + immediate_signed(copied)) + "];");
          line("\tmove64 " + plus_address(location.offset, copied) + " z64;");
          copied += 8;
        }
        while (copied < bytes) {
          const std::size_t width = bytes - copied >= 4 ? 4 : bytes - copied >= 2 ? 2 : 1;
          line("\tmove" + immediate_signed(width * 8) + " z" + immediate_signed(width * 8) + " [x64" +
               (copied == 0 ? std::string() : "+" + immediate_signed(copied)) + "];");
          line("\tmove" + immediate_signed(width * 8) + " " + plus_address(location.offset, copied) + " z" + immediate_signed(width * 8) + ";");
          copied += width;
        }
        continue;
      }
      if (register_index < 4) {
        emit_store_register_to_location(location, parameter.type, reg + width_name(parameter.type));
      } else {
        emit_load_memory_to_register("[bp+" + immediate_signed(16 + (register_index - 4) * 8) + "]", "x", parameter.type);
        emit_store_register_to_location(location, parameter.type, "x" + width_name(parameter.type));
      }
    }
  }

  void emit_store_register_to_location(const Location &location, const LowType &type, const std::string &source_register) {
    const std::string width = width_name(type);
    const std::string source = source_register == "x" + width ? source_register : source_register;
    if (type.is_float() && type.float_kind == LowType::FLOAT_F32)
      line("\tmove32 " + stack_operand(location) + " " + source + ";");
    else if ((type.is_float() && type.float_kind == LowType::FLOAT_F64) || type.is_pointer() || is_integer_type(type))
      line("\tmove" + width + " " + stack_operand(location) + " " + source + ";");
    else if (type.is_float() && type.float_kind == LowType::FLOAT_F80) {
      line("\tmove64 " + stack_operand(location) + " " + source + ";");
    }
  }

  void emit_load_memory_to_register(const std::string &memory, const std::string &register_name, const LowType &type) {
    if (type.is_float() && type.float_kind == LowType::FLOAT_F32)
      line("\tmove32 " + register_name + "32 " + memory + ";");
    else if ((type.is_float() && type.float_kind == LowType::FLOAT_F64) || type.is_pointer() || is_integer_type(type))
      line("\tmove" + width_name(type) + " " + register_name + width_name(type) + " " + memory + ";");
  }

  void emit_block(const Function &function, const Block &block) {
    line(cy_block_label(validator_.symbol_label(function.symbol_id), validator_.block_label(block.block_id)) + ":");
    for (std::size_t i = 0; i < block.instructions.size(); ++i) emit_instruction(function, block.instructions[i]);
  }

  void emit_zero_extend_register(const std::string &base, const LowType &type) {
    const int width = integer_width(type);
    if (width != 0 && width < 32) line("\tmove64 " + reg64(base) + " 0;");
  }

  void emit_load_value(const Operand &operand, const LowType &type, const std::string &base_register) {
    if (type.is_float() && type.float_kind == LowType::FLOAT_F80) throw LowirError("f80 value requires memory lowering");
    const std::string full_register = reg64(base_register);
    const std::string register_name = is_float_type(type) ? base_register + width_name(type) : base_register + width_name(type);
    if (operand.kind == Operand::OP_INTEGER) {
      const std::string move_width = is_integer_type(type) ? "64" : width_name(type);
      line("\tmove" + move_width + " " + (is_integer_type(type) ? full_register : register_name) + " " + immediate_signed(operand.int_value) + ";");
      return;
    }
    if (operand.kind == Operand::OP_FLOAT) {
      std::string literal = validator_.presentation(operand.presentation_id);
      if (type.is_float() && type.float_kind == LowType::FLOAT_F32 && literal[literal.size() - 1] != 'f' && literal[literal.size() - 1] != 'F') literal += "f";
      line("\tmove" + width_name(type) + " " + register_name + " " + literal + ";");
      return;
    }
    if (operand.kind == Operand::OP_GLOBAL) {
      const std::string &label = validator_.symbol_label(operand.symbol_id);
      if (validator_.is_function_symbol(operand.symbol_id))
        line("\tmove64 " + full_register + " " + label + ";");
      else if (type.is_float() && type.float_kind == LowType::FLOAT_F32)
        line("\tmove32 " + register_name + " [" + label + "];");
      else
        line("\tmove" + width_name(type) + " " + register_name + " [" + label + "];");
      return;
    }
    const Location &location = current_layout_->find(operand);
    emit_zero_extend_register(base_register, type);
    if (type.is_float() && type.float_kind == LowType::FLOAT_F32)
      line("\tmove32 " + register_name + " " + stack_operand(location) + ";");
    else
      line("\tmove" + width_name(type) + " " + register_name + " " + stack_operand(location) + ";");
  }

  std::string f80_scratch_low(std::size_t index) const { return address_at(current_layout_->f80_scratch_offset() + index * 16); }

  std::string f80_scratch_high(std::size_t index) const { return address_at(current_layout_->f80_scratch_offset() + index * 16 - 8); }

  void emit_f80_padding(std::size_t index) {
    const std::size_t offset = current_layout_->f80_scratch_offset() + index * 16;
    line("\tmove64 z64 0;");
    line("\tmove32 " + address_at(offset - 10) + " z32;");
    line("\tmove16 " + address_at(offset - 14) + " z16;");
  }

  void emit_f80_copy(const std::string &source_low, const std::string &source_high, const std::string &destination_low, const std::string &destination_high) {
    line("\tmove64 z64 " + source_low + ";");
    line("\tmove64 " + destination_low + " z64;");
    line("\tmove64 z64 " + source_high + ";");
    line("\tmove64 " + destination_high + " z64;");
  }

  void emit_f80_literal(const Operand &operand, std::size_t scratch) {
    const std::string literal = operand.presentation_id.valid() ? validator_.presentation(operand.presentation_id) : "0.0L";
    line("\tmove80 " + f80_scratch_low(scratch) + " " + literal + ";");
    emit_f80_padding(scratch);
  }

  void emit_f80_operand_to_scratch(const Operand &operand, std::size_t scratch) {
    if (operand.kind == Operand::OP_FLOAT) {
      emit_f80_literal(operand, scratch);
      return;
    }
    std::string source_low;
    std::string source_high;
    if (operand.kind == Operand::OP_GLOBAL) {
      line("\tmove64 x64 " + validator_.symbol_label(operand.symbol_id) + ";");
      source_low = "[x64]";
      source_high = "[x64+8]";
    } else if (operand.kind == Operand::OP_TEMP || operand.kind == Operand::OP_SLOT) {
      const Location &location = current_layout_->find(operand);
      emit_address_of_location(location, "x64");
      source_low = "[x64]";
      source_high = "[x64+8]";
    } else {
      throw LowirError("invalid f80 operand");
    }
    emit_f80_copy(source_low, source_high, f80_scratch_low(scratch), f80_scratch_high(scratch));
  }

  void emit_f80_result_from_scratch(const lowir_model::ValueId destination, std::size_t scratch) {
    const Location &location = current_layout_->find(destination);
    emit_f80_copy(f80_scratch_low(scratch), f80_scratch_high(scratch), stack_operand(location), address_at(location.offset - 8));
  }

  void emit_f80_storage_address(const Operand &storage, const std::string &reg) {
    if (storage.kind == Operand::OP_GLOBAL) {
      line("\tmove64 " + reg + " " + validator_.symbol_label(storage.symbol_id) + ";");
    } else if (storage.kind == Operand::OP_SLOT) {
      emit_address_of_location(current_layout_->find(storage), reg);
    } else {
      emit_load_pointer(storage, reg == "x64" ? "x" : "y");
    }
  }

  void emit_f80_load_instruction(const Instruction &instruction) {
    emit_f80_storage_address(instruction.first, "x64");
    emit_f80_copy("[x64]", "[x64+8]", f80_scratch_low(0), f80_scratch_high(0));
    emit_f80_result_from_scratch(instruction.dest_id, 0);
  }

  void emit_f80_store_instruction(const Instruction &instruction) {
    emit_f80_operand_to_scratch(instruction.first, 0);
    emit_f80_storage_address(instruction.second, "x64");
    emit_f80_copy(f80_scratch_low(0), f80_scratch_high(0), "[x64]", "[x64+8]");
  }

  void emit_load_pointer(const Operand &operand, const std::string &base_register) {
    if (operand.kind == Operand::OP_GLOBAL) {
      if (validator_.is_function_symbol(operand.symbol_id))
        line("\tmove64 " + reg64(base_register) + " " + validator_.symbol_label(operand.symbol_id) + ";");
      else
        line("\tmove64 " + reg64(base_register) + " [" + validator_.symbol_label(operand.symbol_id) + "];");
      return;
    }
    if (operand.kind == Operand::OP_INTEGER) {
      line("\tmove64 " + reg64(base_register) + " " + immediate_signed(operand.int_value) + ";");
      return;
    }
    const Location &location = current_layout_->find(operand);
    if (operand.kind == Operand::OP_SLOT && !current_layout_->type(operand).is_pointer() &&
        !current_layout_->type(operand).is_object()) {
      emit_address_of_location(location, reg64(base_register));
    } else {
      line("\tmove64 " + reg64(base_register) + " " + stack_operand(location) + ";");
    }
  }

  void emit_address_of_location(const Location &location, const std::string &reg) {
    line("\tisub64 " + reg + " bp " + immediate_signed(static_cast<long long>(location.offset)) + ";");
  }

  void emit_result(const lowir_model::ValueId destination, const std::string &base_register) {
    const Location &location = current_layout_->find(destination);
    const LowType type = validator_.value_type(destination);
    const std::string width = width_name(type);
    const std::string reg = base_register + width;
    if (type.is_float() && type.float_kind == LowType::FLOAT_F32)
      line("\tmove32 " + stack_operand(location) + " " + reg + ";");
    else if (type.is_float() && type.float_kind == LowType::FLOAT_F80)
      throw LowirError("f80 result requires memory lowering");
    else
      line("\tmove" + width + " " + stack_operand(location) + " " + reg + ";");
  }

  void emit_address_result(const lowir_model::ValueId destination, const std::string &source) { emit_result(destination, source); }

  void emit_addr_instruction(const Instruction &instruction) {
    if (instruction.first.kind == Operand::OP_SLOT) {
      emit_address_of_location(current_layout_->find(instruction.first), "x64");
    } else if (instruction.first.kind == Operand::OP_GLOBAL) {
      line("\tmove64 x64 " + validator_.symbol_label(instruction.first.symbol_id) + ";");
    } else {
      throw LowirError("invalid addr operand");
    }
    emit_address_result(instruction.dest_id, "x");
  }

  bool is_direct_storage(const Operand &operand) const { return operand.kind == Operand::OP_SLOT || operand.kind == Operand::OP_GLOBAL; }

  void emit_load_instruction(const Instruction &instruction) {
    const LowType &type = instruction.type;
    if (type.is_float() && type.float_kind == LowType::FLOAT_F80) {
      emit_f80_load_instruction(instruction);
      return;
    }
    const bool atomic = instruction.kind == Instruction::IK_ATOMIC_LOAD;
    if (is_direct_storage(instruction.first)) {
      if (instruction.first.kind == Operand::OP_SLOT) {
        emit_load_memory_to_register(stack_operand(current_layout_->find(instruction.first)), "x", type);
      } else {
        emit_load_memory_to_register("[" + validator_.symbol_label(instruction.first.symbol_id) + "]", "x", type);
      }
    } else {
      emit_load_pointer(instruction.first, atomic ? "y" : "x");
      if (type.is_float() && type.float_kind == LowType::FLOAT_F32)
        line("\tmove32 x32 [x64];");
      else if (atomic)
        line("\tmove" + width_name(type) + " x" + width_name(type) + " [y64];");
      else
        line("\tmove" + width_name(type) + " x" + width_name(type) + " [x64];");
    }
    if (!is_direct_storage(instruction.first) && type.is_integer() && type.integer_kind == LowType::INTEGER_I32) {
      line("\tmove8 t8 32;");
      line("\tlshift64 x64 x64 t8;");
      line("\tsrshift64 x64 x64 t8;");
    }
    emit_result(instruction.dest_id, "x");
  }

  void emit_store_instruction(const Instruction &instruction) {
    const LowType &type = instruction.type;
    if (type.is_float() && type.float_kind == LowType::FLOAT_F80) {
      emit_f80_store_instruction(instruction);
      return;
    }
    if (instruction.kind == Instruction::IK_ATOMIC_STORE && !is_direct_storage(instruction.second)) {
      emit_load_pointer(instruction.second, "y");
      emit_load_value(instruction.first, type, "x");
      line("\tmove" + width_name(type) + " [y64] x" + width_name(type) + ";");
      return;
    }
    emit_load_value(instruction.first, type, "x");
    if (is_direct_storage(instruction.second)) {
      const std::string destination = instruction.second.kind == Operand::OP_SLOT ? stack_operand(current_layout_->find(instruction.second))
                                                                                  : "[" + validator_.symbol_label(instruction.second.symbol_id) + "]";
      line("\tmove" + width_name(type) + " " + destination + " x" + width_name(type) + ";");
    } else {
      emit_load_pointer(instruction.second, "y");
      line("\tmove" + width_name(type) + " [y64] x" + width_name(type) + ";");
    }
  }

  void emit_index_instruction(const Instruction &instruction) {
    if (instruction.first.kind == Operand::OP_TEMP && current_layout_->type(instruction.first).is_object()) {
      emit_load_value(instruction.first, pointer_type(), "y");
    } else {
      emit_load_pointer(instruction.first, "y");
    }
    emit_load_value(instruction.second, i64_type(), "x");
    const std::size_t scale = type_storage_size(instruction.type);
    if (scale != 1) {
      line("\tmove64 z64 " + immediate_signed(static_cast<long long>(scale)) + ";");
      line("\tsmul64 x64 x64 z64;");
    }
    line("\tiadd64 x64 y64 x64;");
    emit_address_result(instruction.dest_id, "x");
  }

  static std::string unary_spelling(UnaryOperator op) {
    switch (op) {
    case lowir_model::UOP_NEG: return "neg";
    case lowir_model::UOP_NOT: return "not";
    case lowir_model::UOP_BITNOT: return "bitnot";
    case lowir_model::UOP_DECAY: return "decay";
    case lowir_model::UOP_BSWAP: return "bswap";
    default: throw LowirError("unsupported unary operator");
    }
  }

  static std::string binary_spelling(BinaryOperator op) {
    switch (op) {
    case lowir_model::BOP_ADD: return "add";
    case lowir_model::BOP_SUB: return "sub";
    case lowir_model::BOP_MUL: return "mul";
    case lowir_model::BOP_DIV: return "div";
    case lowir_model::BOP_MOD: return "mod";
    case lowir_model::BOP_UDIV: return "udiv";
    case lowir_model::BOP_UMOD: return "umod";
    case lowir_model::BOP_AND: return "and";
    case lowir_model::BOP_OR: return "or";
    case lowir_model::BOP_XOR: return "xor";
    case lowir_model::BOP_SHL: return "shl";
    case lowir_model::BOP_SHR: return "shr";
    case lowir_model::BOP_USHR: return "ushr";
    default: throw LowirError("unsupported binary operator");
    }
  }

  static std::string compare_spelling(ComparePredicate predicate) {
    switch (predicate) {
    case lowir_model::CPP_EQ: return "eq";
    case lowir_model::CPP_NE: return "ne";
    case lowir_model::CPP_LT: return "lt";
    case lowir_model::CPP_LE: return "le";
    case lowir_model::CPP_GT: return "gt";
    case lowir_model::CPP_GE: return "ge";
    case lowir_model::CPP_ULT: return "ult";
    case lowir_model::CPP_ULE: return "ule";
    case lowir_model::CPP_UGT: return "ugt";
    case lowir_model::CPP_UGE: return "uge";
    default: throw LowirError("unsupported comparison predicate");
    }
  }

  void emit_unary_instruction(const Instruction &instruction) {
    const LowType &type = instruction.type;
    if (type.is_float() && type.float_kind == LowType::FLOAT_F80) {
      if (instruction.unary_operator != lowir_model::UOP_NEG) throw LowirError("unsupported f80 unary operator");
      emit_f80_operand_to_scratch(instruction.first, 0);
      Operand zero;
      zero.kind = Operand::OP_FLOAT;
      zero.float_value = 0.0L;
      emit_f80_literal(zero, 1);
      line("\tfsub80 " + f80_scratch_low(2) + " " + f80_scratch_low(1) + " " + f80_scratch_low(0) + ";");
      emit_f80_padding(2);
      emit_f80_result_from_scratch(instruction.dest_id, 2);
      return;
    }
    if (instruction.unary_operator == lowir_model::UOP_DECAY) {
      emit_load_value(instruction.first, type, "x");
      emit_result(instruction.dest_id, "x");
      return;
    }
    if (instruction.unary_operator == lowir_model::UOP_NOT) {
      emit_load_value(instruction.first, type, "x");
      line("\tieq" + width_name(type) + " z8 x" + width_name(type) + " 0;");
      emit_boolean_result(instruction.dest_id);
      return;
    }
    emit_load_value(instruction.first, type, "x");
    if (instruction.unary_operator == lowir_model::UOP_NEG) {
      line("\tmove64 y64 0;");
      line("\tisub" + width_name(type) + " x" + width_name(type) + " y" + width_name(type) + " x" + width_name(type) + ";");
    } else if (instruction.unary_operator == lowir_model::UOP_BITNOT) {
      line("\tnot" + width_name(type) + " x" + width_name(type) + " x" + width_name(type) + ";");
    } else if (instruction.unary_operator == lowir_model::UOP_BSWAP) {
      line("\tbswap" + width_name(type) + " x" + width_name(type) + " x" + width_name(type) + ";");
    } else {
      throw LowirError("unsupported unary operator");
    }
    emit_result(instruction.dest_id, "x");
  }

  std::string integer_binary_opcode(BinaryOperator op, const LowType &type) const {
    const std::string width = width_name(type);
    switch (op) {
    case lowir_model::BOP_ADD: return "iadd" + width;
    case lowir_model::BOP_SUB: return "isub" + width;
    case lowir_model::BOP_MUL:
      return (type.integer_kind == LowType::INTEGER_U8 || type.integer_kind == LowType::INTEGER_U16 || type.integer_kind == LowType::INTEGER_U32 ||
                      type.integer_kind == LowType::INTEGER_U64
                  ? "umul"
                  : "smul") +
             width;
    case lowir_model::BOP_DIV: return "sdiv" + width;
    case lowir_model::BOP_MOD: return "smod" + width;
    case lowir_model::BOP_UDIV: return "udiv" + width;
    case lowir_model::BOP_UMOD: return "umod" + width;
    case lowir_model::BOP_AND: return "and" + width;
    case lowir_model::BOP_OR: return "or" + width;
    case lowir_model::BOP_XOR: return "xor" + width;
    case lowir_model::BOP_SHL: return "lshift" + width;
    case lowir_model::BOP_SHR: return "srshift" + width;
    case lowir_model::BOP_USHR: return "urshift" + width;
    default: throw LowirError("unsupported integer binary operator");
    }
  }

  void emit_binary_instruction(const Instruction &instruction) {
    const LowType &type = instruction.type;
    const std::string op = binary_spelling(instruction.binary_operator);
    if (type.is_float() && type.float_kind == LowType::FLOAT_F80) {
      emit_f80_operand_to_scratch(instruction.first, 0);
      emit_f80_operand_to_scratch(instruction.second, 1);
      if (instruction.binary_operator != lowir_model::BOP_ADD && instruction.binary_operator != lowir_model::BOP_SUB &&
          instruction.binary_operator != lowir_model::BOP_MUL && instruction.binary_operator != lowir_model::BOP_DIV)
        throw LowirError("unsupported f80 binary operator");
      line("\tf" + op + "80 " + f80_scratch_low(2) + " " + f80_scratch_low(0) + " " + f80_scratch_low(1) + ";");
      emit_f80_padding(2);
      emit_f80_result_from_scratch(instruction.dest_id, 2);
      return;
    }
    emit_load_value(instruction.first, type, "y");
    emit_load_value(instruction.second, type, "x");
    if (is_float_type(type)) {
      line("\tf" + op + width_name(type) + " x" + width_name(type) + " y" + width_name(type) + " x" + width_name(type) + ";");
    } else {
      const std::string width = width_name(type);
      if (instruction.binary_operator == lowir_model::BOP_SHL || instruction.binary_operator == lowir_model::BOP_SHR ||
          instruction.binary_operator == lowir_model::BOP_USHR) {
        line("\tmove64 z64 x64;");
        line("\tmove8 x8 z8;");
        line("\t" + integer_binary_opcode(instruction.binary_operator, type) + " x" + width + " y" + width + " x8;");
      } else {
        line("\t" + integer_binary_opcode(instruction.binary_operator, type) + " x" + width + " y" + width + " x" + width + ";");
      }
    }
    emit_result(instruction.dest_id, "x");
  }

  std::string compare_opcode(ComparePredicate predicate, const LowType &type) const {
    const std::string width = width_name(type);
    const std::string name = compare_spelling(predicate);
    if (is_float_type(type)) return "f" + name + width;
    if (predicate == lowir_model::CPP_EQ || predicate == lowir_model::CPP_NE) return "i" + name + width;
    if (predicate == lowir_model::CPP_LT || predicate == lowir_model::CPP_LE || predicate == lowir_model::CPP_GT || predicate == lowir_model::CPP_GE)
      return "s" + name + width;
    if (predicate == lowir_model::CPP_ULT || predicate == lowir_model::CPP_ULE || predicate == lowir_model::CPP_UGT || predicate == lowir_model::CPP_UGE)
      return "u" + name.substr(1) + width;
    throw LowirError("unsupported comparison predicate");
  }

  void emit_boolean_result(const lowir_model::ValueId destination) {
    line("\tmove64 x64 0;");
    line("\tmove8 x8 z8;");
    emit_result(destination, "x");
  }

  void emit_cmp_instruction(const Instruction &instruction) {
    const LowType &type = instruction.type;
    if (type.is_float() && type.float_kind == LowType::FLOAT_F80) {
      emit_f80_operand_to_scratch(instruction.first, 0);
      emit_f80_operand_to_scratch(instruction.second, 1);
      line("\tf" + compare_spelling(instruction.compare_predicate) + "80 z8 " + f80_scratch_low(0) + " " + f80_scratch_low(1) + ";");
      emit_boolean_result(instruction.dest_id);
      return;
    }
    emit_load_value(instruction.first, type, "y");
    emit_load_value(instruction.second, type, "x");
    line("\t" + compare_opcode(instruction.compare_predicate, type) + " z8 y" + width_name(type) + " x" + width_name(type) + ";");
    emit_boolean_result(instruction.dest_id);
  }

  void emit_integer_conversion(const Instruction &instruction) {
    const LowType &dst = instruction.type;
    const LowType &src = instruction.source_type;
    emit_load_value(instruction.first, src, "x");
    const int dst_width = integer_width(dst);
    const int src_width = integer_width(src);
    if ((instruction.conversion_operator == lowir_model::COP_SEXT || instruction.conversion_operator == lowir_model::COP_ZEXT) && dst_width > src_width &&
        (instruction.conversion_operator == lowir_model::COP_SEXT || instruction.first.kind != Operand::OP_INTEGER)) {
      const int shift = dst_width - src_width;
      line("\tmove8 t8 " + immediate_signed(shift) + ";");
      line("\tlshift" + width_name(dst) + " x" + width_name(dst) + " x" + width_name(dst) + " t8;");
      line("\t" + std::string(instruction.conversion_operator == lowir_model::COP_SEXT ? "srshift" : "urshift") + width_name(dst) + " x" + width_name(dst) +
           " x" + width_name(dst) + " t8;");
    }
    emit_result(instruction.dest_id, "x");
  }

  void emit_convert_instruction(const Instruction &instruction) {
    const LowType &dst = instruction.type;
    const LowType &src = instruction.source_type;
    if (is_integer_type(dst) && is_integer_type(src)) {
      emit_integer_conversion(instruction);
      return;
    }
    const bool dst_f80 = dst.is_float() && dst.float_kind == LowType::FLOAT_F80;
    const bool src_f80 = src.is_float() && src.float_kind == LowType::FLOAT_F80;
    if (dst_f80) {
      emit_load_or_convert_to_f80(instruction.first, src, instruction.conversion_operator, 0);
      emit_f80_result_from_scratch(instruction.dest_id, 0);
      return;
    }
    if (src_f80) {
      emit_f80_operand_to_scratch(instruction.first, 0);
      const Location &destination = current_layout_->find(instruction.dest_id);
      if (is_float_type(dst)) {
        line("\tf80convf" + width_name(dst) + " " + stack_operand(destination) + " " + f80_scratch_low(0) + ";");
      } else if (is_integer_type(dst)) {
        line("\tf80conv" + std::string(instruction.conversion_operator == lowir_model::COP_FPTOUI ? "u" : "s") + width_name(dst) + " " +
             stack_operand(destination) + " " + f80_scratch_low(0) + ";");
      } else
        throw LowirError("unsupported f80 conversion destination");
      return;
    }
    if (is_float_type(src) && is_float_type(dst)) {
      emit_load_or_convert_to_f80(instruction.first, src, instruction.conversion_operator, 0);
      const Location &destination = current_layout_->find(instruction.dest_id);
      line("\tf80convf" + width_name(dst) + " " + stack_operand(destination) + " " + f80_scratch_low(0) + ";");
      return;
    }
    if (is_integer_type(src) && is_float_type(dst)) {
      emit_load_or_convert_to_f80(instruction.first, src, instruction.conversion_operator, 0);
      const Location &destination = current_layout_->find(instruction.dest_id);
      line("\tf80convf" + width_name(dst) + " " + stack_operand(destination) + " " + f80_scratch_low(0) + ";");
      return;
    }
    if (is_float_type(src) && is_integer_type(dst)) {
      emit_load_or_convert_to_f80(instruction.first, src, instruction.conversion_operator, 0);
      const Location &destination = current_layout_->find(instruction.dest_id);
      line("\tf80conv" + std::string(instruction.conversion_operator == lowir_model::COP_FPTOUI ? "u" : "s") + width_name(dst) + " " +
           stack_operand(destination) + " " + f80_scratch_low(0) + ";");
      return;
    }
    throw LowirError("unsupported floating conversion");
  }

  void emit_load_or_convert_to_f80(const Operand &operand, const LowType &source_type, ConversionOperator conversion, std::size_t scratch) {
    if (source_type.is_float() && source_type.float_kind == LowType::FLOAT_F80) {
      emit_f80_operand_to_scratch(operand, scratch);
      return;
    }
    emit_load_value(operand, source_type, "x");
    if (is_float_type(source_type)) {
      line("\tf" + width_name(source_type) + "convf80 " + f80_scratch_low(scratch) + " x" + width_name(source_type) + ";");
    } else if (is_integer_type(source_type)) {
      const std::string prefix = conversion == lowir_model::COP_UITOFP ? "u" : "s";
      line("\t" + prefix + width_name(source_type) + "convf80 " + f80_scratch_low(scratch) + " x" + width_name(source_type) + ";");
    } else
      throw LowirError("unsupported conversion source");
    emit_f80_padding(scratch);
  }

  struct CallView {
    const std::vector<Parameter> *parameters;
    LowType return_type;
    lowir_model::SymbolId target_id;
    bool direct;
    CallView() : parameters(0), direct(false) {}
  };

  CallView call_view(const Instruction &instruction) const {
    CallView result;
    result.target_id = instruction.direct_callee_id;
    if (instruction.direct_callee_id.valid()) {
      result.direct = true;
      const Function *function = validator_.function_for(instruction.direct_callee_id);
      const FunctionDeclaration *declaration = validator_.function_declaration_for(instruction.direct_callee_id);
      if (function != 0) {
        result.parameters = &function->params;
        result.return_type = function->return_type;
      } else if (declaration != 0) {
        result.parameters = &declaration->params;
        result.return_type = declaration->return_type;
      } else {
        throw LowirError("missing direct call target");
      }
    } else {
      result.parameters = &instruction.call_params;
      result.return_type = instruction.call_return_type;
    }
    return result;
  }

  void emit_object_address(const Operand &operand, const std::string &reg) {
    if (operand.kind == Operand::OP_SLOT || operand.kind == Operand::OP_TEMP) {
      emit_address_of_location(current_layout_->find(operand), reg);
    } else if (operand.kind == Operand::OP_GLOBAL) {
      line("\tmove64 " + reg + " " + validator_.symbol_label(operand.symbol_id) + ";");
    } else {
      throw LowirError("object argument is not addressable");
    }
  }

  void emit_call_argument(const Operand &argument, const Parameter *parameter, std::size_t index) {
    const std::string reg = parameter_register(index);
    const LowType type = parameter == 0 ? i64_type() : parameter->type;
    if (parameter != 0 && type.is_float() && type.float_kind == LowType::FLOAT_F80) {
      if (argument.kind == Operand::OP_TEMP || argument.kind == Operand::OP_SLOT) {
        emit_object_address(argument, "x64");
      } else {
        emit_f80_operand_to_scratch(argument, 3);
        emit_address_of_location(Location(current_layout_->f80_scratch_offset() + 3 * 16), "x64");
      }
      line("\tmove64 " + reg + "64 x64;");
      return;
    }
    if (parameter != 0 && type.is_object()) {
      emit_object_address(argument, "x64");
      line("\tmove64 " + reg + "64 x64;");
      return;
    }
    if (parameter != 0 && type.is_pointer() && argument.kind == Operand::OP_SLOT && !current_layout_->type(argument).is_pointer()) {
      emit_object_address(argument, "x64");
      line("\tmove64 " + reg + "64 x64;");
      return;
    }
    emit_load_value(argument, type, reg);
  }

  void emit_call_instruction(const Instruction &instruction) {
    const CallView target = call_view(instruction);
    const bool direct = target.direct;
    if (!direct) {
      emit_load_pointer(instruction.first, "x");
      line("\tisub64 sp sp 8;");
      line("\tmove64 [sp] x64;");
    }
    const bool hidden_return = direct && !instruction.call_returns_void &&
                               (target.return_type.is_object() || (target.return_type.is_float() && target.return_type.float_kind == LowType::FLOAT_F80));
    const std::size_t hidden = hidden_return ? 1 : 0;
    const std::size_t register_arguments = 4 - hidden;
    const std::size_t stack_arguments = instruction.args.size() > register_arguments ? instruction.args.size() - register_arguments : 0;
    if (hidden_return) {
      emit_object_address(OperandForDestination(instruction.dest_id), "x64");
      line("\tmove64 x64 x64;");
    }
    if (stack_arguments != 0) {
      line("\tisub64 sp sp " + immediate_signed(static_cast<long long>(stack_arguments * 8)) + ";");
    }
    for (std::size_t i = 0; i < instruction.args.size(); ++i) {
      const Parameter *parameter = target.parameters != 0 && i < target.parameters->size() ? &(*target.parameters)[i] : 0;
      const std::size_t register_index = i + hidden;
      emit_call_argument(instruction.args[i], parameter, register_index);
      if (register_index >= 4) {
        line("\tmove64 [sp] 0;");
        line("\tmove64 [sp] x64;");
      }
    }
    if (direct)
      line("\tcall " + validator_.symbol_label(target.target_id) + ";");
    else {
      line("\tcall [sp];");
      line("\tiadd64 sp sp 8;");
    }
    if (direct && stack_arguments != 0) {
      line("\tiadd64 sp sp " + immediate_signed(static_cast<long long>(stack_arguments * 8)) + ";");
    }
    if (!instruction.call_returns_void && !target.return_type.is_object() && !(target.return_type.is_float() && target.return_type.float_kind == LowType::FLOAT_F80)) {
      emit_result(instruction.dest_id, "x");
    }
  }

  Operand OperandForDestination(const lowir_model::ValueId destination) const {
    Operand operand;
    operand.kind = Operand::OP_TEMP;
    operand.value_id = destination;
    return operand;
  }

  void emit_copy_bytes(const Operand &source, const Operand &destination, std::size_t bytes) {
    if (destination.kind == Operand::OP_TEMP && current_layout_->type(destination).is_object())
      emit_object_address(destination, "x64");
    else
      emit_load_pointer(destination, "x");
    if (source.kind == Operand::OP_TEMP && current_layout_->type(source).is_object())
      emit_object_address(source, "y64");
    else
      emit_load_pointer(source, "y");
    std::size_t copied = 0;
    while (bytes - copied >= 8) {
      line("\tmove64 z64 [y64];");
      line("\tmove64 [x64] z64;");
      copied += 8;
      if (copied < bytes) {
        line("\tiadd64 x64 x64 8;");
        line("\tiadd64 y64 y64 8;");
      }
    }
    while (copied < bytes) {
      const std::size_t remaining = bytes - copied;
      const std::size_t width = remaining >= 4 ? 4 : remaining >= 2 ? 2 : 1;
      line("\tmove" + immediate_signed(static_cast<long long>(width * 8)) + " z" + immediate_signed(static_cast<long long>(width * 8)) + " [y64];");
      line("\tmove" + immediate_signed(static_cast<long long>(width * 8)) + " [x64] z" + immediate_signed(static_cast<long long>(width * 8)) + ";");
      copied += width;
      if (copied < bytes) {
        line("\tiadd64 x64 x64 " + immediate_signed(static_cast<long long>(width)) + ";");
        line("\tiadd64 y64 y64 " + immediate_signed(static_cast<long long>(width)) + ";");
      }
    }
  }

  void emit_zero_bytes(const Operand &destination, std::size_t bytes) {
    emit_load_pointer(destination, "x");
    line("\tmove64 z64 0;");
    std::size_t written = 0;
    while (bytes - written >= 8) {
      line("\tmove64 [x64] z64;");
      written += 8;
      if (written < bytes) line("\tiadd64 x64 x64 8;");
    }
    while (written < bytes) {
      const std::size_t remaining = bytes - written;
      const std::size_t width = remaining >= 4 ? 4 : remaining >= 2 ? 2 : 1;
      line("\tmove" + immediate_signed(static_cast<long long>(width * 8)) + " [x64] z" + immediate_signed(static_cast<long long>(width * 8)) + ";");
      written += width;
      if (written < bytes) line("\tiadd64 x64 x64 " + immediate_signed(static_cast<long long>(width)) + ";");
    }
  }

  void emit_atomic_exchange(const Instruction &instruction) {
    emit_load_pointer(instruction.first, "y");
    emit_load_value(instruction.second, instruction.type, "x");
    line("\tmove" + width_name(instruction.type) + " t" + width_name(instruction.type) + " [y64];");
    line("\tmove" + width_name(instruction.type) + " [y64] x" + width_name(instruction.type) + ";");
    line("\tmove64 x64 0;");
    if (width_name(instruction.type) == "64")
      line("\tmove64 x64 t64;");
    else
      line("\tmove" + width_name(instruction.type) + " x" + width_name(instruction.type) + " t" + width_name(instruction.type) + ";");
    emit_result(instruction.dest_id, "x");
  }

  void emit_atomic_add_fetch(const Instruction &instruction) {
    emit_load_pointer(instruction.first, "y");
    line("\tmove" + width_name(instruction.type) + " x" + width_name(instruction.type) + " [y64];");
    emit_load_value(instruction.second, instruction.type, "z");
    line("\tiadd" + width_name(instruction.type) + " x" + width_name(instruction.type) + " x" + width_name(instruction.type) + " z" +
         width_name(instruction.type) + ";");
    line("\tmove" + width_name(instruction.type) + " [y64] x" + width_name(instruction.type) + ";");
    emit_result(instruction.dest_id, "x");
  }

  void emit_atomic_compare_exchange(const Instruction &instruction) {
    const std::string success = "__atomic_cmpxchg_success__" + immediate_signed(atomic_label_counter_++);
    const std::string end = "__atomic_cmpxchg_end__" + immediate_signed(atomic_label_counter_++);
    emit_load_pointer(instruction.first, "y");
    emit_load_pointer(instruction.second, "z");
    line("\tmove" + width_name(instruction.type) + " t" + width_name(instruction.type) + " [y64];");
    line("\tmove" + width_name(instruction.type) + " x" + width_name(instruction.type) + " [z64];");
    line("\tieq" + width_name(instruction.type) + " x8 t" + width_name(instruction.type) + " x" + width_name(instruction.type) + ";");
    line("\tjumpif x8 " + success + ";");
    line("\tmove" + width_name(instruction.type) + " [z64] t" + width_name(instruction.type) + ";");
    line("\tmove64 x64 0;");
    emit_result(instruction.dest_id, "x");
    line("\tjump " + end + ";");
    line(success + ":");
    emit_load_value(instruction.third, instruction.type, "x");
    line("\tmove" + width_name(instruction.type) + " [y64] x" + width_name(instruction.type) + ";");
    line("\tmove64 x64 1;");
    emit_result(instruction.dest_id, "x");
    line(end + ":");
  }

  void emit_branch(const Function &function, const Instruction &instruction) {
    emit_load_value(instruction.first, i64_type(), "x");
    line("\tieq64 z8 x64 0;");
    line("\tjumpif z8 " + cy_block_label(validator_.symbol_label(function.symbol_id), validator_.block_label(instruction.third.block_id)) + ";");
    line("\tjump " + cy_block_label(validator_.symbol_label(function.symbol_id), validator_.block_label(instruction.second.block_id)) + ";");
  }

  void emit_switch(const Function &function, const Instruction &instruction) {
    emit_load_value(instruction.first, i64_type(), "x");
    for (std::size_t i = 0; i < instruction.args.size(); i += 2) {
      emit_load_value(instruction.args[i], i64_type(), "t");
      line("\tieq64 z8 x64 t64;");
      line("\tjumpif z8 " + cy_block_label(validator_.symbol_label(function.symbol_id), validator_.block_label(instruction.args[i + 1].block_id)) + ";");
    }
    line("\tjump " + cy_block_label(validator_.symbol_label(function.symbol_id), validator_.block_label(instruction.second.block_id)) + ";");
  }

  void emit_eh_push(const Function &function, const Operand &target) {
    line("\tisub64 sp sp 32;");
    line("\tmove64 z64 [g____cppgm_eh_top];");
    line("\tmove64 [sp] z64;");
    line("\tmove64 z64 " + cy_block_label(validator_.symbol_label(function.symbol_id), validator_.block_label(target.block_id)) + ";");
    line("\tmove64 [sp+8] z64;");
    line("\tmove64 [sp+16] bp;");
    line("\tmove64 z64 sp;");
    line("\tiadd64 z64 z64 32;");
    line("\tmove64 [sp+24] z64;");
    line("\tmove64 z64 sp;");
    line("\tmove64 [g____cppgm_eh_top] z64;");
  }

  void emit_eh_end() {
    line("\tmove64 x64 [g____cppgm_eh_top];");
    line("\tmove64 y64 [x64];");
    line("\tmove64 [g____cppgm_eh_top] y64;");
    line("\tmove64 sp x64;");
    line("\tiadd64 sp sp 32;");
  }

  void emit_eh_unwind() {
    const std::string handler = "__eh_handler__" + immediate_signed(eh_label_counter_++);
    const std::string unhandled = "__eh_unhandled__" + immediate_signed(eh_label_counter_++);
    line("\tmove64 x64 [g____cppgm_eh_top];");
    line("\tieq64 z8 x64 0;");
    line("\tjumpif z8 " + unhandled + ";");
    line(handler + ":");
    line("\tmove64 y64 [x64];");
    line("\tmove64 [g____cppgm_eh_top] y64;");
    line("\tmove64 z64 [x64+8];");
    line("\tmove64 bp [x64+16];");
    line("\tmove64 sp [x64+24];");
    line("\tjump z64;");
    line(unhandled + ":");
    line("\tmove64 x64 [g____cppgm_eh_value];");
    line("\tcall fn____cppgm_eh_unhandled;");
    line("\tsyscall1 t64 60 x64;");
    blank();
  }

  void emit_eh_exception(const Instruction &instruction) {
    if (instruction.type.is_float() && instruction.type.float_kind == LowType::FLOAT_F32)
      line("\tmove32 x32 [g____cppgm_eh_value];");
    else
      line("\tmove" + width_name(instruction.type) + " x" + width_name(instruction.type) + " [g____cppgm_eh_value];");
    emit_result(instruction.dest_id, "x");
  }

  void emit_eh_unhandled_function() {
    line("fn____cppgm_eh_unhandled:");
    line("\tsyscall1 t64 60 x64;");
  }

  void emit_eh_runtime_global(const std::string &name) {
    line(cy_global(name) + ":");
    line("\tdata64 0;");
  }

  void emit_return(const Function &function, const Instruction &instruction) {
    if (instruction.type.is_void()) {
      line("\tjump " + validator_.symbol_label(function.symbol_id) + "__epilogue;");
      return;
    }
    if (instruction.type.is_object()) {
      emit_object_address(instruction.first, "x64");
      line("\tmove64 y64 [bp-8];");
      const std::size_t bytes = type_storage_size(instruction.type);
      std::size_t copied = 0;
      while (copied + 8 <= bytes) {
        line("\tmove64 z64 [x64" + (copied == 0 ? std::string() : "+" + immediate_signed(copied)) + "];");
        line("\tmove64 [y64" + (copied == 0 ? std::string() : "+" + immediate_signed(copied)) + "] z64;");
        copied += 8;
      }
      while (copied < bytes) {
        const std::size_t width = bytes - copied >= 4 ? 4 : bytes - copied >= 2 ? 2 : 1;
        line("\tmove" + immediate_signed(width * 8) + " z" + immediate_signed(width * 8) + " [x64" +
             (copied == 0 ? std::string() : "+" + immediate_signed(copied)) + "];");
        line("\tmove" + immediate_signed(width * 8) + " [y64" + (copied == 0 ? std::string() : "+" + immediate_signed(copied)) + "] z" +
             immediate_signed(width * 8) + ";");
        copied += width;
      }
      line("\tjump " + validator_.symbol_label(function.symbol_id) + "__epilogue;");
      return;
    }
    if (instruction.type.is_float() && instruction.type.float_kind == LowType::FLOAT_F80) {
      emit_f80_operand_to_scratch(instruction.first, 0);
      line("\tmove64 x64 [bp-8];");
      emit_f80_copy(f80_scratch_low(0), f80_scratch_high(0), "[x64]", "[x64+8]");
      line("\tjump " + validator_.symbol_label(function.symbol_id) + "__epilogue;");
      return;
    }
    emit_load_value(instruction.first, instruction.type, "x");
    line("\tjump " + validator_.symbol_label(function.symbol_id) + "__epilogue;");
  }

  void emit_instruction(const Function &function, const Instruction &instruction) {
    switch (instruction.kind) {
    case Instruction::IK_CONST:
      if (instruction.type.is_float() && instruction.type.float_kind == LowType::FLOAT_F80) {
        emit_f80_operand_to_scratch(instruction.first, 0);
        emit_f80_result_from_scratch(instruction.dest_id, 0);
      } else {
        emit_load_value(instruction.first, instruction.type, "x");
        emit_result(instruction.dest_id, "x");
      }
      break;
    case Instruction::IK_COPY:
      if (instruction.type.is_float() && instruction.type.float_kind == LowType::FLOAT_F80) {
        emit_f80_operand_to_scratch(instruction.first, 0);
        emit_f80_result_from_scratch(instruction.dest_id, 0);
      } else {
        emit_load_value(instruction.first, instruction.type, "x");
        emit_result(instruction.dest_id, "x");
      }
      break;
    case Instruction::IK_ADDR:
      emit_addr_instruction(instruction);
      break;
    case Instruction::IK_LOAD:
    case Instruction::IK_ATOMIC_LOAD:
      emit_load_instruction(instruction);
      break;
    case Instruction::IK_STORE:
    case Instruction::IK_ATOMIC_STORE:
      emit_store_instruction(instruction);
      break;
    case Instruction::IK_INDEX:
      emit_index_instruction(instruction);
      break;
    case Instruction::IK_UNARY:
      emit_unary_instruction(instruction);
      break;
    case Instruction::IK_BINARY:
      emit_binary_instruction(instruction);
      break;
    case Instruction::IK_CMP:
      emit_cmp_instruction(instruction);
      break;
    case Instruction::IK_CONVERT:
      emit_convert_instruction(instruction);
      break;
    case Instruction::IK_ATOMIC_ADD_FETCH:
      emit_atomic_add_fetch(instruction);
      break;
    case Instruction::IK_ATOMIC_EXCHANGE:
      emit_atomic_exchange(instruction);
      break;
    case Instruction::IK_ATOMIC_COMPARE_EXCHANGE:
      emit_atomic_compare_exchange(instruction);
      break;
    case Instruction::IK_ATOMIC_THREAD_FENCE:
    case Instruction::IK_ATOMIC_SIGNAL_FENCE:
      break;
    case Instruction::IK_CALL:
      emit_call_instruction(instruction);
      break;
    case Instruction::IK_COPYOBJ:
      emit_copy_bytes(instruction.first, instruction.second, instruction.byte_count);
      break;
    case Instruction::IK_ZEROINIT:
      emit_zero_bytes(instruction.first, instruction.byte_count);
      break;
    case Instruction::IK_JUMP:
      line("\tjump " + cy_block_label(validator_.symbol_label(function.symbol_id), validator_.block_label(instruction.first.block_id)) + ";");
      break;
    case Instruction::IK_BRANCH:
      emit_branch(function, instruction);
      break;
    case Instruction::IK_SWITCH:
      emit_switch(function, instruction);
      break;
    case Instruction::IK_RETURN:
      emit_return(function, instruction);
      break;
    case Instruction::IK_EH_TRY:
    case Instruction::IK_EH_CLEANUP:
      emit_eh_push(function, instruction.first);
      break;
    case Instruction::IK_EH_END:
      emit_eh_end();
      break;
    case Instruction::IK_THROW:
      emit_load_value(instruction.first, instruction.type, "x");
      line("\tmove64 [g____cppgm_eh_value] x64;");
      emit_eh_unwind();
      break;
    case Instruction::IK_EXCEPTION:
    case Instruction::IK_EXCEPTION_SELECTOR:
      emit_eh_exception(instruction);
      break;
    case Instruction::IK_RESUME:
      emit_eh_unwind();
      break;
    case Instruction::IK_EH_CLEANUP_CLAUSE:
    case Instruction::IK_EH_CATCH:
    case Instruction::IK_EH_FILTER:
    case Instruction::IK_EH_CATCH_ALL:
      break;
    default:
      throw LowirError("unsupported PA13 instruction emission");
    }
  }

  std::string data_literal(const Operand &operand) const {
    if (operand.kind == Operand::OP_INTEGER) return immediate_signed(operand.int_value);
    if (operand.kind == Operand::OP_FLOAT) return validator_.presentation(operand.presentation_id);
    return "0";
  }

  void emit_zero_data(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) line("\tdata8 0;");
  }

  void emit_f80_data(const long double value) {
    unsigned char bytes[16];
    std::memset(bytes, 0, sizeof(bytes));
    std::memcpy(bytes, &value, sizeof(long double) < 16 ? sizeof(long double) : 16);
    std::uint64_t low = 0;
    std::uint16_t high = 0;
    std::memcpy(&low, bytes, sizeof(low));
    std::memcpy(&high, bytes + 8, sizeof(high));
    line("\tdata64 " + immediate_signed(static_cast<long long>(low)) + ";");
    line("\tdata16 " + immediate_signed(static_cast<long long>(high)) + ";");
    emit_zero_data(6);
  }

  void emit_global_item(const GlobalDefinition::DataItem &item, std::size_t *offset) {
    if (item.kind == GlobalDefinition::DataItem::ITEM_ZERO) {
      emit_zero_data(item.zero_bytes);
      *offset += item.zero_bytes;
      return;
    }
    const std::size_t alignment = type_storage_alignment(item.type);
    while (alignment > 1 && *offset % alignment != 0) {
      line("\tdata8 0;");
      ++*offset;
    }
    if (item.kind == GlobalDefinition::DataItem::ITEM_ADDR) {
      const std::string &target = validator_.symbol_label(item.symbol_id);
      if (item.addr_addend == 0)
        line("\tdata64 " + target + ";");
      else
        line("\tdata64 (" + target + (item.addr_addend > 0 ? " + " : " - ") + immediate_signed(std::llabs(item.addr_addend)) + ");");
    } else if (item.type.is_float() && item.type.float_kind == LowType::FLOAT_F80) {
      emit_f80_data(item.literal_operand.float_value);
    } else {
      const std::string width = width_name(item.type);
      line("\tdata" + width + " " + data_literal(item.literal_operand) + ";");
    }
    *offset += item.kind == GlobalDefinition::DataItem::ITEM_ADDR ? 8 : type_storage_size(item.type);
  }

  void emit_global(const GlobalDefinition &global) {
    line(validator_.symbol_label(global.symbol_id) + ":");
    if (global.structured) {
      std::size_t offset = 0;
      for (std::size_t i = 0; i < global.data_items.size(); ++i) emit_global_item(global.data_items[i], &offset);
    } else if (global.init_kind == GlobalDefinition::INIT_ADDR) {
      const std::string &target = validator_.symbol_label(global.init_operand.symbol_id);
      if (global.addr_addend == 0)
        line("\tdata64 " + target + ";");
      else
        line("\tdata64 (" + target + (global.addr_addend > 0 ? " + " : " - ") + immediate_signed(std::llabs(global.addr_addend)) + ");");
    } else if (global.init_kind == GlobalDefinition::INIT_ZERO) {
      if (global.type.is_float() && global.type.float_kind == LowType::FLOAT_F80)
        emit_f80_data(0.0L);
      else
        line("\tdata" + width_name(global.type) + " 0;");
    } else if (global.type.is_float() && global.type.float_kind == LowType::FLOAT_F80) {
      emit_f80_data(global.init_operand.float_value);
    } else {
      line("\tdata" + width_name(global.type) + " " + data_literal(global.init_operand) + ";");
    }
    if (&global != &program_.globals.back()) blank();
  }

  const Program &program_;
  const Validator &validator_;
  std::ostringstream out_;
  const Function *current_function_;
  const FunctionLayout *current_layout_;
  std::size_t atomic_label_counter_ = 0;
  std::size_t eh_label_counter_ = 0;
  bool has_eh_ = false;
};

void compile(const std::vector<std::string> &source_files, const std::string &output_file) {
  if (source_files.empty()) throw LowirError("no LowIR source files");
  Program program = lowir_model::parse_lowir_program_files(source_files);
  Validator validator(program);
  validator.validate();
  Emitter emitter(program, validator);
  const std::string output = emitter.emit();
  std::ofstream file(output_file.c_str(), std::ios::out | std::ios::trunc);
  if (!file) throw LowirError("unable to open CY86 output file");
  file << output;
  if (!file) throw LowirError("unable to write CY86 output file");
}

} // namespace lowir2cy86
