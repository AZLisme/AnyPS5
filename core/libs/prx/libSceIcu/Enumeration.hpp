#ifndef CORE_LIBS_PRX_LIBSCEICU_ENUMERATION_HPP
#define CORE_LIBS_PRX_LIBSCEICU_ENUMERATION_HPP

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

namespace IcuEnumeration {

struct Enumeration;
using Close = void (APS5_VABI *)(Enumeration*);
using Count = std::int32_t (APS5_VABI *)(Enumeration*, std::int32_t*);
using WideNext = const char16_t* (APS5_VABI *)(Enumeration*, std::int32_t*, std::int32_t*);
using NarrowNext = const char* (APS5_VABI *)(Enumeration*, std::int32_t*, std::int32_t*);
using Reset = void (APS5_VABI *)(Enumeration*, std::int32_t*);

struct Enumeration {
    void* baseContext;
    void* context;
    Close close;
    Count count;
    WideNext uNext;
    NarrowNext next;
    Reset reset;
};

static_assert(sizeof(Enumeration) == 56);
static_assert(offsetof(Enumeration, context) == 8 && offsetof(Enumeration, close) == 16);
static_assert(offsetof(Enumeration, count) == 24 && offsetof(Enumeration, uNext) == 32);
static_assert(offsetof(Enumeration, next) == 40 && offsetof(Enumeration, reset) == 48);

} // namespace IcuEnumeration

extern "C" {
void APS5_VABI uenum_close_nid_postfix(IcuEnumeration::Enumeration*);
std::int32_t APS5_VABI uenum_count_nid_postfix(IcuEnumeration::Enumeration*, std::int32_t*);
const char* APS5_VABI uenum_next_nid_postfix(IcuEnumeration::Enumeration*, std::int32_t*, std::int32_t*);
const char16_t* APS5_VABI uenum_unext_nid_postfix(IcuEnumeration::Enumeration*, std::int32_t*, std::int32_t*);
void APS5_VABI uenum_reset_nid_postfix(IcuEnumeration::Enumeration*, std::int32_t*);
const char* APS5_VABI uenum_nextDefault_nid_postfix(IcuEnumeration::Enumeration*, std::int32_t*, std::int32_t*);
const char16_t* APS5_VABI uenum_unextDefault_nid_postfix(IcuEnumeration::Enumeration*, std::int32_t*, std::int32_t*);
IcuEnumeration::Enumeration* APS5_VABI uenum_openCharStringsEnumeration_nid_postfix(const char* const*, std::int32_t, std::int32_t*);
IcuEnumeration::Enumeration* APS5_VABI uenum_openUCharStringsEnumeration_nid_postfix(const char16_t* const*, std::int32_t, std::int32_t*);
}

#endif
