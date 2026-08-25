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
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

using namespace std;

namespace {

abi_mangle::AbiBuiltinKind builtin_kind(const string & name)
{
  struct BuiltinName {
    const char * spelling;
    abi_mangle::AbiBuiltinKind kind;
  };
  static const BuiltinName names[] = {
    {"void", abi_mangle::ABI_BUILTIN_VOID},
    {"wchar", abi_mangle::ABI_BUILTIN_WCHAR},
    {"bool", abi_mangle::ABI_BUILTIN_BOOL},
    {"char", abi_mangle::ABI_BUILTIN_CHAR},
    {"schar", abi_mangle::ABI_BUILTIN_SIGNED_CHAR},
    {"uchar", abi_mangle::ABI_BUILTIN_UNSIGNED_CHAR},
    {"short", abi_mangle::ABI_BUILTIN_SHORT},
    {"ushort", abi_mangle::ABI_BUILTIN_UNSIGNED_SHORT},
    {"int", abi_mangle::ABI_BUILTIN_INT},
    {"uint", abi_mangle::ABI_BUILTIN_UNSIGNED_INT},
    {"long", abi_mangle::ABI_BUILTIN_LONG},
    {"ulong", abi_mangle::ABI_BUILTIN_UNSIGNED_LONG},
    {"longlong", abi_mangle::ABI_BUILTIN_LONG_LONG},
    {"ulonglong", abi_mangle::ABI_BUILTIN_UNSIGNED_LONG_LONG},
    {"int128", abi_mangle::ABI_BUILTIN_INT128},
    {"uint128", abi_mangle::ABI_BUILTIN_UNSIGNED_INT128},
    {"float", abi_mangle::ABI_BUILTIN_FLOAT},
    {"double", abi_mangle::ABI_BUILTIN_DOUBLE},
    {"longdouble", abi_mangle::ABI_BUILTIN_LONG_DOUBLE},
    {"float128", abi_mangle::ABI_BUILTIN_FLOAT128},
    {"ellipsis", abi_mangle::ABI_BUILTIN_ELLIPSIS},
    {"char16", abi_mangle::ABI_BUILTIN_CHAR16},
    {"char32", abi_mangle::ABI_BUILTIN_CHAR32},
    {"char8", abi_mangle::ABI_BUILTIN_CHAR8},
    {"nullptr", abi_mangle::ABI_BUILTIN_NULLPTR},
    {"complex-float", abi_mangle::ABI_BUILTIN_COMPLEX_FLOAT},
    {"complex-double", abi_mangle::ABI_BUILTIN_COMPLEX_DOUBLE},
    {"complex-longdouble", abi_mangle::ABI_BUILTIN_COMPLEX_LONG_DOUBLE}
  };
  for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
    if(name == names[i].spelling) return names[i].kind;
  }
  return abi_mangle::ABI_BUILTIN_INVALID;
}

abi_mangle::AbiStandardSubstitutionKind standard_substitution_kind(const string & spelling)
{
  if(spelling.empty() || spelling == "-") {
    return abi_mangle::ABI_STANDARD_SUBSTITUTION_NONE;
  }
  if(spelling == "Sa") return abi_mangle::ABI_STANDARD_SUBSTITUTION_ALLOCATOR;
  if(spelling == "Sb") return abi_mangle::ABI_STANDARD_SUBSTITUTION_BASIC_STRING;
  if(spelling == "Ss") return abi_mangle::ABI_STANDARD_SUBSTITUTION_STRING;
  if(spelling == "Si") return abi_mangle::ABI_STANDARD_SUBSTITUTION_ISTREAM;
  if(spelling == "So") return abi_mangle::ABI_STANDARD_SUBSTITUTION_OSTREAM;
  if(spelling == "Sd") return abi_mangle::ABI_STANDARD_SUBSTITUTION_IOSTREAM;
  throw logic_error("unknown ABI standard substitution '" + spelling + "'");
}

void validate_standard_substitution_name(
  abi_mangle::AbiStandardSubstitutionKind kind,
  const abi_mangle::AbiQualifiedName & name)
{
  if(name.components.size() != 2 || name.components[0] != "std") {
    throw logic_error("standard substitution name is not in std namespace");
  }
  const string & component = name.components[1];
  const bool valid =
    (kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_ALLOCATOR &&
     component == "allocator") ||
    ((kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_BASIC_STRING ||
      kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_STRING) &&
     component == "basic_string") ||
    (kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_ISTREAM &&
     component == "basic_istream") ||
    (kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_OSTREAM &&
     component == "basic_ostream") ||
    (kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_IOSTREAM &&
     component == "basic_iostream");
  if(!valid) throw logic_error("standard substitution code and name disagree");
}

void validate_standard_substitution_component(
  abi_mangle::AbiStandardSubstitutionKind kind,
  const string & component)
{
  const bool valid =
    (kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_ALLOCATOR &&
     component == "allocator") ||
    ((kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_BASIC_STRING ||
      kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_STRING) &&
     component == "basic_string") ||
    (kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_ISTREAM &&
     component == "basic_istream") ||
    (kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_OSTREAM &&
     component == "basic_ostream") ||
    (kind == abi_mangle::ABI_STANDARD_SUBSTITUTION_IOSTREAM &&
     component == "basic_iostream");
  if(!valid) throw logic_error("standard substitution code and name disagree");
}

bool parse_boolean_word(const string & spelling, const string & field)
{
  if(spelling == "yes" || spelling == "true") return true;
  if(spelling == "no" || spelling == "false") return false;
  throw logic_error("invalid boolean ABI field " + field);
}

abi_mangle::AbiQualifiedName parse_qualified_name(const string & spelling)
{
  abi_mangle::AbiQualifiedName result;
  size_t begin = spelling.compare(0, 2, "::") == 0 ? 2 : 0;
  if(begin == spelling.size()) throw logic_error("empty qualified ABI name");
  while(begin < spelling.size()) {
    const size_t separator = spelling.find("::", begin);
    const size_t end = separator == string::npos ? spelling.size() : separator;
    if(end == begin) throw logic_error("empty component in qualified ABI name");
    result.components.push_back(spelling.substr(begin, end - begin));
    if(separator == string::npos) return result;
    begin = separator + 2;
    if(begin == spelling.size()) {
      throw logic_error("trailing scope separator in qualified ABI name");
    }
  }
  throw logic_error("empty qualified ABI name");
}

abi_mangle::AbiQualifiedName parse_source_component(const string & spelling)
{
  abi_mangle::AbiQualifiedName result = parse_qualified_name(spelling);
  if(result.components.size() != 1) {
    throw logic_error("ABI source name must have one component");
  }
  return result;
}

struct DefinitionInterner
{
  map<string, size_t> indexes;
  vector<string> labels;
  vector<unsigned char> defined;

  size_t intern(const string & spelling)
  {
    if(spelling.empty() || spelling == "-") {
      throw logic_error("invalid ABI definition reference");
    }
    map<string, size_t>::const_iterator found = indexes.find(spelling);
    if(found != indexes.end()) return found->second;
    const size_t index = labels.size();
    indexes.insert(make_pair(spelling, index));
    labels.push_back(spelling);
    defined.push_back(0);
    return index;
  }

  abi_mangle::AbiDefinitionId reference(const string & spelling)
  {
    abi_mangle::AbiDefinitionId result;
    result.index = intern(spelling);
    return result;
  }

  abi_mangle::AbiDefinitionId define(const string & spelling)
  {
    const size_t index = intern(spelling);
    if(defined[index]) {
      throw logic_error("duplicate ABI definition id '" + spelling + "'");
    }
    defined[index] = 1;
    abi_mangle::AbiDefinitionId result;
    result.index = index;
    return result;
  }

  bool is_defined(size_t index) const
  {
    return index < defined.size() && defined[index] != 0;
  }

  const string & spelling(size_t index) const
  {
    if(index >= labels.size()) throw logic_error("unknown ABI definition index");
    return labels[index];
  }
};

abi_mangle::AbiFunctionSpecialTerminalKind special_terminal_kind(const string & spelling)
{
  if(spelling == "constructor-complete") {
    return abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_COMPLETE;
  }
  if(spelling == "constructor-base") {
    return abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_BASE;
  }
  if(spelling == "constructor-allocating") {
    return abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_ALLOCATING;
  }
  if(spelling == "destructor-deleting") {
    return abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_DELETING;
  }
  if(spelling == "destructor-complete") {
    return abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_COMPLETE;
  }
  if(spelling == "destructor-base") {
    return abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_BASE;
  }
  return abi_mangle::ABI_SPECIAL_TERMINAL_NONE;
}

abi_mangle::AbiOperatorTerminalKind operator_terminal_kind(const string & spelling)
{
  struct OperatorName {
    const char * spelling;
    abi_mangle::AbiOperatorTerminalKind kind;
  };
  static const OperatorName names[] = {
    {"new", abi_mangle::ABI_OPERATOR_TERMINAL_NEW},
    {"new-array", abi_mangle::ABI_OPERATOR_TERMINAL_NEW_ARRAY},
    {"delete", abi_mangle::ABI_OPERATOR_TERMINAL_DELETE},
    {"delete-array", abi_mangle::ABI_OPERATOR_TERMINAL_DELETE_ARRAY},
    {"unary-plus", abi_mangle::ABI_OPERATOR_TERMINAL_UNARY_PLUS},
    {"binary-plus", abi_mangle::ABI_OPERATOR_TERMINAL_BINARY_PLUS},
    {"plus", abi_mangle::ABI_OPERATOR_TERMINAL_PLUS},
    {"unary-minus", abi_mangle::ABI_OPERATOR_TERMINAL_UNARY_MINUS},
    {"binary-minus", abi_mangle::ABI_OPERATOR_TERMINAL_BINARY_MINUS},
    {"minus", abi_mangle::ABI_OPERATOR_TERMINAL_MINUS},
    {"address-of", abi_mangle::ABI_OPERATOR_TERMINAL_ADDRESS_OF},
    {"deref", abi_mangle::ABI_OPERATOR_TERMINAL_DEREF},
    {"complement", abi_mangle::ABI_OPERATOR_TERMINAL_COMPLEMENT},
    {"multiply", abi_mangle::ABI_OPERATOR_TERMINAL_MULTIPLY},
    {"divide", abi_mangle::ABI_OPERATOR_TERMINAL_DIVIDE},
    {"remainder", abi_mangle::ABI_OPERATOR_TERMINAL_REMAINDER},
    {"bit-and", abi_mangle::ABI_OPERATOR_TERMINAL_BIT_AND},
    {"bit-or", abi_mangle::ABI_OPERATOR_TERMINAL_BIT_OR},
    {"bit-xor", abi_mangle::ABI_OPERATOR_TERMINAL_BIT_XOR},
    {"assign", abi_mangle::ABI_OPERATOR_TERMINAL_ASSIGN},
    {"plus-assign", abi_mangle::ABI_OPERATOR_TERMINAL_PLUS_ASSIGN},
    {"minus-assign", abi_mangle::ABI_OPERATOR_TERMINAL_MINUS_ASSIGN},
    {"multiply-assign", abi_mangle::ABI_OPERATOR_TERMINAL_MULTIPLY_ASSIGN},
    {"divide-assign", abi_mangle::ABI_OPERATOR_TERMINAL_DIVIDE_ASSIGN},
    {"remainder-assign", abi_mangle::ABI_OPERATOR_TERMINAL_REMAINDER_ASSIGN},
    {"bit-and-assign", abi_mangle::ABI_OPERATOR_TERMINAL_BIT_AND_ASSIGN},
    {"bit-or-assign", abi_mangle::ABI_OPERATOR_TERMINAL_BIT_OR_ASSIGN},
    {"bit-xor-assign", abi_mangle::ABI_OPERATOR_TERMINAL_BIT_XOR_ASSIGN},
    {"left-shift", abi_mangle::ABI_OPERATOR_TERMINAL_LEFT_SHIFT},
    {"right-shift", abi_mangle::ABI_OPERATOR_TERMINAL_RIGHT_SHIFT},
    {"left-shift-assign", abi_mangle::ABI_OPERATOR_TERMINAL_LEFT_SHIFT_ASSIGN},
    {"right-shift-assign", abi_mangle::ABI_OPERATOR_TERMINAL_RIGHT_SHIFT_ASSIGN},
    {"equal", abi_mangle::ABI_OPERATOR_TERMINAL_EQUAL},
    {"not-equal", abi_mangle::ABI_OPERATOR_TERMINAL_NOT_EQUAL},
    {"less", abi_mangle::ABI_OPERATOR_TERMINAL_LESS},
    {"greater", abi_mangle::ABI_OPERATOR_TERMINAL_GREATER},
    {"less-equal", abi_mangle::ABI_OPERATOR_TERMINAL_LESS_EQUAL},
    {"greater-equal", abi_mangle::ABI_OPERATOR_TERMINAL_GREATER_EQUAL},
    {"logical-not", abi_mangle::ABI_OPERATOR_TERMINAL_LOGICAL_NOT},
    {"logical-and", abi_mangle::ABI_OPERATOR_TERMINAL_LOGICAL_AND},
    {"logical-or", abi_mangle::ABI_OPERATOR_TERMINAL_LOGICAL_OR},
    {"increment", abi_mangle::ABI_OPERATOR_TERMINAL_INCREMENT},
    {"decrement", abi_mangle::ABI_OPERATOR_TERMINAL_DECREMENT},
    {"comma", abi_mangle::ABI_OPERATOR_TERMINAL_COMMA},
    {"member-pointer", abi_mangle::ABI_OPERATOR_TERMINAL_MEMBER_POINTER},
    {"arrow", abi_mangle::ABI_OPERATOR_TERMINAL_ARROW},
    {"call", abi_mangle::ABI_OPERATOR_TERMINAL_CALL},
    {"operator-call", abi_mangle::ABI_OPERATOR_TERMINAL_CALL},
    {"index", abi_mangle::ABI_OPERATOR_TERMINAL_INDEX}
  };
  for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
    if(spelling == names[i].spelling) return names[i].kind;
  }
  if(spelling == "literal") return abi_mangle::ABI_OPERATOR_TERMINAL_LITERAL;
  throw logic_error("unknown ABI operator terminal '" + spelling + "'");
}

string special_terminal_spelling(abi_mangle::AbiFunctionSpecialTerminalKind terminal)
{
  switch(terminal) {
  case abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_COMPLETE:
    return "constructor-complete";
  case abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_BASE:
    return "constructor-base";
  case abi_mangle::ABI_SPECIAL_TERMINAL_CONSTRUCTOR_ALLOCATING:
    return "constructor-allocating";
  case abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_DELETING:
    return "destructor-deleting";
  case abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_COMPLETE:
    return "destructor-complete";
  case abi_mangle::ABI_SPECIAL_TERMINAL_DESTRUCTOR_BASE:
    return "destructor-base";
  case abi_mangle::ABI_SPECIAL_TERMINAL_NONE:
    break;
  }
  throw logic_error("unknown ABI special function terminal");
}

string operator_terminal_spelling(abi_mangle::AbiOperatorTerminalKind terminal)
{
  switch(terminal) {
  case abi_mangle::ABI_OPERATOR_TERMINAL_NEW: return "new";
  case abi_mangle::ABI_OPERATOR_TERMINAL_NEW_ARRAY: return "new-array";
  case abi_mangle::ABI_OPERATOR_TERMINAL_DELETE: return "delete";
  case abi_mangle::ABI_OPERATOR_TERMINAL_DELETE_ARRAY: return "delete-array";
  case abi_mangle::ABI_OPERATOR_TERMINAL_UNARY_PLUS: return "unary-plus";
  case abi_mangle::ABI_OPERATOR_TERMINAL_BINARY_PLUS: return "binary-plus";
  case abi_mangle::ABI_OPERATOR_TERMINAL_PLUS: return "plus";
  case abi_mangle::ABI_OPERATOR_TERMINAL_UNARY_MINUS: return "unary-minus";
  case abi_mangle::ABI_OPERATOR_TERMINAL_BINARY_MINUS: return "binary-minus";
  case abi_mangle::ABI_OPERATOR_TERMINAL_MINUS: return "minus";
  case abi_mangle::ABI_OPERATOR_TERMINAL_ADDRESS_OF: return "address-of";
  case abi_mangle::ABI_OPERATOR_TERMINAL_DEREF: return "deref";
  case abi_mangle::ABI_OPERATOR_TERMINAL_COMPLEMENT: return "complement";
  case abi_mangle::ABI_OPERATOR_TERMINAL_MULTIPLY: return "multiply";
  case abi_mangle::ABI_OPERATOR_TERMINAL_DIVIDE: return "divide";
  case abi_mangle::ABI_OPERATOR_TERMINAL_REMAINDER: return "remainder";
  case abi_mangle::ABI_OPERATOR_TERMINAL_BIT_AND: return "bit-and";
  case abi_mangle::ABI_OPERATOR_TERMINAL_BIT_OR: return "bit-or";
  case abi_mangle::ABI_OPERATOR_TERMINAL_BIT_XOR: return "bit-xor";
  case abi_mangle::ABI_OPERATOR_TERMINAL_ASSIGN: return "assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_PLUS_ASSIGN: return "plus-assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_MINUS_ASSIGN: return "minus-assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_MULTIPLY_ASSIGN: return "multiply-assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_DIVIDE_ASSIGN: return "divide-assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_REMAINDER_ASSIGN: return "remainder-assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_BIT_AND_ASSIGN: return "bit-and-assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_BIT_OR_ASSIGN: return "bit-or-assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_BIT_XOR_ASSIGN: return "bit-xor-assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_LEFT_SHIFT: return "left-shift";
  case abi_mangle::ABI_OPERATOR_TERMINAL_RIGHT_SHIFT: return "right-shift";
  case abi_mangle::ABI_OPERATOR_TERMINAL_LEFT_SHIFT_ASSIGN: return "left-shift-assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_RIGHT_SHIFT_ASSIGN: return "right-shift-assign";
  case abi_mangle::ABI_OPERATOR_TERMINAL_EQUAL: return "equal";
  case abi_mangle::ABI_OPERATOR_TERMINAL_NOT_EQUAL: return "not-equal";
  case abi_mangle::ABI_OPERATOR_TERMINAL_LESS: return "less";
  case abi_mangle::ABI_OPERATOR_TERMINAL_GREATER: return "greater";
  case abi_mangle::ABI_OPERATOR_TERMINAL_LESS_EQUAL: return "less-equal";
  case abi_mangle::ABI_OPERATOR_TERMINAL_GREATER_EQUAL: return "greater-equal";
  case abi_mangle::ABI_OPERATOR_TERMINAL_LOGICAL_NOT: return "logical-not";
  case abi_mangle::ABI_OPERATOR_TERMINAL_LOGICAL_AND: return "logical-and";
  case abi_mangle::ABI_OPERATOR_TERMINAL_LOGICAL_OR: return "logical-or";
  case abi_mangle::ABI_OPERATOR_TERMINAL_INCREMENT: return "increment";
  case abi_mangle::ABI_OPERATOR_TERMINAL_DECREMENT: return "decrement";
  case abi_mangle::ABI_OPERATOR_TERMINAL_COMMA: return "comma";
  case abi_mangle::ABI_OPERATOR_TERMINAL_MEMBER_POINTER: return "member-pointer";
  case abi_mangle::ABI_OPERATOR_TERMINAL_ARROW: return "arrow";
  case abi_mangle::ABI_OPERATOR_TERMINAL_CALL: return "call";
  case abi_mangle::ABI_OPERATOR_TERMINAL_INDEX: return "index";
  case abi_mangle::ABI_OPERATOR_TERMINAL_LITERAL: return "literal";
  case abi_mangle::ABI_OPERATOR_TERMINAL_NONE:
    break;
  }
  throw logic_error("unknown ABI operator terminal");
}

abi_mangle::AbiExpressionOperatorKind expression_operator_kind(const string & spelling)
{
  struct ExpressionOperatorName {
    const char * spelling;
    abi_mangle::AbiExpressionOperatorKind kind;
  };
  static const ExpressionOperatorName names[] = {
    {"nw", abi_mangle::ABI_EXPRESSION_OPERATOR_NEW},
    {"na", abi_mangle::ABI_EXPRESSION_OPERATOR_NEW_ARRAY},
    {"dl", abi_mangle::ABI_EXPRESSION_OPERATOR_DELETE},
    {"da", abi_mangle::ABI_EXPRESSION_OPERATOR_DELETE_ARRAY},
    {"aw", abi_mangle::ABI_EXPRESSION_OPERATOR_AWAIT},
    {"ps", abi_mangle::ABI_EXPRESSION_OPERATOR_UNARY_PLUS},
    {"ng", abi_mangle::ABI_EXPRESSION_OPERATOR_UNARY_MINUS},
    {"ad", abi_mangle::ABI_EXPRESSION_OPERATOR_ADDRESS_OF},
    {"de", abi_mangle::ABI_EXPRESSION_OPERATOR_DEREF},
    {"co", abi_mangle::ABI_EXPRESSION_OPERATOR_COMPLEMENT},
    {"pl", abi_mangle::ABI_EXPRESSION_OPERATOR_PLUS},
    {"mi", abi_mangle::ABI_EXPRESSION_OPERATOR_MINUS},
    {"ml", abi_mangle::ABI_EXPRESSION_OPERATOR_MULTIPLY},
    {"dv", abi_mangle::ABI_EXPRESSION_OPERATOR_DIVIDE},
    {"rm", abi_mangle::ABI_EXPRESSION_OPERATOR_REMAINDER},
    {"an", abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_AND},
    {"or", abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_OR},
    {"eo", abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_XOR},
    {"aS", abi_mangle::ABI_EXPRESSION_OPERATOR_ASSIGN},
    {"pL", abi_mangle::ABI_EXPRESSION_OPERATOR_PLUS_ASSIGN},
    {"mI", abi_mangle::ABI_EXPRESSION_OPERATOR_MINUS_ASSIGN},
    {"mL", abi_mangle::ABI_EXPRESSION_OPERATOR_MULTIPLY_ASSIGN},
    {"dV", abi_mangle::ABI_EXPRESSION_OPERATOR_DIVIDE_ASSIGN},
    {"rM", abi_mangle::ABI_EXPRESSION_OPERATOR_REMAINDER_ASSIGN},
    {"aN", abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_AND_ASSIGN},
    {"oR", abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_OR_ASSIGN},
    {"eO", abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_XOR_ASSIGN},
    {"ls", abi_mangle::ABI_EXPRESSION_OPERATOR_LEFT_SHIFT},
    {"rs", abi_mangle::ABI_EXPRESSION_OPERATOR_RIGHT_SHIFT},
    {"lS", abi_mangle::ABI_EXPRESSION_OPERATOR_LEFT_SHIFT_ASSIGN},
    {"rS", abi_mangle::ABI_EXPRESSION_OPERATOR_RIGHT_SHIFT_ASSIGN},
    {"eq", abi_mangle::ABI_EXPRESSION_OPERATOR_EQUAL},
    {"ne", abi_mangle::ABI_EXPRESSION_OPERATOR_NOT_EQUAL},
    {"lt", abi_mangle::ABI_EXPRESSION_OPERATOR_LESS},
    {"gt", abi_mangle::ABI_EXPRESSION_OPERATOR_GREATER},
    {"le", abi_mangle::ABI_EXPRESSION_OPERATOR_LESS_EQUAL},
    {"ge", abi_mangle::ABI_EXPRESSION_OPERATOR_GREATER_EQUAL},
    {"ss", abi_mangle::ABI_EXPRESSION_OPERATOR_SPACESHIP},
    {"nt", abi_mangle::ABI_EXPRESSION_OPERATOR_LOGICAL_NOT},
    {"aa", abi_mangle::ABI_EXPRESSION_OPERATOR_LOGICAL_AND},
    {"oo", abi_mangle::ABI_EXPRESSION_OPERATOR_LOGICAL_OR},
    {"pp", abi_mangle::ABI_EXPRESSION_OPERATOR_INCREMENT},
    {"mm", abi_mangle::ABI_EXPRESSION_OPERATOR_DECREMENT},
    {"cm", abi_mangle::ABI_EXPRESSION_OPERATOR_COMMA},
    {"pm", abi_mangle::ABI_EXPRESSION_OPERATOR_MEMBER_POINTER},
    {"pt", abi_mangle::ABI_EXPRESSION_OPERATOR_ARROW},
    {"cl", abi_mangle::ABI_EXPRESSION_OPERATOR_CALL},
    {"ix", abi_mangle::ABI_EXPRESSION_OPERATOR_INDEX},
    {"qu", abi_mangle::ABI_EXPRESSION_OPERATOR_CONDITIONAL}
  };
  for(size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
    if(spelling == names[i].spelling) return names[i].kind;
  }
  throw logic_error("unknown ABI expression operator '" + spelling + "'");
}

bool is_unary_expression_operator(
  abi_mangle::AbiExpressionOperatorKind kind)
{
  switch(kind) {
  // These are the one-operand productions represented by the normalized
  // `unary` fact.  nw/na have a different grammar (an expression list,
  // underscore, and a type), so accepting them here would create a malformed
  // prefix tree even though they are valid operator-name vocabulary.
  case abi_mangle::ABI_EXPRESSION_OPERATOR_DELETE:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_DELETE_ARRAY:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_AWAIT:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_UNARY_PLUS:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_UNARY_MINUS:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_ADDRESS_OF:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_DEREF:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_COMPLEMENT:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_LOGICAL_NOT:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_INCREMENT:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_DECREMENT:
    return true;
  default:
    return false;
  }
}

bool is_binary_expression_operator(
  abi_mangle::AbiExpressionOperatorKind kind)
{
  switch(kind) {
  case abi_mangle::ABI_EXPRESSION_OPERATOR_PLUS:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_MINUS:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_MULTIPLY:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_DIVIDE:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_REMAINDER:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_AND:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_OR:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_XOR:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_PLUS_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_MINUS_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_MULTIPLY_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_DIVIDE_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_REMAINDER_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_AND_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_OR_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_BIT_XOR_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_LEFT_SHIFT:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_RIGHT_SHIFT:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_LEFT_SHIFT_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_RIGHT_SHIFT_ASSIGN:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_EQUAL:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_NOT_EQUAL:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_LESS:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_GREATER:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_LESS_EQUAL:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_GREATER_EQUAL:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_SPACESHIP:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_LOGICAL_AND:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_LOGICAL_OR:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_COMMA:
  case abi_mangle::ABI_EXPRESSION_OPERATOR_MEMBER_POINTER:
    return true;
  default:
    return false;
  }
}

abi_mangle::AbiExpressionCastKind expression_cast_kind(const string & spelling)
{
  if(spelling == "dc") return abi_mangle::ABI_EXPRESSION_CAST_DYNAMIC;
  if(spelling == "sc") return abi_mangle::ABI_EXPRESSION_CAST_STATIC;
  if(spelling == "cc") return abi_mangle::ABI_EXPRESSION_CAST_CONST;
  if(spelling == "rc") return abi_mangle::ABI_EXPRESSION_CAST_REINTERPRET;
  throw logic_error("unknown ABI expression cast '" + spelling + "'");
}

abi_mangle::AbiExpressionMemberAccessKind expression_member_access_kind(
  const string & spelling)
{
  if(spelling == "dt") return abi_mangle::ABI_EXPRESSION_MEMBER_ACCESS_DOT;
  if(spelling == "pt") return abi_mangle::ABI_EXPRESSION_MEMBER_ACCESS_ARROW;
  throw logic_error("unknown ABI member access operator '" + spelling + "'");
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

abi_mangle::AbiType parse_compact_type_at(const string & spelling, size_t & at,
                                          DefinitionInterner & interner);

bool consume_compact_prefix(const string & spelling, size_t & at, const char * prefix)
{
  const size_t length = strlen(prefix);
  if(spelling.compare(at, length, prefix) != 0) return false;
  at += length;
  return true;
}

abi_mangle::AbiType make_unary_type(abi_mangle::AbiTypeKind kind,
                                    abi_mangle::AbiType child)
{
  abi_mangle::AbiType result;
  result.kind = kind;
  result.types.push_back(std::move(child));
  return result;
}

abi_mangle::AbiType parse_compact_type(const string & spelling,
                                       DefinitionInterner & interner)
{
  size_t at = 0;
  abi_mangle::AbiType result = parse_compact_type_at(spelling, at, interner);
  if(at != spelling.size()) throw logic_error("trailing compact ABI type fields");
  return result;
}

abi_mangle::AbiType parse_compact_type_at(const string & spelling, size_t & at,
                                          DefinitionInterner & interner)
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
                                                  parse_compact_type_at(spelling, at, interner));
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
    const size_t bound_value = parse_index(bound);
    ++at;
    if(at == spelling.size()) throw logic_error("missing compact array element type");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_ARRAY;
    result.array_bound.kind = abi_mangle::ABI_ARRAY_BOUND_VALUE;
    result.array_bound.value = bound_value;
    result.types.push_back(parse_compact_type_at(spelling, at, interner));
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
    result.types.push_back(parse_compact_type(owner, interner));
    result.types.push_back(parse_compact_type_at(spelling, at, interner));
    return result;
  }

  if(consume_compact_prefix(spelling, at, "named:")) {
    if(at == spelling.size()) throw logic_error("empty named ABI type");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_NAMED;
    result.name = parse_qualified_name(spelling.substr(at));
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
  const string token = spelling.substr(at);
  result.builtin = builtin_kind(token);
  result.kind = result.builtin == abi_mangle::ABI_BUILTIN_INVALID ?
    abi_mangle::ABI_TYPE_NAME_OR_REFERENCE : abi_mangle::ABI_TYPE_BUILTIN;
  if(result.kind == abi_mangle::ABI_TYPE_NAME_OR_REFERENCE) {
    result.name = parse_qualified_name(token);
  }
  at = spelling.size();
  return result;
}

abi_mangle::AbiType parse_type_words(const vector<string> & words, size_t & at,
                                     DefinitionInterner & interner)
{
  if(at >= words.size()) throw logic_error("missing ABI type");
  const string spelling = words[at++];

  if(spelling == "ptr" || spelling == "ref" || spelling == "rref") {
    const abi_mangle::AbiTypeKind kind = spelling == "ptr" ?
      abi_mangle::ABI_TYPE_POINTER : (spelling == "ref" ?
      abi_mangle::ABI_TYPE_LVALUE_REFERENCE : abi_mangle::ABI_TYPE_RVALUE_REFERENCE);
    return make_unary_type(kind, parse_type_words(words, at, interner));
  }
  if(spelling == "const" || spelling == "volatile") {
    abi_mangle::AbiType result = make_unary_type(abi_mangle::ABI_TYPE_CV,
                                                  parse_type_words(words, at, interner));
    result.is_const = spelling == "const";
    result.is_volatile = spelling == "volatile";
    return result;
  }
  if(spelling == "array") {
    if(at >= words.size()) throw logic_error("missing ABI array bound");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_ARRAY;
    result.array_bound.kind = abi_mangle::ABI_ARRAY_BOUND_VALUE;
    result.array_bound.value = parse_index(words[at++]);
    result.types.push_back(parse_type_words(words, at, interner));
    return result;
  }
  if(spelling == "member-pointer") {
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_MEMBER_POINTER;
    result.types.push_back(parse_type_words(words, at, interner));
    result.types.push_back(parse_type_words(words, at, interner));
    return result;
  }
  if(spelling == "function-type" || spelling == "function-type-variadic") {
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_FUNCTION;
    result.variadic = spelling == "function-type-variadic";
    result.types.push_back(parse_type_words(words, at, interner));
    while(at < words.size()) result.types.push_back(parse_type_words(words, at, interner));
    return result;
  }
  if(spelling == "vendor") {
    if(at >= words.size()) throw logic_error("missing vendor qualifier name");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_VENDOR_QUALIFIED;
    result.name = parse_source_component(words[at++]);
    result.types.push_back(parse_type_words(words, at, interner));
    return result;
  }
  if(spelling == "builtin-transform") {
    if(at >= words.size()) throw logic_error("missing builtin transform name");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_BUILTIN_TRANSFORM;
    result.name = parse_source_component(words[at++]);
    result.types.push_back(parse_type_words(words, at, interner));
    return result;
  }
  if(spelling == "tagged") {
    abi_mangle::AbiType result = parse_type_words(words, at, interner);
    while(at < words.size()) result.abi_tags.push_back(words[at++]);
    return result;
  }
  if(spelling == "named" || spelling == "name") {
    if(at >= words.size()) throw logic_error("missing named ABI type name");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_NAMED;
    result.name = parse_qualified_name(words[at++]);
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
      result.standard_substitution_kind = standard_substitution_kind(
        result.standard_substitution);
      if(result.standard_substitution_kind ==
         abi_mangle::ABI_STANDARD_SUBSTITUTION_NONE) {
        throw logic_error("std-template requires a standard substitution");
      }
      if(at >= words.size()) throw logic_error("missing standard template flag");
      if(words[at] != "yes" && words[at] != "no" &&
         words[at] != "true" && words[at] != "false") {
        throw logic_error("invalid standard template argument flag");
      }
      result.standard_substitution_includes_arguments =
        words[at] == "yes" || words[at] == "true";
      ++at;
      if(at >= words.size()) throw logic_error("missing standard template name");
      result.name = parse_qualified_name(words[at++]);
    } else {
      result.name = parse_qualified_name(words[at++]);
    }
    while(at < words.size()) result.argument_refs.push_back(interner.reference(words[at++]));
    if(spelling == "std-template") {
      validate_standard_substitution_name(result.standard_substitution_kind, result.name);
    }
    return result;
  }
  if(spelling == "member") {
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_MEMBER;
    result.types.push_back(parse_type_words(words, at, interner));
    if(at >= words.size()) throw logic_error("missing member ABI type name");
    result.name = parse_source_component(words[at++]);
    return result;
  }
  if(spelling == "member-template") {
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_MEMBER_TEMPLATE_SPECIALIZATION;
    result.types.push_back(parse_type_words(words, at, interner));
    if(at >= words.size()) throw logic_error("missing member template source name");
    result.name = parse_source_component(words[at++]);
    while(at < words.size()) result.argument_refs.push_back(interner.reference(words[at++]));
    return result;
  }
  if(spelling == "decltype" || spelling == "decltype-id") {
    if(at >= words.size()) throw logic_error("missing decltype expression id");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_DECLTYPE_EXPRESSION;
    result.decltype_kind = spelling == "decltype-id" ?
      abi_mangle::ABI_DECLTYPE_ID_OR_MEMBER : abi_mangle::ABI_DECLTYPE_EXPRESSION;
    result.expression_ref = interner.reference(words[at++]);
    return result;
  }
  if(spelling == "pack") {
    return make_unary_type(abi_mangle::ABI_TYPE_PACK_EXPANSION,
                           parse_type_words(words, at, interner));
  }
  if(spelling == "namespace-lambda") {
    if(at >= words.size()) throw logic_error("missing namespace lambda source name");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_NAMESPACE_LAMBDA;
    result.name = parse_source_component(words[at++]);
    while(at < words.size()) result.namespace_qualifiers.push_back(words[at++]);
    return result;
  }
  if(spelling == "lambda-closure") {
    if(at + 1 >= words.size()) throw logic_error("incomplete lambda closure type");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_LAMBDA_CLOSURE;
    result.context_ref = interner.reference(words[at++]);
    result.discriminator = words[at++];
    return result;
  }
  if(spelling == "local-type") {
    if(at + 1 >= words.size()) throw logic_error("incomplete local type");
    abi_mangle::AbiType result;
    result.kind = abi_mangle::ABI_TYPE_LOCAL_TYPE;
    result.context_ref = interner.reference(words[at++]);
    result.name = parse_source_component(words[at++]);
    if(at < words.size()) result.discriminator = words[at++];
    return result;
  }

  return parse_compact_type(spelling, interner);
}

void require_end(const vector<string> & words, size_t at)
{
  if(at != words.size()) throw logic_error("unexpected extra ABI fact fields");
}

abi_mangle::AbiFunctionTarget parse_function_path(const vector<string> & words,
                                                  size_t name_at,
                                                  bool path_keyword,
                                                  DefinitionInterner & interner)
{
  if(name_at >= words.size()) throw logic_error("missing ABI function name");
  abi_mangle::AbiFunctionTarget result;
  result.kind = abi_mangle::ABI_FUNCTION_TARGET_PATH;
  result.name = parse_qualified_name(words[name_at++]);
  if(path_keyword) {
    while(name_at < words.size()) {
      abi_mangle::AbiFunctionPathOperand operand;
      operand.kind = abi_mangle::ABI_FUNCTION_PATH_TEMPLATE_ARGUMENT;
      operand.argument_ref = interner.reference(words[name_at++]);
      result.path_operands.push_back(operand);
    }
  } else {
    while(name_at < words.size()) {
      size_t type_at = name_at;
      result.signature_parameter_types.push_back(parse_type_words(words, type_at, interner));
      name_at = type_at;
    }
  }
  return result;
}

abi_mangle::AbiFunctionRecord parse_function_record(const vector<string> & words,
                                                   DefinitionInterner & interner)
{
  if(words.empty()) throw logic_error("empty ABI function record");
  abi_mangle::AbiFunctionRecord result;
  if(words[0] == "name-source") {
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_NAME_SOURCE;
    if(words.size() >= 2 && words[1] != "-") {
      result.source_name = parse_qualified_name(words[1]);
    }
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
    parse_source_component(words[1]);
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_NAME_TEMPLATE;
    result.name = words[1];
    if(words.size() >= 3) result.substitution = words[2];
    if(words.size() >= 4) result.complete_substitution = words[3];
    size_t argument_at = 4;
    // The normalized form carries the standard-substitution identity and
    // whether that identity already includes its template arguments before
    // the typed argument references.  Keep the references typed; the
    // spelling fields are only adapter metadata for the ABI abbreviation.
    if(words.size() >= 6 &&
       (words[4] == "-" || words[5] == "yes" || words[5] == "no")) {
      result.standard_substitution = words[4] == "-" ? string() : words[4];
      result.standard_substitution_kind = standard_substitution_kind(words[4]);
      if(words[5] != "yes" && words[5] != "no") {
        throw logic_error("invalid standard substitution argument flag");
      }
      result.standard_substitution_includes_arguments = words[5] == "yes";
      argument_at = 6;
    }
    for(size_t i = argument_at; i < words.size(); ++i) {
      result.argument_refs.push_back(interner.reference(words[i]));
    }
    if(result.standard_substitution_kind !=
       abi_mangle::ABI_STANDARD_SUBSTITUTION_NONE) {
      validate_standard_substitution_component(result.standard_substitution_kind,
                                               result.name);
    }
    return result;
  }
  if(words[0] == "function-template-arg") {
    if(words.size() != 2) throw logic_error("invalid function-template-arg record");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_FUNCTION_TEMPLATE_ARGUMENT;
    result.argument_refs.push_back(interner.reference(words[1]));
    return result;
  }
  if(words[0] == "function-template-prefix") {
    if(words.size() != 2) throw logic_error("invalid function-template-prefix record");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_FUNCTION_TEMPLATE_PREFIX;
    result.substitution = words[1];
    result.has_function_template_prefix = true;
    if(words[1].compare(0, 14, "operator-name:") == 0) {
      const string terminal = words[1].substr(14);
      if(terminal.empty()) throw logic_error("empty function-template operator prefix");
      if(terminal == "cv") {
        result.function_template_prefix_conversion = true;
      } else if(terminal == "cl") {
        result.function_template_prefix_operator =
          abi_mangle::ABI_OPERATOR_TERMINAL_CALL;
      } else if(terminal == "ix") {
        result.function_template_prefix_operator =
          abi_mangle::ABI_OPERATOR_TERMINAL_INDEX;
      } else {
        throw logic_error("unknown function-template operator prefix '" + terminal + "'");
      }
    } else {
      result.function_template_prefix_name = parse_qualified_name(words[1]);
    }
    return result;
  }
  if(words[0] == "local-context") {
    if(words.size() < 2) throw logic_error("incomplete ABI local context record");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_LOCAL_CONTEXT;
    result.context_ref = interner.reference(words[1]);
    if(words.size() >= 3) result.source_name = parse_source_component(words[2]);
    if(words.size() >= 4) result.discriminator = words[3];
    return result;
  }
  if(words[0] == "lambda-context") {
    if(words.size() < 3) throw logic_error("incomplete ABI lambda context record");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_LAMBDA_CONTEXT;
    result.context_ref = interner.reference(words[1]);
    result.discriminator = words[2];
    for(size_t i = 3; i < words.size(); ++i) {
      size_t at = i;
      result.types.push_back(parse_type_words(words, at, interner));
      i = at - 1;
    }
    return result;
  }
  if(words[0] == "namespace-lambda-context") {
    if(words.size() < 2) throw logic_error("incomplete ABI namespace lambda context record");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_NAMESPACE_LAMBDA_CONTEXT;
    result.source_name = parse_source_component(words[1]);
    for(size_t i = 2; i < words.size(); ++i) result.namespace_qualifiers.push_back(words[i]);
    return result;
  }
  if(words[0] == "terminal" || words[0] == "terminal-source") {
    if(words.size() != 2) throw logic_error("invalid ABI terminal record");
    result.kind = words[0] == "terminal" ? abi_mangle::ABI_FUNCTION_RECORD_TERMINAL :
      abi_mangle::ABI_FUNCTION_RECORD_TERMINAL_SOURCE;
    result.terminal = words[1];
    result.special_terminal = words[0] == "terminal" ? special_terminal_kind(words[1]) :
      abi_mangle::ABI_SPECIAL_TERMINAL_NONE;
    if(result.special_terminal != abi_mangle::ABI_SPECIAL_TERMINAL_NONE) {
      result.terminal.clear();
    }
    return result;
  }
  if(words[0] == "operator-terminal") {
    if(words.size() < 2 || words.size() > 3) throw logic_error("invalid operator terminal");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_OPERATOR_TERMINAL;
    result.operator_terminal = operator_terminal_kind(words[1]);
    if(words.size() == 3) result.literal_suffix = words[2];
    if(result.operator_terminal == abi_mangle::ABI_OPERATOR_TERMINAL_LITERAL &&
       result.literal_suffix.empty()) {
      throw logic_error("literal operator terminal has no suffix");
    }
    return result;
  }
  if(words[0] == "conversion-terminal") {
    if(words.size() != 2) throw logic_error("invalid conversion terminal");
    result.kind = abi_mangle::ABI_FUNCTION_RECORD_CONVERSION_TERMINAL;
    size_t at = 1;
    result.conversion_type = parse_type_words(words, at, interner);
    result.has_conversion_type = true;
    require_end(words, at);
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
    result.type = parse_type_words(words, at, interner);
    require_end(words, at);
    return result;
  }
  throw logic_error("unknown ABI function record '" + words[0] + "'");
}

abi_mangle::AbiType make_expression_int_type()
{
  abi_mangle::AbiType result;
  result.kind = abi_mangle::ABI_TYPE_BUILTIN;
  result.builtin = abi_mangle::ABI_BUILTIN_INT;
  return result;
}

abi_mangle::AbiDependentExpression parse_dependent_expression(
  const vector<string> & words, DefinitionInterner & interner)
{
  if(words.size() < 3) throw logic_error("incomplete ABI expression definition");
  abi_mangle::AbiDependentExpression result;
  const string kind = words[2];

  if(kind == "literal") {
    result.kind = abi_mangle::ABI_EXPRESSION_LITERAL;
    size_t at = 3;
    // An untyped normalized literal is an int.  An optional explicit type is
    // retained as a typed value operand rather than as rendered ABI text.
    if(words.size() == 4) {
      result.value_type = make_expression_int_type();
    } else {
      result.value_type = parse_type_words(words, at, interner);
    }
    if(at >= words.size()) throw logic_error("missing ABI expression literal value");
    result.value = parse_value(words[at++]);
    require_end(words, at);
    return result;
  }
  if(kind == "integral-value") {
    result.kind = abi_mangle::ABI_EXPRESSION_INTEGRAL_VALUE;
    size_t at = 3;
    result.value_type = parse_type_words(words, at, interner);
    if(at >= words.size()) throw logic_error("missing ABI integral expression value");
    result.value = parse_value(words[at++]);
    require_end(words, at);
    return result;
  }
  if(kind == "template-param" || kind == "function-param") {
    result.kind = kind == "template-param" ?
      abi_mangle::ABI_EXPRESSION_TEMPLATE_PARAMETER :
      abi_mangle::ABI_EXPRESSION_FUNCTION_PARAMETER;
    if(words.size() != 4) throw logic_error("invalid ABI parameter expression");
    result.index = parse_index(words[3]);
    return result;
  }
  if(kind == "entity-reference") {
    if(words.size() != 4) throw logic_error("invalid entity-reference expression");
    result.kind = abi_mangle::ABI_EXPRESSION_ENTITY;
    result.entity_ref = interner.reference(words[3]);
    return result;
  }
  if(kind == "external-entity") {
    if(words.size() != 4 || words[3] == "-") {
      throw logic_error("invalid external-entity expression");
    }
    result.kind = abi_mangle::ABI_EXPRESSION_EXTERNAL_ENTITY;
    result.symbol = words[3];
    return result;
  }
  if(kind == "unary") {
    if(words.size() != 5) throw logic_error("invalid unary ABI expression");
    result.kind = abi_mangle::ABI_EXPRESSION_UNARY;
    result.operator_kind = expression_operator_kind(words[3]);
    if(!is_unary_expression_operator(result.operator_kind)) {
      throw logic_error("ABI unary expression uses a non-unary operator");
    }
    result.expression_refs.push_back(interner.reference(words[4]));
    return result;
  }
  if(kind == "binary") {
    if(words.size() != 6) throw logic_error("invalid binary ABI expression");
    result.kind = abi_mangle::ABI_EXPRESSION_BINARY;
    result.operator_kind = expression_operator_kind(words[3]);
    if(!is_binary_expression_operator(result.operator_kind)) {
      throw logic_error("ABI binary expression uses a non-binary operator");
    }
    result.expression_refs.push_back(interner.reference(words[4]));
    result.expression_refs.push_back(interner.reference(words[5]));
    return result;
  }
  if(kind == "conditional") {
    if(words.size() != 6) throw logic_error("invalid conditional ABI expression");
    result.kind = abi_mangle::ABI_EXPRESSION_CONDITIONAL;
    result.operator_kind = abi_mangle::ABI_EXPRESSION_OPERATOR_CONDITIONAL;
    result.expression_refs.push_back(interner.reference(words[3]));
    result.expression_refs.push_back(interner.reference(words[4]));
    result.expression_refs.push_back(interner.reference(words[5]));
    return result;
  }
  if(kind == "pack") {
    if(words.size() != 4) throw logic_error("invalid pack ABI expression");
    result.kind = abi_mangle::ABI_EXPRESSION_PACK_EXPANSION;
    result.expression_refs.push_back(interner.reference(words[3]));
    return result;
  }
  if(kind == "call") {
    if(words.size() < 4) throw logic_error("call ABI expression has no callee");
    result.kind = abi_mangle::ABI_EXPRESSION_CALL;
    for(size_t i = 3; i < words.size(); ++i) {
      result.expression_refs.push_back(interner.reference(words[i]));
    }
    return result;
  }
  if(kind == "conversion") {
    if(words.size() < 5) throw logic_error("incomplete conversion ABI expression");
    result.kind = abi_mangle::ABI_EXPRESSION_CONVERSION;
    size_t at = 3;
    if(words[at] == "cv") ++at;
    result.cast_kind = abi_mangle::ABI_EXPRESSION_CAST_NONE;
    result.type = parse_type_words(words, at, interner);
    if(at + 1 != words.size()) throw logic_error("conversion ABI expression needs one operand");
    result.expression_refs.push_back(interner.reference(words[at]));
    return result;
  }
  if(kind == "cast") {
    if(words.size() < 6) throw logic_error("incomplete cast ABI expression");
    result.kind = abi_mangle::ABI_EXPRESSION_CAST;
    size_t at = 3;
    result.cast_kind = expression_cast_kind(words[at++]);
    result.type = parse_type_words(words, at, interner);
    if(at + 1 != words.size()) throw logic_error("cast ABI expression needs one operand");
    result.expression_refs.push_back(interner.reference(words[at]));
    return result;
  }
  if(kind == "template-id") {
    if(words.size() < 4) throw logic_error("incomplete template-id ABI expression");
    result.kind = abi_mangle::ABI_EXPRESSION_TEMPLATE_ID;
    result.name = parse_source_component(words[3]);
    for(size_t i = 4; i < words.size(); ++i) {
      result.argument_refs.push_back(interner.reference(words[i]));
    }
    return result;
  }
  if(kind == "type-trait") {
    if(words.size() < 5) throw logic_error("incomplete type-trait ABI expression");
    result.kind = abi_mangle::ABI_EXPRESSION_TYPE_TRAIT;
    result.name = parse_source_component(words[3]);
    size_t at = 4;
    while(at < words.size()) result.type_arguments.push_back(
      parse_type_words(words, at, interner));
    if(result.type_arguments.empty()) throw logic_error("type-trait has no operands");
    return result;
  }
  if(kind == "sizeof-type") {
    if(words.size() < 4) throw logic_error("sizeof-type ABI expression has no type");
    result.kind = abi_mangle::ABI_EXPRESSION_SIZEOF_TYPE;
    size_t at = 3;
    result.type = parse_type_words(words, at, interner);
    require_end(words, at);
    return result;
  }
  if(kind == "member") {
    if(words.size() < 5) throw logic_error("incomplete member ABI expression");
    result.kind = abi_mangle::ABI_EXPRESSION_MEMBER;
    size_t at = 3;
    result.type = parse_type_words(words, at, interner);
    if(at >= words.size()) throw logic_error("member ABI expression has no owner-close flag");
    const string close = words[at++];
    if(close == "yes" || close == "true" || close == "1") {
      result.close_member_owner = true;
    } else if(close == "no" || close == "false" || close == "0") {
      result.close_member_owner = false;
    } else {
      throw logic_error("invalid member ABI owner-close flag");
    }
    if(at >= words.size()) {
      throw logic_error("member ABI expression has no member name");
    }
    result.name = parse_source_component(words[at++]);
    require_end(words, at);
    return result;
  }
  if(kind == "object-member") {
    if(words.size() < 6) throw logic_error("incomplete object-member ABI expression");
    result.kind = abi_mangle::ABI_EXPRESSION_OBJECT_MEMBER;
    result.member_access_kind = expression_member_access_kind(words[3]);
    result.expression_refs.push_back(interner.reference(words[4]));
    result.name = parse_source_component(words[5]);
    for(size_t i = 6; i < words.size(); ++i) {
      result.argument_refs.push_back(interner.reference(words[i]));
    }
    return result;
  }
  throw logic_error("unknown ABI expression kind '" + kind + "'");
}

abi_mangle::AbiFactRecord parse_definition_record(const vector<string> & words,
                                                  DefinitionInterner & interner)
{
  if(words.size() < 2) throw logic_error("incomplete ABI definition record");
  abi_mangle::AbiFactRecord result;
  result.kind = abi_mangle::ABI_FACT_RECORD_DEFINITION;
  result.definition.id = interner.define(words[1]);
  if(words[0] == "let-type") {
    result.definition.kind = abi_mangle::ABI_DEFINITION_TYPE;
    size_t at = 2;
    result.definition.type = parse_type_words(words, at, interner);
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
      result.definition.template_argument.type = parse_type_words(words, at, interner);
    } else if(kind == "value") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_VALUE;
      result.definition.template_argument.value_type = parse_type_words(words, at, interner);
      result.definition.template_argument.has_value_type = true;
      if(at >= words.size()) throw logic_error("missing ABI template value");
      result.definition.template_argument.value = parse_value(words[at++]);
    } else if(kind == "dependent-value") {
      result.definition.template_argument.kind =
        abi_mangle::ABI_TEMPLATE_ARGUMENT_DEPENDENT_VALUE;
      result.definition.template_argument.type = parse_type_words(words, at, interner);
      result.definition.template_argument.value_type = parse_type_words(words, at, interner);
      result.definition.template_argument.has_value_type = true;
      if(at >= words.size()) throw logic_error("missing dependent ABI template value");
      result.definition.template_argument.value = parse_value(words[at++]);
    } else if(kind == "expression") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_EXPRESSION;
      if(at >= words.size()) throw logic_error("missing ABI expression argument reference");
      result.definition.template_argument.entity_ref = interner.reference(words[at++]);
    } else if(kind == "template-param-template") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_TEMPLATE_PARAMETER_TEMPLATE;
      if(at >= words.size()) throw logic_error("missing template-template parameter index");
      result.definition.template_argument.index = parse_index(words[at++]);
    } else if(kind == "entity-address") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_ENTITY;
      if(at >= words.size()) throw logic_error("missing entity reference");
      result.definition.template_argument.entity_ref = interner.reference(words[at++]);
      result.definition.template_argument.address_of = true;
    } else if(kind == "member-external-address") {
      result.definition.template_argument.kind =
        abi_mangle::ABI_TEMPLATE_ARGUMENT_MEMBER_EXTERNAL_ENTITY;
      if(at >= words.size()) throw logic_error("missing member external symbol");
      result.definition.template_argument.symbol = words[at++];
      if(result.definition.template_argument.symbol.empty() ||
         result.definition.template_argument.symbol == "-") {
        throw logic_error("member external symbol is empty");
      }
      result.definition.template_argument.owner_type = parse_type_words(words, at, interner);
      if(at >= words.size()) throw logic_error("missing member external name");
      result.definition.template_argument.name = parse_source_component(words[at++]);
      if(at + 5 >= words.size()) throw logic_error("incomplete member external function facts");
      result.definition.template_argument.member_is_function =
        parse_boolean_word(words[at++], "member function");
      result.definition.template_argument.member_function_const =
        parse_boolean_word(words[at++], "member const");
      result.definition.template_argument.member_function_volatile =
        parse_boolean_word(words[at++], "member volatile");
      result.definition.template_argument.member_function_lvalue_ref =
        parse_boolean_word(words[at++], "member lvalue-ref");
      result.definition.template_argument.member_function_rvalue_ref =
        parse_boolean_word(words[at++], "member rvalue-ref");
      result.definition.template_argument.member_function_variadic =
        parse_boolean_word(words[at++], "member variadic");
      result.definition.template_argument.address_of = true;
      while(at < words.size()) {
        result.definition.template_argument.parameter_types.push_back(
          parse_type_words(words, at, interner));
      }
      if(!result.definition.template_argument.member_is_function &&
         (result.definition.template_argument.member_function_const ||
          result.definition.template_argument.member_function_volatile ||
          result.definition.template_argument.member_function_lvalue_ref ||
          result.definition.template_argument.member_function_rvalue_ref ||
          result.definition.template_argument.member_function_variadic ||
          !result.definition.template_argument.parameter_types.empty())) {
        throw logic_error("data member external address has function facts");
      }
      if(result.definition.template_argument.member_function_lvalue_ref &&
         result.definition.template_argument.member_function_rvalue_ref) {
        throw logic_error("member external function has conflicting ref qualifiers");
      }
    } else if(kind == "template-entity") {
      result.definition.template_argument.kind = abi_mangle::ABI_TEMPLATE_ARGUMENT_TEMPLATE_ENTITY;
      if(at >= words.size()) throw logic_error("missing template entity name");
      result.definition.template_argument.name = parse_qualified_name(words[at++]);
    } else if(kind == "member-template-entity") {
      result.definition.template_argument.kind =
        abi_mangle::ABI_TEMPLATE_ARGUMENT_MEMBER_TEMPLATE_ENTITY;
      result.definition.template_argument.owner_type = parse_type_words(words, at, interner);
      if(at >= words.size()) throw logic_error("missing member template name");
      result.definition.template_argument.name = parse_source_component(words[at++]);
      if(at >= words.size()) throw logic_error("missing member template substitution");
      result.definition.template_argument.substitution = words[at++];
      if(result.definition.template_argument.substitution == "-") {
        throw logic_error("member template substitution metadata is empty");
      }
    } else {
      throw logic_error("unknown ABI template argument kind '" + kind + "'");
    }
    require_end(words, at);
    return result;
  }
  if(words[0] == "let-expr") {
    result.definition.kind = abi_mangle::ABI_DEFINITION_EXPRESSION;
    result.definition.expression = parse_dependent_expression(words, interner);
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
      size_t function_at = 3;
      if(function_at < words.size() && words[function_at] == "path") ++function_at;
      result.definition.context.function = parse_function_path(words, function_at, false, interner);
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
      if(words[3] == "-") throw logic_error("external entity symbol is empty");
      result.definition.entity.name = parse_qualified_name(words[3]);
      require_end(words, 4);
    } else if(entity_kind == "variable" || entity_kind == "internal-variable") {
      result.definition.entity.kind = abi_mangle::ABI_ENTITY_FACT_VARIABLE;
      result.definition.entity.name = parse_qualified_name(words[3]);
      result.definition.entity.internal_linkage = entity_kind == "internal-variable";
      require_end(words, 4);
    } else if(entity_kind == "function") {
      result.definition.entity.kind = abi_mangle::ABI_ENTITY_FACT_FUNCTION;
      result.definition.entity.function = parse_function_path(words, 3, false, interner);
    } else {
      throw logic_error("unknown ABI entity kind '" + entity_kind + "'");
    }
    return result;
  }
  throw logic_error("unknown ABI definition record '" + words[0] + "'");
}

void resolve_definition_ref(abi_mangle::AbiDefinitionId & ref,
                           const DefinitionInterner & interner)
{
  if(ref.index == abi_mangle::ABI_INVALID_DEFINITION_ID) return;
  if(!interner.is_defined(ref.index)) {
    throw logic_error("unknown ABI definition id '" + interner.spelling(ref.index) + "'");
  }
}

void canonicalize_type(abi_mangle::AbiType & type,
                       const DefinitionInterner & interner)
{
  if(type.kind == abi_mangle::ABI_TYPE_NAME_OR_REFERENCE &&
     type.definition_ref.index == abi_mangle::ABI_INVALID_DEFINITION_ID &&
     type.name.components.size() == 1) {
    map<string, size_t>::const_iterator found = interner.indexes.find(type.name.components[0]);
    if(found != interner.indexes.end() && interner.is_defined(found->second)) {
      type.definition_ref.index = found->second;
      type.name.components.clear();
    }
  }
  resolve_definition_ref(type.definition_ref, interner);
  resolve_definition_ref(type.expression_ref, interner);
  resolve_definition_ref(type.context_ref, interner);
  if(type.array_bound.kind == abi_mangle::ABI_ARRAY_BOUND_EXPRESSION) {
    resolve_definition_ref(type.array_bound.expression_ref, interner);
  }
  for(vector<abi_mangle::AbiType>::iterator it = type.types.begin();
      it != type.types.end(); ++it) {
    canonicalize_type(*it, interner);
  }
  for(vector<abi_mangle::AbiDefinitionId>::iterator it = type.argument_refs.begin();
      it != type.argument_refs.end(); ++it) {
    resolve_definition_ref(*it, interner);
  }
}

void canonicalize_function_target(abi_mangle::AbiFunctionTarget & target,
                                  const DefinitionInterner & interner)
{
  resolve_definition_ref(target.context_ref, interner);
  for(vector<abi_mangle::AbiFunctionPathOperand>::iterator it = target.path_operands.begin();
      it != target.path_operands.end(); ++it) {
    resolve_definition_ref(it->argument_ref, interner);
    canonicalize_type(it->type, interner);
  }
  for(vector<abi_mangle::AbiType>::iterator it = target.signature_parameter_types.begin();
      it != target.signature_parameter_types.end(); ++it) {
    canonicalize_type(*it, interner);
  }
}

void canonicalize_argument(abi_mangle::AbiTemplateArgument & argument,
                           const DefinitionInterner & interner)
{
  canonicalize_type(argument.type, interner);
  canonicalize_type(argument.value_type, interner);
  canonicalize_type(argument.owner_type, interner);
  resolve_definition_ref(argument.entity_ref, interner);
  for(vector<abi_mangle::AbiType>::iterator it = argument.parameter_types.begin();
      it != argument.parameter_types.end(); ++it) {
    canonicalize_type(*it, interner);
  }
  for(vector<abi_mangle::AbiDefinitionId>::iterator it = argument.argument_refs.begin();
      it != argument.argument_refs.end(); ++it) {
    resolve_definition_ref(*it, interner);
  }
}

void canonicalize_expression(abi_mangle::AbiDependentExpression & expression,
                             const DefinitionInterner & interner)
{
  canonicalize_type(expression.type, interner);
  canonicalize_type(expression.value_type, interner);
  resolve_definition_ref(expression.entity_ref, interner);
  for(vector<abi_mangle::AbiDefinitionId>::iterator it = expression.expression_refs.begin();
      it != expression.expression_refs.end(); ++it) {
    resolve_definition_ref(*it, interner);
  }
  for(vector<abi_mangle::AbiDefinitionId>::iterator it = expression.argument_refs.begin();
      it != expression.argument_refs.end(); ++it) {
    resolve_definition_ref(*it, interner);
  }
  for(vector<abi_mangle::AbiType>::iterator it = expression.type_arguments.begin();
      it != expression.type_arguments.end(); ++it) {
    canonicalize_type(*it, interner);
  }
}

void canonicalize_case(abi_mangle::AbiFactCase & fact_case,
                       const DefinitionInterner & interner)
{
  for(vector<abi_mangle::AbiFactRecord>::iterator it = fact_case.records.begin();
      it != fact_case.records.end(); ++it) {
    if(it->kind == abi_mangle::ABI_FACT_RECORD_DEFINITION) {
      abi_mangle::AbiDefinitionRecord & definition = it->definition;
      if(definition.kind == abi_mangle::ABI_DEFINITION_TYPE) {
        canonicalize_type(definition.type, interner);
      } else if(definition.kind == abi_mangle::ABI_DEFINITION_TEMPLATE_ARGUMENT) {
        canonicalize_argument(definition.template_argument, interner);
      } else if(definition.kind == abi_mangle::ABI_DEFINITION_EXPRESSION) {
        canonicalize_expression(definition.expression, interner);
      } else if(definition.kind == abi_mangle::ABI_DEFINITION_CONTEXT) {
        if(definition.context.kind == abi_mangle::ABI_CONTEXT_FUNCTION) {
          canonicalize_function_target(definition.context.function, interner);
        }
      } else if(definition.kind == abi_mangle::ABI_DEFINITION_ENTITY &&
                definition.entity.kind == abi_mangle::ABI_ENTITY_FACT_FUNCTION) {
        canonicalize_function_target(definition.entity.function, interner);
      }
    } else if(it->kind == abi_mangle::ABI_FACT_RECORD_TARGET) {
      canonicalize_type(it->target.type, interner);
      canonicalize_type(it->target.base_type, interner);
      canonicalize_function_target(it->target.function, interner);
    } else {
      canonicalize_type(it->function.type, interner);
      if(it->function.has_conversion_type) {
        canonicalize_type(it->function.conversion_type, interner);
      }
      resolve_definition_ref(it->function.context_ref, interner);
      for(vector<abi_mangle::AbiDefinitionId>::iterator ref = it->function.argument_refs.begin();
          ref != it->function.argument_refs.end(); ++ref) {
        resolve_definition_ref(*ref, interner);
      }
      for(vector<abi_mangle::AbiType>::iterator type = it->function.types.begin();
          type != it->function.types.end(); ++type) {
        canonicalize_type(*type, interner);
      }
    }
  }
}

}  // namespace

namespace abi_mangle {

AbiFactRecord parse_fact_record_words_with_context(const vector<string> & words,
                                                   DefinitionInterner & interner)
{
  if(words.empty()) throw logic_error("empty ABI fact record");
  if(words[0].compare(0, 4, "let-") == 0) {
    return parse_definition_record(words, interner);
  }
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
    result.function = parse_function_record(words, interner);
    return result;
  }

  AbiFactRecord result;
  result.kind = ABI_FACT_RECORD_TARGET;
  if(words[0] == "type") {
    result.target.kind = ABI_TARGET_FACT_TYPE;
    size_t at = 1;
    result.target.type = parse_type_words(words, at, interner);
    require_end(words, at);
  } else if(words[0] == "function") {
    result.target.kind = ABI_TARGET_FACT_FUNCTION;
    if(words.size() < 2) throw logic_error("incomplete ABI function target");
    if(words[1] == "encoding") {
      require_end(words, 2);
      result.target.function.kind = ABI_FUNCTION_TARGET_ENCODING;
    } else if(words[1] == "path") {
      result.target.function = parse_function_path(words, 2, true, interner);
    } else if(words[1] == "namespace-lambda") {
      if(words.size() < 4) throw logic_error("incomplete namespace lambda function target");
      result.target.function.kind = ABI_FUNCTION_TARGET_NAMESPACE_LAMBDA;
      result.target.function.source_name = parse_source_component(words[2]).components[0];
      result.target.function.special_terminal = special_terminal_kind(words[3]);
      if(words[3] == "operator-call" || words[3] == "call") {
        result.target.function.operator_terminal = ABI_OPERATOR_TERMINAL_CALL;
      }
      if(result.target.function.special_terminal == ABI_SPECIAL_TERMINAL_NONE &&
         result.target.function.operator_terminal == ABI_OPERATOR_TERMINAL_NONE) {
        result.target.function.terminal = words[3];
      }
      for(size_t i = 4; i < words.size(); ++i) result.target.function.namespace_qualifiers.push_back(words[i]);
    } else if(words[1] == "local" || words[1] == "lambda") {
      if(words.size() < 5) {
        throw logic_error(words[1] == "lambda" ?
                          "incomplete lambda function target" :
                          "incomplete local function target");
      }
      result.target.function.kind = words[1] == "local" ? ABI_FUNCTION_TARGET_LOCAL : ABI_FUNCTION_TARGET_LAMBDA;
      result.target.function.context_ref = interner.reference(words[2]);
      if(words[1] == "lambda") {
        result.target.function.discriminator = words[3];
        result.target.function.special_terminal = special_terminal_kind(words[4]);
        if(words[4] == "operator-call" || words[4] == "call") {
          result.target.function.operator_terminal = ABI_OPERATOR_TERMINAL_CALL;
        }
        if(result.target.function.special_terminal == ABI_SPECIAL_TERMINAL_NONE &&
           result.target.function.operator_terminal == ABI_OPERATOR_TERMINAL_NONE) {
          result.target.function.terminal = words[4];
        }
        for(size_t i = 5; i < words.size(); ++i) {
          size_t at = i;
          result.target.function.signature_parameter_types.push_back(parse_type_words(words, at, interner));
          i = at - 1;
        }
      } else {
        result.target.function.source_name = parse_source_component(words[3]).components[0];
        result.target.function.special_terminal = special_terminal_kind(words[4]);
        if(words[4] == "operator-call" || words[4] == "call") {
          result.target.function.operator_terminal = ABI_OPERATOR_TERMINAL_CALL;
        }
        if(result.target.function.special_terminal == ABI_SPECIAL_TERMINAL_NONE &&
           result.target.function.operator_terminal == ABI_OPERATOR_TERMINAL_NONE) {
          result.target.function.terminal = words[4];
        }
        if(words.size() >= 6) result.target.function.discriminator = words[5];
        for(size_t i = 6; i < words.size(); ++i) {
          size_t at = i;
          result.target.function.signature_parameter_types.push_back(parse_type_words(words, at, interner));
          i = at - 1;
        }
      }
    } else {
      result.target.function = parse_function_path(words, 1, false, interner);
    }
  } else if(words[0] == "c-function") {
    if(words.size() < 2) throw logic_error("incomplete C function target");
    result.target.kind = ABI_TARGET_FACT_FUNCTION;
    result.target.linkage = abi_mangle::ABI_LINKAGE_C;
    result.target.function = parse_function_path(words, 1, false, interner);
  } else if(words[0] == "variable") {
    if(words.size() != 2) throw logic_error("invalid ABI variable target");
    result.target.kind = ABI_TARGET_FACT_VARIABLE;
    result.target.name = parse_qualified_name(words[1]);
  } else if(words[0] == "typeinfo" || words[0] == "vtable" || words[0] == "vtt") {
    if(words.size() < 2) throw logic_error("incomplete ABI special target");
    result.target.kind = words[0] == "typeinfo" ? ABI_TARGET_FACT_TYPEINFO :
      (words[0] == "vtable" ? ABI_TARGET_FACT_VTABLE : ABI_TARGET_FACT_VTT);
    size_t at = 1;
    result.target.type = parse_type_words(words, at, interner);
    require_end(words, at);
  } else if(words[0] == "construction-vtable") {
    if(words.size() < 4) throw logic_error("incomplete construction vtable target");
    result.target.kind = ABI_TARGET_FACT_CONSTRUCTION_VTABLE;
    size_t at = 1;
    result.target.type = parse_type_words(words, at, interner);
    if(at >= words.size()) throw logic_error("missing construction vtable offset");
    result.target.base_offset = parse_index(words[at++]);
    result.target.base_type = parse_type_words(words, at, interner);
    require_end(words, at);
  } else if(words[0] == "tls-wrapper") {
    if(words.size() != 3 || words[1] != "variable") throw logic_error("invalid TLS wrapper target");
    result.target.kind = ABI_TARGET_FACT_THREAD_LOCAL_WRAPPER;
    result.target.name = parse_qualified_name(words[2]);
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
    result.target.function = parse_function_path(function_words, 1, true, interner);
  } else {
    throw logic_error("unknown ABI target record '" + words[0] + "'");
  }
  return result;
}

AbiFactRecord parse_fact_record_words(const vector<string> & words)
{
  DefinitionInterner interner;
  return parse_fact_record_words_with_context(words, interner);
}

AbiFactFile parse_fact_text(const string & text)
{
  AbiFactFile result;
  AbiFactCase current;
  DefinitionInterner interner;
  const auto finish_case = [&]() {
    if(current.records.empty()) return;
    canonicalize_case(current, interner);
    current.definition_labels = interner.labels;
    result.cases.push_back(std::move(current));
    current = AbiFactCase();
    interner = DefinitionInterner();
  };
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
      finish_case();
      continue;
    }
    if(words[0] == "case") {
      if(words.size() != 2) throw logic_error("case requires one label");
      finish_case();
      current.label = words[1];
      continue;
    }
    AbiFactRecord record = parse_fact_record_words_with_context(words, interner);
    current.records.push_back(std::move(record));
  }
  finish_case();
  if(result.cases.empty()) throw logic_error("ABI fact file has no cases");
  return result;
}

string join_qualified_name(const AbiQualifiedName & name)
{
  if(name.components.empty()) throw logic_error("empty ABI name");
  string result;
  for(vector<string>::const_iterator it = name.components.begin();
      it != name.components.end(); ++it) {
    if(it != name.components.begin()) result += "::";
    result += *it;
  }
  return result;
}

string builtin_spelling(AbiBuiltinKind kind)
{
  static const char * const names[] = {
    "", "void", "wchar", "bool", "char", "schar", "uchar", "short",
    "ushort", "int", "uint", "long", "ulong", "longlong", "ulonglong",
    "int128", "uint128", "float", "double", "longdouble", "float128",
    "ellipsis", "char16", "char32", "char8", "nullptr", "complex-float",
    "complex-double", "complex-longdouble"
  };
  const size_t index = static_cast<size_t>(kind);
  if(index >= sizeof(names) / sizeof(names[0]) || kind == ABI_BUILTIN_INVALID) {
    throw logic_error("invalid ABI builtin kind");
  }
  return names[index];
}

string definition_ref_spelling(const AbiFactCase & fact_case,
                               const AbiDefinitionId & ref)
{
  if(ref.index < fact_case.definition_labels.size() &&
     !fact_case.definition_labels[ref.index].empty()) {
    return fact_case.definition_labels[ref.index];
  }
  ostringstream index;
  index << ref.index;
  return "D" + index.str();
}

void append_type_words(const AbiFactCase & fact_case,
                       const AbiType & type,
                       vector<string> & words);

string join_words(const vector<string> & words)
{
  ostringstream output;
  for(vector<string>::const_iterator it = words.begin(); it != words.end(); ++it) {
    if(it != words.begin()) output << " ";
    output << *it;
  }
  return output.str();
}

string standard_substitution_spelling(AbiStandardSubstitutionKind kind)
{
  switch(kind) {
  case ABI_STANDARD_SUBSTITUTION_ALLOCATOR: return "Sa";
  case ABI_STANDARD_SUBSTITUTION_BASIC_STRING: return "Sb";
  case ABI_STANDARD_SUBSTITUTION_STRING: return "Ss";
  case ABI_STANDARD_SUBSTITUTION_ISTREAM: return "Si";
  case ABI_STANDARD_SUBSTITUTION_OSTREAM: return "So";
  case ABI_STANDARD_SUBSTITUTION_IOSTREAM: return "Sd";
  case ABI_STANDARD_SUBSTITUTION_NONE: break;
  }
  throw logic_error("missing ABI standard substitution spelling");
}

void append_type_words(const AbiFactCase & fact_case,
                       const AbiType & type,
                       vector<string> & words)
{
  // `tagged` is a wrapper in the input grammar, while the typed model keeps
  // its tags on the underlying type.  Strip them only while serializing the
  // base so the result parses back to the same typed shape.
  if(!type.abi_tags.empty()) {
    words.push_back("tagged");
    AbiType base = type;
    base.abi_tags.clear();
    append_type_words(fact_case, base, words);
    words.insert(words.end(), type.abi_tags.begin(), type.abi_tags.end());
    return;
  }
  ostringstream value;
  switch(type.kind) {
  case ABI_TYPE_BUILTIN:
    words.push_back(builtin_spelling(type.builtin));
    return;
  case ABI_TYPE_NAMED:
    words.push_back("named");
    words.push_back(join_qualified_name(type.name));
    return;
  case ABI_TYPE_NAME_OR_REFERENCE:
    if(type.definition_ref.index != ABI_INVALID_DEFINITION_ID) {
      words.push_back(definition_ref_spelling(fact_case, type.definition_ref));
    } else {
      words.push_back(join_qualified_name(type.name));
    }
    return;
  case ABI_TYPE_TEMPLATE_PARAMETER:
    words.push_back("template-param");
    value << type.index;
    words.push_back(value.str());
    return;
  case ABI_TYPE_POINTER:
    if(type.types.size() != 1) throw logic_error("pointer ABI type is incomplete");
    words.push_back("ptr");
    append_type_words(fact_case, type.types[0], words);
    return;
  case ABI_TYPE_LVALUE_REFERENCE:
    if(type.types.size() != 1) throw logic_error("lvalue-reference ABI type is incomplete");
    words.push_back("ref");
    append_type_words(fact_case, type.types[0], words);
    return;
  case ABI_TYPE_RVALUE_REFERENCE:
    if(type.types.size() != 1) throw logic_error("rvalue-reference ABI type is incomplete");
    words.push_back("rref");
    append_type_words(fact_case, type.types[0], words);
    return;
  case ABI_TYPE_CV:
    if(type.types.size() != 1 || (type.is_const == type.is_volatile)) {
      throw logic_error("cv ABI type is incomplete");
    }
    words.push_back(type.is_const ? "const" : "volatile");
    append_type_words(fact_case, type.types[0], words);
    return;
  case ABI_TYPE_PACK_EXPANSION:
    if(type.types.size() != 1) throw logic_error("pack ABI type is incomplete");
    words.push_back("pack");
    append_type_words(fact_case, type.types[0], words);
    return;
  case ABI_TYPE_VENDOR_QUALIFIED:
    if(type.name.components.size() != 1 || type.types.size() != 1) {
      throw logic_error("vendor-qualified ABI type is incomplete");
    }
    words.push_back("vendor");
    words.push_back(type.name.components[0]);
    append_type_words(fact_case, type.types[0], words);
    return;
  case ABI_TYPE_ARRAY:
    if(type.array_bound.kind != ABI_ARRAY_BOUND_VALUE || type.types.size() != 1) {
      throw logic_error("unsupported array ABI bound in serializer");
    }
    words.push_back("array");
    value << type.array_bound.value;
    words.push_back(value.str());
    append_type_words(fact_case, type.types[0], words);
    return;
  case ABI_TYPE_BUILTIN_TRANSFORM:
    if(type.name.components.size() != 1 || type.types.size() != 1) {
      throw logic_error("builtin-transform ABI type is incomplete");
    }
    words.push_back("builtin-transform");
    words.push_back(type.name.components[0]);
    append_type_words(fact_case, type.types[0], words);
    return;
  case ABI_TYPE_FUNCTION:
    if(type.types.empty()) throw logic_error("function ABI type has no result");
    words.push_back(type.variadic ? "function-type-variadic" : "function-type");
    for(vector<AbiType>::const_iterator it = type.types.begin();
        it != type.types.end(); ++it) {
      append_type_words(fact_case, *it, words);
    }
    return;
  case ABI_TYPE_MEMBER_POINTER:
    if(type.types.size() != 2) throw logic_error("member-pointer ABI type is incomplete");
    words.push_back("member-pointer");
    append_type_words(fact_case, type.types[0], words);
    append_type_words(fact_case, type.types[1], words);
    return;
  case ABI_TYPE_TEMPLATE_SPECIALIZATION:
    if(type.name.components.empty()) throw logic_error("template ABI type has no name");
    words.push_back("template");
    words.push_back(join_qualified_name(type.name));
    for(vector<AbiDefinitionId>::const_iterator it = type.argument_refs.begin();
        it != type.argument_refs.end(); ++it) {
      words.push_back(definition_ref_spelling(fact_case, *it));
    }
    return;
  case ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION:
    throw logic_error("template-parameter specialization serializer is outside PA14's touched boundary");
  case ABI_TYPE_STD_TEMPLATE_SPECIALIZATION:
    if(type.standard_substitution_kind == ABI_STANDARD_SUBSTITUTION_NONE ||
       type.name.components.empty()) {
      throw logic_error("standard template ABI type lacks typed substitution metadata");
    }
    words.push_back("std-template");
    words.push_back(type.standard_substitution.empty() ?
                    standard_substitution_spelling(type.standard_substitution_kind) :
                    type.standard_substitution);
    words.push_back(type.standard_substitution_includes_arguments ? "yes" : "no");
    words.push_back(join_qualified_name(type.name));
    for(vector<AbiDefinitionId>::const_iterator it = type.argument_refs.begin();
        it != type.argument_refs.end(); ++it) {
      words.push_back(definition_ref_spelling(fact_case, *it));
    }
    return;
  case ABI_TYPE_MEMBER:
    if(type.types.size() != 1 || type.name.components.size() != 1) {
      throw logic_error("member ABI type is incomplete");
    }
    words.push_back("member");
    append_type_words(fact_case, type.types[0], words);
    words.push_back(type.name.components[0]);
    return;
  case ABI_TYPE_MEMBER_TEMPLATE_SPECIALIZATION:
    if(type.types.size() != 1 || type.name.components.size() != 1) {
      throw logic_error("member-template ABI type is incomplete");
    }
    words.push_back("member-template");
    append_type_words(fact_case, type.types[0], words);
    words.push_back(type.name.components[0]);
    for(vector<AbiDefinitionId>::const_iterator it = type.argument_refs.begin();
        it != type.argument_refs.end(); ++it) {
      words.push_back(definition_ref_spelling(fact_case, *it));
    }
    return;
  case ABI_TYPE_DECLTYPE_EXPRESSION:
    words.push_back(type.decltype_kind == ABI_DECLTYPE_ID_OR_MEMBER ?
                    "decltype-id" : "decltype");
    words.push_back(definition_ref_spelling(fact_case, type.expression_ref));
    return;
  case ABI_TYPE_LAMBDA_CLOSURE:
    words.push_back("lambda-closure");
    words.push_back(definition_ref_spelling(fact_case, type.context_ref));
    words.push_back(type.discriminator);
    return;
  case ABI_TYPE_LOCAL_TYPE:
    if(type.name.components.size() != 1) throw logic_error("local ABI type has no source component");
    words.push_back("local-type");
    words.push_back(definition_ref_spelling(fact_case, type.context_ref));
    words.push_back(type.name.components[0]);
    if(!type.discriminator.empty()) words.push_back(type.discriminator);
    return;
  case ABI_TYPE_NAMESPACE_LAMBDA:
    if(type.name.components.size() != 1) throw logic_error("namespace lambda ABI type has no source component");
    words.push_back("namespace-lambda");
    words.push_back(type.name.components[0]);
    words.insert(words.end(), type.namespace_qualifiers.begin(),
                 type.namespace_qualifiers.end());
    return;
  }
  throw logic_error("unknown ABI type kind in serializer");
}

string type_spelling(const AbiFactCase & fact_case, const AbiType & type)
{
  vector<string> words;
  append_type_words(fact_case, type, words);
  return join_words(words);
}

string expression_operator_spelling(AbiExpressionOperatorKind kind)
{
  switch(kind) {
  case ABI_EXPRESSION_OPERATOR_NEW: return "nw";
  case ABI_EXPRESSION_OPERATOR_NEW_ARRAY: return "na";
  case ABI_EXPRESSION_OPERATOR_DELETE: return "dl";
  case ABI_EXPRESSION_OPERATOR_DELETE_ARRAY: return "da";
  case ABI_EXPRESSION_OPERATOR_AWAIT: return "aw";
  case ABI_EXPRESSION_OPERATOR_UNARY_PLUS: return "ps";
  case ABI_EXPRESSION_OPERATOR_UNARY_MINUS: return "ng";
  case ABI_EXPRESSION_OPERATOR_ADDRESS_OF: return "ad";
  case ABI_EXPRESSION_OPERATOR_DEREF: return "de";
  case ABI_EXPRESSION_OPERATOR_COMPLEMENT: return "co";
  case ABI_EXPRESSION_OPERATOR_PLUS: return "pl";
  case ABI_EXPRESSION_OPERATOR_MINUS: return "mi";
  case ABI_EXPRESSION_OPERATOR_MULTIPLY: return "ml";
  case ABI_EXPRESSION_OPERATOR_DIVIDE: return "dv";
  case ABI_EXPRESSION_OPERATOR_REMAINDER: return "rm";
  case ABI_EXPRESSION_OPERATOR_BIT_AND: return "an";
  case ABI_EXPRESSION_OPERATOR_BIT_OR: return "or";
  case ABI_EXPRESSION_OPERATOR_BIT_XOR: return "eo";
  case ABI_EXPRESSION_OPERATOR_ASSIGN: return "aS";
  case ABI_EXPRESSION_OPERATOR_PLUS_ASSIGN: return "pL";
  case ABI_EXPRESSION_OPERATOR_MINUS_ASSIGN: return "mI";
  case ABI_EXPRESSION_OPERATOR_MULTIPLY_ASSIGN: return "mL";
  case ABI_EXPRESSION_OPERATOR_DIVIDE_ASSIGN: return "dV";
  case ABI_EXPRESSION_OPERATOR_REMAINDER_ASSIGN: return "rM";
  case ABI_EXPRESSION_OPERATOR_BIT_AND_ASSIGN: return "aN";
  case ABI_EXPRESSION_OPERATOR_BIT_OR_ASSIGN: return "oR";
  case ABI_EXPRESSION_OPERATOR_BIT_XOR_ASSIGN: return "eO";
  case ABI_EXPRESSION_OPERATOR_LEFT_SHIFT: return "ls";
  case ABI_EXPRESSION_OPERATOR_RIGHT_SHIFT: return "rs";
  case ABI_EXPRESSION_OPERATOR_LEFT_SHIFT_ASSIGN: return "lS";
  case ABI_EXPRESSION_OPERATOR_RIGHT_SHIFT_ASSIGN: return "rS";
  case ABI_EXPRESSION_OPERATOR_EQUAL: return "eq";
  case ABI_EXPRESSION_OPERATOR_NOT_EQUAL: return "ne";
  case ABI_EXPRESSION_OPERATOR_LESS: return "lt";
  case ABI_EXPRESSION_OPERATOR_GREATER: return "gt";
  case ABI_EXPRESSION_OPERATOR_LESS_EQUAL: return "le";
  case ABI_EXPRESSION_OPERATOR_GREATER_EQUAL: return "ge";
  case ABI_EXPRESSION_OPERATOR_SPACESHIP: return "ss";
  case ABI_EXPRESSION_OPERATOR_LOGICAL_NOT: return "nt";
  case ABI_EXPRESSION_OPERATOR_LOGICAL_AND: return "aa";
  case ABI_EXPRESSION_OPERATOR_LOGICAL_OR: return "oo";
  case ABI_EXPRESSION_OPERATOR_INCREMENT: return "pp";
  case ABI_EXPRESSION_OPERATOR_DECREMENT: return "mm";
  case ABI_EXPRESSION_OPERATOR_COMMA: return "cm";
  case ABI_EXPRESSION_OPERATOR_MEMBER_POINTER: return "pm";
  case ABI_EXPRESSION_OPERATOR_ARROW: return "pt";
  case ABI_EXPRESSION_OPERATOR_CALL: return "cl";
  case ABI_EXPRESSION_OPERATOR_INDEX: return "ix";
  case ABI_EXPRESSION_OPERATOR_CONDITIONAL: return "qu";
  case ABI_EXPRESSION_OPERATOR_NONE: break;
  }
  throw logic_error("missing ABI expression operator spelling");
}

string expression_cast_spelling(AbiExpressionCastKind kind)
{
  switch(kind) {
  case ABI_EXPRESSION_CAST_DYNAMIC: return "dc";
  case ABI_EXPRESSION_CAST_STATIC: return "sc";
  case ABI_EXPRESSION_CAST_CONST: return "cc";
  case ABI_EXPRESSION_CAST_REINTERPRET: return "rc";
  case ABI_EXPRESSION_CAST_NONE: break;
  }
  throw logic_error("missing ABI expression cast spelling");
}

string expression_member_access_spelling(AbiExpressionMemberAccessKind kind)
{
  switch(kind) {
  case ABI_EXPRESSION_MEMBER_ACCESS_DOT: return "dt";
  case ABI_EXPRESSION_MEMBER_ACCESS_ARROW: return "pt";
  case ABI_EXPRESSION_MEMBER_ACCESS_NONE: break;
  }
  throw logic_error("missing ABI member access spelling");
}

void append_expression_words(const AbiFactCase & fact_case,
                             const AbiDependentExpression & expression,
                             vector<string> & words)
{
  ostringstream value;
  switch(expression.kind) {
  case ABI_EXPRESSION_LITERAL:
    words.push_back("literal");
    if(expression.value_type.kind != ABI_TYPE_BUILTIN ||
       expression.value_type.builtin != ABI_BUILTIN_INT) {
      append_type_words(fact_case, expression.value_type, words);
    }
    value << expression.value;
    words.push_back(value.str());
    return;
  case ABI_EXPRESSION_INTEGRAL_VALUE:
    words.push_back("integral-value");
    append_type_words(fact_case, expression.value_type, words);
    value << expression.value;
    words.push_back(value.str());
    return;
  case ABI_EXPRESSION_TEMPLATE_PARAMETER:
    words.push_back("template-param");
    value << expression.index;
    words.push_back(value.str());
    return;
  case ABI_EXPRESSION_FUNCTION_PARAMETER:
    words.push_back("function-param");
    value << expression.index;
    words.push_back(value.str());
    return;
  case ABI_EXPRESSION_ENTITY:
    words.push_back("entity-reference");
    words.push_back(definition_ref_spelling(fact_case, expression.entity_ref));
    return;
  case ABI_EXPRESSION_EXTERNAL_ENTITY:
    if(expression.symbol.empty() || expression.symbol == "-") {
      throw logic_error("external entity expression has no serializable symbol");
    }
    words.push_back("external-entity");
    words.push_back(expression.symbol);
    return;
  case ABI_EXPRESSION_UNARY:
    if(expression.expression_refs.size() != 1 ||
       !is_unary_expression_operator(expression.operator_kind)) {
      throw logic_error("unary ABI expression is not a canonical unary fact");
    }
    words.push_back("unary");
    words.push_back(expression_operator_spelling(expression.operator_kind));
    words.push_back(definition_ref_spelling(fact_case, expression.expression_refs[0]));
    return;
  case ABI_EXPRESSION_BINARY:
    if(expression.expression_refs.size() != 2 ||
       !is_binary_expression_operator(expression.operator_kind)) {
      throw logic_error("binary ABI expression is not a canonical binary fact");
    }
    words.push_back("binary");
    words.push_back(expression_operator_spelling(expression.operator_kind));
    words.push_back(definition_ref_spelling(fact_case, expression.expression_refs[0]));
    words.push_back(definition_ref_spelling(fact_case, expression.expression_refs[1]));
    return;
  case ABI_EXPRESSION_CONDITIONAL:
    if(expression.expression_refs.size() != 3) {
      throw logic_error("conditional ABI expression has the wrong arity");
    }
    words.push_back("conditional");
    for(vector<AbiDefinitionId>::const_iterator it = expression.expression_refs.begin();
        it != expression.expression_refs.end(); ++it) {
      words.push_back(definition_ref_spelling(fact_case, *it));
    }
    return;
  case ABI_EXPRESSION_PACK_EXPANSION:
    if(expression.expression_refs.size() != 1) {
      throw logic_error("pack ABI expression has the wrong arity");
    }
    words.push_back("pack");
    words.push_back(definition_ref_spelling(fact_case, expression.expression_refs[0]));
    return;
  case ABI_EXPRESSION_CALL:
    if(expression.expression_refs.empty()) throw logic_error("call ABI expression has no callee");
    words.push_back("call");
    for(vector<AbiDefinitionId>::const_iterator it = expression.expression_refs.begin();
        it != expression.expression_refs.end(); ++it) {
      words.push_back(definition_ref_spelling(fact_case, *it));
    }
    return;
  case ABI_EXPRESSION_CONVERSION:
    if(expression.expression_refs.size() != 1) {
      throw logic_error("conversion ABI expression has the wrong arity");
    }
    words.push_back("conversion");
    append_type_words(fact_case, expression.type, words);
    words.push_back(definition_ref_spelling(fact_case, expression.expression_refs[0]));
    return;
  case ABI_EXPRESSION_CAST:
    if(expression.expression_refs.size() != 1) {
      throw logic_error("cast ABI expression has the wrong arity");
    }
    words.push_back("cast");
    words.push_back(expression_cast_spelling(expression.cast_kind));
    append_type_words(fact_case, expression.type, words);
    words.push_back(definition_ref_spelling(fact_case, expression.expression_refs[0]));
    return;
  case ABI_EXPRESSION_TEMPLATE_ID:
    if(expression.name.components.size() != 1 || expression.argument_refs.empty()) {
      throw logic_error("template-id ABI expression is incomplete");
    }
    words.push_back("template-id");
    words.push_back(expression.name.components[0]);
    for(vector<AbiDefinitionId>::const_iterator it = expression.argument_refs.begin();
        it != expression.argument_refs.end(); ++it) {
      words.push_back(definition_ref_spelling(fact_case, *it));
    }
    return;
  case ABI_EXPRESSION_TYPE_TRAIT:
    if(expression.name.components.size() != 1 || expression.type_arguments.empty()) {
      throw logic_error("type-trait ABI expression is incomplete");
    }
    words.push_back("type-trait");
    words.push_back(expression.name.components[0]);
    for(vector<AbiType>::const_iterator it = expression.type_arguments.begin();
        it != expression.type_arguments.end(); ++it) {
      append_type_words(fact_case, *it, words);
    }
    return;
  case ABI_EXPRESSION_SIZEOF_TYPE:
    words.push_back("sizeof-type");
    append_type_words(fact_case, expression.type, words);
    return;
  case ABI_EXPRESSION_MEMBER:
    if(expression.name.components.size() != 1) {
      throw logic_error("member ABI expression has no source component");
    }
    words.push_back("member");
    append_type_words(fact_case, expression.type, words);
    words.push_back(expression.close_member_owner ? "yes" : "no");
    words.push_back(expression.name.components[0]);
    return;
  case ABI_EXPRESSION_OBJECT_MEMBER:
    if(expression.expression_refs.size() != 1 || expression.name.components.size() != 1) {
      throw logic_error("object-member ABI expression is incomplete");
    }
    words.push_back("object-member");
    words.push_back(expression_member_access_spelling(expression.member_access_kind));
    words.push_back(definition_ref_spelling(fact_case, expression.expression_refs[0]));
    words.push_back(expression.name.components[0]);
    for(vector<AbiDefinitionId>::const_iterator it = expression.argument_refs.begin();
        it != expression.argument_refs.end(); ++it) {
      words.push_back(definition_ref_spelling(fact_case, *it));
    }
    return;
  }
  throw logic_error("unknown ABI expression kind in serializer");
}

void serialize_function_terminal(ostringstream & output,
                                 const AbiFunctionTarget & target)
{
  if(target.operator_terminal != ABI_OPERATOR_TERMINAL_NONE) {
    if(target.operator_terminal == ABI_OPERATOR_TERMINAL_LITERAL) {
      throw logic_error("cannot serialize a literal function target without its suffix");
    }
    output << operator_terminal_spelling(target.operator_terminal);
  } else if(target.special_terminal != ABI_SPECIAL_TERMINAL_NONE) {
    output << special_terminal_spelling(target.special_terminal);
  } else if(!target.terminal.empty()) {
    output << target.terminal;
  } else {
    throw logic_error("function target has no terminal");
  }
}

enum FunctionTargetSerializationMode
{
  FUNCTION_TARGET_SERIALIZE_TOP_LEVEL,
  FUNCTION_TARGET_SERIALIZE_CONTEXT
};

void serialize_function_target(ostringstream & output,
                               const AbiFactCase & fact_case,
                               const AbiFunctionTarget & target,
                               FunctionTargetSerializationMode mode)
{
  const bool context_form = mode == FUNCTION_TARGET_SERIALIZE_CONTEXT;
  if(context_form && target.kind != ABI_FUNCTION_TARGET_PATH) {
    throw logic_error("ABI function context must use a path target");
  }
  if(!context_form && target.kind != ABI_FUNCTION_TARGET_PATH) {
    output << "function ";
  }
  switch(target.kind) {
  case ABI_FUNCTION_TARGET_PATH:
    if(!target.terminal.empty() ||
       target.special_terminal != ABI_SPECIAL_TERMINAL_NONE ||
       target.operator_terminal != ABI_OPERATOR_TERMINAL_NONE ||
       !target.source_name.empty() || !target.discriminator.empty() ||
       !target.namespace_qualifiers.empty()) {
      throw logic_error("ABI path target has incompatible terminal fields");
    }
    if(!context_form && !target.path_operands.empty() &&
       !target.signature_parameter_types.empty()) {
      throw logic_error("ABI path target mixes template operands and signature parameters");
    }
    if(context_form && !target.path_operands.empty()) {
      throw logic_error("ABI function context cannot carry template path operands");
    }
    if(!context_form && target.path_operands.empty() &&
       !target.signature_parameter_types.empty()) {
      output << "function " << join_qualified_name(target.name);
    } else {
      output << "function path " << join_qualified_name(target.name);
    }
    for(vector<AbiFunctionPathOperand>::const_iterator it = target.path_operands.begin();
        it != target.path_operands.end(); ++it) {
      if(it->kind == ABI_FUNCTION_PATH_TEMPLATE_ARGUMENT) {
        output << " " << definition_ref_spelling(fact_case, it->argument_ref);
      } else {
        throw logic_error("cannot serialize unsupported ABI function path operand");
      }
    }
    if(context_form || target.path_operands.empty()) {
      for(vector<AbiType>::const_iterator it = target.signature_parameter_types.begin();
          it != target.signature_parameter_types.end(); ++it) {
        output << " " << type_spelling(fact_case, *it);
      }
    }
    break;
  case ABI_FUNCTION_TARGET_ENCODING:
    output << "encoding";
    break;
  case ABI_FUNCTION_TARGET_LOCAL:
    output << "local " << definition_ref_spelling(fact_case, target.context_ref)
           << " " << target.source_name << " ";
    serialize_function_terminal(output, target);
    output << " " << target.discriminator;
    for(vector<AbiType>::const_iterator it = target.signature_parameter_types.begin();
        it != target.signature_parameter_types.end(); ++it) {
      output << " " << type_spelling(fact_case, *it);
    }
    break;
  case ABI_FUNCTION_TARGET_LAMBDA:
    output << "lambda " << definition_ref_spelling(fact_case, target.context_ref)
           << " " << target.discriminator << " ";
    serialize_function_terminal(output, target);
    for(vector<AbiType>::const_iterator it = target.signature_parameter_types.begin();
        it != target.signature_parameter_types.end(); ++it) {
      output << " " << type_spelling(fact_case, *it);
    }
    break;
  case ABI_FUNCTION_TARGET_NAMESPACE_LAMBDA:
    output << "namespace-lambda " << target.source_name << " ";
    serialize_function_terminal(output, target);
    for(vector<string>::const_iterator it = target.namespace_qualifiers.begin();
        it != target.namespace_qualifiers.end(); ++it) {
      output << " " << *it;
    }
    break;
  }
}

void serialize_template_argument(ostringstream & output,
                                 const AbiFactCase & fact_case,
                                 const AbiTemplateArgument & argument)
{
  ostringstream value;
  switch(argument.kind) {
  case ABI_TEMPLATE_ARGUMENT_TYPE:
    output << "type " << type_spelling(fact_case, argument.type);
    return;
  case ABI_TEMPLATE_ARGUMENT_VALUE:
    if(!argument.has_value_type) throw logic_error("typed ABI value has no value type");
    value << argument.value;
    output << "value " << type_spelling(fact_case, argument.value_type)
           << " " << value.str();
    return;
  case ABI_TEMPLATE_ARGUMENT_DEPENDENT_VALUE:
    if(!argument.has_value_type) throw logic_error("dependent ABI value has no value type");
    value << argument.value;
    output << "dependent-value " << type_spelling(fact_case, argument.type)
           << " " << type_spelling(fact_case, argument.value_type)
           << " " << value.str();
    return;
  case ABI_TEMPLATE_ARGUMENT_EXPRESSION:
    output << "expression " << definition_ref_spelling(fact_case, argument.entity_ref);
    return;
  case ABI_TEMPLATE_ARGUMENT_TEMPLATE_PARAMETER_TEMPLATE:
    output << "template-param-template " << argument.index;
    return;
  case ABI_TEMPLATE_ARGUMENT_ENTITY:
    if(!argument.address_of) throw logic_error("unaddressed entity argument has no canonical fact spelling");
    output << "entity-address " << definition_ref_spelling(fact_case, argument.entity_ref);
    return;
  case ABI_TEMPLATE_ARGUMENT_MEMBER_EXTERNAL_ENTITY:
    if(argument.symbol.empty() || argument.symbol == "-") {
      throw logic_error("member external argument has no symbol");
    }
    output << "member-external-address " << argument.symbol << " "
           << type_spelling(fact_case, argument.owner_type) << " "
           << join_qualified_name(argument.name) << " "
           << (argument.member_is_function ? "yes" : "no") << " "
           << (argument.member_function_const ? "yes" : "no") << " "
           << (argument.member_function_volatile ? "yes" : "no") << " "
           << (argument.member_function_lvalue_ref ? "yes" : "no") << " "
           << (argument.member_function_rvalue_ref ? "yes" : "no") << " "
           << (argument.member_function_variadic ? "yes" : "no");
    for(vector<AbiType>::const_iterator it = argument.parameter_types.begin();
        it != argument.parameter_types.end(); ++it) {
      output << " " << type_spelling(fact_case, *it);
    }
    return;
  case ABI_TEMPLATE_ARGUMENT_TEMPLATE_ENTITY:
    output << "template-entity " << join_qualified_name(argument.name);
    return;
  case ABI_TEMPLATE_ARGUMENT_MEMBER_TEMPLATE_ENTITY:
    if(argument.name.components.size() != 1 || argument.substitution.empty() ||
       argument.substitution == "-") {
      throw logic_error("member template argument is incomplete");
    }
    output << "member-template-entity " << type_spelling(fact_case, argument.owner_type)
           << " " << argument.name.components[0] << " " << argument.substitution;
    return;
  case ABI_TEMPLATE_ARGUMENT_EXTERNAL_ENTITY:
    throw logic_error("external entity argument has no supported normalized serializer form");
  case ABI_TEMPLATE_ARGUMENT_UNTYPED_VALUE:
    throw logic_error("untyped value argument has no supported normalized serializer form");
  case ABI_TEMPLATE_ARGUMENT_PACK:
    throw logic_error("template argument pack has no supported normalized serializer form");
  }
  throw logic_error("unknown ABI template argument kind in serializer");
}

void serialize_entity_definition(ostringstream & output,
                                 const AbiFactCase & fact_case,
                                 const AbiEntityFact & entity)
{
  if(entity.kind == ABI_ENTITY_FACT_SYMBOL) {
    output << "symbol " << join_qualified_name(entity.name);
    return;
  }
  if(entity.kind == ABI_ENTITY_FACT_VARIABLE) {
    output << (entity.internal_linkage ? "internal-variable " : "variable ")
           << join_qualified_name(entity.name);
    return;
  }
  if(entity.kind == ABI_ENTITY_FACT_FUNCTION) {
    if(entity.function.kind != ABI_FUNCTION_TARGET_PATH ||
       !entity.function.path_operands.empty()) {
      throw logic_error("entity function serializer requires a plain function path");
    }
    output << "function " << join_qualified_name(entity.function.name);
    for(vector<AbiType>::const_iterator it = entity.function.signature_parameter_types.begin();
        it != entity.function.signature_parameter_types.end(); ++it) {
      output << " " << type_spelling(fact_case, *it);
    }
    return;
  }
  throw logic_error("unknown ABI entity kind in serializer");
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
        const string label = definition_ref_spelling(*c, d.id);
        if(d.kind == ABI_DEFINITION_TYPE) {
          output << "let-type " << label << " " << type_spelling(*c, d.type) << "\n";
        } else if(d.kind == ABI_DEFINITION_TEMPLATE_ARGUMENT) {
          output << "let-arg " << label << " ";
          serialize_template_argument(output, *c, d.template_argument);
          output << "\n";
        } else if(d.kind == ABI_DEFINITION_EXPRESSION) {
          vector<string> expression_words;
          append_expression_words(*c, d.expression, expression_words);
          output << "let-expr " << label << " " << join_words(expression_words) << "\n";
        } else if(d.kind == ABI_DEFINITION_CONTEXT && d.context.kind == ABI_CONTEXT_RAW) {
          output << "let-context " << label << " raw " << d.context.fragment << "\n";
        } else if(d.kind == ABI_DEFINITION_CONTEXT && d.context.kind == ABI_CONTEXT_FUNCTION) {
          output << "let-context " << label << " ";
          serialize_function_target(output, *c, d.context.function,
                                    FUNCTION_TARGET_SERIALIZE_CONTEXT);
          output << "\n";
        } else if(d.kind == ABI_DEFINITION_ENTITY) {
          output << "let-entity " << label << " ";
          serialize_entity_definition(output, *c, d.entity);
          output << "\n";
        } else {
          throw logic_error("unsupported ABI definition kind in serializer");
        }
      } else if(it->kind == ABI_FACT_RECORD_FUNCTION) {
        const AbiFunctionRecord & f = it->function;
        if(f.kind == ABI_FUNCTION_RECORD_PARAMETER) {
          output << "param " << type_spelling(*c, f.type) << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_RESULT) {
          output << "result " << type_spelling(*c, f.type) << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_VARIADIC) {
          output << "variadic\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_ABI_TAG) {
          output << "abi-tag " << f.name << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_NAME_SOURCE) {
          output << "name-source ";
          if(f.source_name.components.empty()) output << "-";
          else output << join_qualified_name(f.source_name);
          output << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_NAME_STD) {
          output << "name-std\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_NAME_TEMPLATE) {
          if(f.name.empty()) throw logic_error("template name record has no name");
          // The first two optional fields are adapter observations.  Emit
          // canonical placeholders so the parser reaches the typed standard
          // substitution flag and argument-reference range deterministically.
          output << "name-template " << f.name << " - - ";
          if(f.standard_substitution_kind == ABI_STANDARD_SUBSTITUTION_NONE) {
            output << "- no";
          } else {
            output << (f.standard_substitution.empty() ?
                       standard_substitution_spelling(f.standard_substitution_kind) :
                       f.standard_substitution)
                   << " " << (f.standard_substitution_includes_arguments ? "yes" : "no");
          }
          for(vector<AbiDefinitionId>::const_iterator argument = f.argument_refs.begin();
              argument != f.argument_refs.end(); ++argument) {
            output << " " << definition_ref_spelling(*c, *argument);
          }
          output << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_FUNCTION_TEMPLATE_ARGUMENT) {
          if(f.argument_refs.size() != 1) throw logic_error("function template argument record is incomplete");
          output << "function-template-arg "
                 << definition_ref_spelling(*c, f.argument_refs[0]) << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_FUNCTION_TEMPLATE_PREFIX) {
          output << "function-template-prefix ";
          if(f.function_template_prefix_conversion) {
            output << "operator-name:cv";
          } else if(f.function_template_prefix_operator != ABI_OPERATOR_TERMINAL_NONE) {
            output << "operator-name:";
            if(f.function_template_prefix_operator == ABI_OPERATOR_TERMINAL_CALL) output << "cl";
            else if(f.function_template_prefix_operator == ABI_OPERATOR_TERMINAL_INDEX) output << "ix";
            else throw logic_error("unsupported function template operator prefix in serializer");
          } else {
            output << join_qualified_name(f.function_template_prefix_name);
          }
          output << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_TERMINAL) {
          output << "terminal ";
          if(f.special_terminal != ABI_SPECIAL_TERMINAL_NONE)
            output << special_terminal_spelling(f.special_terminal);
          else if(!f.terminal.empty())
            output << f.terminal;
          else
            throw logic_error("terminal record has no spelling");
          output << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_TERMINAL_SOURCE) {
          output << "terminal-source " << f.terminal << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_OPERATOR_TERMINAL) {
          output << "operator-terminal " << operator_terminal_spelling(f.operator_terminal);
          if(f.operator_terminal == ABI_OPERATOR_TERMINAL_LITERAL)
            output << " " << f.literal_suffix;
          else if(!f.literal_suffix.empty())
            throw logic_error("non-literal operator terminal has a literal suffix");
          output << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_CONVERSION_TERMINAL) {
          if(!f.has_conversion_type)
            throw logic_error("conversion terminal has no typed conversion type");
          output << "conversion-terminal " << type_spelling(*c, f.conversion_type) << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_LOCAL_CONTEXT) {
          output << "local-context " << definition_ref_spelling(*c, f.context_ref);
          if(!f.source_name.components.empty()) output << " " << join_qualified_name(f.source_name);
          if(!f.discriminator.empty()) output << " " << f.discriminator;
          output << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_LAMBDA_CONTEXT) {
          output << "lambda-context " << definition_ref_spelling(*c, f.context_ref)
                 << " " << f.discriminator;
          for(vector<AbiType>::const_iterator type = f.types.begin(); type != f.types.end(); ++type)
            output << " " << type_spelling(*c, *type);
          output << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_NAMESPACE_LAMBDA_CONTEXT) {
          output << "namespace-lambda-context " << join_qualified_name(f.source_name);
          for(vector<string>::const_iterator qualifier = f.namespace_qualifiers.begin();
              qualifier != f.namespace_qualifiers.end(); ++qualifier)
            output << " " << *qualifier;
          output << "\n";
        } else if(f.kind == ABI_FUNCTION_RECORD_QUALIFIER) {
          output << "function-qualifier";
          for(vector<AbiFunctionQualifier>::const_iterator qualifier = f.qualifiers.begin();
              qualifier != f.qualifiers.end(); ++qualifier) {
            switch(*qualifier) {
            case ABI_FUNCTION_QUALIFIER_CONST: output << " const"; break;
            case ABI_FUNCTION_QUALIFIER_VOLATILE: output << " volatile"; break;
            case ABI_FUNCTION_QUALIFIER_LVALUE_REFERENCE: output << " lvalue-ref"; break;
            case ABI_FUNCTION_QUALIFIER_RVALUE_REFERENCE: output << " rvalue-ref"; break;
            }
          }
          output << "\n";
        } else {
          throw logic_error("unsupported ABI function record in serializer");
        }
      } else {
        const AbiTargetRecord & t = it->target;
        switch(t.kind) {
        case ABI_TARGET_FACT_TYPE: output << "type " << type_spelling(*c, t.type) << "\n"; break;
        case ABI_TARGET_FACT_FUNCTION:
          serialize_function_target(output, *c, t.function,
                                    FUNCTION_TARGET_SERIALIZE_TOP_LEVEL);
          output << "\n";
          break;
        case ABI_TARGET_FACT_VARIABLE: output << "variable " << join_qualified_name(t.name) << "\n"; break;
        case ABI_TARGET_FACT_TYPEINFO: output << "typeinfo " << type_spelling(*c, t.type) << "\n"; break;
        case ABI_TARGET_FACT_VTABLE: output << "vtable " << type_spelling(*c, t.type) << "\n"; break;
        case ABI_TARGET_FACT_VTT: output << "vtt " << type_spelling(*c, t.type) << "\n"; break;
        case ABI_TARGET_FACT_CONSTRUCTION_VTABLE:
          output << "construction-vtable " << type_spelling(*c, t.type) << " "
                 << t.base_offset << " " << type_spelling(*c, t.base_type) << "\n";
          break;
        case ABI_TARGET_FACT_THREAD_LOCAL_WRAPPER:
          output << "tls-wrapper variable " << join_qualified_name(t.name) << "\n";
          break;
        case ABI_TARGET_FACT_THUNK:
        case ABI_TARGET_FACT_VIRTUAL_BASE_THUNK:
          output << (t.kind == ABI_TARGET_FACT_THUNK ? "thunk " : "virtual-base-thunk ")
                 << t.this_adjust;
          if(t.result_adjust_virtual) {
            output << " virtual-result " << t.result_adjust << " " << t.result_vcall_offset;
          } else if(t.has_result_adjust) {
            output << " " << t.result_adjust;
          }
          if(t.function.kind != ABI_FUNCTION_TARGET_PATH || !t.function.path_operands.empty()) {
            throw logic_error("thunk serializer requires a plain function path");
          }
          output << " function path " << join_qualified_name(t.function.name) << "\n";
          break;
        default: throw logic_error("unsupported ABI target in serializer");
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
