
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

    Bool8 DMA, DMAOutOfSync;
    u16 DMAAddr;
    u8 DMAData;

    u16 ControllerStatusBuffer;
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

    .DMA = false,
    .DMAOutOfSync = false,
    .DMAAddr = 0,
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
        NESPPU_ExternalWrite(&Nes->PPU, Address & 0x07, Byte);
    }
    /* controller capture */
    else if (Address == 0x4016)
    {
        Nes->ControllerStatusBuffer = Platform_GetControllerState();
        Nes->ControllerStatusBuffer |= 0xFF00;
    }
    /* Object Attribute Memory direct access (OAM DMA) */
    else if (Address == 0x4014)
    {
        Nes->DMAAddr = (u16)Byte << 8;
        Nes->DMA = true;
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
        return NESPPU_ExternalRead(&Nes->PPU, Address & 0x07);
    }
    /* get controller status */
    else if (Address == 0x4017 || Address == 0x4016)
    {
        u8 ButtonStatus = Nes->ControllerStatusBuffer & 0x1;
        Nes->ControllerStatusBuffer >>= 1;
        return ButtonStatus;
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
        if (Cartridge.MapperInterface == NULL)
        {
            return "Unsupported mapper";
        }
        else
        {
            Nes_ConnectCartridge(Cartridge);
        }
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
        if (Nes->DMA)
        {
            /* start syncing on even clk */
            if (Nes->DMAOutOfSync)
            {
                if (Nes->Clk % 2 == 1)
                {
                    Nes->DMAOutOfSync = false;
                }
            }
            else
            {
                /* read cpu memory on even clk */
                if (Nes->Clk % 2 == 0)
                {
                    Nes->DMAData = NesInternal_ReadByte(NULL, Nes->DMAAddr);
                }
                /* write to ppu memory on odd clk */
                else
                {
                    u8 OAMAddr = Nes->DMAAddr++ & 0x00FF;
                    Nes->PPU.ObjectAttributeMemory[OAMAddr] = Nes->DMAData;
                }

                /* transfer is complete (1 page) */
                if ((Nes->DMAAddr & 0x00FF) == 0)
                {
                    Nes->DMA = false;
                    Nes->DMAOutOfSync = true;
                }
            }
        }
        else 
        {
            MC6502_StepClock(&Nes->CPU);
        }
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



