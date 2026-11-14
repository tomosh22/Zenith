# AUDIO design: device, threads and platform

## Scope of this file

This file is part of the AUDIO design family (entry: `Zenith2/Docs/Design/AUDIO.md`). It specifies AUDIO's threads and their loops, the real-time contract of the device thread and how it is enforced, the cross-thread channels, sample memory retirement, the device backends (miniaudio, null pacer, offline, the test backend), the device state machine with its retry schedule, the Windows and Android output paths including Android audio focus and its Java bridge, and output latency. PLAT declares no audio device layer (`Zenith2/Docs/Design/PLAT.md` → "Explicit exclusions"), so the device layer is AUDIO's, built on miniaudio inside `Internal/Device/`.

## Threads

### Thread table

| Thread | Name | Priority (`Zenith2/Docs/Design/CORE/Threading.md` → "Threads") | Stack | Lifetime | Waits on |
|---|---|---|---|---|---|
| device | the thread on which the backend calls the data callback: on WASAPI a miniaudio worker, on AAudio the AAudio data callback thread, for `miniaudioNull` miniaudio's null-backend thread; possibly a new thread after a reroute (the miniaudio configuration ruling records which, `Zenith2/Docs/Design/AUDIO.md` → "Rulings AUDIO records in its own tickets") | the backend's: WASAPI with MMCSS "Pro Audio"; AAudio's real-time callback (SCHED_FIFO when granted) | the backend's | between backend `Start` and `Stop` | nothing (the backend's own pacing) |
| null pacer | `Zenith.AudioNul` | `THREAD_PRIORITY_TIME_CRITICAL` | 64 KiB | while the state is NULL_DEVICE or LOST | a timed semaphore wait until the next block's deadline |
| control | `Zenith.AudioCtl` | `THREAD_PRIORITY_NORMAL` | 256 KiB (miniaudio init and JNI) | `Zenith_AudioSystem::Create` to its destructor | its semaphore, 100 ms timeout |
| decode | `Zenith.AudioDec` | `THREAD_PRIORITY_HIGH` | 256 KiB (codec state) | `Create` to the destructor | its semaphore, 5 ms timeout |

Only one of the device thread and the null pacer mixes at any moment: a hand-over (§ "Transition table") stops one at a block boundary before the other starts, and the mixer's block counter is continuous across it. AUDIO creates no job and never blocks a job worker: `Audio.Update` touches only its world, and `Audio.Mix` takes no lock of AUDIO's and waits for nothing; besides publishing the snapshot, pushing decode requests and posting the decode semaphore it calls `Zenith_AssetLoader::RequestLoad` (dialogue lines, locale variants), which takes ASSET's request-table lock at `uLOCK_LEVEL_STREAMING_REQUESTS` (250) for a bounded critical section (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetLoader"); `Audio.Mix` holds no other lock while it does, so no lock-order entry arises. Under the offline backend no decode thread exists (`Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing").

### Control thread loop

1. Wait on the control semaphore with a timeout of min(100 ms, time to the next retry deadline).
2. Drain three channels: the main-thread request ring (a `Zenith_SpscRing`, one producer: START, STOP, STOP_AUDIO, RESUME_AUDIO, WINDOW_FOCUS_GAINED, LAYOUT_CHANGED, SHUTDOWN), the backend notification queue (a `Zenith_BoundedQueue<Zenith_AudioControlRequest>` of 64 cells, `QUEUE_FULL_POLICY_DROP_NEWEST`, counter names `audio.control.backend`: miniaudio may raise STOPPED, REROUTED and INTERRUPTION_BEGIN/END and its log callback from several of its own threads at once, so this channel is multi-producer; producers call only `TryPush`, which takes no lock and calls no hook), and the focus ring (a `Zenith_SpscRing`; its one producer is the Java main looper).
3. Feed each event, in channel order (main, backend, focus), to the device state machine (§ "Device state machine"), which performs the backend calls (open, start, stop, close) on this thread.
4. Housekeeping every 100 ms while RUNNING: read the latency estimate; on Android, the xrun adaptation (§ "Buffer sizing and xrun adaptation"); on retry deadlines, the RETRY_TIMER event.
5. On SHUTDOWN: close the device, uninitialise the miniaudio context, detach from the JVM (Android), exit.

The control thread owns the miniaudio context (`ma_context`) and device (`ma_device`): it is the only AUDIO thread that calls `ma_context_init`, `ma_device_init`, `ma_device_start`, `ma_device_stop`, `ma_device_uninit` and `ma_context_uninit`. miniaudio may still run work on threads of its own (a reroute's stream re-initialisation, notifications); AUDIO makes no claim about them beyond what `Zenith2/Docs/Design/AUDIO.md` → "ZENITH_SUPPORTED matrix and miniaudio configuration" states, and holds nothing when it calls miniaudio.

### Decode thread loop

The decode thread owns no slot state and frees nothing. It reaches stream slots and cache entries only through the two rings of § "Audio.Mix and decode thread channels", and keeps per slot only its private refill state: the bound stream block, the byte cursor, the CURRENT and NEXT chunk roles, the outstanding read, the read priority, the prebuffer target and three flags (awaiting prebuffer, awaiting starvation recovery, closing).

1. Wait on the decode semaphore (5 ms timeout; `Audio.Mix` posts it after its M7 pushes, and the device never posts it). A wake by the timeout is safe: everything the thread reads that `Audio.Mix` wrote arrived through the request ring (its acquire refresh orders the record and the memory it names), and everything else is the thread's own private state.
2. Pop every request in ring order and apply it: OPEN binds the slot's private state to the record's stream block, seeks the decoder (or the PCM16 offset) to the start frame, sets the prebuffer target and the read priority, submits the first read through the read source (a memory stream reads nothing) and pushes OPENED; SEEK requests cancellation of the outstanding read (keeping its NEXT buffer until the read reaches a terminal status), resets the cursor and chunk roles at the record's start frame, sets the prebuffer target and the awaiting-prebuffer flag; STARVE raises the read priority to `ASSET_PRIORITY_CRITICAL` for every read submitted from now on and sets the awaiting-recovery flag; CLOSE requests cancellation of the outstanding read and sets the closing flag; CACHE_FILL appends the job to the thread's private job list (at most `uAUDIO_MAX_CACHE_FILLS`).
3. For each slot with private state, in slot order, run the refill step of `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream refill algorithm" (incremental decode of the current chunk, read submission ahead of need), then: when awaiting prebuffer and the ring holds at least the prebuffer target, push PREBUFFERED and clear the flag; when awaiting recovery and the ring holds at least the prebuffer target, set the read priority back to `ASSET_PRIORITY_HIGH`, push STARVED_RECOVERED and clear the flag; on a read FAILED, or CANCELLED by something other than this slot's own CLOSE or SEEK (a read a SEEK cancelled is discarded and its NEXT buffer freed for the next read), push READ_FAILED, on a decoder error DECODE_FAILED, and stop refilling the slot (it waits for its CLOSE); when closing and no read is outstanding (none was, or it reached a terminal status), drop the private state and push CLOSED. The ring fill is measured on the producer side as ⌊N / C⌋ − (`GetWritableCount()` − (N mod C)) / C frames, a lower bound of the readable frames.
4. Run pending cache-fill jobs (decode a whole cache-eligible compressed sound into the cache entry's storage `Audio.Mix` allocated), at most 2 ms of decode per wake so streams are never starved by a cache fill; a finished job pushes CACHE_FILLED, a failed one CACHE_FAILED. A job's decoder is the thread's own: created at the job's start and destroyed at its end on this thread.
5. Publish its busy time (atomic) for `audio.decode.us_delta`.
6. When the stop flag is set (destruction, `Zenith2/Docs/Design/AUDIO.md` → "Shutdown and detach"), request cancellation of every outstanding read, wait for their terminal statuses with the read source's `ServiceUntilComplete` (2,000 ms in all; a read still running then is logged as a leak), and exit.

### Audio.Mix and decode thread channels

<!-- elaboration -->
Stream slots and cache fills cross between `Audio.Mix` (a job worker per frame) and the decode thread through exactly one ring each way, and through nothing else. Ownership is split so that no field has two writers:

| Datum | Owner (sole writer) | The other side's access |
|---|---|---|
| a slot's state (IDLE … CLOSING), generation, owner world and voice, recorded retirement epoch E, the asset handle in its stream block | `Audio.Mix` (M1 on events, M7 on decisions) | none: the decode thread learns of a slot only from requests |
| a stream block (ring storage and its `Zenith_SpscRing<sint16>`, chunk buffer pair, packet scratch, decoder) | allocated by `Audio.Mix` M7 before the OPEN push; retired by M1 after CLOSED, freed by M9 at E ≤ R | the decode thread uses it from popping OPEN to pushing CLOSED (the ring's release/acquire hands it over each way); the device pops the stream ring |
| a slot's refill state (cursor, chunk roles, outstanding read, read priority, prebuffer target, flags) | the decode thread | none: `Audio.Mix` sees only events |
| a cache entry's storage | allocated by `Audio.Mix` M7 before the CACHE_FILL push; written by the decode thread until CACHE_FILLED or CACHE_FAILED | until the answer is drained (M1), `Audio.Mix` lists the entry in no snapshot and never evicts or purges it |

| Channel | Type and capacity | Producer → consumer | Full policy | Counters |
|---|---|---|---|---|
| decode request ring | `Zenith_SpscRing<Zenith_AudioDecodeRequest>` over `uAUDIO_DECODE_REQUEST_CAPACITY` (256) records of 64 bytes (16 KiB, allocated at create) | `Audio.Mix` M7 (the producer role handed over by frame-graph edges) → the decode thread (under the offline backend, `ServiceBeforeFrame`) | cannot fill (bound 128 below); a failed `TryPush` is `ZENITH_ASSERT` (an AUDIO defect) | `audio.decode.requests.full` (always 0), `.dropped` (0, asserted), `.high_water` |
| decode event ring | `Zenith_SpscRing<Zenith_AudioDecodeEvent>` over `uAUDIO_DECODE_EVENT_CAPACITY` (512) records of 32 bytes (16 KiB, allocated at create) | the decode thread (offline: `ServiceBeforeFrame`) → `Audio.Mix` M1 (the consumer role handed over by frame-graph edges) | cannot fill (bound 256 below); a failed `TryPush` is `ZENITH_ASSERT` | `audio.decode.events.full` (always 0), `.dropped` (0, asserted), `.high_water` |

`Zenith_AudioDecodeRequest` (Internal, 64 bytes, trivially copyable):

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | kind: 1 OPEN, 2 SEEK, 3 STARVE, 4 CLOSE, 5 CACHE_FILL |
| 1 | u8 | read priority (`Zenith_AssetPriority`; OPEN: HIGH, or CRITICAL for a music transition due within 500 ms) |
| 2 | u16 | stream slot (kinds 1–4) or cache entry index (kind 5) |
| 4 | u16 | slot or entry generation |
| 6 | u16 | flags: bit 0 loop, bit 1 memory stream (RESIDENT_COMPRESSED, no read) |
| 8 | u64 | request sequence: strictly increasing from 1 per system |
| 16 | u64 | OPEN, SEEK: the start frame |
| 24 | u32 | the sound record index |
| 28 | u32 | OPEN, SEEK: the prebuffer target in frames (the effective prebuffer, or 2,048 at memory WARNING) |
| 32 | ptr | OPEN: the stream block (`Zenith_AudioStreamBlock*`); CACHE_FILL: the entry's int16 storage |
| 40 | u64 | CACHE_FILL: the storage bytes |
| 48 | 16 bytes | 0 |

`Zenith_AudioDecodeEvent` (Internal, 32 bytes, trivially copyable):

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | kind: 1 OPENED, 2 PREBUFFERED, 3 STARVED_RECOVERED, 4 READ_FAILED, 5 DECODE_FAILED, 6 CLOSED, 7 CACHE_FILLED, 8 CACHE_FAILED |
| 1 | u8 | 0 |
| 2 | u16 | stream slot or cache entry index |
| 4 | u16 | generation (copied from the request it answers) |
| 6 | u16 | 0 |
| 8 | u64 | the sequence of the request it answers (OPENED, PREBUFFERED, STARVED_RECOVERED, CLOSED, CACHE_FILLED, CACHE_FAILED); 0 for READ_FAILED and DECODE_FAILED |
| 16 | u64 | READ_FAILED: the read's `Zenith_AssetError` code; DECODE_FAILED: the decoder's error; PREBUFFERED: the frames buffered; otherwise 0 |
| 24 | u64 | 0 |

Rules (`Audio.Mix` side):

1. A slot has at most three requests outstanding, where a request is outstanding from its push until its answering event is drained in M1: OPEN is answered by PREBUFFERED (OPENED only advances OPENING → PREBUFFERING), SEEK by PREBUFFERED, STARVE by STARVED_RECOVERED, CLOSE by CLOSED, and a READ_FAILED or DECODE_FAILED answers every outstanding request of the slot but the CLOSE it causes. `Audio.Mix` pushes SEEK or STARVE only after OPEN is answered; pushes a STARVE only while no STARVE is outstanding; pushes a SEEK only while no SEEK is outstanding (a newer seek target is held in the slot and pushed when the earlier SEEK is answered; a STARVE is not pushed while a SEEK is outstanding, whose prebuffer covers it); and pushes nothing after CLOSE. So the outstanding set is {OPEN}, or a subset of {SEEK, STARVE} plus at most one CLOSE: at most 3 per slot.
2. At most `uAUDIO_MAX_CACHE_FILLS` (32) CACHE_FILL requests are outstanding; a cache miss while 32 are outstanding plays from the memory stream and requests its fill at the first post of that sound after fewer than 32 are outstanding.
3. Request ring bound: 32 slots × 3 + 32 = 128 ≤ 256 records. Event ring bound: each outstanding request is answered by at most 2 events (OPEN: OPENED and PREBUFFERED), a slot generation pushes at most one failure event, and a cache fill one answer, so at most 32 × (3 × 2 + 1) + 32 = 256 ≤ 512 records are ever unread, whatever the decode thread's or `Audio.Mix`'s scheduling (a slot is reused only after M1 drained its CLOSED, and the ring is FIFO, so no event of an older generation is still unread).
4. A slot's generation increments when M1 returns it to IDLE; an event whose generation differs from the slot's current one, or that arrives for a slot in IDLE, is discarded (it cannot occur by rule 3's FIFO argument; the check is defence against defects and counts nothing).
5. When M7 decides a close it records E, the epoch of the snapshot M8 builds in the same frame, and lists the slot's voice in no snapshot from then on; E is therefore no earlier than the first snapshot built without the voice's reference to the ring. M1 appends the stream block with that E when CLOSED arrives, so its memory is freed only when E ≤ R (§ "Sample memory retirement").

`Test_AudioStream_MixDecodeChannelStress` (win-asan) opens, seeks and closes streams at frame rate against a decode thread running at full speed for 10^5 cycles, over the scripted read source with seeded random read delays and a 1-in-50 FAILED injection, and asserts: every OPEN answered by exactly one PREBUFFERED or one failure; every CLOSE answered by exactly one CLOSED, never before the slot's read reached a terminal status; every stream block retired exactly once and freed only after the released epoch passed its E; both rings' high water within 128 and 256 with no drop; no event of a stale generation applied.

### Null pacer thread loop

The pacer renders blocks at real time with no device: deadline_n = t0 + n × 256 / 48,000 s (computed in integer nanoseconds: n × 5,333,333 + n/3 to stay exact over long runs); at each deadline it calls `Zenith_AudioMixer::RenderBlocks` for 256 frames into a scratch buffer that is discarded (or copied to the capture ring) and records the clock pair. If it falls more than 4 blocks behind (the host stalled), it skips to the current block and counts `audio.mixer.pacer_skips`. The pacer is what makes the headless path run voices, events, virtualisation and budgets exactly as with a device ([Z2-N-ARCH-867]; Architecture's module table: "a null output device; voices, events and budgets still run").

## Real-time contract

### Rules for code on the device thread

Code reachable from `Zenith_AudioMixer::RenderBlocks`:

1. allocates and frees nothing (no `Zenith_Heap`, no `new`, no third-party allocation);
2. takes no `Zenith_Mutex` or any OS lock, waits on no semaphore or condition variable, never sleeps, yields or joins;
3. logs nothing and calls no profiler hook (`Zenith_GetProfilerHooks()` is never read there);
4. performs no I/O and no system call other than reading the monotonic clock (`Zenith_GetMonotonicTimeNs`, `Zenith2/Docs/Design/CORE/Diagnostics.md` → "Clocks": a vDSO `clock_gettime` or `QueryPerformanceCounter` read) and, at most once per device thread, `Zenith_GetCurrentThreadID` (which may be the `gettid` system call on Android: bounded, non-blocking, and only on the first callback of a new thread, § "Floating-point setup on the device thread");
5. calls no miniaudio function other than returning from the data callback, and no codec function;
6. touches only memory published to it through the channels below, its own preallocated state, and the caller's output buffer;
7. bounds its work: a callback of uFrames frames renders exactly ⌈max(0, uFrames − b0) / 256⌉ ≤ ⌈uFrames / 256⌉ blocks (b0 ∈ [0, 255] the frames left in the one-block FIFO at entry, `Zenith2/Docs/Design/AUDIO.md` → "Mixer callback step by step" step 2), so the FIFO never holds more than 256 frames whatever the callback size: 2 blocks for the default 480-frame WASAPI period, 4 for a 1,024-frame period, 0 or 1 for an AAudio burst below 256 frames; uFrames itself is bounded by the backend's buffer (WASAPI's shared-mode buffer, AAudio's buffer capacity), which the backend reports at open; each block touches at most 256 voices, 64 buses and 4 aux returns; every loop bound is a constant, `uFrames`, or a snapshot count clamped to its capacity;
8. never traps: indices from the snapshot are clamped, NaNs are cleared at the final clamp, integer arithmetic on positions is unsigned 64-bit with defined wrap.

The callback holds a `Zenith_NoAllocationScope` for its whole body (the assumption pending CORE in `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas": a field of the Debug thread state present in every configuration and checked wherever `ZENITH_ASSERTS_ENABLED=1`), so a violation of rule 1 asserts in every asserting preset that renders audio; `Test_AudioRealTime_NoAllocationInCallback` renders 10,000 blocks with every DSP feature active and asserts that AUDIO's test heap's `GetStats()` shows the same total allocation count before the first and after the last block.

### Real-time source set enforcement

<!-- elaboration -->
The device thread runs only `Internal/RealTime/**` and the trampolines in `Internal/Device/` (each a single call to `RenderBlocks`). `Test_AudioRealTime_SourceSetIncludeWhitelist` scans every file under `Internal/RealTime/` (the test target gets the source path as the compile definition `ZENITH_AUDIO_SOURCE_DIR`) and fails if any `#include` is outside the whitelist: `ZenithAudio/Zenith_AudioTypes.h`, `ZenithAudio/Zenith_AudioMath.h`, `Internal/RealTime/**`, `ZenithCore/Zenith_Types.h`, `ZenithCore/Zenith_SpscRing.h`, `ZenithCore/Zenith_TripleBuffer.h`, `ZenithCore/Zenith_Clock.h` (`Zenith_GetMonotonicTimeNs`), `ZenithCore/Zenith_Multithreading.h` (for `Zenith_GetCurrentThreadID` alone; the identifier scan below rejects every other name it declares), `ZenithCore/Zenith_NoAllocationScope.h`, `ZenithCore/Zenith_Assert.h`, `ZenithJobs/Zenith_FloatEnvironment.h` (`Zenith_IsExpectedFloatEnvironment`, `Zenith_ApplyFloatEnvironment`, [Z2-N-CONTRACT-360]), `<atomic>`, `<cstdint>`, `<cstring>`, `<cmath>` (for `std::sqrt` alone; a second scan rejects any other `std::` function from `<cmath>` by name: `sin`, `cos`, `tan`, `exp`, `log`, `pow`, `atan`, `asin`, `acos`, `sinh`, `cosh`, `tanh`). It also rejects the identifiers `Zenith_Mutex`, `Zenith_Semaphore`, `Zenith_ConditionVariable`, `ZENITH_LOG`, `Zenith_GetProfilerHooks`, `Zenith_Heap`, `Zenith_New`, `ma_`, `Zenith_Thread`, `Zenith_SleepUs`, `Zenith_YieldThread`, `Zenith_SetThreadName` and `Zenith_BoundedQueue` in those files. `ZENITH_ASSERT` is allowed (Debug-only, no allocation on its fast path; a failed assert terminates).

### Lock-free channels

| Channel | Primitive | Producer → consumer | Full or empty behaviour |
|---|---|---|---|
| mixer snapshot | `Zenith_TripleBuffer<Zenith_AudioMixerSnapshot>` | `Audio.Mix` (step M8; the producer role passes between job workers frame to frame through the frame graph's edges) → device | a newer snapshot supersedes an unread one (DROP_OLDEST, depth 1); none new → mix the previous ([Z2-N-ARCH-435]) |
| feedback | `Zenith_SpscRing<Zenith_AudioFeedbackRecord>`, 1,024 records | device → `Audio.Mix` (step M1, the only consumer) | full: drop (the ring's `.dropped`), recovered by the slot-state table |
| meter frame | `Zenith_TripleBuffer<Zenith_AudioMeterFrame>`, three slots of 1,040 bytes (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Meter frame record") | device (every 4th block) → `Audio.Mix` (step M1) | a newer frame supersedes an unread one (DROP_OLDEST, depth 1), counted in `audio.meters.dropped`; meters never occupy the feedback ring |
| slot-state table | `std::atomic<uint32>[256]`: generation << 8 \| state (0 free, 1 playing, 2 fading out, 3 ended) | device (release) → `Audio.Mix` (acquire) | — |
| callback timing | `std::atomic<uint64>` maxima and an overrun count, relaxed | device → `Audio.Mix` | — |
| stream ring (per stream) | `Zenith_SpscRing<sint16>` of N = the smallest power of two ≥ `audio.streams.ringFrames` × channels samples, pushed and popped in whole frames only (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots") | decode thread (offline: the owner thread in `ServiceBeforeFrame`) → device | empty: the voice outputs silence for the missing frames, skips them when they arrive, and pushes one underrun record per episode |
| capture ring | `Zenith_SpscRing<float>` | device → control thread | full: drop the block, count the gap |
| frame counter | `std::atomic<uint64>` (release by the device, acquire by readers) | device → any | — |
| applied and released epochs | two `std::atomic<uint64>` | device → control side | — |
| main request ring, main state, acknowledgements, voice inspector | `Zenith_SpscRing<Zenith_AudioMainRequest>` (64), `Zenith_TripleBuffer<Zenith_AudioMainState>`, atomics, `Zenith_TripleBuffer<Zenith_AudioInspectorFrame>` | main thread ↔ `Audio.Mix` | `Zenith2/Docs/Design/AUDIO.md` → "Main thread to Audio.Mix channels" (they never involve the device thread) |
| decode request ring, decode event ring | `Zenith_SpscRing<Zenith_AudioDecodeRequest>` (256), `Zenith_SpscRing<Zenith_AudioDecodeEvent>` (512) | `Audio.Mix` ↔ decode thread | § "Audio.Mix and decode thread channels" (they never involve the device thread) |
| control requests | two `Zenith_SpscRing<Zenith_AudioControlRequest>` (64 each: main, focus) and one `Zenith_BoundedQueue` (64, backend notifications, multi-producer) | → control thread | full: the request is dropped and counted (`audio.control.dropped`); the main thread re-sends a state request (STOP_AUDIO, RESUME_AUDIO) whose push failed on its next frame, so a state change is never lost |

The control side guarantees the snapshot's targets are complete, so dropping an unread snapshot loses nothing: a voice start stays listed (with its start flag) in every snapshot until it is acknowledged, either by the feedback ring's "started" (or "ended") record or by the slot-state table showing the assigned generation in state 1 playing, 2 fading out or 3 ended (`Audio.Mix` step M1), so even a start whose snapshot was superseded is applied by the next one, and a dropped "started" record never leaves the flag set. `Test_AudioRtChannels_StressSnapshotAndFeedback` runs the control side and a device-like thread at full speed for 10^6 frames under win-asan and asserts every start is applied exactly once and every end is reported exactly once, either by its record or by the slot-state reconciliation (the test shrinks the feedback ring to 8 records to force drops). The primitives' own concurrency proofs are CORE's `linux-tsan` stress tests (ZenithAudio has no `linux-tsan` tree, [Z2-N-ARCH-173]).

### Sample memory retirement

<!-- elaboration -->
Memory the device may read (sample data, decoded-cache entries, stream rings, real-time layouts) is never freed while a snapshot that references it could still be applied, nor while a device voice that read it is still fading. Applying a snapshot is not enough: a voice the snapshot no longer lists keeps reading its samples for its 5 ms stop fade, a voice made virtual for its 10 ms fade, and a closed stream's ring until its voice's fade ends.

Two epochs. Every snapshot carries an epoch, strictly increasing from 1 (epoch 0 is the empty snapshot of `Zenith_AudioSystem::Create`). After applying one, the device stores its epoch as the applied epoch A (release). Each device voice slot records L, its last listed epoch: the epoch of the last snapshot the device APPLIED that listed the voice (written at every apply that lists it, kept unchanged once a snapshot stops listing it). At the end of every block the device stores the released epoch

R = A when no slot is fading, otherwise min(A, min over fading slots of L) (release).

When the control side stops referencing a block of such memory (a voice ended and its sound unloaded, a cache entry evicted, a layout replaced, a stream slot closed), it records E, the epoch of the first snapshot it BUILDS that no longer references the memory, and appends (pointer, E) to the retire list (a fixed array of 4,096 entries, allocated at create). When the list is full, the control side does not release the memory it was about to retire: it keeps the memory referenced by its owner record (the sound record, the cache entry, the stream slot's block or the layout), marks that owner retire-pending, counts `audio.retire.deferred`, and retries the append in the next `Audio.Mix`'s M9, after that step has freed every due entry, in the order the appends were first attempted; a retire-pending owner is never reused, so nothing is freed early and nothing leaks. `Test_AudioRetirement_FullListDefersRelease` shrinks the list to 4 entries, retires 10 sound buffers in one frame and asserts that 4 are appended, 6 are counted deferred and stay allocated (win-asan), and every one is freed after the released epoch passes its E. `Audio.Mix` step M9 frees every entry with E ≤ R, never on A alone.

Why E ≤ R is safe even when the triple buffer supersedes snapshots. A device voice can read the memory only if some applied snapshot listed it with that memory, and every snapshot that lists it has an epoch ≤ E − 1 (every snapshot of epoch ≥ E was built without it). Two cases cover every moment such a voice can read the memory: while no snapshot ≥ E has been applied, A ≤ E − 1, so R ≤ A < E; once a snapshot ≥ E is applied the voice is unlisted and fading with L ≤ E − 1, so R ≤ L < E for as long as the fade lasts. R ≥ E therefore implies that a snapshot ≥ E was applied and that no voice which could read the memory is still fading. The rule uses the epoch the device applied, never the epoch the control side built next: under the old orphan-epoch rule (R = min(A, O − 1) with O "the epoch at which the snapshot stopped listing it", which the device can only observe as the first applied non-listing epoch) a superseded snapshot makes O exceed E.

Worked case (normative). Snapshot 5 lists voice v reading sample buffer S. Snapshot 6 is built without v, so S is retired with E = 6. Snapshot 6 is superseded unread by snapshot 7 (routine: a 480-frame WASAPI period renders two blocks per callback, and 144 Hz frames publish about 1.4 snapshots per 10 ms). The device applies 7: A = 7; v is unlisted and begins its 5 ms (240-frame) fade with L(v) = 5. R = min(7, 5) = 5 < 6: S stays allocated. The fade ends within the next block; the device then stores R = 7 ≥ 6, and the next M9 frees S. (The old rule gave O = 7, R = min(7, 6) = 6 ≥ E, freeing S during the fade.) `Test_AudioRetirement_SupersededSnapshotKeepsFadingMemory` scripts exactly this sequence on the scripted backend (the backend withholds `AcquireLatest` across the publishes of 6 and 7, and `HoldDeviceFades` keeps v's fade open across three further `Audio.Mix` runs) and asserts under win-asan that S, v's cache entry and its stream ring stay allocated until the fade ends, that R reads 5 throughout the fade, and that they are freed at the first M9 after R reaches 7.

Dropped feedback. A voice-ended record lost to a full feedback ring would leave the control side thinking the slot busy. The slot-state table removes the dependence: the device writes each slot's (generation, state) whenever it changes, and `Audio.Mix` step M1 compares it with its ownership table every frame; a slot reported free or ended for the generation AUDIO assigned is ended even when no record arrived, and a slot reported playing, fading out or ended for that generation acknowledges the voice's start even when the "started" record was dropped (each reconciliation counts `audio.feedback.reconciled_delta`), so neither a start flag nor a slot can stay stuck. The sound loader's `Unload` acknowledges at once (the memory is queued, `Zenith_AssetUnloadContext::AcknowledgeUnload`'s "queued for deferred deletion"). While the device is stopped (SUSPENDED, INTERRUPTED, CLOSED) no snapshot is applied, so retirement waits; at shutdown, after the device and the pacer are stopped, the list is freed whole. `Test_AudioRetirement_FreesOnlyAfterEpochAcknowledged` drives the scripted backend to withhold application and asserts nothing is freed until it applies the epoch; `Test_AudioRetirement_NothingFreedDuringFade` holds a stopped voice's fade open (`Zenith_AudioSystemTestAccess::HoldDeviceFades`) across many applied snapshots and asserts its sample buffer, cache entry and stream ring stay allocated until the fade ends and R passes their E; `Test_AudioRetirement_ReconcilesDroppedEndRecord` fills the feedback ring and asserts the slot is recovered through the slot-state table.

### Floating-point setup on the device thread

Each callback calls `Zenith_IsExpectedFloatEnvironment()` (`ZenithJobs/Zenith_FloatEnvironment.h`: a read of the MXCSR or FPCR register, no system call); when it returns false (the first callback on a thread, a WASAPI reopen, an AAudio stream recreation, a thread miniaudio created after a reroute) it calls `Zenith_ApplyFloatEnvironment()` (FTZ and DAZ, round-to-nearest; `Zenith2/Docs/Contracts.md` → "Floating-point environment", [Z2-N-CONTRACT-360]), reads `Zenith_GetCurrentThreadID` once and stores it in the mixer (`std::atomic<uint64>`, for the status and the thread-change telemetry event). The control, decode and pacer threads apply it at thread start. `Test_AudioRealTime_FloatEnvironmentAppliedOnNewThread` asserts the denormal flags inside the callback of the miniaudio null backend.

## Device backends

### Backend selection

| `audio.device.backend` | Headless | Backend | Used for |
|---|---|---|---|
| `auto` | no | miniaudio with WASAPI (Windows) or AAudio (Android) | games and the editor |
| `auto` | yes | null pacer | headless runs (forced, [Z2-N-ARCH-867]) |
| `miniaudioNull` | either | miniaudio with its null backend | CI tests of the real miniaudio device path (`Test_AudioDevice_MiniaudioNullBackendRunsCallback`) |
| `null` | either | null pacer | tests, servers |
| `offline` | either | offline (no thread; frames pulled by `Zenith_AudioOfflineRenderer`) | bit-exact tests, the editor's render-to-file |

ZenithAudio is not built on Linux at all (it is `ZENITH_GPU`, never linux-tsan); every backend above exists on Windows and Android, and the CI device-path tests run on Windows.

### miniaudio backend

`Internal/Device/Zenith_AudioMiniaudioBackend.cpp` implements `Zenith_AudioDeviceBackend`:

- Context: `ma_context_init` with the backend list {WASAPI} on Windows, {AAudio} on Android, {null} for `miniaudioNull`; allocation callbacks to the root heap (`Internal/Device/Zenith_AudioMiniaudioAllocator.cpp`, [Z2-N-CONTRACT-240]); logging callback forwarding to `audio.device` through the control thread only (a log callback, on whatever miniaudio thread raises it, copies at most 120 bytes of the message into a backend notification record and pushes it; the control thread logs it).
- Device: `ma_device_type_playback`, format `ma_format_f32`, channels from the output layout (2, 6 or 8), sample rate 48,000 (miniaudio's resampler converts when the endpoint runs at another rate; the status reports the endpoint's rate), `periodSizeInFrames` from `audio.device.periodFrames`, `performanceProfile = ma_performance_profile_low_latency`, `noPreSilencedOutputBuffer = MA_TRUE`, `noClip = MA_TRUE` (AUDIO clamps itself), the data callback the trampoline to `RenderBlocks`, the notification callback pushing to the backend notification queue.
- The data callback's user data is the mixer pointer; it never touches the backend object.

### Null backend

The null pacer (§ "Null pacer thread loop"); `IsRealTime()` true, `GetInfo()` reports 48,000 Hz, the layout's channels, period 256, latency 0 and the name "Null".

### Offline backend

No thread. `Zenith_AudioOfflineRenderer::Render` calls `RenderBlocks` on the caller's thread; the clock is the rendered frame count, the latency 0, `IsRealTime()` false (the control side then schedules triggers against the rendered clock and never applies the wall-clock fit).

### Test backend

`Tests/Support/Zenith_AudioScriptedBackend.h` implements `Zenith_AudioDeviceBackend` for tests: `Open` succeeds or fails per a script; a test thread or the test itself calls the render callback with scripted frame counts (including odd sizes such as 1, 191, 480, 1,024), injects notifications (STOPPED, REROUTED, INTERRUPTION_BEGIN/END) at scripted frames, and can delay or withhold callbacks to simulate overruns and starvation. It is injected through `Zenith_AudioSystemTestAccess::CreateWithBackend`; no concrete class is doubled.

## Device state machine

### States

| State | Meaning | Who mixes |
|---|---|---|
| CLOSED | before `Start`, after shutdown | nobody |
| OPENING | the control thread is opening the device (the open is synchronous on the control thread, which feeds OPEN_OK or OPEN_FAILED before draining another event; the other cells exist so the machine is total and tests can drive it directly) | entered from LOST or REROUTING: the null pacer, which keeps running until the device starts; entered from CLOSED, SUSPENDED or INTERRUPTED: nobody, the mixer clock paused |
| RUNNING | the device is started | the device thread |
| REROUTING | the device must reopen with a new format | the null pacer |
| LOST | no device; retries scheduled | the null pacer |
| SUSPENDED | the app is backgrounded (`OnStopAudio`) | nobody; the mixer clock is paused and voices keep their positions |
| INTERRUPTED | Android focus lost (call, alarm, another media app); the flag `m_bFocusLostPermanently` records an `AUDIOFOCUS_LOSS` | nobody; as SUSPENDED |
| CLOSING | shutdown in progress | nobody after the device stopped |
| NULL_DEVICE | headless or the `null` backend: permanent null pacer | the null pacer |
| OFFLINE | the offline backend | the offline renderer's caller |

### Events

START (`Zenith_AudioSystem::Start`), OPEN_OK, OPEN_FAILED, STOPPED_UNEXPECTEDLY (miniaudio's stopped notification without a requested stop; `AAUDIO_ERROR_DISCONNECTED` when miniaudio could not reroute), REROUTED (miniaudio's rerouted notification), FORMAT_CHANGED (a reroute to an endpoint whose channel count differs from the layout, or a user `audio.outputLayout` change), RETRY_TIMER, STOP_AUDIO, RESUME_AUDIO, FOCUS_LOSS_TRANSIENT, FOCUS_LOSS_CAN_DUCK, FOCUS_LOSS, FOCUS_GAIN, WINDOW_FOCUS_GAINED (`PLATFORM_EVENT_WINDOW_FOCUS_GAINED` from the frame listener, or the first `PLATFORM_EVENT_INPUT_TRANSITION` of a frame while INTERRUPTED with the permanent flag, at most once per 2 s, § "Audio focus"; fed to the machine on Android only), SHUTDOWN.

### Transition table

<!-- elaboration -->
Every state × event cell is defined; `Test_AudioDevice_TransitionTable` drives each of the 150 cells on the scripted backend and asserts the next state, the backend calls and who mixes. "Ignored" means unchanged state and `audio.device.ignored_events` + 1, except WINDOW_FOCUS_GAINED, which is ignored without counting in every state but INTERRUPTED. Actions used below:

| Action | Meaning |
|---|---|
| open | (Android) request focus first: GRANTED → open the device; DELAYED or FAILED → go to INTERRUPTED with a focus request pending instead; then `Open` with the current layout and feed the result; a device stopped by quiesce and still initialised is not re-created: open checks that the endpoint's format still matches the layout (otherwise drop-device and create a new one) and feeds OPEN_OK |
| start | stop the pacer at a block boundary if it runs; start the device; fade in 10 ms (50 ms after an interruption); `audio.device.reopens` + 1 if it is a reopen; telemetry event |
| to-pacer | start the null pacer within one block period (the mixer clock continues) |
| quiesce | fade out 20 ms, stop the device or the pacer, pause the mixer clock |
| drop-device | uninit the device (the next entry to OPENING opens a new one) |
| duck / unduck | master duck target −12 dB over 100 ms / back to 0 dB over 250 ms (a target, so it persists through any state) |
| relayout | rebuild the output layout from `audio.outputLayout` and the endpoint's channel count; the next open uses it |

| Event \ State | CLOSED | OPENING | RUNNING | REROUTING | LOST | SUSPENDED | INTERRUPTED | CLOSING | NULL_DEVICE | OFFLINE |
|---|---|---|---|---|---|---|---|---|---|---|
| START | OPENING (init the context once; open) for `auto` or `miniaudioNull`; NULL_DEVICE (start the pacer) for `null` or headless; OFFLINE for `offline` | ignored | ignored | ignored | ignored | ignored | ignored | ignored | ignored | ignored |
| OPEN_OK | ignored | RUNNING (start) | ignored | RUNNING (start) | ignored | ignored | ignored | ignored | ignored | ignored |
| OPEN_FAILED | ignored | LOST (to-pacer unless running; schedule the retry; log once per code) | ignored | LOST (as OPENING) | ignored | ignored | ignored | ignored | ignored | ignored |
| STOPPED_UNEXPECTEDLY | ignored | LOST (as OPEN_FAILED) | LOST (to-pacer; drop-device; retry at 0 s) | LOST (drop-device; retry at 0 s) | ignored | SUSPENDED (drop-device) | INTERRUPTED (drop-device) | ignored | ignored | ignored |
| REROUTED | ignored | OPENING (no action: the open in progress uses the new default) | RUNNING (same channel count: update name and latency; telemetry) | REROUTING (no action) | OPENING (immediate retry: open) | SUSPENDED (record the route change for `WhenDeviceChanged`) | INTERRUPTED (record the route change) | ignored | ignored | ignored |
| FORMAT_CHANGED | ignored | OPENING (relayout before the open) | REROUTING (fade 10 ms; stop and drop-device; to-pacer; relayout; open) | REROUTING (relayout) | LOST (relayout for the next retry) | SUSPENDED (relayout; drop-device) | INTERRUPTED (relayout; drop-device) | ignored | NULL_DEVICE (relayout; the pacer renders the new channel count) | OFFLINE (relayout) |
| RETRY_TIMER | ignored | ignored | ignored | ignored | OPENING (open with the current default endpoint) | ignored | ignored | ignored | ignored | ignored |
| STOP_AUDIO | ignored | SUSPENDED (close what the open created; never start; stop the pacer if it runs; pause the clock; abandon focus) | SUSPENDED (quiesce; abandon focus) | SUSPENDED (stop the pacer; pause the clock) | SUSPENDED (stop the pacer; cancel retries) | ignored | SUSPENDED (remember the focus loss; abandon focus) | ignored | NULL_DEVICE (pause the pacer and the clock) | ignored |
| RESUME_AUDIO | ignored | ignored | ignored | ignored | ignored | OPENING (open; the remembered focus loss makes open request focus first) | INTERRUPTED, or OPENING when a re-request of focus is GRANTED | ignored | NULL_DEVICE (resume the pacer and the clock) | ignored |
| FOCUS_LOSS_TRANSIENT | ignored | INTERRUPTED (close what the open created; stop the pacer if it runs; pause the clock) | INTERRUPTED (quiesce) | INTERRUPTED (stop the pacer; pause the clock) | INTERRUPTED (stop the pacer; cancel retries, so no retry opens the device during a call) | SUSPENDED (remember the loss) | INTERRUPTED (update the recorded loss) | ignored | ignored | ignored |
| FOCUS_LOSS_CAN_DUCK | ignored | OPENING (duck) | RUNNING (duck; the game ducks itself, `setWillPauseWhenDucked(false)`) | REROUTING (duck) | LOST (duck) | SUSPENDED (duck) | INTERRUPTED (duck) | ignored | ignored | ignored |
| FOCUS_LOSS | ignored | as FOCUS_LOSS_TRANSIENT, and set `m_bFocusLostPermanently` | as FOCUS_LOSS_TRANSIENT, and set the flag | as FOCUS_LOSS_TRANSIENT, and set the flag | as FOCUS_LOSS_TRANSIENT, and set the flag | SUSPENDED (remember a permanent loss) | INTERRUPTED (set the flag) | ignored | ignored | ignored |
| FOCUS_GAIN | ignored | OPENING (unduck) | RUNNING (unduck) | REROUTING (unduck) | LOST (unduck) | SUSPENDED (forget the remembered loss) | OPENING (clear the flag; unduck; open; the fade-in is 50 ms) | ignored | ignored | ignored |
| WINDOW_FOCUS_GAINED | ignored | ignored | ignored | ignored | ignored | ignored | with `m_bFocusLostPermanently`: re-request focus, GRANTED → OPENING (clear the flag; open), otherwise unchanged; without it: unchanged | ignored | ignored | ignored |
| SHUTDOWN | CLOSED (no-op) | CLOSING → CLOSED (close; stop the pacer; uninit the context; abandon focus) | CLOSING → CLOSED (quiesce; uninit device and context; abandon focus) | CLOSING → CLOSED (stop the pacer; uninit) | CLOSING → CLOSED (stop the pacer; cancel retries; uninit) | CLOSING → CLOSED (uninit; abandon focus) | CLOSING → CLOSED (uninit) | ignored | CLOSING → CLOSED (stop the pacer) | CLOSING → CLOSED |

The mix never depends on the state: voices, events and budgets run in every state except SUSPENDED and INTERRUPTED (where the mixer clock is paused and `Audio.Update` keeps accepting commands, applying them to instances whose voices stay paused). On resume, voices continue from their positions; voices whose scheduled start passed during the pause start immediately (late, counted).

`Start` returns immediately unless `audio.device.required` is true, in which case it waits (on the main thread, at startup step 15, outside frames) up to 2,000 ms for RUNNING and returns `AUDIO_ERROR_DEVICE_OPEN_FAILED` otherwise.

### Retry schedule

Retries after LOST or OPEN_FAILED at 0 s, 1 s, 2 s, 4 s, 8 s after the loss, then every 8 s, indefinitely, each logged at debug level after the first; a Windows endpoint-arrival or Android route event (the backend's REROUTED or a new default device) triggers an immediate retry. `Test_AudioDevice_RetryScheduleAfterLoss` asserts the schedule with a fake clock on the scripted backend.

## Windows output

### WASAPI configuration

Shared mode through miniaudio: `wasapi.usage = ma_wasapi_usage_pro_audio` (MMCSS "Pro Audio" on the device thread), automatic stream routing on (`noAutoStreamRouting = MA_FALSE`), the requested period 480 frames (10 ms), and miniaudio's sample-rate conversion when the endpoint mix format is not 48 kHz. The endpoint's channel count selects the layout under `auto`. The stage 3 ruling records the period miniaudio obtains on the reference desktop (§ "Rulings AUDIO records in its own tickets" of the entry).

### Endpoint changes on Windows

A new default endpoint (headphones plugged, a USB device, the user changing the default) is followed by miniaudio's automatic routing: the REROUTED notification arrives; with the same channel count the mix continues; with another, FORMAT_CHANGED reopens. Removing the only endpoint stops the device (STOPPED_UNEXPECTEDLY → LOST); an endpoint arriving triggers the retry. `audio.device.endpoint` is not a v1 key: the default endpoint is always used.

### Focus on Windows

Windows has no audio focus. `audio.muteWhenUnfocused` (default false) fades the master to silence over 100 ms on `PLATFORM_EVENT_WINDOW_FOCUS_LOST` and back on `PLATFORM_EVENT_WINDOW_FOCUS_GAINED` (presentation-only platform events), delivered by the frame listener's platform-event hook on the main thread.

## Android output

### AAudio configuration

Through miniaudio's AAudio backend: performance mode low latency, sharing mode shared (exclusive when `audio.android.exclusive`, falling back to shared when refused), usage `AAUDIO_USAGE_GAME`, content type `AAUDIO_CONTENT_TYPE_SONIFICATION`, format float, 48,000 Hz (the S24's native rate; miniaudio converts otherwise), channels 2 (the S24's speakers and wired or Bluetooth headsets are stereo; 5.1 and 7.1 are Windows layouts). The data callback is AAudio's real-time callback, which delivers `framesPerBurst` frames per call (the S24 value is the Stage 3 ruling's), served from the 256-frame block FIFO.

### Buffer sizing and xrun adaptation

At open the buffer size is set to the minimum m = max(2, ceil((256 + burst) / burst)) bursts (`AAudioStream_setBufferSizeInFrames` on miniaudio's stream handle, read from the device's internal AAudio stream pointer in `Internal/Android/Zenith_AudioAAudioTuning.cpp`), so the buffer always covers one 256-frame block plus one burst and a callback that renders a block cannot underrun while the mixer renders faster than real time (`Zenith2/Docs/Design/AUDIO.md` → "Measuring the mixer callback row"); with a 96-frame burst m = 4 bursts (384 frames, 8 ms). Every 100 ms the control thread reads `AAudioStream_getXRunCount`; an increase raises the buffer by 1 burst (at most max(8, m + 4) bursts, within the stream's capacity) and counts `audio.device.underruns`; 60 s without an xrun lowers it by 1 burst (at least m). On Windows the requested 480-frame period already exceeds a block, and WASAPI's shared-mode buffer holds two periods. The steady-state value on the S24 is the Stage 3 ruling's measurement.

### Latency measurement

Android: from `AAudioStream_getTimestamp(CLOCK_MONOTONIC, &framePosition, &timeNs)`, latency = (frames written − framePosition) / 48,000 + (now − timeNs), plus the FIFO's buffered frames; sampled every 100 ms and smoothed (median of the last 9). Windows: the WASAPI stream latency (`IAudioClient::GetStreamLatency` through miniaudio's device info) plus the buffered period frames plus the FIFO. The null pacer reports 0. The value feeds the clock (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "The device clock") and `audio.device.latency_us`.

### Device changes on Android

Plugging or unplugging a wired headset, connecting or disconnecting Bluetooth, or a USB audio device: AAudio disconnects the stream (`AAUDIO_ERROR_DISCONNECTED`); miniaudio reroutes by reopening on the new default route and reports REROUTED, or reports a stop when it cannot (STOPPED_UNEXPECTEDLY → LOST, with the retry at 0 s). A Bluetooth route raises the measured latency (typically 150–250 ms), which the clock and the scheduling offset absorb. Output to the speaker continues when a headset is unplugged (Android's default route); AUDIO does not pause on unplug (the `ACTION_AUDIO_BECOMING_NOISY` convention is a media-player one; a game may observe it through `Zenith.Audio.WhenDeviceChanged` and pause itself).

### Audio focus

The focus states and AUDIO's behaviour:

| Focus change | Android constant | AUDIO behaviour |
|---|---|---|
| gained | `AUDIOFOCUS_GAIN` | RUNNING (from INTERRUPTED: reopen and fade in 50 ms) |
| lost transiently | `AUDIOFOCUS_LOSS_TRANSIENT` (a call, an alarm, a navigation prompt) | INTERRUPTED: fade out 20 ms, stop, pause the mixer clock; resume on GAIN |
| lost transiently, may duck | `AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK` | stay RUNNING with the master ducked −12 dB; restore on GAIN |
| lost | `AUDIOFOCUS_LOSS` (another media app took focus) | INTERRUPTED with `m_bFocusLostPermanently`; AUDIO requests focus again at the next RESUME_AUDIO and at the next WINDOW_FOCUS_GAINED (the user returning to the game, including from the notification shade without backgrounding it). A permanent loss taken while the game window keeps focus (a Bluetooth media button, a playlist started from a notification or a wearable) produces neither event; for it the frame listener also feeds WINDOW_FOCUS_GAINED for the first `PLATFORM_EVENT_INPUT_TRANSITION` of a frame while `Zenith_AudioStatus` shows INTERRUPTED with the permanent flag, at most once per 2 s (the default; the Stage 3 focus ruling chooses among this, a periodic re-request and none, `Zenith2/Docs/Design/AUDIO.md` → "Rulings AUDIO records in its own tickets"); the S24 behaviour of both cases is recorded by device run AUDIO-DR-03 |

Focus is requested at START, at every RESUME_AUDIO and, after a permanent loss, at every WINDOW_FOCUS_GAINED (including the input-transition form above); abandoned at STOP_AUDIO and SHUTDOWN. `Zenith.Audio.WhenDeviceChanged` (Stage 4) lets a game pause its own simulation on an interruption; AUDIO never pauses the world itself. The S24 device run "audio focus: phone call, alarm, other media app" is the acceptance evidence for Z2-I-AUDIO-13 (`Zenith2/Docs/Design/AUDIO.md` → "Device runs").

### Focus Java bridge

<!-- elaboration -->
`Internal/Android/Java/org/zenith2/audio/Zenith2AudioFocus.java` (packaged by MOD's Gradle template with a keep rule; assumption pending MOD and PROC in `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas"):

```java
package org.zenith2.audio;

public final class Zenith2AudioFocus implements android.media.AudioManager.OnAudioFocusChangeListener
{
	private final long m_lNative;                          // the native owner, passed back on every callback
	private final android.media.AudioManager m_xManager;
	private final android.media.AudioFocusRequest m_xRequest;

	public Zenith2AudioFocus(android.app.Activity xActivity, long lNative)
	{
		m_lNative = lNative;
		m_xManager = (android.media.AudioManager) xActivity.getSystemService(android.content.Context.AUDIO_SERVICE);
		android.media.AudioAttributes xAttributes = new android.media.AudioAttributes.Builder()
			.setUsage(android.media.AudioAttributes.USAGE_GAME)
			.setContentType(android.media.AudioAttributes.CONTENT_TYPE_SONIFICATION).build();
		m_xRequest = new android.media.AudioFocusRequest.Builder(android.media.AudioManager.AUDIOFOCUS_GAIN)
			.setAudioAttributes(xAttributes).setWillPauseWhenDucked(false).setAcceptsDelayedFocusGain(true)
			.setOnAudioFocusChangeListener(this, new android.os.Handler(android.os.Looper.getMainLooper())).build();
	}
	public int requestFocus() { return m_xManager.requestAudioFocus(m_xRequest); }   // GRANTED 1, FAILED 0, DELAYED 2
	public void abandonFocus() { m_xManager.abandonAudioFocusRequest(m_xRequest); }
	@Override public void onAudioFocusChange(int iChange) { nativeOnFocusChange(m_lNative, iChange); }
	private static native void nativeOnFocusChange(long lNative, int iChange);
}
```

Native side (`Internal/Android/Zenith_AudioFocusAndroid.cpp`): at control-thread start, `AttachCurrentThread` on the `JavaVM*` from PLAT (assumption pending PLAT), load the class through the activity's class loader (`getClassLoader().loadClass("org.zenith2.audio.Zenith2AudioFocus")`, because `FindClass` on a native thread sees only system classes), `RegisterNatives` for `nativeOnFocusChange`, construct the object with the native owner's address as the `long`, and keep a global reference. `nativeOnFocusChange` runs on the Java main looper: it converts the change to a `Zenith_AudioFocusState`, pushes it to the focus ring and posts the control semaphore; it touches nothing else, so it holds no lock and keeps no static state (the owner arrives as the `long`, Z2-D-046). `requestFocus` and `abandonFocus` are called from the control thread. A JNI failure (class missing from a mis-packaged or over-shrunk APK) logs once and leaves AUDIO running without focus handling. `Test_AudioFocusBridge_JavaMatchesNativeRegistration` (a Windows unit test reading the Java source through `ZENITH_AUDIO_SOURCE_DIR`) asserts that the class name, the constructor signature `(Landroid/app/Activity;J)V`, the native method `nativeOnFocusChange` with signature `(JI)V`, and the methods `requestFocus` and `abandonFocus` in the Java file equal the strings in `Zenith_AudioFocusAndroid.cpp`'s registration table, so a rename on either side fails the `Test` step; packaging is MOD's (the keep rule) and its APK check a PROC hand-off.

### Lifecycle on Android

PLAT's lifecycle state machine calls `OnStopAudio` first on SUSPENDING × DID_ENTER_BACKGROUND and `OnResumeAudio` on RESUMING × DID_ENTER_FOREGROUND (`Zenith2/Docs/Design/PLAT/Android.md` → "Lifecycle state machine"); the runtime forwards both to AUDIO's frame listener ([Z2-N-ARCH-879]; `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"), which pushes STOP_AUDIO or RESUME_AUDIO to the control ring and returns. The control thread fades and stops within 30 ms; nothing is mixed while backgrounded; process death loses nothing (audio state is never saved). The frame listener also forwards `PLATFORM_EVENT_WINDOW_FOCUS_GAINED` as WINDOW_FOCUS_GAINED (permanent focus-loss recovery).

## Latency

### Latency budget composition

Output latency = block FIFO (the 0 … 255 frames left between callbacks, 0 … 5.3 ms; never more, because a block is rendered only into an empty FIFO) + the device buffer (Windows: 2 × the 10 ms period plus the endpoint's stream latency; Android: m … max(8, m + 4) bursts) + the hardware path (Bluetooth adds its codec delay). Trigger latency from simulation to sound adds the frame (≤ one frame time) and the scheduling of `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Scheduling a trigger" (0 … 100 ms, by design aligned with the presented frame). The proposed row "Audio output latency" (`Zenith2/Docs/Design/AUDIO.md` → "Pending new rows") bounds the first sum on the S24's speaker route and the reference desktop's wired output.

### Latency reporting

`Zenith_AudioStatus::m_uLatencyUs`, the counter `audio.device.latency_us`, the profiler row, and the frame dump report the measured value; the device run "output and latency" records it with a loopback measurement.
