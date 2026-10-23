#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
char16_t* APS5_VABI u_strFromJavaModifiedUTF8WithSub_nid_postfix(char16_t*, std::int32_t, std::int32_t*, const char*, std::int32_t, std::int32_t, std::int32_t*, std::int32_t*);
char* APS5_VABI u_strToJavaModifiedUTF8_nid_postfix(char*, std::int32_t, std::int32_t*, const char16_t*, std::int32_t, std::int32_t*);
}

static void Require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "ICU modified UTF8: %s\n", message); std::abort(); }
}

int main() {
    constexpr std::int32_t mark = 12345;
    constexpr std::array<char16_t, 5> source{0x41, 0, 0xd800, 0xdc00, 0xffff};
    constexpr std::array<std::uint8_t, 13> expected{0x41, 0xc0, 0x80, 0xed, 0xa0, 0x80, 0xed, 0xb0, 0x80, 0xef, 0xbf, 0xbf, 0};
    std::array<char, 20> output;
    output.fill(0x55);
    std::int32_t length = mark, substitutions = mark, error = 0;
    Require(u_strToJavaModifiedUTF8_nid_postfix(output.data(), output.size(), &length, source.data(), source.size(), &error) == output.data(), "to return pointer");
    Require(error == 0 && length == 12 && output[13] == 0x55, "CESU8 surrogate halves and modified NUL size");
    for (std::size_t index = 0; index < expected.size(); ++index) Require(static_cast<std::uint8_t>(output[index]) == expected[index], "modified encoding oracle");
    std::array<char16_t, 16> restored;
    restored.fill(0x5555);
    Require(u_strFromJavaModifiedUTF8WithSub_nid_postfix(restored.data(), restored.size(), &length, output.data(), 12, -1, &substitutions, &error) == restored.data(), "from return pointer");
    Require(error == 0 && length == 5 && substitutions == 0 && std::equal(source.begin(), source.end(), restored.begin()) && restored[5] == 0 && restored[6] == 0x5555, "modified roundtrip with embedded NUL");
    const std::array<char, 4> standardFour{char(0xf0), char(0x90), char(0x80), char(0x80)};
    restored.fill(0x5555); error = 0; length = mark;
    Require(u_strFromJavaModifiedUTF8WithSub_nid_postfix(restored.data(), restored.size(), &length, standardFour.data(), standardFour.size(), -1, &substitutions, &error) == nullptr && error == 10 && length == mark && restored[0] == 0x5555, "standard four-byte UTF8 is not modified UTF8");
    for (std::int32_t capacity = 0; capacity <= 3; ++capacity) {
        restored.fill(0x5555); error = 0;
        u_strFromJavaModifiedUTF8WithSub_nid_postfix(restored.data(), capacity, &length, standardFour.data(), standardFour.size(), 0x10000, &substitutions, &error);
        Require(substitutions == 1 && length == (capacity == 0 ? 1 : 2), "source-defined capacity-dependent supplementary substitution length");
        Require(error == (capacity < 2 ? 15 : capacity == 2 ? -124 : 0), "supplementary replacement capacity status");
        Require(restored[capacity] == 0x5555, "supplementary replacement canary");
        if (capacity > 0) Require(restored[0] == 0xd800, "partial replacement leading surrogate");
        if (capacity > 1) Require(restored[1] == 0xdc00, "replacement trailing surrogate");
        if (capacity > 2) Require(restored[2] == 0, "replacement terminator");
    }
    const std::array<char, 10> overlong{char(0xc0), char(0x80), char(0xc1), char(0xbf), char(0xe0), char(0x80), char(0x80), char(0xed), char(0xa0), char(0x80)};
    error = 0;
    u_strFromJavaModifiedUTF8WithSub_nid_postfix(restored.data(), restored.size(), &length, overlong.data(), overlong.size(), -1, &substitutions, &error);
    Require(error == 0 && length == 4 && substitutions == 0 && restored[0] == 0 && restored[1] == 0x7f && restored[2] == 0 && restored[3] == 0xd800, "modified overlong and surrogate acceptance");
    const std::array<char, 5> malformed{char(0xf0), char(0x90), char(0x80), 'A', char(0xff)};
    error = 0;
    u_strFromJavaModifiedUTF8WithSub_nid_postfix(restored.data(), restored.size(), &length, malformed.data(), malformed.size(), 0xfffd, &substitutions, &error);
    Require(error == 0 && length == 3 && substitutions == 2 && restored[0] == 0xfffd && restored[1] == 0x41 && restored[2] == 0xfffd && restored[3] == 0, "malformed prefix consumption");
    const std::array<char16_t, 2> shortSource{0x800, 'A'};
    output.fill(0x55); error = 0;
    u_strToJavaModifiedUTF8_nid_postfix(output.data(), 2, &length, shortSource.data(), shortSource.size(), &error);
    Require(error == 15 && length == 4 && output[0] == 0x55 && output[2] == 0x55, "whole encoded units stop at first overflow");
    output.fill(0x55); error = 0;
    u_strToJavaModifiedUTF8_nid_postfix(output.data(), output.size(), &length, source.data(), -1, &error);
    Require(error == 0 && length == 1 && output[0] == 'A' && output[1] == 0 && output[2] == 0x55, "NUL-terminated source");
    error = 0; length = substitutions = mark;
    Require(u_strFromJavaModifiedUTF8WithSub_nid_postfix(restored.data(), restored.size(), &length, nullptr, 1, 0xfffd, &substitutions, &error) == nullptr && error == 1 && length == mark && substitutions == mark, "invalid argument output preservation");
    for (const bool sharedStatus : {false, true}) {
        std::int32_t shared = 0;
        error = 0; restored.fill(0x5555);
        u_strFromJavaModifiedUTF8WithSub_nid_postfix(restored.data(), restored.size(), &shared, "A", 1, 0xfffd, &shared, sharedStatus ? &shared : &error);
        Require(shared == 1 && error == 0 && restored[0] == 0x41 && restored[1] == (sharedStatus ? 0x5555 : 0), "modified conversion retains substitution-before-length output ordering");
    }
    error = 0;
    Require(u_strToJavaModifiedUTF8_nid_postfix(nullptr, 0, &length, source.data(), source.size(), &error) == nullptr && error == 15 && length == 12, "modified preflight");
}
