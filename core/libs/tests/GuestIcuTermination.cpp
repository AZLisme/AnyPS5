#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
std::int32_t APS5_VABI u_terminateChars_nid_postfix(char*, std::int32_t, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI u_terminateUChars_nid_postfix(char16_t*, std::int32_t, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI u_terminateUChar32s_nid_postfix(std::int32_t*, std::int32_t, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI u_terminateWChars_nid_postfix(std::uint16_t*, std::int32_t, std::int32_t, std::int32_t*);
}

static void Require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "ICU termination: %s\n", message); std::abort(); }
}

template<typename TUnit>
static void Check(std::int32_t (APS5_VABI* terminate)(TUnit*, std::int32_t, std::int32_t, std::int32_t*)) {
    struct Case { std::int32_t capacity, length, incoming, expected, written; };
    constexpr std::array<Case, 14> cases{{
        {4, 1, 0, 0, 1}, {4, 1, -124, 0, 1}, {4, 1, -128, -128, 1}, {4, 1, 1, 1, -1},
        {4, 4, 0, -124, -1}, {4, 5, 0, 15, -1}, {0, 0, 0, -124, -1}, {0, 1, 0, 15, -1},
        {-1, 0, 0, 15, -1}, {4, -1, -124, -124, -1}, {4, -2, 0, 0, -1}, {4, 1, 15, 15, -1},
        {1, 0, -128, -128, 0}, {0, 0, -128, -124, -1}
    }};
    constexpr TUnit mark = static_cast<TUnit>(0x55);
    for (const auto& test : cases) {
        std::array<TUnit, 8> output;
        output.fill(mark);
        auto error = test.incoming;
        Require(terminate(output.data(), test.capacity, test.length, &error) == test.length && error == test.expected, "return/status fixture");
        for (std::size_t index = 0; index < output.size(); ++index) Require(output[index] == (static_cast<std::int32_t>(index) == test.written ? 0 : mark), "terminator location/canary");
    }
    Require(terminate(nullptr, 4, 1, nullptr) == 1, "null error means no operation");
    std::int32_t error = 10;
    Require(terminate(nullptr, 4, 1, &error) == 1 && error == 10, "incoming failure bypasses destination");
    error = 0;
    Require(terminate(nullptr, 0, 0, &error) == 0 && error == -124, "null empty destination warning");
    error = 0;
    bool threw = false;
    try { terminate(nullptr, 4, 1, &error); }
    catch (const std::runtime_error&) { threw = true; }
    Require(threw, "undefined null write must throw");
}

int main() {
    Check(u_terminateChars_nid_postfix);
    Check(u_terminateUChars_nid_postfix);
    Check(u_terminateUChar32s_nid_postfix);
    Check(u_terminateWChars_nid_postfix);
    std::array<std::uint16_t, 4> guestWide{0x1111, 0x2222, 0x3333, 0x4444};
    std::int32_t error = 0;
    u_terminateWChars_nid_postfix(guestWide.data(), 3, 1, &error);
    Require(error == 0 && guestWide == std::array<std::uint16_t, 4>{0x1111, 0, 0x3333, 0x4444}, "PS5 wchar termination stores exactly sixteen bits");
}
