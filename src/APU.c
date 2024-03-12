
#include "Nes.h"
#include "Utils.h"
#include "Common.h"



typedef struct Sweeper 
{
    Bool8 Enable;
    Bool8 Negate;
    Bool8 Reload;
    Bool8 IsMuting;

    u16 ShiftCount;
    u16 ShifterResult;
    u16 Divider;
} Sweeper;

typedef struct Sequencer 
{
    double DutyCycle;
    u16 Looping;
    u16 LengthCounter;
    u16 Reload;
    u16 Timer;

    /* envelope */
    u8 Volume;
    u16 DividerClock;
    u16 DividerPeriodReload;
    u8 DecayCounter;
    Bool8 Enable;
    Bool8 ConstantVolume;
    Bool8 Start;

    Sweeper Sweeper;
} Sequencer;

typedef struct TriangleSequencer 
{
    Bool8 Ctrl;
    Bool8 CounterReloadFlag;
    u8 LinearCounter;
    u8 CounterReload;
    u16 TimerReload;
    u16 Timer;
    Bool8 Enable;
} TriangleSequencer;

#define COUNTER_STOP 0xFFFF

typedef struct NESAPU 
{
    Sequencer Pulse1;
    Sequencer Pulse2;
    TriangleSequencer Triangle;
    Sequencer Noise;

    u64 ClockCounter;
    u64 FrameClockCounter;

    double AudioSample;
    double ElapsedTime;
    u8 LengthCounterTable[0x20];
    u16 NTSCNoisePeriodTable[0x10];

    u32 RandSeed;
} NESAPU;



void NESAPU_Reset(NESAPU *This)
{
    This->ElapsedTime = 0;
    This->Triangle = (TriangleSequencer) { 0 };
    This->Pulse1 = (Sequencer) { 0 };
    This->Pulse2 = (Sequencer) { 0 };
    This->Noise = (Sequencer) { 0 };
}

NESAPU NESAPU_Init(u64 SeedForNoiseGeneration)
{
    NESAPU APU = {
        /* https://www.nesdev.org/wiki/APU_Length_Counter */
        .LengthCounterTable = {
            10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14, 
            12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
        },
        .NTSCNoisePeriodTable = {
            4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
        },
    };
    NESAPU_Reset(&APU);
    return APU;
}



typedef double (*SampleGenerator)(Sequencer *, double t);

static double GenerateSquareWave(Sequencer *Seq, double t)
{
    double Freq = NES_CPU_CLK / (16.0 * (double)(Seq->Timer + 1));
    t *= Freq;
    return (t - (i64)t) < Seq->DutyCycle? 
        1.0 : -1.0;
}


static double NESAPU_SequenceSample(Sequencer *Seq, double t, SampleGenerator SampleGenerator)
{
    double Sample = 0;
    Sample = SampleGenerator(Seq, t);
    return Sample;
}


static void NESAPU_SequencerUpdateVolume(Sequencer *Seq)
{
    if (!Seq->Start)
    {
        /* clock the divisor */
        if (Seq->DividerClock == 0)
        {
            Seq->DividerClock = Seq->Volume;
            if (Seq->DecayCounter)
                Seq->DecayCounter--;
            else if (Seq->Looping)
                Seq->DecayCounter = 15;
        }
        else
        {
            Seq->DividerClock--;
        }
    }
    else
    {
        Seq->Start = false;
        Seq->DecayCounter = 15;
        Seq->DividerClock = Seq->DividerPeriodReload;
    }
}

static double NESAPU_SequencerGetVolume(Sequencer *Seq)
{
    Seq->Sweeper.IsMuting = 
        Seq->Timer < 8
        || Seq->Sweeper.ShifterResult > 0x07FF;
    if (Seq->Sweeper.IsMuting)
        return 0;

    if (Seq->ConstantVolume)
        return (double)Seq->Volume * (1.0 / 15);
    return (double)Seq->DecayCounter * (1.0 / 15);
}

static void NESAPU_UpdateTriangleSequencer(TriangleSequencer *Seq)
{
    if (Seq->CounterReloadFlag)
    {
        Seq->LinearCounter = Seq->CounterReload;
    }
    else if (Seq->LinearCounter)
    {
        Seq->LinearCounter--;
    }

    if (!Seq->Ctrl)
    {
        Seq->CounterReloadFlag = 0;
    }
}

static double NESAPU_GetTriangleSample(TriangleSequencer *Seq, double t)
{
    if (!Seq->LinearCounter)
        return 0;
    double Freq = NES_CPU_CLK / (32.0 * (double)(Seq->Timer + 1));
    t *= Freq;
    t -= (i64)t;
    if (t > .5)
        return -t;
    return t;
}


static void NESAPU_CalculateTargetPeriod(Sequencer *Seq, Bool8 UseOnesComplement)
{
    uint ChangeAmount = Seq->Timer >> Seq->Sweeper.ShiftCount;
    if (Seq->Sweeper.Negate)
    {
        ChangeAmount = UseOnesComplement? 
            ~ChangeAmount
            : -ChangeAmount;
    }

    u16 NewPeriod = Seq->Timer + ChangeAmount;
    if (NewPeriod & 0x8000)
        NewPeriod = 0;
    Seq->Sweeper.ShifterResult = NewPeriod;
}

static void NESAPU_UpdatePeriod(Sequencer *Seq)
{
    if (Seq->Sweeper.Divider == 0 && Seq->Sweeper.Enable && Seq->Sweeper.ShiftCount != 0)
    {
        if (!Seq->Sweeper.IsMuting)
            Seq->Timer = Seq->Sweeper.ShifterResult;
        /* else the period remain unchanged */
    }
    if (Seq->Sweeper.Divider == 0 || Seq->Sweeper.Reload)
    {
        Seq->Sweeper.Divider = Seq->DividerPeriodReload + 1;
        Seq->Sweeper.Reload = false;
    }
    else Seq->Sweeper.Divider--;

}


void NESAPU_StepClock(NESAPU *This)
{
#define IS_QUARTER_CLK(Clk) ((Clk) == 3729 || (Clk) == 7457 || (Clk) == 11186 || (Clk) == 14916)
#define IS_HALF_CLK(Clk) ((Clk) == 7457 || (Clk) == 14916)

    if (This->ClockCounter % 6 == 0)
    {
        This->ElapsedTime += 2.0 / NES_CPU_CLK;
        This->FrameClockCounter++;
        NESAPU_CalculateTargetPeriod(&This->Pulse1, true);
        NESAPU_CalculateTargetPeriod(&This->Pulse2, false);

        /* adjust volume envelope */
        if (IS_QUARTER_CLK(This->FrameClockCounter))
        {
            NESAPU_SequencerUpdateVolume(&This->Pulse1);
            NESAPU_SequencerUpdateVolume(&This->Pulse2);
            NESAPU_UpdateTriangleSequencer(&This->Triangle);

        }
        /* note length and freq sweep */
        if (IS_HALF_CLK(This->FrameClockCounter))
        {
            NESAPU_UpdatePeriod(&This->Pulse1);
            NESAPU_UpdatePeriod(&This->Pulse2);
        }

        /* clock pulse 1 */
        double Pulse1 = 
            NESAPU_SequencerGetVolume(&This->Pulse1)
            * NESAPU_SequenceSample(&This->Pulse1, This->ElapsedTime, GenerateSquareWave); 
        double Pulse2 = 
            NESAPU_SequencerGetVolume(&This->Pulse2)
            * NESAPU_SequenceSample(&This->Pulse2, This->ElapsedTime, GenerateSquareWave);
        double Triangle = NESAPU_GetTriangleSample(&This->Triangle, This->ElapsedTime);
        double Noise = 0;
        double DMC = 0;

        /* sound mixer */
        This->AudioSample = INT16_MAX 
            * (0.0752*(Pulse1 + Pulse2) 
             + 0.0835*Triangle 
             + 0.0494*Noise 
             + 0.0335*DMC
        );
        
        if (This->FrameClockCounter == 14916)
            This->FrameClockCounter = 0;
    }
    This->ClockCounter++;
#undef IS_QUARTER_CLK
#undef IS_HALF_CLK
}


u8 NESAPU_ExternalRead(NESAPU *This, u16 Address)
{
    switch (Address)
    {
    default: 
        break;
    /* DMC */
    case 0x4010:
    case 0x4011:
    case 0x4012:
    case 0x4013:
    /* Channel enable/length counter stat */
    case 0x4015:
    /* frame counter */
    case 0x4017:
        break;
    }
    return 0;
}




void NESAPU_ExternalWrite(NESAPU *This, u16 Address, u8 Byte)
{
    static const double DutyCycleLookup[4] = { .125, .250, .500, .750 };
    switch (Address)
    {
    /* pulse 1 */
    case 0x4000:
    {
        This->Pulse1.DutyCycle = DutyCycleLookup[Byte >> 6];
        This->Pulse1.Looping = Byte & 0x20;
        This->Pulse1.ConstantVolume = Byte & 0x10;
        This->Pulse1.Volume = Byte & 0x0F;
    } break;
    case 0x4001:
    {
        This->Pulse1.DividerPeriodReload = (Byte >> 4) & 0x7;
        This->Pulse1.Sweeper.Enable = Byte & 0x80;
        This->Pulse1.Sweeper.Negate = Byte & 0x08;
        This->Pulse1.Sweeper.ShiftCount = Byte & 0x07;
        This->Pulse1.Sweeper.Reload = true;
    } break;
    case 0x4002:
    {
        MASKED_LOAD(This->Pulse1.Reload, Byte, 0x00FF);
    } break;
    case 0x4003:
    {
        MASKED_LOAD(This->Pulse1.Reload, (u16)Byte << 8, 0x0700);
        This->Pulse1.Timer = This->Pulse1.Reload;
        This->Pulse1.Start = true;
    } break;


    /* pulse 2 */
    case 0x4004:
    {
        This->Pulse2.DutyCycle = DutyCycleLookup[Byte >> 6];
        This->Pulse2.Looping = Byte & 0x20;
        This->Pulse2.ConstantVolume = Byte & 0x10;
        This->Pulse2.Volume = Byte & 0x0F;
    } break;
    case 0x4005:
    {
        This->Pulse2.DividerPeriodReload = (Byte >> 4) & 0x7;
        This->Pulse2.Sweeper.Enable = Byte & 0x80;
        This->Pulse2.Sweeper.Negate = Byte & 0x08;
        This->Pulse2.Sweeper.ShiftCount = Byte & 0x07;
        This->Pulse2.Sweeper.Reload = true;
    } break;
    case 0x4006:
    {
        MASKED_LOAD(This->Pulse2.Reload, Byte, 0x00FF);
    } break;
    case 0x4007:
    {
        MASKED_LOAD(This->Pulse2.Reload, (u16)Byte << 8, 0x0700);
        This->Pulse2.Timer = This->Pulse2.Reload;
        This->Pulse2.LengthCounter = This->LengthCounterTable[Byte >> 3];
        This->Pulse2.Start = true;
    } break;


    /* triangle */
    case 0x4008:
    {
        This->Triangle.Ctrl = Byte & 0x80;
        This->Triangle.CounterReload = Byte & 0x7F;
    } break;
    case 0x4009: break; /* unused */
    case 0x400A:
    {
        MASKED_LOAD(This->Triangle.TimerReload, Byte, 0x00FF);
    } break;
    case 0x400B:
    {
        MASKED_LOAD(This->Triangle.TimerReload, (u16)Byte << 8, 0x0700);
        This->Triangle.Timer = This->Triangle.TimerReload;
        This->Triangle.CounterReloadFlag = true;
    } break;


    /* noise */
    case 0x400C:
    {
        This->Noise.Looping = Byte & 0x20;
        This->Noise.ConstantVolume = Byte & 0x10;
        This->Noise.Volume = Byte & 0x0F;
    } break;
    case 0x400D: break; /* unused */
    case 0x400E:
    {
        This->Noise.DividerPeriodReload = This->NTSCNoisePeriodTable[Byte & 0x0F];
    } break;
    case 0x400F:
    {
        This->Noise.LengthCounter = This->LengthCounterTable[Byte >> 3];
        This->Noise.Start = true;
    } break;
    /* DMC */
    case 0x4010:
    case 0x4011:
    case 0x4012:
    case 0x4013:
    /* Channel enable/length counter stat */
    case 0x4015:
    {
        This->Pulse1.Enable = Byte & (1 << 0);
        This->Pulse2.Enable = Byte & (1 << 1);
        This->Triangle.Enable = Byte & (1 << 2);
        This->Noise.Enable = Byte & (1 << 3);
    } break;
    /* frame counter */
    case 0x4017:
    break;
    }
}



