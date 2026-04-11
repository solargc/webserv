#include "Response.hpp"

Response::Response(const Request& req, const RouteConfig& route) {	
	compose(req, route);
}

void Response::compose(const Request& req, const RouteConfig& route) {
	std::string filePath = route.documentRoot + req.path;
	std::ifstream file(filePath.c_str());
	if (!file.is_open()) {
		std::cout << "Error: Failed to open a file" << std::endl;
		return;
	}
	std::stringstream ss;
	ss << file.rdbuf();
	std::string fileContent = ss.str();
	ss.str("");
	ss.clear();
	ss << fileContent.size();
	std::string fileSize = ss.str();

	raw = "HTTP/1.1 200 OK\r\n";
	raw += "Content-Type: text/html\r\n";
	raw += "Content-Length: " + fileSize + "\r\n";
	raw += "\r\n";
	raw += fileContent;	
}

const std::string &Response::getRaw() const {
	return raw;
}
