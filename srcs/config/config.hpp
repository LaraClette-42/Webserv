#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <sstream>
#include <cstdlib>


struct ConfigBlock {
    std::string                        host;
    int                                port;
    std::string                        server_name;
    std::string                        root;
    std::vector<std::string>           index;
    long                               client_max_body_size;
    std::map<int, std::string>         error_pages;
    std::set<std::string>              allow_methods;
    bool                               autoindex;
    std::string                        upload_store;
    std::string                        redirect;
    std::map<std::string, std::string> cgi_pass;
    std::map<std::string, ConfigBlock> locations;

    ConfigBlock();
};

class ConfigFile {
public:
    void                            parse(const std::string &path);
    const std::vector<ConfigBlock>& getServers() const;

private:
    std::vector<ConfigBlock> _servers;

    void        checkPath(const std::string &path);
    ConfigBlock parseBlock(std::ifstream &file, int &lineNum);
    void        applyKeyword(ConfigBlock &block, const std::string &key,
                             const std::string &value, int lineNum);
};

#endif
