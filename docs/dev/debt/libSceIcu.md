# libSceIcu technical debt

### Functional

- [UTF-16 search, comparison and memory helpers](../../../core/libs/prx/libSceIcu/Search.cpp) have no console verification; the console ICU version is unknown. Their public signatures and behavior follow ICU 78.1's [ustring.h](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/unicode/ustring.h) and [ustring.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustring.cpp). Undefined null reads and writes throw; defined null and zero-count shortcuts remain available. Search paths that measure a 32-bit length throw if it exceeds `INT32_MAX`, instead of narrowing it; streaming reference paths retain their early returns. Other ICU exports remain absent.
