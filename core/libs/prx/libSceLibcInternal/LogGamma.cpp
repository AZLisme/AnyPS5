#include "prx/libc/include/General.hpp"
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cmath>
#include <limits>
#include <numbers>
#include <type_traits>

#ifdef _WIN32
extern "C" double __lgamma_r(double value, int* sign);
extern "C" float __lgammaf_r(float value, int* sign);
#endif

namespace {

class ErrnoGuard {
public:
    ErrnoGuard() : saved(errno) {}
    ~ErrnoGuard() { errno = saved; }
    ErrnoGuard(const ErrnoGuard&) = delete;
    ErrnoGuard& operator=(const ErrnoGuard&) = delete;
private:
    int saved;
};

double PositiveLogGamma(double value) {
    int sign;
#ifdef _WIN32
    return __lgamma_r(value, &sign);
#else
    return ::lgamma_r(value, &sign);
#endif
}

float PositiveLogGamma(float value) {
    int sign;
#ifdef _WIN32
    return __lgammaf_r(value, &sign);
#else
    return ::lgammaf_r(value, &sign);
#endif
}

template<typename TFloat>
TFloat NegativeSine(TFloat magnitude, TFloat bias) {
    volatile TFloat rounded = magnitude + TFloat{4} * bias;
    if (static_cast<TFloat>(rounded) - TFloat{4} * bias == magnitude) return 0;
    rounded = magnitude + bias;
    using TBits = std::conditional_t<sizeof(TFloat) == 4, std::uint32_t, std::uint64_t>;
    auto quadrant = std::bit_cast<TBits>(static_cast<TFloat>(rounded)) & 7;
    TFloat base = static_cast<TFloat>(rounded) - bias;
    if (base > magnitude) {
        base -= TFloat{0.25};
        quadrant = (quadrant - 1) & 7;
    }
    const TFloat reduced = magnitude - base + static_cast<TFloat>(quadrant) * TFloat{0.25};
    constexpr TFloat pi = std::numbers::pi_v<TFloat>;
    TFloat phase;
    bool cosine = false;
    TFloat direction = -1;
    switch (quadrant) {
    case 0: phase = reduced; break;
    case 1:
    case 2: phase = TFloat{0.5} - reduced; cosine = true; break;
    case 3:
    case 4: phase = TFloat{1} - reduced; break;
    case 5:
    case 6: phase = reduced - TFloat{1.5}; cosine = true; direction = 1; break;
    default: phase = reduced - TFloat{2}; break;
    }
    if (std::fabs(phase) > TFloat{0.25})
        NotImplemented_nid_no_patch("lgamma_r: FreeBSD reduction outside the trigonometric kernel domain");
    return direction * (cosine ? std::cos(pi * phase) : std::sin(pi * phase));
}

template<typename TFloat>
TFloat ComputeLogGamma(TFloat value, int& sign, TFloat tiny, TFloat large, TFloat bias) {
    if (!std::isfinite(value)) return value * value;
    if (value == 0) {
        sign = std::signbit(value) ? -1 : 1;
        std::feraiseexcept(FE_DIVBYZERO);
        return std::numeric_limits<TFloat>::infinity();
    }
    const auto magnitude = std::fabs(value);
    if (magnitude < tiny) {
        sign = value < 0 ? -1 : 1;
        return -std::log(magnitude);
    }
    if (value < 0) {
        TFloat whole;
        const auto fraction = -std::modf(value, &whole);
        if (fraction == 0) {
            std::feraiseexcept(FE_DIVBYZERO);
            return std::numeric_limits<TFloat>::infinity();
        }
        const auto sine = NegativeSine(magnitude, bias);
        if (sine == 0) {
            std::feraiseexcept(FE_DIVBYZERO);
            return std::numeric_limits<TFloat>::infinity();
        }
        sign = sine < 0 ? -1 : 1;
        const auto adjustment = std::log(std::numbers::pi_v<TFloat> / std::fabs(magnitude * sine));
        return adjustment - PositiveLogGamma(magnitude);
    }
    if (value == 1 || value == 2) return 0;
    if (value >= large) return value * (std::log(value) - TFloat{1});
    return PositiveLogGamma(value);
}

template<typename TFloat>
TFloat LogGamma(TFloat value, int* sign, TFloat tiny, TFloat large, TFloat bias) {
    if (!sign) NotImplemented_nid_no_patch("lgamma_r: null sign output");
    const ErrnoGuard guard;
    int resultSign = 1;
    const auto result = ComputeLogGamma(value, resultSign, tiny, large, bias);
    *sign = resultSign;
    return result;
}

} // namespace

extern "C" {

double APS5_VABI lgamma_r_nid_postfix(double value, int* sign) {
    return LogGamma(value, sign, 0x1p-56, 0x1p56, 0x1p50);
}

float APS5_VABI lgammaf_r_nid_postfix(float value, int* sign) {
    return LogGamma(value, sign, 0x1p-27f, 0x1p27f, 0x1p21f);
}

double APS5_VABI gamma_r_nid_postfix(double value, int* sign) {
    return lgamma_r_nid_postfix(value, sign);
}

float APS5_VABI gammaf_r_nid_postfix(float value, int* sign) {
    return lgammaf_r_nid_postfix(value, sign);
}

}
