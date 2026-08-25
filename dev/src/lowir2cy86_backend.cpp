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

class LowirError : public std::runtime_error {
public:
  explicit LowirError(const std::string &message) : std::runtime_error(message) {}
};

struct Token {
  std::string text;
  std::size_t line;
};

class Lexer {
public:
  explicit Lexer(const std::string &text) { tokenize(text); }

  const std::vector<Token> &tokens() const { return tokens_; }

private:
  void tokenize(const std::string &text) {
    std::size_t i = 0;
    std::size_t line = 1;
    while (i < text.size()) {
      const char c = text[i];
      if (c == '\n') {
        ++line;
        ++i;
      } else if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') {
        ++i;
      } else if (c == '#') {
        while (i < text.size() && text[i] != '\n') {
          ++i;
        }
      } else if (c == '-' && i + 1 < text.size() && text[i + 1] == '>') {
        Token token;
        token.text = "->";
        token.line = line;
        tokens_.push_back(token);
        i += 2;
      } else if (is_punctuation(c)) {
        Token token;
        token.text.assign(1, c);
        token.line = line;
        tokens_.push_back(token);
        ++i;
      } else {
        const std::size_t begin = i;
        while (i < text.size() && !is_space(text[i]) && !is_punctuation(text[i]) && text[i] != '#') {
          ++i;
        }
        if (begin == i) {
          throw LowirError("invalid LowIR character");
        }
        Token token;
        token.text = text.substr(begin, i - begin);
        token.line = line;
        tokens_.push_back(token);
      }
    }
  }

  static bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }

  static bool is_punctuation(char c) {
    switch (c) {
    case '(':
    case ')':
    case '{':
    case '}':
    case '[':
    case ']':
    case ':':
    case ',':
    case '=':
    case '+':
    case '-':
    case '!':
      return true;
    default:
      return false;
    }
  }

  std::vector<Token> tokens_;
};

bool starts_with(const std::string &value, const std::string &prefix) { return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0; }

bool is_integer_text(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  std::size_t i = 0;
  if (text[i] == '+' || text[i] == '-') {
    ++i;
  }
  if (i == text.size()) {
    return false;
  }
  if (i + 2 <= text.size() && text[i] == '0' && (text[i + 1] == 'x' || text[i + 1] == 'X')) {
    i += 2;
    if (i == text.size()) {
      return false;
    }
    for (; i < text.size(); ++i) {
      if (!std::isxdigit(static_cast<unsigned char>(text[i]))) {
        return false;
      }
    }
    return true;
  }
  for (; i < text.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(text[i]))) {
      return false;
    }
  }
  return true;
}

bool is_float_text(const std::string &text) {
  if (text.empty()) {
    return false;
  }
  return text.find('.') != std::string::npos || text.find('e') != std::string::npos || text.find('E') != std::string::npos || text[text.size() - 1] == 'f' ||
         text[text.size() - 1] == 'F' || text[text.size() - 1] == 'L' || text[text.size() - 1] == 'l';
}

long long parse_integer(const std::string &text) {
  errno = 0;
  char *end = 0;
  const long long value = std::strtoll(text.c_str(), &end, 0);
  if (errno == ERANGE || end == text.c_str() || *end != '\0') {
    throw LowirError("invalid integer literal");
  }
  return value;
}

long double parse_float(const std::string &text) {
  std::string normalized = text;
  if (!normalized.empty() && (normalized[normalized.size() - 1] == 'f' || normalized[normalized.size() - 1] == 'F' ||
                              normalized[normalized.size() - 1] == 'l' || normalized[normalized.size() - 1] == 'L')) {
    normalized.erase(normalized.size() - 1);
  }
  errno = 0;
  char *end = 0;
  const long double value = std::strtold(normalized.c_str(), &end);
  if (errno == ERANGE || end == normalized.c_str() || *end != '\0') {
    throw LowirError("invalid floating literal");
  }
  return value;
}

std::string signed_literal_text(const std::string &first, const std::string &second) { return first == "-" ? "-" + second : second; }

bool is_name_kind(const std::string &text, char prefix) { return text.size() > 1 && text[0] == prefix; }

int integer_width(const std::string &text) {
  if (text == "i1") return 1;
  if (text == "i8" || text == "u8") return 8;
  if (text == "i16" || text == "u16") return 16;
  if (text == "i32" || text == "u32") return 32;
  if (text == "i64" || text == "u64") return 64;
  return 0;
}

bool is_float_type(const std::string &text) { return text == "f32" || text == "f64" || text == "f80"; }

bool is_integer_type(const std::string &text) { return integer_width(text) != 0; }

bool is_scalar_type(const std::string &text) { return is_integer_type(text) || is_float_type(text) || text == "ptr"; }

bool is_void_type(const std::string &text) { return text == "void"; }

bool parse_object_type(const std::string &text, std::size_t *bytes, std::size_t *alignment) {
  if (!starts_with(text, "obj<") || text.size() < 7 || text[text.size() - 1] != '>') {
    return false;
  }
  const std::size_t x = text.find('x', 4);
  if (x == std::string::npos || x + 1 >= text.size() - 1) {
    return false;
  }
  const std::string byte_text = text.substr(4, x - 4);
  const std::string align_text = text.substr(x + 1, text.size() - x - 2);
  if (!is_integer_text(byte_text) || !is_integer_text(align_text)) {
    return false;
  }
  const long long b = parse_integer(byte_text);
  const long long a = parse_integer(align_text);
  if (b <= 0 || a <= 0 || (a & (a - 1)) != 0) {
    return false;
  }
  *bytes = static_cast<std::size_t>(b);
  *alignment = static_cast<std::size_t>(a);
  return true;
}

bool is_valid_type(const std::string &text) {
  if (is_scalar_type(text) || is_void_type(text)) {
    return true;
  }
  std::size_t bytes = 0;
  std::size_t alignment = 0;
  return parse_object_type(text, &bytes, &alignment);
}

std::size_t type_storage_size(const std::string &text) {
  if (text == "f80") return 16;
  std::size_t bytes = 0;
  std::size_t alignment = 0;
  if (parse_object_type(text, &bytes, &alignment)) {
    return bytes;
  }
  const int width = integer_width(text);
  if (width != 0) return width <= 8 ? 1 : width <= 16 ? 2 : width <= 32 ? 4 : 8;
  if (text == "f32") return 4;
  if (text == "f64" || text == "ptr") return 8;
  return 0;
}

std::size_t type_storage_alignment(const std::string &text) {
  if (text == "f80") return 8;
  std::size_t bytes = 0;
  std::size_t alignment = 0;
  if (parse_object_type(text, &bytes, &alignment)) return alignment;
  const std::size_t size = type_storage_size(text);
  return size >= 8 ? 8 : size;
}

std::string without_prefix(const std::string &text) { return text.size() > 0 ? text.substr(1) : text; }

std::string cy_function(const std::string &name) { return "fn__" + without_prefix(name); }

std::string cy_global(const std::string &name) { return "g__" + without_prefix(name); }

std::string cy_block(const std::string &function, const std::string &block) { return cy_function(function) + "__" + without_prefix(block); }

bool is_terminator(Instruction::Kind kind) {
  return kind == Instruction::IK_JUMP || kind == Instruction::IK_BRANCH || kind == Instruction::IK_SWITCH || kind == Instruction::IK_RETURN ||
         kind == Instruction::IK_THROW || kind == Instruction::IK_RESUME;
}

} // namespace

class Parser {
public:
  explicit Parser(const std::string &text) : tokens_(Lexer(text).tokens()), position_(0) {}

  Program parse() {
    Program program;
    while (!at_end()) {
      if (accept("declare")) {
        parse_declaration(program);
      } else if (accept("global")) {
        program.globals.push_back(parse_global_definition());
      } else if (accept("function")) {
        program.functions.push_back(parse_function_definition());
      } else if (accept("alias")) {
        program.object_aliases.push_back(parse_object_alias());
      } else {
        fail("expected top-level LowIR item");
      }
    }
    return program;
  }

private:
  const Token &peek() const {
    if (at_end()) {
      throw LowirError("unexpected end of LowIR input");
    }
    return tokens_[position_];
  }

  bool at_end() const { return position_ >= tokens_.size(); }

  bool next_is(const std::string &text) const { return !at_end() && tokens_[position_].text == text; }

  bool accept(const std::string &text) {
    if (next_is(text)) {
      ++position_;
      return true;
    }
    return false;
  }

  std::string take(const std::string &what) {
    if (at_end()) {
      fail("expected " + what);
    }
    return tokens_[position_++].text;
  }

  void expect(const std::string &text) {
    if (!accept(text)) {
      fail("expected '" + text + "'");
    }
  }

  void fail(const std::string &message) const {
    if (at_end()) {
      throw LowirError(message + " at end of input");
    }
    std::ostringstream out;
    out << message << " near '" << tokens_[position_].text << "'";
    throw LowirError(out.str());
  }

  std::string parse_name(char prefix, const std::string &what) {
    const std::string name = take(what);
    if (!is_name_kind(name, prefix)) {
      fail("expected " + what);
    }
    return name;
  }

  LowType parse_type() {
    LowType type;
    type.text = take("type");
    if (!is_valid_type(type.text)) {
      fail("invalid LowIR type");
    }
    return type;
  }

  std::string parse_scalar_text() {
    if (accept("-")) {
      return signed_literal_text("-", take("literal"));
    }
    const std::string value = take("literal");
    if (!is_integer_text(value) && !is_float_text(value) && value != "nullptr") {
      fail("expected scalar literal");
    }
    return value;
  }

  long long parse_integer_literal() {
    const std::string value = parse_scalar_text();
    if (!is_integer_text(value)) {
      fail("expected integer literal");
    }
    return parse_integer(value);
  }

  Operand parse_operand() {
    std::string value;
    if (accept("-"))
      value = signed_literal_text("-", take("literal"));
    else
      value = take("value");
    Operand operand;
    operand.text = value;
    if (is_name_kind(value, '%')) {
      operand.kind = Operand::OP_TEMP;
    } else if (is_name_kind(value, '$')) {
      operand.kind = Operand::OP_SLOT;
    } else if (is_name_kind(value, '@')) {
      operand.kind = Operand::OP_GLOBAL;
    } else if (is_name_kind(value, '^')) {
      operand.kind = Operand::OP_LABEL;
    } else if (value == "nullptr" || is_integer_text(value)) {
      operand.kind = Operand::OP_INTEGER;
      operand.int_value = value == "nullptr" ? 0 : parse_integer(value);
      operand.literal_type.text = "i64";
    } else if (is_float_text(value)) {
      operand.kind = Operand::OP_FLOAT;
      operand.float_value = parse_float(value);
      operand.literal_type.text = "f64";
    } else {
      fail("invalid LowIR operand");
    }
    return operand;
  }

  std::vector<std::pair<std::string, std::string>> parse_metadata_items() {
    std::vector<std::pair<std::string, std::string>> items;
    expect("[");
    if (!next_is("]")) {
      while (true) {
        const std::string key = take("metadata key");
        expect("=");
        const std::string value = take("metadata value");
        for (std::size_t i = 0; i < items.size(); ++i) {
          if (items[i].first == key) {
            fail("duplicate metadata key");
          }
        }
        items.push_back(std::make_pair(key, value));
        if (!accept(",")) {
          break;
        }
      }
    }
    expect("]");
    return items;
  }

  static void require_value(const std::string &value, const std::string &expected, const std::string &key) {
    if (value != expected) {
      throw LowirError("invalid " + key + " metadata value");
    }
  }

  void apply_symbol_metadata(SymbolMetadata *metadata, const std::vector<std::pair<std::string, std::string>> &items, bool global_context) {
    for (std::size_t i = 0; i < items.size(); ++i) {
      const std::string &key = items[i].first;
      const std::string &value = items[i].second;
      if (key == "role") {
        if (value == "entry")
          metadata->role = lowir_model::SR_ENTRY;
        else if (value == "init")
          metadata->role = lowir_model::SR_INIT;
        else if (value == "fini")
          metadata->role = lowir_model::SR_FINI;
        else if (value == "eh_top")
          metadata->role = lowir_model::SR_EH_TOP;
        else if (value == "eh_value")
          metadata->role = lowir_model::SR_EH_VALUE;
        else if (value == "eh_type")
          metadata->role = lowir_model::SR_EH_TYPE;
        else if (value == "eh_unhandled")
          metadata->role = lowir_model::SR_EH_UNHANDLED;
        else if (value == "eh_allocate_exception")
          metadata->role = lowir_model::SR_EH_ALLOCATE_EXCEPTION;
        else if (value == "eh_begin_catch")
          metadata->role = lowir_model::SR_EH_BEGIN_CATCH;
        else if (value == "eh_call_unexpected")
          metadata->role = lowir_model::SR_EH_CALL_UNEXPECTED;
        else if (value == "eh_current_exception_type")
          metadata->role = lowir_model::SR_EH_CURRENT_EXCEPTION_TYPE;
        else if (value == "eh_end_catch")
          metadata->role = lowir_model::SR_EH_END_CATCH;
        else if (value == "eh_rethrow")
          metadata->role = lowir_model::SR_EH_RETHROW;
        else if (value == "eh_throw")
          metadata->role = lowir_model::SR_EH_THROW;
        else if (value == "eh_personality")
          metadata->role = lowir_model::SR_EH_PERSONALITY;
        else if (value == "eh_resume")
          metadata->role = lowir_model::SR_EH_RESUME;
        else
          fail("unknown role metadata");
      } else if (key == "linkage") {
        if (value == "c")
          metadata->linkage = lowir_model::LLM_C;
        else if (value == "cpp")
          metadata->linkage = lowir_model::LLM_CPP;
        else
          fail("unknown linkage metadata");
      } else if (key == "binding") {
        if (value == "internal")
          metadata->binding = lowir_model::SBM_INTERNAL;
        else if (value == "strong")
          metadata->binding = lowir_model::SBM_STRONG;
        else if (value == "weak")
          metadata->binding = lowir_model::SBM_WEAK;
        else
          fail("unknown binding metadata");
      } else if (key == "object") {
        metadata->object_symbol = value;
      } else if (key == "tls_for") {
        if (global_context) fail("tls_for is not global metadata");
        if (!is_name_kind(value, '@')) fail("invalid tls_for metadata");
        metadata->tls_for_symbol = value;
      } else if (key == "keep_alias") {
        if (value != "yes" && value != "no") fail("invalid keep_alias metadata value");
        metadata->keep_internal_alias = value == "yes";
      } else if (key == "prefer_local") {
        if (value != "yes" && value != "no") fail("invalid prefer_local metadata value");
        metadata->prefer_local_object_binding = value == "yes";
      } else if (key == "trivial_lifecycle") {
        if (global_context) fail("invalid global metadata key");
        if (value != "yes" && value != "no") fail("invalid trivial_lifecycle metadata value");
        metadata->object_trivial_lifecycle = value == "yes";
      } else if (key == "force_inline") {
        if (global_context) fail("invalid global metadata key");
        if (value != "yes" && value != "no") fail("invalid force_inline metadata value");
        metadata->force_inline = value == "yes";
      } else if (key == "storage") {
        if (!global_context) fail("storage is not function metadata");
        // Storage is applied by the global parser, where it has a dedicated
        // enum.  Reaching this branch is a caller error.
        fail("internal storage metadata placement error");
      } else {
        fail("unknown symbol metadata key");
      }
    }
  }

  void apply_global_metadata(GlobalStorageMode *storage, SymbolMetadata *metadata, const std::vector<std::pair<std::string, std::string>> &items) {
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (items[i].first == "storage") {
        if (items[i].second == "readonly")
          *storage = lowir_model::GSM_READONLY;
        else if (items[i].second == "thread_local")
          *storage = lowir_model::GSM_THREAD_LOCAL;
        else
          fail("unknown global storage metadata");
      } else {
        std::vector<std::pair<std::string, std::string>> one(1, items[i]);
        apply_symbol_metadata(metadata, one, true);
      }
    }
  }

  void apply_function_metadata(FunctionBoundaryMetadata *boundary, SymbolMetadata *metadata, const std::vector<std::pair<std::string, std::string>> &items,
                               bool call_signature) {
    for (std::size_t i = 0; i < items.size(); ++i) {
      const std::string &key = items[i].first;
      const std::string &value = items[i].second;
      if (key == "arity") {
        if (value == "fixed")
          boundary->arity = lowir_model::CAM_FIXED;
        else if (value == "variadic")
          boundary->arity = lowir_model::CAM_VARIADIC;
        else if (value == "prototype_relaxed")
          boundary->arity = lowir_model::CAM_PROTOTYPE_RELAXED;
        else
          fail("unknown function arity metadata");
      } else if (key == "effects") {
        if (value == "readnone")
          boundary->effects = lowir_model::CFXM_READNONE;
        else if (value == "readonly")
          boundary->effects = lowir_model::CFXM_READONLY;
        else if (value == "readwrite")
          boundary->effects = lowir_model::CFXM_READWRITE;
        else
          fail("unknown function effect metadata");
      } else if (key == "unwind") {
        if (value == "may")
          boundary->unwind = lowir_model::CUM_MAY;
        else if (value == "no")
          boundary->unwind = lowir_model::CUM_NO;
        else
          fail("unknown function unwind metadata");
      } else if (key == "return") {
        if (value == "returns")
          boundary->returns = lowir_model::CRM_RETURNS;
        else if (value == "noreturn")
          boundary->returns = lowir_model::CRM_NORETURN;
        else
          fail("unknown function return metadata");
      } else {
        if (call_signature) {
          fail("symbol metadata is not legal on a call signature");
        }
        std::vector<std::pair<std::string, std::string>> one(1, items[i]);
        apply_symbol_metadata(metadata, one, false);
      }
    }
  }

  void apply_parameter_metadata(ParameterMetadata *metadata, const std::vector<std::pair<std::string, std::string>> &items) {
    for (std::size_t i = 0; i < items.size(); ++i) {
      const std::string &key = items[i].first;
      const std::string &value = items[i].second;
      if (key == "pass") {
        if (value == "direct")
          metadata->passing = lowir_model::PPM_DIRECT;
        else if (value == "indirect_result")
          metadata->passing = lowir_model::PPM_INDIRECT_RESULT;
        else if (value == "by_address")
          metadata->passing = lowir_model::PPM_BY_ADDRESS;
        else if (value == "reference")
          metadata->passing = lowir_model::PPM_REFERENCE;
        else if (value == "decay")
          metadata->passing = lowir_model::PPM_DECAY;
        else
          fail("unknown parameter pass metadata");
      } else if (key == "capture") {
        if (value == "nocapture")
          metadata->capture = lowir_model::PCM_NOCAPTURE;
        else if (value == "maycapture")
          metadata->capture = lowir_model::PCM_MAYCAPTURE;
        else
          fail("unknown parameter capture metadata");
      } else if (key == "access") {
        if (value == "none")
          metadata->access = lowir_model::PAM_NONE;
        else if (value == "read")
          metadata->access = lowir_model::PAM_READ;
        else if (value == "write")
          metadata->access = lowir_model::PAM_WRITE;
        else if (value == "readwrite")
          metadata->access = lowir_model::PAM_READWRITE;
        else
          fail("unknown parameter access metadata");
      } else if (key == "alias") {
        if (value != "noalias") fail("unknown parameter alias metadata");
        metadata->alias = lowir_model::PALM_NOALIAS;
      } else {
        fail("unknown parameter metadata key");
      }
    }
  }

  Parameter parse_parameter() {
    Parameter parameter;
    parameter.name = parse_name('%', "parameter name");
    expect(":");
    parameter.type = parse_type();
    if (next_is("[")) {
      apply_parameter_metadata(&parameter.metadata, parse_metadata_items());
    }
    return parameter;
  }

  std::vector<Parameter> parse_parameters() {
    std::vector<Parameter> parameters;
    expect("(");
    if (!next_is(")")) {
      while (true) {
        parameters.push_back(parse_parameter());
        if (!accept(",")) break;
      }
    }
    expect(")");
    return parameters;
  }

  InstructionDebugLocation parse_debug_location() {
    InstructionDebugLocation location;
    expect("!");
    expect("dbg");
    expect("(");
    location.file = take("debug file");
    expect(",");
    const long long line = parse_integer_literal();
    expect(",");
    const long long column = parse_integer_literal();
    expect(")");
    if (line <= 0 || column <= 0) {
      fail("debug line and column must be positive");
    }
    location.line = static_cast<std::size_t>(line);
    location.column = static_cast<std::size_t>(column);
    return location;
  }

  void parse_optional_debug(Instruction *instruction) {
    if (next_is("!")) {
      instruction->debug_location = parse_debug_location();
    }
  }

  GlobalDefinition parse_global_definition() {
    GlobalDefinition global;
    global.name = parse_name('@', "global name");
    bool readonly_keyword = accept("readonly");
    bool thread_local_keyword = accept("thread_local");
    if (next_is("[")) {
      apply_global_metadata(&global.storage, &global.metadata, parse_metadata_items());
    }
    if (next_is("=")) {
      global.structured = true;
      if (readonly_keyword) global.storage = lowir_model::GSM_READONLY;
      if (thread_local_keyword) global.storage = lowir_model::GSM_THREAD_LOCAL;
      expect("=");
      expect("{");
      if (next_is("}")) fail("structured global requires data");
      while (!next_is("}")) {
        GlobalDefinition::DataItem item;
        if (accept("ptr")) {
          expect("addr");
          item.kind = GlobalDefinition::DataItem::ITEM_ADDR;
          item.type.text = "ptr";
          item.symbol = take("address symbol");
          if (!is_name_kind(item.symbol, '@')) fail("invalid global address initializer");
          if (accept("+"))
            item.addr_addend = parse_integer_literal();
          else if (accept("-"))
            item.addr_addend = -parse_integer_literal();
        } else if (accept("zero")) {
          item.kind = GlobalDefinition::DataItem::ITEM_ZERO;
          item.zero_bytes = static_cast<std::size_t>(parse_integer_literal());
        } else {
          item.kind = GlobalDefinition::DataItem::ITEM_INTEGER;
          item.type = parse_type();
          item.literal_operand = parse_operand();
          if (item.literal_operand.kind != Operand::OP_INTEGER && item.literal_operand.kind != Operand::OP_FLOAT) {
            fail("structured global item requires scalar literal");
          }
        }
        global.data_items.push_back(item);
      }
      expect("}");
      return global;
    }

    expect(":");
    global.type = parse_type();
    if (next_is("[")) {
      apply_global_metadata(&global.storage, &global.metadata, parse_metadata_items());
    }
    if (readonly_keyword) global.storage = lowir_model::GSM_READONLY;
    if (thread_local_keyword) global.storage = lowir_model::GSM_THREAD_LOCAL;
    expect("=");
    if (accept("zero")) {
      global.init_kind = GlobalDefinition::INIT_ZERO;
    } else if (accept("addr")) {
      global.init_kind = GlobalDefinition::INIT_ADDR;
      global.init_operand = parse_operand();
      if (global.init_operand.kind != Operand::OP_GLOBAL) {
        fail("global address initializer requires a symbol");
      }
      if (accept("+"))
        global.addr_addend = parse_integer_literal();
      else if (accept("-"))
        global.addr_addend = -parse_integer_literal();
    } else {
      global.init_kind = GlobalDefinition::INIT_INTEGER;
      global.init_operand = parse_operand();
      if (global.init_operand.kind != Operand::OP_INTEGER && global.init_operand.kind != Operand::OP_FLOAT) {
        fail("global initializer requires scalar literal");
      }
    }
    return global;
  }

  GlobalDeclaration parse_global_declaration() {
    GlobalDeclaration declaration;
    declaration.name = parse_name('@', "global name");
    if (accept("readonly")) declaration.storage = lowir_model::GSM_READONLY;
    if (accept("thread_local")) declaration.storage = lowir_model::GSM_THREAD_LOCAL;
    if (accept(":")) {
      declaration.has_type = true;
      declaration.type = parse_type();
    }
    if (next_is("[")) {
      apply_global_metadata(&declaration.storage, &declaration.metadata, parse_metadata_items());
    }
    return declaration;
  }

  void parse_declaration(Program &program) {
    if (accept("global")) {
      program.global_declarations.push_back(parse_global_declaration());
    } else if (accept("function")) {
      FunctionDeclaration declaration;
      declaration.name = parse_name('@', "function name");
      declaration.params = parse_parameters();
      expect("->");
      declaration.return_type = parse_type();
      if (next_is("[")) {
        apply_function_metadata(&declaration.boundary, &declaration.metadata, parse_metadata_items(), false);
      }
      if (next_is("!")) {
        // Function declarations do not use debug locations in the PA13
        // grammar; consuming one here would make malformed input look valid.
        fail("debug location is only valid on a function definition");
      }
      program.function_declarations.push_back(declaration);
    } else {
      fail("expected global or function after declare");
    }
  }

  ObjectAlias parse_object_alias() {
    ObjectAlias alias;
    expect("object");
    alias.object_symbol = take("object alias symbol");
    if (alias.object_symbol.empty() || alias.object_symbol[0] == '@' || alias.object_symbol[0] == '%' || alias.object_symbol[0] == '$') {
      fail("invalid object alias symbol");
    }
    expect("=");
    alias.target = take("object alias target");
    if (!is_name_kind(alias.target, '@')) fail("invalid object alias target");
    return alias;
  }

  Function parse_function_definition() {
    Function function;
    function.name = parse_name('@', "function name");
    function.params = parse_parameters();
    expect("->");
    function.return_type = parse_type();
    if (next_is("[")) {
      apply_function_metadata(&function.boundary, &function.metadata, parse_metadata_items(), false);
    }
    if (next_is("!")) {
      function.debug_location = parse_debug_location();
    }
    expect("{");
    Block *current = 0;
    while (!next_is("}")) {
      if (at_end()) fail("unterminated function definition");
      if (accept("slot")) {
        const std::string name = parse_name('$', "slot name");
        expect(":");
        function.slots.push_back(std::make_pair(name, parse_type()));
      } else if (accept("block")) {
        Block block;
        block.label = parse_name('^', "block name");
        expect(":");
        function.blocks.push_back(block);
        current = &function.blocks.back();
      } else {
        if (current == 0) fail("instruction appears before a block");
        current->instructions.push_back(parse_instruction());
      }
    }
    expect("}");
    return function;
  }

  Instruction parse_assignment() {
    Instruction instruction;
    instruction.dest = parse_name('%', "temporary destination");
    expect("=");
    const std::string opcode = take("instruction");
    if (opcode == "const") {
      instruction.kind = Instruction::IK_CONST;
      instruction.type = parse_type();
      instruction.first = parse_operand();
    } else if (opcode == "copy") {
      instruction.kind = Instruction::IK_COPY;
      instruction.type = parse_type();
      instruction.first = parse_operand();
    } else if (opcode == "addr") {
      instruction.kind = Instruction::IK_ADDR;
      instruction.type.text = "ptr";
      instruction.first = parse_operand();
    } else if (opcode == "load" || opcode == "atomic_load") {
      instruction.kind = opcode == "load" ? Instruction::IK_LOAD : Instruction::IK_ATOMIC_LOAD;
      instruction.type = parse_type();
      instruction.first = parse_operand();
      if (instruction.kind == Instruction::IK_ATOMIC_LOAD) {
        expect(",");
        instruction.byte_alignment = static_cast<std::size_t>(parse_integer_literal());
      }
    } else if (opcode == "index") {
      instruction.kind = Instruction::IK_INDEX;
      instruction.type = parse_type();
      if (next_is("[")) {
        const std::vector<std::pair<std::string, std::string>> items = parse_metadata_items();
        if (items.size() != 1 || items[0].first != "projection") {
          fail("invalid index metadata");
        }
        const std::string &value = items[0].second;
        if (value == "array_element")
          instruction.index_projection = lowir_model::IPK_ARRAY_ELEMENT;
        else if (value == "field")
          instruction.index_projection = lowir_model::IPK_FIELD;
        else if (value == "base_subobject")
          instruction.index_projection = lowir_model::IPK_BASE_SUBOBJECT;
        else if (value == "reference_field")
          instruction.index_projection = lowir_model::IPK_REFERENCE_FIELD;
        else
          fail("unknown index projection metadata");
      }
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
    } else if (opcode == "unary") {
      instruction.kind = Instruction::IK_UNARY;
      instruction.op = take("unary operator");
      instruction.type = parse_type();
      instruction.first = parse_operand();
    } else if (opcode == "binary" || opcode == "cmp") {
      instruction.kind = opcode == "binary" ? Instruction::IK_BINARY : Instruction::IK_CMP;
      instruction.op = take(opcode == "binary" ? "binary operator" : "comparison predicate");
      instruction.type = parse_type();
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
    } else if (opcode == "convert") {
      instruction.kind = Instruction::IK_CONVERT;
      instruction.op = take("conversion operator");
      instruction.type = parse_type();
      instruction.source_type = parse_type();
      instruction.first = parse_operand();
    } else if (opcode == "atomic_add_fetch") {
      instruction.kind = Instruction::IK_ATOMIC_ADD_FETCH;
      instruction.type = parse_type();
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
      expect(",");
      instruction.byte_alignment = static_cast<std::size_t>(parse_integer_literal());
    } else if (opcode == "atomic_exchange") {
      instruction.kind = Instruction::IK_ATOMIC_EXCHANGE;
      instruction.type = parse_type();
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
      expect(",");
      instruction.byte_alignment = static_cast<std::size_t>(parse_integer_literal());
    } else if (opcode == "atomic_compare_exchange") {
      instruction.kind = Instruction::IK_ATOMIC_COMPARE_EXCHANGE;
      instruction.type = parse_type();
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
      expect(",");
      instruction.third = parse_operand();
      expect(",");
      instruction.byte_alignment = static_cast<std::size_t>(parse_integer_literal());
      expect(",");
      instruction.eh_selector = parse_integer_literal();
    } else if (opcode == "call") {
      instruction.kind = Instruction::IK_CALL;
      instruction.call_return_type = parse_type();
      instruction.first = parse_operand();
      instruction.args = parse_argument_list();
      parse_call_signature(&instruction);
    } else if (opcode == "exception" || opcode == "exception_selector") {
      instruction.kind = opcode == "exception" ? Instruction::IK_EXCEPTION : Instruction::IK_EXCEPTION_SELECTOR;
      instruction.type = parse_type();
    } else {
      fail("unknown assignment instruction");
    }
    parse_optional_debug(&instruction);
    return instruction;
  }

  std::vector<Operand> parse_argument_list() {
    std::vector<Operand> arguments;
    expect("(");
    if (!next_is(")")) {
      while (true) {
        arguments.push_back(parse_operand());
        if (!accept(",")) break;
      }
    }
    expect(")");
    return arguments;
  }

  void parse_call_signature(Instruction *instruction) {
    if (!accept("as")) return;
    instruction->has_call_signature = true;
    instruction->call_params = parse_parameters();
    expect("->");
    instruction->call_return_type = parse_type();
    if (next_is("[")) {
      SymbolMetadata no_symbol_metadata;
      apply_function_metadata(&instruction->call_boundary, &no_symbol_metadata, parse_metadata_items(), true);
    }
  }

  Instruction parse_instruction() {
    if (next_is("%")) {
      // '%' is not emitted as a separate token by the lexer. This branch is
      // retained only to make malformed punctuation diagnostics explicit.
      fail("invalid temporary spelling");
    }
    if (!at_end() && starts_with(peek().text, "%")) {
      return parse_assignment();
    }

    Instruction instruction;
    const std::string opcode = take("instruction");
    if (opcode == "store" || opcode == "atomic_store") {
      instruction.kind = opcode == "store" ? Instruction::IK_STORE : Instruction::IK_ATOMIC_STORE;
      instruction.type = parse_type();
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
      if (instruction.kind == Instruction::IK_ATOMIC_STORE) {
        expect(",");
        instruction.byte_alignment = static_cast<std::size_t>(parse_integer_literal());
      }
    } else if (opcode == "atomic_thread_fence" || opcode == "atomic_signal_fence") {
      instruction.kind = opcode == "atomic_thread_fence" ? Instruction::IK_ATOMIC_THREAD_FENCE : Instruction::IK_ATOMIC_SIGNAL_FENCE;
      instruction.byte_alignment = static_cast<std::size_t>(parse_integer_literal());
    } else if (opcode == "call") {
      instruction.kind = Instruction::IK_CALL;
      if (accept("void")) {
        instruction.call_returns_void = true;
        instruction.call_return_type.text = "void";
      } else {
        instruction.call_return_type = parse_type();
      }
      instruction.first = parse_operand();
      instruction.args = parse_argument_list();
      parse_call_signature(&instruction);
    } else if (opcode == "copyobj") {
      instruction.kind = Instruction::IK_COPYOBJ;
      const std::string span = take("object size and alignment");
      const std::size_t x = span.find('x');
      if (x == std::string::npos || !is_integer_text(span.substr(0, x)) || !is_integer_text(span.substr(x + 1))) {
        fail("invalid object size and alignment");
      }
      instruction.byte_count = static_cast<std::size_t>(parse_integer(span.substr(0, x)));
      instruction.byte_alignment = static_cast<std::size_t>(parse_integer(span.substr(x + 1)));
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
    } else if (opcode == "zeroinit") {
      instruction.kind = Instruction::IK_ZEROINIT;
      const std::string span = take("object size and alignment");
      const std::size_t x = span.find('x');
      if (x == std::string::npos || !is_integer_text(span.substr(0, x)) || !is_integer_text(span.substr(x + 1))) {
        fail("invalid object size and alignment");
      }
      instruction.byte_count = static_cast<std::size_t>(parse_integer(span.substr(0, x)));
      instruction.byte_alignment = static_cast<std::size_t>(parse_integer(span.substr(x + 1)));
      instruction.first = parse_operand();
    } else if (opcode == "eh_try" || opcode == "eh_cleanup") {
      instruction.kind = opcode == "eh_try" ? Instruction::IK_EH_TRY : Instruction::IK_EH_CLEANUP;
      instruction.first = parse_operand();
    } else if (opcode == "eh_catch") {
      instruction.kind = Instruction::IK_EH_CATCH;
      instruction.first = parse_operand();
    } else if (opcode == "eh_filter") {
      instruction.kind = Instruction::IK_EH_FILTER;
      if (!next_is("!")) {
        instruction.args.push_back(parse_operand());
        while (accept(",")) instruction.args.push_back(parse_operand());
      }
    } else if (opcode == "eh_catch_all")
      instruction.kind = Instruction::IK_EH_CATCH_ALL;
    else if (opcode == "eh_end")
      instruction.kind = Instruction::IK_EH_END;
    else if (opcode == "throw") {
      instruction.kind = Instruction::IK_THROW;
      instruction.type = parse_type();
      instruction.first = parse_operand();
    } else if (opcode == "resume")
      instruction.kind = Instruction::IK_RESUME;
    else if (opcode == "jump") {
      instruction.kind = Instruction::IK_JUMP;
      instruction.first = parse_operand();
    } else if (opcode == "branch") {
      instruction.kind = Instruction::IK_BRANCH;
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
      expect(",");
      instruction.third = parse_operand();
    } else if (opcode == "switch") {
      instruction.kind = Instruction::IK_SWITCH;
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
      while (accept(",")) {
        instruction.args.push_back(parse_operand());
        expect(":");
        instruction.args.push_back(parse_operand());
      }
    } else if (opcode == "return") {
      instruction.kind = Instruction::IK_RETURN;
      instruction.type = parse_type();
      if (instruction.type.text != "void") instruction.first = parse_operand();
    } else {
      fail("unknown LowIR instruction");
    }
    parse_optional_debug(&instruction);
    return instruction;
  }

  std::vector<Token> tokens_;
  std::size_t position_;
};

class Validator {
private:
  enum SymbolKind { SYMBOL_GLOBAL, SYMBOL_FUNCTION };

  struct Symbol {
    SymbolKind kind;
    std::string type;
    bool declaration;
    GlobalStorageMode storage;
    const Function *function;
    const FunctionDeclaration *function_declaration;
  };

  struct Value {
    std::string type;
    bool known;
    Value() : known(false) {}
    explicit Value(const std::string &t) : type(t), known(true) {}
  };

public:
  explicit Validator(const Program &program) : program_(program) {}

  void validate() {
    collect_symbols();
    validate_aliases();
    validate_roles_and_tls();
    for (std::size_t i = 0; i < program_.global_declarations.size(); ++i) {
      validate_global_declaration(program_.global_declarations[i]);
    }
    for (std::size_t i = 0; i < program_.globals.size(); ++i) {
      validate_global(program_.globals[i]);
    }
    for (std::size_t i = 0; i < program_.function_declarations.size(); ++i) {
      validate_function_header(program_.function_declarations[i].params, program_.function_declarations[i].return_type.text, 0);
    }
    for (std::size_t i = 0; i < program_.functions.size(); ++i) {
      validate_function(program_.functions[i]);
    }
    if (entry_function() == 0) {
      throw LowirError("program has no entry function");
    }
  }

  const Function *entry_function() const {
    const Function *selected = 0;
    for (std::size_t i = 0; i < program_.functions.size(); ++i) {
      const Function &function = program_.functions[i];
      const bool legacy = function.name == "@main";
      if (function.metadata.role == lowir_model::SR_ENTRY || (function.metadata.role == lowir_model::SR_NONE && legacy)) {
        if (selected != 0) throw LowirError("multiple entry functions");
        selected = &function;
      }
    }
    return selected;
  }

  const Function *init_function() const { return find_hook(lowir_model::SR_INIT, "@__cppgm_init"); }

  const Function *fini_function() const { return find_hook(lowir_model::SR_FINI, "@__cppgm_fini"); }

private:
  const Function *find_hook(lowir_model::SymbolRole role, const std::string &legacy) const {
    const Function *selected = 0;
    for (std::size_t i = 0; i < program_.functions.size(); ++i) {
      const Function &function = program_.functions[i];
      if (function.metadata.role == role || (function.metadata.role == lowir_model::SR_NONE && function.name == legacy)) {
        if (selected != 0) throw LowirError("multiple runtime hook functions");
        selected = &function;
      }
    }
    return selected;
  }

  void collect_symbols() {
    for (std::size_t i = 0; i < program_.global_declarations.size(); ++i) {
      add_global(program_.global_declarations[i].name, program_.global_declarations[i].has_type ? program_.global_declarations[i].type.text : std::string(),
                 true, program_.global_declarations[i].storage, 0);
    }
    for (std::size_t i = 0; i < program_.globals.size(); ++i) {
      const GlobalDefinition &global = program_.globals[i];
      add_global(global.name, global.structured ? std::string() : global.type.text, false, global.storage, 0);
    }
    for (std::size_t i = 0; i < program_.function_declarations.size(); ++i) {
      Symbol symbol;
      symbol.kind = SYMBOL_FUNCTION;
      symbol.type = program_.function_declarations[i].return_type.text;
      symbol.declaration = true;
      symbol.storage = lowir_model::GSM_DEFAULT;
      symbol.function = 0;
      symbol.function_declaration = &program_.function_declarations[i];
      add_symbol(program_.function_declarations[i].name, symbol);
    }
    for (std::size_t i = 0; i < program_.functions.size(); ++i) {
      Symbol symbol;
      symbol.kind = SYMBOL_FUNCTION;
      symbol.type = program_.functions[i].return_type.text;
      symbol.declaration = false;
      symbol.storage = lowir_model::GSM_DEFAULT;
      symbol.function = &program_.functions[i];
      symbol.function_declaration = 0;
      add_symbol(program_.functions[i].name, symbol);
    }
  }

  void add_global(const std::string &name, const std::string &type, bool declaration, GlobalStorageMode storage, const Function *unused) {
    (void)unused;
    Symbol symbol;
    symbol.kind = SYMBOL_GLOBAL;
    symbol.type = type;
    symbol.declaration = declaration;
    symbol.storage = storage;
    symbol.function = 0;
    symbol.function_declaration = 0;
    add_symbol(name, symbol);
  }

  void add_symbol(const std::string &name, const Symbol &symbol) {
    if (symbols_.find(name) != symbols_.end()) {
      throw LowirError("duplicate top-level symbol");
    }
    symbols_[name] = symbol;
  }

  void validate_aliases() const {
    std::set<std::string> aliases;
    for (std::size_t i = 0; i < program_.object_aliases.size(); ++i) {
      const ObjectAlias &alias = program_.object_aliases[i];
      if (!aliases.insert(alias.object_symbol).second) {
        throw LowirError("duplicate object alias");
      }
      if (symbols_.find(alias.target) == symbols_.end()) {
        throw LowirError("undefined object alias target");
      }
    }
  }

  void validate_roles_and_tls() const {
    std::set<int> singleton_roles;
    for (std::map<std::string, Symbol>::const_iterator it = symbols_.begin(); it != symbols_.end(); ++it) {
      const std::string &name = it->first;
      const Symbol &symbol = it->second;
      const SymbolMetadata *metadata = 0;
      if (symbol.kind == SYMBOL_GLOBAL) {
        for (std::size_t i = 0; i < program_.global_declarations.size(); ++i)
          if (program_.global_declarations[i].name == name) metadata = &program_.global_declarations[i].metadata;
        for (std::size_t i = 0; i < program_.globals.size(); ++i)
          if (program_.globals[i].name == name) metadata = &program_.globals[i].metadata;
        if (metadata != 0) {
          if (metadata->role != lowir_model::SR_NONE && metadata->role != lowir_model::SR_EH_TOP && metadata->role != lowir_model::SR_EH_VALUE &&
              metadata->role != lowir_model::SR_EH_TYPE) {
            throw LowirError("function role attached to global");
          }
          if (metadata->role != lowir_model::SR_NONE && !singleton_roles.insert(static_cast<int>(metadata->role)).second) {
            throw LowirError("duplicate singleton role");
          }
        }
      } else {
        const SymbolMetadata *function_metadata = 0;
        if (symbol.function != 0)
          function_metadata = &symbol.function->metadata;
        else if (symbol.function_declaration != 0)
          function_metadata = &symbol.function_declaration->metadata;
        if (function_metadata != 0) {
          if (function_metadata->role == lowir_model::SR_EH_TOP || function_metadata->role == lowir_model::SR_EH_VALUE ||
              function_metadata->role == lowir_model::SR_EH_TYPE) {
            throw LowirError("global role attached to function");
          }
          if (function_metadata->role != lowir_model::SR_NONE && !singleton_roles.insert(static_cast<int>(function_metadata->role)).second) {
            throw LowirError("duplicate singleton role");
          }
          if (!function_metadata->tls_for_symbol.empty()) {
            std::map<std::string, Symbol>::const_iterator target = symbols_.find(function_metadata->tls_for_symbol);
            if (target == symbols_.end() || target->second.kind != SYMBOL_GLOBAL || target->second.storage != lowir_model::GSM_THREAD_LOCAL) {
              throw LowirError("tls_for target is not thread local");
            }
            if (!tls_wrappers_.insert(function_metadata->tls_for_symbol).second) {
              throw LowirError("duplicate tls wrapper");
            }
          }
        }
      }
    }
  }

  void validate_global_declaration(const GlobalDeclaration &global) const {
    if (global.has_type && !is_valid_type(global.type.text)) {
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
          if (!is_scalar_type(item.type.text) || item.type.text == "void") throw LowirError("invalid structured global item type");
          if (item.literal_operand.kind != Operand::OP_INTEGER && item.literal_operand.kind != Operand::OP_FLOAT)
            throw LowirError("invalid structured global literal");
        } else if (symbols_.find(item.symbol) == symbols_.end()) {
          throw LowirError("undefined structured global address");
        }
      }
    } else if (global.init_kind == GlobalDefinition::INIT_ADDR) {
      if (global.type.text != "ptr") throw LowirError("address global must have ptr type");
      if (symbols_.find(global.init_operand.text) == symbols_.end()) throw LowirError("undefined global address initializer");
    } else {
      if (!is_scalar_type(global.type.text) || global.type.text == "void") throw LowirError("invalid scalar global type");
      if (global.init_operand.kind != Operand::OP_INTEGER && global.init_operand.kind != Operand::OP_FLOAT)
        throw LowirError("invalid scalar global initializer");
    }
  }

  void validate_function_header(const std::vector<Parameter> &parameters, const std::string &return_type, const Function *function) const {
    if (!is_valid_type(return_type)) {
      // void is a valid return type; the function argument is only used to
      // keep this check readable for declaration callers.
      if (!is_valid_type(return_type)) throw LowirError("invalid function return type");
    }
    std::set<std::string> names;
    bool saw_indirect_result = false;
    for (std::size_t i = 0; i < parameters.size(); ++i) {
      const Parameter &parameter = parameters[i];
      if (!names.insert(parameter.name).second) throw LowirError("duplicate parameter");
      if (!is_valid_type(parameter.type.text) || parameter.type.text == "void") throw LowirError("invalid parameter type");
      const ParameterMetadata &metadata = parameter.metadata;
      const bool pointer_metadata = metadata.passing != lowir_model::PPM_DIRECT || metadata.capture != lowir_model::PCM_DEFAULT ||
                                    metadata.access != lowir_model::PAM_DEFAULT || metadata.alias != lowir_model::PALM_DEFAULT;
      if (pointer_metadata && parameter.type.text != "ptr") throw LowirError("pointer parameter metadata on non-pointer");
      if (metadata.passing == lowir_model::PPM_INDIRECT_RESULT) {
        if (i != 0) throw LowirError("indirect result is not first parameter");
        saw_indirect_result = true;
      }
    }
    if (saw_indirect_result && return_type != "void") throw LowirError("indirect result on non-void function");
  }

  Value operand_value(const Operand &operand, const std::map<std::string, std::string> &values, const std::map<std::string, std::string> &slots) const {
    if (operand.kind == Operand::OP_TEMP) {
      std::map<std::string, std::string>::const_iterator it = values.find(operand.text);
      if (it == values.end()) throw LowirError("use of undefined temporary");
      return Value(it->second);
    }
    if (operand.kind == Operand::OP_SLOT) {
      std::map<std::string, std::string>::const_iterator it = slots.find(operand.text);
      if (it == slots.end()) throw LowirError("use of undefined slot");
      return Value(it->second);
    }
    if (operand.kind == Operand::OP_GLOBAL) {
      std::map<std::string, Symbol>::const_iterator it = symbols_.find(operand.text);
      if (it == symbols_.end()) throw LowirError("use of undefined global or function");
      return Value(it->second.kind == SYMBOL_FUNCTION ? "ptr" : it->second.type);
    }
    if (operand.kind == Operand::OP_INTEGER) return Value("i64");
    if (operand.kind == Operand::OP_FLOAT) return Value("f64");
    return Value();
  }

  void require_value(const Operand &operand, const std::map<std::string, std::string> &values, const std::map<std::string, std::string> &slots) const {
    (void)operand_value(operand, values, slots);
  }

  bool types_compatible(const std::string &expected, const Value &actual) const {
    if (!actual.known || expected.empty() || actual.type.empty()) return true;
    if (expected == actual.type) return true;
    if ((is_integer_type(expected) && is_integer_type(actual.type)) || (is_float_type(expected) && is_float_type(actual.type))) return true;
    if (expected == "ptr" && starts_with(actual.type, "obj<")) return true;
    return false;
  }

  void validate_function(const Function &function) const {
    validate_function_header(function.params, function.return_type.text, &function);
    if (function.blocks.empty()) throw LowirError("function has no blocks");
    std::map<std::string, std::string> slots;
    for (std::size_t i = 0; i < function.slots.size(); ++i) {
      if (!slots.insert(std::make_pair(function.slots[i].first, function.slots[i].second.text)).second) throw LowirError("duplicate slot");
      if (!is_valid_type(function.slots[i].second.text) || function.slots[i].second.text == "void") throw LowirError("invalid slot type");
    }
    std::set<std::string> blocks;
    for (std::size_t i = 0; i < function.blocks.size(); ++i) {
      if (!blocks.insert(function.blocks[i].label).second) throw LowirError("duplicate block");
    }
    std::map<std::string, std::string> values;
    for (std::size_t i = 0; i < function.params.size(); ++i) values[function.params[i].name] = function.params[i].type.text;
    for (std::size_t b = 0; b < function.blocks.size(); ++b) {
      const Block &block = function.blocks[b];
      if (block.instructions.empty()) throw LowirError("block has no terminator");
      bool terminated = false;
      for (std::size_t i = 0; i < block.instructions.size(); ++i) {
        const Instruction &instruction = block.instructions[i];
        if (terminated) throw LowirError("instruction after terminator");
        validate_instruction(function, instruction, blocks, values, slots);
        if (is_terminator(instruction.kind)) terminated = true;
      }
      if (!terminated) throw LowirError("block has no terminator");
    }
  }

  void require_block(const Operand &operand, const std::set<std::string> &blocks) const {
    if (operand.kind != Operand::OP_LABEL || blocks.find(operand.text) == blocks.end()) throw LowirError("undefined block target");
  }

  const Symbol *find_function_symbol(const std::string &name) const {
    std::map<std::string, Symbol>::const_iterator it = symbols_.find(name);
    if (it == symbols_.end() || it->second.kind != SYMBOL_FUNCTION) return 0;
    return &it->second;
  }

  void validate_call(const Function &function, const Instruction &instruction, const std::map<std::string, std::string> &values,
                     const std::map<std::string, std::string> &slots) const {
    const Value callee = operand_value(instruction.first, values, slots);
    (void)callee;
    const Function *target = 0;
    const FunctionDeclaration *target_declaration = 0;
    if (instruction.first.kind == Operand::OP_GLOBAL) {
      const Symbol *symbol = find_function_symbol(instruction.first.text);
      if (symbol != 0) {
        target = symbol->function;
        target_declaration = symbol->function_declaration;
      }
    }
    const bool direct = target != 0 || target_declaration != 0;
    if (!direct && !instruction.has_call_signature) throw LowirError("indirect call is missing signature");
    const std::vector<Parameter> *parameters = 0;
    FunctionBoundaryMetadata boundary;
    std::string return_type;
    if (direct) {
      if (target != 0) {
        parameters = &target->params;
        boundary = target->boundary;
        return_type = target->return_type.text;
      } else {
        parameters = &target_declaration->params;
        boundary = target_declaration->boundary;
        return_type = target_declaration->return_type.text;
      }
      if (instruction.has_call_signature) {
        // A signature on a direct call is not needed, but if present it is
        // still a boundary declaration and must agree with the result type.
        if (instruction.call_return_type.text != return_type) throw LowirError("direct call signature return mismatch");
      }
    } else {
      parameters = &instruction.call_params;
      boundary = instruction.call_boundary;
      return_type = instruction.call_return_type.text;
    }
    const std::size_t required = parameters->size();
    if (boundary.arity == lowir_model::CAM_FIXED && instruction.args.size() != required) throw LowirError("fixed call arity mismatch");
    if (boundary.arity != lowir_model::CAM_FIXED && instruction.args.size() < required) throw LowirError("call has too few arguments");
    for (std::size_t i = 0; i < required && i < instruction.args.size(); ++i) {
      const Value actual = operand_value(instruction.args[i], values, slots);
      const bool addressable_slot = (*parameters)[i].type.text == "ptr" && instruction.args[i].kind == Operand::OP_SLOT &&
                                    slots.find(instruction.args[i].text) != slots.end() && slots.find(instruction.args[i].text)->second != "ptr";
      if (!addressable_slot && !types_compatible((*parameters)[i].type.text, actual)) throw LowirError("call argument type mismatch");
    }
    if (instruction.call_returns_void) {
      if (return_type != "void") throw LowirError("void call has non-void target");
    } else if (instruction.dest.empty()) {
      // Calls returning a value always have a destination in the grammar.
      throw LowirError("missing call destination");
    }
    (void)function;
  }

  void define_value(const Instruction &instruction, const std::string &type, std::map<std::string, std::string> *values) const {
    if (instruction.dest.empty()) return;
    if (!values->insert(std::make_pair(instruction.dest, type)).second) throw LowirError("duplicate temporary definition");
  }

  void validate_instruction(const Function &function, const Instruction &instruction, const std::set<std::string> &blocks,
                            std::map<std::string, std::string> &values, const std::map<std::string, std::string> &slots) const {
    const auto require = [&](const Operand &operand) { require_value(operand, values, slots); };
    switch (instruction.kind) {
    case Instruction::IK_CONST:
      if (instruction.type.text == "void") throw LowirError("void constant");
      if (instruction.first.kind != Operand::OP_INTEGER && instruction.first.kind != Operand::OP_FLOAT) throw LowirError("constant is not scalar");
      define_value(instruction, instruction.type.text, &values);
      break;
    case Instruction::IK_COPY:
      require(instruction.first);
      define_value(instruction, instruction.type.text, &values);
      break;
    case Instruction::IK_ADDR:
      if (instruction.first.kind != Operand::OP_SLOT && instruction.first.kind != Operand::OP_GLOBAL) throw LowirError("addr requires addressable operand");
      if (instruction.first.kind == Operand::OP_GLOBAL && symbols_.find(instruction.first.text) == symbols_.end()) throw LowirError("addr of undefined symbol");
      define_value(instruction, "ptr", &values);
      break;
    case Instruction::IK_LOAD:
    case Instruction::IK_ATOMIC_LOAD:
      require(instruction.first);
      if (instruction.first.kind == Operand::OP_TEMP && values[instruction.first.text] != "ptr" && !starts_with(values[instruction.first.text], "obj<"))
        throw LowirError("load source is not pointer");
      if (instruction.kind == Instruction::IK_ATOMIC_LOAD && instruction.byte_alignment > 5) throw LowirError("invalid atomic order");
      define_value(instruction, instruction.type.text, &values);
      break;
    case Instruction::IK_STORE:
    case Instruction::IK_ATOMIC_STORE:
      require(instruction.first);
      require(instruction.second);
      if (instruction.second.kind == Operand::OP_TEMP && values[instruction.second.text] != "ptr") throw LowirError("store destination is not pointer");
      if (instruction.kind == Instruction::IK_ATOMIC_STORE && instruction.byte_alignment > 5) throw LowirError("invalid atomic order");
      break;
    case Instruction::IK_INDEX:
      require(instruction.first);
      require(instruction.second);
      if (instruction.first.kind == Operand::OP_TEMP && values[instruction.first.text] != "ptr" && !starts_with(values[instruction.first.text], "obj<"))
        throw LowirError("index base is not addressable");
      define_value(instruction, "ptr", &values);
      break;
    case Instruction::IK_UNARY:
      require(instruction.first);
      if (instruction.op == "decay" && instruction.type.text != "ptr") throw LowirError("decay requires ptr type");
      if (instruction.op == "bswap" && (instruction.type.text != "i16" && instruction.type.text != "i32" && instruction.type.text != "i64"))
        throw LowirError("invalid bswap type");
      if (instruction.op != "neg" && instruction.op != "not" && instruction.op != "bitnot" && instruction.op != "decay" && instruction.op != "bswap")
        throw LowirError("unknown unary operator");
      define_value(instruction, instruction.type.text, &values);
      break;
    case Instruction::IK_BINARY:
      require(instruction.first);
      require(instruction.second);
      if (is_float_type(instruction.type.text)) {
        if (instruction.op != "add" && instruction.op != "sub" && instruction.op != "mul" && instruction.op != "div")
          throw LowirError("invalid floating binary operator");
      } else if (instruction.op != "add" && instruction.op != "sub" && instruction.op != "mul" && instruction.op != "div" && instruction.op != "mod" &&
                 instruction.op != "udiv" && instruction.op != "umod" && instruction.op != "and" && instruction.op != "or" && instruction.op != "xor" &&
                 instruction.op != "shl" && instruction.op != "shr" && instruction.op != "ushr")
        throw LowirError("unknown binary operator");
      define_value(instruction, instruction.type.text, &values);
      break;
    case Instruction::IK_CMP:
      require(instruction.first);
      require(instruction.second);
      define_value(instruction, "i64", &values);
      break;
    case Instruction::IK_CONVERT: {
      require(instruction.first);
      const int dst_width = integer_width(instruction.type.text);
      const int src_width = integer_width(instruction.source_type.text);
      if ((instruction.op == "sext" || instruction.op == "zext") && (dst_width == 0 || src_width == 0 || dst_width <= src_width))
        throw LowirError("invalid integer widening conversion");
      if (instruction.op == "trunc" && (dst_width == 0 || src_width == 0 || dst_width >= src_width)) throw LowirError("invalid integer truncation conversion");
      if ((instruction.op == "sitofp" || instruction.op == "uitofp") &&
          (!is_integer_type(instruction.source_type.text) || !is_float_type(instruction.type.text)))
        throw LowirError("invalid integer to float conversion");
      if ((instruction.op == "fptosi" || instruction.op == "fptoui") &&
          (!is_float_type(instruction.source_type.text) || !is_integer_type(instruction.type.text)))
        throw LowirError("invalid float to integer conversion");
      if ((instruction.op == "fpext" || instruction.op == "fptrunc") && (!is_float_type(instruction.source_type.text) || !is_float_type(instruction.type.text)))
        throw LowirError("invalid floating conversion");
      define_value(instruction, instruction.type.text, &values);
      break;
    }
    case Instruction::IK_ATOMIC_ADD_FETCH:
      require(instruction.first);
      require(instruction.second);
      if (instruction.byte_alignment > 5) throw LowirError("invalid atomic order");
      define_value(instruction, instruction.type.text, &values);
      break;
    case Instruction::IK_ATOMIC_EXCHANGE:
      require(instruction.first);
      require(instruction.second);
      if (instruction.byte_alignment > 5) throw LowirError("invalid atomic order");
      define_value(instruction, instruction.type.text, &values);
      break;
    case Instruction::IK_ATOMIC_COMPARE_EXCHANGE:
      require(instruction.first);
      require(instruction.second);
      require(instruction.third);
      if (instruction.byte_alignment > 5 || instruction.eh_selector > 5) throw LowirError("invalid atomic order");
      define_value(instruction, "i64", &values);
      break;
    case Instruction::IK_ATOMIC_THREAD_FENCE:
    case Instruction::IK_ATOMIC_SIGNAL_FENCE:
      if (instruction.byte_alignment > 5) throw LowirError("invalid atomic order");
      break;
    case Instruction::IK_CALL:
      validate_call(function, instruction, values, slots);
      if (!instruction.call_returns_void) define_value(instruction, instruction.call_return_type.text, &values);
      break;
    case Instruction::IK_COPYOBJ:
      if (instruction.byte_count == 0 || instruction.byte_alignment == 0 || (instruction.byte_alignment & (instruction.byte_alignment - 1)) != 0)
        throw LowirError("invalid storage operation alignment");
      require(instruction.first);
      require(instruction.second);
      if (instruction.second.kind == Operand::OP_TEMP && values[instruction.second.text] != "ptr") throw LowirError("copy destination is not pointer");
      break;
    case Instruction::IK_ZEROINIT:
      if (instruction.byte_count == 0 || instruction.byte_alignment == 0 || (instruction.byte_alignment & (instruction.byte_alignment - 1)) != 0)
        throw LowirError("invalid storage operation alignment");
      require(instruction.first);
      if (instruction.first.kind == Operand::OP_TEMP && values[instruction.first.text] != "ptr") throw LowirError("zero destination is not pointer");
      break;
    case Instruction::IK_EH_TRY:
    case Instruction::IK_EH_CLEANUP:
      require_block(instruction.first, blocks);
      break;
    case Instruction::IK_EH_CATCH:
      if (instruction.first.kind != Operand::OP_GLOBAL || symbols_.find(instruction.first.text) == symbols_.end())
        throw LowirError("invalid exception catch symbol");
      break;
    case Instruction::IK_EH_FILTER:
      for (std::size_t i = 0; i < instruction.args.size(); ++i)
        if (instruction.args[i].kind != Operand::OP_GLOBAL || symbols_.find(instruction.args[i].text) == symbols_.end())
          throw LowirError("invalid exception filter symbol");
      break;
    case Instruction::IK_EH_CATCH_ALL:
    case Instruction::IK_EH_END:
      break;
    case Instruction::IK_THROW:
      require(instruction.first);
      break;
    case Instruction::IK_EXCEPTION:
      if (instruction.type.text == "void") throw LowirError("void exception value");
      define_value(instruction, instruction.type.text, &values);
      break;
    case Instruction::IK_EXCEPTION_SELECTOR:
      define_value(instruction, instruction.type.text, &values);
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
      if (instruction.type.text != function.return_type.text) throw LowirError("return type mismatch");
      if (instruction.type.text != "void") require(instruction.first);
      break;
    default:
      throw LowirError("unsupported instruction");
    }
  }

  const Program &program_;
  std::map<std::string, Symbol> symbols_;
  mutable std::set<std::string> tls_wrappers_;
};

struct Location {
  std::string type;
  std::size_t offset;
  bool valid;
  Location() : offset(0), valid(false) {}
  Location(const std::string &t, std::size_t o) : type(t), offset(o), valid(true) {}
};

class FunctionLayout {
public:
  explicit FunctionLayout(const Function &function) : function_(function), frame_size_(0), f80_scratch_offset_(0), has_f80_(false) { build(); }

  const Location &find(const std::string &name) const {
    std::map<std::string, Location>::const_iterator it = locations_.find(name);
    if (it == locations_.end()) throw LowirError("missing emitted value location");
    return it->second;
  }

  bool has(const std::string &name) const { return locations_.find(name) != locations_.end(); }

  std::size_t frame_size() const { return frame_size_; }

  bool has_f80() const { return has_f80_; }

  std::size_t f80_scratch_offset() const {
    if (!has_f80_) throw LowirError("f80 scratch requested without f80 value");
    return f80_scratch_offset_;
  }

private:
  void allocate(const std::string &name, const std::string &type) {
    if (locations_.find(name) != locations_.end()) {
      throw LowirError("duplicate emitted location");
    }
    const std::size_t size = type == "f80" ? 16 : (starts_with(type, "obj<") ? type_storage_size(type) : 8);
    const std::size_t alignment = type == "f80" ? 8 : (starts_with(type, "obj<") ? type_storage_alignment(type) : 8);
    if (alignment > 1) {
      const std::size_t remainder = frame_size_ % alignment;
      if (remainder != 0) frame_size_ += alignment - remainder;
    }
    frame_size_ += size;
    locations_[name] = Location(type, frame_size_);
  }

  static std::string definition_type(const Instruction &instruction) {
    switch (instruction.kind) {
    case Instruction::IK_CMP:
    case Instruction::IK_ATOMIC_COMPARE_EXCHANGE:
      return "i64";
    case Instruction::IK_ADDR:
    case Instruction::IK_INDEX:
      return "ptr";
    case Instruction::IK_CALL:
      return instruction.call_returns_void ? std::string() : instruction.call_return_type.text;
    default:
      return instruction.type.text;
    }
  }

  void build() {
    const bool hidden_return = starts_with(function_.return_type.text, "obj<") || function_.return_type.text == "f80";
    if (hidden_return) frame_size_ += 8;
    for (std::size_t i = 0; i < function_.params.size(); ++i) allocate(function_.params[i].name, function_.params[i].type.text);
    for (std::size_t i = 0; i < function_.slots.size(); ++i) allocate(function_.slots[i].first, function_.slots[i].second.text);
    for (std::size_t b = 0; b < function_.blocks.size(); ++b) {
      for (std::size_t i = 0; i < function_.blocks[b].instructions.size(); ++i) {
        const Instruction &instruction = function_.blocks[b].instructions[i];
        if (!instruction.dest.empty()) {
          const std::string type = definition_type(instruction);
          if (!type.empty()) allocate(instruction.dest, type);
        }
      }
    }
    bool needs_scratch = false;
    for (std::size_t i = 0; i < function_.params.size(); ++i)
      if (function_.params[i].type.text == "f80") has_f80_ = true;
    if (function_.return_type.text == "f80") has_f80_ = true;
    for (std::size_t i = 0; i < function_.slots.size(); ++i)
      if (function_.slots[i].second.text == "f80") has_f80_ = true;
    for (std::size_t b = 0; b < function_.blocks.size(); ++b) {
      for (std::size_t i = 0; i < function_.blocks[b].instructions.size(); ++i) {
        const Instruction &instruction = function_.blocks[b].instructions[i];
        if (instruction.kind == Instruction::IK_CONVERT) needs_scratch = true;
        if (instruction.type.text == "f80" || instruction.source_type.text == "f80" || instruction.call_return_type.text == "f80") has_f80_ = true;
      }
    }
    if (has_f80_ || needs_scratch) {
      if (has_f80_) f80_scratch_offset_ = frame_size_ + 16;
      frame_size_ += 64;
    }
  }

  const Function &function_;
  std::size_t frame_size_;
  std::size_t f80_scratch_offset_;
  bool has_f80_;
  std::map<std::string, Location> locations_;
};

class Emitter {
public:
  explicit Emitter(const Program &program) : program_(program), validator_(program), out_() {}

  std::string emit() {
    const Function *entry = validator_.entry_function();
    if (entry == 0) throw LowirError("missing entry function");
    collect_output_symbols();
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

  static std::string width_name(const std::string &type) {
    if (type == "f32") return "32";
    if (type == "f64") return "64";
    if (type == "f80") return "80";
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

  struct OutputSymbol {
    enum Kind { OS_GLOBAL, OS_FUNCTION, OS_FUNCTION_DECLARATION } kind;
    const Function *function;
    const FunctionDeclaration *declaration;

    OutputSymbol() : kind(OS_GLOBAL), function(0), declaration(0) {}
  };

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

  void collect_output_symbols() {
    symbols_.clear();
    output_symbols_.clear();
    functions_.clear();
    function_declarations_.clear();
    has_eh_ = false;
    for (std::size_t i = 0; i < program_.globals.size(); ++i) add_output_global(program_.globals[i].name, cy_global(program_.globals[i].name));
    for (std::size_t i = 0; i < program_.global_declarations.size(); ++i)
      add_output_global(program_.global_declarations[i].name, cy_global(program_.global_declarations[i].name));
    for (std::size_t i = 0; i < program_.function_declarations.size(); ++i) {
      const FunctionDeclaration *declaration = &program_.function_declarations[i];
      symbols_[declaration->name] = cy_function(declaration->name);
      OutputSymbol symbol;
      symbol.kind = OutputSymbol::OS_FUNCTION_DECLARATION;
      symbol.declaration = declaration;
      output_symbols_[declaration->name] = symbol;
      function_declarations_[declaration->name] = declaration;
    }
    for (std::size_t i = 0; i < program_.functions.size(); ++i) {
      const Function *function = &program_.functions[i];
      symbols_[function->name] = cy_function(function->name);
      OutputSymbol symbol;
      symbol.kind = OutputSymbol::OS_FUNCTION;
      symbol.function = function;
      output_symbols_[function->name] = symbol;
      functions_[function->name] = function;
      for (std::size_t b = 0; b < function->blocks.size(); ++b) {
        for (std::size_t j = 0; j < function->blocks[b].instructions.size(); ++j) {
          const Instruction::Kind kind = function->blocks[b].instructions[j].kind;
          if (kind == Instruction::IK_EH_TRY || kind == Instruction::IK_EH_CLEANUP || kind == Instruction::IK_EH_END || kind == Instruction::IK_THROW ||
              kind == Instruction::IK_EXCEPTION || kind == Instruction::IK_RESUME) {
            has_eh_ = true;
          }
        }
      }
    }
  }

  void add_output_global(const std::string &name, const std::string &label) {
    symbols_[name] = label;
    OutputSymbol symbol;
    symbol.kind = OutputSymbol::OS_GLOBAL;
    output_symbols_[name] = symbol;
  }

  const OutputSymbol *output_symbol(const std::string &name) const {
    std::map<std::string, OutputSymbol>::const_iterator it = output_symbols_.find(name);
    return it == output_symbols_.end() ? 0 : &it->second;
  }

  bool is_function_symbol(const std::string &name) const {
    const OutputSymbol *symbol = output_symbol(name);
    return symbol != 0 && symbol->kind != OutputSymbol::OS_GLOBAL;
  }

  void emit_start(const Function *entry) {
    line("start:");
    line("\tmove64 bp sp;");
    const Function *init = validator_.init_function();
    const Function *fini = validator_.fini_function();
    if (init != 0) {
      line("\tcall " + cy_function(init->name) + ";");
    }
    line("\tcall " + cy_function(entry->name) + ";");
    if (fini != 0) {
      line("\tisub64 sp sp 8;");
      line("\tmove64 [sp] x64;");
      line("\tcall " + cy_function(fini->name) + ";");
      line("\tmove64 x64 [sp];");
      line("\tiadd64 sp sp 8;");
    }
    line("\tsyscall1 t64 60 x64;");
    blank();
  }

  std::string stack_operand(const Location &location) const { return address_at(location.offset); }

  void emit_function(const Function &function) {
    FunctionLayout layout(function);
    current_function_ = &function;
    current_layout_ = &layout;
    line(cy_function(function.name) + ":");
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
    line(cy_function(function.name) + "__epilogue:");
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
    const std::size_t hidden = starts_with(function.return_type.text, "obj<") || function.return_type.text == "f80" ? 1 : 0;
    if (hidden) line("\tmove64 [bp-8] x64;");
    for (std::size_t i = 0; i < function.params.size(); ++i) {
      const Parameter &parameter = function.params[i];
      const Location &location = current_layout_->find(parameter.name);
      const std::size_t register_index = i + hidden;
      const std::string reg = parameter_register(register_index);
      if (parameter.type.text == "f80") {
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
      if (starts_with(parameter.type.text, "obj<")) {
        if (register_index < 4)
          line("\tmove64 x64 " + reg + "64;");
        else
          line("\tmove64 x64 [bp+" + immediate_signed(16 + (register_index - 4) * 8) + "];");
        const std::size_t bytes = type_storage_size(parameter.type.text);
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
        emit_store_register_to_location(location, reg + width_name(parameter.type.text));
      } else {
        emit_load_memory_to_register("[bp+" + immediate_signed(16 + (register_index - 4) * 8) + "]", "x", parameter.type.text);
        emit_store_register_to_location(location, "x" + width_name(parameter.type.text));
      }
    }
  }

  void emit_store_register_to_location(const Location &location, const std::string &source_register) {
    const std::string width = width_name(location.type);
    const std::string source = source_register == "x" + width ? source_register : source_register;
    if (location.type == "f32")
      line("\tmove32 " + stack_operand(location) + " " + source + ";");
    else if (location.type == "f64" || location.type == "ptr" || is_integer_type(location.type))
      line("\tmove" + width + " " + stack_operand(location) + " " + source + ";");
    else if (location.type == "f80") {
      line("\tmove64 " + stack_operand(location) + " " + source + ";");
    }
  }

  void emit_load_memory_to_register(const std::string &memory, const std::string &register_name, const std::string &type) {
    if (type == "f32")
      line("\tmove32 " + register_name + "32 " + memory + ";");
    else if (type == "f64" || type == "ptr" || is_integer_type(type))
      line("\tmove" + width_name(type) + " " + register_name + width_name(type) + " " + memory + ";");
  }

  void emit_block(const Function &function, const Block &block) {
    line(cy_block(function.name, block.label) + ":");
    for (std::size_t i = 0; i < block.instructions.size(); ++i) emit_instruction(function, block.instructions[i]);
  }

  void emit_zero_extend_register(const std::string &base, const std::string &type) {
    const int width = integer_width(type);
    if (width != 0 && width < 32) line("\tmove64 " + reg64(base) + " 0;");
  }

  void emit_load_value(const Operand &operand, const std::string &type, const std::string &base_register) {
    if (type == "f80") throw LowirError("f80 value requires memory lowering");
    const std::string full_register = reg64(base_register);
    const std::string register_name = is_float_type(type) ? base_register + width_name(type) : base_register + width_name(type);
    if (operand.kind == Operand::OP_INTEGER) {
      const std::string move_width = is_integer_type(type) ? "64" : width_name(type);
      line("\tmove" + move_width + " " + (is_integer_type(type) ? full_register : register_name) + " " + immediate_signed(operand.int_value) + ";");
      return;
    }
    if (operand.kind == Operand::OP_FLOAT) {
      std::string literal = operand.text;
      if (type == "f32" && literal[literal.size() - 1] != 'f' && literal[literal.size() - 1] != 'F') literal += "f";
      line("\tmove" + width_name(type) + " " + register_name + " " + literal + ";");
      return;
    }
    if (operand.text == "nullptr") {
      line("\tmove64 " + full_register + " 0;");
      return;
    }
    if (operand.kind == Operand::OP_GLOBAL) {
      std::map<std::string, std::string>::const_iterator it = symbols_.find(operand.text);
      if (it == symbols_.end()) throw LowirError("unknown output symbol");
      if (is_function_symbol(operand.text))
        line("\tmove64 " + full_register + " " + it->second + ";");
      else if (type == "f32")
        line("\tmove32 " + register_name + " [" + it->second + "];");
      else
        line("\tmove" + width_name(type) + " " + register_name + " [" + it->second + "];");
      return;
    }
    const Location &location = current_layout_->find(operand.text);
    emit_zero_extend_register(base_register, type);
    if (type == "f32")
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
    const std::string literal = operand.text.empty() ? "0.0L" : operand.text;
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
      line("\tmove64 x64 " + cy_global(operand.text) + ";");
      source_low = "[x64]";
      source_high = "[x64+8]";
    } else if (operand.kind == Operand::OP_TEMP || operand.kind == Operand::OP_SLOT) {
      const Location &location = current_layout_->find(operand.text);
      emit_address_of_location(location, "x64");
      source_low = "[x64]";
      source_high = "[x64+8]";
    } else {
      throw LowirError("invalid f80 operand");
    }
    emit_f80_copy(source_low, source_high, f80_scratch_low(scratch), f80_scratch_high(scratch));
  }

  void emit_f80_result_from_scratch(const std::string &destination, std::size_t scratch) {
    const Location &location = current_layout_->find(destination);
    emit_f80_copy(f80_scratch_low(scratch), f80_scratch_high(scratch), stack_operand(location), address_at(location.offset - 8));
  }

  void emit_f80_storage_address(const Operand &storage, const std::string &reg) {
    if (storage.kind == Operand::OP_GLOBAL) {
      line("\tmove64 " + reg + " " + cy_global(storage.text) + ";");
    } else if (storage.kind == Operand::OP_SLOT) {
      emit_address_of_location(current_layout_->find(storage.text), reg);
    } else {
      emit_load_pointer(storage, reg == "x64" ? "x" : "y");
    }
  }

  void emit_f80_load_instruction(const Instruction &instruction) {
    emit_f80_storage_address(instruction.first, "x64");
    emit_f80_copy("[x64]", "[x64+8]", f80_scratch_low(0), f80_scratch_high(0));
    emit_f80_result_from_scratch(instruction.dest, 0);
  }

  void emit_f80_store_instruction(const Instruction &instruction) {
    emit_f80_operand_to_scratch(instruction.first, 0);
    emit_f80_storage_address(instruction.second, "x64");
    emit_f80_copy(f80_scratch_low(0), f80_scratch_high(0), "[x64]", "[x64+8]");
  }

  void emit_load_pointer(const Operand &operand, const std::string &base_register) {
    if (operand.kind == Operand::OP_GLOBAL) {
      if (is_function_symbol(operand.text))
        line("\tmove64 " + reg64(base_register) + " " + cy_function(operand.text) + ";");
      else
        line("\tmove64 " + reg64(base_register) + " [" + cy_global(operand.text) + "];");
      return;
    }
    if (operand.kind == Operand::OP_INTEGER) {
      line("\tmove64 " + reg64(base_register) + " " + immediate_signed(operand.int_value) + ";");
      return;
    }
    if (operand.text == "nullptr") {
      line("\tmove64 " + reg64(base_register) + " 0;");
      return;
    }
    const Location &location = current_layout_->find(operand.text);
    if (operand.kind == Operand::OP_SLOT && location.type != "ptr" && !starts_with(location.type, "obj<")) {
      emit_address_of_location(location, reg64(base_register));
    } else {
      line("\tmove64 " + reg64(base_register) + " " + stack_operand(location) + ";");
    }
  }

  void emit_address_of_location(const Location &location, const std::string &reg) {
    line("\tisub64 " + reg + " bp " + immediate_signed(static_cast<long long>(location.offset)) + ";");
  }

  void emit_result(const std::string &destination, const std::string &type, const std::string &base_register) {
    const Location &location = current_layout_->find(destination);
    const std::string width = width_name(type);
    const std::string reg = base_register + width;
    if (type == "f32")
      line("\tmove32 " + stack_operand(location) + " " + reg + ";");
    else if (type == "f80")
      throw LowirError("f80 result requires memory lowering");
    else
      line("\tmove" + width + " " + stack_operand(location) + " " + reg + ";");
  }

  void emit_address_result(const std::string &destination, const std::string &source) { emit_result(destination, "ptr", source); }

  void emit_addr_instruction(const Instruction &instruction) {
    if (instruction.first.kind == Operand::OP_SLOT) {
      emit_address_of_location(current_layout_->find(instruction.first.text), "x64");
    } else if (instruction.first.kind == Operand::OP_GLOBAL) {
      line("\tmove64 x64 " + (is_function_symbol(instruction.first.text) ? cy_function(instruction.first.text) : cy_global(instruction.first.text)) + ";");
    } else {
      throw LowirError("invalid addr operand");
    }
    emit_address_result(instruction.dest, "x");
  }

  bool is_direct_storage(const Operand &operand) const { return operand.kind == Operand::OP_SLOT || operand.kind == Operand::OP_GLOBAL; }

  void emit_load_instruction(const Instruction &instruction) {
    const std::string type = instruction.type.text;
    if (type == "f80") {
      emit_f80_load_instruction(instruction);
      return;
    }
    const bool atomic = instruction.kind == Instruction::IK_ATOMIC_LOAD;
    if (is_direct_storage(instruction.first)) {
      if (instruction.first.kind == Operand::OP_SLOT) {
        emit_load_memory_to_register(stack_operand(current_layout_->find(instruction.first.text)), "x", type);
      } else {
        emit_load_memory_to_register("[" + cy_global(instruction.first.text) + "]", "x", type);
      }
    } else {
      emit_load_pointer(instruction.first, atomic ? "y" : "x");
      if (type == "f32")
        line("\tmove32 x32 [x64];");
      else if (atomic)
        line("\tmove" + width_name(type) + " x" + width_name(type) + " [y64];");
      else
        line("\tmove" + width_name(type) + " x" + width_name(type) + " [x64];");
    }
    if (!is_direct_storage(instruction.first) && type == "i32") {
      line("\tmove8 t8 32;");
      line("\tlshift64 x64 x64 t8;");
      line("\tsrshift64 x64 x64 t8;");
    }
    emit_result(instruction.dest, type, "x");
  }

  void emit_store_instruction(const Instruction &instruction) {
    const std::string type = instruction.type.text;
    if (type == "f80") {
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
      const std::string destination = instruction.second.kind == Operand::OP_SLOT ? stack_operand(current_layout_->find(instruction.second.text))
                                                                                  : "[" + cy_global(instruction.second.text) + "]";
      line("\tmove" + width_name(type) + " " + destination + " x" + width_name(type) + ";");
    } else {
      emit_load_pointer(instruction.second, "y");
      line("\tmove" + width_name(type) + " [y64] x" + width_name(type) + ";");
    }
  }

  void emit_index_instruction(const Instruction &instruction) {
    if (instruction.first.kind == Operand::OP_TEMP && current_layout_->find(instruction.first.text).type.find("obj<") == 0) {
      emit_load_value(instruction.first, "ptr", "y");
    } else {
      emit_load_pointer(instruction.first, "y");
    }
    emit_load_value(instruction.second, "i64", "x");
    const std::size_t scale = type_storage_size(instruction.type.text);
    if (scale != 1) {
      line("\tmove64 z64 " + immediate_signed(static_cast<long long>(scale)) + ";");
      line("\tsmul64 x64 x64 z64;");
    }
    line("\tiadd64 x64 y64 x64;");
    emit_address_result(instruction.dest, "x");
  }

  void emit_unary_instruction(const Instruction &instruction) {
    const std::string type = instruction.type.text;
    if (type == "f80") {
      if (instruction.op != "neg") throw LowirError("unsupported f80 unary operator");
      emit_f80_operand_to_scratch(instruction.first, 0);
      Operand zero;
      zero.kind = Operand::OP_FLOAT;
      zero.text = "0.0L";
      emit_f80_literal(zero, 1);
      line("\tfsub80 " + f80_scratch_low(2) + " " + f80_scratch_low(1) + " " + f80_scratch_low(0) + ";");
      emit_f80_padding(2);
      emit_f80_result_from_scratch(instruction.dest, 2);
      return;
    }
    if (instruction.op == "decay") {
      emit_load_value(instruction.first, type, "x");
      emit_result(instruction.dest, type, "x");
      return;
    }
    if (instruction.op == "not") {
      emit_load_value(instruction.first, type, "x");
      line("\tieq" + width_name(type) + " z8 x" + width_name(type) + " 0;");
      emit_boolean_result(instruction.dest);
      return;
    }
    emit_load_value(instruction.first, type, "x");
    if (instruction.op == "neg") {
      line("\tmove64 y64 0;");
      line("\tisub" + width_name(type) + " x" + width_name(type) + " y" + width_name(type) + " x" + width_name(type) + ";");
    } else if (instruction.op == "bitnot") {
      line("\tnot" + width_name(type) + " x" + width_name(type) + " x" + width_name(type) + ";");
    } else if (instruction.op == "bswap") {
      line("\tbswap" + width_name(type) + " x" + width_name(type) + " x" + width_name(type) + ";");
    } else {
      throw LowirError("unsupported unary operator");
    }
    emit_result(instruction.dest, type, "x");
  }

  std::string integer_binary_opcode(const std::string &op, const std::string &type) const {
    const std::string width = width_name(type);
    if (op == "add") return "iadd" + width;
    if (op == "sub") return "isub" + width;
    if (op == "mul") return (type.size() > 0 && type[0] == 'u' ? "umul" : "smul") + width;
    if (op == "div") return "sdiv" + width;
    if (op == "mod") return "smod" + width;
    if (op == "udiv") return "udiv" + width;
    if (op == "umod") return "umod" + width;
    if (op == "and") return "and" + width;
    if (op == "or") return "or" + width;
    if (op == "xor") return "xor" + width;
    if (op == "shl") return "lshift" + width;
    if (op == "shr") return "srshift" + width;
    if (op == "ushr") return "urshift" + width;
    throw LowirError("unsupported integer binary operator");
  }

  void emit_binary_instruction(const Instruction &instruction) {
    const std::string type = instruction.type.text;
    if (type == "f80") {
      emit_f80_operand_to_scratch(instruction.first, 0);
      emit_f80_operand_to_scratch(instruction.second, 1);
      if (instruction.op != "add" && instruction.op != "sub" && instruction.op != "mul" && instruction.op != "div")
        throw LowirError("unsupported f80 binary operator");
      line("\tf" + instruction.op + "80 " + f80_scratch_low(2) + " " + f80_scratch_low(0) + " " + f80_scratch_low(1) + ";");
      emit_f80_padding(2);
      emit_f80_result_from_scratch(instruction.dest, 2);
      return;
    }
    emit_load_value(instruction.first, type, "y");
    emit_load_value(instruction.second, type, "x");
    if (is_float_type(type)) {
      line("\tf" + instruction.op + width_name(type) + " x" + width_name(type) + " y" + width_name(type) + " x" + width_name(type) + ";");
    } else {
      const std::string width = width_name(type);
      if (instruction.op == "shl" || instruction.op == "shr" || instruction.op == "ushr") {
        line("\tmove64 z64 x64;");
        line("\tmove8 x8 z8;");
        line("\t" + integer_binary_opcode(instruction.op, type) + " x" + width + " y" + width + " x8;");
      } else {
        line("\t" + integer_binary_opcode(instruction.op, type) + " x" + width + " y" + width + " x" + width + ";");
      }
    }
    emit_result(instruction.dest, type, "x");
  }

  std::string compare_opcode(const std::string &predicate, const std::string &type) const {
    const std::string width = width_name(type);
    if (is_float_type(type)) return "f" + predicate + width;
    if (predicate == "eq" || predicate == "ne") return "i" + predicate + width;
    if (predicate == "lt" || predicate == "le" || predicate == "gt" || predicate == "ge") return "s" + predicate + width;
    if (predicate == "ult" || predicate == "ule" || predicate == "ugt" || predicate == "uge") return "u" + predicate.substr(1) + width;
    throw LowirError("unsupported comparison predicate");
  }

  void emit_boolean_result(const std::string &destination) {
    line("\tmove64 x64 0;");
    line("\tmove8 x8 z8;");
    emit_result(destination, "i64", "x");
  }

  void emit_cmp_instruction(const Instruction &instruction) {
    const std::string type = instruction.type.text;
    if (type == "f80") {
      emit_f80_operand_to_scratch(instruction.first, 0);
      emit_f80_operand_to_scratch(instruction.second, 1);
      line("\tf" + instruction.op + "80 z8 " + f80_scratch_low(0) + " " + f80_scratch_low(1) + ";");
      emit_boolean_result(instruction.dest);
      return;
    }
    emit_load_value(instruction.first, type, "y");
    emit_load_value(instruction.second, type, "x");
    line("\t" + compare_opcode(instruction.op, type) + " z8 y" + width_name(type) + " x" + width_name(type) + ";");
    emit_boolean_result(instruction.dest);
  }

  void emit_integer_conversion(const Instruction &instruction) {
    const std::string dst = instruction.type.text;
    const std::string src = instruction.source_type.text;
    emit_load_value(instruction.first, src, "x");
    const int dst_width = integer_width(dst);
    const int src_width = integer_width(src);
    if ((instruction.op == "sext" || instruction.op == "zext") && dst_width > src_width &&
        (instruction.op == "sext" || instruction.first.kind != Operand::OP_INTEGER)) {
      const int shift = dst_width - src_width;
      line("\tmove8 t8 " + immediate_signed(shift) + ";");
      line("\tlshift" + width_name(dst) + " x" + width_name(dst) + " x" + width_name(dst) + " t8;");
      line("\t" + std::string(instruction.op == "sext" ? "srshift" : "urshift") + width_name(dst) + " x" + width_name(dst) + " x" + width_name(dst) + " t8;");
    }
    emit_result(instruction.dest, dst, "x");
  }

  void emit_convert_instruction(const Instruction &instruction) {
    const std::string dst = instruction.type.text;
    const std::string src = instruction.source_type.text;
    if (is_integer_type(dst) && is_integer_type(src)) {
      emit_integer_conversion(instruction);
      return;
    }
    const bool dst_f80 = dst == "f80";
    const bool src_f80 = src == "f80";
    if (dst_f80) {
      emit_load_or_convert_to_f80(instruction.first, src, instruction.op, 0);
      emit_f80_result_from_scratch(instruction.dest, 0);
      return;
    }
    if (src_f80) {
      emit_f80_operand_to_scratch(instruction.first, 0);
      const Location &destination = current_layout_->find(instruction.dest);
      if (is_float_type(dst)) {
        line("\tf80convf" + width_name(dst) + " " + stack_operand(destination) + " " + f80_scratch_low(0) + ";");
      } else if (is_integer_type(dst)) {
        line("\tf80conv" + std::string(instruction.op == "fptoui" ? "u" : "s") + width_name(dst) + " " + stack_operand(destination) + " " + f80_scratch_low(0) +
             ";");
      } else
        throw LowirError("unsupported f80 conversion destination");
      return;
    }
    if (is_float_type(src) && is_float_type(dst)) {
      emit_load_or_convert_to_f80(instruction.first, src, instruction.op, 0);
      const Location &destination = current_layout_->find(instruction.dest);
      line("\tf80convf" + width_name(dst) + " " + stack_operand(destination) + " " + f80_scratch_low(0) + ";");
      return;
    }
    if (is_integer_type(src) && is_float_type(dst)) {
      emit_load_or_convert_to_f80(instruction.first, src, instruction.op, 0);
      const Location &destination = current_layout_->find(instruction.dest);
      line("\tf80convf" + width_name(dst) + " " + stack_operand(destination) + " " + f80_scratch_low(0) + ";");
      return;
    }
    if (is_float_type(src) && is_integer_type(dst)) {
      emit_load_or_convert_to_f80(instruction.first, src, instruction.op, 0);
      const Location &destination = current_layout_->find(instruction.dest);
      line("\tf80conv" + std::string(instruction.op == "fptoui" ? "u" : "s") + width_name(dst) + " " + stack_operand(destination) + " " + f80_scratch_low(0) +
           ";");
      return;
    }
    throw LowirError("unsupported floating conversion");
  }

  void emit_load_or_convert_to_f80(const Operand &operand, const std::string &source_type, const std::string &conversion, std::size_t scratch) {
    if (source_type == "f80") {
      emit_f80_operand_to_scratch(operand, scratch);
      return;
    }
    emit_load_value(operand, source_type, "x");
    if (is_float_type(source_type)) {
      line("\tf" + width_name(source_type) + "convf80 " + f80_scratch_low(scratch) + " x" + width_name(source_type) + ";");
    } else if (is_integer_type(source_type)) {
      const std::string prefix = conversion == "uitofp" ? "u" : "s";
      line("\t" + prefix + width_name(source_type) + "convf80 " + f80_scratch_low(scratch) + " x" + width_name(source_type) + ";");
    } else
      throw LowirError("unsupported conversion source");
    emit_f80_padding(scratch);
  }

  const Function *find_function(const std::string &name) const {
    std::map<std::string, const Function *>::const_iterator it = functions_.find(name);
    return it == functions_.end() ? 0 : it->second;
  }

  const FunctionDeclaration *find_function_declaration(const std::string &name) const {
    std::map<std::string, const FunctionDeclaration *>::const_iterator it = function_declarations_.find(name);
    return it == function_declarations_.end() ? 0 : it->second;
  }

  std::vector<Parameter> call_parameters(const Instruction &instruction, const Function **function_target, std::string *return_type) const {
    *function_target = 0;
    if (instruction.first.kind == Operand::OP_GLOBAL) {
      const Function *function = find_function(instruction.first.text);
      if (function != 0) {
        *function_target = function;
        *return_type = function->return_type.text;
        return function->params;
      }
      const FunctionDeclaration *declaration = find_function_declaration(instruction.first.text);
      if (declaration != 0) {
        *return_type = declaration->return_type.text;
        return declaration->params;
      }
    }
    *return_type = instruction.call_return_type.text;
    return instruction.call_params;
  }

  void emit_object_address(const Operand &operand, const std::string &reg) {
    if (operand.kind == Operand::OP_SLOT || operand.kind == Operand::OP_TEMP) {
      emit_address_of_location(current_layout_->find(operand.text), reg);
    } else if (operand.kind == Operand::OP_GLOBAL) {
      line("\tmove64 " + reg + " " + cy_global(operand.text) + ";");
    } else {
      throw LowirError("object argument is not addressable");
    }
  }

  void emit_call_argument(const Operand &argument, const Parameter *parameter, std::size_t index) {
    const std::string reg = parameter_register(index);
    const std::string type = parameter == 0 ? "i64" : parameter->type.text;
    if (parameter != 0 && type == "f80") {
      if (argument.kind == Operand::OP_TEMP || argument.kind == Operand::OP_SLOT) {
        emit_object_address(argument, "x64");
      } else {
        emit_f80_operand_to_scratch(argument, 3);
        emit_address_of_location(Location("f80", current_layout_->f80_scratch_offset() + 3 * 16), "x64");
      }
      line("\tmove64 " + reg + "64 x64;");
      return;
    }
    if (parameter != 0 && starts_with(type, "obj<")) {
      emit_object_address(argument, "x64");
      line("\tmove64 " + reg + "64 x64;");
      return;
    }
    if (parameter != 0 && type == "ptr" && argument.kind == Operand::OP_SLOT && current_layout_->find(argument.text).type != "ptr") {
      emit_object_address(argument, "x64");
      line("\tmove64 " + reg + "64 x64;");
      return;
    }
    emit_load_value(argument, type, reg);
  }

  void emit_indirect_callee(const Operand &callee) {
    emit_load_pointer(callee, "x");
    line("\tisub64 sp sp 8;");
    line("\tmove64 [sp] x64;");
  }

  void emit_call_instruction(const Instruction &instruction) {
    const Function *target = 0;
    std::string return_type;
    const std::vector<Parameter> parameters = call_parameters(instruction, &target, &return_type);
    const bool direct = target != 0 || find_function_declaration(instruction.first.text) != 0;
    if (!direct) emit_indirect_callee(instruction.first);
    const bool hidden_return = direct && !instruction.call_returns_void && (starts_with(return_type, "obj<") || return_type == "f80");
    if (hidden_return) {
      emit_object_address(OperandForDestination(instruction.dest), "x64");
      line("\tmove64 x64 x64;");
    }
    const std::size_t hidden = hidden_return ? 1 : 0;
    const std::size_t register_arguments = 4 - hidden;
    const std::size_t stack_arguments = instruction.args.size() > register_arguments ? instruction.args.size() - register_arguments : 0;
    if (stack_arguments != 0) {
      line("\tisub64 sp sp " + immediate_signed(static_cast<long long>(stack_arguments * 8)) + ";");
    }
    for (std::size_t i = 0; i < instruction.args.size(); ++i) {
      const Parameter *parameter = i < parameters.size() ? &parameters[i] : 0;
      const std::size_t register_index = i + hidden;
      emit_call_argument(instruction.args[i], parameter, register_index);
      if (register_index >= 4) {
        line("\tmove64 [sp] 0;");
        line("\tmove64 [sp] x64;");
      }
    }
    if (direct)
      line("\tcall " + cy_function(instruction.first.text) + ";");
    else {
      line("\tcall [sp];");
      line("\tiadd64 sp sp 8;");
    }
    if (direct && stack_arguments != 0) {
      line("\tiadd64 sp sp " + immediate_signed(static_cast<long long>(stack_arguments * 8)) + ";");
    }
    if (!instruction.call_returns_void && !starts_with(return_type, "obj<") && return_type != "f80") {
      emit_result(instruction.dest, return_type, "x");
    }
  }

  Operand OperandForDestination(const std::string &destination) const {
    Operand operand;
    operand.kind = Operand::OP_TEMP;
    operand.text = destination;
    return operand;
  }

  void emit_copy_bytes(const Operand &source, const Operand &destination, std::size_t bytes) {
    if (destination.kind == Operand::OP_TEMP && starts_with(current_layout_->find(destination.text).type, "obj<"))
      emit_object_address(destination, "x64");
    else
      emit_load_pointer(destination, "x");
    if (source.kind == Operand::OP_TEMP && starts_with(current_layout_->find(source.text).type, "obj<"))
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
    emit_load_value(instruction.second, instruction.type.text, "x");
    line("\tmove" + width_name(instruction.type.text) + " t" + width_name(instruction.type.text) + " [y64];");
    line("\tmove" + width_name(instruction.type.text) + " [y64] x" + width_name(instruction.type.text) + ";");
    line("\tmove64 x64 0;");
    if (width_name(instruction.type.text) == "64")
      line("\tmove64 x64 t64;");
    else
      line("\tmove" + width_name(instruction.type.text) + " x" + width_name(instruction.type.text) + " t" + width_name(instruction.type.text) + ";");
    emit_result(instruction.dest, instruction.type.text, "x");
  }

  void emit_atomic_add_fetch(const Instruction &instruction) {
    emit_load_pointer(instruction.first, "y");
    line("\tmove" + width_name(instruction.type.text) + " x" + width_name(instruction.type.text) + " [y64];");
    emit_load_value(instruction.second, instruction.type.text, "z");
    line("\tiadd" + width_name(instruction.type.text) + " x" + width_name(instruction.type.text) + " x" + width_name(instruction.type.text) + " z" +
         width_name(instruction.type.text) + ";");
    line("\tmove" + width_name(instruction.type.text) + " [y64] x" + width_name(instruction.type.text) + ";");
    emit_result(instruction.dest, instruction.type.text, "x");
  }

  void emit_atomic_compare_exchange(const Instruction &instruction) {
    const std::string success = "__atomic_cmpxchg_success__" + immediate_signed(atomic_label_counter_++);
    const std::string end = "__atomic_cmpxchg_end__" + immediate_signed(atomic_label_counter_++);
    emit_load_pointer(instruction.first, "y");
    emit_load_pointer(instruction.second, "z");
    line("\tmove" + width_name(instruction.type.text) + " t" + width_name(instruction.type.text) + " [y64];");
    line("\tmove" + width_name(instruction.type.text) + " x" + width_name(instruction.type.text) + " [z64];");
    line("\tieq" + width_name(instruction.type.text) + " x8 t" + width_name(instruction.type.text) + " x" + width_name(instruction.type.text) + ";");
    line("\tjumpif x8 " + success + ";");
    line("\tmove" + width_name(instruction.type.text) + " [z64] t" + width_name(instruction.type.text) + ";");
    line("\tmove64 x64 0;");
    emit_result(instruction.dest, "i64", "x");
    line("\tjump " + end + ";");
    line(success + ":");
    emit_load_value(instruction.third, instruction.type.text, "x");
    line("\tmove" + width_name(instruction.type.text) + " [y64] x" + width_name(instruction.type.text) + ";");
    line("\tmove64 x64 1;");
    emit_result(instruction.dest, "i64", "x");
    line(end + ":");
  }

  void emit_branch(const Function &function, const Instruction &instruction) {
    emit_load_value(instruction.first, "i64", "x");
    line("\tieq64 z8 x64 0;");
    line("\tjumpif z8 " + cy_block(function.name, instruction.third.text) + ";");
    line("\tjump " + cy_block(function.name, instruction.second.text) + ";");
  }

  void emit_switch(const Function &function, const Instruction &instruction) {
    emit_load_value(instruction.first, "i64", "x");
    for (std::size_t i = 0; i < instruction.args.size(); i += 2) {
      emit_load_value(instruction.args[i], "i64", "t");
      line("\tieq64 z8 x64 t64;");
      line("\tjumpif z8 " + cy_block(function.name, instruction.args[i + 1].text) + ";");
    }
    line("\tjump " + cy_block(function.name, instruction.second.text) + ";");
  }

  void emit_eh_push(const Function &function, const Operand &target) {
    line("\tisub64 sp sp 32;");
    line("\tmove64 z64 [g____cppgm_eh_top];");
    line("\tmove64 [sp] z64;");
    line("\tmove64 z64 " + cy_block(function.name, target.text) + ";");
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
    if (instruction.type.text == "f32")
      line("\tmove32 x32 [g____cppgm_eh_value];");
    else
      line("\tmove" + width_name(instruction.type.text) + " x" + width_name(instruction.type.text) + " [g____cppgm_eh_value];");
    emit_result(instruction.dest, instruction.type.text, "x");
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
    if (instruction.type.text == "void") {
      line("\tjump " + cy_function(function.name) + "__epilogue;");
      return;
    }
    if (starts_with(instruction.type.text, "obj<")) {
      emit_object_address(instruction.first, "x64");
      line("\tmove64 y64 [bp-8];");
      const std::size_t bytes = type_storage_size(instruction.type.text);
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
      line("\tjump " + cy_function(function.name) + "__epilogue;");
      return;
    }
    if (instruction.type.text == "f80") {
      emit_f80_operand_to_scratch(instruction.first, 0);
      line("\tmove64 x64 [bp-8];");
      emit_f80_copy(f80_scratch_low(0), f80_scratch_high(0), "[x64]", "[x64+8]");
      line("\tjump " + cy_function(function.name) + "__epilogue;");
      return;
    }
    emit_load_value(instruction.first, instruction.type.text, "x");
    line("\tjump " + cy_function(function.name) + "__epilogue;");
  }

  void emit_instruction(const Function &function, const Instruction &instruction) {
    switch (instruction.kind) {
    case Instruction::IK_CONST:
      if (instruction.type.text == "f80") {
        emit_f80_operand_to_scratch(instruction.first, 0);
        emit_f80_result_from_scratch(instruction.dest, 0);
      } else {
        emit_load_value(instruction.first, instruction.type.text, "x");
        emit_result(instruction.dest, instruction.type.text, "x");
      }
      break;
    case Instruction::IK_COPY:
      if (instruction.type.text == "f80") {
        emit_f80_operand_to_scratch(instruction.first, 0);
        emit_f80_result_from_scratch(instruction.dest, 0);
      } else {
        emit_load_value(instruction.first, instruction.type.text, "x");
        emit_result(instruction.dest, instruction.type.text, "x");
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
      line("\tjump " + cy_block(function.name, instruction.first.text) + ";");
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
      emit_load_value(instruction.first, instruction.type.text, "x");
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

  static std::string data_literal(const Operand &operand) { return operand.text.empty() || operand.text == "nullptr" ? "0" : operand.text; }

  void emit_zero_data(std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) line("\tdata8 0;");
  }

  void emit_f80_data(const std::string &literal) {
    const long double value = parse_float(literal);
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
    const std::size_t alignment = type_storage_alignment(item.type.text);
    while (alignment > 1 && *offset % alignment != 0) {
      line("\tdata8 0;");
      ++*offset;
    }
    if (item.kind == GlobalDefinition::DataItem::ITEM_ADDR) {
      std::string target = symbols_[item.symbol];
      if (target.empty()) target = item.symbol;
      if (item.addr_addend == 0)
        line("\tdata64 " + target + ";");
      else
        line("\tdata64 (" + target + (item.addr_addend > 0 ? " + " : " - ") + immediate_signed(std::llabs(item.addr_addend)) + ");");
    } else if (item.type.text == "f80") {
      emit_f80_data(item.literal_operand.text);
    } else {
      const std::string width = width_name(item.type.text);
      line("\tdata" + width + " " + data_literal(item.literal_operand) + ";");
    }
    *offset += item.kind == GlobalDefinition::DataItem::ITEM_ADDR ? 8 : type_storage_size(item.type.text);
  }

  void emit_global(const GlobalDefinition &global) {
    line(cy_global(global.name) + ":");
    if (global.structured) {
      std::size_t offset = 0;
      for (std::size_t i = 0; i < global.data_items.size(); ++i) emit_global_item(global.data_items[i], &offset);
    } else if (global.init_kind == GlobalDefinition::INIT_ADDR) {
      const std::string target = symbols_[global.init_operand.text];
      if (global.addr_addend == 0)
        line("\tdata64 " + target + ";");
      else
        line("\tdata64 (" + target + (global.addr_addend > 0 ? " + " : " - ") + immediate_signed(std::llabs(global.addr_addend)) + ");");
    } else if (global.init_kind == GlobalDefinition::INIT_ZERO) {
      if (global.type.text == "f80")
        emit_f80_data("0.0L");
      else
        line("\tdata" + width_name(global.type.text) + " 0;");
    } else if (global.type.text == "f80") {
      emit_f80_data(global.init_operand.text);
    } else {
      line("\tdata" + width_name(global.type.text) + " " + data_literal(global.init_operand) + ";");
    }
    if (&global != &program_.globals.back()) blank();
  }

  const Program &program_;
  Validator validator_;
  std::ostringstream out_;
  std::map<std::string, std::string> symbols_;
  std::map<std::string, OutputSymbol> output_symbols_;
  std::map<std::string, const Function *> functions_;
  std::map<std::string, const FunctionDeclaration *> function_declarations_;
  const Function *current_function_;
  const FunctionLayout *current_layout_;
  std::size_t atomic_label_counter_ = 0;
  std::size_t eh_label_counter_ = 0;
  bool has_eh_ = false;
};

void compile(const std::vector<std::string> &source_files, const std::string &output_file) {
  if (source_files.empty()) throw LowirError("no LowIR source files");
  std::string text;
  for (std::size_t i = 0; i < source_files.size(); ++i) {
    std::ifstream input(source_files[i].c_str());
    if (!input) throw LowirError("unable to open LowIR source file");
    std::ostringstream contents;
    contents << input.rdbuf();
    text += contents.str();
    text += "\n";
  }
  Parser parser(text);
  const Program program = parser.parse();
  Validator validator(program);
  validator.validate();
  Emitter emitter(program);
  const std::string output = emitter.emit();
  std::ofstream file(output_file.c_str(), std::ios::out | std::ios::trunc);
  if (!file) throw LowirError("unable to open CY86 output file");
  file << output;
  if (!file) throw LowirError("unable to write CY86 output file");
}

} // namespace lowir2cy86
