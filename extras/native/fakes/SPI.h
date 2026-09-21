#pragma once
#include "variant.h"
constexpr uint8_t MSBFIRST = 1, SPI_MODE0 = 0;
struct SPISettings { SPISettings(uint32_t, uint8_t, uint8_t) {} };
class SPIClass {
public:
 SPIClass(SERCOM *, uint8_t, uint8_t, uint8_t, SercomSpiTXPad, SercomRXPad) { ++observed::spiConstructed; }
 ~SPIClass() { ++observed::spiDestroyed; }
 bool begin() { ++observed::spiBegins; return observed::spiBeginResult; }
 void beginTransaction(SPISettings) { ++observed::spiTransactions; ++observed::activeTransactions; }
 void endTransaction() { ++observed::transactionEnds; --observed::activeTransactions; }
};
extern SPIClass SPI;
