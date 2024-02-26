
#ifdef STANDALONE
#  undef STANDALONE
#endif /* STANDALONE */

#include "Common.h"
#include "Utils.h"
#include "Nes.h"

#include "Disassembler.c"
#include "6502.c"
#include "PPU.c"
#include "Cartridge.c"



typedef struct InstructionInfo 
{
    u16 Address;
    u16 ByteCount;
    SmallString String;
} InstructionInfo;

typedef struct NES 
{
    u64 Clk;
    MC6502 *CPU;
    NESPPU *PPU;
    NESCartridge *Cartridge;

    u8 Ram[NES_CPU_RAM_SIZE];
} NES;

static MC6502 sCpu;
static NESPPU sPpu;
static NESCartridge sConnectedCartridge;
static u32 sScreenBuffers[NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT * 2];
static u32 *sBackBuffer = &sScreenBuffers[0];
static u32 *sReadyBuffer = &sScreenBuffers[1];


static Bool8 sNesSystemSingleStepCPU = false;
static NES sNes = {
    .CPU = &sCpu,
    .PPU = &sPpu,
    .Cartridge = NULL,
    .Ram = { 0 },
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
    /* IO registers: PPU */
    else if (IN_RANGE(0x2000, Address, 0x3FFF))
    {
        Address &= 0x07;
        NESPPU *PPU = Nes->PPU;
        switch ((NESPPU_CtrlReg)Address)
        {
        case PPU_CTRL:
        {
            PPU->Ctrl = Byte;
            PPU->Loopy.t = 
                (PPU->Loopy.t & ~0x0C00) 
                | (((u16)Byte << 10) & 0x0C00);
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
                PPU->Loopy.t = 
                    (PPU->Loopy.t & ~0x1F)
                    | (Byte >> 3);
            }
            else /* second write */
            {
                PPU->Loopy.t = 
                    (PPU->Loopy.t & ~0x0CE0)
                    | ((u16)Byte << 12)
                    | (((u16)Byte & 0xF8) << 5);
            }
        } break;
        case PPU_ADDR:
        {
            /* latching because each cpu cycle is 3 ppu cycles */
            if (PPU->Loopy.w++ == 0) /* first write */
            {
                Byte &= 0x3F; /* clear 2 topmost bits */
                PPU->Loopy.t = 
                    (PPU->Loopy.t & 0x00FF) 
                    | ((u16)Byte << 8);
            }
            else /* second write */
            {
                PPU->Loopy.t = 
                    (PPU->Loopy.t & 0x7F00)
                    | Byte;
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

u8 Nes_ReadByte(void *UserData, u16 Address)
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
        NESPPU *PPU = Nes->PPU;
        switch ((NESPPU_CtrlReg)Address)
        {
        case PPU_CTRL: /* read not allowed */ break;
        case PPU_MASK: /* read not allowed */ break;
        case PPU_STATUS: 
        {
            u8 Status = PPU->Status; 
            PPU->Status &= ~PPUSTATUS_VBLANK;
            PPU->Loopy.w = 0;
            return Status | PPUSTATUS_VBLANK;
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
    const u8 *BytePtr = INESFile;
    if (!Memcmp(
        BytePtr, 
        BYTE_ARRAY(0x4E, 0x45, 0x53, 0x1A), /* NES\x1A */
        4
    ))
    {
        return "Unrecognized file format";
    }

    const u8 *FileDataStart = BytePtr + 16;
    BytePtr += 4;

    isize PrgRomSize = (isize)(*BytePtr++) * 16 * 1024;
    isize ChrRomSize = (isize)(*BytePtr++) * 8  * 1024;
    u8 Flag6 = *BytePtr++;
    u8 Flag7 = *BytePtr++;

    u8 MapperID = (Flag6 >> 4) | (Flag7 & 0xF0);
    if (Flag6 & (1 << 3)) /* alternate nametable layout */
    {
    }
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

    const u8 *FilePrgRom = FileDataStart;
    const u8 *FileChrRom = FileDataStart + PrgRomSize;
    uint INESFileVersion = (Flag7 >> 2) & 0x3;
    switch (INESFileVersion)
    {
    case 0: /* iNes 0.7 */
    case 1: /* iNes */
    {
        NESCartridge Cartridge = NESCartridge_Init(
            FilePrgRom, PrgRomSize, 
            FileChrRom, ChrRomSize, 
            MapperID
        );
        Nes_ConnectCartridge(Cartridge);
    } break;
    case 2: /* iNes 2.0 */
    {
        return "Unsupported iNes file version (2.0)";
    } break;
    }
    return NULL;
}

static void Nes_OnPPUFrameCompletion(NESPPU *PPU)
{
    /* swap the front and back buffers */
    u32 *Tmp = sReadyBuffer;
    sReadyBuffer = sBackBuffer;
    sBackBuffer = Tmp;

    PPU->ScreenOutput = sBackBuffer;
}


static int Nes_DisassembleAt(
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
            (InstructionBuffer->Address + ByteIndex) % MemorySize
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

static void Nes_Disassemble(
    char *BeforePC, isize BeforePCSize,
    char *AtPC, isize AtPCSize,
    char *AfterPC, isize AfterPCSize,
    u16 PC, const u8 *Memory, i32 MemorySize
)
{
#define INS_COUNT 11
    static InstructionInfo InstructionBuffer[INS_COUNT] = { 0 };
    static Bool8 Initialized = false;

    if (!Initialized || 
        !IN_RANGE(
            InstructionBuffer[0].Address, 
            PC,
            InstructionBuffer[INS_COUNT - 1].Address
        ))
    {
        Initialized = true;
        int CurrentPC = PC;
        for (int i = 0; i < INS_COUNT; i++)
        {
            int InstructionSize = DisassembleSingleOpcode(
                &InstructionBuffer[i].String, 
                CurrentPC,
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
            InstructionBuffer[i].Address = CurrentPC;
            CurrentPC += InstructionSize;
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


Nes_DisplayableStatus Nes_PlatformQueryDisplayableStatus(void)
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



void Nes_OnEntry(void)
{
    sCpu = MC6502_Init(0, &sNes, Nes_ReadByte, Nes_WriteByte);
    sPpu = NESPPU_Init(
        Nes_OnPPUFrameCompletion, 
        sBackBuffer, 
        &sNes.Cartridge
    );
}



static Bool8 Nes_StepClock(NES *Nes)
{
    Nes->Clk++;
    Bool8 FrameCompleted = NESPPU_StepClock(Nes->PPU);
    if (Nes->Clk % 3 == 0)
    {
        MC6502_StepClock(Nes->CPU);
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
            } while (sNes.CPU->CyclesLeft > 0);
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
}

void Nes_OnEmulatorSingleStep(void)
{
    sNesSystemSingleStepCPU = true;
    sNes.Clk = 2;
}

void Nes_OnEmulatorReset(void)
{
    sNes.Clk = 0;
    MC6502_Reset(&sCpu);
    NESPPU_Reset(&sPpu);
}



