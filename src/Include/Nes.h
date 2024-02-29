#ifndef NES_H
#define NES_H

#include "Common.h"

#define NES_CPU_RAM_SIZE 0x0800
#define NES_MASTER_CLK 21477272
#define NES_CPU_CLK 21441960
#define NES_SCREEN_HEIGHT 240
#define NES_SCREEN_WIDTH 256
#define NES_SCREEN_BUFFER_SIZE (NES_SCREEN_WIDTH * NES_SCREEN_HEIGHT)

typedef struct Nes_DisplayableStatus 
{
    u16 PC, SP;
    u8 A, X, Y;
    u8 N, V, U, B, D, I, Z, C;

    char DisasmBeforePC[512];
    char DisasmAtPC[128];
    char DisasmAfterPC[512];
} Nes_DisplayableStatus;

typedef struct Platform_FrameBuffer 
{
    const void *Data;
    u32 Width, Height;
} Platform_FrameBuffer;

typedef u16 Nes_ControllerStatus;



Platform_FrameBuffer Nes_PlatformQueryFrameBuffer(void);
Nes_DisplayableStatus Nes_PlatformQueryDisplayableStatus(void);

/* returns NULL on success, or a static error string on failure (no lifetime) */
const char *Nes_ParseINESFile(const void *FileBuffer, isize BufferSizeBytes);
void Nes_OnEntry(void);
void Nes_OnLoop(double ElapsedTime);
void Nes_AtExit(void);

void Nes_OnEmulatorToggleHalt(void);
void Nes_OnEmulatorReset(void);
void Nes_OnEmulatorSingleStep(void);
void Nes_OnEmulatorSingleFrame(void);

double Platform_GetTimeMillisec(void);
Nes_ControllerStatus Platform_GetControllerState(void);


#endif /* NES_H */

