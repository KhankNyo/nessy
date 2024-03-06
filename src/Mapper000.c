#ifndef NES_MAPPER_000_C

#include <stdlib.h>
#include "MapperInterface.h"
#include "Utils.h"


typedef struct NESMapper000 
{
    u16 PrgRamSize;
    u16 PrgRomSize;
    u16 ChrMemSize;
    /* data after */
} NESMapper000;


static u8 *Mp000_GetPrgRamPtr(NESMapper000 *Mp000)
{
    u8 *BytePtr = ((u8*)Mp000) + sizeof *Mp000;
    return BytePtr;
}

static u8 *Mp000_GetPrgRomPtr(NESMapper000 *Mp000)
{
    u8 *BytePtr = ((u8*)Mp000) + sizeof *Mp000;
    return BytePtr + Mp000->PrgRamSize;
}

static u8 *Mp000_GetChrPtr(NESMapper000 *Mp000)
{
    u8 *BytePtr = ((u8*)Mp000) + sizeof *Mp000;
    return BytePtr + Mp000->PrgRamSize + Mp000->PrgRomSize;
}


NESMapperInterface *NESMapper000_Init(const void *PrgRom, isize PrgRomSize, const void *ChrRom, isize ChrRomSize)
{
    NESMapper000 *Mapper = NULL;
    if (ChrRomSize)
    {
        isize TotalSize = sizeof *Mapper + PrgRomSize + ChrRomSize;
        Mapper = malloc(TotalSize);
        DEBUG_ASSERT(Mapper);

        Mapper->PrgRamSize = 0;
        Mapper->PrgRomSize = PrgRomSize;
        Mapper->ChrMemSize = ChrRomSize;

        /* copy the prg and chr rom */
        Memcpy(Mp000_GetPrgRomPtr(Mapper), PrgRom, PrgRomSize);
        Memcpy(Mp000_GetChrPtr(Mapper), ChrRom, ChrRomSize);
    }
    else /* no chr rom */
    {
        isize ChrRamSize = 8*1024;
        isize TotalSize = sizeof *Mapper + PrgRomSize + ChrRamSize;
        Mapper = malloc(TotalSize);
        DEBUG_ASSERT(Mapper);

        Mapper->PrgRamSize = 0;
        Mapper->PrgRomSize = PrgRomSize;
        Mapper->ChrMemSize = ChrRamSize;

        /* copy chr rom only, don't copy chr rom cuz it does not have one */
        Memcpy(Mp000_GetPrgRomPtr(Mapper), PrgRom, PrgRomSize);
        Memset(Mp000_GetChrPtr(Mapper), 0, ChrRamSize);
    }
    return Mapper;
}

void NESMapper000_Destroy(NESMapperInterface *Mapper)
{
    DEBUG_ASSERT(Mapper != NULL);
    free(Mapper);
}


u8 NESMapper000_Read(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper000 *Mapper = MapperInterface;
    if (IN_RANGE(0x0000, Addr, 0x2000))
    {
        u16 PhysAddr = Addr & (Mapper->ChrMemSize - 1);
        return Mp000_GetChrPtr(Mapper)[PhysAddr];
    }
    else if (IN_RANGE(0x6000, Addr, 0x7FFF))
    {
        if (Mapper->PrgRamSize)
        {
            Addr %= Mapper->PrgRamSize;
            return Mp000_GetPrgRamPtr(Mapper)[Addr];
        }
    }
    else if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        u16 PhysAddr = Addr & (Mapper->PrgRomSize - 1);
        return Mp000_GetPrgRomPtr(Mapper)[PhysAddr];
    }

    return 0;
}

void NESMapper000_Write(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper000 *Mapper = MapperInterface;
    if (IN_RANGE(0x0000, Addr, 0x2000))
    {
        u16 PhysAddr = Addr & (Mapper->ChrMemSize - 1);
        Mp000_GetChrPtr(Mapper)[PhysAddr] = Byte;
    }
    else if (IN_RANGE(0x6000, Addr, 0x7FFF))
    {
        if (Mapper->PrgRamSize)
        {
            Addr &= Mapper->PrgRamSize - 1;
            Mp000_GetPrgRamPtr(Mapper)[Addr] = Byte;
        }
    }
    else if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        /* cannot write to rom */
    }
}

u8 NESMapper000_DebugCPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper000_Read(Mapper, Addr);
}

u8 NESMapper000_DebugPPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper000_Read(Mapper, Addr);
}


#endif /* NES_MAPPER_000_C */

