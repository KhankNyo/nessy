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
#ifndef true 
#  define true 1
#else 
#  define false 0
#endif /* Bool8 */

typedef struct SmallString {
    char Data[64];
} SmallString;



#endif /* COMMON_H */

