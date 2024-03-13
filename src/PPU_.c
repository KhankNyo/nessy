#ifndef NES_PPU__C
#define NES_PPU__C

#include "Nes.h"
#include "Cartridge.h"

/* Ctrl */
#define CTRL_NMI_ENABLE (1 << 7)
#define CTRL_SPR_HEIGHT (1 << 5)
#define CTRL_BG_BASE (1 << 4)
#define CTRL_SPR_BASE (1 << 3)
#define CTRL_INC_32 (1 << 2)
/* Mask */
#define MASK_ENABLE_BG          (1 << 3)
#define MASK_ENABLE_SPR         (1 << 4)
#define MASK_ENABLE_BG_LEFT     (1 << 1)
#define MASK_ENABLE_SPR_LEFT    (1 << 2)
/* Status */
#define STATUS_SPR_OVERFLOW (1 << 5)
#define STATUS_SPR_0_HIT    (1 << 6)
#define STATUS_VBLANK       (1 << 7)


#define GET_BASE_ADDR(fl) ((This->Ctrl & (fl))? 0x1000: 0x0000)
#define LAST_VISIBLE_CLK 256
#define LAST_VISIBLE_SCANLINE 239


typedef struct NESPPU NESPPU;
typedef void (*NESPPU_FrameCompletionCallback)(void *);
typedef void (*NESPPU_NmiCallback)(void *);


static u32 sPPURGBPalette[];
struct NESPPU 
{
    /* interface */
    void *UserData;
    NESPPU_FrameCompletionCallback FrameCallback;
    NESPPU_NmiCallback NmiCallback;
    NESCartridge **CartridgeHandle;
    u32 *ScreenBuffer;

    /* exposed registers */
    u8 Ctrl;
    u8 Mask;
    u8 Status;


    /* internal */
    u8 PaletteColorIndex[0x20];
    u8 Nametable[NES_NAMETABLE_SIZE*2];
    int Clk;
    int Scanline; 

    uint FineX:3;
    uint WriteLatch:1;
    uint VRAMAddr:15;
    uint TRAMAddr:15;
    u8 ReadBuffer;

    u8 OAMAddr;
    u8 OAM[256];
};





NESPPU NESPPU_Init(
    void *UserData, 
    NESPPU_FrameCompletionCallback FrameCallback, 
    NESPPU_NmiCallback NmiCallback, 
    u32 ScreenBuffer[NES_SCREEN_BUFFER_SIZE], 
    NESCartridge **CartridgeHandle)
{
    NESPPU This = {
        .UserData = UserData,
        .FrameCallback = FrameCallback,
        .NmiCallback = NmiCallback,
        .CartridgeHandle = CartridgeHandle,

        .ScreenBuffer = ScreenBuffer,
    };
    for (uint i = 0; i < STATIC_ARRAY_SIZE(This.PaletteColorIndex); i++)
    {
        This.PaletteColorIndex[i] = i;
    }
    return This;
}



static Bool8 NESPPU_IsBackgroundRenderingEnabled(const NESPPU *This)
{
    Bool8 ShouldRender = This->Mask & MASK_ENABLE_BG;
    Bool8 ShouldRenderEdge = This->Mask & MASK_ENABLE_BG_LEFT;
    return ShouldRender && (ShouldRenderEdge || This->Clk > 8);
}

static Bool8 NESPPU_IsSpriteRenderingEnabled(const NESPPU *This)
{
    Bool8 ShouldRender = This->Mask & MASK_ENABLE_SPR;
    Bool8 ShouldRenderEdge = This->Mask & MASK_ENABLE_SPR_LEFT;
    return ShouldRender && (ShouldRenderEdge || This->Clk > 8);
}

static u16 NESPPU_MirrorNametableAddr(const NESPPU *This, u16 LogicalAddr)
{
    NESNametableMirroring Mirroring = NAMETABLE_VERTICAL;
    if (This->CartridgeHandle && *This->CartridgeHandle)
        Mirroring = (*This->CartridgeHandle)->MirroringMode;

    u16 PhysAddr = 0;
    switch (Mirroring)
    {
    case NAMETABLE_VERTICAL:
    {
    } break;
    case NAMETABLE_HORIZONTAL:
    {
    } break;
    case NAMETABLE_ONESCREEN_HI:
    {
    } break;
    case NAMETABLE_ONESCREEN_LO:
    {
    } break;
    }
    return PhysAddr;
}

static void NESPPU_InternalWrite(NESPPU *This, u16 Addr, u8 Byte)
{
    Addr &= 0x3FFF;
    if (Addr < 0x2000)
    {
        if (This->CartridgeHandle && *This->CartridgeHandle)
            NESCartridge_PPUWrite(*This->CartridgeHandle, Addr, Byte);
    }
    else if (IN_RANGE(0x2000, Addr, 0x3EFF))
    {
        u16 PhysAddr = NESPPU_MirrorNametableAddr(This, Addr);
        This->Nametable[PhysAddr] = Byte;
    }
    else 
    {
        u16 PhysAddr = Addr & 0x1F;
        /* background palette */
        if ((PhysAddr & 0x3) == 0)
            PhysAddr &= 0xF;
        This->PaletteColorIndex[PhysAddr] = Byte;
    }
}

static u8 NESPPU_InternalRead(NESPPU *This, u16 Addr)
{
    Addr &= 0x3FFF;
    /* chr rom/ram */
    if (Addr < 0x2000)
    {
        if (This->CartridgeHandle && *This->CartridgeHandle)
            return NESCartridge_PPURead(*This->CartridgeHandle, Addr);
    }
    /* nametable */
    else if (IN_RANGE(0x2000, Addr, 0x3EFF))
    {
        u16 PhysAddr = NESPPU_MirrorNametableAddr(This, Addr);
        return This->Nametable[PhysAddr];
    }
    /* palette */
    else
    {
        u16 PhysAddr = Addr & 0x1F;
        /* background palette */
        if ((PhysAddr & 0x3) == 0)
            PhysAddr &= 0xF;
        return This->PaletteColorIndex[PhysAddr];
    }
    return 0;
}



void NESPPU_ExternalWrite(NESPPU *This, u16 Addr, u8 Byte)
{
    /* putting names in place of magic number would actually make it harder to follow nesdev 
     * (because they often use the address instead of the name to refer to mmio registers) */

    Addr = 0x2000 + (Addr % 8);
    switch (Addr)
    {
    case 0x2000: /* Ctrl */
    {
        Bool8 NmiWasDisabled = (This->Ctrl & CTRL_NMI_ENABLE) == 0;
        This->Ctrl = Byte;
        MASKED_LOAD(This->TRAMAddr, (u16)Byte << 10, 0x0C00);

        Bool8 InVBlank = 
            (This->Scanline == 241 && This->Clk > 0) 
            || IN_RANGE(242, This->Scanline, 260)
            || (This->Scanline == -1 && This->Clk == 0);
        if (InVBlank 
        && (This->Status & STATUS_VBLANK) 
        && NmiWasDisabled 
        && (This->Ctrl & CTRL_NMI_ENABLE))
        {
            This->NmiCallback(This->UserData);
        }
    } break;
    case 0x2001: /* Mask */
    {
        This->Mask = Byte;
    } break;
    case 0x2003: /* OAM Addr */
    {
        This->OAMAddr = Byte;
    } break;
    case 0x2004: /* OAM data */
    {
        /* note that only writes inc oamaddr, reads don't */
        This->OAM[This->OAMAddr++] = Byte;
    } break;
    case 0x2005: /* Scroll */
    {
        if (This->WriteLatch == 0)
        {
            MASKED_LOAD(This->TRAMAddr, Byte >> 3, 0x001F);
            This->FineX = Byte & 0x07;
        }
        else
        {
            uint FineYPos = 12;
            uint CoarseYPos = 5;
            MASKED_LOAD(This->TRAMAddr, (u16)Byte << CoarseYPos, (0x1F << CoarseYPos));
            MASKED_LOAD(This->TRAMAddr, (u16)Byte << FineYPos, (0x07 << FineYPos));
        }
        This->WriteLatch = ~This->WriteLatch;
    } break;
    case 0x2006: /* vram Address */
    {
        if (This->WriteLatch == 0)
        {
            MASKED_LOAD(This->TRAMAddr, (u16)Byte << 8, 0x3F00);
        }
        else 
        {
            MASKED_LOAD(This->TRAMAddr, (u16)Byte, 0x00FF);
            This->VRAMAddr = This->TRAMAddr;
        }
        This->WriteLatch = ~This->WriteLatch;
    } break;
    case 0x2007: /* vram Data */
    {
        NESPPU_InternalWrite(This, This->VRAMAddr, Byte);

        if (IN_RANGE(-1, This->Scanline, LAST_VISIBLE_SCANLINE)
        && (NESPPU_IsBackgroundRenderingEnabled(This) || NESPPU_IsSpriteRenderingEnabled(This)))
        { 
            /* bizzare update of vram addr */
        }
        else
        {
            /* update vram addr normally */
            This->VRAMAddr += This->Ctrl & CTRL_INC_32? 
                32 : 1;
        }
    } break;
    default: break;
    }
}

u8 NESPPU_ExternalRead(NESPPU *This, u16 Addr)
{
    Addr = 0x2000 + (Addr % 8);
    u8 Data = 0;
    switch (Addr)
    {
    case 0x2002: /* status */
    {
        Data = This->Status;
        This->Status |= ~STATUS_VBLANK;
        This->WriteLatch = 0;
    } break;
    case 0x2004: /* OAM Data */
    {
        /* read does not inc oamaddr, only writes do */
        Data = This->OAM[This->OAMAddr];
    } break;
    case 0x2007: /* vram Data */
    {
        Data = This->ReadBuffer;
        This->ReadBuffer = NESPPU_InternalRead(This, This->VRAMAddr);
        if (This->VRAMAddr >= 0x3F00) /* no buffering */
            Data = This->ReadBuffer;

        if (IN_RANGE(-1, This->Scanline, LAST_VISIBLE_SCANLINE) 
        && (NESPPU_IsBackgroundRenderingEnabled(This) || NESPPU_IsSpriteRenderingEnabled(This)))
        {
            /* bizzare update of vram addr during rendering */
            /* TODO: this inc will cause the inc in step clk to be delayed for that one cycle 
             * https://www.nesdev.org/wiki/PPU_scrolling */
        }
        else /* update vram addr normally outside of rendering */
        {
            This->VRAMAddr += This->Ctrl & CTRL_INC_32? 
                32 : 1;
        }
    } break;
    default: break;
    }
    return Data;
}


Bool8 NESPPU_StepClock(NESPPU *This)
{

    if (IN_RANGE(-1, This->Scanline, LAST_VISIBLE_SCANLINE))
    {
        if (IN_RANGE(1, This->Clk, LAST_VISIBLE_CLK)
        && IN_RANGE(321, This->Clk, 336)) /* ready for next scanline */
        {
        }

        /* constantly put 0 here during visible and pre scanline */
        if (IN_RANGE(257, This->Clk, 320))
        {
            This->OAMAddr = 0;
        }
    }


    
    This->Clk++;
    Bool8 FrameCompleted = 
        (This->Scanline == LAST_VISIBLE_SCANLINE 
         && This->Clk == LAST_VISIBLE_CLK);
    if (This->Clk == 341)
    {
        This->Clk = 0;
        This->Scanline++;
        if (This->Scanline == 261)
        {
            This->Scanline = -1;
        }
    }
    return FrameCompleted;
}






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


#endif /* NES_PPU__C */

