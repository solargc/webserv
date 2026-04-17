#include "Response.hpp"

std::string Response::status404() {
	std::string status = "HTTP/1.1 404 Not Found\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 13\r\n"
		"\r\n"
		"404 Not Found";
		return status;
}

std::string Response::status405() {
	std::string status = "HTTP/1.1 405 Method Not Allowed\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 22\r\n"
		"\r\n"
		"405 Method not allowed";
		return status;
}

std::string Response::status201() {
	std::string status = "HTTP/1.1 201 Created\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 22\r\n"
		"\r\n"
		"201 Created";
		return status;
}

std::string Response::status200(std::ifstream& file) {
	std::stringstream ss;
	ss << file.rdbuf();
	std::string fileContent = ss.str();
	ss.str("");
	ss.clear();
	ss << fileContent.size();
	std::string fileSize = ss.str();

	std::string status = "HTTP/1.1 200 OK\r\n";
	status += "Content-Type: text/plain\r\n";
	status += "Content-Length: " + fileSize + "\r\n";
	status += "\r\n";
	status += fileContent;
	return status;
}

std::string Response::defaultStatus(std::string code) {
	if (code == "404")
		return status404();
	else if (code == "405")
		return status405();
	else
		return status201();
}

std::string Response::status(std::string code, std::string statusDir, std::ifstream& file200) {
	std::string status = statusDir + "/" + code + ".html";
	std::ifstream file(status.c_str());
	if (!file.good())
		return status200(file200);

	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}

std::string Response::status(std::string code, std::string statusDir) {
	std::string status = statusDir + "/" + code + ".html";
	std::ifstream file(status.c_str());
	if (!file.good())
		return defaultStatus(code);

	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}
