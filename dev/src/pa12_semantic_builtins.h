#pragma once

#include <vector>

#include "pa11_semantic_storage.h"

namespace pa11_semantic_internal
{

// PA12 semantic facts for fixed compiler-provided function boundaries.  The
// enums are deliberately independent of LowIR; PA15 maps them once at the
// typed IR boundary.
enum class BuiltinKind { None, ConstantP, Abort, Strlen, Unreachable, Memcpy, Memmove };
enum class BuiltinCallEffects { Default, ReadNone, ReadOnly, ReadWrite };
enum class BuiltinCallUnwind { Default, May, No };
enum class BuiltinCallReturn { Default, Returns, NoReturn };
enum class BuiltinParameterCapture { Default, NoCapture, MayCapture };
enum class BuiltinParameterAccess { Default, None, Read, Write, ReadWrite };
enum class BuiltinParameterAlias { Default, NoAlias };

struct BuiltinParameterFact
{
	BuiltinParameterCapture capture;
	BuiltinParameterAccess access;
	BuiltinParameterAlias alias;
	BuiltinParameterFact(
		BuiltinParameterCapture capture = BuiltinParameterCapture::Default,
		BuiltinParameterAccess access = BuiltinParameterAccess::Default,
		BuiltinParameterAlias alias = BuiltinParameterAlias::Default)
		: capture(capture), access(access), alias(alias) {}
};

// A descriptor is created only after typed semantic analysis selects its
// builtin.  BindingId is the canonical demand edge; PA15 never recovers these
// facts from source spelling.
struct BuiltinFunctionFact
{
	BuiltinKind kind;
	BindingId binding;
	NameId object_symbol;
	BuiltinCallEffects effects;
	BuiltinCallUnwind unwind;
	BuiltinCallReturn returns;
	std::vector<BuiltinParameterFact> parameters;
	BuiltinFunctionFact(BuiltinKind kind = BuiltinKind::None,
		BindingId binding = BindingId(), NameId object_symbol = NameId())
		: kind(kind), binding(binding), object_symbol(object_symbol),
		effects(BuiltinCallEffects::Default), unwind(BuiltinCallUnwind::Default),
		returns(BuiltinCallReturn::Default), parameters() {}
};

}
