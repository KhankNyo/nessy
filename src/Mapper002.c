#ifndef NES_MAPPER_002_C
#define NES_MAPPER_002_C

#include <stdlib.h>
#include "MapperInterface.h"
#include "Utils.h"


typedef struct NESMapper002 
{
    u8 *PrgRom;
    u8 *CurrentRomBank;
    u8 *LastRomBank;
    u32 PrgRomSize;

    u32 ChrRamSize;
    u8 *ChrRam;
    /* data after */
} NESMapper002;



NESMapperInterface *NESMapper002_Init(const void *PrgRom, isize PrgRomSize, isize ChrRamSize)
{
    u8 *BytePtr = malloc(sizeof(NESMapper002) + PrgRomSize + ChrRamSize);
    DEBUG_ASSERT(BytePtr);

    NESMapper002 *Mapper = (NESMapper002 *)BytePtr;
    Mapper->PrgRom = BytePtr + sizeof(*Mapper);
    Mapper->PrgRomSize = PrgRomSize;
    Mapper->CurrentRomBank = Mapper->PrgRom;
    Mapper->LastRomBank = Mapper->PrgRom + PrgRomSize - 0x4000;

    const u8 *TmpPtr = PrgRom;
    for (isize i = 0; i < PrgRomSize; i++)
        Mapper->PrgRom[i] = TmpPtr[i];

    Mapper->ChrRam = Mapper->PrgRom + PrgRomSize;
    Mapper->ChrRamSize = ChrRamSize;
    Memset(Mapper->ChrRam, 0, ChrRamSize);

    return Mapper;
}

void NESMapper002_Reset(NESMapperInterface *MapperInterface)
{
    NESMapper002 *Mapper2 = MapperInterface;
    Mapper2->CurrentRomBank = Mapper2->PrgRom;
}

void NESMapper002_Destroy(NESMapperInterface *Mapper)
{
    free(Mapper);
}



u8 NESMapper002_CPURead(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper002 *Mapper = MapperInterface;
    if (IN_RANGE(0x8000, Addr, 0xBFFF))
    {
        return Mapper->CurrentRomBank[Addr & 0x3FFF];
    }
    if (IN_RANGE(0xC000, Addr, 0xFFFF))
    {
        return Mapper->LastRomBank[Addr & 0x3FFF];
    }
    return 0;
}

u8 NESMapper002_PPURead(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper002 *Mapper = MapperInterface;
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        return Mapper->ChrRam[Addr];
    }
    return 0;
}



void NESMapper002_PPUWrite(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper002 *Mapper = MapperInterface;
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        Mapper->ChrRam[Addr] = Byte;
    }
}

void NESMapper002_CPUWrite(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper002 *Mapper = MapperInterface;
    if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        u8 *Base = (Byte & 0x7) * 0x4000 + Mapper->PrgRom;
        Mapper->CurrentRomBank = Base;
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

