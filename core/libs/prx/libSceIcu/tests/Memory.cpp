#include "prx/libSceIcu/Memory.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

extern "C" void APS5_VABI u_setMemoryFunctions_nid_postfix(const void*, IcuMemory::AllocateCallback, IcuMemory::ReallocateCallback, IcuMemory::FreeCallback, std::int32_t*);

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "ICU memory callbacks: %s\n", message);
    std::abort();
}

struct Event { char kind; void* pointer; std::size_t size; };
struct State { std::vector<Event> events; bool fail = false; };

void* APS5_VABI Allocate(const void* context, std::size_t size) {
    auto& state = *const_cast<State*>(static_cast<const State*>(context));
    state.events.push_back({'a', nullptr, size});
    return state.fail ? nullptr : std::malloc(size);
}
void* APS5_VABI Reallocate(const void* context, void* pointer, std::size_t size) {
    auto& state = *const_cast<State*>(static_cast<const State*>(context));
    state.events.push_back({'r', pointer, size});
    return state.fail ? nullptr : std::realloc(pointer, size);
}
void APS5_VABI Free(const void* context, void* pointer) {
    auto& state = *const_cast<State*>(static_cast<const State*>(context));
    state.events.push_back({'f', pointer, 0});
    std::free(pointer);
}

void Defaults() {
    auto* zero = IcuMemory::Allocate_nid_no_patch(0);
    Require(zero && zero == IcuMemory::Allocate_nid_no_patch(0), "stable default zero allocation");
    const auto* words = static_cast<const std::int32_t*>(zero);
    Require(std::all_of(words, words + 6, [](auto value) { return value == 0; }), "zero block bytes");
    IcuMemory::Free_nid_no_patch(zero);
    auto* bytes = static_cast<unsigned char*>(IcuMemory::Allocate_nid_no_patch(16));
    Require(bytes, "default allocation");
    std::memset(bytes, 0x7a, 16);
    bytes = static_cast<unsigned char*>(IcuMemory::Reallocate_nid_no_patch(bytes, 32));
    Require(bytes && std::all_of(bytes, bytes + 16, [](auto byte) { return byte == 0x7a; }), "default realloc preservation");
    Require(IcuMemory::Reallocate_nid_no_patch(bytes, 0) == zero, "default zero-size realloc");
    IcuMemory::Free_nid_no_patch(nullptr);
    Require(IcuMemory::Reallocate_nid_no_patch(zero, 0) == zero, "zero-block realloc zero");
}

void Configuration() {
    State initial, other;
    std::int32_t status = 5;
    u_setMemoryFunctions_nid_postfix(nullptr, nullptr, nullptr, nullptr, &status);
    Require(status == 5, "incoming failure changed");
    status = -9;
    u_setMemoryFunctions_nid_postfix(&initial, Allocate, Reallocate, Free, &status);
    Require(status == -9, "successful setup changed warning");
    for (int missing = 0; missing < 3; ++missing) {
        status = 0;
        u_setMemoryFunctions_nid_postfix(&other, missing == 0 ? nullptr : Allocate, missing == 1 ? nullptr : Reallocate, missing == 2 ? nullptr : Free, &status);
        Require(status == 1, "missing callback status");
        void* pointer = IcuMemory::Allocate_nid_no_patch(3);
        Require(pointer && initial.events.back().kind == 'a' && initial.events.back().size == 3 && other.events.empty(), "invalid setup replaced context");
        IcuMemory::Free_nid_no_patch(pointer);
    }
    bool threw = false;
    try { u_setMemoryFunctions_nid_postfix(&other, Allocate, Reallocate, Free, nullptr); }
    catch (const std::runtime_error&) { threw = true; }
    Require(threw, "null status did not throw");
    IcuMemory::callbacks_nid_no_patch = {};
}

void Custom() {
    State state;
    std::int32_t status = 0;
    u_setMemoryFunctions_nid_postfix(&state, Allocate, Reallocate, Free, &status);
    auto* zero = IcuMemory::Allocate_nid_no_patch(0);
    IcuMemory::Free_nid_no_patch(zero);
    Require(IcuMemory::Reallocate_nid_no_patch(zero, 0) == zero && state.events.empty(), "zero block called callbacks");
    IcuMemory::Free_nid_no_patch(nullptr);
    Require(state.events.size() == 1 && state.events.back().kind == 'f' && !state.events.back().pointer, "free-null callback");
    Require(IcuMemory::Reallocate_nid_no_patch(nullptr, 0) == zero && state.events.size() == 2 && state.events.back().kind == 'f', "realloc-null-zero callback");
    auto* bytes = static_cast<unsigned char*>(IcuMemory::Reallocate_nid_no_patch(nullptr, 8));
    Require(bytes && state.events.back().kind == 'r' && !state.events.back().pointer && state.events.back().size == 8, "realloc-null-positive callback");
    std::memset(bytes, 0x4e, 8);
    state.fail = true;
    Require(IcuMemory::Reallocate_nid_no_patch(bytes, 16) == nullptr && state.events.back().kind == 'r' && state.events.back().pointer == bytes, "failed realloc result or input");
    Require(std::all_of(bytes, bytes + 8, [](auto byte) { return byte == 0x4e; }), "failed realloc changed source");
    Require(IcuMemory::Allocate_nid_no_patch(7) == nullptr && IcuMemory::ClearAllocate_nid_no_patch(2, 4) == nullptr, "failed allocation result");
    state.fail = false;
    Require(IcuMemory::Reallocate_nid_no_patch(bytes, 0) == zero && state.events.back().kind == 'f' && state.events.back().pointer == bytes, "zero realloc free callback");
    bytes = static_cast<unsigned char*>(IcuMemory::Reallocate_nid_no_patch(zero, 12));
    Require(bytes && state.events.back().kind == 'a' && state.events.back().size == 12, "zero-block realloc calls allocation");
    IcuMemory::Free_nid_no_patch(bytes);
    bytes = static_cast<unsigned char*>(IcuMemory::ClearAllocate_nid_no_patch(3, 7));
    Require(bytes && state.events.back().size == 21 && std::all_of(bytes, bytes + 21, [](auto byte) { return byte == 0; }), "clear allocation byte count");
    IcuMemory::Free_nid_no_patch(bytes);
    const auto count = state.events.size();
    Require(IcuMemory::ClearAllocate_nid_no_patch(std::numeric_limits<std::size_t>::max() / 8 + 1, 8) == zero && state.events.size() == count, "unsigned product wrap zero behavior");
    IcuMemory::callbacks_nid_no_patch = {};
}

} // namespace

int main() {
    const std::array<void*, 10> heap{};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    Defaults();
    Configuration();
    Custom();
}
