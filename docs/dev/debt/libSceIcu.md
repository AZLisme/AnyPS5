# libSceIcu technical debt

### Functional

- [UTF-16 scalar counting and search](../../../core/libs/prx/libSceIcu/Utf16.cpp): the PS5 ICU version and console equivalence remain unverified. The five APIs follow the stable public contracts and implementation of ICU 78.1 [ustring.h](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/unicode/ustring.h) and [ustring.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustring.cpp), including isolated-surrogate matches, counted embedded NULs, truncated pairs and invalid scalar rejection. Null search inputs that require a read and terminated counts beyond INT32_MAX throw; the public API does not define such valid inputs. Invalid buffers and overlapping concurrent modification remain caller errors.

- [UTF-16 scalar ABI](../../../core/libs/prx/libSceIcu/Utf16.cpp): console argument and return-type compatibility is not measured. Export names and NIDs are in the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceIcu.c). The adapter uses public ICU UChar as a 16-bit unit, UChar32 and lengths as signed 32 bits, UBool as signed 8 bits, and guest System V calling conventions; the inventory alone does not establish these types.
