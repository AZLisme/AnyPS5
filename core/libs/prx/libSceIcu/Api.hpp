#pragma once

#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"

extern "C" {

char16_t* APS5_VABI u_strFromJavaModifiedUTF8WithSub_nid_postfix(char16_t*, std::int32_t, std::int32_t*, const char*, std::int32_t, std::int32_t, std::int32_t*, std::int32_t*);
char* APS5_VABI u_strToJavaModifiedUTF8_nid_postfix(char*, std::int32_t, std::int32_t*, const char16_t*, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI u_terminateChars_nid_postfix(char*, std::int32_t, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI u_terminateUChars_nid_postfix(char16_t*, std::int32_t, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI u_terminateUChar32s_nid_postfix(std::int32_t*, std::int32_t, std::int32_t, std::int32_t*);
std::int32_t APS5_VABI u_terminateWChars_nid_postfix(std::uint16_t*, std::int32_t, std::int32_t, std::int32_t*);
char16_t* APS5_VABI u_strFromUTF32_nid_postfix(char16_t*, std::int32_t, std::int32_t*, const std::int32_t*, std::int32_t, std::int32_t*);
char16_t* APS5_VABI u_strFromUTF32WithSub_nid_postfix(char16_t*, std::int32_t, std::int32_t*, const std::int32_t*, std::int32_t, std::int32_t, std::int32_t*, std::int32_t*);
std::int32_t* APS5_VABI u_strToUTF32_nid_postfix(std::int32_t*, std::int32_t, std::int32_t*, const char16_t*, std::int32_t, std::int32_t*);
std::int32_t* APS5_VABI u_strToUTF32WithSub_nid_postfix(std::int32_t*, std::int32_t, std::int32_t*, const char16_t*, std::int32_t, std::int32_t, std::int32_t*, std::int32_t*);

}
