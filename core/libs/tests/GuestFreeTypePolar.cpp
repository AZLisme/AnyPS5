#include "prx/libSceFreeType/libSceFreeType.hpp"

#include <ft2build.h>
#include FT_TRIGONOMETRY_H

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <numbers>
#include <source_location>
#include <stdexcept>

using FreeType::GuestVector;



namespace {

constexpr std::int64_t Turn = 360ll << 16;

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "FreeType polar helper regression at %s:%u\n", location.file_name(), location.line());
        std::abort();
    }
}

bool Equal(const GuestVector& left, const GuestVector& right) {
    return left.x == right.x && left.y == right.y;
}

FT_Angle Reduced(std::int64_t angle) {
    const auto value = angle % Turn;
    return static_cast<FT_Angle>(value == 0 && angle != 0 ? Turn : value);
}

void Check(GuestVector original, std::int64_t angle) {
    GuestVector actual = original;
    FT_Vector expected{static_cast<FT_Pos>(original.x), static_cast<FT_Pos>(original.y)};
    const auto referenceAngle = angle >= -Turn * 3 && angle <= Turn * 3 ? static_cast<FT_Angle>(angle) : Reduced(angle);
    FT_Vector_Rotate(&expected, referenceAngle);
    FT_Vector_Rotate_nid_postfix(&actual, angle);
    Require(actual.x == expected.x && actual.y == expected.y);
    const double radians = static_cast<double>(angle % Turn) / 65536 * std::numbers::pi / 180;
    Require(std::abs(actual.x - (original.x * std::cos(radians) - original.y * std::sin(radians))) <= 2);
    Require(std::abs(actual.y - (original.x * std::sin(radians) + original.y * std::cos(radians))) <= 2);
    actual = original;
    const auto length = FT_Vector_Length_nid_postfix(&actual);
    expected = {static_cast<FT_Pos>(original.x), static_cast<FT_Pos>(original.y)};
    Require(length == FT_Vector_Length(&expected) && Equal(actual, original));
    Require(std::abs(length - std::hypot(static_cast<double>(original.x), static_cast<double>(original.y))) <= 1);
    const auto direction = FT_Atan2_nid_postfix(original.x, original.y);
    Require(direction == FT_Atan2(expected.x, expected.y));
    if (original.x || original.y) {
        const double exact = std::atan2(static_cast<double>(original.y), static_cast<double>(original.x)) * 180 / std::numbers::pi;
        Require(std::abs(std::remainder(direction / 65536.0 - exact, 360)) < 1.0 / 1024);
    }
    std::int64_t actualLength = -19, actualAngle = -23;
    FT_Fixed expectedLength = -19;
    FT_Angle expectedAngle = -23;
    FT_Vector_Polarize(&expected, &expectedLength, &expectedAngle);
    FT_Vector_Polarize_nid_postfix(&actual, &actualLength, &actualAngle);
    Require(actualLength == expectedLength && actualAngle == expectedAngle && Equal(actual, original));
    FT_Vector_From_Polar(&expected, static_cast<FT_Fixed>(original.x), referenceAngle);
    FT_Vector_From_Polar_nid_postfix(&actual, original.x, angle);
    Require(actual.x == expected.x && actual.y == expected.y);
}

void TestSmallVectors() {
    constexpr std::array<std::int64_t, 13> angles{0, 1, -1, 30ll << 16, -(45ll << 16), 90ll << 16, -(180ll << 16), Turn, -Turn, Turn * 2 + 1, -Turn * 2 - 1, std::numeric_limits<std::int64_t>::max(), std::numeric_limits<std::int64_t>::min()};
    for (std::int64_t x = -32; x <= 32; ++x) {
        for (std::int64_t y = -32; y <= 32; ++y) {
            for (auto angle : angles) Check({x, y}, angle);
        }
    }
    Require(FT_Atan2_nid_postfix(1, 0) == 0);
    Require(FT_Atan2_nid_postfix(0, 1) == 90ll << 16);
    Require(FT_Atan2_nid_postfix(0, -1) == -(90ll << 16));
    Require(FT_Atan2_nid_postfix(-1, 0) == -(180ll << 16));
    Require(FT_Atan2_nid_postfix(0, 0) == 0);
    GuestVector triangle{3, 4};
    Require(FT_Vector_Length_nid_postfix(&triangle) == 5);
}

void TestWholeTurns() {
    for (const auto angle : {Turn, -Turn, Turn * 3, -Turn * 3}) {
        GuestVector vector{1000000000, 900000000};
        FT_Vector_Rotate_nid_postfix(&vector, angle);
        Require(Equal(vector, {999999972, 900000030}));
    }
}

void TestNullAndAliasing() {
    FT_Vector_Rotate_nid_postfix(nullptr, 1);
    FT_Vector_From_Polar_nid_postfix(nullptr, std::numeric_limits<std::int64_t>::min(), 1);
    Require(FT_Vector_Length_nid_postfix(nullptr) == 0);
    GuestVector vector{3, 4};
    std::int64_t length = 123, angle = 456;
    FT_Vector_Polarize_nid_postfix(nullptr, &length, &angle);
    FT_Vector_Polarize_nid_postfix(&vector, nullptr, &angle);
    FT_Vector_Polarize_nid_postfix(&vector, &length, nullptr);
    Require(length == 123 && angle == 456 && Equal(vector, {3, 4}));
    GuestVector zero{};
    FT_Vector_Polarize_nid_postfix(&zero, &length, &angle);
    Require(length == 123 && angle == 456);
    FT_Vector native{3, 4};
    FT_Fixed nativeLength{};
    FT_Angle nativeAngle{};
    FT_Vector_Polarize(&native, &nativeLength, &nativeAngle);
    FT_Vector_Polarize_nid_postfix(&vector, &vector.x, &vector.y);
    Require(Equal(vector, {nativeLength, nativeAngle}));
    vector = {3, 4};
    FT_Vector_Polarize_nid_postfix(&vector, &length, &length);
    Require(length == FT_Atan2(3, 4) && Equal(vector, {3, 4}));
}

void TestHostRotationBoundaries() {
    constexpr std::int64_t maximum = 0x7fffffff;
    for (const auto original : {GuestVector{maximum - 1, -1}, GuestVector{-maximum, 0}, GuestVector{maximum - 63, 0}, GuestVector{maximum - 64, 0}, GuestVector{-(maximum - 64), 0}}) {
        for (const auto angle : std::array<std::int64_t, 5>{90ll << 16, 180ll << 16, Turn, -Turn, 1}) {
            auto vector = original;
            if constexpr (sizeof(FT_Pos) == 8) {
                FT_Vector native{static_cast<FT_Pos>(original.x), static_cast<FT_Pos>(original.y)};
                FT_Vector_Rotate(&native, static_cast<FT_Angle>(angle));
                FT_Vector_Rotate_nid_postfix(&vector, angle);
                Require(Equal(vector, {native.x, native.y}));
            } else {
                const auto magnitude = [](std::int64_t value) { return value < 0 ? -value : value; };
                if (magnitude(original.x) + magnitude(original.y) > maximum - 64) {
                    bool rejected = false;
                    try { FT_Vector_Rotate_nid_postfix(&vector, angle); } catch (const std::runtime_error&) { rejected = true; }
                    Require(rejected && Equal(vector, original));
                    rejected = false;
                    try { FT_Vector_From_Polar_nid_postfix(&vector, original.x, angle); } catch (const std::runtime_error&) { rejected = true; }
                    Require(rejected && Equal(vector, original));
                } else {
                    FT_Vector native{static_cast<FT_Pos>(original.x), static_cast<FT_Pos>(original.y)};
                    FT_Vector_Rotate(&native, static_cast<FT_Angle>(angle));
                    FT_Vector_Rotate_nid_postfix(&vector, angle);
                    Require(Equal(vector, {native.x, native.y}));
                }
            }
        }
    }
    if constexpr (sizeof(FT_Pos) == 8) {
        GuestVector first{maximum - 1, -1};
        FT_Vector_Rotate_nid_postfix(&first, 90ll << 16);
        Require(Equal(first, {-64, 2147483648ll}));
        GuestVector second{-maximum, 0};
        FT_Vector_Rotate_nid_postfix(&second, 180ll << 16);
        Require(Equal(second, {2147483648ll, 66}));
    }
}

void TestWideAndUnknown() {
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    GuestVector vector{maximum, -maximum};
    FT_Vector_Rotate_nid_postfix(&vector, 0);
    Require(Equal(vector, {maximum, -maximum}));
    FT_Vector_From_Polar_nid_postfix(&vector, maximum, 0);
    Require(Equal(vector, {maximum, 0}) && FT_Vector_Length_nid_postfix(&vector) == maximum);
    vector = {0, -maximum};
    Require(FT_Vector_Length_nid_postfix(&vector) == maximum);
    vector = {std::numeric_limits<std::int64_t>::min(), 0};
    bool threw = false;
    try { FT_Vector_Length_nid_postfix(&vector); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
    const GuestVector outside{1ll << 40, 2};
    vector = outside;
    threw = false;
    try { FT_Vector_Rotate_nid_postfix(&vector, 1); } catch (const std::runtime_error&) { threw = true; }
    Require(threw && Equal(vector, outside));
    std::int64_t length = 123, angle = 456;
    threw = false;
    try { FT_Vector_Polarize_nid_postfix(&vector, &length, &angle); } catch (const std::runtime_error&) { threw = true; }
    Require(threw && length == 123 && angle == 456 && Equal(vector, outside));
    threw = false;
    try { FT_Atan2_nid_postfix(outside.x, outside.y); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
    threw = false;
    try { FT_Vector_Length_nid_postfix(&vector); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);
    threw = false;
    try { FT_Vector_From_Polar_nid_postfix(&vector, outside.x, 1); } catch (const std::runtime_error&) { threw = true; }
    Require(threw && Equal(vector, outside));
    for (const GuestVector wide : {GuestVector{0x3fffffff, -0x3fffffff}, GuestVector{0x40000000, 0x40000000}, GuestVector{0x7fffffff, -0x7fffffff}}) {
        vector = wide;
        FT_Vector native{static_cast<FT_Pos>(wide.x), static_cast<FT_Pos>(wide.y)};
        Require(FT_Vector_Length_nid_postfix(&vector) == static_cast<std::uint32_t>(FT_Vector_Length(&native)));
        FT_Fixed expectedLength{};
        FT_Angle expectedAngle{};
        FT_Vector_Polarize(&native, &expectedLength, &expectedAngle);
        FT_Vector_Polarize_nid_postfix(&vector, &length, &angle);
        Require(length == static_cast<std::uint32_t>(expectedLength) && angle == expectedAngle && Equal(vector, wide));
        if constexpr (sizeof(FT_Pos) == 8) {
            FT_Vector_Rotate(&native, 45ll << 16);
            FT_Vector_Rotate_nid_postfix(&vector, 45ll << 16);
            Require(vector.x == native.x && vector.y == native.y);
        } else if (wide.x == 0x7fffffff) {
            threw = false;
            try { FT_Vector_Rotate_nid_postfix(&vector, 45ll << 16); } catch (const std::runtime_error&) { threw = true; }
            Require(threw && Equal(vector, wide));
        }
    }
    if constexpr (sizeof(FT_Pos) == 8) {
        vector = {0xffffffffll, -0xffffffffll};
        FT_Vector native{static_cast<FT_Pos>(vector.x), static_cast<FT_Pos>(vector.y)};
        FT_Fixed nativeLength{};
        FT_Angle nativeAngle{};
        FT_Vector_Polarize(&native, &nativeLength, &nativeAngle);
        FT_Vector_Polarize_nid_postfix(&vector, &length, &angle);
        Require(length == nativeLength && angle == nativeAngle);
        Require(FT_Atan2_nid_postfix(vector.x, vector.y) == FT_Atan2(native.x, native.y));
    }
}

}

int main() {
    TestSmallVectors();
    TestWholeTurns();
    TestHostRotationBoundaries();
    TestNullAndAliasing();
    TestWideAndUnknown();
}
