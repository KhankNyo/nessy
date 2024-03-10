
#include "Nes.h"
#include "Utils.h"
#include "Common.h"


typedef struct NESAPU 
{
    Bool8 Pulse1Enable;
    u16 Pulse1Reload;
    u16 Pulse1Timer;
    u32 Pulse1Sequence;
    double Pulse1DutyCycle;

    u32 ClockCounter;
    u32 FrameClockCounter;

    double AudioSample;
    double ElapsedTime;
} NESAPU;


NESAPU NESAPU_Init(void)
{
    NESAPU APU = {
        0
    };
    return APU;
}


static double SquarePulseSample(double Frequency, double t, double DutyCycle, int HarmonicCount)
{
    double a = 0;
    double b = 0;
    double p = DutyCycle * TAU;

    for (int n = 1; n < HarmonicCount; n++)
    {
        double c = n * Frequency * TAU * t;
        a += -Sin64(c) / n;
        b += -Sin64(c - p * n) / n;
    }
    return (2.0 / PI) * (a - b);
}

void NESAPU_StepClock(NESAPU *This)
{
#define IS_QUARTER_CLK(Clk) ((Clk) == 3729 || (Clk) == 7457 || (Clk) == 11186 || (Clk) == 14916)
#define IS_HALF_CLK(Clk) ((Clk) == 7457 || (Clk) == 14916)

    This->ElapsedTime += .3333333333333333 / 1789773;
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
        if (This->Pulse1Enable)
        {
            double Frequency = 1789773 / (16.0 * (double)(This->Pulse1Reload + 1));
            double SampleOut = 16000 * SquarePulseSample(Frequency, This->ElapsedTime, This->Pulse1DutyCycle, 20);
            This->AudioSample = SampleOut;
        }
        
        
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
    switch (Address)
    {
    /* pulse 1 */
    case 0x4000:
    {
        static const u8 SequenceLookup[4] = { 0x01, 0x03, 0x0F, 0xFC };
        static const double DutyCycleLookup[4] = { .125, .250, .500, .750 };
        This->Pulse1Sequence = SequenceLookup[Byte >> 6];
        This->Pulse1DutyCycle = DutyCycleLookup[Byte >> 6];
    } break;
    case 0x4001:
    {
    } break;
    case 0x4002:
    {
        This->Pulse1Reload = (This->Pulse1Reload & 0xFF00) | Byte;
    } break;
    case 0x4003:
    {
        This->Pulse1Reload = (This->Pulse1Reload & 0x00FF) | (((u16)Byte << 8) & 0x0700);
        This->Pulse1Timer = This->Pulse1Reload;
    } break;
    /* pulse 2 */
    case 0x4004:
    case 0x4005:
    case 0x4006:
    case 0x4007:
    /* triangle */
    case 0x4008:
    case 0x4009:
    case 0x400A:
    case 0x400B:
    /* noise */
    case 0x400C:
    case 0x400D:
    case 0x400E:
    case 0x400F:
    /* DMC */
    case 0x4010:
    case 0x4011:
    case 0x4012:
    case 0x4013:
    /* Channel enable/length counter stat */
    case 0x4015:
    {
        This->Pulse1Enable = Byte & 0x01;
    } break;
    /* frame counter */
    case 0x4017:
    break;
    }
}



