#include "HttpRequest.hpp"
#include "HttpStatus.hpp"
#include "../utils/utils.hpp"
#include <cstdlib>

static std::string unchunkBody(const std::string &chunked) {
    std::string result;
    size_t pos = 0;

    while (pos < chunked.size()) {
        size_t lineEnd = chunked.find("\r\n", pos);
        if (lineEnd == std::string::npos)
            break;
        std::string sizeLine = chunked.substr(pos, lineEnd - pos);
        size_t semi = sizeLine.find(';');
        if (semi != std::string::npos)
            sizeLine = sizeLine.substr(0, semi);
        long chunkSize = std::strtol(sizeLine.c_str(), NULL, 16);
        pos = lineEnd + 2;

        if (chunkSize <= 0)
            break;
        if (pos + static_cast<size_t>(chunkSize) > chunked.size())
            break;
        result.append(chunked, pos, static_cast<size_t>(chunkSize));
        pos += static_cast<size_t>(chunkSize) + 2;
    }
    return result;
}

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

        std::string key = toLower(trim(line.substr(0, dbpoint)));
        std::string value = trim(line.substr(dbpoint + 1));
        request.headers[key] = value;
    }
}

HttpRequest HttpParser::parse(const std::string &raw) {
    HttpRequest request;

    if (raw.find("\r\n\r\n") == std::string::npos && raw.find("\n\n") == std::string::npos) {
        request.status = HTTP_INCOMPLETE_REQUEST;
        return request;
    }

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

    std::map<std::string, std::string>::const_iterator te = request.headers.find("transfer-encoding");
    if (te != request.headers.end() && te->second.find("chunked") != std::string::npos) {
        request.body = unchunkBody(raw.substr(position));
        return request;
    }

    std::map<std::string, std::string>::const_iterator cl = request.headers.find("content-length");
    if (cl != request.headers.end()) {
        char *end;
        long contentLength = std::strtol(cl->second.c_str(), &end, 10);
        if (*end != '\0' || contentLength < 0) {
            request.status = HTTP_BAD_REQUEST;
            return request;
        }
        size_t len = static_cast<size_t>(contentLength);
        if (position + len > raw.size()) {
            request.status = HTTP_INCOMPLETE_REQUEST;
            return request;
        }
        request.body = raw.substr(position, len);
    }

    return request;
}
