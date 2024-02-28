
#include "Common.h"
#include "Cartridge.h"
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
#define PPUCTRL_SPR_SIZE        (1 << 5)  /* 0: 8x8  ; 1: 8x16  */
#define PPUCTRL_SLAVE_MODE      (1 << 6)  /* unused */
#define PPUCTRL_NMI_ENABLE      (1 << 7)
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

    u8 NameTableByteLatch;
    u8 AttrBitsLatch;
    u8 PatternLoLatch;
    u8 PatternHiLatch;

    u16 PatternHiShifter;
    u16 PatternLoShifter;
    u16 AttrHiShifter;
    u16 AttrLoShifter;


    u8 PaletteColorIndex[0x20];
    u8 NameAndAttributeTable[0x800];
    u8 ObjectAttributeMemory[256];

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
        if (IN_RANGE(0x0000, LogicalAddress, 0x03FF) || IN_RANGE(0x0800, LogicalAddress, 0x0BFF))
            PhysicalAddress = LogicalAddress & 0x03FF;
        else if (IN_RANGE(0x0400, LogicalAddress, 0x07FF) || IN_RANGE(0x0C00, LogicalAddress, 0x0FFF))
            PhysicalAddress = (LogicalAddress & 0x03FF) + 0x0400;
    } break;
    case NAMETABLE_HORIZONTAL:
    {
        if (IN_RANGE(0x0000, LogicalAddress, 0x07FF))
            PhysicalAddress = LogicalAddress & 0x03FF;
        else if (IN_RANGE(0x0800, LogicalAddress, 0x0FFF))
            PhysicalAddress = (LogicalAddress & 0x03FF) + 0x0400;
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
        Address = NESPPU_MirrorNameTableAddr(This->CartridgeHandle, Address & 0x0FFF);
        return This->NameAndAttributeTable[Address];
    }
    else /* 0x3F00-0x3FFF: sprite palette, image palette */
    {
        Address &= 0x1F;
        if (Address == 0x0010) 
            Address = 0x0000;
        if (Address == 0x0014) 
            Address = 0x0004;
        if (Address == 0x0018)
            Address = 0x0008;
        if (Address == 0x001C)
            Address = 0x000C;
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
        Address = NESPPU_MirrorNameTableAddr(This->CartridgeHandle, Address & 0xFFF);
        This->NameAndAttributeTable[Address] = Byte;
    }
    else /* 0x3F00-0x3FFF: sprite palette, image palette */
    {
        Address &= 0x1F;
        if (Address == 0x0010) 
            Address = 0x0000;
        if (Address == 0x0014) 
            Address = 0x0004;
        if (Address == 0x0018)
            Address = 0x0008;
        if (Address == 0x001C)
            Address = 0x000C;
        This->PaletteColorIndex[Address] = Byte;
    }
}




void NESPPU_UpdateBackgroundShifters(NESPPU *This)
{
    if (This->Mask & PPUMASK_SHOW_BG)
    {
        This->PatternHiShifter <<= 1;
        This->PatternLoShifter <<= 1;
        This->AttrHiShifter <<= 1;
        This->AttrLoShifter <<= 1;
    }
}

void NESPPU_ReloadBackgroundShifters(NESPPU *This)
{
    if (This->Mask & PPUMASK_SHOW_BG)
    {
        MASKED_LOAD(This->PatternHiShifter, This->PatternHiLatch, 0xFF);
        MASKED_LOAD(This->PatternLoShifter, This->PatternLoLatch, 0xFF);

        u16 InflatedLoAttrBit = This->AttrBitsLatch & 0x01? 
            0xFF : 0x00;
        u16 InflatedHiAttrBit = This->AttrBitsLatch & 0x02?
            0xFF : 0x00;

        MASKED_LOAD(This->AttrLoShifter, InflatedLoAttrBit, 0xFF);
        MASKED_LOAD(This->AttrHiShifter, InflatedHiAttrBit, 0xFF);
    }
}

Bool8 NESPPU_StepClock(NESPPU *This)
{
#define COARSE_X_MASK 0x001F
#define COARSE_Y_MASK 0x03E0
#define NAMETABLE_OFFSET 0x2000
    /* 
     * WARNING: the following was known to cause cancer, stroke, prostate cancer and even death. 
     *          Continue with descretion. YOU HAVE BEEN WARNED.
     * In all seriousness, consult https://www.nesdev.org/w/images/default/4/4f/Ppu.svg before reading this 
     * */

    /* putting these magic numbers in variable name makes this harder to read */

    uint InRenderingMode = This->Mask & (PPUMASK_SHOW_BG | PPUMASK_SHOW_SPR);
    Bool8 PixelXIsVisible = IN_RANGE(1, This->Clk, 256);
    Bool8 PixelYIsVisible = IN_RANGE(0, This->Scanline, 239);
    if (IN_RANGE(-1, This->Scanline, 239))
    {
        /* fetching for rendering       [1, 256] 
         * prefetching for next frame   [321, 336] */
        if (PixelXIsVisible || IN_RANGE(321, This->Clk, 336))
        {
            NESPPU_UpdateBackgroundShifters(This);

            /* basically a state machine */
            /* subtract 1 bc clk == 0 was an idle clk */
            switch ((This->Clk - 1) % 8)
            {
            /* fetch NameTable byte */
            case 0:
            {
                if (This->Clk >= 8) 
                {
                    NESPPU_ReloadBackgroundShifters(This);
                }

                /* 
                 * To fetch a nametable byte, use a 12 bit addr containing: 
                 * msb          lsb
                 * NN yyyyy xxxxx
                 * ^^--------------- nametable y, x respectively
                 *    ^^^^^--------- CoarseY
                 *          ^^^^^--- CoarseX
                 * is the index into a nametable byte 
                 */
                u16 NameTableAddr = 
                    NAMETABLE_OFFSET 
                    + (This->Loopy.v & 0x0FFF);
                This->NameTableByteLatch = NESPPU_ReadInternalMemory(This, NameTableAddr);
            } break;
            /* fetch Attribute Table byte */
            case 2:
            {
                u16 AttrTableBase = 0x03C0;
                u16 NameTableSelects = This->Loopy.v & 0x0C00;             /* nametable select bits (11 and 10) */
                u16 CoarseX = (This->Loopy.v & COARSE_X_MASK) >> (0 + 2);  /* upper 3 bits of coarse-x */
                u16 CoarseY = (This->Loopy.v & COARSE_Y_MASK) >> (2 + 2);  /* upper 3 bits of coarse-y */
                /* AttrByteAddr should be:
                 * msb           lsb
                 * NN 1111 yyy xxx
                 * ^^------------------ nametable y, x respectively
                 *    ^^^^------------- 0x03C0 AttrTable offset
                 *         ^^^--------- (CoarseY >> 2)
                 *             ^^^----- (CoarseX >> 2)
                 */
                u16 AttrTableByteAddr = 
                    (AttrTableBase + NAMETABLE_OFFSET)
                    | NameTableSelects
                    | CoarseX 
                    | CoarseY;
                u8 AttrByte = NESPPU_ReadInternalMemory(This, AttrTableByteAddr);

                /* bit select: 
                 * bit 1 of CoarseX determines 0 or 2
                 * bit 1 of CoarseY determines 0 or 4
                 * the sum of those determines the bit offset (0, 2, 4, 6)
                 * */
                if (This->Loopy.v & 0x02)
                    AttrByte >>= 2;
                if (This->Loopy.v & (1 << 6))
                    AttrByte >>= 4;
                This->AttrBitsLatch = AttrByte & 0x03;
            } break;
            /* read pattern table low
             * using the byte from the NameTable a few cycles earlier */
            case 4:
            {
                u16 PatternLoAddr = This->Ctrl & PPUCTRL_BG_PATTERN_ADDR?
                    0x1000 : 0x0000;
                uint PatternTableSize = 8*2;    /* 16 bytes per pattern table */
                PatternLoAddr += (u16)This->NameTableByteLatch * PatternTableSize;
                PatternLoAddr += (This->Loopy.v >> 12) & 0x7;        /* fine y, fetching lsb pattern table */
                /* NOTE: fine y's width is 3 bits, but we got 4 bits of space before adding fine y, 
                 * the 3rd bit specifies whether we're reading the lsb or msb pattern table */

                This->PatternLoLatch = NESPPU_ReadInternalMemory(
                    This, 
                    PatternLoAddr
                );
            } break;
            /* read pattern table high (+8 bytes from low) */
            case 6:
            {
                u16 PatternHiAddr = This->Ctrl & PPUCTRL_BG_PATTERN_ADDR?
                    0x1000 : 0x0000;
                uint PatternTableSize = 16; /* 8 bytes per pattern table (8x8 pixels) */
                PatternHiAddr += (u16)This->NameTableByteLatch * PatternTableSize;    /* lsb and msb */
                PatternHiAddr += (This->Loopy.v >> 12) + 8;     /* fine y, +8 to get the msb pattern table */

                This->PatternHiLatch = NESPPU_ReadInternalMemory(
                    This, 
                    PatternHiAddr
                );
            } break;
            /* prepare v to fetch next NameTable and AttrTable */
            case 7:
            {
                if (!InRenderingMode)
                    break;

                /* increment coarse-y every 8 pixels */
                u16 NewCoarseX = (This->Loopy.v & COARSE_X_MASK);
                NewCoarseX += 1;

                /* if CoarseX carries, 
                 * we switch nametable-x bit to select a different horizontal nametable, 
                 * the specific nametable will be determined by the mirroring mode */
                This->Loopy.v ^= (NewCoarseX == 32) << 10;

                /* reloads CoarseX */
                MASKED_LOAD(This->Loopy.v, NewCoarseX, COARSE_X_MASK);

                /* we have finished rendering a scanline */
                if (This->Clk == 256) 
                {
                    /* increment fine y, it resides in the topmost nybble */
                    u16 NewFineY = (This->Loopy.v & 0x7000) + 0x1000;
                    if (NewFineY > 0x7000) /* FineY wrap? */
                    {
                        /* then update CoarseY */
                        u16 CoarseY = This->Loopy.v & COARSE_Y_MASK;

                        /* we've reached the last row of a nametable (a nametable is 32x30) */
                        if (CoarseY == (29 << 5))
                        {
                            /* then we should read the next nametable in the vertical direction */
                            CoarseY = 0;                /* make coarse y wrap */
                            This->Loopy.v ^= 1 << 11;   /* invert nametable-y bit */
                        }
                        else /* increment CoarseY normally */
                        {
                            CoarseY += 1 << 5;
                            /* NOTE: when CoarseY is 31, the increment will carry, 
                             * but we don't care about it */
                        }

                        /* reloads CoarseY */
                        MASKED_LOAD(This->Loopy.v, CoarseY, COARSE_Y_MASK);
                    }
                    /* remeber to reload fine y */
                    MASKED_LOAD(This->Loopy.v, NewFineY, 0x7000);
                }
            } break;
            }
        }
        /* right after we finished rendering a line
         * load nametable-x and coarse-x from t into v */
        else if (This->Clk == 257 && InRenderingMode)
        {
            u16 NameTableXAndCoarseXMask = COARSE_X_MASK | (1 << 10); /* nametable x */
            MASKED_LOAD(This->Loopy.v, This->Loopy.t, NameTableXAndCoarseXMask);
        }
        /* dummy last fetches, but mappers like MMC5 are sensitive to it */
        else if (This->Clk == 337 || This->Clk == 339)
        {
            u16 NameTableAddr = NAMETABLE_OFFSET + (This->Loopy.v & 0x0FFF);
            NESPPU_ReadInternalMemory(This, NameTableAddr);
        }

        /* prerender scanline */
        if (This->Scanline == -1)
        {
            /* vblank ends */
            if (This->Clk == 1)
            {
                This->Status &= ~(
                    PPUSTATUS_VBLANK
                    | PPUSTATUS_SPR0_HIT 
                    | PPUSTATUS_SPR_OVERFLOW
                );
            }
            /* constantly reload y-components */
            else if (IN_RANGE(280, This->Clk, 304) && InRenderingMode)
            {
                u16 NameTableYAndCoarseYMask = COARSE_Y_MASK | (1 << 11) | 0xF000; /* nametable y */
                MASKED_LOAD(This->Loopy.v, This->Loopy.t, NameTableYAndCoarseYMask);
            }
        }
    }
    /* vblank begins */
    else if (This->Scanline == 241 && This->Clk == 1)
    {
        This->Status |= PPUSTATUS_VBLANK;
        if (This->Ctrl & PPUCTRL_NMI_ENABLE)
        {
            This->NmiCallback(This);
        }
    }




    /* then render the bloody pixel */
    if (PixelXIsVisible 
    && PixelYIsVisible)
    {
        u16 FineXSelect = 0x8000 >> This->Loopy.x;
        u8 Bit0 = (This->PatternLoShifter & FineXSelect) != 0;
        u8 Bit1 = (This->PatternHiShifter & FineXSelect) != 0;
        u8 Bit2 = (This->AttrLoShifter & FineXSelect) != 0;
        u8 Bit3 = (This->AttrHiShifter & FineXSelect) != 0;

        u8 PaletteIndex = Bit0 | (Bit1 << 1) | (Bit2 << 2) | (Bit3 << 3);
        u32 Color = sPPURGBPalette[NESPPU_ReadInternalMemory(This, 0x3F00 + PaletteIndex)];
        This->ScreenOutput[This->ScreenIndex++] = Color;

        if (This->ScreenIndex >= NES_SCREEN_BUFFER_SIZE)
            This->ScreenIndex = 0;
    }



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
        }
    }
    return FrameCompleted;
#undef NAMETABLE_OFFSET
#undef COARSE_X_MASK
#undef COARSE_Y_MASK
}


