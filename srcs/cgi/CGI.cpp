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
    if (!config.upload_store.empty())
        _env["UPLOAD_PATH"] = config.upload_store;
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

    setNonBlockingFd(FdIn[1]);
    setNonBlockingFd(FdOut[0]);

    const int in_write = FdIn[1];
    const int out_read = FdOut[0];

    const char *bodyPtr = _body.data();
    std::size_t bodyLeft = _body.size();
    bool stdin_open = true;
    bool stdout_eof = false;
    std::string output;
    char buffer[4096];

    if (bodyLeft == 0) {
        close(in_write);
        stdin_open = false;
    }

    while (1) {
        if (stdout_eof && !stdin_open)
            break;

        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        int fd_max = -1;

        if (!stdout_eof) {
            FD_SET(out_read, &readfds);
            fd_max = out_read;
        }
        if (stdin_open && bodyLeft > 0) {
            FD_SET(in_write, &writefds);
            if (in_write > fd_max)
                fd_max = in_write;
        }

        if (fd_max < 0)
            break;

        int rc = select(fd_max + 1, &readfds, &writefds, NULL, NULL);
        if (rc < 0) {
            if (errno == EINTR)
                continue;
            if (stdin_open)
                close(in_write);
            close(out_read);
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            throw std::runtime_error("CGI: select failed");
        }

        if (FD_ISSET(out_read, &readfds)) {
            for (;;) {
                ssize_t n = read(out_read, buffer, sizeof(buffer));
                if (n > 0)
                    output.append(buffer, static_cast<std::size_t>(n));
                else if (n == 0) {
                    stdout_eof = true;
                    break;
                } else if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                else {
                    if (stdin_open)
                        close(in_write);
                    close(out_read);
                    kill(pid, SIGKILL);
                    waitpid(pid, NULL, 0);
                    throw std::runtime_error("CGI: read from stdout failed");
                }
            }
        }

        if (stdin_open && bodyLeft > 0 && FD_ISSET(in_write, &writefds)) {
            for (;;) {
                ssize_t w = write(in_write, bodyPtr, bodyLeft);
                if (w > 0) {
                    bodyPtr += static_cast<std::size_t>(w);
                    bodyLeft -= static_cast<std::size_t>(w);
                } else if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                    break;
                else {
                    if (stdin_open)
                        close(in_write);
                    close(out_read);
                    kill(pid, SIGKILL);
                    waitpid(pid, NULL, 0);
                    throw std::runtime_error("CGI: write to stdin failed");
                }
                if (bodyLeft == 0)
                    break;
            }
        }

        if (stdin_open && bodyLeft == 0) {
            close(in_write);
            stdin_open = false;
        }

        if (stdout_eof && stdin_open) {
            close(in_write);
            stdin_open = false;
        }
    }

    close(out_read);

    int status = 0;
    waitpid(pid, &status, 0);
    for (int i = 0; envp[i] != NULL; ++i)
        delete[] envp[i];
    delete[] envp;
    return output;
}

// bool CGI::isCGI(const std::string& path) {
//     return path.find("/cgi-bin/") == 0;
// }