#pragma once

#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "include/HashToken.hpp"

extern "C" {

std::int32_t APS5_VABI uhash_hashChars_nid_postfix(IcuPrimitives::HashToken);
std::int32_t APS5_VABI uhash_hashUChars_nid_postfix(IcuPrimitives::HashToken);
std::int32_t APS5_VABI uhash_hashIChars_nid_postfix(IcuPrimitives::HashToken);
std::int32_t APS5_VABI uhash_hashLong_nid_postfix(IcuPrimitives::HashToken);
std::int8_t APS5_VABI uhash_compareChars_nid_postfix(IcuPrimitives::HashToken, IcuPrimitives::HashToken);
std::int8_t APS5_VABI uhash_compareUChars_nid_postfix(IcuPrimitives::HashToken, IcuPrimitives::HashToken);
std::int8_t APS5_VABI uhash_compareIChars_nid_postfix(IcuPrimitives::HashToken, IcuPrimitives::HashToken);
std::int8_t APS5_VABI uhash_compareLong_nid_postfix(IcuPrimitives::HashToken, IcuPrimitives::HashToken);
std::int32_t APS5_VABI ustr_hashCharsN_nid_postfix(const char*, std::int32_t);
std::int32_t APS5_VABI ustr_hashUCharsN_nid_postfix(const char16_t*, std::int32_t);

}
