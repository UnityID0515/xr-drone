#include "message.h"

#include <cstring>
#include <string>

static_assert(sizeof(float) == 4, "float32 required");

namespace {

constexpr char kPingName[] = "ping";
constexpr char kPositionName[] = "position";
constexpr char kAttitudeName[] = "attitude";
constexpr char kMovementName[] = "movement";

// Assumes a little-endian host (x86/ARM), matching the wire format.
std::vector<uint8_t> make_packet(const char* name, const float* data,
                                 size_t count) {
  const size_t name_len = std::strlen(name);
  std::vector<uint8_t> out(1 + name_len + count * sizeof(float));
  out[0] = static_cast<uint8_t>(name_len);
  std::memcpy(out.data() + 1, name, name_len);
  if (count > 0) {
    std::memcpy(out.data() + 1 + name_len, data, count * sizeof(float));
  }
  return out;
}

}  // namespace

std::vector<uint8_t> encode_ping() { return make_packet(kPingName, nullptr, 0); }

std::vector<uint8_t> encode(const Position& m) {
  const float f[6] = {m.x, m.y, m.z, m.vx, m.vy, m.vz};
  return make_packet(kPositionName, f, 6);
}

std::vector<uint8_t> encode(const Attitude& m) {
  const float f[4] = {m.w, m.x, m.y, m.z};
  return make_packet(kAttitudeName, f, 4);
}

std::vector<uint8_t> encode(const Movement& m) {
  const float f[3] = {m.x, m.y, m.z};
  return make_packet(kMovementName, f, 3);
}

bool decode_header(const uint8_t* buf, size_t len, MessageType& type,
                   const uint8_t*& payload, size_t& payload_len) {
  if (len < 1) return false;
  const size_t name_len = buf[0];
  if (len < 1 + name_len) return false;

  const std::string name(reinterpret_cast<const char*>(buf + 1), name_len);
  if (name == kPingName) {
    type = MessageType::kPing;
  } else if (name == kPositionName) {
    type = MessageType::kPosition;
  } else if (name == kAttitudeName) {
    type = MessageType::kAttitude;
  } else if (name == kMovementName) {
    type = MessageType::kMovement;
  } else {
    type = MessageType::kUnknown;
  }
  payload = buf + 1 + name_len;
  payload_len = len - 1 - name_len;
  return true;
}

bool decode(const uint8_t* payload, size_t payload_len, Position& out) {
  if (payload_len != 6 * sizeof(float)) return false;
  float f[6];
  std::memcpy(f, payload, sizeof(f));
  out = {f[0], f[1], f[2], f[3], f[4], f[5]};
  return true;
}

bool decode(const uint8_t* payload, size_t payload_len, Attitude& out) {
  if (payload_len != 4 * sizeof(float)) return false;
  float f[4];
  std::memcpy(f, payload, sizeof(f));
  out = {f[0], f[1], f[2], f[3]};
  return true;
}

bool decode(const uint8_t* payload, size_t payload_len, Movement& out) {
  if (payload_len != 3 * sizeof(float)) return false;
  float f[3];
  std::memcpy(f, payload, sizeof(f));
  out = {f[0], f[1], f[2]};
  return true;
}

BodyVelocity to_body_velocity(const Movement& m) { return {-m.z, m.x, -m.y}; }

Position ned_to_eus(const Position& p) {
  return {p.y, -p.z, -p.x, p.vy, -p.vz, -p.vx};
}

Position eus_to_ned(const Position& p) {
  return {-p.z, p.x, -p.y, -p.vz, p.vx, -p.vy};
}

Attitude ned_to_eus(const Attitude& a) { return {a.w, a.y, -a.z, -a.x}; }

Attitude eus_to_ned(const Attitude& a) { return {a.w, -a.z, a.x, -a.y}; }
