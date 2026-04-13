#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>

class Client {
  public:
    Client(int fd);
    ~Client();

    int getFd() const;
    void appendData(const char *buf, int n);
	void clearBuffer();
	void clearData();
    const std::string &getBuffer() const;

  private:
    int fd;
    std::string buffer;
};

#endif
