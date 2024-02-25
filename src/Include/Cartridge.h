#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "MapperInterface.h"
typedef struct NESCartridge
{
    void *Rom;
    isize RomSizeBytes;
    NESMapperInterface *MapperInterface;
} NESCartridge;


void NESCartridge_Destroy(NESCartridge *Cartridge);
NESCartridge NESCartridge_Init(
    const void *PrgRom, isize PrgRomSize, 
    const void *ChrRom, isize ChrRomSize, 
    u8 MapperID
);
u8 NESCartridge_Read(NESCartridge *Cartridge, u16 Address);
void NESCartridge_Write(NESCartridge *Cartridge, u16 Address, u8 Byte);

#endif /* CARTRIDGE_H */

