#include "Server.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "SocketSetup.hpp"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

Server::Server(const std::vector<ServerConfig> &configs) : _configs(configs) {
    for (size_t i = 0; i < _configs.size(); i++) {
        int fd = createListenSocket(_configs[i]);
        registerFd(fd, _configs[i]);
        std::cout << "Server listening on " << _configs[i].host << ":"
                  << _configs[i].port << std::endl;
    }
}

Server::~Server() {
    for (size_t i = 0; i < clients.size(); i++) {
        delete clients[i];
    }
    for (size_t i = 0; i < pollEntries.size(); i++) {
        if (pollEntries[i].type == POLL_LISTEN)
            close(pollEntries[i].pfd.fd);
    }
}

void Server::registerFd(int fd, const ServerConfig &config) {
    PollEntry entry = {};
    entry.pfd.fd = fd;
    entry.pfd.events = POLLIN;
    entry.type = POLL_LISTEN;
    entry.client = NULL;
    entry.config = &config;
    pollEntries.push_back(entry);
}

void Server::registerClientFd(Client *client) {
    PollEntry entry = {};
    entry.pfd.fd = client->getFd();
    entry.pfd.events = POLLIN;
    entry.type = POLL_CLIENT;
    entry.client = client;
    entry.config = NULL;
    pollEntries.push_back(entry);
}

void Server::registerCgiFd(int fd, Client *client, PollEntryType type) {
    PollEntry entry = {};
    entry.pfd.fd = fd;
    if (type == POLL_CGI_STDIN)
        entry.pfd.events = POLLOUT;
    else
        entry.pfd.events = POLLIN;
    entry.type = type;
    entry.client = client;
    entry.config = NULL;
    pollEntries.push_back(entry);
}

void Server::removePollFd(int fd) {
    for (size_t i = 0; i < pollEntries.size();) {
        if (pollEntries[i].pfd.fd == fd)
            pollEntries.erase(pollEntries.begin() + i);
        else
            i++;
    }
}

static bool hasClient(const std::vector<Client *> &clients, Client *client) {
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i] == client)
            return true;
    }
    return false;
}

static bool headerNameEquals(const std::string &actual,
                             const std::string &expected) {
    if (actual.size() != expected.size())
        return false;
    for (size_t i = 0; i < actual.size(); i++) {
        if (std::tolower(static_cast<unsigned char>(actual[i])) !=
            std::tolower(static_cast<unsigned char>(expected[i])))
            return false;
    }
    return true;
}

static bool parseContentLength(const std::string &buf, unsigned long &length) {
    size_t pos = buf.find("\r\n");
    if (pos == std::string::npos)
        return false;
    pos += 2;

    size_t valuePos = std::string::npos;
    while (pos < buf.size()) {
        size_t lineEnd = buf.find("\r\n", pos);
        if (lineEnd == std::string::npos)
            return false;
        if (lineEnd == pos)
            return false;

        std::string line = buf.substr(pos, lineEnd - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos &&
            headerNameEquals(line.substr(0, colon), "content-length")) {
            valuePos = pos + colon + 1;
            break;
        }
        pos = lineEnd + 2;
    }

    if (valuePos == std::string::npos)
        return false;
    pos = valuePos;
    while (pos < buf.size() && buf[pos] == ' ')
        pos++;

    size_t end = 0;
    while (pos + end < buf.size() && std::isdigit(buf[pos + end]))
        end++;
    if (end == 0)
        return false;

    std::string lenStr = buf.substr(pos, end);
    char *endptr;
    length = std::strtoul(lenStr.c_str(), &endptr, 10);
    return *endptr == '\0';
}

void Server::acceptClient(int listenFd, const ServerConfig *config) {
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
        Client *client = new Client(fd, config);
        clients.push_back(client);
        registerClientFd(client);
    }
}

bool Server::isRequestComplete(const std::string &buf) const {
    unsigned long conLen = 0;
    if (!parseContentLength(buf, conLen))
        return true;

    size_t bodyPos = buf.find("\r\n\r\n");
    if (bodyPos == std::string::npos)
        return false;
    bodyPos += 4;
    return (buf.size() - bodyPos == static_cast<size_t>(conLen));
}

static bool contentLengthExceedsLimit(const std::string &buf, size_t limit) {
    if (limit == 0)
        return false;

    unsigned long conLen = 0;
    if (!parseContentLength(buf, conLen))
        return false;
    return conLen > static_cast<unsigned long>(limit);
}

static bool bodyExceedsLimit(const std::string &buf, size_t limit) {
    if (limit == 0)
        return false;

    size_t bodyPos = buf.find("\r\n\r\n");
    if (bodyPos == std::string::npos)
        return false;
    bodyPos += 4;
    return buf.size() - bodyPos > limit;
}

static bool routeMatchesPath(const std::string &routePath,
                             const std::string &requestPath) {
    if (routePath == "/")
        return true;
    if (requestPath == routePath)
        return true;
    if (requestPath.size() <= routePath.size())
        return false;
    if (requestPath.find(routePath) != 0)
        return false;
    return requestPath[routePath.size()] == '/';
}

const RouteConfig *Server::findRoute(const Request &req,
                                     const ServerConfig *config) {
    const RouteConfig *best = NULL;

    for (size_t i = 0; i < config->routes.size(); i++) {
        if (!routeMatchesPath(config->routes[i].path, req.path))
            continue;
        if (best == NULL || config->routes[i].path.size() > best->path.size())
            best = &config->routes[i];
    }
    return best;
}

void Server::readClient(Client *client) {
    char buffer[4096];
    ssize_t n = recv(client->getFd(), buffer, sizeof(buffer), 0);
    if (n > 0)
        client->appendData(buffer, n);
    else if (n == 0) {
        removeClient(client);
        return;
    } else {
        removeClient(client);
        return;
    }

    const std::string &buf = client->getBuffer();
    if (buf.find("\r\n\r\n") == std::string::npos)
        return;

    const ServerConfig *config = client->getServerConfig();
    if (contentLengthExceedsLimit(buf, config->clientMaxBodySize) ||
        bodyExceedsLimit(buf, config->clientMaxBodySize)) {
        std::string err = Response::status("413", *config);
        client->appendSendData(err);
        client->clearData();
        return;
    }

    if (!isRequestComplete(buf))
        return;

    Request req;
    if (!req.parse(buf)) {
        std::string err = Response::status("400", *config);
        client->appendSendData(err);
        client->clearData();
        std::cout << "Bad request" << std::endl;
        return;
    }

    const RouteConfig *route = findRoute(req, config);
    if (route == NULL) {
        std::string err = Response::status("404", *config);
        client->appendSendData(err);
        client->clearData();
        return;
    }

    if (route->hasRedirect) {
        std::string res = Response::redirect(route->redirectCode,
                                             route->redirectTarget);
        client->appendSendData(res);
        client->clearData();
        return;
    }

    size_t i = 0;
    for (; i < route->allowedMethods.size(); i++) {
        if (route->allowedMethods[i] == req.method) {
            handleMethods(req, client, route, config);
            break;
        }
    }

    if (i >= route->allowedMethods.size()) {
        std::string err = Response::status("405", *config);
        client->appendSendData(err);
        client->clearData();
        return;
    }

    std::cout << "Method:  " << req.method << std::endl;
    std::cout << "Path:    " << req.path << std::endl;
    std::cout << "Version: " << req.version << std::endl;
    for (std::map<std::string, std::string>::iterator it = req.headers.begin();
         it != req.headers.end(); ++it)
        std::cout << "Header:  " << it->first << ": " << it->second
                  << std::endl;
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
    for (size_t i = 0; i < pollEntries.size();) {
        if (pollEntries[i].pfd.fd == fd ||
            ((pollEntries[i].type == POLL_CGI_STDIN ||
              pollEntries[i].type == POLL_CGI_STDOUT) &&
             pollEntries[i].client == client)) {
            pollEntries.erase(pollEntries.begin() + i);
        } else {
            i++;
        }
    }
}

void Server::handleCgiStdin(Client *client, int fd) {
    const std::string &input = client->getCgiInput();
    size_t sent = client->getCgiInputSent();
    if (sent >= input.size()) {
        close(fd);
        removePollFd(fd);
        client->markCgiStdinClosed();
        return;
    }

    ssize_t n = write(fd, input.c_str() + sent, input.size() - sent);
    if (n > 0)
        client->addCgiInputSent(n);
    else {
        close(fd);
        removePollFd(fd);
        client->markCgiStdinClosed();
        return;
    }

    if (client->getCgiInputSent() >= input.size()) {
        close(fd);
        removePollFd(fd);
        client->markCgiStdinClosed();
    }
}

void Server::handleCgiStdout(Client *client, int fd) {
    char buffer[4096];
    ssize_t n = read(fd, buffer, sizeof(buffer));
    if (n > 0) {
        client->appendCgiOutput(buffer, n);
        return;
    }
    close(fd);
    removePollFd(fd);
    client->markCgiStdoutClosed();
}

void Server::checkCgiComplete(Client *client) {
    if (!client->hasCgi())
        return;
    if (!client->isCgiExited()) {
        int status = 0;
        pid_t result = waitpid(client->getCgiPid(), &status, WNOHANG);
        if (result != 0)
            client->markCgiExited();
    }
    if (client->isCgiExited() && client->isCgiStdinClosed() &&
        client->isCgiStdoutClosed())
        finishCgi(client);
}

void Server::finishCgi(Client *client) {
    std::string res = Response::status("200", client->getServerConfig()->statusDir,
                                       client->getCgiOutput());
    client->appendSendData(res);
    client->resetCgi();
}

void Server::run() {
    while (true) {
        if (pollEntries.empty())
            throw std::runtime_error("No sockets configured");

        // Register POLLOUT for clients with pending data
        for (size_t i = 0; i < pollEntries.size(); i++) {
            if (pollEntries[i].type != POLL_CLIENT)
                continue;
            Client *client = pollEntries[i].client;
            if (client && client->hasPendingData())
                pollEntries[i].pfd.events = POLLIN | POLLOUT;
            else
                pollEntries[i].pfd.events = POLLIN;
        }

        std::vector<pollfd> fds;
        for (size_t i = 0; i < pollEntries.size(); i++)
            fds.push_back(pollEntries[i].pfd);

        int pollResult = poll(&fds[0], fds.size(), -1);
        if (pollResult < 0) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("poll() failed");
        }

        std::vector<PollEntry> ready(pollEntries.begin(), pollEntries.end());
        for (size_t i = 0; i < ready.size(); i++)
            ready[i].pfd.revents = fds[i].revents;

        for (size_t i = 0; i < ready.size(); i++) {
            PollEntry &entry = ready[i];
            int fd = entry.pfd.fd;
            bool isCgi = entry.type == POLL_CGI_STDIN ||
                         entry.type == POLL_CGI_STDOUT;

            if (isCgi && entry.client && hasClient(clients, entry.client)) {
                if ((entry.pfd.revents & POLLOUT) &&
                    entry.type == POLL_CGI_STDIN)
                    handleCgiStdin(entry.client, fd);
                if ((entry.pfd.revents & POLLIN) &&
                    entry.type == POLL_CGI_STDOUT)
                    handleCgiStdout(entry.client, fd);
                if (entry.pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    if (entry.type == POLL_CGI_STDIN &&
                        !entry.client->isCgiStdinClosed()) {
                        close(fd);
                        removePollFd(fd);
                        entry.client->markCgiStdinClosed();
                    } else if (entry.type == POLL_CGI_STDOUT &&
                               !entry.client->isCgiStdoutClosed()) {
                        close(fd);
                        removePollFd(fd);
                        entry.client->markCgiStdoutClosed();
                    }
                }
                checkCgiComplete(entry.client);
                continue;
            }

            if (entry.pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                if (entry.type == POLL_CLIENT && entry.client &&
                    hasClient(clients, entry.client))
                    removeClient(entry.client);
                continue;
            }

            if (entry.pfd.revents & POLLIN) {
                if (entry.type == POLL_LISTEN) {
                    acceptClient(fd, entry.config);
                } else if (entry.type == POLL_CLIENT && entry.client &&
                           hasClient(clients, entry.client)) {
                    readClient(entry.client);
                }
            }

            if (entry.pfd.revents & POLLOUT) {
                Client *client = entry.client;
                if (entry.type == POLL_CLIENT && client &&
                    hasClient(clients, client) && client->hasPendingData()) {
                    const std::string &buf = client->getSendBuffer();
                    ssize_t n = send(fd, buf.c_str(), buf.size(), 0);
                    if (n > 0)
                        client->drainSendBuffer(n);
                    else
                        removeClient(client);
                }
            }
        }
    }
}
