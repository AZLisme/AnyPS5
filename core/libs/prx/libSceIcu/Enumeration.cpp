#include "Enumeration.hpp"
#include "Memory.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <limits>
#include <string>

namespace {

using IcuEnumeration::Enumeration;
constexpr std::int32_t Unsupported = 16;
constexpr std::int32_t NoMemory = 7;

struct ArrayEnumeration {
    Enumeration base;
    std::int32_t index;
    std::int32_t count;
};

static_assert(sizeof(ArrayEnumeration) == 64 && offsetof(ArrayEnumeration, index) == 56);

void Status(std::int32_t* status) {
    if (!status) NotImplemented_nid_no_patch(__func__);
}

template<class T>
std::int32_t Length(const T* text) {
    if (!text) NotImplemented_nid_no_patch(__func__);
    const auto length = std::char_traits<T>::length(text);
    if (length > std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
    return static_cast<std::int32_t>(length);
}

void* Buffer(Enumeration* enumeration, std::int32_t length, std::size_t width) {
    if (length < 0 || static_cast<std::size_t>(length) + 1 > (static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) - 8) / width)
        NotImplemented_nid_no_patch(__func__);
    const auto requested = static_cast<std::int32_t>((static_cast<std::size_t>(length) + 1) * width);
    std::int32_t available = 0;
    if (enumeration->baseContext) std::memcpy(&available, enumeration->baseContext, sizeof(available));
    if (!enumeration->baseContext || available < requested) {
        available = requested + 8;
        const auto allocation = static_cast<std::size_t>(available) + sizeof(available);
        enumeration->baseContext = enumeration->baseContext
            ? IcuMemory::Reallocate_nid_no_patch(enumeration->baseContext, allocation)
            : IcuMemory::Allocate_nid_no_patch(allocation);
        if (!enumeration->baseContext) return nullptr;
        std::memcpy(enumeration->baseContext, &available, sizeof(available));
    }
    return static_cast<char*>(enumeration->baseContext) + sizeof(available);
}

bool Invariant(char16_t unit) {
    if (unit > 0x7f) return false;
    if (unit == 0x0a || unit == 0x21 || unit == 0x23 || unit == 0x24 || unit == 0x40 || unit == 0x60) return false;
    return !(unit >= 0x5b && unit <= 0x5e) && !(unit >= 0x7b && unit <= 0x7e);
}

void APS5_VABI CloseArray_nid_no_patch(Enumeration* enumeration) {
    IcuMemory::Free_nid_no_patch(enumeration);
}

std::int32_t APS5_VABI CountArray_nid_no_patch(Enumeration* enumeration, std::int32_t*) {
    return reinterpret_cast<ArrayEnumeration*>(enumeration)->count;
}

const char* APS5_VABI NextArray_nid_no_patch(Enumeration* enumeration, std::int32_t* length, std::int32_t*) {
    auto* array = reinterpret_cast<ArrayEnumeration*>(enumeration);
    if (array->index >= array->count) return nullptr;
    const auto* result = static_cast<const char* const*>(enumeration->context)[array->index++];
    if (length) *length = Length(result);
    return result;
}

const char16_t* APS5_VABI WideArray_nid_no_patch(Enumeration* enumeration, std::int32_t* length, std::int32_t*) {
    auto* array = reinterpret_cast<ArrayEnumeration*>(enumeration);
    if (array->index >= array->count) return nullptr;
    const auto* result = static_cast<const char16_t* const*>(enumeration->context)[array->index++];
    if (length) *length = Length(result);
    return result;
}

void APS5_VABI ResetArray_nid_no_patch(Enumeration* enumeration, std::int32_t*) {
    reinterpret_cast<ArrayEnumeration*>(enumeration)->index = 0;
}

Enumeration* OpenArray(const void* strings, std::int32_t count, std::int32_t* status, bool wide) {
    Status(status);
    if (*status > 0 || count < 0 || (count != 0 && !strings)) return nullptr;
    auto* result = static_cast<ArrayEnumeration*>(IcuMemory::Allocate_nid_no_patch(sizeof(ArrayEnumeration)));
    if (!result) { *status = NoMemory; return nullptr; }
    result->base = {nullptr, const_cast<void*>(strings), CloseArray_nid_no_patch, CountArray_nid_no_patch,
        wide ? WideArray_nid_no_patch : uenum_unextDefault_nid_postfix,
        wide ? uenum_nextDefault_nid_postfix : NextArray_nid_no_patch, ResetArray_nid_no_patch};
    result->index = 0;
    result->count = count;
    return &result->base;
}

} // namespace

extern "C" {

void APS5_VABI uenum_close_nid_postfix(Enumeration* enumeration) {
    if (!enumeration) return;
    if (enumeration->close) {
        if (enumeration->baseContext) IcuMemory::Free_nid_no_patch(enumeration->baseContext);
        enumeration->close(enumeration);
    } else IcuMemory::Free_nid_no_patch(enumeration);
}

std::int32_t APS5_VABI uenum_count_nid_postfix(Enumeration* enumeration, std::int32_t* status) {
    if (!enumeration) return -1;
    Status(status);
    if (*status > 0) return -1;
    if (enumeration->count) return enumeration->count(enumeration, status);
    *status = Unsupported;
    return -1;
}

const char* APS5_VABI uenum_next_nid_postfix(Enumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    if (!enumeration) return nullptr;
    Status(status);
    if (*status > 0) return nullptr;
    if (!enumeration->next) { *status = Unsupported; return nullptr; }
    std::int32_t ignored = 0;
    return enumeration->next(enumeration, length ? length : &ignored, status);
}

const char16_t* APS5_VABI uenum_unext_nid_postfix(Enumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    if (!enumeration) return nullptr;
    Status(status);
    if (*status > 0) return nullptr;
    if (!enumeration->uNext) { *status = Unsupported; return nullptr; }
    return enumeration->uNext(enumeration, length, status);
}

void APS5_VABI uenum_reset_nid_postfix(Enumeration* enumeration, std::int32_t* status) {
    if (!enumeration) return;
    Status(status);
    if (*status > 0) return;
    if (enumeration->reset) enumeration->reset(enumeration, status);
    else *status = Unsupported;
}

const char16_t* APS5_VABI uenum_unextDefault_nid_postfix(Enumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    if (!enumeration) NotImplemented_nid_no_patch(__func__);
    std::int32_t count = 0;
    char16_t* result = nullptr;
    if (enumeration->next) {
        const auto* input = enumeration->next(enumeration, &count, status);
        if (input) {
            result = static_cast<char16_t*>(Buffer(enumeration, count, sizeof(char16_t)));
            if (!result) { Status(status); *status = NoMemory; }
            else for (std::int64_t index = 0; index <= count; ++index) result[index] = static_cast<std::uint8_t>(input[index]);
        }
    } else { Status(status); *status = Unsupported; }
    if (length) *length = count;
    return result;
}

const char* APS5_VABI uenum_nextDefault_nid_postfix(Enumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    if (!enumeration) NotImplemented_nid_no_patch(__func__);
    if (!enumeration->uNext) { Status(status); *status = Unsupported; return nullptr; }
    const auto* input = enumeration->uNext(enumeration, length, status);
    if (!input) return nullptr;
    if (!length) NotImplemented_nid_no_patch(__func__);
    auto* result = static_cast<char*>(Buffer(enumeration, *length, sizeof(char)));
    if (!result) { Status(status); *status = NoMemory; return nullptr; }
    const auto count = *length;
    for (std::int64_t index = 0; index <= count; ++index) result[index] = Invariant(input[index]) ? static_cast<char>(input[index]) : 0;
    return result;
}

Enumeration* APS5_VABI uenum_openCharStringsEnumeration_nid_postfix(const char* const* strings, std::int32_t count, std::int32_t* status) {
    return OpenArray(strings, count, status, false);
}

Enumeration* APS5_VABI uenum_openUCharStringsEnumeration_nid_postfix(const char16_t* const* strings, std::int32_t count, std::int32_t* status) {
    return OpenArray(strings, count, status, true);
}

}
