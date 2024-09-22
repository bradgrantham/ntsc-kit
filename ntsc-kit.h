#ifndef _NTSC_KIT_H_
#define _NTSC_KIT_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define NTSC_COLORBURST_FREQUENCY       3579545

enum
{
    NTSC_LINE_SAMPLES_UNINITIALIZED = 0, // Do not use
};

typedef enum
{
    NTSC_LINE_SAMPLES_910 = 1,           // 910 samples, 4 per colorburst cycle
    NTSC_LINE_SAMPLES_912 = 2,           // 912 samples, 4 per colorburst cycle
    NTSC_LINE_SAMPLES_1368 = 3,          // 1368 samples, 6 per colorburst cycle
} NTSCLineConfig;

// Number of samples we target; if we're doing 4x colorburst at 228 cycles, that's 912 samples at 14.318180MHz

#define NTSC_EQPULSE_LINES	3
#define NTSC_VSYNC_LINES	3
#define NTSC_VBLANK_LINES	12
#define NTSC_FRAME_LINES	525

/* these are in units of one scanline */
#define NTSC_EQ_PULSE_INTERVAL	.04
#define NTSC_VSYNC_BLANK_INTERVAL	.43
#define NTSC_HOR_SYNC_DUR	.075
#define NTSC_FRONTPORCH		.02
/* BACKPORCH including COLORBURST */
#define NTSC_BACKPORCH		.075

#define NTSC_COLORBURST_CYCLES  9

#define NTSC_SYNC_TIP_VOLTAGE   0.0f
#define NTSC_SYNC_PORCH_VOLTAGE   .285f
#define NTSC_SYNC_BLACK_VOLTAGE   .339f
#define NTSC_SYNC_WHITE_VOLTAGE   1.0f  /* VCR had .912v */

typedef int (*NTSCModeInitFunc)(void* private_data, uint8_t blackvalue, uint8_t whitevalue);
typedef void (*NTSCModeFiniFunc)(void* private_data);
typedef void (*NTSCModeFillLineBufferFunc)(int frameIndex, int lineWithinField, int lineNumber, size_t maxSamples, uint8_t* lineBuffer);
typedef int (*NTSCModeNeedsColorburstFunc)();

void NTSCSetMode(bool interlaced, NTSCLineConfig line_config, void* private_data, NTSCModeInitFunc initFunc, NTSCModeFiniFunc finiFunc, NTSCModeFillLineBufferFunc fillBufferFunc, NTSCModeNeedsColorburstFunc needsColorBurstFunc);
void NTSCWaitNextField();
int NTSCWaitNextLine();

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* _NTSC_KIT_PLATFORM_H_ */
