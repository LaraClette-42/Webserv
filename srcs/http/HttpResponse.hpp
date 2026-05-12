#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <netinet/in.h>
#include "HttpRequest.hpp"
#include "../config/config.hpp"

struct HttpResponse {
    int                                status;
    std::map<std::string, std::string> headers;
    std::string                        body;

    HttpResponse() : status(200) {}
    std::string serialize() const;
};

class HttpResponseBuilder {
public:
    static HttpResponse build(const sockaddr_in &clientAddr, const HttpRequest &request, const ConfigBlock &config);
    static std::string  statusMessage(int status);
};

#endif
