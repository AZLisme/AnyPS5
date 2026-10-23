#include "HashTable.hpp"
#include "Memory.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <limits>

namespace IcuHash {

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

} // namespace IcuHash

namespace {

using namespace IcuHash;

constexpr auto Deleted = std::numeric_limits<std::int32_t>::min();
constexpr auto Empty = Deleted + 1;
constexpr std::int32_t AllocationError = 7;
constexpr std::array<std::int32_t, 29> Capacities{
    7, 13, 31, 61, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749,
    65521, 131071, 262139, 524287, 1048573, 2097143, 4194301, 8388593,
    16777213, 33554393, 67108859, 134217689, 268435399, 536870909, 1073741789, 2147483647};

Table& Require(Table* table, const char* function) {
    if (!table) NotImplemented_nid_no_patch(function);
    return *table;
}

const Table& Require(const Table* table, const char* function) {
    if (!table) NotImplemented_nid_no_patch(function);
    return *table;
}

void Initialize(Element* elements, std::int32_t length) {
    for (std::int32_t index = 0; index < length; ++index) elements[index] = {Empty, {.pointer = nullptr}, {.pointer = nullptr}};
}

Element& Find(const Table& table, Token key, std::int32_t code) {
    const auto hash = code & 0x7fffffff;
    const auto length = static_cast<std::uint32_t>(table.length);
    const auto start = static_cast<std::uint32_t>(hash ^ 0x4000000) % length;
    const auto stride = static_cast<std::uint32_t>(hash) % (length - 1) + 1;
    std::int32_t deleted = -1;
    auto index = start;
    do {
        auto& element = table.elements[index];
        if (element.hash == hash && table.comparator(key, element.key)) return element;
        if (element.hash == Empty) return table.elements[deleted < 0 ? index : static_cast<std::uint32_t>(deleted)];
        if (element.hash == Deleted && deleted < 0) deleted = static_cast<std::int32_t>(index);
        index = (index + static_cast<std::uint64_t>(stride)) % length;
    } while (index != start);
    if (deleted >= 0) return table.elements[deleted];
    NotImplemented_nid_no_patch(__func__);
}

void DeleteArguments(Table& table, void* key, void* value) {
    if (key && table.keyDeleter) table.keyDeleter(key);
    if (value && table.valueDeleter) table.valueDeleter(value);
}

void* Replace(Table& table, Element& element, std::int32_t hash, void* key, void* value) {
    void* previous = element.value.pointer;
    if (element.key.pointer && element.key.pointer != key && table.keyDeleter) table.keyDeleter(element.key.pointer);
    if (table.valueDeleter) {
        if (previous && previous != value) table.valueDeleter(previous);
        previous = nullptr;
    }
    element = {hash, {.pointer = value}, {.pointer = key}};
    return previous;
}

void* Remove(Table& table, const void* key) {
    const Token token{.pointer = const_cast<void*>(key)};
    auto& element = Find(table, token, table.hasher(token));
    if (element.hash < 0) return nullptr;
    --table.count;
    return Replace(table, element, Deleted, nullptr, nullptr);
}

bool Grow(Table& table, std::int32_t& status) {
    if (table.count <= table.highWater || table.primeIndex + 1 >= static_cast<std::int32_t>(Capacities.size())) return true;
    const auto next = static_cast<std::int32_t>(table.primeIndex) + 1;
    table.primeIndex = static_cast<std::int8_t>(next);
    const auto length = Capacities[next];
    auto* elements = static_cast<Element*>(IcuMemory::Allocate_nid_no_patch(static_cast<std::size_t>(length) * sizeof(Element)));
    if (!elements) {
        status = AllocationError;
        return false;
    }
    Initialize(elements, length);
    auto* old = table.elements;
    const auto oldLength = table.length;
    table.elements = elements;
    table.length = length;
    table.highWater = static_cast<std::int32_t>(length * table.highRatio);
    table.count = 0;
    for (auto index = oldLength; index > 0;) {
        const auto& element = old[--index];
        if (element.hash < 0) continue;
        Find(table, element.key, element.hash) = element;
        ++table.count;
    }
    IcuMemory::Free_nid_no_patch(old);
    return true;
}

} // namespace

extern "C" {

IcuHash::Table* APS5_VABI uhash_open_nid_postfix(IcuHash::Hasher hasher, IcuHash::Comparator comparator,
    IcuHash::Comparator valueComparator, std::int32_t* status) {
    if (!status) NotImplemented_nid_no_patch(__func__);
    if (*status > 0) return nullptr;
    if (!hasher || !comparator) NotImplemented_nid_no_patch(__func__);
    auto* table = static_cast<Table*>(IcuMemory::Allocate_nid_no_patch(sizeof(Table)));
    if (!table) {
        *status = AllocationError;
        return nullptr;
    }
    *table = {nullptr, hasher, comparator, valueComparator, nullptr, nullptr, 0, 127, 63, 0, 0.5f, 0.0f, 4, 1};
    table->elements = static_cast<Element*>(IcuMemory::Allocate_nid_no_patch(127 * sizeof(Element)));
    if (!table->elements) {
        *status = AllocationError;
        IcuMemory::Free_nid_no_patch(table);
        return nullptr;
    }
    Initialize(table->elements, table->length);
    return table;
}

void APS5_VABI uhash_close_nid_postfix(IcuHash::Table* table) {
    if (!table) return;
    for (std::int32_t index = 0; index < table->length; ++index) {
        const auto& element = table->elements[index];
        if (element.hash < 0) continue;
        if (table->keyDeleter && element.key.pointer) table->keyDeleter(element.key.pointer);
        if (table->valueDeleter && element.value.pointer) table->valueDeleter(element.value.pointer);
    }
    IcuMemory::Free_nid_no_patch(table->elements);
    table->elements = nullptr;
    IcuMemory::Free_nid_no_patch(table);
}

std::int32_t APS5_VABI uhash_count_nid_postfix(const IcuHash::Table* table) {
    return Require(table, __func__).count;
}

IcuHash::Deleter APS5_VABI uhash_setKeyDeleter_nid_postfix(IcuHash::Table* table, IcuHash::Deleter deleter) {
    auto& target = Require(table, __func__);
    const auto previous = target.keyDeleter;
    target.keyDeleter = deleter;
    return previous;
}

IcuHash::Deleter APS5_VABI uhash_setValueDeleter_nid_postfix(IcuHash::Table* table, IcuHash::Deleter deleter) {
    auto& target = Require(table, __func__);
    const auto previous = target.valueDeleter;
    target.valueDeleter = deleter;
    return previous;
}

void* APS5_VABI uhash_get_nid_postfix(const IcuHash::Table* table, const void* key) {
    const auto& target = Require(table, __func__);
    const IcuHash::Token token{.pointer = const_cast<void*>(key)};
    return Find(target, token, target.hasher(token)).value.pointer;
}

void* APS5_VABI uhash_remove_nid_postfix(IcuHash::Table* table, const void* key) {
    return Remove(Require(table, __func__), key);
}

void* APS5_VABI uhash_put_nid_postfix(IcuHash::Table* table, void* key, void* value, std::int32_t* status) {
    auto& target = Require(table, __func__);
    if (!status) NotImplemented_nid_no_patch(__func__);
    if (*status > 0) {
        DeleteArguments(target, key, value);
        return nullptr;
    }
    if (!value) return Remove(target, key);
    if (!Grow(target, *status)) {
        DeleteArguments(target, key, value);
        return nullptr;
    }
    const IcuHash::Token token{.pointer = key};
    const auto code = target.hasher(token);
    auto& element = Find(target, token, code);
    if (element.hash < 0) {
        if (target.count == target.length - 1) {
            *status = AllocationError;
            DeleteArguments(target, key, value);
            return nullptr;
        }
        ++target.count;
    }
    return Replace(target, element, code & 0x7fffffff, key, value);
}

const IcuHash::Element* APS5_VABI uhash_nextElement_nid_postfix(const IcuHash::Table* table, std::int32_t* position) {
    const auto& target = Require(table, __func__);
    if (!position || *position < -1 || *position == std::numeric_limits<std::int32_t>::max()) NotImplemented_nid_no_patch(__func__);
    for (std::int64_t index = static_cast<std::int64_t>(*position) + 1; index < target.length; ++index) {
        if (target.elements[index].hash < 0) continue;
        *position = static_cast<std::int32_t>(index);
        return &target.elements[index];
    }
    return nullptr;
}

void APS5_VABI uhash_removeAll_nid_postfix(IcuHash::Table* table) {
    auto& target = Require(table, __func__);
    for (std::int32_t index = 0; index < target.length; ++index) {
        auto& element = target.elements[index];
        if (element.hash < 0) continue;
        --target.count;
        Replace(target, element, Deleted, nullptr, nullptr);
    }
}

}
