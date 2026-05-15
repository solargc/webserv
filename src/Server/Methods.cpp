#include "Server.hpp"
#include "SocketSetup.hpp"
#include "Request.hpp"
#include "Response.hpp"

#include <sys/socket.h>
#include <cstdio>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static bool isSafePath(const std::string &path) {
    size_t i = 0;
    while (i < path.size()) {
        // find next slash or start
        size_t seg = path.find('/', i);
        std::string part = (seg == std::string::npos)
            ? path.substr(i)
            : path.substr(i, seg - i);
        if (part == "..")
            return false;
        if (seg == std::string::npos)
            break;
        i = seg + 1;
    }
    return true;
}

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
	if (!isSafePath(req.path)) {
		std::string err = Response::status("400", config->statusDir);
		client->appendSendData(err);
		return;
	}
	if (req.method == "GET")
		handleGet(req, client, route, config);
	else if (req.method == "POST")
		handlePost(req, client, route, config);
	else if (req.method == "DELETE")
		handleDelete(req, client, route, config);
	else {
		std::string err = Response::status("405", config->statusDir);
		client->appendSendData(err);
	}
}

void Server::handlePost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	if (req.path.size() < 3 || req.path.substr(req.path.size() - 3) != ".py")
		StaticPost(req, client, route, config);
	else
		CGIPost(req, client, route, config);
}

void Server::CGIPost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	int stdinPipe[2];
	int stdoutPipe[2];
	if (pipe(stdinPipe) < 0) {
		std::string err = Response::status("500", config->statusDir);
		client->appendSendData(err);
		return;
	}
	if (pipe(stdoutPipe) < 0) {
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		std::string err = Response::status("500", config->statusDir);
		client->appendSendData(err);
		return;
	}
	pid_t pid = fork();
	if (pid == 0) {
		close(stdinPipe[1]);
		close(stdoutPipe[0]);
		dup2(stdinPipe[0], STDIN_FILENO);
		dup2(stdoutPipe[1], STDOUT_FILENO);
		close(stdinPipe[0]);
		close(stdoutPipe[1]);
		std::string file = route->documentRoot + req.path;
		char *argv[] = {(char*)"/usr/bin/python3", const_cast<char*>(file.c_str()), NULL};
		char *envp[] = {NULL};
		execve("/usr/bin/python3", argv, envp);
	}
	else {
		close(stdinPipe[0]);
		close(stdoutPipe[1]);
		write(stdinPipe[1], req.body.c_str(), req.body.size());
		close(stdinPipe[1]);
		char buf[4096];
		int n = read(stdoutPipe[0], buf, sizeof(buf) - 1);
		if (n < 0) n = 0;
		buf[n] = '\0';
		waitpid(pid, NULL, 0);
		std::string res = Response::status("200", config->statusDir, buf);
		client->appendSendData(res);
	}
}

void Server::StaticPost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	std::cout << "static post entered" << std::endl;
	int i = 1;
	std::stringstream newFile;
	newFile << route->uploadPath << "/post" << i; 
	if (!directoryExists(route->uploadPath.c_str())) {
		std::string err = Response::status("404", config->statusDir);
		client->appendSendData(err);
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
	client->appendSendData(res);
}

void Server::handleGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	if (req.path.size() < 3 || req.path.substr(req.path.size() - 3) != ".py")
		StaticGet(req, client, route, config);
	else
		CGIGet(req, client, route, config);
}

void Server::CGIGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	int pipefd[2];
	if (pipe(pipefd) < 0) {
		std::string err = Response::status("500", config->statusDir);
		client->appendSendData(err);
		return;
	}
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
		int n = read(pipefd[0], buf, sizeof(buf) - 1);
		if (n < 0) n = 0;
		buf[n] = '\0';
		std::cout << buf << std::endl;
		waitpid(pid, NULL, 0);
		std::string res = Response::status("200", config->statusDir, buf);
		client->appendSendData(res);
	}
}

void Server::StaticGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	Response response(req, *route, config->statusDir);
	std::string raw = response.getRaw();
	client->appendSendData(raw);
}

void Server::handleDelete(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	std::string file = route->documentRoot + req.path;
	if (!fileExists(file)) {
		std::string err = Response::status("404", config->statusDir);
		client->appendSendData(err);
		return;
	}
	remove(file.c_str());
	std::string res = Response::status("204", config->statusDir);
	client->appendSendData(res);
}
