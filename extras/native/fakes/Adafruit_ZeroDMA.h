#pragma once
#include "variant.h"
enum ZeroDMAstatus { DMA_STATUS_OK, DMA_STATUS_ERR_NOT_FOUND, DMA_STATUS_BUSY };
enum dma_transfer_trigger_action { DMA_TRIGGER_ACTON_BEAT = 2 };
enum dma_beat_size { DMA_BEAT_SIZE_BYTE = 0 };
struct DmacDescriptor { unsigned marker; };
class Adafruit_ZeroDMA {
public:
 using Callback = void (*)(Adafruit_ZeroDMA *);
 void setTrigger(uint8_t) {}
 void setAction(dma_transfer_trigger_action) {}
 ZeroDMAstatus allocate() {
  if (observed::allocationFails) return DMA_STATUS_ERR_NOT_FOUND;
  for (unsigned i=0; i<DMAC_CH_NUM; ++i) if (!observed::owners[i]) {
   channel=i; observed::owners[i]=this; ++observed::allocated; return DMA_STATUS_OK;
  }
  return DMA_STATUS_ERR_NOT_FOUND;
 }
 DmacDescriptor *addDescriptor(void *, void *, uint32_t, dma_beat_size, bool, bool) {
  return observed::descriptorFails ? nullptr : &descriptor;
 }
 void changeDescriptor(DmacDescriptor *, void *, void *, uint32_t) {}
 void loop(bool) {}
 void setCallback(Callback cb) { callback=cb; }
 ZeroDMAstatus startJob() { ++observed::starts; active=true; return DMA_STATUS_OK; }
 void complete() { active=false; if (callback) callback(this); }
 void abort() {
  ++observed::aborts;
  if (channel>=DMAC_CH_NUM) ++observed::invalidAborts;
  auto starts=observed::starts;
  if (callback) { ++observed::callbackAtAbort; callback(this); }
  observed::restartsDuringAbort+=observed::starts-starts;
  active=false;
 }
 ZeroDMAstatus trackedFree() {
  ++observed::frees;
  if (channel>=DMAC_CH_NUM) { ++observed::invalidFrees; return DMA_STATUS_ERR_NOT_FOUND; }
  if (active) return DMA_STATUS_BUSY;
  observed::owners[channel]=nullptr; --observed::allocated; channel=255;
  return DMA_STATUS_OK;
 }
 Callback completionCallback() const { return callback; }
private:
 unsigned channel=255;
 bool active=false;
 Callback callback=nullptr;
 DmacDescriptor descriptor={};
};
