# libSceIcu technical debt

### Functional

- [UTF-16 copying](../../../core/libs/prx/libSceIcu/Copy.cpp): the console ICU version and exact copy/concatenation behavior are unverified. These five exports follow [ICU 78.1 ustring.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustring.cpp): `u_strncpy` stops at the first copied NUL without padding, and `u_strncat` appends up to the positive count of code units followed by NUL. The latter follows the implementation, despite its header's conflicting count-minus-one prose. Nonpositive counts leave the destination untouched, without reading either pointer. Code units, including unpaired surrogates, are copied without Unicode validation. As in the reference, callers supply sufficient storage and valid nonoverlapping ranges; invalid positive-count pointers and overlapping `u_memcpy` are outside its contract.
