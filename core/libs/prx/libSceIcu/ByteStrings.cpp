#include "prx/libSceIcu/include/Api.hpp"
#include "Memory.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace {

unsigned char Lower_nid_no_patch(unsigned char byte) {
    return byte >= 'A' && byte <= 'Z' ? static_cast<unsigned char>(byte + ('a' - 'A')) : byte;
}

template<typename TInteger>
std::int32_t IntegerText_nid_no_patch(char* buffer, TInteger number, std::uint32_t radix, const char* function) {
    if (!buffer || radix < 2 || radix > 16 || (radix == 10 && number == std::numeric_limits<TInteger>::min()))
        NotImplemented_nid_no_patch(function);
    using TUnsigned = std::make_unsigned_t<TInteger>;
    auto magnitude = static_cast<TUnsigned>(number);
    const bool negative = number < 0 && radix == 10;
    if (negative) magnitude = TUnsigned{0} - magnitude;
    std::array<char, 29> digits{};
    std::size_t count = 0;
    do {
        if (count == digits.size()) NotImplemented_nid_no_patch(function);
        const auto digit = static_cast<unsigned>(magnitude % radix);
        digits[count++] = static_cast<char>(digit < 10 ? '0' + digit : 'A' + digit - 10);
        magnitude /= radix;
    } while (magnitude != 0);
    std::size_t written = 0;
    if (negative) buffer[written++] = '-';
    while (count != 0) buffer[written++] = digits[--count];
    buffer[written] = 0;
    return static_cast<std::int32_t>(written);
}

int Compare_nid_no_patch(const char* left, const char* right, std::size_t count) {
    if (!left) return right ? -1 : 0;
    if (!right) return 1;
    while (count-- != 0) {
        const auto first = static_cast<unsigned char>(*left++);
        const auto second = static_cast<unsigned char>(*right++);
        if (first == 0) return second == 0 ? 0 : -1;
        if (second == 0) return 1;
        const int difference = static_cast<int>(Lower_nid_no_patch(first)) - static_cast<int>(Lower_nid_no_patch(second));
        if (difference != 0) return difference;
    }
    return 0;
}

} // namespace

extern "C" {


std::int32_t APS5_VABI T_CString_integerToString_nid_postfix(char* buffer, std::int32_t number, std::int32_t radix) {
    return IntegerText_nid_no_patch(buffer, number, static_cast<std::uint32_t>(radix), __func__);
}

std::int32_t APS5_VABI T_CString_int64ToString_nid_postfix(char* buffer, std::int64_t number, std::uint32_t radix) {
    return IntegerText_nid_no_patch(buffer, number, radix, __func__);
}

std::int32_t APS5_VABI T_CString_stringToInteger_nid_postfix(const char* text, std::int32_t radix) {
    if (!text) NotImplemented_nid_no_patch(__func__);
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(strtoul_nid_postfix(text, nullptr, radix)));
}

char* APS5_VABI T_CString_toLowerCase_nid_postfix(char* text) {
    if (text) {
        for (char* current = text;; ++current) {
            const auto byte = static_cast<unsigned char>(*current);
            *current = static_cast<char>(Lower_nid_no_patch(byte));
            if (byte == 0) break;
        }
    }
    return text;
}

char* APS5_VABI T_CString_toUpperCase_nid_postfix(char* text) {
    if (text) {
        for (char* current = text;; ++current) {
            const char byte = *current;
            *current = byte >= 'a' && byte <= 'z' ? static_cast<char>(byte - ('a' - 'A')) : byte;
            if (byte == 0) break;
        }
    }
    return text;
}

int APS5_VABI uprv_stricmp_nid_postfix(const char* left, const char* right) {
    return Compare_nid_no_patch(left, right, std::numeric_limits<std::size_t>::max());
}

int APS5_VABI uprv_strnicmp_nid_postfix(const char* left, const char* right, std::uint32_t count) {
    return Compare_nid_no_patch(left, right, count);
}

char* APS5_VABI uprv_strdup_nid_postfix(const char* source) {
    if (!source) NotImplemented_nid_no_patch(__func__);
    const auto bytes = std::strlen(source) + 1;
    auto* result = static_cast<char*>(IcuMemory::Allocate_nid_no_patch(bytes));
    if (result) std::memcpy(result, source, bytes);
    return result;
}

char* APS5_VABI uprv_strndup_nid_postfix(const char* source, std::int32_t count) {
    if (!source || count == std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
    if (count < 0) return uprv_strdup_nid_postfix(source);
    auto* result = static_cast<char*>(IcuMemory::Allocate_nid_no_patch(static_cast<std::size_t>(count) + 1));
    if (result) {
        if (count != 0) std::memcpy(result, source, static_cast<std::size_t>(count));
        result[count] = 0;
    }
    return result;
}

std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char* path) {
    return path && path[0] == '/' ? 1 : 0;
}

void APS5_VABI u_setMemoryFunctions_nid_postfix(const void* context, IcuMemory::AllocateCallback allocate, IcuMemory::ReallocateCallback reallocate, IcuMemory::FreeCallback free, std::int32_t* status) {
    if (!status) NotImplemented_nid_no_patch(__func__);
    IcuMemory::Configure_nid_no_patch(context, allocate, reallocate, free, status);
}

}
