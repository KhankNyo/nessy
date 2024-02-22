#ifndef MC6502_H
#define MC6502_H


#include "Common.h"

typedef struct MC6502 MC6502;
typedef void (*MC6502WriteByte)(MC6502 *, u16 Address, u8 Byte);
typedef u8 (*MC6502ReadByte)(MC6502 *, u16 Address);
typedef enum MC6502Flags 
{
    /* upper 8 bits: size, lower 8 bits: mask */
    FLAG_N = 0x0780,
    FLAG_V = 0x0640,
    FLAG_UNUSED = 0x0520,
    FLAG_B = 0x0410,
    FLAG_D = 0x0308,
    FLAG_I = 0x0204,
    FLAG_Z = 0x0102,
    FLAG_C = 0x0001,
} MC6502Flags;

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
#  define SET_FLAG(fl, BooleanValue) \
    (This->Flags = \
        (This->Flags & ~(fl & 0xFF)) \
        | (((BooleanValue) != 0) \
            << (fl >> 8)\
          )\
    )
#  define GET_FLAG(fl) ((This->Flags >> (fl >> 8)) & 0x1)
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

static void PushWord(MC6502 *This, u16 Word)
{
    PushByte(This, Word >> 8);
    PushByte(This, Word & 0xFF);
}

static void PushFlags(MC6502 *This)
{
    PushByte(This, This->Flags | FLAG_B | FLAG_UNUSED);
}

static void PopFlags(MC6502 *This)
{
    This->Flags = PopByte(This);
}

static void SetAdditionFlags(MC6502 *This, u8 a, u8 b, u16 Result)
{
    SET_FLAG(FLAG_N, (Result >> 7) & 0x1);
    SET_FLAG(FLAG_Z, (Result & 0xFF) == 0);
    SET_FLAG(FLAG_C, Result > 0xFF);

    /* get sign for v flag */
    a >>= 7;
    b >>= 7;
    Result = (Result >> 7) & 0x1;
    SET_FLAG(FLAG_V, 
        (a == b) && (a != Result)
    );
}

/* Opcode: 0b aaa bbb cc
 *                ^^^ ^^ */
static u16 GetEffectiveAddress(MC6502 *This, u16 Opcode)
{
    uint aaa = AAA(Opcode);
    uint bbb = BBB(Opcode);
    uint cc  = CC(Opcode);
    Bool8 UseRegY = cc >= 2 && (aaa == 4 || aaa == 5);
    switch (bbb)
    {
    case 0: /* (ind,X) */
    {
        u8 AddressPointer = (u8)(FetchByte(This) + This->X);
        u16 Address = This->ReadByte(This, (u8)AddressPointer++);
        Address |= (u16)This->ReadByte(This, AddressPointer) << 8;

        This->CyclesLeft += 4;
        return Address;
    } break;
    case 1: /* zpg */
    {
        This->CyclesLeft += 1;
        return FetchByte(This);
    } break;
    case 2: /* #imm */
    {
        return This->PC++;
    } break;
    case 3: /* abs */
    {
        This->CyclesLeft += 2;
        return FetchWord(This);
    } break;
    case 4: /* (ind),Y */
    {
        u8 AddressPointer = FetchByte(This);
        u16 Address = This->ReadByte(This, (u8)AddressPointer++);
        Address |= (u16)This->ReadByte(This, AddressPointer);
        u16 IndexedAddress = Address + This->Y;

        /* page boundary crossing */
        This->CyclesLeft += 3 + ((Address >> 8) != (IndexedAddress >> 8));
        return IndexedAddress;
    } break;
    case 5: /* zpg,X/Y */
    {
        uint IndexRegister = UseRegY? This->Y : This->X;
        u8 Address = FetchByte(This) + IndexRegister;
        This->CyclesLeft += 2;
        return Address;
    } break;
    case 6: /* abs,Y */
    {
        u16 Address = FetchWord(This);
        u16 IndexedAddress = Address + This->Y;

        This->CyclesLeft += 2 + ((Address >> 8) != (IndexedAddress >> 8));
        return IndexedAddress;
    } break;
    case 7: /* abs,X/Y */
    {
        Bool8 RMW = cc >= 2 && IN_RANGE(0, aaa, 3);

        uint IndexRegister = UseRegY? This->Y : This->X;
        u16 Address = FetchWord(This);
        u16 IndexedAddress = Address + IndexRegister;

        This->CyclesLeft += 2 
            + (((Address >> 8) != (IndexedAddress >> 8)) 
                || RMW);
        return IndexedAddress;
    } break;
    default: return 0xdead;
    }
}



void MC6502StepClock(MC6502 *This)
{
#define TEST_NZ(Data) do {\
    SET_FLAG(FLAG_N, ((Data) >> 7) & 0x1);\
    SET_FLAG(FLAG_Z, (Data) == 0);\
} while (0)
#define DO_COMPARISON(Left, Right) do {\
    u16 Tmp = (u16)(Left) + -(u16)(Right);\
    TEST_NZ(Tmp);\
    SET_FLAG(FLAG_C, Tmp > 0xFF);\
} while (0) 
    if (This->Halt || This->CyclesLeft-- > 0)
        return;

    This->CyclesLeft = 0;
    u8 Opcode = FetchByte(This);
    This->Opcode = Opcode;
    /* 
     * opcode: 0baaabbbcc
     * the switch handles corner-case instructions first,
     * and then the instructions that have patterns follow later 
     * */
    switch (Opcode)
    {
    case 0x00: /* BRK */
    {
        /* break mark */
        (void)FetchByte(This);
        PushWord(This, This->PC);
        PushFlags(This);
        This->Flags |= FLAG_I;

        This->CyclesLeft = 7;
    } break;
    case 0x4C: /* JMP abs */ 
    {
        This->PC = FetchWord(This);
        This->CyclesLeft = 3;
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
        This->CyclesLeft = 5;
    } break;
    case 0x20: /* JSR */
    {
        /* NOTE: this must be done in this order, 
         * RTI, RTS will pop and add 1 to PC */

        u16 SubroutineAddress = FetchByte(This);
        /* now PC is pointing at the MSB of the address */
        PushWord(This, This->PC);
        SubroutineAddress |= (u16)FetchByte(This) << 8;
        This->PC = SubroutineAddress;

        This->CyclesLeft = 6;
    } break;
    case 0x40: /* RTI */ 
    {
        PopFlags(This);
        This->PC = PopWord(This);
        This->CyclesLeft = 6;
    } break;
    case 0x60: /* RTS */
    {
        This->PC = PopWord(This) + 1;
        This->CyclesLeft = 6;
    } break;

    /* stack */
    case 0x08: /* PHP */
    {
        PushFlags(This); 
        This->CyclesLeft = 3;
    } break;
    case 0x28: /* PLP */
    {
        PopFlags(This); 
        This->CyclesLeft = 3;
    } break;
    case 0x48: /* PHA */
    {
        PushByte(This, This->A); 
        This->CyclesLeft = 3;
    } break;
    case 0x68: /* PLA */
    {
        This->A = PopByte(This);
        TEST_NZ(This->A);
        This->CyclesLeft = 4;
    } break;

    /* by 1 instructions on x and y */
#define INCREMENT(Register, Value) do {\
    This->Register += Value;\
    TEST_NZ(This->Register);\
    This->CyclesLeft = 2;\
} while (0)
    case 0x88: /* DEY */ INCREMENT(Y, -1); break;
    case 0xCA: /* DEX */ INCREMENT(X, -1); break;
    case 0xC8: /* INY */ INCREMENT(Y, 1); break;
    case 0xE8: /* INX */ INCREMENT(X, 1); break;
#undef INCREMENT

    /* group cc = 0 immediate instructions */
    case 0xA0: /* LDY */
    {
        This->Y = FetchByte(This);
        TEST_NZ(This->Y);
        This->CyclesLeft = 2;
    } break;
    case 0xA2: /* LDX */
    {
        This->X = FetchByte(This);
        TEST_NZ(This->X);
        This->CyclesLeft = 2;
    } break;
    case 0xC0: /* CPY */
    {
        DO_COMPARISON(
            This->Y, 
            FetchByte(This)
        );
        This->CyclesLeft = 2;
    } break;
    case 0xE0: /* CPX */
    {
        DO_COMPARISON(
            This->X,
            FetchByte(This)
        );
        This->CyclesLeft = 2;
    } break;

    /* transfers */
#define TRANSFER(Dst, Src) do {\
    This->Dst = This->Src;\
    TEST_NZ(This->Dst);\
    This->CyclesLeft = 2;\
} while (0)
    case 0xAA: /* TAX */ TRANSFER(X, A);  break;
    case 0xA8: /* TAY */ TRANSFER(Y, A);  break;
    case 0x8A: /* TXA */ TRANSFER(A, X);  break;
    case 0x98: /* TYA */ TRANSFER(A, Y);  break;
    case 0xBA: /* TSX */ TRANSFER(X, SP); break;
    case 0x9A: /* TXS */
    {
        This->SP = This->X; 
        /* does not set flags */
        This->CyclesLeft = 2;
    } break;
#undef TRANSFER

    /* RMW accumulator */
    case 0x0A: /* ASL */
    {
        SET_FLAG(FLAG_C, This->A >> 7);
        This->A <<= 1;
        TEST_NZ(This->A);

        This->CyclesLeft = 2;
    } break;
    case 0x2A: /* ROL */
    {
        uint PrevFlag = GET_FLAG(FLAG_C);
        SET_FLAG(FLAG_C, This->A >> 7);
        This->A = (This->A << 1) | PrevFlag;
        TEST_NZ(This->A);

        This->CyclesLeft = 2;
    } break;
    case 0x4A: /* LSR */
    {
        SET_FLAG(FLAG_C, This->A & 0x1);
        This->A >>= 1;
        TEST_NZ(This->A);

        This->CyclesLeft = 2;
    } break;
    case 0x6A: /* ROR */
    {
        uint PrevFlag = GET_FLAG(FLAG_C);
        SET_FLAG(FLAG_C, This->A >> 7);
        This->A = (This->A >> 1) | (PrevFlag << 7);
        TEST_NZ(This->A);

        This->CyclesLeft = 2;
    } break;


    /* group cc = 0 nops */
    case 0x80: /* nop imm */
    {
        FetchByte(This);
        This->CyclesLeft = 2;
    } break;
    /* group cc = 1 nops */
    case 0x89:
    /* official nop */
    case 0xEA: This->CyclesLeft = 2; break;

    /* illegal nops with addressing mode in cc = 0 */
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
    case 0xFC:
    {
        This->CyclesLeft = 2;
        GetEffectiveAddress(This, Opcode);
    } break;
    case 0x9C: ABS_OP("(i) SHY", ",X"); break;
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
            switch (AAA(Opcode))
            {
            case 0: /* CLC */ SET_FLAG(FLAG_C, 0); break;
            case 1: /* SEC */ SET_FLAG(FLAG_C, 1); break;
            case 2: /* CLI */ SET_FLAG(FLAG_I, 0); break;
            case 3: /* SEI */ SET_FLAG(FLAG_I, 1); break;
            case 4: /* TYA, already handled */ break;
            case 5: /* CLV */ SET_FLAG(FLAG_V, 0); break;
            case 6: /* CLD */ SET_FLAG(FLAG_D, 0); break;
            case 7: /* SED */ SET_FLAG(FLAG_D, 1); break;
            }
            This->CyclesLeft = 2;
        }
        else if (bbb == 4) /* branch */
        {
            i8 BranchOffset = FetchByte(This);

            /* aaa:
             * ffc: 
             *  ff: flag value: 0..3 
             *  c: compare value
             * branch taken if flag matches c */
            static const MC6502Flags Lookup[] = {
                FLAG_N, FLAG_V, FLAG_C, FLAG_Z
            };

            uint ffc = AAA(Opcode);
            This->CyclesLeft = 2; /* assumes not branching */
            if ((ffc & 1) == GET_FLAG(Lookup[ffc >> 1]))
            {
                u16 TargetAddress = 
                    0xFFFF 
                    & ((i32)This->PC 
                     + (i32)BranchOffset);

                /* if crossing page boundary
                 * +2 else +1 cycles */
                This->CyclesLeft += 1 + ((TargetAddress >> 8) != (This->PC >> 8));

                This->PC = TargetAddress;
            }
        }
        else /* STY, LDY, CPY, CPX */
        {
            This->CyclesLeft = 2;
            u16 Address = GetEffectiveAddress(This, Opcode);
            switch (AAA(Opcode))
            {
            case 0: /* nop */ break;
            case 1: /* BIT, ignore illegals */ 
            {
                u8 Value = This->ReadByte(This, Address);

                This->Flags =  /* flag N, V */
                    (This->Flags & ~0xC0) 
                    | (Value & 0xC0);
                SET_FLAG(FLAG_Z, Value == 0);
            } break;
            case 2: /* nop */
            case 3: /* nop */ 
                break;
            case 4: /* STY, ignore SHY */
            {
                This->WriteByte(This, Address, This->Y);
            } break;
            case 5: /* LDY */
            {
                This->Y = This->ReadByte(This, Address);
                TEST_NZ(This->Y);
            } break;
            case 6: /* CPY, ignore illegals */
            {
                DO_COMPARISON(
                    This->Y, 
                    This->ReadByte(This, Address)
                );
            } break;
            case 7: /* CPX, ignore illegals */
            {
                DO_COMPARISON(
                    This->X, 
                    This->ReadByte(This, Address)
                );
            } break;
            }
        }
    } break;
    case 1: /* accumulator instructions */
    {
        This->CyclesLeft = 2;
        u16 Address = GetEffectiveAddress(This, Opcode);

        switch (AAA(Opcode))
        {
        case 0: /* ORA */
        {
            u8 Byte = This->ReadByte(This, Address);
            This->A |= Byte;
            TEST_NZ(This->A);
        } break;
        case 1: /* AND */
        {
            u8 Byte = This->ReadByte(This, Address);
            This->A |= Byte;
            TEST_NZ(This->A);
        } break;
        case 2: /* EOR */
        {
            u8 Byte = This->ReadByte(This, Address);
            This->A ^= Byte;
            TEST_NZ(This->A);
        } break;
        case 3: /* ADC */
        {
            u8 Byte = This->ReadByte(This, Address);
            u16 Result = This->A + Byte + GET_FLAG(FLAG_C);
            SetAdditionFlags(This, This->A, Byte, Result);
            This->A = Result & 0xFF;
        } break;
        case 4: /* STA */
        {
            This->WriteByte(This, Address, This->A);
        } break;
        case 5: /* LDA */
        {
            This->A = This->ReadByte(This, Address);
            TEST_NZ(This->A);
        } break;
        case 6: /* CMP */
        {
            DO_COMPARISON(This->A, This->ReadByte(This, Address));
        } break;
        case 7: /* SBC */
        {
            u16 Src = -(u16)This->ReadByte(This, Address);
            u16 Result = This->A + Src + !GET_FLAG(FLAG_C);
            SetAdditionFlags(This, This->A, Src & 0xFF, Result);
            This->A = Result & 0xFF;
        } break;
        }
    } break;
    case 2: /* RMW and load/store X */
    {
        uint bbb = BBB(Opcode);
        if (bbb == 4) /* JAM */
        {
            This->Halt = true;
            break;
        }
        if (bbb == 6) /* NOP */
        {
            This->CyclesLeft = 2;
            break;
        }

        u16 Address = GetEffectiveAddress(This, Opcode);
        uint aaa = AAA(Opcode);
        if (aaa == 4) /* STX */
        {
            This->WriteByte(This, Address, This->X);
        }
        else if (aaa == 5) /* LDX */
        {
            This->X = This->ReadByte(This, Address);
            TEST_NZ(This->X);
        }
        else /* RMW */
        {
            This->CyclesLeft += 4;
            /* NOTE: RMW instructions do: 
             *   read 
             *   write old
             *   write new */
            u8 Byte = This->ReadByte(This, Address);
            u8 Result = 0;
            This->WriteByte(This, Address, Byte);

            switch (AAA(Opcode))
            {
            case 0: /* ASL */
            {
                SET_FLAG(FLAG_C, Byte >> 7);
                Result = Byte << 1;
            } break;
            case 1: /* ROL */
            {
                uint PrevCarry = GET_FLAG(FLAG_C);
                SET_FLAG(FLAG_C, Byte >> 7);
                Result = (Byte << 1) | PrevCarry;
            } break;
            case 2: /* LSR */
            {
                SET_FLAG(FLAG_C, Byte & 0x1);
                Result = Byte >> 1;
            } break;
            case 3: /* ROR */
            {
                uint PrevFlag = GET_FLAG(FLAG_C);
                SET_FLAG(FLAG_C, Byte & 0x1);
                Result = (Byte >> 1) | (PrevFlag << 7);
            } break;
            case 6: /* DEC */
            {
                Result = Byte - 1;
            } break;
            case 7: /* INC */
            {
                Result = Byte + 1;
            } break;
            }
            TEST_NZ(Result);
            This->WriteByte(This, Address, Result);
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

