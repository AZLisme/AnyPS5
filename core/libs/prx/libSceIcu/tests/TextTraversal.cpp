#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>



namespace {

void Require(bool value) {
    if (!value) throw std::runtime_error("ICU provider traversal regression");
}

bool Pair(char16_t first, char16_t second) { return first >= 0xd800 && first <= 0xdbff && second >= 0xdc00 && second <= 0xdfff; }

struct Provider {
    std::vector<char16_t> text;
    std::vector<char16_t> buffer;
    std::int64_t prefix = 0;
    std::int64_t stride = 1;
    std::int32_t span = 1;
    bool split = false;
    unsigned accesses = 0;
    unsigned maps = 0;
    std::int64_t Size() const { return prefix + static_cast<std::int64_t>(text.size()); }
    char16_t At(std::int64_t position) const { return position < prefix ? u'a' : text[position - prefix]; }
    bool Inside(std::int64_t position) const { return position > 0 && position < Size() && Pair(At(position - 1), At(position)); }
};

std::int8_t APS5_VABI Access(Icu::Text* text, std::int64_t native, std::int8_t forward) {
    auto& provider = *const_cast<Provider*>(static_cast<const Provider*>(text->context));
    ++provider.accesses;
    const auto unit = std::clamp<std::int64_t>(native / provider.stride, 0, provider.Size());
    std::int64_t first, end;
    if ((forward && unit < provider.Size()) || unit == 0) {
        first = unit;
        if (!provider.split && provider.Inside(first)) --first;
        end = std::min(provider.Size(), first + provider.span);
        if (!provider.split && provider.Inside(end)) ++end;
    } else {
        end = unit;
        if (!provider.split && provider.Inside(end)) ++end;
        first = std::max<std::int64_t>(0, end - provider.span - (forward ? 0 : 1));
        if (!provider.split && provider.Inside(first)) --first;
    }
    provider.buffer.clear();
    for (auto i = first; i < end; ++i) provider.buffer.push_back(provider.At(i));
    text->chunkContents = provider.buffer.data();
    text->chunkNativeStart = first * provider.stride;
    text->chunkNativeLimit = end * provider.stride;
    text->chunkLength = end - first;
    text->chunkOffset = unit - first;
    text->nativeIndexingLimit = provider.stride == 1 ? text->chunkLength : 0;
    return forward ? unit < provider.Size() : unit > 0;
}

std::int32_t APS5_VABI Map(const Icu::Text* text, std::int64_t native) {
    auto& provider = *const_cast<Provider*>(static_cast<const Provider*>(text->context));
    ++provider.maps;
    return (native - text->chunkNativeStart) / provider.stride;
}

const Icu::TextFunctions Functions = [] {
    Icu::TextFunctions table{};
    table.tableSize = sizeof(table);
    table.access = Access;
    table.mapNativeIndexToUTF16 = Map;
    return table;
}();

Icu::Text Make(Provider& provider) {
    Icu::Text text{};
    text.magic = 0x345ad82c;
    text.sizeOfStruct = sizeof(text);
    text.pFuncs = &Functions;
    text.context = &provider;
    text.privA = 0x123456789abcdef;
    text.privB = 0x12345678;
    Access(&text, 0, true);
    return text;
}

std::int64_t Native(const Icu::Text& text, const Provider& provider) { return text.chunkNativeStart + text.chunkOffset * provider.stride; }

std::int32_t Expected(const Provider& provider, std::int64_t unit) {
    if (unit == provider.Size()) return -1;
    const auto value = provider.At(unit);
    if (unit + 1 < provider.Size() && Pair(value, provider.At(unit + 1))) return ((value - 0xd800) << 10) + provider.At(unit + 1) - 0xdc00 + 0x10000;
    return value;
}

void State(const Icu::Text& text, const Provider& provider, std::int64_t expected) {
    Require(Native(text, provider) == expected * provider.stride);
    Require(text.privA == 0x123456789abcdef && text.privB == 0x12345678 && text.context == &provider && text.pFuncs == &Functions);
    Require(text.chunkOffset >= 0 && text.chunkOffset <= text.chunkLength);
}

void Sequential(Provider provider) {
    const auto original = provider.text;
    auto text = Make(provider);
    std::int64_t position = 0;
    while (position < provider.Size()) {
        const auto value = Expected(provider, position);
        Require(utext_current32_nid_postfix(&text) == value);
        State(text, provider, position);
        Require(utext_next32_nid_postfix(&text) == value);
        position += value > 0xffff ? 2 : 1;
        State(text, provider, position);
    }
    Require(utext_current32_nid_postfix(&text) == -1 && utext_next32_nid_postfix(&text) == -1);
    while (position != 0) {
        --position;
        if (position > 0 && Pair(provider.At(position - 1), provider.At(position))) --position;
        Require(utext_previous32_nid_postfix(&text) == Expected(provider, position));
        State(text, provider, position);
    }
    Require(utext_previous32_nid_postfix(&text) == -1 && provider.text == original && provider.accesses > 1);
}

void RandomAccess(Provider provider) {
    auto text = Make(provider);
    for (auto native = provider.prefix * provider.stride - 3; native <= provider.Size() * provider.stride + 3; ++native) {
        auto unit = std::clamp<std::int64_t>(native / provider.stride, 0, provider.Size());
        if (provider.Inside(unit)) --unit;
        const auto value = Expected(provider, unit);
        Require(utext_next32From_nid_postfix(&text, native) == value);
        State(text, provider, unit + (value > 0xffff ? 2 : value == -1 ? 0 : 1));
        const auto before = unit == 0 ? -1 : provider.Inside(unit - 1) ? unit - 2 : unit - 1;
        Require(utext_previous32From_nid_postfix(&text, native) == (before < 0 ? -1 : Expected(provider, before)));
        State(text, provider, std::max<std::int64_t>(0, before));
    }
    if (provider.stride != 1 && provider.span > 1) Require(provider.maps != 0);
}

unsigned wideMaps = 0;

std::int32_t APS5_VABI WideMap(const Icu::Text*, std::int64_t index) {
    ++wideMaps;
    return index >= (1ll << 32) ? 2 : 0;
}

std::int8_t APS5_VABI EmptyAccess(Icu::Text* text, std::int64_t, std::int8_t) {
    text->chunkNativeStart = text->chunkNativeLimit = 0;
    text->chunkLength = text->chunkOffset = 0;
    return 1;
}

void SourceBoundaries() {
    const std::array<char16_t, 3> data{0xd800, 0xdc00, 0xd800};
    Icu::TextFunctions functions{};
    functions.tableSize = sizeof(functions);
    functions.mapNativeIndexToUTF16 = WideMap;
    Icu::Text text{};
    text.chunkContents = data.data();
    text.chunkLength = data.size();
    text.chunkNativeLimit = 1ll << 34;
    text.pFuncs = &functions;
    Require(utext_next32From_nid_postfix(&text, 1ll << 32) == 0x10000 && text.chunkOffset == 2 && wideMaps == 1);
    functions.access = EmptyAccess;
    text.chunkLength = text.chunkOffset = 0;
    text.chunkNativeLimit = 0;
    bool rejected = false;
    try { utext_previous32From_nid_postfix(&text, 1); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected && data == std::array<char16_t, 3>{0xd800, 0xdc00, 0xd800});
}

} // namespace

int main() {
    for (const auto span : {1, 2, 3, 8, 32}) for (const auto stride : {1, 3}) {
        Provider provider{{0x41, 0xd800, 0xdc00, 0x42, 0xdc00, 0xd800, 0x43, 0xd83d, 0xde00, 0, 0x5a, 0xd800}};
        provider.span = span;
        provider.stride = stride;
        Sequential(provider);
        RandomAccess(provider);
        provider.prefix = 1ll << 34;
        RandomAccess(provider);
        provider.prefix = 0;
        provider.split = true;
        Sequential(provider);
    }
    Provider empty;
    auto text = Make(empty);
    Require(utext_current32_nid_postfix(&text) == -1 && utext_next32_nid_postfix(&text) == -1 && utext_previous32_nid_postfix(&text) == -1);
    Require(utext_next32From_nid_postfix(&text, -1) == -1 && utext_previous32From_nid_postfix(&text, 10) == -1);
    SourceBoundaries();
}
