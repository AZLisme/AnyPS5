#ifndef CORE_LIBS_PRX_LIBSCEICU_CONVERSIONBUFFERS_HPP
#define CORE_LIBS_PRX_LIBSCEICU_CONVERSIONBUFFERS_HPP

#include "prx/libc/include/General.hpp"
#include <cstdint>
#include <limits>
#include <string>

namespace {

constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidCharacter = 10;
constexpr std::int32_t BufferOverflow = 15;
constexpr std::int32_t NotTerminated = -124;

bool IsSurrogate(std::int32_t value) { return value >= 0xd800 && value <= 0xdfff; }

template<typename TDestination, typename TSource>
bool ValidateConversion(TDestination* destination, std::int32_t capacity, const TSource* source,
                        std::int32_t length, std::int32_t substitution, std::int32_t* error) {
    if (!error) NotImplemented_nid_no_patch(__func__);
    if (*error > 0) return false;
    if ((!source && length != 0) || length < -1 || capacity < 0 || (!destination && capacity > 0) ||
        substitution > 0x10ffff || IsSurrogate(substitution)) {
        *error = IllegalArgument;
        return false;
    }
    return true;
}

template<typename TSource>
std::int32_t SourceLength(const TSource* source, std::int32_t length) {
    if (length >= 0) return length;
    std::size_t count = 0;
    while (source[count] != 0) {
        if (++count > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(__func__);
    }
    return static_cast<std::int32_t>(count);
}

template<typename TDestination>
std::int32_t Terminate(TDestination* destination, std::int32_t capacity, std::int32_t length, std::int32_t* error) {
    if (!error || *error > 0 || length < 0) return length;
    if (length < capacity) {
        if (!destination) NotImplemented_nid_no_patch(__func__);
        destination[length] = 0;
        if (*error == NotTerminated) *error = 0;
    } else {
        *error = length == capacity ? NotTerminated : BufferOverflow;
    }
    return length;
}

template<typename TDestination>
void FinishConversion(TDestination* destination, std::int32_t capacity, std::int64_t required,
                      std::int32_t* length, std::int32_t* error) {
    if (required > std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
    if (length) *length = static_cast<std::int32_t>(required);
    Terminate(destination, capacity, static_cast<std::int32_t>(required), error);
}

} // namespace

#endif
