#include "CGI.hpp"
#include "../http/HttpRequest.hpp"
#include "../server/Server.hpp"
#include "../config/config.hpp"

CGI::CGI(const sockaddr_in &clientAddr, const HttpRequest &request, const ConfigBlock &config) {
    setupEnvironment(clientAddr, request, config);
    _scriptPath = config.root + request.path; // Full path to script
}

CGI::~CGI() {}

void CGI::setupEnvironment(const sockaddr_in &clientAddr, const HttpRequest &request, const ConfigBlock &config) {
    // CGI Environment Variables
    _env["REQUEST_METHOD"] = request.method;
    _env["SCRIPT_NAME"] = request.path;
    _env["QUERY_STRING"] = request.query;
    _env["CONTENT_LENGTH"] = std::to_string(request.body.length());
    _env["CONTENT_TYPE"] = request.headers.count("Content-Type") ? 
                         request.headers.at("Content-Type") : "";
    
    _env["SERVER_PROTOCOL"] = request.version;
    _env["SERVER_NAME"] = config.server_name;
    _env["SERVER_PORT"] = std::to_string(config.port);
    

     _env["REMOTE_ADDR"] = Server::formatIPv4(clientAddr);
    _env["REMOTE_HOST"] = "localhost";

    /// Add HTTP headers as environment variables with "HTTP_" prefix
}

std::string CGI::executeScript() {
    // Implementation for executing CGI script
    return "";
}

bool CGI::isCGI(const std::string& path) {
    return path.find("/cgi-bin/") == 0;
}