#include "abi_mangle.h"

#include <algorithm>
#include <map>
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
    : fact_case_(fact_case), substitution_trie_(1), next_substitution_index_(0)
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
      if(target->name.components.size() == 1) {
        return target->name.components[0];
      }
      std::string result = "_Z";
      append_data_name(target->name, result);
      return result;
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
      {
        std::string result = "_ZTW";
        append_data_name(target->name, result);
        return result;
      }
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

  // Substitution identity is semantic name structure, not rendered ABI text.
  // A trie stores each qualified-name edge once, so registering every ABI
  // prefix does not copy an ever-growing vector for each prefix.
  typedef std::size_t ComponentId;
  static const ComponentId INVALID_COMPONENT_ID = static_cast<ComponentId>(-1);
  static const std::size_t INVALID_SUBSTITUTION_INDEX = static_cast<std::size_t>(-1);

  struct TagKey
  {
    std::vector<ComponentId> components;

    bool operator<(const TagKey & other) const
    {
      return components < other.components;
    }
  };

  struct NameTrieNode
  {
    std::map<ComponentId, std::size_t> children;
    std::map<TagKey, std::size_t> tagged_substitutions;
    std::size_t untagged_substitution;

    NameTrieNode()
      : untagged_substitution(INVALID_SUBSTITUTION_INDEX)
    {}
  };

  std::map<std::string, ComponentId> component_indexes_;
  std::vector<std::string> component_spellings_;
  std::vector<NameTrieNode> substitution_trie_;
  std::size_t next_substitution_index_;

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

  ComponentId find_component(const std::string & spelling) const
  {
    std::map<std::string, ComponentId>::const_iterator found =
      component_indexes_.find(spelling);
    return found == component_indexes_.end() ? INVALID_COMPONENT_ID : found->second;
  }

  ComponentId intern_component(const std::string & spelling)
  {
    std::map<std::string, ComponentId>::const_iterator found =
      component_indexes_.find(spelling);
    if(found != component_indexes_.end()) return found->second;
    const ComponentId id = component_spellings_.size();
    component_indexes_.insert(std::make_pair(spelling, id));
    component_spellings_.push_back(spelling);
    return id;
  }

  std::vector<ComponentId> canonical_tag_ids(const std::vector<std::string> & tags,
                                              bool create,
                                              bool * known)
  {
    std::vector<ComponentId> result;
    *known = true;
    for(std::vector<std::string>::const_iterator it = tags.begin();
        it != tags.end(); ++it) {
      const ComponentId id = create ? intern_component(*it) : find_component(*it);
      if(id == INVALID_COMPONENT_ID) {
        *known = false;
        return std::vector<ComponentId>();
      }
      result.push_back(id);
    }
    std::sort(result.begin(), result.end(),
              [this](ComponentId left, ComponentId right) {
                return component_spellings_[left] < component_spellings_[right];
              });
    return result;
  }

  std::size_t find_path_node(const std::vector<std::string> & components) const
  {
    if(components.empty()) return 0;
    std::size_t node = 0;
    for(std::vector<std::string>::const_iterator it = components.begin();
        it != components.end(); ++it) {
      const ComponentId id = find_component(*it);
      if(id == INVALID_COMPONENT_ID) return INVALID_SUBSTITUTION_INDEX;
      std::map<ComponentId, std::size_t>::const_iterator child =
        substitution_trie_[node].children.find(id);
      if(child == substitution_trie_[node].children.end()) {
        return INVALID_SUBSTITUTION_INDEX;
      }
      node = child->second;
    }
    return node;
  }

  std::size_t ensure_path_child(std::size_t node, ComponentId component)
  {
    std::map<ComponentId, std::size_t>::const_iterator found =
      substitution_trie_[node].children.find(component);
    if(found != substitution_trie_[node].children.end()) return found->second;
    const std::size_t child = substitution_trie_.size();
    substitution_trie_.push_back(NameTrieNode());
    substitution_trie_[node].children.insert(std::make_pair(component, child));
    return child;
  }

  bool find_substitution(const std::vector<std::string> & components,
                         const std::vector<std::string> & tags,
                         std::size_t * index)
  {
    const std::size_t node = find_path_node(components);
    if(node == INVALID_SUBSTITUTION_INDEX) return false;
    bool known = false;
    const std::vector<ComponentId> tag_ids = canonical_tag_ids(tags, false, &known);
    if(!known) return false;
    if(tag_ids.empty()) {
      if(substitution_trie_[node].untagged_substitution == INVALID_SUBSTITUTION_INDEX) {
        return false;
      }
      *index = substitution_trie_[node].untagged_substitution;
      return true;
    }
    TagKey key;
    key.components = tag_ids;
    std::map<TagKey, std::size_t>::const_iterator found =
      substitution_trie_[node].tagged_substitutions.find(key);
    if(found == substitution_trie_[node].tagged_substitutions.end()) return false;
    *index = found->second;
    return true;
  }

  void add_substitution_at_node(std::size_t node,
                                const std::vector<std::string> & tags)
  {
    bool known = false;
    const std::vector<ComponentId> tag_ids = canonical_tag_ids(tags, true, &known);
    (void)known;
    if(tag_ids.empty()) {
      if(substitution_trie_[node].untagged_substitution == INVALID_SUBSTITUTION_INDEX) {
        substitution_trie_[node].untagged_substitution = next_substitution_index_++;
      }
      return;
    }
    TagKey key;
    key.components = tag_ids;
    if(substitution_trie_[node].tagged_substitutions.find(key) ==
       substitution_trie_[node].tagged_substitutions.end()) {
      substitution_trie_[node].tagged_substitutions.insert(
        std::make_pair(key, next_substitution_index_++));
    }
  }

  void add_substitution(const std::vector<std::string> & components,
                        const std::vector<std::string> & tags)
  {
    if(components.empty()) return;
    std::size_t node = 0;
    for(std::vector<std::string>::const_iterator it = components.begin();
        it != components.end(); ++it) {
      node = ensure_path_child(node, intern_component(*it));
    }
    add_substitution_at_node(node, tags);
  }

  void append_substitution(std::size_t index, std::string & output) const
  {
    static const char digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    output.push_back('S');
    if(index != 0) {
      std::size_t value = index - 1;
      char reversed[32];
      std::size_t count = 0;
      do {
        reversed[count++] = digits[value % 36];
        value /= 36;
      } while(value != 0);
      while(count != 0) output.push_back(reversed[--count]);
    }
    output.push_back('_');
  }

  std::size_t deepest_untagged_prefix(const std::vector<std::string> & components,
                                      std::size_t component_count,
                                      std::size_t * node)
  {
    std::size_t current = 0;
    std::size_t deepest = 0;
    *node = 0;
    for(std::size_t i = 0; i < component_count; ++i) {
      current = ensure_path_child(current, intern_component(components[i]));
      if(substitution_trie_[current].untagged_substitution !=
         INVALID_SUBSTITUTION_INDEX) {
        deepest = i + 1;
        *node = current;
      }
    }
    return deepest;
  }

  void append_scope_prefix(const std::vector<std::string> & components,
                           std::string & output)
  {
    if(components.empty()) throw std::logic_error("empty ABI scope prefix");
    if(components[0] == "std") {
      if(components.size() == 1) {
        output += "St";
      } else {
        output += "St";
        for(std::size_t i = 1; i < components.size(); ++i) {
          output += source_name(components[i]);
        }
      }
      return;
    }

    std::size_t node = 0;
    const std::size_t start = deepest_untagged_prefix(components, components.size(), &node);
    if(start != 0) {
      append_substitution(substitution_trie_[node].untagged_substitution, output);
    }
    for(std::size_t i = start; i < components.size(); ++i) {
      node = ensure_path_child(node, intern_component(components[i]));
      output += source_name(components[i]);
      if(substitution_trie_[node].untagged_substitution == INVALID_SUBSTITUTION_INDEX) {
        substitution_trie_[node].untagged_substitution = next_substitution_index_++;
      }
    }
  }

  void append_named_type_name(const AbiQualifiedName & name,
                              const std::vector<std::string> & tags,
                              std::string & output)
  {
    const std::vector<std::string> & components = name.components;
    if(components.empty()) throw std::logic_error("empty qualified ABI name");
    std::size_t index = 0;
    if(find_substitution(components, tags, &index)) {
      append_substitution(index, output);
      return;
    }

    const std::string suffix = tag_suffix(tags);
    if(components.size() == 1) {
      output += source_name(components[0]);
      output += suffix;
      add_substitution(components, tags);
      return;
    }
    if(components[0] == "std") {
      if(components.size() == 2) {
        output += "St";
        output += source_name(components[1]);
        output += suffix;
      } else {
        output += "NSt";
        for(std::size_t i = 1; i < components.size(); ++i) {
          output += source_name(components[i]);
          if(i + 1 == components.size()) output += suffix;
        }
        output.push_back('E');
      }
      add_substitution(components, tags);
      return;
    }

    output.push_back('N');
    std::size_t node = 0;
    const std::size_t prefix_count = components.size() - 1;
    const std::size_t start = deepest_untagged_prefix(components, prefix_count, &node);
    if(start != 0) {
      append_substitution(substitution_trie_[node].untagged_substitution, output);
    }
    for(std::size_t i = start; i < prefix_count; ++i) {
      node = ensure_path_child(node, intern_component(components[i]));
      output += source_name(components[i]);
      if(substitution_trie_[node].untagged_substitution == INVALID_SUBSTITUTION_INDEX) {
        substitution_trie_[node].untagged_substitution = next_substitution_index_++;
      }
    }
    node = ensure_path_child(node, intern_component(components.back()));
    output += source_name(components.back());
    output += suffix;
    add_substitution_at_node(node, tags);
    output.push_back('E');
  }

  void append_nested_name(const std::vector<std::string> & owner,
                          const std::vector<AbiFunctionQualifier> & qualifiers,
                          const std::string & terminal,
                          const std::vector<std::string> & tags,
                          std::string & output)
  {
    if(owner.empty()) {
      output += terminal;
      output += tag_suffix(tags);
      return;
    }
    const bool simple_std_function = owner.size() == 1 && owner[0] == "std" &&
      qualifiers.empty();
    if(simple_std_function) {
      output += "St";
      output += terminal;
      output += tag_suffix(tags);
      return;
    }

    output.push_back('N');
    append_function_qualifiers(qualifiers, output);
    append_scope_prefix(owner, output);
    output += terminal;
    output += tag_suffix(tags);
    output.push_back('E');
  }

  void append_data_name(const AbiQualifiedName & name, std::string & output)
  {
    if(name.components.empty()) throw std::logic_error("empty ABI data name");
    if(name.components.size() == 1) {
      output += source_name(name.components[0]);
      return;
    }
    std::vector<std::string> owner(name.components.begin(), name.components.end() - 1);
    append_nested_name(owner, std::vector<AbiFunctionQualifier>(),
                       source_name(name.components.back()),
                       std::vector<std::string>(), output);
  }

  std::string encode_name(const AbiQualifiedName & name,
                          const std::vector<std::string> & tags)
  {
    std::string result;
    append_named_type_name(name, tags, result);
    return result;
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
      append_named_type_name(type->name, type->abi_tags, output);
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
      output.push_back('I');
      append_type(type->types.at(0), output);
      output.push_back('E');
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
    if(type->is_volatile) output.push_back('V');
    if(type->is_const) output.push_back('K');
    append_type(type->types[0], output);
    if(type->types.size() == 1) {
      output.push_back('v');
    } else {
      for(std::size_t i = 1; i < type->types.size(); ++i) {
        append_type(type->types[i], output);
      }
    }
    if(type->variadic) output.push_back('z');
    if(type->lvalue_ref) output.push_back('R');
    if(type->rvalue_ref) output.push_back('O');
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

  unsigned long long decimal_discriminator(const std::string & spelling) const
  {
    if(spelling.empty() || spelling[0] == '-') {
      throw std::logic_error("invalid ABI discriminator");
    }
    std::istringstream input(spelling);
    unsigned long long value = 0;
    char extra = 0;
    if(!(input >> value) || (input >> extra)) {
      throw std::logic_error("invalid ABI discriminator");
    }
    return value;
  }

  std::string local_entity_discriminator(const std::string & spelling) const
  {
    if(spelling.empty()) return std::string();
    const unsigned long long ordinal = decimal_discriminator(spelling);
    if(ordinal == 0) return std::string();
    return "_" + number_string(ordinal - 1);
  }

  std::string lambda_discriminator(const std::string & spelling) const
  {
    if(spelling.empty()) return std::string();
    return number_string(decimal_discriminator(spelling));
  }

  void append_local_context_prefix(const AbiLocalContext & context,
                                   std::string & output)
  {
    if(context.kind == ABI_CONTEXT_RAW) {
      // This is the one explicitly normalized raw-context boundary in the
      // fact contract.  It is never reparsed into a second semantic model.
      output += context.fragment;
      return;
    }
    output.push_back('Z');
    append_function_target_encoding(context.function, output);
    output.push_back('E');
  }

  void append_lambda_type(const AbiType & type, std::string & output)
  {
    const AbiLocalContext & context = definition(type.context_ref).context;
    append_local_context_prefix(context, output);
    output += "Ul";
    if(type.types.empty()) output += "v";
    else for(std::vector<AbiType>::const_iterator it = type.types.begin();
              it != type.types.end(); ++it) append_type(*it, output);
    output.push_back('E');
    output += lambda_discriminator(type.discriminator);
    output.push_back('_');
  }

  void append_namespace_lambda_type(const AbiType & type, std::string & output)
  {
    if(type.name.components.size() != 1) {
      throw std::logic_error("namespace lambda source name is not a component");
    }
    output.push_back('N');
    for(std::vector<std::string>::const_iterator it = type.namespace_qualifiers.begin();
        it != type.namespace_qualifiers.end(); ++it) {
      output += source_name(*it);
    }
    output += source_name(type.name.components[0]);
    output.push_back('E');
  }

  void append_local_type(const AbiType & type, std::string & output)
  {
    const AbiLocalContext & context = definition(type.context_ref).context;
    append_local_context_prefix(context, output);
    if(type.name.components.size() != 1) {
      throw std::logic_error("local type name is not a component");
    }
    output.push_back('N');
    output += source_name(type.name.components[0]);
    output += local_entity_discriminator(type.discriminator);
    output.push_back('E');
  }

  struct FunctionFacts
  {
    bool std_prefix = false;
    bool variadic = false;
    std::string terminal;
    std::string literal_suffix;
    AbiFunctionSpecialTerminalKind special_terminal = ABI_SPECIAL_TERMINAL_NONE;
    AbiOperatorTerminalKind operator_terminal = ABI_OPERATOR_TERMINAL_NONE;
    bool has_conversion_type = false;
    AbiType conversion_type;
    std::vector<std::string> tags;
    std::vector<AbiFunctionQualifier> qualifiers;
    std::size_t parameter_count = 0;
    const AbiFunctionRecord * local_context = NULL;
    const AbiFunctionRecord * lambda_context = NULL;
    const AbiFunctionRecord * namespace_lambda_context = NULL;
  };

  void append_function_qualifiers(const std::vector<AbiFunctionQualifier> & qualifiers,
                                  std::string & output) const
  {
    bool is_const = false;
    bool is_volatile = false;
    bool lvalue = false;
    bool rvalue = false;
    for(std::vector<AbiFunctionQualifier>::const_iterator it = qualifiers.begin();
        it != qualifiers.end(); ++it) {
      if(*it == ABI_FUNCTION_QUALIFIER_CONST) is_const = true;
      else if(*it == ABI_FUNCTION_QUALIFIER_VOLATILE) is_volatile = true;
      else if(*it == ABI_FUNCTION_QUALIFIER_LVALUE_REFERENCE) lvalue = true;
      else if(*it == ABI_FUNCTION_QUALIFIER_RVALUE_REFERENCE) rvalue = true;
    }
    if(is_volatile) output.push_back('V');
    if(is_const) output.push_back('K');
    if(lvalue && rvalue) throw std::logic_error("conflicting ABI ref qualifiers");
    if(lvalue) output.push_back('R');
    if(rvalue) output.push_back('O');
  }

  void collect_function_facts(const AbiFunctionTarget & target,
                              FunctionFacts & facts) const
  {
    if(target.kind == ABI_FUNCTION_TARGET_PATH ||
       target.kind == ABI_FUNCTION_TARGET_LOCAL) {
      facts.parameter_count += target.signature_parameter_types.size();
    }
    if(target.operator_terminal != ABI_OPERATOR_TERMINAL_NONE) {
      facts.operator_terminal = target.operator_terminal;
    }
    for(std::vector<AbiFactRecord>::const_iterator it = fact_case_.records.begin();
        it != fact_case_.records.end(); ++it) {
      if(it->kind != ABI_FACT_RECORD_FUNCTION) continue;
      const AbiFunctionRecord & record = it->function;
      switch(record.kind) {
      case ABI_FUNCTION_RECORD_NAME_STD:
        facts.std_prefix = true;
        break;
      case ABI_FUNCTION_RECORD_TERMINAL:
      case ABI_FUNCTION_RECORD_TERMINAL_SOURCE:
        facts.terminal = record.terminal;
        facts.special_terminal = record.special_terminal;
        break;
      case ABI_FUNCTION_RECORD_OPERATOR_TERMINAL:
        facts.operator_terminal = record.operator_terminal;
        facts.literal_suffix = record.literal_suffix;
        break;
      case ABI_FUNCTION_RECORD_CONVERSION_TERMINAL:
        if(!record.has_conversion_type) {
          throw std::logic_error("conversion terminal has no typed conversion type");
        }
        facts.has_conversion_type = true;
        facts.conversion_type = record.conversion_type;
        break;
      case ABI_FUNCTION_RECORD_ABI_TAG:
        facts.tags.push_back(record.name);
        break;
      case ABI_FUNCTION_RECORD_QUALIFIER:
        facts.qualifiers.insert(facts.qualifiers.end(), record.qualifiers.begin(),
                                record.qualifiers.end());
        break;
      case ABI_FUNCTION_RECORD_VARIADIC:
        facts.variadic = true;
        break;
      case ABI_FUNCTION_RECORD_PARAMETER:
        ++facts.parameter_count;
        break;
      case ABI_FUNCTION_RECORD_LOCAL_CONTEXT:
        facts.local_context = &record;
        break;
      case ABI_FUNCTION_RECORD_LAMBDA_CONTEXT:
        facts.lambda_context = &record;
        break;
      case ABI_FUNCTION_RECORD_NAMESPACE_LAMBDA_CONTEXT:
        facts.namespace_lambda_context = &record;
        break;
      default:
        break;
      }
    }
  }

  AbiQualifiedName function_source_components() const
  {
    AbiQualifiedName components;
    for(std::vector<AbiFactRecord>::const_iterator it = fact_case_.records.begin();
        it != fact_case_.records.end(); ++it) {
      if(it->kind != ABI_FACT_RECORD_FUNCTION ||
         it->function.kind != ABI_FUNCTION_RECORD_NAME_SOURCE) continue;
      components.components.insert(components.components.end(),
                                   it->function.source_name.components.begin(),
                                   it->function.source_name.components.end());
    }
    return components;
  }

  std::string special_terminal_code(AbiFunctionSpecialTerminalKind terminal) const
  {
    switch(terminal) {
    case ABI_SPECIAL_TERMINAL_CONSTRUCTOR_COMPLETE: return "C1";
    case ABI_SPECIAL_TERMINAL_CONSTRUCTOR_BASE: return "C2";
    case ABI_SPECIAL_TERMINAL_CONSTRUCTOR_ALLOCATING: return "C3";
    case ABI_SPECIAL_TERMINAL_DESTRUCTOR_DELETING: return "D0";
    case ABI_SPECIAL_TERMINAL_DESTRUCTOR_COMPLETE: return "D1";
    case ABI_SPECIAL_TERMINAL_DESTRUCTOR_BASE: return "D2";
    case ABI_SPECIAL_TERMINAL_NONE: break;
    }
    throw std::logic_error("unknown ABI special function terminal");
  }

  std::string operator_code(AbiOperatorTerminalKind terminal,
                            bool member,
                            std::size_t parameter_count,
                            const std::string & literal_suffix) const
  {
    switch(terminal) {
    case ABI_OPERATOR_TERMINAL_NEW: return "nw";
    case ABI_OPERATOR_TERMINAL_NEW_ARRAY: return "na";
    case ABI_OPERATOR_TERMINAL_DELETE: return "dl";
    case ABI_OPERATOR_TERMINAL_DELETE_ARRAY: return "da";
    case ABI_OPERATOR_TERMINAL_UNARY_PLUS: return "ps";
    case ABI_OPERATOR_TERMINAL_BINARY_PLUS: return "pl";
    case ABI_OPERATOR_TERMINAL_PLUS:
      return member || parameter_count > 1 ? "pl" : "ps";
    case ABI_OPERATOR_TERMINAL_UNARY_MINUS: return "ng";
    case ABI_OPERATOR_TERMINAL_BINARY_MINUS: return "mi";
    case ABI_OPERATOR_TERMINAL_MINUS:
      return member || parameter_count > 1 ? "mi" : "ng";
    case ABI_OPERATOR_TERMINAL_ADDRESS_OF: return "ad";
    case ABI_OPERATOR_TERMINAL_DEREF: return "de";
    case ABI_OPERATOR_TERMINAL_COMPLEMENT: return "co";
    case ABI_OPERATOR_TERMINAL_MULTIPLY: return "ml";
    case ABI_OPERATOR_TERMINAL_DIVIDE: return "dv";
    case ABI_OPERATOR_TERMINAL_REMAINDER: return "rm";
    case ABI_OPERATOR_TERMINAL_BIT_AND: return "an";
    case ABI_OPERATOR_TERMINAL_BIT_OR: return "or";
    case ABI_OPERATOR_TERMINAL_BIT_XOR: return "eo";
    case ABI_OPERATOR_TERMINAL_PLUS_ASSIGN: return "pL";
    case ABI_OPERATOR_TERMINAL_MINUS_ASSIGN: return "mI";
    case ABI_OPERATOR_TERMINAL_MULTIPLY_ASSIGN: return "mL";
    case ABI_OPERATOR_TERMINAL_DIVIDE_ASSIGN: return "dV";
    case ABI_OPERATOR_TERMINAL_REMAINDER_ASSIGN: return "rM";
    case ABI_OPERATOR_TERMINAL_BIT_AND_ASSIGN: return "aN";
    case ABI_OPERATOR_TERMINAL_BIT_OR_ASSIGN: return "oR";
    case ABI_OPERATOR_TERMINAL_BIT_XOR_ASSIGN: return "eO";
    case ABI_OPERATOR_TERMINAL_LEFT_SHIFT: return "ls";
    case ABI_OPERATOR_TERMINAL_RIGHT_SHIFT: return "rs";
    case ABI_OPERATOR_TERMINAL_LEFT_SHIFT_ASSIGN: return "lS";
    case ABI_OPERATOR_TERMINAL_RIGHT_SHIFT_ASSIGN: return "rS";
    case ABI_OPERATOR_TERMINAL_EQUAL: return "eq";
    case ABI_OPERATOR_TERMINAL_NOT_EQUAL: return "ne";
    case ABI_OPERATOR_TERMINAL_LESS: return "lt";
    case ABI_OPERATOR_TERMINAL_GREATER: return "gt";
    case ABI_OPERATOR_TERMINAL_LESS_EQUAL: return "le";
    case ABI_OPERATOR_TERMINAL_GREATER_EQUAL: return "ge";
    case ABI_OPERATOR_TERMINAL_LOGICAL_NOT: return "nt";
    case ABI_OPERATOR_TERMINAL_LOGICAL_AND: return "aa";
    case ABI_OPERATOR_TERMINAL_LOGICAL_OR: return "oo";
    case ABI_OPERATOR_TERMINAL_INCREMENT: return "pp";
    case ABI_OPERATOR_TERMINAL_DECREMENT: return "mm";
    case ABI_OPERATOR_TERMINAL_COMMA: return "cm";
    case ABI_OPERATOR_TERMINAL_MEMBER_POINTER: return "pm";
    case ABI_OPERATOR_TERMINAL_ARROW: return "pt";
    case ABI_OPERATOR_TERMINAL_CALL: return "cl";
    case ABI_OPERATOR_TERMINAL_INDEX: return "ix";
    case ABI_OPERATOR_TERMINAL_LITERAL:
      return "li" + source_name(literal_suffix);
    case ABI_OPERATOR_TERMINAL_NONE: break;
    }
    throw std::logic_error("unknown ABI operator terminal");
  }

  void append_conversion_name(const std::vector<std::string> & components,
                              const FunctionFacts & facts,
                              std::string & output)
  {
    if(!facts.has_conversion_type) {
      throw std::logic_error("conversion terminal has no typed conversion type");
    }
    std::vector<std::string> owner = components;
    if(!owner.empty() && owner.back() == "operator") owner.pop_back();
    if(owner.empty()) throw std::logic_error("conversion function has no owner");
    output.push_back('N');
    append_function_qualifiers(facts.qualifiers, output);
    append_scope_prefix(owner, output);
    output += "cv";
    append_type(facts.conversion_type, output);
    output += tag_suffix(facts.tags);
    output.push_back('E');
  }

  void append_function_name_from_components(const AbiQualifiedName & original,
                                            const FunctionFacts & facts,
                                            std::string & output)
  {
    AbiQualifiedName components = original;
    if(facts.std_prefix) {
      if(components.components.empty() || components.components[0] != "std") {
        components.components.insert(components.components.begin(), "std");
      }
    }
    if(components.components.empty()) throw std::logic_error("ABI function has no name components");

    const bool has_special = facts.special_terminal != ABI_SPECIAL_TERMINAL_NONE;
    const bool has_operator = facts.operator_terminal != ABI_OPERATOR_TERMINAL_NONE;
    if(has_special) {
      append_nested_name(components.components, facts.qualifiers,
                         special_terminal_code(facts.special_terminal), facts.tags, output);
      return;
    }
    if(has_operator) {
      if(!components.components.empty() && components.components.back() == "operator") {
        components.components.pop_back();
      }
      const bool member = !components.components.empty();
      append_nested_name(components.components, facts.qualifiers,
                         operator_code(facts.operator_terminal, member,
                                       facts.parameter_count, facts.literal_suffix),
                         facts.tags, output);
      return;
    }
    if(facts.has_conversion_type) {
      append_conversion_name(components.components, facts, output);
      return;
    }

    if(components.components.size() == 1 && facts.qualifiers.empty()) {
      output += source_name(components.components[0]);
      output += tag_suffix(facts.tags);
      return;
    }
    std::vector<std::string> owner(components.components.begin(), components.components.end() - 1);
    append_nested_name(owner, facts.qualifiers,
                       source_name(components.components.back()), facts.tags, output);
  }

  void append_namespace_lambda_function(const std::string & source,
                                        const std::vector<std::string> & namespaces,
                                        const FunctionFacts & facts,
                                        std::string & output)
  {
    if(source.empty()) throw std::logic_error("namespace lambda has no source name");
    output.push_back('N');
    append_function_qualifiers(facts.qualifiers, output);
    if(!namespaces.empty()) append_scope_prefix(namespaces, output);
    output += source_name(source);
    if(facts.operator_terminal != ABI_OPERATOR_TERMINAL_NONE) {
      output += operator_code(facts.operator_terminal, true,
                              facts.parameter_count, facts.literal_suffix);
    } else if(!facts.terminal.empty()) {
      output += source_name(facts.terminal);
    } else {
      throw std::logic_error("namespace lambda has no terminal");
    }
    output += tag_suffix(facts.tags);
    output.push_back('E');
  }

  void append_local_function_name(const AbiFunctionTarget & target,
                                  const FunctionFacts & facts,
                                  std::string & output)
  {
    const AbiLocalContext & context = definition(target.context_ref).context;
    append_local_context_prefix(context, output);
    output.push_back('N');
    append_function_qualifiers(facts.qualifiers, output);
    output += source_name(target.source_name);
    output += local_entity_discriminator(target.discriminator);
    if(target.operator_terminal != ABI_OPERATOR_TERMINAL_NONE) {
      output += operator_code(target.operator_terminal, true,
                              facts.parameter_count, facts.literal_suffix);
    } else if(target.special_terminal != ABI_SPECIAL_TERMINAL_NONE) {
      output += special_terminal_code(target.special_terminal);
    } else if(!target.terminal.empty()) {
      output += source_name(target.terminal);
    } else {
      throw std::logic_error("local function has no terminal");
    }
    output += tag_suffix(facts.tags);
    output.push_back('E');
  }

  void append_local_context_function_name(const AbiFunctionRecord & record,
                                          const FunctionFacts & facts,
                                          std::string & output)
  {
    const AbiLocalContext & context = definition(record.context_ref).context;
    append_local_context_prefix(context, output);
    output.push_back('N');
    append_function_qualifiers(facts.qualifiers, output);
    if(record.kind == ABI_FUNCTION_RECORD_LOCAL_CONTEXT) {
      if(record.source_name.components.size() != 1) {
        throw std::logic_error("local context name is not a source component");
      }
      output += source_name(record.source_name.components[0]);
      output += local_entity_discriminator(record.discriminator);
    } else {
      output += "Ul";
      if(record.types.empty()) output += "v";
      else for(std::vector<AbiType>::const_iterator it = record.types.begin();
                it != record.types.end(); ++it) append_type(*it, output);
      output.push_back('E');
      output += lambda_discriminator(record.discriminator);
      output.push_back('_');
    }
    if(facts.operator_terminal != ABI_OPERATOR_TERMINAL_NONE) {
      output += operator_code(facts.operator_terminal, true,
                              facts.parameter_count, facts.literal_suffix);
    } else if(facts.special_terminal != ABI_SPECIAL_TERMINAL_NONE) {
      output += special_terminal_code(facts.special_terminal);
    } else if(!facts.terminal.empty()) {
      output += source_name(facts.terminal);
    } else if(record.kind == ABI_FUNCTION_RECORD_LOCAL_CONTEXT &&
              record.source_name.components.size() == 1 &&
              !record.source_name.components[0].empty() &&
              record.source_name.components[0][0] == '$') {
      // A normalized local lambda call may use its source-name directly in
      // local-context form; its terminal is the closure call operator.
      output += "cl";
    } else {
      throw std::logic_error("local context has no terminal");
    }
    output += tag_suffix(facts.tags);
    output.push_back('E');
  }

  void append_function_name(const AbiFunctionTarget & target,
                            const FunctionFacts & facts,
                            std::string & output)
  {
    if(target.kind == ABI_FUNCTION_TARGET_LOCAL ||
       target.kind == ABI_FUNCTION_TARGET_LAMBDA) {
      if(target.source_name.empty() && target.kind == ABI_FUNCTION_TARGET_LOCAL) {
        throw std::logic_error("local function has no entity name");
      }
      if(target.kind == ABI_FUNCTION_TARGET_LAMBDA) {
        const AbiLocalContext & context = definition(target.context_ref).context;
        append_local_context_prefix(context, output);
        output.push_back('N');
        append_function_qualifiers(facts.qualifiers, output);
        output += "Ul";
        if(target.signature_parameter_types.empty()) output += "v";
        else for(std::vector<AbiType>::const_iterator it = target.signature_parameter_types.begin();
                  it != target.signature_parameter_types.end(); ++it) append_type(*it, output);
        output.push_back('E');
        output += lambda_discriminator(target.discriminator);
        output.push_back('_');
        if(target.operator_terminal != ABI_OPERATOR_TERMINAL_NONE) {
          output += operator_code(target.operator_terminal, true,
                                  facts.parameter_count, facts.literal_suffix);
        } else if(target.special_terminal != ABI_SPECIAL_TERMINAL_NONE) {
          output += special_terminal_code(target.special_terminal);
        } else if(!target.terminal.empty()) {
          output += source_name(target.terminal);
        } else {
          throw std::logic_error("lambda function has no terminal");
        }
        output += tag_suffix(facts.tags);
        output.push_back('E');
        return;
      }
      append_local_function_name(target, facts, output);
      return;
    }
    if(target.kind == ABI_FUNCTION_TARGET_NAMESPACE_LAMBDA) {
      append_namespace_lambda_function(target.source_name, target.namespace_qualifiers,
                                       facts, output);
      return;
    }
    if(target.kind == ABI_FUNCTION_TARGET_ENCODING) {
      if(facts.local_context != NULL) {
        append_local_context_function_name(*facts.local_context, facts, output);
        return;
      }
      if(facts.lambda_context != NULL) {
        append_local_context_function_name(*facts.lambda_context, facts, output);
        return;
      }
      if(facts.namespace_lambda_context != NULL) {
        const AbiFunctionRecord & record = *facts.namespace_lambda_context;
        if(record.source_name.components.size() != 1) {
          throw std::logic_error("namespace lambda source name is not a component");
        }
        append_namespace_lambda_function(record.source_name.components[0],
                                         record.namespace_qualifiers, facts, output);
        return;
      }
      append_function_name_from_components(function_source_components(), facts, output);
      return;
    }
    append_function_name_from_components(target.name, facts, output);
  }

  void append_function_parameters(const AbiFunctionTarget & target,
                                  const FunctionFacts & facts,
                                  std::string & output,
                                  bool include_case_records = true)
  {
    bool has_parameter = false;
    if(target.kind == ABI_FUNCTION_TARGET_PATH) {
      for(std::vector<AbiFunctionPathOperand>::const_iterator it = target.path_operands.begin();
          it != target.path_operands.end(); ++it) {
        if(it->kind != ABI_FUNCTION_PATH_TYPE) continue;
        append_type(it->type, output);
        has_parameter = true;
      }
    }
    if(target.kind == ABI_FUNCTION_TARGET_PATH ||
       target.kind == ABI_FUNCTION_TARGET_LOCAL) {
      for(std::vector<AbiType>::const_iterator it = target.signature_parameter_types.begin();
          it != target.signature_parameter_types.end(); ++it) {
        append_type(*it, output);
        has_parameter = true;
      }
    }
    if(include_case_records) {
      for(std::vector<AbiFactRecord>::const_iterator it = fact_case_.records.begin();
          it != fact_case_.records.end(); ++it) {
        if(it->kind != ABI_FACT_RECORD_FUNCTION ||
           it->function.kind != ABI_FUNCTION_RECORD_PARAMETER) continue;
        append_type(it->function.type, output);
        has_parameter = true;
      }
    }
    if(!has_parameter) output.push_back('v');
    if(facts.variadic) output.push_back('z');
  }

  void append_function_target_encoding(const AbiFunctionTarget & target,
                                       std::string & output)
  {
    FunctionFacts facts;
    if(target.operator_terminal != ABI_OPERATOR_TERMINAL_NONE) {
      facts.operator_terminal = target.operator_terminal;
    }
    facts.parameter_count = target.signature_parameter_types.size();
    append_function_name(target, facts, output);
    append_function_parameters(target, facts, output, false);
  }

  std::string encode_function(const AbiFunctionTarget & target)
  {
    std::string result = "_Z";
    FunctionFacts facts;
    collect_function_facts(target, facts);
    append_function_name(target, facts, result);
    append_function_parameters(target, facts, result);
    return result;
  }

  void append_signed_offset(long long value, std::string & output) const
  {
    if(value < 0) {
      const unsigned long long raw = static_cast<unsigned long long>(value);
      output.push_back('n');
      output += number_string(0 - raw);
    } else {
      output += number_string(static_cast<unsigned long long>(value));
    }
  }

  void append_nonvirtual_call_offset(long long value, std::string & output) const
  {
    output.push_back('h');
    append_signed_offset(value, output);
    output.push_back('_');
  }

  void append_virtual_call_offset(long long fixed,
                                  long long vcall,
                                  std::string & output) const
  {
    output.push_back('v');
    append_signed_offset(fixed, output);
    output.push_back('_');
    append_signed_offset(vcall, output);
    output.push_back('_');
  }

  std::string encode_thunk(const AbiTargetRecord & target)
  {
    std::string result = "_ZT";
    if(target.kind == ABI_TARGET_FACT_VIRTUAL_BASE_THUNK) {
      append_virtual_call_offset(0, target.this_adjust, result);
    } else if(target.result_adjust_virtual) {
      result.push_back('c');
      append_nonvirtual_call_offset(target.this_adjust, result);
      append_virtual_call_offset(target.result_adjust, target.result_vcall_offset, result);
    } else if(target.has_result_adjust) {
      result.push_back('c');
      append_nonvirtual_call_offset(target.this_adjust, result);
      append_nonvirtual_call_offset(target.result_adjust, result);
    } else {
      append_nonvirtual_call_offset(target.this_adjust, result);
    }
    append_function_target_encoding(target.function, result);
    return result;
  }
};

}  // namespace

std::string mangle_abi_fact_case(const AbiFactCase & fact_case)
{
  return FactEncoder(fact_case).encode();
}

}  // namespace abi_mangle
