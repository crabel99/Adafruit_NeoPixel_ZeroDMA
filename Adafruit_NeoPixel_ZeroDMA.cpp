/*
DMA NeoPixel library for M0-based boards (Feather M0, Arduino Zero, etc.).
Doesn't require stopping interrupts, so millis()/micros() don't lose time,
soft PWM (for servos, etc.) still operate normally, etc.

Supports any pin with a valid SERCOM SPI MOSI route.  The library uses
silicon-level mux tables to dynamically discover which SERCOM (and which PAD)
matches the requested pin at run-time, preferring predefined SPI interfaces
(e.g., PIN_SPI_MOSI on Feather M0) if available, then falling back to
alternate SERCOM routes if needed.  altSercom parameter allows override of
the mux preference (C vs D variant) in case of conflicts with other peripherals.

0/1 bit timing does not precisely match NeoPixel/WS2812/SK6812 datasheet
specs, but it seems to work well enough.  Use at your own peril.

Currently this only supports strip declaration with length & pin known at
compile time, so it's not a 100% drop-in replacement for all NeoPixel code
right now.  But probably 99%+ of all sketches are written that way, so it's
perfectly usable for most.  The stock NeoPixel library has the option of
setting the length & pin number at run-time (so these can be stored in a
config file or in EEPROM)...this is entirely possible here, just hasn't
been written yet.  In that regard, TO DO:
setPin(uint8_t p)
updateLength(uint16_t n)
updateType(neoPixelType t)
UPDATE: no, don't.  Please just use the C++ 'new' operator to allocate a
strip (passing length & type) if needed that way.  It's been added to the
NeoPixel library roadmap that these functions are deprecated.

Have not tested this yet with multiple instances (DMA-driven NeoPixels on
multiple pins), but in theory it should work.  Should also be OK mixing
DMA and non-DMA NeoPixels in same sketch (just use different constructor
and pins for each).
*/

#include "Adafruit_NeoPixel_ZeroDMA.h"
#include "bittable.h"       // Optional, see comments in show()
#include "pins.h"           // Silicon-level pin routing tables
#include "wiring_private.h" // pinPeripheral() function

/** @brief Initialize a NeoPixel strand
    @param n Number of pixels
    @param p Pin to use (we will figure out what Sercom to use)
    @param t The color order / type of pixels
*/
Adafruit_NeoPixel_ZeroDMA::Adafruit_NeoPixel_ZeroDMA(uint16_t n, uint8_t p,
                                                     neoPixelType t)
    : Adafruit_NeoPixel_ZeroDMA(n, p, t, false) {}

/** @brief Initialize a NeoPixel strand
    @param n Number of pixels
    @param p Pin to use (we will figure out what Sercom to use)
    @param t The color order / type of pixels
    @param altSercom If true, prefer ALT SERCOM variant; if false (default),
   prefer primary
*/
Adafruit_NeoPixel_ZeroDMA::Adafruit_NeoPixel_ZeroDMA(uint16_t n, uint8_t p,
                                                     neoPixelType t,
                                                     bool altSercom)
    : Adafruit_NeoPixel(n, p, t), spi(NULL), dmaBuf(NULL), stagingBuf(NULL),
      brightness(256), _useAltSercom(altSercom), dmaDescriptor(NULL),
      dmaBufferBytes(0), dmaActive(false), refreshPending(false),
      stagingEncoding(false), dmaAllocated(false), ownsSpi(false),
      spiTransactionStarted(false) {}

/** @brief Default constructor exists for API compatibility, but this object
  still assumes fixed pin/length/type once DMA is initialized.

  Inherited dynamic APIs (setPin/updateLength/updateType) exist in the base
  class, but this class does not implement a full stop/reallocate/restart DMA
  lifecycle around those changes. Using that workflow after begin() is not
  considered a supported path.
*/
Adafruit_NeoPixel_ZeroDMA::Adafruit_NeoPixel_ZeroDMA(void)
    : Adafruit_NeoPixel(), spi(NULL), dmaBuf(NULL), stagingBuf(NULL),
      brightness(256), _useAltSercom(false), dmaDescriptor(NULL),
      dmaBufferBytes(0), dmaActive(false), refreshPending(false),
      stagingEncoding(false), dmaAllocated(false), ownsSpi(false),
      spiTransactionStarted(false) {}

Adafruit_NeoPixel_ZeroDMA *
    Adafruit_NeoPixel_ZeroDMA::dmaOwners[DMAC_CH_NUM] = {NULL};

Adafruit_NeoPixel_ZeroDMA::~Adafruit_NeoPixel_ZeroDMA() {
  releaseResources();
}

void Adafruit_NeoPixel_ZeroDMA::releaseResources() {
  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  unregisterDmaOwner();
  if (dmaAllocated) {
    dma.setCallback(NULL);
    dma.abort();
    dma.free();
    dmaAllocated = false;
  }
  dmaDescriptor = NULL;
  dmaActive = false;
  refreshPending = false;
  stagingEncoding = false;
  __set_PRIMASK(primask);

  if (spiTransactionStarted) {
    spi->endTransaction();
    spiTransactionStarted = false;
  }
  if (ownsSpi)
    delete spi;
  spi = NULL;
  ownsSpi = false;
  free(dmaBuf);
  free(stagingBuf);
  dmaBuf = NULL;
  stagingBuf = NULL;
  dmaBufferBytes = 0;
}

bool Adafruit_NeoPixel_ZeroDMA::registerDmaOwner() {
  for (uint8_t i = 0; i < DMAC_CH_NUM; ++i) {
    if (dmaOwners[i] == NULL) {
      dmaOwners[i] = this;
      return true;
    }
  }
  return false;
}

void Adafruit_NeoPixel_ZeroDMA::unregisterDmaOwner() {
  for (uint8_t i = 0; i < DMAC_CH_NUM; ++i) {
    if (dmaOwners[i] == this)
      dmaOwners[i] = NULL;
  }
}

void Adafruit_NeoPixel_ZeroDMA::dmaCallback(Adafruit_ZeroDMA *completedDma) {
  for (uint8_t i = 0; i < DMAC_CH_NUM; ++i) {
    Adafruit_NeoPixel_ZeroDMA *owner = dmaOwners[i];
    if (owner != NULL && &owner->dma == completedDma) {
      owner->handleDmaComplete();
      return;
    }
  }
}

void Adafruit_NeoPixel_ZeroDMA::handleDmaComplete() {
  dmaActive = false;
  if (refreshPending && !stagingEncoding) {
    uint8_t *completed = dmaBuf;
    dmaBuf = stagingBuf;
    stagingBuf = completed;
    refreshPending = false;
    (void)startTransfer(dmaBuf);
  }
}

/** @brief Discover the SERCOM configuration for the Arduino pin stored in
    @p pin by querying the board's variant table (g_APinDescription) and a
  compile-time silicon mux table generated from parsed datasheet mux tables.

    Two-pass lookup honours @p _useAltSercom: pass 0 tries the preferred mux
    variant (MUX D when altSercom=true, MUX C otherwise); pass 1 accepts the
    other as a fallback.  Only pads valid as SPI MOSI (PAD 0, 2, or 3) are
    considered — PAD 1 is clock-only.

    @returns true and populates all out-parameters on success.
*/
bool Adafruit_NeoPixel_ZeroDMA::_setupSercomFromPin(SERCOM **outSercom,
                                                    AdafruitNeoPixelZeroDmaSercom **outSercomBase,
                                                    uint8_t *outDmacID,
                                                    SercomSpiTXPad *outPadTX,
                                                    EPioType *outPinFunc) {
  if ((uint32_t)pin >= PINS_COUNT)
    return false;

  const uint8_t ulPort = (uint8_t)g_APinDescription[pin].ulPort;
  const uint8_t ulPin = (uint8_t)g_APinDescription[pin].ulPin;
  const uint8_t nSercoms = (uint8_t)(sizeof(_sercoms) / sizeof(_sercoms[0]));
  const size_t tableSize = sizeof(_sercomPinTable) / sizeof(_sercomPinTable[0]);

  // preferredMux: 2 = MUX C / PIO_SERCOM,  3 = MUX D / PIO_SERCOM_ALT
  const uint8_t preferredMux = _useAltSercom ? 3u : 2u;
  const uint8_t fallbackMux = _useAltSercom ? 2u : 3u;

  for (int pass = 0; pass < 2; pass++) {
    const uint8_t targetMux = (pass == 0) ? preferredMux : fallbackMux;
    for (size_t i = 0; i < tableSize; i++) {
      const _SercomPinLookup &e = _sercomPinTable[i];
      if (e.port != ulPort || e.portPin != ulPin || e.mux != targetMux)
        continue;
      if (e.sercomNum >= nSercoms)
        continue; // SERCOM absent on this chip variant

      // PAD1 is clock-only and cannot serve as MOSI.
      SercomSpiTXPad padTX;
      switch (e.pad) {
      case 0:
        padTX = SPI_PAD_0_SCK_1;
        break;
      case 2:
        padTX = SPI_PAD_2_SCK_3;
        break;
      case 3:
        padTX = SPI_PAD_3_SCK_1;
        break;
      default:
        continue;
      }

      *outSercom = _sercoms[e.sercomNum];
      *outSercomBase = _sercomBases[e.sercomNum];
      *outDmacID = _sercomDmacId[e.sercomNum];
      *outPadTX = padTX;
      *outPinFunc = (EPioType)e.mux;
      return true;
    }
  }
  return false;
}

/** @brief Initialize the underlying SPI SERCOM for DMA transfers
    @param sercom Pointer to the underlying SERCOM from the Arduino core
    @param sercomBase the 'raw' Sercom register base address
    @param dmacID the DMAC id that matches the TX for the sercom (check DS)
    @param mosi The MOSI pin (where we send data to the neopixel)
    @param padTX the pinmux set up for SPI SERCOM pin config
    @param pinFunc The pinmux setup for which 'type' of pinmux we use
    @returns True or false on success
*/
bool Adafruit_NeoPixel_ZeroDMA::begin(SERCOM *sercom, AdafruitNeoPixelZeroDmaSercom *sercomBase,
                                      uint8_t dmacID, uint8_t mosi,
                                      SercomSpiTXPad padTX, EPioType pinFunc) {

  if (mosi != pin)
    return false; // Invalid pin

  if (spiTransactionStarted)
    return true;

  Adafruit_NeoPixel::begin(); // Call base class begin() function 1st
  // TO DO: Check for successful malloc in base class here

  // DMA buffer is 3X the NeoPixel buffer size.  Each bit is expanded
  // 3:1 to allow use of SPI peripheral to generate NeoPixel-like timing
  // (0b100 for a zero bit, 0b110 for a one bit).  SPI is clocked at
  // 2.4 MHz, the 3:1 sizing then creates NeoPixel-like 800 KHz bitrate.
  // The final 90 zero bytes hold the data line low for 300 microseconds at
  // 2.4 MHz, satisfying the reset/latch interval before the one-shot DMA
  // descriptor completes.

  uint8_t bytesPerPixel = (wOffset == rOffset) ? 3 : 4;
  dmaBufferBytes = (numLEDs * bytesPerPixel * 8 * 3 + 7) / 8 + 90;
  dmaBuf = (uint8_t *)malloc(dmaBufferBytes);
  stagingBuf = (uint8_t *)malloc(dmaBufferBytes);
  if (!dmaBuf || !stagingBuf) {
    releaseResources();
    return false;
  }

#if SPI_INTERFACES_COUNT > 0
  if (pin == PIN_SPI_MOSI) { // If NeoPixel pin is main SPI MOSI...
    spi = &SPI;              // Use the existing SPIClass object
    padTX = PAD_SPI_TX;
  }
#endif
#if SPI_INTERFACES_COUNT > 1
  else if (pin == PIN_SPI1_MOSI) { // If NeoPixel pin = secondary SPI MOSI...
    spi = &SPI1;                   // Use the SPI1 SPIClass object
    padTX = PAD_SPI1_TX;
  }
#endif
#if SPI_INTERFACES_COUNT > 2
  else if (pin == PIN_SPI2_MOSI) { // Ditto, tertiary SPI
    spi = &SPI2;
    padTX = PAD_SPI2_TX;
  }
#endif
#if SPI_INTERFACES_COUNT > 3
  else if (pin == PIN_SPI3_MOSI) {
    spi = &SPI3;
    padTX = PAD_SPI3_TX;
  }
#endif
#if SPI_INTERFACES_COUNT > 4
  else if (pin == PIN_SPI4_MOSI) {
    spi = &SPI4;
    padTX = PAD_SPI4_TX;
  }
#endif
#if SPI_INTERFACES_COUNT > 5
  else if (pin == PIN_SPI5_MOSI) {
    spi = &SPI5;
    padTX = PAD_SPI5_TX;
  }
#endif
  if (spi == NULL) {
    // Only MOSI is used; apply the selected mux after SPI initialization.
    spi = new SPIClassSAMD(sercom, mosi, mosi, mosi, padTX, SERCOM_RX_PAD_1);
    ownsSpi = (spi != NULL);
  }
  if (spi == NULL || !spi->begin()) {
    releaseResources();
    return false;
  }
  pinPeripheral(mosi, pinFunc);
  dma.setTrigger(dmacID);
  dma.setAction(DMA_TRIGGER_ACTON_BEAT);
  if (DMA_STATUS_OK != dma.allocate()) {
    releaseResources();
    return false;
  }
  dmaAllocated = true;
#if defined(SERCOM0_REGS)
  void *dataReg = (void *)(&sercomBase->SPIM.SERCOM_DATA);
#else
  void *dataReg = (void *)(&sercomBase->SPI.DATA.reg);
#endif
  dmaDescriptor = dma.addDescriptor(
      dmaBuf, dataReg, dmaBufferBytes,
      DMA_BEAT_SIZE_BYTE, true, false);
  if (dmaDescriptor == NULL || !registerDmaOwner()) {
    releaseResources();
    return false;
  }
  memset(dmaBuf, 0, dmaBufferBytes);
  memset(stagingBuf, 0, dmaBufferBytes);
  dma.loop(false);
  dma.setCallback(dmaCallback);
  spi->beginTransaction(SPISettings(2400000, MSBFIRST, SPI_MODE0));
  spiTransactionStarted = true;
  return true;
}

/** @brief Initialize SPI SERCOM and DMA from the selected pin

  Uses silicon mux discovery via _setupSercomFromPin(). If no compatible
  SERCOM MOSI route exists for the selected pin, this returns false.

    @returns True on success, false otherwise
 */
bool Adafruit_NeoPixel_ZeroDMA::begin(void) {
  SERCOM *sercom = nullptr;
  AdafruitNeoPixelZeroDmaSercom *sercomBase = nullptr;
  uint8_t dmacID = 0;
  SercomSpiTXPad padTX = SPI_PAD_0_SCK_1;
  EPioType pinFunc = PIO_SERCOM;

  // Try the smart lookup with ALT preference
  if (_setupSercomFromPin(&sercom, &sercomBase, &dmacID, &padTX, &pinFunc)) {
    return begin(sercom, sercomBase, dmacID, pin, padTX, pinFunc);
  }

  return false; // No SERCOM found for this pin
}

/** @brief Convert the NeoPixel buffer to larger DMA buffer and start xfer
 */
void Adafruit_NeoPixel_ZeroDMA::encodeInto(uint8_t *buffer) {
  if (buffer == NULL)
    return;

  // Expand 8 bits 'abcdefgh' to 24 bits '1a01b01c01d01e01f01g01h0'
#ifdef _BITTABLE_H_
  // If bittable.h is included, 3:1 bit expansion is handled using a table
  // lookup -- each byte of input (from NeoPixel buffer) is replaced with
  // three bytes output (from table to DMA buffer).  This is about twice
  // as quick as math below but the table requires about 1KB of code space.
  uint8_t *in = pixels, *out = buffer;
  uint32_t expanded;
  for (uint16_t p = numBytes; p--;) {
    expanded = bitExpand[(*in++ * brightness) >> 8];
    *out++ = expanded >> 16; // Shifting 32-bit table entry is
    *out++ = expanded >> 8;  // about 11% faster than copying
    *out++ = expanded;       // three values from a uint8_t table.
  }
#else
  // If bittable.h is NOT included, 3:1 bit expansion is done on the fly.
  // More complex, but smaller executable.
  uint8_t *in = pixels, *out = buffer, i, abef, cdgh;
  uint32_t expanded;
  for (uint16_t p = numBytes; p--;) {
    cdgh = (*in++ * brightness) >> 8;
    abef = cdgh & 0b11001100; // ab00ef00
    cdgh &= 0b00110011;       // 00cd00gh
    expanded = ((abef * 0b1010000010100000) & 0b010010000000010010000000) |
               ((cdgh * 0b0000101000001010) & 0b000000010010000000010010) |
               0b100100100100100100100100;
    *out++ = expanded >> 16;
    *out++ = expanded >> 8;
    *out++ = expanded;
  }
#endif
}

bool Adafruit_NeoPixel_ZeroDMA::startTransfer(uint8_t *buffer) {
  if (buffer == NULL || dmaDescriptor == NULL)
    return false;
  dma.changeDescriptor(dmaDescriptor, buffer, NULL, dmaBufferBytes);
  dmaActive = (DMA_STATUS_OK == dma.startJob());
  return dmaActive;
}

/** @brief Encode the latest pixels and submit one finite DMA transfer. */
void Adafruit_NeoPixel_ZeroDMA::show(void) {
  if (dmaBuf == NULL || stagingBuf == NULL)
    return;

  if (!dmaActive) {
    encodeInto(dmaBuf);
    (void)startTransfer(dmaBuf);
    return;
  }

  stagingEncoding = true;
  encodeInto(stagingBuf);
  stagingEncoding = false;

  const uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (dmaActive) {
    refreshPending = true;
  } else {
    uint8_t *completed = dmaBuf;
    dmaBuf = stagingBuf;
    stagingBuf = completed;
    refreshPending = false;
    (void)startTransfer(dmaBuf);
  }
  __set_PRIMASK(primask);
}

/** @brief
    Brightness is stored differently here than in normal NeoPixel library.
    In either case it's *specified* the same: 0 (off) to 255 (brightest).
    Classic NeoPixel rearranges this internally so 0 is max, 1 is off and
    255 is just below max...it's a decision based on how fixed-point math
    is handled in that code.  Here it's stored internally as 1 (off) to
    256 (brightest), requiring a 16-bit value.
    @param b 0 - 255 brightness value
*/
void Adafruit_NeoPixel_ZeroDMA::setBrightness(uint8_t b) {
  brightness = (uint16_t)b + 1; // 0-255 in, 1-256 out
}

/** @brief The brightness, back adjusted to 0-255 standard expectation
    @returns 0 for off, 255 for max brightness */
uint8_t Adafruit_NeoPixel_ZeroDMA::getBrightness(void) const {
  return brightness - 1; // 1-256 in, 0-255 out
}
