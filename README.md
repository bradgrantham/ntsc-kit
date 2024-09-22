Include ntsc-kit.h in system headers used by applications.

* `void NTSCSetMode(bool interlaced, NTSCLineConfig line_config, void* private_data, NTSCModeInitFunc initFunc, NTSCModeFiniFunc finiFunc, NTSCModeFillLineBufferFunc fillLineBufferFunc, NTSCModeNeedsColorburstFunc needsColorBurstFunc);`
  * Call this function in an application to start scanning out an NTSC Mode.
* `void NTSCWaitNextField();`
  * Call this function to wait until scanout has left the visible area of the NTSC "field".  When this returns the scanlines representing the NTSC equalizing pulses, vertical sync, and vertical blanking have begun.  E.g. Wait on this to get a head start processing the next frame before scanout.
  * Line pacing is dictated by the system line scanout hardware, which platforms should endeavour to derive from a 3.579545MHz clock source within .0003%.  For an emulation loop, an application can use this function to move forward emulation for the following amounts of machine time:
    * `NTSC_LINE_SAMPLES_910`: 227.5 colorburst cycles, 63.556µs of machine time, or a 15734.264Hz clock
    * `NTSC_LINE_SAMPLES_912` and `NTSC_LINE_SAMPLES_1368`: 228 colorburst cycles, 63.695µs of machine time, or a 15699.75Hz clock.
* `int NTSCWaitNextLine();`
  * This function returns with a line value "N" once scanout has begin on line N - 2 and the `fillBufferFunc` function is in progress filling the buffer for line N - 1 .  E.g. Wait on this to perform processing for line N.
  * For an emulation loop, an application can use this function to move forward emulation for the following amounts of machine time:
    * `NTSC_LINE_SAMPLES_910`: 16.72ms
    * `NTSC_LINE_SAMPLES_912` and `NTSC_LINE_SAMPLES_1368`: 16.68ms



Implement the Platform functions in ntsc-kit-platform.h in the system.

* `void PlatformEnableNTSCScanout(NTSCLineConfig line_config, bool interlaced);`
  * This platform function is called by `NTSCSetMode` once line buffers are initialized to enable scanout for the specified line_config and interlacing.
* `void PlatformDisableNTSCScanout();`
  * This platform function is called by `NTSCSetMode` before altering line buffers and timing.  On return any DMA or scanout of line buffers, i.e. platform calls to the NTSC Kit function`NTSCFillLineBuffer` must be completed
* `uint8_t PlatformVoltageToDACValue(float voltage);`
  * Platform should convert a voltage to an 8-bit DAC value.  This function is called at initialization time in order to cache NTSC signal levels and is not called from within `NTSCFillLineBuffer` .



Call thes NTSC-Kit function from Platform functions:

* `void NTSCInitialize();`
  * Call this function from Platform system startup to initialize variables used by the NTSC component.

* `void NTSCFillLineBuffer(int frameNumber, int lineNumber, unsigned char *lineBuffer);`
  * Call this function from the Platform line scanout service function.  This function will call the application's `needsColorBurstFunc` and will fill in the provided `lineBuffer` for the frame and line including calling the application's  `fillLineBufferFunc` for the visible field area.
