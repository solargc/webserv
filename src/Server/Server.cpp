#include "Server.hpp"
#include "SocketSetup.hpp"
#include "Request.hpp"
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

Server::Server(const std::vector<ServerConfig> &configs) {
    for (size_t i = 0; i < configs.size(); i++) {
        int fd = createListenSocket(configs[i]);
        registerFd(fd);
        std::cout << "Server listening on " << configs[i].host << ":" << configs[i].port << std::endl;
    }
}

Server::~Server() {
    for (size_t i = 0; i < clients.size(); i++)
        delete clients[i];
    for (size_t i = 0; i < listenFds.size(); i++)
        close(listenFds[i]);
}

void Server::registerFd(int fd) {
    pollfd pfd = {};
    pfd.fd = fd;
    pfd.events = POLLIN;
    fds.push_back(pfd);
    listenFds.push_back(fd);
}

bool Server::isListenSocket(int fd) const {
    for (size_t i = 0; i < listenFds.size(); i++)
        if (listenFds[i] == fd)
            return true;
    return false;
}

void Server::acceptClient(int listenFd) {
    int fd = accept(listenFd, NULL, NULL);
    if (fd < 0)
        return;
    pollfd pfd = {};
    pfd.fd = fd;
    pfd.events = POLLIN;
    fds.push_back(pfd);
    clients.push_back(new Client(fd));
}

void Server::readClient(Client *client) {
    char buffer[4096];
    int n = recv(client->getFd(), buffer, sizeof(buffer), 0);
    if (n <= 0) {
        removeClient(client);
        return;
    }
    client->appendData(buffer, n);

    const std::string &buf = client->getBuffer();
    if (buf.find("\r\n\r\n") == std::string::npos)
        return;

    Request req;
    if (!req.parse(buf)) {
        std::cout << "Bad request" << std::endl;
        return;
    }
    std::cout << "Method:  " << req.method  << std::endl;
    std::cout << "Path:    " << req.path    << std::endl;
    std::cout << "Version: " << req.version << std::endl;
    for (std::map<std::string, std::string>::iterator it = req.headers.begin(); it != req.headers.end(); ++it)
        std::cout << "Header:  " << it->first << ": " << it->second << std::endl;
}

void Server::removeClient(Client *client) {
    int fd = client->getFd();
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i] == client) {
            delete clients[i];
            clients.erase(clients.begin() + i);
            break;
        }
    }
    for (size_t i = 0; i < fds.size(); i++) {
        if (fds[i].fd == fd) {
            fds.erase(fds.begin() + i);
            break;
        }
    }
}

void Server::run() {
    while (true) {
        if (poll(&fds[0], fds.size(), -1) < 0)
            throw std::runtime_error("poll() failed");

        for (size_t i = 0; i < fds.size(); i++) {
            if (!(fds[i].revents & POLLIN))
                continue;
            if (isListenSocket(fds[i].fd))
                acceptClient(fds[i].fd);
            else {
                Client *client = NULL;
                for (size_t j = 0; j < clients.size(); j++)
                    if (clients[j]->getFd() == fds[i].fd)
                        client = clients[j];
                if (client)
                    readClient(client);
            }
        }
    }
}
