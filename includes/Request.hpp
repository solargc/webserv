#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>

class Request {
  public:
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;

    bool parse(const std::string &raw);

  private:
    bool parseRequestLine(const std::string &line);
    bool parseHeaders(const std::string &raw, size_t &pos);
};

#endif
