#include "lowir_model.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace lowir_model {

namespace {

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
          throw ParseError("invalid LowIR character");
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
    throw ParseError("invalid integer literal");
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
    throw ParseError("invalid floating literal");
  }
  return value;
}

std::string signed_literal_text(const std::string &first, const std::string &second) { return first == "-" ? "-" + second : second; }

bool is_name_kind(const std::string &text, char prefix) { return text.size() > 1 && text[0] == prefix; }

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

LowType make_type(const std::string &text) {
  LowType type;
  if (text == "void") {
    type.kind = LowType::TYPE_VOID;
  } else if (text == "ptr") {
    type.kind = LowType::TYPE_POINTER;
  } else if (text == "i1") {
    type.kind = LowType::TYPE_INTEGER;
    type.integer_kind = LowType::INTEGER_I1;
  } else if (text == "i8") {
    type.kind = LowType::TYPE_INTEGER;
    type.integer_kind = LowType::INTEGER_I8;
  } else if (text == "u8") {
    type.kind = LowType::TYPE_INTEGER;
    type.integer_kind = LowType::INTEGER_U8;
  } else if (text == "i16") {
    type.kind = LowType::TYPE_INTEGER;
    type.integer_kind = LowType::INTEGER_I16;
  } else if (text == "u16") {
    type.kind = LowType::TYPE_INTEGER;
    type.integer_kind = LowType::INTEGER_U16;
  } else if (text == "i32") {
    type.kind = LowType::TYPE_INTEGER;
    type.integer_kind = LowType::INTEGER_I32;
  } else if (text == "u32") {
    type.kind = LowType::TYPE_INTEGER;
    type.integer_kind = LowType::INTEGER_U32;
  } else if (text == "i64") {
    type.kind = LowType::TYPE_INTEGER;
    type.integer_kind = LowType::INTEGER_I64;
  } else if (text == "u64") {
    type.kind = LowType::TYPE_INTEGER;
    type.integer_kind = LowType::INTEGER_U64;
  } else if (text == "f32") {
    type.kind = LowType::TYPE_FLOAT;
    type.float_kind = LowType::FLOAT_F32;
  } else if (text == "f64") {
    type.kind = LowType::TYPE_FLOAT;
    type.float_kind = LowType::FLOAT_F64;
  } else if (text == "f80") {
    type.kind = LowType::TYPE_FLOAT;
    type.float_kind = LowType::FLOAT_F80;
  } else {
    std::size_t bytes = 0;
    std::size_t alignment = 0;
    if (!parse_object_type(text, &bytes, &alignment)) return LowType();
    type.kind = LowType::TYPE_OBJECT;
    type.object_bytes = bytes;
    type.object_alignment = alignment;
  }
  return type;
}

LowType builtin_type(const char *text) { return make_type(text); }

UnaryOperator parse_unary_operator(const std::string &text) {
  if (text == "neg") return lowir_model::UOP_NEG;
  if (text == "not") return lowir_model::UOP_NOT;
  if (text == "bitnot") return lowir_model::UOP_BITNOT;
  if (text == "decay") return lowir_model::UOP_DECAY;
  if (text == "bswap") return lowir_model::UOP_BSWAP;
  return lowir_model::UOP_INVALID;
}

BinaryOperator parse_binary_operator(const std::string &text) {
  if (text == "add") return lowir_model::BOP_ADD;
  if (text == "sub") return lowir_model::BOP_SUB;
  if (text == "mul") return lowir_model::BOP_MUL;
  if (text == "div") return lowir_model::BOP_DIV;
  if (text == "mod") return lowir_model::BOP_MOD;
  if (text == "udiv") return lowir_model::BOP_UDIV;
  if (text == "umod") return lowir_model::BOP_UMOD;
  if (text == "and") return lowir_model::BOP_AND;
  if (text == "or") return lowir_model::BOP_OR;
  if (text == "xor") return lowir_model::BOP_XOR;
  if (text == "shl") return lowir_model::BOP_SHL;
  if (text == "shr") return lowir_model::BOP_SHR;
  if (text == "ushr") return lowir_model::BOP_USHR;
  return lowir_model::BOP_INVALID;
}

ComparePredicate parse_compare_predicate(const std::string &text) {
  if (text == "eq") return lowir_model::CPP_EQ;
  if (text == "ne") return lowir_model::CPP_NE;
  if (text == "lt") return lowir_model::CPP_LT;
  if (text == "le") return lowir_model::CPP_LE;
  if (text == "gt") return lowir_model::CPP_GT;
  if (text == "ge") return lowir_model::CPP_GE;
  if (text == "ult") return lowir_model::CPP_ULT;
  if (text == "ule") return lowir_model::CPP_ULE;
  if (text == "ugt") return lowir_model::CPP_UGT;
  if (text == "uge") return lowir_model::CPP_UGE;
  return lowir_model::CPP_INVALID;
}

ConversionOperator parse_conversion_operator(const std::string &text) {
  if (text == "sext") return lowir_model::COP_SEXT;
  if (text == "zext") return lowir_model::COP_ZEXT;
  if (text == "trunc") return lowir_model::COP_TRUNC;
  if (text == "sitofp") return lowir_model::COP_SITOFP;
  if (text == "uitofp") return lowir_model::COP_UITOFP;
  if (text == "fptosi") return lowir_model::COP_FPTOSI;
  if (text == "fptoui") return lowir_model::COP_FPTOUI;
  if (text == "fpext") return lowir_model::COP_FPEXT;
  if (text == "fptrunc") return lowir_model::COP_FPTRUNC;
  return lowir_model::COP_INVALID;
}

AtomicOrder parse_atomic_order(long long value) {
  switch (value) {
  case 0: return lowir_model::AO_RELAXED;
  case 1: return lowir_model::AO_CONSUME;
  case 2: return lowir_model::AO_ACQUIRE;
  case 3: return lowir_model::AO_RELEASE;
  case 4: return lowir_model::AO_ACQ_REL;
  case 5: return lowir_model::AO_SEQ_CST;
  default: return lowir_model::AO_INVALID;
  }
}
class Parser {
public:
  explicit Parser(const std::string &text) : tokens_(Lexer(text).tokens()), position_(0), presentation_(), presentation_ids_() {}

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
    program.presentation = presentation_;
    return program;
  }

private:
  const Token &peek() const {
    if (at_end()) {
      throw ParseError("unexpected end of LowIR input");
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
      throw ParseError(message + " at end of input");
    }
    std::ostringstream out;
    out << message << " near '" << tokens_[position_].text << "'";
    throw ParseError(out.str());
  }

  lowir_model::SpellingId intern(const std::string &text) {
    std::map<std::string, lowir_model::SpellingId>::const_iterator found = presentation_ids_.find(text);
    if (found != presentation_ids_.end()) return found->second;
    const lowir_model::SpellingId id(presentation_.size());
    presentation_.push_back(text);
    presentation_ids_[text] = id;
    return id;
  }

  lowir_model::SpellingId parse_name(char prefix, const std::string &what) {
    const std::string name = take(what);
    if (!is_name_kind(name, prefix)) {
      fail("expected " + what);
    }
    return intern(name);
  }

  LowType parse_type() {
    const std::string spelling = take("type");
    LowType type = make_type(spelling);
    if (!type.valid()) {
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
    operand.presentation_id = intern(value);
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
      operand.literal_type = builtin_type("i64");
    } else if (is_float_text(value)) {
      operand.kind = Operand::OP_FLOAT;
      operand.float_value = parse_float(value);
      operand.literal_type = builtin_type("f64");
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
      throw ParseError("invalid " + key + " metadata value");
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
        metadata->object_symbol_id = intern(value);
      } else if (key == "tls_for") {
        if (global_context) fail("tls_for is not global metadata");
        if (!is_name_kind(value, '@')) fail("invalid tls_for metadata");
        metadata->tls_for_name_id = intern(value);
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
    parameter.name_id = parse_name('%', "parameter name");
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
    location.file_id = intern(take("debug file"));
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
    global.name_id = parse_name('@', "global name");
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
          item.type = builtin_type("ptr");
          const std::string symbol = take("address symbol");
          if (!is_name_kind(symbol, '@')) fail("invalid global address initializer");
          item.symbol_name_id = intern(symbol);
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
    declaration.name_id = parse_name('@', "global name");
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
      declaration.name_id = parse_name('@', "function name");
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
    const std::string object_symbol = take("object alias symbol");
    if (object_symbol.empty() || object_symbol[0] == '@' || object_symbol[0] == '%' || object_symbol[0] == '$') {
      fail("invalid object alias symbol");
    }
    alias.object_name_id = intern(object_symbol);
    expect("=");
    const std::string target = take("object alias target");
    if (!is_name_kind(target, '@')) fail("invalid object alias target");
    alias.target_name_id = intern(target);
    return alias;
  }

  Function parse_function_definition() {
    Function function;
    function.name_id = parse_name('@', "function name");
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
        const lowir_model::SpellingId name = parse_name('$', "slot name");
        expect(":");
        Function::Slot slot;
        slot.name_id = name;
        slot.type = parse_type();
        function.slots.push_back(slot);
      } else if (accept("block")) {
        Block block;
        block.label_id = parse_name('^', "block name");
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
    instruction.destination_name_id = parse_name('%', "temporary destination");
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
      instruction.type = builtin_type("ptr");
      instruction.first = parse_operand();
    } else if (opcode == "load" || opcode == "atomic_load") {
      instruction.kind = opcode == "load" ? Instruction::IK_LOAD : Instruction::IK_ATOMIC_LOAD;
      instruction.type = parse_type();
      instruction.first = parse_operand();
      if (instruction.kind == Instruction::IK_ATOMIC_LOAD) {
        expect(",");
        instruction.atomic_order = parse_atomic_order(parse_integer_literal());
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
      instruction.unary_operator = parse_unary_operator(take("unary operator"));
      instruction.type = parse_type();
      instruction.first = parse_operand();
    } else if (opcode == "binary" || opcode == "cmp") {
      instruction.kind = opcode == "binary" ? Instruction::IK_BINARY : Instruction::IK_CMP;
      const std::string operator_spelling = take(opcode == "binary" ? "binary operator" : "comparison predicate");
      if (opcode == "binary") instruction.binary_operator = parse_binary_operator(operator_spelling);
      else instruction.compare_predicate = parse_compare_predicate(operator_spelling);
      instruction.type = parse_type();
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
    } else if (opcode == "convert") {
      instruction.kind = Instruction::IK_CONVERT;
      instruction.conversion_operator = parse_conversion_operator(take("conversion operator"));
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
      instruction.atomic_order = parse_atomic_order(parse_integer_literal());
    } else if (opcode == "atomic_exchange") {
      instruction.kind = Instruction::IK_ATOMIC_EXCHANGE;
      instruction.type = parse_type();
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
      expect(",");
      instruction.atomic_order = parse_atomic_order(parse_integer_literal());
    } else if (opcode == "atomic_compare_exchange") {
      instruction.kind = Instruction::IK_ATOMIC_COMPARE_EXCHANGE;
      instruction.type = parse_type();
      instruction.first = parse_operand();
      expect(",");
      instruction.second = parse_operand();
      expect(",");
      instruction.third = parse_operand();
      expect(",");
      instruction.atomic_order = parse_atomic_order(parse_integer_literal());
      expect(",");
      instruction.atomic_failure_order = parse_atomic_order(parse_integer_literal());
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
        instruction.atomic_order = parse_atomic_order(parse_integer_literal());
      }
    } else if (opcode == "atomic_thread_fence" || opcode == "atomic_signal_fence") {
      instruction.kind = opcode == "atomic_thread_fence" ? Instruction::IK_ATOMIC_THREAD_FENCE : Instruction::IK_ATOMIC_SIGNAL_FENCE;
      instruction.atomic_order = parse_atomic_order(parse_integer_literal());
    } else if (opcode == "call") {
      instruction.kind = Instruction::IK_CALL;
      if (accept("void")) {
        instruction.call_returns_void = true;
        instruction.call_return_type = builtin_type("void");
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
      if (!instruction.type.is_void()) instruction.first = parse_operand();
    } else {
      fail("unknown LowIR instruction");
    }
    parse_optional_debug(&instruction);
    return instruction;
  }

  std::vector<Token> tokens_;
  std::size_t position_;
  std::vector<std::string> presentation_;
  std::map<std::string, lowir_model::SpellingId> presentation_ids_;
};

}  // namespace

LowirProgram parse_lowir_program_text(const std::string &text, const std::string &source_name)
{
  (void)source_name;
  return Parser(text).parse();
}

LowirProgram parse_lowir_program_files(const std::vector<std::string> &paths)
{
  std::string text;
  for (std::size_t i = 0; i < paths.size(); ++i) {
    std::ifstream input(paths[i].c_str());
    if (!input) throw ParseError("unable to open LowIR source file");
    std::ostringstream contents;
    contents << input.rdbuf();
    text += contents.str();
    text += "\n";
  }
  return parse_lowir_program_text(text, paths.empty() ? std::string("<empty>") : paths[0]);
}

}  // namespace lowir_model
