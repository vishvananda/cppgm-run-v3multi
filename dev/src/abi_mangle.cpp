#include "abi_mangle.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace abi_mangle {
namespace {

std::string number_string(unsigned long long value)
{
  std::ostringstream output;
  output << value;
  return output.str();
}

std::string source_name(const std::string & name)
{
  if(name.empty()) {
    throw std::logic_error("empty ABI source name");
  }
  std::ostringstream output;
  output << name.size() << name;
  return output.str();
}

bool is_unsupported_wide_integral_value_type(const AbiType & original)
{
  const AbiType * type = &original;
  while(type->kind == ABI_TYPE_CV) {
    if(type->types.empty()) return false;
    type = &type->types[0];
  }
  return type->kind == ABI_TYPE_BUILTIN &&
    (type->builtin == ABI_BUILTIN_INT128 ||
     type->builtin == ABI_BUILTIN_UNSIGNED_INT128);
}

// This is only for the true external-symbol boundary (C linkage or a raw
// symbol fact).  Semantic names are kept as AbiQualifiedName throughout the
// encoder and are never reconstructed from this spelling.
std::string join_name_for_external_symbol(const AbiQualifiedName & name)
{
  if(name.components.empty()) {
    throw std::logic_error("empty external ABI name");
  }
  std::string result;
  for(std::vector<std::string>::const_iterator it = name.components.begin();
      it != name.components.end(); ++it) {
    if(it != name.components.begin()) result += "::";
    result += *it;
  }
  return result;
}

std::string tag_suffix(const std::vector<std::string> & tags)
{
  std::vector<std::string> sorted = tags;
  std::sort(sorted.begin(), sorted.end());
  std::ostringstream output;
  for(std::vector<std::string>::const_iterator it = sorted.begin();
      it != sorted.end(); ++it) {
    output << "B" << source_name(*it);
  }
  return output.str();
}

class ActiveDefinitionScope
{
public:
  ActiveDefinitionScope(std::vector<unsigned char> & state, std::size_t index)
    : state_(state), index_(index)
  {
    if(index_ >= state_.size() || state_[index_] != 0) {
      throw std::logic_error("cyclic ABI type definition");
    }
    state_[index_] = 1;
  }

  ~ActiveDefinitionScope()
  {
    state_[index_] = 0;
  }

private:
  std::vector<unsigned char> & state_;
  std::size_t index_;
};

class FactEncoder
{
public:
  explicit FactEncoder(const AbiFactCase & fact_case)
    : fact_case_(fact_case)
  {
    for(std::vector<AbiFactRecord>::const_iterator it = fact_case_.records.begin();
        it != fact_case_.records.end(); ++it) {
      if(it->kind != ABI_FACT_RECORD_DEFINITION) {
        continue;
      }
      if(it->definition.id.index == ABI_INVALID_DEFINITION_ID) {
        throw std::logic_error("ABI definition is not canonically indexed");
      }
      if(it->definition.id.index >= definitions_.size()) {
        definitions_.resize(it->definition.id.index + 1, NULL);
      }
      if(definitions_[it->definition.id.index] != NULL) {
        throw std::logic_error("duplicate ABI definition id");
      }
      definitions_[it->definition.id.index] = &it->definition;
    }
    active_types_.assign(definitions_.size(), 0);
  }

  std::string encode()
  {
    const AbiTargetRecord * target = NULL;
    for(std::vector<AbiFactRecord>::const_iterator it = fact_case_.records.begin();
        it != fact_case_.records.end(); ++it) {
      if(it->kind != ABI_FACT_RECORD_TARGET) {
        continue;
      }
      if(target != NULL) {
        throw std::logic_error("ABI fact case has multiple targets");
      }
      target = &it->target;
    }
    if(target == NULL) {
      throw std::logic_error("ABI fact case has no target");
    }

    switch(target->kind) {
    case ABI_TARGET_FACT_TYPE:
      return encode_type(target->type);
    case ABI_TARGET_FACT_FUNCTION:
      if(target->linkage == ABI_LINKAGE_C) {
        if(target->function.name.components.empty()) {
          throw std::logic_error("C linkage function has an empty name");
        }
        return target->function.name.components.back();
      }
      return encode_function(target->function);
    case ABI_TARGET_FACT_VARIABLE: {
      if(target->name.components.size() == 1) return target->name.components[0];
      return "_Z" + encode_name(target->name, std::vector<std::string>());
    }
    case ABI_TARGET_FACT_TYPEINFO:
      return "_ZTI" + encode_type(target->type);
    case ABI_TARGET_FACT_VTABLE:
      return "_ZTV" + encode_type(target->type);
    case ABI_TARGET_FACT_VTT:
      return "_ZTT" + encode_type(target->type);
    case ABI_TARGET_FACT_CONSTRUCTION_VTABLE:
      return "_ZTC" + encode_type(target->type) + number_string(target->base_offset) +
        "_" + encode_type(target->base_type);
    case ABI_TARGET_FACT_THREAD_LOCAL_WRAPPER:
      return "_ZTH" + encode_name(target->name, std::vector<std::string>());
    case ABI_TARGET_FACT_THUNK:
    case ABI_TARGET_FACT_VIRTUAL_BASE_THUNK:
      return encode_thunk(*target);
    }
    throw std::logic_error("unknown ABI target kind");
  }

private:
  const AbiFactCase & fact_case_;
  std::vector<const AbiDefinitionRecord *> definitions_;
  std::vector<unsigned char> active_types_;

  const AbiDefinitionRecord & definition(const AbiDefinitionId & id) const
  {
    if(id.index == ABI_INVALID_DEFINITION_ID || id.index >= definitions_.size() ||
       definitions_[id.index] == NULL) {
      throw std::logic_error("unknown ABI definition index");
    }
    return *definitions_[id.index];
  }

  const AbiType & type_definition(const AbiDefinitionId & id) const
  {
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_TYPE) {
      throw std::logic_error("ABI definition is not a type definition");
    }
    return record.type;
  }

  const AbiTemplateArgument & argument_definition(const AbiDefinitionId & id) const
  {
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_TEMPLATE_ARGUMENT) {
      throw std::logic_error("ABI definition is not an argument definition");
    }
    return record.template_argument;
  }

  std::string encode_name(const AbiQualifiedName & name,
                          const std::vector<std::string> & tags)
  {
    const std::vector<std::string> & components = name.components;
    if(components.empty()) {
      throw std::logic_error("empty qualified ABI name");
    }
    const std::string suffix = tag_suffix(tags);
    std::ostringstream output;
    if(components.size() == 1) {
      output << source_name(components[0]) << suffix;
      return output.str();
    }

    if(components[0] == "std") {
      if(components.size() == 2) {
        output << "St" << source_name(components[1]) << suffix;
        return output.str();
      }
      output << "NSt";
      for(std::size_t i = 1; i < components.size(); ++i) {
        output << source_name(components[i]);
        if(i + 1 == components.size()) {
          output << suffix;
        }
      }
      output << "E";
      return output.str();
    }

    output << "N";
    for(std::size_t i = 0; i < components.size(); ++i) {
      output << source_name(components[i]);
      if(i + 1 == components.size()) {
        output << suffix;
      }
    }
    output << "E";
    return output.str();
  }

  std::string encode_type(const AbiType & original)
  {
    std::string result;
    append_type(original, result);
    return result;
  }

  void append_type(const AbiType & original, std::string & output)
  {
    const AbiType * type = &original;
    if(type->kind == ABI_TYPE_NAME_OR_REFERENCE &&
       type->definition_ref.index != ABI_INVALID_DEFINITION_ID) {
      const AbiType & definition_type = type_definition(type->definition_ref);
      ActiveDefinitionScope active(active_types_, type->definition_ref.index);
      append_type(definition_type, output);
      return;
    }

    switch(type->kind) {
    case ABI_TYPE_NAME_OR_REFERENCE:
    case ABI_TYPE_NAMED:
      output += encode_name(type->name, type->abi_tags);
      return;
    case ABI_TYPE_BUILTIN:
      output += builtin_code(type->builtin);
      return;
    case ABI_TYPE_TEMPLATE_PARAMETER:
      output += template_parameter(type->index);
      return;
    case ABI_TYPE_POINTER:
      output.push_back('P');
      append_type(type->types.at(0), output);
      return;
    case ABI_TYPE_LVALUE_REFERENCE:
      output.push_back('R');
      append_type(type->types.at(0), output);
      return;
    case ABI_TYPE_RVALUE_REFERENCE:
      output.push_back('O');
      append_type(type->types.at(0), output);
      return;
    case ABI_TYPE_CV:
      append_cv_type(type, output);
      return;
    case ABI_TYPE_ARRAY:
      append_array(type, output);
      return;
    case ABI_TYPE_FUNCTION:
      append_function_type(type, output);
      return;
    case ABI_TYPE_MEMBER_POINTER:
      output.push_back('M');
      append_type(type->types.at(0), output);
      append_type(type->types.at(1), output);
      return;
    case ABI_TYPE_PACK_EXPANSION:
      output.push_back('D');
      output.push_back('p');
      append_type(type->types.at(0), output);
      return;
    case ABI_TYPE_VENDOR_QUALIFIED:
      if(type->name.components.size() != 1) {
        throw std::logic_error("vendor qualifier is not a source component");
      }
      output.push_back('U');
      output += source_name(type->name.components[0]);
      append_type(type->types.at(0), output);
      return;
    case ABI_TYPE_BUILTIN_TRANSFORM:
      if(type->name.components.size() != 1) {
        throw std::logic_error("builtin transform is not a source component");
      }
      output.push_back('u');
      output += source_name(type->name.components[0]);
      append_type(type->types.at(0), output);
      return;
    case ABI_TYPE_TEMPLATE_SPECIALIZATION:
    case ABI_TYPE_STD_TEMPLATE_SPECIALIZATION:
    case ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION:
      append_template_specialization(*type, output);
      return;
    case ABI_TYPE_MEMBER:
      append_member_type(*type, output);
      return;
    case ABI_TYPE_MEMBER_TEMPLATE_SPECIALIZATION:
      append_template_specialization(*type, output);
      return;
    case ABI_TYPE_DECLTYPE_EXPRESSION:
      output += "Dt";
      output += encode_expression_reference(type->expression_ref);
      output.push_back('E');
      return;
    case ABI_TYPE_LAMBDA_CLOSURE:
      append_lambda_type(*type, output);
      return;
    case ABI_TYPE_LOCAL_TYPE:
      append_local_type(*type, output);
      return;
    case ABI_TYPE_NAMESPACE_LAMBDA:
      append_namespace_lambda_type(*type, output);
      return;
    }
    throw std::logic_error("unknown ABI type kind");
  }

  void append_cv_type(const AbiType * type, std::string & output)
  {
    bool is_const = false;
    bool is_volatile = false;
    const AbiType * base = type;
    while(base->kind == ABI_TYPE_CV) {
      is_const = is_const || base->is_const;
      is_volatile = is_volatile || base->is_volatile;
      if(base->types.empty()) {
        throw std::logic_error("qualified ABI type has no base type");
      }
      base = &base->types[0];
    }
    if(is_volatile) output.push_back('V');
    if(is_const) output.push_back('K');
    append_type(*base, output);
  }

  void append_array(const AbiType * type, std::string & output)
  {
    output.push_back('A');
    switch(type->array_bound.kind) {
    case ABI_ARRAY_BOUND_VALUE:
      output += number_string(type->array_bound.value);
      break;
    case ABI_ARRAY_BOUND_RAW:
      output += type->array_bound.raw;
      break;
    case ABI_ARRAY_BOUND_EXPRESSION:
      output += encode_expression_reference(type->array_bound.expression_ref);
      break;
    }
    output.push_back('_');
    append_type(type->types.at(0), output);
  }

  void append_function_type(const AbiType * type, std::string & output)
  {
    if(type->types.empty()) {
      throw std::logic_error("function ABI type has no result type");
    }
    output.push_back('F');
    if(type->is_const) output.push_back('K');
    if(type->is_volatile) output.push_back('V');
    append_type(type->types[0], output);
    if(type->types.size() == 1) {
      output.push_back('v');
    } else {
      for(std::size_t i = 1; i < type->types.size(); ++i) {
        append_type(type->types[i], output);
      }
    }
    if(type->variadic) output.push_back('z');
    output.push_back('E');
  }

  std::string builtin_code(AbiBuiltinKind name) const
  {
    static const char * const codes[] = {
      "", "v", "w", "b", "c", "a", "h", "s", "t", "i", "j", "l", "m",
      "x", "y", "n", "o", "f", "d", "e", "g", "z", "Ds", "Di", "Du", "Dn",
      "Cf", "Cd", "Ce"
    };
    const std::size_t index = static_cast<std::size_t>(name);
    if(index == 0 || index >= sizeof(codes) / sizeof(codes[0])) {
      throw std::logic_error("invalid ABI builtin kind");
    }
    return codes[index];
  }

  std::string template_parameter(std::size_t index) const
  {
    if(index == 0) {
      return "T_";
    }
    return "T" + number_string(index - 1) + "_";
  }

  void append_template_specialization(const AbiType & type,
                                      std::string & output)
  {
    std::string prefix;
    if(type.kind == ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION) {
      prefix = template_parameter(type.index);
    } else if(!type.standard_substitution.empty()) {
      prefix = type.standard_substitution;
    } else {
      prefix = encode_name(type.name, type.abi_tags);
    }
    output += prefix;
    output.push_back('I');
    for(std::vector<AbiDefinitionId>::const_iterator it = type.argument_refs.begin();
        it != type.argument_refs.end(); ++it) {
      output += encode_argument(argument_definition(*it));
    }
    output.push_back('E');
  }

  std::string encode_argument(const AbiTemplateArgument & argument)
  {
    switch(argument.kind) {
    case ABI_TEMPLATE_ARGUMENT_TYPE:
      return encode_type(argument.type);
    case ABI_TEMPLATE_ARGUMENT_VALUE:
    case ABI_TEMPLATE_ARGUMENT_DEPENDENT_VALUE:
      return encode_value_argument(argument);
    case ABI_TEMPLATE_ARGUMENT_UNTYPED_VALUE:
      return "L" + number_string(static_cast<unsigned long long>(argument.value)) + "E";
    case ABI_TEMPLATE_ARGUMENT_EXPRESSION:
      return encode_expression_reference(argument.entity_ref);
    case ABI_TEMPLATE_ARGUMENT_PACK: {
      std::string result = "J";
      for(std::vector<AbiDefinitionId>::const_iterator it = argument.argument_refs.begin();
          it != argument.argument_refs.end(); ++it) {
        result += encode_argument(argument_definition(*it));
      }
      return result + "E";
    }
    case ABI_TEMPLATE_ARGUMENT_TEMPLATE_PARAMETER_TEMPLATE:
      return template_parameter(argument.index);
    case ABI_TEMPLATE_ARGUMENT_TEMPLATE_ENTITY:
      return encode_name(argument.name, std::vector<std::string>());
    case ABI_TEMPLATE_ARGUMENT_MEMBER_TEMPLATE_ENTITY:
      if(argument.name.components.size() != 1) {
        throw std::logic_error("member template name is not a source component");
      }
      return encode_type(argument.owner_type) + source_name(argument.name.components[0]);
    case ABI_TEMPLATE_ARGUMENT_EXTERNAL_ENTITY:
      return "L" + argument.symbol + "E";
    case ABI_TEMPLATE_ARGUMENT_MEMBER_EXTERNAL_ENTITY:
    case ABI_TEMPLATE_ARGUMENT_ENTITY:
      return encode_entity_argument(argument);
    }
    throw std::logic_error("unknown ABI template argument kind");
  }

  bool is_unsigned_builtin(const AbiType & type, unsigned int * bits) const
  {
    if(type.kind != ABI_TYPE_BUILTIN) return false;
    if(type.builtin == ABI_BUILTIN_UNSIGNED_INT) { *bits = 32; return true; }
    if(type.builtin == ABI_BUILTIN_UNSIGNED_LONG) { *bits = 64; return true; }
    if(type.builtin == ABI_BUILTIN_UNSIGNED_CHAR) { *bits = 8; return true; }
    if(type.builtin == ABI_BUILTIN_UNSIGNED_SHORT) { *bits = 16; return true; }
    if(type.builtin == ABI_BUILTIN_UNSIGNED_LONG_LONG) { *bits = 64; return true; }
    if(type.builtin == ABI_BUILTIN_UNSIGNED_INT128) { *bits = 128; return true; }
    return false;
  }

  std::string encode_value_argument(const AbiTemplateArgument & argument)
  {
    if(!argument.has_value_type) {
      throw std::logic_error("typed ABI value is missing its value type");
    }
    if(is_unsupported_wide_integral_value_type(argument.value_type)) {
      throw std::logic_error("128-bit integral ABI values are unsupported by the stored value representation");
    }
    const std::string type = encode_type(argument.value_type);
    const unsigned long long raw = static_cast<unsigned long long>(argument.value);
    unsigned int bits = 0;
    std::string value;
    if(is_unsigned_builtin(argument.value_type, &bits)) {
      unsigned long long normalized = raw;
      if(bits < 64) {
        normalized &= (static_cast<unsigned long long>(1) << bits) - 1;
      }
      value = number_string(normalized);
    } else if(argument.value < 0) {
      const unsigned long long magnitude = 0 - raw;
      value = "n" + number_string(magnitude);
    } else {
      value = number_string(raw);
    }
    return "L" + type + value + "E";
  }

  std::string encode_entity_argument(const AbiTemplateArgument & argument)
  {
    if(!argument.symbol.empty()) {
      return "L" + argument.symbol + "E";
    }
    const AbiEntityFact & entity = definition(argument.entity_ref).entity;
    if(entity.kind == ABI_ENTITY_FACT_SYMBOL) {
      return "L" + join_name_for_external_symbol(entity.name) + "E";
    }
    if(entity.kind == ABI_ENTITY_FACT_VARIABLE) {
      std::string result = "L_Z" + encode_name(entity.name,
                                                std::vector<std::string>()) + "E";
      return result;
    }
    return "L_Z" + encode_function(entity.function) + "E";
  }

  std::string encode_expression_reference(const AbiDefinitionId & id)
  {
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_EXPRESSION) {
      throw std::logic_error("ABI definition is not an expression");
    }
    if(record.expression.kind == ABI_EXPRESSION_LITERAL ||
       record.expression.kind == ABI_EXPRESSION_INTEGRAL_VALUE) {
      return number_string(static_cast<unsigned long long>(record.expression.value));
    }
    return record.expression.text;
  }

  void append_member_type(const AbiType & type, std::string & output)
  {
    if(type.types.empty() || type.name.components.size() != 1) {
      throw std::logic_error("member ABI type is incomplete");
    }
    append_type(type.types[0], output);
    output += source_name(type.name.components[0]);
  }

  void append_lambda_type(const AbiType & type, std::string & output)
  {
    const AbiLocalContext & context = definition(type.context_ref).context;
    if(context.kind == ABI_CONTEXT_RAW) output += context.fragment;
    else output += "Z" + encode_function(context.function) + "E";
    output += "Ul";
    if(type.types.empty()) output += "v";
    else for(std::vector<AbiType>::const_iterator it = type.types.begin();
              it != type.types.end(); ++it) append_type(*it, output);
    output.push_back('E');
    if(!type.discriminator.empty() && type.discriminator != "0") output += type.discriminator;
    output.push_back('_');
  }

  void append_namespace_lambda_type(const AbiType & type, std::string & output)
  {
    if(type.name.components.size() != 1) {
      throw std::logic_error("namespace lambda source name is not a component");
    }
    output += "Ul" + type.name.components[0] + "E";
    if(!type.discriminator.empty() && type.discriminator != "0") output += type.discriminator;
    output.push_back('_');
    for(std::vector<std::string>::const_iterator it = type.namespace_qualifiers.begin();
        it != type.namespace_qualifiers.end(); ++it) {
      (void)it;
    }
  }

  void append_local_type(const AbiType & type, std::string & output)
  {
    const AbiLocalContext & context = definition(type.context_ref).context;
    if(context.kind == ABI_CONTEXT_RAW) output += context.fragment;
    else output += "Z" + encode_function(context.function) + "E";
    if(type.name.components.size() != 1) {
      throw std::logic_error("local type name is not a component");
    }
    output += "N" + source_name(type.name.components[0]) + "E";
    if(!type.discriminator.empty() && type.discriminator != "0") output += "_" + type.discriminator;
  }

  std::string encode_function_type_parameters(const AbiFunctionTarget & target)
  {
    std::string result;
    bool has_parameter = false;
    for(std::vector<AbiFunctionPathOperand>::const_iterator it = target.path_operands.begin();
        it != target.path_operands.end(); ++it) {
      if(it->kind != ABI_FUNCTION_PATH_TYPE) continue;
      result += encode_type(it->type);
      has_parameter = true;
    }
    for(std::vector<AbiType>::const_iterator it = target.signature_parameter_types.begin();
        it != target.signature_parameter_types.end(); ++it) {
      result += encode_type(*it);
      has_parameter = true;
    }
    for(std::vector<AbiFactRecord>::const_iterator it = fact_case_.records.begin();
        it != fact_case_.records.end(); ++it) {
      if(it->kind != ABI_FACT_RECORD_FUNCTION ||
         it->function.kind != ABI_FUNCTION_RECORD_PARAMETER) continue;
      result += encode_type(it->function.type);
      has_parameter = true;
    }
    bool variadic = false;
    for(std::vector<AbiFactRecord>::const_iterator it = fact_case_.records.begin();
        it != fact_case_.records.end(); ++it) {
      if(it->kind == ABI_FACT_RECORD_FUNCTION &&
         it->function.kind == ABI_FUNCTION_RECORD_VARIADIC) variadic = true;
    }
    if(!has_parameter) result += "v";
    if(variadic) result += "z";
    return result;
  }

  AbiQualifiedName source_components() const
  {
    AbiQualifiedName components;
    for(std::vector<AbiFactRecord>::const_iterator it = fact_case_.records.begin();
        it != fact_case_.records.end(); ++it) {
      if(it->kind != ABI_FACT_RECORD_FUNCTION ||
         it->function.kind != ABI_FUNCTION_RECORD_NAME_SOURCE) continue;
      if(it->function.source_name.components.empty()) continue;
      components.components.insert(components.components.end(),
                                   it->function.source_name.components.begin(),
                                   it->function.source_name.components.end());
    }
    return components;
  }

  std::string encode_name_with_std_prefix(const AbiQualifiedName & name,
                                          const std::vector<std::string> & tags,
                                          bool std_prefix)
  {
    if(name.components.empty()) {
      throw std::logic_error("ABI function has no name components");
    }
    if(!std_prefix) {
      return encode_name(name, tags);
    }
    AbiQualifiedName prefixed;
    prefixed.components.push_back("std");
    prefixed.components.insert(prefixed.components.end(), name.components.begin(),
                               name.components.end());
    return encode_name(prefixed, tags);
  }

  std::string operator_code(const std::string & name, bool unary) const
  {
    if(name == "new") return "nw";
    if(name == "new-array") return "na";
    if(name == "delete") return "dl";
    if(name == "delete-array") return "da";
    if(name == "plus" || name == "binary-plus" || name == "plus-assign") return name == "plus-assign" ? "pL" : "pl";
    if(name == "unary-plus") return "ps";
    if(name == "minus" || name == "binary-minus" || name == "minus-assign") return name == "minus-assign" ? "mI" : "mi";
    if(name == "unary-minus") return "ng";
    if(name == "address-of") return "ad";
    if(name == "deref") return "de";
    if(name == "multiply") return "ml";
    if(name == "divide") return "dv";
    if(name == "remainder") return "rm";
    if(name == "bit-and") return "an";
    if(name == "bit-or") return "or";
    if(name == "bit-xor") return "eo";
    if(name == "increment") return "pp";
    if(name == "decrement") return "mm";
    if(name == "comma") return "cm";
    if(name == "member-pointer") return "pm";
    if(name == "arrow") return "pt";
    if(name == "call" || name == "operator-call") return "cl";
    if(name == "index") return "ix";
    if(name == "equal") return "eq";
    if(name == "not-equal") return "ne";
    if(name == "less") return "lt";
    if(name == "greater") return "gt";
    if(name == "less-equal") return "le";
    if(name == "greater-equal") return "ge";
    if(name == "logical-not") return "nt";
    if(name == "logical-and") return "aa";
    if(name == "logical-or") return "oo";
    if(name == "assign") return "aS";
    (void)unary;
    throw std::logic_error("unknown ABI operator terminal '" + name + "'");
  }

  std::string encode_function_name(const AbiFunctionTarget & target)
  {
    if(target.kind == ABI_FUNCTION_TARGET_PATH && target.name.components.empty()) {
      throw std::logic_error("function path has an empty name");
    }
    AbiQualifiedName components;
    bool std_prefix = false;
    std::vector<std::string> tags;
    std::string terminal;
    std::string conversion;
    std::string literal;
    bool has_operator = false;
    bool constructor = false;
    bool destructor = false;
    AbiFunctionSpecialTerminalKind special_terminal = ABI_SPECIAL_TERMINAL_NONE;
    for(std::vector<AbiFactRecord>::const_iterator fact = fact_case_.records.begin();
        fact != fact_case_.records.end(); ++fact) {
      if(fact->kind != ABI_FACT_RECORD_FUNCTION) continue;
      const AbiFunctionRecord & record = fact->function;
      switch(record.kind) {
      case ABI_FUNCTION_RECORD_NAME_STD:
        std_prefix = true;
        break;
      case ABI_FUNCTION_RECORD_NAME_SOURCE:
        break;
      case ABI_FUNCTION_RECORD_TERMINAL:
        terminal = record.terminal;
        special_terminal = record.special_terminal;
        break;
      case ABI_FUNCTION_RECORD_TERMINAL_SOURCE:
        terminal = record.terminal;
        special_terminal = record.special_terminal;
        break;
      case ABI_FUNCTION_RECORD_OPERATOR_TERMINAL:
        terminal = record.terminal;
        literal = record.literal_suffix;
        has_operator = true;
        break;
      case ABI_FUNCTION_RECORD_CONVERSION_TERMINAL:
        conversion = record.terminal;
        break;
      case ABI_FUNCTION_RECORD_ABI_TAG:
        tags.push_back(record.name);
        break;
      default:
        break;
      }
    }
    if(target.kind == ABI_FUNCTION_TARGET_PATH) {
      components = target.name;
    } else {
      components = source_components();
      if(target.kind == ABI_FUNCTION_TARGET_NAMESPACE_LAMBDA) {
        std::string result = "N";
        for(std::vector<std::string>::const_iterator it = target.namespace_qualifiers.begin();
            it != target.namespace_qualifiers.end(); ++it) {
          result += source_name(*it);
        }
        result += "Ul" + source_name(target.source_name) + "E_";
        if(target.terminal == "operator-call") result += "cl";
        return result;
      }
    }

    for(std::vector<std::string>::const_iterator it = components.components.begin();
        it != components.components.end(); ++it) {
      if(*it == "operator") has_operator = true;
    }
    constructor = special_terminal == ABI_SPECIAL_TERMINAL_CONSTRUCTOR_COMPLETE ||
      special_terminal == ABI_SPECIAL_TERMINAL_CONSTRUCTOR_BASE ||
      special_terminal == ABI_SPECIAL_TERMINAL_CONSTRUCTOR_ALLOCATING;
    destructor = special_terminal == ABI_SPECIAL_TERMINAL_DESTRUCTOR_DELETING ||
      special_terminal == ABI_SPECIAL_TERMINAL_DESTRUCTOR_COMPLETE ||
      special_terminal == ABI_SPECIAL_TERMINAL_DESTRUCTOR_BASE;

    if(constructor || destructor) {
      if(components.components.empty()) throw std::logic_error("special function has no owner");
      std::string result = "N";
      for(std::vector<std::string>::const_iterator it = components.components.begin();
          it != components.components.end(); ++it) {
        result += source_name(*it);
      }
      if(constructor) {
        switch(special_terminal) {
        case ABI_SPECIAL_TERMINAL_CONSTRUCTOR_COMPLETE: result += "C1"; break;
        case ABI_SPECIAL_TERMINAL_CONSTRUCTOR_BASE: result += "C2"; break;
        case ABI_SPECIAL_TERMINAL_CONSTRUCTOR_ALLOCATING: result += "C3"; break;
        default: throw std::logic_error("constructor has no special terminal kind");
        }
      } else {
        switch(special_terminal) {
        case ABI_SPECIAL_TERMINAL_DESTRUCTOR_DELETING: result += "D0"; break;
        case ABI_SPECIAL_TERMINAL_DESTRUCTOR_COMPLETE: result += "D1"; break;
        case ABI_SPECIAL_TERMINAL_DESTRUCTOR_BASE: result += "D2"; break;
        default: throw std::logic_error("destructor has no special terminal kind");
        }
      }
      return result + "E";
    }

    if(conversion.empty() && has_operator) {
      const std::string code = literal.empty() ? operator_code(terminal, false) :
        "li" + source_name(literal);
      if(components.components.size() > 1 && components.components.back() == "operator") {
        components.components.pop_back();
      }
      std::string result = encode_name_with_std_prefix(components, tags, std_prefix);
      if(result.size() >= 1 && result[result.size() - 1] == 'E') {
        result.erase(result.size() - 1);
        result += code + "E";
      } else {
        result += code;
      }
      return result;
    }
    if(!conversion.empty()) {
      if(components.components.size() > 1 && components.components.back() == "operator") {
        components.components.pop_back();
      }
      std::string result = encode_name_with_std_prefix(components, tags, std_prefix);
      if(result.size() >= 1 && result[result.size() - 1] == 'E') result.erase(result.size() - 1);
      for(std::vector<AbiFactRecord>::const_iterator fact = fact_case_.records.begin();
          fact != fact_case_.records.end(); ++fact) {
        if(fact->kind == ABI_FACT_RECORD_FUNCTION &&
           fact->function.kind == ABI_FUNCTION_RECORD_CONVERSION_TERMINAL &&
           fact->function.has_conversion_type) {
          result += "cv" + encode_type(fact->function.conversion_type) + "E";
          return result;
        }
      }
      throw std::logic_error("conversion terminal has no typed conversion type");
    }
    if(target.kind == ABI_FUNCTION_TARGET_PATH) {
      return encode_name(target.name, tags);
    }
    return encode_name_with_std_prefix(components, tags, std_prefix);
  }

  std::string encode_function(const AbiFunctionTarget & target)
  {
    const std::string name = encode_function_name(target);
    const std::string params = encode_function_type_parameters(target);
    return "_Z" + name + params;
  }

  std::string encode_thunk(const AbiTargetRecord & target)
  {
    std::ostringstream prefix;
    if(target.kind == ABI_TARGET_FACT_VIRTUAL_BASE_THUNK) prefix << "Tv";
    else if(target.result_adjust_virtual) prefix << "Tc";
    else if(target.has_result_adjust) prefix << "Tc";
    else prefix << "Th";
    prefix << target.this_adjust;
    if(target.has_result_adjust && !target.result_adjust_virtual) prefix << "_" << target.result_adjust;
    if(target.result_adjust_virtual) prefix << "_" << target.result_adjust << "_" << target.result_vcall_offset;
    return prefix.str() + encode_function(target.function);
  }
};

}  // namespace

std::string mangle_abi_fact_case(const AbiFactCase & fact_case)
{
  return FactEncoder(fact_case).encode();
}

}  // namespace abi_mangle
