#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>

struct RouteConfig {
    std::string path;
    std::vector<std::string> allowedMethods;
    std::string documentRoot;
    std::string indexFile;
};

struct ServerConfig {
    std::string host;
    int port;
    std::vector<RouteConfig> routes;
};

class Config {
  public:
    Config(const std::string &path);
    const std::vector<ServerConfig> &getConfigs() const;

  private:
    std::vector<ServerConfig> configs;
    std::vector<std::string> tokenize(const std::string &content);
    void buildServers(const std::vector<std::string> &tokens);
    ServerConfig parseServer(const std::vector<std::string> &tokens, size_t &i);
    RouteConfig parseRoute(const std::vector<std::string> &tokens, size_t &i);
};

#endif
