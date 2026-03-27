#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include "Config.hpp"
#include <poll.h>
#include <vector>

class Server {
  public:
    Server(const std::vector<ServerConfig> &configs);
    ~Server();
    void run();

  private:
    std::vector<ServerConfig> configs;
    std::vector<pollfd> fds;
    std::vector<int> listenFds;
    std::vector<Client *> clients;

    void registerFd(int fd);
    bool isListenFd(int fd) const;
    void acceptClient(int listenFd);
    void readClient(Client *client);
    void removeClient(Client *client);
};

#endif
