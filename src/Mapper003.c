#ifndef NES_MAPPER_003_C

#include <stdlib.h>
#include "MapperInterface.h"
#include "Utils.h"


#define BANK_SIZE 8*1024

typedef struct NESMapper003 
{
    u32 PrgRomSize;
    u32 ChrMemSize;
    u32 CurrentChrBank;
    /* data after */
} NESMapper003;

static u8 *Mp003_GetPrgRomPtr(NESMapper003 *Mp003)
{
    u8 *BytePtr = ((u8*)Mp003) + sizeof *Mp003;
    return BytePtr;
}

static u8 *Mp003_GetChrPtr(NESMapper003 *Mp003)
{
    u8 *BytePtr = ((u8*)Mp003) + sizeof *Mp003;
    return BytePtr + Mp003->PrgRomSize;
}


NESMapperInterface *NESMapper003_Init(const void *PrgRom, isize PrgRomSize, const void *ChrRom, isize ChrRomSize)
{
    NESMapper003 *Mapper = NULL;
    if (ChrRomSize)
    {
        isize TotalSize = sizeof *Mapper + PrgRomSize + ChrRomSize;
        Mapper = malloc(TotalSize);
        DEBUG_ASSERT(Mapper);

        Mapper->PrgRomSize = PrgRomSize;
        Mapper->ChrMemSize = ChrRomSize;
        Mapper->CurrentChrBank = 0;

        /* copy the prg and chr rom */
        Memcpy(Mp003_GetPrgRomPtr(Mapper), PrgRom, PrgRomSize);
        Memcpy(Mp003_GetChrPtr(Mapper), ChrRom, ChrRomSize);
    }
    return Mapper;
}

void NESMapper003_Destroy(NESMapperInterface *Mapper)
{
    DEBUG_ASSERT(Mapper != NULL);
    free(Mapper);
}


u8 NESMapper003_Read(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper003 *Mapper = MapperInterface;
    /* palette */
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        u32 PhysAddr = Addr & (BANK_SIZE - 1);
        u32 BaseAddr = Mapper->CurrentChrBank * BANK_SIZE;
        return Mp003_GetChrPtr(Mapper)[BaseAddr + PhysAddr];
    }
    /* prg rom */
    else if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        u16 PhysAddr = Addr & (Mapper->PrgRomSize - 1);
        return Mp003_GetPrgRomPtr(Mapper)[PhysAddr];
    }

    return 0;
}

void NESMapper003_Write(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper003 *Mapper = MapperInterface;
    /* palette */
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        u32 PhysAddr = Addr & (BANK_SIZE - 1);
        u32 BaseAddr = Mapper->CurrentChrBank * BANK_SIZE;
        Mp003_GetChrPtr(Mapper)[BaseAddr + PhysAddr] = Byte;
    }
    /* prg rom */
    else if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        /* bus conflict, byte anded with value at the location */
        u16 PhysAddr = Addr & (Mapper->PrgRomSize - 1);
        u8 ValueAtAddr = Mp003_GetPrgRomPtr(Mapper)[PhysAddr];

        /* lower 2 bits only */
        Mapper->CurrentChrBank = ValueAtAddr & Byte & 0x03;
    }
}

u8 NESMapper003_DebugCPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper003_Read(Mapper, Addr);
}

u8 NESMapper003_DebugPPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper003_Read(Mapper, Addr);
}


#undef BANK_SIZE
#endif /* NES_MAPPER_003_C */

