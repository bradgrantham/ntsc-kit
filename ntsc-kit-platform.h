#ifndef _NTSC_KIT_PLATFORM_H_
#define _NTSC_KIT_PLATFORM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

// Defined by platform, called by NTSC-Kit
void PlatformEnableNTSCScanout();
void PlatformDisableNTSCScanout();
uint8_t PlatformVoltageToDACValue(float voltage);
float PlatformDACValueToVoltage(uint8_t dac_value);

// Defined by NTSC-KIT, called by platform, preferably right after
// beginning scanout of the *other* scanline buffer
void NTSCFillLineBuffer(int frameNumber, int lineNumber, unsigned char *lineBuffer);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* _NTSC_KIT_PLATFORM_H_ */
