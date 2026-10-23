#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <bit>
#include <cerrno>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>

extern "C" {
double APS5_VABI uprv_floor_nid_postfix(double);
double APS5_VABI uprv_ceil_nid_postfix(double);
double APS5_VABI uprv_fabs_nid_postfix(double);
double APS5_VABI uprv_modf_nid_postfix(double, double*);
double APS5_VABI uprv_fmod_nid_postfix(double, double);
double APS5_VABI uprv_pow_nid_postfix(double, double);
double APS5_VABI uprv_pow10_nid_postfix(std::int32_t);
double APS5_VABI uprv_round_nid_postfix(double);
double APS5_VABI uprv_trunc_nid_postfix(double);
double APS5_VABI uprv_log_nid_postfix(double);
}

namespace {

constexpr std::uint64_t sign = 0x8000000000000000;
constexpr std::uint64_t infinity = 0x7ff0000000000000;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU numerical: %s\n", message);
    std::abort();
}

std::uint64_t Bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
}

double Value(std::uint64_t bits) {
    return std::bit_cast<double>(bits);
}

double Integral(double value, int direction) {
    const auto bits = Bits(value);
    const auto negative = (bits & sign) != 0;
    const auto exponent = static_cast<int>((bits >> 52) & 0x7ff) - 1023;
    if ((bits & ~sign) == 0 || exponent >= 52) return value;
    const bool step = direction < 0 ? negative : direction > 0 && !negative;
    if (exponent < 0) return step ? negative ? -1.0 : 1.0 : Value(bits & sign);
    const auto fractional = (std::uint64_t{1} << (52 - exponent)) - 1;
    const auto truncated = Value(bits & ~fractional);
    return step && (bits & fractional) != 0 ? truncated + (negative ? -1.0 : 1.0) : truncated;
}

void FiniteRounding() {
    std::uint64_t state = 0x1c078100a35de17;
    for (unsigned iteration = 0; iteration < 250000; ++iteration) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        if ((state & ~sign) >= infinity) continue;
        const auto value = Value(state);
        Require(Bits(uprv_floor_nid_postfix(value)) == Bits(Integral(value, -1)), "floor matches independently truncated binary significand");
        Require(Bits(uprv_ceil_nid_postfix(value)) == Bits(Integral(value, 1)), "ceiling matches independently truncated binary significand");
        Require(Bits(uprv_trunc_nid_postfix(value)) == Bits(Integral(value, 0)), "truncation preserves integral value and signed zero");
        Require(Bits(uprv_fabs_nid_postfix(value)) == (state & ~sign), "absolute value clears only sign");
    }
}

void RoundingSpecials() {
    for (const auto bits : {std::uint64_t{0}, sign, std::uint64_t{1}, sign | 1, infinity, sign | infinity}) {
        const auto value = Value(bits);
        if ((bits & ~sign) == infinity) {
            Require(Bits(uprv_trunc_nid_postfix(value)) == infinity, "source truncation returns positive infinity for either infinite input");
            Require(Bits(uprv_floor_nid_postfix(value)) == bits && Bits(uprv_ceil_nid_postfix(value)) == bits, "floor and ceiling preserve infinity sign");
        } else {
            Require(Bits(uprv_floor_nid_postfix(value)) == Bits(Integral(value, -1)), "floor zero/subnormal edge");
            Require(Bits(uprv_ceil_nid_postfix(value)) == Bits(Integral(value, 1)), "ceiling zero/subnormal edge");
        }
    }
    for (const auto payload : {std::uint64_t{1}, std::uint64_t{0x8000000000000}, std::uint64_t{0xfffffffffffff}}) {
        for (const auto polarity : {std::uint64_t{0}, sign}) {
            const auto value = Value(infinity | payload | polarity);
            Require(Bits(uprv_trunc_nid_postfix(value)) == 0x7ff8000000000000, "truncation canonicalizes every NaN payload/sign");
            Require(std::isnan(uprv_floor_nid_postfix(value)) && std::isnan(uprv_ceil_nid_postfix(value)), "floor and ceiling accept quiet/signaling NaNs");
            Require(Bits(uprv_fabs_nid_postfix(value)) == (infinity | payload), "absolute value preserves NaN payload and signaling bit");
            Require(std::isnan(uprv_round_nid_postfix(value)), "round accepts NaN");
        }
    }
    for (const auto pair : std::array<std::array<double, 2>, 8>{{{-2.5, -2}, {-1.5, -1}, {-0.5, 0}, {0.5, 1}, {1.5, 2}, {2.5, 3}, {-0.75, -1}, {-0.25, 0}}})
        Require(uprv_round_nid_postfix(pair[0]) == pair[1], "source rounding ties use floor(x+0.5)");
    Require(Bits(uprv_round_nid_postfix(-0.0)) == 0, "source rounding changes negative zero to positive zero");
    Require(uprv_round_nid_postfix(0x1.0000000000001p52) == 0x1.0000000000002p52, "source addition rounding at binary64 integer precision boundary");
    Require(Bits(uprv_round_nid_postfix(Value(infinity))) == infinity && Bits(uprv_round_nid_postfix(Value(infinity | sign))) == (infinity | sign), "round preserves infinite signs");
}

void Modf() {
    struct Guarded {
        std::uint64_t before;
        double integer;
        std::uint64_t after;
    };
    for (const auto value : {-2.75, -2.0, -0.75, -0.0, 0.0, 0.75, 2.0, 2.75}) {
        Guarded output{0x123456789abcdef0, 123, 0xfedcba9876543210};
        const auto fraction = uprv_modf_nid_postfix(value, &output.integer);
        Require(Bits(output.integer) == Bits(Integral(value, 0)), "modf integral output");
        Require(fraction + output.integer == value && std::fabs(fraction) < 1, "modf recomposes input with fractional magnitude below one");
        Require(std::signbit(fraction) == std::signbit(value), "modf fraction preserves negative-zero sign");
        Require(output.before == 0x123456789abcdef0 && output.after == 0xfedcba9876543210, "modf writes only destination double");
    }
    for (const auto polarity : {std::uint64_t{0}, sign}) {
        double integer = 0;
        Require(Bits(uprv_modf_nid_postfix(Value(infinity | polarity), &integer)) == polarity && Bits(integer) == (infinity | polarity), "modf separates signed infinity");
        Require(std::isnan(uprv_modf_nid_postfix(Value(infinity | 1 | polarity), &integer)) && std::isnan(integer), "modf propagates NaN to both outputs");
    }
    double aliased = -2.75;
    Require(uprv_modf_nid_postfix(aliased, &aliased) == -0.75 && aliased == -2, "modf reads value before overwriting same caller object");
    bool rejected = false;
    try { uprv_modf_nid_postfix(1.25, nullptr); } catch (const std::exception&) { rejected = true; }
    Require(rejected, "null modf output throws");
}

void Arithmetic() {
    const auto inf = Value(infinity);
    const auto nan = Value(infinity | 0x8000000000000);
    Require(uprv_fmod_nid_postfix(-5.25, 2) == -1.25 && uprv_fmod_nid_postfix(5.25, -2) == 1.25, "remainder follows dividend sign");
    Require(Bits(uprv_fmod_nid_postfix(-0.0, 2)) == sign && uprv_fmod_nid_postfix(2, inf) == 2, "remainder signed zero and infinite divisor");
    Require(std::isnan(uprv_fmod_nid_postfix(inf, 1)) && std::isnan(uprv_fmod_nid_postfix(1, 0)), "remainder invalid domains produce NaN");
    Require(uprv_pow_nid_postfix(-2, 3) == -8 && uprv_pow_nid_postfix(4, -0.5) == 0.5, "power signs and reciprocal roots");
    Require(Bits(uprv_pow_nid_postfix(-0.0, -3)) == (sign | infinity) && Bits(uprv_pow_nid_postfix(-0.0, 3)) == sign, "power signed zero with odd exponent");
    Require(Bits(uprv_pow_nid_postfix(-inf, -3)) == sign && uprv_pow_nid_postfix(nan, 0) == 1 && uprv_pow_nid_postfix(1, nan) == 1, "power infinity and NaN identities");
    Require(std::isnan(uprv_pow_nid_postfix(-1, 0.5)), "nonintegral power of negative base");
    Require(uprv_pow10_nid_postfix(0) == 1 && uprv_pow10_nid_postfix(1) == 10 && uprv_pow10_nid_postfix(3) == 1000, "integer decimal powers");
    Require(std::isfinite(uprv_pow10_nid_postfix(308)) && Bits(uprv_pow10_nid_postfix(309)) == infinity, "decimal power finite/overflow boundary");
    Require(uprv_pow10_nid_postfix(-323) > 0 && Bits(uprv_pow10_nid_postfix(-324)) == 0, "decimal power subnormal/underflow boundary");
    Require(Bits(uprv_pow10_nid_postfix(std::numeric_limits<std::int32_t>::max())) == infinity && Bits(uprv_pow10_nid_postfix(std::numeric_limits<std::int32_t>::min())) == 0, "full-width int32 exponent extremes");
    Require(Bits(uprv_log_nid_postfix(1)) == 0 && std::fabs(uprv_log_nid_postfix(0x1.5bf0a8b145769p1) - 1) < 1e-15, "natural logarithm values");
    Require(Bits(uprv_log_nid_postfix(0)) == (infinity | sign) && Bits(uprv_log_nid_postfix(-0.0)) == (infinity | sign), "logarithm zero poles");
    Require(std::isnan(uprv_log_nid_postfix(-1)) && std::isnan(uprv_log_nid_postfix(-inf)) && Bits(uprv_log_nid_postfix(inf)) == infinity, "logarithm domain and infinite input");
}

void Environment() {
    std::fenv_t saved;
    Require(std::fegetenv(&saved) == 0, "save native floating environment");
    for (const auto mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        Require(std::fesetround(mode) == 0, "set native rounding mode");
        const auto expected = mode == FE_TONEAREST || mode == FE_UPWARD ? 0x1.0000000000002p52 : 0x1.0000000000001p52;
        Require(uprv_round_nid_postfix(0x1.0000000000001p52) == expected, "rounding expression observes current addition rounding mode");
        Require(Bits(uprv_trunc_nid_postfix(-0.25)) == sign, "truncation remains toward zero under every native rounding mode");
    }
    Require(std::fesetround(FE_TONEAREST) == 0, "set nearest mode for domain checks");
    std::feclearexcept(FE_ALL_EXCEPT);
    errno = 0;
    const auto result = uprv_log_nid_postfix(-1);
    Require(std::isnan(result), "log domain remains NaN under explicit environment");
    if (math_errhandling & MATH_ERREXCEPT) Require((std::fetestexcept(FE_INVALID) & FE_INVALID) != 0, "native domain exception is observable");
    if (math_errhandling & MATH_ERRNO) Require(errno == EDOM, "native domain errno is observable");
    Require(std::fesetenv(&saved) == 0, "restore native floating environment");
}

} // namespace

int main() {
    FiniteRounding();
    RoundingSpecials();
    Modf();
    Arithmetic();
    Environment();
}
