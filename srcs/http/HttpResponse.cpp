#include "HttpResponse.hpp"
#include "HttpStatus.hpp"
#include "../server/Server.hpp"
#include "../CGI.hpp"
#include "../utils/utils.hpp"
#include <sstream>
#include <fstream>
#include <sys/stat.h>

static std::string getContentType(const std::string &path) {
    static std::map<std::string, std::string> types;
    if (types.empty()) {
        types[".html"] = "text/html";
        types[".css"]  = "text/css";
        types[".js"]   = "application/javascript";
        types[".jpg"]  = "image/jpeg";
        types[".jpeg"] = "image/jpeg";
        types[".png"]  = "image/png";
        types[".gif"]  = "image/gif";
        types[".ico"]  = "image/x-icon";
        types[".txt"]  = "text/plain";
        types[".pdf"]  = "application/pdf";
    }
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos)
        return "application/octet-stream";
    std::map<std::string, std::string>::iterator it = types.find(path.substr(dot));
    if (it == types.end())
        return "application/octet-stream";
    return it->second;
}

static std::string readFile(const std::string &path, int &status) {
    std::ifstream file(path.c_str());
    if (!file.is_open()) {
        struct stat info;
        if (stat(path.c_str(), &info) == 0)
            status = HTTP_FORBIDDEN;
        else
            status = HTTP_NOT_FOUND;
        return "";
    }
    std::ostringstream oss;
    oss << file.rdbuf();
    status = HTTP_OK;
    return oss.str();
}

static HttpResponse makeError(int status) {
    HttpResponse response;
    response.status = status;
    response.body   = "<!DOCTYPE html>\n<html><body><h1>"
                    + intToString(status) + " "
                    + HttpResponseBuilder::statusMessage(status)
                    + "</h1></body></html>\n";
    response.headers["Content-Type"]   = "text/html";
    response.headers["Content-Length"] = intToString(response.body.size());
    return response;
}

static HttpResponse handleGet(const HttpRequest &request, const ConfigBlock &config) {
    HttpResponse response;
    std::string  filePath = config.root + request.path;

    struct stat info;
    if (stat(filePath.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
        if (filePath[filePath.size() - 1] != '/')
            filePath += '/';
        for (size_t i = 0; i < config.index.size(); ++i) {
            struct stat candidate;
            if (stat((filePath + config.index[i]).c_str(), &candidate) == 0) {
                filePath += config.index[i];
                break;
            }
        }
    }

    int status = HTTP_OK;
    response.body   = readFile(filePath, status);
    response.status = status;

    if (status == HTTP_OK) {
        response.headers["Content-Type"]   = getContentType(filePath);
        response.headers["Content-Length"] = intToString(response.body.size());
        return response;
    }
    return makeError(status);
}

typedef HttpResponse (*MethodHandler)(const HttpRequest &, const ConfigBlock &);

static std::map<std::string, MethodHandler> makeMethodMap() {
    std::map<std::string, MethodHandler> map;
    map["GET"] = handleGet;
    return map;
}

std::string HttpResponse::serialize() const {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status << " " << HttpResponseBuilder::statusMessage(status) << "\r\n";
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        oss << it->first << ": " << it->second << "\r\n";
    oss << "\r\n" << body;
    return oss.str();
}

std::string HttpResponseBuilder::statusMessage(int status) {
    if (status == HTTP_OK)                    return "OK";
    if (status == HTTP_CREATED)               return "Created";
    if (status == HTTP_NO_CONTENT)            return "No Content";
    if (status == HTTP_MOVED_PERMANENTLY)     return "Moved Permanently";
    if (status == HTTP_FOUND)                 return "Found";
    if (status == HTTP_BAD_REQUEST)           return "Bad Request";
    if (status == HTTP_FORBIDDEN)             return "Forbidden";
    if (status == HTTP_NOT_FOUND)             return "Not Found";
    if (status == HTTP_METHOD_NOT_ALLOWED)    return "Method Not Allowed";
    if (status == HTTP_CONTENT_TOO_LARGE)     return "Content Too Large";
    if (status == HTTP_INTERNAL_SERVER_ERROR) return "Internal Server Error";
    return "Unknown";
}

HttpResponse HttpResponseBuilder::build(const sockaddr_in &clientAddr, const HttpRequest &request, const ConfigBlock &config) {
    static std::map<std::string, MethodHandler> methods = makeMethodMap();
    if (CGI::isCGI(request.path)) {
        HttpResponse response;
        CGI cgi(clientAddr, request, config);
        response.body = cgi.executeScript(config);  // Run CGI process
        response.status = HTTP_OK;
        return response;
    }
    if (request.status != HTTP_OK)
        return makeError(request.status);

    if (!config.allow_methods.empty() && config.allow_methods.find(request.method) == config.allow_methods.end())
        return makeError(HTTP_METHOD_NOT_ALLOWED);

    std::map<std::string, MethodHandler>::iterator it = methods.find(request.method);
    if (it == methods.end())
        return makeError(HTTP_METHOD_NOT_ALLOWED);

    return it->second(request, config);
}
