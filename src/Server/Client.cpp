#include "Client.hpp"
#include <unistd.h>

Client::Client(int fd, const ServerConfig* config) : fd(fd), serverConfig(config) {}

Client::~Client() {
    close(fd);
}

int Client::getFd() const {
    return fd;
}

const ServerConfig* Client::getServerConfig() const {
	return serverConfig;
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

void Client::appendSendData(const std::string &data) {
	sendBuffer += data;
}

bool Client::hasPendingData() const {
	return !sendBuffer.empty();
}

const std::string &Client::getSendBuffer() const {
	return sendBuffer;
}

void Client::drainSendBuffer(size_t bytes) {
	sendBuffer.erase(0, bytes);
}
