#include "prx/libSceCesCs/libSceCesCs.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>


static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "CP874 check failed at line %u\n", location.line());
    std::abort();
}

int main() {
    const auto* profile = sceCesRefersUcsProfileCp874();
    Require(profile != nullptr && profile == sceCesRefersUcsProfileCp874());
    for (unsigned byte = 0; byte < 256; ++byte) {
        std::array<std::uint8_t, 4> encoded{};
        std::uint32_t length = 0;
        Require(sceCesSbcToUtf8(profile, static_cast<std::uint8_t>(byte), encoded.data(), encoded.size(), &length) == 0);
        Require(length >= 1 && length <= 3);
        if (byte < 128) Require(length == 1 && encoded[0] == byte);
        std::uint8_t decoded = 0;
        std::uint32_t consumed = 0;
        Require(sceCesUtf8ToSbc(encoded.data(), length, &consumed, profile, &decoded) == 0);
        Require(decoded == byte && consumed == length);
        std::array<std::uint8_t, 4> shortBuffer;
        shortBuffer.fill(0xa5);
        auto shortLength = 99u;
        Require(sceCesSbcToUtf8(profile, static_cast<std::uint8_t>(byte), shortBuffer.data(), length - 1, &shortLength) == static_cast<int>(0x805c0031));
        Require(shortLength == length);
        for (const auto value : shortBuffer) Require(value == 0xa5);
    }
    constexpr std::array<std::uint8_t, 8> thai{0xc0, 0xd2, 0xc9, 0xd2, 0xe4, 0xb7, 0xc2, 0};
    constexpr std::array<std::uint16_t, 7> unicode{0x0e20, 0x0e32, 0x0e29, 0x0e32, 0x0e44, 0x0e17, 0x0e22};
    constexpr char8_t utf8[] = u8"ภาษาไทย";
    std::array<std::uintptr_t, 16> context{};
    Require(sceCesMbcsUcsContextInit(context.data(), profile) == 0);
    std::array<std::uint8_t, 24> text;
    text.fill(0xa5);
    std::uint32_t consumed = 99;
    std::uint32_t produced = 99;
    Require(sceCesMbcsStrToUtf8Str(context.data(), thai.data(), thai.size(), &consumed, text.data(), text.size(), &produced) == 0);
    Require(consumed == 7 && produced == sizeof(utf8) - 1);
    Require(std::memcmp(text.data(), utf8, produced) == 0 && text[produced] == 0);
    std::array<std::uint8_t, 8> reversed;
    reversed.fill(0xa5);
    Require(sceCesUtf8StrToMbcsStr(context.data(), text.data(), produced, &consumed, reversed.data(), reversed.size(), &produced) == 0);
    Require(consumed == sizeof(utf8) - 1 && produced == 7 && reversed[7] == 0);
    Require(std::memcmp(reversed.data(), thai.data(), 7) == 0);
    std::array<std::uint16_t, 8> wide;
    wide.fill(0xa5a5);
    Require(sceCesMbcsStrToUcs2Str(context.data(), thai.data(), thai.size(), &consumed, wide.data(), wide.size(), &produced) == 0);
    Require(consumed == 7 && produced == unicode.size() && wide[7] == 0);
    Require(std::memcmp(wide.data(), unicode.data(), sizeof(unicode)) == 0);
    std::array<std::uint8_t, 3> euro{};
    Require(sceCesSbcToUtf8(profile, 0x80, euro.data(), euro.size(), nullptr) == 0);
    Require(euro == std::array<std::uint8_t, 3>{0xe2, 0x82, 0xac});
    constexpr std::array<std::uint8_t, 3> unrelated{0xe4, 0xb8, 0x80};
    std::uint8_t output = 0xa5;
    consumed = 99;
    Require(sceCesUtf8ToSbc(unrelated.data(), unrelated.size(), &consumed, profile, &output) == static_cast<int>(0x805c0020));
    Require(consumed == unrelated.size() && output == 0);
}
