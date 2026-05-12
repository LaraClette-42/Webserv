#include "utils.hpp"
#include "../config/config.hpp"
#include <sstream>
#include <cctype>

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

