#include "Server.hpp"
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

Server::Server(const std::vector<ServerConfig> &configs) : configs(configs) {
    for (size_t i = 0; i < configs.size(); i++)
        addListenSocket(configs[i]);
}

Server::~Server() {
    for (size_t i = 0; i < fds.size(); i++)
        close(fds[i].fd);
}

int Server::createSocket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error("socket() failed");
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    return fd;
}

sockaddr_in Server::resolveAddress(const ServerConfig &cfg) {
    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo *res = NULL;
    if (getaddrinfo(cfg.host.c_str(), NULL, &hints, &res) != 0)
        throw std::runtime_error("getaddrinfo() failed for " + cfg.host);
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.port);
    addr.sin_addr = ((sockaddr_in *)res->ai_addr)->sin_addr;
    freeaddrinfo(res);
    return addr;
}

void Server::bindAndListen(int fd, const sockaddr_in &addr) {
    if (bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");
    if (listen(fd, 128) < 0)
        throw std::runtime_error("listen() failed");
}

void Server::registerFd(int fd) {
    pollfd pfd = {};
    pfd.fd = fd;
    pfd.events = POLLIN;
    fds.push_back(pfd);
    listenFds.push_back(fd);
}

bool Server::isListenFd(int fd) const {
    for (size_t i = 0; i < listenFds.size(); i++)
        if (listenFds[i] == fd)
            return true;
    return false;
}

void Server::addListenSocket(const ServerConfig &cfg) {
    int fd = createSocket();
    sockaddr_in addr = resolveAddress(cfg);
    bindAndListen(fd, addr);
    registerFd(fd);
}

void Server::run() {
    while (true) {
        if (poll(&fds[0], fds.size(), -1) < 0)
            throw std::runtime_error("poll() failed");

        for (size_t i = 0; i < fds.size(); i++) {
            if (!(fds[i].revents & POLLIN))
                continue;
            if (isListenFd(fds[i].fd)) {
                int client = accept(fds[i].fd, NULL, NULL);
                if (client >= 0)
                    close(client);
            }
        }
    }
}
