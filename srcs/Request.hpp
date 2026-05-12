#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>
#include <vector>

class Request {
public:

    Request();
    ~Request();

    // Parsing
    int parse(const std::string& Clientbuffer);
    int isCgi() const;
};

#endif
