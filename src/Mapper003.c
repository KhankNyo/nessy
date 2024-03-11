#ifndef NES_MAPPER_003_C
#define NES_MAPPER_003_C

#include <stdlib.h>
#include "MapperInterface.h"
#include "Utils.h"


typedef struct NESMapper003 
{
    u32 PrgRomSize;
    u32 ChrMemSize;
    u32 CurrentChrBank;
    u32 ChrBankSize;
    Bool8 HasBusConflict;
    /* data after */
} NESMapper003;

static u8 *Mp003_GetPrgRomPtr(NESMapper003 *Mp003)
{
    u8 *BytePtr = ((u8*)Mp003) + sizeof *Mp003;
    return BytePtr;
}

static u8 *Mp003_GetChrPtr(NESMapper003 *Mp003)
{
    u8 *BytePtr = Mp003_GetPrgRomPtr(Mp003);
    return BytePtr + Mp003->PrgRomSize;
}



NESMapperInterface *NESMapper003_Init(const void *PrgRom, isize PrgRomSize, const void *ChrRom, isize ChrRomSize, Bool8 HasBusConflict)
{
    DEBUG_ASSERT(ChrRomSize);

    NESMapper003 *Mapper = NULL;
    isize TotalSize = sizeof *Mapper + PrgRomSize + ChrRomSize;
    Mapper = malloc(TotalSize);
    DEBUG_ASSERT(Mapper);

    Mapper->PrgRomSize = PrgRomSize;
    Mapper->ChrMemSize = ChrRomSize;
    Mapper->CurrentChrBank = 0;
    Mapper->ChrBankSize = 8*1024;
    Mapper->HasBusConflict = HasBusConflict;

    /* copy the prg and chr rom */
    u8 *MapperPrgRom = Mp003_GetPrgRomPtr(Mapper);
    u8 *MapperChrRom = Mp003_GetChrPtr(Mapper);
    Memcpy(MapperPrgRom, PrgRom, PrgRomSize);
    Memcpy(MapperChrRom, ChrRom, ChrRomSize);
    return Mapper;
}

void NESMapper003_Reset(NESMapperInterface *MapperInterface)
{
    NESMapper003 *Mapper = MapperInterface;
    Mapper->CurrentChrBank = 0;
    Mapper->ChrBankSize = 8 * 1024;
}

void NESMapper003_Destroy(NESMapperInterface *Mapper)
{
    DEBUG_ASSERT(Mapper != NULL);
    free(Mapper);
}



u8 NESMapper003_PPURead(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper003 *Mapper = MapperInterface;
    /* palette */
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        u32 PhysAddr = Addr % Mapper->ChrBankSize;
        u32 BaseAddr = Mapper->CurrentChrBank * (Mapper->ChrBankSize);
        u8 *ChrRom = Mp003_GetChrPtr(Mapper);
        return ChrRom[BaseAddr + PhysAddr];
    }
    return 0;
}

u8 NESMapper003_CPURead(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper003 *Mapper = MapperInterface;
    /* prg rom */
    if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        u16 PhysAddr = Addr % Mapper->PrgRomSize;
        u8 *PrgRom = Mp003_GetPrgRomPtr(Mapper);
        return PrgRom[PhysAddr];
    }
    return 0;
}

void NESMapper003_PPUWrite(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper003 *Mapper = MapperInterface;
    /* palette */
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        u32 PhysAddr = Addr % Mapper->ChrBankSize;
        u32 BaseAddr = Mapper->CurrentChrBank * Mapper->ChrBankSize;
        u8 *ChrRom = Mp003_GetChrPtr(Mapper);
        ChrRom[BaseAddr + PhysAddr] = Byte;
    }
}

void NESMapper003_CPUWrite(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper003 *Mapper = MapperInterface;
    /* prg rom (chr bank select) */
    if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        u8 ValueAtAddr = 0xFF;
        /* bus conflict, byte anded with value at the location */
        if (Mapper->HasBusConflict)
        {
            u16 PhysAddr = Addr % Mapper->PrgRomSize;
            u8 *PrgRom = Mp003_GetPrgRomPtr(Mapper);
            ValueAtAddr = PrgRom[PhysAddr];
        }
        /* lower 2 bits only */
        Mapper->CurrentChrBank = ValueAtAddr & Byte & 0x03;
    }
}

u8 NESMapper003_DebugCPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper003_CPURead(Mapper, Addr);
}

u8 NESMapper003_DebugPPURead(NESMapperInterface *Mapper, u16 Addr)
{
    return NESMapper003_PPURead(Mapper, Addr);
}


#endif /* NES_MAPPER_003_C */

