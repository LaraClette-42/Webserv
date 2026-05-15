#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <ctime>
#include <netinet/in.h>
#include <string>
#include <vector>
#include "../http/HttpRequest.hpp"

struct Client {
    enum State {
        READING_HEADERS,
        READING_BODY,
        CGI_RUNNING,
        READY_TO_RESPOND,
        WRITING_RESPONSE,
        CLOSING,
    };

    int fd;
    int listen_fd;
    sockaddr_in peer_addr;
    std::vector<HttpRequest> requests;

    std::string read_buffer;
    std::string write_buffer;
    std::size_t write_offset;

    bool keep_alive;
    bool should_close;
    std::time_t last_activity;
    State state;
    std::size_t expected_body_size;

    pid_t cgi_pid;
    int cgi_in_fd;
    int cgi_out_fd;
    std::string cgi_body_write;
    std::string cgi_output;
    std::size_t cgi_body_offset;

    Client();
    Client(int fdValue, int listenFd, const sockaddr_in& peer);
};

#endif