
#include "Nes.h"
//#warning "TODO: not do 2 layers of .c include in Debugger.c"
#include "Disassembler.c"


typedef struct InstructionInfo 
{
    u16 Address;
    u16 ByteCount;
    SmallString String;
} InstructionInfo;

int Nes_DisassembleAt(
    char *Ptr, isize SizeLeft, 
    const InstructionInfo *InstructionBuffer, 
    const u8 *Memory, isize MemorySize)
{
    isize Length = FormatString(Ptr, SizeLeft, 
        "{x4}: ", InstructionBuffer->Address, 
        NULL
    );

    int ExpectedByteCount = 3;
    for (int ByteIndex = 0; ByteIndex < InstructionBuffer->ByteCount; ByteIndex++)
    {
        u8 Byte = Memory[
            (InstructionBuffer->Address - 0x8000 + ByteIndex) % MemorySize
        ];
        Length += FormatString(
            Ptr + Length, 
            SizeLeft - Length, 
            "{x2} ", Byte, 
            NULL
        );
        ExpectedByteCount--;
    }
    while (ExpectedByteCount --> 0)
    {
        Length = AppendString(
            Ptr,
            SizeLeft, 
            Length, 
            "   "
        );
    }
    Length = AppendString(
        Ptr, 
        SizeLeft, 
        Length, 
        InstructionBuffer->String.Data
    );
    while (Length < 28 && Length < SizeLeft)
    {
        Ptr[Length++] = ' ';
    }
    Ptr[Length] = '\0';
    return Length;
}


static Bool8 Nes_DisasmCheckPC(u16 PC, InstructionInfo *Buffer, isize Count)
{
    u16 Addr = Buffer[0].Address;
    Bool8 PCInBuffer = false;
    for (isize i = 0; i < Count; i++)
    {
        if (Addr != Buffer[i].Address) /* misaligned */
            return false;
        if (PC == Addr) /* PC is in the buffer */
            PCInBuffer = true;
        Addr += Buffer[i].ByteCount;
    }
    return PCInBuffer;
}

void Nes_Disassemble(
    char *BeforePC, isize BeforePCSize,
    char *AtPC, isize AtPCSize,
    char *AfterPC, isize AfterPCSize,
    u16 PC, const u8 *Memory, i32 MemorySize
)
{
#define INS_COUNT 11
    static InstructionInfo InstructionBuffer[INS_COUNT] = { 0 };
    static Bool8 Initialized = false;

    if (!Initialized || !Nes_DisasmCheckPC(PC, InstructionBuffer, INS_COUNT))
    {
        Initialized = true;
        int VirtualPC = PC;
        int CurrentPC = PC - 0x8000;
        for (int i = 0; i < INS_COUNT; i++)
        {
            int InstructionSize = DisassembleSingleOpcode(
                &InstructionBuffer[i].String, 
                VirtualPC,
                &Memory[CurrentPC % MemorySize],
                NES_CPU_RAM_SIZE - (CurrentPC % NES_CPU_RAM_SIZE)
            );
            if (-1 == InstructionSize)
            {
                /* retry at addr 0 */
                CurrentPC = 0;
                i--;
                continue;
            }

            InstructionBuffer[i].ByteCount = InstructionSize;
            InstructionBuffer[i].Address = VirtualPC;
            CurrentPC += InstructionSize;
            VirtualPC += InstructionSize;
        }
    }


    int i = 0;
    while (i < INS_COUNT)
    {
        if (InstructionBuffer[i].Address == PC)
            break;

        if (i)
            AppendString(BeforePC++, BeforePCSize--, 0, "\n");
        int Len = Nes_DisassembleAt(BeforePC, BeforePCSize, &InstructionBuffer[i], Memory, MemorySize);
        BeforePC += Len;
        BeforePCSize -= Len;
        i++;
    }

    if (i < INS_COUNT)
    {
        Nes_DisassembleAt(AtPC, AtPCSize, &InstructionBuffer[i], Memory, MemorySize);
        i++;
    }

    while (i < INS_COUNT)
    {
        int Len = Nes_DisassembleAt(AfterPC, AfterPCSize, &InstructionBuffer[i], Memory, MemorySize);
        AfterPC += Len;
        AfterPCSize -= Len;
        AppendString(AfterPC++, AfterPCSize--, 0, "\n");
        i++;
    }
#undef INS_COUNT
}



