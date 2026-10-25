#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <cstddef>

static_assert(sizeof(LibreSslUiMethod) == 56);
static_assert(offsetof(LibreSslUiMethod, writer) == 16);
static_assert(offsetof(LibreSslUiMethod, reader) == 32);
static_assert(offsetof(LibreSslUiMethod, closer) == 40);

extern "C" {

LibreSslUiMethod* APS5_VABI UI_create_method_nid_postfix(char* name) {
    auto* method = static_cast<LibreSslUiMethod*>(calloc_nid_postfix(1, sizeof(LibreSslUiMethod)));
    if (method && name) method->name = strdup_nid_postfix(name);
    return method;
}

void APS5_VABI UI_destroy_method_nid_postfix(LibreSslUiMethod* method) {
    if (!method) NotImplemented_nid_no_patch(__func__);
    free_nid_postfix(method->name);
    method->name = nullptr;
    free_nid_postfix(method);
}

UiOpener APS5_VABI UI_method_get_opener_nid_postfix(LibreSslUiMethod* method) {
    return method ? method->opener : nullptr;
}

int APS5_VABI UI_method_set_opener_nid_postfix(LibreSslUiMethod* method, UiOpener opener) {
    if (!method) return -1;
    method->opener = opener;
    return 0;
}

int APS5_VABI UI_method_set_writer_nid_postfix(LibreSslUiMethod* method, UiWriter writer) {
    if (!method) return -1;
    method->writer = writer;
    return 0;
}


UiOpener APS5_VABI UI_method_get_closer_nid_postfix(LibreSslUiMethod* method) {
    return method ? method->closer : nullptr;
}

UiWriter APS5_VABI UI_method_get_reader_nid_postfix(LibreSslUiMethod* method) {
    return method ? method->reader : nullptr;
}

UiWriter APS5_VABI UI_method_get_writer_nid_postfix(LibreSslUiMethod* method) {
    return method ? method->writer : nullptr;
}

int APS5_VABI UI_method_set_closer_nid_postfix(LibreSslUiMethod* method, UiOpener closer) {
    if (!method) return -1;
    method->closer = closer;
    return 0;
}

int APS5_VABI UI_method_set_reader_nid_postfix(LibreSslUiMethod* method, UiWriter reader) {
    if (!method) return -1;
    method->reader = reader;
    return 0;
}

}
