#pragma once

#include <iosfwd>

#include "IPPTokenStream.h"

// Canonical PA7 forward semantic owner.  The model consumes the typed
// posttoken stream directly; it never reparses rendered token text.
class PA7SemanticModel
{
public:
	struct Impl;

	explicit PA7SemanticModel(const PPTokenBuffer& tokens);
	~PA7SemanticModel();

	PA7SemanticModel(const PA7SemanticModel&) = delete;
	PA7SemanticModel& operator=(const PA7SemanticModel&) = delete;

	void analyze();
	void render(std::ostream& output) const;

private:
	Impl* impl_;
};
