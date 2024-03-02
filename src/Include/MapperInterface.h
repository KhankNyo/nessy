#ifndef MAPPER_INTERFACE_H
#define MAPPER_INTERFACE_H

#include "Common.h"

typedef struct NESMapperInterface 
{
    u8 MapperID;
    u8 *PrgRom;
    isize PrgRomSize;
    u8 *ChrRom;
    isize ChrRomSize;
} NESMapperInterface;

NESMapperInterface *NESMapperInterface_Init(
    u8 MapperID, 
    void *RomMemory, 
    const void *PrgRom, isize PrgRomSize,
    const void *ChrRom, isize ChrRomSize
);
void NESMapperInterface_Destroy(NESMapperInterface *Mapper);
u8 NESMapperInterface_Read(NESMapperInterface *Mapper, u16 Address);
void NESMapperInterface_Write(NESMapperInterface *Mapper, u16 Address, u8 Byte);

u8 NESMapperInterface_DebugCPURead(NESMapperInterface *Mapper, u16 Address);
u8 NESMapperInterface_DebugPPURead(NESMapperInterface *Mapper, u16 Address);

#endif /* MAPPER_INTERFACE_H */


#ifdef MAPPER_INTERFACE_IMPLEMENTATION
#include <stdlib.h>
#include "Utils.h"

typedef struct NESMapper000 
{
    NESMapperInterface Base;
} NESMapper000;

NESMapperInterface *NESMapperInterface_Init(
    u8 MapperID, 
    void *RomMemory, 
    const void *PrgRom, isize PrgRomSize,
    const void *ChrRom, isize ChrRomSize
)
{
    if (MapperID != 0)
        return NULL;

    /* mapper 0 only */
    NESMapper000 *Mapper000 = malloc(sizeof(NESMapper000));
    DEBUG_ASSERT(Mapper000);

    u8 *BytePtr = RomMemory;
    Mapper000->Base.MapperID = MapperID;
    Mapper000->Base.PrgRom = BytePtr;
    Mapper000->Base.PrgRomSize = PrgRomSize;
    Memcpy(Mapper000->Base.PrgRom, PrgRom, PrgRomSize);

    Mapper000->Base.ChrRom = BytePtr + PrgRomSize;
    Mapper000->Base.ChrRomSize = ChrRomSize;
    if (NULL != ChrRom)
    {
        Memcpy(Mapper000->Base.ChrRom, ChrRom, ChrRomSize);
    }
    else
    {
        Memset(Mapper000->Base.ChrRom, 0, ChrRomSize);
    }
    return (NESMapperInterface *)Mapper000;
}

void NESMapperInterface_Destroy(NESMapperInterface *Mapper)
{
    free(Mapper);
}

u8 NESMapperInterface_Read(NESMapperInterface *Mapper, u16 Address)
{
    DEBUG_ASSERT(Mapper->MapperID == 0);
    if (Address < 0x7FFF) /* CHR ROM */
    {
        Address %= Mapper->ChrRomSize;
        return Mapper->ChrRom[Address];
    }
    else /* PRG ROM */
    {
        Address %= Mapper->PrgRomSize;
        return Mapper->PrgRom[Address];
    }
}

void NESMapperInterface_Write(NESMapperInterface *Mapper, u16 Address, u8 Byte)
{
    /* mapper 0 does not write to prg rom or chr rom  */
}


u8 NESMapperInterface_DebugCPURead(NESMapperInterface *Mapper, u16 Address)
{
    DEBUG_ASSERT(Mapper->MapperID == 0);
    Address %= Mapper->PrgRomSize;
    return Mapper->PrgRom[Address];
}

u8 NESMapperInterface_DebugPPURead(NESMapperInterface *Mapper, u16 Address)
{
    DEBUG_ASSERT(Mapper->MapperID == 0);
    Address %= Mapper->PrgRomSize;
    return Mapper->PrgRom[Address];
}


#endif /* MAPPER_INTERFACE_IMPLEMENTATION */

