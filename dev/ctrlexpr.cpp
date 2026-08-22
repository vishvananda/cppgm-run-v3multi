#include <iostream>
#include "ctrlexpr.h"

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;
	return run_ctrlexpr(std::cin, std::cout, std::cerr);
}
