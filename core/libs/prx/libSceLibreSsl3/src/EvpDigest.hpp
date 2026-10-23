#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL3_SRC_EVPDIGEST_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL3_SRC_EVPDIGEST_HPP

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

namespace LibreSsl3 {

struct DigestContext;

struct DigestMethod {
    std::int32_t type;
    std::int32_t keyType;
    std::int32_t digestSize;
    std::uint64_t flags;
    int (APS5_VABI* init)(DigestContext*);
    int (APS5_VABI* update)(DigestContext*, const void*, std::size_t);
    int (APS5_VABI* finalize)(DigestContext*, unsigned char*);
    int (APS5_VABI* copy)(DigestContext*, const DigestContext*);
    int (APS5_VABI* cleanup)(DigestContext*);
    std::int32_t blockSize;
    std::int32_t contextSize;
    int (APS5_VABI* control)(DigestContext*, int, int, void*);
};

struct DigestContext {
    const DigestMethod* digest;
    std::uint64_t flags;
    void* data;
    void* keyContext;
    int (APS5_VABI* update)(DigestContext*, const void*, std::size_t);
};

static_assert(sizeof(DigestMethod) == 80);
static_assert(offsetof(DigestMethod, init) == 24);
static_assert(offsetof(DigestMethod, contextSize) == 68);
static_assert(sizeof(DigestContext) == 40);
static_assert(offsetof(DigestContext, update) == 32);

} // namespace LibreSsl3

#endif
