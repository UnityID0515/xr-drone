FROM px4io/px4-sitl-gazebo:latest

RUN apt-get update && apt-get install -y build-essential git

RUN wget https://github.com/mavlink/MAVSDK/releases/download/v4.2.1/libmavsdk-dev_4.2.1_ubuntu24.04_amd64.deb \
    && dpkg -i libmavsdk-dev_4.2.1_ubuntu24.04_amd64.deb