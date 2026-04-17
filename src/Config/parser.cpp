#include "Config.hpp"
#include <cctype>
#include <cstdlib>
#include <stdexcept>

RouteConfig Config::parseRoute(const std::vector<std::string> &tokens,
                               size_t &i) {
    RouteConfig route;

    if (i >= tokens.size())
        throw std::runtime_error("Expected location path");
    route.path = tokens[i];
    i++;

    if (i >= tokens.size() || tokens[i] != "{")
        throw std::runtime_error("Expected '{' after location path");
    i++;

    while (i < tokens.size() && tokens[i] != "}") {
        if (tokens[i] == "methods") {
            i++;
            while (i < tokens.size() && tokens[i] != ";")
                route.allowedMethods.push_back(tokens[i++]);
            if (i >= tokens.size())
                throw std::runtime_error("Expected ';' after methods");
            i++;
        } else if (tokens[i] == "root") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'root'");
            route.documentRoot = tokens[i];
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after root value");
            i++;
        } else if (tokens[i] == "index") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'index'");
            route.indexFile = tokens[i];
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after index value");
            i++;
        } else if (tokens[i] == "upload_path") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'upload_path'");
            route.uploadPath = tokens[i];
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after upload path value");
            i++;
        } else {
            throw std::runtime_error("Unknown location directive: " +
                                     tokens[i]);
        }
    }

    if (i >= tokens.size())
        throw std::runtime_error("Unexpected end of input in location block");
    i++;
    return route;
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
        } else if (tokens[i] == "status_directory") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'status_directory'");
            server.statusDir = tokens[i];
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after status directory value");
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

void Config::buildServers(const std::vector<std::string> &tokens) {
    size_t i = 0;
    while (i < tokens.size()) {
        if (tokens[i] != "server")
            throw std::runtime_error("Expected 'server', got: " + tokens[i]);
        i++;
        if (i >= tokens.size() || tokens[i] != "{")
            throw std::runtime_error("Expected '{' after server");
        i++;
        configs.push_back(parseServer(tokens, i));
    }
}
