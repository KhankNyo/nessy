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
    NESMapper000 *Mapper000 = malloc(sizeof(NESMapper000));
    DEBUG_ASSERT(Mapper000);

    u8 *BytePtr = RomMemory;
    Mapper000->Base.MapperID = MapperID;
    Mapper000->Base.PrgRom = BytePtr;
    Mapper000->Base.PrgRomSize = PrgRomSize;
    Mapper000->Base.ChrRom = BytePtr + PrgRomSize;
    Mapper000->Base.ChrRomSize = ChrRomSize;

    Memcpy(Mapper000->Base.PrgRom, PrgRom, PrgRomSize);
    Memcpy(Mapper000->Base.ChrRom, ChrRom, PrgRomSize);
    return (NESMapperInterface *)Mapper000;
}

void NESMapperInterface_Destroy(NESMapperInterface *Mapper)
{
    free(Mapper);
}

u8 NESMapperInterface_Read(NESMapperInterface *Mapper, u16 Address)
{
    DEBUG_ASSERT(Mapper->MapperID == 0);
    if (Address < 0x3FFF) /* CHR ROM */
    {
        Address &= Mapper->ChrRomSize - 1;
        return Mapper->ChrRom[Address];
    }
    else /* PRG ROM */
    {
        Address &= Mapper->PrgRomSize - 1;
        return Mapper->PrgRom[Address];
    }
}

void NESMapperInterface_Write(NESMapperInterface *Mapper, u16 Address, u8 Byte)
{
    /* mapper 0 does not write to prg rom or chr rom  */
}


#endif /* MAPPER_INTERFACE_IMPLEMENTATION */

