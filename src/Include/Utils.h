#ifndef UTILS_H
#define UTILS_H

#include "Common.h"

isize Strlen(const char *s);
void Memcpy(void *Dst, const void *Src, isize ByteCount);
void Memset(void *Dst, u8 Byte, isize ByteCount);
Bool8 Memcmp(const void *Dst, const void *Src, isize ByteCount);
u8 FlipByte(u8 Byte);
/* appends string to the buffer at a given index 'At', 
 * string will be truncated if the buffer does not have enough room */
isize AppendString(char *Buffer, isize BufferSize, isize At, const char *String);

/* appends a hexadecimal number to the buffer at a given index 'At'
 * string will be truncated if the buffer does not have enough room */
    /* NOTE: if Buffer does not have enough space for the number and the padding combined, 
     * but does for the number only. The padding will persist and the number will be truncated 
     * from the least significant digit, this is a feature and not a bug */
isize AppendHex(char *Buffer, isize BufferSize, isize At, int DigitCount, u32 Hex);

/*
 * BufferSize must not be zero and Bufer must be valid 
 * returns the bytes written - 1, always less than BufferSize
 * available format:
 *      {x<number>}: appends a hexadecimal number to the string buffer,
 *                  ensures that there are at least <number> amount of digits printed
 *      {s}: copy the argument string to the string buffer
 * example usage: 
 *   FormatString(Buf, BufSize, 
 *       "hex: {x4} {x3}\n", 0xdead, 0xbeef,
 *       "str: {s}", "my string"
 *       NULL
 *   );
 *   Buf = "hex: dead bee 
 *          str: my string"
 * */
isize FormatString(char *Buffer, isize BufferSize, ...);
isize FormatStringArgs(char *Buffer, isize BufferSize, va_list Args);

#endif /* UTILS_H */

