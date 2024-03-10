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




#define PI  3.141592653589793
#define TAU 6.283185307179586


/* 
 * these trig functions are about an order of magnitude faster than the standard library's version, 
 * but they don't accept negative numbers and have high error percentage.
 * They are good enough for audio though
Error percentage: 
    are relative to libc's functions, note: Sin32 was compared with sin not sinf
    --------------- Sin64 ---------------
    Largest diff: 0.005566
    Largest err%: 21382742.743425%
    Ave err%: 5.426688%

    --------------- Sin32 ---------------
    Largest diff: 0.026799
    Largest err%: 21382565.680780%
    Ave err%: 8.757294%


Benchmark:
    source: 
"""""
    double ts = clock();
    for (double x = Start; x <= End; x += Delta) {
        volatile Typename _ = FnName(x);
    }
    printf("%s bench: %fms\n", #FnName, (clock() - ts) * 1000 / CLOCKS_PER_SEC);
""""""
    where   FnName is the individual function: sin, sinf, Sin32, Sin64
            Start = 0.0
            End = 200000.0
            Delta = 0.001
            Typename is float for: sinf, Sin32
                        double for sin,  Sin64
    Note that x always has to be of type double because 
        the sigfig for Delta + End (8) exceeds 32-bit float's maximum sigfig (7), 
        resulting in a + 0 instead of + .001
    Compiled with gcc 13.2.0, flags: -Ofast -flto
    Ran on an i5 7400

    Sin32 bench: 803.000000ms
    Sin64 bench: 684.000000ms
    sinf bench: 9168.000000ms
    sin bench: 8594.000000ms

    interestingly, the 32 bit float versions are slower than the 64 bit ones, 
    probably because of the implicit conversions from double to float at the function call site
*/
double Sin64(double x);
/* like Sin64, but has a period of 0..1 instead of 0..2pi */
double Sint64(double t);

u32 Rand(u32 *SeedAndState);

#endif /* UTILS_H */

