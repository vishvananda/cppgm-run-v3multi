#pragma once

#include <ostream>

#include "pa10_ast.h"

// Consume the typed PA10 syntax owner and render one deterministic PA11
// semantic dump.  The implementation owns scopes, bindings, lookup, and
// canonical type identities; the PA10 renderer is never used as input.
void emit_pa11_types(const PA10Ast& ast, std::ostream& output);

// Extend the same PA11 owner with resolved expression, conversion, and call
// facts.  The renderer is a cold output boundary; it is never parsed back.
void emit_pa12_semantics(const PA10Ast& ast, std::ostream& output);
