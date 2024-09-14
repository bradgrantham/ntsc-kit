#include <stdint.h>

// Defined by platform
extern void PlatformEnableNTSCScanout();
extern void PlatformDisableNTSCScanout();
extern uint8_t PlatformVoltageToDACValue(float voltage);
extern float PlatformDACValueToVoltage(uint8_t dac_value);

