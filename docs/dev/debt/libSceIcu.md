# libSceIcu technical debt

### Functional

- [Numerical primitives](../../../core/libs/prx/libSceIcu/Numerical.cpp): the console ICU version and exact PS5 libm rounding, NaN propagation, errno and floating-exception behavior remain unverified. Ten named exports follow the IEEE-754 path of [ICU 78.1 putil.cpp](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/putil.cpp) and [putilimp.h](https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/common/putilimp.h). Native libm supplies the underlying arithmetic; guest errno inherits the current runtime's host-libm handling. The source's `floor(x + 0.5)` rounding expression, canonical truncation NaN and positive result for either infinite truncation input are retained. A null `uprv_modf` output throws where upstream dereferences it. Other ICU exports remain unimplemented.
