# Practice ROM follow-up work

## ROM DMA/TLB concurrency

### Context and confirmed failure

The practice ROM previously froze at random after tens of seconds or a few
minutes of gameplay. The emulator remained alive, but `g_GlobalTimer` stopped
advancing and only `idleproc` ran.

GDB showed the main game thread blocked in this call chain:

```text
loadAnimationFrame
  -> romCopyAligned
    -> romCopy
      -> romReceiveMesg
        -> osRecvMesg(&memoryMesgQueue, ..., OS_MESG_BLOCK)
```

Relevant code:

- `src/ramrom.c`: `memoryMesgMB`, `memoryMesgQueue`, `romCopy`, and
  `romReceiveMesg`.
- `src/tlb_manage.c`: `tlbmanageTranslateLoadRomFromTlbAddress` also calls
  `romCopy` to load an 8 KiB code page.
- `src/crash.c`: `crashMain` runs the TLB loader on `g_tlbThread`.
- `src/libultrare/io/devmgr.c`: the PI manager sends DMA completions with
  `osSendMesg(..., OS_MESG_NOBLOCK)` and ignores a full return queue.

The main thread and TLB thread were concurrently using the same global
`OSIoMesg` and one-entry completion queue. A GDB trace observed:

```text
main thread: 8552 ROM DMA submissions, 8549 completed waits
TLB thread:  10095 ROM DMA submissions, 10095 completed waits
```

The decisive breakpoint was immediately after the PI manager's completion
`osSendMesg`. It returned `-1` while:

```text
memoryMesgQueue.validCount = 1
memoryMesgQueue.msgCount   = 1
main thread state          = waiting on memoryMesgQueue
TLB thread state           = runnable
```

The PI manager has priority 150, the TLB thread priority 40, and the main
thread priority 10. It can therefore process another queued DMA before either
consumer runs. The first completion fills the shared queue and wakes a waiter;
the second completion finds the queue full and is silently dropped. Eventually
the main thread waits forever.

This is not caused by save-state restoration or ISViewer logging. Both were
excluded during reproduction. The retail ROM appears not to reach the
necessary timing in practice, while the larger practice code overlay causes
far more code-page loads and exposes the latent shared-queue race.

### Immediate fix

`src/ramrom.c` now creates a practice-only one-token mutex queue and holds it
across the complete synchronous transaction:

```text
acquire lock
submit ROM DMA
wait for its completion
release lock
```

Both `romCopy` and `romWrite` use the lock because they share
`memoryMesgMB` and `memoryMesgQueue`. The original implementations remain
unchanged under `#else`.

The lock must remain inside resident `0x700...` code. Acquiring it in a paged
`0x7f...` caller and then taking a TLB miss would deadlock: the faulted thread
would own the lock while the TLB loader waited for it. The current critical
section only calls resident `romCopy`/libultra code and accesses KSEG0 data.

### TLB miss reduction ideas

Reducing TLB ROM loads improves performance and reduces contention, but is not
a substitute for the ROM DMA lock.

- [ ] Change replacement in
  `tlbmanageTranslateLoadRomFromTlbAddress` from unconditional random
  selection to:
  1. scan `g_tlbMappingTable[].entry0` for an unused physical slot;
  2. after all slots are occupied, use deterministic round-robin or a small
     clock/second-chance policy.

  This requires no page RAM and avoids evicting a useful page while unused
  slots still exist.

- [ ] Profile ROM loads per virtual 8 KiB page. Increment a counter indexed by
  `GET_TLB_INDEX_FROM_MASK(maskedAddress)` and print a sorted summary in a
  debug build. `TlbManageTableEntry.RESERVED` is currently eight unused bytes
  per managed page and can hold profiling data without adding BSS.

- [ ] Use the profile to group frequently called practice functions into as
  few 8 KiB linker pages as possible. Keep per-frame code together and move
  menus, save/load implementation, diagnostics, and other cold paths onto
  separate pages. This reduces the working set without allocating RAM.

- [ ] Exclude test and diagnostic implementations from normal practice builds
  using `DEV`/`TEST_CASE`, rather than merely disabling their call sites.

- [ ] Consider pinning a small number of proven-hot pages in existing physical
  slots. Start with 4-8 pages and measure. Pinning consumes no additional page
  memory, but reduces cache capacity for other pages.

- [ ] Only after profiling, evaluate changing the physical cache size.
  `MAPPING_TABLE_COUNT` is 90 and each slot is `TLB_BLOCK_SIZE` (`0x2000`), so
  the current cache already reserves 720 KiB. The practice-only
  `MANAGEMENT_TABLE_COUNT` of 256 expands address tracking, not physical page
  capacity.

### Additional PI/SRAM hardening

- [ ] Propagate SRAM DMA failures through `StateStream`.
  `src/practice/practice_sram.c:sram_epi_dma` returns `-1` if its non-blocking
  submission to the PI command queue fails, but `sram_read`, `sram_write`, and
  the stream implementation currently allow callers to continue. A failed
  page operation must abort save/load and leave the previous saved-state
  header invalid or unchanged.

- [ ] Check the return value from the original `osPiStartDma` calls in
  `doRomCopy`/`doRomWrite`. The new lock prevents the confirmed completion
  queue race, but a rejected PI command would still make `romReceiveMesg`
  block forever.
