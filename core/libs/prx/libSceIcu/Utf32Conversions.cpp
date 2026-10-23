#include "ConversionBuffers.hpp"

namespace {

template<typename TDestination>
void FinishUtf32(TDestination* destination, std::int32_t capacity, std::int64_t required,
                 std::int32_t* length, std::int32_t count, std::int32_t* substitutions, std::int32_t* error) {
    if (required > std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
    if (length) *length = static_cast<std::int32_t>(required);
    if (substitutions) *substitutions = count;
    Terminate(destination, capacity, static_cast<std::int32_t>(required), error);
}

char16_t* FromUtf32(char16_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                    const std::int32_t* source, std::int32_t sourceLength, std::int32_t substitution,
                    std::int32_t* substitutions, std::int32_t* error) {
    if (!ValidateConversion(destination, capacity, source, sourceLength, substitution, error)) return nullptr;
    if (substitutions) *substitutions = 0;
    const auto length = SourceLength(source, sourceLength);
    std::int32_t written = 0;
    std::int64_t missing = 0;
    std::int32_t replaced = 0;
    for (std::int32_t position = 0; position < length; ++position) {
        auto value = source[position];
        if (value < 0 || value > 0x10ffff || IsSurrogate(value)) {
            if (substitution < 0) { *error = InvalidCharacter; return nullptr; }
            value = substitution;
            ++replaced;
        }
        if (value < 0x10000) {
            if (written < capacity) destination[written++] = static_cast<char16_t>(value);
            else ++missing;
        } else if (capacity - written >= 2) {
            destination[written++] = static_cast<char16_t>(0xd7c0 + (value >> 10));
            destination[written++] = static_cast<char16_t>(0xdc00 + (value & 0x3ff));
        } else {
            missing += 2;
        }
    }
    FinishUtf32(destination, capacity, written + missing, destinationLength, replaced, substitutions, error);
    return destination;
}

std::int32_t* ToUtf32(std::int32_t* destination, std::int32_t capacity, std::int32_t* destinationLength,
                      const char16_t* source, std::int32_t sourceLength, std::int32_t substitution,
                      std::int32_t* substitutions, std::int32_t* error) {
    if (!ValidateConversion(destination, capacity, source, sourceLength, substitution, error)) return nullptr;
    if (substitutions) *substitutions = 0;
    const auto length = SourceLength(source, sourceLength);
    std::int32_t count = 0;
    std::int32_t replaced = 0;
    for (std::int32_t position = 0; position < length;) {
        std::int32_t value = source[position++];
        if (IsSurrogate(value)) {
            if (value < 0xdc00 && position < length && source[position] >= 0xdc00 && source[position] <= 0xdfff) {
                value = 0x10000 + ((value - 0xd800) << 10) + source[position++] - 0xdc00;
            } else {
                if (substitution < 0) { *error = InvalidCharacter; return nullptr; }
                value = substitution;
                ++replaced;
            }
        }
        if (count < capacity) destination[count] = value;
        ++count;
    }
    FinishUtf32(destination, capacity, count, destinationLength, replaced, substitutions, error);
    return destination;
}

} // namespace

extern "C" {

char16_t* APS5_VABI u_strFromUTF32_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* length,
    const std::int32_t* source, std::int32_t sourceLength, std::int32_t* error) {
    return FromUtf32(destination, capacity, length, source, sourceLength, -1, nullptr, error);
}

char16_t* APS5_VABI u_strFromUTF32WithSub_nid_postfix(char16_t* destination, std::int32_t capacity, std::int32_t* length,
    const std::int32_t* source, std::int32_t sourceLength, std::int32_t substitution, std::int32_t* substitutions, std::int32_t* error) {
    return FromUtf32(destination, capacity, length, source, sourceLength, substitution, substitutions, error);
}

std::int32_t* APS5_VABI u_strToUTF32_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t* length,
    const char16_t* source, std::int32_t sourceLength, std::int32_t* error) {
    return ToUtf32(destination, capacity, length, source, sourceLength, -1, nullptr, error);
}

std::int32_t* APS5_VABI u_strToUTF32WithSub_nid_postfix(std::int32_t* destination, std::int32_t capacity, std::int32_t* length,
    const char16_t* source, std::int32_t sourceLength, std::int32_t substitution, std::int32_t* substitutions, std::int32_t* error) {
    return ToUtf32(destination, capacity, length, source, sourceLength, substitution, substitutions, error);
}

}
