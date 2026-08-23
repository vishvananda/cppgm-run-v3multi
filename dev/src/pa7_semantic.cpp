#include "pa7_semantic.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <iostream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "cpp_declaration_syntax.h"

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

struct NameIdHash
{
	std::size_t operator()(const NameId& value) const
	{
		std::size_t result = value.value;
		result ^= result >> 17;
		result *= static_cast<std::size_t>(0xed5ad4bbU);
		result ^= result >> 11;
		return result;
	}
};

struct SizeHash
{
	std::size_t operator()(std::size_t value) const
	{
		value ^= value >> 17;
		value *= static_cast<std::size_t>(0xed5ad4bbU);
		value ^= value >> 11;
		return value;
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
		result = combine(result, key.child.value);
		result = combine(result, key.cv);
		result = combine(result, key.unknown_bound ? 1u : 0u);
		result = combine(result, key.bound);
		result = combine(result, key.result.value);
		result = combine(result, key.variadic ? 1u : 0u);
		result = combine(result, key.parameters.size());
		for (std::size_t i = 0; i < key.parameters.size(); ++i)
			result = combine(result, key.parameters[i].value);
		return result;
	}
};

// Open-addressed ownership index.  Slots are always a power-of-two table of
// entry indices, entries retain insertion order, and the 70% threshold leaves
// a bounded probe terminator even for adversarial collisions.
template <typename Key, typename Value, typename Hash>
class FlatHashIndex
{
private:
	struct Entry
	{
		Key key;
		Value value;

		Entry(const Key& key, const Value& value) : key(key), value(value) {}
	};

	static std::size_t empty_slot()
	{
		return std::numeric_limits<std::size_t>::max();
	}

	std::vector<std::size_t> slots_;
	std::vector<Entry> entries_;

	static bool equal_key(const Key& left, const Key& right)
	{
		return !(left < right) && !(right < left);
	}

	static bool power_of_two(std::size_t value)
	{
		return value != 0 && (value & (value - 1)) == 0;
	}

	std::size_t slot_for_key(const Key& key) const
	{
		const std::size_t mask = slots_.size() - 1;
		std::size_t slot = Hash()(key) & mask;
		for (std::size_t probes = 0; probes < slots_.size(); ++probes)
		{
			const std::size_t entry = slots_[slot];
			if (entry == empty_slot() || equal_key(entries_[entry].key, key))
				return slot;
			slot = (slot + 1) & mask;
		}
		throw std::runtime_error("PA7 flat index probe exhausted");
	}

	std::size_t slot_for_entry(std::size_t entry) const
	{
		const std::size_t mask = slots_.size() - 1;
		std::size_t slot = Hash()(entries_[entry].key) & mask;
		for (std::size_t probes = 0; probes < slots_.size(); ++probes)
		{
			if (slots_[slot] == empty_slot())
				return slot;
			slot = (slot + 1) & mask;
		}
		throw std::runtime_error("PA7 flat index rehash probe exhausted");
	}

	void rehash(std::size_t capacity)
	{
		const std::size_t maximum = std::numeric_limits<std::size_t>::max();
		if (!power_of_two(capacity) || capacity < 8 ||
			capacity > maximum / sizeof(std::size_t))
			throw std::runtime_error("PA7 flat index capacity overflow");
		std::vector<std::size_t> slots(capacity, empty_slot());
		slots_.swap(slots);
		for (std::size_t i = 0; i < entries_.size(); ++i)
		{
			const std::size_t slot = slot_for_entry(i);
			slots_[slot] = i;
		}
	}

	void ensure_capacity()
	{
		const std::size_t maximum = std::numeric_limits<std::size_t>::max();
		if (entries_.size() == maximum)
			throw std::runtime_error("PA7 flat index entry overflow");
		if (slots_.empty())
		{
			rehash(8);
			return;
		}
		const std::size_t capacity = slots_.size();
		const std::size_t limit = capacity / 10 * 7 +
			(capacity % 10) * 7 / 10;
		if (entries_.size() + 1 > limit)
		{
			if (capacity > maximum / 2)
				throw std::runtime_error("PA7 flat index growth overflow");
			rehash(capacity * 2);
		}
	}

public:
	FlatHashIndex() : slots_(), entries_() {}

	void clear()
	{
		std::vector<std::size_t>().swap(slots_);
		std::vector<Entry>().swap(entries_);
	}

	std::size_t slot_count() const
	{
		return slots_.size();
	}

	std::size_t entry_count() const
	{
		return entries_.size();
	}

	const Value* find(const Key& key) const
	{
		if (slots_.empty())
			return NULL;
		const std::size_t mask = slots_.size() - 1;
		std::size_t slot = Hash()(key) & mask;
		for (std::size_t probes = 0; probes < slots_.size(); ++probes)
		{
			const std::size_t entry = slots_[slot];
			if (entry == empty_slot())
				return NULL;
			if (equal_key(entries_[entry].key, key))
				return &entries_[entry].value;
			slot = (slot + 1) & mask;
		}
		return NULL;
	}

	Value* find(const Key& key)
	{
		return const_cast<Value*>(
			static_cast<const FlatHashIndex*>(this)->find(key));
	}

	void set(const Key& key, const Value& value)
	{
		Value* existing = find(key);
		if (existing != NULL)
		{
			*existing = value;
			return;
		}
		ensure_capacity();
		entries_.push_back(Entry(key, value));
		const std::size_t entry = entries_.size() - 1;
		const std::size_t slot = slot_for_key(key);
		slots_[slot] = entry;
	}
};

typedef FlatHashIndex<NameId, NamespaceId, NameIdHash> NamespaceIndex;
typedef FlatHashIndex<NameId, EntityId, NameIdHash> EntityIndex;
typedef FlatHashIndex<NameId, TypeId, NameIdHash> TypeIndex;
typedef FlatHashIndex<PPSpellingId, NameId, SizeHash> SpellingIndex;
typedef FlatHashIndex<TypeKey, TypeId, TypeKeyHash> CanonicalTypeIndex;

struct TypeRecord
{
	TypeKey key;
	unsigned int array_tail_cv;
	bool has_array_path;

	TypeRecord() : key(), array_tail_cv(0), has_array_path(false) {}
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
	NamespaceIndex named_children;
	NamespaceIndex namespace_aliases;
	EntityIndex entities;
	TypeIndex aliases;
	EntityIndex using_entities;
	TypeIndex using_types;
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

enum class LookupMode
{
	InNamespace,
	Unqualified
};

struct LookupCacheKey
{
	// The epoch is category-specific: a value declaration cannot invalidate a
	// type lookup, while a namespace/topology change advances all three.
	NamespaceId start;
	NameId name;
	LookupCategory category;
	LookupMode mode;
	std::uint64_t epoch;

	LookupCacheKey(NamespaceId start = NamespaceId(),
		NameId name = NameId(),
		LookupCategory category = LookupCategory::Namespace,
		LookupMode mode = LookupMode::InNamespace,
		std::uint64_t epoch = 0)
		: start(start), name(name), category(category), mode(mode),
		  epoch(epoch)
	{}

	bool operator<(const LookupCacheKey& other) const
	{
		if (start.value != other.start.value)
			return start.value < other.start.value;
		if (name.value != other.name.value)
			return name.value < other.name.value;
		if (category != other.category)
			return static_cast<int>(category) <
				static_cast<int>(other.category);
		if (mode != other.mode)
			return static_cast<int>(mode) < static_cast<int>(other.mode);
		return epoch < other.epoch;
	}
};

struct LookupCacheKeyHash
{
	static std::size_t combine(std::size_t seed, std::size_t value)
	{
		value += static_cast<std::size_t>(0x9e3779b9U) +
			(seed << 6) + (seed >> 2);
		return seed ^ value;
	}

	std::size_t operator()(const LookupCacheKey& key) const
	{
		std::size_t result = key.start.value;
		result = combine(result, key.name.value);
		result = combine(result, static_cast<std::size_t>(key.category));
		result = combine(result, static_cast<std::size_t>(key.mode));
		result = combine(result, static_cast<std::size_t>(key.epoch));
		return result;
	}
};

typedef FlatHashIndex<LookupCacheKey, LookupResult,
	LookupCacheKeyHash> LookupCacheIndex;

} // namespace

struct PA7SemanticModel::Impl
{
	const PPTokenBuffer& input;
	std::vector<CppSyntaxToken> tokens;
	std::vector<PPSpellingId> names;
	SpellingIndex names_by_spelling;
	std::vector<TypeRecord> types;
	CanonicalTypeIndex canonical_types;
	std::vector<NamespaceRecord> namespaces;
	std::vector<EntityRecord> entities;
	NamespaceId global;

	struct LookupFrame
	{
		NamespaceId id;
		std::size_t child_index;
		std::size_t directive_index;
		bool entered;

		LookupFrame(NamespaceId id = NamespaceId())
			: id(id), child_index(0), directive_index(0), entered(false)
		{}
	};

	mutable std::vector<std::uint32_t> lookup_marks;
	mutable std::uint32_t lookup_generation;
	mutable std::vector<LookupFrame> lookup_work;
	mutable LookupCacheIndex namespace_cache;
	mutable LookupCacheIndex type_cache;
	mutable LookupCacheIndex entity_cache;
	mutable std::uint64_t namespace_epoch;
	mutable std::uint64_t type_epoch;
	mutable std::uint64_t entity_epoch;
#ifdef PA7_AUDIT_COUNTERS
	std::size_t lookup_queries;
	std::size_t lookup_namespace_visits;
	std::size_t lookup_cache_hits;
	std::size_t lookup_cache_misses;
#endif

	explicit Impl(const PPTokenBuffer& input)
		: input(input), tokens(), names(), names_by_spelling(), types(),
		  canonical_types(), namespaces(), entities(), global(), lookup_marks(),
		  lookup_generation(0), lookup_work(), namespace_cache(), type_cache(),
		  entity_cache(), namespace_epoch(1), type_epoch(1), entity_epoch(1)
#ifdef PA7_AUDIT_COUNTERS
		  , lookup_queries(0), lookup_namespace_visits(0), lookup_cache_hits(0),
		  lookup_cache_misses(0)
#endif
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
		const NameId* found = names_by_spelling.find(spelling);
		if (found != NULL)
			return *found;
		const NameId result(names.size());
		names.push_back(spelling);
		names_by_spelling.set(spelling, result);
		return result;
	}

	bool lookup_name(PPSpellingId spelling, NameId* result) const
	{
		const NameId* found = names_by_spelling.find(spelling);
		if (found == NULL)
			return false;
		*result = *found;
		return true;
	}

	bool make_lookup_name(const CppSyntaxQualifiedName& source,
		QualifiedName* result) const
	{
		result->global = source.global;
		result->components.clear();
		result->components.reserve(source.components.size());
		for (std::size_t i = 0; i < source.components.size(); ++i)
		{
			NameId name;
			if (!lookup_name(source.components[i], &name))
				return false;
			result->components.push_back(name);
		}
		return true;
	}

	const std::string& name_text(NameId name) const
	{
		if (!name.valid() || name.value >= names.size())
			throw std::runtime_error("invalid PA7 name identity");
		return input.spellings.get(names[name.value]);
	}

	TypeId intern_type(const TypeKey& key)
	{
		const TypeId* found = canonical_types.find(key);
		if (found != NULL)
			return *found;
		const TypeId result(types.size());
		TypeRecord record;
		record.key = key;
		if (key.kind == TypeKind::Cv)
		{
			const TypeRecord& child = types[key.child.value];
			record.array_tail_cv = child.array_tail_cv | key.cv;
			record.has_array_path = child.has_array_path;
		}
		else if (key.kind == TypeKind::Array)
		{
			const TypeRecord& child = types[key.child.value];
			record.array_tail_cv = child.array_tail_cv;
			record.has_array_path = true;
		}
		types.push_back(record);
		canonical_types.set(key, result);
		return result;
	}

	TypeId fundamental(FundamentalType type) const
	{
		TypeKey key;
		key.kind = TypeKind::Fundamental;
		key.fundamental = type;
		const TypeId* found = canonical_types.find(key);
		if (found == NULL)
			throw std::runtime_error("missing fundamental type identity");
		return *found;
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
		const TypeRecord& existing = types[child.value];
		if (existing.has_array_path &&
			(existing.array_tail_cv & qualifiers) == qualifiers)
			return child;
		struct ArrayFrame
		{
			bool unknown_bound;
			std::size_t bound;

			ArrayFrame(bool unknown_bound, std::size_t bound)
				: unknown_bound(unknown_bound), bound(bound)
			{}
		};

		// Sequential aliases can make an owned type path deeper than the
		// parser's per-declaration nesting bound. Walk that path iteratively;
		// the vector is bounded by the number of array facts already owned by
		// this model, rather than by an unrelated syntax-depth constant.
		std::vector<ArrayFrame> arrays;
		TypeId current = child;
		for (;;)
		{
			const TypeKind kind = type_kind(current);
			if (kind == TypeKind::Cv)
			{
				const TypeKey& old = types[current.value].key;
				qualifiers |= old.cv;
				current = old.child;
				continue;
			}
			if (kind == TypeKind::Array)
			{
				const TypeKey& array_type = types[current.value].key;
				arrays.push_back(ArrayFrame(array_type.unknown_bound,
					array_type.bound));
				current = array_type.child;
				continue;
			}
			break;
		}
		if (type_kind(current) == TypeKind::LvalueReference ||
			type_kind(current) == TypeKind::RvalueReference)
		{
			for (std::vector<ArrayFrame>::const_reverse_iterator it =
				arrays.rbegin(); it != arrays.rend(); ++it)
				current = array(current, it->unknown_bound, it->bound);
			return current;
		}
		TypeKey key;
		key.kind = TypeKind::Cv;
		key.child = current;
		key.cv = qualifiers;
		TypeId result = intern_type(key);
		for (std::vector<ArrayFrame>::const_reverse_iterator it =
			arrays.rbegin(); it != arrays.rend(); ++it)
			result = array(result, it->unknown_bound, it->bound);
		return result;
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
		const NamespaceId* found = scope.named_children.find(name);
		if (found != NULL)
		{
			NamespaceRecord& existing = namespaces[found->value];
			if (inline_namespace != existing.inline_namespace)
				throw std::runtime_error("inline namespace reopening mismatch");
			return *found;
		}
		const NamespaceId result = create_namespace(parent, name, false,
			inline_namespace);
		namespaces[parent.value].named_children.set(name, result);
		invalidate_topology();
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
		invalidate_topology();
		return result;
	}

	LookupCacheIndex& cache_for(LookupCategory category) const
	{
		if (category == LookupCategory::Namespace)
			return namespace_cache;
		if (category == LookupCategory::Type)
			return type_cache;
		return entity_cache;
	}

	std::uint64_t epoch_for(LookupCategory category) const
	{
		if (category == LookupCategory::Namespace)
			return namespace_epoch;
		if (category == LookupCategory::Type)
			return type_epoch;
		return entity_epoch;
	}

	void invalidate(LookupCategory category) const
	{
		LookupCacheIndex& cache = cache_for(category);
		std::uint64_t* epoch = NULL;
		if (category == LookupCategory::Namespace)
			epoch = &namespace_epoch;
		else if (category == LookupCategory::Type)
			epoch = &type_epoch;
		else
			epoch = &entity_epoch;
		if (*epoch == std::numeric_limits<std::uint64_t>::max())
			*epoch = 1;
		else
			++*epoch;
		cache.clear();
	}

	void invalidate_topology() const
	{
		invalidate(LookupCategory::Namespace);
		invalidate(LookupCategory::Type);
		invalidate(LookupCategory::Entity);
	}

	void begin_lookup() const
	{
		if (lookup_marks.size() < namespaces.size())
			lookup_marks.resize(namespaces.size(), 0);
		++lookup_generation;
		if (lookup_generation == 0)
		{
			std::fill(lookup_marks.begin(), lookup_marks.end(), 0);
			lookup_generation = 1;
		}
		lookup_work.clear();
	}

	bool mark_lookup_namespace(NamespaceId id) const
	{
		if (!id.valid() || id.value >= namespaces.size())
			return false;
		if (lookup_marks[id.value] == lookup_generation)
			return false;
		lookup_marks[id.value] = lookup_generation;
		return true;
	}

	LookupResult lookup_marked(NameId name, LookupCategory category) const
	{
		while (!lookup_work.empty())
		{
			const std::size_t frame_index = lookup_work.size() - 1;
			LookupFrame& frame = lookup_work[frame_index];
			const NamespaceRecord& record = namespaces[frame.id.value];
			if (!frame.entered)
			{
				frame.entered = true;
#ifdef PA7_AUDIT_COUNTERS
				++const_cast<Impl*>(this)->lookup_namespace_visits;
#endif
				if (category == LookupCategory::Namespace)
				{
					const NamespaceId* direct =
						record.named_children.find(name);
					if (direct != NULL)
						return LookupResult::namespace_result(*direct);
					const NamespaceId* alias =
						record.namespace_aliases.find(name);
					if (alias != NULL)
						return LookupResult::namespace_result(*alias);
				}
				else if (category == LookupCategory::Type)
				{
					const TypeId* direct = record.aliases.find(name);
					if (direct != NULL)
						return LookupResult::type_result(*direct);
					const TypeId* imported = record.using_types.find(name);
					if (imported != NULL)
						return LookupResult::type_result(*imported);
				}
				else
				{
					const EntityId* direct = record.entities.find(name);
					if (direct != NULL)
						return LookupResult::entity_result(*direct);
					const EntityId* imported =
						record.using_entities.find(name);
					if (imported != NULL)
						return LookupResult::entity_result(*imported);
				}
			}

			if (frame.child_index < record.children.size())
			{
				const NamespaceId child_id =
					record.children[frame.child_index++];
				const NamespaceRecord& child = namespaces[child_id.value];
				if ((child.anonymous || child.inline_namespace) &&
					mark_lookup_namespace(child_id))
					lookup_work.push_back(LookupFrame(child_id));
				continue;
			}
			if (frame.directive_index < record.using_directives.size())
			{
				const NamespaceId target =
					record.using_directives[frame.directive_index++];
				if (mark_lookup_namespace(target))
					lookup_work.push_back(LookupFrame(target));
				continue;
			}
			lookup_work.pop_back();
		}
		return LookupResult();
	}

	LookupResult lookup_in_namespace(NamespaceId scope, NameId name,
		LookupCategory category) const
	{
		if (!scope.valid() || scope.value >= namespaces.size())
			return LookupResult();
	#ifdef PA7_AUDIT_COUNTERS
		++const_cast<Impl*>(this)->lookup_queries;
	#endif
		const LookupCacheKey key(scope, name, category,
			LookupMode::InNamespace, epoch_for(category));
		const LookupResult* cached = cache_for(category).find(key);
		if (cached != NULL)
		{
	#ifdef PA7_AUDIT_COUNTERS
			++const_cast<Impl*>(this)->lookup_cache_hits;
	#endif
			return *cached;
		}
	#ifdef PA7_AUDIT_COUNTERS
		++const_cast<Impl*>(this)->lookup_cache_misses;
	#endif
		begin_lookup();
		mark_lookup_namespace(scope);
		lookup_work.push_back(LookupFrame(scope));
		const LookupResult result = lookup_marked(name, category);
		cache_for(category).set(key, result);
		return result;
	}

	LookupResult lookup_unqualified(NamespaceId start, NameId name,
		LookupCategory category) const
	{
		if (!start.valid() || start.value >= namespaces.size())
			return LookupResult();
	#ifdef PA7_AUDIT_COUNTERS
		++const_cast<Impl*>(this)->lookup_queries;
	#endif
		const LookupCacheKey key(start, name, category,
			LookupMode::Unqualified, epoch_for(category));
		const LookupResult* cached = cache_for(category).find(key);
		if (cached != NULL)
		{
	#ifdef PA7_AUDIT_COUNTERS
			++const_cast<Impl*>(this)->lookup_cache_hits;
	#endif
			return *cached;
		}
	#ifdef PA7_AUDIT_COUNTERS
		++const_cast<Impl*>(this)->lookup_cache_misses;
	#endif
		NamespaceId scope = start;
		LookupResult result;
		while (scope.valid())
		{
			LookupResult found = lookup_in_namespace(scope, name, category);
			if (found.found())
			{
				result = found;
				break;
			}
			if (scope.value == global.value)
				break;
			scope = namespaces[scope.value].parent;
		}
		cache_for(category).set(key, result);
		return result;
	}

	LookupResult lookup_qualified(NamespaceId scope, NameId name,
		LookupCategory category) const
	{
		return lookup_in_namespace(scope, name, category);
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

	bool accepts_lookup(const CppSyntaxQualifiedName& source,
		NamespaceId start, LookupCategory category) const
	{
		QualifiedName path;
		if (!make_lookup_name(source, &path))
			return false;
		return lookup_path(path, start, category).found();
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
		if (record.entities.find(name) != NULL)
			throw std::runtime_error("typedef conflicts with value declaration");
		record.aliases.set(name, type);
		invalidate(LookupCategory::Type);
	}

	void declare_namespace_alias(NamespaceId scope, NameId name,
		NamespaceId target)
	{
		namespaces[scope.value].namespace_aliases.set(name, target);
		invalidate_topology();
	}

	void add_using_directive(NamespaceId scope, NamespaceId target)
	{
		std::vector<NamespaceId>& directives =
			namespaces[scope.value].using_directives;
		if (std::find_if(directives.begin(), directives.end(),
			[target](NamespaceId value) { return value.value == target.value; }) ==
			directives.end())
		{
			directives.push_back(target);
			invalidate_topology();
		}
	}

	void add_using_type(NamespaceId scope, NameId name, TypeId type)
	{
		namespaces[scope.value].using_types.set(name, type);
		invalidate(LookupCategory::Type);
	}

	void add_using_entity(NamespaceId scope, NameId name, EntityId entity)
	{
		namespaces[scope.value].using_entities.set(name, entity);
		invalidate(LookupCategory::Entity);
	}

	TypeId merge_types(TypeId old_type, TypeId new_type)
	{
		struct ArrayMerge
		{
			bool old_unknown;
			std::size_t old_bound;
			bool new_unknown;
			std::size_t new_bound;
			TypeId new_value;
		};
		std::vector<ArrayMerge> stack;
		TypeId old_cursor = old_type;
		TypeId new_cursor = new_type;
		while (old_cursor.value != new_cursor.value)
		{
			const TypeKey& old_key = types[old_cursor.value].key;
			const TypeKey& new_key = types[new_cursor.value].key;
			if (old_key.kind != TypeKind::Array ||
				new_key.kind != TypeKind::Array)
			break;
			ArrayMerge merge = {old_key.unknown_bound, old_key.bound,
				new_key.unknown_bound, new_key.bound, new_cursor};
			stack.push_back(merge);
			old_cursor = old_key.child;
			new_cursor = new_key.child;
		}
		TypeId result = old_cursor.value == new_cursor.value ? old_cursor :
			new_cursor;
		for (std::vector<ArrayMerge>::const_reverse_iterator it = stack.rbegin();
			it != stack.rend(); ++it)
		{
			if (it->old_unknown && !it->new_unknown)
				result = array(result, false, it->new_bound);
			else if (!it->old_unknown && it->new_unknown)
				result = array(result, false, it->old_bound);
			else if (it->old_unknown && it->new_unknown)
				result = array(result, true, 0);
			else if (it->old_bound == it->new_bound)
				result = array(result, false, it->old_bound);
			else
				result = it->new_value;
		}
		return result;
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
		EntityId* found = record.entities.find(name);
		if (found != NULL)
		{
			EntityRecord& existing = entities[found->value];
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
		record.entities.set(name, id);
		if (is_function)
			record.functions.push_back(id);
		else
			record.variables.push_back(id);
		invalidate(LookupCategory::Entity);
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
		struct RenderTask
		{
			enum Kind { Type, Text } kind;
			TypeId type;
			const char* text;
			std::size_t depth;

			RenderTask(Kind kind, TypeId type, const char* text,
				std::size_t depth)
				: kind(kind), type(type), text(text), depth(depth)
			{}
		};

		std::string result;
		std::vector<RenderTask> tasks;
		tasks.push_back(RenderTask(RenderTask::Type, type, NULL, depth));
		const std::size_t type_walk_limit = types.size();
		while (!tasks.empty())
		{
			const RenderTask task = tasks.back();
			tasks.pop_back();
			if (task.depth > type_walk_limit)
				throw std::runtime_error("type rendering cycle detected");
			if (task.kind == RenderTask::Text)
			{
				result += task.text;
				continue;
			}
			if (!task.type.valid() || task.type.value >= types.size())
				throw std::runtime_error("invalid PA7 type for rendering");
			const TypeKey& key = types[task.type.value].key;
			if (key.kind == TypeKind::Fundamental)
			{
				result += fundamental_type_name(key.fundamental);
				continue;
			}
			if (key.kind == TypeKind::Cv)
			{
				if ((key.cv & 1u) != 0)
					result += "const ";
				if ((key.cv & 2u) != 0)
					result += "volatile ";
				tasks.push_back(RenderTask(RenderTask::Type, key.child, NULL,
					task.depth + 1));
				continue;
			}
			if (key.kind == TypeKind::Pointer)
			{
				if ((key.cv & 1u) != 0)
					result += "const ";
				if ((key.cv & 2u) != 0)
					result += "volatile ";
				result += "pointer to ";
				tasks.push_back(RenderTask(RenderTask::Type, key.child, NULL,
					task.depth + 1));
				continue;
			}
			if (key.kind == TypeKind::LvalueReference)
			{
				result += "lvalue-reference to ";
				tasks.push_back(RenderTask(RenderTask::Type, key.child, NULL,
					task.depth + 1));
				continue;
			}
			if (key.kind == TypeKind::RvalueReference)
			{
				result += "rvalue-reference to ";
				tasks.push_back(RenderTask(RenderTask::Type, key.child, NULL,
					task.depth + 1));
				continue;
			}
			if (key.kind == TypeKind::Array)
			{
				result += "array of ";
				if (key.unknown_bound)
					result += "unknown bound of ";
				else
				{
					std::ostringstream bound;
					bound << key.bound;
					result += bound.str();
					result += " ";
				}
				tasks.push_back(RenderTask(RenderTask::Type, key.child, NULL,
					task.depth + 1));
				continue;
			}

			result += "function of (";
			tasks.push_back(RenderTask(RenderTask::Type, key.result, NULL,
				task.depth + 1));
			tasks.push_back(RenderTask(RenderTask::Text, TypeId(),
				") returning ", task.depth));
			if (key.variadic)
			{
				tasks.push_back(RenderTask(RenderTask::Text, TypeId(),
					"...", task.depth));
				if (!key.parameters.empty())
					tasks.push_back(RenderTask(RenderTask::Text, TypeId(),
						", ", task.depth));
			}
			for (std::size_t i = key.parameters.size(); i != 0; --i)
			{
				tasks.push_back(RenderTask(RenderTask::Type,
					key.parameters[i - 1], NULL, task.depth + 1));
				if (i > 1)
					tasks.push_back(RenderTask(RenderTask::Text, TypeId(),
						", ", task.depth));
			}
		}
		return result;
	}

	void render_namespace(std::ostream& output, NamespaceId id,
		std::size_t depth = 0) const
	{
		struct NamespaceTask
		{
			NamespaceId id;
			std::size_t depth;
			bool closing;

			NamespaceTask(NamespaceId id, std::size_t depth, bool closing)
				: id(id), depth(depth), closing(closing)
			{}
		};

		std::vector<NamespaceTask> tasks;
		tasks.push_back(NamespaceTask(id, depth, false));
		while (!tasks.empty())
		{
			const NamespaceTask task = tasks.back();
			tasks.pop_back();
			if (task.depth > 4096)
				throw std::runtime_error("namespace rendering nesting limit reached");
			const NamespaceRecord& record = namespaces[task.id.value];
			if (task.closing)
			{
				output << "end namespace\n";
				continue;
			}
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
			tasks.push_back(NamespaceTask(task.id, task.depth, true));
			for (std::size_t i = record.children.size(); i != 0; --i)
				tasks.push_back(NamespaceTask(record.children[i - 1],
					task.depth + 1, false));
		}
	}

#ifdef PA7_AUDIT_COUNTERS
	void audit_report() const
	{
		std::size_t namespace_slots = 0;
		std::size_t namespace_entries = 0;
		for (std::size_t i = 0; i < namespaces.size(); ++i)
		{
			const NamespaceRecord& record = namespaces[i];
			namespace_slots += record.named_children.slot_count();
			namespace_slots += record.namespace_aliases.slot_count();
			namespace_slots += record.entities.slot_count();
			namespace_slots += record.aliases.slot_count();
			namespace_slots += record.using_entities.slot_count();
			namespace_slots += record.using_types.slot_count();
			namespace_entries += record.named_children.entry_count();
			namespace_entries += record.namespace_aliases.entry_count();
			namespace_entries += record.entities.entry_count();
			namespace_entries += record.aliases.entry_count();
			namespace_entries += record.using_entities.entry_count();
			namespace_entries += record.using_types.entry_count();
		}
		std::size_t parameter_ids = 0;
		for (std::size_t i = 0; i < types.size(); ++i)
			parameter_ids += types[i].key.parameters.size();
		const std::size_t cache_slots = namespace_cache.slot_count() +
			type_cache.slot_count() + entity_cache.slot_count();
		const std::size_t cache_entries = namespace_cache.entry_count() +
			type_cache.entry_count() + entity_cache.entry_count();
		std::cerr << "PA7_AUDIT lookup_queries=" << lookup_queries
			<< " namespace_visits=" << lookup_namespace_visits
			<< " lookup_cache_hits=" << lookup_cache_hits
			<< " lookup_cache_misses=" << lookup_cache_misses
			<< " namespaces=" << namespaces.size()
			<< " names=" << names.size()
			<< " types=" << types.size()
			<< " type_parameter_ids=" << parameter_ids
			<< " canonical_slots=" << canonical_types.slot_count()
			<< " canonical_entries=" << canonical_types.entry_count()
			<< " namespace_index_slots=" << namespace_slots
			<< " namespace_index_entries=" << namespace_entries
			<< " lookup_cache_slots=" << cache_slots
			<< " lookup_cache_entries=" << cache_entries << "\n";
	}
#endif
};

class PA7SemanticActions : public CppDeclarationSyntaxConsumer
{
public:
	explicit PA7SemanticActions(PA7SemanticModel::Impl& model)
		: model_(model), current_(model.global), namespace_parents_(),
		  active_spec_(), declaration_active_(false)
	{}

	bool accept_type_name(const CppSyntaxQualifiedName& name) const
	{
		return model_.accepts_lookup(name, current_, LookupCategory::Type);
	}

	bool accept_namespace_name(const CppSyntaxQualifiedName& name) const
	{
		return model_.accepts_lookup(name, current_,
			LookupCategory::Namespace);
	}

	bool accept_nested_name_specifier(
		const CppSyntaxQualifiedName& name) const
	{
		if (name.global)
			return true;
		if (name.components.size() < 2)
			return false;
		CppSyntaxQualifiedName prefix;
		prefix.components.assign(name.components.begin(),
			name.components.end() - 1);
		return model_.accepts_lookup(prefix, current_,
			LookupCategory::Namespace);
	}

	void on_namespace_begin(bool inline_namespace, bool anonymous_namespace,
		PPSpellingId name)
	{
		NamespaceId child;
		if (anonymous_namespace)
			child = model_.anonymous_namespace(current_, inline_namespace);
		else
			child = model_.named_namespace(current_, model_.intern_name(name),
				inline_namespace);
		namespace_parents_.push_back(current_);
		current_ = child;
	}

	void on_namespace_end()
	{
		if (namespace_parents_.empty())
			throw std::runtime_error("PA7 namespace action underflow");
		current_ = namespace_parents_.back();
		namespace_parents_.pop_back();
	}

	void on_namespace_alias(PPSpellingId name,
		const CppSyntaxQualifiedName& target)
	{
		model_.declare_namespace_alias(current_, model_.intern_name(name),
			model_.resolve_namespace_path(qualified_name(target), current_));
	}

	void on_using_directive(const CppSyntaxQualifiedName& target)
	{
		model_.add_using_directive(current_,
			model_.resolve_namespace_path(qualified_name(target), current_));
	}

	void on_using_declaration(const CppSyntaxQualifiedName& source)
	{
		QualifiedName introduced = qualified_name(source);
		LookupResult type = model_.lookup_path(introduced, current_,
			LookupCategory::Type);
		if (type.found())
		{
			model_.add_using_type(current_, introduced.last(), type.type);
			return;
		}
		LookupResult entity = model_.lookup_path(introduced, current_,
			LookupCategory::Entity);
		if (!entity.found())
			throw std::runtime_error("unresolved PA7 using declaration");
		model_.add_using_entity(current_, introduced.last(), entity.entity);
	}

	void on_alias_declaration(PPSpellingId name,
		const CppSyntaxTypeId& source)
	{
		model_.declare_alias(current_, model_.intern_name(name),
			type_id(source, current_));
	}

	void on_simple_declaration_begin(const CppSyntaxDeclSpec& source)
	{
		if (declaration_active_)
			throw std::runtime_error("nested PA7 declaration action");
		active_spec_ = decl_spec(source, current_);
		declaration_active_ = true;
	}

	void on_simple_declarator(
		const CppSyntaxDeclarator& declarator_source)
	{
		if (!declaration_active_)
			throw std::runtime_error("PA7 declaration action without begin");
		DeclaratorShape shape = declarator(declarator_source, current_);
		if (!shape.has_name)
			throw std::runtime_error("unnamed PA7 declaration");
		const TypeId type = apply_shape(active_spec_.resolved_type, shape);
		const NameId name = shape.name.last();
		const bool qualified = shape.name.global ||
			shape.name.components.size() > 1;
		const NamespaceId target =
			model_.resolve_declaration_target(shape.name, current_);
		if (active_spec_.is_typedef)
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
					if ((is_function &&
						model_.entities[existing.entity.value].kind !=
							EntityKind::Function) ||
						(!is_function &&
						model_.entities[existing.entity.value].kind !=
							EntityKind::Variable))
						throw std::runtime_error(
							"qualified declaration kind conflict");
					model_.update_entity(existing.entity, type);
				}
				else
					model_.declare_value(target, name, type, is_function);
			}
			else
				model_.declare_value(current_, name, type, is_function);
		}
	}

	void on_simple_declaration_end()
	{
		if (!declaration_active_)
			throw std::runtime_error("PA7 declaration action without begin");
		declaration_active_ = false;
	}

private:
	PA7SemanticModel::Impl& model_;
	NamespaceId current_;
	std::vector<NamespaceId> namespace_parents_;
	BaseSpec active_spec_;
	bool declaration_active_;

	QualifiedName qualified_name(const CppSyntaxQualifiedName& source)
	{
		QualifiedName result;
		result.global = source.global;
		result.components.reserve(source.components.size());
		for (std::size_t i = 0; i < source.components.size(); ++i)
			result.components.push_back(model_.intern_name(source.components[i]));
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
				spec.has_signed ? FundamentalType::ShortInt :
				FundamentalType::ShortInt;
		else if (spec.has_unsigned)
			type = FundamentalType::UnsignedInt;
		else if (spec.has_signed || spec.has_int)
			type = FundamentalType::Int;
		else
			type = FundamentalType::Int;
		return model_.cv(model_.fundamental(type), spec.cv);
	}

	BaseSpec decl_spec(const CppSyntaxDeclSpec& source, NamespaceId scope)
	{
		BaseSpec result;
		result.is_typedef = source.is_typedef;
		result.has_named_type = source.has_named_type;
		result.named_type = qualified_name(source.named_type);
		result.cv = source.cv;
		result.has_char = source.has_char;
		result.has_short = source.has_short;
		result.has_int = source.has_int;
		result.long_count = source.long_count;
		result.has_signed = source.has_signed;
		result.has_unsigned = source.has_unsigned;
		result.has_bool = source.has_bool;
		result.has_wchar = source.has_wchar;
		result.has_char16 = source.has_char16;
		result.has_char32 = source.has_char32;
		result.has_float = source.has_float;
		result.has_double = source.has_double;
		result.has_void = source.has_void;

		if (result.has_named_type)
		{
			if (result.has_char || result.has_short || result.has_int ||
				result.long_count != 0 || result.has_signed ||
				result.has_unsigned || result.has_bool || result.has_wchar ||
				result.has_char16 || result.has_char32 || result.has_float ||
				result.has_double || result.has_void)
				throw std::runtime_error("mixed PA7 type specifiers");
			result.resolved_type = model_.cv(
				model_.lookup_type_path(result.named_type, scope), result.cv);
		}
		else
			result.resolved_type = fundamental_from_spec(result);
		return result;
	}

	std::vector<TypeId> parameters(const CppSyntaxDeclaratorOp& source,
		NamespaceId scope)
	{
		std::vector<TypeId> result;
		if (source.parameters.size() == 1 &&
			!source.parameters[0].has_declarator)
		{
			BaseSpec only = decl_spec(source.parameters[0].spec, scope);
			if (model_.type_kind(only.resolved_type) == TypeKind::Fundamental &&
				model_.types[only.resolved_type.value].key.fundamental ==
					FundamentalType::Void)
				return result;
		}
		result.reserve(source.parameters.size());
		for (std::size_t i = 0; i < source.parameters.size(); ++i)
		{
			const CppSyntaxParameter& parameter = source.parameters[i];
			BaseSpec spec = decl_spec(parameter.spec, scope);
			DeclaratorShape shape;
			if (parameter.has_declarator)
				shape = declarator(parameter.declarator, scope);
			TypeId type = apply_shape(spec.resolved_type, shape);
			type = model_.remove_top_cv(type);
			if (model_.type_kind(type) == TypeKind::Array)
				type = model_.pointer(model_.types[type.value].key.child);
			else if (model_.type_kind(type) == TypeKind::Function)
				type = model_.pointer(type);
			result.push_back(type);
		}
		return result;
	}

	DeclaratorShape declarator(const CppSyntaxDeclarator& source,
		NamespaceId scope)
	{
		DeclaratorShape result;
		result.has_name = source.has_name;
		result.name = qualified_name(source.name);
		result.operations.reserve(source.operations.size());
		for (std::size_t i = 0; i < source.operations.size(); ++i)
		{
			const CppSyntaxDeclaratorOp& source_operation =
				source.operations[i];
			DeclaratorOp operation;
			switch (source_operation.kind)
			{
			case CppSyntaxDeclaratorOpKind::Pointer:
				operation.kind = DeclaratorOpKind::Pointer;
				break;
			case CppSyntaxDeclaratorOpKind::LvalueReference:
				operation.kind = DeclaratorOpKind::LvalueReference;
				break;
			case CppSyntaxDeclaratorOpKind::RvalueReference:
				operation.kind = DeclaratorOpKind::RvalueReference;
				break;
			case CppSyntaxDeclaratorOpKind::Array:
				operation.kind = DeclaratorOpKind::Array;
				break;
			case CppSyntaxDeclaratorOpKind::Function:
				operation.kind = DeclaratorOpKind::Function;
				break;
			}
			operation.cv = source_operation.cv;
			operation.unknown_bound = source_operation.unknown_bound;
			operation.bound = source_operation.bound;
			operation.variadic = source_operation.variadic;
			if (source_operation.kind == CppSyntaxDeclaratorOpKind::Function)
				operation.parameters = parameters(source_operation, scope);
			result.operations.push_back(operation);
		}
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

	TypeId type_id(const CppSyntaxTypeId& source, NamespaceId scope)
	{
		BaseSpec spec = decl_spec(source.spec, scope);
		if (!source.has_declarator)
			return spec.resolved_type;
		return apply_shape(spec.resolved_type,
			declarator(source.declarator, scope));
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
	CppSyntaxTokenCollector collector;
	posttokenize_cpp_tokens(impl_->input, collector);
	if (collector.invalid)
		throw std::runtime_error("invalid PA7 posttoken stream");
	if (collector.tokens.empty() ||
		collector.tokens.back().kind != CppSyntaxTokenKind::End)
			throw std::runtime_error("PA7 token stream has no EOF");
	impl_->tokens.swap(collector.tokens);
	PA7SemanticActions actions(*impl_);
	{
		CppDeclarationSyntaxParser parser(impl_->tokens, actions);
		parser.parse();
	}
	std::vector<CppSyntaxToken>().swap(impl_->tokens);
#ifdef PA7_AUDIT_COUNTERS
	impl_->audit_report();
#endif
}

void PA7SemanticModel::render(std::ostream& output) const
{
	impl_->render_namespace(output, impl_->global);
}
