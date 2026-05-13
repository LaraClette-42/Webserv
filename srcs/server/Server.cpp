#include "Server.hpp"
#include "../http/HttpRequest.hpp"
#include "../http/HttpResponse.hpp"
#include "../http/HttpStatus.hpp"
#include "../utils/utils.hpp"

Server::Server(const std::vector<ConfigBlock>& config)
    : _servers(config) {
    if (_servers.empty()) {
        throw std::runtime_error("At least one listening port is required");
    }
}

Server::~Server() {
    for (std::vector<int>::const_iterator it = _server_fds.begin(); it != _server_fds.end(); ++it) {
        close(*it);
    }
}

bool Server::isServerFd(int fd) const {
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

int Server::createSocket(const ConfigBlock& config) {
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
        setNonBlockingFd(fd);
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
        setNonBlockingFd(new_fd);
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
    char buf[4096];

    ssize_t nbytes = 0;
    while ((nbytes = recv(clientFd, buf, sizeof(buf), 0)) > 0) {
        client.read_buffer.append(buf, static_cast<std::size_t>(nbytes));
        client.last_activity = std::time(NULL);
    }
    if (nbytes == 0) {
        removeClient(clientFd, fds);
        return;
    }
    if (nbytes < 0 && (errno != EAGAIN && errno != EWOULDBLOCK)) {
        removeClient(clientFd, fds);
        return;
    }
    HttpRequest request = HttpParser::parse(client.read_buffer);
    if (request.status == HTTP_INCOMPLETE_REQUEST) {
        return;
    }
    if (request.status != HTTP_OK) {
        removeClient(clientFd, fds);
        return;
    }
    client.requests.push_back(request);
    client.read_buffer.clear();
    if (!client.write_buffer.empty() || client.requests.empty())
        return;
    ConfigBlock activeServer = _servers.front();
    std::vector<int>::const_iterator listenIt =
        std::find(_server_fds.begin(), _server_fds.end(), client.listen_fd);
    if (listenIt != _server_fds.end()) {
        std::size_t index = static_cast<std::size_t>(listenIt - _server_fds.begin());
        if (index < _servers.size())
            activeServer = _servers[index];
    }

    HttpResponse response = HttpResponseBuilder::build(client.peer_addr, client.requests.front(), activeServer);
    client.requests.erase(client.requests.begin());
    client.write_buffer = response.serialize();
    client.write_offset = 0;
    client.should_close = true;
    client.state = Client::WRITING_RESPONSE;

    while (_clients.find(clientFd) != _clients.end()) {
        Client& c = _clients.find(clientFd)->second;
        if (c.write_offset >= c.write_buffer.size())
            break;
        const std::size_t before = c.write_offset;
        handleClientWrite(clientFd, fds);
        if (_clients.find(clientFd) == _clients.end())
            return;
        if (_clients.find(clientFd)->second.write_offset == before)
            break;
    }
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
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        }
        removeClient(clientFd, fds);
        return;
    }
    client.write_offset += static_cast<std::size_t>(sent);
    if (client.write_offset >= client.write_buffer.size()) {
        if (client.should_close) {
            removeClient(clientFd, fds);
            return;
        }
        ConfigBlock activeServer = _servers.front();
        std::vector<int>::const_iterator listenIt =
            std::find(_server_fds.begin(), _server_fds.end(), client.listen_fd);
        if (listenIt != _server_fds.end()) {
            std::size_t index = static_cast<std::size_t>(listenIt - _server_fds.begin());
            if (index < _servers.size())
                activeServer = _servers[index];
        }
        if (!client.requests.empty()) {
            HttpResponse response = HttpResponseBuilder::build(client.peer_addr,client.requests.front(), activeServer);
            client.requests.erase(client.requests.begin());
            client.write_buffer = response.serialize();
            client.write_offset = 0;
            client.state = Client::WRITING_RESPONSE;
            return;
        }
        client.read_buffer.clear();
        client.write_buffer.clear();
        client.write_offset = 0;
        client.state = Client::READING_HEADERS;
    }
}

void Server::run() {
    for (std::vector<ConfigBlock>::const_iterator it = _servers.begin(); it != _servers.end(); ++it) {
        _server_fds.push_back(createSocket(*it));
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
            if (isServerFd(fdCurrent)) {
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

}
//for the moment, multiple server on the same port is not supported
// in Nginx, it is supported by using a server block with the same port
// but different server_name -> check later