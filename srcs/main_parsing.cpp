#include "config/config.hpp"
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
    return 0;
}
