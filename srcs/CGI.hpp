#ifndef CGI_HPP
#define CGI_HPP
#include "http/HttpRequest.hpp"
#include "../config/config.hpp"

#include <unistd.h>
#include <sys/wait.h>

class CGI {
public:
    CGI(const sockaddr_in &clientAddr, const HttpRequest &request, const ConfigBlock &config) ;
    ~CGI();
    static bool isCGI(const std::string& path);
    std::string executeScript();
private:
    void setupEnvironment(const sockaddr_in &clientAddr, const HttpRequest &request, const ConfigBlock &config);
    std::string _scriptPath;
    std::string _body;
    std::map<std::string, std::string> _env;
};  


#endif 