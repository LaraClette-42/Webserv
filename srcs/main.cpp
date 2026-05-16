#include "server/Server.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>


int main(int argc, char** argv) {
    // Sujet : "provided as argument on the command line, or available in a default path"
    const char *configPath = "conf/default.conf";
    if (argc >= 2)
        configPath = argv[1];
    try {
        ConfigFile config;
        config.parse(configPath);
        const std::vector<ConfigBlock> allservers = config.getServers(); 
        Server server(allservers);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
