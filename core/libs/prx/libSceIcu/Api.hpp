#pragma once

#include "prx/libSceIcu/GuestText.hpp"

extern "C" {
std::int32_t APS5_VABI utf8_appendCharSafeBody_nid_postfix(std::uint8_t*, std::int32_t, std::int32_t, std::int32_t, std::int8_t*);
std::int32_t APS5_VABI utf8_nextCharSafeBody_nid_postfix(const std::uint8_t*, std::int32_t*, std::int32_t, std::int32_t, std::int8_t);
std::int32_t APS5_VABI utf8_prevCharSafeBody_nid_postfix(const std::uint8_t*, std::int32_t, std::int32_t*, std::int32_t, std::int8_t);
std::int32_t APS5_VABI utf8_back1SafeBody_nid_postfix(const std::uint8_t*, std::int32_t, std::int32_t);
std::int32_t APS5_VABI u_strCompareIter_nid_postfix(Icu::Iterator*, Icu::Iterator*, std::int8_t);
std::int32_t APS5_VABI utext_current32_nid_postfix(Icu::Text*);
std::int32_t APS5_VABI utext_next32_nid_postfix(Icu::Text*);
std::int32_t APS5_VABI utext_previous32_nid_postfix(Icu::Text*);
std::int32_t APS5_VABI utext_next32From_nid_postfix(Icu::Text*, std::int64_t);
std::int32_t APS5_VABI utext_previous32From_nid_postfix(Icu::Text*, std::int64_t);
}
