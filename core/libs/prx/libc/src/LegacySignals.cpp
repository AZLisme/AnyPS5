#include "prx/libc/include/LegacySignals.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <bit>

namespace {

std::atomic<LegacySigaction_nid_no_patch> signalAction{};
std::atomic<LegacySigprocmask_nid_no_patch> signalMask{};

int ChangeMask(int how, int mask, const char* function) {
    const auto forward = signalMask.load(std::memory_order_acquire);
    if (forward == nullptr) NotImplemented_nid_no_patch(function);
    const GuestSignalSet replacement{{static_cast<std::uint32_t>(mask), 0, 0, 0}};
    GuestSignalSet previous{};
    const int result = forward(how, &replacement, &previous);
    return result == 0 ? std::bit_cast<std::int32_t>(previous.bits[0]) : result;
}

}

extern "C" {

void RegisterLegacySignals_nid_no_patch(LegacySigaction_nid_no_patch action, LegacySigprocmask_nid_no_patch mask) {
    signalAction.store(action, std::memory_order_release);
    signalMask.store(mask, std::memory_order_release);
}

int APS5_VABI sigblock_nid_postfix(int mask) {
    return ChangeMask(1, mask, __func__);
}

int APS5_VABI sigsetmask_nid_postfix(int mask) {
    return ChangeMask(3, mask, __func__);
}

int APS5_VABI sigvec_nid_postfix(int signal, const GuestSignalVector* replacement, GuestSignalVector* previous) {
    const auto forward = signalAction.load(std::memory_order_acquire);
    if (forward == nullptr) NotImplemented_nid_no_patch(__func__);
    GuestSigaction action{};
    if (replacement != nullptr) {
        action.handler = replacement->handler;
        action.flags = replacement->flags ^ 2;
        action.mask.bits[0] = static_cast<std::uint32_t>(replacement->mask);
    }
    GuestSigaction old{};
    const int result = forward(signal, replacement == nullptr ? nullptr : &action, previous == nullptr ? nullptr : &old);
    if (result == 0 && previous != nullptr) {
        previous->handler = old.handler;
        previous->mask = std::bit_cast<std::int32_t>(old.mask.bits[0]);
        previous->flags = old.flags ^ 2;
    }
    return result;
}

}
