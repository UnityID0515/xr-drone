#include <cstdio>

#include "message.h"
#include "udpserver.h"

// Replies to ping with ping; echoes anything else back unchanged.
int main() {
  UdpServer server;
  if (!server.start()) return 1;
  printf("listening on udp/%u\n", UdpServer::kDefaultPort);

  uint8_t buf[1500];
  for (;;) {
    ssize_t n = server.receive(buf, sizeof(buf), 1000);
    if (n < 0) return 1;
    if (n == 0) continue;

    MessageType type;
    const uint8_t* payload;
    size_t payload_len;
    if (decode_header(buf, static_cast<size_t>(n), type, payload,
                      payload_len) &&
        type == MessageType::kPing) {
      printf("ping\n");
      auto reply = encode_ping();
      server.send(reply.data(), reply.size());
      continue;
    }
    printf("rx %zd bytes, echoing\n", n);
    server.send(buf, static_cast<size_t>(n));
  }
}
