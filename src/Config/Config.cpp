#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

Config::Config(const std::string &path) {
    std::ifstream file(path.c_str());
    if (!file.is_open())
        throw std::runtime_error("Cannot open config file: " + path);

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::vector<std::string> tokens = tokenize(buffer.str());
    parse(tokens);
}

const std::vector<ServerConfig> &Config::getServers() const {
    return servers;
}
