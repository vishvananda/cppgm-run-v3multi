// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

#include "exceptions.h"
#include "pa7_semantic.h"
#include "preproc_session.h"

string ReadSource(const string& path)
{
	ifstream input(path.c_str(), ios::in | ios::binary);
	if (!input)
		throw runtime_error("unable to open source file: " + path);
	ostringstream source;
	source << input.rdbuf();
	return source.str();
}

int main(int argc, char** argv)
{
	try
	{
		vector<string> args;

		for (int i = 1; i < argc; i++)
			args.emplace_back(argv[i]);

		if (args.size() < 3 || args[0] != "-o")
			throw logic_error("invalid usage");

		string outfile = args[1];
		size_t nsrcfiles = args.size() - 2;

		ofstream out(outfile);
		if (!out)
			throw runtime_error("unable to open output file: " + outfile);

		out << nsrcfiles << " translation units" << endl;

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i+2];

			out << "start translation unit " << srcfile << endl;

			PPPreprocessConfig config;
			PPPreprocessingSession preprocessing(config);
			const PPTokenBuffer& tokens = preprocessing.preprocess(srcfile,
				ReadSource(srcfile));
			PA7SemanticModel model(tokens);
			model.analyze();
			model.render(out);

			out << "end translation unit" << endl;

		}
	}
	catch (const NotImplementedException& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return CPPGM_EXIT_NOT_IMPLEMENTED;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
