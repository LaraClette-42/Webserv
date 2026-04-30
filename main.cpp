#include "Server.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

static int parsePort(const char* value) {
    char* end = NULL;
    long port = std::strtol(value, &end, 10);
    if (*value == '\0' || *end != '\0' || port < 1 || port > 65535) {
        throw std::runtime_error("Invalid port. Use a value between 1 and 65535.");
    }
    return static_cast<int>(port);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port> [more_ports...]" << std::endl;
        return 1;
    }

    try {
        std::vector<int> ports;
        for (int i = 1; i < argc; ++i) {
            ports.push_back(parsePort(argv[i]));
        }

        Server server(ports);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
