# libSceIcu technical debt

### Functional

- [UTF-8 and UTF-16 conversions](../../../core/libs/prx/libSceIcu/UtfConversions.cpp): exact PS5 version equivalence is unverified. The five conversion exports follow ICU 78.1 [ustrtrns.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustrtrns.cpp), including strict/substitution handling, lenient malformed-byte output, error preservation and buffer termination. A null error pointer and lengths whose result cannot fit the signed 32-bit ICU API throw; ICU requires a valid error pointer and does not define useful overflow behavior there. Source and destination must not overlap, as required by ICU. Other ICU functions remain unimplemented.
