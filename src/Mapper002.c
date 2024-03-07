#ifndef NES_MAPPER_002_C

#include <stdlib.h>
#include "MapperInterface.h"
#include "Utils.h"


typedef struct NESMapper002 
{
    u32 PrgRomSize;
    u32 ChrRamSize;
    u32 CurrentLowRomBank;
    u32 PrgBankSize;
    /* data after */
} NESMapper002;

static u8 *Mp002_GetPrgRomPtr(NESMapper002 *Mp002)
{
    u8 *BytePtr = (u8*)(Mp002 + 1);
    return BytePtr;
}

static u8 *Mp002_GetChrPtr(NESMapper002 *Mp002)
{
    u8 *BytePtr = (u8*)(Mp002 + 1);
    return BytePtr + Mp002->PrgRomSize;
}


NESMapperInterface *NESMapper002_Init(const void *PrgRom, isize PrgRomSize, isize ChrRamSize)
{
    NESMapper002 *Mapper = NULL;
    isize TotalSize = sizeof(*Mapper) + PrgRomSize + ChrRamSize;
    Mapper = malloc(TotalSize);
    DEBUG_ASSERT(Mapper);

    Mapper->ChrRamSize = ChrRamSize;
    Mapper->PrgRomSize = PrgRomSize;
    Mapper->CurrentLowRomBank = 0;
    Mapper->PrgBankSize = 16 * 1024;

    u8 *MapperPrgRom = Mp002_GetPrgRomPtr(Mapper);
    u8 *MapperChrRam = Mp002_GetChrPtr(Mapper);
    Memcpy(MapperPrgRom, PrgRom, PrgRomSize);
    Memset(MapperChrRam, 0, ChrRamSize);
    return Mapper;
}

void NESMapper002_Reset(NESMapperInterface *MapperInterface)
{
    NESMapper002 *Mapper = MapperInterface;
    Mapper->CurrentLowRomBank = 0;
    Mapper->PrgBankSize = 16 * 1024;
}

void NESMapper002_Destroy(NESMapperInterface *Mapper)
{
    DEBUG_ASSERT(Mapper != NULL);
    free(Mapper);
}



u8 NESMapper002_CPURead(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper002 *Mapper = MapperInterface;
    /* prg rom (variable bank) */
    if (IN_RANGE(0x8000, Addr, 0xBFFF))
    {
        u32 PhysAddr = Addr % Mapper->PrgBankSize;
        u32 BaseAddr = Mapper->CurrentLowRomBank * Mapper->PrgBankSize;
        u8 *PrgRom = Mp002_GetPrgRomPtr(Mapper);
        return PrgRom[BaseAddr + PhysAddr];
    }
    /* prg rom (last bank always) */
    else if (IN_RANGE(0xC000, Addr, 0xFFFF))
    {
        u32 PhysAddr = Addr % Mapper->PrgBankSize;
        u32 BaseAddr = Mapper->PrgRomSize - Mapper->PrgBankSize;
        u8 *PrgRom = Mp002_GetPrgRomPtr(Mapper);
        return PrgRom[BaseAddr + PhysAddr];
    }
    return 0;
}

u8 NESMapper002_PPURead(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper002 *Mapper = MapperInterface;
    /* palette */
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        u16 PhysAddr = Addr % Mapper->ChrRamSize;
        u8 *ChrRam = Mp002_GetChrPtr(Mapper);
        return ChrRam[PhysAddr];
    }
    return 0;
}



void NESMapper002_CPUWrite(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper002 *Mapper = MapperInterface;
    /* palette */
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        u16 PhysAddr = Addr % Mapper->ChrRamSize;
        u8 *ChrRam = Mp002_GetChrPtr(Mapper);
        ChrRam[PhysAddr] = Byte;
    }
}

void NESMapper002_PPUWrite(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper002 *Mapper = MapperInterface;
    /* bank select */
    if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        Mapper->CurrentLowRomBank = Byte & 0x7;
    }
}



u8 NESMapper002_DebugCPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper002_CPURead(Mapper, Addr);
}

u8 NESMapper002_DebugPPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper002_PPURead(Mapper, Addr);
}


#endif /* NES_MAPPER_002_C */

