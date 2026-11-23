#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Wire format (little-endian, float32):
//   [u8 header_size][header_size bytes: type name][payload]
// Type names: "ping" (no payload), "position" (6 floats), "attitude" (4 floats),
// "movement" (3 floats, Godot -> server).

enum class MessageType { kUnknown, kPing, kPosition, kAttitude, kMovement };

// Position and velocity. Frame is NED on the MAVSDK side, EUS on the Godot side.
struct Position {
  float x, y, z;
  float vx, vy, vz;
};

// Unit quaternion, body-to-world rotation.
struct Attitude {
  float w, x, y, z;
};

// Velocity command from Godot in the Godot (EUS) frame, in m/s relative to the
// drone's body: x right, y up, z back (so forward is -z).
struct Movement {
  float x, y, z;
};

// The same command in the body frame MAVSDK offboard expects.
struct BodyVelocity {
  float forward, right, down;
};

std::vector<uint8_t> encode_ping();
std::vector<uint8_t> encode(const Position& msg);
std::vector<uint8_t> encode(const Attitude& msg);
std::vector<uint8_t> encode(const Movement& msg);

// Parse the header. On success sets type and points payload into buf.
// Unknown names give kUnknown (still returns true). False if malformed.
bool decode_header(const uint8_t* buf, size_t len, MessageType& type,
                   const uint8_t*& payload, size_t& payload_len);

// False if payload_len does not match the message size exactly.
bool decode(const uint8_t* payload, size_t payload_len, Position& out);
bool decode(const uint8_t* payload, size_t payload_len, Attitude& out);
bool decode(const uint8_t* payload, size_t payload_len, Movement& out);

// Frame conversion. NED (x north, y east, z down) <-> Godot EUS
// (x east, y up, z south). Positions and velocities map as
// (x, y, z) -> (y, -z, -x). The mapping is a proper rotation, so a
// quaternion keeps w and maps its vector part the same way.
Position ned_to_eus(const Position& p);
Position eus_to_ned(const Position& p);
Attitude ned_to_eus(const Attitude& a);
Attitude eus_to_ned(const Attitude& a);

// Godot movement (x right, y up, z back) -> body velocity.
BodyVelocity to_body_velocity(const Movement& m);
