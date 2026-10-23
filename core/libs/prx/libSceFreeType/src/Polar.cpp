#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/General.hpp"

#include <ft2build.h>
#include FT_TRIGONOMETRY_H

#include <cstdint>
#include <limits>

using namespace FreeType;

namespace {

std::uint64_t Magnitude(std::int64_t value) {
    const auto bits = static_cast<std::uint64_t>(value);
    return value < 0 ? 0 - bits : bits;
}

FT_Vector NativeVector(const GuestVector& vector) {
    const auto limit = static_cast<std::uint64_t>(std::numeric_limits<FT_Pos>::max()) < std::numeric_limits<std::uint32_t>::max()
        ? static_cast<std::uint64_t>(std::numeric_limits<FT_Pos>::max()) : std::numeric_limits<std::uint32_t>::max();
    if (Magnitude(vector.x) > limit || Magnitude(vector.y) > limit) NotImplemented_nid_no_patch("FreeType polar vector coordinate range");
    return {static_cast<FT_Pos>(vector.x), static_cast<FT_Pos>(vector.y)};
}

FT_Angle NativeAngle(std::int64_t angle) {
    constexpr std::int64_t turn = 360ll << 16;
    const auto reduced = angle % turn;
    return static_cast<FT_Angle>(reduced == 0 && angle != 0 ? turn : reduced);
}

GuestVector Rotate(const GuestVector& vector, std::int64_t angle) {
    if (angle == 0 || (vector.x == 0 && vector.y == 0)) return vector;
    auto native = NativeVector(vector);
    if constexpr (sizeof(FT_Pos) < sizeof(std::int64_t)) {
        constexpr std::uint64_t roundingMargin = 64;
        if (Magnitude(vector.x) + Magnitude(vector.y) > static_cast<std::uint64_t>(std::numeric_limits<FT_Pos>::max()) - roundingMargin) {
            NotImplemented_nid_no_patch("FreeType rotation outside the host coordinate arithmetic range");
        }
    }
    FT_Vector_Rotate(&native, NativeAngle(angle));
    return {native.x, native.y};
}

}

extern "C" {

std::int64_t APS5_VABI FT_Atan2_nid_postfix(std::int64_t x, std::int64_t y) {
    if (x == 0 && y == 0) return 0;
    const auto native = NativeVector({x, y});
    return FT_Atan2(native.x, native.y);
}

void APS5_VABI FT_Vector_Rotate_nid_postfix(GuestVector* vector, std::int64_t angle) {
    if (vector) *vector = Rotate(*vector, angle);
}

std::int64_t APS5_VABI FT_Vector_Length_nid_postfix(GuestVector* vector) {
    if (!vector) return 0;
    if (vector->x == 0 || vector->y == 0) {
        const auto magnitude = Magnitude(vector->x == 0 ? vector->y : vector->x);
        if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) NotImplemented_nid_no_patch(__func__);
        return static_cast<std::int64_t>(magnitude);
    }
    auto native = NativeVector(*vector);
    return static_cast<std::uint32_t>(FT_Vector_Length(&native));
}

void APS5_VABI FT_Vector_Polarize_nid_postfix(GuestVector* vector, std::int64_t* length, std::int64_t* angle) {
    if (!vector || !length || !angle || (vector->x == 0 && vector->y == 0)) return;
    auto native = NativeVector(*vector);
    FT_Fixed nativeLength{};
    FT_Angle nativeAngle{};
    FT_Vector_Polarize(&native, &nativeLength, &nativeAngle);
    *length = static_cast<std::uint32_t>(nativeLength);
    *angle = nativeAngle;
}

void APS5_VABI FT_Vector_From_Polar_nid_postfix(GuestVector* vector, std::int64_t length, std::int64_t angle) {
    if (vector) *vector = Rotate({length, 0}, angle);
}

}
