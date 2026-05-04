#include "HttpRequest.hpp"
#include "../utils/utils.hpp"

void HttpParser::parseRequestLine(const std::string &line, HttpRequest &request) {
    size_t first = line.find(' ');
    if (first == std::string::npos) { request.status = 400; return; }

    size_t second = line.find(' ', first + 1);
    if (second == std::string::npos) { request.status = 400; return; }

    request.method  = line.substr(0, first);
    request.path    = line.substr(first + 1, second - first - 1);
    request.version = trim(line.substr(second + 1));

    size_t q = request.path.find('?');
    if (q != std::string::npos) {
        request.query = request.path.substr(q + 1);
        request.path  = request.path.substr(0, q);
    }

    if (request.version != "HTTP/1.0" && request.version != "HTTP/1.1")
        request.status = 400;
}

void HttpParser::parseHeaders(const std::string &raw, size_t &position, HttpRequest &request) {
    std::string line;

    while (position < raw.size()) {
        size_t end = raw.find("\r\n", position);
        if (end == std::string::npos) end = raw.find('\n', position);
        if (end == std::string::npos) break;

        line     = raw.substr(position, end - position);
        position = end + (raw[end] == '\r' ? 2 : 1);

        if (line.empty()) break;

        size_t colon = line.find(':');
        if (colon == std::string::npos) { request.status = 400; return; }

        std::string key   = trim(line.substr(0, colon));
        std::string value = trim(line.substr(colon + 1));
        request.headers[key] = value;
    }
}

HttpRequest HttpParser::parse(const std::string &raw) {
    HttpRequest request;

    size_t firstLine = raw.find("\r\n");
    if (firstLine == std::string::npos) firstLine = raw.find('\n');
    if (firstLine == std::string::npos) { request.status = 400; return request; }

    parseRequestLine(raw.substr(0, firstLine), request);
    if (request.status != 200) return request;

    size_t position = firstLine + (raw[firstLine] == '\r' ? 2 : 1);
    parseHeaders(raw, position, request);
    if (request.status != 200) return request;

    request.body = raw.substr(position);
    return request;
}
