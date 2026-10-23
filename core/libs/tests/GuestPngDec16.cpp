#include "prx/libScePngDec/libScePngDec.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>



static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "PNG 16-bit input check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void Put32(std::vector<std::uint8_t>& bytes, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

static void Chunk(std::vector<std::uint8_t>& png, const char* type, const std::vector<std::uint8_t>& bytes) {
    Put32(png, static_cast<std::uint32_t>(bytes.size()));
    const auto start = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), bytes.begin(), bytes.end());
    std::uint32_t crc = 0xFFFFFFFFu;
    for (auto index = start; index < png.size(); ++index) {
        crc ^= png[index];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320u : 0);
    }
    Put32(png, ~crc);
}

static std::uint16_t Sample(unsigned pixel, unsigned channel) {
    return static_cast<std::uint16_t>((0x12 + pixel * 29 + channel * 17) << 8 | (0xFE - pixel * 3 - channel));
}

static std::vector<std::uint8_t> Png(unsigned colorType, unsigned channels, bool transparent) {
    std::vector<std::uint8_t> png{137, 80, 78, 71, 13, 10, 26, 10};
    std::vector<std::uint8_t> header;
    Put32(header, 3);
    Put32(header, 2);
    header.insert(header.end(), {16, static_cast<std::uint8_t>(colorType), 0, 0, 0});
    Chunk(png, "IHDR", header);
    if (transparent) {
        std::vector<std::uint8_t> key;
        for (unsigned channel = 0; channel < channels; ++channel) {
            const auto sample = Sample(0, channel);
            key.push_back(static_cast<std::uint8_t>(sample >> 8));
            key.push_back(static_cast<std::uint8_t>(sample));
        }
        Chunk(png, "tRNS", key);
    }
    std::vector<std::uint8_t> rows;
    for (unsigned row = 0; row < 2; ++row) {
        rows.push_back(0);
        for (unsigned column = 0; column < 3; ++column) {
            for (unsigned channel = 0; channel < channels; ++channel) {
                const auto sample = Sample(row * 3 + column, channel);
                rows.push_back(static_cast<std::uint8_t>(sample >> 8));
                rows.push_back(static_cast<std::uint8_t>(sample));
            }
        }
    }
    const auto length = static_cast<std::uint16_t>(rows.size());
    const auto complement = static_cast<std::uint16_t>(~length);
    std::vector<std::uint8_t> compressed{0x78, 0x01, 0x01, static_cast<std::uint8_t>(length), static_cast<std::uint8_t>(length >> 8),
        static_cast<std::uint8_t>(complement), static_cast<std::uint8_t>(complement >> 8)};
    for (const auto byte : rows) compressed.push_back(byte);
    std::uint32_t first = 1;
    std::uint32_t second = 0;
    for (const auto byte : rows) {
        first = (first + byte) % 65521;
        second = (second + first) % 65521;
    }
    Put32(compressed, second << 16 | first);
    Chunk(png, "IDAT", compressed);
    Chunk(png, "IEND", {});
    return png;
}

int main() {
    constexpr auto invalidWork = static_cast<std::int32_t>(0x80690005);
    constexpr auto invalidParam = static_cast<std::int32_t>(0x80690003);
    std::array<void*, 2> handles{};
    std::array<std::vector<std::uint8_t>, 2> memory;
    for (unsigned attribute = 0; attribute < 2; ++attribute) {
        const PngDecCreateParam create{sizeof(PngDecCreateParam), attribute, 8};
        memory[attribute].resize(scePngDecQueryMemorySize(&create));
        Require(scePngDecCreate(&create, memory[attribute].data(), memory[attribute].size(), &handles[attribute]) == 0);
    }
    for (const auto colorType : {0u, 2u, 4u, 6u}) {
        const unsigned channels = colorType == 0 ? 1 : colorType == 2 ? 3 : colorType == 4 ? 2 : 4;
        for (const bool transparent : {false, true}) {
            if (transparent && colorType >= 4) continue;
            const auto png = Png(colorType, channels, transparent);
            for (const auto pixelFormat : {0u, 1u}) {
                for (const auto pitch : {0u, 16u}) {
                    std::vector<std::uint8_t> output(40, 0xCD);
                    PngDecDecodeParam decode{};
                    decode.png_mem_addr = png.data();
                    decode.png_mem_size = png.size();
                    decode.image_mem_addr = output.data();
                    decode.image_mem_size = output.size();
                    decode.pixel_format = pixelFormat;
                    decode.alpha_value = 0xA5;
                    decode.image_pitch = pitch;
                    PngDecImageInfo info{};
                    Require(scePngDecDecode(handles[0], &decode, &info) == invalidWork);
                    Require(info.bit_depth == 16 && info.image_width == 3 && info.image_height == 2);
                    Require(std::all_of(output.begin(), output.end(), [](auto byte) { return byte == 0xCD; }));
                    Require(scePngDecDecode(handles[1], &decode, &info) == (3 << 16 | 2));
                    Require(info.image_flag == (transparent ? 2u : 0u));
                    const unsigned stride = pitch ? pitch : 12;
                    std::vector<std::uint8_t> expected(40, 0xCD);
                    for (unsigned pixel = 0; pixel < 6; ++pixel) {
                        const unsigned offset = pixel / 3 * stride + pixel % 3 * 4;
                        const bool gray = colorType == 0 || colorType == 4;
                        expected[offset] = Sample(pixel, pixelFormat && !gray ? 2 : 0) >> 8;
                        expected[offset + 1] = Sample(pixel, gray ? 0 : 1) >> 8;
                        expected[offset + 2] = Sample(pixel, !pixelFormat && !gray ? 2 : 0) >> 8;
                        expected[offset + 3] = colorType >= 4 ? Sample(pixel, channels - 1) >> 8 : transparent ? (pixel == 0 ? 0 : 255) : 0xA5;
                    }
                    Require(output == expected);
                    decode.image_pitch = 8;
                    Require(scePngDecDecode(handles[0], &decode, &info) == invalidParam);
                }
            }
        }
    }
    for (auto handle : handles) Require(scePngDecDelete(handle) == 0);
}
