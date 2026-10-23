#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Outline_Embolden_nid_postfix(GuestOutline*, std::int64_t);
int APS5_VABI FT_Outline_EmboldenXY_nid_postfix(GuestOutline*, std::int64_t, std::int64_t);
int APS5_VABI FT_Init_FreeType_nid_postfix(void**);
int APS5_VABI FT_Done_FreeType_nid_postfix(void*);
int APS5_VABI FT_New_Memory_Face_nid_postfix(void*, const unsigned char*, std::int64_t, std::int64_t, GuestFaceRec**);
int APS5_VABI FT_Set_Pixel_Sizes_nid_postfix(GuestFaceRec*, std::uint32_t, std::uint32_t);
int APS5_VABI FT_Load_Glyph_nid_postfix(GuestFaceRec*, std::uint32_t, std::int32_t);
int APS5_VABI FT_Render_Glyph_nid_postfix(GuestGlyphSlotRec*, std::int32_t);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType outline synthesis check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void Compare(std::vector<GuestVector> points, std::vector<std::uint16_t> contours, std::int64_t xStrength, std::int64_t yStrength, bool uniform) {
    std::vector<FT_Vector> nativePoints;
    for (const auto point : points) nativePoints.push_back({static_cast<FT_Pos>(point.x), static_cast<FT_Pos>(point.y)});
    std::vector<unsigned short> nativeContours(contours.begin(), contours.end());
    std::vector<unsigned char> tags(points.size(), FT_CURVE_TAG_ON);
    if (points.size() > 2) tags[1] = FT_CURVE_TAG_CONIC;
    const auto originalTags = tags;
    const auto originalContours = contours;
    GuestOutline guest{static_cast<std::uint16_t>(contours.size()), static_cast<std::uint16_t>(points.size()), points.data(), tags.data(), contours.data(), FT_OUTLINE_EVEN_ODD_FILL};
    FT_Outline native{static_cast<unsigned short>(contours.size()), static_cast<unsigned short>(points.size()), nativePoints.data(), tags.data(), nativeContours.data(), guest.flags};
    const auto expected = uniform ? FT_Outline_Embolden(&native, static_cast<FT_Pos>(xStrength)) :
        FT_Outline_EmboldenXY(&native, static_cast<FT_Pos>(xStrength), static_cast<FT_Pos>(yStrength));
    const auto actual = uniform ? FT_Outline_Embolden_nid_postfix(&guest, xStrength) : FT_Outline_EmboldenXY_nid_postfix(&guest, xStrength, yStrength);
    Require(actual == expected);
    for (std::size_t index = 0; index < points.size(); ++index) {
        Require(points[index].x == nativePoints[index].x && points[index].y == nativePoints[index].y);
    }
    Require(tags == originalTags && contours == originalContours);
    Require(guest.points == points.data() && guest.tags == tags.data() && guest.contours == contours.data());
    Require(guest.flags == FT_OUTLINE_EVEN_ODD_FILL && guest.n_points == points.size() && guest.n_contours == contours.size());
}

static void Render() {
    auto bytes = SquareGlyphFont(65, 600);
    FT_Library library = nullptr;
    FT_Face native = nullptr;
    void* guestLibrary = nullptr;
    GuestFaceRec* guest = nullptr;
    Require(FT_Init_FreeType(&library) == 0);
    Require(FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &native) == 0);
    Require(FT_Init_FreeType_nid_postfix(&guestLibrary) == 0);
    Require(FT_New_Memory_Face_nid_postfix(guestLibrary, bytes.data(), bytes.size(), 0, &guest) == 0);
    Require(FT_Set_Pixel_Sizes(native, 0, 32) == 0);
    Require(FT_Set_Pixel_Sizes_nid_postfix(guest, 0, 32) == 0);
    Require(FT_Load_Glyph(native, 1, FT_LOAD_NO_HINTING) == 0);
    Require(FT_Load_Glyph_nid_postfix(guest, 1, FT_LOAD_NO_HINTING) == 0);
    Require(FT_Outline_EmboldenXY(&native->glyph->outline, 96, 32) == 0);
    Require(FT_Outline_EmboldenXY_nid_postfix(&guest->glyph->outline, 96, 32) == 0);
    Require(FT_Render_Glyph(native->glyph, FT_RENDER_MODE_NORMAL) == 0);
    Require(FT_Render_Glyph_nid_postfix(guest->glyph, FT_RENDER_MODE_NORMAL) == 0);
    const auto& bitmap = guest->glyph->bitmap;
    const auto& expected = native->glyph->bitmap;
    Require(bitmap.width > 0 && bitmap.rows > 0);
    Require(bitmap.width == expected.width && bitmap.rows == expected.rows && bitmap.pitch == expected.pitch);
    Require(guest->glyph->bitmap_left == native->glyph->bitmap_left && guest->glyph->bitmap_top == native->glyph->bitmap_top);
    for (unsigned row = 0; row < bitmap.rows; ++row) {
        for (unsigned column = 0; column < bitmap.width; ++column) {
            Require(bitmap.buffer[row * bitmap.pitch + column] == expected.buffer[row * expected.pitch + column]);
        }
    }
    Require(FT_Done_FreeType_nid_postfix(guestLibrary) == 0);
    Require(FT_Done_FreeType(library) == 0);
}

int main() {
    Require(FT_Outline_Embolden_nid_postfix(nullptr, 64) == FT_Err_Invalid_Outline);
    Require(FT_Outline_EmboldenXY_nid_postfix(nullptr, 0, 0) == FT_Err_Invalid_Outline);
    GuestOutline empty{};
    Require(FT_Outline_Embolden_nid_postfix(&empty, 64) == 0);
    GuestOutline unread{};
    unread.n_points = 4;
    unread.n_contours = 1;
    Require(FT_Outline_EmboldenXY_nid_postfix(&unread, -1, 1) == 0);
    std::array<GuestVector, 4> rectangle{{{0, 0}, {0, 640}, {640, 640}, {640, 0}}};
    std::array<unsigned char, 4> rectangleTags{1, 1, 1, 1};
    std::uint16_t rectangleEnd = 3;
    GuestOutline rectangleOutline{1, 4, rectangle.data(), rectangleTags.data(), &rectangleEnd, 0};
    Require(FT_Outline_Embolden_nid_postfix(&rectangleOutline, 64) == 0);
    Require(rectangle[0].x == 0 && rectangle[0].y == 0);
    Require(rectangle[1].x == 0 && rectangle[1].y == 704);
    Require(rectangle[2].x == 704 && rectangle[2].y == 704);
    Require(rectangle[3].x == 704 && rectangle[3].y == 0);
    const std::vector<std::vector<GuestVector>> shapes{
        {}, {{0, 0}}, {{0, 0}, {64, 0}, {128, 0}},
        {{0, 0}, {0, 640}, {640, 640}, {640, 0}},
        {{0, 0}, {640, 0}, {640, 640}, {0, 640}},
        {{0, 0}, {0, 0}, {0, 640}, {32, 64}, {640, 0}},
        {{-640, -320}, {0, 1024}, {64, 0}, {640, -320}},
        {{0x1000001, 0}, {0x1000001, 640}, {0x1000040, 0}}
    };
    for (const auto& shape : shapes) {
        const std::vector<std::uint16_t> contours = shape.empty() ? std::vector<std::uint16_t>{} : std::vector<std::uint16_t>{static_cast<std::uint16_t>(shape.size() - 1)};
        for (const auto strength : std::array<std::int64_t, 7>{-128, -65, -1, 0, 1, 63, 128}) {
            Compare(shape, contours, strength, strength, true);
            Compare(shape, contours, strength, 96, false);
            Compare(shape, contours, 0, strength, false);
        }
    }
    Compare({{0, 0}, {0, 1024}, {1024, 1024}, {1024, 0}, {256, 256}, {768, 256}, {768, 768}, {256, 768}}, {3, 7}, 96, 32, false);
    const std::vector<GuestVector> repeated{
        {-0x1000000, -0x1000000}, {-0x1000000, 0x1000000}, {0x1000000, 0x1000000}, {0x1000000, -0x1000000},
        {-0x1000000, -0x1000000}, {-0x1000000, 0x1000000}, {0x1000000, 0x1000000}, {0x1000000, -0x1000000},
        {-0x1000000, -0x1000000}, {-0x1000000, 0x1000000}, {0x1000000, 0x1000000}, {0x1000000, -0x1000000}
    };
    if constexpr (sizeof(FT_Pos) < sizeof(std::int64_t)) {
        bool threw = false;
        try { FT_Outline_Embolden_nid_postfix(&empty, std::numeric_limits<std::int64_t>::max()); }
        catch (const std::runtime_error&) { threw = true; }
        Require(threw);
        rectangle[0].x = static_cast<std::int64_t>(std::numeric_limits<FT_Pos>::max()) + 1;
        threw = false;
        try { FT_Outline_Embolden_nid_postfix(&rectangleOutline, 64); }
        catch (const std::runtime_error&) { threw = true; }
        Require(threw);
        auto points = repeated;
        std::array<std::uint16_t, 3> contours{3, 7, 11};
        std::array<unsigned char, 12> tags{};
        GuestOutline outline{3, 12, points.data(), tags.data(), contours.data(), 0};
        threw = false;
        try { FT_Outline_EmboldenXY_nid_postfix(&outline, 64, 32); }
        catch (const std::runtime_error&) { threw = true; }
        Require(threw);
        for (std::size_t index = 0; index < points.size(); ++index) {
            Require(points[index].x == repeated[index].x && points[index].y == repeated[index].y);
        }
    } else {
        Compare(repeated, {3, 7, 11}, 64, 32, false);
    }
    Render();
}
