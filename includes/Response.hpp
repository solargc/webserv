#pragma once

#include "Request.hpp"
#include "Config.hpp"

#include <iostream>
#include <fstream>
#include <sstream>

class Response {
  public:
    Response(const Request &req, const RouteConfig &route);
    const std::string &getRaw() const;
  private:
    std::string raw;
    void compose(const Request &req, const RouteConfig &route);
};
