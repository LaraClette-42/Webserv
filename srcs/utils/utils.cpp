#include "utils.hpp"
#include "../config/config.hpp"
#include <sstream>
#include <cctype>
#include <fcntl.h>
#include <stdexcept>

void setNonBlockingFd(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl(F_GETFL) failed");
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl(F_SETFL) failed");
}

std::string formatIPv4(const sockaddr_in& addr) {
    const unsigned int ip = ntohl(addr.sin_addr.s_addr);
    const unsigned int a = (ip >> 24) & 0xFF;
    const unsigned int b = (ip >> 16) & 0xFF;
    const unsigned int c = (ip >> 8) & 0xFF;
    const unsigned int d = ip & 0xFF;

    std::stringstream ss;
    ss << a << "." << b << "." << c << "." << d << ":" << ntohs(addr.sin_port);
    return ss.str();
}

std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string lineErr(const std::string &msg, int line) {
    std::ostringstream oss;
    oss << "line " << line << ": " << msg;
    return oss.str();
}


std::string intToString(int n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

std::string toLower(const std::string &s) {
    std::string result = s;
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
    return result;
}

