#pragma once

#include "prx/libSceIcu/Memory.hpp"
#include <cstdint>

extern "C" {

std::int32_t APS5_VABI T_CString_integerToString_nid_postfix(char* buffer, std::int32_t number, std::int32_t radix);
std::int32_t APS5_VABI T_CString_int64ToString_nid_postfix(char* buffer, std::int64_t number, std::uint32_t radix);
std::int32_t APS5_VABI T_CString_stringToInteger_nid_postfix(const char* text, std::int32_t radix);
char* APS5_VABI T_CString_toLowerCase_nid_postfix(char* text);
char* APS5_VABI T_CString_toUpperCase_nid_postfix(char* text);
int APS5_VABI uprv_stricmp_nid_postfix(const char* left, const char* right);
int APS5_VABI uprv_strnicmp_nid_postfix(const char* left, const char* right, std::uint32_t count);
char* APS5_VABI uprv_strdup_nid_postfix(const char* source);
char* APS5_VABI uprv_strndup_nid_postfix(const char* source, std::int32_t count);
std::int8_t APS5_VABI uprv_pathIsAbsolute_nid_postfix(const char* path);
void APS5_VABI u_setMemoryFunctions_nid_postfix(const void* context, IcuMemory::AllocateCallback allocate, IcuMemory::ReallocateCallback reallocate, IcuMemory::FreeCallback free, std::int32_t* status);

}
