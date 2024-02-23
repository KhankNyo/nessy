
#ifdef STANDALONE
#  undef STANDALONE
#endif /* STANDALONE */
#include "Common.h"
#include "6502.h"


#define NES_CPU_RAM_SIZE 0x0800


typedef struct NES 
{
    MC6502 *CPU;
    u8 Ram[NES_CPU_RAM_SIZE];
} NES;

static MC6502 sCpu;
static NES sNes = {
    .CPU = &sCpu,
};


void Nes_WriteByte(void *UserData, u16 Address, u8 Byte)
{
    NES *Nes = UserData;

    /* ram range */
    if (Address < 0x2000)
    {
        Address %= NES_CPU_RAM_SIZE;
    }
    /* IO registers */
    else if (IN_RANGE(0x2000, Address, 0x3FFF))
    {
        Address &= 0x07;
    }
    /* IO registers */
    else if (IN_RANGE(0x4000, Address, 0x401F))
    {
    }
    /* Expansion Rom */
    else if (IN_RANGE(0x4020, Address, 0x5FFF))
    {
    }
    /* Save Ram */
    else if (IN_RANGE(0x6000, Address, 0x7FFF))
    {
    }
    /* Cartridge ROM */
    else
    {
    }
}

u8 Nes_ReadByte(void *UserData, u16 Address)
{
    NES *Nes = UserData;

    u8 DataByte = 0;
    /* ram range */
    if (Address < 0x2000)
    {
        Address &= 0x07FF;
    }
    /* IO registers */
    else if (IN_RANGE(0x2000, Address, 0x3FFF))
    {
        Address &= 0x07;
    }
    /* IO registers */
    else if (IN_RANGE(0x4000, Address, 0x401F))
    {
    }
    /* Expansion Rom */
    else if (IN_RANGE(0x4020, Address, 0x5FFF))
    {
    }
    /* Save Ram */
    else if (IN_RANGE(0x6000, Address, 0x7FFF))
    {
    }
    /* Cartridge ROM */
    else
    {
    }

    return DataByte;
}


void Nes_OnEntry(void)
{
}

void Nes_OnLoop(void)
{
}

void Nes_AtExit(void)
{
}




