#include "config.hpp"
#include "../utils/utils.hpp"
#include <sstream>
#include <cstdlib>

typedef void (*Handler)(ConfigBlock &, const std::string &);

static void handleListen(ConfigBlock &block, const std::string &value) {
    size_t colon = value.rfind(':');
    if (colon == std::string::npos) {
        block.host = "0.0.0.0";
        block.port = std::atoi(value.c_str());
    } else {
        block.host = value.substr(0, colon);
        block.port = std::atoi(value.substr(colon + 1).c_str());
    }
    if (block.port <= 0 || block.port > 65535)
        throw std::invalid_argument("listen: invalid port");
}

static void handleServerName(ConfigBlock &block, const std::string &value) {
    block.server_name = value;
}

static void handleRoot(ConfigBlock &block, const std::string &value) {
    block.root = value;
}

static void handleIndex(ConfigBlock &block, const std::string &value) {
    std::istringstream iss(value);
    std::string token;
    while (iss >> token)
        block.index.push_back(token);
}

static void handleClientMaxBodySize(ConfigBlock &block, const std::string &value) {
    char *end;
    long size = std::strtol(value.c_str(), &end, 10);
    if (*end != '\0' || size < 0)
        throw std::invalid_argument("client_max_body_size: expected a non-negative integer");
    block.client_max_body_size = size;
}

static void handleErrorPage(ConfigBlock &block, const std::string &value) {
    std::istringstream iss(value);
    int code;
    std::string path;
    if (!(iss >> code >> path))
        throw std::invalid_argument("error_page: expected <code> <path>");
    block.error_pages[code] = path;
}

static void handleAllowMethods(ConfigBlock &block, const std::string &value) {
    std::istringstream iss(value);
    std::string method;
    while (iss >> method)
        block.allow_methods.insert(method);
}

static void handleAutoIndex(ConfigBlock &block, const std::string &value) {
    if (value == "on")        block.autoindex = true;
    else if (value == "off")  block.autoindex = false;
    else throw std::invalid_argument("autoindex: expected 'on' or 'off'");
}

static void handleUploadStore(ConfigBlock &block, const std::string &value) {
    block.upload_store = value;
}

static void handleReturn(ConfigBlock &block, const std::string &value) {
    block.redirect = value;
}

static void handleCgiPass(ConfigBlock &block, const std::string &value) {
    std::istringstream iss(value);
    std::string ext, interpreter;
    if (!(iss >> ext >> interpreter))
        throw std::invalid_argument("cgi_pass: expected <extension> <interpreter>");
    block.cgi_pass[ext] = interpreter;
}

static std::map<std::string, Handler> makeDispatchMap() {
    std::map<std::string, Handler> m;
    m["listen"]               = handleListen;
    m["server_name"]          = handleServerName;
    m["root"]                 = handleRoot;
    m["index"]                = handleIndex;
    m["client_max_body_size"] = handleClientMaxBodySize;
    m["error_page"]           = handleErrorPage;
    m["allow_methods"]        = handleAllowMethods;
    m["autoindex"]            = handleAutoIndex;
    m["upload_store"]         = handleUploadStore;
    m["return"]               = handleReturn;
    m["cgi_pass"]             = handleCgiPass;
    return m;
}

ConfigBlock::ConfigBlock()
    : port(80), client_max_body_size(-1), autoindex(false) {}

const std::vector<ConfigBlock>& ConfigFile::getServers() const {
    return _servers;
}

void ConfigFile::checkPath(const std::string &path) {
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || path.substr(dot) != ".conf")
        throw std::invalid_argument("not a '.conf' file");
}

void ConfigFile::applyKeyword(ConfigBlock &block, const std::string &key,
                               const std::string &value, int lineNum) {
    static std::map<std::string, Handler> dispatch = makeDispatchMap();

    std::map<std::string, Handler>::iterator it = dispatch.find(key);
    if (it == dispatch.end())
        throw std::invalid_argument(lineErr("unknown keyword '" + key + "'", lineNum));
    it->second(block, value);
}

ConfigBlock ConfigFile::parseBlock(std::ifstream &file, int &lineNum) {
    ConfigBlock block;
    std::string line;

    while (std::getline(file, line)) {
        ++lineNum;
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        if (t == "}") return block;

        if (t.substr(0, 8) == "location") {
            std::string rest = trim(t.substr(8));
            if (rest.empty() || rest[rest.size() - 1] != '{')
                throw std::invalid_argument(lineErr("expected 'location <path> {'", lineNum));
            std::string locPath = trim(rest.substr(0, rest.size() - 1));
            block.locations[locPath] = parseBlock(file, lineNum);
            continue;
        }

        size_t sep = t.find(' ');
        if (sep == std::string::npos)
            throw std::invalid_argument(lineErr("expected 'key value', got '" + t + "'", lineNum));
        applyKeyword(block, t.substr(0, sep), trim(t.substr(sep + 1)), lineNum);
    }
    throw std::invalid_argument("unexpected end of file: missing '}'");
}

void ConfigFile::parse(const std::string &path) {
    checkPath(path);

    std::ifstream file(path.c_str());
    if (!file.is_open())
        throw std::runtime_error("cannot open: " + path);

    std::string line;
    int lineNum = 0;

    while (std::getline(file, line)) {
        ++lineNum;
        std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;

        if (t.size() < 7 || t.substr(0, 6) != "server" || trim(t.substr(6)) != "{")
            throw std::invalid_argument(lineErr("expected 'server {'", lineNum));
        _servers.push_back(parseBlock(file, lineNum));
    }
    if (_servers.empty())
        throw std::runtime_error("no server block found in " + path);
}
