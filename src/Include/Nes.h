#ifndef NES_H
#define NES_H

#include "Common.h"

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

Platform_FrameBuffer Nes_PlatformQueryFrameBuffer(void);

void Nes_OnEntry(void);
void Nes_OnLoop(void);
void Nes_AtExit(void);

double Platform_GetTimeMillisec(void);
void Platform_NesNotifyChangeInStatus(const Nes_DisplayableStatus *Status);


#endif /* NES_H */

