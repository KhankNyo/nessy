#ifndef NES_H
#define NES_H

#include "Common.h"

typedef struct Nes_DisplayableStatus 
{
    u16 PC, SP;
    u8 A, X, Y;
    u8 N, V, U, B, D, I, Z, C;
    u8 Opcode;
} Nes_DisplayableStatus;

Nes_DisplayableStatus Nes_QueryStatus(void);
void Nes_OnEntry(void);
void Nes_OnLoop(void);
void Nes_AtExit(void);


#endif /* NES_H */

