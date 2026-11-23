#pragma once

#include <netinet/in.h>
#include <sys/types.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>

// Minimal single-client UDP server. The first packet from a sender (e.g. the
// Godot ping) records its address; later packets from a new sender replace it.
class UdpServer {
 public:
  static constexpr uint16_t kDefaultPort = 4444;

  explicit UdpServer(uint16_t port = kDefaultPort);
  ~UdpServer();

  UdpServer(const UdpServer&) = delete;
  UdpServer& operator=(const UdpServer&) = delete;

  // Bind 0.0.0.0:port. Returns false on failure.
  bool start();
  void stop();

  // Wait up to timeout_ms for a packet. Returns bytes read, 0 on timeout,
  // -1 on error. Records the sender as the current client.
  ssize_t receive(uint8_t* buf, size_t cap, int timeout_ms);

  // Send to the recorded client from the bound socket. -1 if no client.
  ssize_t send(const uint8_t* buf, size_t len);

  // True if a packet was received within the last timeout_ms.
  bool has_client(int timeout_ms = 5000) const;

 private:
  uint16_t port_;
  int fd_ = -1;
  // receive() runs on one thread while send()/has_client() may be called from
  // others (e.g. MAVSDK callbacks), so the client state is guarded.
  mutable std::mutex mutex_;
  sockaddr_in client_{};
  bool client_known_ = false;
  std::chrono::steady_clock::time_point last_rx_{};
};
