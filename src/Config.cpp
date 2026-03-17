#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

Config::Config(const std::string &path)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
		throw std::runtime_error("Cannot open config file: " + path);

	std::stringstream buffer;
	buffer << file.rdbuf();
	std::string content = buffer.str();
}

const std::vector<ServerConfig> &Config::getServers() const
{
	return _servers;
}
