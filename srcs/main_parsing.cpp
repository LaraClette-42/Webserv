#include "config/config.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <vector>

static void check(const std::string &name, bool ok) {
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
}

static bool fileExists(const std::string &path) {
    std::ifstream f(path.c_str());
    return f.is_open();
}

static void printBlock(const ConfigBlock &block, int indent) {
    std::string tab(indent * 2, ' ');
    if (!block.host.empty())
        std::cout << tab << "listen:               " << block.host << ":" << block.port << "\n";
    if (!block.server_name.empty())
        std::cout << tab << "server_name:          " << block.server_name << "\n";
    if (!block.root.empty())
        std::cout << tab << "root:                 " << block.root << "\n";
    if (!block.index.empty()) {
        std::cout << tab << "index:                ";
        for (size_t i = 0; i < block.index.size(); ++i)
            std::cout << block.index[i] << " ";
        std::cout << "\n";
    }
    if (block.client_max_body_size != 1048576)
        std::cout << tab << "client_max_body_size: " << block.client_max_body_size << "\n";
    if (!block.error_pages.empty()) {
        for (std::map<int, std::string>::const_iterator it = block.error_pages.begin(); it != block.error_pages.end(); ++it)
            std::cout << tab << "error_page:           " << it->first << " " << it->second << "\n";
    }
    if (!block.allow_methods.empty()) {
        std::cout << tab << "allow_methods:        ";
        for (std::set<std::string>::const_iterator it = block.allow_methods.begin(); it != block.allow_methods.end(); ++it)
            std::cout << *it << " ";
        std::cout << "\n";
    }
    if (block.autoindex)
        std::cout << tab << "autoindex:            on\n";
    if (!block.upload_store.empty())
        std::cout << tab << "upload_store:         " << block.upload_store << "\n";
    if (!block.redirect.empty())
        std::cout << tab << "return:               " << block.redirect << "\n";
    if (!block.cgi_pass.empty()) {
        for (std::map<std::string, std::string>::const_iterator it = block.cgi_pass.begin(); it != block.cgi_pass.end(); ++it)
            std::cout << tab << "cgi_pass:             " << it->first << " " << it->second << "\n";
    }
    for (std::map<std::string, ConfigBlock>::const_iterator it = block.locations.begin(); it != block.locations.end(); ++it) {
        std::cout << tab << "location " << it->first << " {\n";
        printBlock(it->second, indent + 1);
        std::cout << tab << "}\n";
    }
}

static ConfigBlock makeServerConfig() {
    ConfigBlock server;
    server.root = "./www";
    server.index.push_back("index.html");
    server.error_pages[404] = "/errors/404.html";

    ConfigBlock locUploads;
    locUploads.upload_store = "./www/uploads";
    locUploads.allow_methods.insert("POST");
    locUploads.allow_methods.insert("DELETE");
    server.locations["/uploads"] = locUploads;

    ConfigBlock locListing;
    locListing.root      = "./www/listing";
    locListing.autoindex = true;
    server.locations["/listing"] = locListing;

    ConfigBlock locOld;
    locOld.redirect = "http://localhost:8080/";
    server.locations["/old"] = locOld;

    return server;
}

static HttpRequest makeRequest(const std::string &method,
                               const std::string &path,
                               const std::string &body = "") {
    HttpRequest req;
    req.method  = method;
    req.path    = path;
    req.version = "HTTP/1.1";
    req.body    = body;
    return req;
}

static void testRequestParsing() {
    std::cout << "\n── REQUEST PARSING ──────────────────────────────────────\n";

    std::string raw =
        "POST /upload?user=42 HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Hello, world!";
    HttpRequest req = HttpParser::parse(raw);
    check("basic: status 200",        req.status  == 200);
    check("basic: method POST",        req.method  == "POST");
    check("basic: path /upload",       req.path    == "/upload");
    check("basic: query user=42",      req.query   == "user=42");
    check("basic: version HTTP/1.1",   req.version == "HTTP/1.1");
    check("basic: Host header parsed", req.headers.count("Host") == 1);
    check("basic: body",               req.body    == "Hello, world!");

    HttpRequest bad = HttpParser::parse("GET /\r\n\r\n");
    check("bad: missing version → 400", bad.status == 400);

    std::string chunked =
        "POST /data HTTP/1.1\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nHello\r\n"
        "6\r\n World\r\n"
        "0\r\n\r\n";
    HttpRequest chunkReq = HttpParser::parse(chunked);
    check("chunked: body decoded",  chunkReq.body == "Hello World");
    check("chunked: status 200",    chunkReq.status == 200);
}

static void testGet() {
    std::cout << "\n── GET ──────────────────────────────────────────────────\n";
    ConfigBlock server = makeServerConfig();

    HttpResponse r = HttpResponseBuilder::build(makeRequest("GET", "/index.html"), server);
    check("GET /index.html → 200",               r.status == 200);
    check("GET /index.html → Content-Type html", r.headers["Content-Type"] == "text/html");

    HttpResponse dir = HttpResponseBuilder::build(makeRequest("GET", "/"), server);
    check("GET / → 200 (index.html found)",      dir.status == 200);

    HttpResponse nf = HttpResponseBuilder::build(makeRequest("GET", "/ghost.html"), server);
    check("GET /ghost.html → 404",               nf.status == 404);
    check("GET /ghost.html → custom error page", nf.body.find("Custom 404") != std::string::npos);
}

static void testAutoindex() {
    std::cout << "\n── AUTOINDEX ────────────────────────────────────────────\n";
    ConfigBlock server = makeServerConfig();

    HttpResponse r = HttpResponseBuilder::build(makeRequest("GET", "/listing"), server);
    check("autoindex → 200",            r.status == 200);
    check("autoindex → lists un.txt",   r.body.find("un.txt")   != std::string::npos);
    check("autoindex → lists deux.txt", r.body.find("deux.txt") != std::string::npos);
}

static void testRedirect() {
    std::cout << "\n── REDIRECT ─────────────────────────────────────────────\n";
    ConfigBlock server = makeServerConfig();

    HttpResponse r = HttpResponseBuilder::build(makeRequest("GET", "/old"), server);
    check("redirect → 301",                r.status == 301);
    check("redirect → Location header set", r.headers["Location"] == "http://localhost:8080/");
}

static void testMethodControl() {
    std::cout << "\n── METHOD CONTROL ───────────────────────────────────────\n";
    ConfigBlock server = makeServerConfig();

    HttpResponse r = HttpResponseBuilder::build(makeRequest("GET", "/uploads/anything"), server);
    check("GET on uploads-only route → 405", r.status == 405);
}

static void testBodySizeLimit() {
    std::cout << "\n── BODY SIZE LIMIT ──────────────────────────────────────\n";
    ConfigBlock server = makeServerConfig();
    server.client_max_body_size = 10;

    HttpRequest req = makeRequest("POST", "/uploads/test.txt", "this body is way too long");
    HttpResponse r  = HttpResponseBuilder::build(req, server);
    check("body > limit → 413", r.status == 413);

    HttpRequest small = makeRequest("POST", "/uploads/small.txt", "hello");
    HttpResponse ok   = HttpResponseBuilder::build(small, server);
    check("body <= limit → 201", ok.status == 201);
    std::remove("./www/uploads/small.txt");
}

static void testPost() {
    std::cout << "\n── POST ─────────────────────────────────────────────────\n";
    ConfigBlock server = makeServerConfig();

    HttpRequest req = makeRequest("POST", "/uploads/hello.txt", "bonjour monde");
    HttpResponse r  = HttpResponseBuilder::build(req, server);
    check("POST → 201",                  r.status == 201);
    check("POST → file created on disk", fileExists("./www/uploads/hello.txt"));
    std::remove("./www/uploads/hello.txt");

    HttpRequest noStore = makeRequest("POST", "/nostore/file.txt", "data");
    HttpResponse forbidden = HttpResponseBuilder::build(noStore, server);
    check("POST without upload_store → 403", forbidden.status == 403);
}

static void testDelete() {
    std::cout << "\n── DELETE ───────────────────────────────────────────────\n";
    ConfigBlock server = makeServerConfig();

    { std::ofstream f("./www/to_delete.txt"); f << "temp"; }
    HttpRequest req = makeRequest("DELETE", "/to_delete.txt");
    HttpResponse r  = HttpResponseBuilder::build(req, server);
    check("DELETE existing file → 204",   r.status == 204);
    check("DELETE → file gone from disk", !fileExists("./www/to_delete.txt"));

    HttpResponse nf = HttpResponseBuilder::build(makeRequest("DELETE", "/ghost.txt"), server);
    check("DELETE missing file → 404",    nf.status == 404);

    { std::ofstream f("./www/uploads/bye.txt"); f << "bye"; }
    HttpRequest delUp = makeRequest("DELETE", "/uploads/bye.txt");
    HttpResponse delR = HttpResponseBuilder::build(delUp, server);
    check("DELETE in allowed location → 204", delR.status == 204);
}

static void testUnknownMethod() {
    std::cout << "\n── UNKNOWN METHOD ───────────────────────────────────────\n";
    ConfigBlock server = makeServerConfig();

    HttpRequest req = makeRequest("PATCH", "/index.html");
    HttpResponse r  = HttpResponseBuilder::build(req, server);
    check("unknown method → 405 (no crash)", r.status == 405);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: webserv <config_file>\n";
        return 1;
    }

    std::cout << "── CONFIG PARSING ───────────────────────────────────────\n";
    try {
        ConfigFile config;
        config.parse(argv[1]);
        const std::vector<ConfigBlock> &servers = config.getServers();
        std::cout << servers.size() << " server(s) found\n\n";
        for (size_t i = 0; i < servers.size(); ++i) {
            std::cout << "server " << i << " {\n";
            printBlock(servers[i], 1);
            std::cout << "}\n\n";
        }
    } catch (std::exception &e) {
        std::cerr << "Config error: " << e.what() << "\n";
        return 1;
    }

    testRequestParsing();
    testGet();
    testAutoindex();
    testRedirect();
    testMethodControl();
    testBodySizeLimit();
    testPost();
    testDelete();
    testUnknownMethod();

    std::cout << "\n";
    return 0;
}
