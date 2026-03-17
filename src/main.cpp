#include <iostream>
#include "Config.hpp"

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: ./webserv <config_file>" << std::endl;
		return 1;
	}
	Config config(argv[1]);
	return 0;
}
