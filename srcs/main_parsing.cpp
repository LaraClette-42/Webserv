#include "config/config.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include <iostream>
#include <set>
#include <map>
#include <vector>

static void printBlock(const ConfigBlock &block, int indent) {
    std::string tab(indent * 2, ' ');

    if (!block.host.empty())
        std::cout << tab << "listen:               " << block.host << ":" << block.port << "\n";
    if (!block.server_name.empty())
        std::cout << tab << "server_name:          " << block.server_name << "\n";
    if (!block.root.empty())
        std::cout << tab << "root:                 " << block.root << "\n";
    if (!block.index.empty()) {
        std::cout << tab << "index:                ";
        for (size_t i = 0; i < block.index.size(); ++i)
            std::cout << block.index[i] << " ";
        std::cout << "\n";
    }
    if (block.client_max_body_size != 1048576)
        std::cout << tab << "client_max_body_size: " << block.client_max_body_size << "\n";
    if (!block.error_pages.empty()) {
        for (std::map<int, std::string>::const_iterator it = block.error_pages.begin(); it != block.error_pages.end(); ++it)
            std::cout << tab << "error_page:           " << it->first << " " << it->second << "\n";
    }
    if (!block.allow_methods.empty()) {
        std::cout << tab << "allow_methods:        ";
        for (std::set<std::string>::const_iterator it = block.allow_methods.begin(); it != block.allow_methods.end(); ++it)
            std::cout << *it << " ";
        std::cout << "\n";
    }
    if (block.autoindex)
        std::cout << tab << "autoindex:            on\n";
    if (!block.upload_store.empty())
        std::cout << tab << "upload_store:         " << block.upload_store << "\n";
    if (!block.redirect.empty())
        std::cout << tab << "return:               " << block.redirect << "\n";
    if (!block.cgi_pass.empty()) {
        for (std::map<std::string, std::string>::const_iterator it = block.cgi_pass.begin(); it != block.cgi_pass.end(); ++it)
            std::cout << tab << "cgi_pass:             " << it->first << " " << it->second << "\n";
    }
    for (std::map<std::string, ConfigBlock>::const_iterator it = block.locations.begin(); it != block.locations.end(); ++it) {
        std::cout << tab << "location " << it->first << " {\n";
        printBlock(it->second, indent + 1);
        std::cout << tab << "}\n";
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: webserv <config_file>\n";
        return 1;
    }
    try {
        ConfigFile config;
        config.parse(argv[1]);

        const std::vector<ConfigBlock> &servers = config.getServers();
        std::cout << servers.size() << " server(s) found\n\n";
        for (size_t i = 0; i < servers.size(); ++i) {
            std::cout << "server " << i << " {\n";
            printBlock(servers[i], 1);
            std::cout << "}\n\n";
        }
    } catch (std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    std::cout << "TEST HTTP parsing\n\n";

    std::string raw =
        "POST /upload?user=42 HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 13\r\n"
        "\r\n"
        "Hello, world!";

    HttpRequest req = HttpParser::parse(raw);
    std::cout << "status:  " << req.status  << "\n";
    std::cout << "method:  " << req.method  << "\n";
    std::cout << "path:    " << req.path    << "\n";
    std::cout << "query:   " << req.query   << "\n";
    std::cout << "version: " << req.version << "\n";
    std::cout << "headers:\n";
    for (std::map<std::string, std::string>::const_iterator it = req.headers.begin(); it != req.headers.end(); ++it)
        std::cout << "  " << it->first << ": " << it->second << "\n";
    std::cout << "body:    " << req.body << "\n";

    std::cout << "\nHTTP response (GET)\n\n";

    ConfigBlock testConfig;
    testConfig.root  = "./www";
    testConfig.index.push_back("index.html");

    HttpRequest getRequest;
    getRequest.method  = "GET";
    getRequest.path    = "/";
    getRequest.version = "HTTP/1.1";

    HttpResponse response = HttpResponseBuilder::build(getRequest, testConfig);
    std::cout << response.serialize();

    std::cout << "\nHTTP response (404)\n\n";

    getRequest.path = "/missing.html";
    HttpResponse notFound = HttpResponseBuilder::build(getRequest, testConfig);
    std::cout << notFound.serialize();

    return 0;
}
