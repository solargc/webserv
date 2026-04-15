#pragma once

#include "Request.hpp"
#include "Config.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

class Response {
  public:
    Response(const Request &req, const RouteConfig &route);

	// Errors
	static std::string error(std::string code, std::string errorDir);

	static std::string defaultError(std::string code);
	static std::string error404();
	static std::string error405();

    const std::string &getRaw() const;
  private:
    std::string raw;
    void compose(const Request &req, const RouteConfig &route);
};
