#ifndef NES_H
#define NES_H

#include "Common.h"

#define NES_CPU_RAM_SIZE 0x0800
#define NES_MASTER_CLK 21477272
#define NES_CPU_CLK 21441960
#define NES_SCREEN_HEIGHT 240
#define NES_SCREEN_WIDTH 256
#define NES_SCREEN_BUFFER_SIZE (NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT)

#define NES_PALETTE_SIZE 32
#define NES_NAMETABLE_SIZE 0x0400
#define NES_PATTERN_TABLE_SIZE 0x1000
#define NES_PATTERN_TABLE_WIDTH_PIX 16*8
#define NES_PATTERN_TABLE_HEIGHT_PIX 16*8


typedef struct Platform_FrameBuffer 
{
    const void *Data;
    u32 Width, Height;
} Platform_FrameBuffer;

typedef struct Nes_DisplayableStatus 
{
    u16 PC, SP, StackValue;
    u8 A, X, Y;
    u8 N, V, U, B, D, I, Z, C;

    u32 Palette[NES_PALETTE_SIZE];
    u32 LeftPatternTable[NES_PATTERN_TABLE_HEIGHT_PIX][NES_PATTERN_TABLE_WIDTH_PIX];
    u32 RightPatternTable[NES_PATTERN_TABLE_HEIGHT_PIX][NES_PATTERN_TABLE_WIDTH_PIX];
    Bool8 PatternTablesAvailable;

    char DisasmBeforePC[512];
    char DisasmAtPC[128];
    char DisasmAfterPC[512];
} Nes_DisplayableStatus;

typedef u16 Nes_ControllerStatus;


/* functions for the platform to request data from the emulator */
/* NOTE: the memory requested is guaranteed to be fixed throughout the program, 
 * so pointers to locations inside the memory buffer can be cached */
/* if the platform was unable to provide a suitable buffer, all Nes_* functions will never be called */
/* the memory given to Nes_OnEntry is guaranteed to be initialized with zero */
isize Nes_PlatformQueryStaticBufferSize(void);
Platform_FrameBuffer Nes_PlatformQueryFrameBuffer(BufferData StaticBuffer);
Nes_DisplayableStatus Nes_PlatformQueryDisplayableStatus(BufferData StaticBuffer);


/* functions for the emulator to request information from the platform */
double Platform_GetTimeMillisec(void);
Nes_ControllerStatus Platform_GetControllerState(void);


/* functions for the emulator to work in */
/* NOTE: Nes_OnEntry and the rest of the functions below it are 
 * only called when the platform was able to create a buffer that has 
 * the size requested by Nes_PlatformQueryStaticBufferSize */
void Nes_OnEntry(BufferData StaticBuffer);
void Nes_OnLoop(BufferData StaticBuffer, double ElapsedTime);
void Nes_AtExit(BufferData StaticBuffer);
/* event handlers */
void Nes_OnEmulatorToggleHalt(BufferData StaticBuffer);
void Nes_OnEmulatorTogglePalette(BufferData StaticBuffer);
void Nes_OnEmulatorReset(BufferData StaticBuffer);
void Nes_OnEmulatorSingleStep(BufferData StaticBuffer);
void Nes_OnEmulatorSingleFrame(BufferData StaticBuffer);
/* returns NULL on success, or a static error string on failure (no lifetime) */
const char *Nes_ParseINESFile(BufferData StaticBuffer, const void *FileBuffer, isize BufferSizeBytes);


#endif /* NES_H */

