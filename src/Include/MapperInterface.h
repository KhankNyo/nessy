#ifndef MAPPER_INTERFACE_H
#define MAPPER_INTERFACE_H

#include "Common.h"
typedef enum NESMapperID 
{
    NES_MAPPER_000,
    NES_MAPPER_002 = 2,
    NES_MAPPER_003 = 3,
} NESMapperID; 
typedef void NESMapperInterface;
typedef NESMapperInterface (*NESMapper_InitFn)(const void *PrgRom, isize PrgRomSize, const void *ChrRom, isize ChrRomSize);
typedef u8 (*NESMapper_ReadFn)(NESMapperInterface *, u16 Addr);
typedef u8 (*NESMapper_DebugCPUReadFn)(NESMapperInterface *, u16 Addr);
typedef u8 (*NESMapper_DebugPPUReadFn)(NESMapperInterface *, u16 Addr);
typedef void (*NESMapper_DebugWriteFn)(NESMapperInterface *, u16 Addr, u8 Byte);

#endif /* MAPPER_INTERFACE_H */

#ifdef MAPPER_INTERFACE_IMPL
#undef MAPPER_INTERFACE_IMPL
#   include "../Mapper000.c"
#   include "../Mapper002.c"
#   include "../Mapper003.c"
#endif /* MAPPER_INTERFACE_IMPL */


