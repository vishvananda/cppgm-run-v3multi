// (C) 2013 CPPGM Foundation www.cppgm.org.  All rights reserved.

#include <vector>
#include <string>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <sstream>
#include <ctime>
#include <cstdlib>

#include "pa6_recognizer.h"
#include "preproc_session.h"

using namespace std;

PPPreprocessConfig BuildPreprocessConfig()
{
	const time_t now = time(NULL);
	const tm* local = localtime(&now);
	const char* stamp = local == NULL ? NULL : asctime(local);
	if (stamp == NULL || string(stamp).size() < 24)
		throw runtime_error("unable to determine build date and time");
	const string value(stamp);
	return PPPreprocessConfig("Vishvananda Abrams",
		value.substr(4, 7) + value.substr(20, 4), value.substr(11, 8));
}

void DoRecog(istream& in, const string& source_path)
{
	if (!in)
		throw runtime_error("unable to open source file: " + source_path);
	ostringstream source;
	source << in.rdbuf();
	if (!in.good() && !in.eof())
		throw runtime_error("unable to read source file: " + source_path);
	PPPreprocessingSession session(BuildPreprocessConfig());
	const PPTokenBuffer& tokens = session.preprocess(source_path, source.str());
	PA6Recognizer recognizer;
	string reason;
	if (!recognizer.recognize(tokens, &reason))
		throw runtime_error(reason.empty() ? "syntax error" : reason);
};

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

		out << "recog " << nsrcfiles << endl;

		for (size_t i = 0; i < nsrcfiles; i++)
		{
			string srcfile = args[i+2];

			try
			{
				ifstream in(srcfile);
				DoRecog(in, srcfile);
				out << srcfile << " OK" << endl;
			}
			catch (const exception& e)
			{
				cerr << e.what() << endl;
				out << srcfile << " BAD" << endl;
			}
		}
		return EXIT_SUCCESS;
	}
	catch (exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
