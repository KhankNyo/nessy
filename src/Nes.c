
#ifdef STANDALONE
#  undef STANDALONE
#endif /* STANDALONE */

#include "Common.h"
#include "Utils.h"
#include "Nes.h"

#include "Debugger.c"
#include "6502.c"
#include "PPU.c"
#include "Cartridge.c"


typedef struct NES 
{
    NESCartridge *Cartridge;
    u64 Clk;
    MC6502 CPU;
    NESPPU PPU;

    u8 Ram[NES_CPU_RAM_SIZE];
} NES;


static NESCartridge sConnectedCartridge;
static u32 sScreenBuffers[2][NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT];
static u32 *sBackBuffer = sScreenBuffers[0];
static u32 *sReadyBuffer = sScreenBuffers[1];


static Bool8 sNesSystemSingleStepCPU = false;
static Bool8 sNesSystemSingleStepFrame = false;
static NES sNes = {
    .Cartridge = NULL,
    .Ram = { 0 },
};


static void NesInternal_WriteByte(void *UserData, u16 Address, u8 Byte)
{
    NES *Nes = UserData;

    /* ram range */
    if (Address < 0x2000)
    {
        Address %= NES_CPU_RAM_SIZE;
        Nes->Ram[Address] = Byte;
    }
    /* IO registers: PPU */
    else if (IN_RANGE(0x2000, Address, 0x3FFF))
    {
        Address &= 0x07;
        NESPPU *PPU = &Nes->PPU;
        switch ((NESPPU_CtrlReg)Address)
        {
        case PPU_CTRL:
        {
            PPU->Ctrl = Byte;
            u16 NametableBits = (u16)Byte << 10;
            MASKED_LOAD(PPU->Loopy.t, NametableBits, 0x0C00);
        } break;
        case PPU_MASK: PPU->Mask = Byte; break;
        case PPU_STATUS: /* write not allowed */ break;
        case PPU_OAM_ADDR:
        case PPU_OAM_DATA:
        case PPU_SCROLL: 
        {
            if (PPU->Loopy.w++ == 0) /* first write */
            {
                PPU->Loopy.x = Byte;
                MASKED_LOAD(PPU->Loopy.t, Byte >> 3, 0x1F);
            }
            else /* second write */
            {
                u16 FineY = (u16)Byte << 12;
                u16 CoarseY = (Byte >> 3) << 5;
                MASKED_LOAD(PPU->Loopy.t, FineY, 0x7000);
                MASKED_LOAD(PPU->Loopy.t, CoarseY, 0x03E0);
            }
        } break;
        case PPU_ADDR:
        {
            /* latching because each cpu cycle is 3 ppu cycles */
            if (PPU->Loopy.w++ == 0) /* first write */
            {
                Byte &= 0x3F;
                u16 AddrHi = (u16)Byte << 8;
                MASKED_LOAD(PPU->Loopy.t, AddrHi, 0xFF00);
            }
            else /* second write */
            {
                MASKED_LOAD(PPU->Loopy.t, Byte, 0x00FF);
                PPU->Loopy.v = PPU->Loopy.t;
            }
        } break;
        case PPU_DATA:
        {
            NESPPU_WriteInternalMemory(PPU, PPU->Loopy.v, Byte);
            PPU->Loopy.v += (PPU->Ctrl & PPUCTRL_INC32)? 32 : 1;
        } break;
        }
    }
    /* IO registers: DMA */
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
    else if (sNes.Cartridge)
    {
        NESCartridge_Write(sNes.Cartridge, Address, Byte);
    }
}

static u8 NesInternal_ReadByte(void *UserData, u16 Address)
{
    NES *Nes = UserData;

    /* ram range */
    if (Address < 0x2000)
    {
        Address %= NES_CPU_RAM_SIZE;
        return Nes->Ram[Address];
    }
    /* IO registers: PPU */
    else if (IN_RANGE(0x2000, Address, 0x3FFF))
    {
        Address &= 0x07;
        NESPPU *PPU = &Nes->PPU;
        switch ((NESPPU_CtrlReg)Address)
        {
        case PPU_CTRL: /* read not allowed */ break;
        case PPU_MASK: /* read not allowed */ break;
        case PPU_STATUS: 
        {
            u8 Status = PPU->Status; 
            PPU->Status &= ~PPUSTATUS_VBLANK;
            PPU->Loopy.w = 0;
            return Status;
        } break;
        case PPU_OAM_ADDR: /* read not allowed */ break;
        case PPU_OAM_DATA:
        {
        } break;
        case PPU_SCROLL: /* read not allowed */ break;
        case PPU_ADDR: /* read not allowed */ break;
        case PPU_DATA:
        {
            u8 ReadValue;
            if (PPU->Loopy.v < 0x3F00)
            {
                static u8 ReadBuffer;
                ReadValue = ReadBuffer;
                ReadBuffer = NESPPU_ReadInternalMemory(PPU, Address);
            }
            else /* no buffered read for this addresses below 0x3F00 */
            {
                ReadValue = NESPPU_ReadInternalMemory(PPU, PPU->Loopy.v);
            }
            PPU->Loopy.v += (PPU->Ctrl & PPUCTRL_INC32)? 32 : 1;
            return ReadValue;
        } break;
        }

        return 0;
    }
    /* IO registers: DMA */
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
    else if (sNes.Cartridge)
    {
        return NESCartridge_Read(sNes.Cartridge, Address);
    }
    return 0xEA;
}

static void Nes_ConnectCartridge(NESCartridge NewCartridge)
{
    if (sNes.Cartridge)
    {
        /* TODO: save?? */
        NESCartridge_Destroy(sNes.Cartridge);
    }
    else
    {
        sNes.Cartridge = &sConnectedCartridge;
    }
    *sNes.Cartridge = NewCartridge;
}


const char *Nes_ParseINESFile(const void *INESFile, isize FileSize)
{
    isize DataSectionOffset = 16;
    if (FileSize < 16 * 1024 + DataSectionOffset)
        goto ErrorFileTooSmall;


    /* check signature */
    const u8 *BytePtr = INESFile;
    if (!Memcmp(
        BytePtr, 
        BYTE_ARRAY(0x4E, 0x45, 0x53, 0x1A), /* NES\x1A */
        4
    ))
    {
        return "Unrecognized file format";
    }


    /* get a pointer to the data section for later processing */
    const u8 *FileDataStart = BytePtr + DataSectionOffset;


    BytePtr += 4;
    isize PrgRomSize = (isize)(*BytePtr++) * 16 * 1024;
    isize ChrRomSize = (isize)(*BytePtr++) * 8  * 1024;
    /* verify file size */
    if (FileSize < ChrRomSize + PrgRomSize + DataSectionOffset)
        goto ErrorFileTooSmall;
    

    /* read flags */
    u8 Flag6 = *BytePtr++;
    u8 Flag7 = *BytePtr++;
    u8 MapperID = (Flag6 >> 4) | (Flag7 & 0xF0);
    if (Flag6 & (1 << 3)) /* alternate nametable layout */
    {
    }
    NESNameTableMirroring NameTableMirroring = NAMETABLE_HORIZONTAL;
    if (Flag6 & (1 << 0)) /* name table vertical */
    {
        NameTableMirroring = NAMETABLE_VERTICAL;
    }

    if (Flag6 & (1 << 1)) /* battery packed ram available (aka SRAM, or WRAM) */
    {
    }
    if (Flag6 & (1 << 2)) /* 512 byte trainer (don't care) */
    {
        FileDataStart += 512;
    }



    const u8 *FilePrgRom = FileDataStart;
    const u8 *FileChrRom = FileDataStart + PrgRomSize;
    uint INESFileVersion = (Flag7 >> 2) & 0x3;
    switch (INESFileVersion)
    {
    /* most of the info we need are the same accross different version anyway */
    case 0: /* iNes 0.7 */
    case 1: /* iNes */
    case 2: /* iNes 2.0 */
    {
        NESCartridge Cartridge = NESCartridge_Init(
            FilePrgRom, PrgRomSize, 
            FileChrRom, ChrRomSize, 
            MapperID, NameTableMirroring
        );
        Nes_ConnectCartridge(Cartridge);
    } break;
    default:
    {
        return "Unknown iNes file version";
    } break;
    }
    return NULL;

ErrorFileTooSmall:
    return "File too small (must be at least 16kb)";
}


Nes_DisplayableStatus Nes_PlatformQueryDisplayableStatus(void)
{
    Nes_DisplayableStatus Status = {
        .A = sNes.CPU.A,
        .X = sNes.CPU.X,
        .Y = sNes.CPU.Y,
        .PC = sNes.CPU.PC,
        .SP = sNes.CPU.SP + 0x100,

        .N = MC6502_FlagGet(sNes.CPU.Flags, FLAG_N),
        .Z = MC6502_FlagGet(sNes.CPU.Flags, FLAG_Z),
        .V = MC6502_FlagGet(sNes.CPU.Flags, FLAG_V),
        .C = MC6502_FlagGet(sNes.CPU.Flags, FLAG_C),
        .I = MC6502_FlagGet(sNes.CPU.Flags, FLAG_I),
        .U = MC6502_FlagGet(sNes.CPU.Flags, FLAG_UNUSED),
        .B = MC6502_FlagGet(sNes.CPU.Flags, FLAG_B),
        .D = MC6502_FlagGet(sNes.CPU.Flags, FLAG_D),
    };
    if (sNes.Cartridge)
    {
        Nes_Disassemble(
            Status.DisasmBeforePC, sizeof Status.DisasmBeforePC,
            Status.DisasmAtPC, sizeof Status.DisasmAtPC,
            Status.DisasmAfterPC, sizeof Status.DisasmAfterPC, 
            Status.PC, sNes.Cartridge->Rom, sNes.Cartridge->RomSizeBytes
        );
    }
    return Status;
}

Platform_FrameBuffer Nes_PlatformQueryFrameBuffer(void)
{
    Platform_FrameBuffer Frame = {
        .Data = sReadyBuffer,
        .Width = NES_SCREEN_WIDTH,
        .Height = NES_SCREEN_HEIGHT,
    };
    return Frame;
}


static void NesInternal_OnPPUFrameCompletion(NESPPU *PPU)
{
    /* swap the front and back buffers */
    u32 *Tmp = sReadyBuffer;
    sReadyBuffer = sBackBuffer;
    sBackBuffer = Tmp;

    PPU->ScreenOutput = sBackBuffer;
}

static void NesInternal_OnPPUNmi(NESPPU *PPU)
{
    (void)PPU;
    MC6502_Interrupt(&sNes.CPU, VEC_NMI);
}

void Nes_OnEntry(void)
{
    sNes.CPU = MC6502_Init(0, &sNes, NesInternal_ReadByte, NesInternal_WriteByte);
    sNes.PPU = NESPPU_Init(
        NesInternal_OnPPUFrameCompletion, 
        NesInternal_OnPPUNmi,
        sBackBuffer, 
        &sNes.Cartridge
    );
}



static Bool8 Nes_StepClock(NES *Nes)
{
    Nes->Clk++;
    Bool8 FrameCompleted = NESPPU_StepClock(&Nes->PPU);
    if (Nes->Clk % 3 == 0)
    {
        MC6502_StepClock(&Nes->CPU);
    }
    return FrameCompleted;
}


void Nes_OnLoop(double ElapsedTime)
{
    static double ResidueTime = 1000.0/60.0;

    if (sNesSystemSingleStepCPU)
    {
        if (sNes.Clk)
        {
            do {
                Nes_StepClock(&sNes);
            } while (sNes.CPU.CyclesLeft > 0);
            sNes.Clk = 0;
        }
    }
    else if (sNesSystemSingleStepFrame)
    {
        if (sNes.Clk)
        {
            do {
            } while (!Nes_StepClock(&sNes));
            sNes.Clk = 0;
        }
    }
    else
    {
        if (ResidueTime < ElapsedTime)
        {
            ResidueTime += 1000.0 / 60.0;
            Bool8 FrameCompleted = false;
            do {
                FrameCompleted = Nes_StepClock(&sNes);
            } while (!FrameCompleted);
        }
    }
}

void Nes_AtExit(void)
{
}




void Nes_OnEmulatorToggleHalt(void)
{
    sNesSystemSingleStepCPU = !sNesSystemSingleStepCPU;
    sNesSystemSingleStepFrame = false;
}

void Nes_OnEmulatorSingleStep(void)
{
    sNesSystemSingleStepCPU = true;
    sNesSystemSingleStepFrame = false;
    sNes.Clk = 2;
}

void Nes_OnEmulatorSingleFrame(void)
{
    sNesSystemSingleStepCPU = false;
    sNesSystemSingleStepFrame = true;
    sNes.Clk = 1;
}

void Nes_OnEmulatorReset(void)
{
    sNes.Clk = 0;
    MC6502_Reset(&sNes.CPU);
    NESPPU_Reset(&sNes.PPU);
}



