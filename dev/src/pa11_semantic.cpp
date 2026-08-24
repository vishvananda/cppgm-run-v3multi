#include "pa11_semantic.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "posttoken.h"

namespace
{
typedef std::size_t Id;
const Id InvalidId = std::numeric_limits<Id>::max();

enum class TypeKind
{
	Fundamental,
	Named,
	Cv,
	Pointer,
	LvalueReference,
	RvalueReference,
	Array,
	Function
};

enum class NamedKind
{
	Class,
	Enum,
	TemplateParameter
};

enum class ClassTag
{
	Struct,
	Class,
	Union
};

struct TypeKey
{
	TypeKind kind;
	FundamentalType fundamental;
	Id child;
	unsigned int cv;
	bool unknown_bound;
	Id bound;
	Id named;
	Id result;
	std::vector<Id> parameters;
	bool variadic;

	TypeKey()
		: kind(TypeKind::Fundamental), fundamental(FundamentalType::Int),
		  child(InvalidId), cv(0), unknown_bound(false), bound(0),
		  named(InvalidId), result(InvalidId), parameters(), variadic(false)
	{}

	bool operator==(const TypeKey& other) const
	{
		return kind == other.kind && fundamental == other.fundamental &&
			child == other.child && cv == other.cv &&
			unknown_bound == other.unknown_bound && bound == other.bound &&
			named == other.named && result == other.result &&
			parameters == other.parameters && variadic == other.variadic;
	}
};

struct TypeKeyHash
{
	static std::size_t combine(std::size_t seed, std::size_t value)
	{
		value += static_cast<std::size_t>(0x9e3779b9U) +
			(seed << 6) + (seed >> 2);
		return seed ^ value;
	}

	std::size_t operator()(const TypeKey& key) const
	{
		std::size_t result = static_cast<std::size_t>(key.kind);
		result = combine(result, static_cast<std::size_t>(key.fundamental));
		result = combine(result, key.child);
		result = combine(result, key.cv);
		result = combine(result, key.unknown_bound ? 1 : 0);
		result = combine(result, key.bound);
		result = combine(result, key.named);
		result = combine(result, key.result);
		result = combine(result, key.variadic ? 1 : 0);
		result = combine(result, key.parameters.size());
		for (std::size_t i = 0; i < key.parameters.size(); ++i)
			result = combine(result, key.parameters[i]);
		return result;
	}
};

struct NamePath
{
	bool global;
	std::vector<Id> components;

	NamePath() : global(false), components() {}

	bool empty() const { return components.empty(); }
	Id last() const
	{
		return components.empty() ? InvalidId : components.back();
	}
};

enum class BindingKind
{
	Type,
	TypeAlias,
	Function,
	Variable,
	Parameter,
	Enumerator
};

struct Binding
{
	BindingKind kind;
	Id name;
	Id type;
	bool has_tag;
	ClassTag class_tag;
	std::vector<ClassTag> declaration_tags;
	Id display_type_name;
	bool has_value;
	std::int64_t value;

	Binding(BindingKind kind = BindingKind::Variable, Id name = InvalidId,
		Id type = InvalidId)
		: kind(kind), name(name), type(type), has_tag(false),
		  class_tag(ClassTag::Struct), declaration_tags(),
		  display_type_name(InvalidId), has_value(false), value(0)
	{}
};

struct ValueRef
{
	Id scope;
	Id binding;

	ValueRef(Id scope = InvalidId, Id binding = InvalidId)
		: scope(scope), binding(binding)
	{}
};

enum class ScopeKind
{
	Namespace,
	Class,
	Function,
	Block,
	Enum,
	TemplateParameters
};

struct Scope
{
	ScopeKind kind;
	Id parent;
	Id name;
	bool inline_namespace;
	std::vector<Id> children;
	std::vector<Binding> bindings;
	std::unordered_map<Id, Id> types;
	std::unordered_map<Id, Id> namespaces;
	std::unordered_map<Id, Id> namespace_aliases;
	std::unordered_map<Id, std::vector<Id> > values;
	std::unordered_map<Id, Id> using_types;
	std::unordered_map<Id, std::vector<Id> > using_values;
	std::vector<Id> using_directives;

	Scope(ScopeKind kind = ScopeKind::Namespace, Id parent = InvalidId,
		Id name = InvalidId, bool inline_namespace = false)
		: kind(kind), parent(parent), name(name),
		  inline_namespace(inline_namespace), children(), bindings(),
		  types(), namespaces(), namespace_aliases(), values(), using_types(),
		  using_values(), using_directives()
	{}
};

struct NamedRecord
{
	NamedKind kind;
	Id name;
	Id owner;
	bool defined;
	ClassTag class_tag;
	bool scoped_enum;
	bool has_underlying;
	Id underlying;
	bool template_template;
	Id scope;

	NamedRecord(NamedKind kind = NamedKind::Class, Id name = InvalidId,
		Id owner = InvalidId)
		: kind(kind), name(name), owner(owner), defined(false),
		  class_tag(ClassTag::Struct), scoped_enum(false),
		  has_underlying(false), underlying(InvalidId),
		  template_template(false), scope(InvalidId)
	{}
};

struct ParamFact
{
	Id name;
	Id type;

	ParamFact(Id name = InvalidId, Id type = InvalidId)
		: name(name), type(type)
	{}
};

struct SpecFact
{
	Id base;
	bool has_base;
	Id anonymous_record;
	unsigned int cv;
	bool is_typedef;
	bool is_constexpr;

	SpecFact()
		: base(InvalidId), has_base(false), anonymous_record(InvalidId), cv(0),
		  is_typedef(false), is_constexpr(false)
	{}
};

struct ConstValue
{
	bool valid;
	bool is_unsigned;
	__int128 value;

	ConstValue(bool valid = false, __int128 value = 0,
		bool is_unsigned = false)
		: valid(valid), is_unsigned(is_unsigned), value(value)
	{}
};

struct DeclaratorName
{
	bool found;
	NamePath path;

	DeclaratorName() : found(false), path() {}
};

struct DeclaratorOp
{
	enum Kind
	{
		Pointer,
		LvalueReference,
		RvalueReference,
		Array,
		Function
	};

	Kind kind;
	unsigned int cv;
	bool unknown_bound;
	Id bound;
	const PA10AstNode* parameter_clause;

	DeclaratorOp(Kind kind = Pointer)
		: kind(kind), cv(0), unknown_bound(false), bound(0),
		  parameter_clause(NULL)
	{}
};

class PA11SemanticModel
{
public:
	explicit PA11SemanticModel(const PA10Ast& ast)
		: ast_(ast), names_(), name_ids_(), types_(), type_ids_(), named_(),
		  scopes_(), global_(InvalidId), deferred_scopes_(),
		  anonymous_union_count_(0)
	{
		global_ = create_scope(ScopeKind::Namespace, InvalidId, InvalidId);
		for (int i = static_cast<int>(FundamentalType::SignedChar);
			i <= static_cast<int>(FundamentalType::NullptrT); ++i)
		{
			TypeKey key;
			key.kind = TypeKind::Fundamental;
			key.fundamental = static_cast<FundamentalType>(i);
			intern_type(key);
		}
	}

	void analyze()
	{
		if (ast_.root.kind != PA10NodeKind::TranslationUnit)
			throw std::runtime_error("PA11 root is not a translation unit");
		for (std::size_t i = 0; i < ast_.root.children.size(); ++i)
			process_declaration(ast_.root.children[i], global_);
		for (std::size_t i = 0; i < deferred_scopes_.size(); ++i)
		{
			const Id scope = deferred_scopes_[i];
			if (scopes_[scope].parent != InvalidId)
				scopes_[scopes_[scope].parent].children.push_back(scope);
		}
	}

	void dump(std::ostream& output) const
	{
		output << "translation-unit\n";
		dump_scope(output, global_, 1);
	}

private:
	const PA10Ast& ast_;
	std::vector<std::string> names_;
	std::unordered_map<std::string, Id> name_ids_;
	std::vector<TypeKey> types_;
	std::unordered_map<TypeKey, Id, TypeKeyHash> type_ids_;
	std::vector<NamedRecord> named_;
	std::vector<Scope> scopes_;
	Id global_;
	std::vector<Id> deferred_scopes_;
	std::size_t anonymous_union_count_;

	static void unsupported(const char* feature)
	{
		throw std::runtime_error(std::string("PA11 semantic feature not implemented: ") +
			feature);
	}

	Id intern_name(const std::string& name)
	{
		std::unordered_map<std::string, Id>::const_iterator found =
			name_ids_.find(name);
		if (found != name_ids_.end())
			return found->second;
		const Id result = names_.size();
		names_.push_back(name);
		name_ids_[name] = result;
		return result;
	}

	const std::string& name_text(Id name) const
	{
		if (name == InvalidId || name >= names_.size())
			throw std::runtime_error("invalid PA11 name identity");
		return names_[name];
	}

	Id qualified_name_id(const NamePath& path)
	{
		std::ostringstream text;
		if (path.global)
			text << "::";
		for (std::size_t i = 0; i < path.components.size(); ++i)
		{
			if (i != 0)
				text << "::";
			text << name_text(path.components[i]);
		}
		return intern_name(text.str());
	}

	Id name_from_spelling(PPSpellingId spelling)
	{
		if (spelling == 0 || spelling >= ast_.producer_spellings.size())
			throw std::runtime_error("invalid PA11 producer spelling");
		return intern_name(ast_.producer_spelling(spelling));
	}

	Id intern_type(const TypeKey& key)
	{
		std::unordered_map<TypeKey, Id, TypeKeyHash>::const_iterator found =
			type_ids_.find(key);
		if (found != type_ids_.end())
			return found->second;
		const Id result = types_.size();
		types_.push_back(key);
		type_ids_[key] = result;
		return result;
	}

	Id fundamental(FundamentalType type) const
	{
		TypeKey key;
		key.kind = TypeKind::Fundamental;
		key.fundamental = type;
		std::unordered_map<TypeKey, Id, TypeKeyHash>::const_iterator found =
			type_ids_.find(key);
		if (found == type_ids_.end())
			throw std::runtime_error("missing PA11 fundamental type");
		return found->second;
	}

	TypeKind type_kind(Id type) const
	{
		if (type == InvalidId || type >= types_.size())
			throw std::runtime_error("invalid PA11 type identity");
		return types_[type].kind;
	}

	Id make_cv(Id child, unsigned int qualifiers)
	{
		if (qualifiers == 0)
			return child;
		if (type_kind(child) == TypeKind::Cv)
		{
			const TypeKey& old = types_[child];
			qualifiers |= old.cv;
			child = old.child;
		}
		TypeKey key;
		key.kind = TypeKind::Cv;
		key.child = child;
		key.cv = qualifiers;
		return intern_type(key);
	}

	Id make_pointer(Id child, unsigned int qualifiers = 0)
	{
		if (type_kind(child) == TypeKind::LvalueReference ||
			type_kind(child) == TypeKind::RvalueReference)
			throw std::runtime_error("pointer to reference type");
		TypeKey key;
		key.kind = TypeKind::Pointer;
		key.child = child;
		key.cv = qualifiers;
		return intern_type(key);
	}

	Id make_reference(Id child, bool rvalue)
	{
		Id unqualified = child;
		if (type_kind(unqualified) == TypeKind::Cv)
			unqualified = types_[unqualified].child;
		if (type_kind(unqualified) == TypeKind::Fundamental &&
			types_[unqualified].fundamental == FundamentalType::Void)
			throw std::runtime_error("reference to void type");
		if (!rvalue && type_kind(child) == TypeKind::LvalueReference)
			return child;
		TypeKey key;
		key.kind = rvalue ? TypeKind::RvalueReference :
			TypeKind::LvalueReference;
		key.child = child;
		return intern_type(key);
	}

	Id make_array(Id child, bool unknown_bound, Id bound)
	{
		const TypeKind kind = type_kind(child);
		if (kind == TypeKind::LvalueReference || kind == TypeKind::RvalueReference)
			throw std::runtime_error("array of reference type");
		TypeKey key;
		key.kind = TypeKind::Array;
		key.child = child;
		key.unknown_bound = unknown_bound;
		key.bound = bound;
		return intern_type(key);
	}

	Id make_function(const std::vector<Id>& parameters, bool variadic,
		Id result)
	{
		TypeKey key;
		key.kind = TypeKind::Function;
		key.parameters = parameters;
		key.variadic = variadic;
		key.result = result;
		return intern_type(key);
	}

	Id create_scope(ScopeKind kind, Id parent, Id name,
		bool inline_namespace = false, bool attach = true)
	{
		const Id result = scopes_.size();
		scopes_.push_back(Scope(kind, parent, name, inline_namespace));
		if (parent != InvalidId && attach)
			scopes_[parent].children.push_back(result);
		if (parent != InvalidId && !attach)
			deferred_scopes_.push_back(result);
		return result;
	}

	const PA10AstNode* child_of_kind(const PA10AstNode& node,
		PA10NodeKind kind) const
	{
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind == kind)
				return &node.children[i];
		return NULL;
	}

	bool is_cv_node(const PA10AstNode& node) const
	{
		return node.kind == PA10NodeKind::CvQualifier ||
			(node.kind == PA10NodeKind::DeclSpecifier && node.has_token &&
				(node.token == SimpleTokenType::KW_CONST ||
				 node.token == SimpleTokenType::KW_VOLATILE));
	}

	unsigned int cv_bit(const PA10AstNode& node) const
	{
		if (!node.has_token)
			return 0;
		if (node.token == SimpleTokenType::KW_CONST)
			return 1u;
		if (node.token == SimpleTokenType::KW_VOLATILE)
			return 2u;
		return 0;
	}

	NamePath name_path(const PA10AstNode& node)
	{
		if (node.name_prefix_count != 0)
			unsupported("decltype-qualified names");
		NamePath result;
		result.global = node.global_name;
		for (std::size_t i = 0; i < node.name_parts.size(); ++i)
		{
			const PA10NameComponent& part = node.name_parts[i];
			if (part.has_template_id)
				unsupported("template-ids");
			result.components.push_back(name_from_spelling(part.spelling));
		}
		if (result.components.empty() && node.producer_spelling != 0)
			result.components.push_back(name_from_spelling(node.producer_spelling));
		if (result.components.empty())
			throw std::runtime_error("PA11 name has no semantic component");
		return result;
	}

	bool find_declarator_name(const PA10AstNode& node, NamePath* result)
	{
		if (node.kind == PA10NodeKind::Identifier &&
			(node.producer_spelling != 0 || !node.name_parts.empty() ||
			 node.global_name || node.name_prefix_count != 0))
		{
			*result = name_path(node);
			return true;
		}
		for (std::size_t i = 0; i < node.children.size(); ++i)
		{
			if (node.children[i].kind == PA10NodeKind::PtrOperator)
				continue;
			if (find_declarator_name(node.children[i], result))
				return true;
		}
		return false;
	}

	DeclaratorName declarator_name(const PA10AstNode& node)
	{
		DeclaratorName result;
		result.found = find_declarator_name(node, &result.path);
		return result;
	}

	ClassTag class_tag(const PA10AstNode& node) const
	{
		const PA10AstNode* key = child_of_kind(node, PA10NodeKind::ClassKey);
		if (key == NULL || !key->has_token)
			throw std::runtime_error("class declaration has no class-key");
		if (key->token == SimpleTokenType::KW_CLASS)
			return ClassTag::Class;
		if (key->token == SimpleTokenType::KW_UNION)
			return ClassTag::Union;
		return ClassTag::Struct;
	}

	Id named_type(Id named)
	{
		TypeKey key;
		key.kind = TypeKind::Named;
		key.named = named;
		return intern_type(key);
	}

	Id named_record_for_type(Id type) const
	{
		Id cursor = type;
		if (type_kind(cursor) == TypeKind::Cv)
			cursor = types_[cursor].child;
		if (type_kind(cursor) != TypeKind::Named)
			return InvalidId;
		return types_[cursor].named;
	}

	Id class_scope_for_type(Id type) const
	{
		const Id record = named_record_for_type(type);
		if (record == InvalidId || record >= named_.size() ||
			named_[record].kind != NamedKind::Class)
			return InvalidId;
		return named_[record].scope;
	}

	Id scope_for_type(Id type) const
	{
		const Id record = named_record_for_type(type);
		if (record == InvalidId || record >= named_.size())
			return InvalidId;
		if (named_[record].kind == NamedKind::Class ||
			named_[record].kind == NamedKind::Enum)
			return named_[record].scope;
		return InvalidId;
	}

	bool direct_value_exists(Id scope, Id name) const
	{
		return scopes_[scope].values.find(name) != scopes_[scope].values.end();
	}

	bool direct_namespace_exists(Id scope, Id name) const
	{
		const Scope& current = scopes_[scope];
		return current.namespaces.find(name) != current.namespaces.end() ||
			current.namespace_aliases.find(name) !=
				current.namespace_aliases.end();
	}

	Id named_namespace(Id parent, Id name)
	{
		Scope& current = scopes_[parent];
		std::unordered_map<Id, Id>::const_iterator found =
			current.namespaces.find(name);
		if (found != current.namespaces.end())
			return found->second;
		if (current.namespace_aliases.find(name) !=
			current.namespace_aliases.end() ||
			current.types.find(name) != current.types.end() ||
			direct_value_exists(parent, name))
			throw std::runtime_error("namespace conflicts with binding");
		return create_named_namespace(parent, name);
	}

	Id create_named_namespace(Id parent, Id name)
	{
		const Id result = create_scope(ScopeKind::Namespace, parent, name);
		scopes_[parent].namespaces[name] = result;
		return result;
	}

	Id lookup_namespace_here(Id scope, Id name) const
	{
		const Scope& current = scopes_[scope];
		std::unordered_map<Id, Id>::const_iterator found =
			current.namespaces.find(name);
		if (found != current.namespaces.end())
			return found->second;
		found = current.namespace_aliases.find(name);
		return found == current.namespace_aliases.end() ? InvalidId : found->second;
	}

	Id lookup_namespace_graph(Id start, Id name,
		std::unordered_set<Id>* visited) const
	{
		if (!visited->insert(start).second)
			return InvalidId;
		const Id direct = lookup_namespace_here(start, name);
		if (direct != InvalidId)
			return direct;
		const Scope& current = scopes_[start];
		for (std::size_t i = current.using_directives.size(); i != 0; --i)
		{
			const Id imported = lookup_namespace_graph(
				current.using_directives[i - 1], name, visited);
			if (imported != InvalidId)
				return imported;
		}
		return InvalidId;
	}

	Id lookup_namespace_unqualified(Id start, Id name) const
	{
		Id scope = start;
		while (scope != InvalidId)
		{
			std::unordered_set<Id> visited;
			const Id found = lookup_namespace_graph(scope, name, &visited);
			if (found != InvalidId)
				return found;
			if (scope == global_)
				break;
			scope = scopes_[scope].parent;
		}
		return InvalidId;
	}

	Id lookup_type_graph(Id start, Id name,
		std::unordered_set<Id>* visited) const
	{
		if (!visited->insert(start).second)
			return InvalidId;
		const Scope& current = scopes_[start];
		std::unordered_map<Id, Id>::const_iterator found =
			current.types.find(name);
		if (found != current.types.end())
			return found->second;
		found = current.using_types.find(name);
		if (found != current.using_types.end())
			return found->second;
		for (std::size_t i = current.using_directives.size(); i != 0; --i)
		{
			const Id target = current.using_directives[i - 1];
			const Id imported = lookup_type_graph(target, name, visited);
			if (imported != InvalidId)
				return imported;
		}
		for (std::size_t i = current.children.size(); i != 0; --i)
		{
			const Scope& child = scopes_[current.children[i - 1]];
			if (!child.inline_namespace)
				continue;
			const Id imported = lookup_type_graph(current.children[i - 1], name,
				visited);
			if (imported != InvalidId)
				return imported;
		}
		return InvalidId;
	}

	Id lookup_type_unqualified(Id start, Id name) const
	{
		Id scope = start;
		while (scope != InvalidId)
		{
			std::unordered_set<Id> visited;
			const Id found = lookup_type_graph(scope, name, &visited);
			if (found != InvalidId)
				return found;
			if (scope == global_)
				break;
			scope = scopes_[scope].parent;
		}
		return InvalidId;
	}

	Id lookup_type_qualified(Id scope, Id name) const
	{
		std::unordered_set<Id> visited;
		return lookup_type_graph(scope, name, &visited);
	}

	std::vector<ValueRef> lookup_value_graph(Id start, Id name,
		std::unordered_set<Id>* visited) const
	{
		if (!visited->insert(start).second)
			return std::vector<ValueRef>();
		const Scope& current = scopes_[start];
		std::unordered_map<Id, std::vector<Id> >::const_iterator found =
			current.values.find(name);
		if (found != current.values.end())
		{
			std::vector<ValueRef> result;
			for (std::size_t i = 0; i < found->second.size(); ++i)
				result.push_back(ValueRef(start, found->second[i]));
			return result;
		}
		for (std::size_t i = current.using_directives.size(); i != 0; --i)
		{
			const std::vector<ValueRef> imported = lookup_value_graph(
				current.using_directives[i - 1], name, visited);
			if (!imported.empty())
				return imported;
		}
		for (std::size_t i = current.children.size(); i != 0; --i)
		{
			const Scope& child = scopes_[current.children[i - 1]];
			if (!child.inline_namespace)
				continue;
			const std::vector<ValueRef> imported = lookup_value_graph(
				current.children[i - 1], name, visited);
			if (!imported.empty())
				return imported;
		}
		return std::vector<ValueRef>();
	}

	std::vector<ValueRef> lookup_value_unqualified(Id start, Id name) const
	{
		Id scope = start;
		while (scope != InvalidId)
		{
			std::unordered_set<Id> visited;
			const std::vector<ValueRef> found = lookup_value_graph(scope, name,
				&visited);
			if (!found.empty())
				return found;
			if (scope == global_)
				break;
			scope = scopes_[scope].parent;
		}
		return std::vector<ValueRef>();
	}

	std::vector<ValueRef> lookup_value_path(const NamePath& path, Id start) const
	{
		if (path.components.empty())
			return std::vector<ValueRef>();
		if (path.components.size() == 1)
		{
			if (!path.global)
				return lookup_value_unqualified(start, path.last());
			std::unordered_set<Id> visited;
			return lookup_value_graph(global_, path.last(), &visited);
		}
		std::vector<Id> prefix(path.components.begin(), path.components.end() - 1);
		const Id scope = path.global ? resolve_global_qualifier_scope(prefix) :
			resolve_qualifier_scope(prefix, start);
		if (scope == InvalidId)
			return std::vector<ValueRef>();
		std::unordered_set<Id> visited;
		return lookup_value_graph(scope, path.last(), &visited);
	}

	Id resolve_qualifier_scope(const std::vector<Id>& components,
		Id start) const
	{
		if (components.empty())
			return InvalidId;
		Id scope = lookup_namespace_unqualified(start, components[0]);
		std::size_t at = 1;
		if (scope == InvalidId)
		{
			const Id type = lookup_type_unqualified(start, components[0]);
			scope = scope_for_type(type);
			if (scope == InvalidId)
				return InvalidId;
		}
		for (; at < components.size(); ++at)
		{
		std::unordered_set<Id> namespace_visited;
		const Id next_namespace = lookup_namespace_graph(scope, components[at],
			&namespace_visited);
			if (next_namespace != InvalidId)
			{
				scope = next_namespace;
				continue;
			}
			const Id type = lookup_type_qualified(scope, components[at]);
			scope = scope_for_type(type);
			if (scope == InvalidId)
				return InvalidId;
		}
		return scope;
	}

	Id lookup_type_path(const NamePath& path, Id start) const
	{
		if (path.components.empty())
			return InvalidId;
		if (path.components.size() == 1)
			return path.global ? lookup_type_qualified(global_, path.last()) :
				lookup_type_unqualified(start, path.last());
		std::vector<Id> prefix(path.components.begin(), path.components.end() - 1);
		const Id scope = path.global ?
			resolve_global_qualifier_scope(prefix) :
			resolve_qualifier_scope(prefix, start);
		return scope == InvalidId ? InvalidId :
			lookup_type_qualified(scope, path.last());
	}

	Id resolve_global_qualifier_scope(const std::vector<Id>& components) const
	{
		if (components.empty())
			return global_;
		std::unordered_set<Id> namespace_visited;
		Id scope = lookup_namespace_graph(global_, components[0],
			&namespace_visited);
		if (scope == InvalidId)
		{
			const Id type = lookup_type_qualified(global_, components[0]);
			scope = scope_for_type(type);
		}
		for (std::size_t i = 1; i < components.size() && scope != InvalidId; ++i)
		{
			std::unordered_set<Id> namespace_visited;
			const Id next_namespace = lookup_namespace_graph(scope, components[i],
				&namespace_visited);
			if (next_namespace != InvalidId)
				scope = next_namespace;
			else
				scope = scope_for_type(lookup_type_qualified(scope,
					components[i]));
		}
		return scope;
	}

	Id resolve_namespace_path(const NamePath& path, Id start) const
	{
		if (path.components.empty())
			return InvalidId;
		if (path.components.size() == 1)
			return path.global ? lookup_namespace_here(global_, path.last()) :
				lookup_namespace_unqualified(start, path.last());
		std::vector<Id> prefix(path.components.begin(), path.components.end() - 1);
		Id scope = path.global ? resolve_global_qualifier_scope(prefix) :
			resolve_qualifier_scope(prefix, start);
		if (scope == InvalidId)
			return InvalidId;
		std::unordered_set<Id> visited;
		return lookup_namespace_graph(scope, path.last(), &visited);
	}

	Id ensure_named_class(Id owner, Id name, ClassTag tag, bool definition)
	{
		Scope& current = scopes_[owner];
		if (direct_namespace_exists(owner, name))
			throw std::runtime_error("class name conflicts with namespace");
		std::unordered_map<Id, Id>::const_iterator found = current.types.find(name);
		Id record_id = InvalidId;
		if (found != current.types.end())
		{
			const Id old_record = named_record_for_type(found->second);
			if (old_record == InvalidId || old_record >= named_.size() ||
				named_[old_record].kind != NamedKind::Class)
				throw std::runtime_error("class name conflicts with type alias");
			if ((tag == ClassTag::Union) !=
				(named_[old_record].class_tag == ClassTag::Union))
				throw std::runtime_error("incompatible union redeclaration");
			record_id = old_record;
		}
		else
		{
			for (std::size_t i = 0; i < current.bindings.size(); ++i)
				if (current.bindings[i].name == name &&
					current.bindings[i].kind == BindingKind::TypeAlias)
					throw std::runtime_error("class name conflicts with type alias");
			if (current.namespace_aliases.find(name) != current.namespace_aliases.end())
				throw std::runtime_error("class name conflicts with namespace");
			NamedRecord record(NamedKind::Class, name, owner);
			record.class_tag = tag;
			record_id = named_.size();
			named_.push_back(record);
			const Id type = named_type(record_id);
			current.types[name] = type;
		}
		if (definition)
		{
			if (named_[record_id].defined)
				throw std::runtime_error("class redefinition");
			named_[record_id].defined = true;
			if (named_[record_id].scope == InvalidId)
				named_[record_id].scope = create_scope(ScopeKind::Class, owner, name);
		}
		return named_type(record_id);
	}

	Id create_anonymous_class(Id owner, ClassTag tag,
		const PA10AstNode& origin)
	{
		NamedRecord record(NamedKind::Class, InvalidId, owner);
		record.class_tag = tag;
		if (tag == ClassTag::Union)
		{
			std::ostringstream name;
			std::size_t begin = origin.source_begin;
			std::size_t end = origin.source_end;
			if (end <= begin)
			{
				begin = anonymous_union_count_++;
				end = begin + 1;
			}
			name << "__anonymous_union_type__" << begin << '_' << end;
			record.name = intern_name(name.str());
		}
		const Id record_id = named_.size();
		named_.push_back(record);
		const Id display_name = record.name;
		named_[record_id].scope = create_scope(ScopeKind::Class, owner,
			display_name, false, tag != ClassTag::Union);
		return named_type(record_id);
	}

	Id ensure_named_enum(Id owner, Id name, bool scoped,
		bool has_underlying, Id underlying, bool definition)
	{
		Scope& current = scopes_[owner];
		if (direct_namespace_exists(owner, name) || direct_value_exists(owner, name))
			throw std::runtime_error("enum name conflicts with binding");
		std::unordered_map<Id, Id>::const_iterator found = current.types.find(name);
		Id record_id = InvalidId;
		if (found != current.types.end())
		{
			record_id = named_record_for_type(found->second);
			if (record_id == InvalidId || record_id >= named_.size() ||
				named_[record_id].kind != NamedKind::Enum)
				throw std::runtime_error("enum name conflicts with type");
			if (named_[record_id].scoped_enum != scoped)
				throw std::runtime_error("incompatible enum redeclaration");
			if (has_underlying && named_[record_id].has_underlying &&
				named_[record_id].underlying != underlying)
				throw std::runtime_error("incompatible enum underlying type");
			if (has_underlying && !named_[record_id].has_underlying)
			{
				named_[record_id].has_underlying = true;
				named_[record_id].underlying = underlying;
			}
		}
		else
		{
			for (std::size_t i = 0; i < current.bindings.size(); ++i)
				if (current.bindings[i].name == name &&
					current.bindings[i].kind == BindingKind::TypeAlias)
					throw std::runtime_error("enum name conflicts with type alias");
			NamedRecord record(NamedKind::Enum, name, owner);
			record.scoped_enum = scoped;
			record.has_underlying = has_underlying;
			record.underlying = underlying;
			record_id = named_.size();
			named_.push_back(record);
			current.types[name] = named_type(record_id);
		}
		if (definition)
		{
			if (named_[record_id].defined)
				throw std::runtime_error("enum redefinition");
			named_[record_id].defined = true;
		}
		if (scoped && named_[record_id].scope == InvalidId)
			named_[record_id].scope = create_scope(ScopeKind::Enum, owner, name);
		return named_type(record_id);
	}

	Id create_anonymous_enum(Id owner, bool scoped, bool has_underlying,
		Id underlying, bool definition)
	{
		NamedRecord record(NamedKind::Enum, InvalidId, owner);
		record.scoped_enum = scoped;
		record.has_underlying = has_underlying;
		record.underlying = underlying;
		record.defined = definition;
		const Id record_id = named_.size();
		named_.push_back(record);
		if (scoped)
			named_[record_id].scope = create_scope(ScopeKind::Enum, owner,
			InvalidId);
		return named_type(record_id);
	}

	void finalize_anonymous_record(Id type, Id name, Id owner)
	{
		const Id record_id = named_record_for_type(type);
		if (record_id == InvalidId || record_id >= named_.size())
			throw std::runtime_error("invalid anonymous type");
		NamedRecord& record = named_[record_id];
		if (record.name != InvalidId)
			return;
		Scope& current = scopes_[owner];
		if (direct_namespace_exists(owner, name) || direct_value_exists(owner, name) ||
			current.types.find(name) != current.types.end())
			throw std::runtime_error("anonymous type conflicts with binding");
		record.name = name;
		current.types[name] = type;
		if (record.scope != InvalidId)
			scopes_[record.scope].name = name;
		if (record.kind == NamedKind::Class)
			add_type_binding(owner, name, type, record.class_tag, true);
		else
		{
			Binding binding(BindingKind::Type, name, type);
			std::size_t position = current.bindings.size();
			for (std::size_t i = 0; i < current.bindings.size(); ++i)
				if (current.bindings[i].kind == BindingKind::Enumerator)
				{
					position = i;
					break;
				}
			current.bindings.insert(current.bindings.begin() + position, binding);
			for (std::unordered_map<Id, std::vector<Id> >::iterator it =
				current.values.begin(); it != current.values.end(); ++it)
				for (std::size_t i = 0; i < it->second.size(); ++i)
					if (it->second[i] >= position)
						++it->second[i];
		}
	}

	void inject_anonymous_union(Id type, Id owner)
	{
		const Id record_id = named_record_for_type(type);
		if (record_id == InvalidId || record_id >= named_.size() ||
			named_[record_id].scope == InvalidId)
			throw std::runtime_error("anonymous union has no scope");
		const Scope& source = scopes_[named_[record_id].scope];
		for (std::size_t i = 0; i < source.bindings.size(); ++i)
		{
			const Binding& binding = source.bindings[i];
			if (binding.kind == BindingKind::Variable ||
				binding.kind == BindingKind::Function)
				add_value(owner, binding.name, binding.type,
					binding.kind == BindingKind::Function);
		}
	}

	bool enum_is_scoped(const PA10AstNode& node) const
	{
		const PA10AstNode* key = child_of_kind(node, PA10NodeKind::EnumKey);
		return key != NULL && key->has_token &&
			(key->token == SimpleTokenType::KW_CLASS ||
			 key->token == SimpleTokenType::KW_STRUCT);
	}

	NamePath enum_name(const PA10AstNode& node)
	{
		if (!node.name_parts.empty() || node.global_name)
			return name_path(node);
		if (node.producer_spelling != 0)
		{
			NamePath result;
			result.components.push_back(name_from_spelling(node.producer_spelling));
			return result;
		}
		return NamePath();
	}

	Id process_enum_specifier(const PA10AstNode& node, Id scope,
		Id* anonymous_record)
	{
		const bool scoped = enum_is_scoped(node);
		const NamePath name = enum_name(node);
		const bool definition = !node.children.empty() &&
			child_of_kind(node, PA10NodeKind::Enumerator) != NULL;
		bool has_underlying = false;
		Id underlying = fundamental(FundamentalType::Int);
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind == PA10NodeKind::TypeId)
			{
				underlying = type_from_type_id(node.children[i], scope);
				has_underlying = true;
			}
		if (name.empty() && !scoped && !definition)
			throw std::runtime_error("opaque unscoped enum");
		if (!scoped && !definition && !has_underlying && !name.empty())
		{
			const Id existing = lookup_type_path(name, scope);
			const Id record = named_record_for_type(existing);
			if (record == InvalidId || record >= named_.size() ||
				named_[record].kind != NamedKind::Enum)
				throw std::runtime_error("undeclared elaborated enum");
			return existing;
		}
		Id owner = scope;
		if (!name.empty())
		{
			owner = declaration_scope(name, scope);
			if (owner == InvalidId)
				throw std::runtime_error("unresolved enum declaration scope");
		}
		Id type;
		bool qualified_definition = false;
		Id qualified_scope = InvalidId;
		if (name.empty())
		{
			type = create_anonymous_enum(owner, scoped, has_underlying,
				underlying, definition);
			if (anonymous_record != NULL)
				*anonymous_record = named_record_for_type(type);
		}
		else
		{
			type = ensure_named_enum(owner, name.last(), scoped, has_underlying,
				underlying, definition);
			if (definition && name.components.size() > 1)
			{
				const Id display = qualified_name_id(name);
				add_type_binding(scope, display, type, ClassTag::Struct, false);
				for (std::size_t i = scopes_[scope].bindings.size(); i != 0; --i)
					if (scopes_[scope].bindings[i - 1].kind == BindingKind::Type &&
						scopes_[scope].bindings[i - 1].name == display)
					{
						scopes_[scope].bindings[i - 1].display_type_name = display;
						break;
					}
				qualified_scope = create_scope(ScopeKind::Enum, scope, display);
				qualified_definition = true;
			}
			else
				add_type_binding(owner, name.last(), type, ClassTag::Struct, false);
		}
		Id value_scope = owner;
		const Id record_id = named_record_for_type(type);
		if (qualified_definition)
			value_scope = qualified_scope;
		else if (record_id != InvalidId && named_[record_id].scope != InvalidId)
			value_scope = named_[record_id].scope;
		std::int64_t next_value = 0;
		bool have_next = false;
		for (std::size_t i = 0; i < node.children.size(); ++i)
		{
			const PA10AstNode& child = node.children[i];
			if (child.kind != PA10NodeKind::Enumerator)
				continue;
			ConstValue value;
			if (!child.children.empty())
				value = eval_constexpr(child.children.front(), owner);
			if (!value.valid)
			{
				if (have_next && next_value == std::numeric_limits<std::int64_t>::max())
					throw std::runtime_error("enumerator value overflow");
				value = ConstValue(true, have_next ?
					static_cast<__int128>(next_value) : 0, false);
			}
			if (value.value < static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) ||
				value.value > static_cast<__int128>(std::numeric_limits<std::int64_t>::max()))
				throw std::runtime_error("enumerator value overflow");
			add_enumerator(value_scope, name_from_spelling(child.producer_spelling),
				type, static_cast<std::int64_t>(value.value));
			if (qualified_definition)
				scopes_[value_scope].bindings.back().display_type_name =
					qualified_name_id(name);
			next_value = static_cast<std::int64_t>(value.value) + 1;
			have_next = true;
		}
		return type;
	}

	void add_enumerator(Id scope, Id name, Id type, std::int64_t value)
	{
		Scope& current = scopes_[scope];
		if (current.types.find(name) != current.types.end() ||
			direct_namespace_exists(scope, name) || direct_value_exists(scope, name))
			throw std::runtime_error("enumerator conflicts with binding");
		Binding binding(BindingKind::Enumerator, name, type);
		binding.has_value = true;
		binding.value = value;
		const Id index = current.bindings.size();
		current.bindings.push_back(binding);
		current.values[name].push_back(index);
	}

	bool integral_type(FundamentalType type) const
	{
		switch (type)
		{
		case FundamentalType::SignedChar:
		case FundamentalType::ShortInt:
		case FundamentalType::Int:
		case FundamentalType::LongInt:
		case FundamentalType::LongLongInt:
		case FundamentalType::UnsignedChar:
		case FundamentalType::UnsignedShortInt:
		case FundamentalType::UnsignedInt:
		case FundamentalType::UnsignedLongInt:
		case FundamentalType::UnsignedLongLongInt:
		case FundamentalType::WcharT:
		case FundamentalType::Char:
		case FundamentalType::Char16T:
		case FundamentalType::Char32T:
		case FundamentalType::Bool:
			return true;
		default:
			return false;
		}
	}

	bool unsigned_type(FundamentalType type) const
	{
		return type == FundamentalType::UnsignedChar ||
			type == FundamentalType::UnsignedShortInt ||
			type == FundamentalType::UnsignedInt ||
			type == FundamentalType::UnsignedLongInt ||
			type == FundamentalType::UnsignedLongLongInt;
	}

	ConstValue literal_constant(const PA10AstNode& node) const
	{
		if (!node.has_literal || node.literal.element_count != 0 ||
			!integral_type(node.literal.type) || node.literal.bytes.empty() ||
			node.literal.bytes.size() > sizeof(std::uint64_t))
			throw std::runtime_error("non-integral constant expression");
		std::uint64_t bits = 0;
		for (std::size_t i = 0; i < node.literal.bytes.size(); ++i)
			bits |= static_cast<std::uint64_t>(node.literal.bytes[i]) << (i * 8);
		__int128 value = static_cast<__int128>(bits);
		if (!unsigned_type(node.literal.type) &&
			(node.literal.bytes.back() & 0x80) != 0 &&
			node.literal.bytes.size() < sizeof(std::uint64_t))
			value -= static_cast<__int128>(1) <<
				(node.literal.bytes.size() * 8);
		return ConstValue(true, value, unsigned_type(node.literal.type));
	}

	void check_constant_range(const ConstValue& value) const
	{
		if (value.is_unsigned)
		{
			if (value.value < 0 || value.value >
				static_cast<__int128>(std::numeric_limits<std::uint64_t>::max()))
				throw std::runtime_error("constant expression overflow");
		}
		else if (value.value < static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) ||
			value.value > static_cast<__int128>(std::numeric_limits<std::int64_t>::max()))
			throw std::runtime_error("constant expression overflow");
	}

	std::size_t type_size(Id type) const
	{
		if (type == InvalidId || type >= types_.size())
			throw std::runtime_error("invalid sizeof type");
		const TypeKey& key = types_[type];
		switch (key.kind)
		{
		case TypeKind::Cv:
			return type_size(key.child);
		case TypeKind::Fundamental:
			switch (key.fundamental)
			{
			case FundamentalType::SignedChar:
			case FundamentalType::UnsignedChar:
			case FundamentalType::Char:
			case FundamentalType::Bool:
				return 1;
			case FundamentalType::ShortInt:
			case FundamentalType::UnsignedShortInt:
			case FundamentalType::Char16T:
				return 2;
			case FundamentalType::Int:
			case FundamentalType::UnsignedInt:
			case FundamentalType::Float:
				return 4;
			case FundamentalType::LongInt:
			case FundamentalType::LongLongInt:
			case FundamentalType::UnsignedLongInt:
			case FundamentalType::UnsignedLongLongInt:
			case FundamentalType::Double:
				return 8;
			case FundamentalType::WcharT:
			case FundamentalType::Char32T:
				return 4;
			case FundamentalType::LongDouble:
				return 16;
			default:
				throw std::runtime_error("sizeof void type");
			}
		case TypeKind::Pointer:
		case TypeKind::LvalueReference:
		case TypeKind::RvalueReference:
			return 8;
		case TypeKind::Array:
			if (key.unknown_bound)
				throw std::runtime_error("sizeof incomplete array");
			return key.bound * type_size(key.child);
		case TypeKind::Function:
			throw std::runtime_error("sizeof function type");
		case TypeKind::Named:
		{
			const Id record_id = key.named;
			if (record_id >= named_.size())
				throw std::runtime_error("invalid named sizeof type");
			const NamedRecord& record = named_[record_id];
			if (record.kind == NamedKind::Enum)
				return type_size(record.has_underlying ? record.underlying :
					fundamental(FundamentalType::Int));
			if (record.kind == NamedKind::TemplateParameter)
				throw std::runtime_error("sizeof template parameter");
			if (!record.defined)
				throw std::runtime_error("sizeof incomplete class");
			return 1;
		}
		}
		throw std::runtime_error("unhandled sizeof type");
	}

	Id expression_type(const PA10AstNode& node, Id scope)
	{
		if (node.kind == PA10NodeKind::Literal ||
			node.kind == PA10NodeKind::KeywordLiteral)
			return fundamental(node.kind == PA10NodeKind::KeywordLiteral ?
				FundamentalType::Bool : node.literal.type);
		if (node.kind == PA10NodeKind::ParenthesizedExpression)
		{
			if (node.children.empty())
				throw std::runtime_error("empty parenthesized expression");
			return expression_type(node.children.front(), scope);
		}
		if (node.kind == PA10NodeKind::IdExpression)
		{
			const NamePath name = name_path(node);
			const std::vector<ValueRef> values = lookup_value_path(name, scope);
			if (!values.empty())
				return scopes_[values.front().scope].bindings[
					values.front().binding].type;
			const Id type = lookup_type_path(name, scope);
			if (type != InvalidId)
				return type;
			throw std::runtime_error("unknown expression name");
		}
		if (node.kind == PA10NodeKind::UnaryExpression ||
			node.kind == PA10NodeKind::BinaryExpression ||
			node.kind == PA10NodeKind::AssignmentExpression ||
			node.kind == PA10NodeKind::ConditionalExpression)
		{
			if (node.children.empty())
				throw std::runtime_error("empty expression");
			return expression_type(node.children.front(), scope);
		}
		if (node.kind == PA10NodeKind::CastExpression)
		{
			if (node.children.size() < 2)
				throw std::runtime_error("invalid cast expression");
			return type_from_type_id(node.children.front(), scope);
		}
		if (node.kind == PA10NodeKind::SizeofExpression ||
			node.kind == PA10NodeKind::TypeTraitExpression)
			return fundamental(FundamentalType::LongLongInt);
		throw std::runtime_error("unsupported expression type");
	}

	Id sizeof_operand_type(const PA10AstNode& node, Id scope)
	{
		if (node.children.empty())
			throw std::runtime_error("sizeof has no operand");
		const PA10AstNode& operand = node.children.front();
		if (operand.kind == PA10NodeKind::TypeId)
			return type_from_type_id(operand, scope);
		if (operand.kind == PA10NodeKind::IdExpression)
		{
			const Id type = lookup_type_path(name_path(operand), scope);
			if (type != InvalidId)
				return type;
		}
		return expression_type(operand, scope);
	}

	ConstValue eval_constexpr(const PA10AstNode& node, Id scope)
	{
		if (node.kind == PA10NodeKind::Literal)
			return literal_constant(node);
		if (node.kind == PA10NodeKind::KeywordLiteral)
			return ConstValue(true, node.has_token &&
				node.token == SimpleTokenType::KW_TRUE ? 1 : 0, false);
		if (node.kind == PA10NodeKind::ParenthesizedExpression)
		{
			if (node.children.empty())
				throw std::runtime_error("empty constant expression");
			return eval_constexpr(node.children.front(), scope);
		}
		if (node.kind == PA10NodeKind::IdExpression)
		{
			const std::vector<ValueRef> values = lookup_value_path(name_path(node),
				scope);
			if (values.empty())
				throw std::runtime_error("constant name is not a value");
			const Binding& binding = scopes_[values.front().scope].bindings[
				values.front().binding];
			if (!binding.has_value)
				throw std::runtime_error("value is not a constant");
			return ConstValue(true, binding.value, false);
		}
		if (node.kind == PA10NodeKind::UnaryExpression)
		{
			if (node.children.size() != 1)
				throw std::runtime_error("invalid unary constant expression");
			ConstValue value = eval_constexpr(node.children.front(), scope);
			switch (node.token)
			{
			case SimpleTokenType::OP_PLUS:
				break;
			case SimpleTokenType::OP_MINUS:
				value.value = -value.value;
				break;
			case SimpleTokenType::OP_LNOT:
				value.value = value.value == 0 ? 1 : 0;
				value.is_unsigned = false;
				break;
			case SimpleTokenType::OP_COMPL:
				value.value = ~value.value;
				break;
			default:
				throw std::runtime_error("unsupported unary constant operator");
			}
			check_constant_range(value);
			return value;
		}
		if (node.kind == PA10NodeKind::BinaryExpression ||
			node.kind == PA10NodeKind::AssignmentExpression)
		{
			if (node.children.size() != 2)
				throw std::runtime_error("invalid binary constant expression");
			const ConstValue left = eval_constexpr(node.children[0], scope);
			if (node.token == SimpleTokenType::OP_LAND && left.value == 0)
				return ConstValue(true, 0, false);
			if (node.token == SimpleTokenType::OP_LOR && left.value != 0)
				return ConstValue(true, 1, false);
			const ConstValue right = eval_constexpr(node.children[1], scope);
			ConstValue result(true, 0, left.is_unsigned || right.is_unsigned);
			switch (node.token)
			{
			case SimpleTokenType::OP_PLUS: result.value = left.value + right.value; break;
			case SimpleTokenType::OP_MINUS: result.value = left.value - right.value; break;
			case SimpleTokenType::OP_STAR: result.value = left.value * right.value; break;
			case SimpleTokenType::OP_DIV:
				if (right.value == 0) throw std::runtime_error("constant division by zero");
				result.value = left.value / right.value; break;
			case SimpleTokenType::OP_MOD:
				if (right.value == 0) throw std::runtime_error("constant modulo by zero");
				result.value = left.value % right.value; break;
			case SimpleTokenType::OP_LSHIFT: result.value = left.value << right.value; break;
			case SimpleTokenType::OP_RSHIFT: result.value = left.value >> right.value; break;
			case SimpleTokenType::OP_BOR: result.value = left.value | right.value; break;
			case SimpleTokenType::OP_XOR: result.value = left.value ^ right.value; break;
			case SimpleTokenType::OP_AMP: result.value = left.value & right.value; break;
			case SimpleTokenType::OP_EQ: result.value = left.value == right.value; result.is_unsigned = false; break;
			case SimpleTokenType::OP_NE: result.value = left.value != right.value; result.is_unsigned = false; break;
			case SimpleTokenType::OP_LT: result.value = left.value < right.value; result.is_unsigned = false; break;
			case SimpleTokenType::OP_LE: result.value = left.value <= right.value; result.is_unsigned = false; break;
			case SimpleTokenType::OP_GT: result.value = left.value > right.value; result.is_unsigned = false; break;
			case SimpleTokenType::OP_GE: result.value = left.value >= right.value; result.is_unsigned = false; break;
			case SimpleTokenType::OP_LAND: result.value = (left.value != 0 && right.value != 0); result.is_unsigned = false; break;
			case SimpleTokenType::OP_LOR: result.value = (left.value != 0 || right.value != 0); result.is_unsigned = false; break;
			default: throw std::runtime_error("unsupported binary constant operator");
			}
			check_constant_range(result);
			return result;
		}
		if (node.kind == PA10NodeKind::ConditionalExpression)
		{
			if (node.children.size() != 3)
				throw std::runtime_error("invalid conditional constant expression");
			return eval_constexpr(node.children[eval_constexpr(node.children[0],
				scope).value != 0 ? 1 : 2], scope);
		}
		if (node.kind == PA10NodeKind::CastExpression)
		{
			if (node.children.size() < 2)
				throw std::runtime_error("invalid cast constant expression");
			return eval_constexpr(node.children.back(), scope);
		}
		if (node.kind == PA10NodeKind::SizeofExpression ||
			node.kind == PA10NodeKind::TypeTraitExpression)
			return ConstValue(true, static_cast<__int128>(type_size(
				sizeof_operand_type(node, scope))), false);
		throw std::runtime_error("unsupported constant expression");
	}

	Id decltype_type(const PA10AstNode& node, Id scope)
	{
		if (node.children.empty())
			throw std::runtime_error("decltype has no expression");
		const PA10AstNode& expression = node.children.front();
		bool parenthesized = expression.kind == PA10NodeKind::ParenthesizedExpression;
		const PA10AstNode* subject = &expression;
		if (parenthesized)
		{
			if (expression.children.empty())
				throw std::runtime_error("empty decltype expression");
			subject = &expression.children.front();
		}
		if (subject->kind == PA10NodeKind::IdExpression)
		{
			const std::vector<ValueRef> values = lookup_value_path(
				name_path(*subject), scope);
			if (!values.empty())
			{
				const Binding& binding = scopes_[values.front().scope].bindings[
					values.front().binding];
				if (parenthesized && binding.kind != BindingKind::Enumerator)
					return make_reference(binding.type, false);
				return binding.type;
			}
		}
		const Id type = expression_type(*subject, scope);
		return parenthesized ? make_reference(type, false) : type;
	}

	void add_type_binding(Id scope, Id name, Id type, ClassTag tag,
		bool has_tag)
	{
		Scope& current = scopes_[scope];
		std::unordered_map<Id, Id>::const_iterator type_found =
			current.types.find(name);
		if (type_found == current.types.end())
			current.types[name] = type;
		else if (type_found->second != type)
			throw std::runtime_error("incompatible type binding");
		for (std::size_t i = 0; i < current.bindings.size(); ++i)
		{
			Binding& existing = current.bindings[i];
			if (existing.kind != BindingKind::Type || existing.name != name)
				continue;
			if (existing.type != type)
				throw std::runtime_error("incompatible type binding");
			if (has_tag)
			{
				existing.has_tag = true;
				bool present = false;
				for (std::size_t j = 0; j < existing.declaration_tags.size(); ++j)
					present = present || existing.declaration_tags[j] == tag;
				if (!present)
					existing.declaration_tags.push_back(tag);
			}
			return;
		}
		Binding binding(BindingKind::Type, name, type);
		binding.has_tag = has_tag;
		binding.class_tag = tag;
		if (has_tag)
			binding.declaration_tags.push_back(tag);
		current.bindings.push_back(binding);
	}

	void add_type_alias(Id scope, Id name, Id type)
	{
		Scope& current = scopes_[scope];
		if (current.namespaces.find(name) != current.namespaces.end() ||
			current.namespace_aliases.find(name) != current.namespace_aliases.end() ||
			direct_value_exists(scope, name))
			throw std::runtime_error("type alias conflicts with binding");
		std::unordered_map<Id, Id>::const_iterator found = current.types.find(name);
		if (found != current.types.end() && found->second != type)
			throw std::runtime_error("type alias redefinition");
		current.types[name] = type;
		current.bindings.push_back(Binding(BindingKind::TypeAlias, name, type));
	}

	Id add_value(Id scope, Id name, Id type, bool function)
	{
		Scope& current = scopes_[scope];
		if (direct_namespace_exists(scope, name))
			throw std::runtime_error("value conflicts with namespace");
		std::unordered_map<Id, Id>::const_iterator type_found = current.types.find(name);
		if (type_found != current.types.end() &&
			type_kind(type_found->second) != TypeKind::Named)
			throw std::runtime_error("value conflicts with type alias");
		const Id binding_id = current.bindings.size();
		current.bindings.push_back(Binding(function ? BindingKind::Function :
			BindingKind::Variable, name, type));
		current.values[name].push_back(binding_id);
		return binding_id;
	}

	Id declaration_scope(const NamePath& path, Id current) const
	{
		if (path.components.size() <= 1 && !path.global)
			return current;
		if (path.components.empty())
			return InvalidId;
		std::vector<Id> prefix(path.components.begin(), path.components.end() - 1);
		return path.global ? resolve_global_qualifier_scope(prefix) :
			resolve_qualifier_scope(prefix, current);
	}

	SpecFact spec_fact(const PA10AstNode& node, Id scope)
	{
		if (node.kind != PA10NodeKind::DeclSpecifierSeq &&
			node.kind != PA10NodeKind::TypeSpecifierSeq)
			throw std::runtime_error("PA11 expected declaration specifier sequence");
		SpecFact result;
		bool has_signed = false;
		bool has_unsigned = false;
		bool has_short = false;
		unsigned int long_count = 0;
		bool has_int = false;
		bool has_char = false;
		bool has_char16 = false;
		bool has_char32 = false;
		bool has_wchar = false;
		bool has_bool = false;
		bool has_float = false;
		bool has_double = false;
		bool has_void = false;
		for (std::size_t i = 0; i < node.children.size(); ++i)
		{
			const PA10AstNode& child = node.children[i];
			if (child.kind == PA10NodeKind::ClassSpecifier)
			{
				const NamePath name = class_name(child);
				const ClassTag tag = class_tag(child);
				Id owner = scope;
				Id type;
				if (name.empty())
				{
					type = create_anonymous_class(scope, tag, child);
					result.anonymous_record = named_record_for_type(type);
				}
				else
				{
					owner = declaration_scope(name, scope);
					if (owner == InvalidId)
						throw std::runtime_error("unresolved class declaration scope");
					type = ensure_named_class(owner, name.last(), tag, true);
					add_type_binding(owner, name.last(), type, tag, true);
				}
				process_class_body(child, type, owner);
				result.base = type;
				result.has_base = true;
				continue;
			}
			if (child.kind == PA10NodeKind::ClassForwardDeclaration)
			{
				const NamePath name = class_name(child);
				if (name.empty())
					unsupported("anonymous class forward declaration");
				const ClassTag tag = class_tag(child);
				const Id owner = declaration_scope(name, scope);
				if (owner == InvalidId)
					throw std::runtime_error("unresolved class declaration scope");
				const Id type = ensure_named_class(owner, name.last(), tag, false);
				add_type_binding(owner, name.last(), type, tag, true);
				result.base = type;
				result.has_base = true;
				continue;
			}
			if (child.kind == PA10NodeKind::EnumSpecifier)
			{
				Id anonymous_record = InvalidId;
				result.base = process_enum_specifier(child, scope,
					&anonymous_record);
				result.anonymous_record = anonymous_record;
				result.has_base = true;
				continue;
			}
			if (child.kind == PA10NodeKind::DecltypeSpecifier ||
				(child.kind == PA10NodeKind::DeclSpecifier && child.has_token &&
				 child.token == SimpleTokenType::KW_DECLTYPE))
			{
				result.base = decltype_type(child, scope);
				result.has_base = true;
				continue;
			}
			if (child.kind == PA10NodeKind::TypeName ||
				(child.kind == PA10NodeKind::DeclSpecifier &&
					(!child.name_parts.empty() || child.producer_spelling != 0)))
			{
				const NamePath name = name_path(child);
				const Id type = lookup_type_path(name, scope);
				if (type == InvalidId)
					throw std::runtime_error("unknown PA11 type name");
				result.base = type;
				result.has_base = true;
				continue;
			}
			if (!child.has_token)
				continue;
			switch (child.token)
			{
			case SimpleTokenType::KW_TYPEDEF:
				result.is_typedef = true;
				break;
			case SimpleTokenType::KW_CONSTEXPR:
				result.is_constexpr = true;
				break;
			case SimpleTokenType::KW_CONST:
			case SimpleTokenType::KW_VOLATILE:
				result.cv |= cv_bit(child);
				break;
			case SimpleTokenType::KW_SIGNED:
				has_signed = true;
				break;
			case SimpleTokenType::KW_UNSIGNED:
				has_unsigned = true;
				break;
			case SimpleTokenType::KW_SHORT:
				has_short = true;
				break;
			case SimpleTokenType::KW_LONG:
				++long_count;
				break;
			case SimpleTokenType::KW_INT:
				has_int = true;
				break;
			case SimpleTokenType::KW_CHAR:
				has_char = true;
				break;
			case SimpleTokenType::KW_CHAR16_T:
				has_char16 = true;
				break;
			case SimpleTokenType::KW_CHAR32_T:
				has_char32 = true;
				break;
			case SimpleTokenType::KW_WCHAR_T:
				has_wchar = true;
				break;
			case SimpleTokenType::KW_BOOL:
				has_bool = true;
				break;
			case SimpleTokenType::KW_FLOAT:
				has_float = true;
				break;
			case SimpleTokenType::KW_DOUBLE:
				has_double = true;
				break;
			case SimpleTokenType::KW_VOID:
				has_void = true;
				break;
			default:
				break;
			}
		}
		if (!result.has_base)
		{
			FundamentalType type = FundamentalType::Int;
			if (has_void)
				type = FundamentalType::Void;
			else if (has_char)
				type = has_signed ? FundamentalType::SignedChar :
					has_unsigned ? FundamentalType::UnsignedChar : FundamentalType::Char;
			else if (has_char16)
				type = FundamentalType::Char16T;
			else if (has_char32)
				type = FundamentalType::Char32T;
			else if (has_wchar)
				type = FundamentalType::WcharT;
			else if (has_bool)
				type = FundamentalType::Bool;
			else if (has_float)
				type = FundamentalType::Float;
			else if (has_double)
				type = long_count == 0 ? FundamentalType::Double :
					FundamentalType::LongDouble;
			else if (long_count >= 2)
				type = has_unsigned ? FundamentalType::UnsignedLongLongInt :
					FundamentalType::LongLongInt;
			else if (long_count == 1)
				type = has_unsigned ? FundamentalType::UnsignedLongInt :
					FundamentalType::LongInt;
			else if (has_short)
				type = has_unsigned ? FundamentalType::UnsignedShortInt :
					has_signed ? FundamentalType::ShortInt : FundamentalType::ShortInt;
			else if (has_unsigned)
				type = FundamentalType::UnsignedInt;
			else if (has_signed || has_int)
				type = FundamentalType::Int;
			else
				throw std::runtime_error("declaration has no PA11 type");
			result.base = fundamental(type);
			result.has_base = true;
		}
		result.base = make_cv(result.base, result.cv);
		return result;
	}

	NamePath class_name(const PA10AstNode& node)
	{
		if (!node.name_parts.empty() || node.global_name)
			return name_path(node);
		if (node.producer_spelling != 0)
		{
			NamePath result;
			result.components.push_back(name_from_spelling(node.producer_spelling));
			return result;
		}
		return NamePath();
	}

	void process_class_body(const PA10AstNode& node, Id type, Id owner)
	{
		const Id class_scope = class_scope_for_type(type);
		if (class_scope == InvalidId)
			throw std::runtime_error("class has no class scope");
		for (std::size_t i = 0; i < node.children.size(); ++i)
		{
			const PA10NodeKind kind = node.children[i].kind;
			if (kind == PA10NodeKind::ClassKey || kind == PA10NodeKind::BaseClause)
				continue;
			if (kind == PA10NodeKind::AccessSpecifier ||
				kind == PA10NodeKind::EmptyDeclaration)
				continue;
			process_declaration(node.children[i], class_scope);
		}
		(void)owner;
	}

	Id type_from_type_id(const PA10AstNode& node, Id scope)
	{
		if (node.kind != PA10NodeKind::TypeId || node.children.empty())
			throw std::runtime_error("invalid PA11 type-id");
		SpecFact spec = spec_fact(node.children.front(), scope);
		Id result = spec.base;
		if (node.children.size() > 1)
			result = apply_declarator(node.children[1], result, scope);
		return result;
	}

	DeclaratorOp pointer_op(const PA10AstNode& node)
	{
		if (!node.has_token)
			unsupported("member-pointer declarators");
		DeclaratorOp result;
		if (node.token == SimpleTokenType::OP_STAR)
			result.kind = DeclaratorOp::Pointer;
		else if (node.token == SimpleTokenType::OP_AMP)
			result.kind = DeclaratorOp::LvalueReference;
		else if (node.token == SimpleTokenType::OP_LAND)
			result.kind = DeclaratorOp::RvalueReference;
		else
			unsupported("unknown pointer operator");
		return result;
	}

	Id literal_bound(const PA10AstNode& node) const
	{
		if (node.kind != PA10NodeKind::Literal || !node.has_literal ||
			node.literal.bytes.size() > sizeof(std::uint64_t))
			throw std::runtime_error("unsupported PA11 array bound");
		std::uint64_t value = 0;
		for (std::size_t i = 0; i < node.literal.bytes.size(); ++i)
			value |= static_cast<std::uint64_t>(node.literal.bytes[i]) << (i * 8);
		if (value == 0 || value > static_cast<std::uint64_t>(InvalidId))
			throw std::runtime_error("invalid PA11 array bound");
		return static_cast<Id>(value);
	}

	std::vector<Id> parameter_types(const PA10AstNode& clause, Id scope,
		bool* variadic, std::vector<ParamFact>* facts)
	{
		if (clause.kind != PA10NodeKind::ParameterClause)
			throw std::runtime_error("invalid PA11 parameter clause");
		std::vector<Id> result;
		*variadic = false;
		for (std::size_t i = 0; i < clause.children.size(); ++i)
		{
			const PA10AstNode& child = clause.children[i];
			if (child.kind == PA10NodeKind::ParameterPack)
			{
				*variadic = true;
				continue;
			}
			if (child.kind != PA10NodeKind::ParameterDeclaration ||
				child.children.empty())
				throw std::runtime_error("invalid PA11 parameter declaration");
			SpecFact spec = spec_fact(child.children.front(), scope);
			Id type = spec.base;
			DeclaratorName name;
			if (child.children.size() > 1)
			{
				name = declarator_name(child.children[1]);
				type = apply_declarator(child.children[1], type, scope);
			}
			const bool unnamed_void = type_kind(type) == TypeKind::Fundamental &&
				types_[type].fundamental == FundamentalType::Void && !name.found;
			if (unnamed_void && clause.children.size() == 1)
			{
				// The one special parameter declaration `(void)` denotes an
				// empty parameter list.  `void *` has a declarator and is kept.
				continue;
			}
			result.push_back(type);
			if (facts != NULL)
				facts->push_back(ParamFact(name.found ? name.path.last() : InvalidId,
					type));
		}
		return result;
	}

	Id apply_prefix(const std::vector<DeclaratorOp>& ops, Id base)
	{
		Id result = base;
		for (std::size_t i = 0; i < ops.size(); ++i)
		{
			switch (ops[i].kind)
			{
			case DeclaratorOp::Pointer:
				result = make_pointer(result, ops[i].cv);
				break;
			case DeclaratorOp::LvalueReference:
				result = make_reference(result, false);
				break;
			case DeclaratorOp::RvalueReference:
				result = make_reference(result, true);
				break;
			case DeclaratorOp::Array:
			case DeclaratorOp::Function:
				throw std::runtime_error("invalid PA11 prefix declarator operation");
			}
		}
		return result;
	}

	Id apply_suffix(const std::vector<DeclaratorOp>& ops, Id base, Id scope)
	{
		Id result = base;
		for (std::size_t i = 0; i < ops.size(); ++i)
		{
			if (ops[i].kind == DeclaratorOp::Array)
			{
				result = make_array(result, ops[i].unknown_bound, ops[i].bound);
				continue;
			}
			if (ops[i].kind == DeclaratorOp::Function)
			{
				bool variadic = false;
				const std::vector<Id> parameters = parameter_types(
					*ops[i].parameter_clause, scope, &variadic, NULL);
				result = make_function(parameters, variadic, result);
				continue;
			}
			throw std::runtime_error("invalid PA11 suffix declarator operation");
		}
		return result;
	}

	Id apply_declarator(const PA10AstNode& node, Id base, Id scope)
	{
		if (node.kind != PA10NodeKind::Declarator &&
			node.kind != PA10NodeKind::AbstractDeclarator)
			throw std::runtime_error("invalid PA11 declarator node");
		std::size_t direct = node.children.size();
		for (std::size_t i = 0; i < node.children.size(); ++i)
		{
			if (node.children[i].kind == PA10NodeKind::Identifier ||
				node.children[i].kind == PA10NodeKind::NestedDeclarator)
			{
				direct = i;
				break;
			}
		}
		std::vector<DeclaratorOp> prefix;
		for (std::size_t i = 0; i < direct; ++i)
		{
			if (node.children[i].kind == PA10NodeKind::PtrOperator)
			{
				DeclaratorOp op = pointer_op(node.children[i]);
				std::size_t at = i + 1;
				while (at < direct && is_cv_node(node.children[at]))
				{
					op.cv |= cv_bit(node.children[at]);
					++at;
				}
				prefix.push_back(op);
				i = at - 1;
			}
			else if (node.children[i].kind != PA10NodeKind::ParameterPack &&
				node.children[i].kind != PA10NodeKind::CvQualifier)
				throw std::runtime_error("invalid PA11 declarator prefix");
		}
		std::vector<DeclaratorOp> suffix;
		if (direct < node.children.size())
		{
			for (std::size_t i = direct + 1; i < node.children.size(); ++i)
			{
				const PA10AstNode& child = node.children[i];
				if (child.kind == PA10NodeKind::ArraySuffix)
				{
					DeclaratorOp op(DeclaratorOp::Array);
					if (child.children.empty())
						op.unknown_bound = true;
					else if (child.children.size() == 1)
					{
						const ConstValue bound = eval_constexpr(
							child.children.front(), scope);
						if (!bound.valid || bound.value <= 0 ||
							bound.value > static_cast<__int128>(InvalidId))
							throw std::runtime_error("invalid PA11 array bound");
						op.bound = static_cast<Id>(bound.value);
					}
					else
						throw std::runtime_error("unsupported PA11 array bound expression");
					suffix.push_back(op);
				}
				else if (child.kind == PA10NodeKind::ParameterClause)
				{
					DeclaratorOp op(DeclaratorOp::Function);
					op.parameter_clause = &child;
					suffix.push_back(op);
				}
				else if (child.kind == PA10NodeKind::FunctionQualifier ||
					child.kind == PA10NodeKind::CvQualifier ||
					child.kind == PA10NodeKind::RefQualifier ||
					child.kind == PA10NodeKind::VirtSpecifier)
				{
					// These facts do not change the PA11 function type model.
				}
				else
					throw std::runtime_error("invalid PA11 declarator suffix");
			}
		}
		Id result = base;
		if (direct < node.children.size() &&
			node.children[direct].kind == PA10NodeKind::NestedDeclarator)
		{
			const Id with_suffix = apply_suffix(suffix, base, scope);
			result = apply_declarator(node.children[direct].children.front(),
				with_suffix, scope);
			result = apply_prefix(prefix, result);
		}
		else
		{
			result = apply_prefix(prefix, base);
			result = apply_suffix(suffix, result, scope);
		}
		return result;
	}

	const PA10AstNode* top_parameter_clause(const PA10AstNode& node) const
	{
		std::size_t direct = node.children.size();
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind == PA10NodeKind::Identifier ||
				node.children[i].kind == PA10NodeKind::NestedDeclarator)
			{
				direct = i;
				break;
			}
		if (direct == node.children.size() ||
			node.children[direct].kind != PA10NodeKind::Identifier)
			return NULL;
		for (std::size_t i = direct + 1; i < node.children.size(); ++i)
			if (node.children[i].kind == PA10NodeKind::ParameterClause)
				return &node.children[i];
		return NULL;
	}

	void process_simple_declaration(const PA10AstNode& node, Id scope)
	{
		if (node.children.empty())
			throw std::runtime_error("invalid PA11 simple declaration");
		const SpecFact spec = spec_fact(node.children.front(), scope);
		if (node.children.size() == 1)
		{
			if (spec.anonymous_record != InvalidId)
			{
				const Id type = named_type(spec.anonymous_record);
				if (named_[spec.anonymous_record].class_tag == ClassTag::Union)
					inject_anonymous_union(type, scope);
			}
			return;
		}
		const PA10AstNode& list = node.children[1];
		if (list.kind != PA10NodeKind::InitDeclaratorList)
			throw std::runtime_error("invalid PA11 declarator list");
		for (std::size_t i = 0; i < list.children.size(); ++i)
		{
			const PA10AstNode& init = list.children[i];
			if (init.kind != PA10NodeKind::InitDeclarator || init.children.empty())
				throw std::runtime_error("invalid PA11 init-declarator");
			const PA10AstNode& declarator = init.children.front();
			const DeclaratorName name = declarator_name(declarator);
			if (!name.found)
				throw std::runtime_error("unnamed PA11 declaration");
			const Id target = declaration_scope(name.path, scope);
			if (target == InvalidId)
				throw std::runtime_error("unresolved PA11 declaration scope");
			if (spec.anonymous_record != InvalidId)
				finalize_anonymous_record(spec.base, name.path.last(), target);
			Id type = apply_declarator(declarator, spec.base, target);
			if (spec.is_constexpr && type_kind(type) != TypeKind::Function)
				type = make_cv(type, 1u);
			if (spec.is_typedef)
				add_type_alias(target, name.path.last(), type);
			else
			{
				const bool function = type_kind(type) == TypeKind::Function;
				const Id binding_id = add_value(target, name.path.last(), type,
					function);
				if ((spec.cv & 1u) != 0 || spec.is_constexpr)
				{
					if (init.children.size() > 1)
					{
						const PA10AstNode& initializer = init.children[1];
						if (initializer.children.empty())
							throw std::runtime_error("empty constant initializer");
						const ConstValue value = eval_constexpr(
							initializer.children.front(), target);
						if (value.value < static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) ||
							value.value > static_cast<__int128>(std::numeric_limits<std::int64_t>::max()))
							throw std::runtime_error("constant initializer overflow");
						scopes_[target].bindings[binding_id].has_value = true;
						scopes_[target].bindings[binding_id].value =
							static_cast<std::int64_t>(value.value);
					}
				}
			}
		}
	}

	void process_function_definition(const PA10AstNode& node, Id scope)
	{
		if (node.children.size() != 3)
			throw std::runtime_error("invalid PA11 function definition");
		const PA10AstNode& declarator = node.children[1];
		const DeclaratorName name = declarator_name(declarator);
		if (!name.found)
			throw std::runtime_error("unnamed PA11 function definition");
		const Id target = declaration_scope(name.path, scope);
		if (target == InvalidId)
			throw std::runtime_error("unresolved PA11 function scope");
		const SpecFact spec = spec_fact(node.children[0], target);
		const Id type = apply_declarator(declarator, spec.base, target);
		if (type_kind(type) != TypeKind::Function)
			throw std::runtime_error("PA11 definition is not a function");
		add_value(target, name.path.last(), type, true);
		const Id function_scope = create_scope(ScopeKind::Function, target,
			name.path.last());
		const PA10AstNode* clause = top_parameter_clause(declarator);
		if (clause != NULL)
		{
			bool variadic = false;
			std::vector<ParamFact> facts;
			parameter_types(*clause, target, &variadic, &facts);
			(void)variadic;
			for (std::size_t i = 0; i < facts.size(); ++i)
			{
				Binding parameter(BindingKind::Parameter, facts[i].name,
					facts[i].type);
				scopes_[function_scope].bindings.push_back(parameter);
			}
		}
		process_compound_statement(node.children[2], function_scope);
	}

	void process_compound_statement(const PA10AstNode& node, Id parent)
	{
		if (node.kind != PA10NodeKind::CompoundStatement)
			throw std::runtime_error("invalid PA11 compound statement");
		const Id block = create_scope(ScopeKind::Block, parent, InvalidId);
		for (std::size_t i = 0; i < node.children.size(); ++i)
		{
			const PA10AstNode& child = node.children[i];
			switch (child.kind)
			{
			case PA10NodeKind::SimpleDeclaration:
				process_simple_declaration(child, block);
				break;
			case PA10NodeKind::CompoundStatement:
				process_compound_statement(child, block);
				break;
			case PA10NodeKind::EmptyDeclaration:
				break;
			default:
				// PA11's first semantic layer creates block scopes; expression
				// and statement semantics belong to later assignments.
				break;
			}
		}
	}

	void process_namespace(const PA10AstNode& node, Id parent)
	{
		Id namespace_id;
		if (node.producer_spelling == 0)
			unsupported("anonymous namespaces");
		const Id name = name_from_spelling(node.producer_spelling);
		namespace_id = named_namespace(parent, name);
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind == PA10NodeKind::InlineMarker)
				scopes_[namespace_id].inline_namespace = true;
		for (std::size_t i = 0; i < node.children.size(); ++i)
		{
			if (node.children[i].kind == PA10NodeKind::InlineMarker)
				continue;
			process_declaration(node.children[i], namespace_id);
		}
	}

	void process_namespace_alias(const PA10AstNode& node, Id scope)
	{
		if (node.producer_spelling == 0 || node.children.size() != 1)
			throw std::runtime_error("invalid PA11 namespace alias");
		const Id name = name_from_spelling(node.producer_spelling);
		const NamePath target_name = name_path(node.children.front());
		const Id target = resolve_namespace_path(target_name, scope);
		if (target == InvalidId)
			throw std::runtime_error("namespace alias target is not a namespace");
		Scope& current = scopes_[scope];
		if (current.namespaces.find(name) != current.namespaces.end() ||
			current.types.find(name) != current.types.end() ||
			direct_value_exists(scope, name))
			throw std::runtime_error("namespace alias conflicts with binding");
		std::unordered_map<Id, Id>::const_iterator old =
			current.namespace_aliases.find(name);
		if (old != current.namespace_aliases.end() && old->second != target)
			throw std::runtime_error("namespace alias redefinition");
		current.namespace_aliases[name] = target;
	}

	void process_using_directive(const PA10AstNode& node, Id scope)
	{
		if (node.children.size() != 1)
			throw std::runtime_error("invalid PA11 using directive");
		const Id target = resolve_namespace_path(name_path(node.children.front()), scope);
		if (target == InvalidId)
			throw std::runtime_error("using directive target is not a namespace");
		scopes_[scope].using_directives.push_back(target);
	}

	void process_using_declaration(const PA10AstNode& node, Id scope)
	{
		if (node.children.size() != 1)
			throw std::runtime_error("invalid PA11 using declaration");
		const NamePath target_name = name_path(node.children.front());
		const Id type = lookup_type_path(target_name, scope);
		const Id introduced = target_name.last();
		Scope& current = scopes_[scope];
		if (current.types.find(introduced) != current.types.end() ||
			direct_value_exists(scope, introduced) ||
			direct_namespace_exists(scope, introduced))
			throw std::runtime_error("using declaration conflicts with binding");
		if (type != InvalidId)
		{
			current.types[introduced] = type;
			current.using_types[introduced] = type;
			BindingKind kind = BindingKind::TypeAlias;
			if (target_name.components.size() > 1)
			{
				std::vector<Id> prefix(target_name.components.begin(),
					target_name.components.end() - 1);
				const Id owner = target_name.global ?
					resolve_global_qualifier_scope(prefix) :
					resolve_qualifier_scope(prefix, scope);
				if (owner != InvalidId)
				{
					const Scope& source = scopes_[owner];
					for (std::size_t i = 0; i < source.bindings.size(); ++i)
						if (source.bindings[i].name == introduced &&
							source.bindings[i].type == type &&
							source.bindings[i].kind == BindingKind::Type)
							kind = BindingKind::Type;
				}
			}
			current.bindings.push_back(Binding(kind, introduced, type));
			return;
		}
		const std::vector<ValueRef> values = lookup_value_path(target_name, scope);
		if (values.empty())
			throw std::runtime_error("using declaration target is not a binding");
		for (std::size_t i = 0; i < values.size(); ++i)
		{
			const Binding& source = scopes_[values[i].scope].bindings[values[i].binding];
			Binding imported = source;
			imported.name = introduced;
			const Id binding_id = current.bindings.size();
			current.bindings.push_back(imported);
			current.values[introduced].push_back(binding_id);
		}
	}

	Id template_parameter_name(const PA10AstNode& node)
	{
		for (std::size_t i = node.children.size(); i != 0; --i)
			if (node.children[i - 1].kind == PA10NodeKind::Identifier)
				return name_path(node.children[i - 1]).last();
		throw std::runtime_error("unnamed template parameter");
	}

	void process_template_parameter(const PA10AstNode& node, Id scope)
	{
		if (node.kind != PA10NodeKind::TypeParameter)
			throw std::runtime_error("unsupported template parameter");
		bool template_template = false;
		for (std::size_t i = 0; i < node.children.size(); ++i)
			if (node.children[i].kind == PA10NodeKind::TemplateTemplateParameter)
				template_template = true;
		const Id name = template_parameter_name(node);
		Scope& current = scopes_[scope];
		if (current.types.find(name) != current.types.end() ||
			direct_namespace_exists(scope, name) || direct_value_exists(scope, name))
			throw std::runtime_error("template parameter conflicts with binding");
		NamedRecord record(NamedKind::TemplateParameter, name, scope);
		record.template_template = template_template;
		const Id record_id = named_.size();
		named_.push_back(record);
		const Id type = named_type(record_id);
		current.types[name] = type;
		add_type_binding(scope, name, type, ClassTag::Struct, false);
		// The parameter list nested inside a template-template parameter is a
		// separate scope owned by the template argument grammar.  Its names are
		// deliberately not visible in the surrounding declaration.
	}

	void process_template_declaration(const PA10AstNode& node, Id parent)
	{
		if (node.children.size() != 2 ||
			node.children[0].kind != PA10NodeKind::TemplateParameterClause)
			throw std::runtime_error("invalid template declaration");
		const Id parameters = create_scope(ScopeKind::TemplateParameters, parent,
			InvalidId);
		const PA10AstNode& clause = node.children[0];
		for (std::size_t i = 0; i < clause.children.size(); ++i)
		{
			if (clause.children[i].kind != PA10NodeKind::TemplateParameterList)
				continue;
			for (std::size_t j = 0; j < clause.children[i].children.size(); ++j)
				process_template_parameter(clause.children[i].children[j], parameters);
		}
		process_declaration(node.children[1], parameters);
	}

	void process_declaration(const PA10AstNode& node, Id scope)
	{
		switch (node.kind)
		{
		case PA10NodeKind::EmptyDeclaration:
			return;
		case PA10NodeKind::NamespaceDefinition:
			process_namespace(node, scope);
			return;
		case PA10NodeKind::NamespaceAliasDefinition:
			process_namespace_alias(node, scope);
			return;
		case PA10NodeKind::UsingDirective:
			process_using_directive(node, scope);
			return;
		case PA10NodeKind::UsingDeclaration:
			process_using_declaration(node, scope);
			return;
		case PA10NodeKind::AliasDeclaration:
			if (node.producer_spelling == 0 || node.children.size() != 1)
				throw std::runtime_error("invalid PA11 alias declaration");
			add_type_alias(scope, name_from_spelling(node.producer_spelling),
				type_from_type_id(node.children.front(), scope));
			return;
		case PA10NodeKind::SimpleDeclaration:
			process_simple_declaration(node, scope);
			return;
		case PA10NodeKind::FunctionDefinition:
			process_function_definition(node, scope);
			return;
		case PA10NodeKind::ClassForwardDeclaration:
		{
			const NamePath name = class_name(node);
			if (name.empty())
				unsupported("anonymous class forward declaration");
			const ClassTag tag = class_tag(node);
			const Id target = declaration_scope(name, scope);
			if (target == InvalidId)
				throw std::runtime_error("unresolved class declaration scope");
			const Id type = ensure_named_class(target, name.last(), tag, false);
			add_type_binding(target, name.last(), type, tag, true);
			return;
		}
		case PA10NodeKind::ClassSpecifier:
		{
			const NamePath name = class_name(node);
			const ClassTag tag = class_tag(node);
			if (name.empty())
			{
				const Id type = create_anonymous_class(scope, tag, node);
				process_class_body(node, type, scope);
				if (tag == ClassTag::Union)
					inject_anonymous_union(type, scope);
				return;
			}
			const Id target = declaration_scope(name, scope);
			if (target == InvalidId)
				throw std::runtime_error("unresolved class declaration scope");
			const Id type = ensure_named_class(target, name.last(), tag, true);
			add_type_binding(target, name.last(), type, tag, true);
			process_class_body(node, type, target);
			return;
		}
		case PA10NodeKind::LinkageSpecification:
			for (std::size_t i = 0; i < node.children.size(); ++i)
				process_declaration(node.children[i], scope);
			return;
		case PA10NodeKind::TemplateDeclaration:
			process_template_declaration(node, scope);
			return;
		case PA10NodeKind::StaticAssertDeclaration:
			if (node.children.empty() || eval_constexpr(node.children.front(), scope).value == 0)
				throw std::runtime_error("static assertion failed");
			return;
		case PA10NodeKind::EnumSpecifier:
		{
			Id anonymous_record = InvalidId;
			process_enum_specifier(node, scope, &anonymous_record);
			if (anonymous_record != InvalidId)
			{
				if (named_[anonymous_record].scoped_enum)
					throw std::runtime_error("anonymous scoped enum needs a name");
			}
			return;
		}
		default:
			unsupported("declaration form");
		}
	}

	std::string render_named(Id type, ClassTag override_tag,
		bool use_override, Id display_name = InvalidId) const
	{
		const Id record_id = named_record_for_type(type);
		if (record_id == InvalidId || record_id >= named_.size())
			throw std::runtime_error("invalid PA11 named type");
		const NamedRecord& record = named_[record_id];
		std::ostringstream result;
		if (record.kind == NamedKind::Enum)
		{
			result << "enum";
			if (record.scoped_enum)
				result << " class";
		}
		else if (record.kind == NamedKind::TemplateParameter)
			result << (record.template_template ? "template-parameter" : "typename");
		else
		{
			const ClassTag tag = use_override ? override_tag : record.class_tag;
			result << (tag == ClassTag::Class ? "class" :
				tag == ClassTag::Union ? "union" : "struct");
		}
		result << ' ' << name_text(display_name == InvalidId ? record.name :
			display_name);
		return result.str();
	}

	std::string render_type(Id type) const
	{
		struct Task
		{
			bool text;
			Id type;
			const char* value;

			Task(bool text, Id type, const char* value)
				: text(text), type(type), value(value)
			{}
		};
		std::string result;
		std::vector<Task> tasks;
		tasks.push_back(Task(false, type, NULL));
		const std::size_t limit = types_.size() + 1;
		std::size_t steps = 0;
		while (!tasks.empty())
		{
			if (++steps > limit * 8 + 32)
				throw std::runtime_error("PA11 type rendering cycle");
			const Task task = tasks.back();
			tasks.pop_back();
			if (task.text)
			{
				result += task.value;
				continue;
			}
			if (task.type == InvalidId || task.type >= types_.size())
				throw std::runtime_error("invalid PA11 type for rendering");
			const TypeKey& key = types_[task.type];
			switch (key.kind)
			{
			case TypeKind::Fundamental:
				result += fundamental_type_name(key.fundamental);
				break;
			case TypeKind::Named:
				result += render_named(task.type, ClassTag::Struct, false);
				break;
			case TypeKind::Cv:
				if ((key.cv & 1u) != 0)
					result += "const ";
				if ((key.cv & 2u) != 0)
					result += "volatile ";
				tasks.push_back(Task(false, key.child, NULL));
				break;
			case TypeKind::Pointer:
				if ((key.cv & 1u) != 0)
					result += "const ";
				if ((key.cv & 2u) != 0)
					result += "volatile ";
				result += "pointer to ";
				tasks.push_back(Task(false, key.child, NULL));
				break;
			case TypeKind::LvalueReference:
				result += "lvalue-reference to ";
				tasks.push_back(Task(false, key.child, NULL));
				break;
			case TypeKind::RvalueReference:
				result += "rvalue-reference to ";
				tasks.push_back(Task(false, key.child, NULL));
				break;
			case TypeKind::Array:
				result += "array of ";
				if (key.unknown_bound)
					result += "unknown bound of ";
				else
				{
					std::ostringstream bound;
					bound << key.bound << ' ';
					result += bound.str();
				}
				tasks.push_back(Task(false, key.child, NULL));
				break;
			case TypeKind::Function:
				result += "function of (";
				tasks.push_back(Task(false, key.result, NULL));
				tasks.push_back(Task(true, InvalidId, ") returning "));
				if (key.variadic)
				{
					tasks.push_back(Task(true, InvalidId, "..."));
					if (!key.parameters.empty())
						tasks.push_back(Task(true, InvalidId, ", "));
				}
				for (std::size_t i = key.parameters.size(); i != 0; --i)
				{
					tasks.push_back(Task(false, key.parameters[i - 1], NULL));
					if (i > 1)
						tasks.push_back(Task(true, InvalidId, ", "));
				}
				break;
			}
		}
		return result;
	}

	std::string render_binding_type(const Binding& binding) const
	{
		if (binding.has_tag && type_kind(binding.type) == TypeKind::Named)
			return render_named(binding.type, binding.class_tag, true,
				binding.display_type_name);
		if (binding.display_type_name != InvalidId &&
			type_kind(binding.type) == TypeKind::Named)
			return render_named(binding.type, ClassTag::Struct, false,
				binding.display_type_name);
		return render_type(binding.type);
	}

	const char* binding_label(BindingKind kind) const
	{
		switch (kind)
		{
		case BindingKind::Type: return "type";
		case BindingKind::TypeAlias: return "type-alias";
		case BindingKind::Function: return "function";
		case BindingKind::Variable: return "variable";
		case BindingKind::Parameter: return "parameter";
		case BindingKind::Enumerator: return "enumerator";
		}
		return "binding";
	}

	void dump_scope(std::ostream& output, Id scope, std::size_t depth) const
	{
		if (scope == InvalidId || scope >= scopes_.size())
			throw std::runtime_error("invalid PA11 scope identity");
		const Scope& current = scopes_[scope];
		for (std::size_t i = 0; i < depth; ++i)
			output << "  ";
		switch (current.kind)
		{
		case ScopeKind::Namespace:
			output << "scope namespace " <<
				(current.name == InvalidId ? "<global>" : name_text(current.name));
			break;
		case ScopeKind::Class:
			output << "scope class " << name_text(current.name);
			break;
		case ScopeKind::Function:
			output << "scope function " << name_text(current.name);
			break;
		case ScopeKind::Block:
			output << "scope block";
			break;
		case ScopeKind::Enum:
			output << "scope enum " << name_text(current.name);
			break;
		case ScopeKind::TemplateParameters:
			output << "scope template-parameters";
			break;
		}
		output << '\n';
		for (std::size_t i = 0; i < current.bindings.size(); ++i)
		{
			const Binding& binding = current.bindings[i];
			const std::size_t tag_count = binding.kind == BindingKind::Type &&
				!binding.declaration_tags.empty() ?
				binding.declaration_tags.size() : 1;
			for (std::size_t tag_index = 0; tag_index < tag_count; ++tag_index)
			{
				for (std::size_t indent = 0; indent < depth + 1; ++indent)
					output << "  ";
				output << binding_label(binding.kind) << ' ';
				if (binding.name != InvalidId)
					output << name_text(binding.name);
				output << ' ';
				if (binding.kind == BindingKind::Type &&
					!binding.declaration_tags.empty())
					output << render_named(binding.type,
						binding.declaration_tags[tag_index], true,
						binding.display_type_name);
				else
					output << render_binding_type(binding);
				if (binding.kind == BindingKind::Enumerator && binding.has_value)
					output << ' ' << binding.value;
				output << '\n';
			}
		}
		for (int function_pass = 0; function_pass != 2; ++function_pass)
			for (std::size_t i = 0; i < current.children.size(); ++i)
			{
				const bool is_function = scopes_[current.children[i]].kind ==
					ScopeKind::Function;
				if ((function_pass == 0 && is_function) ||
					(function_pass == 1 && !is_function))
					continue;
				dump_scope(output, current.children[i], depth + 1);
			}
	}
};
} // namespace

void emit_pa11_types(const PA10Ast& ast, std::ostream& output)
{
	PA11SemanticModel model(ast);
	model.analyze();
	model.dump(output);
}
