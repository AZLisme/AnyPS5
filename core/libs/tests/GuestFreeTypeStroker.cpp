#include "prx/libSceFreeType/libSceFreeType.hpp"

#include <ft2build.h>
#include FT_STROKER_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <vector>

using FreeType::GuestVector;



namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "FreeType stroker regression at %s:%u\n", location.file_name(), location.line());
        std::abort();
    }
}

struct Geometry {
    std::vector<FT_Vector> points;
    std::vector<unsigned char> tags;
    std::vector<unsigned short> contours;
};

Geometry Export(FT_Stroker stroker, bool guest) {
    std::uint32_t points = 999, contours = 999;
    Require((guest ? FT_Stroker_GetCounts_nid_postfix(stroker, &points, &contours) : FT_Stroker_GetCounts(stroker, &points, &contours)) == 0);
    Require(points < 32767 && contours < 32767);
    std::vector<FT_Vector> positions(points + 2, {12345, 23456});
    std::vector<unsigned char> tags(points + 2, 77);
    std::vector<unsigned short> ends(contours + 2, 12345);
    FT_Outline outline{0, 0, positions.data() + 1, tags.data() + 1, ends.data() + 1, 0};
    FT_Stroker_Export(stroker, &outline);
    Require(outline.n_points == points && outline.n_contours == contours);
    Require(positions.front().x == 12345 && positions.front().y == 23456 && positions.back().x == 12345 && positions.back().y == 23456);
    Require(tags.front() == 77 && tags.back() == 77 && ends.front() == 12345 && ends.back() == 12345);
    return {{positions.begin() + 1, positions.end() - 1}, {tags.begin() + 1, tags.end() - 1}, {ends.begin() + 1, ends.end() - 1}};
}

void Equal(const Geometry& actual, const Geometry& expected) {
    Require(actual.tags == expected.tags && actual.contours == expected.contours && actual.points.size() == expected.points.size());
    for (std::size_t i = 0; i < actual.points.size(); ++i) Require(actual.points[i].x == expected.points[i].x && actual.points[i].y == expected.points[i].y);
}

void Path(FT_Stroker actual, FT_Stroker expected, std::uint8_t open, std::int64_t shift = 0) {
    std::array<GuestVector, 7> points{{{shift - 100, shift - 200}, {shift + 400, shift - 200}, {shift + 800, shift + 500}, {shift + 400, shift + 800}, {shift + 200, shift + 1200}, {shift - 500, shift + 800}, {shift - 200, shift + 100}}};
    const auto original = points;
    std::array<FT_Vector, 7> native{};
    for (std::size_t i = 0; i < points.size(); ++i) native[i] = {static_cast<FT_Pos>(points[i].x), static_cast<FT_Pos>(points[i].y)};
    Require(FT_Stroker_BeginSubPath_nid_postfix(actual, &points[0], open) == FT_Stroker_BeginSubPath(expected, &native[0], open));
    Require(FT_Stroker_LineTo_nid_postfix(actual, &points[0]) == FT_Stroker_LineTo(expected, &native[0]));
    Require(FT_Stroker_LineTo_nid_postfix(actual, &points[1]) == FT_Stroker_LineTo(expected, &native[1]));
    Require(FT_Stroker_ConicTo_nid_postfix(actual, &points[2], &points[3]) == FT_Stroker_ConicTo(expected, &native[2], &native[3]));
    Require(FT_Stroker_CubicTo_nid_postfix(actual, &points[4], &points[5], &points[6]) == FT_Stroker_CubicTo(expected, &native[4], &native[5], &native[6]));
    Require(FT_Stroker_EndSubPath_nid_postfix(actual) == FT_Stroker_EndSubPath(expected));
    Require(std::memcmp(points.data(), original.data(), sizeof(points)) == 0);
}

void TestGeometry(FreeType::GuestLibrary* library, FT_Library reference) {
    for (int cap = 0; cap < 3; ++cap) for (int join = 0; join < 4; ++join) {
        for (const auto radius : {0, 1, 64, 128}) for (const auto miter : {-1, 0x10000, 0x20000, 0x50000}) for (const auto open : {0, 1, 255}) {
            FT_Stroker actual = nullptr, expected = nullptr;
            Require(FT_Stroker_New_nid_postfix(library, &actual) == 0 && actual);
            Require(FT_Stroker_New(reference, &expected) == 0);
            FT_Stroker_Set_nid_postfix(actual, radius, cap, join, miter);
            FT_Stroker_Set(expected, radius, static_cast<FT_Stroker_LineCap>(cap), static_cast<FT_Stroker_LineJoin>(join), miter);
            Equal(Export(actual, true), Export(expected, false));
            Path(actual, expected, open);
            const auto first = Export(actual, true);
            Require(!first.points.empty() && !first.contours.empty());
            Equal(first, Export(expected, false));
            Equal(Export(actual, true), first);
            FT_Stroker_Rewind_nid_postfix(actual);
            FT_Stroker_Rewind(expected);
            Equal(Export(actual, true), Geometry{});
            Path(actual, expected, open);
            Equal(Export(actual, true), first);
            Path(actual, expected, open);
            Equal(Export(actual, true), Export(expected, false));
            FT_Stroker_Set_nid_postfix(actual, 32, 0, 0, std::numeric_limits<std::int64_t>::min());
            FT_Stroker_Set(expected, 32, FT_STROKER_LINECAP_BUTT, FT_STROKER_LINEJOIN_ROUND, 0x10000);
            Equal(Export(actual, true), Geometry{});
            Path(actual, expected, 1);
            Equal(Export(actual, true), Export(expected, false));
            if constexpr (sizeof(FT_Pos) == 8) {
                FT_Stroker_Rewind_nid_postfix(actual);
                FT_Stroker_Rewind(expected);
                Path(actual, expected, open, 1ll << 34);
                Equal(Export(actual, true), Export(expected, false));
            }
            FT_Stroker_Done_nid_postfix(actual);
            FT_Stroker_Done(expected);
        }
    }
}

void TestCaps(FreeType::GuestLibrary* library) {
    for (int cap = 0; cap < 3; ++cap) {
        FT_Stroker stroker{};
        Require(FT_Stroker_New_nid_postfix(library, &stroker) == 0);
        FT_Stroker_Set_nid_postfix(stroker, 64, cap, FT_STROKER_LINEJOIN_MITER_FIXED, 0x10000);
        GuestVector first{0, 0}, last{640, 0};
        Require(FT_Stroker_BeginSubPath_nid_postfix(stroker, &first, 1) == 0);
        Require(FT_Stroker_LineTo_nid_postfix(stroker, &last) == 0 && FT_Stroker_EndSubPath_nid_postfix(stroker) == 0);
        const auto geometry = Export(stroker, true);
        Require(geometry.contours.size() == 1);
        FT_Pos xMin = 0, xMax = 640, yMin = 0, yMax = 0;
        for (const auto point : geometry.points) { xMin = std::min(xMin, point.x); xMax = std::max(xMax, point.x); yMin = std::min(yMin, point.y); yMax = std::max(yMax, point.y); }
        Require(xMin == (cap == 0 ? 0 : -64) && xMax == (cap == 0 ? 640 : 704) && yMin == -64 && yMax == 64);
        if (cap == 0) {
            const std::array<GuestVector, 5> rectangle{{{0, 64}, {640, 64}, {640, -64}, {640, -64}, {0, -64}}};
            Require(geometry.points.size() == rectangle.size());
            for (std::size_t i = 0; i < rectangle.size(); ++i) Require(geometry.points[i].x == rectangle[i].x && geometry.points[i].y == rectangle[i].y && geometry.tags[i] == FT_CURVE_TAG_ON);
        } else if (cap == 1) Require(std::find(geometry.tags.begin(), geometry.tags.end(), FT_CURVE_TAG_CUBIC) != geometry.tags.end());
        FT_Stroker_Done_nid_postfix(stroker);
    }
}

void TestOutputValidation(FreeType::GuestLibrary* library) {
    struct GuardedHandle {
        std::uint64_t before;
        FT_Stroker handle;
        std::uint64_t after;
    } output{0x123456789abcdef0, nullptr, 0xfedcba9876543210};
    Require(FT_Stroker_New_nid_postfix(nullptr, nullptr) == FT_Err_Invalid_Library_Handle);
    Require(FT_Stroker_New_nid_postfix(library, &output.handle) == 0 && output.handle);
    Require(output.before == 0x123456789abcdef0 && output.after == 0xfedcba9876543210);
    struct GuardedCounts {
        std::uint32_t before;
        std::uint32_t points;
        std::uint32_t contours;
        std::uint32_t after;
    } counts{0x12345678, 123, 456, 0xabcdef01};
    Require(FT_Stroker_GetCounts_nid_postfix(nullptr, &counts.points, nullptr) == FT_Err_Invalid_Argument);
    Require(counts.points == 0 && counts.contours == 456);
    counts.points = 123;
    Require(FT_Stroker_GetCounts_nid_postfix(nullptr, nullptr, &counts.contours) == FT_Err_Invalid_Argument);
    Require(counts.points == 123 && counts.contours == 0);
    Require(FT_Stroker_GetCounts_nid_postfix(output.handle, &counts.points, &counts.contours) == 0);
    Require(counts.points == 0 && counts.contours == 0 && counts.before == 0x12345678 && counts.after == 0xabcdef01);
    FT_Stroker_Done_nid_postfix(output.handle);
}

void TestErrors(FreeType::GuestLibrary* library) {
    FT_Stroker sentinel = reinterpret_cast<FT_Stroker>(std::uintptr_t{1});
    Require(FT_Stroker_New_nid_postfix(nullptr, &sentinel) == FT_Err_Invalid_Library_Handle && sentinel == reinterpret_cast<FT_Stroker>(std::uintptr_t{1}));
    Require(FT_Stroker_New_nid_postfix(library, nullptr) == FT_Err_Invalid_Argument);
    FT_Stroker_Done_nid_postfix(nullptr);
    FT_Stroker_Rewind_nid_postfix(nullptr);
    FT_Stroker_Set_nid_postfix(nullptr, std::numeric_limits<std::int64_t>::max(), 0, 0, std::numeric_limits<std::int64_t>::max());
    FT_Stroker stroker{};
    Require(FT_Stroker_New_nid_postfix(library, &stroker) == 0);
    GuestVector point{0, 0};
    for (const auto handle : {FT_Stroker{}, stroker}) {
        Require(FT_Stroker_BeginSubPath_nid_postfix(handle, nullptr, 0) == FT_Err_Invalid_Argument);
        Require(FT_Stroker_LineTo_nid_postfix(handle, nullptr) == FT_Err_Invalid_Argument);
        Require(FT_Stroker_ConicTo_nid_postfix(handle, &point, nullptr) == FT_Err_Invalid_Argument);
        Require(FT_Stroker_ConicTo_nid_postfix(handle, nullptr, &point) == FT_Err_Invalid_Argument);
        Require(FT_Stroker_CubicTo_nid_postfix(handle, nullptr, &point, &point) == FT_Err_Invalid_Argument);
        Require(FT_Stroker_CubicTo_nid_postfix(handle, &point, nullptr, &point) == FT_Err_Invalid_Argument);
        Require(FT_Stroker_CubicTo_nid_postfix(handle, &point, &point, nullptr) == FT_Err_Invalid_Argument);
    }
    Require(FT_Stroker_BeginSubPath_nid_postfix(nullptr, &point, 0) == FT_Err_Invalid_Argument);
    Require(FT_Stroker_LineTo_nid_postfix(nullptr, &point) == FT_Err_Invalid_Argument);
    Require(FT_Stroker_ConicTo_nid_postfix(nullptr, &point, &point) == FT_Err_Invalid_Argument);
    Require(FT_Stroker_CubicTo_nid_postfix(nullptr, &point, &point, &point) == FT_Err_Invalid_Argument);
    Require(FT_Stroker_EndSubPath_nid_postfix(nullptr) == FT_Err_Invalid_Argument);
    std::uint32_t points = 123, contours = 456;
    Require(FT_Stroker_GetCounts_nid_postfix(nullptr, &points, &contours) == FT_Err_Invalid_Argument && points == 0 && contours == 0);
    Require(FT_Stroker_GetCounts_nid_postfix(stroker, nullptr, nullptr) == 0);
    points = 123;
    Require(FT_Stroker_GetCounts_nid_postfix(stroker, &points, &points) == 0 && points == 0);
    FT_Stroker_Set_nid_postfix(stroker, 64, 0, 0, 0x10000);
    Require(FT_Stroker_BeginSubPath_nid_postfix(stroker, &point, 1) == 0);
    GuestVector last{640, 0};
    Require(FT_Stroker_LineTo_nid_postfix(stroker, &last) == 0 && FT_Stroker_EndSubPath_nid_postfix(stroker) == 0);
    const auto original = Export(stroker, true);
    points = 123;
    Require(FT_Stroker_GetCounts_nid_postfix(stroker, &points, &points) == 0 && points == original.contours.size());
    if constexpr (sizeof(FT_Pos) < 8) {
        GuestVector wide{1ll << 40, 0};
        for (unsigned operation = 0; operation < 6; ++operation) {
            bool rejected = false;
            try {
                if (operation == 0) FT_Stroker_BeginSubPath_nid_postfix(stroker, &wide, 1);
                if (operation == 1) FT_Stroker_LineTo_nid_postfix(stroker, &wide);
                if (operation == 2) FT_Stroker_ConicTo_nid_postfix(stroker, &point, &wide);
                if (operation == 3) FT_Stroker_CubicTo_nid_postfix(stroker, &point, &point, &wide);
                if (operation == 4) FT_Stroker_Set_nid_postfix(stroker, 1ll << 40, 0, 0, 0x10000);
                if (operation == 5) FT_Stroker_Set_nid_postfix(stroker, 64, 0, 0, 1ll << 40);
            } catch (const std::runtime_error&) { rejected = true; }
            Require(rejected);
            Equal(Export(stroker, true), original);
        }
    }
    FT_Stroker_Done_nid_postfix(stroker);
}

}

int main() {
    FreeType::GuestLibrary* library{};
    FT_Library reference{};
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&reference) == 0);
    TestGeometry(library, reference);
    TestCaps(library);
    TestErrors(library);
    TestOutputValidation(library);
    FT_Done_FreeType(reference);
    Require(FT_Done_FreeType_nid_postfix(library) == 0);
}
