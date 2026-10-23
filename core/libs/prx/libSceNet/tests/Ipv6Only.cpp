#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>

extern "C" {
int APS5_VABI sceNetSocket(const char*, int, int, int);
int APS5_VABI sceNetSocketClose(int);
int APS5_VABI sceNetSetsockopt(int, int, int, const void*, std::uint32_t);
int APS5_VABI sceNetGetsockopt(int, int, int, void*, std::uint32_t*);
int APS5_VABI sceNetBind_nid_postfix(int, const void*, std::uint32_t);
int APS5_VABI sceNetGetsockname(int, void*, std::uint32_t*);
int APS5_VABI sceNetListen(int, int);
int APS5_VABI sceNetConnect(int, const void*, std::uint32_t);
int APS5_VABI sceNetAccept(int, void*, std::uint32_t*);
std::int64_t APS5_VABI sceNetSend(int, const void*, std::size_t, int);
std::int64_t APS5_VABI sceNetSendto(int, const void*, std::size_t, int, const void*, std::uint32_t);
std::int64_t APS5_VABI sceNetRecvfrom(int, void*, std::size_t, int, void*, std::uint32_t*);
int* APS5_VABI sceNetErrnoLoc();
int APS5_VABI setsockopt_nid_postfix(int, int, int, const void*, std::uint32_t);
}

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "IPV6_V6ONLY check failed at line %u (errno %d)\n", location.line(), *sceNetErrnoLoc());
    std::abort();
}

static void RequireOption(int descriptor, int expected) {
    for (std::uint32_t capacity = 0; capacity <= 8; ++capacity) {
        std::array<unsigned char, 8> output;
        output.fill(0xa5);
        auto length = capacity;
        *sceNetErrnoLoc() = 1234;
        Require(sceNetGetsockopt(descriptor, 41, 27, output.data(), &length) == 0);
        Require(*sceNetErrnoLoc() == 1234 && length == std::min<std::uint32_t>(capacity, sizeof(expected)));
        Require(std::memcmp(output.data(), &expected, length) == 0);
        for (auto index = length; index < output.size(); ++index) Require(output[index] == 0xa5);
    }
}

static void Exercise(int descriptor) {
    RequireOption(descriptor, 1);
    for (const int value : {0, 1, -1, std::numeric_limits<int>::min(), 0x10}) {
        *sceNetErrnoLoc() = 1234;
        Require(sceNetSetsockopt(descriptor, 41, 27, &value, sizeof(value)) == 0);
        Require(*sceNetErrnoLoc() == 1234);
        RequireOption(descriptor, value ? 1 : 0);
    }
    const int enabled = 1;
    Require(setsockopt_nid_postfix(descriptor, 41, 27, &enabled, sizeof(enabled)) == 0);
    for (std::uint32_t length = 0; length <= 8; ++length) {
        if (length == sizeof(enabled)) continue;
        Require(sceNetSetsockopt(descriptor, 41, 27, &enabled, length) == static_cast<int>(0x80410116u) && *sceNetErrnoLoc() == 22);
        RequireOption(descriptor, 1);
    }
    Require(sceNetSetsockopt(descriptor, 41, 27, nullptr, sizeof(enabled)) == static_cast<int>(0x80410116u) && *sceNetErrnoLoc() == 22);
    RequireOption(descriptor, 1);
    std::uint32_t length = sizeof(enabled);
    int output = 0;
    Require(sceNetGetsockopt(descriptor, 41, 28, &output, &length) == static_cast<int>(0x8041012au) && *sceNetErrnoLoc() == 42);
    Require(sceNetSetsockopt(descriptor, 41, 28, &enabled, sizeof(enabled)) == static_cast<int>(0x8041012au) && *sceNetErrnoLoc() == 42);
}

static void DualStack(int receiver, int type) {
    const int disabled = 0;
    Require(sceNetSetsockopt(receiver, 41, 27, &disabled, sizeof(disabled)) == 0);
    std::array<std::uint8_t, 28> address{28, 28};
    Require(sceNetBind_nid_postfix(receiver, address.data(), address.size()) == 0);
    auto length = static_cast<std::uint32_t>(address.size());
    Require(sceNetGetsockname(receiver, address.data(), &length) == 0);
    RequireOption(receiver, 0);
    const int enabled = 1;
    Require(sceNetSetsockopt(receiver, 41, 27, &enabled, sizeof(enabled)) == static_cast<int>(0x80410116u) && *sceNetErrnoLoc() == 22);
    RequireOption(receiver, 0);
    std::array<std::uint8_t, 16> destination{16, 2, address[2], address[3], 127, 0, 0, 1};
    const int sender = sceNetSocket("ipv4-peer", 2, type, 0);
    Require(sender >= 0);
    int connection = receiver;
    if (type == 1) {
        Require(sceNetListen(receiver, 1) == 0);
        Require(sceNetConnect(sender, destination.data(), destination.size()) == 0);
        connection = sceNetAccept(receiver, nullptr, nullptr);
        Require(connection >= 0);
    }
    const int timeout = 2000000;
    Require(sceNetSetsockopt(connection, 0xffff, 0x1106, &timeout, sizeof(timeout)) == 0);
    constexpr std::array<char, 4> payload{'d', 'u', 'a', 'l'};
    if (type == 1) Require(sceNetSend(sender, payload.data(), payload.size(), 0) == payload.size());
    else Require(sceNetSendto(sender, payload.data(), payload.size(), 0, destination.data(), destination.size()) == payload.size());
    std::array<char, 4> received{};
    address.fill(0);
    length = address.size();
    Require(sceNetRecvfrom(connection, received.data(), received.size(), 0, address.data(), &length) == received.size());
    Require(received == payload);
    if (type == 2) {
        Require(length == 28 && address[1] == 28);
        Require(address[18] == 0xff && address[19] == 0xff);
        Require(address[20] == 127 && address[21] == 0 && address[22] == 0 && address[23] == 1);
    }
    if (connection != receiver) Require(sceNetSocketClose(connection) == 0);
    Require(sceNetSocketClose(sender) == 0);
}

int main() {
    for (const int type : {1, 2}) {
        const int descriptor = sceNetSocket("ipv6-only", 28, type, 0);
        if (descriptor < 0 && (*sceNetErrnoLoc() == 43 || *sceNetErrnoLoc() == 47)) return 77;
        Require(descriptor >= 0);
        Exercise(descriptor);
        DualStack(descriptor, type);
        Require(sceNetSocketClose(descriptor) == 0);
    }
    const int ipv4 = sceNetSocket("ipv4-only", 2, 2, 0);
    Require(ipv4 >= 0);
    const int value = 1;
    Require(sceNetSetsockopt(ipv4, 41, 27, &value, sizeof(value)) == static_cast<int>(0x80410116u) && *sceNetErrnoLoc() == 22);
    Require(sceNetSocketClose(ipv4) == 0);
}
