#include "prx/libc/include/LegacySignals.hpp"
#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sigblock_nid_postfix(int);
int APS5_VABI sigsetmask_nid_postfix(int);
int APS5_VABI sigvec_nid_postfix(int, const GuestSignalVector*, GuestSignalVector*);
int APS5_VABI sigaction_nid_postfix(int, const GuestSigaction*, GuestSigaction*);
int APS5_VABI _sigprocmask_nid_postfix(int, const GuestSignalSet*, GuestSignalSet*);
int APS5_VABI raise_nid_postfix(int);
int* APS5_VABI __error_nid_postfix();
}

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "legacy signals: %s\n", message);
    std::abort();
}

GuestSignalSet Mask() {
    GuestSignalSet value{};
    Require(_sigprocmask_nid_postfix(0, nullptr, &value) == 0, "query mask");
    return value;
}

bool Equal(const GuestSignalSet& first, const GuestSignalSet& second) {
    for (int word = 0; word < 4; ++word) if (first.bits[word] != second.bits[word]) return false;
    return true;
}

bool Equal(const GuestSignalVector& first, const GuestSignalVector& second) {
    return first.handler == second.handler && first.mask == second.mask && first.flags == second.flags;
}

constexpr std::uint32_t unmaskable = (1u << 8) | (1u << 16);

void Masks() {
    const GuestSignalSet initial = Mask();
    Require(sigsetmask_nid_postfix(0) == std::bit_cast<std::int32_t>(initial.bits[0]), "set returns previous low word");
    for (unsigned bit = 0; bit < 32; ++bit) {
        const auto flag = std::uint32_t{1} << bit;
        Require(sigblock_nid_postfix(std::bit_cast<std::int32_t>(flag)) == 0, "block returns previous mask");
        Require(Mask().bits[0] == (flag & ~unmaskable), "each signed or unsigned bit and unblockable signal");
        Require(sigsetmask_nid_postfix(0) == std::bit_cast<std::int32_t>(flag & ~unmaskable), "old mask retains high bit");
    }
    const GuestSignalSet full{{0x80000001, 0x80000000, 0x12345678, 0x80000001}};
    Require(_sigprocmask_nid_postfix(3, &full, nullptr) == 0, "set extended mask");
    Require(sigblock_nid_postfix(0x40000002) == std::bit_cast<std::int32_t>(full.bits[0]), "block returns signed previous low word");
    GuestSignalSet combined = full;
    combined.bits[0] |= 0x40000002;
    Require(Equal(Mask(), combined), "block preserves upper mask words");
    Require(sigsetmask_nid_postfix(-1) == std::bit_cast<std::int32_t>(combined.bits[0]), "replace returns previous low word");
    Require(Equal(Mask(), GuestSignalSet{{~unmaskable, 0, 0, 0}}), "replace clears upper words");
    *__error_nid_postfix() = 73;
    Require(sigblock_nid_postfix(0) == std::bit_cast<std::int32_t>(~unmaskable), "query through block zero");
    Require(*__error_nid_postfix() == 73, "successful mask operation preserves errno");
    Require(_sigprocmask_nid_postfix(3, &initial, nullptr) == 0, "restore initial mask");
}

int received = 0;
int signalReceived = 0;

void APS5_VABI Handler(int signal) {
    ++received;
    signalReceived = signal;
}

void Vectors() {
    constexpr int signal = 30;
    const auto handler = reinterpret_cast<std::uintptr_t>(&Handler);
    GuestSigaction initial{};
    Require(sigaction_nid_postfix(signal, nullptr, &initial) == 0, "save original action");
    for (int flags = 0; flags < 128; ++flags) {
        const GuestSignalVector replacement{handler, std::bit_cast<std::int32_t>(0x80000005u), flags};
        Require(sigvec_nid_postfix(signal, &replacement, nullptr) == 0, "install each supported flag combination");
        GuestSigaction actual{};
        Require(sigaction_nid_postfix(signal, nullptr, &actual) == 0, "query modern action");
        Require(actual.handler == handler && actual.flags == (flags ^ 2), "legacy interrupt flag reverses restart flag");
        Require(Equal(actual.mask, GuestSignalSet{{0x80000005, 0, 0, 0}}), "legacy vector zero extends mask");
        GuestSignalVector queried{};
        Require(sigvec_nid_postfix(signal, nullptr, &queried) == 0 && Equal(queried, replacement), "query converts flags and mask back");
    }
    const GuestSigaction extended{handler, 0x13, {{0x87654321, 0x12345678, 0x80000001, 0x7fffffff}}};
    Require(sigaction_nid_postfix(signal, &extended, nullptr) == 0, "install action with extended mask");
    GuestSignalVector queried{};
    *__error_nid_postfix() = 81;
    Require(sigvec_nid_postfix(signal, nullptr, &queried) == 0, "query extended action");
    Require(Equal(queried, GuestSignalVector{handler, std::bit_cast<std::int32_t>(0x87654321u), 0x11}), "query truncates modern mask to low word");
    Require(*__error_nid_postfix() == 81, "successful vector preserves errno");
    GuestSignalVector alias{handler, 7, 0};
    Require(sigvec_nid_postfix(signal, &alias, &alias) == 0 && Equal(alias, queried), "aliased input and output returns old action");
    GuestSignalVector newState{};
    Require(sigvec_nid_postfix(signal, nullptr, &newState) == 0 && Equal(newState, GuestSignalVector{handler, 7, 0}), "alias installs original input");
    const GuestSignalVector sentinel{0x12345678, -123, 0x345};
    const GuestSignalVector invalid{handler, -1, 0x80};
    for (const int number : std::array{-1, 0, 129, signal}) {
        GuestSignalVector output = sentinel;
        *__error_nid_postfix() = 0;
        Require(sigvec_nid_postfix(number, &invalid, &output) == -1, "invalid signal or flags fail");
        Require(*__error_nid_postfix() == 22 && Equal(output, sentinel), "failure preserves previous output and exposes errno");
    }
    Require(sigvec_nid_postfix(signal, nullptr, &queried) == 0 && Equal(queried, newState), "failed update leaves disposition unchanged");
    for (const int number : std::array{9, 17}) {
        GuestSignalVector output = sentinel;
        Require(sigvec_nid_postfix(number, &newState, &output) == -1 && Equal(output, sentinel), "uninstallable handlers preserve output");
    }
    Require(sigvec_nid_postfix(signal, nullptr, nullptr) == 0, "null input and output query is accepted");
    const GuestSignalSet saved = Mask();
    Require(sigsetmask_nid_postfix(0) == std::bit_cast<std::int32_t>(saved.bits[0]), "clear mask for delivery");
    received = 0;
    Require(sigblock_nid_postfix(1 << (signal - 1)) == 0, "block installed handler");
    Require(raise_nid_postfix(signal) == 0 && received == 0, "blocked signal becomes pending");
    Require(sigsetmask_nid_postfix(0) == (1 << (signal - 1)), "unblock returns prior mask");
    Require(received == 1 && signalReceived == signal, "unblock delivers pending signal through guest handler");
    Require(raise_nid_postfix(signal) == 0 && received == 2, "unblocked delivery retains vector handler");
    Require(sigaction_nid_postfix(signal, &initial, nullptr) == 0, "restore disposition");
    Require(_sigprocmask_nid_postfix(3, &saved, nullptr) == 0, "restore mask after delivery");
}

void MissingBackend() {
    const auto saved = Mask();
    const GuestSignalVector sentinel{0x12345678, -123, 0x345};
    GuestSignalVector output = sentinel;
    const GuestSignalVector replacement{reinterpret_cast<std::uintptr_t>(&Handler), 7, 0};
    RegisterLegacySignals_nid_no_patch(nullptr, nullptr);
    for (int operation = 0; operation < 3; ++operation) {
        bool threw = false;
        try {
            if (operation == 0) sigblock_nid_postfix(7);
            else if (operation == 1) sigsetmask_nid_postfix(7);
            else sigvec_nid_postfix(30, &replacement, &output);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, "missing backend throws");
        Require(Equal(output, sentinel) && Equal(Mask(), saved), "missing backend leaves output and masks untouched");
    }
    RegisterLegacySignals_nid_no_patch(sigaction_nid_postfix, _sigprocmask_nid_postfix);
}

}

int main() {
    Masks();
    Vectors();
    MissingBackend();
}
