#include "Response.hpp"
#include <sys/stat.h>

static bool isDirectory(const std::string &path) {
	struct stat info;
	if (stat(path.c_str(), &info) != 0)
		return false;
	return (info.st_mode & S_IFDIR) != 0;
}

Response::Response(const Request& req, const RouteConfig& route, std::string statusDir) {	
	compose(req, route, statusDir);
}

Response::Response(const Request& req, const RouteConfig& route, const ServerConfig& config) {
	compose(req, route, config);
}

std::string Response::resolvePath(const Request& req, const RouteConfig& route) {
	std::string relativePath = req.path;
	if (route.path != "/" && relativePath.find(route.path) == 0)
		relativePath = relativePath.substr(route.path.size());
	if (relativePath.empty())
		relativePath = "/";
	if (relativePath[0] != '/')
		relativePath = "/" + relativePath;

	if (!route.documentRoot.empty() &&
		route.documentRoot[route.documentRoot.size() - 1] == '/' &&
		relativePath.size() > 1)
		return route.documentRoot.substr(0, route.documentRoot.size() - 1) + relativePath;
	return route.documentRoot + relativePath;
}

void Response::compose(const Request& req, const RouteConfig& route, std::string statusDir) {
	std::string filePath = resolvePath(req, route);
	if (isDirectory(filePath) && !route.indexFile.empty()) {
		if (filePath[filePath.size() - 1] != '/')
			filePath += "/";
		filePath += route.indexFile;
	}
	std::ifstream file(filePath.c_str());
	if (!file.is_open()) {
		raw = status("404", statusDir);
		return;
	}
	raw = status("200", statusDir, file);
}

void Response::compose(const Request& req, const RouteConfig& route, const ServerConfig& config) {
	std::string filePath = resolvePath(req, route);
	if (isDirectory(filePath)) {
		if (!route.indexFile.empty()) {
			std::string indexPath = filePath;
			if (indexPath[indexPath.size() - 1] != '/')
				indexPath += "/";
			indexPath += route.indexFile;
			std::ifstream indexFile(indexPath.c_str());
			if (indexFile.is_open()) {
				raw = status("200", config.statusDir, indexFile);
				return;
			}
		}
		if (route.autoindex) {
			raw = autoindex(req.path, filePath);
			return;
		}
		raw = status("404", config);
		return;
	}
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
