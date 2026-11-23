# XR Drone

Control a simulated PX4 drone from a Godot front end. A C++ server bridges Godot and PX4 SITL (Gazebo): it streams the drone's position and attitude to Godot and turns Godot's movement commands into MAVSDK offboard velocity setpoints. Everything travels over UDP in a small binary format (see [SPEC.md](SPEC.md)).

```
Godot (host) <--UDP 4444--> xr_drone_server <--MAVLink 14540--> PX4 + Gazebo
```

## Setup

1. Install Docker and VS Code with the Dev Containers extension.
2. Open this folder in VS Code and choose **Reopen in Container**. The image is based on `px4io/px4-sitl-gazebo` and adds the MAVSDK C++ library. UDP port 4444 is published to the host.
3. Install [Godot 4](https://godotengine.org/download/linux/) on the host. It is a single binary and doesn't need to run in the container.

## Usage

Run the **PX4 + Server** task (`Terminal > Run Task`). It builds the server, starts PX4 with Gazebo (headless, vehicle from `PX4_SIM_MODEL`, default `gz_x500`), and starts the server. The server waits for PX4, arms, takes off, and enters offboard mode. Press Ctrl+C in the server terminal to land and exit.

The tasks can also be run one at a time: `Build`, `PX4`, `Server`.

Then open `godot/` in Godot and press play. The client pings `127.0.0.1:4444` every 0.5 s. The server starts streaming telemetry after the first ping and stops if it hears nothing for 5 s.

Without the tasks:

```
cmake -S . -B build && cmake --build build -j
px4-gazebo                       # in one terminal (HEADLESS=1 for no GUI)
./build/xr_drone_server          # in another
```

### Controls

`WASD` moves, `Q`/`E` changes altitude. When no movement packet arrives for 200 ms the drone stops.

## Protocol

Little-endian, `float32` payloads.

```
[u8 header_size][header_size bytes: type name][payload]
```

| Type       | Direction        | Payload                                   |
| ---------- | ---------------- | ----------------------------------------- |
| `ping`     | both             | none (the server replies with a ping)     |
| `position` | server -> Godot  | `x y z vx vy vz`, Godot frame (EUS)       |
| `attitude` | server -> Godot  | quaternion `w x y z`, Godot frame (EUS)   |
| `movement` | Godot -> server  | `x y z` m/s in the Godot frame, body-relative: x right, y up, z back |

The server converts between MAVSDK's NED frame and Godot's EUS frame before sending and after receiving.

## Layout

| Path                | Contents                                              |
| ------------------- | ----------------------------------------------------- |
| `src/main.cpp`      | server: MAVSDK telemetry, flight sequence, offboard   |
| `src/udpserver.cpp` | single-client UDP socket (`include/udpserver.h`)      |
| `src/message.cpp`   | binary codec and frame conversion (`include/message.h`) |
| `tools/`            | `message_test` (codec tests), `udp_echo` (socket check) |
| `godot/`            | Godot project; `scripts/message.gd` is the client codec |
| `.devcontainer/`    | dev container definition                              |
| `.vscode/`          | build and run tasks                                   |

Run the codec tests with `./build/message_test`.
