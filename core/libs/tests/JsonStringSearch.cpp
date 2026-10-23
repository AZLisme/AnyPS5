#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

struct String { void* text; };

extern "C" {
void APS5_VABI _ZN3sce4Json6StringC1Ev(String*);
void APS5_VABI _ZN3sce4Json6StringD1Ev(String*);
String* APS5_VABI _ZN3sce4Json6String6appendEPKcm(String*, const char*, std::size_t);
const char* APS5_VABI _ZNK3sce4Json6String5c_strEv(const String*);
std::size_t APS5_VABI _ZNK3sce4Json6String6lengthEv(const String*);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcm(const String*, const char*, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String4findEPKcmm(const String*, const char*, std::size_t, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String4findERKS1_m(const String*, const String*, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcm(const String*, const char*, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindEPKcmm(const String*, const char*, std::size_t, std::size_t);
std::size_t APS5_VABI _ZNK3sce4Json6String5rfindERKS1_m(const String*, const String*, std::size_t);
}

namespace {

constexpr std::size_t Missing = std::numeric_limits<std::size_t>::max();

struct OwnedString {
    String value{};
    explicit OwnedString(const std::string& text) {
        _ZN3sce4Json6StringC1Ev(&value);
        _ZN3sce4Json6String6appendEPKcm(&value, text.data(), text.size());
    }
    ~OwnedString() { _ZN3sce4Json6StringD1Ev(&value); }
    OwnedString(const OwnedString&) = delete;
    OwnedString& operator=(const OwnedString&) = delete;
};

std::size_t Search(std::string_view text, std::string_view needle, std::size_t position, bool reverse) {
    std::size_t result = Missing;
    for (std::size_t offset = 0; offset <= text.size(); ++offset) {
        if ((!reverse && offset < position) || (reverse && offset > position)) continue;
        if (needle.size() > text.size() - offset) continue;
        bool matches = true;
        for (std::size_t i = 0; i < needle.size(); ++i) {
            if (text[offset + i] != needle[i]) { matches = false; break; }
        }
        if (matches) {
            result = offset;
            if (!reverse) break;
        }
    }
    return result;
}

std::vector<std::string> Sequences(unsigned maximum) {
    std::vector<std::string> result{std::string{}};
    std::size_t begin = 0;
    std::size_t end = 1;
    for (unsigned length = 1; length <= maximum; ++length) {
        for (std::size_t i = begin; i < end; ++i) {
            for (char c : std::array<char, 3>{'a', 'b', '\0'}) result.push_back(result[i] + c);
        }
        begin = end;
        end = result.size();
    }
    return result;
}

bool Check(const std::string& text, const std::string& needle) {
    OwnedString haystack(text);
    OwnedString sequence(needle);
    const std::string_view terminated(needle.c_str());
    for (std::size_t position : std::array<std::size_t, 9>{0, 1, 2, 3, 4, 5, 6, Missing - 1, Missing}) {
        for (bool reverse : {false, true}) {
            const std::size_t expected = Search(text, needle, position, reverse);
            const std::size_t sized = reverse
                ? _ZNK3sce4Json6String5rfindEPKcmm(&haystack.value, needle.data(), position, needle.size())
                : _ZNK3sce4Json6String4findEPKcmm(&haystack.value, needle.data(), position, needle.size());
            const std::size_t object = reverse
                ? _ZNK3sce4Json6String5rfindERKS1_m(&haystack.value, &sequence.value, position)
                : _ZNK3sce4Json6String4findERKS1_m(&haystack.value, &sequence.value, position);
            const std::size_t cstring = reverse
                ? _ZNK3sce4Json6String5rfindEPKcm(&haystack.value, needle.c_str(), position)
                : _ZNK3sce4Json6String4findEPKcm(&haystack.value, needle.c_str(), position);
            if (sized != expected || object != expected || cstring != Search(text, terminated, position, reverse)) {
                std::fprintf(stderr, "substring search mismatch at position %zu, reverse %d\n", position, reverse);
                return false;
            }
        }
    }
    if (_ZNK3sce4Json6String6lengthEv(&haystack.value) != text.size()
        || !std::equal(text.begin(), text.end(), _ZNK3sce4Json6String5c_strEv(&haystack.value))) return false;
    for (std::size_t position : {std::size_t{0}, text.size(), Missing}) {
        const std::string_view prefix(text.data(), text.size() / 2);
        if (_ZNK3sce4Json6String4findEPKcmm(&haystack.value, text.data(), position, prefix.size()) != Search(text, prefix, position, false)
            || _ZNK3sce4Json6String5rfindEPKcmm(&haystack.value, text.data(), position, prefix.size()) != Search(text, prefix, position, true)
            || _ZNK3sce4Json6String4findERKS1_m(&haystack.value, &haystack.value, position) != Search(text, text, position, false)
            || _ZNK3sce4Json6String5rfindERKS1_m(&haystack.value, &haystack.value, position) != Search(text, text, position, true)) return false;
    }
    return true;
}

}

int main() {
    const auto texts = Sequences(4);
    const auto needles = Sequences(3);
    for (const auto& text : texts) {
        for (const auto& needle : needles) if (!Check(text, needle)) return 1;
    }
    const std::string bytes("\x80\xFF\0\x80\xFF", 5);
    if (!Check(bytes, std::string("\x80\xFF", 2))) return 1;
    if (!Check("\xE6\x97\xA5\xE6\x9C\xAC\xE6\x97\xA5", "\xE6\x97\xA5")) return 1;
    return 0;
}
