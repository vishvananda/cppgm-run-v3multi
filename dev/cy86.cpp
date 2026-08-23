// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "cy86_backend.h"
#include "exceptions.h"

namespace
{

bool has_batch_stdin_arg(int argc, char** argv)
{
	for (int i = 1; i < argc; ++i)
		if (std::string(argv[i]) == "--batch-stdin")
			return true;
	return false;
}

std::vector<std::string> split_tabs(const std::string& line)
{
	std::vector<std::string> fields;
	std::size_t begin = 0;
	for (std::size_t at = 0; at <= line.size(); ++at)
	{
		if (at != line.size() && line[at] != '\t')
			continue;
		fields.push_back(line.substr(begin, at - begin));
		begin = at + 1;
	}
	return fields;
}

int run_batch_stdin()
{
	std::string line;
	while (std::getline(std::cin, line))
	{
		int status = EXIT_FAILURE;
		std::vector<std::string> fields = split_tabs(line);
		std::ofstream stdout_file;
		std::ofstream stderr_file;
		std::streambuf* old_stdout = std::cout.rdbuf();
		std::streambuf* old_stderr = std::cerr.rdbuf();

		try
		{
			if (fields.size() < 5)
				throw std::logic_error("invalid batch request");
			stdout_file.open(fields[0].c_str(), std::ios::out | std::ios::trunc);
			if (!stdout_file)
				throw std::runtime_error("unable to open batch stdout");
			if (fields[1] == fields[0])
			{
				std::cout.rdbuf(stdout_file.rdbuf());
				std::cerr.rdbuf(stdout_file.rdbuf());
			}
			else
			{
				stderr_file.open(fields[1].c_str(),
					std::ios::out | std::ios::trunc);
				if (!stderr_file)
					throw std::runtime_error("unable to open batch stderr");
				std::cout.rdbuf(stdout_file.rdbuf());
				std::cerr.rdbuf(stderr_file.rdbuf());
			}

			std::vector<std::string> args(fields.begin() + 4, fields.end());
			cy86_compile(args);
			status = EXIT_SUCCESS;
		}
		catch (const NotImplementedException& error)
		{
			std::cerr << "ERROR: " << error.what() << std::endl;
			status = CPPGM_EXIT_NOT_IMPLEMENTED;
		}
		catch (const std::exception& error)
		{
			std::cerr << "ERROR: " << error.what() << std::endl;
			status = EXIT_FAILURE;
		}

		std::cout.rdbuf(old_stdout);
		std::cerr.rdbuf(old_stderr);
		std::cout << (status == EXIT_SUCCESS ? "EXIT_SUCCESS" :
			status == CPPGM_EXIT_NOT_IMPLEMENTED ? "EXIT_NOT_IMPLEMENTED" :
			"EXIT_FAILURE") << std::endl;
	}
	return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv)
{
	try
	{
		if (has_batch_stdin_arg(argc, argv))
			return run_batch_stdin();

		std::vector<std::string> args;
		for (int i = 1; i < argc; ++i)
			args.push_back(argv[i]);
		cy86_compile(args);
		return EXIT_SUCCESS;
	}
	catch (const NotImplementedException& error)
	{
		std::cerr << "ERROR: " << error.what() << std::endl;
		return CPPGM_EXIT_NOT_IMPLEMENTED;
	}
	catch (const std::exception& error)
	{
		std::cerr << "ERROR: " << error.what() << std::endl;
		return EXIT_FAILURE;
	}
}
