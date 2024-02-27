
#include <stdlib.h>
#include <time.h>

#include "Common.h"
#include "Cartridge.h"


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
    u8 AttrTableByteLatch;
    u8 PatternTableLoLatch;
    u8 PatternTableHiLatch;


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
        Address = NESPPU_MirrorNameTableAddr(This->CartridgeHandle, Address);
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
        Address = NESPPU_MirrorNameTableAddr(This->CartridgeHandle, Address);
        This->NameAndAttributeTable[Address] = Byte;
    }
    else /* 0x3F00-0x3FFF: sprite palette, image palette */
    {
        Address %= 0x20;
        This->PaletteColorIndex[Address] = Byte;
    }
}




void NESPPU_UpdateShifters(NESPPU *This)
{
}

void NESPPU_ReloadShifters(NESPPU *This)
{
}

Bool8 NESPPU_StepClock(NESPPU *This)
{
    /* 
     * WARNING: the following was known to cause cancer, stroke, prostate cancer and even death. 
     *          Continue with descretion. YOU HAVE BEEN WARNED.
     * In all seriousness, consult https://www.nesdev.org/w/images/default/4/4f/Ppu.svg before reading this 
     * */


#if 0
    /* does the render, a single pixel */
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

    /* putting these magic numbers in variable name makes this harder to read */
    /* visible scanlines (rendering) */
    if (IN_RANGE(0, This->Scanline, 239))
    {
        /* idle or something?? */
        if (This->Clk == 0) 
        {
        }
        /* fetching for rendering       [1, 256] 
         * prefetching for next frame   [321, 336] */
        else if (IN_RANGE(1, This->Clk, 256)
              || IN_RANGE(321, This->Clk, 336)
        )
        {
            u16 NameTableOffset = 0x2000;
            NESPPU_UpdateShifters(This);

            /* basically a state machine */
            /* subtract 1 bc clk == 0 was an idle clk */
            switch ((This->Clk - 1) % 8)
            {
            /* fetch NameTable byte */
            case 0:
            {
                if (This->Clk >= 9) 
                {
                    NESPPU_ReloadShifters(This);
                }

                /* 0x0FFF: 12 bit addr containing: 
                 *  nametable-y 
                 *  nametable-x 
                 *  coarse-y 
                 *  coarse-x
                 * is the index into a nametable byte */
                u16 NameTableAddr = NameTableOffset + (This->Loopy.v & 0x0FFF);
                This->NameTableByteLatch = NESPPU_ReadInternalMemory(This, NameTableAddr);
            } break;
            /* fetch Attribute Table byte */
            case 2:
            {
                /* 
                 *  A corresponding tile pointed to by the nametable byte:
                 *  #############
                 *  #  |  #  |  #
                 *  #--0--#--1--#    <-+
                 *  #  |  #  |  #      |
                 *  #############      +-- note the numbers: the 4 tiles (squares) surround those numbers  
                 *  #  |  #  |  #      |                     share that number from the 2-bit portions of the attr byte
                 *  #--2--#--3--#    <-+
                 *  #  |  #  |  #
                 *  #############
                 *  has corresponding attr byte:
                 *             msb    lsb
                 *  attr byte: 33221100  
                 *
                 *
                 * NOTE: to follow this explanation, get a paper and pencil and draw these out 
                 *      or watch Javid9x's video, 
                 *      whatever you do, don't use the NesDev docs
                 *
                 *
                 * because the x-span of a 4x4 tile region is 4 
                 * and 2 consecutive tiles (in the x-direction, and the consecutive tile pairs cannot overlap) 
                 * use the same 2 bits in lower nibble of the attr byte:
                 *
                 *    offset of attr byte   =  (x offset of nametable byte) / 4         (fetch 1 attr byte for 4 nametable bytes)
                 *
                 *    offset of attr bit    = ((x offset of nametable byte) / 2) % 2
                 *                          now we got either 0 or 1, but since we use bit pairs (index of 0 or 2), 
                 *                          we need to multiply it by 2
                 *                          = ((         coarseX          ) / 2) % 2 * 2 
                 *                          = ((         coarseX          ) % 4) / 2 * 2 (equivalent to the above)
                 *                          NOTE: because this is integer division,     x/2 * 2     != x
                 *                                                                  ((x >> 1) << 1) != x
                 *                          so instead of simplifying x/2*2, we simply check bit 1
                 *                          = ((         coarseX          ) % 4) & 0x02   
                 *                          if bit 1 is on, we get a 2, 
                 *                          else we get 0
                 *                          this is exactly what we want
                 *
                 *    => offset of attr byte = coarseX/4
                 *    => offset of attr bit  = (coarseX % 4) & 0x2
                 * NOTE: offset of attr bit is only 0 or 2, so we're missing 4 and 6
                 *    to get 4 or 6, we need to calculate a bit offset of either 0 or 4 from coarseY of the nametable byte, 
                 *    add that offset to the one from coarseX and we're done
                 *
                 * because the y-span of a 4x4 tile region is 4, 
                 * and 2 consecutive scanlines of tiles use the same 2 bits in the attr byte:
                 *    extra offset of attr bit  =  ((y offset of nametable byte) / 2) % 2
                 *                              =  ((          coarseY         ) % 4) / 2
                 *                              now we got 0 or 1, multiply by 4 to get 0 or 4
                 *                              =  ((          coarseY         ) % 4) / 2 * 4
                 *                              simplify
                 *                              =  ((          coarseY         ) % 4) / 2 * 2 * 2
                 *                              = (((          coarseY         ) % 4) & 0x2) * 2
                 *                              so now if bit 1 is on, we get 2*2 = 4
                 *                              else we get 0
                 * Results:
                 *    => offset of attr bit  = 
                 *       ((CoarseX % 4) & 0x02) 
                 *       + ((CoarseY % 4) & 0x02) * 2
                 *    => offset of attr byte = 
                 *       CoarseX/4
                 * */

                u16 AttrTableBase = 0x23C0;

                /* find byte offset */
                u16 CoarseX = (This->Loopy.v & 0x1F);
                u16 AttrByteOffset = CoarseX / 4;

                u16 AttrTableAddr = AttrTableBase + AttrByteOffset;
                u8 AttrTableByte = NESPPU_ReadInternalMemory(This, AttrTableAddr);

                /* find bit offset */
                u16 CoarseY = (This->Loopy.v >> 5) & 0x1F;
                uint BitOffset = (CoarseX % 4) & 0x2;
                BitOffset += ((CoarseY % 4) & 0x2)*2;

                /* only care about 2 bits in the byte */
                This->AttrTableByteLatch = (AttrTableByte >> BitOffset) & 0x3;
            } break;
            /* read pattern table low
             * using the byte from the NameTable a few cycles earlier */
            case 4:
            {
                u16 PatternLoAddr = This->Ctrl & PPUCTRL_BG_PATTERN_ADDR?
                    0x1000 : 0x0000;
                uint PatternTableSize = 8; /* 8 bytes per pattern table (8x8 pixels) */
                PatternLoAddr += (u16)This->NameTableByteLatch * (2 * PatternTableSize);    /* lsb and msb */
                PatternLoAddr += (This->Loopy.v >> 12);                                     /* fine y */

                This->PatternTableLoLatch = NESPPU_ReadInternalMemory(
                    This, 
                    PatternLoAddr
                );
            } break;
            /* read pattern table high (+8 bytes from low) */
            case 6:
            {
                u16 PatternHiAddr = This->Ctrl & PPUCTRL_BG_PATTERN_ADDR?
                    0x1000 : 0x0000;
                uint PatternTableSize = 8; /* 8 bytes per pattern table (8x8 pixels) */
                PatternHiAddr += (u16)This->NameTableByteLatch * (2 * PatternTableSize);    /* lsb and msb */
                PatternHiAddr += (This->Loopy.v >> 12) + 8;             /* fine y, +8 to get the pattern table right after */

                This->PatternTableHiLatch = NESPPU_ReadInternalMemory(
                    This, 
                    PatternHiAddr
                );
            } break;
            /* prepare v to fetch next NameTable and AttrTable */
            case 7:
            {
                This->Loopy.v++;
            } break;
            }
        }
        /* garbage NameTable fetches, but important for sprite  */
        else if (IN_RANGE(257, This->Clk, 320))
        {
        }
        /* dummy fetch, but mappers like MMC5 are sensitive to it */
        else if (This->Clk == 337 || This->Clk == 339)
        {
            NESPPU_ReadInternalMemory(This, This->Loopy.v);
        }
    }
    /* vblank begins */
    else if (This->Scanline == 241 
          && This->Clk == 1
    )
    {
        This->Status |= PPUSTATUS_VBLANK;
        if (This->Ctrl & PPUCTRL_NMI_ENABLE)
            This->NmiCallback(This);
    }
    /* vblank ends, prepare for next frame */
    else if (This->Scanline == -1)
    {
        if (This->Clk == 1)
        {
            This->Status &= ~(
                PPUSTATUS_VBLANK
                | PPUSTATUS_SPR0_HIT 
                | PPUSTATUS_SPR_OVERFLOW
            );
        }
    }
#endif /* */

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
}


