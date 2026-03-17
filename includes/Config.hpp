#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>

struct RouteConfig {
  std::string path;
  std::vector<std::string> methods;
  std::string root;
  std::string index;
};

struct ServerConfig {
  std::string host;
  int port;
  std::vector<RouteConfig> routes;
};

class Config {
public:
  Config(const std::string &path);
  const std::vector<ServerConfig> &getServers() const;

private:
  std::vector<ServerConfig> _servers;
  std::vector<std::string>  tokenize(const std::string &content);
};

#endif
