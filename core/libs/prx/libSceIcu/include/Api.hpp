#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

extern "C" {

void APS5_VABI u_charsToUChars_nid_postfix(const char* source, char16_t* destination, std::int32_t length);
void APS5_VABI u_UCharsToChars_nid_postfix(const char16_t* source, char* destination, std::int32_t length);
std::int8_t APS5_VABI uprv_isInvariantString_nid_postfix(const char* source, std::int32_t length);
std::int8_t APS5_VABI uprv_isInvariantUString_nid_postfix(const char16_t* source, std::int32_t length);
std::int32_t APS5_VABI uprv_compareASCIIPropertyNames_nid_postfix(const char* first, const char* second);
std::int32_t APS5_VABI uprv_compareEBCDICPropertyNames_nid_postfix(const char* first, const char* second);
std::int32_t APS5_VABI uprv_compareInvEbcdicAsAscii_nid_postfix(const char* first, const char* second);
char APS5_VABI uprv_ebcdicToAscii_nid_postfix(char value);
char APS5_VABI uprv_ebcdicToLowercaseAscii_nid_postfix(char value);
std::uint8_t* APS5_VABI uprv_aestrncpy_nid_postfix(std::uint8_t* destination, const std::uint8_t* source, std::int32_t count);

}
