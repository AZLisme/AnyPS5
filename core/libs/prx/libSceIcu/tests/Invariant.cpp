#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

extern "C" {
void APS5_VABI u_charsToUChars_nid_postfix(const char*, char16_t*, std::int32_t);
void APS5_VABI u_UCharsToChars_nid_postfix(const char16_t*, char*, std::int32_t);
std::int8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char*, std::int32_t);
std::int8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t*, std::int32_t);
std::int32_t APS5_VABI uprv_compareASCIIPropertyNames_nid_postfix(const char*, const char*);
std::int32_t APS5_VABI uprv_compareEBCDICPropertyNames_nid_postfix(const char*, const char*);
std::int32_t APS5_VABI uprv_compareInvEbcdicAsAscii_nid_postfix(const char*, const char*);
char APS5_VABI uprv_ebcdicToAscii_nid_postfix(char);
char APS5_VABI uprv_ebcdicToLowercaseAscii_nid_postfix(char);
std::uint8_t* APS5_VABI uprv_aestrncpy_nid_postfix(std::uint8_t*, const std::uint8_t*, std::int32_t);
}

namespace {

void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "ICU invariant: %s\n", message);
        std::abort();
    }
}

bool Invariant(std::uint32_t value) {
    constexpr std::uint32_t masks[]{0xFFFFFBFF, 0xFFFFFFE5, 0x87FFFFFE, 0x87FFFFFE};
    return value < 128 && ((masks[value / 32] >> (value % 32)) & 1) != 0;
}

void Repertoire() {
    std::array<char, 256> bytes{};
    std::array<char16_t, 258> wide{};
    wide.fill(0xABCD);
    for (unsigned value = 0; value < bytes.size(); ++value) bytes[value] = static_cast<char>(value);
    const auto preservedBytes = bytes;
    u_charsToUChars_nid_postfix(bytes.data(), wide.data() + 1, bytes.size());
    Require(wide.front() == 0xABCD && wide.back() == 0xABCD && bytes == preservedBytes, "byte conversion guards/source");
    for (unsigned value = 0; value < bytes.size(); ++value) {
        Require(wide[value + 1] == value, "byte conversion did not zero-extend all bytes");
        const char text[]{static_cast<char>(value), 0};
        Require(uprv_isInvariantString_nid_postfix(text, 1) == Invariant(value), "byte invariant predicate");
        Require(uprv_isInvariantString_nid_postfix(text, -1) == Invariant(value), "terminated byte invariant predicate");
    }
    std::vector<char16_t> unicode(65536);
    std::vector<char> narrowed(unicode.size() + 2, static_cast<char>(0xAA));
    for (std::uint32_t value = 0; value < unicode.size(); ++value) unicode[value] = static_cast<char16_t>(value);
    u_UCharsToChars_nid_postfix(unicode.data(), narrowed.data() + 1, unicode.size());
    Require(static_cast<std::uint8_t>(narrowed.front()) == 0xAA && static_cast<std::uint8_t>(narrowed.back()) == 0xAA, "Unicode conversion guards");
    for (std::uint32_t value = 0; value < unicode.size(); ++value) {
        Require(unicode[value] == value && narrowed[value + 1] == (Invariant(value) ? static_cast<char>(value) : 0), "release Unicode narrowing result/source");
        const char16_t text[]{static_cast<char16_t>(value), 0};
        Require(uprv_isInvariantUString_nid_postfix(text, 1) == Invariant(value), "Unicode invariant predicate");
        Require(uprv_isInvariantUString_nid_postfix(text, -1) == Invariant(value), "terminated Unicode invariant predicate");
    }
    for (const auto length : {std::numeric_limits<std::int32_t>::min(), -1, 0}) {
        u_charsToUChars_nid_postfix(nullptr, nullptr, length);
        u_UCharsToChars_nid_postfix(nullptr, nullptr, length);
        std::array<char16_t, 2> untouched{0xABCD, 0x1234};
        u_charsToUChars_nid_postfix(nullptr, untouched.data(), length);
        Require(untouched == std::array<char16_t, 2>{0xABCD, 0x1234}, "nonpositive widening wrote");
    }
    Require(uprv_isInvariantString_nid_postfix(nullptr, 0) == 1 && uprv_isInvariantUString_nid_postfix(nullptr, 0) == 1, "zero-length invariant read pointer");
    const char embedded[]{'A', 0, '@', 0};
    const char16_t wideEmbedded[]{u'A', 0, u'@', 0};
    for (auto length : {1, 2, -1, -7, std::numeric_limits<std::int32_t>::min()}) {
        Require(uprv_isInvariantString_nid_postfix(embedded, length) == 1 && uprv_isInvariantUString_nid_postfix(wideEmbedded, length) == 1, "negative length or embedded NUL handling");
    }
    Require(uprv_isInvariantString_nid_postfix(embedded, 3) == 0 && uprv_isInvariantUString_nid_postfix(wideEmbedded, 3) == 0, "counted invariant stopped at NUL");
}

std::uint8_t Lower(std::uint8_t value, bool ebcdic) {
    if (ebcdic) {
        const auto upper = (value >= 0xC1 && value <= 0xC9) || (value >= 0xD1 && value <= 0xD9) || (value >= 0xE2 && value <= 0xE9);
        return upper ? value - 0x40 : value;
    }
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

std::vector<std::uint8_t> Normalize(const char* text, bool ebcdic) {
    const std::vector<std::uint8_t> ignored = ebcdic ? std::vector<std::uint8_t>{0x60, 0x6D, 0x40, 0x05, 0x15, 0x25, 0x0B, 0x0C, 0x0D} : std::vector<std::uint8_t>{'-', '_', ' ', 9, 10, 11, 12, 13};
    std::vector<std::uint8_t> normalized;
    while (*text != 0) {
        const auto value = static_cast<std::uint8_t>(*text++);
        if (std::find(ignored.begin(), ignored.end(), value) == ignored.end()) normalized.push_back(Lower(value, ebcdic));
    }
    normalized.push_back(0);
    return normalized;
}

int PropertyOrder(const char* left, const char* right, bool ebcdic) {
    const auto first = Normalize(left, ebcdic), second = Normalize(right, ebcdic);
    std::size_t index = 0;
    while (first[index] == second[index] && first[index] != 0) ++index;
    return static_cast<int>(first[index]) - second[index];
}

std::int32_t EbcdicRank(std::uint8_t value) {
    const auto mapped = static_cast<std::uint8_t>(uprv_ebcdicToAscii_nid_postfix(static_cast<char>(value)));
    return value == 0 ? 0 : mapped != 0 && Invariant(mapped) ? mapped : -static_cast<int>(value);
}

void BytePairs() {
    std::uint64_t asciiHash = 14695981039346656037ull, lowercaseHash = asciiHash;
    for (unsigned left = 0; left < 256; ++left) {
        asciiHash = (asciiHash ^ static_cast<std::uint8_t>(uprv_ebcdicToAscii_nid_postfix(static_cast<char>(left)))) * 1099511628211ull;
        lowercaseHash = (lowercaseHash ^ static_cast<std::uint8_t>(uprv_ebcdicToLowercaseAscii_nid_postfix(static_cast<char>(left)))) * 1099511628211ull;
        for (unsigned right = 0; right < 256; ++right) {
            const char first[]{static_cast<char>(left), 0};
            const char second[]{static_cast<char>(right), 0};
            Require(uprv_compareASCIIPropertyNames_nid_postfix(first, second) == PropertyOrder(first, second, false), "ASCII byte-pair property order");
            Require(uprv_compareEBCDICPropertyNames_nid_postfix(first, second) == PropertyOrder(first, second, true), "EBCDIC byte-pair property order");
            const auto expected = left == right ? 0 : EbcdicRank(left) - EbcdicRank(right);
            Require(uprv_compareInvEbcdicAsAscii_nid_postfix(first, second) == expected, "EBCDIC invariant byte-pair order");
            const char prefixedFirst[]{static_cast<char>(0xFF), static_cast<char>(left), 0};
            const char prefixedSecond[]{static_cast<char>(0xFF), static_cast<char>(right), 0};
            Require(uprv_compareInvEbcdicAsAscii_nid_postfix(prefixedFirst, prefixedSecond) == expected, "identical variant prefix must not terminate comparison");
        }
    }
    Require(asciiHash == 0x1E6F558F68CD78CBull && lowercaseHash == 0x599A3526F37142EBull, "source EBCDIC mapping fingerprint");
    Require(uprv_ebcdicToAscii_nid_postfix(static_cast<char>(0xE0)) == 0x5C && uprv_ebcdicToLowercaseAscii_nid_postfix(static_cast<char>(0xE0)) == 0x7C, "EBCDIC E0 source mapping distinction");
}

void PropertySequences() {
    std::uint32_t state = 0x73519AC3;
    const auto next = [&state] { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; };
    for (unsigned sample = 0; sample < 3000; ++sample) {
        std::array<char, 65> first{}, second{};
        for (unsigned index = 0; index < 64; ++index) {
            first[index] = static_cast<char>(next());
            second[index] = static_cast<char>(next());
        }
        Require(uprv_compareASCIIPropertyNames_nid_postfix(first.data(), second.data()) == PropertyOrder(first.data(), second.data(), false), "ASCII sequence property order");
        Require(uprv_compareEBCDICPropertyNames_nid_postfix(first.data(), second.data()) == PropertyOrder(first.data(), second.data(), true), "EBCDIC sequence property order");
        Require(uprv_compareASCIIPropertyNames_nid_postfix(first.data(), first.data()) == 0 && uprv_compareEBCDICPropertyNames_nid_postfix(first.data(), first.data()) == 0, "same property text differs");
    }
    Require(uprv_compareASCIIPropertyNames_nid_postfix("\t\n White_ SPACE -", "whitespace") == 0, "ASCII loose property equivalence");
    const char ebcdicFirst[]{0x40, static_cast<char>(0xC1), 0x60, 0x05, 0x15, static_cast<char>(0xD1), 0x6D, 0};
    const char ebcdicSecond[]{static_cast<char>(0x81), static_cast<char>(0x91), 0};
    Require(uprv_compareEBCDICPropertyNames_nid_postfix(ebcdicFirst, ebcdicSecond) == 0, "EBCDIC loose property equivalence");
    std::vector<char> ignored(100000, '_');
    ignored.push_back(0);
    Require(uprv_compareASCIIPropertyNames_nid_postfix(ignored.data(), "") == 0, "long delimiter-only property");
}

void Copies() {
    for (unsigned byte = 0; byte < 256; ++byte) {
        const std::array<std::uint8_t, 4> source{static_cast<std::uint8_t>(byte), 0xC1, 0, 0xC2};
        for (int count : {-7, -1, 0, 1, 2, 3, 6}) {
            std::array<std::uint8_t, 12> output;
            output.fill(0xAA);
            auto expected = output;
            const auto length = byte == 0 ? 0 : 2;
            const auto limit = count == -1 ? length + 1 : count;
            for (int index = 0; index < limit; ++index)
                expected[index + 2] = index < length ? static_cast<std::uint8_t>(uprv_ebcdicToAscii_nid_postfix(static_cast<char>(source[index]))) : 0;
            Require(uprv_aestrncpy_nid_postfix(output.data() + 2, source.data(), count) == output.data() + 2 && output == expected, "EBCDIC copy count/padding/canary");
        }
    }
    std::array<std::uint8_t, 32> shared;
    shared.fill(0xAA);
    shared[20] = 0xC1; shared[21] = 0x04; shared[22] = 0xC2; shared[23] = 0;
    auto expected = shared;
    expected[2] = 'A'; expected[3] = 0; expected[4] = 'B'; expected[5] = 0; expected[6] = 0;
    Require(uprv_aestrncpy_nid_postfix(shared.data() + 2, shared.data() + 20, 5) == shared.data() + 2 && shared == expected, "disjoint shared-buffer copy or mapped NUL continuation");
    const std::uint8_t empty[]{0};
    Require(uprv_aestrncpy_nid_postfix(nullptr, empty, 0) == nullptr && uprv_aestrncpy_nid_postfix(nullptr, empty, -7) == nullptr, "no-write copy return");
}

} // namespace

int main() {
    Repertoire();
    BytePairs();
    PropertySequences();
    Copies();
}
