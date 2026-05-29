#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <cstddef>

struct RouteConfig {
    std::string path;
    std::vector<std::string> allowedMethods;
    std::string documentRoot;
    std::string indexFile;
	std::string uploadPath;
    bool autoindex;
    bool hasRedirect;
    int redirectCode;
    std::string redirectTarget;
    std::map<std::string, std::string> cgi;
};

struct ServerConfig {
    std::string host;
    int port;
	std::string statusDir;
    std::map<int, std::string> errorPages;
    size_t clientMaxBodySize;
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
