#include "Config.hpp"
#include <cassert>
#include <iostream>
#include <stdexcept>

static void test_valid_conf() {
    Config cfg("tests/confs/valid.conf");
    const std::vector<ServerConfig> &servers = cfg.getServers();

    assert(servers.size() == 1);
    assert(servers[0].host == "127.0.0.1");
    assert(servers[0].port == 8080);
    // assert(servers[0].client_max_body_size == 1048576);
    // assert(servers[0].error_pages.count(404) == 1);
    // assert(servers[0].error_pages.at(404) == "/errors/404.html");

    assert(servers[0].routes.size() == 2);

    const RouteConfig &r0 = servers[0].routes[0];
    assert(r0.path == "/");
    assert(r0.methods.size() == 3);
    assert(r0.root == "./www");
    assert(r0.index == "index.html");
    // assert(r0.autoindex == false);
    // assert(r0.upload_store == "./uploads");
    // assert(r0.cgi.count(".php") == 1);
    // assert(r0.cgi.at(".php") == "/usr/bin/php-cgi");

    const RouteConfig &r1 = servers[0].routes[1];
    assert(r1.path == "/old");
    // assert(r1.redirect == "http://localhost:8080/");

    std::cout << "test_valid_conf         PASS" << std::endl;
}

static void test_missing_brace() {
    try {
        Config cfg("tests/confs/missing_brace.conf");
        assert(false && "should have thrown");
    } catch (std::exception &e) {
        std::cout << "test_missing_brace      PASS" << std::endl;
    }
}

static void test_unknown_directive() {
    try {
        Config cfg("tests/confs/unknown_directive.conf");
        assert(false && "should have thrown");
    } catch (std::exception &e) {
        std::cout << "test_unknown_directive  PASS" << std::endl;
    }
}

int main() {
    test_valid_conf();
    test_missing_brace();
    test_unknown_directive();
    return 0;
}
