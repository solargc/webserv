#include "Response.hpp"

std::string Response::status200(std::ifstream& file) {
	std::stringstream ss;
	ss << file.rdbuf();
	std::string fileContent = ss.str();
	ss.str("");
	ss.clear();
	ss << fileContent.size();
	std::string fileSize = ss.str();

	std::string status = "HTTP/1.1 200 OK\r\n";
	status += "Content-Type: text/html\r\n";
	status += "Content-Length: " + fileSize + "\r\n";
	status += "\r\n";
	status += fileContent;
	return status;
}

std::string Response::status200(std::string file) {
	std::string status = "HTTP/1.1 200 OK\r\n";
	status += "Content-Type: text/html\r\n";
	status += "Content-Length: ";
	std::ostringstream oss;
	oss << file.size();
	status += oss.str();
	status += "\r\n";
	status += "\r\n";
	status += file;
	return status;
}

std::string Response::formResponse(std::string type) {
	std::string html = "<html>\n"
		"\t<body>\n"
		"\t\t<h1>" + type + "</h1>\n"
		"\t</body>\n"
		"</html>";

	size_t htmlSize = html.size();
	std::stringstream ss;
	ss << htmlSize;

	std::string status = "HTTP/1.1 " + type + "\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: " + ss.str() + "\r\n"
		"\r\n" + html;

	return status;
}

std::string Response::defaultStatus(std::string code) {
	if (code == "400")
		return formResponse("400 Bad Request");
	else if (code == "404")
		return formResponse("404 Not Found");
	else if (code == "405")
		return formResponse("405 Method not allowed");
	else if (code == "201")
		return formResponse("201 Created");
	else
		return formResponse("204 No Content");
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

std::string Response::status(std::string code, std::string statusDir, std::string file200) {
	std::string status = statusDir + "/" + code + ".html";
	std::ifstream file(status.c_str());
	if (!file.good())
		return status200(file200);

	std::stringstream ss;
	ss << file.rdbuf();
	return ss.str();
}
