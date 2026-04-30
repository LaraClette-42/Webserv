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

class Server {
private:
    std::vector<int> _listen_fds;
    std::vector<int> _ports;
    std::map<int, Client> _clients;
    unsigned int _host;

    int createTCP(int port);
    bool isListeningFd(int fd) const;
    void handleNewConnection(int listenFd, fd_set& fds, int& fdMax);
    void handleClientRead(int clientFd, fd_set& fds);
    void handleClientWrite(int clientFd, fd_set& fds);
    void removeClient(int clientFd, fd_set& fds);
    static std::string formatIPv4(const sockaddr_in& addr);

public:
    Server(const std::vector<int>& ports, unsigned int host = INADDR_ANY);
    ~Server();

    void run();

};

#endif