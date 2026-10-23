#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

struct String { void* text; };

extern "C" {
void APS5_VABI _ZN3sce4Json6StringC1Ev(String*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
String* APS5_VABI _ZN3sce4Json6StringpLEh(String*, unsigned char);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
std::size_t APS5_VABI _ZNK3sce4Json6String4sizeEv(const String*);
char APS5_VABI _ZNK3sce4Json6String2atEm(const String*, std::size_t);
}

static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "Json character access: %s\n", message); std::abort(); }
}

static void Unsupported(const String* string, std::size_t position) {
    bool threw = false;
    try { _ZNK3sce4Json6String2atEm(string, position); }
    catch (const std::runtime_error&) { threw = true; }
    Require(threw, "unknown invalid access must throw");
}

int main() {
    constexpr std::uint64_t canary = 0x123456789abcdef0;
    struct Guarded { String string{}; std::uint64_t guard = canary; } guarded;
    _ZN3sce4Json6StringC1Ev(&guarded.string);
    std::string expected;
    const auto* implementation = guarded.string.text;
    for (unsigned size = 0; size <= 256; ++size) {
        if (size != 0) {
            const auto byte = static_cast<unsigned char>((size - 1) * 197u + 101u);
            _ZN3sce4Json6StringpLEh(&guarded.string, byte);
            expected.push_back(static_cast<char>(byte));
        }
        Require(_ZNK3sce4Json6String4sizeEv(&guarded.string) == expected.size(), "source byte length");
        const auto* bytes = _ZNK3sce4Json6String5c_strEv(&guarded.string);
        for (std::size_t position = 0; position < expected.size(); ++position) {
            Require(static_cast<unsigned char>(_ZNK3sce4Json6String2atEm(&guarded.string, position)) == static_cast<unsigned char>(expected[position]), "byte oracle differs");
        }
        Unsupported(&guarded.string, expected.size());
        Unsupported(&guarded.string, expected.size() + 1);
        Unsupported(&guarded.string, std::numeric_limits<std::size_t>::max());
        Require(guarded.string.text == implementation && guarded.guard == canary, "const access changed object or canary");
        Require(_ZNK3sce4Json6String5c_strEv(&guarded.string) == bytes, "const access changed source buffer");
        Require(std::string(bytes, expected.size()) == expected && bytes[expected.size()] == 0, "const access changed bytes or terminator");
    }
    Unsupported(nullptr, 0);
    String unconstructed{};
    Unsupported(&unconstructed, 0);
    _ZN3sce4Json6StringD1Ev(&guarded.string);
}
