#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {
std::int32_t APS5_VABI utf8_appendCharSafeBody_nid_postfix(std::uint8_t*, std::int32_t, std::int32_t, std::int32_t, std::int8_t*);
std::int32_t APS5_VABI utf8_nextCharSafeBody_nid_postfix(const std::uint8_t*, std::int32_t*, std::int32_t, std::int32_t, std::int8_t);
std::int32_t APS5_VABI utf8_prevCharSafeBody_nid_postfix(const std::uint8_t*, std::int32_t, std::int32_t*, std::int32_t, std::int8_t);
std::int32_t APS5_VABI utf8_back1SafeBody_nid_postfix(const std::uint8_t*, std::int32_t, std::int32_t);
}

namespace {

void Require(bool value) {
    if (!value) throw std::runtime_error("ICU UTF-8 macro-body regression");
}

void Scalars() {
    for (std::int32_t value = 0x80; value <= 0x10ffff; ++value) {
        if (value >= 0xd800 && value <= 0xdfff) continue;
        std::array<std::uint8_t, 8> bytes;
        bytes.fill(0xa5);
        std::int8_t error = 7;
        const auto end = utf8_appendCharSafeBody_nid_postfix(bytes.data(), 1, 7, value, &error);
        Require(end == (value < 0x800 ? 3 : value < 0x10000 ? 4 : 5) && error == 7 && bytes[0] == 0xa5 && bytes[end] == 0xa5);
        auto index = 2;
        Require(utf8_nextCharSafeBody_nid_postfix(bytes.data(), &index, end, bytes[1], -1) == value && index == end);
        index = end - 1;
        Require(utf8_prevCharSafeBody_nid_postfix(bytes.data(), 1, &index, bytes[end - 1], -1) == value && index == 1);
        Require(utf8_back1SafeBody_nid_postfix(bytes.data(), 1, end - 1) == 1);
    }
}

void Malformed() {
    const std::array<std::vector<std::uint8_t>, 8> inputs{{{0xc2}, {0xe2, 0x82}, {0xf0, 0x90, 0x80}, {0xe0, 0x80, 0x80}, {0xed, 0xa0, 0x80}, {0xf4, 0x90, 0x80, 0x80}, {0xff, 0x80}, {0xe2, 0x82, 0x41}}};
    constexpr std::array<std::int32_t, 8> consumed{0, 1, 2, 0, 0, 0, 0, 1};
    for (std::size_t n = 0; n < inputs.size(); ++n) {
        const auto& input = inputs[n];
        for (const std::int8_t strict : {-128, -3, -2, -1, 0, 1, 127}) {
            auto position = 1;
            const auto value = utf8_nextCharSafeBody_nid_postfix(input.data(), &position, input.size(), input[0], strict);
            if (n == 4 && strict == -2) Require(value == 0xd800 && position == 3);
            else Require(position == 1 + consumed[n] && value == (strict >= 0 ? std::array<std::int32_t, 4>{0x15, 0x9f, 0xffff, 0x10ffff}[consumed[n]] : strict == -3 ? 0xfffd : -1));
        }
    }
    for (const auto value : {0xfdd0, 0xfdef, 0xfffe, 0xffff, 0x1fffe, 0x10ffff}) {
        std::array<std::uint8_t, 8> bytes{};
        const auto end = utf8_appendCharSafeBody_nid_postfix(bytes.data(), 0, bytes.size(), value, nullptr);
        auto position = 1;
        const auto replacement = end == 3 ? 0xffff : 0x10ffff;
        Require(utf8_nextCharSafeBody_nid_postfix(bytes.data(), &position, end, bytes[0], 1) == replacement && position == end);
        position = end - 1;
        Require(utf8_prevCharSafeBody_nid_postfix(bytes.data(), 0, &position, bytes[end - 1], 1) == replacement && position == 0);
    }
    for (const auto& input : inputs) {
        auto terminated = input;
        terminated.push_back(0);
        auto position = 1;
        const auto bounded = utf8_nextCharSafeBody_nid_postfix(input.data(), &position, input.size(), input[0], -1);
        const auto consumedBytes = position;
        position = 1;
        Require(utf8_nextCharSafeBody_nid_postfix(terminated.data(), &position, -1, input[0], -1) == bounded && position == consumedBytes);
    }
}

void AppendFailures() {
    for (const auto value : {-1, 0, 0x7f, 0x80, 0x7ff, 0x800, 0xd800, 0xdfff, 0x10000, 0x10ffff, 0x110000}) {
        for (std::int32_t capacity = 0; capacity <= 5; ++capacity) {
            std::array<std::uint8_t, 8> bytes;
            bytes.fill(0xa5);
            const auto width = value >= 0 && value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff) ? value <= 0x7ff ? 2 : value <= 0xffff ? 3 : 4 : 0;
            std::int8_t error = 0;
            const auto end = utf8_appendCharSafeBody_nid_postfix(bytes.data(), 1, 1 + capacity, value, &error);
            if (width == 0 || capacity < width) Require(end == 1 && error == 1 && std::all_of(bytes.begin(), bytes.end(), [](auto b) { return b == 0xa5; }));
            else Require(end == 1 + width && error == 0 && bytes[0] == 0xa5 && bytes[end] == 0xa5);
            bytes.fill(0xa5);
            const auto replacementEnd = utf8_appendCharSafeBody_nid_postfix(bytes.data(), 1, 1 + capacity, value, nullptr);
            if (width == 0 || capacity < width) {
                const auto count = std::min(capacity, 3);
                Require(replacementEnd == 1 + count && bytes[0] == 0xa5 && bytes[replacementEnd] == 0xa5);
                if (count == 1) Require(bytes[1] == 0x15);
                if (count == 2) Require(bytes[1] == 0xc2 && bytes[2] == 0x9f);
                if (count == 3) Require(bytes[1] == 0xef && bytes[2] == 0xbf && bytes[3] == 0xbf);
            }
        }
    }
    std::array<std::uint8_t, 2> ascii{};
    Require(utf8_appendCharSafeBody_nid_postfix(ascii.data(), 0, 2, 0, nullptr) == 2 && ascii[0] == 0xc0 && ascii[1] == 0x80);
}

void NoReadCases() {
    std::int32_t position = 0;
    Require(utf8_nextCharSafeBody_nid_postfix(nullptr, &position, 0, 0xe2, -1) == -1 && position == 0);
    Require(utf8_nextCharSafeBody_nid_postfix(nullptr, &position, 5, 0xff, -3) == 0xfffd && position == 0);
    Require(utf8_prevCharSafeBody_nid_postfix(nullptr, 0, &position, 0x80, -1) == -1 && position == 0);
    position = 5;
    Require(utf8_prevCharSafeBody_nid_postfix(nullptr, 0, &position, 0x41, 0) == 0x15 && position == 5);
    std::int8_t error = 0;
    Require(utf8_appendCharSafeBody_nid_postfix(nullptr, 0, 1, 0x800, &error) == 0 && error == 1);
    Require(utf8_appendCharSafeBody_nid_postfix(nullptr, 0, 0, 0x800, nullptr) == 0);
    const std::array<std::uint8_t, 2> bytes{0xc2, 0x80};
    for (const auto last : {-128, 0x180, 0x7fffffbf}) {
        position = 1;
        Require(utf8_prevCharSafeBody_nid_postfix(bytes.data(), 0, &position, last, -1) == (0x80 | (last & 0x3f)) && position == 0);
    }
}

} // namespace

int main() {
    Scalars();
    Malformed();
    AppendFailures();
    NoReadCases();
}
