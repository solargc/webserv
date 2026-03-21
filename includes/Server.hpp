#ifndef SERVER_HPP
#define SERVER_HPP

#include "Config.hpp"
#include <netinet/in.h>
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

    void addListenSocket(const ServerConfig &cfg);
    int createSocket();
    sockaddr_in resolveAddress(const ServerConfig &cfg);
    void bindAndListen(int fd, const sockaddr_in &addr);
    void registerFd(int fd);
    bool isListenFd(int fd) const;
};

#endif
