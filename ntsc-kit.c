#include <math.h>
#include <string.h>
#include "ntsc-kit.h"
#include "ntsc-kit-platform.h"

// Define this as tightly cpu-coupled RAM, does not need to be
// DMA-able, e.g. __attribute__((section (".ccmram"))) on STM parts
#ifndef PLACEMENT_FAST_RAM
#define PLACEMENT_FAST_RAM
#endif // ifndef PLACEMENT_FAST_RAM

// These should be in tightly coupled memory to reduce contention with RAM during DMA 
#define LINE_SAMPLE_STORAGE_MAX 1368
static uint8_t PLACEMENT_FAST_RAM NTSCEqSyncPulseLine[LINE_SAMPLE_STORAGE_MAX];
static uint8_t PLACEMENT_FAST_RAM NTSCVSyncLine[LINE_SAMPLE_STORAGE_MAX];
static uint8_t PLACEMENT_FAST_RAM NTSCBlankLineBW[LINE_SAMPLE_STORAGE_MAX];
static uint8_t PLACEMENT_FAST_RAM NTSCBlankLineColorEven[LINE_SAMPLE_STORAGE_MAX];
static uint8_t PLACEMENT_FAST_RAM NTSCBlankLineColorOdd[LINE_SAMPLE_STORAGE_MAX];

static uint8_t NTSCSyncTip;
static uint8_t NTSCSyncPorch;
static uint8_t NTSCBlack;
static uint8_t NTSCWhite;

static uint8_t NTSCColorburst0;
static uint8_t NTSCColorburst60;
static uint8_t NTSCColorburst90;
static uint8_t NTSCColorburst120;
static uint8_t NTSCColorburst180;
static uint8_t NTSCColorburst240;
static uint8_t NTSCColorburst270;
static uint8_t NTSCColorburst300;

typedef struct NTSCModeTiming
{
    NTSCLineConfig config;
    int line_samples;
    int colorburst_oversampling;
    int eq_pulse;
    int vsync;
    int hsync;
    int front_porch;
    int back_porch;
    int colorburst_offset;
    int app_line_offset;
    int app_line_samples;
} NTSCModeTiming;

static int NTSCCurrentLineConfig = NTSC_LINE_SAMPLES_UNINITIALIZED;
static bool NTSCModeInterlaced = true;
static NTSCModeTiming NTSCCurrentTiming;

static bool NTSCModeFuncsValid = false;

static void* NTSCModePrivateData = NULL;
static NTSCModeFiniFunc NTSCModeFinalize = NULL;
static NTSCModeFillLineBufferFunc NTSCModeFillLineBuffer = NULL;
static NTSCModeInitFunc NTSCModeInit = NULL;
static NTSCModeNeedsColorburstFunc NTSCModeNeedsColorburst = NULL;

static void NTSCCalculateLineClocks(NTSCModeTiming* timing, NTSCLineConfig config)
{
    switch(config)
    {
        case NTSC_LINE_SAMPLES_910:
            timing->colorburst_oversampling = 4;
            timing->line_samples = 910;
            timing->colorburst_offset = 76;
            timing->app_line_offset = 164;
            timing->app_line_samples = 704;
            break;
        case NTSC_LINE_SAMPLES_912:
            timing->colorburst_oversampling = 4;
            timing->line_samples = 912;
            timing->colorburst_offset = 76;
            timing->app_line_offset = 164;
            timing->app_line_samples = 704;
            break;
        case NTSC_LINE_SAMPLES_1368:
            timing->colorburst_oversampling = 4;
            timing->line_samples = 1368;
            timing->colorburst_offset = 114;
            timing->app_line_offset = 246;
            timing->app_line_samples = 1056;
            break;
    }
    timing->hsync = floorf(timing->line_samples * NTSC_HOR_SYNC_DUR + 0.5);
    timing->front_porch = timing->line_samples * NTSC_FRONTPORCH;
    timing->back_porch = timing->line_samples * NTSC_BACKPORCH;
    timing->eq_pulse = timing->line_samples * NTSC_EQ_PULSE_INTERVAL;
    timing->vsync = timing->line_samples * NTSC_VSYNC_BLANK_INTERVAL;
}

static void NTSCFillEqPulseLine(NTSCModeTiming *timing, unsigned char *lineBuffer)
{
    for (int col = 0; col < timing->line_samples; col++) {
        if (col < timing->eq_pulse || (col > timing->line_samples/2 && col < timing->line_samples/2 + timing->eq_pulse)) {
            lineBuffer[col] = NTSCSyncTip;
        } else {
            lineBuffer[col] = NTSCSyncPorch;
        }
    }
}

static void NTSCFillVSyncLine(NTSCModeTiming *timing, unsigned char *lineBuffer)
{
    for (int col = 0; col < timing->line_samples; col++) {
        if (col < timing->vsync || (col > timing->line_samples/2 && col < timing->line_samples/2 + timing->vsync)) {
            lineBuffer[col] = NTSCSyncTip;
        } else {
            lineBuffer[col] = NTSCSyncPorch;
        }
    }
}

// XXX alternate colorburst for alternate lines if necessary for timing
static void NTSCAddColorburst(NTSCModeTiming* timing, unsigned char *lineBuffer, int line)
{
    int cbstart = timing->colorburst_offset;

    if(timing->line_samples == 910) 
    {
        int sample_phase_offset = (line % 2 == 0) ? 0 : 2;
        for(int col = cbstart; col < cbstart + NTSC_COLORBURST_CYCLES * 4 ; col++)
        {
            switch((col + sample_phase_offset) % 4) {
                case 0: lineBuffer[col] = NTSCColorburst0; break;
                case 1: lineBuffer[col] = NTSCColorburst90; break;
                case 2: lineBuffer[col] = NTSCColorburst180; break;
                case 3: lineBuffer[col] = NTSCColorburst270; break;
            }
        }
    }
    else if(timing->line_samples == 912) 
    {
        for(int col = cbstart; col < cbstart + NTSC_COLORBURST_CYCLES * 4 ; col++)
        {
            switch(col % 4) {
                case 0: lineBuffer[col] = NTSCColorburst0; break;
                case 1: lineBuffer[col] = NTSCColorburst90; break;
                case 2: lineBuffer[col] = NTSCColorburst180; break;
                case 3: lineBuffer[col] = NTSCColorburst270; break;
            }
        }
    }
    else if(timing->line_samples == 1368) 
    {
        for(int col = cbstart; col < cbstart + NTSC_COLORBURST_CYCLES * 6; col++)
        {
            switch(col % 6) {
                case 0: lineBuffer[col] = NTSCColorburst0; break;
                case 1: lineBuffer[col] = NTSCColorburst60; break;
                case 2: lineBuffer[col] = NTSCColorburst120; break;
                case 3: lineBuffer[col] = NTSCColorburst180; break;
                case 4: lineBuffer[col] = NTSCColorburst240; break;
                case 5: lineBuffer[col] = NTSCColorburst300; break;
            }
        }
    }
}

static void NTSCFillBlankLine(NTSCModeTiming *timing, unsigned char *lineBuffer, bool withColorburst, int lineNumber)
{
    memset(lineBuffer, NTSCBlack, timing->line_samples);
    for (int col = 0; col < timing->line_samples; col++)
    {
        if (col < timing->hsync) {
            lineBuffer[col] = NTSCSyncTip;
        } else if(col < timing->hsync + timing->back_porch) {
            lineBuffer[col] = NTSCSyncPorch;
        } else if(col >= timing->line_samples - timing->front_porch) {
            lineBuffer[col] = NTSCSyncPorch;
        } else {
            lineBuffer[col] = NTSCBlack;
        }
    }
    if(withColorburst)
    {
        NTSCAddColorburst(timing, lineBuffer, lineNumber);
    }
}

static void NTSCGenerateLineBuffers(NTSCModeTiming *timing)
{
    // one line = (1 / 3579545) * (455/2)

    NTSCFillEqPulseLine(timing, NTSCEqSyncPulseLine);
    NTSCFillVSyncLine(timing, NTSCVSyncLine);
    NTSCFillBlankLine(timing, NTSCBlankLineBW, 0, 0);
    NTSCFillBlankLine(timing, NTSCBlankLineColorEven, 1, 0);
    NTSCFillBlankLine(timing, NTSCBlankLineColorOdd, 1, 1);
}

// XXX Need to optimize this for bandwidth and CPU cycles by copying only the lines at change
void NTSCFillLineBuffer(int frameNumber, int lineNumber, unsigned char *lineBuffer)
{
    NTSCModeTiming *timing = &NTSCCurrentTiming;

    uint8_t *blankLine;
    if(NTSCModeFuncsValid && NTSCModeNeedsColorburst()) {
        blankLine = ((frameNumber + lineNumber) % 2 == 0) ? NTSCBlankLineColorEven : NTSCBlankLineColorOdd;
    } else {
        blankLine = NTSCBlankLineBW;
    }

    if(lineNumber < NTSC_EQPULSE_LINES)
    {

        // 3 lines of equalizing pulse
        memcpy(lineBuffer, NTSCEqSyncPulseLine, timing->line_samples);

    }
    else if(lineNumber - NTSC_EQPULSE_LINES < NTSC_VSYNC_LINES)
    {

        // 3 lines of VSYNC
        memcpy(lineBuffer, NTSCVSyncLine, timing->line_samples);

    }
    else if(lineNumber - (NTSC_EQPULSE_LINES + NTSC_VSYNC_LINES) < NTSC_EQPULSE_LINES)
    {

        // 3 lines of equalizing pulse
        memcpy(lineBuffer, NTSCEqSyncPulseLine, timing->line_samples);

    }
    else if(lineNumber - (NTSC_EQPULSE_LINES + NTSC_VSYNC_LINES + NTSC_EQPULSE_LINES) < NTSC_VBLANK_LINES)
    {
        /* first field 12 blank lines (closed captioning would be on last line) */

        memcpy(lineBuffer, blankLine, timing->line_samples);

    } else if(lineNumber >= 263 && lineNumber <= 271) {

        // eq and vsync for other field in interlace

        // According to VCR this is wrong:
        // REMINDER : SCOPE NUMBERING IS 1-BASED (these following line numbers are corrected to 0-based)
        // 262 first half is from last line of field 0 video
        // Otherwise the next 9 lines (in half-lines) are the same as field 0 lines 0-8
        // And line 284 is half-blank, with content in the second half

        // Handle interlace half line and vertical retrace and sync.
        if(lineNumber <= 264) {
            // lines 263, 264 - last 405 of even field eq pulse then first 405 of eq pulse
            memcpy(lineBuffer, NTSCEqSyncPulseLine + timing->line_samples / 2, timing->line_samples / 2);
            memcpy(lineBuffer + timing->line_samples / 2, NTSCEqSyncPulseLine, timing->line_samples / 2);
        } else if(lineNumber == 265) {
            // line 265 - last 405 of even field eq pulse then first 405 of vsync
            memcpy(lineBuffer, NTSCEqSyncPulseLine + timing->line_samples / 2, timing->line_samples / 2);
            memcpy(lineBuffer + timing->line_samples / 2, NTSCVSyncLine, timing->line_samples / 2);
        } else if(lineNumber <= 267) {
            // lines 266, 267 - last 405 of even field vsync then first 405 of vsync
            memcpy(lineBuffer, NTSCVSyncLine + timing->line_samples / 2, timing->line_samples / 2);
            memcpy(lineBuffer + timing->line_samples / 2, NTSCVSyncLine, timing->line_samples / 2);
        } else if(lineNumber == 268) {
            // even vield lines 268 - last 405 of even field vsync then first 405 of eq pulse
            memcpy(lineBuffer, NTSCVSyncLine + timing->line_samples / 2, timing->line_samples / 2);
            memcpy(lineBuffer + timing->line_samples / 2, NTSCEqSyncPulseLine, timing->line_samples / 2);
        } else if(lineNumber <= 270) {
            // lines 269, 270 - last 405 of even field eq pulse then first 405 of eq pulse
            memcpy(lineBuffer, NTSCEqSyncPulseLine + timing->line_samples / 2, timing->line_samples / 2);
            memcpy(lineBuffer + timing->line_samples / 2, NTSCEqSyncPulseLine, timing->line_samples / 2);
        } else if(lineNumber == 271) {
            // line 271 - last 405 of even field eq pulse then 405 of SyncPorch
            memcpy(lineBuffer, NTSCEqSyncPulseLine + timing->line_samples / 2, timing->line_samples / 2);
            memset(lineBuffer + timing->line_samples / 2, NTSCSyncPorch, timing->line_samples / 2);
        }

    } else if((lineNumber >= 272) && (lineNumber <= 281)) { // XXX half line at 282
    // } else if(lineNumber == 272) { // XXX half line at 282

        /*
         * Lines 272 through 2XX are other part of vertical blank
         */

        // even field vertical safe area
        memcpy(lineBuffer, blankLine, timing->line_samples);

    }
    else // if(
        // ((lineNumber >= NTSC_EQPULSE_LINES + NTSC_VSYNC_LINES + NTSC_EQPULSE_LINES + NTSC_VBLANK_LINES) && (lineNumber < 263))
        // ||
        // ( lineNumber >= 282))
    {

        // if((lineNumber == (NTSC_EQPULSE_LINES + NTSC_VSYNC_LINES + NTSC_EQPULSE_LINES + NTSC_VBLANK_LINES)) || (lineNumber >= 282))
        // {
            memcpy(lineBuffer, blankLine, timing->line_samples);
        // }

        int lineWithinFrame;
        if(NTSCModeInterlaced) {
            lineWithinFrame = (lineNumber % 263) * 2 + lineNumber / 263 - 22;
        } else {
            lineWithinFrame = lineNumber % 263 - 22;
        }

        if(NTSCModeFuncsValid) {
            NTSCModeFillLineBuffer(frameNumber, lineNumber, lineWithinFrame, timing->app_line_samples, lineBuffer + timing->app_line_offset);
        }

        if((lineNumber == 262) && NTSCModeInterlaced) {
            // interlacing, line 262 - overwrite last 405 samples with first 405 samples of EQ pulse
            memcpy(lineBuffer + timing->line_samples / 2, NTSCEqSyncPulseLine, timing->line_samples / 2);
        } else if((lineNumber == 282) && NTSCModeInterlaced) {
            // interlacing, special line 282 - write SyncPorch from BackPorch to middle of line after mode's fillLines()
            memset(lineBuffer + timing->hsync + timing->back_porch, NTSCSyncPorch, timing->line_samples / 2 - (timing->hsync + timing->back_porch));
        }
    }
}

void NTSCSetMode(bool interlaced, NTSCLineConfig line_config, void* private_data, NTSCModeInitFunc initFunc, NTSCModeFiniFunc finiFunc, NTSCModeFillLineBufferFunc fillBufferFunc, NTSCModeNeedsColorburstFunc needsColorBurstFunc)
{
    int same_config = 
        (NTSCModeInterlaced == interlaced) &&
        (initFunc == NTSCModeInit) &&
        (finiFunc == NTSCModeFinalize) &&
        (fillBufferFunc == NTSCModeFillLineBuffer) &&
        (needsColorBurstFunc == NTSCModeNeedsColorburst) &&
        ((int)line_config == NTSCCurrentLineConfig);

    if(NTSCModeFuncsValid && same_config)
    {
        return;
    }

    NTSCModeFuncsValid = false;

    if(NTSCModeFinalize != NULL)
    {
        NTSCModeFinalize(NTSCModePrivateData);
    }

    if(NTSCCurrentLineConfig != NTSC_LINE_SAMPLES_UNINITIALIZED)
    {
        PlatformDisableNTSCScanout();
    }

    NTSCCalculateLineClocks(&NTSCCurrentTiming, line_config);
    NTSCGenerateLineBuffers(&NTSCCurrentTiming);

    NTSCModeNeedsColorburst = needsColorBurstFunc;
    NTSCModeInit = initFunc;
    NTSCModeFillLineBuffer = fillBufferFunc;
    NTSCModeFinalize = finiFunc;
    NTSCModePrivateData = private_data;
    NTSCModeInterlaced = interlaced;

    initFunc(private_data, NTSCBlack, NTSCWhite);

    NTSCCurrentLineConfig = line_config;

    NTSCModeFuncsValid = true;

    PlatformEnableNTSCScanout(line_config, interlaced);
}

extern int PlatformGetNTSCLineNumber();

void NTSCWaitNextField()
{
    // Could try to conservatively guess remaining time before
    // vblank and sleep to reduce memory use
    int field0_vblank;
    int field1_vblank;
    do
    {
        int lineNumber = PlatformGetNTSCLineNumber();
        field0_vblank = (lineNumber > 257) && (lineNumber < 262);
        field1_vblank = (lineNumber > 520) && (lineNumber < NTSC_FRAME_LINES);
    } while(!field0_vblank && !field1_vblank);
}

int NTSCWaitNextLine()
{
    // Could use a platform sleep or some other sleep mechanism
    // moderated by the scanout ISR
    int lineThen = PlatformGetNTSCLineNumber();
    int lineNow;
    while((lineNow = PlatformGetNTSCLineNumber()) != lineThen);
    if(NTSCModeInterlaced)
    {
        return (lineNow + 1) % 525;
    }
    else
    {
        return (lineNow + 1) % 262;
    }
}

void NTSCInitialize()
{
    NTSCSyncTip = PlatformVoltageToDACValue(NTSC_SYNC_TIP_VOLTAGE);
    NTSCSyncPorch = PlatformVoltageToDACValue(NTSC_SYNC_PORCH_VOLTAGE);
    NTSCBlack = PlatformVoltageToDACValue(NTSC_SYNC_BLACK_VOLTAGE);
    NTSCWhite = PlatformVoltageToDACValue(NTSC_SYNC_WHITE_VOLTAGE);

    // Calculate the values for the colorburst that we'll repeat
    // The waveform is defined as sine in the FCC broadcast doc, but for
    // composite the voltages are reversed, so the waveform becomes -sine.
    // Scale amplitude of wave added to DC by .6 to match seen from other
    // sources on oscilloscope

    const float colorburst_amplitude = .6;
    // These are at intervals of 90 degrees - these values replicate
    // the colorburst at 14.31818MHz
    NTSCColorburst0 = NTSCSyncPorch;
    NTSCColorburst90 = NTSCSyncPorch - colorburst_amplitude * NTSCSyncPorch;
    NTSCColorburst180 = NTSCSyncPorch;
    NTSCColorburst270 = NTSCSyncPorch + colorburst_amplitude * NTSCSyncPorch;

    // These, plus the 0 and 180 degree values above, are intervals of 60 degrees
    // these values replicate the colorburst at 21.477270
    NTSCColorburst60 = NTSCSyncPorch - .866 * colorburst_amplitude * NTSCSyncPorch;
    NTSCColorburst120 = NTSCSyncPorch - .866 * colorburst_amplitude * NTSCSyncPorch;
    NTSCColorburst240 = NTSCSyncPorch + .866 * colorburst_amplitude * NTSCSyncPorch;
    NTSCColorburst300 = NTSCSyncPorch + .866 * colorburst_amplitude * NTSCSyncPorch;
}
