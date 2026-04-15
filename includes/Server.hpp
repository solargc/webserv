#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Config.hpp"
#include "Request.hpp"
#include <poll.h>
#include <vector>
#include <map>

class Server {
  public:
    Server(const std::vector<ServerConfig> &configs);
    ~Server();
    void run();

  private:
    std::vector<pollfd> fds;
    std::vector<Client *> clients;
	std::vector<ServerConfig> _configs;
	std::map<int, const ServerConfig*> listenFdToConfig;

    void registerFd(int fd, const ServerConfig& config);
    bool isListenSocket(int fd) const;
    void acceptClient(int listenFd);
    void readClient(Client *client);
    void removeClient(Client *client);

	bool isRequestComplete(const std::string &buf) const;
	const RouteConfig *findRoute(const Request& req, const ServerConfig* config);
	virtual bool directoryExists(const char* path);
	virtual bool fileExists(const std::string& filename);

	// methods
	void handleMethods(Request req, Client *client, const RouteConfig *route, const ServerConfig* config);
	void handleGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config);
	void handlePost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config);
	void handleDelete(Request req, Client *client, const RouteConfig *route, const ServerConfig* config);
};

#endif
