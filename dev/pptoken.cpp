#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace std;

#include "DebugPPTokenStream.h"
#include "exceptions.h"
#include "pp_tokenizer.h"

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	try
	{
		ostringstream oss;
		oss << cin.rdbuf();

		DebugPPTokenStream output;
		tokenize_cpp_source(oss.str(), output);
		return EXIT_SUCCESS;
	}
	catch (const NotImplementedException& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return CPPGM_EXIT_NOT_IMPLEMENTED;
	}
	catch (const exception& e)
	{
		cerr << "ERROR: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
