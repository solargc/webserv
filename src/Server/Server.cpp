#include "Server.hpp"
#include "SocketSetup.hpp"
#include "Request.hpp"
#include "Response.hpp"

#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>

Server::Server(const std::vector<ServerConfig> &configs) : _configs(configs) {
    for (size_t i = 0; i < configs.size(); i++) {
        int fd = createListenSocket(configs[i]);
        registerFd(fd, configs[i]);
        std::cout << "Server listening on " << configs[i].host << ":" << configs[i].port << std::endl;
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

void Server::acceptClient(int listenFd) {
    int fd = accept(listenFd, NULL, NULL);
    if (fd < 0)
        return;
    pollfd pfd = {};
    pfd.fd = fd;
    pfd.events = POLLIN;
    fds.push_back(pfd);
	const ServerConfig* config = listenFdToConfig[listenFd];
	clients.push_back(new Client(fd, config));
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
	size_t conLen = std::atol(buf.substr(pos, end).c_str());
	return (buf.size() - bodyPos == conLen);
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
	int n = recv(client->getFd(), buffer, sizeof(buffer), 0);
	if (n <= 0) {
		removeClient(client);
		return;
	}
	client->appendData(buffer, n);

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
		std::string err = Response::error("404", config->errorDir);
		send(client->getFd(), err.c_str(), err.size(), 0);
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
		std::string err = Response::error("405", config->errorDir);
		send(client->getFd(), err.c_str(), err.size(), 0);
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
