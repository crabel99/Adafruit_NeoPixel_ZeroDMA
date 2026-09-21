#include <iostream>
#include <string>
#include <unordered_set>
#include "Adafruit_NeoPixel.h"
#include "Adafruit_ZeroDMA.h"
#include "SPI.h"
namespace observed {
uint32_t primask=0;
bool spiBeginResult=true, allocationFails=false, descriptorFails=false;
unsigned spiBegins=0, spiTransactions=0, starts=0, aborts=0, frees=0, pinMuxes=0;
unsigned spiConstructed=0, spiDestroyed=0, allocated=0;
unsigned invalidAborts=0, invalidFrees=0, transactionEnds=0, activeTransactions=0;
unsigned callbackAtAbort=0, restartsDuringAbort=0;
void *owners[32]={};
std::unordered_set<void *> buffers;
unsigned mallocCalls=0, failMallocCall=0;
}
void *trackedMalloc(size_t n) {
 if (++observed::mallocCalls==observed::failMallocCall) return nullptr;
 void *p=std::malloc(n); if (p) observed::buffers.insert(p); return p;
}
void trackedFree(void *p) { observed::buffers.erase(p); std::free(p); }
#define malloc trackedMalloc
// Instrument both buffer release and the fake DMA release method.
#define free(...) FREE_DISPATCH(__VA_ARGS__)
#define FREE_DISPATCH(...) trackedFree(__VA_ARGS__)
#include "../../Adafruit_NeoPixel_ZeroDMA.cpp"
#undef malloc
#undef free
SERCOM sercom0{0}, sercom1{1}, sercom2{2}, sercom3{3}, sercom4{4}, sercom5{5}, sercom6{6}, sercom7{7};
sercom_registers_t registers[8]={};
PinDescription g_APinDescription[PINS_COUNT]={};
SPIClass SPI(&sercom0, 9, 9, 9, SPI_PAD_0_SCK_1, SERCOM_RX_PAD_1);
class InspectableStrip : public Adafruit_NeoPixel_ZeroDMA {
public:
 using Adafruit_NeoPixel_ZeroDMA::Adafruit_NeoPixel_ZeroDMA;
 Adafruit_ZeroDMA *managedDma() { return &dma; }
};
unsigned failures=0;
void check(bool condition, const char *message) {
 if (!condition) { ++failures; std::cout << "FAIL " << message << '\n'; }
}
bool initialize(Adafruit_NeoPixel_ZeroDMA &strip, uint8_t pin=33) {
 return strip.begin(&sercom1, &registers[1], 7, pin, SPI_PAD_0_SCK_1, PIO_SERCOM);
}
void released(unsigned constructed, unsigned destroyed) {
 check(observed::allocated==0, "DMA channels released");
 check(observed::invalidAborts==0, "no abort on unallocated DMA");
 check(observed::invalidFrees==0, "no free on unallocated DMA");
 check(observed::activeTransactions==0, "SPI transactions balanced");
 check(observed::spiConstructed-constructed==observed::spiDestroyed-destroyed, "owned SPI deleted exactly once");
 check(observed::buffers.empty(), "DMA buffers released");
 check(observed::callbackAtAbort==0, "callback detached before abort");
 check(observed::restartsDuringAbort==0, "queued DMA not restarted during teardown");
}
int main(int argc, char **argv) {
 if (argc!=2) return 2;
 std::string name=argv[1];
 auto constructed=observed::spiConstructed, destroyed=observed::spiDestroyed;
 if (name=="finite_transfer") {
  InspectableStrip strip(1,33);
  check(initialize(strip), "valid begin succeeds");
  check(observed::starts==0, "begin does not start transfer");
  strip.show(); check(observed::starts==1, "show starts first transfer");
  strip.show(); check(observed::starts==1, "pending frame waits for completion");
  strip.managedDma()->complete(); check(observed::starts==2, "completion submits pending frame");
  strip.managedDma()->complete(); check(observed::starts==2, "finite transfer stops without pending frame");
 }
 else if (name=="never_begun") { Adafruit_NeoPixel_ZeroDMA strip(1,33); }
 else if (name=="invalid_pin") { Adafruit_NeoPixel_ZeroDMA strip(1,33); check(!initialize(strip,32), "invalid pin rejected"); }
 else if (name=="active_destroy" || name=="irq_restore") { if (name=="irq_restore") observed::primask=1; Adafruit_NeoPixel_ZeroDMA strip(1,33); check(initialize(strip), "begin succeeds"); strip.show(); strip.show(); }
 else if (name=="allocation_failure" || name=="descriptor_failure" || name=="spi_failure") {
  observed::allocationFails=name=="allocation_failure";
  observed::descriptorFails=name=="descriptor_failure";
  observed::spiBeginResult=name!="spi_failure";
  Adafruit_NeoPixel_ZeroDMA strip(1,33);
  check(!initialize(strip), "failed initialization propagates");
  check(observed::allocated==0 && observed::buffers.empty(), "failed begin releases immediately");
  check(observed::spiDestroyed-destroyed==1, "failed begin deletes owned SPI immediately");
  check(observed::spiTransactions==0 && observed::starts==0, "failed begin starts no transaction or DMA transfer");
  observed::allocationFails=false; observed::descriptorFails=false; observed::spiBeginResult=true;
  check(initialize(strip), "retry succeeds"); strip.show();
 }
 else if (name=="buffer_failure_1" || name=="buffer_failure_2") {
  observed::failMallocCall=name=="buffer_failure_1" ? 1 : 2;
  Adafruit_NeoPixel_ZeroDMA strip(1,33); check(!initialize(strip), "buffer failure rejected");
  check(observed::buffers.empty(), "partial buffer allocation released immediately");
  observed::failMallocCall=0; check(initialize(strip), "retry after buffer failure");
 }
 else if (name=="borrowed_spi") { Adafruit_NeoPixel_ZeroDMA strip(1,9); check(initialize(strip,9), "borrowed SPI begin"); strip.show(); }
 else if (name=="begin_twice") { Adafruit_NeoPixel_ZeroDMA strip(1,33); check(initialize(strip), "first begin"); strip.show(); check(initialize(strip), "second begin"); check(observed::allocated==1, "second begin retains one DMA channel"); check(observed::buffers.size()==2, "second begin retains two buffers"); }
 else if (name=="channel_reuse") {
  for (unsigned i=0; i<DMAC_CH_NUM*3; ++i) { Adafruit_NeoPixel_ZeroDMA strip(1,33); check(initialize(strip), "repeated lifetime allocates channel"); strip.show(); }
 }
 else if (name=="callback_reuse") {
  Adafruit_ZeroDMA::Callback oldCallback=nullptr; Adafruit_ZeroDMA *oldDma=nullptr;
  { InspectableStrip strip(1,33); check(initialize(strip), "first begin"); strip.show(); strip.show(); oldDma=strip.managedDma(); oldCallback=oldDma->completionCallback(); }
  auto starts=observed::starts; oldCallback(oldDma); check(observed::starts==starts, "stale completion has no owner");
  { InspectableStrip strip(1,33); check(initialize(strip), "reused channel begins"); strip.show(); strip.show(); auto dma=strip.managedDma(); starts=observed::starts; dma->complete(); check(observed::starts==starts+1, "new owner's pending frame starts"); }
 }
 else return 2;
 check(observed::primask==(name=="irq_restore" ? 1u : 0u), "prior interrupt mask restored");
 released(constructed,destroyed);
 std::cout << name << ": " << (failures ? "FAIL" : "PASS") << '\n';
 return failures ? 1 : 0;
}
