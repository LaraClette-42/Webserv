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
    Client& client = _clients[clientFd];
    if (client.cgi_pid != -1) {
        kill(client.cgi_pid, SIGKILL);
        waitpid(client.cgi_pid, NULL, 0);
    }
    if (client.cgi_out_fd != -1) { FD_CLR(client.cgi_out_fd, &fds); close(client.cgi_out_fd); }
    if (client.cgi_in_fd  != -1) { FD_CLR(client.cgi_in_fd,  &fds); close(client.cgi_in_fd);  }
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

int Server::createSocket(const ListenAddr& addr) {
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
    server_addr.sin_port = htons(addr.port);
    server_addr.sin_addr.s_addr = getHost(addr.host);
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

void Server::processCGIResponse(Client &client, const ConfigBlock &server, fd_set &fds) {
    std::string strippedPath;
    ConfigBlock tmp = HttpResponseBuilder::resolveConfig(client.requests.front().path, server, strippedPath);
    HttpRequest adjusted = client.requests.front();
    adjusted.path = strippedPath;
    client.requests.erase(client.requests.begin());
    CGI cgi(client.peer_addr, adjusted, tmp);
    CGIFd getCGI;
    try {
        getCGI = cgi.startCGI(tmp);
    } catch (const std::exception &e) {
        HttpResponse err = HttpResponseBuilder::makeError(HTTP_INTERNAL_SERVER_ERROR, server);
        client.write_buffer = err.serialize();
        client.write_offset = 0;
        client.should_close = true;
        client.state = Client::WRITING_RESPONSE;
        return;
    }
    client.cgi_pid           = getCGI.pid;
    client.cgi_in_fd         = getCGI.in_fd;
    client.cgi_out_fd        = getCGI.out_fd;
    client.cgi_start_time    = std::time(NULL);
    client.cgi_body_write = adjusted.body;
    client.cgi_body_offset   = 0;
    client.cgi_output.clear();
    client.state             = Client::CGI_RUNNING;
    FD_SET(getCGI.out_fd, &fds);
    if (getCGI.out_fd > _fd_max)
        _fd_max = getCGI.out_fd;
    if (adjusted.body.empty()) {
        close(getCGI.in_fd);
        client.cgi_in_fd = -1;
    }
    return;
}


void Server::handleClientRead(int clientFd, fd_set& fds) {
    std::map<int, Client>::iterator it = _clients.find(clientFd);
    if (it == _clients.end()) {
        removeClient(clientFd, fds);
        return;
    }
    Client& client = it->second;
    char buf[4096];

    ssize_t nread = recv(clientFd, buf, sizeof(buf), 0);
    if (nread > 0) {
        client.read_buffer.append(buf, static_cast<std::size_t>(nread));
        client.last_activity = std::time(NULL);
    }
    else if (nread == 0) {
        removeClient(clientFd, fds);
        return;
    }
    else {
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
    std::map<int, std::size_t>::const_iterator listenIt = _fd_to_server.find(client.listen_fd);
    if (listenIt != _fd_to_server.end() && listenIt->second < _servers.size())
        activeServer = _servers[listenIt->second];

    HttpResponse response = HttpResponseBuilder::build(client.peer_addr, client.requests.front(), activeServer);
    if (response.status == HTTP_CGI_PENDING) {
        processCGIResponse(client, activeServer, fds);
        return;
    }
    client.requests.erase(client.requests.begin());
    client.write_buffer = response.serialize();
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
        return;
    }
    const char* data = client.write_buffer.data() + client.write_offset;
    const std::size_t remaining = client.write_buffer.size() - client.write_offset;
    const ssize_t nsent = send(clientFd, data, remaining, 0);
    if (nsent > 0)
        client.write_offset += static_cast<std::size_t>(nsent);
    else {
        removeClient(clientFd, fds);
        return;
    }
    if (client.write_offset >= client.write_buffer.size()) {
        if (client.should_close) {
            removeClient(clientFd, fds);
            return;
        }
        ConfigBlock activeServer = _servers.front();
        std::map<int, std::size_t>::const_iterator listenIt = _fd_to_server.find(client.listen_fd);
        if (listenIt != _fd_to_server.end() && listenIt->second < _servers.size())
            activeServer = _servers[listenIt->second];
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


void Server::handleCgiRead(int clientFd, fd_set& fds) {
    Client& client = _clients[clientFd];
    if (client.cgi_out_fd == -1)
        return;
    char buf[4096];
    ssize_t n = read(client.cgi_out_fd, buf, sizeof(buf));

    if (n > 0) {
        client.cgi_output.append(buf, static_cast<std::size_t>(n));
        return;
    }
    FD_CLR(client.cgi_out_fd, &fds);
    close(client.cgi_out_fd);
    client.cgi_out_fd = -1;

    int status = 0;
    waitpid(client.cgi_pid, &status, 0);
    client.cgi_pid = -1;
    if (n < 0) {
        ConfigBlock activeServer = _servers.front();
        std::map<int, std::size_t>::const_iterator listenIt = _fd_to_server.find(client.listen_fd);
        if (listenIt != _fd_to_server.end() && listenIt->second < _servers.size())
            activeServer = _servers[listenIt->second];
        HttpResponse err = HttpResponseBuilder::makeError(HTTP_BAD_GATEWAY, activeServer);
        client.write_buffer = err.serialize();
    } else {
        HttpResponse response = HttpResponseBuilder::parseCGIResponse(client.cgi_output);
        client.write_buffer = response.serialize();
    }

    client.write_offset = 0;
    client.should_close = true;
    client.state        = Client::WRITING_RESPONSE;
}

void Server::handleCgiWrite(int clientFd, fd_set& fds) {
    Client& client = _clients[clientFd];
    if (client.cgi_in_fd == -1)
        return;
    const char* data = client.cgi_body_write.data() + client.cgi_body_offset;
    std::size_t remaining = client.cgi_body_write.size() - client.cgi_body_offset;

    ssize_t w = write(client.cgi_in_fd, data, remaining);
    if (w > 0)
        client.cgi_body_offset += static_cast<std::size_t>(w);
    else {
        FD_CLR(client.cgi_in_fd, &fds);
        close(client.cgi_in_fd);
        client.cgi_in_fd = -1;
        return;
    }

    if (client.cgi_body_offset >= client.cgi_body_write.size()) {
        FD_CLR(client.cgi_in_fd, &fds);
        close(client.cgi_in_fd);
        client.cgi_in_fd = -1;
    }
}

void Server::checkIdleTimeouts(fd_set& fds) {
    const int IDLE_TIMEOUT = 30;
    std::time_t now = std::time(NULL);
    std::vector<int> toRemove;
    for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
        const Client& client = it->second;
        if (client.state == Client::CGI_RUNNING)
            continue;
        if (now - client.last_activity > IDLE_TIMEOUT)
            toRemove.push_back(it->first);
    }
    for (std::size_t i = 0; i < toRemove.size(); ++i)
        removeClient(toRemove[i], fds);
}

void Server::checkCGITimeouts(fd_set& fds) {
    const int CGI_TIMEOUT = 10;
    std::time_t now = std::time(NULL);
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        Client& client = it->second;
        if (client.state != Client::CGI_RUNNING)
            continue;
        if (now - client.cgi_start_time < CGI_TIMEOUT)
            continue;
        kill(client.cgi_pid, SIGKILL);
        waitpid(client.cgi_pid, NULL, 0);
        client.cgi_pid = -1;
        if (client.cgi_out_fd != -1) { FD_CLR(client.cgi_out_fd, &fds); close(client.cgi_out_fd); client.cgi_out_fd = -1; }
        if (client.cgi_in_fd  != -1) { FD_CLR(client.cgi_in_fd,  &fds); close(client.cgi_in_fd);  client.cgi_in_fd  = -1; }
        ConfigBlock activeServer = _servers.front();
        std::map<int, std::size_t>::const_iterator li = _fd_to_server.find(client.listen_fd);
        if (li != _fd_to_server.end() && li->second < _servers.size())
            activeServer = _servers[li->second];
        HttpResponse err = HttpResponseBuilder::makeError(HTTP_GATEWAY_TIMEOUT, activeServer);
        client.write_buffer = err.serialize();
        client.write_offset = 0;
        client.should_close = true;
        client.state = Client::WRITING_RESPONSE;
    }
}

void Server::run() {
    for (std::size_t i = 0; i < _servers.size(); ++i) {
        const std::vector<ListenAddr>& listens = _servers[i].listens;
        for (std::size_t j = 0; j < listens.size(); ++j) {
            int fd = createSocket(listens[j]);
            _server_fds.push_back(fd);
            _fd_to_server[fd] = i;
        }
    }
    fd_set fds, readfds, writefds;
    FD_ZERO(&fds);
    _fd_max = -1;
    for (std::vector<int>::const_iterator it = _server_fds.begin(); it != _server_fds.end(); ++it) {
        FD_SET(*it, &fds);
        if (*it >_fd_max) {
           _fd_max = *it;
        }
    }
    while (1) {
        readfds = fds;
        FD_ZERO(&writefds);
        for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
            const Client& client = it->second;
            if (client.state == Client::CGI_RUNNING) {
                if (client.cgi_out_fd != -1) {
                    FD_SET(client.cgi_out_fd, &readfds);
                    if (client.cgi_out_fd > _fd_max) 
                        _fd_max = client.cgi_out_fd;
                }
                if (client.cgi_in_fd != -1 && client.cgi_body_offset < client.cgi_body_write.size()) {
                    FD_SET(client.cgi_in_fd, &writefds);
                    if (client.cgi_in_fd > _fd_max)
                        _fd_max = client.cgi_in_fd;
                }
            }
            if (client.write_offset < client.write_buffer.size())
                FD_SET(it->first, &writefds);
        }

        std::map<int, int> cgi_out_to_client;
        std::map<int, int> cgi_in_to_client;
        for (std::map<int, Client>::const_iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second.cgi_out_fd != -1)
                cgi_out_to_client[it->second.cgi_out_fd] = it->first;
            if (it->second.cgi_in_fd != -1)
                cgi_in_to_client[it->second.cgi_in_fd] = it->first;
        }

        struct timeval tv;
        tv.tv_sec  = 1;
        tv.tv_usec = 0;
        if (select(_fd_max + 1, &readfds, &writefds, NULL, &tv) == -1) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("Select error");
        }
        checkCGITimeouts(fds);
        checkIdleTimeouts(fds);
        for (int fdCurrent = 0; fdCurrent < _fd_max + 1; fdCurrent++) {
            if (isServerFd(fdCurrent)) {
                if (FD_ISSET(fdCurrent, &readfds)) {
                    handleNewConnection(fdCurrent, fds, _fd_max);
                }
                continue;
            }
            if (cgi_out_to_client.count(fdCurrent)) {
                if (FD_ISSET(fdCurrent, &readfds))
                    handleCgiRead(cgi_out_to_client[fdCurrent], fds);
                continue;
            }
            if (cgi_in_to_client.count(fdCurrent)) {
                if (FD_ISSET(fdCurrent, &writefds))
                    handleCgiWrite(cgi_in_to_client[fdCurrent], fds);
                continue;
            }
            if (FD_ISSET(fdCurrent, &readfds)) {
                handleClientRead(fdCurrent, fds);
            }
            if (FD_ISSET(fdCurrent, &writefds) && _clients.count(fdCurrent)) {
                handleClientWrite(fdCurrent, fds);
            }
        }
    }

}