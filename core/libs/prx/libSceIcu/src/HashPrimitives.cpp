#include "prx/libSceIcu/libSceIcu.hpp"
#include "SceTypes.hpp"
#include "prx/libSceIcu/include/HashToken.hpp"
#include "prx/libc/include/General.hpp"
#include <bit>
#include <limits>

namespace {

using IcuPrimitives::HashToken;

unsigned char Lower(unsigned char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
}

template<class TUnit> std::int32_t Hash(const TUnit* input, std::int32_t length, bool insensitive, const char* function) {
    if (!input) return 0;
    if (length < std::numeric_limits<std::int32_t>::min() + 32) NotImplemented_nid_no_patch(function);
    if (length <= 0) return 0;
    const std::size_t step = static_cast<std::size_t>((length - 32) / 32 + 1);
    std::uint32_t result = 0;
    for (std::size_t index = 0; index < static_cast<std::size_t>(length); index += step) {
        const std::uint32_t value = input[index];
        result = result * 37 + (insensitive ? Lower(static_cast<unsigned char>(value)) : value);
    }
    return std::bit_cast<std::int32_t>(result);
}

template<class TUnit> std::int32_t Length(const TUnit* input, const char* function) {
    std::size_t length = 0;
    while (input[length]) {
        if (length == static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) NotImplemented_nid_no_patch(function);
        ++length;
    }
    return static_cast<std::int32_t>(length);
}

template<class TUnit> std::int8_t Equal(HashToken first, HashToken second, bool insensitive) {
    const auto* left = static_cast<const TUnit*>(first.pointer);
    const auto* right = static_cast<const TUnit*>(second.pointer);
    if (left == right) return 1;
    if (!left || !right) return 0;
    while (*left) {
        const auto a = static_cast<std::uint32_t>(*left);
        const auto b = static_cast<std::uint32_t>(*right);
        if ((insensitive ? Lower(static_cast<unsigned char>(a)) : a) != (insensitive ? Lower(static_cast<unsigned char>(b)) : b)) return 0;
        ++left;
        ++right;
    }
    return *right == 0;
}

} // namespace

extern "C" {

std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char* input, std::int32_t length) {
    return Hash(reinterpret_cast<const unsigned char*>(input), length, false, __func__);
}

std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const char16_t* input, std::int32_t length) {
    return Hash(input, length, false, __func__);
}

std::int32_t APS5_VABI uhash_hashChars_nid_postfix(HashToken key) {
    const auto* input = static_cast<const unsigned char*>(key.pointer);
    return input ? Hash(input, Length(input, __func__), false, __func__) : 0;
}

std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(HashToken key) {
    const auto* input = static_cast<const char16_t*>(key.pointer);
    return input ? Hash(input, Length(input, __func__), false, __func__) : 0;
}

std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(HashToken key) {
    const auto* input = static_cast<const unsigned char*>(key.pointer);
    return input ? Hash(input, Length(input, __func__), true, __func__) : 0;
}

std::int32_t APS5_VABI uhash_hashLong_nid_postfix(HashToken key) {
    return key.integer;
}

std::int8_t APS5_VABI uhash_compareChars_nid_postfix(HashToken first, HashToken second) {
    return Equal<unsigned char>(first, second, false);
}

std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(HashToken first, HashToken second) {
    return Equal<char16_t>(first, second, false);
}

std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(HashToken first, HashToken second) {
    return Equal<unsigned char>(first, second, true);
}

std::int8_t APS5_VABI uhash_compareLong_nid_postfix(HashToken first, HashToken second) {
    return first.integer == second.integer;
}

}
