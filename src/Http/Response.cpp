#include "Response.hpp"

Response::Response(const Request& req, const RouteConfig& route, std::string statusDir) {	
	compose(req, route, statusDir);
}

Response::Response(const Request& req, const RouteConfig& route, const ServerConfig& config) {
	compose(req, route, config);
}

void Response::compose(const Request& req, const RouteConfig& route, std::string statusDir) {
	std::string filePath = route.documentRoot + req.path;
	if (!filePath.empty() && filePath[filePath.size() - 1] == '/')
		filePath += route.indexFile;
	std::ifstream file(filePath.c_str());
	if (!file.is_open()) {
		raw = status("404", statusDir);
		return;
	}
	raw = status("200", statusDir, file);
}

void Response::compose(const Request& req, const RouteConfig& route, const ServerConfig& config) {
	std::string filePath = route.documentRoot + req.path;
	if (!filePath.empty() && filePath[filePath.size() - 1] == '/')
		filePath += route.indexFile;
	std::ifstream file(filePath.c_str());
	if (!file.is_open()) {
		raw = status("404", config);
		return;
	}
	raw = status("200", config.statusDir, file);
}

const std::string &Response::getRaw() const {
	return raw;
}
