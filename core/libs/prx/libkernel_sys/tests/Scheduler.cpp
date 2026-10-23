#include "SceTypes.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>

extern "C" {
int* APS5_VABI __error_nid_postfix();
int APS5_VABI getpid_nid_postfix();
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadCreate(Pthread*, const PthreadAttr*, PthreadEntry, void*, const char*);
int APS5_VABI scePthreadJoin(Pthread, void**);
int APS5_VABI scePthreadGetschedparam(Pthread, int*, KernelSchedParam*);
int APS5_VABI sched_setscheduler_nid_postfix(int, int, const KernelSchedParam*);
}

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "sched_setscheduler check failed at line %u (errno %d)\n", location.line(), *__error_nid_postfix());
    std::abort();
}

static void RequireState(int policy, int priority) {
    int actualPolicy = -1;
    KernelSchedParam actual{};
    Require(scePthreadGetschedparam(scePthreadSelf(), &actualPolicy, &actual) == 0);
    Require(actualPolicy == policy && actual.sched_priority == priority);
}

static void* APS5_VABI Inherited(void* argument) {
    const auto& expected = *static_cast<const std::array<int, 2>*>(argument);
    RequireState(expected[0], expected[1]);
    const KernelSchedParam requested{450};
    Require(sched_setscheduler_nid_postfix(0, 1, &requested) == 0);
    RequireState(1, 450);
    return nullptr;
}

static void* APS5_VABI Run(void*) {
    for (const int policy : {1, 3}) {
        for (const int priority : {256, 400, 767}) {
            const KernelSchedParam requested{priority};
            *__error_nid_postfix() = 1234;
            Require(sched_setscheduler_nid_postfix(0, policy, &requested) == 0);
            Require(*__error_nid_postfix() == 1234);
            RequireState(policy, priority);
            std::array<int, 2> expected{policy, priority};
            Pthread child = nullptr;
            Require(scePthreadCreate(&child, nullptr, Inherited, &expected, "sched-child") == 0);
            Require(scePthreadJoin(child, nullptr) == 0);
            RequireState(policy, priority);
        }
    }
    for (const int priority : {std::numeric_limits<int>::min(), -1, 0, 255, 768, 896, 959, std::numeric_limits<int>::max()}) {
        const KernelSchedParam requested{priority};
        for (const int policy : {1, 3}) {
            Require(sched_setscheduler_nid_postfix(0, policy, &requested) == -1 && *__error_nid_postfix() == 1);
            RequireState(3, 767);
        }
    }
    const KernelSchedParam requested{400};
    for (const int policy : {std::numeric_limits<int>::min(), -1, 0, 4, std::numeric_limits<int>::max()}) {
        Require(sched_setscheduler_nid_postfix(0, policy, &requested) == -1 && *__error_nid_postfix() == 22);
        RequireState(3, 767);
    }
    const int pid = getpid_nid_postfix();
    for (const int other : {-1, std::numeric_limits<int>::max()}) {
        if (other == pid) continue;
        Require(sched_setscheduler_nid_postfix(other, 0, &requested) == -1 && *__error_nid_postfix() == 3);
        Require(sched_setscheduler_nid_postfix(other, 0, nullptr) == -1 && *__error_nid_postfix() == 14);
        RequireState(3, 767);
    }
    Require(sched_setscheduler_nid_postfix(0, 1, nullptr) == -1 && *__error_nid_postfix() == 14);
    for (const std::array<int, 2> unsupported : {std::array<int, 2>{0, 2}, std::array<int, 2>{pid, 1}}) {
        bool threw = false;
        try {
            sched_setscheduler_nid_postfix(unsupported[0], unsupported[1], &requested);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        RequireState(3, 767);
    }
    return nullptr;
}

int main() {
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Run, nullptr, "setscheduler") == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
}
