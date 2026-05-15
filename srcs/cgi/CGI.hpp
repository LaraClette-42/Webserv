#ifndef CGI_HPP
#define CGI_HPP
#include "../http/HttpRequest.hpp"
#include "../config/config.hpp"
#include "../server/Server.hpp"

#include <unistd.h>
#include <sys/wait.h>


struct CGIFd {
    pid_t pid;
    int in_fd;
    int out_fd;
};
class CGI {
public:
    CGI(const sockaddr_in &clientAddr, const HttpRequest &request, const ConfigBlock &config) ;
    ~CGI();
    // static bool isCGI(const std::string& path);
    std::string executeScript(const ConfigBlock &config);
    CGIFd startCGI(const ConfigBlock &config);

private:
    void setupEnvironment(const sockaddr_in &clientAddr, const HttpRequest &request, const ConfigBlock &config);
    char** getEnvStr() const;
    std::string _scriptPath;
    std::string _body;
    std::map<std::string, std::string> _env;
    
};  


#endif 