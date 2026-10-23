#include "SceTypes.hpp"
#include <cstddef>

struct LibreSslUi;
struct LibreSslUiString;
using UiOpener = int (APS5_VABI *)(LibreSslUi*);
using UiWriter = int (APS5_VABI *)(LibreSslUi*, LibreSslUiString*);

struct LibreSslUiMethod {
    char* name;
    UiOpener opener;
    UiWriter writer;
    UiOpener flusher;
    UiWriter reader;
    UiOpener closer;
    char* (APS5_VABI *promptConstructor)(LibreSslUi*, const char*, const char*);
};

static_assert(sizeof(LibreSslUiMethod) == 56);
static_assert(offsetof(LibreSslUiMethod, writer) == 16);
static_assert(offsetof(LibreSslUiMethod, reader) == 32);
static_assert(offsetof(LibreSslUiMethod, closer) == 40);

extern "C" {

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
