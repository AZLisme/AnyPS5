#ifndef CORE_LIBS_PRX_LIBSCEICU_HASHTABLE_HPP
#define CORE_LIBS_PRX_LIBSCEICU_HASHTABLE_HPP

#include <cstddef>
#include <cstdint>
#include "include/HashToken.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

namespace IcuHash {

using Token = IcuPrimitives::HashToken;

struct Element {
    std::int32_t hash;
    Token value;
    Token key;
};

using Hasher = std::int32_t (APS5_VABI*)(Token);
using Comparator = std::int8_t (APS5_VABI*)(Token, Token);
using Deleter = void (APS5_VABI*)(void*);
struct Table {
    Element* elements;
    Hasher hasher;
    Comparator comparator;
    Comparator valueComparator;
    Deleter keyDeleter;
    Deleter valueDeleter;
    std::int32_t count;
    std::int32_t length;
    std::int32_t highWater;
    std::int32_t lowWater;
    float highRatio;
    float lowRatio;
    std::int8_t primeIndex;
    std::int8_t allocated;
};

static_assert(sizeof(Table) == 80);
static_assert(offsetof(Table, count) == 48);
static_assert(offsetof(Table, primeIndex) == 72);

static_assert(sizeof(Token) == 8);
static_assert(sizeof(Element) == 24);
static_assert(offsetof(Element, value) == 8);
static_assert(offsetof(Element, key) == 16);

} // namespace IcuHash

#endif
