#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Config.hpp"
#include "Request.hpp"
#include <poll.h>
#include <vector>

class Server {
  public:
    Server(const std::vector<ServerConfig> &configs);
    ~Server();
    void run();

  private:
    std::vector<pollfd> fds;
    std::vector<int> listenFds;
    std::vector<Client *> clients;

	std::vector<ServerConfig> _configs;

    void registerFd(int fd);
    bool isListenSocket(int fd) const;
    void acceptClient(int listenFd);
    void readClient(Client *client);
    void removeClient(Client *client);

	bool isRequestComplete(const std::string &buf) const;
	RouteConfig *findRoute(const Request& req);
	virtual bool directoryExists(const char* path);

	// methods
	void handleGet(Request req, Client *client, RouteConfig *route);
	void handlePost(Request req, Client *client, RouteConfig *route);
};

#endif
