#include <cmath>
#include <cstdio>

#include "message.h"

static int failures = 0;

static void check(bool ok, const char* what) {
  if (!ok) {
    printf("FAIL: %s\n", what);
    ++failures;
  }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-6f; }

int main() {
  // Position round trip through the wire format.
  Position p{1, 2, 3, 4, 5, 6};
  auto pkt = encode(p);
  MessageType type;
  const uint8_t* payload;
  size_t payload_len;
  check(decode_header(pkt.data(), pkt.size(), type, payload, payload_len),
        "position header");
  check(type == MessageType::kPosition, "position type");
  Position p2{};
  check(decode(payload, payload_len, p2), "position decode");
  check(p2.x == 1 && p2.vz == 6, "position values");

  // Attitude round trip.
  Attitude a{0.5f, 0.5f, 0.5f, 0.5f};
  auto apkt = encode(a);
  check(decode_header(apkt.data(), apkt.size(), type, payload, payload_len) &&
            type == MessageType::kAttitude,
        "attitude header");
  Attitude a2{};
  check(decode(payload, payload_len, a2) && a2.w == 0.5f, "attitude decode");

  // Movement round trip and mapping to body velocity.
  auto mpkt = encode(Movement{1, 2, 3});
  check(decode_header(mpkt.data(), mpkt.size(), type, payload, payload_len) &&
            type == MessageType::kMovement,
        "movement header");
  Movement m2{};
  check(decode(payload, payload_len, m2) && m2.x == 1 && m2.z == 3,
        "movement decode");
  BodyVelocity bv = to_body_velocity({1, 2, 3});
  check(near(bv.forward, -3) && near(bv.right, 1) && near(bv.down, -2),
        "movement -> body velocity");

  // Ping, unknown names and malformed packets.
  auto ping = encode_ping();
  check(decode_header(ping.data(), ping.size(), type, payload, payload_len) &&
            type == MessageType::kPing && payload_len == 0,
        "ping");
  const uint8_t bad[] = {9, 'a'};
  check(!decode_header(bad, sizeof(bad), type, payload, payload_len),
        "truncated header rejected");
  check(!decode(payload, 3, p2), "wrong payload size rejected");

  // Frame conversion: north 1 m in NED is -Z (forward) in Godot.
  Position n = ned_to_eus({1, 0, 0, 0, 0, 0});
  check(near(n.x, 0) && near(n.y, 0) && near(n.z, -1), "north -> -z");
  // Down 1 m in NED is -Y in Godot.
  Position d = ned_to_eus({0, 0, 1, 0, 0, 0});
  check(near(d.y, -1), "down -> -y");
  // Inverse conversions round trip.
  Position r = eus_to_ned(ned_to_eus(p));
  check(near(r.x, 1) && near(r.y, 2) && near(r.z, 3) && near(r.vx, 4) &&
            near(r.vy, 5) && near(r.vz, 6),
        "position conversion round trip");
  Attitude q{0.1f, 0.2f, 0.3f, 0.4f};
  Attitude q2 = eus_to_ned(ned_to_eus(q));
  check(near(q2.w, 0.1f) && near(q2.x, 0.2f) && near(q2.y, 0.3f) &&
            near(q2.z, 0.4f),
        "attitude conversion round trip");

  printf(failures ? "%d failure(s)\n" : "all passed\n", failures);
  return failures ? 1 : 0;
}
