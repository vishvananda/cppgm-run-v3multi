// Student-facing driver for the PA14 `abimangle` binary.

#include "abi_mangle.h"
#include "exceptions.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

namespace {

bool is_builtin_name(const string & name)
{
  static const char * const names[] = {
    "void", "wchar", "bool", "char", "schar", "uchar", "short", "ushort",
    "int", "uint", "long", "ulong", "longlong", "ulonglong", "int128",
    "uint128", "float", "double", "longdouble", "float128", "ellipsis",
    "char16", "char32", "char8", "nullptr", "complex-float", "complex-double",
    "complex-longdouble"
  };
  for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
    if(name == names[i]) return true;
  }
  return false;
}

size_t parse_index(const string & spelling)
{
  if(spelling.empty() || spelling[0] == '-') {
    throw logic_error("ABI index must be a non-negative decimal integer");
  }
  char * end = NULL;
  errno = 0;
  const unsigned long long value = strtoull(spelling.c_str(), &end, 10);
  if(errno != 0 || end == spelling.c_str() || *end != '\0' ||
     value > numeric_limits<size_t>::max()) {
    throw logic_error("invalid ABI index '" + spelling + "'");
  }
  return static_cast<size_t>(value);
}

long long parse_value(const string & spelling)
{
  if(spelling.empty()) throw logic_error("empty ABI integral value");
  char * end = NULL;
  errno = 0;
  const long long value = strtoll(spelling.c_str(), &end, 10);
  if(errno != 0 || end == spelling.c_str() || *end != '\0') {
    throw logic_error("invalid ABI integral value '" + spelling + "'");
  }
  return value;
}

bool is_single_colon(const string & value, size_t at)
{
  return value[at] == ':' && (at == 0 || value[at - 1] != ':') &&
    (at + 1 == value.size() || value[at + 1] != ':');
}

abi_mangle::AbiType parse_compact_type_at(const string & spelling, size_t & at);

bool consume_compact_prefix(const string & spelling, size_t & at, const char * prefix)
{
  const size_t length = strlen(prefix);
  if(spelling.compare(at, length, prefix) != 0) return false;
  at += length;
  return true;
}

abi_mangle::AbiType make_unary_type(abi_mangle::AbiTypeKind kind,
                                    const abi_mangle::AbiType & child)
{
  abi_mangle::AbiType result;
  result.kind = kind;
  result.types.push_back(child);
  return result;
}

abi_mangle::AbiType parse_compact_type(const string & spelling)
{
  size_t at = 0;
  abi_mangle::AbiType result = parse_compact_type_at(spelling, at);
  if(at != spelling.size()) throw logic_error("trailing compact ABI type fields");
  return result;
}

abi_mangle::AbiType parse_compact_type_at(const string & spelling, size_t & at)
{
  const size_t begin = at;
  abi_mangle::AbiTypeKind unary_kind = abi_mangle::ABI_TYPE_POINTER;
  bool is_cv = false;
  bool is_const = false;
  bool is_volatile = false;
  if(consume_compact_prefix(spelling, at, "ptr:")) {
    unary_kind = abi_mangle::ABI_TYPE_POINTER;
  } else if(consume_compact_prefix(spelling, at, "ref:")) {
    unary_kind = abi_mangle::ABI_TYPE_LVALUE_REFERENCE;
  } else if(consume_compact_prefix(spelling, at, "rref:")) {
    unary_kind = abi_mangle::ABI_TYPE_RVALUE_REFERENCE;
  } else if(consume_compact_prefix(spelling, at, "const:")) {
    unary_kind = abi_mangle::ABI_TYPE_CV;
    is_cv = true;
    is_const = true;
  } else if(consume_compact_prefix(spelling, at, "volatile:")) {
    unary_kind = abi_mangle::ABI_TYPE_CV;
    is_cv = true;
    is_volatile = true;
  }
  if(at != begin) {
    if(at == spelling.size()) throw logic_error("missing child type in compact ABI type");
    abi_mangle::AbiType result = make_unary_type(unary_kind,
                                                  parse_compact_type_at(spelling, at));
    if(is_cv) {
      result.is_const = is_const;
      result.is_volatile = is_volatile;
    }
    return result;
  }

  if(consume_compact_prefix(spelling, at, "array:")) {
    const size_t bound_begin = at;
    while(at < spelling.size() && spelling[at] != ':') ++at;
    if(at == bound_begin || at == spelling.size()) {
      throw logic_error("invalid compact array type '" + spelling + "'");
    }
    const string bound = spelling.substr(bound_begin, at - bound_begin);
    parse_index(bound);
    ++at;
    if(at == spelling.size()) throw logic_error("missing compact array element type");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_ARRAY;
    result.array_bound.kind = abi_mangle::ABI_ARRAY_BOUND_VALUE;
    result.array_bound.value = bound;
    result.types.push_back(parse_compact_type_at(spelling, at));
    return result;
  }

  if(consume_compact_prefix(spelling, at, "memberptr:")) {
    const size_t owner_begin = at;
    size_t separator = string::npos;
    while(at < spelling.size()) {
      if(is_single_colon(spelling, at)) {
        separator = at;
        break;
      }
      ++at;
    }
    if(separator == string::npos || separator == owner_begin || separator + 1 == spelling.size()) {
      throw logic_error("invalid compact member-pointer type '" + spelling + "'");
    }
    const string owner = spelling.substr(owner_begin, separator - owner_begin);
    at = separator + 1;
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_MEMBER_POINTER;
    result.types.push_back(parse_compact_type(owner));
    result.types.push_back(parse_compact_type_at(spelling, at));
    return result;
  }

  if(consume_compact_prefix(spelling, at, "named:")) {
    if(at == spelling.size()) throw logic_error("empty named ABI type");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_NAMED;
    result.name = spelling.substr(at);
    at = spelling.size();
    return result;
  }

  if(consume_compact_prefix(spelling, at, "template-param:")) {
    if(at == spelling.size()) throw logic_error("missing template parameter index");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_TEMPLATE_PARAMETER;
    result.index = parse_index(spelling.substr(at));
    at = spelling.size();
    return result;
  }

  if(at == spelling.size()) throw logic_error("empty compact ABI type");
  abi_mangle::AbiType result;
  result.name = spelling.substr(at);
  result.kind = is_builtin_name(result.name) ? abi_mangle::ABI_TYPE_BUILTIN :
    abi_mangle::ABI_TYPE_NAME_OR_REFERENCE;
  at = spelling.size();
  return result;
}

abi_mangle::AbiType parse_type_words(const vector<string> & words, size_t & at)
{
  if(at >= words.size()) throw logic_error("missing ABI type");
  const string spelling = words[at++];

  if(spelling == "ptr" || spelling == "ref" || spelling == "rref") {
    const abi_mangle::AbiTypeKind kind = spelling == "ptr" ?
      abi_mangle::ABI_TYPE_POINTER : (spelling == "ref" ?
      abi_mangle::ABI_TYPE_LVALUE_REFERENCE : abi_mangle::ABI_TYPE_RVALUE_REFERENCE);
    return make_unary_type(kind, parse_type_words(words, at));
  }
  if(spelling == "const" || spelling == "volatile") {
    abi_mangle::AbiType result = make_unary_type(abi_mangle::ABI_TYPE_CV,
                                                  parse_type_words(words, at));
    result.is_const = spelling == "const";
    result.is_volatile = spelling == "volatile";
    return result;
  }
  if(spelling == "array") {
    if(at >= words.size()) throw logic_error("missing ABI array bound");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_ARRAY;
    result.array_bound.kind = abi_mangle::ABI_ARRAY_BOUND_VALUE;
    result.array_bound.value = words[at++];
    parse_index(result.array_bound.value);
    result.types.push_back(parse_type_words(words, at));
    return result;
  }
  if(spelling == "member-pointer") {
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_MEMBER_POINTER;
    result.types.push_back(parse_type_words(words, at));
    result.types.push_back(parse_type_words(words, at));
    return result;
  }
  if(spelling == "function-type" || spelling == "function-type-variadic") {
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_FUNCTION;
    result.variadic = spelling == "function-type-variadic";
    result.types.push_back(parse_type_words(words, at));
    while(at < words.size()) result.types.push_back(parse_type_words(words, at));
    return result;
  }
  if(spelling == "vendor") {
    if(at >= words.size()) throw logic_error("missing vendor qualifier name");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_VENDOR_QUALIFIED;
    result.name = words[at++];
    result.types.push_back(parse_type_words(words, at));
    return result;
  }
  if(spelling == "builtin-transform") {
    if(at >= words.size()) throw logic_error("missing builtin transform name");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_BUILTIN_TRANSFORM;
    result.name = words[at++];
    result.types.push_back(parse_type_words(words, at));
    return result;
  }
  if(spelling == "tagged") {
    abi_mangle::AbiType result = parse_type_words(words, at);
    while(at < words.size()) result.abi_tags.push_back(words[at++]);
    return result;
  }
  if(spelling == "named") {
    if(at >= words.size()) throw logic_error("missing named ABI type name");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_NAMED;
    result.name = words[at++];
    return result;
  }
  if(spelling == "template-param" || spelling == "template-param-subst") {
    if(at >= words.size()) throw logic_error("missing template parameter index");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_TEMPLATE_PARAMETER;
    result.index = parse_index(words[at++]);
    return result;
  }
  if(spelling == "template" || spelling == "std-template") {
    abi_mangle::AbiType result;
    result.kind = spelling == "template" ? abi_mangle::ABI_TYPE_TEMPLATE_SPECIALIZATION :
      abi_mangle::ABI_TYPE_STD_TEMPLATE_SPECIALIZATION;
    if(at >= words.size()) throw logic_error("missing template ABI name");
    if(spelling == "std-template") {
      result.standard_substitution = words[at++];
      if(at >= words.size()) throw logic_error("missing standard template flag");
      result.standard_substitution_includes_arguments = words[at++] == "yes";
      if(at >= words.size()) throw logic_error("missing standard template name");
      result.name = words[at++];
    } else {
      result.name = words[at++];
    }
    while(at < words.size()) result.argument_refs.push_back(words[at++]);
    return result;
  }
  if(spelling == "member") {
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_MEMBER;
    result.types.push_back(parse_type_words(words, at));
    if(at >= words.size()) throw logic_error("missing member ABI type name");
    result.name = words[at++];
    return result;
  }
  if(spelling == "member-template") {
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_MEMBER_TEMPLATE_SPECIALIZATION;
    result.types.push_back(parse_type_words(words, at));
    while(at < words.size()) result.argument_refs.push_back(words[at++]);
    return result;
  }
  if(spelling == "decltype") {
    if(at >= words.size()) throw logic_error("missing decltype expression id");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_DECLTYPE_EXPRESSION;
    result.expression_ref = words[at++];
    return result;
  }
  if(spelling == "pack") {
    return make_unary_type(abi_mangle::ABI_TYPE_PACK_EXPANSION,
                           parse_type_words(words, at));
  }
  if(spelling == "namespace-lambda") {
    if(at >= words.size()) throw logic_error("missing namespace lambda source name");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_NAMESPACE_LAMBDA;
    result.name = words[at++];
    while(at < words.size()) result.namespace_qualifiers.push_back(words[at++]);
    return result;
  }
  if(spelling == "lambda-closure") {
    if(at + 1 >= words.size()) throw logic_error("incomplete lambda closure type");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_LAMBDA_CLOSURE;
    result.context_ref = words[at++];
    result.discriminator = words[at++];
    return result;
  }
  if(spelling == "local-type") {
    if(at + 1 >= words.size()) throw logic_error("incomplete local type");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_LOCAL_TYPE;
    result.context_ref = words[at++];
    result.name = words[at++];
    if(at < words.size()) result.discriminator = words[at++];
    return result;
  }

  return parse_compact_type(spelling);
}

void require_end(const vector<string> & words, size_t at)
{
  if(at != words.size()) throw logic_error("unexpected extra ABI fact fields");
}

abi_mangle::AbiFunctionTarget parse_function_path(const vector<string> & words,
                                                  size_t name_at,
                                                  bool path_keyword)
{
  if(name_at >= words.size()) throw logic_error("missing ABI function name");
  abi_mangle::AbiFunctionTarget result;
  result.kind = abi_mangle::ABI_FUNCTION_TARGET_PATH;
  result.qualified_name = words[name_at++];
  if(path_keyword) {
    while(name_at < words.size()) {
      abi_mangle::AbiFunctionPathOperand operand;
      operand.kind = abi_mangle::ABI_FUNCTION_PATH_TEMPLATE_ARGUMENT;
      operand.argument_ref = words[name_at++];
      result.path_operands.push_back(operand);
    }
  } else {
    while(name_at < words.size()) {
      size_t type_at = name_at;
      result.signature_parameter_types.push_back(parse_type_words(words, type_at));
      name_at = type_at;
    }
  }
  return result;
}

abi_mangle::AbiFunctionRecord parse_function_record(const vector<string> & words)
{
  if(words.empty()) throw logic_error("empty ABI function record");
  abi_mangle::AbiFunctionRecord result;
  if(words[0] == "name-source") {
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_NAME_SOURCE;
    if(words.size() >= 2) result.source_name = words[1];
    if(words.size() >= 3) result.substitution = words[2];
    if(words.size() > 3) throw logic_error("too many name-source fields");
    return result;
  }
  if(words[0] == "name-std") {
    require_end(words, 1);
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_NAME_STD;
    return result;
  }
  if(words[0] == "name-template") {
    if(words.size() < 2) throw logic_error("missing template name record");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_NAME_TEMPLATE;
    result.name = words[1];
    if(words.size() >= 3) result.substitution = words[2];
    if(words.size() >= 4) result.complete_substitution = words[3];
    for(size_t i = 4; i < words.size(); ++i) result.argument_refs.push_back(words[i]);
    return result;
  }
  if(words[0] == "function-template-arg") {
    if(words.size() != 2) throw logic_error("invalid function-template-arg record");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_FUNCTION_TEMPLATE_ARGUMENT;
    result.argument_refs.push_back(words[1]);
    return result;
  }
  if(words[0] == "function-template-prefix") {
    if(words.size() != 2) throw logic_error("invalid function-template-prefix record");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_FUNCTION_TEMPLATE_PREFIX;
    result.substitution = words[1];
    return result;
  }
  if(words[0] == "local-context" || words[0] == "lambda-context" ||
     words[0] == "namespace-lambda-context") {
    if(words.size() < 2) throw logic_error("incomplete ABI local context record");
    result.kind = words[0] == "local-context" ? abi_mangle::ABI_FUNCTION_RECORD_LOCAL_CONTEXT :
      (words[0] == "lambda-context" ? abi_mangle::ABI_FUNCTION_RECORD_LAMBDA_CONTEXT :
       abi_mangle::ABI_FUNCTION_RECORD_NAMESPACE_LAMBDA_CONTEXT);
    result.context_ref = words[1];
    if(words.size() >= 3) result.source_name = words[2];
    if(words.size() >= 4) result.discriminator = words[3];
    return result;
  }
  if(words[0] == "terminal" || words[0] == "terminal-source") {
    if(words.size() != 2) throw logic_error("invalid ABI terminal record");
    result.kind = words[0] == "terminal" ? abi_mangle::ABI_FUNCTION_RECORD_TERMINAL :
      abi_mangle::ABI_FUNCTION_RECORD_TERMINAL_SOURCE;
    result.terminal = words[1];
    return result;
  }
  if(words[0] == "operator-terminal") {
    if(words.size() < 2 || words.size() > 3) throw logic_error("invalid operator terminal");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_OPERATOR_TERMINAL;
    result.terminal = words[1];
    if(words.size() == 3) result.literal_suffix = words[2];
    return result;
  }
  if(words[0] == "conversion-terminal") {
    if(words.size() != 2) throw logic_error("invalid conversion terminal");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_CONVERSION_TERMINAL;
    result.terminal = words[1];
    return result;
  }
  if(words[0] == "variadic") {
    require_end(words, 1);
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_VARIADIC;
    return result;
  }
  if(words[0] == "abi-tag") {
    if(words.size() != 2) throw logic_error("invalid ABI tag record");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_ABI_TAG;
    result.name = words[1];
    return result;
  }
  if(words[0] == "function-qualifier" || words[0] == "qualifier") {
    if(words.size() == 1) throw logic_error("empty ABI function qualifier record");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_QUALIFIER;
    for(size_t i = 1; i < words.size(); ++i) {
      if(words[i] == "const") result.qualifiers.push_back(abi_mangle::ABI_FUNCTION_QUALIFIER_CONST);
      else if(words[i] == "volatile") result.qualifiers.push_back(abi_mangle::ABI_FUNCTION_QUALIFIER_VOLATILE);
      else if(words[i] == "lvalue-ref") result.qualifiers.push_back(abi_mangle::ABI_FUNCTION_QUALIFIER_LVALUE_REFERENCE);
      else if(words[i] == "rvalue-ref") result.qualifiers.push_back(abi_mangle::ABI_FUNCTION_QUALIFIER_RVALUE_REFERENCE);
      else throw logic_error("unknown ABI function qualifier '" + words[i] + "'");
    }
    return result;
  }
  if(words[0] == "param" || words[0] == "result") {
    result.kind = words[0] == "param" ? abi_mangle::ABI_FUNCTION_RECORD_PARAMETER :
      abi_mangle::ABI_FUNCTION_RECORD_RESULT;
    size_t at = 1;
    result.type = parse_type_words(words, at);
    require_end(words, at);
    return result;
  }
  throw logic_error("unknown ABI function record '" + words[0] + "'");
}

abi_mangle::AbiFactRecord parse_definition_record(const vector<string> & words)
{
  if(words.size() < 2) throw logic_error("incomplete ABI definition record");
  abi_mangle::AbiFactRecord result;
  result.kind = abi_mangle::ABI_FACT_RECORD_DEFINITION;
  result.definition.id = words[1];
  if(result.definition.id.empty() || result.definition.id == "-") throw logic_error("invalid ABI definition id");
  if(words[0] == "let-type") {
    result.definition.kind = abi_mangle::ABI_DEFINITION_TYPE;
    size_t at = 2;
    result.definition.type = parse_type_words(words, at);
    require_end(words, at);
    return result;
  }
  if(words[0] == "let-arg") {
    result.definition.kind = abi_mangle::ABI_DEFINITION_TEMPLATE_ARGUMENT;
    if(words.size() < 3) throw logic_error("incomplete ABI template argument definition");
    const string kind = words[2];
    size_t at = 3;
    if(kind == "type") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_TYPE;
      result.definition.template_argument.type = parse_type_words(words, at);
    } else if(kind == "value") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_VALUE;
      result.definition.template_argument.value_type = parse_type_words(words, at);
      result.definition.template_argument.has_value_type = true;
      if(at >= words.size()) throw logic_error("missing ABI template value");
      result.definition.template_argument.value = parse_value(words[at++]);
    } else if(kind == "expression") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_EXPRESSION;
      if(at >= words.size()) throw logic_error("missing ABI expression argument reference");
      result.definition.template_argument.entity_ref = words[at++];
    } else if(kind == "template-param-template") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_TEMPLATE_PARAMETER_TEMPLATE;
      if(at >= words.size()) throw logic_error("missing template-template parameter index");
      result.definition.template_argument.index = parse_index(words[at++]);
    } else if(kind == "entity-address") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_ENTITY;
      if(at >= words.size()) throw logic_error("missing entity reference");
      result.definition.template_argument.entity_ref = words[at++];
      result.definition.template_argument.address_of = true;
    } else if(kind == "template-entity") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_TEMPLATE_ENTITY;
      if(at >= words.size()) throw logic_error("missing template entity name");
      result.definition.template_argument.name = words[at++];
    } else {
      throw logic_error("unknown ABI template argument kind '" + kind + "'");
    }
    require_end(words, at);
    return result;
  }
  if(words[0] == "let-expr") {
    result.definition.kind = abi_mangle::ABI_DEFINITION_EXPRESSION;
    if(words.size() < 4) throw logic_error("incomplete ABI expression definition");
    const string kind = words[2];
    if(kind == "literal") {
      result.definition.expression.kind = abi_mangle::ABI_EXPRESSION_LITERAL;
      result.definition.expression.value = parse_value(words[3]);
    } else if(kind == "template-param") {
      result.definition.expression.kind = abi_mangle::ABI_EXPRESSION_TEMPLATE_PARAMETER;
      result.definition.expression.index = parse_index(words[3]);
    } else if(kind == "function-param") {
      result.definition.expression.kind = abi_mangle::ABI_EXPRESSION_FUNCTION_PARAMETER;
      result.definition.expression.index = parse_index(words[3]);
    } else {
      result.definition.expression.kind = abi_mangle::ABI_EXPRESSION_ENTITY;
      result.definition.expression.text = words[3];
      for(size_t i = 4; i < words.size(); ++i) result.definition.expression.expression_refs.push_back(words[i]);
    }
    return result;
  }
  if(words[0] == "let-context") {
    if(words.size() < 4) throw logic_error("incomplete ABI context definition");
    result.definition.kind = abi_mangle::ABI_DEFINITION_CONTEXT;
    const string context_kind = words[2];
    if(context_kind == "raw") {
      result.definition.context.kind = abi_mangle::ABI_CONTEXT_RAW;
      result.definition.context.fragment = words[3];
      require_end(words, 4);
    } else if(context_kind == "function") {
      result.definition.context.kind = abi_mangle::ABI_CONTEXT_FUNCTION;
      result.definition.context.function = parse_function_path(words, 3, false);
    } else {
      throw logic_error("unknown ABI context kind '" + context_kind + "'");
    }
    return result;
  }
  if(words[0] == "let-entity") {
    if(words.size() < 4) throw logic_error("incomplete ABI entity definition");
    result.definition.kind = abi_mangle::ABI_DEFINITION_ENTITY;
    const string entity_kind = words[2];
    if(entity_kind == "symbol") {
      result.definition.entity.kind = abi_mangle::ABI_ENTITY_FACT_SYMBOL;
      result.definition.entity.qualified_name = words[3];
      require_end(words, 4);
    } else if(entity_kind == "variable" || entity_kind == "internal-variable") {
      result.definition.entity.kind = abi_mangle::ABI_ENTITY_FACT_VARIABLE;
      result.definition.entity.qualified_name = words[3];
      result.definition.entity.internal_linkage = entity_kind == "internal-variable";
      require_end(words, 4);
    } else if(entity_kind == "function") {
      result.definition.entity.kind = abi_mangle::ABI_ENTITY_FACT_FUNCTION;
      result.definition.entity.function = parse_function_path(words, 3, false);
    } else {
      throw logic_error("unknown ABI entity kind '" + entity_kind + "'");
    }
    return result;
  }
  throw logic_error("unknown ABI definition record '" + words[0] + "'");
}

}  // namespace

namespace abi_mangle {

AbiFactRecord parse_fact_record_words(const vector<string> & words)
{
  if(words.empty()) throw logic_error("empty ABI fact record");
  if(words[0].compare(0, 4, "let-") == 0) return parse_definition_record(words);
  if(words[0] == "param" || words[0] == "result" || words[0] == "variadic" ||
     words[0] == "name-source" || words[0] == "name-std" || words[0] == "name-template" ||
     words[0] == "function-template-arg" || words[0] == "function-template-prefix" ||
     words[0] == "local-context" || words[0] == "lambda-context" ||
     words[0] == "namespace-lambda-context" || words[0] == "terminal" ||
     words[0] == "terminal-source" || words[0] == "operator-terminal" ||
     words[0] == "conversion-terminal" || words[0] == "abi-tag" ||
     words[0] == "function-qualifier" || words[0] == "qualifier") {
    AbiFactRecord result;
    result.kind = ABI_FACT_RECORD_FUNCTION;
    result.function = parse_function_record(words);
    return result;
  }

  AbiFactRecord result;
  result.kind = ABI_FACT_RECORD_TARGET;
  if(words[0] == "type") {
    result.target.kind = ABI_TARGET_FACT_TYPE;
    size_t at = 1;
    result.target.type = parse_type_words(words, at);
    require_end(words, at);
  } else if(words[0] == "function") {
    result.target.kind = ABI_TARGET_FACT_FUNCTION;
    if(words.size() < 2) throw logic_error("incomplete ABI function target");
    if(words[1] == "encoding") {
      require_end(words, 2);
      result.target.function.kind = ABI_FUNCTION_TARGET_ENCODING;
    } else if(words[1] == "path") {
      result.target.function = parse_function_path(words, 2, true);
    } else if(words[1] == "namespace-lambda") {
      if(words.size() < 4) throw logic_error("incomplete namespace lambda function target");
      result.target.function.kind = ABI_FUNCTION_TARGET_NAMESPACE_LAMBDA;
      result.target.function.source_name = words[2];
      result.target.function.terminal = words[3];
      for(size_t i = 4; i < words.size(); ++i) result.target.function.namespace_qualifiers.push_back(words[i]);
    } else if(words[1] == "local" || words[1] == "lambda") {
      if(words.size() < 5) throw logic_error("incomplete local function target");
      result.target.function.kind = words[1] == "local" ? ABI_FUNCTION_TARGET_LOCAL : ABI_FUNCTION_TARGET_LAMBDA;
      result.target.function.context_ref = words[2];
      result.target.function.source_name = words[3];
      result.target.function.terminal = words[4];
      if(words.size() >= 6) result.target.function.discriminator = words[5];
      for(size_t i = 6; i < words.size(); ++i) {
        size_t at = i;
        result.target.function.signature_parameter_types.push_back(parse_type_words(words, at));
        i = at - 1;
      }
    } else {
      result.target.function = parse_function_path(words, 1, false);
    }
  } else if(words[0] == "c-function") {
    if(words.size() < 2) throw logic_error("incomplete C function target");
    result.target.kind = ABI_TARGET_FACT_FUNCTION;
    result.target.c_linkage = true;
    result.target.function = parse_function_path(words, 1, false);
  } else if(words[0] == "variable") {
    if(words.size() != 2) throw logic_error("invalid ABI variable target");
    result.target.kind = ABI_TARGET_FACT_VARIABLE;
    result.target.qualified_name = words[1];
  } else if(words[0] == "typeinfo" || words[0] == "vtable" || words[0] == "vtt") {
    if(words.size() < 2) throw logic_error("incomplete ABI special target");
    result.target.kind = words[0] == "typeinfo" ? ABI_TARGET_FACT_TYPEINFO :
      (words[0] == "vtable" ? ABI_TARGET_FACT_VTABLE : ABI_TARGET_FACT_VTT);
    size_t at = 1;
    result.target.type = parse_type_words(words, at);
    require_end(words, at);
  } else if(words[0] == "construction-vtable") {
    if(words.size() < 4) throw logic_error("incomplete construction vtable target");
    result.target.kind = ABI_TARGET_FACT_CONSTRUCTION_VTABLE;
    size_t at = 1;
    result.target.type = parse_type_words(words, at);
    if(at >= words.size()) throw logic_error("missing construction vtable offset");
    result.target.base_offset = parse_index(words[at++]);
    result.target.base_type = parse_type_words(words, at);
    require_end(words, at);
  } else if(words[0] == "tls-wrapper") {
    if(words.size() != 3 || words[1] != "variable") throw logic_error("invalid TLS wrapper target");
    result.target.kind = ABI_TARGET_FACT_THREAD_LOCAL_WRAPPER;
    result.target.qualified_name = words[2];
  } else if(words[0] == "thunk" || words[0] == "virtual-base-thunk") {
    result.target.kind = words[0] == "thunk" ? ABI_TARGET_FACT_THUNK : ABI_TARGET_FACT_VIRTUAL_BASE_THUNK;
    size_t at = 1;
    if(at >= words.size()) throw logic_error("missing thunk adjustment");
    result.target.this_adjust = parse_value(words[at++]);
    if(at < words.size() && words[at] == "virtual-result") {
      result.target.result_adjust_virtual = true;
      ++at;
      if(at + 1 >= words.size()) throw logic_error("incomplete virtual result adjustment");
      result.target.has_result_adjust = true;
      result.target.result_adjust = parse_value(words[at++]);
      result.target.result_vcall_offset = parse_value(words[at++]);
    } else if(at < words.size() && words[at] != "function") {
      result.target.has_result_adjust = true;
      result.target.result_adjust = parse_value(words[at++]);
    }
    if(at >= words.size() || words[at++] != "function") throw logic_error("missing thunk function target");
    vector<string> function_words(words.begin() + at, words.end());
    if(function_words.empty() || function_words[0] != "path") throw logic_error("thunk requires function path");
    result.target.function = parse_function_path(function_words, 1, true);
  } else {
    throw logic_error("unknown ABI target record '" + words[0] + "'");
  }
  return result;
}

AbiFactFile parse_fact_text(const string & text)
{
  AbiFactFile result;
  AbiFactCase current;
  set<string> definition_ids;
  istringstream input(text);
  string line;
  while(getline(input, line)) {
    const size_t comment = line.find('#');
    if(comment != string::npos) line.erase(comment);
    istringstream line_stream(line);
    vector<string> words;
    string word;
    while(line_stream >> word) words.push_back(word);
    if(words.empty()) {
      if(!current.records.empty()) {
        result.cases.push_back(current);
        current = AbiFactCase();
        definition_ids.clear();
      }
      continue;
    }
    if(words[0] == "case") {
      if(words.size() != 2) throw logic_error("case requires one label");
      if(!current.records.empty()) {
        result.cases.push_back(current);
        current = AbiFactCase();
        definition_ids.clear();
      }
      current.label = words[1];
      continue;
    }
    AbiFactRecord record = parse_fact_record_words(words);
    if(record.kind == ABI_FACT_RECORD_DEFINITION &&
       !definition_ids.insert(record.definition.id).second) {
      throw logic_error("duplicate ABI definition id '" + record.definition.id + "'");
    }
    current.records.push_back(record);
  }
  if(!current.records.empty()) result.cases.push_back(current);
  if(result.cases.empty()) throw logic_error("ABI fact file has no cases");
  return result;
}

string type_spelling(const AbiType & type)
{
  ostringstream index;
  switch(type.kind) {
  case ABI_TYPE_BUILTIN: return type.name;
  case ABI_TYPE_NAMED: return "named:" + type.name;
  case ABI_TYPE_NAME_OR_REFERENCE: return type.name;
  case ABI_TYPE_POINTER: return "ptr:" + type_spelling(type.types.at(0));
  case ABI_TYPE_LVALUE_REFERENCE: return "ref:" + type_spelling(type.types.at(0));
  case ABI_TYPE_RVALUE_REFERENCE: return "rref:" + type_spelling(type.types.at(0));
  case ABI_TYPE_CV: return string(type.is_const ? "const:" : "volatile:") + type_spelling(type.types.at(0));
  case ABI_TYPE_ARRAY: return "array:" + type.array_bound.value + ":" + type_spelling(type.types.at(0));
  case ABI_TYPE_TEMPLATE_PARAMETER:
    index << type.index;
    return "template-param:" + index.str();
  default: return type.name;
  }
}

string serialize_fact_file(const AbiFactFile & file)
{
  ostringstream output;
  bool first_case = true;
  for(vector<AbiFactCase>::const_iterator c = file.cases.begin(); c != file.cases.end(); ++c) {
    if(!first_case) output << "\n";
    first_case = false;
    if(!c->label.empty()) output << "case " << c->label << "\n";
    for(vector<AbiFactRecord>::const_iterator it = c->records.begin(); it != c->records.end(); ++it) {
      if(it->kind == ABI_FACT_RECORD_DEFINITION) {
        const AbiDefinitionRecord & d = it->definition;
        if(d.kind == ABI_DEFINITION_TYPE) output << "let-type " << d.id << " " << type_spelling(d.type) << "\n";
        else if(d.kind == ABI_DEFINITION_TEMPLATE_ARGUMENT && d.template_argument.kind == ABI_TEMPLATE_ARGUMENT_VALUE)
          output << "let-arg " << d.id << " value " << type_spelling(d.template_argument.value_type) << " " << d.template_argument.value << "\n";
        else output << "let-arg " << d.id << " type " << type_spelling(d.template_argument.type) << "\n";
      } else if(it->kind == ABI_FACT_RECORD_FUNCTION) {
        const AbiFunctionRecord & f = it->function;
        if(f.kind == ABI_FUNCTION_RECORD_PARAMETER) output << "param " << type_spelling(f.type) << "\n";
        else if(f.kind == ABI_FUNCTION_RECORD_RESULT) output << "result " << type_spelling(f.type) << "\n";
        else if(f.kind == ABI_FUNCTION_RECORD_VARIADIC) output << "variadic\n";
        else if(f.kind == ABI_FUNCTION_RECORD_ABI_TAG) output << "abi-tag " << f.name << "\n";
        else if(f.kind == ABI_FUNCTION_RECORD_TERMINAL) output << "terminal " << f.terminal << "\n";
        else if(f.kind == ABI_FUNCTION_RECORD_TERMINAL_SOURCE) output << "terminal-source " << f.terminal << "\n";
        else if(f.kind == ABI_FUNCTION_RECORD_NAME_STD) output << "name-std\n";
        else if(f.kind == ABI_FUNCTION_RECORD_NAME_SOURCE) output << "name-source " << f.source_name << "\n";
      } else {
        const AbiTargetRecord & t = it->target;
        switch(t.kind) {
        case ABI_TARGET_FACT_TYPE: output << "type " << type_spelling(t.type) << "\n"; break;
        case ABI_TARGET_FACT_VARIABLE: output << "variable " << t.qualified_name << "\n"; break;
        case ABI_TARGET_FACT_TYPEINFO: output << "typeinfo " << type_spelling(t.type) << "\n"; break;
        case ABI_TARGET_FACT_VTABLE: output << "vtable " << type_spelling(t.type) << "\n"; break;
        case ABI_TARGET_FACT_VTT: output << "vtt " << type_spelling(t.type) << "\n"; break;
        default: output << "target\n"; break;
        }
      }
    }
  }
  return output.str();
}

string mangle_fact_file(const AbiFactFile & file)
{
  ostringstream output;
  for(vector<AbiFactCase>::const_iterator it = file.cases.begin(); it != file.cases.end(); ++it) {
    output << mangle_abi_fact_case(*it) << "\n";
  }
  return output.str();
}

string mangle_fact_files(const vector<string> & input_paths)
{
  ostringstream output;
  for(vector<string>::const_iterator path = input_paths.begin(); path != input_paths.end(); ++path) {
    ifstream input(path->c_str());
    if(!input) throw logic_error("unable to open input file '" + *path + "'");
    ostringstream text;
    text << input.rdbuf();
    output << mangle_fact_file(parse_fact_text(text.str()));
  }
  return output.str();
}

}  // namespace abi_mangle

namespace {

struct AbimangleInvocation
{
  string outfile;
  vector<string> inputs;
};

bool has_help_arg(int argc, char ** argv)
{
  for(int i = 1; i < argc; ++i) {
    const string arg = argv[i];
    if(arg == "--help" || arg == "-h") return true;
  }
  return false;
}

void print_help()
{
  cout << "usage: abimangle -o <outfile> <abi-facts-file>...\n";
}

AbimangleInvocation parse_invocation(int argc, char ** argv)
{
  AbimangleInvocation invocation;
  for(int i = 1; i < argc; ++i) {
    const string arg = argv[i];
    if(arg == "-o") {
      if(i + 1 >= argc) throw logic_error("missing output file after -o");
      invocation.outfile = argv[++i];
    } else {
      invocation.inputs.push_back(arg);
    }
  }
  if(invocation.outfile.empty() || invocation.inputs.empty()) throw logic_error("invalid usage");
  return invocation;
}

int run_abimangle(int argc, char ** argv)
{
  if(has_help_arg(argc, argv)) {
    print_help();
    return EXIT_SUCCESS;
  }
  const AbimangleInvocation invocation = parse_invocation(argc, argv);
  ofstream out(invocation.outfile.c_str());
  if(!out) throw logic_error("unable to open output file '" + invocation.outfile + "'");
  out << abi_mangle::mangle_fact_files(invocation.inputs);
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char ** argv)
{
  try {
    return run_abimangle(argc, argv);
  } catch(const NotImplementedException &) {
    cerr << "abimangle: not implemented\n";
    return EXIT_FAILURE;
  } catch(const exception & e) {
    cerr << "abimangle: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
