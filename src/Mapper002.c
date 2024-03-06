#ifndef NES_MAPPER_002_C

#include <stdlib.h>
#include "MapperInterface.h"
#include "Utils.h"



#define BANK_SIZE (16 * 1024)

typedef struct NESMapper002 
{
    u32 PrgRomSize;
    u32 ChrMemSize;
    u32 CurrentLowRomBank;
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
        Memcpy(Mp002_GetPrgRomPtr(Mapper), PrgRom, PrgRomSize);
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
        Memcpy(Mp002_GetPrgRomPtr(Mapper), PrgRom, PrgRomSize);
        Memset(Mp002_GetChrPtr(Mapper), 0xFF, ChrRamSize);
    }

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
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        u16 PhysAddr = Addr % Mapper->ChrMemSize;
        return Mp002_GetChrPtr(Mapper)[PhysAddr];
    }
    /* prg rom (variable bank) */
    else if (IN_RANGE(0x8000, Addr, 0xBFFF))
    {
        u32 PhysAddr = Addr % BANK_SIZE;
        u32 BaseAddr = Mapper->CurrentLowRomBank * BANK_SIZE;
        return Mp002_GetPrgRomPtr(Mapper)[BaseAddr + PhysAddr];
    }
    /* prg rom (last bank always) */
    else if (IN_RANGE(0xC000, Addr, 0xFFFF))
    {
        u32 PhysAddr = Addr % BANK_SIZE;
        u32 BaseAddr = Mapper->PrgRomSize - BANK_SIZE;
        return Mp002_GetPrgRomPtr(Mapper)[BaseAddr + PhysAddr];
    }

    return 0;
}

void NESMapper002_Write(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper002 *Mapper = MapperInterface;
    /* palette */
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        u16 PhysAddr = Addr % Mapper->ChrMemSize;
        Mp002_GetChrPtr(Mapper)[PhysAddr] = Byte;
    }
    /* bank select */
    else if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        u32 RomMask = 0x7;
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

