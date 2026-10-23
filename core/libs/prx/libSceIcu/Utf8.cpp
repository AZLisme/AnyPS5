#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include "prx/libc/include/General.hpp"

namespace {

bool Trail(std::uint32_t value) { return (value & 0xc0) == 0x80; }

unsigned Width(std::uint32_t lead) {
    if (lead >= 0xc2 && lead <= 0xdf) return 2;
    if (lead >= 0xe0 && lead <= 0xef) return 3;
    if (lead >= 0xf0 && lead <= 0xf4) return 4;
    return 0;
}

bool FirstTrail(std::uint32_t lead, std::uint32_t trail, bool surrogate) {
    if (!Trail(trail)) return false;
    if (lead == 0xe0) return trail >= 0xa0;
    if (lead == 0xed && !surrogate) return trail < 0xa0;
    if (lead == 0xf0) return trail >= 0x90;
    if (lead == 0xf4) return trail < 0x90;
    return true;
}

std::int32_t Error(unsigned consumed, std::int8_t strict) {
    if (strict >= 0) return std::array<std::int32_t, 4>{0x15, 0x9f, 0xffff, 0x10ffff}[consumed];
    return strict == -3 ? 0xfffd : -1;
}

bool Noncharacter(std::uint32_t value) {
    return (value >= 0xfdd0 && value <= 0xfdef) || ((value & 0xfffe) == 0xfffe && value <= 0x10ffff);
}

std::int32_t Previous(const std::uint8_t* text, std::int32_t start, std::int32_t& index, std::int32_t last, std::int8_t strict) {
    if (!Trail(static_cast<std::uint32_t>(last)) || index <= start) return Error(0, strict);
    if (!text || start < 0 || index < 0) NotImplemented_nid_no_patch(__func__);
    const auto original = index;
    auto position = index;
    for (unsigned trails = 1; trails <= 3 && position > start; ++trails) {
        const auto lead = text[--position];
        const auto width = Width(lead);
        if (width != 0) {
            const auto first = trails == 1 ? static_cast<std::uint8_t>(last) : text[position + 1];
            const bool lenient = strict == -2 && width == 3 && trails == 2;
            if (trails >= width || !FirstTrail(lead, first, lenient)) return Error(0, strict);
            index = position;
            if (trails + 1 != width) return Error(trails, strict);
            std::uint32_t value = lead & ((1u << (7 - width)) - 1);
            for (auto i = position + 1; i < original; ++i) value = (value << 6) | (text[i] & 0x3f);
            value = (value << 6) | (last & 0x3f);
            return strict > 0 && Noncharacter(value) ? Error(trails, strict) : static_cast<std::int32_t>(value);
        }
        if (!Trail(lead)) break;
    }
    return Error(0, strict);
}

} // namespace

extern "C" {

std::int32_t APS5_VABI utf8_nextCharSafeBody_nid_postfix(const std::uint8_t* text, std::int32_t* index, std::int32_t length, std::int32_t lead, std::int8_t strict) {
    if (!index) NotImplemented_nid_no_patch(__func__);
    const auto begin = *index;
    const auto width = Width(static_cast<std::uint32_t>(lead));
    if (begin == length || width == 0) return Error(0, strict);
    if (!text || begin < 0 || (length >= 0 && begin > length)) NotImplemented_nid_no_patch(__func__);
    auto position = begin;
    auto value = static_cast<std::uint32_t>(lead) & ((1u << (7 - width)) - 1);
    for (unsigned part = 1; part < width; ++part) {
        if (position == length) break;
        const auto byte = text[position];
        if (!(part == 1 ? FirstTrail(lead, byte, strict == -2) : Trail(byte))) break;
        value = (value << 6) | (byte & 0x3f);
        if (position == std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
        ++position;
        if (part + 1 == width) {
            *index = position;
            return strict > 0 && Noncharacter(value) ? Error(position - begin, strict) : static_cast<std::int32_t>(value);
        }
    }
    *index = position;
    return Error(position - begin, strict);
}

std::int32_t APS5_VABI utf8_prevCharSafeBody_nid_postfix(const std::uint8_t* text, std::int32_t start, std::int32_t* index, std::int32_t last, std::int8_t strict) {
    if (!index) NotImplemented_nid_no_patch(__func__);
    return Previous(text, start, *index, last, strict);
}

std::int32_t APS5_VABI utf8_back1SafeBody_nid_postfix(const std::uint8_t* text, std::int32_t start, std::int32_t index) {
    if (!text || index < 0) NotImplemented_nid_no_patch(__func__);
    auto result = index;
    Previous(text, start, result, text[index], -1);
    return result;
}

std::int32_t APS5_VABI utf8_appendCharSafeBody_nid_postfix(std::uint8_t* text, std::int32_t index, std::int32_t length, std::int32_t scalar, std::int8_t* error) {
    const auto value = static_cast<std::uint32_t>(scalar);
    unsigned width = value <= 0x7ff ? 2 : value <= 0xffff ? 3 : value <= 0x10ffff ? 4 : 0;
    if (value >= 0xd800 && value <= 0xdfff) width = 0;
    const auto available = static_cast<std::int64_t>(length) - index;
    if (width == 0 || available < width) {
        if (error) *error = 1;
        else if (available > 0) {
            width = static_cast<unsigned>(std::min<std::int64_t>(available, 3));
            const auto replacement = std::array<std::uint32_t, 3>{0x15, 0x9f, 0xffff}[width - 1];
            if (!text || index < 0 || static_cast<std::int64_t>(index) + width > std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
            if (width == 1) text[index++] = replacement;
            else {
                text[index++] = (width == 2 ? 0xc0 : 0xe0) | (replacement >> (6 * (width - 1)));
                for (unsigned remaining = width - 1; remaining != 0; --remaining) text[index++] = 0x80 | ((replacement >> (6 * (remaining - 1))) & 0x3f);
            }
        }
        return index;
    }
    if (!text || index < 0 || static_cast<std::int64_t>(index) + width > std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
    text[index++] = (width == 2 ? 0xc0 : width == 3 ? 0xe0 : 0xf0) | (value >> (6 * (width - 1)));
    for (unsigned remaining = width - 1; remaining != 0; --remaining) text[index++] = 0x80 | ((value >> (6 * (remaining - 1))) & 0x3f);
    return index;
}

}
