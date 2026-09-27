# Adafruit_NeoPixel_ZeroDMA

Queued-SPI NeoPixel library for SAMD21, SAMD51, and SAME5x microcontrollers.
Frames are encoded into a staging buffer and submitted through the core SPI
queue. The active frame stays unchanged until its completion callback.

Requires Adafruit_NeoPixel and the SimIO queued SPI/SERCOM implementation.

THIS ONLY WORKS ON CERTAIN PINS. THIS IS NORMAL.

This branch discovers pin support at runtime using the board's pin mux data and
generated SAMD21/SAMD51/SAME5x SERCOM route tables. `begin()` succeeds only
when the selected pin has a valid SERCOM MOSI route (SERCOM PAD 0, 2, or 3).
If no compatible route exists, `begin()` returns `false`.

If using a board's MOSI pin for NeoPixels, that SPI peripheral is not usable
for other devices.

OTHER THINGS TO KNOW:

Queued SPI NeoPixels use 21 bytes per RGB pixel or 28 bytes per RGBW pixel,
plus 180 reset bytes. The base pixel buffer and two expanded frame buffers are
kept at the same time.

0/1 bit timing does not precisely match NeoPixel/WS2812/SK6812 datasheet
specs, but it seems to work well enough. Use at your own peril.

The selected SPI peripheral is dedicated to the strip. Calls to `show()`
coalesce while a frame is active, so the newest pending frame follows it.

Length/pin/type can be provided at run time when constructing the strip
object. What is not supported as a drop-in workflow is changing those values
after `begin()` has initialized SPI resources.

If your project needs configurable strip settings, construct a new
Adafruit_NeoPixel_ZeroDMA instance with the desired values before calling
`begin()`.

## Verification

The CI workflow runs the Python and native lifecycle checks, then compiles
`strandtest` for Adafruit Metro M0 and Metro M4 with SimIOFramework pinned in
the workflow. The former upstream Doxygen deployment is not run because it
published through Adafruit-specific credentials and did not validate this
Framework-pinned build.
