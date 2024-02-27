
#include <stdlib.h>
#include <time.h>

#include "Common.h"
#include "Nes.h"
#include "Cartridge.h"


typedef struct NESPPU NESPPU;
typedef void (*NESPPU_FrameCompletionCallback)(NESPPU *);

/* CTRL */
#define PPUCTRL_NAMETABLE_X     (1 << 0)
#define PPUCTRL_NAMETABLE_Y     (1 << 1)
#define PPUCTRL_INC32           (1 << 2) 
#define PPUCTRL_SPR_PATTERN_ADDR (1 << 3) /* 0: $0000; 1: $1000 */
#define PPUCTRL_BG_PATTERN_ADDR (1 << 4)  /* 0: $0000; 1: $1000 */
#define PPUCTRL_SPR_SIZE        (1 << 5)  /* 0: 8x8  ; 1: 8x16  */
#define PPUCTRL_SLAVE_MODE      (1 << 6)  /* unused */
#define PPUCTRL_MNI_ENABLE      (1 << 7)
/* STATUS */
#define PPUSTATUS_SPR0_HIT      (1 << 5)
#define PPUSTATUS_SPR_OVERFLOW  (1 << 6)
#define PPUSTATUS_VBLANK        (1 << 7)
/* MASK */
#define PPUMASK_GRAYSCALE       (1 << 0)
#define PPUMASK_SHOW_BG_LEFT    (1 << 1)
#define PPUMASK_SHOW_SPR_LEFT   (1 << 2)
#define PPUMASK_SHOW_BG         (1 << 3)
#define PPUMASK_SHOW_SPR        (1 << 4)
#define PPUMASK_EMPHASIZE_RED   (1 << 5)
#define PPUMASK_EMPHASIZE_GREEN (1 << 6)
#define PPUMASK_EMPHASIZE_BLUE  (1 << 7)


struct NESPPU 
{
    uint Clk;
    int Scanline;
    u32 ScreenIndex;

    struct {
        uint t:15;
        uint v:15;
        uint x:3;
        uint w:1;
    } Loopy;
    u8 Ctrl;
    u8 Mask;
    u8 Status;

    u8 PaletteColorIndex[0x20];
    u8 NameAndAttributeTable[0x800];
    u8 ObjectAttributeMemory[256];

    NESPPU_FrameCompletionCallback FrameCompletionCallback;
    u32 *ScreenOutput;
    NESCartridge **CartridgeHandle;
};

typedef enum NESPPU_CtrlReg 
{
    PPU_CTRL = 0,
    PPU_MASK,
    PPU_STATUS,
    PPU_OAM_ADDR,
    PPU_OAM_DATA,
    PPU_SCROLL,
    PPU_ADDR,
    PPU_DATA,
} NESPPU_CtrlReg;

static u32 sPPURGBPalette[] = {
    0x007C7C7C,
    0x000000FC,
    0x000000BC,
    0x004428BC,
    0x00940084,
    0x00A80020,
    0x00A81000,
    0x00881400,
    0x00503000,
    0x00007800,
    0x00006800,
    0x00005800,
    0x00004058,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00BCBCBC,
    0x000078F8,
    0x000058F8,
    0x006844FC,
    0x00D800CC,
    0x00E40058,
    0x00F83800,
    0x00E45C10,
    0x00AC7C00,
    0x0000B800,
    0x0000A800,
    0x0000A844,
    0x00008888,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00F8F8F8,
    0x003CBCFC,
    0x006888FC,
    0x009878F8,
    0x00F878F8,
    0x00F85898,
    0x00F87858,
    0x00FCA044,
    0x00F8B800,
    0x00B8F818,
    0x0058D854,
    0x0058F898,
    0x0000E8D8,
    0x00787878,
    0x00000000,
    0x00000000,
    0x00FCFCFC,
    0x00A4E4FC,
    0x00B8B8F8,
    0x00D8B8F8,
    0x00F8B8F8,
    0x00F8A4C0,
    0x00F0D0B0,
    0x00FCE0A8,
    0x00F8D878,
    0x00D8F878,
    0x00B8F8B8,
    0x00B8F8D8,
    0x0000FCFC,
    0x00F8D8F8,
    0x00000000,
    0x00000000,
};



NESPPU NESPPU_Init(
    NESPPU_FrameCompletionCallback Callback, 
    u32 BackBuffer[NES_SCREEN_BUFFER_SIZE],
    NESCartridge **CartridgeHandle)
{
    NESPPU This = {
        .ScreenOutput = BackBuffer,
        .FrameCompletionCallback = Callback,
        .CartridgeHandle = CartridgeHandle,
    };
    for (uint i = 0; i < STATIC_ARRAY_SIZE(This.PaletteColorIndex); i++)
    {
        This.PaletteColorIndex[i] = i;
    }
    srand(time(NULL));
    return This;
}

void NESPPU_Reset(NESPPU *This)
{
    (void)This;
}

Bool8 NESPPU_StepClock(NESPPU *This)
{
    Bool8 FrameCompleted = false;
    /* does the render, a single pixel */
#if 0
    if (This->Clk < NES_SCREEN_WIDTH 
    && This->Scanline < NES_SCREEN_HEIGHT)
    {
        u32 LookupValue = rand() * STATIC_ARRAY_SIZE(sPPURGBPalette) / RAND_MAX;
        This->ScreenOutput[This->ScreenIndex] = sPPURGBPalette[LookupValue];
        This->ScreenIndex++;
        if (This->ScreenIndex >= NES_SCREEN_BUFFER_SIZE)
            This->ScreenIndex = 0;
    }
#else 
#endif /* */

    This->Clk++;
    if (This->Clk == 341) /* PPU cycles per scanline */
    {
        This->Clk = 0;
        This->Scanline++;
        if (This->Scanline == 261) /* last scanline */
        {
            This->Scanline = -1; /* -1 to wrap around later */
            This->FrameCompletionCallback(This);
            FrameCompleted = true;
        }
    }
    return FrameCompleted;
}


static NESNametableOrientation NESPPU_GetMirroringModeFromCartridge(NESCartridge **CartridgeHandle)
{
    if (!CartridgeHandle || !*CartridgeHandle)
        return NAMETABLE_VERTICAL;

    return (*CartridgeHandle)->MirroringMode;
}

static u16 NESPPU_MirrorNametableAddr(u16 LogicalAddress, NESNametableOrientation MirroringMode)
{
    u16 PhysicalAddress = 0;
    switch (MirroringMode)
    {
    case NAMETABLE_VERTICAL: 
    {
        PhysicalAddress = LogicalAddress & 0x07FF;
    } break;
    case NAMETABLE_HORIZONTAL:
    {
        PhysicalAddress = LogicalAddress & 0x0FFF;
        PhysicalAddress = PhysicalAddress > 0x0800?
            ((PhysicalAddress & 0x3FF) + 0x400)
            :(PhysicalAddress & 0x3FF);
    } break;
    case NAMETABLE_ONESCREEN_HI:
    case NAMETABLE_ONESCREEN_LO:
    {
        DEBUG_ASSERT(false && "TODO: one screen mirroring");
    } break;
    }
    return PhysicalAddress;
}

u8 NESPPU_ReadInternalMemory(NESPPU *This, u16 Address)
{
    Address &= 0x3FFF;
    /* CHR ROM memory from the cartridge */
    if (Address < 0x2000) 
    {
        if (!This->CartridgeHandle || !*This->CartridgeHandle)
            return 0;

        return NESCartridge_Read(*This->CartridgeHandle, Address);
    }
    /* NOTE: PPU VRAM, but cartridge can also hijack these addresses through mappers */
    else if (IN_RANGE(0x2000, Address, 0x3EFF)) 
    {
        NESNametableOrientation MirroringMode = NESPPU_GetMirroringModeFromCartridge(This->CartridgeHandle);
        Address = NESPPU_MirrorNametableAddr(Address, MirroringMode);
        return This->NameAndAttributeTable[Address];
    }
    else /* 0x3F00-0x3FFF: sprite palette, image palette */
    {
        Address %= 0x20;
        return This->PaletteColorIndex[Address];
    }
}

void NESPPU_WriteInternalMemory(NESPPU *This, u16 Address, u8 Byte)
{
    Address &= 0x3FFF;
    /* CHR ROM memory from the cartridge */
    if (Address < 0x2000) 
    {
        if (This->CartridgeHandle && *This->CartridgeHandle)
        {
            NESCartridge_Write(*This->CartridgeHandle, Address, Byte);
        }
    }
    /* NOTE: PPU VRAM, but cartridge can also hijack these addresses thorugh mappers */
    else if (IN_RANGE(0x2000, Address, 0x3EFF)) 
    {
        NESNametableOrientation MirroringMode = NESPPU_GetMirroringModeFromCartridge(This->CartridgeHandle);
        Address = NESPPU_MirrorNametableAddr(Address, MirroringMode);
        This->NameAndAttributeTable[Address] = Byte;
    }
    else /* 0x3F00-0x3FFF: sprite palette, image palette */
    {
        Address %= 0x20;
        This->PaletteColorIndex[Address] = Byte;
    }
}



