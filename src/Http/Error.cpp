#include "Response.hpp"

std::string Response::error404() {
	std::string error = "HTTP/1.1 404 Not Found\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 13\r\n"
		"\r\n"
		"404 Not Found";
		return error;
}

std::string Response::error405() {
	std::string error = "HTTP/1.1 405 Not Found\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 22\r\n"
		"\r\n"
		"405 Method not allowed";
		return error;
}

std::string Response::error(std::string code, std::string errorDir) {
	std::string error = errorDir + code;
	std::ifstream file(filename.c_str());
	if (!file.good()) {
		defaultError(code);
		return;
	}
}
