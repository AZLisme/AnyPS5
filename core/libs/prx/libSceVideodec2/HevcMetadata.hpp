#ifndef CORE_LIBS_PRX_LIBSCEVIDEODEC2_HEVCMETADATA_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEODEC2_HEVCMETADATA_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace Videodec2Hevc {

struct PictureInfo {
    std::uint64_t thisSize;
    bool isValid;
    std::uint64_t ptsData;
    std::uint64_t dtsData;
    std::uint64_t attachedData;
    std::uint32_t width;
    std::uint32_t height;
    std::uint8_t bitDepthLumaMinus8;
    std::uint8_t bitDepthChromaMinus8;
    std::uint8_t timingInfoPresent;
    std::uint32_t numUnitsInTick;
    std::uint32_t timeScale;
    std::uint32_t aspectRatioInfoPresent;
    std::uint8_t aspectRatioIdc;
    std::uint16_t sarWidth;
    std::uint16_t sarHeight;
    std::uint8_t videoSignalTypePresent;
    std::uint8_t videoFormat;
    std::uint8_t fullRange;
    std::uint8_t colourDescriptionPresent;
    std::uint8_t colourPrimaries;
    std::uint8_t transferCharacteristics;
    std::uint8_t matrixCoefficients;
    std::uint8_t frameFieldInfoPresent;
    std::uint32_t picStruct;
    std::uint32_t sourceScanType;
    std::uint32_t duplicate;
    std::uint32_t conformanceWindow;
    std::uint32_t confLeft;
    std::uint32_t confRight;
    std::uint32_t confTop;
    std::uint32_t confBottom;
    std::uint32_t defaultDisplayWindow;
    std::uint32_t displayLeft;
    std::uint32_t displayRight;
    std::uint32_t displayTop;
    std::uint32_t displayBottom;
    std::uint8_t chromaLocInfoPresent;
    std::uint8_t chromaLocTop;
    std::uint8_t chromaLocBottom;
    std::uint8_t fieldSequence;
    std::uint8_t vpsPresent;
    std::uint8_t spsPresent;
    std::uint8_t ppsPresent;
    std::uint8_t delimiterPresent;
    std::uint8_t endSequencePresent;
    std::uint8_t endStreamPresent;
    std::uint8_t fillerPresent;
    std::uint8_t pictureTimingPresent;
    std::uint8_t bufferingPeriodPresent;
    std::uint8_t framePackingPresent;
    std::uint8_t alternativeTransferPresent;
    std::uint8_t idr;
    std::uint8_t irap;
    std::uint8_t profileSpace;
    std::uint8_t tier;
    std::uint8_t profile;
    std::uint8_t progressiveSource;
    std::uint8_t interlacedSource;
    std::uint8_t frameOnlyConstraint;
    std::uint8_t level;
    std::uint8_t subLayerProfilePresent;
    std::uint8_t subLayerLevelPresent;
    std::uint8_t subLayerProfileSpace;
    std::uint8_t subLayerTier;
    std::uint8_t subLayerProfile;
    std::uint8_t subLayerLevel;
    std::uint8_t subLayerOrderingPresent;
    std::uint8_t maxDecPicBufferingMinus1;
    std::uint8_t preferredTransfer;
    std::uint8_t cropping;
    std::uint32_t cropLeft;
    std::uint32_t cropRight;
    std::uint32_t cropTop;
    std::uint32_t cropBottom;
};

static_assert(sizeof(PictureInfo) == 184 && offsetof(PictureInfo, width) == 40 && offsetof(PictureInfo, picStruct) == 80 && offsetof(PictureInfo, vpsPresent) == 136);
static_assert(offsetof(PictureInfo, fillerPresent) == 142 && offsetof(PictureInfo, idr) == 147 && offsetof(PictureInfo, profile) == 151 && offsetof(PictureInfo, cropping) == 165 && offsetof(PictureInfo, cropBottom) == 180);

struct Metadata {
    PictureInfo info{};
    std::string unsupported;
};

struct Vps {
    bool timing = false;
    std::uint32_t numUnitsInTick = 0;
    std::uint32_t timeScale = 0;
    std::vector<std::uint8_t> bytes;
    std::string unsupported;
};

struct Sps {
    PictureInfo info{};
    unsigned vps = 0;
    std::string unsupported;
    std::vector<std::uint8_t> bytes;
};

struct Pps {
    unsigned sps = 0;
    std::vector<std::uint8_t> bytes;
    std::string unsupported;
};

struct State {
    std::array<std::optional<Vps>, 16> vps;
    std::array<std::optional<Sps>, 16> sps;
    std::array<std::optional<Pps>, 64> pps;
    std::string unsupported;
    std::optional<std::uint8_t> preferredTransfer;
    std::string transferUnsupported;
    bool hadPicture = false;
    bool afterEnd = false;
};

Metadata Parse_nid_no_patch(State& state, std::span<const std::uint8_t> annexB);

} // namespace Videodec2Hevc

#endif
