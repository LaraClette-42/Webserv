#ifndef SERVER_HPP
#define SERVER_HPP

#include "Client.hpp"
#include <sys/socket.h>
#include <netinet/in.h> 
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <string.h>
#include <unistd.h> 
#include <sstream>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include "../config/config.hpp"

class Server {
private:
    std::vector<int> _server_fds;
    std::vector<ConfigBlock> _servers;
    std::map<int, Client> _clients;

    int createSocket(const ConfigBlock& port);
    bool isServerFd(int fd) const;
    void handleNewConnection(int listenFd, fd_set& fds, int& fdMax);
    void handleClientRead(int clientFd, fd_set& fds);
    void handleClientWrite(int clientFd, fd_set& fds);
    void removeClient(int clientFd, fd_set& fds);

public:
    Server(const std::vector<ConfigBlock>& config);
    static std::string formatIPv4(const sockaddr_in& addr);

    ~Server();

    void run();

};

#endif