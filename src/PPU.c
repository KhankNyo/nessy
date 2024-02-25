
#include <stdlib.h>
#include <time.h>
#include "Common.h"


typedef struct NESPPU NESPPU;
typedef void (*NESPPU_FrameCompletionCallback)(NESPPU *);

struct NESPPU 
{
    uint Clk;
    int Scanline;
    u32 ScreenIndex;

    u8 PaletteColorIndex[0x20];
    u8 PatternTable[0x2000];
    u8 NameAndAttributeTable[0x400];
    u8 ObjectAttributeMemory[256];

    NESPPU_FrameCompletionCallback FrameCompletionCallback;
    u32 *ScreenOutput;
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

NESPPU NESPPU_Init(NESPPU_FrameCompletionCallback Callback, u32 BackBuffer[NES_SCREEN_BUFFER_SIZE])
{
    NESPPU This = {
        .ScreenOutput = BackBuffer,
        .FrameCompletionCallback = Callback
    };
    srand(time(NULL));
    return This;
}

void NESPPU_StepClock(NESPPU *This)
{
    /* does the render, a single pixel */
    This->ScreenOutput[This->ScreenIndex] = rand() > RAND_MAX/2? 
        0x00FFFFFF : 0x00000000l;

    This->ScreenIndex++;
    if (This->ScreenIndex >= NES_SCREEN_BUFFER_SIZE)
        This->ScreenIndex = 0;

    This->Clk++;
    if (This->Clk >= 341) /* PPU cycles per scanline */
    {
        This->Clk = 0;
        This->Scanline++;
        if (This->Scanline >= 261)
        {
            This->Scanline = -1;
            This->FrameCompletionCallback(This);
        }
    }
}

u8 NESPPU_ReadInternalMemory(NESPPU *This, u16 Address)
{
    Address &= 0x3FFF;
    if (Address < 0x2000) /* pattern table memory */
    {
        return This->PatternTable[Address];
    }
    /* NOTE: manual said 0x2EFF
     * https://www.nesdev.org/NESDoc.pdf */
    else if (IN_RANGE(0x2000, Address, 0x2FFF)) /* name table and attribute table */
    {
        Address %= 0x400;
        return This->NameAndAttributeTable[Address];
    }
    else /* sprite palette, image palette */
    {
        Address %= 0x20;
        return This->PaletteColorIndex[Address];
    }
}

void NESPPU_WriteInternalMemory(NESPPU *This, u16 Address, u8 Byte)
{
    Address &= 0x3FFF;
    if (Address < 0x2000) /* pattern table memory */
    {
        This->PatternTable[Address] = Byte;
    }
    /* NOTE: manual said 0x2EFF
     * https://www.nesdev.org/NESDoc.pdf */
    else if (IN_RANGE(0x2000, Address, 0x2FFF)) /* name table and attribute table */
    {
        Address %= 0x400;
        This->NameAndAttributeTable[Address] = Byte;
    }
    else /* sprite palette, image palette */
    {
        Address %= 0x20;
        This->PaletteColorIndex[Address] = Byte;
    }
}



