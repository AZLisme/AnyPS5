#ifndef CORE_LIBS_PRX_LIBSCEICU_HASHTABLE_HPP
#define CORE_LIBS_PRX_LIBSCEICU_HASHTABLE_HPP

#include <cstddef>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

namespace IcuHash {

union Token {
    void* pointer;
    std::int32_t integer;
};

struct Element {
    std::int32_t hash;
    Token value;
    Token key;
};

using Hasher = std::int32_t (APS5_VABI*)(Token);
using Comparator = std::int8_t (APS5_VABI*)(Token, Token);
using Deleter = void (APS5_VABI*)(void*);
struct Table;

static_assert(sizeof(Token) == 8);
static_assert(sizeof(Element) == 24);
static_assert(offsetof(Element, value) == 8);
static_assert(offsetof(Element, key) == 16);

} // namespace IcuHash

#endif
