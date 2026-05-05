#include "server/Server.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>


int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config file>" << std::endl;
        return 1;
    }
    try {
        ConfigFile config;
        config.parse(argv[1]);
        const std::vector<ConfigBlock> allservers = config.getServers(); 
        Server server(allservers);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
