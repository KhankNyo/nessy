#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "Common.h"

/* -1 if Buffer was too small, else index of the next instruction */
i32 DisassembleSingleOpcode(SmallString *Opcode, const void *Buffer, i32 BufferSizeBytes);

#endif /* DISASSEMBLER_H */


#ifdef DISASSEMBLER_IMPLEMENTATION

#ifdef STANDALONE
#undef STANDALONE
#include <stdio.h>
#define STDFILE_IMPLEMENTATION
#include "stdfile.h"

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <binary file>\n", argv[0]);
        return 1;
    }

    const char *BinaryFileName = argv[1];
    StdFileBuffer FileBuffer = StdFileRead(BinaryFileName, true);
    if (NULL == FileBuffer.Data.Ptr)
    {
        perror(BinaryFileName);
        return 1;
    }

    SmallString Line;
    i32 Size = 0;
    i32 = DisassembleSingleOpcode(Line.Data, FileBuffer.Data.Ptr);
    return 0;
}
#endif /* STANDALONE */
#endif /* DISASSEMBLER_IMPLEMENTATION */

