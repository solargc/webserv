#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "Config.hpp"

#include <string>

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
  private:
    int fd;
	const ServerConfig *serverConfig;
    std::string buffer;

	std::string sendBuffer;
};

#endif
