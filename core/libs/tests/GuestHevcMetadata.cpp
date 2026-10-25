#include "prx/libSceVideodec2/HevcMetadata.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "HevcFixture.hpp"
#include "HevcMetadataFixture.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

using namespace SceVideodec2;


namespace {

constexpr std::uint32_t codecHevc = 974921;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "HEVC metadata: %s\n", message);
    std::abort();
}

template<typename TCall>
void Rejects(TCall call, const char* message) {
    try { call(); }
    catch (const std::runtime_error&) { return; }
    Require(false, message);
}

struct Decoder {
    std::uint64_t handle = 0;
    DecoderMemoryInfo memory{sizeof(memory)};
    std::vector<std::uint8_t> buffer;
    FrameBuffer frame{sizeof(frame)};
    OutputInfo output{sizeof(output)};

    explicit Decoder(unsigned profile) {
        const DecoderConfigInfo config{sizeof(config), 1, codecHevc, profile, 0, 64, 48, 4, 1};
        Require(sceVideodec2QueryDecoderMemoryInfo_nid_postfix(&config, &memory) == 0, "query memory");
        Require(sceVideodec2CreateDecoder_nid_postfix(&config, &memory, &handle) == 0, "create decoder");
        buffer.resize(memory.maxFrameBufferSize);
        frame.frameBuffer = buffer.data();
        frame.frameBufferSize = buffer.size();
    }

    ~Decoder() { if (handle) sceVideodec2DeleteDecoder_nid_postfix(handle); }

    void Decode(const std::vector<std::uint8_t>& bytes, std::uint64_t tag) {
        const InputData input{sizeof(input), bytes.data(), bytes.size(), 1000 + tag, 2000 + tag, 3000 + tag};
        Require(sceVideodec2Decode_nid_postfix(handle, &input, &frame, &output) == 0, "decode unit");
    }

    Videodec2Hevc::PictureInfo Query() {
        struct Guarded {
            Videodec2Hevc::PictureInfo info{sizeof(info)};
            std::uint64_t guard = 0x123456789abcdef0;
        } result;
        Require(sceVideodec2GetHevcPictureInfo_nid_postfix(&output, &result.info) == 0, "query HEVC info");
        Require(result.info.thisSize == sizeof(result.info) && result.info.isValid, "valid HEVC picture");
        Require(result.guard == 0x123456789abcdef0, "HEVC info overwrote adjacent memory");
        Videodec2Hevc::PictureInfo generic{sizeof(generic)};
        Require(sceVideodec2GetPictureInfo_nid_postfix(&output, reinterpret_cast<AvcPictureInfo*>(&generic), nullptr) == 0, "generic HEVC query");
        Require(generic.ptsData == result.info.ptsData && generic.dtsData == result.info.dtsData && generic.attachedData == result.info.attachedData && generic.width == result.info.width && generic.timeScale == result.info.timeScale && generic.spsPresent == result.info.spsPresent, "generic and dedicated info disagree");
        return result.info;
    }
};

struct GuestMetadataMapping {
    static constexpr std::size_t block = 0x10000;
    unsigned char* memory = nullptr;
    bool watched = false;
#ifdef _WIN32
    HANDLE section = nullptr;
#endif

    GuestMetadataMapping() {
        Require(GuestArena::GuestArenaAvailable_nid_postfix(), "guest arena unavailable");
        memory = static_cast<unsigned char*>(GuestArena::GuestArenaAllocate_nid_postfix(2 * block, block));
        Require(memory && GuestArena::GuestArenaContains_nid_postfix(memory, 2 * block), "allocate guest metadata mapping");
#ifdef _WIN32
        section = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_EXECUTE_READWRITE | SEC_RESERVE, 0, 2 * block, nullptr);
        Require(section != nullptr, "create guest metadata section");
        GuestArena::GuestArenaMap_nid_postfix(memory, 2 * block, section, 0, PAGE_READWRITE);
#else
        GuestArena::GuestArenaCommit_nid_postfix(memory, 2 * block, 0x04, block);
#endif
        watched = GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix();
        if (watched) GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(memory, 2 * block);
    }

    ~GuestMetadataMapping() {
        if (watched) GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(memory, 2 * block);
        Protect(memory, 2 * block, 0x04);
        GuestArena::GuestArenaReset_nid_postfix(memory, 2 * block);
        GuestArena::GuestArenaRelease_nid_postfix(memory, 2 * block);
#ifdef _WIN32
        GuestArena::GuestArenaForgetSection_nid_postfix(section);
        Require(CloseHandle(section), "close guest metadata section");
#endif
    }

    Videodec2Hevc::PictureInfo* Info() const {
        return reinterpret_cast<Videodec2Hevc::PictureInfo*>(memory + block - 96);
    }

    void Protect(void* pointer, std::size_t bytes, std::uint32_t protection) {
#ifdef _WIN32
        for (std::size_t offset = 0; offset < bytes; offset += block) {
            DWORD previous = 0;
            Require(VirtualProtect(static_cast<unsigned char*>(pointer) + offset, block, protection, &previous), "protect guest metadata section");
        }
#endif
        GuestArena::GuestArenaSetProtection_nid_postfix(reinterpret_cast<std::uintptr_t>(pointer), bytes, protection);
    }

    void Initialize() {
        Protect(memory, 2 * block, 0x04);
        GuestArena::HostWrite access(memory, 2 * block);
        Require(access.Open(), "initialize guest metadata access");
        std::memset(memory, 0xa7, 2 * block);
        Info()->thisSize = sizeof(*Info());
        Info()->isValid = true;
    }

    std::size_t Collect() const {
        std::size_t writes = 0;
        if (watched) Require(GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(memory), 2 * block,
            [](void* context, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::size_t*>(context) += end - begin; }, &writes), "collect guest metadata writes");
        return writes;
    }
};

void GuestMetadataWrites() {
    Decoder decoder(1);
    decoder.Decode(HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size()).front(), 91);
    if (!decoder.output.isValid) Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0, "flush mapped metadata fixture");
    Require(decoder.output.isValid, "mapped metadata fixture produced no picture");
    const auto expected = decoder.Query();
    GuestMetadataMapping mapping;
    for (const bool generic : {false, true}) {
        for (const bool owned : {false, true}) {
            auto output = decoder.output;
            if (!owned) output.frameBuffer = nullptr;
            const auto query = [&] {
                return generic ? sceVideodec2GetPictureInfo_nid_postfix(&output, reinterpret_cast<AvcPictureInfo*>(mapping.Info()), nullptr) :
                    sceVideodec2GetHevcPictureInfo_nid_postfix(&output, mapping.Info());
            };
            mapping.Initialize();
            auto* first = reinterpret_cast<unsigned char*>(mapping.Info()) - 16;
            std::array<unsigned char, sizeof(Videodec2Hevc::PictureInfo) + 32> initial{};
            std::memcpy(initial.data(), first, initial.size());
            mapping.Collect();
            Require(query() == 0, "guest-mapped metadata query");
            if (owned) {
                Require(mapping.Info()->isValid && mapping.Info()->thisSize == sizeof(expected) &&
                    mapping.Info()->ptsData == expected.ptsData && mapping.Info()->attachedData == expected.attachedData &&
                    mapping.Info()->width == expected.width && mapping.Info()->profile == expected.profile, "guest-mapped metadata value");
                Require(std::memcmp(first, initial.data(), 16) == 0 &&
                    std::memcmp(first + 16 + sizeof(expected), initial.data() + 16 + sizeof(expected), 16) == 0, "guest metadata canaries");
            } else {
                initial[16 + offsetof(Videodec2Hevc::PictureInfo, isValid)] = 0;
                Require(std::memcmp(first, initial.data(), initial.size()) == 0, "invalid-picture mapping changes only isValid");
            }
            if (mapping.watched) Require(mapping.Collect() != 0, "guest metadata write not reported");
#ifdef _WIN32
            mapping.Initialize();
            std::memcpy(initial.data(), first, initial.size());
            const auto refused = mapping.memory + (owned ? GuestMetadataMapping::block : 0);
            mapping.Protect(refused, GuestMetadataMapping::block, 0x02);
            {
                GuestArena::HostWrite access(owned ? static_cast<void*>(mapping.Info()) : static_cast<void*>(&mapping.Info()->isValid),
                    owned ? sizeof(*mapping.Info()) : sizeof(mapping.Info()->isValid));
                Require(!access.Open(), "read-only metadata mapping unexpectedly writable");
            }
            Rejects(query, "read-only metadata query did not reject access");
            Require(std::memcmp(first, initial.data(), initial.size()) == 0, "rejected metadata write changed output or canaries");
#endif
        }
    }
}

void RejectUnchanged(Decoder& decoder);

void CheckCommon(const Videodec2Hevc::PictureInfo& info, unsigned bitDepth) {
    Require(info.width == 64 && info.height == 40, "coded luma dimensions");
    Require(info.bitDepthLumaMinus8 == bitDepth && info.bitDepthChromaMinus8 == bitDepth, "source bit depths");
    Require(info.timingInfoPresent == 1 && info.numUnitsInTick == 1 && info.timeScale == 30, "VUI timing syntax");
    Require(info.aspectRatioInfoPresent == 1 && info.aspectRatioIdc == 1 && info.sarWidth == 1 && info.sarHeight == 1, "VUI sample aspect ratio");
    Require(info.videoSignalTypePresent == 1 && info.videoFormat == 5 && info.fullRange == 0 && info.colourDescriptionPresent == 0 && info.colourPrimaries == 2 && info.transferCharacteristics == 2 && info.matrixCoefficients == 2, "signalled video format and inferred colours");
    Require(info.frameFieldInfoPresent == 0 && info.picStruct == 0 && info.sourceScanType == 1 && info.duplicate == 0, "PTL progressive scan inference");
    Require(info.defaultDisplayWindow == 0 && info.displayLeft == 0 && info.displayRight == 0 && info.displayTop == 0 && info.displayBottom == 0, "default display window inference");
    Require(info.chromaLocInfoPresent == 0 && info.chromaLocTop == 0 && info.chromaLocBottom == 0 && info.fieldSequence == 0, "inferred chroma location and field sequence");
    Require(info.profileSpace == 0 && info.tier == 0 && info.profile == (bitDepth == 0 ? 1 : 2) && info.level == 30 && info.progressiveSource == 1 && info.interlacedSource == 0 && info.frameOnlyConstraint == 1, "general profile-tier-level syntax");
    Require(info.subLayerProfilePresent == 0 && info.subLayerLevelPresent == 0 && info.subLayerProfileSpace == 0 && info.subLayerTier == 0 && info.subLayerProfile == 0 && info.subLayerLevel == 0 && info.subLayerOrderingPresent == 1 && info.maxDecPicBufferingMinus1 == 4, "single-layer PTL and DPB syntax");
    Require(info.cropping == 0 && info.cropLeft == 0 && info.cropRight == 0 && info.cropTop == 0 && info.cropBottom == 0, "absent tail cropping fields");
}

void Clips(bool alternativeTransfer = false) {
    Decoder decoder(1);
    std::size_t pictures = 0;
    const auto take = [&] {
        if (!decoder.output.isValid) return;
        const auto info = decoder.Query();
        CheckCommon(info, 0);
        constexpr std::array<unsigned, 6> order{0, 3, 2, 1, 5, 4};
        Require(pictures < order.size(), "too many pictures");
        const auto tag = order[pictures];
        Require(info.ptsData == 1000 + tag && info.dtsData == 2000 + tag && info.attachedData == 3000 + tag, "per-picture access-unit association through B-frame reordering");
        Require(info.vpsPresent == (pictures == 0) && info.spsPresent == (pictures == 0) && info.ppsPresent == (pictures == 0) && info.delimiterPresent == 1, "presence is from the emitted picture's unit");
        Require(info.conformanceWindow == 0 && info.confLeft == 0 && info.confRight == 0 && info.confTop == 0 && info.confBottom == 0, "no conformance window");
        Require(info.idr == (pictures == 0) && info.irap == (pictures == 0) && info.fillerPresent == 0, "per-picture IDR/IRAP and filler flags");
        Require(info.alternativeTransferPresent == (alternativeTransfer && pictures == 0) && info.preferredTransfer == (alternativeTransfer ? 16 : 0), "alternative transfer persists through B-frame reordering");
        ++pictures;
    };
    auto units = HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size());
    if (alternativeTransfer) {
        const std::array<std::uint8_t, 9> sei{0, 0, 1, 0x4e, 1, 147, 1, 16, 0x80};
        units[0].insert(units[0].begin() + 7, sei.begin(), sei.end());
    }
    for (std::size_t unit = 0; unit < units.size(); ++unit) {
        decoder.Decode(units[unit], unit);
        take();
    }
    for (std::size_t drain = 0; drain <= units.size(); ++drain) {
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0, "drain clip");
        if (!decoder.output.isValid) break;
        take();
    }
    Require(pictures == 6, "decoded all six picture metadata records");
    Videodec2Hevc::PictureInfo invalid{sizeof(invalid)};
    invalid.isValid = true;
    Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "reset");
    Require(sceVideodec2GetHevcPictureInfo_nid_postfix(&decoder.output, &invalid) == 0 && !invalid.isValid, "reset releases picture metadata");
    invalid.thisSize = 8;
    invalid.isValid = true;
    Rejects([&] { sceVideodec2GetPictureInfo_nid_postfix(&decoder.output, reinterpret_cast<AvcPictureInfo*>(&invalid), nullptr); }, "unowned generic HEVC query accepted an undersized destination");
    Require(invalid.isValid, "unowned undersized generic query changed destination");
    decoder.Decode(units.front(), 99);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "decode after reset");
    Require(decoder.Query().attachedData == 3099, "new metadata after reset");
}

void Single(std::span<const std::uint8_t> stream, unsigned bitDepth, unsigned bottomCrop, bool delimiter) {
    Decoder decoder(bitDepth == 0 ? 1 : 2);
    decoder.Decode({stream.begin(), stream.end()}, 7);
    if (!decoder.output.isValid) Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0, "single-picture drain");
    if (bottomCrop != 0) {
        Require(decoder.output.isValid, "cropped picture remains decodable");
        RejectUnchanged(decoder);
        return;
    }
    const auto info = decoder.Query();
    CheckCommon(info, bitDepth);
    Require(info.attachedData == 3007 && info.vpsPresent == 1 && info.spsPresent == 1 && info.ppsPresent == 1 && info.delimiterPresent == delimiter, "single-picture tags and presence");
    Require(info.idr == 1 && info.irap == 1 && info.preferredTransfer == 0 && info.pictureTimingPresent == 0, "single-picture tail metadata");
    Require(info.conformanceWindow == (bottomCrop != 0) && info.confBottom == bottomCrop && info.confLeft == 0 && info.confRight == 0 && info.confTop == 0, "raw chroma-unit conformance offsets");
    auto badOutput = decoder.output;
    auto badInfo = info;
    Rejects([&] { sceVideodec2GetHevcPictureInfo_nid_postfix(nullptr, &badInfo); }, "null output accepted");
    Rejects([&] { sceVideodec2GetHevcPictureInfo_nid_postfix(&badOutput, nullptr); }, "null destination accepted");
    badInfo.thisSize = sizeof(badInfo) - 1;
    Rejects([&] { sceVideodec2GetHevcPictureInfo_nid_postfix(&badOutput, &badInfo); }, "undersized destination accepted");
    badInfo = info;
    badOutput.thisSize = 8;
    Rejects([&] { sceVideodec2GetHevcPictureInfo_nid_postfix(&badOutput, &badInfo); }, "undersized output accepted");
    badOutput = decoder.output;
    badOutput.codecType = 1;
    Rejects([&] { sceVideodec2GetHevcPictureInfo_nid_postfix(&badOutput, &badInfo); }, "another codec accepted");
    std::array<std::uint8_t, sizeof(badInfo)> before;
    std::memcpy(before.data(), &badInfo, before.size());
    Rejects([&] { sceVideodec2GetAvcPictureInfo_nid_postfix(&decoder.output, reinterpret_cast<AvcPictureInfo*>(&badInfo), nullptr); }, "AVC-specific getter accepted HEVC with a large buffer");
    Require(std::memcmp(before.data(), &badInfo, before.size()) == 0, "AVC-specific failure modified its output");
    Rejects([&] { sceVideodec2GetAvcPictureInfo_nid_postfix(&badOutput, reinterpret_cast<AvcPictureInfo*>(&badInfo), nullptr); }, "AVC-specific getter trusted forged codec tag");
    Require(std::memcmp(before.data(), &badInfo, before.size()) == 0, "forged codec tag changed AVC failure output");
}

void UnsupportedPicture() {
    Decoder decoder(1);
    auto units = HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size());
    const std::array<std::uint8_t, 9> unknownSei{0, 0, 1, 0x4e, 1, 200, 1, 0x42, 0x80};
    units[0].insert(units[0].begin() + 7, unknownSei.begin(), unknownSei.end());
    decoder.Decode(units[0], 0);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "unsupported metadata retains decoded output");
    Videodec2Hevc::PictureInfo untouched;
    std::memset(&untouched, 0x5a, sizeof(untouched));
    untouched.thisSize = sizeof(untouched);
    std::array<std::uint8_t, sizeof(untouched)> before;
    std::memcpy(before.data(), &untouched, before.size());
    Rejects([&] { sceVideodec2GetHevcPictureInfo_nid_postfix(&decoder.output, &untouched); }, "unknown SEI metadata did not throw");
    Require(std::memcmp(before.data(), &untouched, before.size()) == 0, "unsupported query changed destination");
    Rejects([&] { sceVideodec2GetPictureInfo_nid_postfix(&decoder.output, reinterpret_cast<AvcPictureInfo*>(&untouched), nullptr); }, "generic unknown metadata did not throw");
    Require(std::memcmp(before.data(), &untouched, before.size()) == 0, "generic unsupported query changed destination");
    Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "clear unsupported metadata on reset");
    const auto clean = HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size());
    decoder.Decode(clean[0], 80);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "clean replacement picture");
    Require(decoder.Query().ptsData == 1080, "replacement metadata does not retain unsupported reason");
    const auto previous = decoder.output;
    Require(sceVideodec2DeleteDecoder_nid_postfix(decoder.handle) == 0, "delete owner");
    decoder.handle = 0;
    untouched = {sizeof(untouched)};
    untouched.isValid = true;
    Require(sceVideodec2GetHevcPictureInfo_nid_postfix(&previous, &untouched) == 0 && !untouched.isValid, "deleted owner releases metadata");
}

std::vector<std::uint8_t> Replacement(unsigned wanted) {
    const auto& original = HevcFixture::HevcMain10Stream;
    std::vector<std::size_t> starts;
    for (std::size_t index = 0; index + 2 < original.size(); ++index)
        if (original[index] == 0 && original[index + 1] == 0 && original[index + 2] == 1) starts.push_back(index + 3);
    for (std::size_t index = 0; index < starts.size(); ++index) {
        const auto header = starts[index];
        if (((original[header] >> 1) & 63) != wanted) continue;
        const auto end = index + 1 == starts.size() ? original.size() : starts[index + 1] - 3;
        std::vector<bool> bits;
        unsigned zeros = 0;
        for (std::size_t byte = header + 2; byte < end; ++byte) {
            const auto value = original[byte];
            if (zeros == 2 && value == 3) { zeros = 0; continue; }
            for (int bit = 7; bit >= 0; --bit) bits.push_back(((value >> bit) & 1) != 0);
            zeros = value == 0 ? zeros + 1 : 0;
        }
        while (!bits.empty() && !bits.back()) bits.pop_back();
        Require(bits.size() >= 3, "parameter-set fixture trailing bits");
        bits.pop_back();
        Require(!bits.back(), "fixture extension flag is zero");
        if (wanted == 32) {
            bits.pop_back();
            Require(!bits.back(), "fixture VPS timing flag is zero");
            bits.pop_back();
            bits.push_back(true);
            for (const unsigned value : std::array{1u, 30u})
                for (int bit = 31; bit >= 0; --bit) bits.push_back(((value >> bit) & 1) != 0);
            bits.insert(bits.end(), {false, true, false});
        } else {
            bits.back() = true;
            bits.insert(bits.end(), 8, false);
        }
        bits.push_back(true);
        while (bits.size() % 8 != 0) bits.push_back(false);
        std::vector<std::uint8_t> encoded;
        zeros = 0;
        for (std::size_t byte = 0; byte < bits.size(); byte += 8) {
            std::uint8_t value = 0;
            for (unsigned bit = 0; bit < 8; ++bit) value = static_cast<std::uint8_t>((value << 1) | bits[byte + bit]);
            if (zeros == 2 && value <= 3) { encoded.push_back(3); zeros = 0; }
            encoded.push_back(value);
            zeros = value == 0 ? zeros + 1 : 0;
        }
        std::vector<std::uint8_t> result(original.begin(), original.begin() + header + 2);
        result.insert(result.end(), encoded.begin(), encoded.end());
        result.insert(result.end(), original.begin() + end, original.end());
        return result;
    }
    Require(false, "fixture parameter set not found");
    return {};
}

void RejectUnchanged(Decoder& decoder) {
    Videodec2Hevc::PictureInfo info;
    std::memset(&info, 0x6b, sizeof(info));
    info.thisSize = sizeof(info);
    std::array<std::uint8_t, sizeof(info)> before;
    std::memcpy(before.data(), &info, sizeof(info));
    Rejects([&] { sceVideodec2GetHevcPictureInfo_nid_postfix(&decoder.output, &info); }, "ambiguous replacement metadata succeeded");
    Require(std::memcmp(before.data(), &info, sizeof(info)) == 0, "ambiguous replacement query mutated output");
}

void ParameterReplacements() {
    const std::vector<std::uint8_t> original(HevcFixture::HevcMain10Stream.begin(), HevcFixture::HevcMain10Stream.end());
    for (const unsigned type : std::array{0u, 32u, 33u, 34u}) {
        Decoder decoder(2);
        decoder.Decode(original, 10);
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "initial replacement-test picture");
        Require(decoder.Query().attachedData == 3010, "initial parameter metadata");
        const auto previous = decoder.output;
        std::vector<std::uint8_t> nextBuffer(decoder.buffer.size());
        decoder.frame.frameBuffer = nextBuffer.data();
        decoder.Decode(type == 0 ? original : Replacement(type), 20);
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "decode repeated or replaced parameter sets");
        if (type == 0) Require(decoder.Query().attachedData == 3020, "identical repeated headers remain supported");
        else RejectUnchanged(decoder);
        Videodec2Hevc::PictureInfo old{sizeof(old)};
        Require(sceVideodec2GetHevcPictureInfo_nid_postfix(&previous, &old) == 0 && old.isValid && old.attachedData == 3010, "previous picture metadata remains immutable after parameter updates");
        if (type != 0) {
            Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "reset after replacement");
            decoder.Decode(original, 30);
            Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "decode after replacement reset");
            RejectUnchanged(decoder);
        }
    }
    Decoder recreated(2);
    recreated.Decode(original, 40);
    Require(sceVideodec2Flush_nid_postfix(recreated.handle, &recreated.frame, &recreated.output) == 0 && recreated.output.isValid, "recreated backend picture");
    Require(recreated.Query().attachedData == 3040, "new backend context recovers supported metadata");
}

void ExtendedMetadata() {
    for (const bool interlaced : {false, true}) {
        const auto bytes = interlaced ? std::span<const std::uint8_t>(HevcMetadataFixture::Interlaced) : std::span<const std::uint8_t>(HevcMetadataFixture::Rich);
        Decoder decoder(1);
        decoder.Decode({bytes.begin(), bytes.end()}, 50);
        if (!decoder.output.isValid) Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0, "extended metadata flush");
        const auto info = decoder.Query();
        Require(info.width == 64 && info.height == 48 && info.numUnitsInTick == 1 && info.timeScale == 24, "extended dimensions and timing");
        if (interlaced) {
            Require(info.fieldSequence == 1 && info.frameFieldInfoPresent == 1 && info.picStruct == 1 && info.sourceScanType == 0 && info.duplicate == 0, "picture-timing SEI and field metadata");
            Require(info.pictureTimingPresent == 1 && info.progressiveSource == 0 && info.interlacedSource == 1 && info.frameOnlyConstraint == 0, "interlaced PTL and SEI presence tail");
        } else {
            Require(info.aspectRatioIdc == 2 && info.sarWidth == 12 && info.sarHeight == 11, "table-derived non-square sample aspect");
            Require(info.videoSignalTypePresent == 1 && info.videoFormat == 1 && info.fullRange == 1 && info.colourDescriptionPresent == 1 && info.colourPrimaries == 9 && info.transferCharacteristics == 16 && info.matrixCoefficients == 9, "explicit HDR color syntax");
            Require(info.chromaLocInfoPresent == 1 && info.chromaLocTop == 3 && info.chromaLocBottom == 3, "explicit chroma sample locations");
            Require(info.defaultDisplayWindow == 0 && info.cropping == 0, "unwindowed rich picture");
        }
        Require(info.profile == 4 && info.level == 30 && info.subLayerOrderingPresent == 1 && info.maxDecPicBufferingMinus1 == 2 && info.idr == 1 && info.irap == 1, "extended PTL and DPB tail");
    }
    Decoder windowed(1);
    windowed.Decode({HevcMetadataFixture::Windowed.begin(), HevcMetadataFixture::Windowed.end()}, 51);
    if (!windowed.output.isValid) Require(sceVideodec2Flush_nid_postfix(windowed.handle, &windowed.frame, &windowed.output) == 0, "windowed picture flush");
    Require(windowed.output.isValid, "windowed picture remains decodable");
    RejectUnchanged(windowed);
}

void CabacPadding() {
    Decoder decoder(2);
    std::vector<std::uint8_t> padded(HevcFixture::HevcMain10Stream.begin(), HevcFixture::HevcMain10Stream.end());
    padded.insert(padded.end(), {0, 0, 3});
    decoder.Decode(padded, 60);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "terminal CABAC prevention byte decode");
    Require(decoder.Query().attachedData == 3060, "terminal CABAC prevention byte metadata");
    Videodec2Hevc::State state;
    padded.back() = 2;
    const auto invalid = Videodec2Hevc::Parse_nid_no_patch(state, padded);
    Require(!invalid.unsupported.empty(), "missing emulation prevention was accepted");
}

void MalformedParameterHistory() {
    const std::vector<std::uint8_t> original(HevcFixture::HevcMain10Stream.begin(), HevcFixture::HevcMain10Stream.end());
    Decoder decoder(2);
    decoder.Decode(original, 70);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "initial malformed-history picture");
    Require(decoder.Query().attachedData == 3070, "initial malformed-history metadata");
    std::vector<std::uint8_t> malformed{0, 0, 1, 0x44, 1, 0x80};
    malformed.insert(malformed.end(), original.begin(), original.end());
    bool failed = false;
    try { decoder.Decode(malformed, 71); }
    catch (const std::runtime_error&) { failed = true; }
    if (!failed) {
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "malformed parameter output");
        RejectUnchanged(decoder);
    }
    Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "reset malformed parameter state");
    decoder.Decode(original, 72);
    Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "clean decode after malformed parameter reset");
    RejectUnchanged(decoder);
}

void TransferAssociation() {
    const auto original = HevcFixture::hevcUnits(HevcFixture::HevcStream.data(), HevcFixture::HevcStream.size());
    const std::array<std::uint8_t, 9> transfer{0, 0, 1, 0x4e, 1, 147, 1, 16, 0x80};
    {
        Decoder decoder(1);
        auto units = original;
        units[0].insert(units[0].begin() + 7, transfer.begin(), transfer.end());
        auto conflict = transfer;
        conflict[7] = 18;
        units[0].insert(units[0].begin() + 7, conflict.begin(), conflict.end());
        unsigned pictures = 0;
        for (std::size_t index = 0; index < units.size(); ++index) {
            decoder.Decode(units[index], index);
            if (decoder.output.isValid) { RejectUnchanged(decoder); ++pictures; }
        }
        for (;;) {
            Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0, "conflicting transfer drain");
            if (!decoder.output.isValid) break;
            RejectUnchanged(decoder);
            ++pictures;
        }
        Require(pictures == 6, "conflicting transfers retain all decoded pictures");
        Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "reset conflicting transfer sequence");
        decoder.Decode(original[0], 90);
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "new IRAP transfer sequence");
        Require(decoder.Query().preferredTransfer == 0, "new CLVS clears prior transfer ambiguity");
    }
    {
        Decoder decoder(1);
        try { decoder.Decode({transfer.begin(), transfer.end()}, 91); }
        catch (const std::runtime_error&) {}
        Require(sceVideodec2Reset_nid_postfix(decoder.handle) == 0, "reset pictureless transfer packet");
        decoder.Decode(original[0], 92);
        Require(sceVideodec2Flush_nid_postfix(decoder.handle, &decoder.frame, &decoder.output) == 0 && decoder.output.isValid, "decode after pictureless transfer");
        RejectUnchanged(decoder);
    }
}

}

int main() {
    GuestMetadataWrites();
    Clips();
    Clips(true);
    Single(HevcFixture::HevcMain10Stream, 2, 0, true);
    Single(HevcFixture::HevcCroppedStream, 0, 2, false);
    UnsupportedPicture();
    ParameterReplacements();
    ExtendedMetadata();
    CabacPadding();
    MalformedParameterHistory();
    TransferAssociation();
}
