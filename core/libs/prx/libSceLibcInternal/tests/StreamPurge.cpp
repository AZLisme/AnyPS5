#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <exception>
#include <source_location>
#include <string_view>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "stream purge check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

class TemporaryStream {
public:
    std::array<char, 64> buffer{};
    FileStream stream;
    TemporaryStream() : stream(std::tmpfile()) {}
    ~TemporaryStream() {
        if (stream.IsOpen()) std::fclose(stream.GetHandle());
    }
    TemporaryStream(const TemporaryStream&) = delete;
    TemporaryStream& operator=(const TemporaryStream&) = delete;
};

#if defined(__GLIBC__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)

std::int64_t DescriptorPosition(FileStream& stream) {
    return ::lseek(::fileno(stream.GetHandle()), 0, SEEK_CUR);
}

void Output(int mode) {
    TemporaryStream fixture;
    auto& stream = fixture.stream;
    auto* native = stream.GetHandle();
    Require(std::setvbuf(native, fixture.buffer.data(), mode, fixture.buffer.size()) == 0);
    constexpr std::string_view discard = "discard these buffered bytes";
    Require(fputc_nid_postfix(discard.front(), &stream) == discard.front());
    Require(fwrite_nid_postfix(discard.data() + 1, 1, discard.size() - 1, &stream) == discard.size() - 1);
    const auto before = DescriptorPosition(stream);
    Require(before == 0 && ftell_nid_postfix(&stream) == static_cast<std::int64_t>(discard.size()));
    errno = 1234;
    Require(fpurge_nid_postfix(&stream) == 0 && errno == 1234);
    Require(DescriptorPosition(stream) == before && ftell_nid_postfix(&stream) == before);
    Require(!feof_nid_postfix(&stream) && !ferror_nid_postfix(&stream));
    constexpr std::string_view keep = "keep";
    Require(fwrite_nid_postfix(keep.data(), 1, keep.size(), &stream) == keep.size());
    Require(std::fflush(native) == 0 && DescriptorPosition(stream) == static_cast<std::int64_t>(keep.size()));
    std::rewind(native);
    std::array<char, 32> actual{};
    Require(fread_nid_postfix(actual.data(), 1, actual.size(), &stream) == keep.size());
    Require(std::string_view(actual.data(), keep.size()) == keep);
}

void Input(bool pushback) {
    TemporaryStream fixture;
    auto& stream = fixture.stream;
    auto* native = stream.GetHandle();
    Require(std::setvbuf(native, fixture.buffer.data(), _IOFBF, fixture.buffer.size()) == 0);
    std::array<unsigned char, 256> content;
    for (std::size_t index = 0; index < content.size(); ++index) content[index] = static_cast<unsigned char>(index);
    Require(std::fwrite(content.data(), 1, content.size(), native) == content.size());
    Require(std::fflush(native) == 0);
    std::rewind(native);
    Require(fgetc_nid_postfix(&stream) == content.front());
    const auto position = DescriptorPosition(stream);
    Require(position > 1 && position < static_cast<std::int64_t>(content.size()));
    if (pushback) {
        Require(ungetc_nid_postfix('Q', &stream) == 'Q');
        Require(ungetc_nid_postfix('R', &stream) == 'R');
    }
    errno = 1234;
    Require(fpurge_nid_postfix(&stream) == 0 && errno == 1234);
    Require(DescriptorPosition(stream) == position && ftell_nid_postfix(&stream) == position);
    Require(fgetc_nid_postfix(&stream) == content[position]);
    Require(!feof_nid_postfix(&stream) && !ferror_nid_postfix(&stream));
    Require(stream.GuestState().readRemaining == 0 && stream.GuestState().writeRemaining == 0);
}

void Indicators() {
    TemporaryStream empty;
    Require(fgetc_nid_postfix(&empty.stream) == EOF && feof_nid_postfix(&empty.stream));
    const auto before = empty.stream.GuestState().flags;
    Require(fpurge_nid_postfix(&empty.stream) == 0 && feof_nid_postfix(&empty.stream));
    Require(empty.stream.GuestState().flags == before && !ferror_nid_postfix(&empty.stream));
    Require(ungetc_nid_postfix('Q', &empty.stream) == 'Q' && !feof_nid_postfix(&empty.stream));
    Require(fpurge_nid_postfix(&empty.stream) == 0 && !feof_nid_postfix(&empty.stream));
    Require(fgetc_nid_postfix(&empty.stream) == EOF && feof_nid_postfix(&empty.stream));
    TemporaryStream broken;
    Require(::close(::fileno(broken.stream.GetHandle())) == 0);
    Require(fgetc_nid_postfix(&broken.stream) == EOF && ferror_nid_postfix(&broken.stream));
    const auto flags = broken.stream.GuestState().flags;
    errno = 1234;
    Require(fpurge_nid_postfix(&broken.stream) == 0 && errno == 1234);
    Require(ferror_nid_postfix(&broken.stream) && broken.stream.GuestState().flags == flags);
}

void BufferModes() {
    TemporaryStream unbuffered;
    auto* native = unbuffered.stream.GetHandle();
    Require(std::setvbuf(native, nullptr, _IONBF, 0) == 0);
    constexpr std::string_view data = "committed";
    Require(fwrite_nid_postfix(data.data(), 1, data.size(), &unbuffered.stream) == data.size());
    const auto before = DescriptorPosition(unbuffered.stream);
    Require(fpurge_nid_postfix(&unbuffered.stream) == 0 && DescriptorPosition(unbuffered.stream) == before);
    std::rewind(native);
    Require(ungetc_nid_postfix('Q', &unbuffered.stream) == 'Q');
    Require(fpurge_nid_postfix(&unbuffered.stream) == 0 && fgetc_nid_postfix(&unbuffered.stream) == data.front());
    flockfile_nid_postfix(&unbuffered.stream);
    Require(fpurge_nid_postfix(&unbuffered.stream) == 0);
    funlockfile_nid_postfix(&unbuffered.stream);
    TemporaryStream line;
    auto* lineNative = line.stream.GetHandle();
    Require(std::setvbuf(lineNative, line.buffer.data(), _IOLBF, line.buffer.size()) == 0);
    constexpr std::string_view committed = "saved\n";
    for (char byte : committed) Require(fputc_nid_postfix(byte, &line.stream) == byte);
    for (char byte : std::string_view("discard")) Require(fputc_nid_postfix(byte, &line.stream) == byte);
    const auto position = DescriptorPosition(line.stream);
    Require(position == static_cast<std::int64_t>(committed.size()));
    Require(fpurge_nid_postfix(&line.stream) == 0 && DescriptorPosition(line.stream) == position);
    std::rewind(lineNative);
    std::array<char, 32> actual{};
    Require(fread_nid_postfix(actual.data(), 1, actual.size(), &line.stream) == committed.size());
    Require(std::string_view(actual.data(), committed.size()) == committed);
}

void GuestPrefix() {
    for (const std::int16_t mode : {0, 1, 2, 4}) {
        TemporaryStream fixture;
        auto& state = fixture.stream.GuestState();
        state.flags = static_cast<std::int16_t>(0x10 | 0x60 | 0x2000 | mode);
        const auto flags = state.flags;
        errno = 1234;
        Require(fpurge_nid_postfix(&fixture.stream) == 0 && errno == 1234);
        Require(state.flags == flags && state.position == nullptr && state.readRemaining == 0);
        Require(state.writeRemaining == 0 && state.buffer == nullptr && state.bufferSize == 0);
    }
    for (int field = 0; field < 6; ++field) {
        TemporaryStream fixture;
        auto& state = fixture.stream.GuestState();
        auto* native = fixture.stream.GetHandle();
        Require(std::setvbuf(native, fixture.buffer.data(), _IOFBF, fixture.buffer.size()) == 0);
        Require(std::fputc('Q', native) == 'Q');
        const auto before = std::ftell(native);
        std::array<unsigned char, 32> buffer{};
        switch (field) {
        case 0: state.buffer = buffer.data(); break;
        case 1: state.position = buffer.data(); break;
        case 2: state.bufferSize = 32; break;
        case 3: state.readRemaining = 19; break;
        case 4: state.writeRemaining = 7; break;
        case 5: state.lineBufferSize = -32; break;
        }
        const auto original = state;
        RequireThrows([&] { fpurge_nid_postfix(&fixture.stream); });
        Require(state.buffer == original.buffer && state.position == original.position && state.bufferSize == original.bufferSize);
        Require(state.readRemaining == original.readRemaining && state.writeRemaining == original.writeRemaining);
        Require(state.lineBufferSize == original.lineBufferSize && state.flags == original.flags);
        Require(std::ftell(native) == before && std::fflush(native) == 0 && DescriptorPosition(fixture.stream) == before);
    }
}

void WideStreams() {
    TemporaryStream output;
    auto* native = output.stream.GetHandle();
    Require(std::fwide(native, 1) > 0);
    Require(std::fputwc(L'A', native) == L'A');
    const auto before = DescriptorPosition(output.stream);
    Require(before == 0 && fpurge_nid_postfix(&output.stream) == 0);
    Require(DescriptorPosition(output.stream) == before);
    Require(std::fputwc(L'B', native) == L'B' && std::fflush(native) == 0);
    std::rewind(native);
    Require(std::fgetwc(native) == L'B' && std::fgetwc(native) == WEOF);
    TemporaryStream input;
    native = input.stream.GetHandle();
    Require(std::fwide(native, 1) > 0);
    Require(std::fputws(L"ABCDE", native) >= 0 && std::fflush(native) == 0);
    std::rewind(native);
    Require(std::fgetwc(native) == L'A');
    const auto position = DescriptorPosition(input.stream);
    Require(position == 5 && !std::feof(native));
    Require(fpurge_nid_postfix(&input.stream) == 0 && DescriptorPosition(input.stream) == position);
    Require(!std::feof(native) && std::fgetwc(native) == WEOF);
}

#endif

void InvalidStreams() {
    RequireThrows([] { fpurge_nid_postfix(nullptr); });
    TemporaryStream closed;
    std::fclose(closed.stream.GetHandle());
    closed.stream.Detach();
    errno = 0;
    Require(fpurge_nid_postfix(&closed.stream) == -1 && errno == 9);
    TemporaryStream attached;
    attached.stream.GuestState().flags = 0;
    attached.stream.GuestState().readRemaining = 19;
    attached.stream.GuestState().bufferSize = 32;
    errno = 0;
    Require(fpurge_nid_postfix(&attached.stream) == -1 && errno == 9);
    Require(attached.stream.GuestState().readRemaining == 19 && attached.stream.GuestState().bufferSize == 32);
    TemporaryStream random;
    random.stream.MarkRandomDevice();
    RequireThrows([&] { fpurge_nid_postfix(&random.stream); });
}

} // namespace

int main() {
    InvalidStreams();
#if defined(__GLIBC__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    Output(_IOFBF);
    Output(_IOLBF);
    Input(false);
    Input(true);
    Indicators();
    BufferModes();
    GuestPrefix();
    WideStreams();
#else
    TemporaryStream stream;
    RequireThrows([&] { fpurge_nid_postfix(&stream.stream); });
#endif
    std::puts("stream purge tests passed");
}
