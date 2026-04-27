#ifndef SERVER_HPP
#define SERVER_HPP

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
class Server {
    private:
    long _fd;
    int _port;
    unsigned int _host;
    sockaddr_in server_addr;
    
    

    Server();

    int createTCP();

};

#endif