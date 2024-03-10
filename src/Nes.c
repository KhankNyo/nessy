
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
#include "APU.c"


typedef struct NES 
{
    NESCartridge *Cartridge;
    u64 Clk;
    MC6502 CPU;
    NESPPU PPU;
    NESAPU APU;

    Bool8 DMA; 
    Bool8 DMAOutOfSync;
    u16 DMAAddr;
    u16 DMASaveAddr;
    u8 DMAData;

    u8 ControllerStatusBuffer;
    u8 Ram[NES_CPU_RAM_SIZE];
} NES;

typedef enum NESEmulationMode 
{
    EMUMODE_SINGLE_STEP,
    EMUMODE_SINGLE_FRAME,
} NESEmulationMode;



typedef struct Emulator 
{
    /* the nes */
    NES Nes;
    NESDisassemblerState DisassemblerState;
    NESCartridge Cartridge;

    /* emulator */
    uint CurrentPalette;
    NESEmulationMode EmulationMode;
    Bool8 EmulationHalted;
    Bool8 EmulationDone;
    double ResidueTime;
    u32 MasterClkPerAudioSample;

    /* screen */
    u32 ScreenBuffer[2][NES_SCREEN_BUFFER_SIZE];
    u32 (*BackBuffer)[NES_SCREEN_BUFFER_SIZE];
    u32 (*FrontBuffer)[NES_SCREEN_BUFFER_SIZE];
} Emulator;



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
    }
    /* Object Attribute Memory direct access (OAM DMA) */
    else if (Address == 0x4014)
    {
        Nes->DMAAddr = (u16)Byte << 8;
        Nes->DMASaveAddr = Nes->DMAAddr;
        Nes->DMA = true;
        Nes->DMAOutOfSync = true;
    }
    /* APU */
    else if (IN_RANGE(0x4000, Address, 0x4013) 
    || Address == 0x4015 || Address == 0x4017)
    {
        NESAPU_ExternalWrite(&Nes->APU, Address, Byte);
    }
    /* Expansion Rom */
    else if (IN_RANGE(0x4020, Address, 0x5FFF))
    {
    }
    /* 0x6000 - 0xFFFF */
    /* Save Ram */
    /* Cartridge ROM */
    else if (Nes->Cartridge)
    {
        NESCartridge_CPUWrite(Nes->Cartridge, Address, Byte);
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
        return NESAPU_ExternalRead(&Nes->APU, Address);
    }
    /* Expansion Rom */
    else if (IN_RANGE(0x4020, Address, 0x5FFF))
    {
    }
    /* 0x6000 - 0xFFFF */
    /* Save Ram */
    /* Cartridge ROM */
    else if (Nes->Cartridge)
    {
        return NESCartridge_CPURead(Nes->Cartridge, Address);
    }
    return 0xEA;
}

static void Nes_ConnectCartridge(Emulator *Emu, NESCartridge NewCartridge)
{
    NES *Nes = &Emu->Nes;
    if (Nes->Cartridge)
    {
        /* there was a physical cartridge inside the nes, 
         * destroy it to make room for a new one */
        /* TODO: save?? */
        NESCartridge_Destroy(Nes->Cartridge);
    }
    else
    {
        /* there wasn't any physical cartridge inside the Nes yet, put one in */
        Nes->Cartridge = &Emu->Cartridge;
    }

    /* update the physical contents of the current cartridge inside the nes */
    *Nes->Cartridge = NewCartridge;
}


const char *Nes_ParseINESFile(Platform_ThreadContext ThreadContext, const void *INESFile, isize FileSize)
{
    /* 
     * refer to this before reading 
     * https://www.nesdev.org/wiki/INES
     */

    isize DataSectionOffset = 16;
    {
        isize MinimalINesFileSize = 16 * KB + DataSectionOffset;
        if (FileSize < MinimalINesFileSize)
            goto ErrorFileTooSmall;
    }


    /* check signature */
    const u8 *BytePtr = INESFile;
    if (!Memcmp(
        BytePtr, 
        BYTE_ARRAY(0x4E, 0x45, 0x53, 0x1A), /* NES\x1A, iNes file signature */
        4
    ))
    {
        return "Unrecognized file format";
    }


    /* get a pointer to the data section for later processing */
    const u8 *FileDataStart = BytePtr + DataSectionOffset;


    /* read flags */
    u8 Flag6 = BytePtr[6];  /* no good name for this magic number */
    u8 Flag7 = BytePtr[7];  /* no good name for this magic number */
    if (Flag6 & (1 << 2)) /* 512 byte trainer (don't care) */
    {
        FileDataStart += 512;
        DataSectionOffset += 512;
    }


    /* verify file size (again) */
    isize PrgRomSize = ((isize)BytePtr[4]) * 16 * KB;   /* prg rom is in 16kb chunk */
    isize ChrRomSize = ((isize)BytePtr[5]) * 8  * KB;   /* chr rom is in 8 kb chunk */
    if (FileSize < ChrRomSize + PrgRomSize + DataSectionOffset)
        goto ErrorFileTooSmall;
    

    /* read flags */
    u8 MapperID = (Flag6 >> 4) | (Flag7 & 0xF0);        
    NESNametableMirroring NametableMirroring = NAMETABLE_HORIZONTAL;
    if (Flag6 & (1 << 0)) /* name table vertical */
    {
        NametableMirroring = NAMETABLE_VERTICAL;
    }
    Bool8 AlternativeNametableLayout = (Flag6 & (1 << 3)) != 0;
    const u8 *FilePrgRom = FileDataStart;
    const u8 *FileChrRom = FileDataStart + PrgRomSize;


    /* determine file version and more info about mappers, ram and rom */
    isize PrgRamSize = 0;
    isize ChrRamSize = 0;
    Bool8 HasBusConflict = false;
    uint INESFileVersion = (Flag7 >> 2) & 0x3;
    if (0 == INESFileVersion 
    || 1 == INESFileVersion)
    {
        /* no chr rom, implying chr ram */
        if (ChrRomSize == 0)
        {
            ChrRamSize = 8 * KB;
            FileChrRom = NULL;
        }
    }
    else if (2 == INESFileVersion)
    {
        if (ChrRomSize == 0)
            FileChrRom = NULL;

        /* https://www.nesdev.org/wiki/NES_2.0#Nametable_layout */
        u8 PrgRamFlag = BytePtr[10];
        u8 ChrRamFlag = BytePtr[11];
        if (PrgRamFlag & 0x0F) /* has PRG RAM? */
        {
            PrgRamSize = 64 << (PrgRamFlag & 0x0F);
        }
        if (PrgRamFlag & 0xF0) /* has non-volatile PRG RAM? */
        {
            /* don't care cuz very few games and mappers have it */
        }
        if (ChrRamFlag & 0x0F) /* has CHR RAM? */
        {
            ChrRamSize = 64 << (ChrRamFlag & 0x0F);
        }
        if (ChrRamFlag & 0xF0) /* has non-volatile CHR RAM? */
        {
            /* don't care cuz very few games and mappers have it */
        }
    }
    else 
    {
        return "Unknown iNes file version (3)";
    }


    Emulator *Emu = ThreadContext.ViewPtr;
    NESCartridge Cartridge = NESCartridge_Init(
        FilePrgRom, PrgRomSize, 
        FileChrRom, ChrRomSize, 
        PrgRamSize, ChrRamSize, 
        MapperID, 
        NametableMirroring, 
        AlternativeNametableLayout,
        HasBusConflict
    );
    if (Cartridge.MapperInterface == NULL)
    {
        return "Unsupported mapper";
    }
    else
    {
        Nes_ConnectCartridge(Emu, Cartridge);
        return NULL; /* no error */
    }

ErrorFileTooSmall:
    return "File too small (must be at least 16kb + 16 bytes)";
}


Nes_DisplayableStatus Nes_PlatformQueryDisplayableStatus(Platform_ThreadContext ThreadContext)
{
    Emulator *Emu = ThreadContext.ViewPtr;
    NES *Nes = &Emu->Nes;

    u16 StackValue = (u16)Nes->Ram[0x100 + (u8)(Nes->CPU.SP + 1)];
    StackValue |= (u16)Nes->Ram[0x100 + (u8)(Nes->CPU.SP + 2)] << 8;

    Nes_DisplayableStatus Status = {
        .A = Nes->CPU.A,
        .X = Nes->CPU.X,
        .Y = Nes->CPU.Y,
        .PC = Nes->CPU.PC,
        .SP = Nes->CPU.SP + 0x100,
        .StackValue = StackValue,

        .N = MC6502_FlagGet(Nes->CPU.Flags, FLAG_N),
        .Z = MC6502_FlagGet(Nes->CPU.Flags, FLAG_Z),
        .V = MC6502_FlagGet(Nes->CPU.Flags, FLAG_V),
        .C = MC6502_FlagGet(Nes->CPU.Flags, FLAG_C),
        .I = MC6502_FlagGet(Nes->CPU.Flags, FLAG_I),
        .U = MC6502_FlagGet(Nes->CPU.Flags, FLAG_UNUSED),
        .B = MC6502_FlagGet(Nes->CPU.Flags, FLAG_B),
        .D = MC6502_FlagGet(Nes->CPU.Flags, FLAG_D),
    };

    NESPPU_GetRGBPalette(&Nes->PPU, Status.Palette, STATIC_ARRAY_SIZE(Status.Palette));
    Status.PatternTablesAvailable = NESPPU_GetPatternTables(
        &Nes->PPU, 
        Status.LeftPatternTable, 
        Status.RightPatternTable, 
        Emu->CurrentPalette
    );
    if (Nes->Cartridge)
    {
        Nes_Disassemble(&Emu->DisassemblerState, Nes->Cartridge, Status.PC, 
            Status.DisasmBeforePC, sizeof Status.DisasmBeforePC,
            Status.DisasmAtPC, sizeof Status.DisasmAtPC,
            Status.DisasmAfterPC, sizeof Status.DisasmAfterPC
        );
    }
    return Status;
}

static void NesInternal_OnPPUFrameCompletion(void *UserData)
{
    Emulator *Emu = UserData;

    /* swap the front and back buffers */
    u32 (*Tmp)[] = Emu->BackBuffer;
    Emu->BackBuffer = Emu->FrontBuffer;
    Emu->FrontBuffer = Tmp;

    Emu->Nes.PPU.ScreenOutput = Emu->BackBuffer[0];
}

static void NesInternal_OnPPUNmi(void *UserData)
{
    Emulator *Emu = UserData;
    MC6502_Interrupt(&Emu->Nes.CPU, VEC_NMI);
}

Platform_FrameBuffer Nes_PlatformQueryFrameBuffer(Platform_ThreadContext ThreadContext)
{
    Emulator *Emu = ThreadContext.ViewPtr;
    Platform_FrameBuffer Frame = {
        .Data = Emu->FrontBuffer,
        .Width = NES_SCREEN_WIDTH,
        .Height = NES_SCREEN_HEIGHT,
    };
    return Frame;
}




isize Nes_PlatformQueryThreadContextSize(void)
{
    return sizeof(Emulator);
}

Platform_AudioConfig Nes_OnEntry(Platform_ThreadContext ThreadContext)
{
    DEBUG_ASSERT(ThreadContext.ViewPtr);
    DEBUG_ASSERT(ThreadContext.SizeBytes == sizeof(Emulator));

    u32 AudioSampleRate = 48000;
    Emulator *Emu = ThreadContext.ViewPtr;

    Emu->MasterClkPerAudioSample = NES_MASTER_CLK / 44100;
    Emu->BackBuffer = &Emu->ScreenBuffer[0];
    Emu->FrontBuffer = &Emu->ScreenBuffer[1];
    Emu->EmulationDone = false;
    Emu->EmulationHalted = false;
    Emu->EmulationMode = EMUMODE_SINGLE_FRAME;
    Emu->CurrentPalette = 0;

    Emu->Nes.CPU = MC6502_Init(
        0, 
        &Emu->Nes, 
        NesInternal_ReadByte, 
        NesInternal_WriteByte
    );
    Emu->Nes.PPU = NESPPU_Init(
        Emu,
        NesInternal_OnPPUFrameCompletion, 
        NesInternal_OnPPUNmi, 
        *Emu->BackBuffer, 
        &Emu->Nes.Cartridge
    );
    Emu->Nes.APU = NESAPU_Init(
    );
    Emu->DisassemblerState = (NESDisassemblerState) {
        .Count = 16,
        .BytesPerLine = 3,
    };

    u32 AudioChannelCount = 1;
    Platform_AudioConfig AudioConfig = {
        .EnableAudio = true,
        .SampleRate = AudioSampleRate,
        .ChannelCount = AudioChannelCount, 
        .BufferSizeBytes = 1024 * AudioChannelCount * sizeof(int16_t),
    };
    return AudioConfig;
}

void Nes_OnAudioFailed(Platform_ThreadContext ThreadContext)
{
}



static Bool8 Nes_StepClock(NES *Nes)
{
    Nes->Clk++;
    NESAPU_StepClock(&Nes->APU);
    Bool8 FrameCompleted = NESPPU_StepClock(&Nes->PPU);
    if (Nes->Clk % 3 == 0)
    {
        if (!Nes->DMA)
        {
            MC6502_StepClock(&Nes->CPU);
        }
        else if (Nes->DMAOutOfSync)
        {
            if (Nes->Clk % 2 == 1)
            {
                Nes->DMAOutOfSync = false;
                Nes->DMASaveAddr = Nes->PPU.OAMAddr;
            }
            /* else wait until CPU clk is odd, and then start syncing */
        }
        else
        {
            /* read cpu memory on even clk */
            if (Nes->Clk % 2 == 0)
            {
                Nes->DMAData = NesInternal_ReadByte(Nes, Nes->DMAAddr);
            }
            /* write to ppu memory on odd clk */
            else
            {
                u16 DMAAddrPrev = Nes->DMAAddr++;
                NESPPU_ExternalWrite(&Nes->PPU, PPU_OAM_DATA, Nes->DMAData);

                /* transfer is complete (1 page has wrapped) */
                if ((DMAAddrPrev & 0xFF00) != (Nes->DMAAddr & 0xFF00))
                {
                    Nes->DMA = false;
                    Nes->DMAOutOfSync = true;
                    Nes->PPU.OAMAddr = Nes->DMASaveAddr;
                }
            }
        }
    }
    return FrameCompleted;
}

int16_t Nes_OnAudioSampleRequest(Platform_ThreadContext ThreadContext, double t)
{
    Emulator *Emu = ThreadContext.ViewPtr;
    NES *Nes = &Emu->Nes;

    if (!Emu->EmulationHalted)
    {
        for (u32 i = 0; i < Emu->MasterClkPerAudioSample; i++)
        {
            Nes_StepClock(Nes);
        }
    }
    else if (!Emu->EmulationDone)
    {
        Emu->EmulationDone = true;
    }

    double AudioSample = Nes->APU.AudioSample;
    if (AudioSample >= INT16_MAX)
        return INT16_MAX;
    if (AudioSample <= INT16_MIN)
        return INT16_MIN;
    return (int16_t)AudioSample;
}





void Nes_OnLoop(Platform_ThreadContext ThreadContext, double ElapsedTime)
{
    return;
    Emulator *Emu = ThreadContext.ViewPtr;
    NES *Nes = &Emu->Nes;
    if (!Emu->EmulationHalted)
    {
        if (Emu->ResidueTime < ElapsedTime)
        {
            Emu->ResidueTime += 1000.0 / 60.0;
            Bool8 FrameCompleted = false;
            do {
                FrameCompleted = Nes_StepClock(Nes);
            } while (!FrameCompleted);
        }
    }
    else
    {
        Emu->ResidueTime = ElapsedTime;
        if (Emu->EmulationDone)
            return;

        Emu->EmulationDone = true;
        switch (Emu->EmulationMode)
        {
        case EMUMODE_SINGLE_STEP:
        {
            do {
                Nes_StepClock(Nes);
            } while (Nes->CPU.CyclesLeft > 0 && !Nes->CPU.Halt);
            Nes_StepClock(Nes);
            Nes_StepClock(Nes);
            Nes_StepClock(Nes);
        } break;
        case EMUMODE_SINGLE_FRAME:
        {
            Bool8 FrameDone;
            do {
                FrameDone = Nes_StepClock(Nes);
            } while (!FrameDone);
        } break;
        }
    }
}

void Nes_AtExit(Platform_ThreadContext ThreadContext)
{
    Emulator *Emu = ThreadContext.ViewPtr;
    if (Emu->Nes.Cartridge)
    {
        NESCartridge_Destroy(Emu->Nes.Cartridge);
    }
}




void Nes_OnEmulatorToggleHalt(Platform_ThreadContext ThreadContext)
{
    Emulator *Emu = ThreadContext.ViewPtr;
    Emu->EmulationDone = true;
    Emu->EmulationHalted = !Emu->EmulationHalted;
}

void Nes_OnEmulatorSingleStep(Platform_ThreadContext ThreadContext)
{
    Emulator *Emu = ThreadContext.ViewPtr;
    Emu->EmulationMode = EMUMODE_SINGLE_STEP;
    Emu->EmulationDone = false;
}

void Nes_OnEmulatorSingleFrame(Platform_ThreadContext ThreadContext)
{
    Emulator *Emu = ThreadContext.ViewPtr;
    Emu->EmulationMode = EMUMODE_SINGLE_FRAME;
    Emu->EmulationDone = false;
}

void Nes_OnEmulatorReset(Platform_ThreadContext ThreadContext)
{
    Emulator *Emu = ThreadContext.ViewPtr;
    Emu->Nes.Clk = 0;
    MC6502_Reset(&Emu->Nes.CPU);
    NESPPU_Reset(&Emu->Nes.PPU);
    NESAPU_Reset(&Emu->Nes.APU);
}

void Nes_OnEmulatorTogglePalette(Platform_ThreadContext ThreadContext)
{
    Emulator *Emu = ThreadContext.ViewPtr;
    Emu->CurrentPalette++;
    Emu->CurrentPalette &= 0x7;
}



