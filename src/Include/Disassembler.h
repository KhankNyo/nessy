#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "Common.h"


#define DISASM_NOT_ENOUGH_SPACE -1
/* returns 
 *     DISASM_NOT_ENOUGH_SPACE if Buffer was too small, 
 *     else index of the next instruction */
i32 DisassembleSingleOpcode(
    SmallString *OutDisassembledInstruction, 
    u16 PC, const u8 *Buffer, i32 BufferSizeBytes
);

#endif /* DISASSEMBLER_H */


#ifdef DISASSEMBLER_IMPLEMENTATION /* { */
#include <stdio.h>
i32 DisassembleSingleOpcode(
    SmallString *OutDisassembledInstruction, u16 PC, const u8 *BufferStart, i32 BufferSizeBytes)
{
#define READ_BYTE() \
    (Current + 1 > BufferEnd? \
        ((OutOfSpace = true), 0) \
        : *Current++\
    )
#define READ_WORD() \
    (Current + 2 > BufferEnd? \
        ((OutOfSpace = true), 0) \
        : (\
            (Current += 2), \
            (Current[-2] | ((u16)Current[-1] << 8))\
          )\
    )

#define FORMAT_OP(...) snprintf(OutDisassembledInstruction->Data, sizeof (SmallString), __VA_ARGS__)
#define ABS_OP(MnemonicString, IndexRegisterString) FORMAT_OP("%s $%04x%s", MnemonicString, READ_WORD(), IndexRegisterString)
#define ZPG_OP(MnemonicString, IndexRegisterString) FORMAT_OP("%s $%02x%s", MnemonicString, READ_BYTE(), IndexRegisterString)
#define IMM_OP(MnemonicString) FORMAT_OP("%s #$%02x", MnemonicString, READ_BYTE())
#define FORMAT_ADDRM(OpcodeByte, MnemonicString) do {\
    switch (BBB(OpcodeByte)) {\
    /* (ind,X) */   case 0: FORMAT_OP("%s ($%02x,X)",   MnemonicString, READ_BYTE()); break;\
    /* zpg */       case 1: ZPG_OP(MnemonicString, ""); break;\
    /* #imm */      case 2: IMM_OP(MnemonicString); break;\
    /* abs */       case 3: ABS_OP(MnemonicString, ""); break;\
    /* (ind),Y */   case 4: FORMAT_OP("%s ($%02x),Y",   MnemonicString, READ_BYTE()); break;\
    /* zpg,X */     case 5: ZPG_OP(MnemonicString, ",X"); break;\
    /* abs,Y */     case 6: ABS_OP(MnemonicString, ",Y"); break;\
    /* abs,X */     case 7: ABS_OP(MnemonicString, ",X"); break;\
    }\
} while (0)


    if (BufferSizeBytes < 1)
        return DISASM_NOT_ENOUGH_SPACE;

    Bool8 OutOfSpace = false;
    const u8 *Current = BufferStart;
    const u8 *BufferEnd = BufferStart + BufferSizeBytes;
    u8 Opcode = READ_BYTE();
    /* 
     * opcode: 0baaabbbcc
     * the switch handles corner-case instructions first,
     * and then the instructions that have patterns follow later 
     * */
    switch (Opcode)
    {
    case 0x00: FORMAT_OP("BRK"); break;
    case 0x4C: ABS_OP("JMP", ""); break;
    case 0x6C: FORMAT_OP("JMP ($%04x)", READ_WORD()); break;
    case 0x20: ABS_OP("JSR", ""); break;
    case 0x40: FORMAT_OP("RTI"); break;
    case 0x60: FORMAT_OP("RTS"); break;

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
    case 0x9E: ABS_OP("(i) SHX", ",Y"); break;
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
        uint bbb = BBB(Opcode);
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
            i8 ByteOffset = READ_BYTE();
            u16 Address = 0xFFFF & ((int32_t)PC + 2 + (int32_t)ByteOffset);
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
            uint aaa = AAA(Opcode);
            ZPG_OP(Mnemonic[aaa], 
                (aaa == 4 || aaa == 5)
                ? ",Y": ",X"
            );
        } break;
        case 7: /* absolute x, y */
        {
            uint aaa = AAA(Opcode);
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

    if (OutOfSpace)
    {
        return DISASM_NOT_ENOUGH_SPACE;
    }
    return Current - BufferStart;

#undef READ_BYTE
#undef READ_WORD
#undef IMM_OP
#undef ZPG_OP
#undef ABS_OP
#undef FORMAT_OP
#undef FORMAT_ADDRM
}


#   ifdef STANDALONE /* { */
#       undef STANDALONE
#       include <stdio.h>

/* maximum range that a 6502 can address */
static u8 sMemory[0x10000];
static u32 sMemorySize;

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <binary file>\n", argv[0]);
        return 1;
    }

    const char *FileName = argv[1];
    {
        FILE *MachineCode = fopen(FileName, "rb");
        if (NULL == MachineCode)
        {
            perror(FileName);
            return 1;
        }

        /* get file size */
        fseek(MachineCode, 0, SEEK_END);
        size_t FileSize = ftell(MachineCode);
        fseek(MachineCode, 0, SEEK_SET);

        /* file too large */
        if (FileSize > STATIC_ARRAY_SIZE(sMemory))
        {
            fprintf(stderr, "File must be inside of the 6502's addressable range.");
            return 1;
        }
        sMemorySize = FileSize;

        /* read into sMemory buffer */
        if (FileSize != fread(sMemory, 1, FileSize, MachineCode))
        {
            perror(FileName);
            fclose(MachineCode);
            return 1;
        }
        fclose(MachineCode);
    }


    SmallString Line;
    i32 CurrentInstructionOffset = 0;
    i32 CurrentMemorySize = sMemorySize;
    while (CurrentMemorySize > 0)
    {
        /* print the address */
        printf("%04x: ", CurrentInstructionOffset);

        const u8 *CurrentInstruction = &sMemory[CurrentInstructionOffset];
        i32 InstructionLength = DisassembleSingleOpcode(
            &Line, 
            CurrentInstructionOffset,
            CurrentInstruction, 
            CurrentMemorySize
        );
        if (DISASM_NOT_ENOUGH_SPACE == InstructionLength)
        {
            printf("Warning: Last instruction could not be disassembled.\n");
            break;
        }
        
        /* print the bytes */
        int ExpectedBytesCount = 4;
        for (int i = 0; i < InstructionLength; i++, ExpectedBytesCount--)
            printf("%02x ", *CurrentInstruction++);
        /* print padding space between the bytes and the instruction */
        if (ExpectedBytesCount)
            printf("%*s", ExpectedBytesCount*3, "");
        /* print the instruction */
        printf("%s\n", Line.Data);

        CurrentMemorySize -= InstructionLength;
        CurrentInstructionOffset += InstructionLength;
    }
    return 0;
}
#   endif /* STANDALONE */ /* } */
#endif /* DISASSEMBLER_IMPLEMENTATION */ /* } */

