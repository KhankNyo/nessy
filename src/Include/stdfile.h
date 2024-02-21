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

StdFileBuffer StdFileRead(const char *FileName, Bool8 AddNullTerminator);
void StdFileCleanup(StdFileBuffer *Buffer);

#endif /* STDFILE_H */

#define STDFILE_IMPLEMENTATION
#ifdef STDFILE_IMPLEMENTATION
#include <stdio.h>
#include <stdlib.h>

StdFileBuffer StdFileRead(const char *FileName, Bool8 AddNullTerminator)
{
    StdFileBuffer FileBuffer = { 0 };
    FILE *File = fopen(FileName, "rb");
    if (NULL == File)
        goto FOpenFailed;

    /* find file size */
    fseek(File, 0, SEEK_END);
    size_t FileSize = ftell(File);
    fseek(File, 0, SEEK_SET);

    void *Buffer = malloc(FileSize + AddNullTerminator);
    if (NULL == Buffer)
        goto MallocFailed;

    if (FileSize != fread(Buffer, 1, FileSize, File))
        goto ReadFailed;

    FileBuffer.Data.Ptr = Buffer;
    FileBuffer.Size = FileSize;
    if (AddNullTerminator)
    {
        FileBuffer.Data.String[FileSize] = '\0';
        FileBuffer.Size += 1;
    }
    fclose(File);
    return FileBuffer;

ReadFailed:
    free(Buffer);
MallocFailed:
    fclose(File);
FOpenFailed:
    return (StdFileBuffer) { 0 };
}

void StdFileCleanup(StdFileBuffer *Buffer)
{
    free(Buffer->Data.Ptr);
    *Buffer = (StdFileBuffer) { 0 };
}

#endif /* STDFILE_IMPLEMENTATION */

