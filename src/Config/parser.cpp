#include "Config.hpp"
#include <cctype>
#include <cstdlib>
#include <stdexcept>

RouteConfig Config::parseRoute(const std::vector<std::string> &tokens,
                               size_t &i) {
    RouteConfig r;

    if (i >= tokens.size())
        throw std::runtime_error("Expected location path");
    r.path = tokens[i];
    i++;

    if (i >= tokens.size() || tokens[i] != "{")
        throw std::runtime_error("Expected '{' after location path");
    i++;

    while (i < tokens.size() && tokens[i] != "}") {
        if (tokens[i] == "methods") {
            i++;
            while (i < tokens.size() && tokens[i] != ";")
                r.methods.push_back(tokens[i++]);
            if (i >= tokens.size())
                throw std::runtime_error("Expected ';' after methods");
            i++;
        } else if (tokens[i] == "root") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'root'");
            r.root = tokens[i];
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after root value");
            i++;
        } else if (tokens[i] == "index") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'index'");
            r.index = tokens[i];
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after index value");
            i++;
        } else {
            throw std::runtime_error("Unknown location directive: " +
                                     tokens[i]);
        }
    }

    if (i >= tokens.size())
        throw std::runtime_error("Unexpected end of input in location block");
    i++;
    return r;
}

ServerConfig Config::parseServer(const std::vector<std::string> &tokens,
                                 size_t &i) {
    ServerConfig server;
    server.port = 0;

    while (i < tokens.size() && tokens[i] != "}") {
        if (tokens[i] == "listen") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected port after 'listen'");
            for (size_t j = 0; j < tokens[i].size(); j++)
                if (!std::isdigit(tokens[i][j]))
                    throw std::runtime_error("Invalid port: " + tokens[i]);
            server.port = std::atoi(tokens[i].c_str());
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after listen value");
            i++;
        } else if (tokens[i] == "host") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'host'");
            server.host = tokens[i];
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after host value");
            i++;
        } else if (tokens[i] == "location") {
            i++;
            server.routes.push_back(parseRoute(tokens, i));
        } else {
            throw std::runtime_error("Unknown server directive: " + tokens[i]);
        }
    }

    if (i >= tokens.size())
        throw std::runtime_error("Unexpected end of input in server block");
    i++;
    return server;
}

void Config::parse(const std::vector<std::string> &tokens) {
    size_t i = 0;
    while (i < tokens.size()) {
        if (tokens[i] != "server")
            throw std::runtime_error("Expected 'server', got: " + tokens[i]);
        i++;
        if (i >= tokens.size() || tokens[i] != "{")
            throw std::runtime_error("Expected '{' after server");
        i++;
        servers.push_back(parseServer(tokens, i));
    }
}
