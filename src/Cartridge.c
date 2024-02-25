
#include <stdlib.h>
#include "Utils.h"
#include "Common.h"

typedef struct NESCartridge
{
    isize PrgRomSize;
    u8 *PrgRom;
    isize ChrRomSize;
    u8 *ChrRom;

    u8 MapperID;
} NESCartridge;

NESCartridge NESCartridge_Init(
    const void *PrgRom, isize PrgRomSize, 
    const void *ChrRom, isize ChrRomSize, 
    u8 MapperID)
{
    NESCartridge Cartridge = {
        .MapperID = MapperID,
        .ChrRomSize = ChrRomSize,
        .PrgRomSize = PrgRomSize,
    };
    Cartridge.PrgRom = malloc(PrgRomSize);
    Cartridge.ChrRom = malloc(PrgRomSize);
    DEBUG_ASSERT(Cartridge.PrgRom);
    DEBUG_ASSERT(Cartridge.ChrRom);

    Memcpy(Cartridge.PrgRom, PrgRom, PrgRomSize);
    Memcpy(Cartridge.ChrRom, ChrRom, ChrRomSize);
    return Cartridge;
}

void NESCartridge_Destroy(NESCartridge *Cartridge)
{
    free(Cartridge->PrgRom);
    free(Cartridge->ChrRom);
    *Cartridge = (NESCartridge){ 0 };
}

