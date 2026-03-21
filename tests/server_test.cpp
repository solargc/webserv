#include "Config.hpp"
#include "Server.hpp"
#include <arpa/inet.h>
#include <cassert>
#include <csignal>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

static void test_server_accepts_connection() {
    Config cfg("tests/confs/minimal.conf");
    Server server(cfg.getServers());

    pid_t pid = fork();
    if (pid == 0) {
        server.run();
        exit(0);
    }

    sleep(1);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_aton("127.0.0.1", &addr.sin_addr);

    int result = connect(fd, (sockaddr *)&addr, sizeof(addr));
    close(fd);

    kill(pid, SIGTERM);
    waitpid(pid, NULL, 0);

    assert(result == 0);
    std::cout << "test_server_accepts_connection  PASS" << std::endl;
}

int main() {
    test_server_accepts_connection();
    return 0;
}
