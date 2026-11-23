#include "udpserver.h"

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>

UdpServer::UdpServer(uint16_t port) : port_(port) {}

UdpServer::~UdpServer() { stop(); }

bool UdpServer::start() {
  if (fd_ >= 0) return true;

  fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_ < 0) {
    perror("socket");
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(port_);

  if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    perror("bind");
    stop();
    return false;
  }
  return true;
}

void UdpServer::stop() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  client_known_ = false;
}

ssize_t UdpServer::receive(uint8_t* buf, size_t cap, int timeout_ms) {
  if (fd_ < 0) return -1;

  pollfd pfd{fd_, POLLIN, 0};
  int ready = ::poll(&pfd, 1, timeout_ms);
  if (ready < 0) {
    perror("poll");
    return -1;
  }
  if (ready == 0) return 0;

  sockaddr_in from{};
  socklen_t from_len = sizeof(from);
  ssize_t n = ::recvfrom(fd_, buf, cap, 0,
                         reinterpret_cast<sockaddr*>(&from), &from_len);
  if (n < 0) {
    perror("recvfrom");
    return -1;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  client_ = from;
  client_known_ = true;
  last_rx_ = std::chrono::steady_clock::now();
  return n;
}

ssize_t UdpServer::send(const uint8_t* buf, size_t len) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (fd_ < 0 || !client_known_) return -1;
  return ::sendto(fd_, buf, len, 0, reinterpret_cast<const sockaddr*>(&client_),
                  sizeof(client_));
}

bool UdpServer::has_client(int timeout_ms) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!client_known_) return false;
  return std::chrono::steady_clock::now() - last_rx_ <
         std::chrono::milliseconds(timeout_ms);
}
