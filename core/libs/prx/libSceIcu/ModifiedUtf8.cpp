#include "prx/libSceIcu/libSceIcu.hpp"
#include "ConversionBuffers.hpp"
#include <array>

namespace {

bool Trail(std::uint8_t byte) { return byte >= 0x80 && byte <= 0xbf; }

void SkipMalformed(const char* source, std::int32_t length, std::int32_t& position, std::uint8_t lead) {
    if (position == length || lead < 0xc2 || lead > 0xf4) return;
    const auto first = static_cast<std::uint8_t>(source[position]);
    if (!Trail(first)) return;
    if ((lead == 0xe0 && first < 0xa0) || (lead == 0xed && first >= 0xa0) ||
        (lead == 0xf0 && first < 0x90) || (lead == 0xf4 && first >= 0x90)) return;
    ++position;
    const auto remaining = lead < 0xe0 ? 0 : lead < 0xf0 ? 1 : 2;
    for (int index = 0; index < remaining && position < length && Trail(static_cast<std::uint8_t>(source[position])); ++index) ++position;
}

char16_t* FromModified(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                      const char* source, std::int32_t sourceLength, std::int32_t substitution,
                      std::int32_t* substitutions, std::int32_t* error) {
    if (!ValidateConversion(destination, capacity, source, sourceLength, substitution, error)) return nullptr;
    if (substitutions) *substitutions = 0;
    const auto length = SourceLength(source, sourceLength);
    std::int32_t written = 0;
    std::int64_t missing = 0;
    std::int32_t replaced = 0;
    for (std::int32_t position = 0; position < length;) {
        const auto lead = static_cast<std::uint8_t>(source[position++]);
        std::int32_t value = lead;
        if (lead >= 0xc0 && lead <= 0xdf && position < length && Trail(static_cast<std::uint8_t>(source[position]))) {
            value = ((lead & 0x1f) << 6) | (source[position++] & 0x3f);
        } else if (lead >= 0xe0 && lead <= 0xef && length - position >= 2 &&
                   Trail(static_cast<std::uint8_t>(source[position])) && Trail(static_cast<std::uint8_t>(source[position + 1]))) {
            value = ((lead & 0x0f) << 12) | ((source[position] & 0x3f) << 6) | (source[position + 1] & 0x3f);
            position += 2;
        } else if (lead >= 0x80) {
            if (substitution < 0) { *error = InvalidCharacter; return nullptr; }
            SkipMalformed(source, length, position, lead);
            value = substitution;
            ++replaced;
        }
        if (written < capacity) {
            if (value <= 0xffff) destination[written++] = static_cast<char16_t>(value);
            else {
                destination[written++] = static_cast<char16_t>(0xd7c0 + (value >> 10));
                if (written < capacity) destination[written++] = static_cast<char16_t>(0xdc00 + (value & 0x3ff));
                else ++missing;
            }
        } else {
            ++missing;
        }
    }
    if (substitutions) *substitutions = replaced;
    FinishConversion(destination, capacity, written + missing, destinationLength, error);
    return destination;
}

} // namespace

extern "C" {

char16_t* APS5_VABI u_strFromJavaModifiedUTF8WithSub_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* length,
    const char* source, std::int32_t sourceLength, std::int32_t substitution, std::int32_t* substitutions, std::int32_t* error) {
    return FromModified(destination, capacity, length, source, sourceLength, substitution, substitutions, error);
}

char* APS5_VABI u_strToJavaModifiedUTF8_nid_postfix(char* destination, std::int32_t capacity, std::int32_t* length,
    const char16_t* source, std::int32_t sourceLength, std::int32_t* error) {
    if (!ValidateConversion(destination, capacity, source, sourceLength, -1, error)) return nullptr;
    const auto sourceUnits = SourceLength(source, sourceLength);
    std::int64_t required = 0;
    bool writing = true;
    for (std::int32_t position = 0; position < sourceUnits; ++position) {
        const auto value = static_cast<std::uint32_t>(source[position]);
        std::array<std::uint8_t, 3> encoded{};
        const unsigned units = value > 0 && value <= 0x7f ? 1 : value <= 0x7ff ? 2 : 3;
        if (units == 1) encoded[0] = static_cast<std::uint8_t>(value);
        else if (units == 2) {
            encoded[0] = static_cast<std::uint8_t>(0xc0 | (value >> 6));
            encoded[1] = static_cast<std::uint8_t>(0x80 | (value & 0x3f));
        } else {
            encoded[0] = static_cast<std::uint8_t>(0xe0 | (value >> 12));
            encoded[1] = static_cast<std::uint8_t>(0x80 | ((value >> 6) & 0x3f));
            encoded[2] = static_cast<std::uint8_t>(0x80 | (value & 0x3f));
        }
        if (writing && required + units <= capacity) {
            for (unsigned unit = 0; unit < units; ++unit) destination[required + unit] = static_cast<char>(encoded[unit]);
        } else writing = false;
        required += units;
    }
    FinishConversion(destination, capacity, required, length, error);
    return destination;
}

}
