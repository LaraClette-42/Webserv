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
    std::vector<int>          _server_fds;
    std::map<int, std::size_t> _fd_to_server;
    std::vector<ConfigBlock>  _servers;
    std::map<int, Client>     _clients;
    int                       _fd_max;

    int createSocket(const ListenAddr& addr);
    bool isServerFd(int fd) const;
    void handleNewConnection(int listenFd, fd_set& fds, int& fdMax);
    void handleClientRead(int clientFd, fd_set& fds);
    void handleClientWrite(int clientFd, fd_set& fds);
    void removeClient(int clientFd, fd_set& fds);
    void processCGIResponse(const HttpResponse &response,
        Client &client, const ConfigBlock &server, fd_set &fds);
    void Server::handleCgiWrite(int clientFd, fd_set& fds);
    void Server::handleCgiRead(int clientFd, fd_set& fds, int& fd_max);
public:
    Server(const std::vector<ConfigBlock>& config);

    ~Server();

    void run();

};

#endif