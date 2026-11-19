#pragma once

#define KOI_INTERNAL_STRINGIFY(x) #x
#define KOI_STRINGIFY(x) KOI_INTERNAL_STRINGIFY(x)

#define KOI_INTERNAL_CONCAT(a, b) a##b
#define KOI_CONCAT(a, b) KOI_INTERNAL_CONCAT(a, b)

#define KOI_BIT(n)             (1UL << (n))
#define KOI_BIT_SET(val, n)    ((val) |= KOI_BIT(n))
#define KOI_BIT_CLEAR(val, n)  ((val) &= ~KOI_BIT(n))
#define KOI_BIT_TOGGLE(val, n) ((val) ^= KOI_BIT(n))
#define KOI_BIT_CHECK(val, n)  ((val) & KOI_BIT(n))

// Reflection expansion macros
#define KOI_PP_EXPAND(x) x
#define KOI_PP_FOR_EACH(M, C, ...) KOI_PP_EXPAND(KOI_CONCAT(KOI_PP_FE_, KOI_PP_NARGS(__VA_ARGS__))(M, C, __VA_ARGS__))
#define KOI_PP_NARGS(...) KOI_PP_EXPAND(KOI_PP_NARGS_I(__VA_ARGS__, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1))
#define KOI_PP_NARGS_I(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, N, ...) N
#define KOI_PP_FE_1(M, C, x) M(C, x)
#define KOI_PP_FE_2(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_1(M, C, __VA_ARGS__))
#define KOI_PP_FE_3(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_2(M, C, __VA_ARGS__))
#define KOI_PP_FE_4(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_3(M, C, __VA_ARGS__))
#define KOI_PP_FE_5(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_4(M, C, __VA_ARGS__))
#define KOI_PP_FE_6(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_5(M, C, __VA_ARGS__))
#define KOI_PP_FE_7(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_6(M, C, __VA_ARGS__))
#define KOI_PP_FE_8(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_7(M, C, __VA_ARGS__))
#define KOI_PP_FE_9(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_8(M, C, __VA_ARGS__))
#define KOI_PP_FE_10(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_9(M, C, __VA_ARGS__))
#define KOI_PP_FE_11(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_10(M, C, __VA_ARGS__))
#define KOI_PP_FE_12(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_11(M, C, __VA_ARGS__))
#define KOI_PP_FE_13(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_12(M, C, __VA_ARGS__))
#define KOI_PP_FE_14(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_13(M, C, __VA_ARGS__))
#define KOI_PP_FE_15(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_14(M, C, __VA_ARGS__))
#define KOI_PP_FE_16(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_15(M, C, __VA_ARGS__))
#define KOI_PP_FE_17(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_16(M, C, __VA_ARGS__))
#define KOI_PP_FE_18(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_17(M, C, __VA_ARGS__))
#define KOI_PP_FE_19(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_18(M, C, __VA_ARGS__))
#define KOI_PP_FE_20(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_19(M, C, __VA_ARGS__))
#define KOI_PP_FE_21(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_20(M, C, __VA_ARGS__))
#define KOI_PP_FE_22(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_21(M, C, __VA_ARGS__))
#define KOI_PP_FE_23(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_22(M, C, __VA_ARGS__))
#define KOI_PP_FE_24(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_23(M, C, __VA_ARGS__))
#define KOI_PP_FE_25(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_24(M, C, __VA_ARGS__))
#define KOI_PP_FE_26(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_25(M, C, __VA_ARGS__))
#define KOI_PP_FE_27(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_26(M, C, __VA_ARGS__))
#define KOI_PP_FE_28(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_27(M, C, __VA_ARGS__))
#define KOI_PP_FE_29(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_28(M, C, __VA_ARGS__))
#define KOI_PP_FE_30(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_29(M, C, __VA_ARGS__))
#define KOI_PP_FE_31(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_30(M, C, __VA_ARGS__))
#define KOI_PP_FE_32(M, C, x, ...) M(C, x), KOI_PP_EXPAND(KOI_PP_FE_31(M, C, __VA_ARGS__))
