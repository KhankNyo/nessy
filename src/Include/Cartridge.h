#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "MapperInterface.h"

typedef enum NESNametableMirroring 
{
    NAMETABLE_HORIZONTAL,
    NAMETABLE_VERTICAL,
    NAMETABLE_ONESCREEN_HI,
    NAMETABLE_ONESCREEN_LO,
} NESNametableMirroring;
typedef struct NESCartridge
{
    NESNametableMirroring MirroringMode;
    NESMapperID MapperID;
    NESMapperInterface *MapperInterface;
} NESCartridge;


void NESCartridge_Destroy(NESCartridge *Cartridge);
NESCartridge NESCartridge_Init(
    const void *PrgRom, isize PrgRomSize, 
    const void *ChrRom, isize ChrRomSize, 
    isize PrgRamSize, isize ChrRamSize,
    u8 MapperID, 
    NESNametableMirroring MirroringMode,
    Bool8 AlternativeNametableLayout, 
    Bool8 HasBusConflict
);

u8 NESCartridge_CPURead(NESCartridge *Cartridge, u16 Address);
u8 NESCartridge_PPURead(NESCartridge *Cartridge, u16 Address);
u8 NESCartridge_DebugCPURead(NESCartridge *Cartridge, u16 Address);
u8 NESCartridge_DebugPPURead(NESCartridge *Cartridge, u16 Address);

void NESCartridge_CPUWrite(NESCartridge *Cartridge, u16 Address, u8 Byte);
void NESCartridge_PPUWrite(NESCartridge *Cartridge, u16 Address, u8 Byte);


#endif /* CARTRIDGE_H */

