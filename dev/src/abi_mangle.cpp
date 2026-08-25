#include "abi_mangle.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>
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

std::vector<std::string> split_qualified_name(const std::string & spelling)
{
  std::string name = spelling;
  while(name.size() >= 2 && name.compare(0, 2, "::") == 0) {
    name.erase(0, 2);
  }
  if(name.empty()) {
    throw std::logic_error("empty qualified ABI name");
  }

  std::vector<std::string> components;
  std::size_t begin = 0;
  while(begin < name.size()) {
    const std::size_t separator = name.find("::", begin);
    const std::size_t end = separator == std::string::npos ? name.size() : separator;
    if(end == begin) {
      throw std::logic_error("empty component in qualified ABI name");
    }
    components.push_back(name.substr(begin, end - begin));
    if(separator == std::string::npos) {
      break;
    }
    begin = separator + 2;
    if(begin == name.size()) {
      throw std::logic_error("trailing scope separator in qualified ABI name");
    }
  }
  return components;
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

class SubstitutionTable
{
public:
  void remember(const std::string & structural_key)
  {
    if(structural_key.empty() || entries_.find(structural_key) != entries_.end()) {
      return;
    }
    entries_[structural_key] = entries_.size();
  }

  bool contains(const std::string & structural_key) const
  {
    return entries_.find(structural_key) != entries_.end();
  }

  std::string spelling(const std::string & structural_key) const
  {
    std::map<std::string, std::size_t>::const_iterator found =
      entries_.find(structural_key);
    if(found == entries_.end()) {
      throw std::logic_error("missing ABI substitution entry");
    }
    if(found->second == 0) {
      return "S_";
    }
    return "S" + number_string(found->second - 1) + "_";
  }

private:
  std::map<std::string, std::size_t> entries_;
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
      if(it->definition.id.empty()) {
        throw std::logic_error("ABI definition has an empty id");
      }
      if(definitions_.find(it->definition.id) != definitions_.end()) {
        throw std::logic_error("duplicate ABI definition id");
      }
      definitions_[it->definition.id] = &it->definition;
    }
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
      if(target->c_linkage) return target->function.qualified_name;
      return encode_function(target->function);
    case ABI_TARGET_FACT_VARIABLE: {
      const std::vector<std::string> components = split_qualified_name(target->qualified_name);
      if(components.size() == 1) return components[0];
      return "_Z" + encode_name(target->qualified_name, std::vector<std::string>());
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
      return "_ZTH" + encode_name(target->qualified_name, std::vector<std::string>());
    case ABI_TARGET_FACT_THUNK:
    case ABI_TARGET_FACT_VIRTUAL_BASE_THUNK:
      return encode_thunk(*target);
    }
    throw std::logic_error("unknown ABI target kind");
  }

private:
  const AbiFactCase & fact_case_;
  std::map<std::string, const AbiDefinitionRecord *> definitions_;
  SubstitutionTable substitutions_;
  std::set<std::string> active_types_;

  const AbiDefinitionRecord & definition(const std::string & id) const
  {
    std::map<std::string, const AbiDefinitionRecord *>::const_iterator found =
      definitions_.find(id);
    if(found == definitions_.end()) {
      throw std::logic_error("unknown ABI definition id '" + id + "'");
    }
    return *found->second;
  }

  const AbiType & type_definition(const std::string & id) const
  {
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_TYPE) {
      throw std::logic_error("ABI id '" + id + "' is not a type definition");
    }
    return record.type;
  }

  const AbiTemplateArgument & argument_definition(const std::string & id) const
  {
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_TEMPLATE_ARGUMENT) {
      throw std::logic_error("ABI id '" + id + "' is not an argument definition");
    }
    return record.template_argument;
  }

  std::string type_key(const AbiType & type) const
  {
    std::ostringstream key;
    key << static_cast<int>(type.kind) << ":" << type.name << ":" << type.index << ":";
    key << (type.is_const ? "c" : "-") << (type.is_volatile ? "v" : "-");
    key << ":" << type.array_bound.value << ":" << type.expression_ref;
    for(std::vector<AbiType>::const_iterator it = type.types.begin();
        it != type.types.end(); ++it) {
      key << "[" << type_key(*it) << "]";
    }
    for(std::vector<std::string>::const_iterator it = type.argument_refs.begin();
        it != type.argument_refs.end(); ++it) {
      key << "{" << *it << "}";
    }
    return key.str();
  }

  std::string encode_name(const std::string & spelling,
                          const std::vector<std::string> & tags)
  {
    const std::vector<std::string> components = split_qualified_name(spelling);
    const std::string suffix = tag_suffix(tags);
    std::ostringstream output;
    if(components.size() == 1) {
      output << source_name(components[0]) << suffix;
      substitutions_.remember("name:" + spelling + ":" + suffix);
      return output.str();
    }

    if(components[0] == "std") {
      if(components.size() == 2) {
        output << "St" << source_name(components[1]) << suffix;
        substitutions_.remember("name:" + spelling + ":" + suffix);
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
      substitutions_.remember("name:" + spelling + ":" + suffix);
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
    substitutions_.remember("name:" + spelling + ":" + suffix);
    return output.str();
  }

  std::string encode_type(const AbiType & original)
  {
    const std::string key = type_key(original);
    if(original.kind != ABI_TYPE_NAME_OR_REFERENCE) {
      substitutions_.remember("type:" + key);
    }
    return encode_type_impl(original);
  }

  std::string encode_type_impl(const AbiType & original)
  {
    const AbiType * type = &original;
    if(type->kind == ABI_TYPE_NAME_OR_REFERENCE && !type->name.empty()) {
      std::map<std::string, const AbiDefinitionRecord *>::const_iterator found =
        definitions_.find(type->name);
      if(found != definitions_.end()) {
        if(active_types_.find(type->name) != active_types_.end()) {
          throw std::logic_error("cyclic ABI type definition");
        }
        active_types_.insert(type->name);
        const std::string result = encode_type_impl(type_definition(type->name));
        active_types_.erase(type->name);
        return result;
      }
    }

    switch(type->kind) {
    case ABI_TYPE_NAME_OR_REFERENCE:
      return encode_name(type->name, type->abi_tags);
    case ABI_TYPE_NAMED:
      return encode_name(type->name, type->abi_tags);
    case ABI_TYPE_BUILTIN:
      return builtin_code(type->name);
    case ABI_TYPE_TEMPLATE_PARAMETER:
      return template_parameter(type->index);
    case ABI_TYPE_POINTER:
      return "P" + encode_type_impl(type->types.at(0));
    case ABI_TYPE_LVALUE_REFERENCE:
      return "R" + encode_type_impl(type->types.at(0));
    case ABI_TYPE_RVALUE_REFERENCE:
      return "O" + encode_type_impl(type->types.at(0));
    case ABI_TYPE_CV:
      return encode_cv_type(*type);
    case ABI_TYPE_PACK_EXPANSION:
      return "Dp" + encode_type_impl(type->types.at(0));
    case ABI_TYPE_VENDOR_QUALIFIED:
      return "U" + source_name(type->name) + encode_type_impl(type->types.at(0));
    case ABI_TYPE_ARRAY:
      return encode_array(*type);
    case ABI_TYPE_FUNCTION:
      return encode_function_type(*type);
    case ABI_TYPE_MEMBER_POINTER:
      return "M" + encode_type_impl(type->types.at(0)) + encode_type_impl(type->types.at(1));
    case ABI_TYPE_TEMPLATE_SPECIALIZATION:
    case ABI_TYPE_STD_TEMPLATE_SPECIALIZATION:
    case ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION:
      return encode_template_specialization(*type);
    case ABI_TYPE_BUILTIN_TRANSFORM:
      return "u" + source_name(type->name) + encode_type_impl(type->types.at(0));
    case ABI_TYPE_MEMBER:
      return encode_member_type(*type);
    case ABI_TYPE_MEMBER_TEMPLATE_SPECIALIZATION:
      return encode_template_specialization(*type);
    case ABI_TYPE_DECLTYPE_EXPRESSION:
      return "Dt" + encode_expression_reference(type->expression_ref) + "E";
    case ABI_TYPE_LAMBDA_CLOSURE:
      return encode_lambda_type(*type);
    case ABI_TYPE_LOCAL_TYPE:
      return encode_local_type(*type);
    case ABI_TYPE_NAMESPACE_LAMBDA:
      return encode_namespace_lambda_type(*type);
    }
    throw std::logic_error("unknown ABI type kind");
  }

  std::string builtin_code(const std::string & name) const
  {
    static const char * const names[] = {
      "void", "wchar", "bool", "char", "schar", "uchar", "short", "ushort",
      "int", "uint", "long", "ulong", "longlong", "ulonglong", "int128",
      "uint128", "float", "double", "longdouble", "float128", "ellipsis",
      "char16", "char32", "char8", "nullptr"
    };
    static const char * const codes[] = {
      "v", "w", "b", "c", "a", "h", "s", "t", "i", "j", "l", "m", "x",
      "y", "n", "o", "f", "d", "e", "g", "z", "Ds", "Di", "Du", "Dn"
    };
    for(std::size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
      if(name == names[i]) {
        return codes[i];
      }
    }
    if(name == "complex-float") return "Cf";
    if(name == "complex-double") return "Cd";
    if(name == "complex-longdouble") return "Ce";
    throw std::logic_error("unknown ABI builtin type '" + name + "'");
  }

  std::string template_parameter(std::size_t index) const
  {
    if(index == 0) {
      return "T_";
    }
    return "T" + number_string(index - 1) + "_";
  }

  std::string encode_cv_type(const AbiType & type)
  {
    bool is_const = false;
    bool is_volatile = false;
    const AbiType * base = &type;
    while(base->kind == ABI_TYPE_CV) {
      is_const = is_const || base->is_const;
      is_volatile = is_volatile || base->is_volatile;
      if(base->types.empty()) {
        throw std::logic_error("qualified ABI type has no base type");
      }
      base = &base->types[0];
    }
    std::string result;
    if(is_volatile) result += "V";
    if(is_const) result += "K";
    return result + encode_type_impl(*base);
  }

  std::string encode_array(const AbiType & type)
  {
    std::string result = "A";
    switch(type.array_bound.kind) {
    case ABI_ARRAY_BOUND_VALUE:
      result += type.array_bound.value;
      break;
    case ABI_ARRAY_BOUND_RAW:
      result += type.array_bound.value;
      break;
    case ABI_ARRAY_BOUND_EXPRESSION:
      result += encode_expression_reference(type.expression_ref);
      break;
    }
    result += "_";
    return result + encode_type_impl(type.types.at(0));
  }

  std::string encode_function_type(const AbiType & type)
  {
    if(type.types.empty()) {
      throw std::logic_error("function ABI type has no result type");
    }
    std::string result = "F";
    if(type.is_const) result += "K";
    if(type.is_volatile) result += "V";
    result += encode_type_impl(type.types[0]);
    if(type.types.size() == 1) {
      result += "v";
    } else {
      for(std::size_t i = 1; i < type.types.size(); ++i) {
        result += encode_type_impl(type.types[i]);
      }
    }
    if(type.variadic) result += "z";
    return result + "E";
  }

  std::string encode_template_specialization(const AbiType & type)
  {
    std::string prefix;
    if(type.kind == ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION) {
      prefix = template_parameter(type.index);
    } else if(!type.standard_substitution.empty()) {
      prefix = type.standard_substitution;
    } else {
      prefix = encode_name(type.name, type.abi_tags);
    }
    std::string result = prefix + "I";
    for(std::vector<std::string>::const_iterator it = type.argument_refs.begin();
        it != type.argument_refs.end(); ++it) {
      result += encode_argument(argument_definition(*it));
    }
    return result + "E";
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
      for(std::vector<std::string>::const_iterator it = argument.argument_refs.begin();
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
      return encode_type(argument.owner_type) + source_name(argument.name);
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
    if(type.name == "uint") { *bits = 32; return true; }
    if(type.name == "ulong") { *bits = 64; return true; }
    if(type.name == "uchar") { *bits = 8; return true; }
    if(type.name == "ushort") { *bits = 16; return true; }
    if(type.name == "ulonglong" || type.name == "uint128") {
      *bits = type.name == "uint128" ? 128 : 64;
      return true;
    }
    return false;
  }

  std::string encode_value_argument(const AbiTemplateArgument & argument)
  {
    if(!argument.has_value_type) {
      throw std::logic_error("typed ABI value is missing its value type");
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
      return "L" + entity.qualified_name + "E";
    }
    if(entity.kind == ABI_ENTITY_FACT_VARIABLE) {
      std::string result = "L_Z" + encode_name(entity.qualified_name,
                                                std::vector<std::string>()) + "E";
      return result;
    }
    return "L_Z" + encode_function(entity.function) + "E";
  }

  std::string encode_expression_reference(const std::string & id)
  {
    if(id.empty()) {
      throw std::logic_error("empty ABI expression reference");
    }
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_EXPRESSION) {
      throw std::logic_error("ABI id '" + id + "' is not an expression");
    }
    if(record.expression.kind == ABI_EXPRESSION_LITERAL ||
       record.expression.kind == ABI_EXPRESSION_INTEGRAL_VALUE) {
      return number_string(static_cast<unsigned long long>(record.expression.value));
    }
    return record.expression.text;
  }

  std::string encode_member_type(const AbiType & type)
  {
    if(type.types.empty() || type.name.empty()) {
      throw std::logic_error("member ABI type is incomplete");
    }
    return encode_type_impl(type.types[0]) + source_name(type.name);
  }

  std::string encode_lambda_type(const AbiType & type)
  {
    const AbiLocalContext & context = definition(type.context_ref).context;
    std::string result = context.kind == ABI_CONTEXT_RAW ? context.fragment :
      "Z" + encode_function(context.function) + "E";
    result += "Ul";
    if(type.types.empty()) result += "v";
    else for(std::vector<AbiType>::const_iterator it = type.types.begin();
              it != type.types.end(); ++it) result += encode_type_impl(*it);
    result += "E";
    if(!type.discriminator.empty() && type.discriminator != "0") result += type.discriminator;
    return result + "_";
  }

  std::string encode_namespace_lambda_type(const AbiType & type)
  {
    std::string result = "Ul" + type.name + "E";
    if(!type.discriminator.empty() && type.discriminator != "0") result += type.discriminator;
    result += "_";
    for(std::vector<std::string>::const_iterator it = type.namespace_qualifiers.begin();
        it != type.namespace_qualifiers.end(); ++it) {
      (void)it;
    }
    return result;
  }

  std::string encode_local_type(const AbiType & type)
  {
    const AbiLocalContext & context = definition(type.context_ref).context;
    std::string result = context.kind == ABI_CONTEXT_RAW ? context.fragment :
      "Z" + encode_function(context.function) + "E";
    result += "N" + source_name(type.name) + "E";
    if(!type.discriminator.empty() && type.discriminator != "0") result += "_" + type.discriminator;
    return result;
  }

  std::string encode_function_type_parameters(const AbiFunctionTarget & target,
                                              const std::vector<AbiFunctionRecord> & records)
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
    for(std::vector<AbiFunctionRecord>::const_iterator it = records.begin();
        it != records.end(); ++it) {
      if(it->kind != ABI_FUNCTION_RECORD_PARAMETER) continue;
      result += encode_type(it->type);
      has_parameter = true;
    }
    bool variadic = false;
    for(std::vector<AbiFunctionRecord>::const_iterator it = records.begin();
        it != records.end(); ++it) {
      if(it->kind == ABI_FUNCTION_RECORD_VARIADIC) variadic = true;
    }
    if(!has_parameter) result += "v";
    if(variadic) result += "z";
    return result;
  }

  std::vector<std::string> source_components(const std::vector<AbiFunctionRecord> & records) const
  {
    std::vector<std::string> components;
    for(std::vector<AbiFunctionRecord>::const_iterator it = records.begin();
        it != records.end(); ++it) {
      if(it->kind != ABI_FUNCTION_RECORD_NAME_SOURCE) continue;
      if(it->source_name.empty() || it->source_name == "-") continue;
      const std::vector<std::string> split = split_qualified_name(it->source_name);
      components.insert(components.end(), split.begin(), split.end());
    }
    return components;
  }

  std::string encode_components(const std::vector<std::string> & components,
                                const std::vector<std::string> & tags,
                                bool std_prefix)
  {
    if(components.empty()) {
      throw std::logic_error("ABI function has no name components");
    }
    std::string spelling;
    if(std_prefix) spelling = "std::";
    for(std::size_t i = 0; i < components.size(); ++i) {
      if(i != 0) spelling += "::";
      spelling += components[i];
    }
    return encode_name(spelling, tags);
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

  std::string encode_function_name(const AbiFunctionTarget & target,
                                   const std::vector<AbiFunctionRecord> & records)
  {
    if(target.kind == ABI_FUNCTION_TARGET_PATH && target.qualified_name.empty()) {
      throw std::logic_error("function path has an empty name");
    }
    std::vector<std::string> components;
    bool std_prefix = false;
    std::vector<std::string> tags;
    std::string terminal;
    std::string conversion;
    std::string literal;
    bool has_operator = false;
    bool constructor = false;
    bool destructor = false;
    for(std::vector<AbiFunctionRecord>::const_iterator it = records.begin();
        it != records.end(); ++it) {
      switch(it->kind) {
      case ABI_FUNCTION_RECORD_NAME_STD:
        std_prefix = true;
        break;
      case ABI_FUNCTION_RECORD_NAME_SOURCE:
        break;
      case ABI_FUNCTION_RECORD_TERMINAL:
        terminal = it->terminal;
        break;
      case ABI_FUNCTION_RECORD_TERMINAL_SOURCE:
        terminal = it->terminal;
        break;
      case ABI_FUNCTION_RECORD_OPERATOR_TERMINAL:
        terminal = it->terminal;
        literal = it->literal_suffix;
        has_operator = true;
        break;
      case ABI_FUNCTION_RECORD_CONVERSION_TERMINAL:
        conversion = it->terminal;
        break;
      case ABI_FUNCTION_RECORD_ABI_TAG:
        tags.push_back(it->name);
        break;
      default:
        break;
      }
    }
    if(target.kind == ABI_FUNCTION_TARGET_PATH) {
      components = split_qualified_name(target.qualified_name);
    } else {
      components = source_components(records);
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

    for(std::vector<std::string>::const_iterator it = components.begin();
        it != components.end(); ++it) {
      if(*it == "operator") has_operator = true;
    }
    if(terminal == "constructor-complete" || terminal == "constructor-base" ||
       terminal == "constructor-allocating") constructor = true;
    if(terminal == "destructor-deleting" || terminal == "destructor-complete" ||
       terminal == "destructor-base") destructor = true;

    if(constructor || destructor) {
      if(components.empty()) throw std::logic_error("special function has no owner");
      std::string result = "N";
      for(std::vector<std::string>::const_iterator it = components.begin();
          it != components.end(); ++it) {
        result += source_name(*it);
      }
      if(constructor) {
        result += terminal == "constructor-base" ? "C2" :
          (terminal == "constructor-allocating" ? "C3" : "C1");
      } else {
        result += terminal == "destructor-deleting" ? "D0" :
          (terminal == "destructor-base" ? "D2" : "D1");
      }
      return result + "E";
    }

    if(conversion.empty() && has_operator) {
      const std::string code = literal.empty() ? operator_code(terminal, false) :
        "li" + source_name(literal);
      if(components.size() > 1 && components.back() == "operator") components.pop_back();
      std::string result = encode_components(components, tags, std_prefix);
      if(result.size() >= 1 && result[result.size() - 1] == 'E') {
        result.erase(result.size() - 1);
        result += code + "E";
      } else {
        result += code;
      }
      return result;
    }
    if(!conversion.empty()) {
      if(components.size() > 1 && components.back() == "operator") components.pop_back();
      std::string result = encode_components(components, tags, std_prefix);
      if(result.size() >= 1 && result[result.size() - 1] == 'E') result.erase(result.size() - 1);
      result += "cv" + encode_type(parse_type_for_conversion(conversion)) + "E";
      return result;
    }
    if(target.kind == ABI_FUNCTION_TARGET_PATH) {
      return encode_name(target.qualified_name, tags);
    }
    return encode_components(components, tags, std_prefix);
  }

  AbiType parse_type_for_conversion(const std::string & name) const
  {
    AbiType result;
    if(name.find("::") != std::string::npos || name == "int" || name == "void") {
      result.kind = name == "int" || name == "void" ? ABI_TYPE_BUILTIN : ABI_TYPE_NAMED;
      result.name = name;
      return result;
    }
    result.kind = ABI_TYPE_BUILTIN;
    result.name = name;
    return result;
  }

  std::string encode_function(const AbiFunctionTarget & target)
  {
    std::vector<AbiFunctionRecord> records;
    for(std::vector<AbiFactRecord>::const_iterator it = fact_case_.records.begin();
        it != fact_case_.records.end(); ++it) {
      if(it->kind == ABI_FACT_RECORD_FUNCTION) records.push_back(it->function);
    }
    const std::string name = encode_function_name(target, records);
    const std::string params = encode_function_type_parameters(target, records);
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
