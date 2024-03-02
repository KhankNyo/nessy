#ifndef NES_DEBUGGER_C
#define NES_DEBUGGER_C

#include "Nes.h"
#include "Cartridge.h"
#include "Disassembler.c"

typedef struct DisassembledInstruction
{
    u16 Address;
    u8 ByteCount;
    u8 LineLength;
    char Line[128 - 4];
} DisassembledInstruction;


typedef struct NESDisassemblerState 
{
    DisassembledInstruction DisasmBuffer[256];
    u8 Count;
    u8 BytesPerLine;
} NESDisassemblerState;


static Bool8 Nes_DisasmCheckPC(u16 PC, DisassembledInstruction *Buffer, isize Count)
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

static u8 Nes_DisRead(void *UserData, u16 VirtualPC)
{
    DEBUG_ASSERT(NULL != UserData && "Unreachable");
    NESCartridge *Cartridge = UserData;
    return NESCartridge_DebugCPURead(Cartridge, VirtualPC);
}

void Nes_Disassemble(
    NESDisassemblerState *This, NESCartridge *Cartridge, u16 VirtualPC, 
    char *BeforePCBuffer, isize BeforePCBufferSizeBytes, 
    char *AtPCBuffer, isize AtPCBufferSizeBytes, 
    char *AfterPCBuffer, isize AfterPCBufferSizeBytes)
{
    DEBUG_ASSERT(This && "nullptr");
    DEBUG_ASSERT(Cartridge && "nullptr");
    DEBUG_ASSERT(BeforePCBuffer && "nullptr");
    DEBUG_ASSERT(AtPCBuffer && "nullptr");
    DEBUG_ASSERT(AfterPCBuffer && "nullptr");

    /* PC is not valid in the buffer, redo disassembly */
    if (!Nes_DisasmCheckPC(VirtualPC, This->DisasmBuffer, This->Count))
    {
        u16 PC = VirtualPC;
        for (uint i = 0; i < This->Count; i++)
        {
            char *Line = This->DisasmBuffer[i].Line;
            int LengthLeft = sizeof This->DisasmBuffer[i].Line;
            SmallString Instruction;

            /* disassemble the instruction */
            int InstructionSize = DisassembleSingleOpcode(
                &Instruction, 
                PC,
                Cartridge, 
                Nes_DisRead
            );
            This->DisasmBuffer[i].ByteCount = InstructionSize;
            This->DisasmBuffer[i].Address = PC;

            /* address */
            int TmpLen = FormatString(Line, LengthLeft, 
                "{x4}: ", PC,
                NULL
            );
            Line += TmpLen;
            LengthLeft -= TmpLen;

            /* bytes */
            for (int k = 0; k < InstructionSize; k++)
            {
                u8 Byte = NESCartridge_DebugCPURead(Cartridge, PC++);
                TmpLen = FormatString(Line, LengthLeft,
                    "{x2} ", Byte,
                    NULL
                );
                Line += TmpLen;
                LengthLeft -= TmpLen;
            }

            /* space */
            for (int k = InstructionSize; k < This->BytesPerLine; k++)
            {
                TmpLen = AppendString(Line, LengthLeft, 0, "   ");
                Line += TmpLen;
                LengthLeft -= TmpLen;
            }
            TmpLen = FormatString(Line, LengthLeft, "{s}\n", Instruction.Data, NULL);
            This->DisasmBuffer[i].LineLength = Line + TmpLen - This->DisasmBuffer[i].Line;
            DEBUG_ASSERT(This->DisasmBuffer[i].LineLength < sizeof This->DisasmBuffer[i].Line);
        }
    }


    char *WritePtr = BeforePCBuffer;
    int LengthLeft = BeforePCBufferSizeBytes;

    /* before PC */
    int i = 0;
    for (; 
        i < This->Count 
        && This->DisasmBuffer[i].Address != VirtualPC
        && LengthLeft >= (int)This->DisasmBuffer[i].LineLength;
        i++)
    {
        Memcpy(WritePtr, This->DisasmBuffer[i].Line, This->DisasmBuffer[i].LineLength);
        WritePtr += This->DisasmBuffer[i].LineLength;
        LengthLeft -= This->DisasmBuffer[i].LineLength;
        if (LengthLeft > 0)
            *WritePtr = '\0';
    }
    BeforePCBuffer[BeforePCBufferSizeBytes - 1] = '\0';

    /* at PC */
    if (i < This->Count 
    && AtPCBufferSizeBytes >= (int)This->DisasmBuffer[i].LineLength
    && This->DisasmBuffer[i].Address == VirtualPC)
    {
        /* remove newline */
        if (WritePtr > BeforePCBuffer && LengthLeft > 0)
            WritePtr[-1] = '\0';
        Memcpy(AtPCBuffer, This->DisasmBuffer[i].Line, This->DisasmBuffer[i].LineLength);
        AtPCBuffer[This->DisasmBuffer[i].LineLength - 1] = '\0';
        i++;
    }

    /* after PC */
    WritePtr = AfterPCBuffer;
    LengthLeft = AfterPCBufferSizeBytes;
    for (; 
        i < This->Count
        && LengthLeft >= (int)This->DisasmBuffer[i].LineLength; 
        i++) 
    {
        Memcpy(WritePtr, This->DisasmBuffer[i].Line, This->DisasmBuffer[i].LineLength);
        WritePtr += This->DisasmBuffer[i].LineLength;
        LengthLeft -= This->DisasmBuffer[i].LineLength;
        if (LengthLeft > 0)
            *WritePtr = '\0';
    }
    AfterPCBuffer[AfterPCBufferSizeBytes - 1] = '\0';
}

#endif /* NES_DEBUGGER_C */
