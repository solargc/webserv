*This project has been created as part of the 42 curriculum by pascal, jacobus.*

# webserv

## Description

webserv is a lightweight HTTP/1.1 server written in C++98, built from scratch without the use of any external libraries. The goal of the project is to gain a deep understanding of how web servers work by implementing one, including socket management, request parsing, response generation, CGI execution, file uploads, and directory listing.

The server is non-blocking and uses `poll()` to handle multiple simultaneous connections efficiently. It supports GET, POST, and DELETE methods, chunked transfer encoding, CGI scripts via `execve`, configurable virtual hosts, custom error pages, and redirects — all driven by a configuration file with nginx-inspired syntax.

## Instructions

### Compilation

```bash
make
```

This produces the `webserv` binary at the root of the repository.

### Running the server

```bash
./webserv config/webserv_mandatory.conf
```

### Tester website

A browser-based tester page is included at `www/tester.html`. Open your browser and navigate to:

```
http://localhost:8081/tester.html
```

### Stopping the server

```bash
kill $(pgrep webserv)
```

Or press Ctrl+C in the terminal where the server is running.

---

## Configuration

The configuration file uses an nginx-inspired syntax. A minimal example:

```
server {
    listen 8080;
    host 127.0.0.1;
    status_directory ./www/error;

    location / {
        methods GET POST DELETE;
        root ./www;
        index index.html;
        upload_store ./www/posts;
    }

    location /cgi-bin {
        methods GET POST;
        root ./www/cgi-bin;
        cgi_extension .py /usr/bin/python3;
    }
}
```

Supported directives: `listen`, `host`, `client_max_body_size`, `status_directory`, `error_page`, `location`, `methods`, `root`, `index`, `autoindex`, `upload_store`, `return`, `cgi_extension`.

---

## curl Examples

The following commands assume the tester config is running on port 8081. They can be used to manually verify server behaviour or as part of a leak/fd testing loop.

**Static GET — serve a file**
```bash
curl -v "http://localhost:8081/index.html"
```
Tests that the server correctly reads and returns a static file with a 200 response.

**Static GET — autoindex directory listing**
```bash
curl -v "http://localhost:8081/listing"
```
Tests that the server generates an HTML directory listing when `autoindex on` is set and no index file exists.

**Static GET — redirect**
```bash
curl -v -L "http://localhost:8081/old"
```
Tests that the server returns a 301 redirect to `/` and that curl follows it.

**Static GET — custom 404 error page**
```bash
curl -v "http://localhost:8081/doesnotexist"
```
Tests that the server returns a 404 with the custom error page defined in the config.

**Static POST — file upload**
```bash
curl -v -X POST "http://localhost:8081/" \
  -H "Content-Type: text/plain" \
  --data-binary "hello from curl"
```
Tests that the server saves the request body as a new file in `./www/posts` and returns 201.

**DELETE — remove an uploaded file**
```bash
curl -v -X DELETE "http://localhost:8081/posts/post1"
```
Tests that the server deletes the specified file and returns 204. Replace `post1` with a file that exists.

**CGI GET — hello world script**
```bash
curl -v "http://localhost:8081/cgi-bin/helloWorld.py"
```
Tests that the server correctly executes a CGI script on a GET request and returns its output.

**CGI GET — addition via query string**
```bash
curl -v "http://localhost:8081/cgi-bin/addition.py?a=8&b=5"
```
Tests CGI execution with query string parameters passed via the environment.

**CGI POST — addition via request body**
```bash
curl -v -X POST "http://localhost:8081/cgi-bin/addition.py" \
  -H "Content-Type: text/plain" \
  --data-binary "8+5"
```
Tests that the server pipes the request body to the CGI script's stdin and returns its stdout as the response body.

**CGI POST — print string**
```bash
curl -v -X POST "http://localhost:8081/cgi-bin/printString.py" \
  -H "Content-Type: text/plain" \
  --data-binary "hello webserv"
```
Tests CGI POST with a plain text body passed to the script.

**Stress test — fd leak check**
```bash
for i in $(seq 1 50); do
  curl -s "http://localhost:8081/cgi-bin/helloWorld.py" > /dev/null
done
lsof -p $(pgrep webserv) | wc -l
```
Sends 50 CGI requests in a loop and counts open file descriptors. The count should remain stable.

---

## Resources

### HTTP and networking

- [RFC 7230 — HTTP/1.1 Message Syntax and Routing](https://datatracker.ietf.org/doc/html/rfc7230)
- [RFC 7231 — HTTP/1.1 Semantics and Content](https://datatracker.ietf.org/doc/html/rfc7231)
- [RFC 3875 — The Common Gateway Interface (CGI/1.1)](https://datatracker.ietf.org/doc/html/rfc3875)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [MDN — HTTP overview](https://developer.mozilla.org/en-US/docs/Web/HTTP/Overview)
- [poll() man page](https://man7.org/linux/man-pages/man2/poll.2.html)

### CGI

- [CGI Programming on the World Wide Web — O'Reilly](https://www.oreilly.com/library/view/cgi-programming-on/9781565921689/)
- [execve() man page](https://man7.org/linux/man-pages/man2/execve.2.html)

### Testing and debugging

- [Valgrind documentation](https://valgrind.org/docs/manual/manual.html)
- [curl documentation](https://curl.se/docs/)

### AI usage

AI tools were used during this project to support learning and reduce repetitive tasks, in line with the 42 AI usage guidelines. All AI-generated content was reviewed, tested, and understood before being used.

Specific uses included:

- Asking for explanations of networking concepts such as `poll()`, non-blocking I/O, and CGI environment variables to build understanding before writing the relevant code.
- Using AI as a sounding board when debugging — describing a problem and comparing its suggestions against our own reasoning and peer review.
- Generating curl commands for manual testing, which were then verified by running them and checking the results.
- Reviewing completed code sections for potential fd leaks and double-close bugs, then validating the findings manually and with Valgrind.
- Generating this README, which was reviewed and edited to accurately reflect the project.

All core implementation — including the server loop, config parser, request/response handling, and CGI execution — was written by the team. Any AI suggestion that touched implementation was only accepted after being fully understood and discussed with a peer.
