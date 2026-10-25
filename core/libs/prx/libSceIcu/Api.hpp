#pragma once

#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "HashTable.hpp"

extern "C" {

IcuHash::Table* APS5_VABI uhash_open_nid_postfix(IcuHash::Hasher, IcuHash::Comparator, IcuHash::Comparator, std::int32_t*);
void APS5_VABI uhash_close_nid_postfix(IcuHash::Table*);
std::int32_t APS5_VABI uhash_count_nid_postfix(const IcuHash::Table*);
void* APS5_VABI uhash_put_nid_postfix(IcuHash::Table*, void*, void*, std::int32_t*);
void* APS5_VABI uhash_get_nid_postfix(const IcuHash::Table*, const void*);
void* APS5_VABI uhash_remove_nid_postfix(IcuHash::Table*, const void*);
void APS5_VABI uhash_removeAll_nid_postfix(IcuHash::Table*);
const IcuHash::Element* APS5_VABI uhash_nextElement_nid_postfix(const IcuHash::Table*, std::int32_t*);
IcuHash::Deleter APS5_VABI uhash_setKeyDeleter_nid_postfix(IcuHash::Table*, IcuHash::Deleter);
IcuHash::Deleter APS5_VABI uhash_setValueDeleter_nid_postfix(IcuHash::Table*, IcuHash::Deleter);

}
