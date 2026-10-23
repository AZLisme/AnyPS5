#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

double APS5_VABI uprv_floor_nid_postfix(double value);
double APS5_VABI uprv_ceil_nid_postfix(double value);
double APS5_VABI uprv_fabs_nid_postfix(double value);
double APS5_VABI uprv_modf_nid_postfix(double value, double* integer);
double APS5_VABI uprv_fmod_nid_postfix(double value, double divisor);
double APS5_VABI uprv_pow_nid_postfix(double value, double exponent);
double APS5_VABI uprv_pow10_nid_postfix(std::int32_t exponent);
double APS5_VABI uprv_round_nid_postfix(double value);
double APS5_VABI uprv_trunc_nid_postfix(double value);
double APS5_VABI uprv_log_nid_postfix(double value);

}
