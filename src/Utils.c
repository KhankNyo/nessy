
#include "Common.h"
#include "Utils.h"


isize Strlen(const char *s)
{
    isize Length = 0;
    while (*s++)
        Length++;
    return Length;
}

void Memcpy(void *Dst, const void *Src, isize ByteCount)
{
    u8 *DstPtr = Dst;
    const u8 *SrcPtr = Src;
    while (ByteCount --> 0)
        *DstPtr++ = *SrcPtr++;
}

Bool8 Memcmp(const void *A, const void *B, isize ByteCount)
{
    const u8 *PtrA = A;
    const u8 *PtrB = B;
    while (ByteCount --> 0 && *PtrA == *PtrB)
    {
        PtrA++;
        PtrB++;
    }
    return ByteCount == 0 || *PtrA == *PtrB;
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
    return Len;
#undef WRITE_BUF
}

