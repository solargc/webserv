#include "Client.hpp"
#include <unistd.h>

Client::Client(int fd, const ServerConfig* config)
	: fd(fd), serverConfig(config), cgiActive(false), cgiPid(-1),
	  cgiStdinFd(-1), cgiStdoutFd(-1), cgiInputSent(0),
	  cgiStdinClosed(true), cgiStdoutClosed(true), cgiExited(true) {}

Client::~Client() {
	resetCgi();
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

void Client::startCgi(pid_t pid, int stdinFd, int stdoutFd,
					  const std::string &input) {
	cgiActive = true;
	cgiPid = pid;
	cgiStdinFd = stdinFd;
	cgiStdoutFd = stdoutFd;
	cgiInput = input;
	cgiInputSent = 0;
	cgiOutput.clear();
	cgiStdinClosed = (stdinFd < 0);
	cgiStdoutClosed = (stdoutFd < 0);
	cgiExited = false;
}

void Client::resetCgi() {
	if (cgiStdinFd >= 0)
		close(cgiStdinFd);
	if (cgiStdoutFd >= 0)
		close(cgiStdoutFd);
	cgiActive = false;
	cgiPid = -1;
	cgiStdinFd = -1;
	cgiStdoutFd = -1;
	cgiInput.clear();
	cgiInputSent = 0;
	cgiOutput.clear();
	cgiStdinClosed = true;
	cgiStdoutClosed = true;
	cgiExited = true;
}

bool Client::hasCgi() const {
	return cgiActive;
}

pid_t Client::getCgiPid() const {
	return cgiPid;
}

int Client::getCgiStdinFd() const {
	return cgiStdinFd;
}

int Client::getCgiStdoutFd() const {
	return cgiStdoutFd;
}

const std::string &Client::getCgiInput() const {
	return cgiInput;
}

size_t Client::getCgiInputSent() const {
	return cgiInputSent;
}

void Client::addCgiInputSent(size_t bytes) {
	cgiInputSent += bytes;
}

void Client::appendCgiOutput(const char *buf, size_t bytes) {
	cgiOutput.append(buf, bytes);
}

const std::string &Client::getCgiOutput() const {
	return cgiOutput;
}

void Client::markCgiStdinClosed() {
	cgiStdinClosed = true;
	cgiStdinFd = -1;
}

void Client::markCgiStdoutClosed() {
	cgiStdoutClosed = true;
	cgiStdoutFd = -1;
}

void Client::markCgiExited() {
	cgiExited = true;
}

bool Client::isCgiStdinClosed() const {
	return cgiStdinClosed;
}

bool Client::isCgiStdoutClosed() const {
	return cgiStdoutClosed;
}

bool Client::isCgiExited() const {
	return cgiExited;
}
