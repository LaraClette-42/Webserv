#ifndef UTILS_HPP
#define UTILS_HPP

#include <netinet/in.h>
#include <string>

void setNonBlockingFd(int fd);
std::string formatIPv4(const sockaddr_in& addr);

std::string trim(const std::string &s);
std::string lineErr(const std::string &msg, int line);
std::string intToString(int n);
std::string toLower(const std::string &s);

#endif
