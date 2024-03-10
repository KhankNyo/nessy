
#include "Nes.h"
#include "Utils.h"
#include "Common.h"


typedef struct Sequencer 
{
    Bool8 Enable;
    Bool8 Looping;
    u16 Reload;
    u16 Timer;
    u16 LengthCounter;
    double DutyCycle;
} Sequencer;

typedef struct NESAPU 
{
    Sequencer Pulse1;
    Sequencer Pulse2;
    Sequencer Triangle;
    Sequencer Noise;

    u64 ClockCounter;
    u64 FrameClockCounter;

    double AudioSample;
    double ElapsedTime;
    u8 LengthCounterTable[0x20];

    u32 RandSeed;
} NESAPU;


NESAPU NESAPU_Init(u64 SeedForNoiseGeneration)
{
    NESAPU APU = {
        /* https://www.nesdev.org/wiki/APU_Length_Counter */
        .LengthCounterTable = {
            10, 254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14, 
            12,  16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
        },
        .RandSeed = SeedForNoiseGeneration,
    };
    return APU;
}



static double VariableDutySquareWave(double t, double DutyCycle, int HarmonicCount)
{
    double y1 = 0;
    double y2 = 0;
    for (int i = 1; i <= HarmonicCount; i++)
    {
        double x = i*t;
        y1 += -Sint64(x) / i;
        y2 += -Sint64(x - DutyCycle*i) / i;
    }
    return (2.0 / PI) * (y1 - y2);
}


static double NESAPU_SequencerGenerateSquareSample(Sequencer *Seq, double t, int HarmonicCount)
{
    double Sample = 0;
    if (Seq->Enable || Seq->LengthCounter)
    {
        double Frequency = NES_CPU_CLK / (16.0 * (double)(Seq->Timer + 1));
        Sample = VariableDutySquareWave(Frequency*t, Seq->DutyCycle, HarmonicCount);
        if (!Seq->Looping)
            Seq->LengthCounter--;
        if (0 == Seq->LengthCounter)
            Seq->Enable = false;
    }

    return Sample;
}

static double NESAPU_SequencerGenerateTriangleSample(Sequencer *Seq, double t, int HarmonicCount)
{
    double Sample = 0;
    if (Seq->Enable || Seq->LengthCounter)
    {
        /* half that of the square sample */
        double Frequency = NES_CPU_CLK / (32.0 * (double)(Seq->Timer + 1));
        for (int i = 1; i <= HarmonicCount; i++)
        {
            double x = t*i*Frequency;
            Sample += Sint64(x) / i;
        }
        Sample *= (2.0 / PI);

        if (!Seq->Looping)
            Seq->LengthCounter--;
        if (0 == Seq->LengthCounter)
            Seq->Enable = false;
    }
    return Sample;
}

static double NESAPU_SequencerGenerateNoiseSample(Sequencer *Seq, u32 *Seed)
{
    double Sample = 0;
    if (Seq->Enable || Seq->LengthCounter)
    {
        Sample = (double)Rand(Seed) * (5.0 / UINT32_MAX);
        if (!Seq->Looping)
        {
            Seq->LengthCounter--;
        }
    }
    else Seq->Enable = false;
    return Sample;
}


void NESAPU_StepClock(NESAPU *This)
{
#define IS_QUARTER_CLK(Clk) ((Clk) == 3729 || (Clk) == 7457 || (Clk) == 11186 || (Clk) == 14916)
#define IS_HALF_CLK(Clk) ((Clk) == 7457 || (Clk) == 14916)

    This->ElapsedTime += .3333333333333333 / NES_CPU_CLK;
    if (This->ClockCounter % 6 == 0)
    {
        This->FrameClockCounter++;
        /* adjust volume envelope */
        if (IS_QUARTER_CLK(This->FrameClockCounter))
        {
        }
        /* note length and freq sweep */
        if (IS_HALF_CLK(This->FrameClockCounter))
        {
        }

        /* clock pulse 1 */
        double Pulse1 = NESAPU_SequencerGenerateSquareSample(&This->Pulse1, This->ElapsedTime, 20);
        double Pulse2 = NESAPU_SequencerGenerateSquareSample(&This->Pulse2, This->ElapsedTime, 20);
        double Triangle = NESAPU_SequencerGenerateTriangleSample(&This->Triangle, This->ElapsedTime, 20);
        double Noise = 0;//NESAPU_SequencerGenerateNoiseSample(&This->Noise, &This->RandSeed);
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

void NESAPU_Reset(NESAPU *This)
{
    This->ElapsedTime = 0;
    This->Pulse1.Enable = false;
    This->Pulse2.Enable = false;
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
    } break;
    case 0x4001:
    {
    } break;
    case 0x4002:
    {
        MASKED_LOAD(This->Pulse1.Reload, Byte, 0x00FF);
    } break;
    case 0x4003:
    {
        MASKED_LOAD(This->Pulse1.Reload, (u16)Byte << 8, 0x0700);
        This->Pulse1.Timer = This->Pulse1.Reload;
        This->Pulse1.LengthCounter = This->LengthCounterTable[Byte >> 3];
    } break;

    /* pulse 2 */
    case 0x4004:
    {
        This->Pulse2.DutyCycle = DutyCycleLookup[Byte >> 6];
        This->Pulse2.Looping = Byte & 0x20;
    } break;
    case 0x4005:
    case 0x4006:
    {
        MASKED_LOAD(This->Pulse2.Reload, Byte, 0x00FF);
    } break;
    case 0x4007:
    {
        MASKED_LOAD(This->Pulse2.Reload, (u16)Byte << 8, 0x0700);
        This->Pulse2.Timer = This->Pulse2.Reload;
        This->Pulse2.LengthCounter = This->LengthCounterTable[Byte >> 3];
    } break;

    /* triangle */
    case 0x4008:
    {
        This->Triangle.Looping = Byte & 0x80;
    } break;
    case 0x4009: break; /* unused */
    case 0x400A:
    {
        MASKED_LOAD(This->Triangle.Reload, Byte, 0x00FF);
    } break;
    case 0x400B:
    {
        MASKED_LOAD(This->Triangle.Reload, (u16)Byte << 8, 0x0700);
        This->Triangle.Timer = This->Triangle.Reload;
        This->Triangle.LengthCounter = This->LengthCounterTable[Byte >> 3];
    } break;
    /* noise */
    case 0x400C:
    {
        This->Noise.Looping = Byte & 0x20;
    } break;
    case 0x400D: break; /* unused */
    case 0x400E:
    {
        This->Noise.Looping = Byte & 0x20;
    } break;
    case 0x400F:
    {
        This->Noise.LengthCounter = This->LengthCounterTable[Byte >> 3];
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



