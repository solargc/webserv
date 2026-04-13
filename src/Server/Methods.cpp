#include "Server.hpp"
#include "SocketSetup.hpp"
#include "Request.hpp"
#include "Response.hpp"

#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

void Server::handleGet(Request req, Client *client, RouteConfig *route) {
	Response response(req, *route);
	std::string raw = response.getRaw();
	send(client->getFd(), raw.c_str(), raw.size(), 0); // send() is the couterpart to recv()
}

bool Server::directoryExists(const char* path) {
    struct stat info;

    if (stat(path, &info) != 0) {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
}

void Server::handlePost(Request req, Client *client, RouteConfig *route) {
	int i = 1;
	std::stringstream newFile;
	newFile << route->uploadPath << "/post" << i; 
	if (!directoryExists(route->uploadPath.c_str())) {
		send(client->getFd(), Response::error404().c_str(), Response::error404().size(), 0);
		return;
	}
	for (; access(newFile.str().c_str(), F_OK) == 0; i++) {
		newFile.str("");
		newFile.clear();
		newFile << route->uploadPath << "/post" << i; 
	}
	std::ofstream outNewFile(newFile.str().c_str());
	outNewFile << req.body;
	outNewFile.close();

	std::string created = "HTTP/1.1 201 Created\r\n"
		"Content-Length: 0\r\n"
		"\r\n";
	send(client->getFd(), created.c_str(), created.size(), 0);
}
