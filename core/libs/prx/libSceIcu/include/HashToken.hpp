#ifndef CORE_LIBS_PRX_LIBSCEICU_INCLUDE_HASHTOKEN_HPP
#define CORE_LIBS_PRX_LIBSCEICU_INCLUDE_HASHTOKEN_HPP

#include <cstdint>

namespace IcuPrimitives {

union HashToken {
    void* pointer;
    std::int32_t integer;
};

static_assert(sizeof(HashToken) == 8 && alignof(HashToken) == 8);

} // namespace IcuPrimitives

#endif
