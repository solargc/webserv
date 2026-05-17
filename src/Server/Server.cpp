#include "Server.hpp"
#include "SocketSetup.hpp"
#include "Request.hpp"
#include "Response.hpp"

#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>

Server::Server(const std::vector<ServerConfig> &configs) : _configs(configs) {
    for (size_t i = 0; i < _configs.size(); i++) {
        int fd = createListenSocket(_configs[i]);
        registerFd(fd, _configs[i]);
        std::cout << "Server listening on " << _configs[i].host << ":" << _configs[i].port << std::endl;
    }
}

Server::~Server() {
    for (size_t i = 0; i < clients.size(); i++) {
        delete clients[i];
	}
	for (std::map<int, const ServerConfig*>::iterator it = listenFdToConfig.begin();
		it != listenFdToConfig.end(); it++) {
		close(it->first);
	}
}

void Server::registerFd(int fd, const ServerConfig& config) {
    pollfd pfd = {};
    pfd.fd = fd;
    pfd.events = POLLIN;
    fds.push_back(pfd);

	listenFdToConfig[fd] = &config;
}

bool Server::isListenSocket(int fd) const {
    return listenFdToConfig.find(fd) != listenFdToConfig.end();
}

static Client *findClient(std::vector<Client *> &clients, int fd) {
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i]->getFd() == fd)
            return clients[i];
    }
    return NULL;
}

void Server::acceptClient(int listenFd) {
    while (true) {
        int fd = accept(listenFd, NULL, NULL);
        if (fd < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;
            return;
        }
        if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
            close(fd);
            continue;
        }
        pollfd pfd = {};
        pfd.fd = fd;
        pfd.events = POLLIN;
        fds.push_back(pfd);
        const ServerConfig* config = listenFdToConfig[listenFd];
        clients.push_back(new Client(fd, config));
    }
}

bool Server::isRequestComplete(const std::string &buf) const {
	size_t pos = buf.find("Content-Length:");
	if (pos == std::string::npos)
		return true;
	pos += 15; // Length of 'Content-Length:'
	if (pos >= buf.size())
		return false;
	if (buf[pos] == ' ')
		++pos; // If there is a space after 'Content-Length'

	size_t end = 0;
	while (pos + end < buf.size() && std::isdigit(buf[pos + end])) {
		++end;
	}
	if (end == 0)
		return false;

	size_t bodyPos = buf.find("\r\n\r\n");
	if (bodyPos == std::string::npos)
		return false;
	bodyPos += 4;
	std::string lenStr = buf.substr(pos, end);
	char *endptr;
	unsigned long conLen = std::strtoul(lenStr.c_str(), &endptr, 10);
	if (*endptr != '\0')
		return false;
	return (buf.size() - bodyPos == static_cast<size_t>(conLen));
}

const RouteConfig *Server::findRoute(const Request& req, const ServerConfig* config) {
		for (size_t i = 0; i < config->routes.size(); i++) {
			if (req.path.find(config->routes[i].path) == 0)
				return &config->routes[i];
	}
	return NULL;
}

void Server::readClient(Client *client) {
	char buffer[4096];
	ssize_t n = recv(client->getFd(), buffer, sizeof(buffer), 0);
	if (n > 0)
		client->appendData(buffer, n);
	else if (n == 0) {
		removeClient(client);
		return;
	}
	else {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return;
		removeClient(client);
		return;
	}

	const std::string &buf = client->getBuffer();
	if (buf.find("\r\n\r\n") == std::string::npos)
		return;

	if (!isRequestComplete(buf))
		return;

	Request req;
	if (!req.parse(buf)) {
		client->clearData();
		std::cout << "Bad request" << std::endl;
		return;
	}

	const ServerConfig* config = client->getServerConfig();
	const RouteConfig *route = findRoute(req, config);
	if (route == NULL) {
		std::string err = Response::status("404", config->statusDir);
		client->appendSendData(err);
		client->clearData();
		return;
	}

	size_t i = 0;
	for (; i < route->allowedMethods.size(); i++) {
		if (route->allowedMethods[i] ==  req.method) {
			handleMethods(req, client, route, config);
			break;
		}
	}

	if (i >= route->allowedMethods.size()) {
		std::string err = Response::status("405", config->statusDir);
		client->appendSendData(err);
		client->clearData();
		return;
	}

	std::cout << "Method:  " << req.method  << std::endl;
	std::cout << "Path:    " << req.path    << std::endl;
	std::cout << "Version: " << req.version << std::endl;
	for (std::map<std::string, std::string>::iterator it = req.headers.begin(); it != req.headers.end(); ++it)
		std::cout << "Header:  " << it->first << ": " << it->second << std::endl;
	client->clearData();
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

// AI RECODE OF RUN FUNCTION. I DON'T REALLY UNDERSTAND THIS.

void Server::run() {
    while (true) {
        if (fds.empty())
            throw std::runtime_error("No sockets configured");

        // Register POLLOUT for clients with pending data
        for (size_t i = 0; i < fds.size(); i++) {
            if (isListenSocket(fds[i].fd))
                continue;
            Client *client = findClient(clients, fds[i].fd);
            if (client && client->hasPendingData())
                fds[i].events = POLLIN | POLLOUT;
            else
                fds[i].events = POLLIN;
        }

        int ret = poll(&fds[0], fds.size(), -1);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("poll() failed");
        }

        std::vector<pollfd> ready(fds.begin(), fds.end());
        for (size_t i = 0; i < ready.size(); i++) {
            int fd = ready[i].fd;

            if (ready[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                if (!isListenSocket(fd)) {
                    Client *client = findClient(clients, fd);
                    if (client)
                        removeClient(client);
                }
                continue;
            }

            if (ready[i].revents & POLLIN) {
                if (isListenSocket(fd)) {
                    acceptClient(fd);
                } else {
                    Client *client = findClient(clients, fd);
                    if (client)
                        readClient(client);
                }
            }

            if (ready[i].revents & POLLOUT) {
                Client *client = findClient(clients, fd);
                if (client && client->hasPendingData()) {
                    const std::string &buf = client->getSendBuffer();
                    ssize_t n = send(fd, buf.c_str(), buf.size(), 0);
                    if (n > 0)
                        client->drainSendBuffer(n);
                    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                        removeClient(client);
                }
            }
        }
    }
}

/*
void Server::run() {
    while (true) {
        if (fds.empty())
            throw std::runtime_error("No sockets configured");

        int ret = poll(&fds[0], fds.size(), -1);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("poll() failed");
        }

        std::vector<pollfd> ready(fds.begin(), fds.end());
        for (size_t i = 0; i < ready.size(); i++) {
            if (!(ready[i].revents & POLLIN))
                continue;
            int fd = ready[i].fd;
            if (isListenSocket(fd)) {
                acceptClient(fd);
            } else {
                Client *client = NULL;
                for (size_t j = 0; j < clients.size(); j++)
                    if (clients[j]->getFd() == fd)
                        client = clients[j];
                if (client)
                    readClient(client);
            }
        }
    }
}
*/
