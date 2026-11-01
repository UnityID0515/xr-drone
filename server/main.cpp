#include <mavsdk/mavsdk.hpp>
#include <mavsdk/plugins/action/action.hpp>
#include <mavsdk/plugins/telemetry/telemetry.hpp>
#include <mavsdk/plugins/offboard/offboard.hpp>

#include <csignal>
#include <thread>

#include "udp_server.hpp"

using namespace mavsdk;
using std::this_thread::sleep_for;
using std::chrono::milliseconds;
using std::chrono::seconds;

static volatile sig_atomic_t running = 1;

int main() {
    struct sigaction sa{};
    sa.sa_handler = [](int) { running = 0; };
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    UDPServer udp(4433, 4444);
    
    Mavsdk mavsdk{Mavsdk::Configuration{ComponentType::GroundStation}};
    if (mavsdk.add_any_connection("udpin://0.0.0.0:14540") != ConnectionResult::Success) {
        fprintf(stderr, "connection failed\n");
        return 1;
    }

    auto system = mavsdk.first_autopilot(10.0);
    if (!system) {
        fprintf(stderr, "no autopilot found\n");
        return 1;
    }

    Action action{*system};
    Telemetry telemetry{*system};
    Offboard offboard{*system};

    telemetry.subscribe_position_velocity_ned([&](Telemetry::PositionVelocityNed pv) {
        udp.send({
            {"type", "position"},
            {"x", pv.position.east_m}, {"y", -pv.position.down_m}, {"z", -pv.position.north_m},
            {"vx", pv.velocity.east_m_s}, {"vy", -pv.velocity.down_m_s}, {"vz", -pv.velocity.north_m_s},
        });
    });

    telemetry.subscribe_attitude_quaternion([&](Telemetry::Quaternion q) {
        udp.send({
            {"type", "attitude"},
            {"w", q.w},
            {"x", q.y},
            {"y", -q.z},
            {"z", -q.x},
        });
    });

    fprintf(stdout, "Connected. Listening on 4433, sending to 4444\n");

    action.hold();
    fprintf(stdout, "Waiting for drone to be ready...\n");
    while (running && !telemetry.health().is_armable) {
        sleep_for(seconds(1));
    }

    if (!running) return 0; 
    if (action.arm() != Action::Result::Success) {
        fprintf(stderr, "arm failed\n");
        return 1;
    }
    fprintf(stdout, "Armed\n");

    if (action.takeoff() != Action::Result::Success) {
        fprintf(stderr, "takeoff failed\n");
        return 1;
    }
    fprintf(stdout, "Taking off\n");

    while (running && !(telemetry.in_air() && telemetry.flight_mode() == Telemetry::FlightMode::Hold))
        sleep_for(milliseconds(100));

    offboard.set_velocity_body({});
    if (offboard.start() != Offboard::Result::Success) {
        fprintf(stderr, "offboard start failed\n");
        action.land();
        return 1;
    }
    fprintf(stdout, "Offboard started\n");

    while (running) {
        auto data = udp.receive();
        if (!data) {
            offboard.set_velocity_body({});
            continue;
        }

        try {
            std::string msg = data->value("type", "");

            if (msg == "movement") {
                Offboard::VelocityBodyYawspeed velocity {
                    .forward_m_s=-data->value("z", 0.0f),
                    .right_m_s=data->value("x", 0.0f),
                    .down_m_s=-data->value("y", 0.0f),
                    .yawspeed_deg_s=0
                };
                offboard.set_velocity_body(velocity);
            }
        } catch (const json::exception& e) {
            fprintf(stderr, "bad message: %s\n", e.what());
        }
    }

    fprintf(stdout, "Shutting down, landing\n");
    if (action.land() != Action::Result::Success) {
        fprintf(stderr, "land failed\n");
        return 1;
    }

    while (telemetry.armed()) sleep_for(milliseconds(100));
    fprintf(stdout, "Landed and disarmed\n");
}
