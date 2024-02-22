#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef unsigned int uint;

typedef u8 Bool8;
enum { false = 0, true };

typedef struct SmallString {
    char Data[64];
} SmallString;

#define STATIC_ARRAY_SIZE(Arr) (sizeof(Arr) / sizeof((Arr)[0]))
#define IN_RANGE(LowRange, n, HighRange) ((LowRange) <= (n) && (n) <= (HighRange))
#define strfy_(x) #x
#define STRFY(x) strfy_(x)


#ifdef DEBUG
#  include <stdio.h>
#  include <stdlib.h>
#  define DEBUG_ASSERT(x) do {\
    if (!(x)) {\
        fprintf(stderr, "Assertion failed on line "STRFY(__LINE__)": " #x);\
        abort();\
    }\
} while (0)
#else
#  define DEBUG_ASSERT(x) (void)(x)
#endif /* DEBUG */


#define AAA(OpcodeByte) (((OpcodeByte) >> 5) & 0x7)
#define BBB(OpcodeByte) (((OpcodeByte) >> 2) & 0x7)
#define CC(OpcodeByte)  ((OpcodeByte) & 0x3)



#endif /* COMMON_H */

