#ifndef DISASSEMBLER_C
#define DISASSEMBLER_C
#include "Utils.h"
#include "Common.h"

typedef u8 (*DisassemblerReadFn)(void *UserData, u16 VirtualPC);
/* returns the size of the instruction (negative if wrapping) */
i32 DisassembleSingleOpcode( 
    SmallString *OutDisassembledInstruction, u16 VirtualPC, void *UserData, DisassemblerReadFn ProvideByte
)
{
#define READ_BYTE() ProvideByte(UserData, VirtualPC++)
    u8 PlaceHolder; /* to prevent the compiler from reordering (if it does) */
#define READ_WORD() \
    ((PlaceHolder = READ_BYTE()), \
      PlaceHolder | ((u16)READ_BYTE() << 8))

#define APPEND(DataType, Index, ...) Append##DataType \
    (OutDisassembledInstruction->Data, sizeof(SmallString), Index, __VA_ARGS__)
#define FMT_OP(MnemonicString, ArgOpenStr, ArgType, Arg, ArgCloseStr) do {\
    const char *Mne_ = MnemonicString;\
    int Len_ = APPEND(String, 0, Mne_);\
    Len_ = APPEND(String, Len_, ArgOpenStr);\
    Len_ = APPEND(Hex, Len_, sizeof(ArgType)*2, Arg);\
    if (0 != sizeof ArgCloseStr)\
        APPEND(String, Len_, ArgCloseStr);\
} while (0) 
#define ABS_OP(MnemonicString, IndexRegisterString) FMT_OP(MnemonicString, " $", u16, READ_WORD(), IndexRegisterString)
#define ZPG_OP(MnemonicString, IndexRegisterString) FMT_OP(MnemonicString, " $", u8,  READ_BYTE(), IndexRegisterString)
#define IND_OP(MnemonicString, IndexRegisterString) FMT_OP(MnemonicString, " #", u8,  READ_BYTE(), IndexRegisterString)
#define IMM_OP(MnemonicString)                      FMT_OP(MnemonicString, " #$",u8,  READ_BYTE(), "")
#define FORMAT_ADDRM(OpcodeByte, MnemonicString) do {\
    switch (BBB(OpcodeByte)) {\
    /* (ind,X) */   case 0: FMT_OP(MnemonicString, " ($", u8,  READ_BYTE(), ",X)"); break;\
    /* zpg */       case 1: FMT_OP(MnemonicString, " $",  u8,  READ_BYTE(), ""); break;\
    /* #imm */      case 2: FMT_OP(MnemonicString, " #$", u8,  READ_BYTE(), ""); break;\
    /* abs */       case 3: FMT_OP(MnemonicString, " $",  u16, READ_WORD(), ""); break;\
    /* (ind),Y */   case 4: FMT_OP(MnemonicString, " ($", u8,  READ_BYTE(), "),Y"); break;\
    /* zpg,X */     case 5: FMT_OP(MnemonicString, " $",  u8,  READ_BYTE(), ",X"); break;\
    /* abs,Y */     case 6: FMT_OP(MnemonicString, " $",  u16, READ_WORD(), ",Y"); break;\
    /* abs,X */     case 7: FMT_OP(MnemonicString, " $",  u16, READ_WORD(), ",X"); break;\
    }\
} while (0)
    u16 PCStart = VirtualPC;
    u8 Opcode = READ_BYTE();
    /* 
     * opcode: 0baaabbbcc
     * the switch handles corner-case instructions first,
     * and then the instructions that have patterns follow later 
     * */
    switch (Opcode)
    {
    case 0x00: IMM_OP("BRK"); break;
    case 0x4C: ABS_OP("JMP", ""); break;
    case 0x6C: FMT_OP("JMP", " ($", u16, READ_WORD(), ")"); break;
    case 0x20: ABS_OP("JSR", ""); break;
    case 0x40: APPEND(String, 0, "RTI"); break;
    case 0x60: APPEND(String, 0, "RTS"); break;

    /* bit */
    case 0x24: ZPG_OP("BIT", ""); break;
    case 0x2C: ABS_OP("BIT", ""); break;

    /* stack */
    case 0x08: APPEND(String, 0, "PHP"); break;
    case 0x28: APPEND(String, 0, "PLP"); break;
    case 0x48: APPEND(String, 0, "PHA"); break;
    case 0x68: APPEND(String, 0, "PLA"); break;

    /* by 1 instructions on x and y */
    case 0x88: APPEND(String, 0, "DEY"); break;
    case 0xCA: APPEND(String, 0, "DEX"); break;
    case 0xC8: APPEND(String, 0, "INY"); break;
    case 0xE8: APPEND(String, 0, "INX"); break;

    /* oddball ldy */
    case 0xBC: ABS_OP("LDY", ",X"); break;

    /* group cc = 0 immediate instructions */
    case 0xA0: IMM_OP("LDY"); break;
    case 0xA2: IMM_OP("LDX"); break;
    case 0xC0: IMM_OP("CPY"); break;
    case 0xE0: IMM_OP("CPX"); break;

    /* transfers */
    case 0xAA: APPEND(String, 0, "TAX"); break;
    case 0xA8: APPEND(String, 0, "TAY"); break;
    case 0xBA: APPEND(String, 0, "TSX"); break;
    case 0x9A: APPEND(String, 0, "TXS"); break;
    case 0x8A: APPEND(String, 0, "TXA"); break;
    case 0x98: APPEND(String, 0, "TYA"); break;

    /* official nop */
    case 0xEA: APPEND(String, 0, "NOP"); break;

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
            APPEND(String, 0, Mnemonic[AAA(Opcode)]);
        }
        else if (bbb == 4) /* branch */
        {
            static const char Mnemonic[8][4] = {
                "BPL", "BMI", "BVC", "BVS", 
                "BCC", "BCS", "BNE", "BEQ"
            };
            i8 ByteOffset = READ_BYTE();
            u16 Address = 0xFFFF & ((int32_t)VirtualPC + 2 + (int32_t)ByteOffset);
            FMT_OP(Mnemonic[AAA(Opcode)], " $", u16, Address, "");
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
        case 4: APPEND(String, 0, "(i) JAM"); break;
        case 6: APPEND(String, 0, "(i) NOP"); break;

        /* accumulator mode, 
         * TXA, TAX, DEX, NOP are already handled */
        case 2: 
        {
            int Len = APPEND(String, 0, Mnemonic[AAA(Opcode)]);
            APPEND(String, Len, " A");
        } break;
        case 0: /* LDX #imm is the only valid instruction, but it's already handled */
        {
            if (AAA(Opcode) < 4)
                APPEND(String, 0, "(i) JAM");
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

    u32 InstructionSize = VirtualPC - PCStart;
    return InstructionSize;
#undef READ_BYTE
#undef READ_WORD
#undef IMM_OP
#undef IND_OP
#undef ZPG_OP
#undef ABS_OP
#undef FMT_OP
#undef APPEND
#undef FORMAT_ADDRM
}


#ifdef STANDALONE 
#undef STANDALONE
#include <stdio.h>

/* maximum range that a 6502 can address */
static u8 sMemory[0x10000];
static u32 sMemorySize = sizeof sMemory;

static Bool8 ReadFileIntoMemory(const char *FileName)
{
    FILE *MachineCode = fopen(FileName, "rb");
    if (NULL == MachineCode)
    {
        perror(FileName);
        goto Fail;
    }

    /* get file size */
    fseek(MachineCode, 0, SEEK_END);
    size_t FileSize = ftell(MachineCode);
    fseek(MachineCode, 0, SEEK_SET);

    /* file too large */
    if (FileSize > STATIC_ARRAY_SIZE(sMemory))
    {
        fprintf(stderr, "File must be inside of the 6502's addressable range.");
        goto Fail;
    }
    sMemorySize = FileSize;

    /* read into sMemory buffer */
    if (FileSize != fread(sMemory, 1, FileSize, MachineCode))
    {
        perror(FileName);
        goto Fail;
    }

    fclose(MachineCode);
    return true;
Fail: 
    if (MachineCode)
        fclose(MachineCode);
    return false;
}

static u8 DisRead(void *UserData, u16 VirtualPC)
{
    (void)UserData;
    return sMemory[VirtualPC];
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <binary file>\n", argv[0]);
        return 1;
    }

    const char *FileName = argv[1];
    if (!ReadFileIntoMemory(FileName))
        return 1;

    SmallString InstructionStr;
    i32 CurrentInstructionOffset = 0;
    i32 CurrentMemorySize = sMemorySize;
    char Line[256];
    char *LineSoFar;
    int LineLen = 0;
#define APPEND_LINE(...) do {\
    int Remaining = sizeof(Line) - LineLen;\
    LineLen += snprintf(LineSoFar, Remaining, __VA_ARGS__);\
    LineSoFar = &Line[0] + LineLen;\
} while (0) 
    while (CurrentMemorySize > 0)
    {
        LineSoFar = Line;
        LineLen = 0;

        /* print the address */
        APPEND_LINE("%04X: ", CurrentInstructionOffset);

        u8 *CurrentInstruction = &sMemory[CurrentInstructionOffset];
        i32 InstructionLength = DisassembleSingleOpcode(
            &InstructionStr, 
            CurrentInstructionOffset,
            NULL,
            DisRead
        );
        if (InstructionLength <= 0)
        {
            printf("Warning: could not fully display the content of instruction at $%04x\n", CurrentInstructionOffset);
            break;
        }
        
        /* print the bytes */
        int ExpectedBytesCount = 4;
        for (int i = 0; i < InstructionLength; i++, ExpectedBytesCount--)
            APPEND_LINE("%02X ", *CurrentInstruction++);
        /* print padding space between the bytes and the instruction */
        if (ExpectedBytesCount)
            APPEND_LINE("%*s", ExpectedBytesCount*3, "");
        /* print the instruction */
        APPEND_LINE("%s", InstructionStr.Data);

        /* line is buffered because printf is extremely slow on Windows, 
         * took around 20s to dump 65k bytes */
        puts(Line);

        CurrentMemorySize -= InstructionLength;
        CurrentInstructionOffset += InstructionLength;
    }
#undef APPEND_LINE
    return 0;
}
#endif /* STANDALONE */ 
#endif /* DISASSEMBLER_C */
