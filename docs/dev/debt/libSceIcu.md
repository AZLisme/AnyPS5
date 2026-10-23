# libSceIcu technical debt

### Functional

- [UTF-16 lexical comparisons](../../../core/libs/prx/libSceIcu/Comparisons.cpp): console equivalence and the console ICU version remain unverified. The five exports follow [ICU 78.1 ustring.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustring.cpp) and its public UTF-16 ABI. Required null buffers throw where upstream dereferences them; nonpositive `u_strncmp`/`u_memcmp` counts and a zero code-point comparison count or identical code-point buffers read nothing. Negative `u_memcmpCodePointOrder` counts select NUL-terminated lengths, preserving the first differing code-unit result. Strings whose length cannot fit int32 throw instead of narrowing the length. Other ICU exports remain unimplemented.
