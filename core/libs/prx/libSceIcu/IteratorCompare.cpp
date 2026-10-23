#include "libSceIcu.hpp"
#include "prx/libc/include/General.hpp"

namespace {

bool Trail(std::int32_t value) { return (value & 0xfc00) == 0xdc00; }
bool Lead(std::int32_t value) { return (value & 0xfc00) == 0xd800; }

bool Paired(Icu::Iterator* iterator, std::int32_t value) {
    if (value <= 0xdbff) {
        if (!iterator->current) NotImplemented_nid_no_patch(__func__);
        return Trail(iterator->current(iterator));
    }
    if (Trail(value)) {
        if (!iterator->previous) NotImplemented_nid_no_patch(__func__);
        iterator->previous(iterator);
        if (!iterator->previous) NotImplemented_nid_no_patch(__func__);
        return Lead(iterator->previous(iterator));
    }
    return false;
}

} // namespace

extern "C" std::int32_t APS5_VABI u_strCompareIter_nid_postfix(Icu::Iterator* first, Icu::Iterator* second, std::int8_t codePointOrder) {
    if (!first || !second || first == second) return 0;
    if (!first->move) NotImplemented_nid_no_patch(__func__);
    first->move(first, 0, 0);
    if (!second->move) NotImplemented_nid_no_patch(__func__);
    second->move(second, 0, 0);
    std::int32_t left, right;
    do {
        if (!first->next) NotImplemented_nid_no_patch(__func__);
        left = first->next(first);
        if (!second->next) NotImplemented_nid_no_patch(__func__);
        right = second->next(second);
        if (left == -1 && right == -1) return 0;
    } while (left == right);
    if (left >= 0xd800 && right >= 0xd800 && codePointOrder) {
        if (!Paired(first, left)) left -= 0x2800;
        if (!Paired(second, right)) right -= 0x2800;
    }
    return left - right;
}
