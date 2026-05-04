#include "HttpRequest.hpp"
#include "HttpStatus.hpp"
#include "../utils/utils.hpp"

void HttpParser::parseRequestLine(const std::string &line, HttpRequest &request) {
    size_t first = line.find(' ');
    if (first == std::string::npos) {
        request.status = HTTP_BAD_REQUEST;
        return;
    }

    size_t second = line.find(' ', first + 1);
    if (second == std::string::npos) {
        request.status = HTTP_BAD_REQUEST;
        return;
    }

    request.method  = line.substr(0, first);
    request.path    = line.substr(first + 1, second - first - 1);
    request.version = trim(line.substr(second + 1));

    size_t q = request.path.find('?');
    if (q != std::string::npos) {
        request.query = request.path.substr(q + 1);
        request.path  = request.path.substr(0, q);
    }

    if (request.version != "HTTP/1.0" && request.version != "HTTP/1.1")
        request.status = HTTP_BAD_REQUEST;
}

void HttpParser::parseHeaders(const std::string &raw, size_t &position, HttpRequest &request) {
    std::string line;

    while (position < raw.size()) {
        size_t end = raw.find("\r\n", position);
        if (end == std::string::npos) end = raw.find('\n', position);
        if (end == std::string::npos)
            break;

        line = raw.substr(position, end - position);
        if (raw[end] == '\r')
            position = end + 2;
        else
            position = end + 1;

        if (line.empty())
            break;

        size_t dbpoint = line.find(':');
        if (dbpoint == std::string::npos) {
            request.status = HTTP_BAD_REQUEST;
            return;
        }

        std::string key = trim(line.substr(0, dbpoint));
        std::string value = trim(line.substr(dbpoint + 1));
        request.headers[key] = value;
    }
}

HttpRequest HttpParser::parse(const std::string &raw) {
    HttpRequest request;

    size_t firstLine = raw.find("\r\n");
    if (firstLine == std::string::npos) 
        firstLine = raw.find('\n');
    if (firstLine == std::string::npos) { 
        request.status = HTTP_BAD_REQUEST;
        return request;
    }

    parseRequestLine(raw.substr(0, firstLine), request);
    if (request.status != HTTP_OK)
        return request;

    size_t position;
    if (raw[firstLine] == '\r')
        position = firstLine + 2;
    else
        position = firstLine + 1;
    parseHeaders(raw, position, request);
    if (request.status != HTTP_OK)
        return request;

    request.body = raw.substr(position);
    return request;
}
