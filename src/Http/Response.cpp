#include "Response.hpp"

Response::Response(const Request& req, const RouteConfig& route, std::string statusDir) {	
	compose(req, route, statusDir);
}

void Response::compose(const Request& req, const RouteConfig& route, std::string statusDir) {
	std::string filePath = route.documentRoot + req.path;
	std::ifstream file(filePath.c_str());
	if (!file.is_open()) {
		raw = status("404", statusDir);
		return;
	}
	raw = status("200", statusDir, file);
}

const std::string &Response::getRaw() const {
	return raw;
}
