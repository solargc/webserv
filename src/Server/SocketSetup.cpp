#include "SocketSetup.hpp"
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <fcntl.h>

static int createTcpSocket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error("socket() failed");
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    return fd;
}

static sockaddr_in resolveAddress(const ServerConfig &cfg) {
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

int createListenSocket(const ServerConfig &cfg) {
    int fd = createTcpSocket();
    sockaddr_in addr = resolveAddress(cfg);
    if (bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");
    if (listen(fd, 128) < 0) {
        throw std::runtime_error("listen() failed");
	}
	fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}
