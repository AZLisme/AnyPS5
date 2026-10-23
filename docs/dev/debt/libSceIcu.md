# libSceIcu technical debt

### Unknown function info

- [ICU exports](../../../core/libs/prx/libSceIcu/Unescape.cpp) implement only `u_unescape` and `u_unescapeAt` from the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceIcu.c); the other 659 named exports are not implemented here. Console ICU version, signatures and behaviour have not been verified.

### Functional

- [Escape decoding](../../../core/libs/prx/libSceIcu/Unescape.cpp) has no PS5 verification. It follows ICU 78.1's [ustring.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/ustring.cpp), including rejection of eight-digit braced hexadecimal escapes although its public header advertises up to eight digits. Null source or offset pointers, null callbacks with an in-range offset, negative capacity with a nonnull destination and source lengths exceeding `INT32_MAX` throw. Null-destination preflight ignores capacity. Closes when: compared with the console, including callback ABI, byte conversion, capacity limits and braced hexadecimal parsing.
