#include "Server.hpp"
#include "SocketSetup.hpp"
#include "Request.hpp"
#include "Response.hpp"

#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <fcntl.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

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

static std::string pathWithoutQuery(const std::string &path) {
	size_t query = path.find('?');
	if (query == std::string::npos)
		return path;
	return path.substr(0, query);
}

static std::string queryString(const std::string &path) {
	size_t query = path.find('?');
	if (query == std::string::npos)
		return "";
	return path.substr(query + 1);
}

static std::string headerValue(const Request &req, const std::string &name) {
	for (std::map<std::string, std::string>::const_iterator it =
			 req.headers.begin(); it != req.headers.end(); ++it) {
		if (it->first.size() != name.size())
			continue;
		bool match = true;
		for (size_t i = 0; i < name.size(); i++) {
			if (std::tolower(static_cast<unsigned char>(it->first[i])) !=
				std::tolower(static_cast<unsigned char>(name[i]))) {
				match = false;
				break;
			}
		}
		if (match)
			return it->second;
	}
	return "";
}

static std::string numberToString(size_t value) {
	std::ostringstream ss;
	ss << value;
	return ss.str();
}

static std::string intToString(int value) {
	std::ostringstream ss;
	ss << value;
	return ss.str();
}

static std::string headerToEnvName(const std::string &header) {
	std::string env = "HTTP_";
	for (size_t i = 0; i < header.size(); i++) {
		if (header[i] == '-')
			env += '_';
		else
			env += std::toupper(static_cast<unsigned char>(header[i]));
	}
	return env;
}

static std::vector<std::string> buildCgiEnv(const Request &req,
											const RouteConfig *route,
											const ServerConfig *config,
											const std::string &scriptPath) {
	std::vector<std::string> env;
	std::string contentLength = headerValue(req, "Content-Length");
	if (contentLength.empty() && !req.body.empty())
		contentLength = numberToString(req.body.size());

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("REQUEST_METHOD=" + req.method);
	env.push_back("QUERY_STRING=" + queryString(req.path));
	env.push_back("SCRIPT_FILENAME=" + scriptPath);
	env.push_back("SCRIPT_NAME=");
	env.push_back("PATH_INFO=" + scriptPath);
	env.push_back("SERVER_PROTOCOL=" + req.version);
	env.push_back("SERVER_SOFTWARE=webserv");
	env.push_back("SERVER_NAME=" + config->host);
	env.push_back("SERVER_PORT=" + intToString(config->port));
	env.push_back("DOCUMENT_ROOT=" + route->documentRoot);
	env.push_back("CONTENT_LENGTH=" + contentLength);
	env.push_back("CONTENT_TYPE=" + headerValue(req, "Content-Type"));
	env.push_back("REDIRECT_STATUS=200");

	for (std::map<std::string, std::string>::const_iterator it =
			 req.headers.begin(); it != req.headers.end(); ++it) {
		if (headerToEnvName(it->first) == "HTTP_CONTENT_LENGTH" ||
			headerToEnvName(it->first) == "HTTP_CONTENT_TYPE")
			continue;
		env.push_back(headerToEnvName(it->first) + "=" + it->second);
	}
	return env;
}

static bool findCgiInterpreter(const Request &req, const RouteConfig *route,
							   std::string &interpreter) {
	std::string path = pathWithoutQuery(req.path);
	for (std::map<std::string, std::string>::const_iterator it =
			 route->cgi.begin(); it != route->cgi.end(); ++it) {
		const std::string &extension = it->first;
		if (path.size() >= extension.size() &&
			path.compare(path.size() - extension.size(), extension.size(),
						 extension) == 0) {
			interpreter = it->second;
			return true;
		}
	}
	return false;
}

static std::string resolveScriptPath(Request req, const RouteConfig *route) {
	req.path = pathWithoutQuery(req.path);
	return Response::resolvePath(req, *route);
}

static std::string directoryName(const std::string &path) {
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return ".";
	if (slash == 0)
		return "/";
	return path.substr(0, slash);
}

static std::string baseName(const std::string &path) {
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return path;
	return path.substr(slash + 1);
}

static std::string absolutePath(const std::string &path) {
	if (path.empty() || path[0] == '/')
		return path;
	char cwd[4096];
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		return path;
	return std::string(cwd) + "/" + path;
}

static bool setCloseOnExec(int fd) {
	int flags = fcntl(fd, F_GETFD);
	if (flags < 0)
		return false;
	return fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
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
		std::string err = Response::status("400", *config);
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
		std::string err = Response::status("405", *config);
		client->appendSendData(err);
	}
}

void Server::handlePost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	std::string interpreter;
	if (findCgiInterpreter(req, route, interpreter))
		CGIPost(req, client, route, config, interpreter);
	else
		StaticPost(req, client, route, config);
}

void Server::CGIPost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config, const std::string &interpreter) {
	int stdinPipe[2];
	int stdoutPipe[2];
	if (pipe(stdinPipe) < 0) {
		std::string err = Response::status("500", *config);
		client->appendSendData(err);
		return;
	}
	if (pipe(stdoutPipe) < 0) {
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		std::string err = Response::status("500", *config);
		client->appendSendData(err);
		return;
	}
	if (!setCloseOnExec(stdinPipe[0]) || !setCloseOnExec(stdinPipe[1]) ||
		!setCloseOnExec(stdoutPipe[0]) || !setCloseOnExec(stdoutPipe[1])) {
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		close(stdoutPipe[0]);
		close(stdoutPipe[1]);
		std::string err = Response::status("500", *config);
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
		std::string file = resolveScriptPath(req, route);
		std::string scriptDir = directoryName(file);
		std::string scriptName = baseName(file);
		std::string exe = absolutePath(interpreter);
		std::string absScript = absolutePath(file);
		if (chdir(scriptDir.c_str()) != 0)
			std::exit(1);
		std::vector<std::string> env = buildCgiEnv(req, route, config, absScript);
		std::vector<char *> envp;
		for (size_t i = 0; i < env.size(); i++)
			envp.push_back(const_cast<char *>(env[i].c_str()));
		envp.push_back(NULL);
		char *argv[] = {const_cast<char*>(exe.c_str()), const_cast<char*>(scriptName.c_str()), NULL};
		execve(exe.c_str(), argv, &envp[0]);
		std::exit(1);
	}
	else {
		close(stdinPipe[0]);
		close(stdoutPipe[1]);
		if (fcntl(stdinPipe[1], F_SETFL, O_NONBLOCK) < 0 ||
			fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK) < 0) {
			close(stdinPipe[1]);
			close(stdoutPipe[0]);
			std::string err = Response::status("500", *config);
			client->appendSendData(err);
			return;
		}
		client->startCgi(pid, stdinPipe[1], stdoutPipe[0], req.body);
		registerCgiFd(stdoutPipe[0], client, POLL_CGI_STDOUT);
		if (req.body.empty()) {
			close(stdinPipe[1]);
			client->markCgiStdinClosed();
		} else {
			registerCgiFd(stdinPipe[1], client, POLL_CGI_STDIN);
		}
	}
}

void Server::StaticPost(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	std::cout << "static post entered" << std::endl;
	if (route->uploadPath.empty()) {
		std::string err = Response::status("403", *config);
		client->appendSendData(err);
		return;
	}
	int i = 1;
	std::stringstream newFile;
	newFile << route->uploadPath << "/post" << i; 
	if (!directoryExists(route->uploadPath.c_str())) {
		std::string err = Response::status("404", *config);
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
	
	std::string res = Response::status("201", *config);
	client->appendSendData(res);
}

void Server::handleGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	std::string interpreter;
	if (findCgiInterpreter(req, route, interpreter))
		CGIGet(req, client, route, config, interpreter);
	else
		StaticGet(req, client, route, config);
}

void Server::CGIGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config, const std::string &interpreter) {
	int stdinPipe[2];
	int stdoutPipe[2];
	if (pipe(stdinPipe) < 0) {
		std::string err = Response::status("500", *config);
		client->appendSendData(err);
		return;
	}
	if (pipe(stdoutPipe) < 0) {
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		std::string err = Response::status("500", *config);
		client->appendSendData(err);
		return;
	}
	if (!setCloseOnExec(stdinPipe[0]) || !setCloseOnExec(stdinPipe[1]) ||
		!setCloseOnExec(stdoutPipe[0]) || !setCloseOnExec(stdoutPipe[1])) {
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		close(stdoutPipe[0]);
		close(stdoutPipe[1]);
		std::string err = Response::status("500", *config);
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
		std::string file = resolveScriptPath(req, route);
		std::string scriptDir = directoryName(file);
		std::string scriptName = baseName(file);
		std::string exe = absolutePath(interpreter);
		std::string absScript = absolutePath(file);
		if (chdir(scriptDir.c_str()) != 0)
			std::exit(1);
		std::vector<std::string> env = buildCgiEnv(req, route, config, absScript);
		std::vector<char *> envp;
		for (size_t i = 0; i < env.size(); i++)
			envp.push_back(const_cast<char *>(env[i].c_str()));
		envp.push_back(NULL);
		char *argv[] = {const_cast<char*>(exe.c_str()), const_cast<char*>(scriptName.c_str()), NULL};
		execve(exe.c_str(), argv, &envp[0]);
		std::exit(1);
	}
	else {
		close(stdinPipe[0]);
		close(stdoutPipe[1]);
		if (fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK) < 0) {
			close(stdinPipe[1]);
			close(stdoutPipe[0]);
			std::string err = Response::status("500", *config);
			client->appendSendData(err);
			return;
		}
		client->startCgi(pid, stdinPipe[1], stdoutPipe[0], "");
		close(stdinPipe[1]);
		client->markCgiStdinClosed();
		registerCgiFd(stdoutPipe[0], client, POLL_CGI_STDOUT);
	}
}

void Server::StaticGet(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	Response response(req, *route, *config);
	std::string raw = response.getRaw();
	client->appendSendData(raw);
}

void Server::handleDelete(Request req, Client *client, const RouteConfig *route, const ServerConfig* config) {
	std::string file = Response::resolvePath(req, *route);
	if (!fileExists(file)) {
		std::string err = Response::status("404", *config);
		client->appendSendData(err);
		return;
	}
	if (std::remove(file.c_str()) != 0) {
		std::string err = Response::status("500", *config);
		client->appendSendData(err);
		return;
	}
	std::string res = Response::status("204", *config);
	client->appendSendData(res);
}
