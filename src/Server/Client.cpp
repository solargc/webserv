#include "Client.hpp"
#include <unistd.h>

Client::Client(int fd) : fd(fd) {}

Client::~Client() {
    close(fd);
}

int Client::getFd() const {
    return fd;
}


void Client::clearData() {
	  buffer.clear();
}

void Client::appendData(const char *buf, int bytesRead) {
    buffer.append(buf, bytesRead);
}

const std::string &Client::getBuffer() const {
    return buffer;
}
