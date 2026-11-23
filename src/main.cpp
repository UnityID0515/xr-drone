// XR-Drone server: bridges PX4 (MAVSDK) and the Godot client over UDP.
//
//   telemetry: PX4 -> MAVSDK callbacks -> encode (NED -> EUS) -> Godot
//   control:   Godot "movement" -> body velocity -> MAVSDK offboard -> PX4
//
// The server learns the client address from its "ping". Threads:
//   - rx thread (started first): answers pings, stores the latest movement.
//   - MAVSDK threads: send telemetry from callbacks.
//   - main thread: arm, take off, then stream offboard setpoints until
//     SIGINT/SIGTERM, then land.

#include <mavsdk/mavsdk.hpp>
#include <mavsdk/plugins/action/action.hpp>
#include <mavsdk/plugins/offboard/offboard.hpp>
#include <mavsdk/plugins/telemetry/telemetry.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#include "message.h"
#include "udpserver.h"

namespace {

using Clock = std::chrono::steady_clock;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

constexpr double kTelemetryRateHz = 30.0;
constexpr int kClientTimeoutMs = 5000;
// No movement packet for this long means "stop" (the client sends while a key
// is held, so silence means released).
constexpr milliseconds kMovementTimeout{200};
constexpr milliseconds kSetpointPeriod{50};  // 20 Hz offboard stream
const char* const kDefaultMavsdkUrl = "udpin://0.0.0.0:14540";

std::atomic<bool> running{true};

// Latest movement command from the client, shared rx thread -> main thread.
class MovementState {
 public:
  void set(const Movement& m) {
    std::lock_guard<std::mutex> lock(mutex_);
    movement_ = m;
    stamp_ = Clock::now();
    fresh_ = true;
  }

  // Zero when nothing arrived recently.
  Movement get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!fresh_ || Clock::now() - stamp_ > kMovementTimeout) return {0, 0, 0};
    return movement_;
  }

 private:
  mutable std::mutex mutex_;
  Movement movement_{0, 0, 0};
  Clock::time_point stamp_{};
  bool fresh_ = false;
};

void rx_loop(UdpServer& server, MovementState& movement) {
  bool had_client = false;
  uint8_t buf[1500];
  while (running) {
    const ssize_t n = server.receive(buf, sizeof(buf), 100);
    if (n < 0) {
      running = false;
      return;
    }

    const bool has_client = server.has_client(kClientTimeoutMs);
    if (has_client != had_client) {
      printf("udp: client %s\n", has_client ? "connected" : "timed out");
      had_client = has_client;
    }
    if (n == 0) continue;

    MessageType type;
    const uint8_t* payload;
    size_t payload_len;
    if (!decode_header(buf, static_cast<size_t>(n), type, payload,
                       payload_len)) {
      continue;
    }

    switch (type) {
      case MessageType::kPing: {
        const auto reply = encode_ping();
        server.send(reply.data(), reply.size());
        break;
      }
      case MessageType::kMovement: {
        Movement m;
        if (decode(payload, payload_len, m)) movement.set(m);
        break;
      }
      default:
        break;
    }
  }
}

// Sleep that wakes early on shutdown. Returns false if shutting down.
bool wait_ms(int ms) {
  sleep_for(milliseconds(ms));
  return running;
}

}  // namespace

int main() {
  struct sigaction sa{};
  sa.sa_handler = [](int) { running = false; };
  sa.sa_flags = SA_RESETHAND;  // a second Ctrl+C kills immediately
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);

  const char* url_env = std::getenv("XR_DRONE_MAVSDK_URL");
  const std::string url = url_env ? url_env : kDefaultMavsdkUrl;

  UdpServer server;
  if (!server.start()) return 1;
  printf("udp: listening on port %u\n", UdpServer::kDefaultPort);

  MovementState movement;
  std::thread rx(rx_loop, std::ref(server), std::ref(movement));

  // Single exit path so the rx thread is always joined.
  const int exit_code = [&]() -> int {
    mavsdk::Mavsdk mavsdk{
        mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::GroundStation}};
    if (mavsdk.add_any_connection(url) != mavsdk::ConnectionResult::Success) {
      fprintf(stderr, "mavsdk: connection to %s failed\n", url.c_str());
      return 1;
    }
    printf("mavsdk: waiting for PX4 on %s ...\n", url.c_str());

    auto system = mavsdk.first_autopilot(30.0);
    if (!system) {
      fprintf(stderr, "mavsdk: no autopilot found within 30 s\n");
      return 1;
    }
    printf("mavsdk: autopilot connected\n");

    mavsdk::Action action{system.value()};
    mavsdk::Telemetry telemetry{system.value()};
    mavsdk::Offboard offboard{system.value()};

    if (telemetry.set_rate_position_velocity_ned(kTelemetryRateHz) !=
        mavsdk::Telemetry::Result::Success) {
      fprintf(stderr, "mavsdk: could not set position rate (using default)\n");
    }
    if (telemetry.set_rate_attitude_quaternion(kTelemetryRateHz) !=
        mavsdk::Telemetry::Result::Success) {
      fprintf(stderr, "mavsdk: could not set attitude rate (using default)\n");
    }

    // These run on MAVSDK threads; UdpServer::send() is thread-safe.
    telemetry.subscribe_position_velocity_ned(
        [&server](mavsdk::Telemetry::PositionVelocityNed pv) {
          if (!server.has_client(kClientTimeoutMs)) return;
          const Position ned{pv.position.north_m,  pv.position.east_m,
                             pv.position.down_m,   pv.velocity.north_m_s,
                             pv.velocity.east_m_s, pv.velocity.down_m_s};
          const auto packet = encode(ned_to_eus(ned));
          server.send(packet.data(), packet.size());
        });

    telemetry.subscribe_attitude_quaternion(
        [&server](mavsdk::Telemetry::Quaternion q) {
          if (!server.has_client(kClientTimeoutMs)) return;
          const auto packet = encode(ned_to_eus(Attitude{q.w, q.x, q.y, q.z}));
          server.send(packet.data(), packet.size());
        });

    action.hold();
    printf("flight: waiting for the drone to be armable ...\n");
    while (running && !telemetry.health().is_armable) {
      if (!wait_ms(1000)) break;
    }
    if (!running) return 0;

    if (action.arm() != mavsdk::Action::Result::Success) {
      fprintf(stderr, "flight: arm failed\n");
      return 1;
    }
    printf("flight: armed\n");

    if (action.takeoff() != mavsdk::Action::Result::Success) {
      fprintf(stderr, "flight: takeoff failed\n");
      return 1;
    }
    printf("flight: taking off\n");

    while (running &&
           !(telemetry.in_air() &&
             telemetry.flight_mode() == mavsdk::Telemetry::FlightMode::Hold)) {
      sleep_for(milliseconds(100));
    }
    if (!running) {
      action.land();
      return 0;
    }

    // Offboard needs a setpoint before it can start.
    offboard.set_velocity_body({});
    if (offboard.start() != mavsdk::Offboard::Result::Success) {
      fprintf(stderr, "flight: offboard start failed\n");
      action.land();
      return 1;
    }
    printf("flight: offboard started\n");

    while (running) {
      const BodyVelocity v = to_body_velocity(movement.get());
      mavsdk::Offboard::VelocityBodyYawspeed setpoint{};
      setpoint.forward_m_s = v.forward;
      setpoint.right_m_s = v.right;
      setpoint.down_m_s = v.down;
      setpoint.yawspeed_deg_s = 0.0f;
      offboard.set_velocity_body(setpoint);
      sleep_for(kSetpointPeriod);
    }

    printf("flight: shutting down, landing\n");
    if (action.land() != mavsdk::Action::Result::Success) {
      fprintf(stderr, "flight: land failed\n");
      return 1;
    }
    while (telemetry.armed()) sleep_for(milliseconds(100));
    printf("flight: landed and disarmed\n");
    return 0;
  }();

  running = false;
  rx.join();
  return exit_code;
}
