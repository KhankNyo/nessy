
#ifdef STANDALONE
#  undef STANDALONE
#endif /* STANDALONE */

#include "Common.h"
#include "Utils.h"

#include "Nes.h"
#define MC6502_IMPLEMENTATION
#include "6502.h"
#include "Cartridge.c"


#define NES_CPU_RAM_SIZE 0x0800


typedef struct NES 
{
    MC6502 *CPU;
    u8 Ram[NES_CPU_RAM_SIZE];
    NESCartridge *Cartridge;
} NES;

static MC6502 sCpu;
static NESCartridge sCartridge;
static NES sNes = {
    .CPU = &sCpu,
    .Ram = {
                    /* top: */
        0xA9, 0x00, /*   lda #00 */
                    /* cont: */
        0x18,       /*   clc */
        0x69, 0x01, /*   adc #1 */
        0xD0, -5,   /*   bnz cont */
        0x4C, 0, 0  /*   jmp top */
    },
};


void Nes_WriteByte(void *UserData, u16 Address, u8 Byte)
{
    NES *Nes = UserData;

    /* ram range */
    if (Address < 0x2000)
    {
        Address %= NES_CPU_RAM_SIZE;
        Nes->Ram[Address] = Byte;
    }
    /* IO registers */
    else if (IN_RANGE(0x2000, Address, 0x3FFF))
    {
        Address &= 0x07;
    }
    /* IO registers */
    else if (IN_RANGE(0x4000, Address, 0x401F))
    {
    }
    /* Expansion Rom */
    else if (IN_RANGE(0x4020, Address, 0x5FFF))
    {
    }
    /* Save Ram */
    else if (IN_RANGE(0x6000, Address, 0x7FFF))
    {
    }
    /* Cartridge ROM */
    else
    {
    }
}

u8 Nes_ReadByte(void *UserData, u16 Address)
{
    NES *Nes = UserData;

    u8 DataByte = 0;
    /* ram range */
    if (Address < 0x2000)
    {
        Address %= NES_CPU_RAM_SIZE;
        DataByte = Nes->Ram[Address];
    }
    /* IO registers */
    else if (IN_RANGE(0x2000, Address, 0x3FFF))
    {
        Address &= 0x07;
    }
    /* IO registers */
    else if (IN_RANGE(0x4000, Address, 0x401F))
    {
    }
    /* Expansion Rom */
    else if (IN_RANGE(0x4020, Address, 0x5FFF))
    {
    }
    /* Save Ram */
    else if (IN_RANGE(0x6000, Address, 0x7FFF))
    {
    }
    /* Cartridge ROM */
    else
    {
    }

    return DataByte;
}


const char *Nes_ParseINESFile(const void *INESFile, isize FileSize)
{
    const u8 *BytePtr = INESFile;
    if (!Memcmp(
        BytePtr, 
        BYTE_ARRAY(0x4E, 0x45, 0x53, 0x1A), /* NES\x1A */
        4
    ))
    {
        return "Unrecognized file format.";
    }

    const u8 *FileDataStart = BytePtr + 16;
    BytePtr += 4;

    isize PrgRomSize = (isize)(*BytePtr++) * 16 * 1024;
    isize ChrRomSize = (isize)(*BytePtr++) * 8  * 1024;
    u8 Flag6 = *BytePtr++;
    u8 Flag7 = *BytePtr++;

    u8 MapperID = (Flag6 >> 4) | (Flag7 & 0xF0);
    if (Flag6 & (1 << 0)) /* name table vertical */
    {
    }
    else /* name table horizontal */
    {
    }
    if (Flag6 & (1 << 1)) /* battery packed ram available (aka SRAM, or WRAM) */
    {
    }
    if (Flag6 & (1 << 2)) /* 512 byte trainer (don't care) */
    {
        FileDataStart += 512;
    }
    if (Flag6 & (1 << 3)) /* alternate nametable layout */
    {
    }

    const u8 *FilePrgRom = FileDataStart;
    const u8 *FileChrRom = FileDataStart + PrgRomSize;
    uint INESFileVersion = (Flag7 >> 2) & 0x3;
    switch (INESFileVersion)
    {
    case 0: /* */
    {
    } break;
    case 1: /* iNes */
    {
        sCartridge = NESCartridge_Init(
            FilePrgRom, PrgRomSize, 
            FileChrRom, ChrRomSize, 
            MapperID
        );
    } break;
    case 2: /* iNes 2.0 */
    {
    } break;
    }
    return NULL;
}


Nes_DisplayableStatus Nes_QueryStatus(void)
{
    Nes_DisplayableStatus Status = {
        .A = sCpu.A,
        .X = sCpu.X,
        .Y = sCpu.Y,
        .PC = sCpu.PC,
        .SP = sCpu.SP + 0x100,

        .N = MC6502_FlagGet(sCpu.Flags, FLAG_N),
        .Z = MC6502_FlagGet(sCpu.Flags, FLAG_Z),
        .V = MC6502_FlagGet(sCpu.Flags, FLAG_V),
        .C = MC6502_FlagGet(sCpu.Flags, FLAG_C),
        .I = MC6502_FlagGet(sCpu.Flags, FLAG_I),
        .U = MC6502_FlagGet(sCpu.Flags, FLAG_UNUSED),
        .B = MC6502_FlagGet(sCpu.Flags, FLAG_B),
        .D = MC6502_FlagGet(sCpu.Flags, FLAG_D),

        .Opcode = sCpu.Opcode,
    };
    return Status;
}

void Nes_OnEntry(void)
{
    sCpu = MC6502_Init(0, &sNes, Nes_ReadByte, Nes_WriteByte);
}

void Nes_OnLoop(void)
{
    MC6502_StepClock(&sCpu);
}

void Nes_AtExit(void)
{
}




