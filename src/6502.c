#include "Common.h"

typedef struct MC6502 MC6502;
typedef void (*MC6502WriteByte)(void *UserData, u16 Address, u8 Byte);
typedef u8 (*MC6502ReadByte)(void *UserData, u16 Address);
typedef u8 (*MC6502RMWInstruction)(MC6502 *This, u8 Byte);

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
    Bool8 HasDecimalMode;
};

MC6502 MC6502_Init(u16 PC, void *UserData, MC6502ReadByte ReadFn, MC6502WriteByte WriteFn);
void MC6502_StepClock(MC6502 *This);

void MC6502_Reset(MC6502 *This);
#define VEC_IRQ 0xFFFE
#define VEC_RES 0xFFFC
#define VEC_NMI 0xFFFA
void MC6502_Interrupt(MC6502 *This, u16 Vector);
u8 MC6502_FlagSet(u8 Byte, uint Value, MC6502Flags Flag);
uint MC6502_FlagGet(u8 Byte, MC6502Flags Flag);



uint MC6502_FlagGet(u8 Byte, MC6502Flags Flag)
{
    uint Pos = Flag >> 8;
    return (Byte >> Pos) & 0x1;
}

u8 MC6502_FlagSet(u8 Byte, uint Value, MC6502Flags Flag)
{
    Value = 0 != Value; /* ensure 0 or 1 */
    uint Pos = Flag >> 8;
    uint Mask = Flag & 0xFF;
    return (Byte & ~Mask) | (Value << Pos);
}

#define TEST_NZ(Data) do {\
    SET_FLAG(FLAG_N, ((Data) >> 7) & 0x1);\
    SET_FLAG(FLAG_Z, ((Data) & 0xFF) == 0);\
} while (0)
#  define SET_FLAG(fl, BooleanValue)    (This->Flags = MC6502_FlagSet(This->Flags, BooleanValue, fl))
#  define GET_FLAG(fl)                  MC6502_FlagGet(This->Flags, fl)
#  define MC6502_MAGIC_CONSTANT 0xff

MC6502 MC6502_Init(u16 PC, void *UserData, MC6502ReadByte ReadFn, MC6502WriteByte WriteFn)
{
    MC6502 This = {
        .PC = PC,
        .UserData = UserData,
        .ReadByte = ReadFn,
        .WriteByte = WriteFn,
        .Halt = false,
        .CyclesLeft = 0,
    };
    MC6502_Reset(&This);
    This.PC = PC;
    return This;
}



void MC6502_Reset(MC6502 *This)
{
    MC6502_Interrupt(This, VEC_RES);
}

static u8 FetchByte(MC6502 *This)
{
    return This->ReadByte(This->UserData, This->PC++);
}

static u16 FetchWord(MC6502 *This)
{
    u16 Word = FetchByte(This);
    Word |= (u16)FetchByte(This) << 8;
    return Word;
}

static u8 PopByte(MC6502 *This)
{
    return This->ReadByte(This->UserData, ++This->SP + 0x100);
}

static u16 PopWord(MC6502 *This)
{
    u16 Word = PopByte(This);
    Word |= (u16)PopByte(This) << 8;
    return Word;
}

static void PushByte(MC6502 *This, u8 Byte)
{
    This->WriteByte(This->UserData, This->SP-- + 0x100, Byte);
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
    This->Flags = PopByte(This) & ~(FLAG_B | FLAG_UNUSED);
}

static void SetAdditionFlags(MC6502 *This, u8 a, u8 b, u16 Result)
{
    SET_FLAG(FLAG_N, (Result & 0x80));
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
    /* */
    Bool8 UseRegY = cc >= 2 && (aaa == 4 || aaa == 5);
    switch (bbb)
    {
    case 0: /* (ind,X) */
    {
        u8 AddressPointer = (u8)(FetchByte(This) + This->X);
        u16 Address = This->ReadByte(This->UserData, (u8)AddressPointer++);
        Address |= (u16)This->ReadByte(This->UserData, AddressPointer) << 8;

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
        u16 Address = This->ReadByte(This->UserData, (u8)AddressPointer++);
        Address |= (u16)This->ReadByte(This->UserData, AddressPointer) << 8;
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
        Bool8 RMW = cc >= 2 && !IN_RANGE(4, aaa, 5);

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



static u8 RMWReadByte(MC6502 *This, u16 Address)
{
    u8 Byte = This->ReadByte(This->UserData, Address);
    This->WriteByte(This->UserData, Address, Byte);
    return Byte;
}

static u8 ROR(MC6502 *This, u8 Value)
{
    uint PrevFlag = GET_FLAG(FLAG_C);
    SET_FLAG(FLAG_C, Value & 0x1);
    u8 Result = (Value >> 1) | (PrevFlag << 7);
    TEST_NZ(Result);

    return Result;
}

static u8 ROL(MC6502 *This, u8 Value)
{
    uint PrevFlag = GET_FLAG(FLAG_C);
    SET_FLAG(FLAG_C, Value >> 7);
    u8 Result = (Value << 1) | PrevFlag;
    TEST_NZ(Result);

    return Result;
}

static u8 ASL(MC6502 *This, u8 Value)
{
    SET_FLAG(FLAG_C, Value >> 7);
    u8 Result = Value << 1;
    TEST_NZ(Result);

    return Result;
}

static u8 INC(MC6502 *This, u8 Value)
{
    u8 Result = Value + 1;
    TEST_NZ(Result);
    return Result;
}

static u8 DEC(MC6502 *This, u8 Value)
{
    u8 Result = Value - 1;
    TEST_NZ(Result);
    return Result;
}

static u8 LSR(MC6502 *This, u8 Value)
{
    SET_FLAG(FLAG_C, Value & 0x1);
    u8 Result = Value >> 1;
    TEST_NZ(Result);

    return Result;
}


static u16 BCD_ADD(u16 a, u16 b)
{
    u16 Lower = (a & 0x0F) + (b & 0x0F);
    if (Lower > 0x09)
        Lower += 0x06;

    u16 Upper = (a & 0xF0) + (b & 0xF0) + (Lower & 0xF0);
    if (IN_RANGE(0xA0, Upper & 0xFF0, 0x130))
        Upper += 0x60;

    u16 Carry = (a + b) & 0xF00;
    u16 Result = (Upper | (Lower & 0x0F)) + Carry;
    return Result;
}

static void ADC(MC6502 *This, u8 Value)
{
    /* BCD addition */
    if (This->HasDecimalMode && GET_FLAG(FLAG_D))
    {
        u16 ValueAndCarry = BCD_ADD(Value, GET_FLAG(FLAG_C));
        u16 Result = BCD_ADD(This->A, ValueAndCarry);
        SetAdditionFlags(This, 
            This->A, 
            (u8)ValueAndCarry, 
            Result
        );
        This->A = (u8)Result;
    }
    else
    {
        u16 Result = This->A + Value + GET_FLAG(FLAG_C);
        SetAdditionFlags(This, This->A, Value, Result);
        This->A = (u8)Result;
    }
}

static void SBC(MC6502 *This, u8 Value)
{
    /* BCD subtraction */
    if (This->HasDecimalMode && GET_FLAG(FLAG_D))
    {
        uint Carry = !GET_FLAG(FLAG_C);
        u8 LowNibble = (This->A & 0x0F) - (Value & 0x0F) - Carry;
        uint CarryFromLowNibble = 0;
        if ((This->A & 0x0F) < (Value & 0x0F) + Carry) /* overflow? */
        {
            LowNibble += 10;
            CarryFromLowNibble = 0x10;
        }

        uint HighNibble = (This->A & 0xF0) - (Value & 0xF0) - CarryFromLowNibble;
        if (HighNibble > 0x90)
            HighNibble -= 0x60;

        uint Result = HighNibble | (LowNibble & 0x0F);
        SET_FLAG(FLAG_C, Result <= 0xFF);
        TEST_NZ(Result);

        This->A = (u8)Result;
    }
    else
    {
        Value = ~Value;
        u16 Result = This->A + Value + GET_FLAG(FLAG_C);
        SetAdditionFlags(This, This->A, Value, Result);
        This->A = (u8)Result;
    }
}

static void FetchVector(MC6502 *This, u16 Vector)
{
    This->PC = This->ReadByte(This->UserData, Vector++);
    This->PC |= (u16)This->ReadByte(This->UserData, Vector) << 8;
}




void MC6502_Interrupt(MC6502 *This, u16 InterruptVector)
{
    if (InterruptVector == VEC_IRQ && GET_FLAG(FLAG_I))
        return;

    PushWord(This, This->PC);
    PushFlags(This);
    This->Flags |= FLAG_I;
    FetchVector(This, InterruptVector);

    if (InterruptVector == VEC_RES)
    {
        This->SP = 0xFD;
        This->A = 0;
        This->X = 0;
        This->Y = 0;
        This->CyclesLeft = 7;
        This->Opcode = 0;
        This->Halt = false;
    }
    else
    {
        This->CyclesLeft = 7;
    }
}

void MC6502_StepClock(MC6502 *This)
{
#define DO_COMPARISON(u8Left, u8Right) do {\
    u16 Tmp = (u16)(u8Left) + (u16)-(u8)(u8Right);\
    TEST_NZ(Tmp);\
    SET_FLAG(FLAG_C, Tmp <= 0xFF);\
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

        /* save state and set I-disable flag */
        PushWord(This, This->PC);
        PushFlags(This);
        This->Flags |= FLAG_I;

        /* fetch interrupt vector */
        FetchVector(This, VEC_IRQ);

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
        u16 Address = This->ReadByte(This->UserData, AddressPointer);

        /* simulate hardware bug */
        AddressPointer = 
            (AddressPointer & 0xFF00) 
            | (0x00FF & (AddressPointer + 1));
        Address |= (u16)This->ReadByte(This->UserData, AddressPointer) << 8;

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
        This->A = ASL(This, This->A);
        This->CyclesLeft = 2;
    } break;
    case 0x2A: /* ROL */
    {
        This->A = ROL(This, This->A);
        This->CyclesLeft = 2;
    } break;
    case 0x4A: /* LSR */
    {
        This->A = LSR(This, This->A);
        This->CyclesLeft = 2;
    } break;
    case 0x6A: /* ROR */
    {
        This->A = ROR(This, This->A);
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
    case 0xFC: /* NOP addrm */
    {
        This->CyclesLeft = 2;
        GetEffectiveAddress(This, Opcode);
    } break;
    case 0x9C: /* SHY abs,x */
    {
        u16 Address = FetchWord(This);
        u8 Byte = This->Y & ((Address >> 8) + 1);
        This->WriteByte(This->UserData, Address + This->Y, Byte);
        This->CyclesLeft = 5;
    } break;

    /* group cc = 2 illegal */
    case 0x9E: /* SHX abs,y */
    {
        u16 Address = FetchWord(This);
        u8 Byte = This->X & ((Address >> 8) + 1);
        This->WriteByte(This->UserData, Address + This->Y, Byte);
        This->CyclesLeft = 5;
    } break;

    /* group cc = 3 oddball illegals */
    case 0x0B:
    case 0x2B: /* ANC #imm */
    {
        u8 Tmp = This->A & FetchByte(This);
        TEST_NZ(Tmp);
        SET_FLAG(FLAG_C, Tmp & 0x80);
        This->CyclesLeft = 2;
    } break;
    case 0x4B: /* ALR #imm */
    {
        u8 Tmp = This->A & FetchByte(This);
        Tmp >>= 1;
        TEST_NZ(Tmp);
        SET_FLAG(FLAG_C, Tmp & 0x80);
        This->CyclesLeft = 2;
    } break;
    case 0x6B: /* ARR #imm */
    {
        u8 Byte = FetchByte(This);
        u8 Left = This->A & Byte;
        u16 Result = (u16)Left + (u16)Byte;
        SetAdditionFlags(This, Left, Byte, Result);
        ROR(This, This->A);
        This->CyclesLeft = 2;
    } break;
    case 0x8B: /* ANE #imm */
    {
        This->A = (This->A | MC6502_MAGIC_CONSTANT)
            & FetchByte(This) 
            & This->X;
        TEST_NZ(This->A);
        This->CyclesLeft = 2;
    } break;
    case 0xAB: /* LXA, aka LAX immediate */ 
    {
        u8 Magic = MC6502_MAGIC_CONSTANT | This->A;
        u8 Byte = FetchByte(This) & Magic;
        This->A = Byte;
        This->X = Byte;
        This->CyclesLeft = 2;
    } break;
    case 0xCB: 
    /* SBX #imm: 
     * https://www.masswerk.at/6502/6502_instruction_set.html#SBX
     *      the documentation on this is confusing, 
     *      it said CMP and DEX at the same time, 
     *      but also said "(A AND X) - oper -> X, setting flags like CMP" 
     *  This implementation assumes the latter logic is true */ 
    {
        u8 Value = This->A & This->X;
        u8 Immediate = FetchByte(This);
        DO_COMPARISON(Value, Immediate);
        This->X = Value - Immediate;
        This->CyclesLeft = 2;
    } break;
    case 0xEB: /* USBC, same as SBC #imm */
    {
        SBC(This, FetchByte(This));
        This->CyclesLeft = 2;
    } break;
    case 0x9B: /* TAS abs,y */
    {
        u16 Address = FetchWord(This) + This->Y;
        This->SP = This->A & This->X;
        u8 Value = This->A & This->X & (u8)((Address >> 8) + 1);
        This->WriteByte(This->UserData, Address, Value);

        This->CyclesLeft = 5;
    } break;
    case 0xBB: /* LAS abs,y */
    {
        u16 Address = FetchWord(This);
        u16 IndexedAddress = Address + This->Y;
        u8 Byte = This->SP & This->ReadByte(This->UserData, IndexedAddress);
        This->A = Byte;
        This->X = Byte;
        This->SP = Byte;

        This->CyclesLeft = 4 + ((IndexedAddress >> 8) != (Address >> 8));
    } break;
    case 0x9F: /* SHA abs,y */
    {
        u16 Address = FetchWord(This);

        u8 Byte = This->A & This->X & ((Address >> 8) + 1);
        This->WriteByte(This->UserData, Address + This->Y, Byte);

        This->CyclesLeft = 5;
    } break;
    case 0x93: /* SHA (ind),y */
    {
        u8 AddressPointer = FetchByte(This);
        u16 Address = This->ReadByte(This->UserData, AddressPointer++);
        Address |= (u16)This->ReadByte(This->UserData, AddressPointer) << 8;

        u8 Byte = This->A & This->X & ((Address >> 8) + 1);
        This->WriteByte(This->UserData, Address + This->Y, Byte);

        This->CyclesLeft = 6;
    } break;



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
                u8 Value = This->ReadByte(This->UserData, Address);

                This->Flags =  /* flag N, V */
                    (This->Flags & ~0xC0) 
                    | (Value & 0xC0);
                SET_FLAG(FLAG_Z, (Value & This->A) == 0);
            } break;
            case 2: /* nop */
            case 3: /* nop */ 
                break;
            case 4: /* STY, ignore SHY */
            {
                This->WriteByte(This->UserData, Address, This->Y);
            } break;
            case 5: /* LDY */
            {
                This->Y = This->ReadByte(This->UserData, Address);
                TEST_NZ(This->Y);
            } break;
            case 6: /* CPY, ignore illegals */
            {
                DO_COMPARISON(
                    This->Y, 
                    This->ReadByte(This->UserData, Address)
                );
            } break;
            case 7: /* CPX, ignore illegals */
            {
                DO_COMPARISON(
                    This->X, 
                    This->ReadByte(This->UserData, Address)
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
            u8 Byte = This->ReadByte(This->UserData, Address);
            This->A |= Byte;
            TEST_NZ(This->A);
        } break;
        case 1: /* AND */
        {
            u8 Byte = This->ReadByte(This->UserData, Address);
            This->A &= Byte;
            TEST_NZ(This->A);
        } break;
        case 2: /* EOR */
        {
            u8 Byte = This->ReadByte(This->UserData, Address);
            This->A ^= Byte;
            TEST_NZ(This->A);
        } break;
        case 3: /* ADC */
        {
            u8 Byte = This->ReadByte(This->UserData, Address);
            ADC(This, Byte);
        } break;
        case 4: /* STA */
        {
            This->WriteByte(This->UserData, Address, This->A);
        } break;
        case 5: /* LDA */
        {
            This->A = This->ReadByte(This->UserData, Address);
            TEST_NZ(This->A);
        } break;
        case 6: /* CMP */
        {
            u8 Byte = This->ReadByte(This->UserData, Address);
            DO_COMPARISON(
                This->A, 
                Byte
            );
        } break;
        case 7: /* SBC */
        {
            u8 Byte = This->ReadByte(This->UserData, Address);
            SBC(This, Byte);
        } break;
        }
    } break;
    case 2: /* RMW and load/store X */
    {
        uint aaa = AAA(Opcode);
        uint bbb = BBB(Opcode);
        if (bbb == 4 || (bbb == 0 && aaa < 4)) /* JAM */
        {
            This->Halt = true;
            break;
        }
        if (bbb == 6) /* NOP impl */
        {
            This->CyclesLeft = 2;
            break;
        }
        if (bbb == 0) /* NOP #imm */
        {
            FetchByte(This);
            This->CyclesLeft = 2;
            break;
        }

        u16 Address = GetEffectiveAddress(This, Opcode);
        if (aaa == 4) /* STX */
        {
            This->WriteByte(This->UserData, Address, This->X);
            This->CyclesLeft += 2;
        }
        else if (aaa == 5) /* LDX */
        {
            This->X = This->ReadByte(This->UserData, Address);
            TEST_NZ(This->X);
            This->CyclesLeft += 2;
        }
        else /* RMW */
        {
            /* NOTE: RMW instructions do: 
             *   read 
             *   write old
             *   write new */
            static MC6502RMWInstruction Lookup[] = {
                ASL,  ROL,  LSR, ROR, 
                NULL, NULL, DEC, INC
            };
            MC6502RMWInstruction RMW = Lookup[aaa];
            DEBUG_ASSERT(RMW != NULL);

            u8 Byte = RMWReadByte(This, Address);
            u8 Result = RMW(This, Byte);
            This->WriteByte(This->UserData, Address, Result);
            This->CyclesLeft += 4;
        }
    } break;
    case 3: /* illegal instructions */
    {
        u16 Address = GetEffectiveAddress(This, Opcode);
        uint aaa = AAA(Opcode);
        if (aaa == 4) /* SAX */
        {
            u8 Value = This->A & This->X;
            This->WriteByte(This->UserData, Address, Value);
        }
        else if (aaa == 5) /* LAX */
        {
            u8 Byte = This->ReadByte(This->UserData, Address);
            This->A = Byte;
            This->X = Byte;
            TEST_NZ(Byte);
        }
        else
        {
            u8 Byte = RMWReadByte(This, Address);
            u8 IntermediateResult = 0;
            switch (AAA(Opcode))
            {
            case 0: /* SLO */
            {
                IntermediateResult = ASL(This, Byte);
                This->A |= IntermediateResult;
                TEST_NZ(This->A);
            } break;
            case 1: /* RLA */
            {
                IntermediateResult = ROL(This, Byte);
                This->A &= IntermediateResult;
                TEST_NZ(This->A);
            } break;
            case 2: /* SRE */
            {
                IntermediateResult = LSR(This, Byte);
                This->A ^= IntermediateResult;
                TEST_NZ(This->A);
            } break;
            case 3: /* RRA */
            {
                IntermediateResult = ROR(This, Byte);
                ADC(This, IntermediateResult);
            } break;
            case 6: /* DCP */
            {
                IntermediateResult = Byte - 1;
                DO_COMPARISON(
                    This->A, 
                    IntermediateResult
                );
            } break;
            case 7: /* ISC */
            {
                IntermediateResult = Byte + 1;
                SBC(This, IntermediateResult);
            } break;
            }
            This->WriteByte(This->UserData, Address, IntermediateResult);
            This->CyclesLeft += 4;
        }
    } break;
    } break;
    }
#undef DO_COMPARISON
}

#undef TEST_NZ
#undef SET_FLAG
#undef GET_FLAG
#undef MC6502_MAGIC_CONSTANT

#ifdef STANDALONE
#undef STANDALONE
#include <stdio.h>
#include "Disassembler.c"

static u8 sMemory[0x10000];
static Bool8 sRWLog = false;

static u8 ReadFn(void *This, u16 Address)
{
    (void)This;
    if (sRWLog)
        printf("[READING] @%04x -> %02x\n", Address, sMemory[Address]);
    return sMemory[Address];
}

static void WriteFn(void *This, u16 Address, u8 Byte)
{
    (void)This;
    if (sRWLog)
        printf("[WRITING] %02x <- %04x <- %02x\n", sMemory[Address], Address, Byte);
    sMemory[Address] = Byte;
}

static void PrintDisassembly(int PC)
{
#define INS_COUNT 10
    static struct {
        u16 Address;
        u16 ByteCount;
        SmallString String;
    } InstructionBuffer[INS_COUNT] = { 0 };
    static Bool8 Initialized = false;

    if (!Initialized || 
        !IN_RANGE(
            InstructionBuffer[0].Address, 
            PC, 
            InstructionBuffer[INS_COUNT - 1].Address
        ))
    {
        Initialized = true;
        int CurrentPC = PC;
        for (int i = 0; i < INS_COUNT; i++)
        {
            int InstructionSize = DisassembleSingleOpcode(
                &InstructionBuffer[i].String, 
                CurrentPC,
                &sMemory[CurrentPC],
                sizeof sMemory - CurrentPC
            );
            if (-1 == InstructionSize)
            {
                /* retry at addr 0 */
                CurrentPC = 0;
                i--;
                continue;
            }

            InstructionBuffer[i].ByteCount = InstructionSize;
            InstructionBuffer[i].Address = CurrentPC;
            CurrentPC += InstructionSize;
            CurrentPC %= sizeof sMemory;
        }
    }

    for (int i = 0; i < INS_COUNT; i++)
    {
        /* print space or pointer */
        const char *Pointer = (InstructionBuffer[i].Address == PC)?
            ">> " : "   ";
        printf("%s", Pointer);

        /* print addr and bytes */
        printf("%04x: ", InstructionBuffer[i].Address);
        int ExpectedByteCount = 4;
        for (int ByteIndex = 0; ByteIndex < InstructionBuffer[i].ByteCount; ByteIndex++)
        {
            u8 Byte = sMemory[
                (InstructionBuffer[i].Address + ByteIndex) 
                % sizeof sMemory
            ];
            printf("%02x ", Byte);
            ExpectedByteCount--;
        }
        while (ExpectedByteCount--)
        {
            printf("   ");
        }

        /* print menmonic */
        printf("%s\n", InstructionBuffer[i].String.Data);
    }
#undef INS_COUNT
}

static void PrintState(const MC6502 *Cpu)
{
    puts("----------------------------");
    printf("A: %02x; X: %02x; Y: %02x; SP: %02x\n",
        Cpu->A, Cpu->X, Cpu->Y, Cpu->SP + 0x100
    );
    printf("PC: %04x; SR: %02x (NV_BDIZC)\n",
        Cpu->PC, Cpu->Flags
    );
    puts("----------------------------");
}

static Bool8 ReadFileIntoMemory(const char *FileName)
{
    /* open file */
    FILE *f = fopen(FileName, "rb");
    if (NULL == f)
    {
        perror(FileName);
        goto Fail;
    }

    /* find file name */
    fseek(f, 0, SEEK_END);
    size_t FileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (FileSize > sizeof sMemory)
    {
        fprintf(stderr, "File must be smaller than the 6502's addressable range.");
        goto Fail;
    }

    /* read into mem */
    if (FileSize != fread(sMemory, 1, sizeof sMemory, f))
    {
        perror(FileName);
        goto Fail;
    }
    fclose(f);
    return true;
Fail:
    if (f)
        fclose(f);
    return false;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: %s <binary file>\n", argv[0]);
        return 0;
    }


    const char *FileName = argv[1];
    if (!ReadFileIntoMemory(FileName))
        return 1;
    

    MC6502 Cpu = MC6502_Init(0x400, NULL, ReadFn, WriteFn);
    Cpu.HasDecimalMode = true;
    u16 RepeatingAddr = 0;
    uint RepeatingCount = 0;
    Bool8 SingleStep = false;
    u16 SingleStepAddr = 0;
    while (1)
    {
        if (Cpu.PC == RepeatingAddr)
            RepeatingCount++;
        else RepeatingCount = 0;
        if (RepeatingCount > 20)
            break;
        if (Cpu.PC == SingleStepAddr)
        {
            SingleStep = true;
            sRWLog = true;
        }
        if (SingleStep)
        {
            PrintDisassembly(Cpu.PC);
            PrintState(&Cpu);
            if ('q' == getc(stdin))
                break;
        }


        RepeatingAddr = Cpu.PC;
        Cpu.CyclesLeft = 0;
        MC6502_StepClock(&Cpu);
    }

    if (Cpu.PC == 0x3469)
    {
        printf("<<< TEST PASSED >>>\n");
    }
    else
    {
        printf("<<< TEST FAILED >>>\n");
    }
    PrintDisassembly(Cpu.PC);
    PrintState(&Cpu);
    return 0;
}

#endif /* STANDALONE */

