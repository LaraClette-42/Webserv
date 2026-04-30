#include "utils.hpp"
#include <sstream>

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
