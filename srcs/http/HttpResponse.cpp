#include "HttpResponse.hpp"
#include "HttpStatus.hpp"
#include "../utils/utils.hpp"
#include "../cgi/CGI.hpp"
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>

static std::string getContentType(const std::string &path) {
    static std::map<std::string, std::string> types;
    if (types.empty()) {
        types[".html"] = "text/html";
        types[".css"] = "text/css";
        types[".js"] = "application/javascript";
        types[".jpg"] = "image/jpeg";
        types[".jpeg"] = "image/jpeg";
        types[".png"] = "image/png";
        types[".gif"] = "image/gif";
        types[".ico"] = "image/x-icon";
        types[".txt"] = "text/plain";
        types[".pdf"] = "application/pdf";
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

static HttpResponse makeError(int status, const ConfigBlock &config) {
    std::map<int, std::string>::const_iterator it = config.error_pages.find(status);
    if (it != config.error_pages.end()) {
        int fileStatus = HTTP_OK;
        std::string body = readFile(config.root + it->second, fileStatus);
        if (fileStatus == HTTP_OK) {
            HttpResponse response;
            response.status = status;
            response.body = body;
            response.headers["Content-Type"] = "text/html";
            response.headers["Content-Length"] = intToString(body.size());
            return response;
        }
    }
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

static HttpResponse makeRedirect(const std::string &url) {
    HttpResponse response;
    response.status = HTTP_MOVED_PERMANENTLY;
    response.headers["Location"] = url;
    response.headers["Content-Length"] = "0";
    return response;
}

static std::string buildAutoindex(const std::string &urlPath, const std::string &dirPath) {
    DIR *dir = opendir(dirPath.c_str());
    if (!dir)
        return "";

    std::ostringstream html;
    html << "<!DOCTYPE html>\n<html>\n<head><title>Index of " << urlPath << "</title></head>\n"
         << "<body>\n<h1>Index of " << urlPath << "</h1>\n<hr>\n<pre>\n";

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".")
            continue;
        struct stat info;
        std::string fullPath = dirPath + name;
        bool isDir = (stat(fullPath.c_str(), &info) == 0 && S_ISDIR(info.st_mode));
        std::string slash = "";
        if (isDir)
            slash = "/";
        html << "<a href=\"" << name << slash << "\">" << name << slash << "</a>\n";
    }
    closedir(dir);

    html << "</pre>\n<hr>\n</body>\n</html>\n";
    return html.str();
}

static bool prefixMatches(const std::string &prefix, const std::string &path) {
    if (path.size() < prefix.size())
        return false;
    if (path.compare(0, prefix.size(), prefix) != 0)
        return false;
    return path.size() == prefix.size() || path[prefix.size()] == '/' || prefix[prefix.size() - 1] == '/';
}

static ConfigBlock resolveConfig(const std::string &path, const ConfigBlock &server,
                                 std::string &strippedPath) {
    const ConfigBlock *best = NULL;
    size_t             bestLen = 0;
    bool               hasOwnRoot = false;

    for (std::map<std::string, ConfigBlock>::const_iterator it = server.locations.begin();
         it != server.locations.end(); ++it) {
        const std::string &prefix = it->first;
        if (prefixMatches(prefix, path) && prefix.size() > bestLen) {
            best = &it->second;
            bestLen = prefix.size();
            hasOwnRoot = !it->second.root.empty();
        }
    }

    strippedPath = path;

    if (!best)
        return server;

    if (hasOwnRoot) {
        strippedPath = path.substr(bestLen);
        if (strippedPath.empty() || strippedPath[0] != '/')
            strippedPath = "/" + strippedPath;
    }

    ConfigBlock merged = server;
    if (!best->root.empty())
        merged.root = best->root;
    if (!best->index.empty())
        merged.index = best->index;
    if (!best->allow_methods.empty())
        merged.allow_methods = best->allow_methods;
    if (!best->redirect.empty())
        merged.redirect = best->redirect;
    if (best->autoindex)
        merged.autoindex = best->autoindex;
    if (!best->upload_store.empty())
        merged.upload_store = best->upload_store;
    if (!best->cgi_pass.empty())
        merged.cgi_pass = best->cgi_pass;
    if (!best->error_pages.empty())
        merged.error_pages = best->error_pages;
    if (best->client_max_body_size != 1048576)
        merged.client_max_body_size = best->client_max_body_size;
    return merged;
}

static HttpResponse handleGet(const HttpRequest &request, const ConfigBlock &config) {
    HttpResponse response;
    std::string  filePath = config.root + request.path;

    struct stat info;
    if (stat(filePath.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) {
        if (filePath[filePath.size() - 1] != '/')
            filePath += '/';
        bool indexFound = false;
        for (size_t i = 0; i < config.index.size(); ++i) {
            struct stat candidate;
            if (stat((filePath + config.index[i]).c_str(), &candidate) == 0) {
                filePath += config.index[i];
                indexFound = true;
                break;
            }
        }
        if (!indexFound) {
            if (config.autoindex) {
                std::string body = buildAutoindex(request.path, filePath);
                if (body.empty())
                    return makeError(HTTP_FORBIDDEN, config);
                response.status = HTTP_OK;
                response.body = body;
                response.headers["Content-Type"] = "text/html";
                response.headers["Content-Length"] = intToString(body.size());
                return response;
            }
            return makeError(HTTP_NOT_FOUND, config);
        }
    }

    int status   = HTTP_OK;
    response.body   = readFile(filePath, status);
    response.status = status;

    if (status == HTTP_OK) {
        response.headers["Content-Type"] = getContentType(filePath);
        response.headers["Content-Length"] = intToString(response.body.size());
        return response;
    }
    return makeError(status, config);
}

static HttpResponse handlePost(const HttpRequest &request, const ConfigBlock &config) {
    if (config.upload_store.empty())
        return makeError(HTTP_FORBIDDEN, config);

    std::string filename = request.path;
    size_t slash = filename.rfind('/');
    if (slash != std::string::npos)
        filename = filename.substr(slash + 1);
    if (filename.empty())
        return makeError(HTTP_BAD_REQUEST, config);

    std::string store = config.upload_store;
    if (store[store.size() - 1] != '/')
        store += '/';

    std::ofstream file((store + filename).c_str(), std::ios::binary);
    if (!file.is_open())
        return makeError(HTTP_FORBIDDEN, config);

    file.write(request.body.c_str(), static_cast<std::streamsize>(request.body.size()));
    if (!file)
        return makeError(HTTP_INTERNAL_SERVER_ERROR, config);

    HttpResponse response;
    response.status = HTTP_CREATED;
    response.headers["Content-Length"] = "0";
    return response;
}

static HttpResponse handleDelete(const HttpRequest &request, const ConfigBlock &config) {
    std::string filePath = config.root + request.path;

    struct stat info;
    if (stat(filePath.c_str(), &info) != 0)
        return makeError(HTTP_NOT_FOUND, config);
    if (S_ISDIR(info.st_mode))
        return makeError(HTTP_FORBIDDEN, config);
    if (remove(filePath.c_str()) != 0)
        return makeError(HTTP_FORBIDDEN, config);

    HttpResponse response;
    response.status = HTTP_NO_CONTENT;
    response.headers["Content-Length"] = "0";
    return response;
}

typedef HttpResponse (*MethodHandler)(const HttpRequest &, const ConfigBlock &);

static std::map<std::string, MethodHandler> makeMethodMap() {
    std::map<std::string, MethodHandler> map;
    map["GET"] = handleGet;
    map["POST"] = handlePost;
    map["DELETE"] = handleDelete;
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
    if (status == HTTP_OK)
        return "OK";
    if (status == HTTP_CREATED)
        return "Created";
    if (status == HTTP_NO_CONTENT)
        return "No Content";
    if (status == HTTP_MOVED_PERMANENTLY)
        return "Moved Permanently";
    if (status == HTTP_FOUND)
        return "Found";
    if (status == HTTP_BAD_REQUEST)
        return "Bad Request";
    if (status == HTTP_FORBIDDEN)
        return "Forbidden";
    if (status == HTTP_NOT_FOUND)
        return "Not Found";
    if (status == HTTP_METHOD_NOT_ALLOWED)
        return "Method Not Allowed";
    if (status == HTTP_CONTENT_TOO_LARGE)
        return "Content Too Large";
    if (status == HTTP_INTERNAL_SERVER_ERROR)
        return "Internal Server Error";
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
        return makeError(request.status, config);

    std::string strippedPath;
    ConfigBlock config = resolveConfig(request.path, config, strippedPath);

    HttpRequest adjusted  = request;
    adjusted.path         = strippedPath;

    if (!config.redirect.empty())
        return makeRedirect(config.redirect);

    if (config.client_max_body_size > 0 &&
        static_cast<long>(request.body.size()) > config.client_max_body_size)
        return makeError(HTTP_CONTENT_TOO_LARGE, config);

    if (!config.allow_methods.empty() &&
        config.allow_methods.find(adjusted.method) == config.allow_methods.end())
        return makeError(HTTP_METHOD_NOT_ALLOWED, config);

    std::map<std::string, MethodHandler>::iterator it = methods.find(adjusted.method);
    if (it == methods.end())
        return makeError(HTTP_METHOD_NOT_ALLOWED, config);

    return it->second(adjusted, config);
}

