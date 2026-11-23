# XR-Drone MAVSDK Server

- **Author:** 黃書昱
- **Project Description:** This project is a MAVSDK server that bridges PX4 and Godot. The server should send telemetry and recv controls via UDP.

## Project Structure

- **Dev Container:** The dev container is built on top of PX4, with some more MAVSDK and C++ dependencies. A task/launch config should start both PX4 and the server, and talk to the Godot client on the host.

- **Godot Client:** This part is **out of scope**. The Godot client will search for a server, and start talking once the ping message has been confirmed.

The message format for the UDP bridge should be kept stupid simple. Initial idea is to use JSON, but it requires a third-party header file. Current idea is to use a message header starting with header size, followed with a message name. Next, the packet will include telemetry or control information, encoded to bytes. This approach is simple, but lacks flexibility and auto sync between server and client with hard-coded decode, but should work with few message types.

## Message Format

The following section details the message format for the UDP socket.

### Header

The header should include 2 sections: the header size and the message type.

- **Header Size:** Decides how many bytes should the recv end read for the message type header. 

- **Message Type:** Currently, there are only 2 message types: position and attitude. Position includes the position and velocity ned info, whether they should be separated is TBD, please suggest your opinion.

### Message

The message should be bytes ordered according to the header message type. For example, position should include { x, y, z, vx, vy, vz } (or separated position and velocity), and attitude should be { x, y, z }. Note that Godot does not use the same coord system as MAVSDK, the former uses EUS and the latter NED. Do he conversion before send and after recv.