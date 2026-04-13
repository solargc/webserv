#include "Response.hpp"

std::string Response::error404()
{
	std::string error = "HTTP/1.1 404 Not Found\r\n"
		"Content-Type: text/plain\r\n"
		"Content-Length: 13\r\n"
		"\r\n"
		"404 Not Found";
		return error;
}
