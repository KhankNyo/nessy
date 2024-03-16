#ifndef NES_MAPPER_000_C
#define NES_MAPPER_000_C

#include <stdlib.h>
#include "MapperInterface.h"
#include "Utils.h"


typedef struct NESMapper000 
{
    u8 *PrgRom;
    u8 *ChrMem;
    u8 *PrgRam;
    u16 PrgRamSize;
    u16 PrgRomSize;
    u16 ChrMemSize;
    /* data after */
} NESMapper000;


NESMapperInterface *NESMapper000_Init(const void *PrgRom, isize PrgRomSize, const void *ChrRom, isize ChrRomSize)
{
    NESMapper000 *Mapper = NULL;
    if (ChrRomSize)
    {
        u8 *Buffer = malloc(sizeof(*Mapper) + PrgRomSize + ChrRomSize);
        Mapper = (NESMapper000 *)Buffer;
        DEBUG_ASSERT(Mapper);

        Mapper->PrgRamSize = 0;
        Mapper->PrgRomSize = PrgRomSize;
        Mapper->ChrMemSize = ChrRomSize;

        Mapper->PrgRom = Buffer + sizeof(*Mapper);
        Mapper->ChrMem = Mapper->PrgRom + PrgRomSize;
        Mapper->PrgRam = NULL;

        /* copy the prg and chr rom */
        Memcpy(Mapper->PrgRom, PrgRom, PrgRomSize);
        Memcpy(Mapper->ChrMem, ChrRom, ChrRomSize);
    }
    else /* no chr rom */
    {
        isize ChrRamSize = 8*1024;
        isize TotalSize = sizeof *Mapper + PrgRomSize + ChrRamSize;
        u8 *Buffer = malloc(TotalSize);
        Mapper = (NESMapper000 *)Buffer;
        DEBUG_ASSERT(Mapper);

        Mapper->PrgRamSize = 0;
        Mapper->PrgRomSize = PrgRomSize;
        Mapper->ChrMemSize = ChrRamSize;

        Mapper->PrgRom = Buffer + sizeof(*Mapper);
        Mapper->ChrMem = Mapper->PrgRom + PrgRomSize;
        Mapper->PrgRam = NULL;

        /* copy chr rom only, don't copy chr rom cuz it does not have one */
        Memcpy(Mapper->PrgRom, PrgRom, PrgRomSize);
        Memset(Mapper->ChrMem, 0, ChrRamSize);
    }
    return Mapper;
}

void NESMapper000_Reset(NESMapperInterface *Mapper)
{
    /* do nothing */
    (void)Mapper;
}

void NESMapper000_Destroy(NESMapperInterface *Mapper)
{
    DEBUG_ASSERT(Mapper != NULL);
    free(Mapper);
}



u8 NESMapper000_PPURead(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper000 *Mapper = MapperInterface;
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        u16 PhysAddr = Addr % Mapper->ChrMemSize;
        return Mapper->ChrMem[PhysAddr];
    }
    return 0;
}

u8 NESMapper000_CPURead(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper000 *Mapper = MapperInterface;
    /* prg ram */
    if (IN_RANGE(0x6000, Addr, 0x7FFF))
    {
        if (Mapper->PrgRamSize)
        {
            u16 PhysAddr = Addr % Mapper->PrgRamSize;
            return Mapper->PrgRam[PhysAddr];
        }
    }
    /* prg rom */
    else if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        u16 PhysAddr = Addr % Mapper->PrgRomSize;
        return Mapper->PrgRom[PhysAddr];
    }
    return 0;
}


void NESMapper000_PPUWrite(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper000 *Mapper = MapperInterface;
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        u16 PhysAddr = Addr % Mapper->ChrMemSize;
        Mapper->ChrMem[PhysAddr] = Byte;
    }
}

void NESMapper000_CPUWrite(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper000 *Mapper = MapperInterface;
    /* can write to prg ram */
    if (IN_RANGE(0x6000, Addr, 0x7FFF) 
    && Mapper->PrgRamSize)
    {
        u16 PhysAddr = Addr % Mapper->PrgRamSize;
        Mapper->PrgRam[PhysAddr] = Byte;
    }
}


u8 NESMapper000_DebugCPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper000_CPURead(Mapper, Addr);
}

u8 NESMapper000_DebugPPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper000_PPURead(Mapper, Addr);
}


#endif /* NES_MAPPER_000_C */

