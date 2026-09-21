#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstring>

enum EPioType { PIO_SERCOM = 2, PIO_SERCOM_ALT = 3 };
enum SercomSpiTXPad { SPI_PAD_0_SCK_1, SPI_PAD_2_SCK_3, SPI_PAD_3_SCK_1 };
enum SercomRXPad { SERCOM_RX_PAD_1 = 1 };
struct SERCOM { unsigned index; };
#ifdef FAKE_NATIVE_REGISTERS
struct sercom_registers_t { struct { uint32_t SERCOM_DATA; } SPIM; };
#else
struct Sercom { struct { struct { uint32_t reg; } DATA; } SPI; };
using sercom_registers_t = Sercom;
#endif
extern sercom_registers_t registers[8];
#ifdef FAKE_NATIVE_REGISTERS
#define SERCOM0_REGS (&registers[0])
#define SERCOM1_REGS (&registers[1])
#define SERCOM2_REGS (&registers[2])
#define SERCOM3_REGS (&registers[3])
#define SERCOM4_REGS (&registers[4])
#define SERCOM5_REGS (&registers[5])
#define SERCOM6_REGS (&registers[6])
#define SERCOM7_REGS (&registers[7])
#endif
#define SERCOM_INST_NUM 8
#define SERCOM0_DMAC_ID_TX 5
#define SERCOM1_DMAC_ID_TX 7
#define SERCOM2_DMAC_ID_TX 9
#define SERCOM3_DMAC_ID_TX 11
#define SERCOM4_DMAC_ID_TX 13
#define SERCOM5_DMAC_ID_TX 15
#define SERCOM6_DMAC_ID_TX 17
#define SERCOM7_DMAC_ID_TX 19
#define PINS_COUNT 34
#define SPI_INTERFACES_COUNT 1
#define PIN_SPI_MOSI 9
#define PAD_SPI_TX SPI_PAD_0_SCK_1
struct PinDescription { uint8_t ulPort, ulPin; };
extern PinDescription g_APinDescription[PINS_COUNT];
extern SERCOM sercom0, sercom1, sercom2, sercom3, sercom4, sercom5, sercom6, sercom7;

namespace observed {
extern bool spiBeginResult;
extern unsigned spiBegins, spiTransactions, starts, aborts, frees, pinMuxes;
extern unsigned spiConstructed, spiDestroyed, allocated;
extern void *owners[32];
}

#ifndef FAKE_NATIVE_REGISTERS
#define SERCOM0 (&registers[0])

#define SERCOM1 (&registers[1])

#define SERCOM2 (&registers[2])

#define SERCOM3 (&registers[3])

#define SERCOM4 (&registers[4])

#define SERCOM5 (&registers[5])

#define SERCOM6 (&registers[6])

#define SERCOM7 (&registers[7])
#endif

#define DMAC_CH_NUM 4
namespace observed { extern uint32_t primask; }
inline uint32_t __get_PRIMASK() { return observed::primask; }
inline void __disable_irq() { observed::primask=1; }
inline void __set_PRIMASK(uint32_t value) { observed::primask=value; }
namespace observed {
extern bool allocationFails, descriptorFails;
extern unsigned invalidAborts, invalidFrees, transactionEnds, activeTransactions;
extern unsigned callbackAtAbort, restartsDuringAbort;
}
