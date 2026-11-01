#pragma once

#include <arpa/inet.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <optional>
#include <vector>

#include "json.hpp"

using json = nlohmann::json;

class UDPServer {
public:
    UDPServer(int recvPort, int sendPort) {
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) perror("socket");

        timeval timeout{.tv_sec = 0, .tv_usec = 200000};
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        sockaddr_in local{
            .sin_family = AF_INET,
            .sin_port = htons(recvPort),
            .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)},
        };
        if (bind(sock, (sockaddr*)&local, sizeof(local)) < 0) perror("bind");

        remote = {
            .sin_family = AF_INET,
            .sin_port = htons(sendPort),
            .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)},
        };
    }

    ~UDPServer() { close(sock); }

    UDPServer(const UDPServer&) = delete;
    UDPServer& operator=(const UDPServer&) = delete;

    void send(const json& j) {
        std::string s = j.dump();
        sendto(sock, s.data(), s.size(), 0, (sockaddr*)&remote, sizeof(remote));
    }

    std::optional<json> receive() {
        ssize_t n = recv(sock, buf.data(), buf.size(), 0);
        if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) perror("recv");
            return std::nullopt;
        }

        json j = json::parse(buf.begin(), buf.begin() + n, nullptr, false);
        if (j.is_discarded()) { fprintf(stderr, "invalid JSON\n"); return std::nullopt; }
        return j;
    }

private:
    int sock;
    sockaddr_in remote{};
    std::vector<char> buf = std::vector<char>(65536);
};
