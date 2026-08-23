#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "IPPTokenStream.h"
#include "cpp_declaration_syntax.h"
#include "cpp_syntax_tokens.h"

namespace CppSemantic
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

	bool operator==(const NameId& other) const
	{
		return value == other.value;
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

	bool operator==(const TypeId& other) const
	{
		return value == other.value;
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

	bool operator<(const EntityId& other) const
	{
		return value < other.value;
	}

	bool operator==(const EntityId& other) const
	{
		return value == other.value;
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

	bool operator<(const NamespaceId& other) const
	{
		return value < other.value;
	}

	bool operator==(const NamespaceId& other) const
	{
		return value == other.value;
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

struct SourceNameKey
{
	NameId name;
	std::size_t translation_unit;

	SourceNameKey(NameId name = NameId(),
		std::size_t translation_unit = 0)
		: name(name), translation_unit(translation_unit)
	{}

	bool operator<(const SourceNameKey& other) const
	{
		if (name.value != other.name.value)
			return name.value < other.name.value;
		return translation_unit < other.translation_unit;
	}
};

struct SourceNameKeyHash
{
	static std::size_t combine(std::size_t seed, std::size_t value)
	{
		value += static_cast<std::size_t>(0x9e3779b9U) +
			(seed << 6) + (seed >> 2);
		return seed ^ value;
	}

	std::size_t operator()(const SourceNameKey& key) const
	{
		return combine(key.name.value, key.translation_unit);
	}
};

struct OccurrenceRange
{
	std::size_t begin;
	std::size_t count;

	OccurrenceRange(std::size_t begin = 0, std::size_t count = 0)
		: begin(begin), count(count)
	{}
};

struct StringHash
{
	std::size_t operator()(const std::string& value) const
	{
		std::size_t result = static_cast<std::size_t>(1469598103934665603ULL);
		for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
		{
			result ^= static_cast<unsigned char>(*it);
			result *= static_cast<std::size_t>(1099511628211ULL);
		}
		return result;
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
struct EntityBucketId
{
	std::size_t value;

	EntityBucketId(std::size_t value = std::numeric_limits<std::size_t>::max())
		: value(value)
	{}

	bool valid() const
	{
		return value != std::numeric_limits<std::size_t>::max();
	}
};

typedef FlatHashIndex<NameId, EntityBucketId, NameIdHash> EntityBucketIndex;
typedef FlatHashIndex<SourceNameKey, EntityBucketId,
	SourceNameKeyHash> SourceEntityBucketIndex;
typedef FlatHashIndex<SourceNameKey, TypeId, SourceNameKeyHash>
	SourceTypeIndex;
typedef FlatHashIndex<SourceNameKey, NamespaceId, SourceNameKeyHash>
	SourceNamespaceIndex;
typedef FlatHashIndex<std::size_t, OccurrenceRange, SizeHash>
	TranslationUnitRangeIndex;
typedef FlatHashIndex<PPSpellingId, NameId, SizeHash> SpellingIndex;
typedef FlatHashIndex<std::string, NameId, StringHash> NameTextIndex;
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
	std::size_t translation_unit;
	bool internal_linkage;
	std::size_t last_declaration_translation_unit;
	bool is_static;
	bool is_thread_local;
	bool is_extern;
	bool is_const;
	bool is_constexpr;
	bool is_inline;
	bool has_definition;
	std::size_t definition_translation_unit;
	bool has_constant;
	std::vector<std::uint8_t> constant_bytes;
	bool has_relocation;
	EntityId relocation;
	std::ptrdiff_t relocation_addend;

	EntityRecord()
		: kind(EntityKind::Variable), name(), type(), owner(),
		  translation_unit(0), internal_linkage(false),
		  last_declaration_translation_unit(
			std::numeric_limits<std::size_t>::max()), is_static(false),
		  is_thread_local(false), is_extern(false), is_const(false),
		  is_constexpr(false), is_inline(false), has_definition(false),
		  definition_translation_unit(std::numeric_limits<std::size_t>::max()),
		  has_constant(false), constant_bytes(), has_relocation(false),
		  relocation(), relocation_addend(0)
	{}
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
	bool internal_scope;
	std::size_t translation_unit;
	std::size_t last_declaration_translation_unit;
	std::vector<EntityId> variables;
	std::vector<EntityId> functions;
	std::vector<NamespaceId> children;
	NamespaceIndex named_children;
	EntityBucketIndex external_entities;
	SourceEntityBucketIndex internal_entities;
	SourceEntityBucketIndex source_entities;
	SourceNamespaceIndex namespace_aliases;
	SourceTypeIndex aliases;
	SourceEntityBucketIndex using_entities;
	SourceTypeIndex using_types;
	std::vector<NamespaceId> lookup_children;
	TranslationUnitRangeIndex lookup_child_ranges;
	std::vector<NamespaceId> lookup_directive_targets;
	TranslationUnitRangeIndex lookup_directive_ranges;

	NamespaceRecord(NamespaceId id = NamespaceId(),
		NamespaceId parent = NamespaceId(), NameId name = NameId(),
		bool anonymous = false, bool inline_namespace = false,
		bool internal_scope = false, std::size_t translation_unit = 0)
		: id(id), parent(parent), name(name), anonymous(anonymous),
		  inline_namespace(inline_namespace), internal_scope(internal_scope),
		  translation_unit(translation_unit),
		  last_declaration_translation_unit(
			std::numeric_limits<std::size_t>::max()),
		  variables(), functions(),
		  children(), named_children(), external_entities(), internal_entities(),
		  source_entities(), namespace_aliases(), aliases(), using_entities(),
		  using_types(), lookup_children(), lookup_child_ranges(),
		  lookup_directive_targets(), lookup_directive_ranges()
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

enum class NamespaceDeclarationKind
{
	Definition,
	Alias
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


struct SemanticCore
{
	const PPTokenBuffer* input;
	std::size_t current_translation_unit;
	std::vector<std::string> name_texts;
	SpellingIndex names_by_spelling;
	NameTextIndex names_by_text;
	std::vector<TypeRecord> types;
	CanonicalTypeIndex canonical_types;
	std::vector<NamespaceRecord> namespaces;
	std::vector<EntityRecord> entities;
	std::vector<std::vector<EntityId> > entity_buckets;
	NamespaceId global;

	struct LookupFrame
	{
		NamespaceId id;
		std::size_t child_index;
		std::size_t child_begin;
		std::size_t child_count;
		std::size_t directive_index;
		std::size_t directive_begin;
		std::size_t directive_count;
		bool entered;

		LookupFrame(NamespaceId id = NamespaceId())
			: id(id), child_index(0), child_begin(0), child_count(0),
			  directive_index(0), directive_begin(0), directive_count(0),
			  entered(false)
		{}
	};

	mutable std::vector<std::uint32_t> lookup_marks;
	mutable std::uint32_t lookup_generation;
	mutable std::vector<LookupFrame> lookup_work;
	mutable std::vector<std::uint32_t> entity_lookup_namespace_marks;
	mutable std::vector<std::uint32_t> entity_lookup_entity_marks;
	mutable std::uint32_t entity_lookup_generation;
	mutable std::vector<NamespaceId> entity_lookup_work;
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

	SemanticCore()
		: input(NULL), current_translation_unit(0), name_texts(),
		  names_by_spelling(), names_by_text(), types(), canonical_types(),
		  namespaces(), entities(), entity_buckets(), global(), lookup_marks(),
		  lookup_generation(0), lookup_work(), entity_lookup_namespace_marks(),
		  entity_lookup_entity_marks(), entity_lookup_generation(0),
		  entity_lookup_work(), namespace_cache(), type_cache(), entity_cache(),
		  namespace_epoch(1), type_epoch(1), entity_epoch(1)
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

	void begin_translation_unit(const PPTokenBuffer* buffer,
		std::size_t translation_unit)
	{
		input = buffer;
		current_translation_unit = translation_unit;
		names_by_spelling.clear();
		namespace_cache.clear();
		type_cache.clear();
		entity_cache.clear();
		lookup_marks.clear();
		lookup_work.clear();
		lookup_generation = 0;
		// Entity lookup marks are reusable across TUs.  Do not reset their
		// generation here: begin_entity_lookup() advances it before the first
		// later-TU query and owns the wraparound clear of both mark arrays.
		entity_lookup_work.clear();
	}

	void end_translation_unit()
	{
		input = NULL;
	}

	NameId intern_text(const std::string& text)
	{
		const NameId* found = names_by_text.find(text);
		if (found != NULL)
			return *found;
		const NameId result(name_texts.size());
		name_texts.push_back(text);
		names_by_text.set(text, result);
		return result;
	}

	NameId intern_name(PPSpellingId spelling)
	{
		if (input == NULL)
			throw std::runtime_error("semantic name intern without a token stream");
		const NameId* found = names_by_spelling.find(spelling);
		if (found != NULL)
			return *found;
		const NameId result = intern_text(input->spellings.get(spelling));
		names_by_spelling.set(spelling, result);
		return result;
	}

	bool lookup_name(PPSpellingId spelling, NameId* result) const
	{
		const NameId* found = names_by_spelling.find(spelling);
		if (found != NULL)
		{
			*result = *found;
			return true;
		}
		if (input == NULL)
			return false;
		const NameId* by_text = names_by_text.find(input->spellings.get(spelling));
		if (by_text == NULL)
			return false;
		*result = *by_text;
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
		if (!name.valid() || name.value >= name_texts.size())
			throw std::runtime_error("invalid PA7 name identity");
		return name_texts[name.value];
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

	TypeId remove_top_cv(TypeId type) const
	{
		if (type_kind(type) == TypeKind::Cv)
			return types[type.value].key.child;
		if (type_kind(type) == TypeKind::Pointer &&
			types[type.value].key.cv != 0)
			return const_cast<SemanticCore*>(this)->pointer(
				types[type.value].key.child, 0);
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

	void append_lookup_child(NamespaceRecord& scope, NamespaceId child)
	{
		OccurrenceRange* range = scope.lookup_child_ranges.find(
			current_translation_unit);
		if (range == NULL)
		{
			scope.lookup_child_ranges.set(current_translation_unit,
				OccurrenceRange(scope.lookup_children.size(), 0));
			range = scope.lookup_child_ranges.find(current_translation_unit);
		}
		scope.lookup_children.push_back(child);
		++range->count;
	}

	void append_lookup_directive(NamespaceRecord& scope, NamespaceId target)
	{
		OccurrenceRange* range = scope.lookup_directive_ranges.find(
			current_translation_unit);
		if (range == NULL)
		{
			scope.lookup_directive_ranges.set(current_translation_unit,
				OccurrenceRange(scope.lookup_directive_targets.size(), 0));
			range = scope.lookup_directive_ranges.find(current_translation_unit);
		}
		scope.lookup_directive_targets.push_back(target);
		++range->count;
	}

	NamespaceId create_namespace(NamespaceId parent, NameId name,
		bool anonymous, bool inline_namespace)
	{
		const NamespaceId id(namespaces.size());
		namespaces.push_back(NamespaceRecord(id, parent, name, anonymous,
			inline_namespace, id.value != 0 &&
			(anonymous || namespaces[parent.value].internal_scope),
			current_translation_unit));
		namespaces.back().last_declaration_translation_unit =
			current_translation_unit;
		if (id.value != 0)
		{
			namespaces[parent.value].children.push_back(id);
			if (anonymous || inline_namespace)
				append_lookup_child(namespaces[parent.value], id);
		}
		return id;
	}

	NamespaceId named_namespace(NamespaceId parent, NameId name,
		bool inline_namespace)
	{
		if (namespace_name_conflicts(parent, name,
			NamespaceDeclarationKind::Definition))
			throw std::runtime_error("namespace name conflicts with declaration");
		NamespaceRecord& scope = namespaces[parent.value];
		const NamespaceId* found = scope.named_children.find(name);
		if (found != NULL)
		{
			NamespaceRecord& existing = namespaces[found->value];
			if (inline_namespace && !existing.inline_namespace)
				throw std::runtime_error("inline namespace reopening mismatch");
			existing.last_declaration_translation_unit =
				current_translation_unit;
			if (existing.inline_namespace)
				append_lookup_child(scope, *found);
			invalidate_topology();
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
		const OccurrenceRange* range = scope.lookup_child_ranges.find(
			current_translation_unit);
		if (range != NULL)
		{
			for (std::size_t i = 0; i < range->count; ++i)
			{
				const NamespaceId id = scope.lookup_children[range->begin + i];
				const NamespaceRecord& existing = namespaces[id.value];
				if (!existing.anonymous)
					continue;
				if (inline_namespace != existing.inline_namespace)
					throw std::runtime_error("inline namespace reopening mismatch");
				return id;
			}
		}
		const NamespaceId result = create_namespace(parent, NameId(), true,
			inline_namespace);
		invalidate_topology();
		return result;
	}

	bool namespace_visible(NamespaceId id) const
	{
		if (!id.valid() || id.value >= namespaces.size())
			return false;
		if (id.value == global.value)
			return true;
		const NamespaceRecord& record = namespaces[id.value];
		return record.last_declaration_translation_unit ==
			current_translation_unit;
	}

	bool namespace_name_declared_here(NamespaceId scope, NameId name) const
	{
		const NamespaceRecord& record = namespaces[scope.value];
		const NamespaceId* direct = record.named_children.find(name);
		const SourceNameKey source_key(name, current_translation_unit);
		return (direct != NULL && namespace_visible(*direct)) ||
			record.namespace_aliases.find(source_key) != NULL;
	}

	bool namespace_name_conflicts(NamespaceId scope, NameId name,
		NamespaceDeclarationKind declaration_kind) const
	{
		const NamespaceRecord& record = namespaces[scope.value];
		const SourceNameKey source_key(name, current_translation_unit);
		if (record.source_entities.find(source_key) != NULL ||
			record.aliases.find(source_key) != NULL ||
			record.using_entities.find(source_key) != NULL ||
			record.using_types.find(source_key) != NULL)
			return true;

		if (declaration_kind == NamespaceDeclarationKind::Definition)
			// Repeated namespace aliases are a PA7 compatibility case; a
			// namespace definition must nevertheless not take that name.
			return record.namespace_aliases.find(source_key) != NULL;

		// Same-TU namespace-alias duplicates are validated by the typed writer;
		// a directly declared namespace is not an alias occupant here.
		const NamespaceId* direct = record.named_children.find(name);
		return direct != NULL && namespace_visible(*direct);
	}

	EntityBucketId ensure_source_entity_bucket(NamespaceId scope, NameId name,
		std::size_t translation_unit, bool using_entity = false)
	{
		SourceEntityBucketIndex& index = using_entity ?
			namespaces[scope.value].using_entities :
			namespaces[scope.value].source_entities;
		const SourceNameKey key(name, translation_unit);
		EntityBucketId* found = index.find(key);
		if (found != NULL)
			return *found;
		const EntityBucketId result(entity_buckets.size());
		entity_buckets.push_back(std::vector<EntityId>());
		index.set(key, result);
		return result;
	}

	bool direct_entity_here(NamespaceId scope, NameId name) const
	{
		if (!scope.valid() || scope.value >= namespaces.size())
			return false;
		return namespaces[scope.value].source_entities.find(
			SourceNameKey(name, current_translation_unit)) != NULL;
	}

	bool using_entity_conflicts_with_declaration(NamespaceId scope, NameId name,
		TypeId type, bool is_function) const
	{
		const SourceEntityBucketIndex& index =
			namespaces[scope.value].using_entities;
		const EntityBucketId* found = index.find(
			SourceNameKey(name, current_translation_unit));
		if (found == NULL)
			return false;
		const std::vector<EntityId>& candidates =
			entity_buckets[found->value];
		for (std::vector<EntityId>::const_iterator it = candidates.begin();
			it != candidates.end(); ++it)
		{
			const EntityRecord& candidate = entities[it->value];
			if (!is_function || candidate.kind != EntityKind::Function ||
				candidate.type.value == type.value)
				return true;
		}
		return false;
	}

	bool using_entity_conflicts_here(NamespaceId scope, NameId name,
		EntityId entity) const
	{
		const EntityRecord& incoming = entities[entity.value];
		const NamespaceRecord& record = namespaces[scope.value];
		const SourceNameKey source_key(name, current_translation_unit);

		const EntityBucketId* direct_bucket =
			record.source_entities.find(source_key);
		if (direct_bucket != NULL)
		{
			const std::vector<EntityId>& candidates =
				entity_buckets[direct_bucket->value];
			for (std::vector<EntityId>::const_iterator it = candidates.begin();
				it != candidates.end(); ++it)
			{
				const EntityRecord& candidate = entities[it->value];
				if (it->value == entity.value)
					continue;
				if (incoming.kind != EntityKind::Function ||
					candidate.kind != EntityKind::Function ||
					candidate.type.value == incoming.type.value)
					return true;
			}
		}

		const EntityBucketId* imported_bucket =
			record.using_entities.find(source_key);
		if (imported_bucket != NULL)
		{
			const std::vector<EntityId>& candidates =
				entity_buckets[imported_bucket->value];
			for (std::vector<EntityId>::const_iterator it = candidates.begin();
				it != candidates.end(); ++it)
			{
				const EntityRecord& candidate = entities[it->value];
				if (it->value == entity.value)
					continue;
				// N3485 7.3.3p14 permits multiple using-declarations
				// to introduce functions with the same signature. Distinct
				// non-functions, and a function/non-function mix, conflict.
				if (incoming.kind != EntityKind::Function ||
					candidate.kind != EntityKind::Function)
					return true;
			}
		}
		return false;
	}

	void mark_entity_declaration(EntityId id)
	{
		if (!id.valid() || id.value >= entities.size())
			throw std::runtime_error("invalid entity declaration occurrence");
		EntityRecord& entity = entities[id.value];
		if (entity.last_declaration_translation_unit ==
			current_translation_unit)
			return;
		entity.last_declaration_translation_unit = current_translation_unit;
		const EntityBucketId bucket = ensure_source_entity_bucket(entity.owner,
			entity.name, current_translation_unit);
		entity_buckets[bucket.value].push_back(id);
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

	void begin_entity_lookup() const
	{
		if (entity_lookup_namespace_marks.size() < namespaces.size())
			entity_lookup_namespace_marks.resize(namespaces.size(), 0);
		if (entity_lookup_entity_marks.size() < entities.size())
			entity_lookup_entity_marks.resize(entities.size(), 0);
		++entity_lookup_generation;
		if (entity_lookup_generation == 0)
		{
			std::fill(entity_lookup_namespace_marks.begin(),
				entity_lookup_namespace_marks.end(), 0);
			std::fill(entity_lookup_entity_marks.begin(),
				entity_lookup_entity_marks.end(), 0);
			entity_lookup_generation = 1;
		}
		entity_lookup_work.clear();
	}

	bool mark_entity_lookup_namespace(NamespaceId id) const
	{
		if (!id.valid() || id.value >= namespaces.size())
			return false;
		if (entity_lookup_namespace_marks[id.value] ==
			entity_lookup_generation)
			return false;
		entity_lookup_namespace_marks[id.value] = entity_lookup_generation;
		return true;
	}

	bool mark_entity_lookup_entity(EntityId id) const
	{
		if (!id.valid() || id.value >= entities.size())
			return false;
		if (entity_lookup_entity_marks[id.value] ==
			entity_lookup_generation)
			return false;
		entity_lookup_entity_marks[id.value] = entity_lookup_generation;
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
				const SourceNameKey source_key(name,
					current_translation_unit);
				const OccurrenceRange* child_range =
					record.lookup_child_ranges.find(current_translation_unit);
				if (child_range != NULL)
				{
					frame.child_begin = child_range->begin;
					frame.child_count = child_range->count;
				}
				const OccurrenceRange* directive_range =
					record.lookup_directive_ranges.find(
						current_translation_unit);
				if (directive_range != NULL)
				{
					frame.directive_begin = directive_range->begin;
					frame.directive_count = directive_range->count;
				}
#ifdef PA7_AUDIT_COUNTERS
				++const_cast<SemanticCore*>(this)->lookup_namespace_visits;
#endif
				if (category == LookupCategory::Namespace)
				{
					const NamespaceId* direct =
						record.named_children.find(name);
					if (direct != NULL && namespace_visible(*direct))
						return LookupResult::namespace_result(*direct);
					const NamespaceId* alias =
						record.namespace_aliases.find(source_key);
					if (alias != NULL && namespace_visible(*alias))
						return LookupResult::namespace_result(*alias);
				}
				else if (category == LookupCategory::Type)
				{
					const TypeId* direct = record.aliases.find(source_key);
					if (direct != NULL)
						return LookupResult::type_result(*direct);
					const TypeId* imported = record.using_types.find(source_key);
					if (imported != NULL)
						return LookupResult::type_result(*imported);
				}
				else
				{
					const EntityBucketId* direct =
						record.source_entities.find(source_key);
					if (direct != NULL)
					{
						const std::vector<EntityId>& candidates =
							entity_buckets[direct->value];
						for (std::vector<EntityId>::const_iterator it =
							candidates.begin(); it != candidates.end(); ++it)
							return LookupResult::entity_result(*it);
					}
					const EntityBucketId* imported =
						record.using_entities.find(source_key);
					if (imported != NULL)
					{
						const std::vector<EntityId>& candidates =
							entity_buckets[imported->value];
						for (std::vector<EntityId>::const_iterator it =
							candidates.begin(); it != candidates.end(); ++it)
							return LookupResult::entity_result(*it);
					}
				}
			}

			if (frame.child_index < frame.child_count)
			{
				const NamespaceId child_id =
					record.lookup_children[frame.child_begin +
					frame.child_index++];
				if (namespace_visible(child_id) &&
					mark_lookup_namespace(child_id))
					lookup_work.push_back(LookupFrame(child_id));
				continue;
			}
			if (frame.directive_index < frame.directive_count)
			{
				const NamespaceId target = record.lookup_directive_targets[
					frame.directive_begin + frame.directive_index++];
				if (namespace_visible(target) && mark_lookup_namespace(target))
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
		++const_cast<SemanticCore*>(this)->lookup_queries;
	#endif
		const LookupCacheKey key(scope, name, category,
			LookupMode::InNamespace, epoch_for(category));
		const LookupResult* cached = cache_for(category).find(key);
		if (cached != NULL)
		{
	#ifdef PA7_AUDIT_COUNTERS
			++const_cast<SemanticCore*>(this)->lookup_cache_hits;
	#endif
			return *cached;
		}
	#ifdef PA7_AUDIT_COUNTERS
		++const_cast<SemanticCore*>(this)->lookup_cache_misses;
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
		++const_cast<SemanticCore*>(this)->lookup_queries;
	#endif
		const LookupCacheKey key(start, name, category,
			LookupMode::Unqualified, epoch_for(category));
		const LookupResult* cached = cache_for(category).find(key);
		if (cached != NULL)
		{
	#ifdef PA7_AUDIT_COUNTERS
			++const_cast<SemanticCore*>(this)->lookup_cache_hits;
	#endif
			return *cached;
		}
	#ifdef PA7_AUDIT_COUNTERS
		++const_cast<SemanticCore*>(this)->lookup_cache_misses;
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

	bool lookup_namespace_here(NamespaceId start, NameId name,
		NamespaceId* result) const
	{
		const LookupResult found = lookup_in_namespace(start, name,
			LookupCategory::Namespace);
		if (!found.found())
			return false;
		*result = found.namespace_id;
		return true;
	}

	bool lookup_namespace(const QualifiedName& path, NamespaceId start,
		NamespaceId* result) const
	{
		const LookupResult found = lookup_path(path, start,
			LookupCategory::Namespace);
		if (!found.found())
			return false;
		*result = found.namespace_id;
		return true;
	}

	bool lookup_type_here(NamespaceId start, NameId name, TypeId* result) const
	{
		const LookupResult found = lookup_in_namespace(start, name,
			LookupCategory::Type);
		if (!found.found())
			return false;
		*result = found.type;
		return true;
	}

	bool lookup_type(const QualifiedName& path, NamespaceId start,
		TypeId* result) const
	{
		const LookupResult found = lookup_path(path, start,
			LookupCategory::Type);
		if (!found.found())
			return false;
		*result = found.type;
		return true;
	}

	bool accepts_lookup(const CppSyntaxQualifiedName& source,
		NamespaceId start, LookupCategory category) const
	{
		QualifiedName path;
		if (!make_lookup_name(source, &path))
			return false;
		return lookup_path(path, start, category).found();
	}

	bool accepts_type_name(const CppSyntaxQualifiedName& source,
		NamespaceId start) const
	{
		return accepts_lookup(source, start, LookupCategory::Type);
	}

	bool accepts_namespace_name(const CppSyntaxQualifiedName& source,
		NamespaceId start) const
	{
		return accepts_lookup(source, start, LookupCategory::Namespace);
	}

	bool accepts_nested_name_specifier(
		const CppSyntaxQualifiedName& source, NamespaceId start) const
	{
		if (source.global)
			return true;
		if (source.components.size() < 2)
			return false;
		CppSyntaxQualifiedName prefix;
		prefix.components.assign(source.components.begin(),
			source.components.end() - 1);
		return accepts_namespace_name(prefix, start);
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
		const SourceNameKey source_key(name, current_translation_unit);
		const TypeId* imported_type = record.using_types.find(source_key);
		if (namespace_name_declared_here(scope, name) ||
			direct_entity_here(scope, name) ||
			record.using_entities.find(source_key) != NULL ||
			(imported_type != NULL && imported_type->value != type.value))
			throw std::runtime_error("typedef conflicts with value declaration");
		record.aliases.set(source_key, type);
		invalidate(LookupCategory::Type);
	}

	void declare_type_alias(NamespaceId scope, NameId name, TypeId type)
	{
		declare_alias(scope, name, type);
	}

	void declare_namespace_alias(NamespaceId scope, NameId name,
		NamespaceId target)
	{
		if (namespace_name_conflicts(scope, name,
			NamespaceDeclarationKind::Alias))
			throw std::runtime_error("namespace alias misuse");
		NamespaceRecord& record = namespaces[scope.value];
		const SourceNameKey source_key(name, current_translation_unit);
		const NamespaceId* existing = record.namespace_aliases.find(source_key);
		if (existing != NULL && existing->value != target.value)
			throw std::runtime_error("namespace alias redefinition");
		record.namespace_aliases.set(source_key, target);
		invalidate_topology();
	}

	void add_using_directive(NamespaceId scope, NamespaceId target)
	{
		append_lookup_directive(namespaces[scope.value], target);
		invalidate_topology();
	}

	void add_using_type(NamespaceId scope, NameId name, TypeId type)
	{
		NamespaceRecord& record = namespaces[scope.value];
		const SourceNameKey source_key(name, current_translation_unit);
		const TypeId* direct_type = record.aliases.find(source_key);
		const TypeId* imported_type = record.using_types.find(source_key);
		if (namespace_name_declared_here(scope, name) ||
			direct_entity_here(scope, name) ||
			record.using_entities.find(source_key) != NULL ||
			(direct_type != NULL && direct_type->value != type.value) ||
			(imported_type != NULL && imported_type->value != type.value))
			throw std::runtime_error("using declaration conflicts with namespace");
		record.using_types.set(source_key, type);
		invalidate(LookupCategory::Type);
	}

	EntityBucketId ensure_external_entity_bucket(NamespaceId scope,
		NameId name)
	{
		EntityBucketIndex& index = namespaces[scope.value].external_entities;
		EntityBucketId* found = index.find(name);
		if (found != NULL)
			return *found;
		const EntityBucketId result(entity_buckets.size());
		entity_buckets.push_back(std::vector<EntityId>());
		index.set(name, result);
		return result;
	}

	EntityBucketId ensure_internal_entity_bucket(NamespaceId scope, NameId name,
		std::size_t translation_unit)
	{
		SourceEntityBucketIndex& index =
			namespaces[scope.value].internal_entities;
		const SourceNameKey key(name, translation_unit);
		EntityBucketId* found = index.find(key);
		if (found != NULL)
			return *found;
		const EntityBucketId result(entity_buckets.size());
		entity_buckets.push_back(std::vector<EntityId>());
		index.set(key, result);
		return result;
	}

	std::vector<EntityId> link_candidates(NamespaceId scope, NameId name) const
	{
		std::vector<EntityId> result;
		const NamespaceRecord& record = namespaces[scope.value];
		const EntityBucketId* external = record.external_entities.find(name);
		if (external != NULL)
			result.insert(result.end(), entity_buckets[external->value].begin(),
				entity_buckets[external->value].end());
		const EntityBucketId* internal = record.internal_entities.find(
			SourceNameKey(name, current_translation_unit));
		if (internal != NULL)
			result.insert(result.end(), entity_buckets[internal->value].begin(),
				entity_buckets[internal->value].end());
		return result;
	}

	void add_using_entity(NamespaceId scope, NameId name, EntityId entity)
	{
		NamespaceRecord& record = namespaces[scope.value];
		const SourceNameKey source_key(name, current_translation_unit);
		if (namespace_name_declared_here(scope, name) ||
			record.aliases.find(source_key) != NULL ||
			record.using_types.find(source_key) != NULL)
			throw std::runtime_error("using declaration conflicts with namespace");
		if (using_entity_conflicts_here(scope, name, entity))
			throw std::runtime_error("using declaration entity conflict");
		const EntityBucketId bucket_id = ensure_source_entity_bucket(scope, name,
			current_translation_unit, true);
		std::vector<EntityId>& bucket = entity_buckets[bucket_id.value];
		if (std::find_if(bucket.begin(), bucket.end(),
			[entity](EntityId value) { return value.value == entity.value; }) ==
			bucket.end())
			bucket.push_back(entity);
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
		const SourceNameKey source_key(name, current_translation_unit);
		if (namespace_name_declared_here(scope, name) ||
			record.aliases.find(source_key) != NULL ||
			record.using_types.find(source_key) != NULL ||
			using_entity_conflicts_with_declaration(scope, name,
				type,
				is_function))
			throw std::runtime_error("declaration conflicts with namespace");
		const EntityKind kind = is_function ? EntityKind::Function :
			EntityKind::Variable;
		EntityBucketId* found = record.external_entities.find(name);
		if (found != NULL && !entity_buckets[found->value].empty())
		{
			EntityRecord& existing = entities[entity_buckets[found->value][0].value];
			if (existing.kind != kind)
				throw std::runtime_error("declaration kind conflict");
			existing.type = merge_types(existing.type, type);
			mark_entity_declaration(EntityId(
				entity_buckets[found->value][0].value));
			invalidate(LookupCategory::Entity);
			return;
		}
		const EntityId id(entities.size());
		EntityRecord entity;
		entity.kind = kind;
		entity.name = name;
		entity.type = type;
		entity.owner = scope;
		entities.push_back(entity);
		entity_buckets[ensure_external_entity_bucket(scope, name).value].push_back(
			id);
		if (is_function)
			record.functions.push_back(id);
		else
			record.variables.push_back(id);
		mark_entity_declaration(id);
		invalidate(LookupCategory::Entity);
	}

	std::vector<EntityId> lookup_entities_here(NamespaceId scope,
		NameId name) const
	{
		std::vector<EntityId> result;
		if (!scope.valid() || scope.value >= namespaces.size() ||
			!namespace_visible(scope))
			return result;
		begin_entity_lookup();
		entity_lookup_work.push_back(scope);
		while (!entity_lookup_work.empty())
		{
			const NamespaceId current = entity_lookup_work.back();
			entity_lookup_work.pop_back();
			if (!mark_entity_lookup_namespace(current))
				continue;
			const NamespaceRecord& record = namespaces[current.value];
			const SourceNameKey source_key(name, current_translation_unit);
			const EntityBucketId* direct =
				record.source_entities.find(source_key);
			if (direct != NULL)
				for (std::vector<EntityId>::const_iterator it =
					entity_buckets[direct->value].begin();
					it != entity_buckets[direct->value].end(); ++it)
					if (mark_entity_lookup_entity(*it))
						result.push_back(*it);
			const EntityBucketId* imported =
				record.using_entities.find(source_key);
			if (imported != NULL)
				for (std::vector<EntityId>::const_iterator it =
					entity_buckets[imported->value].begin();
					it != entity_buckets[imported->value].end(); ++it)
					if (mark_entity_lookup_entity(*it))
						result.push_back(*it);
			const OccurrenceRange* directive_range =
				record.lookup_directive_ranges.find(current_translation_unit);
			if (directive_range != NULL)
				for (std::size_t i = directive_range->count; i != 0; --i)
				{
					const NamespaceId target = record.lookup_directive_targets[
						directive_range->begin + i - 1];
					if (namespace_visible(target))
						entity_lookup_work.push_back(target);
				}
			// The worklist is LIFO: directives are pushed first so child
			// namespaces are popped and visited before using-directive targets.
			const OccurrenceRange* child_range =
				record.lookup_child_ranges.find(current_translation_unit);
			if (child_range != NULL)
				for (std::size_t i = child_range->count; i != 0; --i)
				{
					const NamespaceId child = record.lookup_children[
						child_range->begin + i - 1];
					if (namespace_visible(child))
						entity_lookup_work.push_back(child);
				}
		}
		return result;
	}

	std::vector<EntityId> lookup_entities(const QualifiedName& path,
		NamespaceId start) const
	{
		if (path.components.empty())
			return std::vector<EntityId>();
		if (path.components.size() == 1 && !path.global)
		{
			NamespaceId scope = start;
			while (scope.valid())
			{
				std::vector<EntityId> found =
					lookup_entities_here(scope, path.components[0]);
				if (!found.empty())
					return found;
				if (scope.value == global.value)
					break;
				scope = namespaces[scope.value].parent;
			}
			return std::vector<EntityId>();
		}
		if (path.components.size() == 1 && path.global)
			return lookup_entities_here(global, path.components[0]);
		QualifiedName prefix = path;
		const NameId last = prefix.components.back();
		prefix.components.pop_back();
		NamespaceId scope = resolve_namespace_path(prefix, start);
		return lookup_entities_here(scope, last);
	}

	bool is_enclosing_namespace(NamespaceId current, NamespaceId target) const
	{
		NamespaceId scope = current;
		while (scope.valid())
		{
			if (scope.value == target.value)
				return true;
			if (scope.value == global.value)
				break;
			scope = namespaces[scope.value].parent;
		}
		return false;
	}

	bool is_const_qualified(TypeId type) const
	{
		return type_kind(type) == TypeKind::Cv &&
			(types[type.value].key.cv & 1u) != 0;
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
			namespace_slots += record.external_entities.slot_count();
			namespace_slots += record.internal_entities.slot_count();
			namespace_slots += record.source_entities.slot_count();
			namespace_slots += record.namespace_aliases.slot_count();
			namespace_slots += record.aliases.slot_count();
			namespace_slots += record.using_entities.slot_count();
			namespace_slots += record.using_types.slot_count();
			namespace_slots += record.lookup_child_ranges.slot_count();
			namespace_slots += record.lookup_directive_ranges.slot_count();
			namespace_entries += record.named_children.entry_count();
			namespace_entries += record.external_entities.entry_count();
			namespace_entries += record.internal_entities.entry_count();
			namespace_entries += record.source_entities.entry_count();
			namespace_entries += record.namespace_aliases.entry_count();
			namespace_entries += record.aliases.entry_count();
			namespace_entries += record.using_entities.entry_count();
			namespace_entries += record.using_types.entry_count();
			namespace_entries += record.lookup_child_ranges.entry_count();
			namespace_entries += record.lookup_directive_ranges.entry_count();
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
			<< " names=" << name_texts.size()
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

} // namespace CppSemantic
