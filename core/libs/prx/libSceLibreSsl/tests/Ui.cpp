#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <stdexcept>
#include <unordered_map>
#include <exception>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

struct TestUi { int value; };
struct TestUiString { int value; };

std::unordered_map<void*, std::size_t> owned;
bool failMethod = false;
bool failName = false;

void* APS5_VABI Allocate(std::size_t size) {
    if (failName) { failName = false; return nullptr; }
    auto* pointer = std::malloc(size);
    if (pointer) owned.emplace(pointer, size);
    return pointer;
}
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    if (failMethod) { failMethod = false; return nullptr; }
    auto* pointer = std::calloc(count, size);
    if (pointer) owned.emplace(pointer, count * size);
    return pointer;
}
void APS5_VABI Free(void* pointer) {
    if (!pointer) return;
    if (owned.erase(pointer) != 1) std::abort();
    std::free(pointer);
}
void* APS5_VABI UnexpectedRealloc(void*, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedAlign(std::size_t, std::size_t) { std::abort(); }
void* APS5_VABI UnexpectedRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI UnexpectedPosixAlign(void**, std::size_t, std::size_t) { std::abort(); }

void InstallAllocator() {
    void* api[10]{reinterpret_cast<void*>(Allocate), reinterpret_cast<void*>(Free), reinterpret_cast<void*>(Calloc),
        reinterpret_cast<void*>(UnexpectedRealloc), reinterpret_cast<void*>(UnexpectedAlign), reinterpret_cast<void*>(UnexpectedRealign),
        reinterpret_cast<void*>(UnexpectedPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api);
}

int invocations = 0;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL UI: %s\n", message);
    std::abort();
}

int APS5_VABI SessionFirst(LibreSslUi* ui) { ++invocations; return reinterpret_cast<TestUi*>(ui)->value + 7; }
int APS5_VABI SessionSecond(LibreSslUi* ui) { ++invocations; return -reinterpret_cast<TestUi*>(ui)->value; }
int APS5_VABI StringFirst(LibreSslUi* ui, LibreSslUiString* text) { ++invocations; return reinterpret_cast<TestUi*>(ui)->value - reinterpret_cast<TestUiString*>(text)->value; }
int APS5_VABI StringSecond(LibreSslUi* ui, LibreSslUiString* text) { ++invocations; return reinterpret_cast<TestUi*>(ui)->value + 2 * reinterpret_cast<TestUiString*>(text)->value; }
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
    TestUi testUi{83};
    auto& ui = *reinterpret_cast<LibreSslUi*>(&testUi);
    TestUiString testText{19};
    auto& text = *reinterpret_cast<LibreSslUiString*>(&testText);
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

void Lifecycle() {
    Require(UI_method_get_opener_nid_postfix(nullptr) == nullptr, "null opener getter");
    Require(UI_method_set_opener_nid_postfix(nullptr, SessionFirst) == -1, "null opener setter");
    Require(UI_method_set_writer_nid_postfix(nullptr, StringFirst) == -1, "null writer setter");
    for (const bool withName : {false, true}) {
        char name[] = "owned method";
        auto* method = UI_create_method_nid_postfix(withName ? name : nullptr);
        Require(method && (method->name != nullptr) == withName, "method allocation or optional name");
        Require(!method->opener && !method->writer && !method->reader && !method->flusher && !method->closer && !method->promptConstructor, "method slots were not cleared");
        if (withName) {
            Require(method->name != name && std::strcmp(method->name, name) == 0, "method name was not copied");
            name[0] = 'X';
            Require(method->name[0] == 'o', "method name retained caller storage");
        }
        const auto before = invocations;
        Require(UI_method_set_opener_nid_postfix(method, SessionFirst) == 0 && UI_method_get_opener_nid_postfix(method) == SessionFirst, "opener roundtrip");
        Require(UI_method_set_writer_nid_postfix(method, StringSecond) == 0 && UI_method_get_writer_nid_postfix(method) == StringSecond, "writer roundtrip");
        Require(invocations == before, "method setup called callbacks");
        TestUi ui{83}; TestUiString text{19};
        Require(UI_method_get_opener_nid_postfix(method)(reinterpret_cast<LibreSslUi*>(&ui)) == 90, "opener callback invocation");
        Require(UI_method_get_writer_nid_postfix(method)(reinterpret_cast<LibreSslUi*>(&ui), reinterpret_cast<LibreSslUiString*>(&text)) == 121, "writer callback invocation");
        Require(UI_method_set_opener_nid_postfix(method, nullptr) == 0 && UI_method_set_writer_nid_postfix(method, nullptr) == 0 && !method->opener && !method->writer, "callback clearing");
        UI_destroy_method_nid_postfix(method);
        Require(owned.empty(), "method destruction leaked name or object");
    }
    char name[] = "failure";
    failMethod = true;
    Require(!UI_create_method_nid_postfix(name) && owned.empty(), "method allocation failure");
    failName = true;
    auto* method = UI_create_method_nid_postfix(name);
    Require(method && !method->name && owned.size() == 1, "name failure discarded usable method");
    Require(UI_method_set_opener_nid_postfix(method, SessionFirst) == 0, "method remained unusable after name failure");
    UI_destroy_method_nid_postfix(method);
    Require(owned.empty(), "name failure cleanup");
    bool rejected = false;
    try { UI_destroy_method_nid_postfix(nullptr); } catch (const std::exception&) { rejected = true; }
    Require(rejected, "null destruction did not throw");
}

} // namespace

int main() {
    InstallAllocator();
    NullMethods();
    RoundTripCallbacks();
    Lifecycle();
    return 0;
}
