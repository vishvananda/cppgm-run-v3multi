// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

#include "pa8_semantic.h"
#include "preproc_session.h"

namespace
{

string ReadSource(const string& path)
{
	ifstream input(path.c_str(), ios::in | ios::binary);
	if (!input)
		throw runtime_error("unable to open source file: " + path);
	ostringstream source;
	source << input.rdbuf();
	return source.str();
}

}

int main(int argc, char** argv)
{
	try
	{
		vector<string> args;
		for (int i = 1; i < argc; ++i)
			args.emplace_back(argv[i]);
		if (args.size() < 3 || args[0] != "-o")
			throw logic_error("invalid usage");

		const string outfile = args[1];
		PA8ProgramModel program;
		for (size_t i = 2; i < args.size(); ++i)
		{
			PPPreprocessConfig config;
			PPPreprocessingSession preprocessing(config);
			const PPTokenBuffer& tokens = preprocessing.preprocess(args[i],
				ReadSource(args[i]));
			program.add_translation_unit(tokens);
		}

		vector<char> program_image;
		program.build_image(program_image);
		ofstream out(outfile.c_str(), ios::out | ios::binary | ios::trunc);
		if (!out)
			throw runtime_error("unable to open output file: " + outfile);
		out.write(program_image.data(),
			static_cast<streamsize>(program_image.size()));
		if (!out)
			throw runtime_error("unable to write output file: " + outfile);
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
