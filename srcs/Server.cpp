#include "Server.hpp"
#include "Response.hpp"




Server::Server(const std::vector<ConfigBlock>& config)
    : _configserver(config) {
    if (_configserver.empty()) {
        throw std::runtime_error("At least one listening port is required");
    }
}

Server::~Server() {
    for (std::vector<int>::const_iterator it = _server_fds.begin(); it != _server_fds.end(); ++it) {
        close(*it);
    }
}



static void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("fcntl(F_GETFL) failed");
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("fcntl(F_SETFL) failed");
    }
}

std::string Server::formatIPv4(const sockaddr_in& addr) {
    const unsigned int ip = ntohl(addr.sin_addr.s_addr);
    const unsigned int a = (ip >> 24) & 0xFF;
    const unsigned int b = (ip >> 16) & 0xFF;
    const unsigned int c = (ip >> 8) & 0xFF;
    const unsigned int d = ip & 0xFF;

    std::stringstream ss;
    ss << a << "." << b << "." << c << "." << d << ":" << ntohs(addr.sin_port);
    return ss.str();
}

bool Server::isListeningFd(int fd) const {
    return std::find(_server_fds.begin(), _server_fds.end(), fd) != _server_fds.end();
}

void Server::removeClient(int clientFd, fd_set& fds) {
    close(clientFd);
    FD_CLR(clientFd, &fds);
    _clients.erase(clientFd);
}

uint32_t getHost(std::string host) {
    uint32_t res = 0;
    std::istringstream iss(host);
    std::string segment;
    int shift = 0;

    while (std::getline(iss, segment, '.')) {
    if (shift > 24) 
        break; 
    int val = static_cast<uint32_t>(std::atoi(segment.c_str()));
    res |= (val << shift);
    shift += 8;
      
}
    return res;
}


int Server::createTCP(const ConfigBlock& config) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        throw std::runtime_error("Cannot create socket");
    }

    const int enable = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1) {
        close(fd);
        throw std::runtime_error("Cannot set SO_REUSEADDR");
    }
    try {
        setNonBlocking(fd);
    } catch (...) {
        close(fd);
        throw std::runtime_error("Cannot set listening socket to non-blocking");
    }
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(config.port);

    server_addr.sin_addr.s_addr = getHost(config.host);


    if (bind(fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        close(fd);
        throw std::runtime_error("Cannot bind to IP port");
    }

    if (listen(fd, 1024) == -1) {
        close(fd);
        throw std::runtime_error("Could not listen");
    }

    std::cout << "Listening on " << formatIPv4(server_addr) << std::endl;
    return fd;
}

void Server::handleNewConnection(int listenFd, fd_set& fds, int& fdMax) {
    sockaddr_storage client_saddr;
    socklen_t addrlen = sizeof(client_saddr);
    int new_fd = accept(listenFd, (sockaddr *)&client_saddr, &addrlen);
    if (new_fd == -1)
        return;
    try {
        setNonBlocking(new_fd);
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
        close(new_fd);
        return;
    }
    FD_SET(new_fd, &fds);
    if (new_fd > fdMax) {
        fdMax = new_fd;
    }
    if (client_saddr.ss_family == AF_INET) {
        sockaddr_in client_addr = *(sockaddr_in *)&client_saddr;
        _clients[new_fd] = Client(new_fd, listenFd, client_addr);
        std::cout << "New connection from: " << formatIPv4(client_addr) << std::endl;
    }
}

void Server::handleClientRead(int clientFd, fd_set& fds) {
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) {
        removeClient(clientFd, fds);
        return;
    }
    Client& client = it->second;
    char buf[1024];
    int nbytes = recv(clientFd, buf, sizeof(buf), 0);
    if (nbytes <= 0) {
        removeClient(clientFd, fds);
        return;
    }
    client.last_activity = std::time(NULL);
    client.read_buffer.append(buf, nbytes);
    if (client.read_buffer.find("\r\n\r\n") == std::string::npos) {
        return;
    }
    client.write_buffer = Response::okText("Hello from webserv\n");
    client.write_offset = 0;
    client.should_close = true;
    client.state = Client::WRITING_RESPONSE;
}

void Server::handleClientWrite(int clientFd, fd_set& fds) {
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) {
        removeClient(clientFd, fds);
        return;
    }

    Client& client = it->second;
    if (client.write_offset >= client.write_buffer.size()) {
        client.state = Client::READING_HEADERS;
        return;
    }
    const char* data = client.write_buffer.c_str() + client.write_offset;
    const std::size_t remaining = client.write_buffer.size() - client.write_offset;
    const ssize_t sent = send(clientFd, data, remaining, 0);
    if (sent <= 0) {  
        removeClient(clientFd, fds);
        return;
    }
    client.write_offset += static_cast<std::size_t>(sent);
    if (client.write_offset >= client.write_buffer.size()) {
        if (client.should_close) {
            removeClient(clientFd, fds);
            return;
        }
        client.read_buffer.clear();
        client.write_buffer.clear();
        client.write_offset = 0;
        client.state = Client::READING_HEADERS;
    }
}

void Server::run() {
    for (std::vector<ConfigBlock>::const_iterator it = _configserver.begin(); it != _configserver.end(); ++it) {
        _server_fds.push_back(createTCP(*it));
    }
    fd_set fds, readfds, writefds;
    FD_ZERO(&fds);
    int fd_max = -1;
    for (std::vector<int>::const_iterator it = _server_fds.begin(); it != _server_fds.end(); ++it) {
        FD_SET(*it, &fds);
        if (*it > fd_max) {
            fd_max = *it;
        }
    }
    while (1) {
        readfds = fds;
        FD_ZERO(&writefds);
        for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
            const Client& client = it->second;
            if (client.write_offset < client.write_buffer.size()) {
                FD_SET(it->first, &writefds);
            }
        }

        if (select(fd_max + 1, &readfds, &writefds, NULL, NULL) == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("Select error");
        }
        for (int fdCurrent = 0; fdCurrent < fd_max + 1; fdCurrent++) {
            if (isListeningFd(fdCurrent)) {
                if (FD_ISSET(fdCurrent, &readfds)) {
                    handleNewConnection(fdCurrent, fds, fd_max);
                }
                continue;
            }
            if (FD_ISSET(fdCurrent, &readfds)) {
                handleClientRead(fdCurrent, fds);
            }
            if (FD_ISSET(fdCurrent, &writefds)) {
                handleClientWrite(fdCurrent, fds);
            }
        }
    }
    //handle request in the client.write_buffer() -> send it to hqndle client read. 
    //std::string parserequest(std::String client.writebuffer); 
}
