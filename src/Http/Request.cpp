#include "Request.hpp"

bool Request::parseRequestLine(const std::string &line) {
    size_t first = line.find(' ');
    if (first == std::string::npos)
        return false;
    size_t second = line.find(' ', first + 1);
    if (second == std::string::npos)
        return false;

    method  = line.substr(0, first);
    path    = line.substr(first + 1, second - first - 1);
    version = line.substr(second + 1);
    return !method.empty() && !path.empty() && !version.empty();
}

bool Request::parseHeaders(const std::string &raw, size_t &pos) {
    while (pos < raw.size()) {
        size_t lineEnd = raw.find("\r\n", pos);
        if (lineEnd == std::string::npos)
            return false;

        std::string line = raw.substr(pos, lineEnd - pos);
        pos = lineEnd + 2;

        if (line.empty())
            return true;

        size_t colon = line.find(':');
        if (colon == std::string::npos)
            return false;

        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);

        size_t start = value.find_first_not_of(' ');
        if (start != std::string::npos)
            value = value.substr(start);

        headers[key] = value;
    }
    return false;
}

bool Request::parse(const std::string &raw) {
    size_t pos = 0;

    size_t lineEnd = raw.find("\r\n");
    if (lineEnd == std::string::npos)
        return false;
    if (!parseRequestLine(raw.substr(0, lineEnd)))
        return false;
    pos = lineEnd + 2;

    if (!parseHeaders(raw, pos))
        return false;

    body = raw.substr(pos);
    return true;
}

