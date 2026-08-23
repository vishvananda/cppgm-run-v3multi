#pragma once

#include <iosfwd>
#include <vector>

#include "IPPTokenStream.h"

// PA8 owns one program model for all command-line translation units.  The
// model consumes the typed posttoken stream while a translation unit is live;
// it retains semantic identities and decoded values, not rendered source.
class PA8ProgramModel
{
public:
	struct Impl;

	PA8ProgramModel();
	~PA8ProgramModel();

	PA8ProgramModel(const PA8ProgramModel&) = delete;
	PA8ProgramModel& operator=(const PA8ProgramModel&) = delete;

	void add_translation_unit(const PPTokenBuffer& tokens);
	void build_image(std::vector<char>& image) const;

private:
	Impl* impl_;
};
