#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {
int* APS5_VABI __error_nid_postfix();
int APS5_VABI getpid_nid_postfix();
Pthread APS5_VABI scePthreadSelf();
int APS5_VABI scePthreadSetschedparam(Pthread, int, const KernelSchedParam*);
int APS5_VABI sched_get_priority_min_nid_postfix(int);
int APS5_VABI sched_get_priority_max_nid_postfix(int);
}

namespace {

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

}

extern "C" int APS5_VABI sched_setscheduler_nid_postfix(int pid, int policy, const KernelSchedParam* param) {
    if (!param) return Fail(14);
    const KernelSchedParam requested = *param;
    if (pid != 0) {
        if (pid != getpid_nid_postfix()) return Fail(3);
        NotImplemented_nid_no_patch("sched_setscheduler: explicit process ID requires the initial process thread");
    }
    if (policy == 2)
        NotImplemented_nid_no_patch("sched_setscheduler: SCHED_OTHER priority mapping is unverified");
    if (policy != 1 && policy != 3) return Fail(22);
    if (requested.sched_priority < sched_get_priority_min_nid_postfix(policy) ||
        requested.sched_priority > sched_get_priority_max_nid_postfix(policy)) return Fail(1);
    const int result = scePthreadSetschedparam(scePthreadSelf(), policy, &requested);
    return result == 0 ? 0 : Fail(static_cast<std::uint32_t>(result) & 0xffff);
}
