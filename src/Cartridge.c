
#include <stdlib.h>
#include "Utils.h"
#include "Common.h"
#include "Cartridge.h"

#define MAPPER_INTERFACE_IMPL
#   include "MapperInterface.h"
#undef MAPPER_INTERFACE_IMPL 



void NESCartridge_Destroy(NESCartridge *Cartridge)
{
    if (Cartridge->MapperInterface)
    {
        switch (Cartridge->MapperID)
        {
        case 0: NESMapper000_Destroy(Cartridge->MapperInterface); break;
        case 2: NESMapper002_Destroy(Cartridge->MapperInterface); break;
        case 3: NESMapper003_Destroy(Cartridge->MapperInterface); break;
        }
        *Cartridge = (NESCartridge){ 0 };
    }
}

NESCartridge NESCartridge_Init(
    const void *PrgRom, isize PrgRomSize, 
    const void *ChrRom, isize ChrRomSize, 
    u8 MapperID, 
    NESNameTableMirroring MirroringMode)
{
    NESCartridge Cartridge = {
        .MirroringMode = MirroringMode,
        .MapperID = MapperID,
        .MapperInterface = NULL,
    };

    switch (Cartridge.MapperID)
    {
    case 0: Cartridge.MapperInterface = NESMapper000_Init(PrgRom, PrgRomSize, ChrRom, ChrRomSize); break;
    case 2: Cartridge.MapperInterface = NESMapper002_Init(PrgRom, PrgRomSize, ChrRom, ChrRomSize); break;
    case 3: Cartridge.MapperInterface = NESMapper003_Init(PrgRom, PrgRomSize, ChrRom, ChrRomSize); break;
    }
    return Cartridge;
}


u8 NESCartridge_Read(NESCartridge *Cartridge, u16 Address)
{
    DEBUG_ASSERT(Cartridge->MapperInterface != NULL);
    switch (Cartridge->MapperID)
    {
    case 0: return NESMapper000_Read(Cartridge->MapperInterface, Address);
    case 2: return NESMapper002_Read(Cartridge->MapperInterface, Address); 
    case 3: return NESMapper003_Read(Cartridge->MapperInterface, Address); 
    }
    return 0;
}

void NESCartridge_Write(NESCartridge *Cartridge, u16 Address, u8 Byte)
{    
    DEBUG_ASSERT(Cartridge->MapperInterface != NULL);
    switch (Cartridge->MapperID)
    {
    case 0: NESMapper000_Write(Cartridge->MapperInterface, Address, Byte); break;
    case 2: NESMapper002_Write(Cartridge->MapperInterface, Address, Byte); break;
    case 3: NESMapper003_Write(Cartridge->MapperInterface, Address, Byte); break;
    }
}



u8 NESCartridge_DebugCPURead(NESCartridge *Cartridge, u16 Address)
{
    DEBUG_ASSERT(Cartridge->MapperInterface != NULL);
    switch (Cartridge->MapperID)
    {
    case 0: return NESMapper000_DebugCPURead(Cartridge->MapperInterface, Address);
    case 2: return NESMapper002_DebugCPURead(Cartridge->MapperInterface, Address);
    case 3: return NESMapper003_DebugCPURead(Cartridge->MapperInterface, Address); 
    }
    return 0;
}

u8 NESCartridge_DebugPPURead(NESCartridge *Cartridge, u16 Address)
{
    DEBUG_ASSERT(Cartridge->MapperInterface != NULL);
    switch (Cartridge->MapperID)
    {
    case 0: return NESMapper000_DebugPPURead(Cartridge->MapperInterface, Address);
    case 2: return NESMapper002_DebugPPURead(Cartridge->MapperInterface, Address);
    case 3: return NESMapper003_DebugPPURead(Cartridge->MapperInterface, Address); 
    }
    return 0;
}


