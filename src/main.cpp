#include "Config.hpp"
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./webserv <config_file>" << std::endl;
        return 1;
    }
    try {
        Config config(argv[1]);
        const std::vector<ServerConfig> &servers = config.getServers();
        for (size_t i = 0; i < servers.size(); i++) {
            std::cout << "server " << servers[i].host << ":" << servers[i].port
                      << std::endl;
            for (size_t j = 0; j < servers[i].routes.size(); j++) {
                const RouteConfig &r = servers[i].routes[j];
                std::cout << "  location " << r.path << " root=" << r.root
                          << " index=" << r.index << std::endl;
            }
        }
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
