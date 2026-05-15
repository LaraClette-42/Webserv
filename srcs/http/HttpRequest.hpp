#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>
#include "HttpStatus.hpp"

struct HttpRequest {
    std::string                        method;
    std::string                        path;
    std::string                        url_path;
    std::string                        query;
    std::string                        version;
    std::map<std::string, std::string> headers;
    std::string                        body;
    int                                status;

    HttpRequest() : status(HTTP_OK) {}
};

class HttpParser {
public:
    static HttpRequest parse(const std::string &raw);

private:
    static void parseRequestLine(const std::string &line, HttpRequest &request);
    static void parseHeaders(const std::string &raw, size_t &position, HttpRequest &request);
};

#endif
