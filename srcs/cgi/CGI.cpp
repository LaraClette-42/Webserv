#include "CGI.hpp"
#include "../http/HttpRequest.hpp"
#include "../server/Server.hpp"
#include "../config/config.hpp"
#include "../utils/utils.hpp"

#include <cerrno>
#include <signal.h>
#include <sys/select.h>

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
    _env["CONTENT_LENGTH"] = intToString(request.body.length());
    _env["CONTENT_TYPE"] = request.headers.count("content-type") ? 
                         request.headers.at("content-type") : "";
    _env["SERVER_PROTOCOL"] = request.version;
    _env["SERVER_NAME"] = config.server_name;
    _env["SERVER_PORT"] = intToString(config.listens.empty() ? 80 : config.listens[0].port);
    _env["REMOTE_ADDR"] = formatIPv4(clientAddr);
    _env["REMOTE_HOST"] = "localhost";
    _env["PATH_TRANSLATED"] = config.root + request.path;
    _env["GATEWAY_INTERFACE"] = "CGI/1.1";
    if (!config.upload_store.empty()) {
        // Le CGI fait un chdir() vers son répertoire avant execve.
        // Un chemin relatif comme "www/uploads" serait alors résolu depuis
        // www/cgi-bin/ et pointerait au mauvais endroit. On le rend absolu ici,
        // avant le fork, pendant que le CWD est encore celui du serveur.
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)))
            _env["UPLOAD_PATH"] = std::string(cwd) + "/" + config.upload_store;
        else
            _env["UPLOAD_PATH"] = config.upload_store;
    }
    for (std::map<std::string, std::string>::const_iterator it = request.headers.begin();
         it != request.headers.end(); ++it) {
        if (it->first == "content-type" || it->first == "content-length")
            continue;
        std::string key = "HTTP_";
        for (std::size_t i = 0; i < it->first.size(); ++i)
            key += (it->first[i] == '-') ? '_' : (char)toupper((unsigned char)it->first[i]);
        _env[key] = it->second;
    }
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

CGIFd CGI::startCGI(const ConfigBlock &config) {
    std::string extension = _scriptPath.substr(_scriptPath.find_last_of('.'));
    std::map<std::string, std::string>::const_iterator it = config.cgi_pass.find(extension);
    if (it == config.cgi_pass.end())
        throw std::runtime_error("cgi_pass not configured for extension " + extension);
    
    const std::string interpreter = it->second;
    char **envp = getEnvStr();

    int FdIn[2], FdOut[2];
    if (pipe(FdIn) < 0 || pipe(FdOut) < 0) {
        for (int i = 0; envp[i]; ++i) delete[] envp[i];
        delete[] envp;
        throw std::runtime_error("pipe failed");
    }

    pid_t pid = fork();
    if (pid < 0) {
        for (int i = 0; envp[i]; ++i) delete[] envp[i];
        delete[] envp;
        throw std::runtime_error("fork failed");
    }

    if (pid == 0) {
        dup2(FdIn[0], STDIN_FILENO);
        dup2(FdOut[1], STDOUT_FILENO);
        close(FdIn[1]); close(FdOut[0]);
        std::size_t slash = _scriptPath.find_last_of('/');
        std::string scriptName = _scriptPath;
        if (slash != std::string::npos) {
            chdir(_scriptPath.substr(0, slash).c_str());
            scriptName = _scriptPath.substr(slash + 1);
        }
        char *newArgv[] = {
            const_cast<char*>(interpreter.c_str()),
            const_cast<char*>(scriptName.c_str()),
            NULL
        };
        execve(interpreter.c_str(), newArgv, envp);
        _exit(EXIT_FAILURE);
    }
    close(FdIn[0]);
    close(FdOut[1]);
    setNonBlockingFd(FdIn[1]);
    setNonBlockingFd(FdOut[0]);

    for (int i = 0; envp[i] != NULL; ++i)
        delete[] envp[i];
    delete[] envp;

    CGIFd cgi;
    cgi.pid    = pid;
    cgi.in_fd  = FdIn[1];
    cgi.out_fd = FdOut[0];
    return cgi;
}
