
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


#define COARSE_X_MASK 0x001F
#define COARSE_Y_MASK 0x03E0
#define NAMETABLE_OFFSET 0x2000
#define FINE_Y_MASK 0x7000


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

    u8 PaletteColorIndex[0x20];
    u8 NameAndAttributeTable[0x800];
    union {
        u8 Bytes[256];
        NESPPU_ObjectAttribute Entries[64];
    } OAM;


    NESPPU_ObjectAttribute VisibleSprites[8];
    u8 SprPatternLoShifter[8];
    u8 SprPatternHiShifter[8];
    u8 VisibleSpriteCount;

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
    This->Ctrl = 0;
    This->Mask = 0;
    This->Loopy.w = 0;
    This->CurrentFrameIsOdd = false;
    This->VisibleSpriteCount = 0;
    Memset(This->VisibleSprites, 0xFF, sizeof This->VisibleSprites);
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
        PhysicalAddress = LogicalAddress & 0x07FF;
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
        u16 NametableSelect = (LogicalAddress & 0x0800) >> 1; /* bit 12 determines the nametable */
        PhysicalAddress = (LogicalAddress & 0x03FF) + NametableSelect;
    } break;
    case NAMETABLE_ONESCREEN_HI:
    case NAMETABLE_ONESCREEN_LO:
    {
        DEBUG_ASSERT(false && "TODO: one screen mirroring");
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
        return This->NameAndAttributeTable[Address];
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
        This->NameAndAttributeTable[Address] = Byte;
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




u8 NESPPU_ExternalRead(NESPPU *This, u16 Address)
{
    switch ((NESPPU_CtrlReg)Address)
    {
    case PPU_CTRL: /* read not allowed */ break;
    case PPU_MASK: /* read not allowed */ break;
    case PPU_STATUS: 
    {
        u8 Status = This->Status; 
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
        /* read is buffered */
        if (This->Loopy.v < 0x3F00)
        {
            ReadValue = This->ReadBuffer;
            This->ReadBuffer = NESPPU_ReadInternalMemory(This, Address);
        }
        else /* no buffered read for this addresses below 0x3F00 */
        {
            ReadValue = NESPPU_ReadInternalMemory(This, This->Loopy.v);
        }
        This->Loopy.v += (This->Ctrl & PPUCTRL_INC32)? 32 : 1;
        return ReadValue;
    } break;
    }
    return 0;
}

void NESPPU_ExternalWrite(NESPPU *This, u16 Address, u8 Byte)
{
    switch ((NESPPU_CtrlReg)Address)
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
        This->OAM.Bytes[This->OAMAddr] = Byte;
    } break;
    case PPU_SCROLL: 
    {
        if (This->Loopy.w++ == 0) /* first write */
        {
            This->Loopy.x = Byte;
            MASKED_LOAD(This->Loopy.t, Byte >> 3, 0x1F);
        }
        else /* second write */
        {
            u16 FineY = (u16)Byte << 12;
            u16 CoarseY = (Byte >> 3) << 5;
            MASKED_LOAD(This->Loopy.t, FineY, FINE_Y_MASK);
            MASKED_LOAD(This->Loopy.t, CoarseY, COARSE_Y_MASK);
        }
    } break;
    case PPU_ADDR:
    {
        /* latching because each cpu cycle is 3 ppu cycles */
        if (This->Loopy.w++ == 0) /* first write */
        {
            Byte &= 0x3F;
            u16 AddrHi = (u16)Byte << 8;
            MASKED_LOAD(This->Loopy.t, AddrHi, 0xFF00);
        }
        else /* second write */
        {
            MASKED_LOAD(This->Loopy.t, Byte, 0x00FF);
            This->Loopy.v = This->Loopy.t;
        }
    } break;
    case PPU_DATA:
    {
        NESPPU_WriteInternalMemory(This, This->Loopy.v, Byte);
        This->Loopy.v += (This->Ctrl & PPUCTRL_INC32)? 32 : 1;
    } break;
    }
}




void NESPPU_UpdateBackgroundShifters(NESPPU *This)
{
    if (This->Mask & PPUMASK_SHOW_BG)
    {
        This->BgPatternHiShifter <<= 1;
        This->BgPatternLoShifter <<= 1;
        This->BgAttrHiShifter <<= 1;
        This->BgAttrLoShifter <<= 1;
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

Bool8 NESPPU_StepClock(NESPPU *This)
{
    /* 
     * WARNING: the following code was known to cause cancer, stroke, prostate cancer and even death. 
     *          Continue with descretion. YOU HAVE BEEN WARNED.
     * In all seriousness, consult https://www.nesdev.org/w/images/default/4/4f/Ppu.svg before reading this 
     * */


    uint ShouldRenderBackground = This->Mask & PPUMASK_SHOW_BG;
    uint ShouldRenderForeground = This->Mask & PPUMASK_SHOW_SPR;
    uint ShouldRender = ShouldRenderBackground || ShouldRenderForeground;

    /* putting these magic numbers in variable name makes this harder to read */
    /* visible scanlines (-1 is pre-render) */
    if (IN_RANGE(-1, This->Scanline, 239))
    {
        if (This->Clk == 0 
        && This->Scanline == 0 
        && This->CurrentFrameIsOdd
        && ShouldRender) /* skip cycle on odd frames */
        {
            This->Clk = 1;
        }

        /* extract tile info: nametable byte -> tile pattern hi/lo, and tile attribute */
        if (IN_RANGE(1, This->Clk, 256) || IN_RANGE(321, This->Clk, 336))
        {
            /* Javidx9 */
            NESPPU_UpdateBackgroundShifters(This);

            /* subtract 1 bc clk = 0 was an idle clk */
            switch ((This->Clk - 1) % 8)
            {
            /* fetch nametable byte */
            case 0:
            {
                /* Javidx9 */
                NESPPU_ReloadBackgroundShifters(This);

                /* 
                 * To fetch a nametable byte, use a 12 bit addr containing: 
                 * msb          lsb
                 * NN yyyyy xxxxx
                 * ^^--------------- nametable y, x respectively
                 *    ^^^^^--------- CoarseY
                 *          ^^^^^--- CoarseX
                 * is the index into a nametable byte 
                 */
                u16 NameTableAddr = NAMETABLE_OFFSET + (This->Loopy.v & 0x0FFF);
                This->BgNametableByteLatch = NESPPU_ReadInternalMemory(This, NameTableAddr);
            } break;
            /* fetch tile attribute */
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
                This->BgAttrBitsLatch = AttrByte & 0x03;
            } break;
            /* fetch low pattern table */
            case 4:
            {
                u16 PatternLoAddr = This->Ctrl & PPUCTRL_BG_PATTERN_ADDR?
                    0x1000 : 0x0000;
                uint PatternTableSize = 16;                                  /* 16 bytes per pattern table */
                PatternLoAddr += 
                    (u16)This->BgNametableByteLatch * PatternTableSize;
                PatternLoAddr += ((This->Loopy.v & FINE_Y_MASK) >> 12);      /* fine y, fetching lsb pattern table */
                /* NOTE: fine y's width is 3 bits, but we got 4 bits of space before adding fine y, 
                 * the 3rd bit specifies whether we're reading the lsb or msb pattern table */

                This->BgPatternLoLatch = NESPPU_ReadInternalMemory(
                    This, PatternLoAddr
                );
            } break;
            /* fetch high pattern table */
            case 6:
            {
                u16 PatternHiAddr = This->Ctrl & PPUCTRL_BG_PATTERN_ADDR?
                    0x1000 : 0x0000;
                uint PatternTableSize = 16;                                     /* 8 bytes per pattern table (8x8 pixels) */
                PatternHiAddr += 
                    (u16)This->BgNametableByteLatch * PatternTableSize;
                PatternHiAddr += ((This->Loopy.v & FINE_Y_MASK) >> 12) + 8;     /* fine y, +8 to get the msb pattern table */

                This->BgPatternHiLatch = NESPPU_ReadInternalMemory(
                    This, PatternHiAddr
                );
            } break;
            /* increment X scroll */
            case 7:
            {
                if (!ShouldRender)
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
                    u16 NewFineY = (This->Loopy.v & FINE_Y_MASK) + 0x1000;
                    if (NewFineY > FINE_Y_MASK) /* FineY wrap? */
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
                    MASKED_LOAD(This->Loopy.v, NewFineY, FINE_Y_MASK);
                }
            } break;
            }
        }
        /* right after we finished rendering a line, load x components from t to v */
        else if (This->Clk == 257 && ShouldRender)
        {
            /* also loads background shifters */
            NESPPU_ReloadBackgroundShifters(This);

            u16 NametableXAndCoarseXMask = COARSE_X_MASK | (1 << 10); /* nametable x */
            MASKED_LOAD(This->Loopy.v, This->Loopy.t, NametableXAndCoarseXMask);
        }
        /* dummy last fetches, but mappers like MMC5 are sensitive to it */
        else if (This->Clk == 337 || This->Clk == 339)
        {
            u16 NameTableAddr = NAMETABLE_OFFSET + (This->Loopy.v & 0x0FFF);
            NESPPU_ReadInternalMemory(This, NameTableAddr);
        }

        if (This->Scanline == -1)
        {
            /* vblank ends */
            if (This->Clk == 1)
            {
                /* clear flags */
                This->Status &= ~(
                    PPUSTATUS_VBLANK 
                    | PPUSTATUS_SPR0_HIT 
                    | PPUSTATUS_SPR_OVERFLOW
                );
            }
            /* ppu behavior: constantly reload y-components from t to v */
            else if (IN_RANGE(280, This->Clk, 304) && ShouldRender)
            {
                u16 NameTableYAndCoarseYMask = COARSE_Y_MASK | FINE_Y_MASK | (1 << 11); /* nametable y */
                MASKED_LOAD(This->Loopy.v, This->Loopy.t, NameTableYAndCoarseYMask);
            }
        }


        /* ================== sprite eval/foreground rendering ================== */

        /* evaluate sprite at the end of each visible frame's scanline,
         * TODO: make this cycle accurate */
        if (This->Scanline >= 0 && This->Clk == 257)
        {
            Memset(This->VisibleSprites, 0xFF, sizeof This->VisibleSprites);
            This->VisibleSpriteCount = 0;

            /* evaluate all sprites in the OAM */
            for (uint i = 0; i < STATIC_ARRAY_SIZE(This->OAM.Entries); i++)
            {
                int ScanlineDiff = This->Scanline - This->OAM.Entries[i].y;
                int SpriteHeight = This->Ctrl & PPUCTRL_SPR_SIZE16?
                    16: 8;

                Bool8 SpriteIsVisible = IN_RANGE(0, ScanlineDiff, SpriteHeight - 1);
                if (SpriteIsVisible)
                {
                    /* too many sprites on screen, set overflow flag and exit loop */
                    if (This->VisibleSpriteCount >= 8)
                    {
                        This->Status |= PPUSTATUS_SPR_OVERFLOW;
                        break;
                    }

                    This->VisibleSprites[This->VisibleSpriteCount] = This->OAM.Entries[i];
                    This->VisibleSpriteCount++;
                }
            }
        }

        /* find data from pattern memory to draw the sprites needed */
        /* TODO: make this cycle accurate */
        if (This->Clk == 340)
        {
            for (uint i = 0; i < This->VisibleSpriteCount; i++)
            {
                NESPPU_ObjectAttribute *CurrentSprite = &This->VisibleSprites[i];
                u8 SprPatternBitsLo, SprPatternBitsHi;
                u16 SprPatternAddrLo, SprPatternAddrHi;

                /* 8x16 */
                if (This->Status & PPUCTRL_SPR_SIZE16)
                {
                    /* sprite flipped vertically */
                    if (CurrentSprite->Attribute & 0x80)
                    {
                    }
                    else 
                    {
                    }
                }
                /* 8x8 */
                else
                {
                    /* sprite is flipped vertically */
                    if (CurrentSprite->Attribute & 0x80)
                    {
                        SprPatternAddrLo = This->Ctrl & PPUCTRL_SPR_PATTERN_ADDR?
                            0x1000: 0x0000;
                        SprPatternAddrLo |= (u16)CurrentSprite->ID << 4;
                        SprPatternAddrLo |= 7 - (This->Scanline - CurrentSprite->y);
                    }
                    /* not flipped, read normally */
                    else
                    {
                        SprPatternAddrLo = This->Ctrl & PPUCTRL_SPR_PATTERN_ADDR?
                            0x1000: 0x0000;
                        SprPatternAddrLo |= (u16)CurrentSprite->ID << 4;
                        SprPatternAddrLo |= (This->Scanline - CurrentSprite->y);
                    }
                    /* NOTE: scanline - spr.y is never greater than 7 */
                }
            }
        }
    }
    /* vblank starts */
    else if (This->Scanline == 241 && This->Clk == 1)
    {
        This->Status |= PPUSTATUS_VBLANK;
        if (This->Ctrl & PPUCTRL_NMI_ENABLE)
        {
            This->NmiCallback(This);
        }
    }




    /* render pixels */
    if (IN_RANGE(1, This->Clk, 256) && IN_RANGE(0, This->Scanline, 239))
    {
        /* fine x is the offset from the highest bit in the shift register */
        u16 FineXSelect = 0x8000 >> This->Loopy.x;
        u8 Bit0 = (This->BgPatternLoShifter & FineXSelect) != 0;
        u8 Bit1 = (This->BgPatternHiShifter & FineXSelect) != 0;
        u8 Bit2 = (This->BgAttrLoShifter & FineXSelect) != 0;
        u8 Bit3 = (This->BgAttrHiShifter & FineXSelect) != 0;

        u8 PaletteIndex = Bit0 | (Bit1 << 1) | (Bit2 << 2) | (Bit3 << 3);
        u16 PaletteBase = 0x3F00;
        u32 Color = sPPURGBPalette[
            NESPPU_ReadInternalMemory(This, PaletteBase + PaletteIndex)
            % STATIC_ARRAY_SIZE(sPPURGBPalette)
        ];

        This->ScreenOutput[This->ScreenIndex++] = Color;
        if (This->ScreenIndex >= NES_SCREEN_BUFFER_SIZE)
        {
            This->ScreenIndex = 0;
        }
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
            This->CurrentFrameIsOdd = !This->CurrentFrameIsOdd;
        }
    }
    return FrameCompleted;
#undef NAMETABLE_OFFSET
#undef COARSE_X_MASK
#undef COARSE_Y_MASK
#undef FINE_Y_MASK
}


