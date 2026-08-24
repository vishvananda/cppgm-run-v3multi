#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace pa11_semantic_storage
{
static const std::size_t InvalidIdentityValue =
	std::numeric_limits<std::size_t>::max();

template <typename Domain>
struct DomainId
{
	std::size_t value;

	explicit DomainId(std::size_t value = InvalidIdentityValue) : value(value) {}

	bool valid() const { return value != InvalidIdentityValue; }
	bool operator==(const DomainId& other) const { return value == other.value; }
	bool operator!=(const DomainId& other) const { return value != other.value; }
	bool operator<(const DomainId& other) const { return value < other.value; }
};

struct NameDomain;
struct TypeDomain;
struct NamedRecordDomain;
struct ScopeDomain;
struct BindingDomain;
struct DumpBindingViewDomain;
struct DumpScopeViewDomain;
struct DeclarationFactDomain;
struct FunctionFactDomain;
struct NamespaceFactDomain;
struct CompoundFactDomain;
struct SemanticFactDomain;
struct ConversionFactDomain;

typedef DomainId<NameDomain> NameId;
typedef DomainId<TypeDomain> TypeId;
typedef DomainId<NamedRecordDomain> NamedRecordId;
typedef DomainId<ScopeDomain> ScopeId;
typedef DomainId<BindingDomain> BindingId;
typedef DomainId<DumpBindingViewDomain> DumpBindingViewId;
typedef DomainId<DumpScopeViewDomain> DumpScopeViewId;
typedef DomainId<DeclarationFactDomain> DeclarationFactId;
typedef DomainId<FunctionFactDomain> FunctionFactId;
typedef DomainId<NamespaceFactDomain> NamespaceFactId;
typedef DomainId<CompoundFactDomain> CompoundFactId;
typedef DomainId<SemanticFactDomain> SemanticFactId;
typedef DomainId<ConversionFactDomain> ConversionFactId;

struct ArrayBound
{
	std::size_t value;

	explicit ArrayBound(std::size_t value = 0) : value(value) {}

	bool operator==(const ArrayBound& other) const { return value == other.value; }
};

template <typename Key>
struct IdentityHash
{
	std::size_t operator()(const Key& key) const
	{
		std::size_t result = key.value;
		result ^= result >> 17;
		result *= static_cast<std::size_t>(0xed5ad4bbU);
		result ^= result >> 11;
		return result;
	}
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

struct PointerHash
{
	template <typename Value>
	std::size_t operator()(const Value* value) const
	{
		std::size_t result = reinterpret_cast<std::size_t>(value);
		result ^= result >> 17;
		result *= static_cast<std::size_t>(0xed5ad4bbU);
		result ^= result >> 11;
		return result;
	}
};

// Entries retain insertion order while the probe table stays compact and
// contiguous.  No scope or type record owns a node-based map.
template <typename Key, typename Value, typename Hash>
class FlatIndex
{
	struct Entry
	{
		Key key;
		Value value;

		Entry(const Key& key, const Value& value) : key(key), value(value) {}
	};

	std::vector<std::size_t> slots_;
	std::vector<Entry> entries_;

	static std::size_t empty_slot()
	{
		return InvalidIdentityValue;
	}

	static bool equal_key(const Key& left, const Key& right)
	{
		return left == right;
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
		throw std::runtime_error("PA11 flat index probe exhausted");
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
		throw std::runtime_error("PA11 flat index rehash exhausted");
	}

	void rehash(std::size_t capacity)
	{
		if (!power_of_two(capacity) || capacity < 8 ||
			capacity > InvalidIdentityValue / sizeof(std::size_t))
			throw std::runtime_error("PA11 flat index capacity overflow");
		std::vector<std::size_t> slots(capacity, empty_slot());
		slots_.swap(slots);
		for (std::size_t i = 0; i < entries_.size(); ++i)
			slots_[slot_for_entry(i)] = i;
	}

	void ensure_capacity()
	{
		if (entries_.size() == InvalidIdentityValue)
			throw std::runtime_error("PA11 flat index entry overflow");
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
			if (capacity > InvalidIdentityValue / 2)
				throw std::runtime_error("PA11 flat index growth overflow");
			rehash(capacity * 2);
		}
	}

public:
	FlatIndex() : slots_(), entries_() {}

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
			static_cast<const FlatIndex*>(this)->find(key));
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
		slots_[slot_for_key(key)] = entry;
	}
};

enum class LookupGraphKind
{
	Namespace,
	Type,
	Value
};

struct LookupFrame
{
	LookupGraphKind kind;
	ScopeId scope;
	std::size_t next_using;
	std::size_t next_inline_child;
	bool entered;

	LookupFrame(LookupGraphKind kind, ScopeId scope)
		: kind(kind), scope(scope), next_using(0), next_inline_child(0),
		  entered(false)
	{}
};
} // namespace pa11_semantic_storage
