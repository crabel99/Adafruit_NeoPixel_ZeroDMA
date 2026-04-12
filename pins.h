#pragma once

#include "variant.h"
#include <stdint.h>

struct _SercomPinLookup {
  uint8_t port;      // EPortType value: 0=PORTA, 1=PORTB, 2=PORTC, 3=PORTD
  uint8_t portPin;   // pin within port (0-31)
  uint8_t sercomNum; // SERCOM instance index
  uint8_t pad;       // SERCOM pad (0-3)
  uint8_t mux;       // 2=PIO_SERCOM (MUX C), 3=PIO_SERCOM_ALT (MUX D)
};

static const _SercomPinLookup _sercomPinTable[] = {
#define SERCOM_PIN(port, portPin, sercomNum, pad, mux)                         \
  {port, portPin, sercomNum, pad, mux},
#include "pins.inc"
#undef SERCOM_PIN
};

// SERCOM class instances are defined in each board's variant.cpp.
extern SERCOM sercom0, sercom1, sercom2, sercom3;
#if SERCOM_INST_NUM > 4
extern SERCOM sercom4, sercom5;
#endif
#if SERCOM_INST_NUM > 6
extern SERCOM sercom6, sercom7;
#endif

static SERCOM *const _sercoms[] = {
    &sercom0, &sercom1, &sercom2, &sercom3,
#if SERCOM_INST_NUM > 4
    &sercom4, &sercom5,
#endif
#if SERCOM_INST_NUM > 6
    &sercom6, &sercom7,
#endif
};

static Sercom *const _sercomBases[] = {
    SERCOM0, SERCOM1, SERCOM2, SERCOM3,
#if SERCOM_INST_NUM > 4
    SERCOM4, SERCOM5,
#endif
#if SERCOM_INST_NUM > 6
    SERCOM6, SERCOM7,
#endif
};

static const uint8_t _sercomDmacId[] = {
    SERCOM0_DMAC_ID_TX, SERCOM1_DMAC_ID_TX,
    SERCOM2_DMAC_ID_TX, SERCOM3_DMAC_ID_TX,
#if SERCOM_INST_NUM > 4
    SERCOM4_DMAC_ID_TX, SERCOM5_DMAC_ID_TX,
#endif
#if SERCOM_INST_NUM > 6
    SERCOM6_DMAC_ID_TX, SERCOM7_DMAC_ID_TX,
#endif
};
