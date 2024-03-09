#ifndef COMMON_H
#define COMMON_H

#include <stdarg.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef unsigned int uint;
typedef ptrdiff_t isize;
typedef size_t usize;

typedef u8 Bool8;
enum { false = 0, true };

typedef struct SmallString {
    char Data[64];
} SmallString;

typedef struct BufferData 
{
    void *ViewPtr; /* whoever creates the buffer data struct owns the pointer */
    isize SizeBytes;
} BufferData;



#define STATIC_ARRAY_SIZE(Arr) (sizeof(Arr) / sizeof((Arr)[0]))
#define IN_RANGE(LowRange, n, HighRange) ((LowRange) <= (n) && (n) <= (HighRange))
#define strfy_(x) #x
#define STRFY(x) strfy_(x)

#define BYTE_ARRAY(...) (u8[]){__VA_ARGS__}
#define MASKED_LOAD(AssginableDst, Expr, ExprMask) \
    (AssginableDst = ((AssginableDst) & ~(ExprMask)) | ((Expr) & (ExprMask)))

#define F32_LERP(Left, Right, Percentage) (((Right) - (Left)) * (Percentage))


#ifdef DEBUG
#  include <stdio.h>
#  include <stdlib.h>
#  define DEBUG_ASSERT(x) do {\
    if (!(x)) {\
        fprintf(stderr, "Assertion failed on line "STRFY(__LINE__)": " #x);\
        *((char*)0) = 0;\
    }\
} while (0)
#else
#  define DEBUG_ASSERT(x) (void)(x)
#endif /* DEBUG */


#define AAA(OpcodeByte) (((OpcodeByte) >> 5) & 0x7)
#define BBB(OpcodeByte) (((OpcodeByte) >> 2) & 0x7)
#define CC(OpcodeByte)  ((OpcodeByte) & 0x3)


#define MB (1024 * 1024)
#define KB (1024)



#endif /* COMMON_H */

