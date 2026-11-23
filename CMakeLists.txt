cmake_minimum_required(VERSION 3.16)
project(xr_drone CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(udpserver src/udpserver.cpp)
target_include_directories(udpserver PUBLIC include)

add_library(message src/message.cpp)
target_include_directories(message PUBLIC include)

find_package(MAVSDK REQUIRED)

add_executable(xr_drone_server src/main.cpp)
target_link_libraries(xr_drone_server PRIVATE udpserver message MAVSDK::mavsdk)

add_executable(udp_echo tools/udp_echo.cpp)
target_link_libraries(udp_echo PRIVATE udpserver message)

add_executable(message_test tools/message_test.cpp)
target_link_libraries(message_test PRIVATE message)
