# XR Drone

Control a simulated PX4 drone from a Godot front end. A C++ server bridges Godot and PX4 SITL (Gazebo): it receives movement commands from Godot over UDP and forwards them to the drone through MAVSDK offboard control.

## Setup

- **distrobox:** Install `distrobox` on your preferred Linux distro and create a `Ubuntu:24.04` container.

```
distrobox create -i ubuntu24.04 -n xr-drone -H ~/Projects/xr-drone
distrobox enter xr-drone
```

Remember to update and upgrade after setting up the container.

```
sudo apt update && sudo apt upgrade -y
```

- **lsb-release:** For some reason it is not included in the base installation. 

```
sudo apt install lsb-release -y
```

- **Gazebo dependencies:** These are copy-pasted from the official PX4 docs.

```
# Add OSRF Gazebo repository (one-time setup)
sudo curl -fsSL https://packages.osrfoundation.org/gazebo.gpg \
  -o /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] \
  http://packages.osrfoundation.org/gazebo/ubuntu-stable $(lsb_release -cs) main" \
  | sudo tee /etc/apt/sources.list.d/gazebo-stable.list > /dev/null
sudo apt update
```

- **The `.deb` packages:** Install these `.deb` packages as well.

    - **PX4 Gazebo SITL**: Follow the `GitHub Actions` link and find the latest scheduled build under Artifacts.
    ```
    https://github.com/PX4/PX4-Autopilot/actions/workflows/build_deb_package.yml?query=event%3Aschedule
    ```

    - **MAVSDK C++ library:** The packages are located under `GitHub Releases`, download the latest version.
    ```
    https://github.com/mavlink/MAVSDK/releases
    ```

Now install them both:
```
# Install (resolves Gazebo dependencies automatically)
sudo apt install ./px4-gazebo_*.deb

sudo dpkg -i libmavsdk-dev*.deb
```

- **Godot:** This one can be installed outside of the container, but since it is just a binary it doesn't really matter. Install from the official `Godot Downloads` page.
```
https://godotengine.org/download/linux/
```

## Usage

Build the MAVSDK server first (from the repo root, inside the container):
```
g++ -std=c++20 -I/usr/include/mavsdk server/main.cpp -o server/server -l:libmavsdk.so.4 -pthread
```

Spin up `SITL` in one terminal using:
```
./sitl
```

and start the `MAVSDK` server (The binary that you compiled yourself).

Open up `Godot` and press `play` on the top right corner.

### Controls

Control the drone with `WASD` keys and `Q/E` for altitude.
