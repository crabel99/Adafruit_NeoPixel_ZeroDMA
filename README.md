# Adafruit_NeoPixel_ZeroDMA

DMA-based NeoPixel library for SAMD21 and SAMD51 microcontrollers
(Feather M0, M4, etc.) Doesn't require stopping interrupts, so millis() and
micros() don't lose time, soft PWM (for servos, etc.) still operate normally.

Requires LATEST Adafruit_NeoPixel and Adafruit_ZeroDMA libraries also be
installed (Adafruit SAMD board support automatically includes
Adafruit_ZeroDMA).

THIS ONLY WORKS ON CERTAIN PINS. THIS IS NORMAL.

This branch discovers pin support at runtime using the board's pin mux data.
`begin()` succeeds only when the selected pin has a valid SERCOM MOSI route
(SERCOM PAD 0, 2, or 3). If no compatible route exists, `begin()` returns
`false`.

If using a board's MOSI pin for NeoPixels, that SPI peripheral is not usable
for other devices.

OTHER THINGS TO KNOW:

DMA NeoPixels use a LOT of RAM: 12 bytes/pixel for RGB, 16 bytes/pixel for
RGBW, about 4X as much as regular NeoPixel library (plus a little bit extra
for structures & stuff).

0/1 bit timing does not precisely match NeoPixel/WS2812/SK6812 datasheet
specs, but it seems to work well enough. Use at your own peril.

Have not tested this yet with multiple instances (DMA-driven NeoPixels on
multiple pins), but in theory it should work. Should also be OK mixing DMA
and non-DMA NeoPixels in same sketch (just use different constructor and
pins for each).

Length/pin/type can be provided at run time when constructing the strip
object. What is not supported as a drop-in workflow is changing those values
after `begin()` has initialized DMA/SPI resources.

If your project needs configurable strip settings, construct a new
Adafruit_NeoPixel_ZeroDMA instance with the desired values before calling
`begin()`.
