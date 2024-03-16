#ifndef NES_MAPPER_003_C
#define NES_MAPPER_003_C

#include <stdlib.h>
#include "MapperInterface.h"
#include "Utils.h"


typedef struct NESMapper003 
{
    u8 *PrgRom;
    u32 PrgRomSize;

    u32 ChrRomSize;
    u8 *ChrRom;
    u8 *CurrentChrBank;
    Bool8 HasBusConflict;
    /* data after */
} NESMapper003;


NESMapperInterface *NESMapper003_Init(const void *PrgRom, isize PrgRomSize, const void *ChrRom, isize ChrRomSize, Bool8 HasBusConflict)
{
    DEBUG_ASSERT(ChrRomSize);
    NESMapper003 *Mapper = NULL;
    u8 *Ptr = malloc(sizeof(*Mapper) + PrgRomSize + ChrRomSize);
    DEBUG_ASSERT(Ptr);

    Mapper = (NESMapper003 *)Ptr;
    Mapper->HasBusConflict = HasBusConflict;

    Mapper->PrgRom = Ptr + sizeof(*Mapper);
    Mapper->PrgRomSize = PrgRomSize;
    Memcpy(Mapper->PrgRom, PrgRom, PrgRomSize);

    Mapper->ChrRom = Mapper->PrgRom + PrgRomSize;
    Mapper->CurrentChrBank = Mapper->ChrRom;
    Mapper->ChrRomSize = ChrRomSize;
    if (ChrRom)
        Memcpy(Mapper->ChrRom, ChrRom, ChrRomSize);
    else Memset(Mapper->ChrRom, 0, ChrRomSize);
    return Mapper;
}

void NESMapper003_Reset(NESMapperInterface *MapperInterface)
{
    NESMapper003 *Mapper = MapperInterface;
    Mapper->CurrentChrBank = Mapper->ChrRom;
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
        return Mapper->CurrentChrBank[Addr];
    }
    return 0;
}

u8 NESMapper003_CPURead(NESMapperInterface *MapperInterface, u16 Addr)
{
    NESMapper003 *Mapper = MapperInterface;
    /* prg rom */
    if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        return Mapper->PrgRom[Addr & (Mapper->PrgRomSize - 1)];
    }
    return 0;
}

void NESMapper003_PPUWrite(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper003 *Mapper = MapperInterface;
    /* palette */
    if (IN_RANGE(0x0000, Addr, 0x1FFF))
    {
        Mapper->CurrentChrBank[Addr] = Byte;
    }
}

void NESMapper003_CPUWrite(NESMapperInterface *MapperInterface, u16 Addr, u8 Byte)
{
    NESMapper003 *Mapper = MapperInterface;
    /* prg rom (chr bank select) */
    if (IN_RANGE(0x8000, Addr, 0xFFFF))
    {
        if (Mapper->HasBusConflict)
            Byte &= Mapper->PrgRom[Addr & (Mapper->PrgRomSize - 1)];
        Mapper->CurrentChrBank = Mapper->ChrRom + (Byte & 0x03)*0x2000;
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

