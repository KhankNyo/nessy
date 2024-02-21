#ifndef MC6502_H
#define MC6502_H


#include "Common.h"

typedef struct MC6502 MC6502;
typedef void (*MC6502WriteByte)(MC6502 *, u16 Address, u8 Byte);
typedef u8 (*MC6502ReadByte)(MC6502 *, u16 Address);

struct MC6502 
{
    u8 A, X, Y;
    u8 SP;
    u8 Flags;
    u8 Opcode;
    u16 PC;

    void *UserData;
    MC6502ReadByte ReadByte;
    MC6502WriteByte WriteByte;
    uint CyclesLeft;
    Bool8 Halt;
};

MC6502 MC6502Init(u16 PC, void *UserData, MC6502ReadByte ReadFn, MC6502WriteByte WriteFn);
void MC6502StepClock(MC6502 *This);

#endif /* MC6502_H */



#ifdef MC6502_IMPLEMENTATION
MC6502 MC6502Init(u16 PC, void *UserData, MC6502ReadByte ReadFn, MC6502WriteByte WriteFn)
{
    MC6502 This = {
        .PC = PC,
        .UserData = UserData,
        .ReadByte = ReadFn,
        .WriteByte = WriteFn,
        .Halt = false,
        .CyclesLeft = 0,
    };
    return This;
}

static u8 FetchByte(MC6502 *This)
{
    return This->ReadByte(This, This->PC++);
}

static u16 FetchWord(MC6502 *This)
{
    u16 Word = FetchByte(This);
    Word |= (u16)FetchByte(This) << 8;
    return Word;
}

static u8 PopByte(MC6502 *This)
{
    return This->ReadByte(This, This->SP++ + 0x100);
}

static u16 PopWord(MC6502 *This)
{
    u16 Word = PopByte(This);
    Word |= (u16)PopByte(This) << 8;
    return Word;
}

static void PushByte(MC6502 *This, u8 Byte)
{
    This->WriteByte(This, This->SP + 0x100, Byte);
}


void MC6502StepClock(MC6502 *This)
{
    if (This->Halt || This->CyclesLeft-- > 0)
        return;

    u8 Opcode = FetchByte(This);
    This->Opcode = Opcode;
    /* 
     * opcode: 0baaabbbcc
     * the switch handles corner-case instructions first,
     * and then the instructions that have patterns follow later 
     * */
    uint CyclesLeft = 0;
    switch (Opcode)
    {
    case 0x00: FORMAT_OP("BRK"); break;
    case 0x4C: /* JMP abs */ 
    {
        This->PC = FetchWord(This);
        CyclesLeft = 3;
    } break;
    case 0x6C: /* JMP (ind) */
    {
        u16 AddressPointer = FetchWord(This);
        u16 Address = This->ReadByte(This, AddressPointer);

        /* simulate hardware bug */
        AddressPointer = 
            (AddressPointer & 0xFF00) 
            | (0x00FF & (AddressPointer + 1));
        Address |= (u16)This->ReadByte(This, AddressPointer) << 8;

        This->PC = Address;
        CyclesLeft = 5;
    } break;
    case 0x20: /* JSR */
    {
        /* NOTE: this must be done in this order, 
         * RTI, RTS will pop and add 1 to PC */

        u16 SubroutineAddress = FetchByte(This);
        /* now PC is pointing at the MSB of the address */
        PushByte(This, This->PC >> 8);
        PushByte(This, This->PC & 0xFF);
        SubroutineAddress |= (u16)FetchByte(This) << 8;

        This->PC = SubroutineAddress;
    } break;
    case 0x40: /* RTI */ 
    {
    } break;
    case 0x60: /* RTS */
    {
        u16 ReturnAddress = PopWord(This);
        This->PC = ReturnAddress + 1;
    } break;

    /* bit */
    case 0x24: ZPG_OP("BIT", ""); break;
    case 0x2C: ABS_OP("BIT", ""); break;

    /* stack */
    case 0x08: FORMAT_OP("PHP"); break;
    case 0x28: FORMAT_OP("PLP"); break;
    case 0x48: FORMAT_OP("PHA"); break;
    case 0x68: FORMAT_OP("PLA"); break;

    /* by 1 instructions on x and y */
    case 0x88: FORMAT_OP("DEY"); break;
    case 0xCA: FORMAT_OP("DEX"); break;
    case 0xC8: FORMAT_OP("INY"); break;
    case 0xE8: FORMAT_OP("INX"); break;

    /* oddball ldy */
    case 0xBC: ABS_OP("LDY", ",X"); break;

    /* group cc = 0 immediate instructions */
    case 0xA0: IMM_OP("LDY"); break;
    case 0xA2: IMM_OP("LDX"); break;
    case 0xC0: IMM_OP("CPY"); break;
    case 0xE0: IMM_OP("CPX"); break;

    /* transfers */
    case 0xAA: FORMAT_OP("TAX"); break;
    case 0xA8: FORMAT_OP("TAY"); break;
    case 0xBA: FORMAT_OP("TSX"); break;
    case 0x9A: FORMAT_OP("TXS"); break;
    case 0x8A: FORMAT_OP("TXA"); break;
    case 0x98: FORMAT_OP("TYA"); break;

    /* official nop */
    case 0xEA: FORMAT_OP("NOP"); break;

    /* group cc = 0 nops */
    case 0x80: IMM_OP("(i) NOP"); break;
    case 0x04:
    case 0x44:
    case 0x64:
    case 0x0C:
    case 0x14:
    case 0x34:
    case 0x54:
    case 0x74:
    case 0xD4:
    case 0xF4:
    case 0x1C:
    case 0x3C:
    case 0x5C:
    case 0x7C:
    case 0xDC:
    case 0xFC: FORMAT_ADDRM(Opcode, "(i) NOP"); break;
    case 0x9C: ABS_OP("(i) SHY", ",X"); break;
    /* group cc = 1 nops */
    case 0x89: IMM_OP("(i) NOP"); break;
    /* group cc = 2 illegal */
    case 0x9E: ABS_OP("(i) SHX", ",Y");
    /* group cc = 3 oddball illegals */
    case 0x0B:
    case 0x2B: IMM_OP("(i) ANC"); break;
    case 0x4B: IMM_OP("(i) ALR"); break;
    case 0x6B: IMM_OP("(i) ARR"); break;
    case 0x8B: IMM_OP("(i) ANE"); break;
    case 0xAB: IMM_OP("(i) LXA"); break;
    case 0xCB: IMM_OP("(i) SBX"); break;
    case 0xEB: IMM_OP("(i) USBC"); break;
    case 0x9B: ABS_OP("(i) TAS", ",Y"); break;
    case 0xBB: ABS_OP("(i) LAS", ",Y"); break;
    case 0x9F: ABS_OP("(i) SHA", ",Y"); break;



    default: switch (CC(Opcode))
    {
    case 0: /* load/store y, compare y, x; branch and set/clear flags */
    {
        unsigned bbb = BBB(Opcode);
        if (bbb == 6) /* clear/set flags */
        {
            static const char Mnemonic[8][4] = {
                "CLC", "SEC", "CLI", "SEI", 
                "TYA" /* this is unreachable, but it doesn't really matter since it's an implied instruction */, 
                "CLV", "CLD", "SED"
            };
            /* stupid string format warning even though it's literally unreachable */
            FORMAT_OP("%s", Mnemonic[AAA(Opcode)]);
        }
        else if (bbb == 4) /* branch */
        {
            static const char Mnemonic[8][4] = {
                "BPL", "BMI", "BVC", "BVS", 
                "BCC", "BCS", "BNE", "BEQ"
            };
            int8_t ByteOffset = READ_BYTE();
            uint16_t Address = 0xFFFF & ((int32_t)PC + 2 + (int32_t)ByteOffset);
            FORMAT_OP("%s $%04x   # %d", 
                Mnemonic[AAA(Opcode)], Address, ByteOffset
            );
        }
        else /* STY, LDY, CPY, CPX */
        {
            static const char Mnemonic[8][4] = {
                "???", "???", "???", "???",
                "STY", "LDY", "CPX", "CPY"
            };
            FORMAT_ADDRM(Opcode, Mnemonic[AAA(Opcode)]);
        }
    } break;
    case 1: /* accumulator instructions */
    {
        static const char Mnemonic[8][4] = {
            "ORA", "AND", "EOR", "ADC",
            "STA", "LDA", "CMP", "SBC",
        };
        FORMAT_ADDRM(Opcode, Mnemonic[AAA(Opcode)]);
    } break;
    case 2: /* shift and X-transfer instructions */
    {
        static const char Mnemonic[8][4] = {
            "ASL", "ROL", "LSR", "ROR", 
            "STX", "LDX", "DEC", "INC"
        };
        switch (BBB(Opcode))
        {
        case 1: ZPG_OP(Mnemonic[AAA(Opcode)], ""); break;
        case 3: ABS_OP(Mnemonic[AAA(Opcode)], ""); break;
        case 4: FORMAT_OP("(i) JAM"); break;
        case 6: FORMAT_OP("(i) NOP"); break;

        /* accumulator mode, 
         * TXA, TAX, DEX, NOP are already handled */
        case 2: FORMAT_OP("%s A", Mnemonic[AAA(Opcode)]); break; 
        case 0: /* LDX #imm is the only valid instruction, but it's already handled */
        {
            if (AAA(Opcode) < 4)
                FORMAT_OP("(i) JAM");
            else IMM_OP("(i) NOP");
        } break;
        case 5: /* zero page x, y */
        {
            unsigned aaa = AAA(Opcode);
            ZPG_OP(Mnemonic[aaa], 
                (aaa == 4 || aaa == 5)
                ? ",Y": ",X"
            );
        } break;
        case 7: /* absolute x, y */
        {
            unsigned aaa = AAA(Opcode);
            ABS_OP(Mnemonic[aaa],
                (aaa == 4 || aaa == 5)
                ? ",Y": ",X"
            );
        } break;
        }
    } break;
    case 3: /* illegal instructions */
    {
        /* oddballs are handled before */
        static const char Mnemonic[8][4] = {
            "SLO", "RLA", "SRE", "RRA",
            "SAX", "LAX", "DCP", "ISC"
        };
        FORMAT_ADDRM(Opcode, Mnemonic[AAA(Opcode)]);
    } break;
    } break;
    }
}
#ifdef STANDALONE
#   include <stdio.h>
#   undef STANDALONE
#   define DISASSEMBLER_IMPLEMENTATION
#       include "Disassembler.h"
#   undef DISASSEMBLER_IMPLEMENTATION

static u8 sMemory[0x10000];

static u8 ReadFn(MC6502 *This, u16 Address)
{
    (void)This;
    return sMemory[Address];
}

static void WriteFn(MC6502 *This, u16 Address, u8 Byte)
{
    (void)This;
    sMemory[Address] = Byte;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <binary file>\n", argv[0]);
        return 0;
    }

    {
        /* open file */
        const char *FileName = argv[1];
        FILE *f = fopen(FileName, "rb");
        if (NULL == f)
        {
            perror(FileName);
            return 1;
        }

        /* find file name */
        fseek(f, 0, SEEK_END);
        size_t FileSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (FileSize > sizeof sMemory)
        {
            fprintf(stderr, "File must be smaller than the 6502's addressable range.");
            return 1;
        }

        /* read into mem */
        if (FileSize != fread(sMemory, 1, sizeof sMemory, f))
        {
            perror(FileName);
            return 1;
        }
        fclose(f);
    }

    MC6502 Cpu = MC6502Init(0, NULL, ReadFn, WriteFn);
    while (1)
    {
        MC6502StepClock(&Cpu);
    }
    return 0;
}

#  endif /* STANDALONE */
#endif /* MC6502_IMPLEMENTATION */

