#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "IPPTokenStream.h"

struct PPPreprocessConfig
{
	std::string author;
	std::string build_date;
	std::string build_time;

	PPPreprocessConfig(const std::string& author = std::string(),
		const std::string& build_date = std::string(),
		const std::string& build_time = std::string())
		: author(author), build_date(build_date), build_time(build_time)
	{}
};
// One instance represents one command-line source-file preprocessing
// session.  Includes are recursive within the session; the returned buffer is
// the typed phase-3 stream consumed directly by PA2.
class PPPreprocessingSession
{
public:
	explicit PPPreprocessingSession(const PPPreprocessConfig& config);
	~PPPreprocessingSession();

	const PPTokenBuffer& preprocess(const std::string& source_path,
		const std::string& source);

private:
	struct Impl;
	Impl* impl_;

	PPPreprocessingSession(const PPPreprocessingSession&);
	PPPreprocessingSession& operator=(const PPPreprocessingSession&);
};
