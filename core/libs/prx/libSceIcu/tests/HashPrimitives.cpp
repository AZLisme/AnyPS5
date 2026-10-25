#include "prx/libSceIcu/libSceIcu.hpp"
#include "SceTypes.hpp"
#include "prx/libSceIcu/include/HashToken.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

using IcuPrimitives::HashToken;
using Hasher = std::int32_t (APS5_VABI*)(HashToken);
using Equal = std::int8_t (APS5_VABI*)(HashToken, HashToken);



namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU hash primitives: %s\n", message);
    std::abort();
}

HashToken Pointer(const void* pointer) {
    HashToken token{};
    token.pointer = const_cast<void*>(pointer);
    return token;
}

unsigned char Fold(unsigned char value) {
    constexpr unsigned char alphabet[] = "abcdefghijklmnopqrstuvwxyz";
    return value >= 'A' && value <= 'Z' ? alphabet[value - 'A'] : value;
}

template<class T> std::int32_t Oracle(const T* text, std::size_t length, bool folded = false) {
    const std::size_t spacing = std::max(std::size_t{1}, length / 32);
    std::vector<std::uint32_t> selected;
    for (std::size_t index = 0; index < length; index += spacing) {
        auto value = static_cast<std::uint32_t>(text[index]);
        if (folded) value = Fold(static_cast<unsigned char>(value));
        selected.push_back(value);
    }
    std::uint32_t sum = 0, power = 1;
    for (auto index = selected.size(); index != 0;) {
        sum += selected[--index] * power;
        power *= 37;
    }
    return std::bit_cast<std::int32_t>(sum);
}

void NoReadContracts() {
    const HashToken null = Pointer(nullptr);
    std::array<char16_t, 64> storage{};
    const auto end = Pointer(storage.data() + storage.size());
    for (const auto hasher : std::array<Hasher, 3>{uhash_hashChars_nid_postfix, uhash_hashUChars_nid_postfix, uhash_hashIChars_nid_postfix}) Require(hasher(null) == 0, "null pointer hash");
    for (const auto equal : std::array<Equal, 3>{uhash_compareChars_nid_postfix, uhash_compareUChars_nid_postfix, uhash_compareIChars_nid_postfix}) {
        Require(equal(null, null) == 1 && equal(end, end) == 1, "identity comparison without dereference");
        Require(equal(end, null) == 0 && equal(null, end) == 0, "one null comparison without dereference");
    }
    for (const auto length : {std::numeric_limits<std::int32_t>::min(), -32, -1, 0, 1, std::numeric_limits<std::int32_t>::max()}) {
        Require(ustr_hashCharsN_nid_postfix(nullptr, length) == 0, "null counted byte hash");
        Require(ustr_hashUCharsN_nid_postfix(nullptr, length) == 0, "null counted UTF16 hash");
    }
    for (std::int32_t length = -32; length <= 0; ++length) {
        Require(ustr_hashCharsN_nid_postfix(reinterpret_cast<char*>(storage.data() + 32), length) == 0, "nonpositive byte count");
        Require(ustr_hashUCharsN_nid_postfix(storage.data() + 32, length) == 0, "nonpositive UTF16 count");
    }
    bool byteThrew = false, wideThrew = false;
    try { ustr_hashCharsN_nid_postfix(reinterpret_cast<char*>(storage.data()), std::numeric_limits<std::int32_t>::min()); } catch (const std::exception&) { byteThrew = true; }
    try { ustr_hashUCharsN_nid_postfix(storage.data(), std::numeric_limits<std::int32_t>::min()); } catch (const std::exception&) { wideThrew = true; }
    Require(byteThrew && wideThrew, "signed source length overflow must throw");
}

void CountedAndTerminated() {
    std::uint32_t random = 0x51e29b3;
    for (const auto length : {0, 1, 2, 31, 32, 33, 63, 64, 65, 95, 96, 97, 127, 128, 129, 511, 512, 1023, 1024, 4097, 65537, 131071}) {
        std::vector<unsigned char> bytes(length + 1);
        std::vector<char16_t> units(length + 1);
        for (int index = 0; index < length; ++index) {
            random = random * 1664525 + 1013904223;
            bytes[index] = static_cast<unsigned char>(1 + random % 255);
            units[index] = static_cast<char16_t>(1 + (random >> 8) % 65535);
        }
        const auto originalBytes = bytes;
        const auto originalUnits = units;
        const auto byteHash = Oracle(bytes.data(), length);
        const auto wideHash = Oracle(units.data(), length);
        Require(ustr_hashCharsN_nid_postfix(reinterpret_cast<const char*>(bytes.data()), length) == byteHash, "counted byte oracle");
        Require(ustr_hashUCharsN_nid_postfix(units.data(), length) == wideHash, "counted UTF16 oracle");
        Require(uhash_hashChars_nid_postfix(Pointer(bytes.data())) == byteHash, "terminated byte callback");
        Require(uhash_hashUChars_nid_postfix(Pointer(units.data())) == wideHash, "terminated UTF16 callback");
        Require(uhash_hashIChars_nid_postfix(Pointer(bytes.data())) == Oracle(bytes.data(), length, true), "ASCII fold hash callback");
        Require(bytes == originalBytes && units == originalUnits, "hash input preservation");
        if (length > 64) {
            const auto spacing = std::max(1, length / 32);
            bytes[spacing - 1] ^= 0x5a;
            Require(ustr_hashCharsN_nid_postfix(reinterpret_cast<const char*>(bytes.data()), length) == byteHash, "unsampled units cannot affect hash");
            bytes[0] ^= 1;
            Require(ustr_hashCharsN_nid_postfix(reinterpret_cast<const char*>(bytes.data()), length) != byteHash, "sampled unit affects hash");
        }
    }
    const std::array<unsigned char, 8> bytes{'A', 0, 0xff, 'Z', 0x80, 0xc0, 'a', 0};
    const std::array<char16_t, 8> units{u'A', 0, 0xd800, 0xdc00, 0xffff, 0x100, u'a', 0};
    Require(uhash_hashChars_nid_postfix(Pointer(bytes.data())) == 'A', "terminated byte NUL boundary");
    Require(uhash_hashUChars_nid_postfix(Pointer(units.data())) == u'A', "terminated UTF16 NUL boundary");
    Require(ustr_hashCharsN_nid_postfix(reinterpret_cast<const char*>(bytes.data()), 7) == Oracle(bytes.data(), 7), "counted embedded NUL and unsigned bytes");
    Require(ustr_hashUCharsN_nid_postfix(units.data(), 7) == Oracle(units.data(), 7), "counted surrogates and embedded NUL");
}

void Comparisons() {
    for (unsigned first = 0; first < 256; ++first) for (unsigned second = 0; second < 256; ++second) {
        const std::array<unsigned char, 3> left{static_cast<unsigned char>(first), 'Q', 0};
        const std::array<unsigned char, 3> right{static_cast<unsigned char>(second), 'q', 0};
        const auto a = Pointer(left.data()), b = Pointer(right.data());
        Require(uhash_compareIChars_nid_postfix(a, b) == (Fold(first) == Fold(second)), "all byte pairs ASCII-only equality");
        Require(uhash_compareChars_nid_postfix(a, b) == (first == 0 && second == 0), "case-sensitive tail equality");
        if (Fold(first) == Fold(second)) Require(uhash_hashIChars_nid_postfix(a) == uhash_hashIChars_nid_postfix(b), "equivalent keys must hash equally");
    }
    const std::array<char16_t, 8> left{u'a', 0xd800, 0xffff, 0, u'x', 0, 0, 0};
    auto right = left;
    right[4] = u'y';
    Require(uhash_compareUChars_nid_postfix(Pointer(left.data()), Pointer(right.data())) == 1, "UTF16 terminated comparison ignores suffix");
    right[2] = 0xfffe;
    Require(uhash_compareUChars_nid_postfix(Pointer(left.data()), Pointer(right.data())) == 0, "UTF16 full-unit inequality");
    right = left;
    right[0] = u'A';
    Require(uhash_compareUChars_nid_postfix(Pointer(left.data()), Pointer(right.data())) == 0, "UTF16 comparison is case-sensitive");
}

void IntegerTokens() {
    constexpr std::array<std::int32_t, 7> values{0, 1, -1, 37, 0x12345678, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()};
    for (const auto first : values) for (const auto second : values) {
        HashToken a{}, b{};
        a.integer = first;
        b.integer = second;
        const std::uint32_t poisonA = 0x12345678, poisonB = 0xfedcba98;
        std::memcpy(reinterpret_cast<unsigned char*>(&a) + 4, &poisonA, 4);
        std::memcpy(reinterpret_cast<unsigned char*>(&b) + 4, &poisonB, 4);
        Hasher hash = uhash_hashLong_nid_postfix;
        Equal equal = uhash_compareLong_nid_postfix;
        Require(hash(a) == first && hash(b) == second, "signed int32 identity hash ignores upper union bytes");
        Require(equal(a, b) == (first == second), "int32 equality ignores upper union bytes");
    }
}

} // namespace

int main() {
    NoReadContracts();
    CountedAndTerminated();
    Comparisons();
    IntegerTokens();
    return 0;
}
