
#include "Common.h"
#include "Utils.h"


isize Strlen(const char *s)
{
    isize Length = 0;
    while (*s++)
        Length++;
    return Length;
}

void Memset(void *Dst, u8 Byte, isize ByteCount)
{
    u8 *DstPtr = Dst;
    while (ByteCount-- > 0)
        *DstPtr++ = Byte;
}

void Memcpy(void *Dst, const void *Src, isize ByteCount)
{
    u8 *DstPtr = Dst;
    const u8 *SrcPtr = Src;
    while (ByteCount --> 0)
        *DstPtr++ = *SrcPtr++;
}

u8 FlipByte(u8 Byte)
{
    /* 0b abcd efgh -> 0b hgfe dcba */
    u8 Flipped = (Byte << 4) | (Byte >> 4);                         /* 0b efgh abcd */
    Flipped = ((Flipped & 0xCC) >> 2) | ((Flipped & 0x33) << 2);    /* 0b ghef cdab */
    Flipped = ((Flipped & 0xAA) >> 1) | ((Flipped & 0x55) << 1);    /* 0b hgfe dcba */
    return Flipped;
}

Bool8 Memcmp(const void *A, const void *B, isize ByteCount)
{
    const u8 *PtrA = A;
    const u8 *PtrB = B;
    while (ByteCount > 0 && *PtrA == *PtrB)
    {
        PtrA++;
        PtrB++;
        ByteCount--;
    }
    return ByteCount == 0;
}

isize AppendString(char *Buffer, isize BufferSize, isize At, const char *String)
{
    while (At < BufferSize && *String)
    {
        Buffer[At++] = *String++;
    }
    if (BufferSize > 0 && At < BufferSize)
        Buffer[At] = '\0';
    return At;
}

isize AppendHex(char *Buffer, isize BufferSize, isize At, int MinDigitCount, u32 Hex)
{
    char Stack[sizeof(Hex)*2];
    char *StackPtr = Stack;
    const char *LookupHexDigit = "0123456789ABCDEF";
    char PaddingChar = '0';

    /* generate the reversed version */
    while (Hex)
    {
        *StackPtr++ = LookupHexDigit[Hex & 0x0F];
        Hex >>= 4;
        MinDigitCount--;
    }

    /* pad zeros */
    while (MinDigitCount > 0 && At < BufferSize)
    {
        Buffer[At++] = PaddingChar;
        MinDigitCount--;
    }

    /* spool the number into the buffer */
    while (At < BufferSize && StackPtr > &Stack[0])
    {
        Buffer[At++] = *(--StackPtr);
    }

    /* null terminate */
    if (BufferSize > 0 && At < BufferSize)
        Buffer[At] = '\0';
    return At;
}

isize FormatString(char *Buffer, isize BufferSize, ...)
{
    va_list Args;
    va_start(Args, BufferSize);
    isize Length = FormatStringArgs(Buffer, BufferSize, Args);
    va_end(Args);
    return Length;
}

isize FormatStringArgs(char *Buffer, isize BufferSize, va_list Args)
{
#define WRITE_BUF(Ch) ((LenLeft--), (Buffer[Len++] = Ch))
    DEBUG_ASSERT(NULL != Buffer);
    DEBUG_ASSERT(BufferSize > 0 && "Invalid buffer size");
    isize Len = 0;
    isize LenLeft = BufferSize;

    const char *String = va_arg(Args, char *);
    while (String)
    {
        while (*String && LenLeft && '{' != *String)
            WRITE_BUF(*String++);

        if (!LenLeft) 
            break;
        if ('\0' == *String) 
            goto Continue;

        /* skip '{' */
        String++;
        char FmtSpec = *String++;
        switch (FmtSpec) 
        {
        case 'x':
        {
            u8 DigitCount = *String++ - '0';
            u32 Hex = va_arg(Args, u32);
            Len = AppendHex(Buffer, BufferSize, Len, DigitCount, Hex);
        } break;
        case 's':
        {
            const char *Str = va_arg(Args, char *);
            Len = AppendString(Buffer, BufferSize, Len, Str);
        } break;
        default:
        {
            DEBUG_ASSERT(false && "Unexpected format");
        } break;
        }
        DEBUG_ASSERT('}' == *String && "Expected '}' after format");
        String++;

Continue:
        if ('\0' == *String)
        {
            String = va_arg(Args, char *);
        }
    }
    if (LenLeft > 0)
        Buffer[Len] = '\0';
    else /* BufferSize is assumed to be non-zero */
        Buffer[Len - 1] = '\0';
    return Len;
#undef WRITE_BUF
}



#define INV_2_FACTORIAL (.5f)
#define MAGIC 0.943221151718100

double Sin64(double x) 
{
    /* 
     * using Taylor series: 
     * sin(x) ~ 1 - t^2/2! + a*t^4/4! 
     * where t is x - pi/2
     * where a is .943221151718100
     * (gives the lowest delta compared to cos(x))
     * (and probably has more digits but desmos ran out of precision, 
     * but that's fine because under the hood, 
     * desmos ran on js, and js uses double to calculate it)
     */                                                                             
                                                                                    
    /* force x to be in range of 0..2pi */                                          
    double t = x * (double)(1.0f / TAU); /* this better get eval at comptime */
    t -= (i64)t;
    Bool8 ShouldBeNegated = t >= .5;
    if (ShouldBeNegated)
        t -= .5;
    t = t*TAU - PI/2;
                                                                                    
    /* calculate result  */
    double t2 = t*t;
    double y = 1
        - t2 * (double)INV_2_FACTORIAL
        + t2*t2 * (double)(MAGIC / 24.0f); 
    if (ShouldBeNegated)
        return -y;
    return y;
}

double Sint64(double t) 
{                                                                             
    t -= (i64)t;
    Bool8 ShouldBeNegated = t >= .5;
    if (ShouldBeNegated)
        t -= .5;
    t = t*TAU - PI/2;
                                                                                    
    /* calculate result  */
    double t2 = t*t;
    double y = 1
        - t2 * 0.5
        + t2*t2 * (double)(MAGIC / 24.0f); 
    if (ShouldBeNegated)
        return -y;
    return y;
}


#undef MAGIC
#undef INV_2_FACTORIAL


u32 Rand(u32 *State)
{
    *State = *State * 747796405u + 2891336453u;
    u32 state = *State;
    u32 word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}



