#include "GuestText.hpp"
#include "prx/libc/include/General.hpp"
#include <bit>

namespace {

bool Lead(std::int32_t value) { return (value & 0xfc00) == 0xd800; }
bool Trail(std::int32_t value) { return (value & 0xfc00) == 0xdc00; }
bool Surrogate(std::int32_t value) { return (value & 0xf800) == 0xd800; }
std::int32_t Scalar(std::int32_t lead, std::int32_t trail) { return ((lead - 0xd800) << 10) + trail - 0xdc00 + 0x10000; }

void Check(const Icu::Text* text) {
    if (!text || text->chunkLength < 0 || text->chunkOffset < 0 || text->chunkOffset > text->chunkLength || (text->chunkLength != 0 && !text->chunkContents)) NotImplemented_nid_no_patch(__func__);
}

bool Access(Icu::Text* text, std::int64_t index, bool forward) {
    if (!text->pFuncs || !text->pFuncs->access) NotImplemented_nid_no_patch(__func__);
    const auto result = text->pFuncs->access(text, index, forward);
    Check(text);
    return result != 0;
}

std::int32_t Map(Icu::Text* text, std::int64_t index) {
    if (!text->pFuncs || !text->pFuncs->mapNativeIndexToUTF16) NotImplemented_nid_no_patch(__func__);
    const auto offset = text->pFuncs->mapNativeIndexToUTF16(text, index);
    if (offset < 0 || offset > text->chunkLength) NotImplemented_nid_no_patch(__func__);
    return offset;
}

char16_t At(const Icu::Text* text, std::int32_t offset) {
    Check(text);
    if (offset < 0 || offset >= text->chunkLength) NotImplemented_nid_no_patch(__func__);
    return text->chunkContents[offset];
}

void SetIndex(Icu::Text* text, std::int64_t index) {
    if (index < text->chunkNativeStart || index >= text->chunkNativeLimit) Access(text, index, true);
    else if (const auto narrowed = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(index - text->chunkNativeStart)); narrowed <= text->nativeIndexingLimit) text->chunkOffset = narrowed;
    else text->chunkOffset = Map(text, index);
    Check(text);
    if (text->chunkOffset < text->chunkLength && Trail(At(text, text->chunkOffset))) {
        if (text->chunkOffset == 0) Access(text, text->chunkNativeStart, false);
        if (text->chunkOffset > 0 && Lead(At(text, text->chunkOffset - 1))) --text->chunkOffset;
    }
}

std::int32_t Next(Icu::Text* text) {
    Check(text);
    if (text->chunkOffset >= text->chunkLength && !Access(text, text->chunkNativeLimit, true)) return -1;
    const auto value = At(text, text->chunkOffset++);
    if (!Lead(value)) return value;
    if (text->chunkOffset >= text->chunkLength && !Access(text, text->chunkNativeLimit, true)) return value;
    const auto trail = At(text, text->chunkOffset);
    if (!Trail(trail)) return value;
    ++text->chunkOffset;
    return Scalar(value, trail);
}

std::int32_t Previous(Icu::Text* text) {
    Check(text);
    if (text->chunkOffset <= 0 && !Access(text, text->chunkNativeStart, false)) return -1;
    const auto value = At(text, --text->chunkOffset);
    if (!Trail(value)) return value;
    if (text->chunkOffset <= 0 && !Access(text, text->chunkNativeStart, false)) return value;
    const auto lead = At(text, text->chunkOffset - 1);
    if (!Lead(lead)) return value;
    --text->chunkOffset;
    return Scalar(lead, value);
}

} // namespace

extern "C" {

std::int32_t APS5_VABI utext_current32_nid_postfix(Icu::Text* text) {
    Check(text);
    if (text->chunkOffset == text->chunkLength && !Access(text, text->chunkNativeLimit, true)) return -1;
    const auto value = At(text, text->chunkOffset);
    if (!Lead(value)) return value;
    std::int32_t trail = 0;
    if (text->chunkOffset + 1 < text->chunkLength) trail = At(text, text->chunkOffset + 1);
    else {
        const auto native = text->chunkNativeLimit;
        if (Access(text, native, true)) trail = At(text, text->chunkOffset);
        const auto restored = Access(text, native, false);
        text->chunkOffset = text->chunkLength - 1;
        if (!restored) return -1;
    }
    return Trail(trail) ? Scalar(value, trail) : value;
}

std::int32_t APS5_VABI utext_next32_nid_postfix(Icu::Text* text) { return Next(text); }
std::int32_t APS5_VABI utext_previous32_nid_postfix(Icu::Text* text) { return Previous(text); }

std::int32_t APS5_VABI utext_next32From_nid_postfix(Icu::Text* text, std::int64_t index) {
    Check(text);
    if (index < text->chunkNativeStart || index >= text->chunkNativeLimit) {
        if (!Access(text, index, true)) return -1;
    } else if (index - text->chunkNativeStart <= text->nativeIndexingLimit) text->chunkOffset = static_cast<std::int32_t>(index - text->chunkNativeStart);
    else text->chunkOffset = Map(text, index);
    const auto value = At(text, text->chunkOffset++);
    if (!Surrogate(value)) return value;
    SetIndex(text, index);
    return Next(text);
}

std::int32_t APS5_VABI utext_previous32From_nid_postfix(Icu::Text* text, std::int64_t index) {
    Check(text);
    if (index <= text->chunkNativeStart || index > text->chunkNativeLimit) {
        if (!Access(text, index, false)) return -1;
    } else if (index - text->chunkNativeStart <= text->nativeIndexingLimit) text->chunkOffset = static_cast<std::int32_t>(index - text->chunkNativeStart);
    else {
        text->chunkOffset = Map(text, index);
        if (text->chunkOffset == 0 && !Access(text, index, false)) return -1;
    }
    const auto value = At(text, --text->chunkOffset);
    if (!Surrogate(value)) return value;
    SetIndex(text, index);
    return Previous(text);
}

}
