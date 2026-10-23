#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>

static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53);

extern "C" {

double APS5_VABI uprv_floor_nid_postfix(double value) {
    return std::floor(value);
}

double APS5_VABI uprv_ceil_nid_postfix(double value) {
    return std::ceil(value);
}

double APS5_VABI uprv_fabs_nid_postfix(double value) {
    return std::fabs(value);
}

double APS5_VABI uprv_modf_nid_postfix(double value, double* integer) {
    if (integer == nullptr) NotImplemented_nid_no_patch(__func__);
    GuestArena::HostWrite access(integer, sizeof(*integer));
    return std::modf(value, integer);
}

double APS5_VABI uprv_fmod_nid_postfix(double value, double divisor) {
    return std::fmod(value, divisor);
}

double APS5_VABI uprv_pow_nid_postfix(double value, double exponent) {
    return std::pow(value, exponent);
}

double APS5_VABI uprv_pow10_nid_postfix(std::int32_t exponent) {
    return std::pow(10.0, static_cast<double>(exponent));
}

double APS5_VABI uprv_round_nid_postfix(double value) {
    return std::floor(value + 0.5);
}

double APS5_VABI uprv_trunc_nid_postfix(double value) {
    constexpr std::uint64_t infinity = 0x7ff0000000000000;
    const auto bits = std::bit_cast<std::uint64_t>(value);
    const auto magnitude = bits & 0x7fffffffffffffff;
    if (magnitude > infinity) return std::bit_cast<double>(std::uint64_t{0x7ff8000000000000});
    if (magnitude == infinity) return std::numeric_limits<double>::infinity();
    return (bits >> 63) != 0 ? std::ceil(value) : std::floor(value);
}

double APS5_VABI uprv_log_nid_postfix(double value) {
    return std::log(value);
}

}
