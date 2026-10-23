#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <numbers>
#include <source_location>
#include <thread>

extern "C" {
double APS5_VABI lgamma_r_nid_postfix(double, int*);
float APS5_VABI lgammaf_r_nid_postfix(float, int*);
double APS5_VABI gamma_r_nid_postfix(double, int*);
float APS5_VABI gammaf_r_nid_postfix(float, int*);
extern int signgam_nid_postfix;
}

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "reentrant gamma check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

template<typename TFloat, typename TFunction>
void Check(TFunction function, TFloat input, TFloat expected, int expectedSign, TFloat tolerance) {
    int sign = 0x12345678;
    errno = 1234;
    const TFloat result = function(input, &sign);
    Require(errno == 1234 && sign == expectedSign);
    Require(std::fabs(result - expected) <= tolerance);
}

template<typename TFloat, typename TFunction>
void Special(TFunction function) {
    constexpr auto infinity = std::numeric_limits<TFloat>::infinity();
    for (TFloat input : {TFloat{0}, -TFloat{0}, TFloat{-1}, TFloat{-2}, TFloat{-100}, -std::numeric_limits<TFloat>::max()}) {
        int sign = 99;
        errno = 1234;
        std::feclearexcept(FE_ALL_EXCEPT);
        const auto result = function(input, &sign);
        Require(std::isinf(result) && !std::signbit(result));
        Require(sign == (input == 0 && std::signbit(input) ? -1 : 1) && errno == 1234);
        Require(std::fetestexcept(FE_ALL_EXCEPT) == FE_DIVBYZERO);
    }
    for (TFloat input : {infinity, -infinity, std::numeric_limits<TFloat>::quiet_NaN()}) {
        int sign = 99;
        errno = 1234;
        std::feclearexcept(FE_ALL_EXCEPT);
        const auto result = function(input, &sign);
        Require(sign == 1 && errno == 1234);
        Require(std::isnan(input) ? std::isnan(result) : std::isinf(result) && !std::signbit(result));
        Require(std::fetestexcept(FE_ALL_EXCEPT) == 0);
    }
    for (TFloat input : {TFloat{1}, TFloat{2}}) {
        int sign = 99;
        std::feclearexcept(FE_ALL_EXCEPT);
        Require(function(input, &sign) == 0 && sign == 1 && std::fetestexcept(FE_ALL_EXCEPT) == 0);
    }
    int sign;
    errno = 1234;
    std::feclearexcept(FE_ALL_EXCEPT);
    Require(std::isinf(function(std::numeric_limits<TFloat>::max(), &sign)) && sign == 1 && errno == 1234);
    Require((std::fetestexcept(FE_ALL_EXCEPT) & (FE_OVERFLOW | FE_INEXACT)) == (FE_OVERFLOW | FE_INEXACT));
    std::feclearexcept(FE_ALL_EXCEPT);
    std::feraiseexcept(FE_INVALID);
    function(TFloat{1}, &sign);
    Require(std::fetestexcept(FE_ALL_EXCEPT) == FE_INVALID);
    RequireThrows([&] { function(TFloat{1}, nullptr); });
}

template<typename TFloat, typename TFunction>
void Finite(TFunction function, TFloat tolerance) {
    const auto halfLogPi = std::log(std::numbers::pi_v<TFloat>) / 2;
    Check(function, TFloat{0.5}, halfLogPi, 1, tolerance);
    Check(function, TFloat{-0.5}, halfLogPi + std::log(TFloat{2}), -1, tolerance);
    Check(function, TFloat{-1.5}, halfLogPi + std::log(TFloat{4} / 3), 1, tolerance);
    Check(function, TFloat{3}, std::log(TFloat{2}), 1, tolerance);
    Check(function, TFloat{6}, std::log(TFloat{120}), 1, tolerance);
    const auto tiny = std::numeric_limits<TFloat>::denorm_min();
    Check(function, tiny, -std::log(tiny), 1, tolerance);
    Check(function, -tiny, -std::log(tiny), -1, tolerance);
    Check(function, TFloat{0x1p-60}, -std::log(TFloat{0x1p-60}), 1, tolerance);
    Check(function, TFloat{-0x1p-60}, -std::log(TFloat{0x1p-60}), -1, tolerance);
    for (int number = 1; number < 1000; ++number) {
        const auto input = -static_cast<TFloat>(number) - TFloat{0.25};
        int sign = 99;
        Require(std::isfinite(function(input, &sign)) && sign == (number % 2 ? 1 : -1));
        const auto left = std::nextafter(-static_cast<TFloat>(number), -std::numeric_limits<TFloat>::infinity());
        const auto right = std::nextafter(-static_cast<TFloat>(number), TFloat{0});
        function(left, &sign);
        Require(sign == (number % 2 ? 1 : -1));
        function(right, &sign);
        Require(sign == (number % 2 ? -1 : 1));
    }
}

void Concurrent() {
    signgam_nid_postfix = 314159;
#ifndef _WIN32
    const auto savedHostSign = ::signgam;
    ::signgam = 271828;
#endif
    std::atomic<bool> valid = true;
    std::array<std::thread, 8> threads;
    for (int index = 0; index < static_cast<int>(threads.size()); ++index) {
        threads[index] = std::thread([&, index] {
            for (int iteration = 0; iteration < 2000; ++iteration) {
                const double input = -(index + iteration % 20) - 0.5;
                int sign = 0;
                errno = 1234;
                const double result = lgamma_r_nid_postfix(input, &sign);
                if (!std::isfinite(result) || sign != ((index + iteration % 20) % 2 ? 1 : -1) || errno != 1234)
                    valid = false;
                const float other = gammaf_r_nid_postfix(static_cast<float>(input), &sign);
                if (!std::isfinite(other) || sign != ((index + iteration % 20) % 2 ? 1 : -1) || errno != 1234)
                    valid = false;
            }
        });
    }
    for (auto& thread : threads) thread.join();
    Require(valid && signgam_nid_postfix == 314159);
#ifndef _WIN32
    Require(::signgam == 271828);
    ::signgam = savedHostSign;
#endif
}

template<typename TFloat, typename TFunction>
void ReductionBoundaries(TFunction function) {
    const std::array<TFloat, 9> inputs = [] {
        if constexpr (sizeof(TFloat) == 4)
            return std::array<TFloat, 9>{-0x1.fffffep20f, -0x1.000002p21f, -0x1.00000ap21f,
                -0x1.fffffep21f, -0x1.000002p22f, -0x1.7ffffep22f, -0x1.800002p22f,
                -0x1.800006p22f, -0x1.fffffep22f};
        else
            return std::array<TFloat, 9>{-0x1.fffffffffffffp49, -0x1.0000000000001p50, -0x1.0000000000005p50,
                -0x1.fffffffffffffp50, -0x1.0000000000001p51, -0x1.7ffffffffffffp51, -0x1.8000000000001p51,
                -0x1.8000000000003p51, -0x1.fffffffffffffp51};
    }();
    constexpr int signs[] = {1, -1, -1, 1, -1, 1, 0, 0, 0};
    for (std::size_t index = 0; index < inputs.size(); ++index) {
        int sign = 0x12345678;
        errno = 1234;
        if (signs[index] == 0) {
            RequireThrows([&] { function(inputs[index], &sign); });
            Require(sign == 0x12345678 && errno == 1234);
        } else {
            std::feclearexcept(FE_ALL_EXCEPT);
            Require(std::isfinite(function(inputs[index], &sign)) && sign == signs[index] && errno == 1234);
            Require(std::fetestexcept(FE_ALL_EXCEPT) == FE_INEXACT);
        }
    }
}

} // namespace

int main() {
    for (auto function : {lgamma_r_nid_postfix, gamma_r_nid_postfix}) {
        Special<double>(function);
        Finite(function, 0x1p-45);
        ReductionBoundaries<double>(function);
    }
    for (auto function : {lgammaf_r_nid_postfix, gammaf_r_nid_postfix}) {
        Special<float>(function);
        Finite(function, 0x1p-16f);
        ReductionBoundaries<float>(function);
    }
    Concurrent();
    std::puts("reentrant gamma tests passed");
}
