#ifndef NES_PPU_C
#define NES_PPU_C

#include "Common.h"
#include "Cartridge.h"
#include "Utils.h"
#include "Nes.h"


typedef struct NESPPU NESPPU;
typedef void (*NESPPU_FrameCompletionCallback)(NESPPU *);
typedef void (*NESPPU_NmiCallback)(NESPPU *);

/* CTRL */
#define PPUCTRL_NAMETABLE_X     (1 << 0)
#define PPUCTRL_NAMETABLE_Y     (1 << 1)
#define PPUCTRL_INC32           (1 << 2) 
#define PPUCTRL_SPR_PATTERN_ADDR (1 << 3) /* 0: $0000; 1: $1000 */
#define PPUCTRL_BG_PATTERN_ADDR (1 << 4)  /* 0: $0000; 1: $1000 */
#define PPUCTRL_SPR_SIZE16      (1 << 5)  /* 0: 8x8  ; 1: 8x16  */
#define PPUCTRL_SLAVE_MODE      (1 << 6)  /* unused */
#define PPUCTRL_NMI_ENABLE      (1 << 7)
/* STATUS */
#define PPUSTATUS_SPR0_HIT      (0x40)
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


#define COARSE_X_MASK 0x001F
#define COARSE_Y_MASK 0x03E0
#define NAMETABLE_OFFSET 0x2000
#define FINE_Y_MASK 0x7000
#define TILE_SIZE 16

#define GET_COARSE_X(u16Register) (((u16Register) & COARSE_X_MASK))
#define GET_COARSE_Y(u16Register) (((u16Register) & COARSE_Y_MASK) >> 5)
#define GET_NAMETABLE_SELECT(u16Register) (((u16Register) >> 12) & 0x3)
#define GET_FINE_Y(u16Register) (((u16Register) & FINE_Y_MASK) >> 12)

#define SET_COARSE_X(u16Register, Val) MASKED_LOAD(u16Register, Val, 0x001F)
#define SET_COARSE_Y(u16Register, Val) MASKED_LOAD(u16Register, (u16)(Val) << 5, 0x03E0)
#define SET_NAMETABLE_SELECT(u16Register, Val) MASKED_LOAD(u16Register, (u16)(Val) << 10, 0x0C00)
#define SET_FINE_Y(u16Register, Val) MASKED_LOAD(u16Register, (u16)(Val) << 12, 0x7000)


#define PPU_OA_PRIORITIZE_FOREGROUND    (1 << 5)
#define PPU_OA_FLIP_HORIZONTAL          (1 << 6)
#define PPU_OA_FLIP_VERTICAL            (1 << 7)
#define PPU_OA_PALETTE                  0x03
typedef struct NESPPU_ObjectAttribute 
{
    u8 y, 
       ID, 
       Attribute, 
       x;
} NESPPU_ObjectAttribute;

struct NESPPU 
{
    uint Clk;
    int Scanline;
    u32 ScreenIndex;

    /* loopy registers */
    struct {
        uint t:15;
        uint v:15;
        uint x:3;
        uint w:1;
    } Loopy;
    u8 Ctrl;
    u8 Mask;
    u8 Status;
    Bool8 CurrentFrameIsOdd;
    Bool8 SupressNMIThisFrame;
    Bool8 ShouldNotSetVBlankThisFrame;


    u8 BgNametableByteLatch;
    u8 BgAttrBitsLatch;
    u8 BgPatternLoLatch;
    u8 BgPatternHiLatch;

    u16 BgPatternHiShifter;
    u16 BgPatternLoShifter;
    u16 BgAttrHiShifter;
    u16 BgAttrLoShifter;

    u8 ReadBuffer;
    u8 OAMAddr;

    u8 PaletteColorIndex[NES_PALETTE_SIZE];
    u8 NametableMemory[NES_NAMETABLE_SIZE*2];
    union {
        NESPPU_ObjectAttribute Entries[64];
        u8 Bytes[256];
    } OAM;


    NESPPU_ObjectAttribute VisibleSprites[8];
    u8 SprPatternLoShifter[8];
    u8 SprPatternHiShifter[8];
    u8 VisibleSpriteCount;
    Bool8 Spr0IsVisible;

    NESPPU_FrameCompletionCallback FrameCompletionCallback;
    NESPPU_NmiCallback NmiCallback;
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
    0x00545454,
    0x00001E74,
    0x00081090,
    0x00300088,
    0x00440064,
    0x005C0030,
    0x00540400,
    0x003C1800,
    0x00202A00,
    0x00083A00,
    0x00004000,
    0x00003C00,
    0x0000323C,
    0x00000000,
    0x00000000,
    0x00000000,
    0x00989698,

    0x00084CC4,
    0x003032EC,
    0x005C1EE4,
    0x008814B0,
    0x00A01464,
    0x00982220,
    0x00783C00,
    0x00545A00,
    0x00287200,
    0x00087C00,
    0x00007628,
    0x00006678,
    0x00000000,
    0x00000000,
    0x00000000,

    0x00ECEEEC,
    0x004C9AEC,
    0x00787CEC,
    0x00B062EC,
    0x00E454EC,
    0x00EC58B4,
    0x00EC6A64,
    0x00D48820,
    0x00A0AA00,
    0x0074C400,
    0x004C6C20,
    0x0038CC6c,
    0x0038B4CC,
    0x003C3C3C,
    0x00000000,
    0x00000000,

    0x00ECEEEC,
    0x00A8CCEC,
    0x00BCBCEC,
    0x00D4B2EC,
    0x00ECAEEC,
    0x00ECAED4,
    0x00ECB4B0,
    0x00E4C490,
    0x00CCD278,
    0x00B4DE78,
    0x00A8E290,
    0x0098E2B4,
    0x00A0D6E4,
    0x00A0A2A0,
    0x00000000,
    0x00000000
};



NESPPU NESPPU_Init(
    NESPPU_FrameCompletionCallback FrameCallback, 
    NESPPU_NmiCallback NmiCallback,
    u32 BackBuffer[],
    NESCartridge **CartridgeHandle)
{
    NESPPU This = {
        .ScreenOutput = BackBuffer,
        .FrameCompletionCallback = FrameCallback,
        .NmiCallback = NmiCallback,
        .CartridgeHandle = CartridgeHandle,
        .Mask = PPUMASK_SHOW_BG,
    };
    for (uint i = 0; i < STATIC_ARRAY_SIZE(This.PaletteColorIndex); i++)
    {
        This.PaletteColorIndex[i] = i;
    }
    return This;
}

void NESPPU_Reset(NESPPU *This)
{
    This->Clk = 0;
    This->Scanline = 0;
    This->Ctrl = 0;
    This->Mask = 0;
    This->Loopy.w = 0;
    This->CurrentFrameIsOdd = false;
    This->VisibleSpriteCount = 0;
    This->SupressNMIThisFrame = false;
    This->ShouldNotSetVBlankThisFrame = false;
    Memset(This->VisibleSprites, 0xFF, sizeof This->VisibleSprites);

    This->ScreenIndex = 0;
}



static u16 NESPPU_MirrorNameTableAddr(NESCartridge **CartridgeHandle, u16 LogicalAddress)
{
    NESNameTableMirroring MirroringMode = NAMETABLE_VERTICAL;
    if (CartridgeHandle && *CartridgeHandle)
        MirroringMode = (*CartridgeHandle)->MirroringMode;

    u16 PhysicalAddress = 0;
    switch (MirroringMode)
    {
    case NAMETABLE_VERTICAL: 
    {
        /*
         * Logical:
         * 0x0000 ---- 0x0400 ---- 0x0800 ---- 0x0C00 ---- 0x1000 
         *    |     0     |     1     |     2     |     3     |
         *    *-----------*-----------*-----------*-----------*
         * Physical:
         *    |     0     |     1     |     0     |     1     |
         *    *-----------*-----------*-----------*-----------*
         */
        PhysicalAddress = LogicalAddress % (NES_NAMETABLE_SIZE*2);
    } break;
    case NAMETABLE_HORIZONTAL:
    {
        /*
         * Logical:
         * 0x0000 ---- 0x0400 ---- 0x0800 ---- 0x0C00 ---- 0x1000 
         *    |     0     |     1     |     2     |     3     |
         *    *-----------*-----------*-----------*-----------*
         * Physical:
         *    |     0     |     0     |     1     |     1     |
         *    *-----------*-----------*-----------*-----------*
         */
        PhysicalAddress = LogicalAddress % NES_NAMETABLE_SIZE;
        if (IN_RANGE(0x0800, LogicalAddress, 0x0FFF))
        {
            PhysicalAddress += NES_NAMETABLE_SIZE;
        }
    } break;
    case NAMETABLE_ONESCREEN_HI:
    {
        PhysicalAddress = LogicalAddress % NES_NAMETABLE_SIZE;
    } break;
    case NAMETABLE_ONESCREEN_LO:
    {
        PhysicalAddress = (LogicalAddress % NES_NAMETABLE_SIZE) + NES_NAMETABLE_SIZE;
    } break;
    }
    return PhysicalAddress;
}



static u8 NESPPU_ReadInternalMemory(NESPPU *This, u16 Address)
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
        Address = NESPPU_MirrorNameTableAddr(This->CartridgeHandle, Address & 0x0FFF);
        return This->NametableMemory[Address];
    }
    else /* 0x3F00-0x3FFF: sprite palette, image palette */
    {
        Address &= 0x1F;
        /* mirror background entries */
        if ((Address & 0x0003) == 0)
            Address &= 0xF;
        return This->PaletteColorIndex[Address];
    }
}

static void NESPPU_WriteInternalMemory(NESPPU *This, u16 Address, u8 Byte)
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
        Address = NESPPU_MirrorNameTableAddr(This->CartridgeHandle, Address & 0xFFF);
        This->NametableMemory[Address] = Byte;
    }
    else /* 0x3F00-0x3FFF: sprite palette, image palette */
    {
        Address &= 0x1F;
        /* mirror background entries */
        if ((Address & 0x0003) == 0)
            Address &= 0xF;
        This->PaletteColorIndex[Address] = Byte;
    }
}


static u32 NESPPU_GetRGBFromPixelAndPalette(NESPPU *This, u8 Pixel, u8 Palette)
{
    /* take 2 lower bits only */
    Pixel &= 0x3;
    Palette &= 0x7;

    u16 PaletteBase = 0x3F00;
    u16 PaletteIndex = (Palette << 2) | Pixel;
    u8 ColorIndex = NESPPU_ReadInternalMemory(This, PaletteBase + PaletteIndex);
    return sPPURGBPalette[ColorIndex % STATIC_ARRAY_SIZE(sPPURGBPalette)];
}


void NESPPU_GetRGBPalette(NESPPU *This, u32 *RGBPaletteArray, isize ArrayElemCount)
{
    DEBUG_ASSERT(ArrayElemCount == NES_PALETTE_SIZE);
    for (int i = 0; i < NES_PALETTE_SIZE; i++)
    {
        u32 Color = NESPPU_GetRGBFromPixelAndPalette(This, i >> 2, i);
        RGBPaletteArray[i] = Color;
    }
}

Bool8 NESPPU_GetPatternTables(NESPPU *This, 
    u32 RGBLeftPatternTable[NES_PATTERN_TABLE_HEIGHT_PIX][NES_PATTERN_TABLE_WIDTH_PIX], 
    u32 RGBRightPatternTable[NES_PATTERN_TABLE_HEIGHT_PIX][NES_PATTERN_TABLE_WIDTH_PIX], 
    u8 Palette)
{
    if (!This->CartridgeHandle || !*This->CartridgeHandle)
        return false;

    Palette &= 0x7;
    NESCartridge *Cartridge = *This->CartridgeHandle;
    int HorizontalTileCount = 16;   /* number of tiles a pattern table contains in the horizontal direction */
    int VerticalTileCount = 16;     /* number of tiles a pattern table contains in the vertical direction */
    int TileWidthPixel = 8;         /* tile width in pixel */
    int TileHeightPixel = 8;        /* tile height in pixel */

    for (int y = 0;
        y < VerticalTileCount;
        y++)
    {
        for (int x = 0; 
            x < HorizontalTileCount;
            x++)
        {
            int TileByteIndex = (y*HorizontalTileCount + x) * TILE_SIZE;
            for (int YPixel = 0; 
                YPixel < TileHeightPixel;
                YPixel++)
            {
                int RightTable = NES_PATTERN_TABLE_SIZE;
                u8 LeftPatternLow = NESCartridge_DebugPPURead(Cartridge, TileByteIndex + YPixel);
                u8 LeftPatternHigh = NESCartridge_DebugPPURead(Cartridge, TileByteIndex + YPixel + TILE_SIZE/2);
                u8 RightPatternLow = NESCartridge_DebugPPURead(Cartridge, RightTable + TileByteIndex + YPixel);
                u8 RightPatternHigh = NESCartridge_DebugPPURead(Cartridge, RightTable + TileByteIndex + YPixel + TILE_SIZE/2);

                for (int XPixel = 0; 
                    XPixel < TileWidthPixel; 
                    XPixel++)
                {
                    u8 LeftPatternPixel = (LeftPatternLow & 1) | ((LeftPatternHigh & 1) << 1);
                    u8 RightPatternPixel = (RightPatternLow & 1) | ((RightPatternHigh & 1) << 1);
                    u32 LeftPatternColor = NESPPU_GetRGBFromPixelAndPalette(This, LeftPatternPixel, Palette);
                    u32 RightPatternColor = NESPPU_GetRGBFromPixelAndPalette(This, RightPatternPixel, Palette);

                    RGBLeftPatternTable[YPixel + y*TileHeightPixel][7 - XPixel + x*TileWidthPixel] = LeftPatternColor;
                    RGBRightPatternTable[YPixel + y*TileHeightPixel][7 - XPixel + x*TileWidthPixel] = RightPatternColor;

                    LeftPatternLow >>= 1;
                    LeftPatternHigh >>= 1;
                    RightPatternLow >>= 1;
                    RightPatternHigh >>= 1;
                }
            }
        }
    }
    return true;
}



u8 NESPPU_ExternalRead(NESPPU *This, u16 Address)
{
    switch ((NESPPU_CtrlReg)(Address & 0x7))
    {
    case PPU_CTRL: /* read not allowed */ break;
    case PPU_MASK: /* read not allowed */ break;
    case PPU_STATUS: 
    {
        u8 Status = This->Status;
        if (This->Scanline == 240)
        {
            /* reading status 4 to 1 cycles before vblank is about to set */
            if (This->Clk == 0 || This->Clk == 1) 
            {
                /* disables status for the whole duration, disables NMI */
                This->ShouldNotSetVBlankThisFrame = true;
                This->SupressNMIThisFrame = true;
            }
            /* reading status when vblank is set, or 1 cycle after */
            else if (This->Clk == 1 || This->Clk == 2)
            {
                This->SupressNMIThisFrame = true;
            }
        }
        This->Status &= ~PPUSTATUS_VBLANK;
        This->Loopy.w = 0;
        return Status;
    } break;
    case PPU_OAM_ADDR: /* read not allowed */ break;
    case PPU_OAM_DATA:
    {
        return This->OAM.Bytes[This->OAMAddr];
    } break;
    case PPU_SCROLL: /* read not allowed */ break;
    case PPU_ADDR: /* read not allowed */ break;
    case PPU_DATA:
    {
        u8 ReadValue;
        if (This->Loopy.v >= 0x3F00) /* palette addr: read directly */
        {
            ReadValue = NESPPU_ReadInternalMemory(This, This->Loopy.v);
        }
        else /* other addr: buffer the read, return prev read */
        {
            ReadValue = This->ReadBuffer;
        }
        /* NOTE: always read into buffer, even when reading palette */
        This->ReadBuffer = NESPPU_ReadInternalMemory(This, This->Loopy.v);

        This->Loopy.v += (This->Ctrl & PPUCTRL_INC32)? 32 : 1;
        return ReadValue;
    } break;
    }
    return 0;
}

void NESPPU_ExternalWrite(NESPPU *This, u16 Address, u8 Byte)
{
    switch ((NESPPU_CtrlReg)(Address & 0x7))
    {
    case PPU_CTRL:
    {
        This->Ctrl = Byte;
        u16 NametableBits = (u16)Byte << 10;
        MASKED_LOAD(This->Loopy.t, NametableBits, 0x0C00);
    } break;
    case PPU_MASK: This->Mask = Byte; break;
    case PPU_STATUS: /* write not allowed */ break;
    case PPU_OAM_ADDR:
    {
        This->OAMAddr = Byte;
    } break;
    case PPU_OAM_DATA:
    {
        This->OAM.Bytes[This->OAMAddr++] = Byte;
    } break;
    case PPU_SCROLL: 
    {
        if (This->Loopy.w == 0) /* first write */
        {
            This->Loopy.x = Byte & 0x7;
            SET_COARSE_X(This->Loopy.t, Byte >> 3);
        }
        else /* second write */
        {
            SET_FINE_Y(This->Loopy.t, Byte & 0x7);
            SET_COARSE_Y(This->Loopy.t, Byte >> 3);
        }
        This->Loopy.w = ~This->Loopy.w;
    } break;
    case PPU_ADDR:
    {
        /* latching because each cpu cycle is 3 ppu cycles */
        if (This->Loopy.w == 0) /* first write */
        {
            u16 AddrHi = ((u16)Byte << 8) & 0x3F00;
            MASKED_LOAD(This->Loopy.t, AddrHi, 0xFF00);
        }
        else /* second write */
        {
            MASKED_LOAD(This->Loopy.t, (u16)Byte, 0x00FF);
            This->Loopy.v = This->Loopy.t;
        }
        This->Loopy.w = ~This->Loopy.w;
    } break;
    case PPU_DATA:
    {
        NESPPU_WriteInternalMemory(This, This->Loopy.v, Byte);
        This->Loopy.v += (This->Ctrl & PPUCTRL_INC32)? 32 : 1;
    } break;
    }
}




void NESPPU_UpdateShifters(NESPPU *This)
{
    if (This->Mask & PPUMASK_SHOW_BG)
    {
        This->BgPatternHiShifter <<= 1;
        This->BgPatternLoShifter <<= 1;
        This->BgAttrHiShifter <<= 1;
        This->BgAttrLoShifter <<= 1;
    }

    if ((This->Mask & PPUMASK_SHOW_SPR) && IN_RANGE(1, This->Clk, 257))
    {
        for (uint i = 0; i < This->VisibleSpriteCount; i++)
        {
            /* wait until we arrived at the sprite to render */
            if (This->VisibleSprites[i].x > 0)
            {
                This->VisibleSprites[i].x--;
            }
            else
            {
                This->SprPatternHiShifter[i] <<= 1;
                This->SprPatternLoShifter[i] <<= 1;
            }
        }
    }
}

void NESPPU_ReloadBackgroundShifters(NESPPU *This)
{
    if (This->Mask & PPUMASK_SHOW_BG)
    {
        MASKED_LOAD(This->BgPatternHiShifter, This->BgPatternHiLatch, 0xFF);
        MASKED_LOAD(This->BgPatternLoShifter, This->BgPatternLoLatch, 0xFF);

        u16 InflatedLoAttrBit = This->BgAttrBitsLatch & 0x01? 
            0xFF : 0x00;
        u16 InflatedHiAttrBit = This->BgAttrBitsLatch & 0x02?
            0xFF : 0x00;

        MASKED_LOAD(This->BgAttrLoShifter, InflatedLoAttrBit, 0xFF);
        MASKED_LOAD(This->BgAttrHiShifter, InflatedHiAttrBit, 0xFF);
    }
}

static void NESPPU_RenderSinglePixel(NESPPU *This)
{
    Bool8 ShouldRenderBackground = (This->Mask & PPUMASK_SHOW_BG) != 0;
    if (!(This->Mask & PPUMASK_SHOW_BG_LEFT) && This->Clk < 8)
        ShouldRenderBackground = false;
    Bool8 ShouldRenderForeground = (This->Mask & PPUMASK_SHOW_SPR) != 0;
    if (!(This->Mask & PPUMASK_SHOW_SPR_LEFT) && This->Clk < 8)
        ShouldRenderForeground = false;
    /* get background pixel values from shift registers */
    u8 BackgroundPixel = 0;
    u8 BackgroundPalette = 0;
    if (ShouldRenderBackground)
    {
        /* fine x is the offset from the highest bit in the shift register */
        u16 FineXSelect = 0x8000 >> This->Loopy.x;
        u8 LowBitOfPixel = (This->BgPatternLoShifter & FineXSelect) != 0;
        u8 HighBitOfPixel = (This->BgPatternHiShifter & FineXSelect) != 0;
        u8 LowBitOfPalette = (This->BgAttrLoShifter & FineXSelect) != 0;
        u8 HighBitOfPalette = (This->BgAttrHiShifter & FineXSelect) != 0;

        BackgroundPixel = LowBitOfPixel | (HighBitOfPixel << 1);
        BackgroundPalette = LowBitOfPalette | (HighBitOfPalette << 1);
    }


    /* get foreground (sprite) pixel values */
    u8 ForegroundPixel = 0;
    u8 ForegroundPalette = 0;
    Bool8 PrioritizeForeground = 0;
    Bool8 RenderingSpr0 = false;
    if (ShouldRenderForeground)
    {
        for (uint i = 0; i < This->VisibleSpriteCount; i++)
        {
            NESPPU_ObjectAttribute *CurrentSprite = &This->VisibleSprites[i];
            /* in the process of rendering */
            if (CurrentSprite->x == 0)
            {
                u8 LowBitOfPixel = (This->SprPatternLoShifter[i] >> 7) & 0x1;
                u8 HighBitOfPixel = (This->SprPatternHiShifter[i] >> 7) & 0x1;

                ForegroundPixel = LowBitOfPixel | (HighBitOfPixel << 1);
                ForegroundPalette = (CurrentSprite->Attribute & PPU_OA_PALETTE) | 0x04; /* foreground offset into palette */
                PrioritizeForeground = (CurrentSprite->Attribute & PPU_OA_PRIORITIZE_FOREGROUND) == 0;

                /* foreground (sprite) is visible, and since the earlier the sprite is, 
                 * the higher the priority it has, so we render it while
                 * discarding other sprites that come after it in the VisibleSprites buffer */
                if (ForegroundPixel != 0)
                {
                    RenderingSpr0 = i == 0 && This->Spr0IsVisible;
                    break;
                }

                /* otherwise the sprite is transparent and we keep looking 
                 * for a nontransparent sprite to render */
            }
        }
    }


    /* determine the final pixel value taken the background and foreground into account */
    /* default is transparent (whatever the background color is) */
    u8 Pixel = 0;
    u8 Palette = 0;
    if (BackgroundPixel && ForegroundPixel)
    {
        uint LowerBound = (This->Mask & (PPUMASK_SHOW_SPR_LEFT | PPUMASK_SHOW_BG_LEFT))?
            1 : 9;
        if (RenderingSpr0 && IN_RANGE(LowerBound, This->Clk, 257)
        && ShouldRenderBackground && ShouldRenderForeground)
        {
            This->Status |= PPUSTATUS_SPR0_HIT;
        }

        if (PrioritizeForeground)
        {
            Pixel = ForegroundPixel;
            Palette = ForegroundPalette;
        }
        else
        {
            Pixel = BackgroundPixel;
            Palette = BackgroundPalette;
        }
    }
    /* transparent foreground */
    else if (BackgroundPixel)
    {
        Pixel = BackgroundPixel;
        Palette = BackgroundPalette;
    }
    /* transparent background */
    else if (ForegroundPixel)
    {
        Pixel = ForegroundPixel;
        Palette = ForegroundPalette;
    }


    if (IN_RANGE(1, This->Clk, NES_SCREEN_WIDTH) 
    && IN_RANGE(0, This->Scanline, NES_SCREEN_HEIGHT - 1))
    {
        u32 Color = NESPPU_GetRGBFromPixelAndPalette(This, Pixel, Palette);
        This->ScreenOutput[This->ScreenIndex++] = Color;
        if (This->ScreenIndex >= NES_SCREEN_BUFFER_SIZE)
            This->ScreenIndex = 0;
    }
}

static void NESPPU_FetchBackgroundData(NESPPU *This)
{
    NESPPU_UpdateShifters(This);
    switch ((This->Clk - 1) % 8)
    {
    case 0: /* fetch nametable byte */
    {
        NESPPU_ReloadBackgroundShifters(This);
        /* addr: 0010 NN yyyyy xxxxx 
         *       ^^^^ ------------------ NAMETABLE_OFFSET (0x2000)
         *            ^^ --------------- nametable y, x
         *               ^^^^^ --------- coarse y
         *                     ^^^^^ --- coarse x
         */
        u16 NametableAddr = NAMETABLE_OFFSET + (This->Loopy.v & 0x0FFF);
        This->BgNametableByteLatch = NESPPU_ReadInternalMemory(This, NametableAddr);
    } break;
    case 2: /* fetch tile attribute from the nametable byte above */
    {
        /* addr: 0010 NN 1111 yyy xxx
         *       ^^^^ ------------------ NAMETABLE_OFFSET (0x2000)
         *            ^^ --------------- nametable y, x
         *               ^^^^ ---------- Attribute table offset (0x03C0)
         *                    ^^^ ------ coarse y
         *                        ^^^ -- coarse x
         * */
        u16 CoarseX = GET_COARSE_X(This->Loopy.v);
        u16 CoarseY = GET_COARSE_Y(This->Loopy.v);
        u16 AttributeTableBase = NAMETABLE_OFFSET + 0x03C0;
        u16 NametableSelect = This->Loopy.v & 0x0C00;

        u16 Address = 
            AttributeTableBase 
            | NametableSelect 
            | ((CoarseY >> 2) << 3) 
            | (CoarseX >> 2);
        u8 AttrByte = NESPPU_ReadInternalMemory(This, Address);

        if (CoarseX & (1 << 1))
            AttrByte >>= 2;
        if (CoarseY & (1 << 1))
            AttrByte >>= 4;
        This->BgAttrBitsLatch = AttrByte & 0x3;
    } break;
    case 4: /* fetch pattern low bit */
    {
        u16 BaseAddress = This->Ctrl & PPUCTRL_BG_PATTERN_ADDR? 0x1000 : 0x0000;
        u16 Index       = (u16)This->BgNametableByteLatch * TILE_SIZE;
        u16 TileOffset  = GET_FINE_Y(This->Loopy.v);

        This->BgPatternLoLatch = NESPPU_ReadInternalMemory(This, 
            BaseAddress | Index | TileOffset
        );
    } break;
    case 6: /* fetch pattern high bit */
    {
        u16 BaseAddress = This->Ctrl & PPUCTRL_BG_PATTERN_ADDR? 0x1000 : 0x0000;
        u16 Index       = (u16)This->BgNametableByteLatch * TILE_SIZE;
        u16 TileOffset  = GET_FINE_Y(This->Loopy.v) + 8;

        This->BgPatternHiLatch = NESPPU_ReadInternalMemory(This, 
            BaseAddress | Index | TileOffset
        );
    } break;
    case 7: /* end of tile: update x scroll */
    {
        if (This->Mask & (PPUMASK_SHOW_BG | PPUMASK_SHOW_SPR))
        {
            /* increment coarse-y every 8 pixels */
            u16 NewCoarseX = GET_COARSE_X(This->Loopy.v);
            NewCoarseX += 1;

            /* if CoarseX carries, 
             * we switch nametable-x bit to select a different horizontal nametable, 
             * the specific nametable will be determined by the mirroring mode */
            This->Loopy.v ^= (NewCoarseX == 32) << 10;

            /* reloads CoarseX */
            MASKED_LOAD(This->Loopy.v, NewCoarseX, COARSE_X_MASK);
        }
    } break;
    }
}


static void NESPPU_LoadSpriteData(NESPPU *This)
{
    for (uint i = 0; i < This->VisibleSpriteCount; i++)
    {
        NESPPU_ObjectAttribute *CurrentSprite = &This->VisibleSprites[i];
        int CurrentHeight = This->Scanline - CurrentSprite->y;

        /* get low sprite addresses */
        u16 SprPatternAddrLo, SprPatternAddrHi;
        /* 8x16 */
        if (This->Ctrl & PPUCTRL_SPR_SIZE16)
        {
            u16 BaseAddr = (u16)(CurrentSprite->ID & 0x1) << 12; /* first bit determines the pattern table location */
            uint IsLowerTile = CurrentHeight >= 8;
            u16 SpriteIndex = (u16)(CurrentSprite->ID & 0xFE) + IsLowerTile;
            if (CurrentHeight > 8) 
                CurrentHeight -= 8;
            u16 PatternSelect = CurrentSprite->Attribute & PPU_OA_FLIP_VERTICAL
                ? (7 - CurrentHeight)
                : CurrentHeight;

            SprPatternAddrLo = 
                BaseAddr 
                | (SpriteIndex * TILE_SIZE)
                | (PatternSelect & 0x7);
        }
        /* 8x8 */
        else
        {
            u16 BaseAddr = This->Ctrl & PPUCTRL_SPR_PATTERN_ADDR 
                ? 0x1000 : 0x0000;
            u16 SpriteIndex = CurrentSprite->ID;
            u16 PatternSelect = CurrentSprite->Attribute & PPU_OA_FLIP_VERTICAL 
                ? (7 - CurrentHeight)
                : CurrentHeight;

            SprPatternAddrLo = 
                BaseAddr 
                | (SpriteIndex * TILE_SIZE) 
                | (PatternSelect & 0x7);
            /* NOTE: scanline - spr.y is never greater than 7 */
        }

        /* hi and lo tiles are generally 8 bytes apart in the NES */
        SprPatternAddrHi = SprPatternAddrLo + 8;
        u8 SprPatternBitsLo = NESPPU_ReadInternalMemory(This, SprPatternAddrLo);
        u8 SprPatternBitsHi = NESPPU_ReadInternalMemory(This, SprPatternAddrHi);

        /* flips a sprite horizontally */
        /* reverse the bit pattern */
        if (CurrentSprite->Attribute & PPU_OA_FLIP_HORIZONTAL) 
        {
            SprPatternBitsLo = FlipByte(SprPatternBitsLo);
            SprPatternBitsHi = FlipByte(SprPatternBitsHi);
        }

        /* load them into the shift registers */
        This->SprPatternLoShifter[i] = SprPatternBitsLo;
        This->SprPatternHiShifter[i] = SprPatternBitsHi;
    }
}



Bool8 NESPPU_StepClock(NESPPU *This)
{
    /* 
     * WARNING: the following code was known to cause cancer, stroke, prostate cancer and even death. 
     *          Continue with descretion. YOU HAVE BEEN WARNED.
     * In all seriousness, consult https://www.nesdev.org/w/images/default/4/4f/Ppu.svg before reading this 
     * */


    uint ShouldRenderBackground = This->Mask & PPUMASK_SHOW_BG;
    if (!(This->Mask & PPUMASK_SHOW_BG_LEFT) && This->Clk < 8)
        ShouldRenderBackground = false;
    uint ShouldRenderForeground = This->Mask & PPUMASK_SHOW_SPR;
    if (!(This->Mask & PPUMASK_SHOW_SPR_LEFT) && This->Clk < 8)
        ShouldRenderBackground = false;
    uint ShouldRender = ShouldRenderBackground || ShouldRenderForeground;

    /* putting these magic numbers in variable name makes this harder to read */
    /* visible scanlines (-1 is pre-render) */
    if (IN_RANGE(-1, This->Scanline, 239))
    {
        /* prerender scanline */
        if (This->Scanline == -1)
        {
            if (This->Clk == 1)
            {
                This->Status &= ~(
                    PPUSTATUS_VBLANK 
                    | PPUSTATUS_SPR0_HIT 
                    | PPUSTATUS_SPR_OVERFLOW
                );

                /* clear sprite shifter to prevent garbage sprite data from being rendered */
                Memset(This->SprPatternHiShifter, 0, sizeof This->SprPatternHiShifter);
                Memset(This->SprPatternLoShifter, 0, sizeof This->SprPatternLoShifter);
            }
            /* constantly reload y components */
            else if (ShouldRender && IN_RANGE(280, This->Clk, 304))
            {
                u16 YComponentMask = COARSE_Y_MASK | FINE_Y_MASK | (1 << 11); /* nametable y */
                MASKED_LOAD(This->Loopy.v, This->Loopy.t, YComponentMask);
            }
        }

        /* ================= background ================= */
        if (This->Clk == 0)
        {
            /* do nothing */
        }
        else if (IN_RANGE(2, This->Clk, 256) || IN_RANGE(321, This->Clk, 337)) /* fetching data cycles */
        {
            NESPPU_FetchBackgroundData(This);

            /* end of visible frame, update y components */
            if (ShouldRender && This->Clk == 256)
            {
                if (GET_FINE_Y(This->Loopy.v) != 7)              /* no wrapping */
                {
                    This->Loopy.v += 0x1000;            /* increment fine y */
                }
                else                                    /* fine y overflows into coarse y */
                {
                    This->Loopy.v &= ~FINE_Y_MASK;      /* clear fine y */
                    u16 CoarseY = GET_COARSE_Y(This->Loopy.v);
                    if (CoarseY == 29)                  /* wrap and switch nametable */
                    {
                        CoarseY = 0;
                        This->Loopy.v ^= 0x0800;        /* flip nametable y */
                    }
                    else if (CoarseY == 31) /* wrap */
                        CoarseY = 0;
                    else CoarseY++;

                    MASKED_LOAD(This->Loopy.v, CoarseY << 5, COARSE_Y_MASK);
                }
            }
            else if (This->Clk == 337)
            {
                u16 NametableAddr = NAMETABLE_OFFSET + (This->Loopy.v & 0x0FFF);
                NESPPU_ReadInternalMemory(This, NametableAddr);
            }
        }
        else /* horizontal blank: 257..320, 337..340 */
        {
            /* horizontal blank entry: update x component and shifters */
            if (ShouldRender && This->Clk == 257)
            {
                NESPPU_ReloadBackgroundShifters(This);
                u16 NametableXAndCoarseXMask = COARSE_X_MASK | (1 << 10); /* nametable x */
                MASKED_LOAD(This->Loopy.v, This->Loopy.t, NametableXAndCoarseXMask);
            }
            /* horizontal blank ends: dummy fetches */
            else if (This->Clk == 339)
            {
                u16 NametableAddr = NAMETABLE_OFFSET + (This->Loopy.v & 0x0FFF);
                NESPPU_ReadInternalMemory(This, NametableAddr);
            }
        }


        /* ================= foreground ================= */
        if (ShouldRender)
        {
            /* SPRITE EVALUATION: evaluate sprite at the end of each visible frame */
            if (This->Scanline >= 0 && This->Clk == 257)
            {
                /* clear sprites to 0xFF, y pos of FF means the sprites are never visible */
                Memset(This->VisibleSprites, 0xFF, sizeof This->VisibleSprites);
                This->VisibleSpriteCount = 0;

                This->Spr0IsVisible = false;
                uint i = 0;
                while (i < STATIC_ARRAY_SIZE(This->OAM.Entries) && This->VisibleSpriteCount < 9)
                {
                    int ScanlineDiff = This->Scanline - This->OAM.Entries[i].y;
                    int SpriteHeight = This->Ctrl & PPUCTRL_SPR_SIZE16? 16 : 8;
                    if (IN_RANGE(0, ScanlineDiff, SpriteHeight - 1))
                    {
                        if (This->VisibleSpriteCount < 8)
                        {
                            if (i == 0) /* sprite 0 */
                                This->Spr0IsVisible = true;
                            This->VisibleSprites[This->VisibleSpriteCount] = This->OAM.Entries[i];
                        }
                        This->VisibleSpriteCount++;
                    }
                    i++;
                }
                if (This->VisibleSpriteCount > 8)
                {
                    This->Status |= PPUSTATUS_SPR_OVERFLOW;
                    This->VisibleSpriteCount = 8;
                }
            }
            /* find data from pattern memory to draw the sprites needed */
            else if (This->Clk == 340)
            {
                NESPPU_LoadSpriteData(This);
            }
        }
    }
    /* vblank starts */
    else if (This->Scanline == 241 && This->Clk == 1)
    {
        if (!This->ShouldNotSetVBlankThisFrame)
            This->Status |= PPUSTATUS_VBLANK;
        else This->ShouldNotSetVBlankThisFrame = false;

        if (This->SupressNMIThisFrame)
        {
            This->SupressNMIThisFrame = false;
        }
        else
        {
            if (This->Ctrl & PPUCTRL_NMI_ENABLE)
            {
                This->NmiCallback(This);
            }
        }
        
    }



    NESPPU_RenderSinglePixel(This);



    Bool8 FrameCompleted = false;
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
            This->CurrentFrameIsOdd = !This->CurrentFrameIsOdd;
        }
    }
    return FrameCompleted;
}

#endif /* NES_C */
