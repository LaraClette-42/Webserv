#include "Request.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

Request::Request(){}

Request::~Request() {}

int Request::parse(const std::string& Clientbuffer) {
    return 0;
}

int Request::isCgi() const {
    return 1;
}