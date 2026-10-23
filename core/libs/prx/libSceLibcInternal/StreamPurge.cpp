#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cerrno>

namespace {

class LockedStream {
public:
    explicit LockedStream(FileStream* value) : stream(value) { flockfile_nid_postfix(stream); }
    ~LockedStream() { funlockfile_nid_postfix(stream); }
    LockedStream(const LockedStream&) = delete;
    LockedStream& operator=(const LockedStream&) = delete;
private:
    FileStream* stream;
};

} // namespace

extern "C" int APS5_VABI fpurge_nid_postfix(FileStream* stream) {
    if (!stream) NotImplemented_nid_no_patch(__func__);
    if (!stream->IsOpen()) { errno = 9; return -1; }
    const LockedStream lock(stream);
    auto& state = stream->GuestState();
    if (!std::atomic_ref(state.flags).load(std::memory_order_relaxed)) { errno = 9; return -1; }
    if (state.buffer || state.position || state.bufferSize || state.lineBufferSize ||
        std::atomic_ref(state.readRemaining).load(std::memory_order_relaxed) ||
        std::atomic_ref(state.writeRemaining).load(std::memory_order_relaxed))
        NotImplemented_nid_no_patch(__func__);
    if (stream->IsRandomDevice()) NotImplemented_nid_no_patch(__func__);
#if defined(__GLIBC__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    const int saved = errno;
#if defined(__GLIBC__)
    ::__fpurge(stream->GetHandle());
    const int result = 0;
#else
    const int result = ::fpurge(stream->GetHandle());
#endif
    if (result == 0) errno = saved;
    return result;
#else
    NotImplemented_nid_no_patch(__func__);
#endif
}
