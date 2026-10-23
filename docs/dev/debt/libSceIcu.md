# libSceIcu technical debt

### Functional

- [Hash primitives](../../../core/libs/prx/libSceIcu/src/HashPrimitives.cpp): the PS5 ICU version and console ABI/behavior remain unverified. The eight `uhash` primitive callbacks and `ustr_hashCharsN`/`ustr_hashUCharsN` follow ICU 78.1 [uhash.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/uhash.cpp), [ustring.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustring.cpp) and the LP64 `UHashTok` union. ASCII-insensitive callbacks fold only A-Z, independent of locale; UTF-16 callbacks compare/hash code units without Unicode validation or case folding. Non-null counted lengths below INT32_MIN+32, and terminated inputs exceeding INT32_MAX units, throw where the source's length arithmetic overflows. Other nonpositive counted lengths and null counted inputs return zero without reading units.
