#pragma once

#include "Request.hpp"
#include "Config.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

class Response {
  public:
    Response(const Request &req, const RouteConfig &route, std::string statusDir);

	// Errors
	static std::string status(std::string code, std::string statusDir);
	static std::string status(std::string code, std::string statusDir, std::ifstream &file200);
	static std::string status(std::string code, std::string statusDir, std::string file200);

	static std::string defaultStatus(std::string code);
	static std::string status200(std::ifstream &file);
	static std::string status200(std::string file);
	static std::string formResponse(std::string);
	static std::string statusWithBody(std::string code, std::string body);
	static std::string statusText(std::string code);

    const std::string &getRaw() const;
  private:
    std::string raw;
    void compose(const Request &req, const RouteConfig &route, std::string statusDir);
};
