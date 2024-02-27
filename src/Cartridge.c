
#include <stdlib.h>
#include "Utils.h"
#include "Common.h"
#include "Cartridge.h"
#define MAPPER_INTERFACE_IMPLEMENTATION
#include "MapperInterface.h"
#undef MAPPER_INTERFACE_IMPLEMENTATION

void NESCartridge_Destroy(NESCartridge *Cartridge)
{
    if (Cartridge->Rom)
    {
        NESMapperInterface_Destroy(Cartridge->MapperInterface);
        free(Cartridge->Rom);
        *Cartridge = (NESCartridge){ 0 };
    }
}

NESCartridge NESCartridge_Init(
    const void *PrgRom, isize PrgRomSize, 
    const void *ChrRom, isize ChrRomSize, 
    u8 MapperID, 
    NESNameTableMirroring MirroringMode)
{
    /* if no ChrRom, use ChrRam */
    if (ChrRomSize == 0)
        ChrRomSize = 8 * 1024;

    NESCartridge Cartridge = {
        .RomSizeBytes = PrgRomSize + ChrRomSize,
        .MirroringMode = MirroringMode,
    };
    /* create memory for both prg and chr rom */
    Cartridge.Rom = malloc(Cartridge.RomSizeBytes);
    DEBUG_ASSERT(Cartridge.Rom);

    Cartridge.MapperInterface = NESMapperInterface_Init(
        MapperID, 
        Cartridge.Rom, 
        PrgRom, PrgRomSize, 
        ChrRom, ChrRomSize
    );
    if (NULL == Cartridge.MapperInterface)
        NESCartridge_Destroy(&Cartridge);

    return Cartridge;
}


u8 NESCartridge_Read(NESCartridge *Cartridge, u16 Address)
{
    return NESMapperInterface_Read(Cartridge->MapperInterface, Address);
}

void NESCartridge_Write(NESCartridge *Cartridge, u16 Address, u8 Byte)
{    
    NESMapperInterface_Write(Cartridge->MapperInterface, Address, Byte);
}


