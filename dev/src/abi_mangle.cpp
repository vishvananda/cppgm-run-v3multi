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
    : fact_case_(fact_case), substitution_state_()
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
    active_identity_definitions_.assign(definitions_.size(), 0);
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
  typedef std::size_t StructuralId;
  static const ComponentId INVALID_COMPONENT_ID = static_cast<ComponentId>(-1);
  static const StructuralId INVALID_STRUCTURAL_ID = static_cast<StructuralId>(-1);
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
    StructuralId path_identity;

    NameTrieNode()
      : untagged_substitution(INVALID_SUBSTITUTION_INDEX),
        path_identity(INVALID_STRUCTURAL_ID)
    {}
  };

  struct SubstitutionState
  {
    std::vector<NameTrieNode> substitution_trie;
    std::map<StructuralId, std::size_t> structural_substitution_indexes;
    std::size_t next_substitution_index;
    bool pending_function_prefix_candidate;
    StructuralId pending_function_prefix_identity;

    SubstitutionState()
      : substitution_trie(1),
        next_substitution_index(0),
        pending_function_prefix_candidate(false),
        pending_function_prefix_identity(INVALID_STRUCTURAL_ID)
    {}

    void swap(SubstitutionState & other)
    {
      substitution_trie.swap(other.substitution_trie);
      structural_substitution_indexes.swap(other.structural_substitution_indexes);
      std::swap(next_substitution_index, other.next_substitution_index);
      std::swap(pending_function_prefix_candidate,
                other.pending_function_prefix_candidate);
      std::swap(pending_function_prefix_identity,
                other.pending_function_prefix_identity);
    }
  };

  std::map<std::string, ComponentId> component_indexes_;
  std::vector<std::string> component_spellings_;

  struct StructuralKey
  {
    unsigned int domain;
    unsigned int kind;
    std::vector<unsigned long long> scalars;
    std::vector<ComponentId> components;
    std::vector<StructuralId> children;

    StructuralKey()
      : domain(0), kind(0)
    {}

    bool operator<(const StructuralKey & other) const
    {
      if(domain != other.domain) return domain < other.domain;
      if(kind != other.kind) return kind < other.kind;
      if(scalars != other.scalars) return scalars < other.scalars;
      if(components != other.components) return components < other.components;
      return children < other.children;
    }
  };

  enum StructuralDomain
  {
    STRUCTURAL_NAME,
    STRUCTURAL_TEMPLATE_PREFIX,
    STRUCTURAL_TEMPLATE_ID,
    STRUCTURAL_TYPE,
    STRUCTURAL_ARGUMENT,
    STRUCTURAL_EXPRESSION,
    STRUCTURAL_ENTITY,
    STRUCTURAL_CONTEXT,
    STRUCTURAL_FUNCTION_PREFIX,
    STRUCTURAL_FUNCTION_TARGET
  };

  std::map<StructuralKey, StructuralId> structural_indexes_;
  std::vector<StructuralKey> structural_nodes_;
  std::map<std::size_t, StructuralId> type_definition_identities_;
  std::map<std::size_t, StructuralId> argument_definition_identities_;
  std::map<std::size_t, StructuralId> expression_definition_identities_;
  std::map<std::size_t, StructuralId> entity_definition_identities_;
  std::map<std::size_t, StructuralId> context_definition_identities_;
  std::vector<unsigned char> active_identity_definitions_;
  SubstitutionState substitution_state_;

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

  StructuralId intern_structural(const StructuralKey & key)
  {
    std::map<StructuralKey, StructuralId>::const_iterator found =
      structural_indexes_.find(key);
    if(found != structural_indexes_.end()) return found->second;
    const StructuralId id = structural_nodes_.size();
    structural_indexes_.insert(std::make_pair(key, id));
    structural_nodes_.push_back(key);
    return id;
  }

  void append_name_identity_fields(StructuralKey & key,
                                   const std::vector<std::string> & components,
                                   const std::vector<std::string> & tags)
  {
    key.scalars.push_back(components.size());
    key.scalars.push_back(tags.size());
    for(std::vector<std::string>::const_iterator it = components.begin();
        it != components.end(); ++it) {
      key.components.push_back(intern_component(*it));
    }
    std::vector<ComponentId> tag_ids;
    for(std::vector<std::string>::const_iterator it = tags.begin();
        it != tags.end(); ++it) {
      tag_ids.push_back(intern_component(*it));
    }
    std::sort(tag_ids.begin(), tag_ids.end());
    key.components.insert(key.components.end(), tag_ids.begin(), tag_ids.end());
  }

  void append_name_identity_fields(StructuralKey & key,
                                   const AbiQualifiedName & name,
                                   const std::vector<std::string> & tags)
  {
    append_name_identity_fields(key, name.components, tags);
  }

  StructuralId name_identity(const std::vector<std::string> & components,
                             const std::vector<std::string> & tags)
  {
    if(components.empty()) throw std::logic_error("empty ABI name identity");
    StructuralId path = INVALID_STRUCTURAL_ID;
    std::size_t node = 0;
    for(std::vector<std::string>::const_iterator it = components.begin();
        it != components.end(); ++it) {
      node = ensure_path_child(node, intern_component(*it));
      path = substitution_state_.substitution_trie[node].path_identity;
    }
    if(tags.empty()) return path;
    StructuralKey key;
    key.domain = STRUCTURAL_NAME;
    key.kind = 2;
    key.scalars.push_back(tags.size());
    key.children.push_back(path);
    for(std::vector<std::string>::const_iterator it = tags.begin();
        it != tags.end(); ++it) {
      key.components.push_back(intern_component(*it));
    }
    std::sort(key.components.begin(), key.components.end());
    return intern_structural(key);
  }

  StructuralId name_component_identity(const std::string & component)
  {
    const std::size_t node = ensure_path_child(0, intern_component(component));
    return substitution_state_.substitution_trie[node].path_identity;
  }

  StructuralId name_identity_with_path(StructuralId path,
                                       const std::vector<std::string> & tags)
  {
    if(path == INVALID_STRUCTURAL_ID) {
      throw std::logic_error("empty ABI name path identity");
    }
    if(tags.empty()) return path;
    StructuralKey key;
    key.domain = STRUCTURAL_NAME;
    key.kind = 2;
    key.scalars.push_back(tags.size());
    key.children.push_back(path);
    for(std::vector<std::string>::const_iterator it = tags.begin();
        it != tags.end(); ++it) {
      key.components.push_back(intern_component(*it));
    }
    std::sort(key.components.begin(), key.components.end());
    return intern_structural(key);
  }

  AbiStandardSubstitutionKind standard_kind_from_spelling(
    const std::string & spelling) const
  {
    if(spelling.empty() || spelling == "-") return ABI_STANDARD_SUBSTITUTION_NONE;
    if(spelling == "Sa") return ABI_STANDARD_SUBSTITUTION_ALLOCATOR;
    if(spelling == "Sb") return ABI_STANDARD_SUBSTITUTION_BASIC_STRING;
    if(spelling == "Ss") return ABI_STANDARD_SUBSTITUTION_STRING;
    if(spelling == "Si") return ABI_STANDARD_SUBSTITUTION_ISTREAM;
    if(spelling == "So") return ABI_STANDARD_SUBSTITUTION_OSTREAM;
    if(spelling == "Sd") return ABI_STANDARD_SUBSTITUTION_IOSTREAM;
    throw std::logic_error("unknown ABI standard substitution");
  }

  AbiStandardSubstitutionKind effective_standard_kind(
    AbiStandardSubstitutionKind kind, const std::string & spelling) const
  {
    if(kind != ABI_STANDARD_SUBSTITUTION_NONE) return kind;
    return standard_kind_from_spelling(spelling);
  }

  std::string standard_substitution_code(AbiStandardSubstitutionKind kind) const
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
    throw std::logic_error("missing ABI standard substitution");
  }

  unsigned long long normalized_integral_value(const AbiType & type,
                                              long long value) const
  {
    const AbiType * base = &type;
    while(base->kind == ABI_TYPE_CV) {
      if(base->types.empty()) throw std::logic_error("qualified value type has no base");
      base = &base->types[0];
    }
    unsigned int bits = 0;
    if(base->kind == ABI_TYPE_BUILTIN) {
      switch(base->builtin) {
      case ABI_BUILTIN_UNSIGNED_CHAR: bits = 8; break;
      case ABI_BUILTIN_UNSIGNED_SHORT: bits = 16; break;
      case ABI_BUILTIN_UNSIGNED_INT: bits = 32; break;
      case ABI_BUILTIN_UNSIGNED_LONG:
      case ABI_BUILTIN_UNSIGNED_LONG_LONG: bits = 64; break;
      default: break;
      }
    }
    unsigned long long raw = static_cast<unsigned long long>(value);
    if(bits != 0 && bits < 64) {
      raw &= (static_cast<unsigned long long>(1) << bits) - 1;
    }
    return raw;
  }

  StructuralId type_definition_identity(const AbiDefinitionId & id)
  {
    std::map<std::size_t, StructuralId>::const_iterator found =
      type_definition_identities_.find(id.index);
    if(found != type_definition_identities_.end()) return found->second;
    if(id.index >= active_identity_definitions_.size() ||
       active_identity_definitions_[id.index] != 0) {
      throw std::logic_error("cyclic ABI type identity");
    }
    active_identity_definitions_[id.index] = 1;
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_TYPE) {
      active_identity_definitions_[id.index] = 0;
      throw std::logic_error("ABI definition is not a type identity");
    }
    const StructuralId result = type_identity(record.type);
    active_identity_definitions_[id.index] = 0;
    type_definition_identities_.insert(std::make_pair(id.index, result));
    return result;
  }

  StructuralId type_identity(const AbiType & original)
  {
    if(original.kind == ABI_TYPE_NAME_OR_REFERENCE &&
       original.definition_ref.index != ABI_INVALID_DEFINITION_ID) {
      return type_definition_identity(original.definition_ref);
    }

    if(original.kind == ABI_TYPE_CV) {
      bool is_const = false;
      bool is_volatile = false;
      const AbiType * base = &original;
      while(base->kind == ABI_TYPE_CV) {
        is_const = is_const || base->is_const;
        is_volatile = is_volatile || base->is_volatile;
        if(base->types.empty()) throw std::logic_error("qualified ABI type has no base");
        base = &base->types[0];
      }
      if(!is_const && !is_volatile) return type_identity(*base);
      StructuralKey key;
      key.domain = STRUCTURAL_TYPE;
      key.kind = ABI_TYPE_CV;
      key.scalars.push_back(is_const ? 1 : 0);
      key.scalars.push_back(is_volatile ? 1 : 0);
      key.children.push_back(type_identity(*base));
      return intern_structural(key);
    }

    StructuralKey key;
    key.domain = STRUCTURAL_TYPE;
    switch(original.kind) {
    case ABI_TYPE_NAME_OR_REFERENCE:
    case ABI_TYPE_NAMED:
      key.kind = ABI_TYPE_NAMED;
      append_name_identity_fields(key, original.name, original.abi_tags);
      break;
    case ABI_TYPE_BUILTIN:
      key.kind = ABI_TYPE_BUILTIN;
      key.scalars.push_back(original.builtin);
      break;
    case ABI_TYPE_TEMPLATE_PARAMETER:
      key.kind = ABI_TYPE_TEMPLATE_PARAMETER;
      key.scalars.push_back(original.index);
      break;
    case ABI_TYPE_POINTER:
    case ABI_TYPE_LVALUE_REFERENCE:
    case ABI_TYPE_RVALUE_REFERENCE:
    case ABI_TYPE_PACK_EXPANSION:
      key.kind = original.kind;
      if(original.types.empty()) throw std::logic_error("unary ABI type has no child");
      key.children.push_back(type_identity(original.types[0]));
      break;
    case ABI_TYPE_VENDOR_QUALIFIED:
    case ABI_TYPE_BUILTIN_TRANSFORM:
      key.kind = original.kind;
      append_name_identity_fields(key, original.name, std::vector<std::string>());
      if(original.types.empty()) throw std::logic_error("qualified ABI type has no child");
      key.children.push_back(type_identity(original.types[0]));
      break;
    case ABI_TYPE_ARRAY:
      key.kind = ABI_TYPE_ARRAY;
      key.scalars.push_back(original.array_bound.kind);
      if(original.array_bound.kind == ABI_ARRAY_BOUND_VALUE) {
        key.scalars.push_back(original.array_bound.value);
      } else if(original.array_bound.kind == ABI_ARRAY_BOUND_RAW) {
        key.components.push_back(intern_component(original.array_bound.raw));
      } else {
        key.children.push_back(expression_identity(original.array_bound.expression_ref));
      }
      if(original.types.empty()) throw std::logic_error("array ABI type has no element");
      key.children.push_back(type_identity(original.types[0]));
      break;
    case ABI_TYPE_FUNCTION:
      key.kind = ABI_TYPE_FUNCTION;
      key.scalars.push_back(original.is_const ? 1 : 0);
      key.scalars.push_back(original.is_volatile ? 1 : 0);
      key.scalars.push_back(original.variadic ? 1 : 0);
      key.scalars.push_back(original.lvalue_ref ? 1 : 0);
      key.scalars.push_back(original.rvalue_ref ? 1 : 0);
      for(std::vector<AbiType>::const_iterator it = original.types.begin();
          it != original.types.end(); ++it) key.children.push_back(type_identity(*it));
      break;
    case ABI_TYPE_MEMBER_POINTER:
      key.kind = ABI_TYPE_MEMBER_POINTER;
      key.scalars.push_back(original.types.size());
      for(std::vector<AbiType>::const_iterator it = original.types.begin();
          it != original.types.end(); ++it) key.children.push_back(type_identity(*it));
      break;
    case ABI_TYPE_TEMPLATE_SPECIALIZATION:
    case ABI_TYPE_STD_TEMPLATE_SPECIALIZATION:
    case ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION:
      key.kind = original.kind;
      if(original.kind == ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION) {
        key.scalars.push_back(original.index);
      } else {
        const AbiStandardSubstitutionKind standard = effective_standard_kind(
          original.standard_substitution_kind, original.standard_substitution);
        key.scalars.push_back(standard);
        key.scalars.push_back(original.standard_substitution_includes_arguments ? 1 : 0);
        append_name_identity_fields(key, original.name, original.abi_tags);
      }
      for(std::vector<AbiDefinitionId>::const_iterator it = original.argument_refs.begin();
          it != original.argument_refs.end(); ++it) key.children.push_back(argument_identity(*it));
      break;
    case ABI_TYPE_MEMBER:
      key.kind = ABI_TYPE_MEMBER;
      if(original.types.empty() || original.name.components.size() != 1) {
        throw std::logic_error("member ABI type is incomplete");
      }
      key.children.push_back(type_identity(original.types[0]));
      key.components.push_back(intern_component(original.name.components[0]));
      break;
    case ABI_TYPE_MEMBER_TEMPLATE_SPECIALIZATION:
      key.kind = ABI_TYPE_MEMBER_TEMPLATE_SPECIALIZATION;
      key.scalars.push_back(original.types.size());
      for(std::vector<AbiType>::const_iterator it = original.types.begin();
          it != original.types.end(); ++it) key.children.push_back(type_identity(*it));
      key.scalars.push_back(original.argument_refs.size());
      for(std::vector<AbiDefinitionId>::const_iterator it = original.argument_refs.begin();
          it != original.argument_refs.end(); ++it) key.children.push_back(argument_identity(*it));
      break;
    case ABI_TYPE_DECLTYPE_EXPRESSION:
      key.kind = ABI_TYPE_DECLTYPE_EXPRESSION;
      key.children.push_back(expression_identity(original.expression_ref));
      break;
    case ABI_TYPE_LAMBDA_CLOSURE:
      key.kind = ABI_TYPE_LAMBDA_CLOSURE;
      key.children.push_back(context_identity(original.context_ref));
      key.components.push_back(intern_component(original.discriminator));
      key.scalars.push_back(original.types.size());
      for(std::vector<AbiType>::const_iterator it = original.types.begin();
          it != original.types.end(); ++it) key.children.push_back(type_identity(*it));
      break;
    case ABI_TYPE_LOCAL_TYPE:
      key.kind = ABI_TYPE_LOCAL_TYPE;
      key.children.push_back(context_identity(original.context_ref));
      append_name_identity_fields(key, original.name, std::vector<std::string>());
      key.components.push_back(intern_component(original.discriminator));
      break;
    case ABI_TYPE_NAMESPACE_LAMBDA:
      key.kind = ABI_TYPE_NAMESPACE_LAMBDA;
      append_name_identity_fields(key, original.name, original.abi_tags);
      key.scalars.push_back(original.namespace_qualifiers.size());
      for(std::vector<std::string>::const_iterator it = original.namespace_qualifiers.begin();
          it != original.namespace_qualifiers.end(); ++it) {
        key.components.push_back(intern_component(*it));
      }
      break;
    case ABI_TYPE_CV:
      break;
    }
    return intern_structural(key);
  }

  StructuralId argument_identity(const AbiDefinitionId & id)
  {
    std::map<std::size_t, StructuralId>::const_iterator found =
      argument_definition_identities_.find(id.index);
    if(found != argument_definition_identities_.end()) return found->second;
    if(id.index >= active_identity_definitions_.size() ||
       active_identity_definitions_[id.index] != 0) {
      throw std::logic_error("cyclic ABI argument identity");
    }
    active_identity_definitions_[id.index] = 1;
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_TEMPLATE_ARGUMENT) {
      active_identity_definitions_[id.index] = 0;
      throw std::logic_error("ABI definition is not an argument identity");
    }
    const AbiTemplateArgument & argument = record.template_argument;
    StructuralKey key;
    key.domain = STRUCTURAL_ARGUMENT;
    key.kind = argument.kind;
    key.scalars.push_back(argument.has_value_type ? 1 : 0);
    key.scalars.push_back(argument.address_of ? 1 : 0);
    key.scalars.push_back(argument.member_is_function ? 1 : 0);
    key.scalars.push_back(argument.member_function_const ? 1 : 0);
    key.scalars.push_back(argument.member_function_volatile ? 1 : 0);
    key.scalars.push_back(argument.member_function_lvalue_ref ? 1 : 0);
    key.scalars.push_back(argument.member_function_rvalue_ref ? 1 : 0);
    key.scalars.push_back(argument.member_function_variadic ? 1 : 0);
    switch(argument.kind) {
    case ABI_TEMPLATE_ARGUMENT_TYPE:
      key.children.push_back(type_identity(argument.type));
      break;
    case ABI_TEMPLATE_ARGUMENT_VALUE:
      key.children.push_back(type_identity(argument.value_type));
      key.scalars.push_back(normalized_integral_value(argument.value_type,
                                                       argument.value));
      break;
    case ABI_TEMPLATE_ARGUMENT_DEPENDENT_VALUE:
      key.children.push_back(type_identity(argument.type));
      key.children.push_back(type_identity(argument.value_type));
      key.scalars.push_back(normalized_integral_value(argument.value_type,
                                                       argument.value));
      break;
    case ABI_TEMPLATE_ARGUMENT_UNTYPED_VALUE:
      key.scalars.push_back(static_cast<unsigned long long>(argument.value));
      break;
    case ABI_TEMPLATE_ARGUMENT_EXPRESSION:
      key.children.push_back(expression_identity(argument.entity_ref));
      break;
    case ABI_TEMPLATE_ARGUMENT_TEMPLATE_ENTITY:
      append_name_identity_fields(key, argument.name, std::vector<std::string>());
      break;
    case ABI_TEMPLATE_ARGUMENT_MEMBER_TEMPLATE_ENTITY:
      key.children.push_back(type_identity(argument.owner_type));
      key.scalars.push_back(argument.name.components.size());
      if(argument.name.components.size() != 1) {
        throw std::logic_error("member template identity is not a source component");
      }
      key.components.push_back(intern_component(argument.name.components[0]));
      break;
    case ABI_TEMPLATE_ARGUMENT_TEMPLATE_PARAMETER_TEMPLATE:
      key.scalars.push_back(argument.index);
      break;
    case ABI_TEMPLATE_ARGUMENT_EXTERNAL_ENTITY:
      key.scalars.push_back(argument.symbol.size());
      key.components.push_back(intern_component(argument.symbol));
      break;
    case ABI_TEMPLATE_ARGUMENT_MEMBER_EXTERNAL_ENTITY:
      key.scalars.push_back(argument.symbol.size());
      key.components.push_back(intern_component(argument.symbol));
      key.children.push_back(type_identity(argument.owner_type));
      key.scalars.push_back(argument.name.components.size());
      if(argument.name.components.size() != 1) {
        throw std::logic_error("member external identity is not a source component");
      }
      key.components.push_back(intern_component(argument.name.components[0]));
      key.scalars.push_back(argument.parameter_types.size());
      for(std::vector<AbiType>::const_iterator it = argument.parameter_types.begin();
          it != argument.parameter_types.end(); ++it) key.children.push_back(type_identity(*it));
      break;
    case ABI_TEMPLATE_ARGUMENT_ENTITY:
      key.children.push_back(entity_identity(argument.entity_ref));
      break;
    case ABI_TEMPLATE_ARGUMENT_PACK:
      key.scalars.push_back(argument.argument_refs.size());
      for(std::vector<AbiDefinitionId>::const_iterator it = argument.argument_refs.begin();
          it != argument.argument_refs.end(); ++it) key.children.push_back(argument_identity(*it));
      break;
    }
    const StructuralId result = intern_structural(key);
    active_identity_definitions_[id.index] = 0;
    argument_definition_identities_.insert(std::make_pair(id.index, result));
    return result;
  }

  StructuralId expression_identity(const AbiDefinitionId & id)
  {
    std::map<std::size_t, StructuralId>::const_iterator found =
      expression_definition_identities_.find(id.index);
    if(found != expression_definition_identities_.end()) return found->second;
    if(id.index >= active_identity_definitions_.size() ||
       active_identity_definitions_[id.index] != 0) {
      throw std::logic_error("cyclic ABI expression identity");
    }
    active_identity_definitions_[id.index] = 1;
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_EXPRESSION) {
      active_identity_definitions_[id.index] = 0;
      throw std::logic_error("ABI definition is not an expression identity");
    }
    const AbiDependentExpression & expression = record.expression;
    StructuralKey key;
    key.domain = STRUCTURAL_EXPRESSION;
    key.kind = expression.kind;
    key.scalars.push_back(static_cast<unsigned long long>(expression.value));
    key.scalars.push_back(expression.index);
    key.scalars.push_back(expression.text.size());
    if(!expression.text.empty()) key.components.push_back(intern_component(expression.text));
    key.scalars.push_back(expression.op.size());
    if(!expression.op.empty()) key.components.push_back(intern_component(expression.op));
    if(expression.type.kind != ABI_TYPE_NAME_OR_REFERENCE ||
       expression.type.definition_ref.index != ABI_INVALID_DEFINITION_ID ||
       !expression.type.name.components.empty()) {
      key.children.push_back(type_identity(expression.type));
    }
    if(expression.value_type.kind != ABI_TYPE_NAME_OR_REFERENCE ||
       expression.value_type.definition_ref.index != ABI_INVALID_DEFINITION_ID ||
       !expression.value_type.name.components.empty()) {
      key.children.push_back(type_identity(expression.value_type));
    }
    if(expression.entity_ref.index != ABI_INVALID_DEFINITION_ID) {
      key.children.push_back(entity_identity(expression.entity_ref));
    }
    key.scalars.push_back(expression.expression_refs.size());
    for(std::vector<AbiDefinitionId>::const_iterator it = expression.expression_refs.begin();
        it != expression.expression_refs.end(); ++it) key.children.push_back(expression_identity(*it));
    key.scalars.push_back(expression.argument_refs.size());
    for(std::vector<AbiDefinitionId>::const_iterator it = expression.argument_refs.begin();
        it != expression.argument_refs.end(); ++it) key.children.push_back(argument_identity(*it));
    key.scalars.push_back(expression.type_arguments.size());
    for(std::vector<AbiType>::const_iterator it = expression.type_arguments.begin();
        it != expression.type_arguments.end(); ++it) key.children.push_back(type_identity(*it));
    const StructuralId result = intern_structural(key);
    active_identity_definitions_[id.index] = 0;
    expression_definition_identities_.insert(std::make_pair(id.index, result));
    return result;
  }

  StructuralId function_target_identity(const AbiFunctionTarget & target)
  {
    StructuralKey key;
    key.domain = STRUCTURAL_FUNCTION_TARGET;
    key.kind = target.kind;
    key.scalars.push_back(target.special_terminal);
    key.scalars.push_back(target.operator_terminal);
    key.scalars.push_back(target.terminal.size());
    if(!target.terminal.empty()) key.components.push_back(intern_component(target.terminal));
    key.scalars.push_back(target.source_name.size());
    if(!target.source_name.empty()) key.components.push_back(intern_component(target.source_name));
    key.scalars.push_back(target.name.components.size());
    for(std::vector<std::string>::const_iterator it = target.name.components.begin();
        it != target.name.components.end(); ++it) key.components.push_back(intern_component(*it));
    key.scalars.push_back(target.namespace_qualifiers.size());
    for(std::vector<std::string>::const_iterator it = target.namespace_qualifiers.begin();
        it != target.namespace_qualifiers.end(); ++it) key.components.push_back(intern_component(*it));
    key.scalars.push_back(target.discriminator.size());
    if(!target.discriminator.empty()) key.components.push_back(intern_component(target.discriminator));
    if(target.context_ref.index != ABI_INVALID_DEFINITION_ID) {
      key.children.push_back(context_identity(target.context_ref));
    }
    key.scalars.push_back(target.path_operands.size());
    for(std::vector<AbiFunctionPathOperand>::const_iterator it = target.path_operands.begin();
        it != target.path_operands.end(); ++it) {
      key.scalars.push_back(it->kind);
      if(it->kind == ABI_FUNCTION_PATH_TEMPLATE_ARGUMENT) {
        key.children.push_back(argument_identity(it->argument_ref));
      } else {
        key.children.push_back(type_identity(it->type));
      }
    }
    key.scalars.push_back(target.signature_parameter_types.size());
    for(std::vector<AbiType>::const_iterator it = target.signature_parameter_types.begin();
        it != target.signature_parameter_types.end(); ++it) key.children.push_back(type_identity(*it));
    return intern_structural(key);
  }

  StructuralId entity_identity(const AbiDefinitionId & id)
  {
    std::map<std::size_t, StructuralId>::const_iterator found =
      entity_definition_identities_.find(id.index);
    if(found != entity_definition_identities_.end()) return found->second;
    if(id.index >= active_identity_definitions_.size() ||
       active_identity_definitions_[id.index] != 0) {
      throw std::logic_error("cyclic ABI entity identity");
    }
    active_identity_definitions_[id.index] = 1;
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_ENTITY) {
      active_identity_definitions_[id.index] = 0;
      throw std::logic_error("ABI definition is not an entity identity");
    }
    const AbiEntityFact & entity = record.entity;
    StructuralKey key;
    key.domain = STRUCTURAL_ENTITY;
    key.kind = entity.kind;
    key.scalars.push_back(entity.internal_linkage ? 1 : 0);
    key.scalars.push_back(entity.name.components.size());
    for(std::vector<std::string>::const_iterator it = entity.name.components.begin();
        it != entity.name.components.end(); ++it) key.components.push_back(intern_component(*it));
    if(entity.kind == ABI_ENTITY_FACT_FUNCTION) {
      key.children.push_back(function_target_identity(entity.function));
    }
    const StructuralId result = intern_structural(key);
    active_identity_definitions_[id.index] = 0;
    entity_definition_identities_.insert(std::make_pair(id.index, result));
    return result;
  }

  StructuralId context_identity(const AbiDefinitionId & id)
  {
    std::map<std::size_t, StructuralId>::const_iterator found =
      context_definition_identities_.find(id.index);
    if(found != context_definition_identities_.end()) return found->second;
    if(id.index >= active_identity_definitions_.size() ||
       active_identity_definitions_[id.index] != 0) {
      throw std::logic_error("cyclic ABI context identity");
    }
    active_identity_definitions_[id.index] = 1;
    const AbiDefinitionRecord & record = definition(id);
    if(record.kind != ABI_DEFINITION_CONTEXT) {
      active_identity_definitions_[id.index] = 0;
      throw std::logic_error("ABI definition is not a context identity");
    }
    StructuralKey key;
    key.domain = STRUCTURAL_CONTEXT;
    key.kind = record.context.kind;
    if(record.context.kind == ABI_CONTEXT_RAW) {
      key.components.push_back(intern_component(record.context.fragment));
    } else {
      key.children.push_back(function_target_identity(record.context.function));
    }
    const StructuralId result = intern_structural(key);
    active_identity_definitions_[id.index] = 0;
    context_definition_identities_.insert(std::make_pair(id.index, result));
    return result;
  }

  StructuralId template_id_identity(const AbiFunctionRecord & record,
                                     const std::vector<std::string> & tags)
  {
    StructuralKey key;
    // A template-id occurring as a name component and the corresponding
    // specialization type are the same ABI entity.  Give both paths the
    // same typed structural key so a later type occurrence can find the
    // name occurrence (and vice versa).
    key.domain = STRUCTURAL_TYPE;
    const AbiStandardSubstitutionKind standard = effective_standard_kind(
      record.standard_substitution_kind, record.standard_substitution);
    key.kind = standard == ABI_STANDARD_SUBSTITUTION_NONE ?
      ABI_TYPE_TEMPLATE_SPECIALIZATION : ABI_TYPE_STD_TEMPLATE_SPECIALIZATION;
    key.scalars.push_back(standard);
    key.scalars.push_back(record.standard_substitution_includes_arguments ? 1 : 0);
    std::vector<std::string> name(1, record.name);
    append_name_identity_fields(key, name, tags);
    for(std::vector<AbiDefinitionId>::const_iterator it = record.argument_refs.begin();
        it != record.argument_refs.end(); ++it) key.children.push_back(argument_identity(*it));
    return intern_structural(key);
  }

  void register_structural_candidate(StructuralId id)
  {
    if(substitution_state_.structural_substitution_indexes.find(id) ==
       substitution_state_.structural_substitution_indexes.end()) {
      substitution_state_.structural_substitution_indexes.insert(
        std::make_pair(id, substitution_state_.next_substitution_index++));
    }
  }

  bool find_structural_substitution(StructuralId id, std::size_t * index) const
  {
    std::map<StructuralId, std::size_t>::const_iterator found =
      substitution_state_.structural_substitution_indexes.find(id);
    if(found == substitution_state_.structural_substitution_indexes.end()) return false;
    *index = found->second;
    return true;
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
        substitution_state_.substitution_trie[node].children.find(id);
      if(child == substitution_state_.substitution_trie[node].children.end()) {
        return INVALID_SUBSTITUTION_INDEX;
      }
      node = child->second;
    }
    return node;
  }

  std::size_t ensure_path_child(std::size_t node, ComponentId component)
  {
    std::map<ComponentId, std::size_t>::const_iterator found =
      substitution_state_.substitution_trie[node].children.find(component);
    if(found != substitution_state_.substitution_trie[node].children.end()) {
      return found->second;
    }
    const std::size_t child = substitution_state_.substitution_trie.size();
    substitution_state_.substitution_trie.push_back(NameTrieNode());
    substitution_state_.substitution_trie[node].children.insert(
      std::make_pair(component, child));
    StructuralKey key;
    key.domain = STRUCTURAL_NAME;
    key.kind = 1;
    key.components.push_back(component);
    if(substitution_state_.substitution_trie[node].path_identity !=
       INVALID_STRUCTURAL_ID) {
      key.children.push_back(
        substitution_state_.substitution_trie[node].path_identity);
    }
    substitution_state_.substitution_trie[child].path_identity =
      intern_structural(key);
    return child;
  }

  bool find_substitution(const std::vector<std::string> & components,
                         const std::vector<std::string> & tags,
                         std::size_t * index)
  {
    return find_structural_substitution(name_identity(components, tags), index);
  }

  void add_substitution_at_node(std::size_t node,
                                StructuralId path_identity,
                                const std::vector<std::string> & tags)
  {
    const StructuralId identity = name_identity_with_path(path_identity, tags);
    register_structural_candidate(identity);
    const std::size_t index =
      substitution_state_.structural_substitution_indexes[identity];
    bool known = false;
    const std::vector<ComponentId> tag_ids = canonical_tag_ids(tags, true, &known);
    if(tag_ids.empty()) {
      if(substitution_state_.substitution_trie[node].untagged_substitution ==
         INVALID_SUBSTITUTION_INDEX) {
        substitution_state_.substitution_trie[node].untagged_substitution = index;
      }
      return;
    }
    TagKey key;
    key.components = tag_ids;
    if(substitution_state_.substitution_trie[node].tagged_substitutions.find(key) ==
       substitution_state_.substitution_trie[node].tagged_substitutions.end()) {
      substitution_state_.substitution_trie[node].tagged_substitutions.insert(
        std::make_pair(key, index));
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
    add_substitution_at_node(
      node, substitution_state_.substitution_trie[node].path_identity, tags);
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
      if(substitution_state_.substitution_trie[current].untagged_substitution !=
         INVALID_SUBSTITUTION_INDEX) {
        deepest = i + 1;
        *node = current;
      }
    }
    return deepest;
  }

  void append_scope_prefix(const std::vector<std::string> & components,
                           std::string & output,
                           StructuralId * complete_identity = NULL)
  {
    if(components.empty()) throw std::logic_error("empty ABI scope prefix");
    if(components[0] == "std") {
      // ``std`` itself is the fixed ABI abbreviation St, while longer
      // prefixes (notably std::__1) participate in the ordinary substitution
      // table after their first structural occurrence.
      std::size_t node = ensure_path_child(0, intern_component("std"));
      std::size_t deepest = 1;
      std::size_t deepest_node = node;
      std::size_t current = node;
      for(std::size_t i = 1; i < components.size(); ++i) {
        current = ensure_path_child(current, intern_component(components[i]));
        if(substitution_state_.substitution_trie[current].untagged_substitution !=
           INVALID_SUBSTITUTION_INDEX) {
          deepest = i + 1;
          deepest_node = current;
        }
      }
      if(deepest > 1) {
        node = deepest_node;
        append_substitution(
          substitution_state_.substitution_trie[node].untagged_substitution, output);
      } else {
        output += "St";
        node = deepest_node;
      }
      for(std::size_t i = deepest; i < components.size(); ++i) {
        node = ensure_path_child(node, intern_component(components[i]));
        output += source_name(components[i]);
        if(substitution_state_.substitution_trie[node].untagged_substitution ==
           INVALID_SUBSTITUTION_INDEX) {
          const StructuralId identity =
            substitution_state_.substitution_trie[node].path_identity;
          register_structural_candidate(identity);
          substitution_state_.substitution_trie[node].untagged_substitution =
            substitution_state_.structural_substitution_indexes[identity];
        }
      }
      if(complete_identity != NULL) {
        *complete_identity =
          substitution_state_.substitution_trie[node].path_identity;
      }
      return;
    }

    std::size_t node = 0;
    const std::size_t start = deepest_untagged_prefix(components, components.size(), &node);
    if(start != 0) {
      append_substitution(
        substitution_state_.substitution_trie[node].untagged_substitution, output);
    }
    for(std::size_t i = start; i < components.size(); ++i) {
      node = ensure_path_child(node, intern_component(components[i]));
      output += source_name(components[i]);
      if(substitution_state_.substitution_trie[node].untagged_substitution ==
         INVALID_SUBSTITUTION_INDEX) {
        const StructuralId identity =
          substitution_state_.substitution_trie[node].path_identity;
        register_structural_candidate(identity);
        substitution_state_.substitution_trie[node].untagged_substitution =
          substitution_state_.structural_substitution_indexes[identity];
      }
    }
    if(complete_identity != NULL) {
      *complete_identity = substitution_state_.substitution_trie[node].path_identity;
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
        output.push_back('N');
        std::vector<std::string> owner(components.begin(), components.end() - 1);
        append_scope_prefix(owner, output);
        output += source_name(components.back());
        output += suffix;
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
      append_substitution(
        substitution_state_.substitution_trie[node].untagged_substitution, output);
    }
    for(std::size_t i = start; i < prefix_count; ++i) {
      node = ensure_path_child(node, intern_component(components[i]));
      output += source_name(components[i]);
      if(substitution_state_.substitution_trie[node].untagged_substitution ==
         INVALID_SUBSTITUTION_INDEX) {
        const StructuralId identity =
          substitution_state_.substitution_trie[node].path_identity;
        register_structural_candidate(identity);
        substitution_state_.substitution_trie[node].untagged_substitution =
          substitution_state_.structural_substitution_indexes[identity];
      }
    }
    node = ensure_path_child(node, intern_component(components.back()));
    output += source_name(components.back());
    output += suffix;
    add_substitution_at_node(
      node, substitution_state_.substitution_trie[node].path_identity, tags);
    output.push_back('E');
  }

  void append_nested_name(const std::vector<std::string> & owner,
                          const std::vector<AbiFunctionQualifier> & qualifiers,
                          const std::string & terminal,
                          const std::vector<std::string> & tags,
                          const std::vector<AbiDefinitionId> & template_arguments,
                          std::string & output)
  {
    if(owner.empty()) {
      output += terminal;
      output += tag_suffix(tags);
      append_template_arguments(template_arguments, output);
      return;
    }
    const bool simple_std_function = owner.size() == 1 && owner[0] == "std" &&
      qualifiers.empty();
    if(simple_std_function) {
      output += "St";
      output += terminal;
      output += tag_suffix(tags);
      append_template_arguments(template_arguments, output);
      return;
    }

    output.push_back('N');
    append_function_qualifiers(qualifiers, output);
    append_scope_prefix(owner, output);
    output += terminal;
    output += tag_suffix(tags);
    append_template_arguments(template_arguments, output);
    output.push_back('E');
  }

  void append_nested_name(const std::vector<std::string> & owner,
                          const std::vector<AbiFunctionQualifier> & qualifiers,
                          const std::string & terminal,
                          const std::vector<std::string> & tags,
                          std::string & output)
  {
    const std::vector<AbiDefinitionId> empty_arguments;
    append_nested_name(owner, qualifiers, terminal, tags, empty_arguments, output);
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

  void append_template_arguments(const std::vector<AbiDefinitionId> & refs,
                                 std::string & output)
  {
    if(refs.empty()) return;
    if(substitution_state_.pending_function_prefix_candidate) {
      register_structural_candidate(
        substitution_state_.pending_function_prefix_identity);
      substitution_state_.pending_function_prefix_candidate = false;
    }
    output.push_back('I');
    for(std::vector<AbiDefinitionId>::const_iterator it = refs.begin();
        it != refs.end(); ++it) {
      output += encode_argument(argument_definition(*it));
    }
    output.push_back('E');
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

    if(type->kind == ABI_TYPE_TEMPLATE_PARAMETER) {
      append_template_parameter(type->index, output);
      return;
    }

    const bool substitutable = type->kind != ABI_TYPE_NAME_OR_REFERENCE &&
      type->kind != ABI_TYPE_NAMED && type->kind != ABI_TYPE_BUILTIN;
    if(substitutable) {
      const StructuralId identity = type_identity(*type);
      std::size_t substitution = 0;
      if(find_structural_substitution(identity, &substitution)) {
        append_substitution(substitution, output);
        return;
      }
      append_type_body(*type, output);
      register_structural_candidate(identity);
      return;
    }
    append_type_body(*type, output);
  }

  void append_type_body(const AbiType & original, std::string & output)
  {
    const AbiType * type = &original;

    switch(type->kind) {
    case ABI_TYPE_NAME_OR_REFERENCE:
    case ABI_TYPE_NAMED:
      append_named_type_name(type->name, type->abi_tags, output);
      return;
    case ABI_TYPE_BUILTIN:
      output += builtin_code(type->builtin);
      return;
    case ABI_TYPE_TEMPLATE_PARAMETER:
      append_template_parameter(type->index, output);
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

  void append_template_parameter(std::size_t index, std::string & output)
  {
    StructuralKey key;
    key.domain = STRUCTURAL_TYPE;
    key.kind = ABI_TYPE_TEMPLATE_PARAMETER;
    key.scalars.push_back(index);
    const StructuralId identity = intern_structural(key);
    std::size_t substitution = 0;
    if(find_structural_substitution(identity, &substitution)) {
      append_substitution(substitution, output);
      return;
    }
    output += template_parameter(index);
    register_structural_candidate(identity);
  }

  void append_template_specialization(const AbiType & type,
                                      std::string & output)
  {
    const AbiStandardSubstitutionKind standard = effective_standard_kind(
      type.standard_substitution_kind, type.standard_substitution);
    if(type.kind == ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION) {
      append_template_parameter(type.index, output);
    } else if(standard != ABI_STANDARD_SUBSTITUTION_NONE) {
      output += standard_substitution_code(standard);
    } else {
      if(type.name.components.empty()) {
        throw std::logic_error("template specialization has no name");
      }
      std::size_t name_substitution = 0;
      if(find_substitution(type.name.components, type.abi_tags,
                           &name_substitution)) {
        if(type.name.components.size() == 1) {
          append_substitution(name_substitution, output);
        } else {
          // A qualified class template remains a nested-name even when its
          // template prefix is substituted: N <substitution> I...E E.
          output.push_back('N');
          append_substitution(name_substitution, output);
        }
      } else if(type.name.components.size() == 1) {
        output += source_name(type.name.components[0]);
        output += tag_suffix(type.abi_tags);
        add_substitution(type.name.components, type.abi_tags);
      } else if(type.name.components.size() == 2 &&
                type.name.components[0] == "std") {
        output += "St";
        output += source_name(type.name.components[1]);
        output += tag_suffix(type.abi_tags);
        add_substitution(type.name.components, type.abi_tags);
      } else {
        output.push_back('N');
        std::vector<std::string> owner(type.name.components.begin(),
                                       type.name.components.end() - 1);
        append_scope_prefix(owner, output);
        output += source_name(type.name.components.back());
        output += tag_suffix(type.abi_tags);
        add_substitution(type.name.components, type.abi_tags);
      }
    }
    if(standard != ABI_STANDARD_SUBSTITUTION_NONE &&
       type.standard_substitution_includes_arguments) return;
    output.push_back('I');
    for(std::vector<AbiDefinitionId>::const_iterator it = type.argument_refs.begin();
        it != type.argument_refs.end(); ++it) {
      output += encode_argument(argument_definition(*it));
    }
    output.push_back('E');
    if(standard == ABI_STANDARD_SUBSTITUTION_NONE &&
       type.kind != ABI_TYPE_TEMPLATE_PARAMETER_SPECIALIZATION &&
       type.name.components.size() > 1 &&
       !(type.name.components.size() == 2 && type.name.components[0] == "std")) {
      output.push_back('E');
    }
  }

  std::string encode_argument(const AbiTemplateArgument & argument)
  {
    switch(argument.kind) {
    case ABI_TEMPLATE_ARGUMENT_TYPE:
      return encode_type(argument.type);
    case ABI_TEMPLATE_ARGUMENT_VALUE:
      return encode_value_argument(argument);
    case ABI_TEMPLATE_ARGUMENT_DEPENDENT_VALUE:
      if(!argument.has_value_type) {
        throw std::logic_error("dependent ABI value is missing its value type");
      }
      return "Tn" + encode_type(argument.type) +
        encode_value_literal(argument.value_type, argument.value);
    case ABI_TEMPLATE_ARGUMENT_UNTYPED_VALUE:
      return "L" + number_string(static_cast<unsigned long long>(argument.value)) + "E";
    case ABI_TEMPLATE_ARGUMENT_EXPRESSION:
      return "X" + encode_expression_reference(argument.entity_ref) + "E";
    case ABI_TEMPLATE_ARGUMENT_PACK: {
      std::string result = "J";
      for(std::vector<AbiDefinitionId>::const_iterator it = argument.argument_refs.begin();
          it != argument.argument_refs.end(); ++it) {
        result += encode_argument(argument_definition(*it));
      }
      return result + "E";
    }
    case ABI_TEMPLATE_ARGUMENT_TEMPLATE_PARAMETER_TEMPLATE:
      {
        std::string result;
        append_template_parameter(argument.index, result);
        return result;
      }
    case ABI_TEMPLATE_ARGUMENT_TEMPLATE_ENTITY:
      return encode_name(argument.name, std::vector<std::string>());
    case ABI_TEMPLATE_ARGUMENT_MEMBER_TEMPLATE_ENTITY:
      if(argument.name.components.size() != 1) {
        throw std::logic_error("member template name is not a source component");
      }
      {
        std::string result = "N";
        append_member_template_prefix(argument.owner_type, result);
        std::vector<std::string> member_name(1, argument.name.components[0]);
        std::size_t member_substitution = 0;
        if(find_substitution(member_name, std::vector<std::string>(),
                             &member_substitution)) {
          append_substitution(member_substitution, result);
        } else {
          result += source_name(argument.name.components[0]);
        }
        result.push_back('E');
        add_substitution(member_name, std::vector<std::string>());
        return result;
      }
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
    return encode_value_literal(argument.value_type, argument.value);
  }

  std::string encode_value_literal(const AbiType & value_type, long long value)
  {
    const std::string type = encode_type(value_type);
    const unsigned long long raw = static_cast<unsigned long long>(value);
    unsigned int bits = 0;
    std::string encoded_value;
    if(is_unsigned_builtin(value_type, &bits)) {
      unsigned long long normalized = raw;
      if(bits < 64) {
        normalized &= (static_cast<unsigned long long>(1) << bits) - 1;
      }
      encoded_value = number_string(normalized);
    } else if(value < 0) {
      const unsigned long long magnitude = 0 - raw;
      encoded_value = "n" + number_string(magnitude);
    } else {
      encoded_value = number_string(raw);
    }
    return "L" + type + encoded_value + "E";
  }

  void append_member_template_prefix(const AbiType & original,
                                     std::string & output)
  {
    if(original.kind == ABI_TYPE_NAME_OR_REFERENCE &&
       original.definition_ref.index != ABI_INVALID_DEFINITION_ID) {
      const AbiType & definition_type = type_definition(original.definition_ref);
      ActiveDefinitionScope active(active_types_, original.definition_ref.index);
      append_member_template_prefix(definition_type, output);
      return;
    }
    const AbiStandardSubstitutionKind standard = effective_standard_kind(
      original.standard_substitution_kind, original.standard_substitution);
    if(original.kind == ABI_TYPE_TEMPLATE_SPECIALIZATION ||
       original.kind == ABI_TYPE_STD_TEMPLATE_SPECIALIZATION) {
      if(standard != ABI_STANDARD_SUBSTITUTION_NONE) {
        output += standard_substitution_code(standard);
      } else {
        if(original.name.components.empty()) {
          throw std::logic_error("member template owner has no name");
        }
        std::size_t name_substitution = 0;
        if(find_substitution(original.name.components, original.abi_tags,
                             &name_substitution)) {
          append_substitution(name_substitution, output);
        } else if(original.name.components.size() > 1) {
          std::vector<std::string> owner(original.name.components.begin(),
                                         original.name.components.end() - 1);
          append_scope_prefix(owner, output);
          output += source_name(original.name.components.back());
          output += tag_suffix(original.abi_tags);
          add_substitution(original.name.components, original.abi_tags);
        } else {
          output += source_name(original.name.components.back());
          output += tag_suffix(original.abi_tags);
          add_substitution(original.name.components, original.abi_tags);
        }
      }
      if(standard == ABI_STANDARD_SUBSTITUTION_NONE ||
         !original.standard_substitution_includes_arguments) {
        output.push_back('I');
        for(std::vector<AbiDefinitionId>::const_iterator it =
              original.argument_refs.begin(); it != original.argument_refs.end(); ++it) {
          output += encode_argument(argument_definition(*it));
        }
        output.push_back('E');
      }
      return;
    }
    append_type(original, output);
  }

  std::string encode_entity_argument(const AbiTemplateArgument & argument)
  {
    std::string symbol;
    if(!argument.symbol.empty()) {
      symbol = argument.symbol;
    } else {
      symbol = encode_entity_symbol(definition(argument.entity_ref).entity);
    }
    if(argument.address_of) return "XadL" + symbol + "EE";
    return "L" + symbol + "E";
  }

  std::string encode_entity_symbol(const AbiEntityFact & entity)
  {
    // An external entity's nested name is its own ABI spelling boundary.  It
    // has an independent substitution walk and must not add C/ns/etc. to the
    // enclosing template's candidate table.  Swapping the state object keeps
    // this isolation O(1), even when the enclosing case has many candidates.
    SubstitutionState nested_state;
    substitution_state_.swap(nested_state);
    std::string result;
    try {
      if(entity.kind == ABI_ENTITY_FACT_SYMBOL) {
        result = join_name_for_external_symbol(entity.name);
      } else if(entity.kind == ABI_ENTITY_FACT_VARIABLE) {
        result = encode_variable_symbol(entity);
      } else if(entity.kind == ABI_ENTITY_FACT_FUNCTION) {
        result = encode_function(entity.function);
      } else {
        throw std::logic_error("unknown ABI entity kind");
      }
    } catch(...) {
      substitution_state_.swap(nested_state);
      throw;
    }
    substitution_state_.swap(nested_state);
    return result;
  }

  std::string encode_variable_symbol(const AbiEntityFact & entity)
  {
    if(entity.name.components.empty()) {
      throw std::logic_error("ABI variable entity has no name");
    }
    std::string result = "_Z";
    if(entity.internal_linkage && entity.name.components.size() > 1) {
      result.push_back('N');
      for(std::vector<std::string>::const_iterator it = entity.name.components.begin();
          it + 1 != entity.name.components.end(); ++it) {
        result += source_name(*it);
      }
      result.push_back('L');
      result += source_name(entity.name.components.back());
      result.push_back('E');
    } else {
      append_data_name(entity.name, result);
    }
    return result;
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
    if(record.expression.kind == ABI_EXPRESSION_ENTITY) {
      const std::string symbol = encode_entity_symbol(
        definition(record.expression.entity_ref).entity);
      return "L" + symbol + "E";
    }
    return record.expression.text;
  }

  void append_member_type(const AbiType & type, std::string & output)
  {
    if(type.types.empty() || type.name.components.size() != 1) {
      throw std::logic_error("member ABI type is incomplete");
    }
    output.push_back('N');
    append_type(type.types[0], output);
    output += source_name(type.name.components[0]);
    output.push_back('E');
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
    const unsigned long long number = ordinal - 1;
    if(number < 10) return "_" + number_string(number);
    return "__" + number_string(number) + "_";
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
    bool has_result = false;
    bool is_function_template = false;
    const AbiFunctionRecord * function_template_prefix = NULL;
    AbiType result;
    std::string terminal;
    std::string literal_suffix;
    AbiFunctionSpecialTerminalKind special_terminal = ABI_SPECIAL_TERMINAL_NONE;
    AbiOperatorTerminalKind operator_terminal = ABI_OPERATOR_TERMINAL_NONE;
    bool has_conversion_type = false;
    AbiType conversion_type;
    std::vector<std::string> tags;
    std::vector<AbiFunctionQualifier> qualifiers;
    std::vector<AbiDefinitionId> template_argument_refs;
    std::vector<const AbiFunctionRecord *> name_records;
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
    for(std::vector<AbiFunctionPathOperand>::const_iterator it =
          target.path_operands.begin(); it != target.path_operands.end(); ++it) {
      switch(it->kind) {
      case ABI_FUNCTION_PATH_TEMPLATE_ARGUMENT:
        facts.template_argument_refs.push_back(it->argument_ref);
        facts.is_function_template = true;
        break;
      case ABI_FUNCTION_PATH_RESULT_TYPE:
        if(facts.has_result) {
          throw std::logic_error("ABI function has multiple result records");
        }
        facts.has_result = true;
        facts.result = it->type;
        break;
      case ABI_FUNCTION_PATH_VARIADIC:
        facts.variadic = true;
        break;
      case ABI_FUNCTION_PATH_TYPE:
        // Ordinary typed path operands are signature operands.  They are
        // consumed by append_function_parameters; retaining them here is
        // what lets the adapter serve both ordinary and template paths.
        break;
      }
    }
    for(std::vector<AbiFactRecord>::const_iterator it = fact_case_.records.begin();
        it != fact_case_.records.end(); ++it) {
      if(it->kind != ABI_FACT_RECORD_FUNCTION) continue;
      const AbiFunctionRecord & record = it->function;
      switch(record.kind) {
      case ABI_FUNCTION_RECORD_NAME_SOURCE:
      case ABI_FUNCTION_RECORD_NAME_TEMPLATE:
        facts.name_records.push_back(&record);
        break;
      case ABI_FUNCTION_RECORD_NAME_STD:
        facts.std_prefix = true;
        break;
      case ABI_FUNCTION_RECORD_FUNCTION_TEMPLATE_ARGUMENT:
        facts.is_function_template = true;
        facts.template_argument_refs.insert(facts.template_argument_refs.end(),
                                             record.argument_refs.begin(),
                                             record.argument_refs.end());
        break;
      case ABI_FUNCTION_RECORD_FUNCTION_TEMPLATE_PREFIX:
        facts.is_function_template = true;
        if(facts.function_template_prefix != NULL) {
          throw std::logic_error("ABI function has multiple template prefixes");
        }
        facts.function_template_prefix = &record;
        break;
      case ABI_FUNCTION_RECORD_TERMINAL:
        facts.terminal = record.terminal;
        facts.special_terminal = record.special_terminal;
        break;
      case ABI_FUNCTION_RECORD_TERMINAL_SOURCE:
        facts.terminal = record.terminal;
        facts.special_terminal = ABI_SPECIAL_TERMINAL_NONE;
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
      case ABI_FUNCTION_RECORD_RESULT:
        if(facts.has_result) {
          throw std::logic_error("ABI function has multiple result records");
        }
        facts.has_result = true;
        facts.result = record.type;
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
    // A final template name component is itself the function-template
    // encoding for inline normalized names.  A template component in an
    // owner position (for example Holder<...>::f) is not.
    if(!facts.is_function_template && !facts.name_records.empty() &&
       facts.name_records.back()->kind == ABI_FUNCTION_RECORD_NAME_TEMPLATE) {
      facts.is_function_template = true;
    }
  }

  StructuralId function_prefix_identity(const AbiFunctionTarget & target,
                                        const FunctionFacts & facts)
  {
    StructuralKey key;
    key.domain = STRUCTURAL_FUNCTION_PREFIX;
    key.kind = target.kind;
    key.scalars.push_back(target.special_terminal);
    key.scalars.push_back(target.operator_terminal);
    key.scalars.push_back(facts.special_terminal);
    key.scalars.push_back(facts.operator_terminal);
    key.scalars.push_back(facts.std_prefix ? 1 : 0);
    key.scalars.push_back(facts.has_conversion_type ? 1 : 0);

    key.scalars.push_back(target.name.components.size());
    for(std::vector<std::string>::const_iterator it = target.name.components.begin();
        it != target.name.components.end(); ++it) {
      key.components.push_back(intern_component(*it));
    }
    key.scalars.push_back(target.source_name.size());
    if(!target.source_name.empty()) key.components.push_back(intern_component(target.source_name));
    key.scalars.push_back(target.terminal.size());
    if(!target.terminal.empty()) key.components.push_back(intern_component(target.terminal));
    key.scalars.push_back(target.namespace_qualifiers.size());
    for(std::vector<std::string>::const_iterator it = target.namespace_qualifiers.begin();
        it != target.namespace_qualifiers.end(); ++it) {
      key.components.push_back(intern_component(*it));
    }
    key.scalars.push_back(target.discriminator.size());
    if(!target.discriminator.empty()) key.components.push_back(intern_component(target.discriminator));

    key.scalars.push_back(facts.name_records.size());
    for(std::vector<const AbiFunctionRecord *>::const_iterator it =
          facts.name_records.begin(); it != facts.name_records.end(); ++it) {
      const AbiFunctionRecord & record = **it;
      key.scalars.push_back(record.kind);
      if(record.kind == ABI_FUNCTION_RECORD_NAME_SOURCE) {
        key.scalars.push_back(record.source_name.components.size());
        for(std::vector<std::string>::const_iterator component =
              record.source_name.components.begin();
            component != record.source_name.components.end(); ++component) {
          key.components.push_back(intern_component(*component));
        }
      } else if(record.kind == ABI_FUNCTION_RECORD_NAME_TEMPLATE) {
        key.scalars.push_back(record.name.size());
        key.components.push_back(intern_component(record.name));
        key.scalars.push_back(effective_standard_kind(
          record.standard_substitution_kind, record.standard_substitution));
        key.scalars.push_back(record.standard_substitution_includes_arguments ? 1 : 0);
      }
    }

    key.scalars.push_back(facts.qualifiers.size());
    for(std::vector<AbiFunctionQualifier>::const_iterator it = facts.qualifiers.begin();
        it != facts.qualifiers.end(); ++it) key.scalars.push_back(*it);
    key.scalars.push_back(facts.tags.size());
    std::vector<std::string> sorted_tags = facts.tags;
    std::sort(sorted_tags.begin(), sorted_tags.end());
    for(std::vector<std::string>::const_iterator it = sorted_tags.begin();
        it != sorted_tags.end(); ++it) key.components.push_back(intern_component(*it));
    key.scalars.push_back(facts.literal_suffix.size());
    if(!facts.literal_suffix.empty()) key.components.push_back(intern_component(facts.literal_suffix));

    if(facts.function_template_prefix != NULL) {
      const AbiFunctionRecord & prefix = *facts.function_template_prefix;
      key.scalars.push_back(1);
      key.scalars.push_back(prefix.function_template_prefix_name.components.size());
      for(std::vector<std::string>::const_iterator it =
            prefix.function_template_prefix_name.components.begin();
          it != prefix.function_template_prefix_name.components.end(); ++it) {
        key.components.push_back(intern_component(*it));
      }
      key.scalars.push_back(prefix.function_template_prefix_operator);
      key.scalars.push_back(prefix.function_template_prefix_conversion ? 1 : 0);
    } else {
      key.scalars.push_back(0);
    }
    if(facts.has_conversion_type) key.children.push_back(type_identity(facts.conversion_type));
    if(target.context_ref.index != ABI_INVALID_DEFINITION_ID) {
      key.children.push_back(context_identity(target.context_ref));
    }
    key.scalars.push_back(target.path_operands.size());
    for(std::vector<AbiFunctionPathOperand>::const_iterator it = target.path_operands.begin();
        it != target.path_operands.end(); ++it) {
      if(it->kind == ABI_FUNCTION_PATH_TEMPLATE_ARGUMENT) continue;
      key.scalars.push_back(it->kind);
      if(it->kind == ABI_FUNCTION_PATH_TYPE || it->kind == ABI_FUNCTION_PATH_RESULT_TYPE) {
        key.children.push_back(type_identity(it->type));
      }
    }
    return intern_structural(key);
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
      return (member ? parameter_count != 0 : parameter_count > 1) ? "pl" : "ps";
    case ABI_OPERATOR_TERMINAL_UNARY_MINUS: return "ng";
    case ABI_OPERATOR_TERMINAL_BINARY_MINUS: return "mi";
    case ABI_OPERATOR_TERMINAL_MINUS:
      return (member ? parameter_count != 0 : parameter_count > 1) ? "mi" : "ng";
    case ABI_OPERATOR_TERMINAL_ADDRESS_OF: return "ad";
    case ABI_OPERATOR_TERMINAL_DEREF: return "de";
    case ABI_OPERATOR_TERMINAL_COMPLEMENT: return "co";
    case ABI_OPERATOR_TERMINAL_MULTIPLY: return "ml";
    case ABI_OPERATOR_TERMINAL_DIVIDE: return "dv";
    case ABI_OPERATOR_TERMINAL_REMAINDER: return "rm";
    case ABI_OPERATOR_TERMINAL_BIT_AND: return "an";
    case ABI_OPERATOR_TERMINAL_BIT_OR: return "or";
    case ABI_OPERATOR_TERMINAL_BIT_XOR: return "eo";
    case ABI_OPERATOR_TERMINAL_ASSIGN: return "aS";
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
    append_template_arguments(facts.template_argument_refs, output);
    output.push_back('E');
  }

  void append_name_template_component(const AbiFunctionRecord & record,
                                      const std::vector<std::string> & tags,
                                      std::string & output,
                                      StructuralId * complete_identity_out = NULL)
  {
    if(record.name.empty()) {
      throw std::logic_error("ABI template name has no source component");
    }
    const AbiStandardSubstitutionKind standard = effective_standard_kind(
      record.standard_substitution_kind, record.standard_substitution);
    // Compute the complete identity before emitting operands, but publish it
    // only after the template-id has actually been traversed.  Substitution
    // candidates are introduced by completed ABI constructs; reserving an
    // index before their operands would make later indices depend on guesses.
    const StructuralId complete_identity = template_id_identity(record, tags);
    if(standard != ABI_STANDARD_SUBSTITUTION_NONE) {
      output += standard_substitution_code(standard);
    } else {
      output += source_name(record.name);
    }
    output += tag_suffix(tags);
    if(standard == ABI_STANDARD_SUBSTITUTION_NONE ||
       !record.standard_substitution_includes_arguments) {
      append_template_arguments(record.argument_refs, output);
    }
    register_structural_candidate(complete_identity);
    if(complete_identity_out != NULL) *complete_identity_out = complete_identity;
  }

  StructuralId mixed_prefix_identity(StructuralId parent,
                                     StructuralId component)
  {
    if(parent == INVALID_STRUCTURAL_ID) return component;
    StructuralKey key;
    key.domain = STRUCTURAL_TEMPLATE_PREFIX;
    key.kind = 0;
    key.children.push_back(parent);
    key.children.push_back(component);
    return intern_structural(key);
  }

  void append_mixed_plain_component(StructuralId & parent,
                                    const std::string & component,
                                    std::string & output)
  {
    const StructuralId component_identity = name_component_identity(component);
    const StructuralId complete_identity =
      mixed_prefix_identity(parent, component_identity);
    std::size_t substitution = 0;
    if(find_structural_substitution(complete_identity, &substitution)) {
      append_substitution(substitution, output);
    } else {
      output += source_name(component);
    }
    register_structural_candidate(complete_identity);
    parent = complete_identity;
  }

  void append_structured_function_name(const FunctionFacts & facts,
                                       std::string & output)
  {
    struct NameComponent
    {
      bool is_template;
      std::string source;
      const AbiFunctionRecord * record;

      NameComponent()
        : is_template(false), record(NULL)
      {}
    };

    std::vector<NameComponent> components;
    for(std::vector<const AbiFunctionRecord *>::const_iterator it =
          facts.name_records.begin(); it != facts.name_records.end(); ++it) {
      const AbiFunctionRecord & record = **it;
      if(record.kind == ABI_FUNCTION_RECORD_NAME_TEMPLATE) {
        NameComponent component;
        component.is_template = true;
        component.source = record.name;
        component.record = &record;
        components.push_back(component);
        continue;
      }
      for(std::vector<std::string>::const_iterator source =
            record.source_name.components.begin();
          source != record.source_name.components.end(); ++source) {
        if(source->empty()) continue;
        NameComponent component;
        component.source = *source;
        components.push_back(component);
      }
    }
    if(components.empty()) {
      throw std::logic_error("ABI function has no encoded name components");
    }

    if(facts.std_prefix && components.front().source != "std") {
      NameComponent std_component;
      std_component.source = "std";
      components.insert(components.begin(), std_component);
    }

    const NameComponent & final_component = components.back();
    const bool final_has_template = final_component.is_template;
    if(components.size() == 1 && facts.qualifiers.empty()) {
      if(final_has_template) {
        append_name_template_component(*final_component.record, facts.tags, output);
      } else {
        output += source_name(final_component.source);
        output += tag_suffix(facts.tags);
        append_template_arguments(facts.template_argument_refs, output);
      }
      return;
    }

    if(components.size() == 2 && !components[0].is_template &&
       components[0].source == "std" && facts.qualifiers.empty()) {
      output += "St";
      if(final_has_template) {
        append_name_template_component(*final_component.record, facts.tags, output);
      } else {
        output += source_name(final_component.source);
        output += tag_suffix(facts.tags);
        append_template_arguments(facts.template_argument_refs, output);
      }
      return;
    }

    output.push_back('N');
    append_function_qualifiers(facts.qualifiers, output);
    const std::size_t owner_count = components.size() - 1;
    std::size_t plain_begin = 0;
    bool template_seen = false;
    StructuralId owner_identity = INVALID_STRUCTURAL_ID;
    for(std::size_t i = 0; i < owner_count; ++i) {
      if(components[i].is_template) {
        if(plain_begin != i) {
          if(!template_seen) {
            std::vector<std::string> plain;
            for(std::size_t j = plain_begin; j < i; ++j) {
              plain.push_back(components[j].source);
            }
            append_scope_prefix(plain, output, &owner_identity);
          } else {
            for(std::size_t j = plain_begin; j < i; ++j) {
              append_mixed_plain_component(owner_identity,
                                           components[j].source, output);
            }
          }
        }
        StructuralId template_identity = INVALID_STRUCTURAL_ID;
        append_name_template_component(*components[i].record,
                                       std::vector<std::string>(), output,
                                       &template_identity);
        owner_identity = mixed_prefix_identity(owner_identity, template_identity);
        register_structural_candidate(owner_identity);
        plain_begin = i + 1;
        template_seen = true;
      }
    }
    if(plain_begin < owner_count) {
      if(!template_seen) {
        std::vector<std::string> plain;
        for(std::size_t j = plain_begin; j < owner_count; ++j) {
          plain.push_back(components[j].source);
        }
        append_scope_prefix(plain, output);
      } else {
        for(std::size_t j = plain_begin; j < owner_count; ++j) {
          append_mixed_plain_component(owner_identity,
                                       components[j].source, output);
        }
      }
    }
    if(final_has_template) {
      append_name_template_component(*final_component.record, facts.tags, output);
    } else {
      output += source_name(final_component.source);
      output += tag_suffix(facts.tags);
      append_template_arguments(facts.template_argument_refs, output);
    }
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
                         special_terminal_code(facts.special_terminal), facts.tags,
                         facts.template_argument_refs, output);
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
                         facts.tags, facts.template_argument_refs, output);
      return;
    }
    if(facts.has_conversion_type) {
      append_conversion_name(components.components, facts, output);
      return;
    }

    if(components.components.size() == 1 && facts.qualifiers.empty()) {
      output += source_name(components.components[0]);
      output += tag_suffix(facts.tags);
      append_template_arguments(facts.template_argument_refs, output);
      return;
    }
    std::vector<std::string> owner(components.components.begin(), components.components.end() - 1);
    append_nested_name(owner, facts.qualifiers,
                       source_name(components.components.back()), facts.tags,
                       facts.template_argument_refs, output);
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
    } else if(facts.special_terminal != ABI_SPECIAL_TERMINAL_NONE) {
      output += special_terminal_code(facts.special_terminal);
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
      if(!facts.name_records.empty() &&
         facts.operator_terminal == ABI_OPERATOR_TERMINAL_NONE &&
         facts.special_terminal == ABI_SPECIAL_TERMINAL_NONE &&
         !facts.has_conversion_type) {
        append_structured_function_name(facts, output);
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
    // A result record is present for function-template encodings because the
    // Itanium ABI includes the return type in a template function signature.
    // Conversion functions encode their conversion type in the name instead.
    if(facts.is_function_template && facts.has_result && !facts.has_conversion_type) {
      append_type(facts.result, output);
    }
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
    const bool final_name_template = !facts.name_records.empty() &&
      facts.name_records.back()->kind == ABI_FUNCTION_RECORD_NAME_TEMPLATE;
    const bool saved_pending = substitution_state_.pending_function_prefix_candidate;
    const StructuralId saved_identity =
      substitution_state_.pending_function_prefix_identity;
    substitution_state_.pending_function_prefix_candidate = facts.is_function_template &&
      !facts.template_argument_refs.empty() &&
      !(facts.function_template_prefix == NULL && final_name_template);
    if(substitution_state_.pending_function_prefix_candidate) {
      substitution_state_.pending_function_prefix_identity =
        function_prefix_identity(target, facts);
    }
    append_function_name(target, facts, result);
    append_function_parameters(target, facts, result);
    substitution_state_.pending_function_prefix_candidate = saved_pending;
    substitution_state_.pending_function_prefix_identity = saved_identity;
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
