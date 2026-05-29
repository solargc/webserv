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
	enum PollEntryType {
		POLL_LISTEN,
		POLL_CLIENT,
		POLL_CGI_STDIN,
		POLL_CGI_STDOUT
	};

	struct PollEntry {
		pollfd pfd;
		PollEntryType type;
		Client *client;
		const ServerConfig *config;
	};

    std::vector<PollEntry> pollEntries;
    std::vector<Client *> clients;
	std::vector<ServerConfig> _configs;

    void registerFd(int fd, const ServerConfig& config);
    void registerClientFd(Client *client);
    void registerCgiFd(int fd, Client *client, PollEntryType type);
    void removePollFd(int fd);
    void acceptClient(int listenFd, const ServerConfig *config);
    void readClient(Client *client);
    void handleCgiStdin(Client *client, int fd);
    void handleCgiStdout(Client *client, int fd);
    void drainCgiStdout(Client *client, int fd);
    void checkCgiComplete(Client *client);
    void finishCgi(Client *client);
    void removeClient(Client *client);

	bool isRequestComplete(const std::string &buf) const;
	const RouteConfig *findRoute(const Request& req, const ServerConfig* config);
	virtual bool directoryExists(const char* path);
	virtual bool fileExists(const std::string& filename);

	// methods
	void handleMethods(Request req, Client *client, const RouteConfig *route, const ServerConfig* config);

	void handleGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config);
	void CGIGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config, const std::string &interpreter);
	void StaticGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config);

	void handlePost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config);
	void CGIPost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config, const std::string &interpreter);
	void StaticPost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config);

	void handleDelete(Request req, Client *client, const RouteConfig *route, const ServerConfig* config);
};

#endif
