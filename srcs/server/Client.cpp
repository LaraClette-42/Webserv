#include "Client.hpp"

Client::Client()
    : fd(-1),
      listen_fd(-1),
      write_offset(0),
      keep_alive(true),
      should_close(false),
      last_activity(0),
      state(READING_HEADERS),
      expected_body_size(0),
      cgi_pid(-1),
      cgi_in_fd(-1),
      cgi_out_fd(-1),
      cgi_body_offset(0) {
}

Client::Client(int fdValue, int listenFd, const sockaddr_in& peer)
    : fd(fdValue),
      listen_fd(listenFd),
      peer_addr(peer),
      write_offset(0),
      keep_alive(true),
      should_close(false),
      last_activity(std::time(NULL)),
      state(READING_HEADERS),
      expected_body_size(0),
      cgi_pid(-1),
      cgi_in_fd(-1),
      cgi_out_fd(-1),
      cgi_body_offset(0) {
}
