#include "CGI.hpp"
#include "../http/HttpRequest.hpp"
#include "../server/Server.hpp"
#include "../config/config.hpp"

CGI::CGI(const sockaddr_in &clientAddr, const HttpRequest &request,
    const ConfigBlock &config) : _body(request.body)
{
    setupEnvironment(clientAddr, request, config);
    _scriptPath = config.root + request.path;
}

CGI::~CGI() {}

void CGI::setupEnvironment(const sockaddr_in &clientAddr, const HttpRequest &request, const ConfigBlock &config) {
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
    _env["PATH_TRANSLATED"] = config.root + request.path;
    _env["GATEWAY_INTERFACE"] = "CGI/1.1";  
}

char** CGI::getEnvStr() const {
    char **envStr = new char*[this->_env.size() + 1];
    int j = 0;
    for (std::map<std::string, std::string>::const_iterator i = this->_env.begin(); i != this->_env.end(); ++i) {
        std::string var = i->first + "=" + i->second;
        envStr[j] = new char[var.size() + 1];
        envStr[j] = strcpy(envStr[j], (const char*)var.c_str());
        j++;
    }
    envStr[j] = NULL;
    return envStr;
}

std::string CGI::executeScript(const ConfigBlock &config) {
    std::string extension = _scriptPath.substr(_scriptPath.find_last_of('.'));
    std::map<std::string, std::string>::const_iterator it = config.cgi_pass.find(extension);
    if (it == config.cgi_pass.end())
        throw std::runtime_error("cgi_pass not configured for extension " + extension);
    
    const std::string interpreter = it->second;
    char *argv[] = {
        const_cast<char*>(interpreter.c_str()),
        const_cast<char*>(_scriptPath.c_str()),
        NULL
    };
    char **envp = getEnvStr();

    int FdIn[2], FdOut[2];
    if (pipe(FdIn) < 0 || pipe(FdOut) < 0)
        throw std::runtime_error("pipe failed");

    pid_t pid = fork();
    if (pid < 0)
        throw std::runtime_error("fork failed");

    if (pid == 0) {
        dup2(FdIn[0], STDIN_FILENO);
        dup2(FdOut[1], STDOUT_FILENO);
        close(FdIn[1]); close(FdOut[0]);
        execve(interpreter.c_str(), argv, envp);
        _exit(EXIT_FAILURE);
    }

    close(FdIn[0]);
    close(FdOut[1]);

    write(FdIn[1], _body.data(), _body.size());
    close(FdIn[1]);

    std::string output;
    char buffer[4096];
    ssize_t n;
    while ((n = read(FdOut[0], buffer, sizeof(buffer))) > 0)
        output.append(buffer, n);
    close(FdOut[0]);

    int status;
    waitpid(pid, &status, 0);
    return output;

}

bool CGI::isCGI(const std::string& path) {
    return path.find("/cgi-bin/") == 0;
}