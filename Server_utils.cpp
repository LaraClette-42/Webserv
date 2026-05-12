#include "Server.hpp"



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
    return std::find(_listen_fds.begin(), _listen_fds.end(), fd) != _listen_fds.end();
}

void Server::removeClient(int clientFd, fd_set& fds) {
    close(clientFd);
    FD_CLR(clientFd, &fds);
    _clients.erase(clientFd);
}
