#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct LibreSslUi { int value; };
struct LibreSslUiString { int value; };
using UiOpener = int (APS5_VABI *)(LibreSslUi*);
using UiWriter = int (APS5_VABI *)(LibreSslUi*, LibreSslUiString*);

struct LibreSslUiMethod {
    char* name;
    UiOpener opener;
    UiWriter writer;
    UiOpener flusher;
    UiWriter reader;
    UiOpener closer;
    char* (APS5_VABI *promptConstructor)(LibreSslUi*, const char*, const char*);
};

static_assert(sizeof(LibreSslUiMethod) == 56);
static_assert(offsetof(LibreSslUiMethod, reader) == 32);
static_assert(offsetof(LibreSslUiMethod, closer) == 40);

extern "C" {
UiOpener APS5_VABI UI_method_get_closer_nid_postfix(LibreSslUiMethod*);
UiWriter APS5_VABI UI_method_get_reader_nid_postfix(LibreSslUiMethod*);
UiWriter APS5_VABI UI_method_get_writer_nid_postfix(LibreSslUiMethod*);
int APS5_VABI UI_method_set_closer_nid_postfix(LibreSslUiMethod*, UiOpener);
int APS5_VABI UI_method_set_reader_nid_postfix(LibreSslUiMethod*, UiWriter);
}

namespace {

int invocations = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL UI: %s\n", message);
    std::abort();
}

int APS5_VABI SessionFirst(LibreSslUi* ui) { ++invocations; return ui->value + 7; }
int APS5_VABI SessionSecond(LibreSslUi* ui) { ++invocations; return -ui->value; }
int APS5_VABI StringFirst(LibreSslUi* ui, LibreSslUiString* text) { ++invocations; return ui->value - text->value; }
int APS5_VABI StringSecond(LibreSslUi* ui, LibreSslUiString* text) { ++invocations; return ui->value + 2 * text->value; }
char* APS5_VABI Prompt(LibreSslUi*, const char* text, const char*) { ++invocations; return const_cast<char*>(text); }

struct GuardedMethod {
    std::uint64_t before;
    LibreSslUiMethod method;
    std::uint64_t after;
};

void CheckPreserved(const GuardedMethod& actual, const GuardedMethod& expected) {
    Require(std::memcmp(&actual, &expected, sizeof(actual)) == 0, "unrelated method bytes changed");
}

void NullMethods() {
    Require(UI_method_get_closer_nid_postfix(nullptr) == nullptr, "null closer getter");
    Require(UI_method_get_reader_nid_postfix(nullptr) == nullptr, "null reader getter");
    Require(UI_method_get_writer_nid_postfix(nullptr) == nullptr, "null writer getter");
    Require(UI_method_set_closer_nid_postfix(nullptr, SessionFirst) == -1, "null closer setter");
    Require(UI_method_set_reader_nid_postfix(nullptr, StringFirst) == -1, "null reader setter");
    Require(UI_method_set_closer_nid_postfix(nullptr, nullptr) == -1, "null closer clearing");
    Require(UI_method_set_reader_nid_postfix(nullptr, nullptr) == -1, "null reader clearing");
    Require(invocations == 0, "null method invoked a callback");
}

void RoundTripCallbacks() {
    constexpr std::array<UiOpener, 3> sessions{nullptr, SessionFirst, SessionSecond};
    constexpr std::array<UiWriter, 3> strings{nullptr, StringFirst, StringSecond};
    char name[] = "guest method";
    LibreSslUi ui{83};
    LibreSslUiString text{19};
    for (const auto initialReader : strings) {
        for (const auto initialWriter : strings) {
            for (const auto initialCloser : sessions) {
                for (const auto nextReader : strings) {
                    for (const auto nextCloser : sessions) {
                        GuardedMethod guarded{0x123456789abcdef0, {name, SessionSecond, initialWriter, SessionFirst, initialReader, initialCloser, Prompt}, 0xfedcba9876543210};
                        auto expected = guarded;
                        const auto callsBefore = invocations;
                        Require(UI_method_get_closer_nid_postfix(&guarded.method) == initialCloser, "initial closer identity");
                        Require(UI_method_get_reader_nid_postfix(&guarded.method) == initialReader, "initial reader identity");
                        Require(UI_method_get_writer_nid_postfix(&guarded.method) == initialWriter, "initial writer identity");
                        CheckPreserved(guarded, expected);
                        Require(UI_method_set_reader_nid_postfix(&guarded.method, nextReader) == 0, "reader replacement");
                        expected.method.reader = nextReader;
                        CheckPreserved(guarded, expected);
                        Require(UI_method_set_closer_nid_postfix(&guarded.method, nextCloser) == 0, "closer replacement");
                        expected.method.closer = nextCloser;
                        CheckPreserved(guarded, expected);
                        const auto reader = UI_method_get_reader_nid_postfix(&guarded.method);
                        const auto writer = UI_method_get_writer_nid_postfix(&guarded.method);
                        const auto closer = UI_method_get_closer_nid_postfix(&guarded.method);
                        Require(reader == nextReader && writer == initialWriter && closer == nextCloser, "replacement callback identities");
                        Require(invocations == callsBefore, "registration or retrieval invoked callback");
                        if (reader) Require(reader(&ui, &text) == (reader == StringFirst ? 64 : 121), "reader callback arguments and result");
                        if (writer) Require(writer(&ui, &text) == (writer == StringFirst ? 64 : 121), "writer callback arguments and result");
                        if (closer) Require(closer(&ui) == (closer == SessionFirst ? 90 : -83), "closer callback argument and result");
                        CheckPreserved(guarded, expected);
                    }
                }
            }
        }
    }
}

} // namespace

int main() {
    NullMethods();
    RoundTripCallbacks();
    return 0;
}
