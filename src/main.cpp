#include <iostream>

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: ./webserv <config_file>" << std::endl;
		return 1;
	}
	std::cout << "Config: " << argv[1] << std::endl;
	return 0;
}
