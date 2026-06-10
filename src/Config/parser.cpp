#include "Config.hpp"
#include <cctype>
#include <cstdlib>
#include <stdexcept>

static bool isNumber(const std::string &value) {
    if (value.empty())
        return false;
    for (size_t i = 0; i < value.size(); i++)
        if (!std::isdigit(value[i]))
            return false;
    return true;
}

static bool parseBool(const std::string &value, const std::string &directive) {
    if (value == "on")
        return true;
    if (value == "off")
        return false;
    throw std::runtime_error("Expected 'on' or 'off' after '" + directive + "'");
}

static size_t parseBodySize(const std::string &value) {
    if (value.empty())
        throw std::runtime_error("Expected value after 'client_max_body_size'");

    size_t numberEnd = 0;
    while (numberEnd < value.size() && std::isdigit(value[numberEnd]))
        numberEnd++;
    if (numberEnd == 0 || numberEnd + 1 < value.size())
        throw std::runtime_error("Invalid client_max_body_size: " + value);

    unsigned long size = std::strtoul(value.substr(0, numberEnd).c_str(), NULL, 10);
    if (numberEnd == value.size())
        return static_cast<size_t>(size);

    char unit = value[numberEnd];
    if (unit == 'k' || unit == 'K')
        size *= 1024;
    else if (unit == 'm' || unit == 'M')
        size *= 1024 * 1024;
    else if (unit == 'g' || unit == 'G')
        size *= 1024 * 1024 * 1024;
    else
        throw std::runtime_error("Invalid client_max_body_size unit: " + value);
    return static_cast<size_t>(size);
}

RouteConfig Config::parseRoute(const std::vector<std::string> &tokens,
                               size_t &i) {
    RouteConfig route;
    route.autoindex = false;
    route.hasRedirect = false;
    route.redirectCode = 0;
    route.hasMaxBodySize = false;
    route.clientMaxBodySize = 0;

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
        } else if (tokens[i] == "upload_store") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'upload_store'");
            route.uploadPath = tokens[i];
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after upload store value");
            i++;
        } else if (tokens[i] == "autoindex") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'autoindex'");
            route.autoindex = parseBool(tokens[i], "autoindex");
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after autoindex value");
            i++;
        } else if (tokens[i] == "return" || tokens[i] == "redirect") {
            std::string directive = tokens[i];
            i++;
            if (i >= tokens.size() || !isNumber(tokens[i]))
                throw std::runtime_error("Expected status code after '" + directive + "'");
            route.redirectCode = std::atoi(tokens[i].c_str());
            if (route.redirectCode < 300 || route.redirectCode > 399)
                throw std::runtime_error("Redirect code must be 3xx");
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected target after '" + directive + "'");
            route.redirectTarget = tokens[i];
            route.hasRedirect = true;
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after redirect target");
            i++;
        } else if (tokens[i] == "client_max_body_size") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'client_max_body_size'");
            route.clientMaxBodySize = parseBodySize(tokens[i]);
            route.hasMaxBodySize = true;
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after client_max_body_size value");
            i++;
        } else if (tokens[i] == "cgi_extension" || tokens[i] == "cgi") {
            std::string directive = tokens[i];
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected extension after '" + directive + "'");
            std::string extension = tokens[i];
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected executable after '" + directive + "'");
            route.cgi[extension] = tokens[i];
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after CGI directive");
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
    server.clientMaxBodySize = 0;

    while (i < tokens.size() && tokens[i] != "}") {
        if (tokens[i] == "listen") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected port after 'listen'");
            if (!isNumber(tokens[i]))
                throw std::runtime_error("Invalid port: " + tokens[i]);
            server.port = std::atoi(tokens[i].c_str());
            if (server.port < 1 || server.port > 65535)
                throw std::runtime_error("Port out of range: " + tokens[i]);
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
        } else if (tokens[i] == "client_max_body_size") {
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected value after 'client_max_body_size'");
            server.clientMaxBodySize = parseBodySize(tokens[i]);
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after client_max_body_size value");
            i++;
        } else if (tokens[i] == "error_page") {
            i++;
            if (i >= tokens.size() || !isNumber(tokens[i]))
                throw std::runtime_error("Expected status code after 'error_page'");
            int code = std::atoi(tokens[i].c_str());
            if (code < 300 || code > 599)
                throw std::runtime_error("Invalid error_page status code");
            i++;
            if (i >= tokens.size())
                throw std::runtime_error("Expected path after 'error_page'");
            server.errorPages[code] = tokens[i];
            i++;
            if (i >= tokens.size() || tokens[i] != ";")
                throw std::runtime_error("Expected ';' after error_page path");
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
