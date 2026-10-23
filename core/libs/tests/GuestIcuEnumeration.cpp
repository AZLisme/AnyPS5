#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace {

using IcuEnumeration::Enumeration;
std::unordered_map<void*, std::size_t> allocations;
bool failAllocation = false;
bool failReallocation = false;
unsigned reallocations = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU enumeration: %s\n", message);
    std::abort();
}

void* APS5_VABI Allocate(std::size_t size) {
    if (failAllocation) { failAllocation = false; return nullptr; }
    void* result = std::malloc(size);
    Require(result != nullptr, "test allocation failed");
    Require(allocations.emplace(result, size).second, "allocation reused a live block");
    return result;
}

void APS5_VABI Free(void* pointer) {
    if (!pointer) return;
    Require(allocations.erase(pointer) == 1, "freed borrowed storage or freed twice");
    std::free(pointer);
}

void* APS5_VABI Reallocate(void* pointer, std::size_t size) {
    ++reallocations;
    if (failReallocation) { failReallocation = false; return nullptr; }
    Require(!pointer || allocations.erase(pointer) == 1, "reallocated unknown storage");
    void* result = std::realloc(pointer, size);
    Require(result != nullptr, "test reallocation failed");
    Require(allocations.emplace(result, size).second, "reallocation duplicated a live block");
    return result;
}

void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    auto* result = Allocate(count * size);
    if (result) std::memset(result, 0, count * size);
    return result;
}

void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

void InstallAllocator() {
    void* api[10]{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(Reallocate), reinterpret_cast<void*>(UnexpectedAlign), reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api);
}

void Arrays() {
    const std::array<const char*, 4> strings{"alpha", "", "same", "same"};
    const std::array<const char16_t*, 4> wide{u"alpha", u"", u"same", u"same"};
    for (bool fromWide : {false, true}) {
        std::int32_t error = -128;
        auto* enumeration = fromWide ? uenum_openUCharStringsEnumeration_nid_postfix(wide.data(), 4, &error)
                                     : uenum_openCharStringsEnumeration_nid_postfix(strings.data(), 4, &error);
        Require(enumeration && error == -128 && allocations.at(enumeration) == 64, "array enumeration creation and layout");
        Require(enumeration->context == (fromWide ? static_cast<const void*>(wide.data()) : static_cast<const void*>(strings.data())), "array pointer was copied or replaced");
        Require(uenum_count_nid_postfix(enumeration, &error) == 4 && error == -128, "array count and warning");
        for (int round = 0; round < 3; ++round) {
            uenum_reset_nid_postfix(enumeration, &error);
            for (std::int32_t index = 0; index < 4; ++index) {
                std::int32_t length = -1;
                if ((index + round) % 2 == 0) {
                    const auto* value = uenum_next_nid_postfix(enumeration, &length, &error);
                    Require(value && std::strcmp(value, strings[index]) == 0 && length == static_cast<std::int32_t>(std::strlen(strings[index])), "narrow iteration order or length");
                    if (!fromWide) Require(value == strings[index], "native narrow pointer identity");
                } else {
                    const auto* value = uenum_unext_nid_postfix(enumeration, &length, &error);
                    Require(value && std::u16string(value) == wide[index] && length == static_cast<std::int32_t>(std::char_traits<char16_t>::length(wide[index])), "wide iteration order or length");
                    if (fromWide) Require(value == wide[index], "native wide pointer identity");
                }
                Require(error == -128 && uenum_count_nid_postfix(enumeration, &error) == 4, "count advanced iterator or changed status");
            }
            std::int32_t length = 91;
            Require(!uenum_next_nid_postfix(enumeration, &length, &error), "narrow end returned data");
            Require(length == 91, "narrow end optional length semantics");
            length = 92;
            Require(!uenum_unext_nid_postfix(enumeration, &length, &error), "wide end returned data");
            Require(length == (fromWide ? 92 : 0), "wide end optional length semantics");
        }
        uenum_reset_nid_postfix(enumeration, &error);
        Require(std::strcmp(uenum_next_nid_postfix(enumeration, nullptr, &error), "alpha") == 0, "next nullable length");
        Require(uenum_unext_nid_postfix(enumeration, nullptr, &error) != nullptr, "unext nullable length");
        uenum_close_nid_postfix(enumeration);
        Require(allocations.empty(), "array close leaked owned buffers");
    }
    Require(strings[0] == std::string("alpha") && std::u16string(wide[0]) == u"alpha", "borrowed array strings changed");
}

void EmptyAndErrors() {
    uenum_close_nid_postfix(nullptr);
    Require(uenum_count_nid_postfix(nullptr, nullptr) == -1, "null count dereferenced status");
    Require(!uenum_next_nid_postfix(nullptr, nullptr, nullptr) && !uenum_unext_nid_postfix(nullptr, nullptr, nullptr), "null iterator dereferenced status");
    uenum_reset_nid_postfix(nullptr, nullptr);
    const char* narrow[] = {"alpha"};
    const char16_t* wide[] = {u"alpha"};
    for (bool fromWide : {false, true}) {
        for (std::int32_t count : {-1, 0, 1}) {
            std::int32_t error = 0;
            auto* enumeration = fromWide ? uenum_openUCharStringsEnumeration_nid_postfix(nullptr, count, &error)
                                         : uenum_openCharStringsEnumeration_nid_postfix(nullptr, count, &error);
            Require(error == 0 && (enumeration != nullptr) == (count == 0), "invalid count or null array changed status");
            if (enumeration) {
                std::int32_t length = 93;
                Require(uenum_count_nid_postfix(enumeration, &error) == 0 && !uenum_next_nid_postfix(enumeration, &length, &error), "empty iteration");
                uenum_reset_nid_postfix(enumeration, &error);
                uenum_close_nid_postfix(enumeration);
            }
        }
        std::int32_t error = 10;
        auto* enumeration = fromWide ? uenum_openUCharStringsEnumeration_nid_postfix(wide, 1, &error)
                                     : uenum_openCharStringsEnumeration_nid_postfix(narrow, 1, &error);
        Require(!enumeration && error == 10 && allocations.empty(), "failed status allocated an enumeration");
        error = 0;
        failAllocation = true;
        enumeration = fromWide ? uenum_openUCharStringsEnumeration_nid_postfix(wide, 1, &error)
                               : uenum_openCharStringsEnumeration_nid_postfix(narrow, 1, &error);
        Require(!enumeration && error == 7 && !failAllocation && allocations.empty(), "constructor allocation failure");
    }
}

struct CallbackContext {
    unsigned calls = 0;
    bool missingLength = false;
    bool closeSawFreedBase = false;
    std::int32_t* expectedStatus = nullptr;
};

void Callback(Enumeration* enumeration, std::int32_t* status) {
    auto* context = static_cast<CallbackContext*>(enumeration->context);
    Require(status == context->expectedStatus, "callback status pointer changed");
    ++context->calls;
}

std::int32_t APS5_VABI Count(Enumeration* enumeration, std::int32_t* status) { Callback(enumeration, status); return 321; }
const char* APS5_VABI Next(Enumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    Callback(enumeration, status);
    Require(length != nullptr, "next callback did not receive dummy length");
    *length = 3;
    return "abc";
}
const char16_t* APS5_VABI Wide(Enumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    Callback(enumeration, status);
    static_cast<CallbackContext*>(enumeration->context)->missingLength = length == nullptr;
    if (length) *length = 3;
    return u"abc";
}
const char16_t* APS5_VABI AliasedWide(Enumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    Callback(enumeration, status);
    static constexpr char16_t text[]{1, 0x80, u'A', 0};
    *length = 3;
    return text;
}
const char16_t* APS5_VABI EndWide(Enumeration* enumeration, std::int32_t* length, std::int32_t* status) {
    Callback(enumeration, status);
    static_cast<CallbackContext*>(enumeration->context)->missingLength = length == nullptr;
    return nullptr;
}
const char* APS5_VABI EndNext(Enumeration* enumeration, std::int32_t*, std::int32_t* status) {
    Callback(enumeration, status);
    return nullptr;
}
void APS5_VABI Reset(Enumeration* enumeration, std::int32_t* status) { Callback(enumeration, status); *status = 25; }
void APS5_VABI Close(Enumeration* enumeration) {
    auto* context = static_cast<CallbackContext*>(enumeration->context);
    context->closeSawFreedBase = enumeration->baseContext && allocations.count(enumeration->baseContext) == 0;
    ++context->calls;
}

void Dispatch() {
    std::int32_t error = -128;
    CallbackContext context{0, false, false, &error};
    struct Guarded { std::uint64_t before; Enumeration enumeration; std::uint64_t after; };
    Guarded guarded{0x123456789abcdef0, {nullptr, &context, Close, Count, Wide, Next, Reset}, 0xfedcba9876543210};
    const auto original = guarded;
    auto* enumeration = &guarded.enumeration;
    Require(uenum_count_nid_postfix(enumeration, &error) == 321, "custom count callback");
    Require(std::strcmp(uenum_next_nid_postfix(enumeration, nullptr, &error), "abc") == 0, "custom next callback");
    Require(std::u16string(uenum_unext_nid_postfix(enumeration, nullptr, &error)) == u"abc" && context.missingLength, "custom unext nullable length forwarding");
    Require(context.calls == 3 && std::memcmp(&guarded, &original, sizeof(guarded)) == 0, "callback dispatch altered unrelated enum bytes");
    error = 10;
    std::int32_t length = 99;
    Require(uenum_count_nid_postfix(enumeration, &error) == -1 && !uenum_next_nid_postfix(enumeration, &length, &error) && !uenum_unext_nid_postfix(enumeration, &length, &error), "latched callback failure");
    uenum_reset_nid_postfix(enumeration, &error);
    Require(context.calls == 3 && error == 10 && length == 99, "failed status dispatched or mutated outputs");
    error = 0;
    uenum_reset_nid_postfix(enumeration, &error);
    Require(context.calls == 4 && error == 25, "custom reset status propagation");
    enumeration->baseContext = Allocate(32);
    uenum_close_nid_postfix(enumeration);
    Require(context.calls == 5 && context.closeSawFreedBase && allocations.empty(), "base context was not freed before custom close");
    Enumeration missing{};
    error = 0;
    Require(uenum_count_nid_postfix(&missing, &error) == -1 && error == 16, "missing count callback");
    error = 0;
    Require(!uenum_next_nid_postfix(&missing, &length, &error) && error == 16 && length == 99, "missing next callback");
    error = 0;
    Require(!uenum_unext_nid_postfix(&missing, &length, &error) && error == 16 && length == 99, "missing unext callback");
    error = 0;
    uenum_reset_nid_postfix(&missing, &error);
    Require(error == 16, "missing reset callback");
    auto* fallback = static_cast<Enumeration*>(Allocate(sizeof(Enumeration)));
    *fallback = {};
    fallback->baseContext = Allocate(16);
    auto* retainedBase = fallback->baseContext;
    uenum_close_nid_postfix(fallback);
    Require(allocations.size() == 1 && allocations.count(retainedBase) == 1, "missing close callback fallback ownership");
    Free(retainedBase);
}

void BuffersAndFailures() {
    const std::string longString(80, 'x');
    const char* strings[]{"a", "abcdefgh", longString.c_str()};
    std::int32_t error = 0, length = -1;
    auto* enumeration = uenum_openCharStringsEnumeration_nid_postfix(strings, 3, &error);
    const auto* first = uenum_unext_nid_postfix(enumeration, &length, &error);
    Require(first && length == 1 && first[0] == u'a', "first conversion buffer");
    std::int32_t capacity = 0;
    std::memcpy(&capacity, enumeration->baseContext, sizeof(capacity));
    Require(capacity == 12 && allocations.at(enumeration->baseContext) == 16, "base buffer prefix capacity and allocation geometry");
    auto* oldBuffer = enumeration->baseContext;
    const auto before = reallocations;
    const auto* second = uenum_unext_nid_postfix(enumeration, &length, &error);
    Require(second && length == 8 && reallocations == before + 1, "buffer growth did not use realloc");
    std::memcpy(&capacity, enumeration->baseContext, sizeof(capacity));
    Require(capacity == 26 && allocations.at(enumeration->baseContext) == 30, "grown buffer capacity and geometry");
    oldBuffer = enumeration->baseContext;
    failReallocation = true;
    Require(!uenum_unext_nid_postfix(enumeration, &length, &error) && error == 7 && length == 80 && enumeration->baseContext == nullptr && allocations.count(oldBuffer) == 1, "growth failure status and base pointer semantics");
    uenum_close_nid_postfix(enumeration);
    Free(oldBuffer);
    Require(allocations.empty(), "growth failure cleanup");
    error = 0;
    enumeration = uenum_openCharStringsEnumeration_nid_postfix(strings, 3, &error);
    failAllocation = true;
    length = 91;
    Require(!uenum_unext_nid_postfix(enumeration, &length, &error) && error == 7 && length == 1 && enumeration->baseContext == nullptr, "initial conversion buffer allocation failure");
    error = 0;
    Require(std::strcmp(uenum_next_nid_postfix(enumeration, &length, &error), "abcdefgh") == 0 && length == 8, "failed conversion did not consume exactly one item");
    uenum_close_nid_postfix(enumeration);
    Require(allocations.empty(), "initial buffer failure cleanup");
    const char16_t* wide[]{u"alpha"};
    error = 0;
    enumeration = uenum_openUCharStringsEnumeration_nid_postfix(wide, 1, &error);
    failAllocation = true;
    length = 91;
    Require(!uenum_next_nid_postfix(enumeration, &length, &error) && error == 7 && length == 5 && enumeration->baseContext == nullptr, "initial narrow conversion allocation failure");
    uenum_close_nid_postfix(enumeration);
    Require(allocations.empty(), "narrow buffer failure cleanup");
}

void DirectDefaults() {
    std::int32_t error = 10, length = 99;
    CallbackContext context{0, false, false, &error};
    Enumeration enumeration{nullptr, &context, Close, Count, Wide, Next, Reset};
    const auto* wide = uenum_unextDefault_nid_postfix(&enumeration, nullptr, &error);
    Require(wide && std::u16string(wide) == u"abc" && context.calls == 1 && error == 10, "direct unext default prefailed callback dispatch");
    const auto* narrow = uenum_nextDefault_nid_postfix(&enumeration, &length, &error);
    Require(narrow && std::strcmp(narrow, "abc") == 0 && length == 3 && context.calls == 2 && error == 10, "direct next default prefailed callback dispatch");
    auto* data = static_cast<char*>(enumeration.baseContext) + sizeof(std::int32_t);
    std::memset(data, 0x55, 8);
    enumeration.uNext = AliasedWide;
    const auto* aliased = uenum_nextDefault_nid_postfix(&enumeration, reinterpret_cast<std::int32_t*>(data), &error);
    Require(aliased == data && data[0] == 1 && data[1] == 0 && data[2] == 'A' && data[3] == 0 && data[4] == 0x55 && error == 10, "aliased output length changed conversion bound");
    enumeration.uNext = EndWide;
    Require(!uenum_nextDefault_nid_postfix(&enumeration, nullptr, &error) && context.missingLength && context.calls == 4 && error == 10, "direct next default nullable length at end");
    context.expectedStatus = nullptr;
    enumeration.uNext = Wide;
    Require(std::strcmp(uenum_nextDefault_nid_postfix(&enumeration, &length, nullptr), "abc") == 0 && length == 3, "direct next default unread null status");
    Require(std::u16string(uenum_unextDefault_nid_postfix(&enumeration, &length, nullptr)) == u"abc" && length == 3, "direct unext default unread null status");
    enumeration.uNext = EndWide;
    Require(!uenum_nextDefault_nid_postfix(&enumeration, nullptr, nullptr) && context.missingLength, "direct next default null status and length at end");
    enumeration.next = EndNext;
    length = 99;
    Require(!uenum_unextDefault_nid_postfix(&enumeration, &length, nullptr) && length == 0, "direct unext default null status at end");
    uenum_close_nid_postfix(&enumeration);
    Require(allocations.empty() && context.closeSawFreedBase, "direct default buffer ownership");
    enumeration = {};
    length = 99;
    Require(!uenum_unextDefault_nid_postfix(&enumeration, &length, &error) && error == 16 && length == 0, "direct unext default missing callback length");
    error = 10;
    length = 99;
    Require(!uenum_nextDefault_nid_postfix(&enumeration, &length, &error) && error == 16 && length == 99, "direct next default missing callback length");
    error = 10;
    Require(!uenum_nextDefault_nid_postfix(&enumeration, nullptr, &error) && error == 16, "direct next default absent callback nullable length");
}

void InvariantConversion() {
    constexpr std::uint32_t invariant[4]{0xfffffbff, 0xffffffe5, 0x87fffffe, 0x87fffffe};
    for (std::uint32_t unit = 0; unit <= 0xffff; ++unit) {
        const char16_t input[]{static_cast<char16_t>(unit), 0};
        const char16_t* strings[]{input};
        std::int32_t error = 0, length = -1;
        auto* enumeration = uenum_openUCharStringsEnumeration_nid_postfix(strings, 1, &error);
        const auto* result = uenum_next_nid_postfix(enumeration, &length, &error);
        const bool maps = unit <= 0x7f && (invariant[unit / 32] & (std::uint32_t{1} << (unit % 32))) != 0;
        Require(result && error == 0 && length == (unit == 0 ? 0 : 1) && result[0] == (maps ? static_cast<char>(unit) : 0) && result[length] == 0, "invariant conversion differs from release source");
        uenum_close_nid_postfix(enumeration);
    }
    Require(allocations.empty(), "invariant conversion lifetimes");
}

} // namespace

int main() {
    InstallAllocator();
    Arrays();
    EmptyAndErrors();
    Dispatch();
    BuffersAndFailures();
    DirectDefaults();
    InvariantConversion();
    Require(allocations.empty(), "owned allocations remain live");
}
