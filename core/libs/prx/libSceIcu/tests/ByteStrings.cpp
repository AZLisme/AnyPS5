#include "prx/libSceIcu/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>



namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "ICU byte strings: %s\n", message);
    std::abort();
}

template<typename TCall>
void Unsupported(TCall call) {
    bool threw = false;
    try { call(); } catch (const std::runtime_error&) { threw = true; }
    Require(threw, "unestablished input did not throw");
}

template<typename TInteger>
void Number(TInteger number, unsigned radix) {
    using TUnsigned = std::make_unsigned_t<TInteger>;
    const bool negative = radix == 10 && number < 0;
    auto bits = static_cast<TUnsigned>(number);
    if (negative) bits = TUnsigned{0} - bits;
    std::array<char, 128> text{};
    const auto converted = std::to_chars(text.data(), text.data() + text.size(), bits, radix);
    Require(converted.ec == std::errc{}, "oracle formatting failed");
    std::string expected(negative ? "-" : "");
    expected.append(text.data(), converted.ptr);
    for (char& byte : expected) if (byte >= 'a' && byte <= 'f') byte -= 'a' - 'A';
    std::array<char, 96> output;
    output.fill('~');
    const auto original = output;
    const auto call = [&] {
        if constexpr (sizeof(TInteger) == 4) return T_CString_integerToString_nid_postfix(output.data() + 3, number, radix);
        else return T_CString_int64ToString_nid_postfix(output.data() + 3, number, radix);
    };
    if (static_cast<std::size_t>(converted.ptr - text.data()) > 29 || (negative && number == std::numeric_limits<TInteger>::min())) {
        Unsupported(call);
        Require(output == original, "unknown numeric input changed output");
    } else {
        const auto length = call();
        Require(length == static_cast<std::int32_t>(expected.size()), "formatted length");
        auto guarded = original;
        std::copy(expected.begin(), expected.end(), guarded.begin() + 3);
        guarded[3 + expected.size()] = 0;
        Require(output == guarded, "formatted value, termination or canary");
    }
}

void Numbers() {
    std::uint64_t random = 0x74112345678abcde;
    for (unsigned radix = 2; radix <= 16; ++radix) {
        for (const auto value : {std::int64_t{0}, std::int64_t{1}, std::int64_t{-1}, std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::int64_t>::max()}) Number(value, radix);
        for (const auto value : {std::int32_t{0}, std::int32_t{1}, std::int32_t{-1}, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) Number(value, radix);
        for (unsigned index = 0; index < 300; ++index) {
            random ^= random << 13;
            random ^= random >> 7;
            random ^= random << 17;
            Number(std::bit_cast<std::int64_t>(random), radix);
            Number(std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(random)), radix);
        }
    }
    for (const int radix : {std::numeric_limits<int>::min(), -1, 0, 1, 17, std::numeric_limits<int>::max()}) {
        std::array<char, 40> output;
        output.fill('~');
        const auto initial = output;
        Unsupported([&] { T_CString_integerToString_nid_postfix(output.data(), 1, radix); });
        Unsupported([&] { T_CString_int64ToString_nid_postfix(output.data(), 1, static_cast<std::uint32_t>(radix)); });
        Require(output == initial, "invalid radix changed output");
    }
    Unsupported([] { T_CString_integerToString_nid_postfix(nullptr, 1, 10); });
    Unsupported([] { T_CString_int64ToString_nid_postfix(nullptr, 1, 10); });
}

void Parsing() {
    struct Case { const char* text; int radix; std::int32_t expected; int error; };
    const Case cases[] = {
        {"", 10, 0, 73}, {"x", 10, 0, 73}, {" \t-1tail", 10, -1, 73},
        {"2147483648", 10, std::numeric_limits<std::int32_t>::min(), 73},
        {"4294967295", 10, -1, 73}, {"4294967296", 10, 0, 73},
        {"18446744073709551615", 10, -1, 73}, {"18446744073709551616", 10, -1, 34},
        {"-18446744073709551615", 10, 1, 73}, {"-18446744073709551616", 10, -1, 34},
        {"0b11", 0, 0, 73}, {"0x", 0, 0, 73}, {"0xFFFFFFFF", 0, -1, 73},
        {"-0x80000000", 0, std::numeric_limits<std::int32_t>::min(), 73},
        {"077", 0, 63, 73}, {"+77tail", 8, 63, 73}, {"z", 36, 35, 73},
        {"123", 1, 0, 22}, {"123", 37, 0, 22}, {"123", -2, 0, 22}
    };
    for (const auto& test : cases) {
        *__error_nid_postfix() = 73;
        Require(T_CString_stringToInteger_nid_postfix(test.text, test.radix) == test.expected, "LP64 parsing or narrowing");
        Require(*__error_nid_postfix() == test.error, "parsing errno");
    }
    Unsupported([] { T_CString_stringToInteger_nid_postfix(nullptr, 10); });
}

void CasingAndPaths() {
    Require(T_CString_toLowerCase_nid_postfix(nullptr) == nullptr && T_CString_toUpperCase_nid_postfix(nullptr) == nullptr, "null case mapping");
    Require(uprv_pathIsAbsolute_nid_postfix(nullptr) == 0, "null path");
    for (unsigned byte = 0; byte <= 255; ++byte) {
        std::array<char, 5> initial{static_cast<char>(byte), 0, 'G', 'H', 0};
        auto lower = initial, upper = initial;
        auto expectedLower = initial, expectedUpper = initial;
        if (byte >= 'A' && byte <= 'Z') expectedLower[0] += 'a' - 'A';
        if (byte >= 'a' && byte <= 'z') expectedUpper[0] -= 'a' - 'A';
        Require(T_CString_toLowerCase_nid_postfix(lower.data()) == lower.data() && lower == expectedLower, "invariant lowercase or guard");
        Require(T_CString_toUpperCase_nid_postfix(upper.data()) == upper.data() && upper == expectedUpper, "invariant uppercase or guard");
        Require(uprv_pathIsAbsolute_nid_postfix(initial.data()) == (byte == '/'), "guest POSIX absolute path");
        Require(initial[2] == 'G', "path query changed source");
    }
    for (const char* path : {"", ".", "../x", "relative/file", "C:/x", "\\x", "http://host/x", "/", "//host/x", "/a/../b"})
        Require(uprv_pathIsAbsolute_nid_postfix(path) == (*path == '/'), "host-independent path syntax");
}

unsigned Fold(unsigned byte) { return byte >= 'A' && byte <= 'Z' ? byte + 32 : byte; }

void Comparisons() {
    const auto* invalid = reinterpret_cast<const char*>(std::uintptr_t{1});
    Require(uprv_stricmp_nid_postfix(nullptr, nullptr) == 0, "null comparison");
    Require(uprv_stricmp_nid_postfix(nullptr, invalid) == -1 && uprv_stricmp_nid_postfix(invalid, nullptr) == 1, "null ordering reads other operand");
    Require(uprv_strnicmp_nid_postfix(invalid, invalid, 0) == 0, "zero count read nonnull operands");
    Require(uprv_strnicmp_nid_postfix(nullptr, invalid, 0) == -1 && uprv_strnicmp_nid_postfix(invalid, nullptr, 0) == 1, "null ordering before zero count");
    for (unsigned first = 0; first <= 255; ++first) {
        for (unsigned second = 0; second <= 255; ++second) {
            const char left[]{static_cast<char>(first), 0}, right[]{static_cast<char>(second), 0};
            const int expected = first == 0 ? (second == 0 ? 0 : -1) : second == 0 ? 1 : static_cast<int>(Fold(first)) - static_cast<int>(Fold(second));
            Require(uprv_stricmp_nid_postfix(left, right) == expected, "full-byte case comparison oracle");
            Require(uprv_strnicmp_nid_postfix(left, right, 1) == expected && uprv_strnicmp_nid_postfix(left, right, 2) == expected, "bounded comparison oracle");
        }
    }
    Require(uprv_stricmp_nid_postfix("AbCdEf", "aBcDeF") == 0, "whole-string folding");
    Require(uprv_strnicmp_nid_postfix("AbCdEX", "aBcDeY", 5) == 0 && uprv_strnicmp_nid_postfix("AbCdEX", "aBcDeY", 6) == -1, "comparison bound boundary");
    Require(uprv_strnicmp_nid_postfix("AbCdEf", "aBcDeF", std::numeric_limits<std::uint32_t>::max()) == 0, "large bound stops at NUL");
}

struct Allocator {
    std::vector<std::size_t> sizes;
    std::map<void*, std::size_t> blocks;
    bool fail = false;
};

void* APS5_VABI AllocateBytes(const void* context, std::size_t bytes) {
    auto& state = *const_cast<Allocator*>(static_cast<const Allocator*>(context));
    state.sizes.push_back(bytes);
    if (state.fail || bytes > std::numeric_limits<std::size_t>::max() - 16) return nullptr;
    auto* allocation = static_cast<unsigned char*>(std::malloc(bytes + 16));
    if (!allocation) return nullptr;
    std::memset(allocation, 0xb7, bytes + 16);
    void* result = allocation + 8;
    state.blocks.emplace(result, bytes);
    return result;
}
void APS5_VABI FreeBytes(const void* context, void* pointer) {
    if (!pointer) return;
    auto& state = *const_cast<Allocator*>(static_cast<const Allocator*>(context));
    const auto found = state.blocks.find(pointer);
    Require(found != state.blocks.end(), "allocator context or allocation ownership");
    auto* allocation = static_cast<unsigned char*>(pointer) - 8;
    for (std::size_t index = 0; index < 8; ++index)
        Require(allocation[index] == 0xb7 && allocation[8 + found->second + index] == 0xb7, "duplicate allocation canary");
    state.blocks.erase(found);
    std::free(allocation);
}
void* APS5_VABI ReallocateBytes(const void* context, void* pointer, std::size_t bytes) {
    if (!pointer) return AllocateBytes(context, bytes);
    auto& state = *const_cast<Allocator*>(static_cast<const Allocator*>(context));
    const auto found = state.blocks.find(pointer);
    Require(found != state.blocks.end(), "reallocation ownership");
    const auto oldSize = found->second;
    void* replacement = AllocateBytes(context, bytes);
    if (!replacement) return nullptr;
    std::memcpy(replacement, pointer, std::min(bytes, oldSize));
    FreeBytes(context, pointer);
    return replacement;
}

void Duplication() {
    char* defaultCopy = uprv_strdup_nid_postfix("default guest heap");
    Require(defaultCopy && std::strcmp(defaultCopy, "default guest heap") == 0, "default guest allocation");
    free_nid_postfix(defaultCopy);
    Allocator state;
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&state, AllocateBytes, ReallocateBytes, FreeBytes, &status);
    Require(status == 0, "memory callback setup");
    const std::array<char, 7> source{'a', 0, static_cast<char>(0xff), 'Z', 0, 'G', 'H'};
    for (const int count : {std::numeric_limits<std::int32_t>::min(), -2, -1, 0, 1, 2, 3, 4, 5, 7}) {
        const std::size_t copied = count < 0 ? 1 : static_cast<std::size_t>(count);
        char* result = uprv_strndup_nid_postfix(source.data(), count);
        Require(result && state.sizes.back() == copied + 1, "counted duplication allocation size");
        Require(std::memcmp(result, source.data(), copied) == 0 && result[copied] == 0, "counted duplication truncates at embedded NUL");
        result[0] = 'X';
        Require(source[0] == 'a', "duplicate aliases source");
        FreeBytes(&state, result);
    }
    char* zero = uprv_strndup_nid_postfix(reinterpret_cast<const char*>(std::uintptr_t{1}), 0);
    Require(zero && zero[0] == 0 && state.sizes.back() == 1, "zero count read source");
    FreeBytes(&state, zero);
    char* full = uprv_strdup_nid_postfix(source.data());
    Require(full && full[0] == 'a' && full[1] == 0 && state.sizes.back() == 2, "terminated duplication");
    FreeBytes(&state, full);
    state.fail = true;
    Require(uprv_strdup_nid_postfix(source.data()) == nullptr && uprv_strndup_nid_postfix(source.data(), 4) == nullptr, "allocation failure return");
    const auto allocations = state.sizes.size();
    Unsupported([] { uprv_strdup_nid_postfix(nullptr); });
    Unsupported([] { uprv_strndup_nid_postfix(nullptr, 0); });
    Unsupported([&] { uprv_strndup_nid_postfix(source.data(), std::numeric_limits<std::int32_t>::max()); });
    Require(state.sizes.size() == allocations, "unsupported duplicate called allocator");
    Require(state.blocks.empty(), "duplication leaked callback allocations");
}

} // namespace

int main() {
    const std::array<void*, 10> heap{};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    Numbers();
    Parsing();
    CasingAndPaths();
    Comparisons();
    Duplication();
}
