#include "prx/libSceFreeType/include/GuestObjects.hpp"

#include FT_OUTLINE_H

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

extern "C" int APS5_VABI FT_Outline_Get_Orientation_nid_postfix(FreeType::GuestOutline* outline);

namespace {

int Embolden(FreeType::GuestOutline* outline, std::int64_t xStrength, std::int64_t yStrength) {
    if (!outline) return FT_Outline_EmboldenXY(nullptr, 0, 0);
    if (xStrength / 2 == 0 && yStrength / 2 == 0) {
        FT_Outline native{};
        return FT_Outline_EmboldenXY(&native, static_cast<FT_Pos>(xStrength), static_cast<FT_Pos>(yStrength));
    }
    if constexpr (sizeof(FT_Pos) < sizeof(std::int64_t)) {
        if (xStrength < -0x1000000 || xStrength > 0x1000000 || yStrength < -0x1000000 || yStrength > 0x1000000) {
            throw std::runtime_error("FT_Outline_Embolden: strengths outside the host arithmetic range are not implemented");
        }
    }
    const auto pointCount = outline->n_points;
    const auto contourCount = outline->n_contours;
    std::vector<FT_Vector> points;
    if (pointCount > 0) {
        points.reserve(pointCount);
        for (int index = 0; index < pointCount; ++index) {
            const auto point = outline->points[index];
            if (point.x < std::numeric_limits<FT_Pos>::min() || point.x > std::numeric_limits<FT_Pos>::max() ||
                point.y < std::numeric_limits<FT_Pos>::min() || point.y > std::numeric_limits<FT_Pos>::max()) {
                throw std::runtime_error("FT_Outline_Embolden: coordinates outside the host FT_Pos range are not implemented");
            }
            points.push_back({static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)});
        }
    }
    FT_Outline native{contourCount, pointCount, points.data(), outline->tags,
                      outline->contours, outline->flags};
    if constexpr (sizeof(FT_Pos) < sizeof(std::int64_t)) {
        if (FT_Outline_Get_Orientation(&native) != FT_Outline_Get_Orientation_nid_postfix(outline)) {
            throw std::runtime_error("FT_Outline_Embolden: a winding area outside the host arithmetic range is not implemented");
        }
    }
    const int error = FT_Outline_EmboldenXY(&native, static_cast<FT_Pos>(xStrength), static_cast<FT_Pos>(yStrength));
    for (int index = 0; index < pointCount; ++index) outline->points[index] = {points[index].x, points[index].y};
    return error;
}

}

extern "C" {

int APS5_VABI FT_Outline_Embolden_nid_postfix(FreeType::GuestOutline* outline, std::int64_t strength) {
    return Embolden(outline, strength, strength);
}

int APS5_VABI FT_Outline_EmboldenXY_nid_postfix(FreeType::GuestOutline* outline, std::int64_t xStrength, std::int64_t yStrength) {
    return Embolden(outline, xStrength, yStrength);
}

}
