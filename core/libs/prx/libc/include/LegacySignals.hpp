#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_LEGACYSIGNALS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_LEGACYSIGNALS_HPP

#include "general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

struct GuestSignalSet {
    std::uint32_t bits[4];
};

struct GuestSigaction {
    std::uintptr_t handler;
    int flags;
    GuestSignalSet mask;
};

struct GuestSignalVector {
    std::uintptr_t handler;
    int mask;
    int flags;
};

static_assert(sizeof(GuestSignalSet) == 16);
static_assert(sizeof(GuestSigaction) == 32 && offsetof(GuestSigaction, flags) == 8 && offsetof(GuestSigaction, mask) == 12);
static_assert(sizeof(GuestSignalVector) == 16 && offsetof(GuestSignalVector, mask) == 8 && offsetof(GuestSignalVector, flags) == 12);

using LegacySigaction_nid_no_patch = int (APS5_VABI *)(int, const GuestSigaction*, GuestSigaction*);
using LegacySigprocmask_nid_no_patch = int (APS5_VABI *)(int, const GuestSignalSet*, GuestSignalSet*);

extern "C" void RegisterLegacySignals_nid_no_patch(LegacySigaction_nid_no_patch action, LegacySigprocmask_nid_no_patch mask);

#endif
