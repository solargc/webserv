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

	static std::string defaultStatus(std::string code);
	static std::string status200(std::ifstream &file);
	static std::string status201();
	static std::string status404();
	static std::string status405();

    const std::string &getRaw() const;
  private:
    std::string raw;
    void compose(const Request &req, const RouteConfig &route, std::string statusDir);
};
