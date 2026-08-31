#pragma once

#include <cstdint>

#include "pa12_semantic_selection.h"

namespace pa11_semantic_internal
{
using namespace pa11_semantic_storage;

struct InheritingConstructorRelation
{
	NamedRecordId base_record;
	SourcePoint declaration_point;
};

enum class ConstructorActionTarget { Base, Member };

struct ConstructorActionFact
{
	ConstructorActionTarget target;
	NamedRecordId base_record;
	BindingId member;
	BindingId constructor;
	SemanticFactId initializer;
	std::size_t argument_begin;
	std::size_t argument_count;
	bool value_initialize;
	TypeId object_type;
	TypeId callable_type;

	ConstructorActionFact(
		ConstructorActionTarget target = ConstructorActionTarget::Member,
		NamedRecordId base_record = NamedRecordId(),
		BindingId member = BindingId(),
		BindingId constructor = BindingId(),
		SemanticFactId initializer = SemanticFactId())
		: target(target), base_record(base_record), member(member),
		  constructor(constructor), initializer(initializer),
		  argument_begin(InvalidIdentityValue), argument_count(0),
		  value_initialize(false), object_type(), callable_type()
	{}
};

struct DestructorActionFact
{
	ConstructorActionTarget target;
	NamedRecordId base_record;
	BindingId member;
	BindingId destructor;
	TypeId object_type;

	DestructorActionFact(
		ConstructorActionTarget target = ConstructorActionTarget::Member,
		NamedRecordId base_record = NamedRecordId(),
		BindingId member = BindingId(),
		BindingId destructor = BindingId(),
		TypeId object_type = TypeId())
		: target(target), base_record(base_record), member(member),
		  destructor(destructor), object_type(object_type)
	{}
};

enum class LifetimeStorageKind { Automatic, Namespace };

struct LifetimeFact
{
	BindingId object;
	TypeId object_type;
	BindingId destructor;
	ScopeId scope;
	LifetimeStorageKind storage;

	LifetimeFact(BindingId object = BindingId(),
		TypeId object_type = TypeId(), BindingId destructor = BindingId(),
		ScopeId scope = ScopeId(),
		LifetimeStorageKind storage = LifetimeStorageKind::Automatic)
		: object(object), object_type(object_type), destructor(destructor),
		  scope(scope), storage(storage)
	{}
};

enum class ConstructorRuntimeCacheState : std::uint8_t
{
	Unseen,
	InProgress,
	Complete
};
}
