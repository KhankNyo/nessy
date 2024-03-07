
#include <stdlib.h>
#include "Utils.h"
#include "Common.h"
#include "Cartridge.h"

#define MAPPER_INTERFACE_IMPL
#   include "MapperInterface.h"
#undef MAPPER_INTERFACE_IMPL 


static NESMapper_ReadFn sMapperCPURead[0x100] = { 
    [0] = NESMapper000_CPURead,
    [2] = NESMapper002_CPURead,
    [3] = NESMapper003_CPURead,
};
static NESMapper_WriteFn sMapperCPUWrite[0x100] = {
    [0] = NESMapper000_CPUWrite,
    [2] = NESMapper002_CPUWrite,
    [3] = NESMapper003_CPUWrite,
};

static NESMapper_ReadFn sMapperPPURead[0x100] = { 
    [0] = NESMapper000_PPURead,
    [2] = NESMapper002_PPURead,
    [3] = NESMapper003_PPURead,
};
static NESMapper_WriteFn sMapperPPUWrite[0x100] = {
    [0] = NESMapper000_PPUWrite,
    [2] = NESMapper002_PPUWrite,
    [3] = NESMapper003_PPUWrite,
};

static NESMapper_ResetFn sMapperReset[0x100] = {
    [0] = NESMapper000_Reset,
    [2] = NESMapper002_Reset,
    [3] = NESMapper003_Reset,
};

static NESMapper_DebugCPUReadFn sMapperDebugCPURead[0x100] = {
    [0] = NESMapper000_DebugCPURead,
    [2] = NESMapper002_DebugCPURead,
    [3] = NESMapper003_DebugCPURead,
};
static NESMapper_DebugPPUReadFn sMapperDebugPPURead[0x100] = {
    [0] = NESMapper000_DebugPPURead,
    [2] = NESMapper002_DebugPPURead,
    [3] = NESMapper003_DebugPPURead,
};


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

void NESCartridge_Reset(NESCartridge *Cartridge)
{
    DEBUG_ASSERT(Cartridge->MapperInterface);
    DEBUG_ASSERT(sMapperReset[Cartridge->MapperID]);
    sMapperReset[Cartridge->MapperID](Cartridge->MapperInterface);
}

NESCartridge NESCartridge_Init(
    const void *PrgRom, isize PrgRomSize, 
    const void *ChrRom, isize ChrRomSize, 
    isize PrgRamSize, isize ChrRamSize, 
    u8 MapperID, 
    NESNametableMirroring MirroringMode,
    Bool8 AlternativeNametableLayout, 
    Bool8 HasBusConflict)
{
    NESCartridge Cartridge = {
        .MirroringMode = MirroringMode,
        .MapperID = MapperID,
        .MapperInterface = NULL,
    };

    switch (Cartridge.MapperID)
    {
    case 0: Cartridge.MapperInterface = NESMapper000_Init(PrgRom, PrgRomSize, ChrRom, ChrRomSize); break;
    case 2: Cartridge.MapperInterface = NESMapper002_Init(PrgRom, PrgRomSize, ChrRamSize); break;
    case 3: Cartridge.MapperInterface = NESMapper003_Init(PrgRom, PrgRomSize, ChrRom, ChrRomSize, HasBusConflict); break;
    }
    return Cartridge;
}


u8 NESCartridge_CPURead(NESCartridge *Cartridge, u16 Address)
{
    DEBUG_ASSERT(Cartridge->MapperInterface != NULL);
    DEBUG_ASSERT(sMapperCPURead[Cartridge->MapperID]);
    return sMapperCPURead[Cartridge->MapperID](Cartridge->MapperInterface, Address);
}

void NESCartridge_CPUWrite(NESCartridge *Cartridge, u16 Address, u8 Byte)
{    
    DEBUG_ASSERT(Cartridge->MapperInterface != NULL);
    DEBUG_ASSERT(sMapperCPUWrite[Cartridge->MapperID]);
    sMapperCPUWrite[Cartridge->MapperID](Cartridge->MapperInterface, Address, Byte);
}

u8 NESCartridge_PPURead(NESCartridge *Cartridge, u16 Address)
{
    DEBUG_ASSERT(Cartridge->MapperInterface != NULL);
    DEBUG_ASSERT(sMapperPPURead[Cartridge->MapperID]);
    return sMapperPPURead[Cartridge->MapperID](Cartridge->MapperInterface, Address);
}

void NESCartridge_PPUWrite(NESCartridge *Cartridge, u16 Address, u8 Byte)
{    
    DEBUG_ASSERT(Cartridge->MapperInterface != NULL);
    DEBUG_ASSERT(sMapperPPUWrite[Cartridge->MapperID]);
    sMapperPPUWrite[Cartridge->MapperID](Cartridge->MapperInterface, Address, Byte);
}



u8 NESCartridge_DebugCPURead(NESCartridge *Cartridge, u16 Address)
{
    DEBUG_ASSERT(Cartridge->MapperInterface != NULL);
    DEBUG_ASSERT(sMapperDebugCPURead[Cartridge->MapperID]);
    return sMapperDebugCPURead[Cartridge->MapperID](Cartridge->MapperInterface, Address);
}

u8 NESCartridge_DebugPPURead(NESCartridge *Cartridge, u16 Address)
{
    DEBUG_ASSERT(Cartridge->MapperInterface != NULL);
    DEBUG_ASSERT(sMapperDebugPPURead[Cartridge->MapperID]);
    return sMapperDebugPPURead[Cartridge->MapperID](Cartridge->MapperInterface, Address);
}


