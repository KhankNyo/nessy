

/*
 * before reading any line here, 
 * it is highly recommended to understand how the nes' APU works
 * https://www.nesdev.org/wiki/APU
 *
 * some part of the code below is inherently spaghetti, 
 * and that is because digital logic itself is inherently spaghetti 
 * Some examples are:
 *   reading a register changes the state of an entire system (Envelope start flag and address 0x4003, 0x4007, 0x400F)
 *   a module accessing and modifying other modules' state directly (Sweep unit updating the timer period of the entire sequencer directly)
 * */


#include "Nes.h"
#include "Utils.h"
#include "Common.h"


#define COUNTER_STOP 0xFFFF
#define MAX_VOLUME 15


typedef struct Sweeper 
{
    Bool8 EnableFlag;
    Bool8 NegateFlag;
    Bool8 ReloadFlag;
    Bool8 MutingFlag;

    u16 ShiftCount;
    u16 ShiftedTimerPeriod;
    u16 ClkDivider;
} Sweeper;

typedef struct Envelope
{
    Bool8 ConstantVolumeFlag;
    Bool8 LoopFlag;
    Bool8 EnableFlag;
    Bool8 StartFlag;

    u16 ClkDivider;
    u16 TmpClkDivider;
    u16 LengthCounter;

    u8 Volume;
    u8 VolumeDecayCounter;
} Envelope;

typedef struct Sequencer 
{
    double DutyCyclePercentage;
    u16 TmpTimerPeriod;
    u16 TimerPeriod;

    /* volume ctrl */
    Envelope Envelope;

    /* pitch ctrl */
    Sweeper Sweeper;
} Sequencer;

typedef struct TriangleSequencer 
{
    Bool8 Ctrl;
    Bool8 LinearCounterReloadFlag;
    Bool8 EnableFlag;
    u8 TmpLinearCounter;
    u8 LinearCounter;
    u16 TmpTimerPeriod;
    u16 TimerPeriod;
} TriangleSequencer;

typedef struct NoiseSequencer 
{
    u16 Timer;
    u16 LinearFeedbackShiftRegister:15;
    Bool8 ModeFlag;

    /* volume ctrl */
    Envelope Envelope;
} NoiseSequencer;


typedef struct NESAPU 
{
    Sequencer Pulse1;
    Sequencer Pulse2;
    TriangleSequencer Triangle;
    NoiseSequencer Noise;

    u64 ClockCounter;
    u64 FrameClockCounter;

    double AudioSample;
    double ElapsedTime;
    u8 LengthCounterTable[0x20];
    u16 NTSCNoisePeriodTable[0x10];
} NESAPU;



void NESAPU_Reset(NESAPU *This)
{
    This->ElapsedTime = 0;
    This->Pulse1    = (Sequencer) { 0 };
    This->Pulse2    = (Sequencer) { 0 };
    This->Triangle  = (TriangleSequencer) { 0 };
    This->Noise     = (NoiseSequencer) { 
        .LinearFeedbackShiftRegister = 1 
    };
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




/* sample generator */


typedef double (*SampleGenerator)(Sequencer *, double t);

static double GenerateSquareWave(Sequencer *Seq, double t)
{
    /* 16.0: nameless magic, consult nesdev */
    double Freq = NES_CPU_CLK / (16.0 * (double)(Seq->TimerPeriod + 1));
    t *= Freq;
    double FractionalPart = t - (i64)t;
    return FractionalPart < Seq->DutyCyclePercentage? 
        1.0 : -1.0;
}


static double NESAPU_SequenceSample(Sequencer *Seq, double t, SampleGenerator SampleGenerator)
{
    double Sample = 0;
    Sample = SampleGenerator(Seq, t);
    return Sample;
}




/* square wave sequencer */

static void NESAPU_EnvelopeUpdate(Envelope *VolumeCtrl)
{
    if (!VolumeCtrl->StartFlag)
    {
        /* clock the divider */
        if (VolumeCtrl->ClkDivider == 0)
        {
            VolumeCtrl->ClkDivider = VolumeCtrl->Volume;
            if (VolumeCtrl->VolumeDecayCounter)
                VolumeCtrl->VolumeDecayCounter--;
            else if (VolumeCtrl->LoopFlag)
                VolumeCtrl->VolumeDecayCounter = MAX_VOLUME;
        }
        else
        {
            VolumeCtrl->ClkDivider--;
        }
    }
    else
    {
        VolumeCtrl->StartFlag = false;
        VolumeCtrl->VolumeDecayCounter = MAX_VOLUME;
        VolumeCtrl->ClkDivider = VolumeCtrl->TmpClkDivider;

    }


    if (!VolumeCtrl->EnableFlag)
    {
        VolumeCtrl->LengthCounter = 0;
    }
    else if (VolumeCtrl->LengthCounter && !VolumeCtrl->LoopFlag)
    {
        VolumeCtrl->LengthCounter--;
    }
}

static double NESAPU_SequencerGetVolume(Sweeper *PitchCtrl, Envelope *VolumeCtrl, u16 CurrentTimerPeriod)
{
    PitchCtrl->MutingFlag = 
        CurrentTimerPeriod < 8 /* nameless magic, consult nesdev Sweeper section for details */
        || PitchCtrl->ShiftedTimerPeriod > 0x07FF; /* yet another nameless magic, consult Sweeper section of nesdev */
    if (PitchCtrl->MutingFlag || !VolumeCtrl->EnableFlag || 0 == VolumeCtrl->LengthCounter)
        return 0;

    if (VolumeCtrl->ConstantVolumeFlag)
        return (double)VolumeCtrl->Volume * (1.0 / MAX_VOLUME);
    return (double)VolumeCtrl->VolumeDecayCounter * (1.0 / MAX_VOLUME);
}



/* triangle wave sequencer (pretty much a singleton here) */

static void NESAPU_TriangleSequencerUpdate(TriangleSequencer *Seq)
{
    if (Seq->LinearCounterReloadFlag)
    {
        Seq->LinearCounter = Seq->TmpLinearCounter;
    }
    else if (Seq->LinearCounter)
    {
        Seq->LinearCounter--;
    }

    if (!Seq->Ctrl)
    {
        Seq->LinearCounterReloadFlag = false;
    }
}

static double NESAPU_GetTriangleSample(TriangleSequencer *Seq, double t)
{
    if (!Seq->LinearCounter || !Seq->EnableFlag)
        return 0;

    /* another nameless magic, please for the love of god consult APU Triangle sequencer in nesdev wiki */
    double Freq = NES_CPU_CLK / (32.0 * (double)(Seq->TimerPeriod + 1));
    t *= Freq;
    t -= (i64)t;
    if (t < .5)
        return t;
    return -t + 1;
}



/* sweeper */

static void NESAPU_SweeperUpdate(Sweeper *PitchCtrl, u16 CurrentTimerPeriod, Bool8 UseOnesComplement)
{
    uint ChangeAmount = CurrentTimerPeriod >> PitchCtrl->ShiftCount;
    if (PitchCtrl->NegateFlag)
    {
        ChangeAmount = UseOnesComplement? 
            ~ChangeAmount
            : -ChangeAmount;
    }

    u16 NewPeriod = CurrentTimerPeriod + ChangeAmount;
    if (NewPeriod & 0x8000) /* sign bit */
        NewPeriod = 0;
    PitchCtrl->ShiftedTimerPeriod = NewPeriod;
}

static u16 NESAPU_SweeperUpdateTimerPeriod(Sweeper *PitchCtrl, u16 CurrentTimerPeriod, u16 EnvelopeClkDivider)
{
    u16 NewTimerPeriod = CurrentTimerPeriod;
    if (PitchCtrl->ClkDivider == 0 && PitchCtrl->EnableFlag && PitchCtrl->ShiftCount != 0)
    {
        if (!PitchCtrl->MutingFlag)
            NewTimerPeriod = PitchCtrl->ShiftedTimerPeriod;
        /* else the period remain unchanged */
    }
    if (PitchCtrl->ClkDivider == 0 || PitchCtrl->ReloadFlag)
    {
        PitchCtrl->ClkDivider = EnvelopeClkDivider;
        PitchCtrl->ReloadFlag = false;
    }
    else PitchCtrl->ClkDivider--;

    return NewTimerPeriod;
}



/* noise */

static double NESAPU_GetNoiseSample(NoiseSequencer *Noise, double t)
{
    if ((Noise->LinearFeedbackShiftRegister & 0x1) || Noise->Envelope.LengthCounter == 0)
        return 0;
    return (double)Noise->Envelope.Volume * (1.0 / MAX_VOLUME);
}

static void NESAPU_NoiseUpdateShiftRegister(NoiseSequencer *Noise)
{
    u16 TheOtherBit = Noise->ModeFlag
        ? Noise->LinearFeedbackShiftRegister >> 6 
        : Noise->LinearFeedbackShiftRegister >> 1; 
    u16 Feedback = Noise->LinearFeedbackShiftRegister ^ TheOtherBit;
    Feedback &= 0x0001;

    Noise->LinearFeedbackShiftRegister >>= 1;
    Noise->LinearFeedbackShiftRegister |= Feedback << 14;
}






void NESAPU_StepClock(NESAPU *This)
{
#define IS_HALF_CLK(Clk) ((Clk) == 7457 || (Clk) == 14916)
#define IS_QUARTER_CLK(Clk) (IS_HALF_CLK(Clk) || ((Clk) == 3729 || (Clk) == 11186))

    /* the noise sequencer is updated every cpu clk??? */
    if (This->ClockCounter % 3 == 0)
    {
        NESAPU_NoiseUpdateShiftRegister(&This->Noise);
    }

    if (This->ClockCounter % 6 == 0)
    {
        This->ElapsedTime += 2.0 / NES_CPU_CLK;
        This->FrameClockCounter++;


        /* NOTE: sweeper is updated every frame clk (every 2 CPU clk) */
        NESAPU_SweeperUpdate(&This->Pulse1.Sweeper, This->Pulse1.TimerPeriod, true);
        NESAPU_SweeperUpdate(&This->Pulse2.Sweeper, This->Pulse2.TimerPeriod, false);


        /* adjust volume envelope */
        if (IS_QUARTER_CLK(This->FrameClockCounter))
        {
            NESAPU_EnvelopeUpdate(&This->Pulse1.Envelope);
            NESAPU_EnvelopeUpdate(&This->Pulse2.Envelope);
            NESAPU_TriangleSequencerUpdate(&This->Triangle);
            NESAPU_EnvelopeUpdate(&This->Noise.Envelope);
        }
        /* note length and freq sweep */
        if (IS_HALF_CLK(This->FrameClockCounter))
        {
            This->Pulse1.TimerPeriod = NESAPU_SweeperUpdateTimerPeriod(
                &This->Pulse1.Sweeper, 
                This->Pulse1.TimerPeriod, 
                This->Pulse1.Envelope.TmpClkDivider
            );
            This->Pulse2.TimerPeriod = NESAPU_SweeperUpdateTimerPeriod(
                &This->Pulse2.Sweeper,
                This->Pulse2.TimerPeriod,
                This->Pulse2.Envelope.TmpClkDivider
            );
        }
        /* the end of frame, reset the frame clk */
        if (This->FrameClockCounter == 14916)
            This->FrameClockCounter = 0;


        double Pulse1 = 
            NESAPU_SequencerGetVolume(&This->Pulse1.Sweeper, &This->Pulse1.Envelope, This->Pulse1.TimerPeriod)
            * NESAPU_SequenceSample(&This->Pulse1, This->ElapsedTime, GenerateSquareWave); 
        double Pulse2 = 
            NESAPU_SequencerGetVolume(&This->Pulse2.Sweeper, &This->Pulse2.Envelope, This->Pulse2.TimerPeriod)
            * NESAPU_SequenceSample(&This->Pulse2, This->ElapsedTime, GenerateSquareWave);
        double Triangle = NESAPU_GetTriangleSample(&This->Triangle, This->ElapsedTime);
        double Noise = NESAPU_GetNoiseSample(&This->Noise, This->ElapsedTime);
        double DMC = 0;


        /* sound mixer, a bunch of magic, consult APU mixer section of nesdev for details */
        This->AudioSample = INT16_MAX 
            * (0.0752*(Pulse1 + Pulse2) 
             + 0.0835*Triangle 
             + 0.0494*Noise 
             + 0.0335*DMC
        );
    }
    This->ClockCounter++;
#undef IS_QUARTER_CLK
#undef IS_HALF_CLK
}



/* the following code has even more magic numbers, 
 * please please PLEASE consult APU section of nesdev */


u8 NESAPU_ExternalRead(NESAPU *This, u16 Addr)
{
    switch (Addr)
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



static void NESAPU_WritePulseRegisters(NESAPU *This, Sequencer *Pulse, u16 Addr, u8 Byte)
{
    static const double DutyCycleLookup[4] = { .125, .250, .500, .750 };
    switch (Addr)
    {
    case 0x4000:
    case 0x4004:
    {
        Pulse->DutyCyclePercentage = DutyCycleLookup[Byte >> 6];
        Pulse->Envelope.LoopFlag = Byte & (1 << 5);
        Pulse->Envelope.ConstantVolumeFlag = Byte & (1 << 4);
        Pulse->Envelope.Volume = Byte & 0x0F;
    } break;
    case 0x4001:
    case 0x4005:
    {
        Pulse->Envelope.TmpClkDivider = (Byte >> 4) & 0x7;
        Pulse->Sweeper.EnableFlag = Byte & 0x80;
        Pulse->Sweeper.NegateFlag = Byte & 0x08;
        Pulse->Sweeper.ShiftCount = Byte & 0x07;
        Pulse->Sweeper.ReloadFlag = true;
    } break;
    case 0x4002:
    case 0x4006:
    {
        MASKED_LOAD(Pulse->TmpTimerPeriod, Byte, 0x00FF);
    } break;
    case 0x4003:
    case 0x4007:
    {
        MASKED_LOAD(Pulse->TmpTimerPeriod, (u16)Byte << 8, 0x0700);
        Pulse->TimerPeriod = Pulse->TmpTimerPeriod;
        Pulse->Envelope.LengthCounter = This->LengthCounterTable[Byte >> 3];
        Pulse->Envelope.StartFlag = true;
    } break;
    }
}

void NESAPU_ExternalWrite(NESAPU *This, u16 Addr, u8 Byte)
{
    if (IN_RANGE(0x4000, Addr, 0x4003))
        NESAPU_WritePulseRegisters(This, &This->Pulse1, Addr, Byte);
    else if (IN_RANGE(0x4004, Addr, 0x4007))
        NESAPU_WritePulseRegisters(This, &This->Pulse2, Addr, Byte);
    else switch (Addr)
    {
    /* triangle */
    case 0x4008:
    {
        This->Triangle.Ctrl = Byte & 0x80;
        This->Triangle.TmpLinearCounter = Byte & 0x7F;
    } break;
    case 0x4009:
    {
        /* unused */
    } break;
    case 0x400A:
    {
        MASKED_LOAD(This->Triangle.TmpTimerPeriod, Byte, 0x00FF);
    } break;
    case 0x400B:
    {
        MASKED_LOAD(This->Triangle.TmpTimerPeriod, (u16)Byte << 8, 0x0700);
        This->Triangle.TimerPeriod = This->Triangle.TmpTimerPeriod;
        This->Triangle.LinearCounterReloadFlag = true;
    } break;


    /* noise */
    case 0x400C:
    {
        This->Noise.Envelope.LoopFlag            = Byte & 0x20;
        This->Noise.Envelope.ConstantVolumeFlag  = Byte & 0x10;
        This->Noise.Envelope.Volume              = Byte & 0x0F;
    } break;
    case 0x400D: 
    {
        /* unused */
    } break; 
    case 0x400E:
    {
        This->Noise.Timer = This->NTSCNoisePeriodTable[Byte & 0x0F];
        This->Noise.ModeFlag = Byte & 0x80;
    } break;
    case 0x400F:
    {
        This->Noise.Envelope.LengthCounter = This->LengthCounterTable[Byte >> 3];
        This->Noise.Envelope.StartFlag = true;
    } break;


    /* DMC */
    case 0x4010:
    case 0x4011:
    case 0x4012:
    case 0x4013:
    {
    } break;


    /* Channel enable/length counter stat */
    case 0x4015:
    {
        This->Pulse1.Envelope.EnableFlag    = Byte & (1 << 0);
        This->Pulse2.Envelope.EnableFlag    = Byte & (1 << 1);
        This->Triangle.EnableFlag           = Byte & (1 << 2);
        This->Noise.Envelope.EnableFlag     = Byte & (1 << 3);
    } break;


    /* frame counter */
    case 0x4017:
    {
    } break;
    }
}



