#ifndef STDFILE_H
#define STDFILE_H

#include "Common.h"

typedef struct StdFileBuffer 
{
    union {
        void *Ptr;
        u8 *BytePtr;
        char *String;
    } Data;
    size_t Size;
} StdFileBuffer;
StdFileBuffer StdFileRead(const char *FileName, Bool8 ReadRaw);
void StdFileCleanup(StdFileBuffer *Buffer);

#endif /* STDFILE_H */

