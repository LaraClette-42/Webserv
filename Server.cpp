#include "Server.hpp"

std::string returnIPAddress(sockaddr_in *client_addr) {
    uint32_t ip = client_addr->sin_addr.s_addr;

    std::stringstream ss;
    ss << (ip & 0xFF) << "." 
    << ((ip >> 8) & 0xFF) << "."
    << ((ip >> 16) & 0xFF) << "."
    << ((ip << 24) & 0xFF);
    std::string str = ss.str();
    return str;  
}

int Server::createTCP() {
    
    _fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_fd == -1)
        throw std::runtime_error("Cannot create socket");
    
	memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(_port);
    server_addr.sin_addr.s_addr = htonl(_host);
    std::string str = returnIPAddress(&server_addr);
    std::cout << "New connexion from: " << str << std::endl;
	if (bind(_fd,(sockaddr*)&server_addr, sizeof(server_addr)) == -1)
		throw std::runtime_error("Cannot bind to IP port");

	if (listen(_fd, 128) == -1) //setup max connections regarding the need
		throw std::runtime_error("Could not listen");
	//create the poll() loop before accept() and recv()

    fd_set fds, readfds;
    FD_ZERO(&fds);
    FD_SET(_fd, &fds);
    int fd_max = _fd;
    socklen_t addrlen;
    struct sockaddr_storage client_saddr;
    unsigned int client_port;
    unsigned int client_addr;
    //using select 
    while (1) {
        readfds = fds;
        if (select(fd_max + 1, &readfds, NULL, NULL, NULL) == - 1)
            throw std::runtime_error("Select error");
        for (int fdCurrent = 0; fdCurrent < fd_max + 1; fdCurrent++) {
            if (FD_ISSET(fdCurrent, &readfds)) {
                if (fdCurrent == _fd) {
                    addrlen = sizeof(struct sockaddr_storage);
                    int new_fd;
                    if ((new_fd = accept(_fd, (sockaddr *)&client_saddr, &addrlen)) == -1)
                        throw std::runtime_error("Accept error"); //continue;
                    FD_SET(new_fd, &fds);
                    if (new_fd > fd_max)
                        fd_max = new_fd;
                    if (client_saddr.ss_family == AF_INET) {
                        sockaddr_in *client_saddr_in = (sockaddr_in *)&client_saddr;
                        client_saddr_in->sin_port = htons(client_port);
                        client_saddr_in->sin_addr.s_addr = htonl(client_addr);
                        std::string str = returnIPAddress(client_saddr_in);
                        std::cout << "New connexion from: " << str << std::endl;
                    }
                } else {
                char buf[1024];
                int nbytes = recv(fdCurrent, buf, sizeof(buf), 0);
                if (nbytes <= 0) {
                    close(fdCurrent);
                    FD_CLR(fdCurrent, &fds);
                } 
                else {
                    send(fdCurrent, buf, nbytes, 0); 
                }
                }
            }
        }
    }
    //using poll() 
}





