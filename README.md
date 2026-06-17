# webserv

A small HTTP/1.1 server written in C++98, built for the 42 webserv project.

It serves static files, handles GET/POST/DELETE, runs CGI scripts, and is
configured through an nginx-style config file. Everything runs in a single
non-blocking `poll()` event loop, so one process handles all clients, CGI
pipes and listening sockets at once.

## Build

```
make
```

Compiles with `-Wall -Wextra -Werror -std=c++98`. Use `make re` for a clean
rebuild, `make clean`/`make fclean` to remove objects/binary.

## Run

```
./webserv <config_file>
```

For example:

```
./webserv config/webserv.conf
```

## Configuration

A config defines one or more `server` blocks. Each server listens on a
host/port and contains `location` blocks matched by longest path prefix.

```
server {
	listen 8080;
	host 127.0.0.1;
	client_max_body_size 1M;
	status_directory ./www/error;
	error_page 404 ./www/error/404.html;

	location / {
		methods GET POST DELETE;
		root ./www;
		index index.html;
		autoindex off;
		upload_store ./www/posts;
	}

	location /old {
		methods GET;
		return 301 /;
	}

	location /cgi-bin {
		methods GET POST;
		root ./www/cgi-bin;
		cgi_extension .py /usr/bin/python3;
	}
}
```

Supported directives:

- `listen`, `host` — where the server accepts connections
- `client_max_body_size` — reject bodies above this size (e.g. `1M`)
- `status_directory`, `error_page <code> <file>` — custom error pages
- `methods` — allowed HTTP methods for a location
- `root`, `index` — where files are served from
- `autoindex on|off` — directory listing
- `upload_store` — where POST uploads are written
- `return <code> <target>` — redirect
- `cgi_extension <ext> <interpreter>` — run matching files through an interpreter

Example configs live in `config/`.

## Layout

- `src/Config` — tokenizer and parser for the config file
- `src/Server` — socket setup, the poll loop, client state, request handling and CGI
- `src/Http` — request parsing and response building
- `includes` — headers
