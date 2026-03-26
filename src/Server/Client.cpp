#include "Client.hpp"
#include <unistd.h>

Client::Client(int fd) : fd(fd) {}

Client::~Client() {
    close(fd);
}

int Client::getFd() const {
    return fd;
}

void Client::appendData(const char *buf, int n) {
    buffer.append(buf, n);
}

const std::string &Client::getBuffer() const {
    return buffer;
}
