#include "prx/libSceIcu/GuestText.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>

extern "C" std::int32_t APS5_VABI u_strCompareIter_nid_postfix(Icu::Iterator*, Icu::Iterator*, std::int8_t);

namespace {

void Require(bool value) {
    if (!value) throw std::runtime_error("ICU iterator comparison regression");
}

struct Context {
    std::uint64_t before = 0x123456789abcdef0;
    std::vector<char16_t> text;
    unsigned moves = 0;
    unsigned reads = 0;
    std::uint64_t after = 0xfedcba9876543210;
};

std::int32_t APS5_VABI Move(Icu::Iterator* iterator, std::int32_t delta, std::int32_t origin) {
    auto& context = *const_cast<Context*>(static_cast<const Context*>(iterator->context));
    Require(origin == 0 && delta == 0);
    ++context.moves;
    return iterator->index = iterator->start;
}

std::int32_t APS5_VABI Current(Icu::Iterator* iterator) {
    auto& context = *const_cast<Context*>(static_cast<const Context*>(iterator->context));
    ++context.reads;
    return iterator->index < iterator->limit ? context.text[iterator->index] : -1;
}

std::int32_t APS5_VABI Next(Icu::Iterator* iterator) {
    const auto value = Current(iterator);
    if (value != -1) ++iterator->index;
    return value;
}

std::int32_t APS5_VABI Previous(Icu::Iterator* iterator) {
    if (iterator->index == iterator->start) return -1;
    --iterator->index;
    return Current(iterator);
}

std::int32_t APS5_VABI InstallNext(Icu::Iterator* iterator, std::int32_t delta, std::int32_t origin) {
    iterator->next = Next;
    return Move(iterator, delta, origin);
}

Icu::Iterator Make(Context& context) {
    Icu::Iterator result{};
    result.context = &context;
    result.length = result.limit = context.text.size();
    result.index = result.length;
    result.move = Move;
    result.current = Current;
    result.next = Next;
    result.previous = Previous;
    result.reservedField = 0x12345678;
    return result;
}

std::vector<std::int32_t> Scalars(const std::vector<char16_t>& input) {
    std::vector<std::int32_t> result;
    for (std::size_t i = 0; i < input.size(); ++i) {
        auto value = static_cast<std::int32_t>(input[i]);
        if (value >= 0xd800 && value <= 0xdbff && i + 1 < input.size() && input[i + 1] >= 0xdc00 && input[i + 1] <= 0xdfff) {
            value = ((value - 0xd800) << 10) + input[++i] - 0xdc00 + 0x10000;
        }
        result.push_back(value);
    }
    return result;
}

template<class T> int Order(const std::vector<T>& first, const std::vector<T>& second) {
    return std::lexicographical_compare(first.begin(), first.end(), second.begin(), second.end()) ? -1 : first == second ? 0 : 1;
}

void Check(const std::vector<char16_t>& first, const std::vector<char16_t>& second) {
    for (const auto scalarOrder : {false, true}) {
        Context left{}, right{};
        left.text = first;
        right.text = second;
        auto a = Make(left), b = Make(right);
        const auto value = u_strCompareIter_nid_postfix(&a, &b, scalarOrder);
        const auto expected = scalarOrder ? Order(Scalars(first), Scalars(second)) : Order(first, second);
        Require((value < 0 ? -1 : value > 0 ? 1 : 0) == expected);
        Require(left.moves == 1 && right.moves == 1 && left.reads != 0 && right.reads != 0);
        Require(a.index >= a.start && a.index <= a.limit && b.index >= b.start && b.index <= b.limit);
        Require(a.reservedField == 0x12345678 && b.reservedField == 0x12345678);
        Require(left.before == 0x123456789abcdef0 && left.after == 0xfedcba9876543210 && right.before == left.before && right.after == left.after);
    }
}

} // namespace

int main() {
    std::vector<std::vector<char16_t>> inputs{{}, {0}, {0x61}, {0xd800, 0xdc00}, {0xd800, 0xdc01}, {0x61, 0xd800, 0xdc00}, {0x61, 0xd800, 0xdc01}};
    for (const auto first : std::array<char16_t, 7>{0, 0x61, 0xd800, 0xdbff, 0xdc00, 0xdfff, 0xe000}) {
        inputs.push_back({first});
        for (const auto second : std::array<char16_t, 7>{0, 0x61, 0xd800, 0xdbff, 0xdc00, 0xdfff, 0xe000}) inputs.push_back({first, second});
    }
    for (const auto& first : inputs) for (const auto& second : inputs) Check(first, second);
    Icu::Iterator invalid{};
    Require(u_strCompareIter_nid_postfix(nullptr, &invalid, 1) == 0);
    Require(u_strCompareIter_nid_postfix(&invalid, nullptr, 1) == 0);
    Require(u_strCompareIter_nid_postfix(&invalid, &invalid, 1) == 0);
    Context left{}, right{};
    left.text = {0x7a, 0xd800, 0xdc00, 0x61};
    right.text = {0x7a, 0xe000, 0x61};
    auto a = Make(left), b = Make(right);
    a.start = b.start = 1;
    Require(u_strCompareIter_nid_postfix(&a, &b, 0) == -0x800);
    Require(u_strCompareIter_nid_postfix(&a, &b, 1) == 0x2000);
    left.text = right.text = {0x61, 0x62};
    a = Make(left);
    b = Make(right);
    a.move = b.move = InstallNext;
    a.next = b.next = nullptr;
    a.current = b.current = a.previous = b.previous = nullptr;
    Require(u_strCompareIter_nid_postfix(&a, &b, 1) == 0);
}
