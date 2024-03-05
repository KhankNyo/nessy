#ifndef NES_MAPPER_002_C

#include <stdlib.h>
#include "MapperInterface.h"
#include "Utils.h"



#define BANK_SIZE 0x4000

typedef struct NESMapper002 
{
    u32 PrgRomSize;
    u32 ChrMemSize;
    u32 CurrentLowRomBank;
    /* data after */
} NESMapper002;

static u8 *Mp002_GetPrgRomPtr(NESMapper002 *Mp002)
{
    u8 *BytePtr = ((u8*)Mp002) + sizeof *Mp002;
    return BytePtr;
}

static u8 *Mp002_GetChrPtr(NESMapper002 *Mp002)
{
    u8 *BytePtr = ((u8*)Mp002) + sizeof *Mp002;
    return BytePtr + Mp002->PrgRomSize;
}


NESMapperInterface *NESMapper002_Init(const void *PrgRom, isize PrgRomSize, const void *ChrRom, isize ChrRomSize)
{
    NESMapper002 *Mapper = NULL;
    if (ChrRomSize)
    {
        isize TotalSize = sizeof *Mapper + PrgRomSize + ChrRomSize;
        Mapper = malloc(TotalSize);
        DEBUG_ASSERT(Mapper);

        Mapper->ChrMemSize = ChrRomSize;
        Mapper->PrgRomSize = PrgRomSize;
        Mapper->CurrentLowRomBank = 0;

        /* copy the prg and chr rom */
        Memcpy(Mp002_GetChrPtr(Mapper), ChrRom, ChrRomSize);
    }
    else /* chr ram */
    {
        u16 ChrRamSize = 8*1024;
        isize TotalSize = sizeof *Mapper + PrgRomSize + ChrRamSize;
        Mapper = malloc(TotalSize);
        DEBUG_ASSERT(Mapper);

        Mapper->ChrMemSize = ChrRamSize;
        Mapper->PrgRomSize = PrgRomSize;
        Mapper->CurrentLowRomBank = 0;

        /* copy the prg and chr rom */
        Memset(Mp002_GetChrPtr(Mapper), 0, ChrRamSize);
    }

    Memcpy(Mp002_GetPrgRomPtr(Mapper), PrgRom, PrgRomSize);
    return Mapper;
}

void NESMapper002_Destroy(NESMapperInterface *Mapper)
{
    DEBUG_ASSERT(Mapper != NULL);
    free(Mapper);
}


u8 NESMapper002_Read(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper002 *Mapper = MapperInterface;
    /* palette */
    if (IN_RANGE(0x0000, Addr, 0x2000))
    {
        u16 PhysAddr = Addr & (Mapper->ChrMemSize - 1);
        return Mp002_GetChrPtr(Mapper)[PhysAddr];
    }
    /* prg rom (variable bank) */
    else if (IN_RANGE(0x8000, Addr, 0xBFFF))
    {
        u16 PhysAddr = Addr & (BANK_SIZE - 1);
        PhysAddr += Mapper->CurrentLowRomBank * BANK_SIZE;
        PhysAddr &= Mapper->PrgRomSize - 1;
        return Mp002_GetPrgRomPtr(Mapper)[PhysAddr];
    }
    /* prg rom (last bank always) */
    else if (IN_RANGE(0xC000, Addr, 0xFFFF))
    {
        u32 PhysAddr = Addr & (BANK_SIZE - 1);
        PhysAddr += Mapper->PrgRomSize - BANK_SIZE;
        return Mp002_GetPrgRomPtr(Mapper)[PhysAddr];
    }

    return 0;
}

void NESMapper002_Write(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper002 *Mapper = MapperInterface;
    /* palette */
    if (IN_RANGE(0x0000, Addr, 0x2000))
    {
        u16 PhysAddr = Addr & (Mapper->ChrMemSize - 1);
        Mp002_GetChrPtr(Mapper)[PhysAddr] = Byte;
    }
    /* bank select */
    else if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        u32 RomMask = 0xF;
        Mapper->CurrentLowRomBank = Byte & RomMask;
    }
}

u8 NESMapper002_DebugCPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper002_Read(Mapper, Addr);
}

u8 NESMapper002_DebugPPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper002_Read(Mapper, Addr);
}


#undef BANK_SIZE
#endif /* NES_MAPPER_002_C */

