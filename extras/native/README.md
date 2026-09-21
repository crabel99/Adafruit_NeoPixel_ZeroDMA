# NeoPixel lifecycle tests

These host tests compile the checkout's actual `Adafruit_NeoPixel_ZeroDMA.cpp`
through `lifecycle.cpp`. They replace Arduino, SPI and ZeroDMA with observable
fakes. No firmware source is copied or generated.

Run from the library root using the SimIO Backend interpreter:

```sh
/Users/crabel/Documents/src/SimIO/SimIOBackend/.venv/bin/python extras/native/run.py --register-style legacy --report legacy-after.json
/Users/crabel/Documents/src/SimIO/SimIOBackend/.venv/bin/python extras/native/run.py --register-style native --report native-after.json
```

The runner stores compiler scratch files, executable, compiler output, source
hashes and case results in `build/native-lifecycle/`. It sets `TMPDIR` to that
workspace build directory. The compiler is the host `/usr/bin/c++`, not a
replacement for PlatformIO firmware builds.

The legacy fake exposes `Sercom`, `SERCOMn` and `SPI.DATA.reg`. The native fake
exposes `sercom_registers_t`, `SERCOMn_REGS` and `SPIM.SERCOM_DATA`. Both modes
execute the same lifecycle cases.

## Behavior checked

- A successful begin allocates without starting DMA. Show submits one finite
  transfer; completion submits a pending frame and otherwise stops.
- Destruction detaches the completion callback before aborting, releases the
  allocated channel, balances a started SPI transaction, deletes owned SPI and
  releases both frame buffers.
- Destruction before begin or after invalid-pin rejection never aborts or frees
  an unallocated channel.
- Failed SPI begin, DMA allocation, descriptor allocation and either frame-buffer
  allocation return false, release partial resources immediately and permit retry.
- Borrowed SPI survives destruction. Repeated successful begin retains one
  channel and two buffers.
- Repeated object lifetimes reuse a deliberately small four-channel pool.
- Removed callback owners ignore stale completions, while a new owner processes
  its own pending frame. Cleanup restores the caller's interrupt mask.

For buffer accounting the production translation unit's `malloc` and `free`
calls are redirected to tracked functions. The fake DMA release method is named
`trackedFree` because the same function-like macro also rewrites `dma.free()`.
It still models the public `free()` contract: busy channels reject release and
an aborted channel removes its allocation owner. The fake invokes any remaining
callback during abort so cleanup ordering is observable.

## Evidence and limits

`build/native-lifecycle/before.json` records the initial failing local-source
reproduction. Its callback-reuse case selected a stale allocation-table pointer,
so that single assertion is not valid evidence. The final test obtains each live
object's actual DMA member instead. Channel leaks and exhaustion were reproduced
independently in the active-destroy and repeated-lifetime cases.

`native-before-compat-build.log` records the old local checkout failing to compile
against native E54 register names. `legacy-after.json` and `native-after.json`
record 14 passing behavioral cases each after lifecycle cleanup and the existing
upstream register-name compatibility changes were integrated.

The fakes do not reproduce hardware DMA timing, real SERCOM channel allocation,
interrupt races, flash ECC, or the SAME54 cold-start fault. PlatformIO compilation
and the original hardware acceptance remain separate requirements. The preexisting
finite-transfer and double-buffer implementation is preserved by this change.
