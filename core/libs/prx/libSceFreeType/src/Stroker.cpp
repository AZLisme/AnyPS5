#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

#include FT_STROKER_H

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

using namespace FreeType;

namespace {

FT_Pos HostPosition(std::int64_t value) {
    if (value < std::numeric_limits<FT_Pos>::min() || value > std::numeric_limits<FT_Pos>::max()) NotImplemented_nid_no_patch("FreeType stroker coordinate outside the host range");
    return static_cast<FT_Pos>(value);
}

FT_Vector HostVector(const GuestVector& value) {
    return {HostPosition(value.x), HostPosition(value.y)};
}

}

extern "C" {

int APS5_VABI FT_Stroker_New_nid_postfix(GuestLibrary* library, FT_Stroker* stroker) {
    if (!library || !library->host) return FT_Err_Invalid_Library_Handle;
    if (!stroker) return FT_Err_Invalid_Argument;
    GuestArena::HostWrite access(stroker, sizeof(*stroker));
    return FT_Stroker_New(library->host, stroker);
}

void APS5_VABI FT_Stroker_Done_nid_postfix(FT_Stroker stroker) {
    FT_Stroker_Done(stroker);
}

void APS5_VABI FT_Stroker_Set_nid_postfix(FT_Stroker stroker, std::int64_t radius, std::int32_t cap, std::int32_t join, std::int64_t miterLimit) {
    if (!stroker) return;
    const auto hostRadius = HostPosition(radius);
    const auto hostMiterLimit = HostPosition(std::max<std::int64_t>(miterLimit, 0x10000));
    FT_Stroker_Set(stroker, hostRadius, static_cast<FT_Stroker_LineCap>(cap), static_cast<FT_Stroker_LineJoin>(join), hostMiterLimit);
}

void APS5_VABI FT_Stroker_Rewind_nid_postfix(FT_Stroker stroker) {
    FT_Stroker_Rewind(stroker);
}

int APS5_VABI FT_Stroker_BeginSubPath_nid_postfix(FT_Stroker stroker, GuestVector* to, std::uint8_t open) {
    if (!stroker || !to) return FT_Err_Invalid_Argument;
    auto native = HostVector(*to);
    return FT_Stroker_BeginSubPath(stroker, &native, open);
}

int APS5_VABI FT_Stroker_EndSubPath_nid_postfix(FT_Stroker stroker) {
    return FT_Stroker_EndSubPath(stroker);
}

int APS5_VABI FT_Stroker_LineTo_nid_postfix(FT_Stroker stroker, GuestVector* to) {
    if (!stroker || !to) return FT_Err_Invalid_Argument;
    auto native = HostVector(*to);
    return FT_Stroker_LineTo(stroker, &native);
}

int APS5_VABI FT_Stroker_ConicTo_nid_postfix(FT_Stroker stroker, GuestVector* control, GuestVector* to) {
    if (!stroker || !control || !to) return FT_Err_Invalid_Argument;
    auto nativeControl = HostVector(*control);
    auto nativeTo = HostVector(*to);
    return FT_Stroker_ConicTo(stroker, &nativeControl, &nativeTo);
}

int APS5_VABI FT_Stroker_CubicTo_nid_postfix(FT_Stroker stroker, GuestVector* control1, GuestVector* control2, GuestVector* to) {
    if (!stroker || !control1 || !control2 || !to) return FT_Err_Invalid_Argument;
    auto nativeControl1 = HostVector(*control1);
    auto nativeControl2 = HostVector(*control2);
    auto nativeTo = HostVector(*to);
    return FT_Stroker_CubicTo(stroker, &nativeControl1, &nativeControl2, &nativeTo);
}

int APS5_VABI FT_Stroker_GetCounts_nid_postfix(FT_Stroker stroker, std::uint32_t* points, std::uint32_t* contours) {
    std::optional<GuestArena::HostWrite> pointAccess;
    std::optional<GuestArena::HostWrite> contourAccess;
    if (points) pointAccess.emplace(points, sizeof(*points));
    if (contours && contours != points) contourAccess.emplace(contours, sizeof(*contours));
    return FT_Stroker_GetCounts(stroker, points, contours);
}

}
