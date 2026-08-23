#include "pa7_semantic.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "posttoken.h"

namespace
{

struct NameId
{
	std::size_t value;

	NameId(std::size_t value = std::numeric_limits<std::size_t>::max())
		: value(value)
	{}

	bool valid() const
	{
		return value != std::numeric_limits<std::size_t>::max();
	}

	bool operator<(const NameId& other) const
	{
		return value < other.value;
	}
};

struct TypeId
{
	std::size_t value;

	TypeId(std::size_t value = std::numeric_limits<std::size_t>::max())
		: value(value)
	{}

	bool valid() const
	{
		return value != std::numeric_limits<std::size_t>::max();
	}

	bool operator<(const TypeId& other) const
	{
		return value < other.value;
	}
};

struct EntityId
{
	std::size_t value;

	EntityId(std::size_t value = std::numeric_limits<std::size_t>::max())
		: value(value)
	{}

	bool valid() const
	{
		return value != std::numeric_limits<std::size_t>::max();
	}
};

struct NamespaceId
{
	std::size_t value;

	NamespaceId(std::size_t value = std::numeric_limits<std::size_t>::max())
		: value(value)
	{}

	bool valid() const
	{
		return value != std::numeric_limits<std::size_t>::max();
	}
};

enum class PA7TokenKind
{
	Fixed,
	Identifier,
	Literal,
	End
};

struct PA7Token
{
	PA7TokenKind kind;
	SimpleTokenType fixed;
	PPSpellingId spelling;
	LiteralData literal;

	PA7Token(PA7TokenKind kind = PA7TokenKind::End,
		SimpleTokenType fixed = SimpleTokenType::OP_SEMICOLON,
		PPSpellingId spelling = 0)
		: kind(kind), fixed(fixed), spelling(spelling), literal()
	{}
};

class PA7TokenCollector : public IPostTokenOutput
{
public:
	PA7TokenCollector() : tokens(), invalid(false) {}

	void emit_invalid(const std::string& source)
	{
		(void)source;
		invalid = true;
	}

	void emit_simple(const std::string& source, SimpleTokenType type)
	{
		(void)source;
		tokens.push_back(PA7Token(PA7TokenKind::Fixed, type));
	}

	void emit_simple_identifier(const std::string& source,
		SimpleTokenType type)
	{
		(void)source;
		tokens.push_back(PA7Token(PA7TokenKind::Fixed, type));
	}

	void emit_simple_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source, SimpleTokenType type)
	{
		(void)source;
		PA7Token token(PA7TokenKind::Fixed, type, spelling);
		tokens.push_back(token);
	}

	void emit_identifier(const std::string& source)
	{
		(void)source;
		// The typed posttoken path calls the spelling-aware overload below.
		// This fallback is retained for legacy direct callers only.
		tokens.push_back(PA7Token(PA7TokenKind::Identifier));
	}

	void emit_identifier_with_spelling(PPSpellingId spelling,
		const std::string& source)
	{
		(void)source;
		tokens.push_back(PA7Token(PA7TokenKind::Identifier,
			SimpleTokenType::OP_SEMICOLON, spelling));
	}

	void emit_literal(const std::string& source, const LiteralData& value)
	{
		(void)source;
		PA7Token token(PA7TokenKind::Literal);
		token.literal = value;
		tokens.push_back(token);
	}

	void emit_user_defined_literal(const UserDefinedLiteralData& value)
	{
		(void)value;
		invalid = true;
	}

	void emit_eof()
	{
		if (tokens.empty() || tokens.back().kind != PA7TokenKind::End)
			tokens.push_back(PA7Token(PA7TokenKind::End));
	}

	std::vector<PA7Token> tokens;
	bool invalid;
};

enum class TypeKind
{
	Fundamental,
	Cv,
	Pointer,
	LvalueReference,
	RvalueReference,
	Array,
	Function
};

struct TypeKey
{
	TypeKind kind;
	FundamentalType fundamental;
	TypeId child;
	unsigned int cv;
	bool unknown_bound;
	std::size_t bound;
	TypeId result;
	std::vector<TypeId> parameters;
	bool variadic;

	TypeKey()
		: kind(TypeKind::Fundamental), fundamental(FundamentalType::Int),
		  child(), cv(0), unknown_bound(false), bound(0), result(),
		  parameters(), variadic(false)
	{}

	bool operator<(const TypeKey& other) const
	{
		if (kind != other.kind)
			return static_cast<int>(kind) < static_cast<int>(other.kind);
		if (fundamental != other.fundamental)
			return static_cast<int>(fundamental) <
				static_cast<int>(other.fundamental);
		if (child.value != other.child.value)
			return child.value < other.child.value;
		if (cv != other.cv)
			return cv < other.cv;
		if (unknown_bound != other.unknown_bound)
			return unknown_bound < other.unknown_bound;
		if (bound != other.bound)
			return bound < other.bound;
		if (result.value != other.result.value)
			return result.value < other.result.value;
		if (variadic != other.variadic)
			return variadic < other.variadic;
		return parameters < other.parameters;
	}
};

struct TypeRecord
{
	TypeKey key;
};

enum class EntityKind
{
	Variable,
	Function,
	TypeAlias
};

struct EntityRecord
{
	EntityKind kind;
	NameId name;
	TypeId type;
	NamespaceId owner;
};

struct QualifiedName
{
	bool global;
	std::vector<NameId> components;

	QualifiedName() : global(false), components() {}

	bool empty() const
	{
		return components.empty();
	}

	NameId last() const
	{
		if (components.empty())
			return NameId();
		return components.back();
	}
};

struct UsingDeclaration
{
	NameId introduced;
	EntityId entity;
	TypeId type;
	bool is_type;

	UsingDeclaration(NameId introduced = NameId(), EntityId entity = EntityId(),
		TypeId type = TypeId(), bool is_type = false)
		: introduced(introduced), entity(entity), type(type), is_type(is_type)
	{}
};

struct NamespaceRecord
{
	NamespaceId id;
	NamespaceId parent;
	NameId name;
	bool anonymous;
	bool inline_namespace;
	NamespaceId anonymous_child;
	std::vector<EntityId> variables;
	std::vector<EntityId> functions;
	std::vector<NamespaceId> children;
	std::map<NameId, NamespaceId> named_children;
	std::map<NameId, NamespaceId> namespace_aliases;
	std::map<NameId, EntityId> entities;
	std::map<NameId, TypeId> aliases;
	std::map<NameId, EntityId> using_entities;
	std::map<NameId, TypeId> using_types;
	std::vector<NamespaceId> using_directives;

	NamespaceRecord(NamespaceId id = NamespaceId(),
		NamespaceId parent = NamespaceId(), NameId name = NameId(),
		bool anonymous = false, bool inline_namespace = false)
		: id(id), parent(parent), name(name), anonymous(anonymous),
		  inline_namespace(inline_namespace), anonymous_child(), variables(),
		  functions(),
		  children(), named_children(), namespace_aliases(), entities(),
		  aliases(), using_entities(), using_types(), using_directives()
	{}
};

struct BaseSpec
{
	bool is_typedef;
	bool has_named_type;
	QualifiedName named_type;
	TypeId resolved_type;
	unsigned int cv;
	bool has_char;
	bool has_short;
	bool has_int;
	unsigned int long_count;
	bool has_signed;
	bool has_unsigned;
	bool has_bool;
	bool has_wchar;
	bool has_char16;
	bool has_char32;
	bool has_float;
	bool has_double;
	bool has_void;

	BaseSpec()
		: is_typedef(false), has_named_type(false), named_type(),
		  resolved_type(), cv(0),
		  has_char(false), has_short(false), has_int(false), long_count(0),
		  has_signed(false), has_unsigned(false), has_bool(false),
		  has_wchar(false), has_char16(false), has_char32(false),
		  has_float(false), has_double(false), has_void(false)
	{}
};

enum class DeclaratorOpKind
{
	Pointer,
	LvalueReference,
	RvalueReference,
	Array,
	Function
};

struct DeclaratorOp
{
	DeclaratorOpKind kind;
	unsigned int cv;
	bool unknown_bound;
	std::size_t bound;
	std::vector<TypeId> parameters;
	bool variadic;

	DeclaratorOp(DeclaratorOpKind kind = DeclaratorOpKind::Pointer)
		: kind(kind), cv(0), unknown_bound(false), bound(0), parameters(),
		  variadic(false)
	{}
};

struct DeclaratorShape
{
	bool has_name;
	QualifiedName name;
	std::vector<DeclaratorOp> operations;

	DeclaratorShape() : has_name(false), name(), operations() {}
};

enum class LookupCategory
{
	Namespace,
	Type,
	Entity
};

struct LookupResult
{
	LookupCategory category;
	NamespaceId namespace_id;
	EntityId entity;
	TypeId type;

	LookupResult()
		: category(LookupCategory::Namespace), namespace_id(), entity(), type()
	{}

	static LookupResult namespace_result(NamespaceId value)
	{
		LookupResult result;
		result.category = LookupCategory::Namespace;
		result.namespace_id = value;
		return result;
	}

	static LookupResult entity_result(EntityId value)
	{
		LookupResult result;
		result.category = LookupCategory::Entity;
		result.entity = value;
		return result;
	}

	static LookupResult type_result(TypeId value)
	{
		LookupResult result;
		result.category = LookupCategory::Type;
		result.type = value;
		return result;
	}

	bool found() const
	{
		return (category == LookupCategory::Namespace && namespace_id.valid()) ||
			(category == LookupCategory::Entity && entity.valid()) ||
			(category == LookupCategory::Type && type.valid());
	}
};

} // namespace

struct PA7SemanticModel::Impl
{
	const PPTokenBuffer& input;
	std::vector<PA7Token> tokens;
	std::vector<PPSpellingId> names;
	std::map<PPSpellingId, NameId> names_by_spelling;
	std::vector<TypeRecord> types;
	std::map<TypeKey, TypeId> canonical_types;
	std::vector<NamespaceRecord> namespaces;
	std::vector<EntityRecord> entities;
	NamespaceId global;

	explicit Impl(const PPTokenBuffer& input)
		: input(input), tokens(), names(), names_by_spelling(), types(),
		  canonical_types(), namespaces(), entities(), global()
	{
		global = create_namespace(NamespaceId(), NameId(), true, false);
		for (int i = static_cast<int>(FundamentalType::SignedChar);
			i <= static_cast<int>(FundamentalType::NullptrT); ++i)
		{
			TypeKey key;
			key.kind = TypeKind::Fundamental;
			key.fundamental = static_cast<FundamentalType>(i);
			intern_type(key);
		}
	}

	NameId intern_name(PPSpellingId spelling)
	{
		std::map<PPSpellingId, NameId>::const_iterator found =
			names_by_spelling.find(spelling);
		if (found != names_by_spelling.end())
			return found->second;
		const NameId result(names.size());
		names.push_back(spelling);
		names_by_spelling[spelling] = result;
		return result;
	}

	const std::string& name_text(NameId name) const
	{
		if (!name.valid() || name.value >= names.size())
			throw std::runtime_error("invalid PA7 name identity");
		return input.spellings.get(names[name.value]);
	}

	TypeId intern_type(const TypeKey& key)
	{
		std::map<TypeKey, TypeId>::const_iterator found =
			canonical_types.find(key);
		if (found != canonical_types.end())
			return found->second;
		const TypeId result(types.size());
		TypeRecord record;
		record.key = key;
		types.push_back(record);
		canonical_types[key] = result;
		return result;
	}

	TypeId fundamental(FundamentalType type) const
	{
		TypeKey key;
		key.kind = TypeKind::Fundamental;
		key.fundamental = type;
		std::map<TypeKey, TypeId>::const_iterator found =
			canonical_types.find(key);
		if (found == canonical_types.end())
			throw std::runtime_error("missing fundamental type identity");
		return found->second;
	}

	TypeKind type_kind(TypeId type) const
	{
		if (!type.valid() || type.value >= types.size())
			throw std::runtime_error("invalid PA7 type identity");
		return types[type.value].key.kind;
	}

	TypeId cv(TypeId child, unsigned int qualifiers)
	{
		if (qualifiers == 0)
			return child;
		if (type_kind(child) == TypeKind::LvalueReference ||
			type_kind(child) == TypeKind::RvalueReference)
			return child;
		if (type_kind(child) == TypeKind::Cv)
		{
			const TypeKey& old = types[child.value].key;
			qualifiers |= old.cv;
			child = old.child;
		}
		if (type_kind(child) == TypeKind::Array)
		{
			const TypeKey& array_type = types[child.value].key;
			return array(cv(array_type.child, qualifiers),
				array_type.unknown_bound, array_type.bound);
		}
		TypeKey key;
		key.kind = TypeKind::Cv;
		key.child = child;
		key.cv = qualifiers;
		return intern_type(key);
	}

	TypeId pointer(TypeId child, unsigned int qualifiers = 0)
	{
		TypeKey key;
		key.kind = TypeKind::Pointer;
		key.child = child;
		key.cv = qualifiers;
		return intern_type(key);
	}

	TypeId remove_top_cv(TypeId type)
	{
		if (type_kind(type) == TypeKind::Cv)
			return types[type.value].key.child;
		if (type_kind(type) == TypeKind::Pointer &&
			types[type.value].key.cv != 0)
			return pointer(types[type.value].key.child, 0);
		return type;
	}

	TypeId reference(TypeId child, bool rvalue)
	{
		if (type_kind(child) == TypeKind::LvalueReference)
			return child;
		if (type_kind(child) == TypeKind::RvalueReference)
		{
			const TypeId referred = types[child.value].key.child;
			if (!rvalue)
			{
				TypeKey key;
				key.kind = TypeKind::LvalueReference;
				key.child = referred;
				return intern_type(key);
			}
			child = referred;
		}
		TypeKey key;
		key.kind = rvalue ? TypeKind::RvalueReference :
			TypeKind::LvalueReference;
		key.child = child;
		return intern_type(key);
	}

	TypeId array(TypeId child, bool unknown_bound, std::size_t bound)
	{
		TypeKey key;
		key.kind = TypeKind::Array;
		key.child = child;
		key.unknown_bound = unknown_bound;
		key.bound = bound;
		return intern_type(key);
	}

	TypeId function(const std::vector<TypeId>& parameters, bool variadic,
		TypeId result)
	{
		TypeKey key;
		key.kind = TypeKind::Function;
		key.parameters = parameters;
		key.variadic = variadic;
		key.result = result;
		return intern_type(key);
	}

	NamespaceId create_namespace(NamespaceId parent, NameId name,
		bool anonymous, bool inline_namespace)
	{
		const NamespaceId id(namespaces.size());
		namespaces.push_back(NamespaceRecord(id, parent, name, anonymous,
			inline_namespace));
		if (id.value != 0)
			namespaces[parent.value].children.push_back(id);
		return id;
	}

	NamespaceId named_namespace(NamespaceId parent, NameId name,
		bool inline_namespace)
	{
		NamespaceRecord& scope = namespaces[parent.value];
		std::map<NameId, NamespaceId>::const_iterator found =
			scope.named_children.find(name);
		if (found != scope.named_children.end())
		{
			NamespaceRecord& existing = namespaces[found->second.value];
			if (inline_namespace != existing.inline_namespace)
				throw std::runtime_error("inline namespace reopening mismatch");
			return found->second;
		}
		const NamespaceId result = create_namespace(parent, name, false,
			inline_namespace);
		namespaces[parent.value].named_children[name] = result;
		return result;
	}

	NamespaceId anonymous_namespace(NamespaceId parent,
		bool inline_namespace)
	{
		NamespaceRecord& scope = namespaces[parent.value];
		if (scope.anonymous_child.valid())
		{
			NamespaceRecord& existing =
				namespaces[scope.anonymous_child.value];
			if (inline_namespace != existing.inline_namespace)
				throw std::runtime_error("inline namespace reopening mismatch");
			return scope.anonymous_child;
		}
		const NamespaceId result = create_namespace(parent, NameId(), true,
			inline_namespace);
		namespaces[parent.value].anonymous_child = result;
		return result;
	}

	LookupResult lookup_in_namespace(NamespaceId scope, NameId name,
		LookupCategory category, std::vector<unsigned char>* visited) const
	{
		if (!scope.valid() || scope.value >= namespaces.size())
			return LookupResult();
		if (scope.value >= visited->size())
			visited->resize(namespaces.size(), 0);
		if ((*visited)[scope.value] != 0)
			return LookupResult();
		(*visited)[scope.value] = 1;
		const NamespaceRecord& record = namespaces[scope.value];
		if (category == LookupCategory::Namespace)
		{
			std::map<NameId, NamespaceId>::const_iterator direct =
				record.named_children.find(name);
			if (direct != record.named_children.end())
				return LookupResult::namespace_result(direct->second);
			std::map<NameId, NamespaceId>::const_iterator alias =
				record.namespace_aliases.find(name);
			if (alias != record.namespace_aliases.end())
				return LookupResult::namespace_result(alias->second);
		}
		else if (category == LookupCategory::Type)
		{
			std::map<NameId, TypeId>::const_iterator direct =
				record.aliases.find(name);
			if (direct != record.aliases.end())
				return LookupResult::type_result(direct->second);
			std::map<NameId, TypeId>::const_iterator imported =
				record.using_types.find(name);
			if (imported != record.using_types.end())
				return LookupResult::type_result(imported->second);
		}
		else
		{
			std::map<NameId, EntityId>::const_iterator direct =
				record.entities.find(name);
			if (direct != record.entities.end())
				return LookupResult::entity_result(direct->second);
			std::map<NameId, EntityId>::const_iterator imported =
				record.using_entities.find(name);
			if (imported != record.using_entities.end())
				return LookupResult::entity_result(imported->second);
		}

		// Anonymous and inline namespaces inject their declarations into the
		// enclosing namespace.  Children remain in first-declaration order;
		// this loop is also the deterministic tie-break for valid inputs.
		for (std::size_t i = 0; i < record.children.size(); ++i)
		{
			const NamespaceRecord& child = namespaces[record.children[i].value];
			if (!child.anonymous && !child.inline_namespace)
				continue;
			LookupResult injected = lookup_in_namespace(child.id, name,
				category, visited);
			if (injected.found())
				return injected;
		}
		for (std::size_t i = 0; i < record.using_directives.size(); ++i)
		{
			LookupResult imported = lookup_in_namespace(
				record.using_directives[i], name, category, visited);
			if (imported.found())
				return imported;
		}
		return LookupResult();
	}

	LookupResult lookup_unqualified(NamespaceId start, NameId name,
		LookupCategory category) const
	{
		NamespaceId scope = start;
		while (scope.valid())
		{
			std::vector<unsigned char> visited(namespaces.size(), 0);
			LookupResult found = lookup_in_namespace(scope, name, category,
				&visited);
			if (found.found())
				return found;
			if (scope.value == global.value)
				break;
			scope = namespaces[scope.value].parent;
		}
		return LookupResult();
	}

	LookupResult lookup_qualified(NamespaceId scope, NameId name,
		LookupCategory category) const
	{
		std::vector<unsigned char> visited(namespaces.size(), 0);
		return lookup_in_namespace(scope, name, category, &visited);
	}

	NamespaceId resolve_namespace_path(const QualifiedName& path,
		NamespaceId start) const
	{
		if (path.components.empty())
			throw std::runtime_error("empty PA7 namespace path");
		std::size_t at = 0;
		NamespaceId scope = path.global ? global : NamespaceId();
		if (path.global)
		{
			// The global qualifier selects the global namespace before the
			// first component; it is not itself a text name.
			scope = global;
		}
		else
		{
			LookupResult first = lookup_unqualified(start,
				path.components[0], LookupCategory::Namespace);
			if (!first.found())
				throw std::runtime_error("unresolved PA7 namespace name");
			scope = first.namespace_id;
			at = 1;
		}
		for (; at < path.components.size(); ++at)
		{
			LookupResult next = lookup_qualified(scope, path.components[at],
				LookupCategory::Namespace);
			if (!next.found())
				throw std::runtime_error("unresolved PA7 qualified namespace");
			scope = next.namespace_id;
		}
		return scope;
	}

	TypeId lookup_type_path(const QualifiedName& path, NamespaceId start) const
	{
		LookupResult result = lookup_path(path, start, LookupCategory::Type);
		if (!result.found())
			throw std::runtime_error("unresolved PA7 typedef name");
		return result.type;
	}

	LookupResult lookup_entity_path(const QualifiedName& path,
		NamespaceId start) const
	{
		return lookup_path(path, start, LookupCategory::Entity);
	}

	LookupResult lookup_path(const QualifiedName& path, NamespaceId start,
		LookupCategory category) const
	{
		if (path.components.empty())
			return LookupResult();
		if (path.components.size() == 1 && !path.global)
			return lookup_unqualified(start, path.components[0], category);
		if (path.components.size() == 1 && path.global)
			return lookup_qualified(global, path.components[0], category);
		QualifiedName prefix = path;
		const NameId last = prefix.components.back();
		prefix.components.pop_back();
		NamespaceId scope = resolve_namespace_path(prefix, start);
		return lookup_qualified(scope, last, category);
	}

	NamespaceId resolve_declaration_target(const QualifiedName& path,
		NamespaceId start) const
	{
		if (path.components.size() <= 1 && !path.global)
			return start;
		QualifiedName prefix = path;
		prefix.components.pop_back();
		if (prefix.components.empty() && prefix.global)
			return global;
		return resolve_namespace_path(prefix, start);
	}

	void declare_alias(NamespaceId scope, NameId name, TypeId type)
	{
		NamespaceRecord& record = namespaces[scope.value];
		if (record.entities.find(name) != record.entities.end())
			throw std::runtime_error("typedef conflicts with value declaration");
		record.aliases[name] = type;
	}

	void declare_namespace_alias(NamespaceId scope, NameId name,
		NamespaceId target)
	{
		namespaces[scope.value].namespace_aliases[name] = target;
	}

	void add_using_directive(NamespaceId scope, NamespaceId target)
	{
		std::vector<NamespaceId>& directives =
			namespaces[scope.value].using_directives;
		if (std::find_if(directives.begin(), directives.end(),
			[target](NamespaceId value) { return value.value == target.value; }) ==
			directives.end())
			directives.push_back(target);
	}

	void add_using_type(NamespaceId scope, NameId name, TypeId type)
	{
		namespaces[scope.value].using_types[name] = type;
	}

	void add_using_entity(NamespaceId scope, NameId name, EntityId entity)
	{
		namespaces[scope.value].using_entities[name] = entity;
	}

	TypeId merge_types(TypeId old_type, TypeId new_type)
	{
		if (old_type.value == new_type.value)
			return old_type;
		const TypeKey& old_key = types[old_type.value].key;
		const TypeKey& new_key = types[new_type.value].key;
		if (old_key.kind != TypeKind::Array ||
			new_key.kind != TypeKind::Array)
			return new_type;
		const TypeId child = merge_types(old_key.child, new_key.child);
		if (old_key.unknown_bound && !new_key.unknown_bound)
			return array(child, false, new_key.bound);
		if (!old_key.unknown_bound && new_key.unknown_bound)
			return array(child, false, old_key.bound);
		if (old_key.unknown_bound && new_key.unknown_bound)
			return array(child, true, 0);
		if (old_key.bound == new_key.bound)
			return array(child, false, old_key.bound);
		return new_type;
	}

	void update_entity(EntityId id, TypeId type)
	{
		entities[id.value].type = merge_types(entities[id.value].type, type);
	}

	void declare_value(NamespaceId scope, NameId name, TypeId type,
		bool is_function)
	{
		NamespaceRecord& record = namespaces[scope.value];
		const EntityKind kind = is_function ? EntityKind::Function :
			EntityKind::Variable;
		std::map<NameId, EntityId>::iterator found = record.entities.find(name);
		if (found != record.entities.end())
		{
			EntityRecord& existing = entities[found->second.value];
			if (existing.kind != kind)
				throw std::runtime_error("declaration kind conflict");
			existing.type = merge_types(existing.type, type);
			return;
		}
		const EntityId id(entities.size());
		EntityRecord entity;
		entity.kind = kind;
		entity.name = name;
		entity.type = type;
		entity.owner = scope;
		entities.push_back(entity);
		record.entities[name] = id;
		if (is_function)
			record.functions.push_back(id);
		else
			record.variables.push_back(id);
	}

	static std::uint64_t literal_value(const LiteralData& literal)
	{
		if (literal.bytes.size() > sizeof(std::uint64_t))
			throw std::runtime_error("array bound literal is too wide");
		std::uint64_t result = 0;
		for (std::size_t i = 0; i < literal.bytes.size(); ++i)
			result |= static_cast<std::uint64_t>(literal.bytes[i]) << (i * 8);
		return result;
	}

	std::string render_type(TypeId type, std::size_t depth = 0) const
	{
		if (depth > 4096)
			throw std::runtime_error("type rendering nesting limit reached");
		const TypeKey& key = types[type.value].key;
		if (key.kind == TypeKind::Fundamental)
			return fundamental_type_name(key.fundamental);
		if (key.kind == TypeKind::Cv)
		{
			std::string result;
			if ((key.cv & 1u) != 0)
				result += "const ";
			if ((key.cv & 2u) != 0)
				result += "volatile ";
			return result + render_type(key.child, depth + 1);
		}
		if (key.kind == TypeKind::Pointer)
		{
			std::string result;
			if ((key.cv & 1u) != 0)
				result += "const ";
			if ((key.cv & 2u) != 0)
				result += "volatile ";
			return result + "pointer to " + render_type(key.child,
				depth + 1);
		}
		if (key.kind == TypeKind::LvalueReference)
			return "lvalue-reference to " + render_type(key.child, depth + 1);
		if (key.kind == TypeKind::RvalueReference)
			return "rvalue-reference to " + render_type(key.child, depth + 1);
		if (key.kind == TypeKind::Array)
		{
			std::ostringstream bound;
			if (key.unknown_bound)
				return "array of unknown bound of " +
					render_type(key.child, depth + 1);
			else
				bound << key.bound;
			return "array of " + bound.str() + " " +
				render_type(key.child, depth + 1);
		}
		std::string result = "function of (";
		for (std::size_t i = 0; i < key.parameters.size(); ++i)
		{
			if (i != 0)
				result += ", ";
			result += render_type(key.parameters[i], depth + 1);
		}
		if (key.variadic)
		{
			if (!key.parameters.empty())
				result += ", ";
			result += "...";
		}
		return result + ") returning " + render_type(key.result,
			depth + 1);
	}

	void render_namespace(std::ostream& output, NamespaceId id,
		std::size_t depth = 0) const
	{
		if (depth > 4096)
			throw std::runtime_error("namespace rendering nesting limit reached");
		const NamespaceRecord& record = namespaces[id.value];
		if (record.anonymous)
			output << "start unnamed namespace\n";
		else
			output << "start namespace " << name_text(record.name) << "\n";
		if (record.inline_namespace)
			output << "inline namespace\n";
		for (std::size_t i = 0; i < record.variables.size(); ++i)
		{
			const EntityRecord& entity = entities[record.variables[i].value];
			output << "variable " << name_text(entity.name) << " " <<
				render_type(entity.type) << "\n";
		}
		for (std::size_t i = 0; i < record.functions.size(); ++i)
		{
			const EntityRecord& entity = entities[record.functions[i].value];
			output << "function " << name_text(entity.name) << " " <<
				render_type(entity.type) << "\n";
		}
		for (std::size_t i = 0; i < record.children.size(); ++i)
			render_namespace(output, record.children[i], depth + 1);
		output << "end namespace\n";
	}
};

class PA7Parser
{
public:
	PA7Parser(PA7SemanticModel::Impl& model,
		const std::vector<PA7Token>& tokens)
		: model_(model), tokens_(tokens), position_(0), current_(model.global),
		  work_(0), work_limit_(tokens.size() >
			std::numeric_limits<std::size_t>::max() / 64 ?
			std::numeric_limits<std::size_t>::max() :
			tokens.size() * 64 + 1024), nesting_(0)
	{}

	void parse()
	{
		while (!at_end())
			parse_declaration(current_);
		if (!at_end())
			throw std::runtime_error("PA7 parser did not consume translation unit");
	}

private:
	PA7SemanticModel::Impl& model_;
	const std::vector<PA7Token>& tokens_;
	std::size_t position_;
	NamespaceId current_;
	std::size_t work_;
	std::size_t work_limit_;
	std::size_t nesting_;

	static const std::size_t kMaxNesting = 4096;

	void tick()
	{
		if (++work_ > work_limit_)
			throw std::runtime_error("PA7 parser work limit reached");
	}

	bool at_end() const
	{
		return position_ >= tokens_.size() ||
			tokens_[position_].kind == PA7TokenKind::End;
	}

	const PA7Token& look(std::size_t offset = 0) const
	{
		const std::size_t at = position_ + offset;
		if (at >= tokens_.size())
			throw std::runtime_error("PA7 parser read past end");
		return tokens_[at];
	}

	bool fixed(SimpleTokenType type, std::size_t offset = 0) const
	{
		return look(offset).kind == PA7TokenKind::Fixed &&
			look(offset).fixed == type;
	}

	bool identifier(std::size_t offset = 0) const
	{
		return look(offset).kind == PA7TokenKind::Identifier;
	}

	bool literal(std::size_t offset = 0) const
	{
		return look(offset).kind == PA7TokenKind::Literal;
	}

	void consume_fixed(SimpleTokenType type)
	{
		tick();
		if (!fixed(type))
			throw std::runtime_error("unexpected PA7 fixed token");
		++position_;
	}

	NameId consume_identifier()
	{
		tick();
		if (!identifier())
			throw std::runtime_error("expected PA7 identifier");
		const NameId result = model_.intern_name(look().spelling);
		++position_;
		return result;
	}

	void enter_nesting()
	{
		if (++nesting_ > kMaxNesting)
			throw std::runtime_error("PA7 nesting limit reached");
	}

	void leave_nesting()
	{
		if (nesting_ == 0)
			throw std::runtime_error("PA7 nesting underflow");
		--nesting_;
	}

	void parse_declaration(NamespaceId scope)
	{
		current_ = scope;
		if (fixed(SimpleTokenType::OP_SEMICOLON))
		{
			consume_fixed(SimpleTokenType::OP_SEMICOLON);
			return;
		}
		if (fixed(SimpleTokenType::KW_NAMESPACE))
		{
			parse_namespace_definition(scope, false);
			return;
		}
		if (fixed(SimpleTokenType::KW_INLINE))
		{
			consume_fixed(SimpleTokenType::KW_INLINE);
			parse_namespace_definition(scope, true);
			return;
		}
		if (fixed(SimpleTokenType::KW_USING))
		{
			parse_using_declaration_or_alias(scope);
			return;
		}
		parse_simple_declaration(scope);
	}

	void parse_namespace_definition(NamespaceId parent, bool inline_namespace)
	{
		consume_fixed(SimpleTokenType::KW_NAMESPACE);
		NameId name;
		bool anonymous = true;
		if (identifier())
		{
			name = consume_identifier();
			anonymous = false;
		}
		if (fixed(SimpleTokenType::OP_ASS))
		{
			consume_fixed(SimpleTokenType::OP_ASS);
			QualifiedName target = parse_qualified_name();
			consume_fixed(SimpleTokenType::OP_SEMICOLON);
			if (anonymous)
				throw std::runtime_error("unnamed namespace alias");
			model_.declare_namespace_alias(parent, name,
				model_.resolve_namespace_path(target, parent));
			return;
		}
		consume_fixed(SimpleTokenType::OP_LBRACE);
		NamespaceId child;
		if (anonymous)
			child = model_.anonymous_namespace(parent, inline_namespace);
		else
			child = model_.named_namespace(parent, name, inline_namespace);
		enter_nesting();
		while (!fixed(SimpleTokenType::OP_RBRACE))
		{
			if (at_end())
				throw std::runtime_error("unterminated PA7 namespace");
			parse_declaration(child);
		}
		consume_fixed(SimpleTokenType::OP_RBRACE);
		leave_nesting();
		current_ = parent;
	}

	QualifiedName parse_qualified_name()
	{
		QualifiedName result;
		if (fixed(SimpleTokenType::OP_COLON2))
		{
			result.global = true;
			consume_fixed(SimpleTokenType::OP_COLON2);
		}
		if (!identifier())
			throw std::runtime_error("expected PA7 qualified-name component");
		result.components.push_back(consume_identifier());
		while (fixed(SimpleTokenType::OP_COLON2))
		{
			consume_fixed(SimpleTokenType::OP_COLON2);
			if (!identifier())
				throw std::runtime_error("missing PA7 qualified-name component");
			result.components.push_back(consume_identifier());
		}
		return result;
	}

	void parse_using_declaration_or_alias(NamespaceId scope)
	{
		consume_fixed(SimpleTokenType::KW_USING);
		if (fixed(SimpleTokenType::KW_NAMESPACE))
		{
			consume_fixed(SimpleTokenType::KW_NAMESPACE);
			QualifiedName target = parse_qualified_name();
			consume_fixed(SimpleTokenType::OP_SEMICOLON);
			model_.add_using_directive(scope,
				model_.resolve_namespace_path(target, scope));
			return;
		}
		QualifiedName introduced = parse_qualified_name();
		if (fixed(SimpleTokenType::OP_ASS))
		{
			if (introduced.global || introduced.components.size() != 1)
				throw std::runtime_error("qualified PA7 alias declaration");
			consume_fixed(SimpleTokenType::OP_ASS);
			TypeId type = parse_type_id(scope);
			consume_fixed(SimpleTokenType::OP_SEMICOLON);
			model_.declare_alias(scope, introduced.last(), type);
			return;
		}
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
		if (introduced.components.empty())
			throw std::runtime_error("empty PA7 using declaration");
		LookupResult type = model_.lookup_path(introduced, scope,
			LookupCategory::Type);
		if (type.found())
		{
			model_.add_using_type(scope, introduced.last(), type.type);
			return;
		}
		LookupResult entity = model_.lookup_path(introduced, scope,
			LookupCategory::Entity);
		if (!entity.found())
			throw std::runtime_error("unresolved PA7 using declaration");
		model_.add_using_entity(scope, introduced.last(), entity.entity);
	}

	bool is_cv(SimpleTokenType type) const
	{
		return type == SimpleTokenType::KW_CONST ||
			type == SimpleTokenType::KW_VOLATILE;
	}

	bool consume_decl_specifier(BaseSpec* spec)
	{
		if (!fixed(SimpleTokenType::KW_TYPEDEF) &&
			!fixed(SimpleTokenType::KW_CONST) &&
			!fixed(SimpleTokenType::KW_VOLATILE) &&
			!fixed(SimpleTokenType::KW_CHAR) &&
			!fixed(SimpleTokenType::KW_CHAR16_T) &&
			!fixed(SimpleTokenType::KW_CHAR32_T) &&
			!fixed(SimpleTokenType::KW_WCHAR_T) &&
			!fixed(SimpleTokenType::KW_BOOL) &&
			!fixed(SimpleTokenType::KW_SHORT) &&
			!fixed(SimpleTokenType::KW_INT) &&
			!fixed(SimpleTokenType::KW_LONG) &&
			!fixed(SimpleTokenType::KW_SIGNED) &&
			!fixed(SimpleTokenType::KW_UNSIGNED) &&
			!fixed(SimpleTokenType::KW_FLOAT) &&
			!fixed(SimpleTokenType::KW_DOUBLE) &&
			!fixed(SimpleTokenType::KW_VOID) &&
			!fixed(SimpleTokenType::KW_STATIC) &&
			!fixed(SimpleTokenType::KW_THREAD_LOCAL) &&
			!fixed(SimpleTokenType::KW_EXTERN))
			return false;
		if (fixed(SimpleTokenType::KW_TYPEDEF))
		{
			spec->is_typedef = true;
			consume_fixed(SimpleTokenType::KW_TYPEDEF);
		}
		else if (fixed(SimpleTokenType::KW_CONST))
		{
			spec->cv |= 1u;
			consume_fixed(SimpleTokenType::KW_CONST);
		}
		else if (fixed(SimpleTokenType::KW_VOLATILE))
		{
			spec->cv |= 2u;
			consume_fixed(SimpleTokenType::KW_VOLATILE);
		}
		else if (fixed(SimpleTokenType::KW_CHAR))
		{
			spec->has_char = true;
			consume_fixed(SimpleTokenType::KW_CHAR);
		}
		else if (fixed(SimpleTokenType::KW_CHAR16_T))
		{
			spec->has_char16 = true;
			consume_fixed(SimpleTokenType::KW_CHAR16_T);
		}
		else if (fixed(SimpleTokenType::KW_CHAR32_T))
		{
			spec->has_char32 = true;
			consume_fixed(SimpleTokenType::KW_CHAR32_T);
		}
		else if (fixed(SimpleTokenType::KW_WCHAR_T))
		{
			spec->has_wchar = true;
			consume_fixed(SimpleTokenType::KW_WCHAR_T);
		}
		else if (fixed(SimpleTokenType::KW_BOOL))
		{
			spec->has_bool = true;
			consume_fixed(SimpleTokenType::KW_BOOL);
		}
		else if (fixed(SimpleTokenType::KW_SHORT))
		{
			spec->has_short = true;
			consume_fixed(SimpleTokenType::KW_SHORT);
		}
		else if (fixed(SimpleTokenType::KW_INT))
		{
			spec->has_int = true;
			consume_fixed(SimpleTokenType::KW_INT);
		}
		else if (fixed(SimpleTokenType::KW_LONG))
		{
			++spec->long_count;
			consume_fixed(SimpleTokenType::KW_LONG);
		}
		else if (fixed(SimpleTokenType::KW_SIGNED))
		{
			spec->has_signed = true;
			consume_fixed(SimpleTokenType::KW_SIGNED);
		}
		else if (fixed(SimpleTokenType::KW_UNSIGNED))
		{
			spec->has_unsigned = true;
			consume_fixed(SimpleTokenType::KW_UNSIGNED);
		}
		else if (fixed(SimpleTokenType::KW_FLOAT))
		{
			spec->has_float = true;
			consume_fixed(SimpleTokenType::KW_FLOAT);
		}
		else if (fixed(SimpleTokenType::KW_DOUBLE))
		{
			spec->has_double = true;
			consume_fixed(SimpleTokenType::KW_DOUBLE);
		}
		else if (fixed(SimpleTokenType::KW_VOID))
		{
			spec->has_void = true;
			consume_fixed(SimpleTokenType::KW_VOID);
		}
		else
		{
			consume_fixed(fixed(SimpleTokenType::KW_STATIC) ?
				SimpleTokenType::KW_STATIC :
				fixed(SimpleTokenType::KW_THREAD_LOCAL) ?
				SimpleTokenType::KW_THREAD_LOCAL : SimpleTokenType::KW_EXTERN);
		}
		return true;
	}

	BaseSpec parse_decl_specifiers(NamespaceId scope)
	{
		BaseSpec result;
		bool consumed = false;
		while (true)
		{
			if (identifier() || fixed(SimpleTokenType::OP_COLON2))
			{
				if (result.has_named_type || result.has_char ||
					result.has_short || result.has_int || result.long_count != 0 ||
					result.has_signed || result.has_unsigned || result.has_bool ||
					result.has_wchar || result.has_char16 || result.has_char32 ||
					result.has_float || result.has_double || result.has_void)
					break;
				result.has_named_type = true;
				result.named_type = parse_qualified_name();
				consumed = true;
				continue;
			}
			if (!consume_decl_specifier(&result))
				break;
			consumed = true;
		}
		if (!consumed)
			throw std::runtime_error("missing PA7 declaration specifier");

		TypeId base;
		if (result.has_named_type)
		{
			if (result.has_char || result.has_short || result.has_int ||
				result.long_count != 0 || result.has_signed ||
				result.has_unsigned || result.has_bool || result.has_wchar ||
				result.has_char16 || result.has_char32 || result.has_float ||
				result.has_double || result.has_void)
				throw std::runtime_error("mixed PA7 type specifiers");
			base = model_.lookup_type_path(result.named_type, scope);
			base = model_.cv(base, result.cv);
		}
		else
			base = fundamental_from_spec(result);
		result.resolved_type = base;
		return result;
	}

	TypeId fundamental_from_spec(const BaseSpec& spec)
	{
		FundamentalType type;
		if (spec.has_char)
		{
			if (spec.has_signed)
				type = FundamentalType::SignedChar;
			else if (spec.has_unsigned)
				type = FundamentalType::UnsignedChar;
			else
				type = FundamentalType::Char;
		}
		else if (spec.has_char16)
			type = FundamentalType::Char16T;
		else if (spec.has_char32)
			type = FundamentalType::Char32T;
		else if (spec.has_wchar)
			type = FundamentalType::WcharT;
		else if (spec.has_bool)
			type = FundamentalType::Bool;
		else if (spec.has_float)
			type = FundamentalType::Float;
		else if (spec.has_double)
			type = spec.long_count == 0 ? FundamentalType::Double :
				FundamentalType::LongDouble;
		else if (spec.has_void)
			type = FundamentalType::Void;
		else if (spec.long_count >= 2)
			type = spec.has_unsigned ? FundamentalType::UnsignedLongLongInt :
				spec.has_signed ? FundamentalType::LongLongInt :
				FundamentalType::LongLongInt;
		else if (spec.long_count == 1)
			type = spec.has_unsigned ? FundamentalType::UnsignedLongInt :
				FundamentalType::LongInt;
		else if (spec.has_short)
			type = spec.has_unsigned ? FundamentalType::UnsignedShortInt :
				FundamentalType::ShortInt;
		else if (spec.has_unsigned)
			type = FundamentalType::UnsignedInt;
		else if (spec.has_signed || spec.has_int)
			type = FundamentalType::Int;
		else
			type = FundamentalType::Int;
		TypeId result = model_.fundamental(type);
		return model_.cv(result, spec.cv);
	}

	std::size_t parse_array_bound()
	{
		if (!literal())
			throw std::runtime_error("PA7 array bound is not a literal");
		const std::uint64_t value = model_.literal_value(look().literal);
		if (value == 0 || value > std::numeric_limits<std::size_t>::max())
			throw std::runtime_error("invalid PA7 array bound");
		++position_;
		return static_cast<std::size_t>(value);
	}

	std::vector<TypeId> parse_parameter_clause(bool* variadic)
	{
		consume_fixed(SimpleTokenType::OP_LPAREN);
		std::vector<TypeId> parameters;
		*variadic = false;
		if (fixed(SimpleTokenType::OP_RPAREN))
		{
			consume_fixed(SimpleTokenType::OP_RPAREN);
			return parameters;
		}
		if (fixed(SimpleTokenType::KW_VOID, 0) &&
			fixed(SimpleTokenType::OP_RPAREN, 1))
		{
			consume_fixed(SimpleTokenType::KW_VOID);
			consume_fixed(SimpleTokenType::OP_RPAREN);
			return parameters;
		}
		while (true)
		{
			if (fixed(SimpleTokenType::OP_DOTS))
			{
				consume_fixed(SimpleTokenType::OP_DOTS);
				*variadic = true;
				break;
			}
			BaseSpec spec = parse_decl_specifiers(current_);
			TypeId base = spec.resolved_type;
			DeclaratorShape shape;
			if (!fixed(SimpleTokenType::OP_COMMA) &&
				!fixed(SimpleTokenType::OP_RPAREN) &&
				!fixed(SimpleTokenType::OP_DOTS))
				shape = parse_ptr_declarator(true);
			TypeId type = apply_shape(base, shape);
			if (!shape.has_name && shape.operations.empty() &&
				model_.type_kind(type) == TypeKind::Fundamental &&
				model_.types[type.value].key.fundamental == FundamentalType::Void &&
				fixed(SimpleTokenType::OP_RPAREN) && parameters.empty())
				break;
			type = model_.remove_top_cv(type);
			if (model_.type_kind(type) == TypeKind::Array)
				type = model_.pointer(model_.types[type.value].key.child);
			else if (model_.type_kind(type) == TypeKind::Function)
				type = model_.pointer(type);
			parameters.push_back(type);
			if (fixed(SimpleTokenType::OP_DOTS))
			{
				consume_fixed(SimpleTokenType::OP_DOTS);
				*variadic = true;
				break;
			}
			if (fixed(SimpleTokenType::OP_RPAREN))
				break;
			consume_fixed(SimpleTokenType::OP_COMMA);
		}
		consume_fixed(SimpleTokenType::OP_RPAREN);
		return parameters;
	}

	bool abstract_parenthesis_is_grouped()
	{
		if (!fixed(SimpleTokenType::OP_LPAREN))
			return false;
		const SimpleTokenType next = look(1).fixed;
		if (next == SimpleTokenType::OP_STAR ||
			next == SimpleTokenType::OP_AMP ||
			next == SimpleTokenType::OP_LAND ||
			next == SimpleTokenType::OP_LSQUARE)
			return true;
		if (next == SimpleTokenType::OP_LPAREN)
		{
			const SimpleTokenType nested = look(2).fixed;
			return nested == SimpleTokenType::OP_STAR ||
				nested == SimpleTokenType::OP_AMP ||
				nested == SimpleTokenType::OP_LAND ||
				nested == SimpleTokenType::OP_LSQUARE;
		}
		if (look(1).kind == PA7TokenKind::Identifier)
		{
			QualifiedName candidate;
			candidate.components.push_back(
				model_.intern_name(look(1).spelling));
			return !model_.lookup_path(candidate, current_,
				LookupCategory::Type).found();
		}
		return false;
	}

	DeclaratorShape parse_noptr_declarator(bool allow_abstract)
	{
		DeclaratorShape result;
		if (identifier() || fixed(SimpleTokenType::OP_COLON2))
		{
			result.has_name = true;
			result.name = parse_qualified_name();
		}
		else if (fixed(SimpleTokenType::OP_LPAREN) && allow_abstract &&
			!abstract_parenthesis_is_grouped())
		{
			// In an abstract-declarator, an initial `()` or `(...)` is the
			// function suffix itself, not a parenthesized empty declarator.
		}
		else if (fixed(SimpleTokenType::OP_LPAREN))
		{
			consume_fixed(SimpleTokenType::OP_LPAREN);
			result = parse_ptr_declarator(allow_abstract);
			consume_fixed(SimpleTokenType::OP_RPAREN);
		}
		else if (!allow_abstract)
			throw std::runtime_error("expected PA7 declarator-id");

		while (fixed(SimpleTokenType::OP_LPAREN) ||
			fixed(SimpleTokenType::OP_LSQUARE))
		{
			if (fixed(SimpleTokenType::OP_LPAREN))
			{
				DeclaratorOp operation(DeclaratorOpKind::Function);
				operation.parameters = parse_parameter_clause(
					&operation.variadic);
				result.operations.push_back(operation);
			}
			else
			{
				consume_fixed(SimpleTokenType::OP_LSQUARE);
				DeclaratorOp operation(DeclaratorOpKind::Array);
				if (literal())
					operation.bound = parse_array_bound();
				else
					operation.unknown_bound = true;
				consume_fixed(SimpleTokenType::OP_RSQUARE);
				result.operations.push_back(operation);
			}
		}
		return result;
	}

	TypeId parse_type_id(NamespaceId scope)
	{
		BaseSpec spec = parse_decl_specifiers(scope);
		TypeId base = spec.resolved_type;
		if (fixed(SimpleTokenType::OP_STAR) ||
			fixed(SimpleTokenType::OP_AMP) ||
			fixed(SimpleTokenType::OP_LAND) ||
			fixed(SimpleTokenType::OP_LPAREN) ||
			fixed(SimpleTokenType::OP_LSQUARE))
		{
			DeclaratorShape shape = parse_ptr_declarator(true);
			base = apply_shape(base, shape);
		}
		return base;
	}

	DeclaratorShape parse_ptr_declarator(bool allow_abstract)
	{
		enter_nesting();
		std::vector<DeclaratorOp> prefixes;
		while (fixed(SimpleTokenType::OP_STAR) ||
			fixed(SimpleTokenType::OP_AMP) ||
			fixed(SimpleTokenType::OP_LAND))
		{
			DeclaratorOp operation;
			if (fixed(SimpleTokenType::OP_STAR))
			{
				operation.kind = DeclaratorOpKind::Pointer;
				consume_fixed(SimpleTokenType::OP_STAR);
				while (is_cv(look().fixed))
				{
					if (fixed(SimpleTokenType::KW_CONST))
					{
						operation.cv |= 1u;
						consume_fixed(SimpleTokenType::KW_CONST);
					}
					else
					{
						operation.cv |= 2u;
						consume_fixed(SimpleTokenType::KW_VOLATILE);
					}
				}
			}
			else if (fixed(SimpleTokenType::OP_AMP))
			{
				operation.kind = DeclaratorOpKind::LvalueReference;
				consume_fixed(SimpleTokenType::OP_AMP);
			}
			else
			{
				operation.kind = DeclaratorOpKind::RvalueReference;
				consume_fixed(SimpleTokenType::OP_LAND);
			}
			prefixes.push_back(operation);
		}
		DeclaratorShape result = parse_noptr_declarator(allow_abstract);
		// Prefix operators are inserted at the declarator hole after the
		// direct-declarator suffixes.  This preserves C++ binding:
		// `*f()` is a function returning pointer, while `(*f)()` is a
		// pointer to function.
		result.operations.insert(result.operations.end(), prefixes.begin(),
			prefixes.end());
		leave_nesting();
		return result;
	}

	TypeId apply_shape(TypeId base, const DeclaratorShape& shape)
	{
		TypeId result = base;
		for (std::vector<DeclaratorOp>::const_reverse_iterator it =
			shape.operations.rbegin(); it != shape.operations.rend(); ++it)
		{
			const DeclaratorOp& operation = *it;
			switch (operation.kind)
			{
			case DeclaratorOpKind::Pointer:
				result = model_.pointer(result, operation.cv);
				break;
			case DeclaratorOpKind::LvalueReference:
				result = model_.reference(result, false);
				break;
			case DeclaratorOpKind::RvalueReference:
				result = model_.reference(result, true);
				break;
			case DeclaratorOpKind::Array:
				result = model_.array(result, operation.unknown_bound,
					operation.bound);
				break;
			case DeclaratorOpKind::Function:
				result = model_.function(operation.parameters, operation.variadic,
					result);
				break;
			}
		}
		return result;
	}

	void parse_simple_declaration(NamespaceId scope)
	{
		BaseSpec spec = parse_decl_specifiers(scope);
		TypeId base = spec.resolved_type;
		while (true)
		{
			DeclaratorShape shape = parse_ptr_declarator(false);
			if (!shape.has_name)
				throw std::runtime_error("unnamed PA7 declaration");
			const TypeId type = apply_shape(base, shape);
			const NameId name = shape.name.last();
			const bool qualified = shape.name.global ||
				shape.name.components.size() > 1;
			const NamespaceId target = model_.resolve_declaration_target(
				shape.name, scope);
			if (spec.is_typedef)
				model_.declare_alias(target, name, type);
			else
			{
				const bool is_function =
					model_.type_kind(type) == TypeKind::Function;
				if (qualified)
				{
					LookupResult existing = model_.lookup_qualified(target, name,
						LookupCategory::Entity);
					if (existing.found())
					{
						if ((is_function && model_.entities[existing.entity.value].kind !=
							EntityKind::Function) ||
							(!is_function && model_.entities[existing.entity.value].kind !=
							EntityKind::Variable))
							throw std::runtime_error("qualified declaration kind conflict");
						model_.update_entity(existing.entity, type);
					}
					else
						model_.declare_value(target, name, type, is_function);
				}
				else
					model_.declare_value(scope, name, type, is_function);
			}
			if (!fixed(SimpleTokenType::OP_COMMA))
				break;
			consume_fixed(SimpleTokenType::OP_COMMA);
		}
		consume_fixed(SimpleTokenType::OP_SEMICOLON);
	}
};

PA7SemanticModel::PA7SemanticModel(const PPTokenBuffer& tokens)
	: impl_(new Impl(tokens))
{}

PA7SemanticModel::~PA7SemanticModel()
{
	delete impl_;
}

void PA7SemanticModel::analyze()
{
	PA7TokenCollector collector;
	posttokenize_cpp_tokens(impl_->input, collector);
	if (collector.invalid)
		throw std::runtime_error("invalid PA7 posttoken stream");
	if (collector.tokens.empty() ||
		collector.tokens.back().kind != PA7TokenKind::End)
		throw std::runtime_error("PA7 token stream has no EOF");
	impl_->tokens.swap(collector.tokens);
	PA7Parser parser(*impl_, impl_->tokens);
	parser.parse();
}

void PA7SemanticModel::render(std::ostream& output) const
{
	impl_->render_namespace(output, impl_->global);
}
