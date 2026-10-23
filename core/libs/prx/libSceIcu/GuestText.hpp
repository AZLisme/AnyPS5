#ifndef CORE_LIBS_PRX_LIBSCEICU_GUESTTEXT_HPP
#define CORE_LIBS_PRX_LIBSCEICU_GUESTTEXT_HPP

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

namespace Icu {

struct Iterator {
    const void* context;
    std::int32_t length;
    std::int32_t start;
    std::int32_t index;
    std::int32_t limit;
    std::int32_t reservedField;
    std::int32_t (APS5_VABI *getIndex)(Iterator*, std::int32_t);
    std::int32_t (APS5_VABI *move)(Iterator*, std::int32_t, std::int32_t);
    std::int8_t (APS5_VABI *hasNext)(Iterator*);
    std::int8_t (APS5_VABI *hasPrevious)(Iterator*);
    std::int32_t (APS5_VABI *current)(Iterator*);
    std::int32_t (APS5_VABI *next)(Iterator*);
    std::int32_t (APS5_VABI *previous)(Iterator*);
    std::int32_t (APS5_VABI *reservedFn)(Iterator*, std::int32_t);
    std::uint32_t (APS5_VABI *getState)(const Iterator*);
    void (APS5_VABI *setState)(Iterator*, std::uint32_t, std::int32_t*);
};

struct Text;

struct TextFunctions {
    std::int32_t tableSize;
    std::int32_t reserved1;
    std::int32_t reserved2;
    std::int32_t reserved3;
    Text* (APS5_VABI *clone)(Text*, const Text*, std::int8_t, std::int32_t*);
    std::int64_t (APS5_VABI *nativeLength)(Text*);
    std::int8_t (APS5_VABI *access)(Text*, std::int64_t, std::int8_t);
    std::int32_t (APS5_VABI *extract)(Text*, std::int64_t, std::int64_t, char16_t*, std::int32_t, std::int32_t*);
    std::int32_t (APS5_VABI *replace)(Text*, std::int64_t, std::int64_t, const char16_t*, std::int32_t, std::int32_t*);
    void (APS5_VABI *copy)(Text*, std::int64_t, std::int64_t, std::int64_t, std::int8_t, std::int32_t*);
    std::int64_t (APS5_VABI *mapOffsetToNative)(const Text*);
    std::int32_t (APS5_VABI *mapNativeIndexToUTF16)(const Text*, std::int64_t);
    void (APS5_VABI *close)(Text*);
    void (APS5_VABI *spare1)(Text*);
    void (APS5_VABI *spare2)(Text*);
    void (APS5_VABI *spare3)(Text*);
};

struct Text {
    std::uint32_t magic;
    std::int32_t flags;
    std::int32_t providerProperties;
    std::int32_t sizeOfStruct;
    std::int64_t chunkNativeLimit;
    std::int32_t extraSize;
    std::int32_t nativeIndexingLimit;
    std::int64_t chunkNativeStart;
    std::int32_t chunkOffset;
    std::int32_t chunkLength;
    const char16_t* chunkContents;
    const TextFunctions* pFuncs;
    void* pExtra;
    const void* context;
    const void* p;
    const void* q;
    const void* r;
    void* privP;
    std::int64_t a;
    std::int32_t b;
    std::int32_t c;
    std::int64_t privA;
    std::int32_t privB;
    std::int32_t privC;
};

static_assert(sizeof(Iterator) == 112 && offsetof(Iterator, getIndex) == 32 && offsetof(Iterator, current) == 64 && offsetof(Iterator, setState) == 104);
static_assert(sizeof(TextFunctions) == 112 && offsetof(TextFunctions, access) == 32 && offsetof(TextFunctions, mapNativeIndexToUTF16) == 72 && offsetof(TextFunctions, spare3) == 104);
static_assert(sizeof(Text) == 144 && offsetof(Text, chunkNativeStart) == 32 && offsetof(Text, chunkOffset) == 40 && offsetof(Text, chunkContents) == 48 && offsetof(Text, pFuncs) == 56 && offsetof(Text, privC) == 140);

} // namespace Icu

#endif
