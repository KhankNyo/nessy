#ifndef CARTRIDGE_H
#define CARTRIDGE_H

#include "MapperInterface.h"

typedef enum NESNameTableMirroring 
{
    NAMETABLE_HORIZONTAL,
    NAMETABLE_VERTICAL,
    NAMETABLE_ONESCREEN_HI,
    NAMETABLE_ONESCREEN_LO,
} NESNameTableMirroring;
typedef struct NESCartridge
{
    void *Rom;
    isize RomSizeBytes;
    NESNameTableMirroring MirroringMode;
    NESMapperInterface *MapperInterface;
} NESCartridge;


void NESCartridge_Destroy(NESCartridge *Cartridge);
NESCartridge NESCartridge_Init(
    const void *PrgRom, isize PrgRomSize, 
    const void *ChrRom, isize ChrRomSize, 
    u8 MapperID, 
    NESNameTableMirroring MirroringMode
);
u8 NESCartridge_Read(NESCartridge *Cartridge, u16 Address);
void NESCartridge_Write(NESCartridge *Cartridge, u16 Address, u8 Byte);


u8 NESCartridge_DebugCPURead(NESCartridge *Cartridge, u16 Address);
u8 NESCartridge_DebugPPURead(NESCartridge *Cartridge, u16 Address);


#endif /* CARTRIDGE_H */

