# webserv
http web server written in C++98

Config: parse config file into ServerConfig/RouteConfig

# todo

Server: create sockets, run poll() loop
Request: parse incoming HTTP bytes into method/path/headers/body
Response: build HTTP response string from request + config
Client: one connection, state machine (reading / writing / done)
CGI
