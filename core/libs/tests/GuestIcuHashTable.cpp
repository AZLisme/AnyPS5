#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/HashTable.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <vector>

using namespace IcuHash;



static void Require(bool good, std::source_location location = std::source_location::current()) {
    if (good) return;
    std::fprintf(stderr, "ICU hash table check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void Throws(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

static int failAllocation;
static std::vector<std::size_t> allocations;
static unsigned frees;
static unsigned hasherCalls;
static Table* active;
static std::vector<void*> deletedKeys;
static std::vector<void*> deletedValues;
static std::vector<int> countsAtDelete;
static bool changeValueDeleter;
static void* replacementDuringClose;
static void* closingTable;

static void* APS5_VABI Allocate(std::size_t size) {
    allocations.push_back(size);
    if (failAllocation && --failAllocation == 0) return nullptr;
    return std::malloc(size);
}
static void APS5_VABI Free(void* memory) {
    if (memory == closingTable && closingTable) {
        void* elements = memory;
        std::memcpy(&elements, memory, sizeof(elements));
        Require(!elements);
        closingTable = nullptr;
    }
    ++frees;
    std::free(memory);
}
static void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    auto* result = Allocate(count * size);
    if (result) std::memset(result, 0, count * size);
    return result;
}
static void* APS5_VABI Reallocate(void* memory, std::size_t size) { return std::realloc(memory, size); }
static void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
static void* APS5_VABI Realign(void* memory, std::size_t size, std::size_t) { return std::realloc(memory, size); }
static int APS5_VABI PosixAlign(void** memory, std::size_t, std::size_t size) {
    *memory = std::malloc(size);
    return *memory ? 0 : 12;
}

static std::int32_t APS5_VABI Hash(Token key) {
    ++hasherCalls;
    return key.pointer ? (*static_cast<int*>(key.pointer) % 11) | static_cast<std::int32_t>(0x80000000u) : 0;
}
static std::int8_t APS5_VABI Equal(Token first, Token second) {
    if (!first.pointer || !second.pointer) return first.pointer == second.pointer;
    return *static_cast<int*>(first.pointer) == *static_cast<int*>(second.pointer);
}
static void APS5_VABI DeleteValue(void* value) {
    deletedValues.push_back(value);
    countsAtDelete.push_back(uhash_count_nid_postfix(active));
}
static void APS5_VABI DeleteKey(void* key) {
    deletedKeys.push_back(key);
    countsAtDelete.push_back(uhash_count_nid_postfix(active));
    if (changeValueDeleter) uhash_setValueDeleter_nid_postfix(active, DeleteValue);
    if (replacementDuringClose) {
        auto* replacement = replacementDuringClose;
        replacementDuringClose = nullptr;
        std::int32_t status = 0;
        Require(!uhash_put_nid_postfix(active, key, replacement, &status) && !status);
    }
}

static Table* Open(std::int32_t& status) {
    auto* table = uhash_open_nid_postfix(Hash, Equal, nullptr, &status);
    Require(table != nullptr);
    return table;
}

static void Basic() {
    std::int32_t status = -128;
    auto* table = Open(status);
    Require(status == -128 && uhash_count_nid_postfix(table) == 0);
    int first = 3, alias = 3, other = 14, value = 77, replacement = 88;
    Require(uhash_put_nid_postfix(table, &first, &value, &status) == nullptr);
    Require(uhash_get_nid_postfix(table, &alias) == &value);
    Require(uhash_get_nid_postfix(table, &other) == nullptr);
    Require(uhash_put_nid_postfix(table, &alias, &replacement, &status) == &value);
    Require(uhash_count_nid_postfix(table) == 1);
    std::int32_t position = -1;
    const auto* element = uhash_nextElement_nid_postfix(table, &position);
    Require(element && element->hash == 3 && element->key.pointer == &alias && element->value.pointer == &replacement);
    const auto last = position;
    Require(!uhash_nextElement_nid_postfix(table, &position) && position == last);
    position = 100000;
    Require(!uhash_nextElement_nid_postfix(table, &position) && position == 100000);
    Require(uhash_put_nid_postfix(table, &first, nullptr, &status) == &replacement);
    Require(!uhash_count_nid_postfix(table) && !uhash_remove_nid_postfix(table, &first));
    Require(!uhash_put_nid_postfix(table, nullptr, &value, &status));
    Require(uhash_get_nid_postfix(table, nullptr) == &value);
    Require(uhash_remove_nid_postfix(table, nullptr) == &value);
    uhash_close_nid_postfix(table);
    uhash_close_nid_postfix(nullptr);
}

static void Ownership() {
    std::int32_t status = 0;
    active = Open(status);
    int key = 9, alias = 9, third = 9, value = 1, replacement = 2;
    deletedKeys.clear(); deletedValues.clear(); countsAtDelete.clear();
    Require(!uhash_setKeyDeleter_nid_postfix(active, DeleteKey));
    Require(uhash_setKeyDeleter_nid_postfix(active, DeleteKey) == DeleteKey);
    Require(!uhash_setValueDeleter_nid_postfix(active, DeleteValue));
    Require(uhash_setValueDeleter_nid_postfix(active, nullptr) == DeleteValue);
    Require(!uhash_put_nid_postfix(active, &key, &value, &status));
    changeValueDeleter = true;
    Require(!uhash_put_nid_postfix(active, &alias, &replacement, &status));
    changeValueDeleter = false;
    Require(deletedKeys == std::vector<void*>{&key} && deletedValues == std::vector<void*>{&value});
    Require(countsAtDelete == std::vector<int>({1, 1}));
    Require(!uhash_put_nid_postfix(active, &alias, &replacement, &status));
    Require(deletedKeys.size() == 1 && deletedValues.size() == 1);
    Require(!uhash_put_nid_postfix(active, &third, nullptr, &status));
    Require(deletedKeys.back() == &alias && deletedValues.back() == &replacement);
    Require(countsAtDelete[countsAtDelete.size() - 1] == 0 && uhash_count_nid_postfix(active) == 0);
    status = 3;
    Require(!uhash_put_nid_postfix(active, &third, &value, &status) && status == 3);
    Require(deletedKeys.back() == &third && deletedValues.back() == &value);
    uhash_close_nid_postfix(active);
    active = nullptr;
}

static void GrowthAndIteration() {
    std::int32_t status = 0;
    auto* table = Open(status);
    std::array<int, 600> keys{}, values{};
    for (int index = 0; index < 600; ++index) {
        keys[index] = index; values[index] = 1000 + index;
        Require(!uhash_put_nid_postfix(table, &keys[index], &values[index], &status));
    }
    Require(status == 0 && uhash_count_nid_postfix(table) == 600);
    std::array<bool, 600> visited{};
    std::int32_t position = -1;
    int count = 0;
    while (const auto* element = uhash_nextElement_nid_postfix(table, &position)) {
        const auto index = *static_cast<int*>(element->key.pointer);
        Require(index >= 0 && index < 600 && !visited[index]);
        visited[index] = true;
        Require(element->value.pointer == &values[index]);
        Require(uhash_remove_nid_postfix(table, element->key.pointer) == &values[index]);
        ++count;
    }
    Require(count == 600 && !uhash_count_nid_postfix(table));
    for (int index = 0; index < 600; ++index) Require(!uhash_put_nid_postfix(table, &keys[index], &values[index], &status));
    uhash_removeAll_nid_postfix(table);
    Require(!uhash_count_nid_postfix(table));
    for (const auto& key : keys) Require(!uhash_get_nid_postfix(table, &key));
    uhash_close_nid_postfix(table);
}

static void CloseMutation() {
    std::int32_t status = 0;
    active = Open(status);
    int key = 13, value = 7, replacement = 9;
    uhash_setKeyDeleter_nid_postfix(active, DeleteKey);
    uhash_setValueDeleter_nid_postfix(active, DeleteValue);
    Require(!uhash_put_nid_postfix(active, &key, &value, &status));
    deletedKeys.clear(); deletedValues.clear(); countsAtDelete.clear();
    replacementDuringClose = &replacement;
    closingTable = active;
    uhash_close_nid_postfix(active);
    Require(!closingTable && !replacementDuringClose);
    Require(deletedKeys == std::vector<void*>{&key});
    Require(deletedValues == std::vector<void*>({&value, &replacement}));
    Require(countsAtDelete == std::vector<int>({1, 1, 1}));
    active = nullptr;
}

static void Failures() {
    std::int32_t status = 1;
    allocations.clear();
    Require(!uhash_open_nid_postfix(nullptr, nullptr, nullptr, &status) && allocations.empty());
    status = 0; failAllocation = 1;
    Require(!uhash_open_nid_postfix(Hash, Equal, nullptr, &status) && status == 7);
    status = 0; failAllocation = 2;
    const auto oldFrees = frees;
    Require(!uhash_open_nid_postfix(Hash, Equal, nullptr, &status) && status == 7 && frees == oldFrees + 1);
    failAllocation = 0; status = 0;
    auto* table = Open(status);
    Require(allocations.back() == 127 * 24 && allocations[allocations.size() - 2] == 80);
    std::array<int, 65> keys{}, values{};
    for (int index = 0; index < 64; ++index) {
        keys[index] = index; values[index] = 100 + index;
        Require(!uhash_put_nid_postfix(table, &keys[index], &values[index], &status));
    }
    active = table;
    uhash_setKeyDeleter_nid_postfix(table, DeleteKey);
    uhash_setValueDeleter_nid_postfix(table, DeleteValue);
    keys[64] = 64; values[64] = 164;
    const auto oldHashCalls = hasherCalls;
    failAllocation = 1;
    Require(!uhash_put_nid_postfix(table, &keys[64], &values[64], &status) && status == 7);
    Require(hasherCalls == oldHashCalls && uhash_count_nid_postfix(table) == 64);
    Require(allocations.back() == 251 * 24 && deletedKeys.back() == &keys[64] && deletedValues.back() == &values[64]);
    failAllocation = 0; status = 0;
    Require(!uhash_put_nid_postfix(table, &keys[64], &values[64], &status));
    Require(allocations.back() == 509 * 24 && uhash_count_nid_postfix(table) == 65);
    uhash_close_nid_postfix(table);
    active = nullptr;
    Throws([&] { uhash_open_nid_postfix(Hash, Equal, nullptr, nullptr); });
    Throws([&] { uhash_count_nid_postfix(nullptr); });
}

static void ExhaustedGrowth() {
    std::int32_t status = 0;
    auto* table = Open(status);
    std::array<int, 127> keys{}, values{};
    for (int index = 0; index < 127; ++index) { keys[index] = index; values[index] = index + 1; }
    for (int index = 0; index < 64; ++index) Require(!uhash_put_nid_postfix(table, &keys[index], &values[index], &status));
    for (int attempt = 0; attempt < 24; ++attempt) {
        failAllocation = 1;
        status = 0;
        Require(!uhash_put_nid_postfix(table, &keys[64], &values[64], &status));
        Require(status == 7 && uhash_count_nid_postfix(table) == 64);
    }
    failAllocation = 0;
    status = 0;
    for (int index = 64; index < 126; ++index) Require(!uhash_put_nid_postfix(table, &keys[index], &values[index], &status));
    Require(uhash_count_nid_postfix(table) == 126 && status == 0);
    Require(!uhash_put_nid_postfix(table, &keys[126], &values[126], &status));
    Require(status == 7 && uhash_count_nid_postfix(table) == 126);
    Require(!uhash_get_nid_postfix(table, &keys[126]));
    Require(uhash_remove_nid_postfix(table, &keys[3]) == &values[3]);
    status = 0;
    Require(!uhash_put_nid_postfix(table, &keys[126], &values[126], &status) && status == 0);
    Require(uhash_get_nid_postfix(table, &keys[126]) == &values[126]);
    uhash_close_nid_postfix(table);
}

int main() {
    void* api[10]{reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Free), reinterpret_cast<void*>(&Calloc),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign), reinterpret_cast<void*>(&PosixAlign)};
    ApplicationHeapRegister_nid_no_patch(api);
    Basic();
    Ownership();
    CloseMutation();
    GrowthAndIteration();
    Failures();
    ExhaustedGrowth();
    std::puts("ICU hash table tests passed");
}
