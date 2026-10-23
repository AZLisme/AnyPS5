#include "prx/libSceJson2/libSceJson2.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

using namespace SceJson2;

static void Require(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr, "Json array mutation: %s\n", message); std::abort(); }
}

struct TestValue {
    Value value{};
    TestValue() { _ZN3sce4Json5ValueC1Ev(&value); }
    ~TestValue() { _ZN3sce4Json5ValueD1Ev(&value); }
};
struct TestArray {
    Array value{};
    TestArray() { _ZN3sce4Json5ArrayC1Ev(&value); }
    ~TestArray() { _ZN3sce4Json5ArrayD1Ev(&value); }
};
struct TestIterator {
    ArrayIterator value{};
    TestIterator(const Array& array, std::size_t index) {
        _ZNK3sce4Json5Array5beginEv(&value, &array);
        _ZN3sce4Json5Array8iterator7advanceEm(&value, index);
    }
    ~TestIterator() { _ZN3sce4Json5Array8iteratorD1Ev(&value); }
};

static std::string Serialized(const Array& array) {
    TestValue copy;
    _ZN3sce4Json5ValueD1Ev(&copy.value);
    _ZN3sce4Json5ValueC1ERKNS0_5ArrayE(&copy.value, &array);
    String text{};
    _ZN3sce4Json6StringC1Ev(&text);
    Require(_ZN3sce4Json5Value9serializeERNS0_6StringE(&copy.value, &text) == 0, "serialize array");
    std::string result(_ZNK3sce4Json6String5c_strEv(&text));
    _ZN3sce4Json6StringD1Ev(&text);
    return result;
}

static void Parse(TestValue& value, const char* text) {
    Require(_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm(&value.value, text, std::strlen(text)) == 0, "parse fixture");
}

static void Fill(Array& array, std::size_t count) {
    TestValue number;
    for (std::size_t index = 0; index < count; ++index) {
        _ZN3sce4Json5Value3setEl(&number.value, static_cast<std::int64_t>(index));
        _ZN3sce4Json5Array9push_backERKNS0_5ValueE(&array, &number.value);
    }
}

static void Check(const Array& array, const std::vector<std::int64_t>& expected) {
    Require(_ZNK3sce4Json5Array4sizeEv(&array) == expected.size(), "oracle size");
    for (std::size_t index = 0; index < expected.size(); ++index) {
        TestIterator iterator(array, index);
        Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array8iteratordeEv(&iterator.value)) == expected[index], "oracle order");
    }
}

static void InsertPositions() {
    constexpr std::uint64_t guard = 0x123456789abcdef0;
    for (std::size_t size = 0; size <= 6; ++size) {
        for (std::size_t position = 0; position <= size; ++position) {
            TestArray array;
            Fill(array.value, size);
            TestIterator where(array.value, position);
            TestValue inserted;
            _ZN3sce4Json5Value3setEl(&inserted.value, -42);
            struct Result { ArrayIterator iterator{}; std::uint64_t canary = guard; } result;
            Require(_ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&result.iterator, &array.value, &where.value, &inserted.value) == &result.iterator, "insert result pointer");
            Require(result.canary == guard && result.iterator.state != where.value.state, "insert owned result and canary");
            _ZN3sce4Json5Value3setEl(&inserted.value, 99);
            Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array8iteratordeEv(&result.iterator)) == -42, "insert deep copy");
            std::vector<std::int64_t> expected;
            for (std::size_t index = 0; index < size; ++index) expected.push_back(static_cast<std::int64_t>(index));
            expected.insert(expected.begin() + static_cast<std::ptrdiff_t>(position), -42);
            Check(array.value, expected);
            if (position < size) Require(*_ZNK3sce4Json5Value10getIntegerEv(_ZNK3sce4Json5Array8iteratordeEv(&where.value)) == static_cast<std::int64_t>(position), "insert preserves position iterator");
            _ZN3sce4Json5Array8iteratorD1Ev(&result.iterator);
        }
    }
}

static void ErasePositions() {
    for (std::size_t size = 1; size <= 6; ++size) {
        for (std::size_t position = 0; position < size; ++position) {
            TestArray array;
            Fill(array.value, size);
            TestIterator where(array.value, position);
            TestIterator following(array.value, position + 1);
            ArrayIterator result{};
            Require(_ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&result, &array.value, &where.value) == &result, "erase result pointer");
            Require(!_ZNK3sce4Json5Array8iteratorneERKS2_(&result, &following.value), "erase returns following iterator");
            std::vector<std::int64_t> expected;
            for (std::size_t index = 0; index < size; ++index) if (index != position) expected.push_back(static_cast<std::int64_t>(index));
            Check(array.value, expected);
            _ZN3sce4Json5Array8iteratorD1Ev(&result);
        }
        TestArray array;
        Fill(array.value, size);
        for (std::size_t remaining = size; remaining != 0; --remaining) {
            _ZN3sce4Json5Array8pop_backEv(&array.value);
            std::vector<std::int64_t> expected;
            for (std::size_t index = 0; index < remaining - 1; ++index) expected.push_back(static_cast<std::int64_t>(index));
            Check(array.value, expected);
        }
    }
}

static void AliasingAndAssignment() {
    TestValue source;
    Parse(source, "[1,{\"nested\":[\"retained\",false]},3]");
    auto* array = _ZN3sce4Json5Value10referArrayEv(&source.value);
    TestIterator where(*array, 1);
    ArrayIterator result{};
    _ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&result, array, &where.value, _ZNK3sce4Json5Array8iteratordeEv(&where.value));
    _ZN3sce4Json5Value3setEPKc(_ZNK3sce4Json5Array8iteratordeEv(&where.value), "changed");
    Require(Serialized(*array) == "[1,{\"nested\":[\"retained\",false]},\"changed\",3]", "aliased nested insert is independent");
    TestArray destination;
    Fill(destination.value, 4);
    Require(_ZN3sce4Json5ArrayaSERKS1_(&destination.value, array) == &destination.value, "assignment returns destination");
    Require(Serialized(destination.value) == Serialized(*array), "assignment copies complete nested array");
    const void* storage = destination.value.items;
    Require(_ZN3sce4Json5ArrayaSERKS1_(&destination.value, &destination.value) == &destination.value && destination.value.items == storage, "self assignment preserves storage");
    _ZN3sce4Json5Array8pop_backEv(array);
    Require(_ZNK3sce4Json5Array4sizeEv(&destination.value) == 4, "assignment is independent");
    TestArray empty;
    _ZN3sce4Json5ArrayaSERKS1_(&destination.value, &empty.value);
    Require(Serialized(destination.value) == "[]", "assignment from empty array");
    _ZN3sce4Json5Array8iteratorD1Ev(&result);
    TestValue nested;
    Parse(nested, "[[\"nested\"],\"discarded\"]");
    auto* outer = _ZN3sce4Json5Value10referArrayEv(&nested.value);
    TestIterator first(*outer, 0);
    const auto* inner = _ZNK3sce4Json5Value8getArrayEv(_ZNK3sce4Json5Array8iteratordeEv(&first.value));
    _ZN3sce4Json5ArrayaSERKS1_(outer, inner);
    Require(Serialized(*outer) == "[\"nested\"]", "assignment from contained array survives source destruction");
}

template<typename TCall>
static void Unsupported(TCall call) {
    bool threw = false;
    try { call(); } catch (const std::runtime_error&) { threw = true; }
    Require(threw, "unknown invalid-input behavior must throw");
}

static void IteratorAssignments() {
    TestArray array;
    Fill(array.value, 2);
    TestIterator position(array.value, 0), begin(array.value, 0), end(array.value, 2);
    Require(_ZN3sce4Json5Array8iteratoraSERKS2_(&position.value, &end.value) == &position.value, "end assignment returns iterator");
    TestValue number;
    _ZN3sce4Json5Value3setEl(&number.value, 7);
    ArrayIterator result{};
    _ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&result, &array.value, &position.value, &number.value);
    Check(array.value, {0, 1, 7});
    _ZN3sce4Json5Array8iteratorD1Ev(&result);
    ArrayIterator copiedEnd{};
    _ZN3sce4Json5Array8iteratorC1ERKS2_(&copiedEnd, &end.value);
    _ZN3sce4Json5Value3setEl(&number.value, 9);
    _ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&result, &array.value, &copiedEnd, &number.value);
    Check(array.value, {0, 1, 7, 9});
    _ZN3sce4Json5Array8iteratorD1Ev(&result);
    _ZN3sce4Json5Array8iteratorD1Ev(&copiedEnd);
    ArrayIterator defaultIterator{};
    _ZN3sce4Json5Array8iteratorC1Ev(&defaultIterator);
    _ZN3sce4Json5Array8iteratoraSERKS2_(&position.value, &defaultIterator);
    Unsupported([&] { _ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&result, &array.value, &position.value); });
    _ZN3sce4Json5Array8iteratoraSERKS2_(&position.value, &begin.value);
    _ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&result, &array.value, &position.value);
    Check(array.value, {1, 7, 9});
    _ZN3sce4Json5Array8iteratorD1Ev(&result);
    TestIterator updatedBegin(array.value, 0);
    ArrayIterator restored{};
    _ZN3sce4Json5Array8iteratoraSERKS2_(&restored, &updatedBegin.value);
    _ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&result, &array.value, &restored);
    Check(array.value, {7, 9});
    _ZN3sce4Json5Array8iteratorD1Ev(&result);
    _ZN3sce4Json5Array8iteratorD1Ev(&restored);
    _ZN3sce4Json5Array8iteratorD1Ev(&defaultIterator);
    const void* retained = end.value.state;
    _ZN3sce4Json5Array8iteratoraSERKS2_(&end.value, &end.value);
    Require(end.value.state == retained, "self assignment preserves end state");
}

static void InvalidPositions() {
    TestArray array, foreign;
    Fill(array.value, 2);
    Fill(foreign.value, 2);
    TestValue value;
    TestIterator end(array.value, 2), other(foreign.value, 0);
    ArrayIterator defaultIterator{};
    _ZN3sce4Json5Array8iteratorC1Ev(&defaultIterator);
    ArrayIterator result{reinterpret_cast<ArrayIteratorState*>(std::uintptr_t{0x1234})};
    const auto original = Serialized(array.value);
    Unsupported([&] { _ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&result, &array.value, &end.value); });
    for (const auto* invalid : {&other.value, &defaultIterator}) {
        Unsupported([&] { _ZN3sce4Json5Array5eraseERKNS1_8iteratorE(&result, &array.value, invalid); });
        Unsupported([&] { _ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&result, &array.value, invalid, &value.value); });
    }
    Unsupported([&] { _ZN3sce4Json5Array6insertERKNS1_8iteratorERKNS0_5ValueE(&end.value, &array.value, &end.value, &value.value); });
    Require(result.state == reinterpret_cast<ArrayIteratorState*>(std::uintptr_t{0x1234}) && Serialized(array.value) == original, "invalid operation preserves result and array");
    _ZN3sce4Json5Array8iteratorD1Ev(&defaultIterator);
    TestArray empty;
    Unsupported([&] { _ZN3sce4Json5Array8pop_backEv(&empty.value); });
    Require(Serialized(empty.value) == "[]", "empty pop preserves array");
}

int main() {
    InsertPositions();
    ErasePositions();
    AliasingAndAssignment();
    IteratorAssignments();
    InvalidPositions();
}
