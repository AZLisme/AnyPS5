#include "prx/libkernel_sys/libkernel_sys.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"

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
        NotImplemented_nid_no_patch(__func__);
    }
    if (policy == 2)
        NotImplemented_nid_no_patch(__func__);
    if (policy != 1 && policy != 3) return Fail(22);
    if (requested.sched_priority < sched_get_priority_min_nid_postfix(policy) ||
        requested.sched_priority > sched_get_priority_max_nid_postfix(policy)) return Fail(1);
    const int result = scePthreadSetschedparam(scePthreadSelf(), policy, &requested);
    return result == 0 ? 0 : Fail(static_cast<std::uint32_t>(result) & 0xffff);
}
