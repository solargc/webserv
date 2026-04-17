#include "Server.hpp"
#include "SocketSetup.hpp"
#include "Request.hpp"
#include "Response.hpp"

#include <sys/socket.h>
#include <cstdio>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

bool Server::directoryExists(const char* path) {
    struct stat info;

    if (stat(path, &info) != 0) {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
}

bool Server::fileExists(const std::string& filename) {
    std::ifstream file(filename.c_str());
    return file.good();
}

void Server::handleMethods(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	if (req.method == "GET")
		handleGet(req, client, route, config);
	else if (req.method == "POST")
		handlePost(req, client, route, config);
	else if (req.method == "DELETE")
		handleDelete(req, client, route, config);
	else {
		std::string err = Response::status("405", config->statusDir);
		send(client->getFd(), err.c_str(), err.size(), 0);
		return;
	}
}

void Server::handlePost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	if (req.path.size() < 3 || req.path.substr(req.path.size() - 3) == ".py")
		CGIPost(req, client, route, config);
	else
		StaticPost(req, client, route, config);
}

void Server::CGIPost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	(void)client;
	(void)route;
	(void)req;
	(void)config;
	std::cout << "TEMPORARY POOP" <<std::endl;
}

void Server::StaticPost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	int i = 1;
	std::stringstream newFile;
	newFile << route->uploadPath << "/post" << i; 
	if (!directoryExists(route->uploadPath.c_str())) {
		std::string err = Response::status("404", config->statusDir);
		send(client->getFd(), err.c_str(), err.size(), 0);
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
	
	std::string res = Response::status("201", config->statusDir);
	send(client->getFd(), res.c_str(), res.size(), 0);
}

void Server::handleGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	if (req.path.size() < 3 || req.path.substr(req.path.size() - 3) == ".py")
		CGIGet(req, client, route, config);
	else
		StaticGet(req, client, route, config);
}

void Server::CGIGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	(void)config;
	(void)client;
	int pipefd[2];
	pipe(pipefd);
	pid_t pid = fork();
	if (pid == 0) {
		close(pipefd[0]);
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[1]);
		std::string file = route->documentRoot + req.path;
		char *argv[] = {(char*)"/usr/bin/python3", const_cast<char*>(file.c_str()), NULL};
		char *envp[] = {NULL};
		execve("/usr/bin/python3", argv, envp);
	}
	else {
		close(pipefd[1]);
		char buf[4096];
		int n = read(pipefd[0], buf, sizeof(buf));
		buf[n] = '\0';
		std::cout << buf << std::endl;
		waitpid(pid, NULL, 0);
	}
}

void Server::StaticGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	(void)config;
	Response response(req, *route, config->statusDir);
	std::string raw = response.getRaw();
	send(client->getFd(), raw.c_str(), raw.size(), 0); // send() is the couterpart to recv()
}

void Server::handleDelete(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	(void)config;
	std::string file = route->documentRoot + req.path;
	if (!fileExists(file)) {
		std::string err = Response::status("404", config->statusDir);
		send(client->getFd(), err.c_str(), err.size(), 0);
		return;
	}
	remove(file.c_str());
	std::string deleted = "HTTP/1.1 204 No Content\r\n"
		"Content-Length: 0\r\n"
		"\r\n";
	send(client->getFd(), deleted.c_str(), deleted.size(), 0);
}
