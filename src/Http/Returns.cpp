#include "Response.hpp"
#include <cctype>
#include <cstdlib>
#include <dirent.h>

static bool headerNameEquals(const std::string &actual,
							 const std::string &expected) {
	if (actual.size() != expected.size())
		return false;
	for (size_t i = 0; i < actual.size(); i++) {
		if (std::tolower(static_cast<unsigned char>(actual[i])) !=
			std::tolower(static_cast<unsigned char>(expected[i])))
			return false;
	}
	return true;
}

static std::string trimHeaderValue(const std::string &value) {
	size_t start = value.find_first_not_of(" \t");
	if (start == std::string::npos)
		return "";
	size_t end = value.find_last_not_of(" \t\r");
	return value.substr(start, end - start + 1);
}

std::string Response::status200(std::ifstream& file) {
	std::stringstream ss;
	ss << file.rdbuf();
	return statusWithBody("200", ss.str());
}

std::string Response::status200(std::string file) {
	return statusWithBody("200", file);
}

std::string Response::formResponse(std::string type) {
	std::string html = "<html>\n"
		"\t<body>\n"
		"\t\t<h1>" + type + "</h1>\n"
		"\t</body>\n"
		"</html>";

	return statusWithBody(type.substr(0, 3), html);
}

std::string Response::statusText(std::string code) {
	if (code == "200")
		return "OK";
	else if (code == "201")
		return "Created";
	else if (code == "204")
		return "No Content";
	else if (code == "301")
		return "Moved Permanently";
	else if (code == "302")
		return "Found";
	else if (code == "303")
		return "See Other";
	else if (code == "307")
		return "Temporary Redirect";
	else if (code == "308")
		return "Permanent Redirect";
	else if (code == "400")
		return "Bad Request";
	else if (code == "403")
		return "Forbidden";
	else if (code == "404")
		return "Not Found";
	else if (code == "405")
		return "Method not allowed";
	else if (code == "413")
		return "Payload Too Large";
	else if (code == "500")
		return "Internal Server Error";
	return "No Content";
}

std::string Response::statusWithBody(std::string code, std::string body) {
	std::string type = code + " " + statusText(code);
	if (code == "204") {
		return "HTTP/1.1 " + type + "\r\n"
			"Content-Length: 0\r\n"
			"\r\n";
	}

	std::stringstream ss;
	ss << body.size();
	std::string status = "HTTP/1.1 " + type + "\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: " + ss.str() + "\r\n"
		"\r\n" + body;
	return status;
}

std::string Response::redirect(int code, const std::string &target) {
	std::stringstream ss;
	ss << code;
	std::string codeString = ss.str();
	std::string type = codeString + " " + statusText(codeString);

	return "HTTP/1.1 " + type + "\r\n"
		"Location: " + target + "\r\n"
		"Content-Length: 0\r\n"
		"\r\n";
}

std::string Response::fromCgiOutput(const std::string &output) {
	size_t separator = output.find("\r\n\r\n");
	size_t separatorSize = 4;
	if (separator == std::string::npos) {
		separator = output.find("\n\n");
		separatorSize = 2;
	}
	if (separator == std::string::npos)
		return statusWithBody("200", output);

	std::string headerBlock = output.substr(0, separator);
	std::string body = output.substr(separator + separatorSize);
	std::string code = "200";
	std::string reason = statusText(code);
	std::string headers;
	bool hasContentType = false;

	size_t pos = 0;
	while (pos < headerBlock.size()) {
		size_t lineEnd = headerBlock.find('\n', pos);
		if (lineEnd == std::string::npos)
			lineEnd = headerBlock.size();
		std::string line = headerBlock.substr(pos, lineEnd - pos);
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);

		size_t colon = line.find(':');
		if (colon != std::string::npos) {
			std::string name = line.substr(0, colon);
			std::string value = trimHeaderValue(line.substr(colon + 1));
			if (headerNameEquals(name, "Status")) {
				if (value.size() >= 3) {
					code = value.substr(0, 3);
					if (value.size() > 4)
						reason = value.substr(4);
					else
						reason = statusText(code);
				}
			} else if (!headerNameEquals(name, "Content-Length")) {
				if (headerNameEquals(name, "Content-Type"))
					hasContentType = true;
				headers += name + ": " + value + "\r\n";
			}
		}
		pos = lineEnd + 1;
	}

	if (!hasContentType)
		headers += "Content-Type: text/html\r\n";

	std::stringstream ss;
	ss << body.size();
	return "HTTP/1.1 " + code + " " + reason + "\r\n" +
		headers +
		"Content-Length: " + ss.str() + "\r\n"
		"\r\n" + body;
}

std::string Response::autoindex(const std::string &requestPath,
								const std::string &directoryPath) {
	DIR *dir = opendir(directoryPath.c_str());
	if (dir == NULL)
		return defaultStatus("404");

	std::string basePath = requestPath;
	if (basePath.empty())
		basePath = "/";
	if (basePath[basePath.size() - 1] != '/')
		basePath += "/";

	std::string html = "<html>\n"
		"\t<body>\n"
		"\t\t<h1>Index of " + requestPath + "</h1>\n"
		"\t\t<ul>\n";

	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		std::string name = entry->d_name;
		if (name == ".")
			continue;
		html += "\t\t\t<li><a href=\"" + basePath + name + "\">" +
			name + "</a></li>\n";
	}
	closedir(dir);

	html += "\t\t</ul>\n"
		"\t</body>\n"
		"</html>";
	return statusWithBody("200", html);
}

std::string Response::defaultStatus(std::string code) {
	if (code == "400")
		return formResponse("400 Bad Request");
	else if (code == "403")
		return formResponse("403 Forbidden");
	else if (code == "404")
		return formResponse("404 Not Found");
	else if (code == "405")
		return formResponse("405 Method not allowed");
	else if (code == "413")
		return formResponse("413 Payload Too Large");
	else if (code == "500")
		return formResponse("500 Internal Server Error");
	else if (code == "201")
		return formResponse("201 Created");
	else if (code == "204")
		return formResponse("204 No Content");
	return formResponse("500 Internal Server Error");
}

std::string Response::status(std::string code, std::string statusDir, std::ifstream& file200) {
	if (code == "200")
		return status200(file200);
	std::string status = statusDir + "/" + code + ".html";
	std::ifstream file(status.c_str());
	if (!file.good())
		return status200(file200);

	std::stringstream ss;
	ss << file.rdbuf();
	return statusWithBody(code, ss.str());
}

std::string Response::status(std::string code, std::string statusDir) {
	std::string status = statusDir + "/" + code + ".html";
	std::ifstream file(status.c_str());
	if (!file.good())
		return defaultStatus(code);

	std::stringstream ss;
	ss << file.rdbuf();
	return statusWithBody(code, ss.str());
}

std::string Response::status(std::string code, const ServerConfig &config) {
	int statusCode = std::atoi(code.c_str());
	std::map<int, std::string>::const_iterator it = config.errorPages.find(statusCode);
	if (it != config.errorPages.end()) {
		std::ifstream file(it->second.c_str());
		if (file.good()) {
			std::stringstream ss;
			ss << file.rdbuf();
			return statusWithBody(code, ss.str());
		}
	}
	return status(code, config.statusDir);
}

std::string Response::status(std::string code, std::string statusDir, std::string file200) {
	if (code == "200")
		return status200(file200);
	std::string status = statusDir + "/" + code + ".html";
	std::ifstream file(status.c_str());
	if (!file.good())
		return status200(file200);

	std::stringstream ss;
	ss << file.rdbuf();
	return statusWithBody(code, ss.str());
}
