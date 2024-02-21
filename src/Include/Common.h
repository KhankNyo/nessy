#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;

typedef u8 Bool8;
enum { false = 0, true };

typedef struct SmallString {
    char Data[64];
} SmallString;

#define STATIC_ARRAY_SIZE(Arr) (sizeof(Arr) / sizeof((Arr)[0]))


#define AAA(OpcodeByte) (((OpcodeByte) >> 5) & 0x7)
#define BBB(OpcodeByte) (((OpcodeByte) >> 2) & 0x7)
#define CC(OpcodeByte)  ((OpcodeByte) & 0x3)



#endif /* COMMON_H */

