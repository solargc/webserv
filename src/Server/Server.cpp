#include "Server.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "SocketSetup.hpp"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static const int CGI_TIMEOUT_SECONDS = 5;
static const int CLIENT_TIMEOUT_SECONDS = 30;

static bool setCloseOnExec(int fd) {
    return fcntl(fd, F_SETFD, FD_CLOEXEC) == 0;
}

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

static std::string headerValue(const std::string &buf,
                               const std::string &name) {
    size_t pos = buf.find("\r\n");
    if (pos == std::string::npos)
        return "";
    pos += 2;

    while (pos < buf.size()) {
        size_t lineEnd = buf.find("\r\n", pos);
        if (lineEnd == std::string::npos || lineEnd == pos)
            return "";
        std::string line = buf.substr(pos, lineEnd - pos);
        size_t colon = line.find(':');
        if (colon != std::string::npos &&
            headerNameEquals(line.substr(0, colon), name)) {
            size_t valueStart = colon + 1;
            while (valueStart < line.size() && line[valueStart] == ' ')
                valueStart++;
            return line.substr(valueStart);
        }
        pos = lineEnd + 2;
    }
    return "";
}

static bool requestExpectsContinue(const std::string &buf) {
    std::string value = headerValue(buf, "expect");
    for (size_t i = 0; i < value.size(); i++)
        value[i] = std::tolower(static_cast<unsigned char>(value[i]));
    return value.find("100-continue") != std::string::npos;
}

static bool isChunkedRequest(const std::string &buf) {
    std::string transferEncoding = headerValue(buf, "transfer-encoding");
    size_t pos = 0;
    while (pos < transferEncoding.size()) {
        while (pos < transferEncoding.size() &&
               (transferEncoding[pos] == ' ' || transferEncoding[pos] == ','))
            pos++;
        size_t end = pos;
        while (end < transferEncoding.size() && transferEncoding[end] != ',')
            end++;
        std::string token = transferEncoding.substr(pos, end - pos);
        while (!token.empty() && token[token.size() - 1] == ' ')
            token.erase(token.size() - 1);
        if (headerNameEquals(token, "chunked"))
            return true;
        pos = end;
    }
    return false;
}

static bool parseChunkSize(const std::string &line, size_t &chunkSize) {
    size_t end = line.find(';');
    std::string sizePart = line.substr(0, end);
    if (sizePart.empty())
        return false;

    char *endptr;
    unsigned long parsed = std::strtoul(sizePart.c_str(), &endptr, 16);
    if (*endptr != '\0')
        return false;
    chunkSize = static_cast<size_t>(parsed);
    return true;
}

static bool decodeChunkedBody(const std::string &chunkedBody,
                              std::string &decoded) {
    decoded.clear();
    size_t pos = 0;
    while (true) {
        size_t lineEnd = chunkedBody.find("\r\n", pos);
        if (lineEnd == std::string::npos)
            return false;

        size_t chunkSize = 0;
        if (!parseChunkSize(chunkedBody.substr(pos, lineEnd - pos), chunkSize))
            return false;
        pos = lineEnd + 2;

        if (chunkSize == 0) {
            size_t trailerEnd = chunkedBody.find("\r\n\r\n", pos);
            if (trailerEnd == pos)
                return true;
            if (trailerEnd != std::string::npos)
                return true;
            if (chunkedBody.compare(pos, 2, "\r\n") == 0)
                return true;
            return false;
        }

        if (pos + chunkSize + 2 > chunkedBody.size())
            return false;
        decoded.append(chunkedBody, pos, chunkSize);
        pos += chunkSize;
        if (chunkedBody.compare(pos, 2, "\r\n") != 0)
            return false;
        pos += 2;
    }
}

static bool chunkedRequestComplete(const std::string &buf) {
    size_t bodyPos = buf.find("\r\n\r\n");
    if (bodyPos == std::string::npos)
        return false;
    bodyPos += 4;
    if (buf.size() - bodyPos < 5)
        return false;
    if (buf.compare(buf.size() - 5, 5, "0\r\n\r\n") == 0)
        return true;
    if (buf.compare(buf.size() - 4, 4, "\r\n\r\n") == 0)
        return buf.find("\r\n0\r\n", bodyPos) != std::string::npos;
    return false;
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
        if (!setCloseOnExec(fd)) {
            close(fd);
            continue;
        }
        Client *client = new Client(fd, config);
        clients.push_back(client);
        registerClientFd(client);
    }
}

bool Server::isRequestComplete(const std::string &buf) const {
    if (isChunkedRequest(buf))
        return chunkedRequestComplete(buf);

    unsigned long conLen = 0;
    if (!parseContentLength(buf, conLen))
        return true;

    size_t bodyPos = buf.find("\r\n\r\n");
    if (bodyPos == std::string::npos)
        return false;
    bodyPos += 4;
    return (buf.size() - bodyPos >= static_cast<size_t>(conLen));
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

static size_t effectiveBodyLimit(const std::string &buf,
                                 const ServerConfig *config) {
    size_t limit = config->clientMaxBodySize;
    size_t lineEnd = buf.find("\r\n");
    std::string line = buf.substr(0, lineEnd);
    size_t sp1 = line.find(' ');
    if (sp1 == std::string::npos)
        return limit;
    size_t sp2 = line.find(' ', sp1 + 1);
    std::string path = line.substr(sp1 + 1, sp2 - sp1 - 1);
    size_t query = path.find('?');
    if (query != std::string::npos)
        path = path.substr(0, query);

    const RouteConfig *best = NULL;
    for (size_t i = 0; i < config->routes.size(); i++) {
        if (!routeMatchesPath(config->routes[i].path, path))
            continue;
        if (best == NULL || config->routes[i].path.size() > best->path.size())
            best = &config->routes[i];
    }
    if (best != NULL && best->hasMaxBodySize)
        limit = best->clientMaxBodySize;
    return limit;
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
    size_t bodyLimit = effectiveBodyLimit(buf, config);
    if (!isChunkedRequest(buf) && (contentLengthExceedsLimit(buf, bodyLimit) ||
                                   bodyExceedsLimit(buf, bodyLimit))) {
        std::string err = Response::status("413", *config);
        client->appendSendData(err);
        client->clearData();
        return;
    }

    if (requestExpectsContinue(buf) && !client->isContinueSent()) {
        client->appendSendData("HTTP/1.1 100 Continue\r\n\r\n");
        client->markContinueSent();
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
    if (isChunkedRequest(buf)) {
        std::string decodedBody;
        if (!decodeChunkedBody(req.body, decodedBody)) {
            std::string err = Response::status("400", *config);
            client->appendSendData(err);
            client->clearData();
            return;
        }
        req.body = decodedBody;
        if (bodyLimit != 0 && req.body.size() > bodyLimit) {
            std::string err = Response::status("413", *config);
            client->appendSendData(err);
            client->clearData();
            return;
        }
    }

    const RouteConfig *route = findRoute(req, config);
    if (route == NULL) {
        std::string err = Response::status("404", *config);
        client->appendSendData(err);
        client->clearData();
        return;
    }

    if (route->hasRedirect) {
        std::string res =
            Response::redirect(route->redirectCode, route->redirectTarget);
        client->appendSendData(res);
        client->clearData();
        return;
    }

    bool head = (req.method == "HEAD");
    size_t responseStart = client->getSendBuffer().size();

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
        if (head)
            client->stripBodyFrom(responseStart);
        client->clearData();
        return;
    }

    if (head)
        client->stripBodyFrom(responseStart);

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
    if (n > 0) {
        client->addCgiInputSent(n);
        client->refreshCgiActivity();
    } else if (n < 0) {
        return;
    } else {
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
        client->refreshCgiActivity();
        return;
    }
    if (n < 0)
        return;
    close(fd);
    removePollFd(fd);
    client->markCgiStdoutClosed();
}

void Server::drainCgiStdout(Client *client, int fd) {
    char buffer[4096];
    bool madeProgress = false;

    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            client->appendCgiOutput(buffer, n);
            client->refreshCgiActivity();
            madeProgress = true;
            continue;
        }
        if (n < 0) {
            if (client->hasCgiStdoutHangup() && !madeProgress) {
                close(fd);
                removePollFd(fd);
                client->markCgiStdoutClosed();
            }
            return;
        }
        close(fd);
        removePollFd(fd);
        client->markCgiStdoutClosed();
        return;
    }
}

void Server::checkCgiComplete(Client *client) {
    if (!client->hasCgi())
        return;
    if (client->getCgiStartedAt() > 0 &&
        time(NULL) - client->getCgiStartedAt() > CGI_TIMEOUT_SECONDS) {
        kill(client->getCgiPid(), SIGKILL);
        if (!client->isCgiStdinClosed()) {
            removePollFd(client->getCgiStdinFd());
            close(client->getCgiStdinFd());
            client->markCgiStdinClosed();
        }
        if (!client->isCgiStdoutClosed()) {
            removePollFd(client->getCgiStdoutFd());
            close(client->getCgiStdoutFd());
            client->markCgiStdoutClosed();
        }
        waitpid(client->getCgiPid(), NULL, 0);
        client->appendSendData(
            Response::status("500", *client->getServerConfig()));
        client->resetCgi();
        return;
    }
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
    std::string res = Response::fromCgiOutput(client->getCgiOutput());
    client->appendSendData(res);
    client->resetCgi();
}

bool Server::hasActiveCgi() const {
    for (size_t i = 0; i < clients.size(); i++) {
        if (clients[i]->hasCgi())
            return true;
    }
    return false;
}

void Server::checkClientTimeouts() {
    std::vector<Client *> snapshot(clients.begin(), clients.end());
    time_t now = time(NULL);
    for (size_t i = 0; i < snapshot.size(); i++) {
        Client *client = snapshot[i];
        if (!hasClient(clients, client))
            continue;
        if (client->hasCgi())
            continue;
        if (now - client->getLastActivity() > CLIENT_TIMEOUT_SECONDS)
            removeClient(client);
    }
}

void Server::checkCgiProcesses() {
    std::vector<Client *> snapshot(clients.begin(), clients.end());
    for (size_t i = 0; i < snapshot.size(); i++) {
        if (hasClient(clients, snapshot[i]))
            checkCgiComplete(snapshot[i]);
    }
}

void Server::run() {
    while (true) {
        if (pollEntries.empty())
            throw std::runtime_error("No sockets configured");

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

        int timeout = (hasActiveCgi() || !clients.empty()) ? 1000 : -1;
        int pollResult = poll(&fds[0], fds.size(), timeout);
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
            bool isCgi =
                entry.type == POLL_CGI_STDIN || entry.type == POLL_CGI_STDOUT;

            if (isCgi && entry.client && hasClient(clients, entry.client)) {
                if (entry.type == POLL_CGI_STDIN) {
                    if ((entry.pfd.revents & POLLOUT) &&
                        !entry.client->isCgiStdinClosed())
                        handleCgiStdin(entry.client, fd);
                    if ((entry.pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) &&
                        !entry.client->isCgiStdinClosed()) {
                        close(fd);
                        removePollFd(fd);
                        entry.client->markCgiStdinClosed();
                    }
                } else if (entry.type == POLL_CGI_STDOUT) {
                    if ((entry.pfd.revents & POLLHUP) &&
                        !entry.client->isCgiStdoutClosed())
                        entry.client->markCgiStdoutHangup();
                    if ((entry.pfd.revents & POLLNVAL) &&
                        !entry.client->isCgiStdoutClosed()) {
                        close(fd);
                        removePollFd(fd);
                        entry.client->markCgiStdoutClosed();
                    } else if ((entry.pfd.revents & (POLLERR | POLLHUP)) &&
                               !entry.client->isCgiStdoutClosed()) {
                        drainCgiStdout(entry.client, fd);
                    } else if ((entry.pfd.revents & POLLIN) &&
                               !entry.client->isCgiStdoutClosed()) {
                        handleCgiStdout(entry.client, fd);
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
                    if (n > 0) {
                        client->drainSendBuffer(n);
                        client->refreshActivity();
                    } else
                        removeClient(client);
                }
            }
        }
        checkCgiProcesses();
        checkClientTimeouts();
    }
}
