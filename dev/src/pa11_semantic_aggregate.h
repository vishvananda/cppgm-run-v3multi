#pragma once

#include <cstdint>

#include "pa11_semantic_storage.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

enum class AggregateElementKind : std::uint8_t { ArrayElement, Member };

struct AggregateElementFact
{
	AggregateElementKind kind;
	TypeId type;
	BindingId member;
	std::size_t index;
	SemanticFactId initializer;

	AggregateElementFact(AggregateElementKind kind = AggregateElementKind::ArrayElement,
		TypeId type = TypeId(), BindingId member = BindingId(), std::size_t index = 0,
		SemanticFactId initializer = SemanticFactId())
		: kind(kind), type(type), member(member), index(index), initializer(initializer)
	{}
};

struct AggregateFactRange
{
	std::size_t begin;
	std::size_t count;
	std::size_t total_count;

	AggregateFactRange(std::size_t begin = InvalidIdentityValue, std::size_t count = 0,
		std::size_t total_count = 0)
		: begin(begin), count(count), total_count(total_count)
	{}
};
}
