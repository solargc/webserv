#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "Config.hpp"

#include <string>
#include <ctime>
#include <sys/types.h>

class Client {
  public:
    Client(int fd, const ServerConfig* config);
    ~Client();

    int getFd() const;
    void appendData(const char *buf, int n);
	void clearBuffer();
	void clearData();
    const std::string &getBuffer() const;
	const ServerConfig* getServerConfig() const;

	void appendSendData(const std::string &data);
	bool hasPendingData() const;
	const std::string &getSendBuffer() const;
	void drainSendBuffer(size_t bytes);
	void stripBodyFrom(size_t responseStart);

	void startCgi(pid_t pid, int stdinFd, int stdoutFd, std::string &input);
	void resetCgi();
	bool hasCgi() const;
	pid_t getCgiPid() const;
	time_t getCgiStartedAt() const;
	void refreshCgiActivity();
	int getCgiStdinFd() const;
	int getCgiStdoutFd() const;
	const std::string &getCgiInput() const;
	size_t getCgiInputSent() const;
	void addCgiInputSent(size_t bytes);
	void appendCgiOutput(const char *buf, size_t bytes);
	const std::string &getCgiOutput() const;
	void markCgiStdinClosed();
	void markCgiStdoutClosed();
	void markCgiExited();
	void markCgiStdoutHangup();
	bool isCgiStdinClosed() const;
	bool isCgiStdoutClosed() const;
	bool isCgiExited() const;
	bool hasCgiStdoutHangup() const;
  private:
    int fd;
	const ServerConfig *serverConfig;
    std::string buffer;

	std::string sendBuffer;

	bool cgiActive;
	pid_t cgiPid;
	time_t cgiStartedAt;
	int cgiStdinFd;
	int cgiStdoutFd;
	std::string cgiInput;
	size_t cgiInputSent;
	std::string cgiOutput;
	bool cgiStdinClosed;
	bool cgiStdoutClosed;
	bool cgiExited;
	bool cgiStdoutHangup;
};

#endif
