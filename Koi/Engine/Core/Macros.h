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
