#include "prx/libSceJxr/Stream.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

extern "C" {
std::int64_t APS5_VABI readIS_nid_postfix(void*, Jxr::BitIo*);
std::int64_t APS5_VABI writeIS_nid_postfix(void*, Jxr::BitIo*);
}

namespace {

constexpr std::size_t PacketSize = 4096;
alignas(16384) std::array<std::uint8_t, 8192> ring;
std::array<std::uint8_t, PacketSize> output;
std::vector<char> calls;
void* lastBuffer;
std::size_t lastSize;
std::size_t lastPosition;
std::int64_t readResult;
std::int64_t writeResult;
std::int64_t seekResult;
Jxr::Stream* lastStream;

struct Mutation {
    Jxr::BitIo* io = nullptr;
    std::uint8_t* afterSeek = nullptr;
    Jxr::Stream* afterSeekStream = nullptr;
    std::uint8_t* afterRead = nullptr;
    std::uint8_t* afterWrite = nullptr;
} mutation;

void Require(bool result, const char* message) {
    if (result) return;
    std::fprintf(stderr, "JXR stream: %s\n", message);
    std::abort();
}

std::int64_t APS5_VABI Read(Jxr::Stream* stream, void* buffer, std::size_t size) {
    calls.push_back('r');
    lastStream = stream;
    lastBuffer = buffer;
    lastSize = size;
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<std::uint8_t>(i * 17 + lastPosition);
    if (mutation.afterRead) mutation.io->start = mutation.afterRead;
    return readResult;
}

std::int64_t APS5_VABI Write(Jxr::Stream*, const void* buffer, std::size_t size) {
    calls.push_back('w');
    lastBuffer = const_cast<void*>(buffer);
    lastSize = size;
    Require(size == output.size(), "write length differs from one packet");
    std::memcpy(output.data(), buffer, size);
    if (mutation.afterWrite) mutation.io->start = mutation.afterWrite;
    return writeResult;
}

std::int64_t APS5_VABI SetPosition(Jxr::Stream*, std::size_t position) {
    calls.push_back('s');
    lastPosition = position;
    if (mutation.afterSeek) mutation.io->start = mutation.afterSeek;
    if (mutation.afterSeekStream) mutation.io->stream = mutation.afterSeekStream;
    return seekResult;
}

void Reads(Jxr::Stream& stream) {
    for (std::size_t half : {std::size_t{0}, PacketSize}) {
        for (std::size_t position : {std::size_t{0}, std::size_t{91}, std::numeric_limits<std::size_t>::max() - 100}) {
            for (std::int64_t result : {std::int64_t{0}, std::int64_t{-73}, std::int64_t{0x100000001}}) {
                ring.fill(0xcc);
                calls.clear();
                readResult = result;
                seekResult = -99;
                Jxr::BitIo io{0x77777777, 0x12345678, 11, ~8192, ring.data() + half, ring.data() + (half ^ PacketSize), &stream, position};
                const auto initial = io;
                Require(readIS_nid_postfix(nullptr, &io) == 0, "read helper propagated an ignored callback error");
                Require(calls == std::vector<char>({'s', 'r'}), "read callbacks are absent or out of order");
                Require(lastPosition == position && lastSize == PacketSize && lastBuffer == initial.start, "read callback arguments differ");
                Require(io.referenceOffset == position + PacketSize && io.start == ring.data() + (half ^ PacketSize), "read cursor did not advance and wrap");
                Require(io.current == initial.current && io.accumulator == initial.accumulator && io.bitsUsed == initial.bitsUsed && io.mask == initial.mask && io.stream == initial.stream, "read changed unrelated bit state");
                std::uint32_t shadow;
                std::memcpy(&shadow, initial.start, sizeof(shadow));
                Require(io.shadow == shadow, "read shadow is not the first four bytes in host order");
                for (std::size_t i = 0; i < PacketSize; ++i) {
                    Require(initial.start[i] == static_cast<std::uint8_t>(i * 17 + position), "refill data differs");
                    Require(ring[(half ^ PacketSize) + i] == 0xcc, "refill changed the other half of the ring");
                }
            }
        }
    }
}

void Writes(Jxr::Stream& stream) {
    for (std::size_t i = 0; i < ring.size(); ++i) ring[i] = static_cast<std::uint8_t>(i * 23);
    for (std::size_t half : {std::size_t{0}, PacketSize}) {
        for (std::int64_t result : {std::int64_t{0}, std::int64_t{-73}, std::int64_t{0x100000001}, std::numeric_limits<std::int64_t>::min()}) {
            calls.clear();
            writeResult = result;
            Jxr::BitIo io{7, 19, 13, ~8192, ring.data() + half, ring.data() + (half ^ PacketSize), &stream, 1234};
            const auto initial = io;
            Require(writeIS_nid_postfix(nullptr, &io) == result, "write result was truncated or changed");
            Require(calls == std::vector<char>({'w'}) && lastBuffer == initial.start && lastSize == PacketSize, "write callback arguments differ");
            Require(std::equal(output.begin(), output.end(), initial.start), "write data differs from buffered bytes");
            Require(io.start == (result < 0 ? initial.start : ring.data() + (half ^ PacketSize)), "failed write advanced or successful write did not wrap");
            Require(io.shadow == initial.shadow && io.accumulator == initial.accumulator && io.bitsUsed == initial.bitsUsed && io.mask == initial.mask && io.current == initial.current && io.stream == initial.stream && io.referenceOffset == initial.referenceOffset, "write changed unrelated bit state");
        }
    }
}

void Buffered() {
    for (std::size_t half : {std::size_t{0}, PacketSize}) {
        for (std::size_t consumed : {std::size_t{0}, PacketSize - 1}) {
            Jxr::BitIo io{123, 456, 7, ~8192, ring.data() + half, ring.data() + half + consumed, nullptr, 789};
            const auto initial = io;
            calls.clear();
            Require(readIS_nid_postfix(nullptr, &io) == 0 && writeIS_nid_postfix(nullptr, &io) == 0, "buffered data unexpectedly required a stream");
            Require(calls.empty() && std::memcmp(&initial, &io, sizeof(io)) == 0, "buffered operation mutated state or performed I/O");
        }
    }
}

void CallbackMutation(Jxr::Stream& stream) {
    Jxr::Stream replacement{};
    readResult = -73;
    seekResult = -99;
    ring.fill(0xcc);
    calls.clear();
    Jxr::BitIo io{7, 19, 13, ~8192, ring.data(), ring.data() + PacketSize, &stream, 91};
    mutation = {&io, ring.data() + PacketSize, &replacement, nullptr, nullptr};
    Require(readIS_nid_postfix(nullptr, &io) == 0, "seek mutation read failed");
    Require(calls == std::vector<char>({'s', 'r'}) && lastStream == &stream, "read did not retain the stream selected before seek");
    Require(lastBuffer == ring.data() + PacketSize && lastSize == PacketSize, "read did not use the buffer selected by seek");
    Require(io.stream == &replacement && io.start == ring.data() && io.referenceOffset == 91 + PacketSize, "seek mutation was lost before wrapping");
    std::uint32_t shadow;
    std::memcpy(&shadow, ring.data() + PacketSize, sizeof(shadow));
    Require(io.shadow == shadow && ring.front() == 0xcc, "seek-selected buffer shadow differs");

    ring.fill(0xab);
    calls.clear();
    io = {7, 19, 13, ~8192, ring.data(), ring.data() + PacketSize, &stream, 123};
    mutation = {&io, nullptr, nullptr, ring.data() + PacketSize, nullptr};
    Require(readIS_nid_postfix(nullptr, &io) == 0, "read mutation failed");
    Require(lastBuffer == ring.data() && io.start == ring.data(), "read mutation did not precede final wrap");
    Require(io.shadow == 0xabababab && io.referenceOffset == 123 + PacketSize, "shadow did not use the pointer changed by read");

    for (std::int64_t result : {std::int64_t{0}, std::int64_t{0x100000001}, std::int64_t{-73}}) {
        ring.fill(0xef);
        calls.clear();
        io = {7, 19, 13, ~8192, ring.data(), ring.data() + PacketSize, &stream, 789};
        mutation = {&io, nullptr, nullptr, nullptr, ring.data() + PacketSize};
        writeResult = result;
        Require(writeIS_nid_postfix(nullptr, &io) == result, "mutating write result differs");
        Require(calls == std::vector<char>({'w'}) && lastBuffer == ring.data(), "mutating write buffer differs");
        Require(io.start == ring.data() + (result < 0 ? PacketSize : 0), "write callback mutation did not precede success wrap or failed return");
        Require(io.referenceOffset == 789 && io.shadow == 7, "mutating write changed unrelated state");
    }
    mutation = {};
}

}

int main() {
    Jxr::Stream stream{};
    stream.read = Read;
    stream.write = Write;
    stream.setPosition = SetPosition;
    Reads(stream);
    Writes(stream);
    Buffered();
    CallbackMutation(stream);
    return 0;
}
