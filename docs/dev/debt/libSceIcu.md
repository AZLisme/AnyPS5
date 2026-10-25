# libSceIcu technical debt

### Functional

- [UTF-16 set searches and tokenization](../../../core/libs/prx/libSceIcu/Tokenize.cpp) have no console verification; the console ICU version is unknown. Signatures and behavior follow ICU 78.1's [ustring.h](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/unicode/ustring.h) and [ustring.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustring.cpp). `u_strtok_r` preserves that source's one-code-unit delimiter overwrite, including the trailing surrogate left after splitting at a supplementary delimiter. Invalid null inputs and rejected guest-write scopes throw; an initial continuation assignment is retained if a later delimiter write is rejected. Set-search offsets beyond `INT32_MAX` throw; other library exports remain absent.
