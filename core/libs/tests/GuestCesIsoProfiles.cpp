#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

extern "C" {
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_3();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_6();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_7();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_8();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileIso8859_11();
const std::uint8_t* APS5_VABI sceCesRefersUcsProfileCp1252();
int APS5_VABI sceCesSbcToUtf8(const std::uint8_t*, std::uint8_t, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8ToSbc(const std::uint8_t*, std::uint32_t, std::uint32_t*, const std::uint8_t*, std::uint8_t*);
int APS5_VABI sceCesMbcsUcsContextInit(void*, const void*);
int APS5_VABI sceCesMbcsStrToUtf8Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrGetUtf8Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesMbcsStrToUcs2Str(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint16_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesMbcsStrGetUcs2Len(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUtf8StrToMbcsStr(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUtf8StrGetMbcsLen(void*, const std::uint8_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
int APS5_VABI sceCesUcs2StrToMbcsStr(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint8_t*, std::uint32_t, std::uint32_t*);
int APS5_VABI sceCesUcs2StrGetMbcsLen(void*, const std::uint16_t*, std::uint32_t, std::uint32_t*, std::uint32_t*);
}

namespace {

constexpr int Unassigned = static_cast<int>(0x805C0020u);
constexpr int DestinationEnd = static_cast<int>(0x805C0031u);
constexpr int SourceEnd = static_cast<int>(0x805C0011u);
constexpr int InvalidEncode = static_cast<int>(0x805C0014u);
constexpr std::uint32_t Mark = 0x12345678;

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

std::uint32_t Decode(const std::array<std::uint8_t, 4>& bytes, std::uint32_t count) {
    if (count == 1) {
        Require(bytes[0] < 0x80, "noncanonical ASCII");
        return bytes[0];
    }
    Require(count == 2 || count == 3, "unexpected UTF-8 length");
    Require((bytes[1] & 0xC0) == 0x80, "invalid UTF-8 continuation");
    if (count == 2) {
        Require(bytes[0] >= 0xC2 && bytes[0] <= 0xDF, "noncanonical UTF-8 pair");
        return ((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F);
    }
    Require(bytes[0] >= 0xE0 && bytes[0] <= 0xEF && (bytes[2] & 0xC0) == 0x80, "invalid UTF-8 triple");
    const std::uint32_t code = ((bytes[0] & 15) << 12) | ((bytes[1] & 63) << 6) | (bytes[2] & 63);
    Require(code >= 0x800 && !(code >= 0xD800 && code <= 0xDFFF), "noncanonical UTF-8 triple");
    return code;
}

std::uint32_t Encode(std::uint32_t code, std::array<std::uint8_t, 4>& out) {
    if (code < 0x80) { out[0] = static_cast<std::uint8_t>(code); return 1; }
    if (code < 0x800) {
        out[0] = static_cast<std::uint8_t>(0xC0 | code / 64);
        out[1] = static_cast<std::uint8_t>(0x80 | code % 64);
        return 2;
    }
    out[0] = static_cast<std::uint8_t>(0xE0 | code / 4096);
    out[1] = static_cast<std::uint8_t>(0x80 | (code / 64) % 64);
    out[2] = static_cast<std::uint8_t>(0x80 | code % 64);
    return 3;
}

void CheckStrings(void* context, std::uint8_t byte, std::uint32_t code, const std::array<std::uint8_t, 4>& utf8, std::uint32_t size) {
    const std::array<std::uint8_t, 3> input{byte, 0, 0x7F};
    std::array<std::uint8_t, 8> output;
    output.fill(0xAA);
    std::array<std::uint16_t, 3> wide{0xAAAA, 0xBBBB, 0xCCCC};
    std::uint32_t read = Mark, written = Mark;
    if (code == std::numeric_limits<std::uint32_t>::max()) {
        Require(sceCesMbcsStrToUtf8Str(context, input.data(), input.size(), &read, output.data(), output.size(), &written) == Unassigned, "undefined string byte accepted");
        Require(read == Mark && written == Mark && output[0] == 0xAA, "undefined string changed output");
        Require(sceCesMbcsStrToUcs2Str(context, input.data(), input.size(), &read, wide.data(), wide.size(), &written) == Unassigned, "undefined UCS2 byte accepted");
        Require(read == Mark && written == Mark && wide[0] == 0xAAAA, "undefined UCS2 changed output");
        Require(sceCesMbcsStrGetUtf8Len(context, input.data(), input.size(), &read, &written) == Unassigned, "undefined UTF8 length accepted");
        Require(sceCesMbcsStrGetUcs2Len(context, input.data(), input.size(), &read, &written) == Unassigned, "undefined UCS2 length accepted");
        Require(read == Mark && written == Mark, "undefined length outputs changed");
        const std::array<std::uint8_t, 3> prefix{'A', byte, 'B'};
        Require(sceCesMbcsStrToUtf8Str(context, prefix.data(), prefix.size(), &read, output.data(), output.size(), &written) == Unassigned, "undefined prefix string accepted");
        Require(output[0] == 'A' && output[1] == 0xAA && read == Mark && written == Mark, "undefined prefix failure accounting");
        return;
    }
    const std::uint32_t consumed = byte == 0 ? 0 : 1;
    const std::uint32_t produced = byte == 0 ? 0 : size;
    Require(sceCesMbcsStrToUtf8Str(context, input.data(), input.size(), &read, output.data(), produced, &written) == 0, "string decode failed");
    Require(read == consumed && written == produced && std::memcmp(output.data(), utf8.data(), produced) == 0 && output[produced] == 0xAA, "string decode length/terminator mismatch");
    Require(sceCesMbcsStrGetUtf8Len(context, input.data(), input.size(), &read, &written) == 0 && read == consumed && written == produced, "UTF8 measure mismatch");
    Require(sceCesMbcsStrToUcs2Str(context, input.data(), input.size(), &read, wide.data(), consumed, &written) == 0, "UCS2 decode failed");
    Require(read == consumed && written == consumed && (consumed == 0 || wide[0] == code) && wide[consumed] == (consumed == 0 ? 0xAAAA : 0xBBBB), "UCS2 decode mismatch");
    Require(sceCesMbcsStrGetUcs2Len(context, input.data(), input.size(), &read, &written) == 0 && read == consumed && written == consumed, "UCS2 measure mismatch");
    output.fill(0xAA);
    Require(sceCesUtf8StrToMbcsStr(context, utf8.data(), size, &read, output.data(), consumed, &written) == 0 && read == produced && written == consumed, "UTF8 string encode mismatch");
    Require((consumed == 0 || output[0] == byte) && output[consumed] == 0xAA, "UTF8 string encode canary");
    Require(sceCesUtf8StrGetMbcsLen(context, utf8.data(), size, &read, &written) == 0 && read == produced && written == consumed, "MBCS UTF8 measure mismatch");
    const std::array<std::uint16_t, 2> unicode{static_cast<std::uint16_t>(code), 0};
    output.fill(0xAA);
    Require(sceCesUcs2StrToMbcsStr(context, unicode.data(), unicode.size(), &read, output.data(), consumed, &written) == 0 && read == consumed && written == consumed, "UCS2 string encode mismatch");
    Require((consumed == 0 || output[0] == byte) && output[consumed] == 0xAA, "UCS2 string encode canary");
    Require(sceCesUcs2StrGetMbcsLen(context, unicode.data(), unicode.size(), &read, &written) == 0 && read == consumed && written == consumed, "MBCS UCS2 measure mismatch");
    if (byte != 0) {
        output.fill(0xAA);
        read = written = Mark;
        Require(sceCesMbcsStrToUtf8Str(context, input.data(), 1, &read, output.data(), size - 1, &written) == DestinationEnd, "short string destination accepted");
        Require(read == Mark && written == Mark && output[0] == 0xAA, "short string destination changed");
    }
}

void CheckProfile(const std::uint8_t* profile, std::uint64_t fingerprint, unsigned holes) {
    const void* context = nullptr;
    Require(sceCesMbcsUcsContextInit(&context, profile) == 0, "context init failed");
    std::array<int, 65536> reverse;
    reverse.fill(-1);
    std::uint64_t hash = 14695981039346656037ull;
    unsigned missing = 0;
    for (unsigned byte = 0; byte < 256; ++byte) {
        std::array<std::uint8_t, 4> utf8{0xAA, 0xAA, 0xAA, 0xAA};
        std::uint32_t size = Mark;
        const auto result = sceCesSbcToUtf8(profile, static_cast<std::uint8_t>(byte), utf8.data(), 3, &size);
        std::uint32_t code = std::numeric_limits<std::uint32_t>::max();
        if (result == Unassigned) {
            ++missing;
            Require(size == Mark && utf8 == std::array<std::uint8_t, 4>{0xAA, 0xAA, 0xAA, 0xAA}, "undefined scalar output changed");
        } else {
            Require(result == 0 && size >= 1 && size <= 3 && utf8[size] == 0xAA, "scalar decode/canary failure");
            code = Decode(utf8, size);
            Require(reverse[code] == -1, "unexpected duplicate mapping");
            reverse[code] = static_cast<int>(byte);
            for (std::uint32_t capacity = 0; capacity < size; ++capacity) {
                std::array<std::uint8_t, 4> shortOut{0x55, 0x55, 0x55, 0x55};
                std::uint32_t length = Mark;
                Require(sceCesSbcToUtf8(profile, static_cast<std::uint8_t>(byte), shortOut.data(), capacity, &length) == DestinationEnd, "short scalar destination accepted");
                Require(length == Mark && shortOut == std::array<std::uint8_t, 4>{0x55, 0x55, 0x55, 0x55}, "short scalar destination changed");
            }
            std::uint8_t encoded = 0xAA;
            std::uint32_t read = Mark;
            Require(sceCesUtf8ToSbc(utf8.data(), size, &read, profile, &encoded) == 0 && read == size && encoded == byte, "scalar roundtrip mismatch");
            Require(sceCesUtf8ToSbc(utf8.data(), size, nullptr, profile, &encoded) == 0, "optional scalar length rejected");
            for (std::uint32_t limit = 0; limit < size; ++limit) {
                encoded = 0xAA; read = Mark;
                Require(sceCesUtf8ToSbc(utf8.data(), limit, &read, profile, &encoded) == SourceEnd && read == Mark && encoded == 0xAA, "truncated scalar changed output");
            }
        }
        for (unsigned shift : {0u, 8u, 16u, 24u}) hash = (hash ^ ((code >> shift) & 255)) * 1099511628211ull;
        CheckStrings(&context, static_cast<std::uint8_t>(byte), code, utf8, size);
    }
    Require(hash == fingerprint && missing == holes, "authoritative mapping fingerprint mismatch");
    for (std::uint32_t code = 0; code < 65536; ++code) {
        std::array<std::uint8_t, 4> utf8{};
        const auto size = Encode(code, utf8);
        std::uint32_t read = Mark;
        std::uint8_t encoded = 0xAA;
        const int result = sceCesUtf8ToSbc(utf8.data(), size, &read, profile, &encoded);
        const bool surrogate = code >= 0xD800 && code <= 0xDFFF;
        const int expected = surrogate ? InvalidEncode : reverse[code] >= 0 ? 0 : Unassigned;
        Require(result == expected, "BMP reverse mapping status mismatch");
        Require(expected == 0 ? read == size && encoded == reverse[code] : read == Mark && encoded == 0xAA, "BMP reverse output mismatch");
    }
    const std::uint8_t supplementary[]{0xF0, 0x9F, 0x98, 0x80};
    std::uint8_t encoded = 0xAA;
    std::uint32_t read = Mark;
    Require(sceCesUtf8ToSbc(supplementary, 4, &read, profile, &encoded) == Unassigned && encoded == 0xAA && read == Mark, "supplementary scalar accepted");
}

} // namespace

int main() {
    using Getter = const std::uint8_t* (APS5_VABI*)();
    const std::array<Getter, 5> getters{sceCesRefersUcsProfileIso8859_3, sceCesRefersUcsProfileIso8859_6, sceCesRefersUcsProfileIso8859_7, sceCesRefersUcsProfileIso8859_8, sceCesRefersUcsProfileIso8859_11};
    const std::array<std::uint64_t, 5> fingerprints{0x9E4489E44693942Full, 0x7E987F4368C09D74ull, 0xC0592DC3D5DF11ECull, 0xA6BFCE4921487375ull, 0xFD7ECD0FDADF47DFull};
    const std::array<unsigned, 5> holes{7, 45, 3, 36, 8};
    for (std::size_t index = 0; index < getters.size(); ++index) {
        const auto profile = getters[index]();
        Require(profile && profile == getters[index]() && profile != sceCesRefersUcsProfileCp1252(), "unstable or aliased profile");
        for (std::size_t previous = 0; previous < index; ++previous) Require(profile != getters[previous](), "distinct profiles share identity");
        CheckProfile(profile, fingerprints[index], holes[index]);
    }
}
