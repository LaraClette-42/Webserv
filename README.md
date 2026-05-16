*This project has been created as part of the 42 curriculum by rlaigle, zoolszew.*

# Webserv

## Description

Webserv is an HTTP/1.1 server written in C++98. The goal is to implement a web server from scratch, capable of serving static files, executing CGI scripts, handling file uploads, and processing GET, POST and DELETE requests. The server relies on a single non-blocking `select()` loop to manage all I/O across multiple simultaneous clients without threads or forking outside of CGI.

## Instructions

**Compilation**

```
make
```

**Execution**

```
./webserv [configuration file]
```

If no configuration file is provided, the server loads `conf/default.conf` by default.

The configuration file controls listening addresses and ports, the document root, accepted HTTP methods per route, directory listing, HTTP redirections, upload paths, client body size limits, custom error pages and CGI interpreters by file extension. A working example is available in `conf/default.conf`, which starts two servers on ports 8080 and 8081.

**Browser test**

Start the server and open `http://127.0.0.1:8080` in a browser. The default page lets you exercise static file serving, file upload, directory listing, HTTP redirection, and CGI execution (Python, Perl, shell). A cookie and session demo is available at `/cgi-bin/session.py`.

**Stress test**

```
siege -b http://127.0.0.1:8080/
```

Availability should remain above 99.5%.

## Resources

RFC 7230 — HTTP/1.1 Message Syntax and Routing  
RFC 7231 — HTTP/1.1 Semantics and Content  
RFC 3875 — The Common Gateway Interface (CGI/1.1)  
NGINX documentation, used as a reference for configuration syntax and expected HTTP behaviour  
Beej's Guide to Network Programming  

AI was used throughout the project for several tasks: generating boilerplate for the HTTP parser and CGI environment setup, reviewing compliance with the RFC and the subject requirements line by line. All produced code was read, tested and understood before being integrated into the project.
