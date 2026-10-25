#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>



static void Require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "ICU UTF32: %s\n", message); std::abort(); }
}

int main() {
    constexpr std::int32_t mark = 12345;
    constexpr std::array<std::int32_t, 5> source{0x41, 0, 0xffff, 0x10000, 0x10ffff};
    constexpr std::array<char16_t, 8> expected{0x41, 0, 0xffff, 0xd800, 0xdc00, 0xdbff, 0xdfff, 0};
    std::array<char16_t, 12> output;
    output.fill(0x5555);
    std::int32_t length = mark, substitutions = mark, error = 0;
    Require(u_strFromUTF32WithSub_nid_postfix(output.data(), output.size(), &length, source.data(), source.size(), 0xfffd, &substitutions, &error) == output.data(), "from return pointer");
    Require(error == 0 && length == 7 && substitutions == 0 && std::equal(expected.begin(), expected.end(), output.begin()) && output[8] == 0x5555, "explicit source with embedded zero and Unicode boundaries");
    std::array<std::int32_t, 8> restored;
    restored.fill(mark);
    Require(u_strToUTF32WithSub_nid_postfix(restored.data(), restored.size(), &length, output.data(), 7, 0xfffd, &substitutions, &error) == restored.data(), "to return pointer");
    Require(error == 0 && length == 5 && substitutions == 0 && std::equal(source.begin(), source.end(), restored.begin()) && restored[5] == 0 && restored[6] == mark, "Unicode boundary roundtrip");
    output.fill(0x5555);
    u_strFromUTF32_nid_postfix(output.data(), output.size(), &length, source.data(), -1, &error);
    Require(error == 0 && length == 1 && output[0] == 0x41 && output[1] == 0 && output[2] == 0x5555, "NUL-terminated source length");
    error = 0;
    Require(u_strFromUTF32_nid_postfix(nullptr, 0, &length, source.data(), source.size(), &error) == nullptr && length == 7 && error == 15, "from preflight");
    const std::array<std::int32_t, 2> skipped{0x10000, 0x41};
    output.fill(0x5555); error = 0;
    u_strFromUTF32_nid_postfix(output.data(), 1, &length, skipped.data(), skipped.size(), &error);
    Require(error == 15 && length == 3 && output[0] == 0x41 && output[1] == 0x5555, "whole-surrogate skip retains later BMP write");
    const std::array<std::int32_t, 3> invalid{0xd800, -1, 0x110000};
    for (const auto substitution : {0xfffd, 0x10000}) {
        output.fill(0x5555); error = 0;
        u_strFromUTF32WithSub_nid_postfix(output.data(), output.size(), &length, invalid.data(), invalid.size(), substitution, &substitutions, &error);
        Require(error == 0 && length == (substitution == 0xfffd ? 3 : 6) && substitutions == 3 && output[length] == 0 && output[length + 1] == 0x5555, "invalid scalar substitutions");
    }
    const std::array<std::int32_t, 3> strict{0x41, 0xd800, 0x42};
    output.fill(0x5555); error = 0; length = substitutions = mark;
    Require(u_strFromUTF32WithSub_nid_postfix(output.data(), output.size(), &length, strict.data(), strict.size(), -1, &substitutions, &error) == nullptr, "strict failure return");
    Require(error == 10 && length == mark && substitutions == 0 && output[0] == 0x41 && output[1] == 0x5555, "strict failure preserves lengths and unterminated prefix");
    const std::array<char16_t, 5> unpaired{0xd800, 0x41, 0xdc00, 0xdbff, 0xdfff};
    restored.fill(mark); error = 0;
    u_strToUTF32WithSub_nid_postfix(restored.data(), restored.size(), &length, unpaired.data(), unpaired.size(), 0xfffd, &substitutions, &error);
    Require(error == 0 && length == 4 && substitutions == 2 && restored[0] == 0xfffd && restored[1] == 0x41 && restored[2] == 0xfffd && restored[3] == 0x10ffff && restored[4] == 0 && restored[5] == mark, "unpaired UTF16 replacement and valid pair");
    for (const auto warning : {-124, -128}) {
        error = warning;
        u_strFromUTF32_nid_postfix(output.data(), output.size(), &length, source.data(), 1, &error);
        Require(error == (warning == -124 ? 0 : warning), "incoming warning termination policy");
    }
    error = 0; length = substitutions = mark; output.fill(0x5555);
    Require(u_strFromUTF32WithSub_nid_postfix(output.data(), output.size(), &length, source.data(), 1, 0xd800, &substitutions, &error) == nullptr, "invalid substitution return");
    Require(error == 1 && length == mark && substitutions == mark && output[0] == 0x5555, "argument validation precedes output initialization");
    error = 10; length = mark;
    Require(u_strToUTF32_nid_postfix(nullptr, -1, &length, nullptr, -2, &error) == nullptr && error == 10 && length == mark, "incoming failure bypasses argument validation");
    error = 0;
    u_strFromUTF32WithSub_nid_postfix(output.data(), output.size(), nullptr, source.data(), 1, 0xfffd, nullptr, &error);
    Require(error == 0 && output[0] == 0x41 && output[1] == 0, "optional length and substitution outputs");
    for (const bool sharedStatus : {false, true}) {
        std::int32_t shared = 0;
        error = 0; output.fill(0x5555);
        u_strFromUTF32WithSub_nid_postfix(output.data(), output.size(), &shared, source.data(), 1, 0xfffd, &shared, sharedStatus ? &shared : &error);
        Require(shared == 0 && error == 0 && output[0] == 0x41 && output[1] == 0 && output[2] == 0x5555, "from shared length/substitution/status ordering");
        const std::array<char16_t, 1> one{0x41};
        restored.fill(mark); shared = error = 0;
        u_strToUTF32WithSub_nid_postfix(restored.data(), restored.size(), &shared, one.data(), one.size(), 0xfffd, &shared, sharedStatus ? &shared : &error);
        Require(shared == 0 && error == 0 && restored[0] == 0x41 && restored[1] == 0 && restored[2] == mark, "to shared length/substitution/status ordering");
    }
    bool threw = false;
    try { u_strFromUTF32_nid_postfix(nullptr, 0, nullptr, nullptr, 0, nullptr); }
    catch (const std::runtime_error&) { threw = true; }
    Require(threw, "undefined null error pointer must throw");
}
