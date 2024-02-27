#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "MapperInterface.h"

typedef enum NESNametableOrientation 
{
    NAMETABLE_HORIZONTAL,
    NAMETABLE_VERTICAL,
    NAMETABLE_ONESCREEN_HI,
    NAMETABLE_ONESCREEN_LO,
} NESNametableOrientation;
typedef struct NESCartridge
{
    void *Rom;
    isize RomSizeBytes;
    NESNametableOrientation MirroringMode;
    NESMapperInterface *MapperInterface;
} NESCartridge;


void NESCartridge_Destroy(NESCartridge *Cartridge);
NESCartridge NESCartridge_Init(
    const void *PrgRom, isize PrgRomSize, 
    const void *ChrRom, isize ChrRomSize, 
    u8 MapperID, 
    NESNametableOrientation MirroringMode
);
u8 NESCartridge_Read(NESCartridge *Cartridge, u16 Address);
void NESCartridge_Write(NESCartridge *Cartridge, u16 Address, u8 Byte);

#endif /* CARTRIDGE_H */

