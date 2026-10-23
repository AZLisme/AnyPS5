#pragma once

#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>



namespace IcuMemory {

using AllocateCallback = void* (APS5_VABI *)(const void*, std::size_t);
using ReallocateCallback = void* (APS5_VABI *)(const void*, void*, std::size_t);
using FreeCallback = void (APS5_VABI *)(const void*, void*);

struct Callbacks {
    const void* context = nullptr;
    AllocateCallback allocate = nullptr;
    ReallocateCallback reallocate = nullptr;
    FreeCallback free = nullptr;
};

inline Callbacks callbacks_nid_no_patch;
inline const std::int32_t zeroMemory_nid_no_patch[6]{};

inline void* Zero_nid_no_patch() {
    return const_cast<std::int32_t*>(zeroMemory_nid_no_patch);
}

inline void* Allocate_nid_no_patch(std::size_t size) {
    if (size == 0) return Zero_nid_no_patch();
    return callbacks_nid_no_patch.allocate ? callbacks_nid_no_patch.allocate(callbacks_nid_no_patch.context, size) : malloc_nid_postfix(size);
}

inline void Free_nid_no_patch(void* pointer) {
    if (pointer == Zero_nid_no_patch()) return;
    if (callbacks_nid_no_patch.free) callbacks_nid_no_patch.free(callbacks_nid_no_patch.context, pointer);
    else free_nid_postfix(pointer);
}

inline void* Reallocate_nid_no_patch(void* pointer, std::size_t size) {
    if (pointer == Zero_nid_no_patch()) return Allocate_nid_no_patch(size);
    if (size == 0) {
        Free_nid_no_patch(pointer);
        return Zero_nid_no_patch();
    }
    return callbacks_nid_no_patch.reallocate ? callbacks_nid_no_patch.reallocate(callbacks_nid_no_patch.context, pointer, size) : realloc_nid_postfix(pointer, size);
}

inline void* ClearAllocate_nid_no_patch(std::size_t count, std::size_t size) {
    const auto bytes = count * size;
    void* pointer = Allocate_nid_no_patch(bytes);
    if (pointer) std::memset(pointer, 0, bytes);
    return pointer;
}

inline void Configure_nid_no_patch(const void* context, AllocateCallback allocate, ReallocateCallback reallocate, FreeCallback free, std::int32_t* status) {
    if (*status > 0) return;
    if (!allocate || !reallocate || !free) {
        *status = 1;
        return;
    }
    callbacks_nid_no_patch = {context, allocate, reallocate, free};
}

} // namespace IcuMemory
