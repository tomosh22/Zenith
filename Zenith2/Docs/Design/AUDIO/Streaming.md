# AUDIO design: loading, decoding and streaming

## Scope of this file

This file is part of the AUDIO design family (entry: `Zenith2/Docs/Design/AUDIO.md`). It specifies the two kind loaders and bank residency, the load-mode rule and the decoded-sample cache, the decoders (PCM16, IMA ADPCM, the D-SEL-CODEC codec), seeking and loop points, audio streaming (stream slots, the refill algorithm, priorities and starvation), AUDIO's half of coordinated world streaming (Z2-I-STREAM-11), and memory accounting with the shedding policy of `Zenith2/Docs/Architecture.md` → "Shedding per subsystem". Loading and I/O scheduling are ASSET's and PLAT's (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetKindLoader", `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetReader header"); AUDIO decides only what to read and when, and submits every stream read through its own read seam `Zenith_AudioReadSource` (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioReadSource"), whose production implementation forwards to `Zenith_AssetReader`.

## Sound and bank loaders

### Sound loader

`Zenith_AudioSoundLoader` implements `Zenith_AssetKindLoader` for kind 10, registered with `Zenith_AssetKindLoaderDesc{ "Audio.SoundLoader", 1, 0 }` (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Registration functions"):

1. `BeginLoad`: request the header, seek table, markers and codec setup sections; for a RESIDENT sound also the samples section, with `m_xDestination` an AUDIO-owned buffer allocated from AUDIO's heap at exactly the section's decoded size (so ZenithAssets decodes, a no-op for codec 0, straight into the memory the mixer will read); an allocation failure is `Fail(AUDIO_ERROR_OUT_OF_MEMORY << 8)`.
2. `OnSectionsLoaded`: run the checks of `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound file checks"; on failure free the buffer and `Fail` with the check code (the asset becomes the NULL_OBJECT fallback per the kind table); otherwise build the internal sound record in a slot of the sound record pool (the header fields, pointers to the sample buffer, the seek table and markers; for a codec sound a parsed setup; the record is immutable from here on), `SetResidentBytes(MEMORY_SUBSYSTEM_AUDIO, bytes, 0)`, push `SOUND_ACTIVATED` to the loader hand-off (§ "Loader hand-off") and `Activate`.
3. `Cancel`: free what was allocated, `AcknowledgeCancel`.
4. `Unload`: push `SOUND_UNLOADED` to the loader hand-off and `AcknowledgeUnload` ("queued for deferred deletion"); `Audio.Mix` step M2 moves the record and its buffers to the retire list (`Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement").
5. `DropCpuCopies`: return 0 (no sound is GPU-resident).

Each callback returns within ASSET's 1 ms rule: parsing is linear in the small sections; the samples are not touched beyond the size check. Loader callbacks run on ASSET's background slices, possibly concurrently for different assets (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetKindLoader"); they touch only their own record and the two lock-free queues below, never AUDIO's control state.

### Bank loader

`Zenith_AudioBankLoader` implements kind 11 (`"Audio.BankLoader"`, format version 1): request every section; run the bank checks (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank file checks"); build the record in a slot of the bank record pool (immutable from here on), push `BANK_ACTIVATED` to the loader hand-off and `Activate`. Every bank activates with its full tables: there is no dormant bank (a locale variant bank is simply loaded or not, `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Locale selection"). `Unload` pushes `BANK_UNLOADED` and acknowledges. `Audio.Mix` step M2 applies the events in hand-off order: the bank list (256 entries) gains or loses the bank, the process event index is rebuilt (merge of sorted arrays, O(total events)) and published (§ "Event index publication"), and a duplicate event ID across active banks keeps the event of the bank that joined the list first and counts `audio.events.duplicate_delta` with a one-time warning naming both banks' GUIDs (an active locale variant's events replace its base bank's by design and are not duplicates). A master bank activation also builds and publishes the real-time layout from its mixer section (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Bus hierarchy").

An audition-only bank (header flag bit 3, `Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank header section") is the exception: the preview cook `Zenith_AudioCookBankPreview` sets the bit, so the editor's in-process preview path (row AU18) decides it and the loader only copies it into the record. M2 never adds such a bank to the bank list, never rebuilds the event index for it, never builds a layout from it and never requests its locale variants; its BANK_ACTIVATED and BANK_UNLOADED events only mark the record live or dead. An audition reaches its events by the record index the main thread resolved from the desc's handle (`Zenith2/Docs/Design/AUDIO.md` → "Main thread to Audio.Mix channels" rule 2), so a preview of a loaded bank never duplicates or shadows that bank's events and counts no `audio.events.duplicate_delta`.

### Loader hand-off

<!-- elaboration -->
The loaders are multi-producer (any ASSET slice, concurrently) and `Audio.Mix` is the one consumer, so the hand-off is CORE's lock-free `Zenith_BoundedQueue<Zenith_AudioLoaderEvent>` (`Zenith2/Docs/Design/CORE/Containers.md` → "Zenith_BoundedQueue"), created at `Zenith_AudioSystem::Create` with hard capacity `uAUDIO_LOADER_HANDOFF_CAPACITY` (65,536), policy `QUEUE_FULL_POLICY_REJECT` and counter names `audio.loader.handoff`. Producers call only `TryPush` (no lock, no hook); `Audio.Mix` calls only `TryPop`, until empty, in step M2.

```cpp
struct Zenith_AudioLoaderEvent            // 8 bytes
{
	uint8 m_eKind;                       // 1 SOUND_ACTIVATED, 2 SOUND_UNLOADED, 3 BANK_ACTIVATED, 4 BANK_UNLOADED
	uint8 m_uReserved = 0;
	uint16 m_uReserved2 = 0;
	uint32 m_uRecord;                    // index into the sound or bank record pool
};
```

Records come from two fixed pools allocated at create (`uAUDIO_MAX_SOUND_RECORDS` sound records, `uAUDIO_MAX_BANKS` bank records), each with a lock-free free list (a `Zenith_BoundedQueue<uint32>` of free indices): a loader takes an index with `TryPop` (an empty free list fails the load with `AUDIO_ERROR_CAPACITY`), and `Audio.Mix` returns it with `TryPush` only when the record's retirement completes. A record therefore emits at most two events (activated, unloaded) before its index is reused, so at most 2 × (16,384 + 256) = 33,280 events are ever outstanding: the queue cannot fill, and a `QUEUE_ERROR_FULL` from `TryPush` is `ZENITH_ASSERT` (an AUDIO defect). `Test_AudioBankLoader_ConcurrentActivationsHandOff` activates two banks and 64 sounds from four threads at once and asserts that after the next `Audio.Mix` both banks are in the index exactly once, every sound record is live, and the queue's statistics show no full event.

### Event index publication

`Audio.Mix` builds a new index into a newly allocated block (control side), then publishes it with `m_pxEventIndex.store(pxNew, release)`. Every reader (`Zenith_AudioSystem::FindEvent`, `Audio.Update`'s posts, the bridge's event lookup) loads the pointer once with acquire at its start and uses that index for its whole call or node. The replaced index goes to the frame-retire list and is freed in step M9 of the second `Audio.Mix` after the swap: a reader in `Audio.Update` of the current frame ran before the swap (it is ordered before `Audio.Mix`), and a main-thread reader outside the EPILOGUE subgraph finishes before the next `Audio.Mix` starts, so one `Audio.Mix` of delay suffices and the second is margin. Removed bank tables are retired the same way. `Test_AudioEventIndex_ReaderKeepsOldIndexUntilRetired` holds a pointer from before a swap across one `Audio.Mix` and asserts it stays valid (win-asan).

### Bank dependencies and residency

A bank's resident sounds are hard dependencies (they load before the bank activates, and stay while it does); streamed and dialogue sounds are soft (loaded on demand). A bank stays resident while any of these holds a handle: an emitter's `m_xBank`, a `Zenith.Audio.Bank` row, the master bank, the always-loaded list (`audio.banks.alwaysLoaded`), or AUDIO's retention cache. Posting an event whose bank is not loaded does nothing (the event is unknown): a game declares its banks in data (a bank component in the scene, an emitter's bank, or the always-loaded list), which keeps loads off the trigger path. The retention cache keeps a bank AUDIO itself requested (the active locale variant of a base bank, or the audition panel's bank) for `audio.banks.retainSeconds` after its last use; the active locale variant is held for as long as its base bank is active.

### Master bank at start

`Zenith_Module_Audio_Start` acquires the master bank (`audio.masterBank`, or the engine default `Zenith2/Engine/ZenithAudio/Internal/Content/Audio/master.bank.json` cooked into the engine content) and calls `Zenith_AssetLoader::RequestLoad` at `ASSET_PRIORITY_CRITICAL` followed by `ServiceUntilComplete(load, 5000)` on the main thread outside frames (startup step 15). A failure or timeout keeps the built-in master-only layout, logs an error and does not fail the start; `Test_AudioSystem_MasterBankFailureUsesBuiltInLayout` asserts it.

## Load modes

### Load mode rule

<!-- elaboration -->
The cook resolves `audio.loadMode = auto` by the decoded duration d of the sound at its cooked rate:

| Duration | Load mode | Codec |
|---|---|---|
| d ≤ 1.5 s | RESIDENT_PCM | PCM16 (transients: zero decode cost, exact attack) |
| 1.5 s < d ≤ 10 s | RESIDENT_PCM | IMA ADPCM (3.88:1, decoded on the device thread per 256-frame block) |
| 10 s < d ≤ `audio.streamThresholdSeconds` (30 s) | RESIDENT_COMPRESSED | the D-SEL-CODEC codec; decoded into the cache when cache eligible, else streamed from memory |
| d > 30 s, music segments, dialogue lines | STREAMED | the D-SEL-CODEC codec |

RESIDENT_COMPRESSED sounds keep their packets in memory; a voice of one plays from the decoded-sample cache when the sound is cache-eligible (decoded size ≤ 4 s of audio), and otherwise through a stream slot whose reads come from memory instead of `ReadRange` (same refill algorithm, zero I/O). Until D-SEL-CODEC is ruled, the cook emits ADPCM for every `auto` sound and STREAMED PCM16 for long ones, so Stage 3 work does not wait on the ruling.

### Decoded-sample cache

A cache of decoded int16 PCM for RESIDENT_COMPRESSED cache-eligible sounds, capped at `audio.cache.decodedBytes` (16 MiB default), allocated from AUDIO's heap per entry. A post of such a sound looks up the cache in `Audio.Mix` M7: a hit plays at once; a miss allocates the entry's storage (its decoded size is known from the header), takes a copy of the sound's asset handle for the entry, pushes CACHE_FILL to the decode thread (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"; at most `uAUDIO_MAX_CACHE_FILLS` outstanding, a miss beyond them requests no fill) and plays the voice from a stream slot reading the packets from memory until the fill completes (the voice continues on the stream; subsequent posts use the cache). The entry becomes a hit when M1 drains CACHE_FILLED; CACHE_FAILED retires it. Either answer releases the entry's handle copy, which keeps the sound's packets resident while the decode thread reads them. Eviction is least recently used among entries no voice references, through retirement. At memory WARNING the cache is purged to 50% of its cap; the cap itself stays.

## Decoders

### PCM16 decoding

Resident PCM16 is read in place by the device thread (int16 → float by × 1/32768). Streamed PCM16 is copied by the decode thread from the read chunk into the ring.

### IMA ADPCM decoding

Standard IMA ADPCM: the 89-entry step table and the 16-entry index table; per sample, diff = step/8 + (bit2 ? step : 0) + (bit1 ? step/2 : 0) + (bit0 ? step/4 : 0) (integer arithmetic on the exact IMA formula with bits 0–2 and sign bit 3), predictor = clamp(predictor ± diff, −32768, 32767), index = clamp(index + table[code], 0, 88). A block decodes from its channel headers independently. The device thread decodes resident ADPCM one block (256 frames) at a time into the voice's 2-block decode window (preallocated per device voice slot: 2 × 256 × 8 channels × 2 bytes); a seek or loop wrap decodes the containing block and skips into it. `Test_AudioAdpcm_DecodeMatchesReferenceEncoderRoundTrip` encodes generated signals with the cook encoder and asserts the decode equals the encoder's own reconstruction bit for bit and stays within 0.5% RMS of the source.

### Codec decoder interface

The D-SEL-CODEC codec is decoded only on the decode thread through `Zenith_AudioStreamDecoder` (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioStreamDecoder header"), one decoder per stream slot or cache-fill job, created from the sound's setup section: a stream slot's decoder by `Audio.Mix` M7 into the slot's stream block before the OPEN push (the request ring hands it to the decode thread, and CLOSED hands it back for retirement), a cache-fill job's decoder by the decode thread itself at the job's start, destroyed at its end. The codec library's allocations route to AUDIO's heap through its allocator hooks (or, if it has none, its state structure is sized by its query function and allocated by AUDIO). Decode errors end the voice with a fade, never assert.

### Seeking and loop points

| Codec | Seek to frame f | Loop wrap |
|---|---|---|
| PCM16 | direct offset f × channels × 2 | direct |
| IMA ADPCM | block floor(f / 256), skip f mod 256 | decode the loop-start block, skip in |
| codec | the seek table entry with the largest frame ≤ f; reset the decoder; decode, discarding pre-roll + (f − entry frame) | the loop entry (flag bit 0) at loop start makes a wrap a seek; the ring is filled across the loop point ahead of time, so the device sees a continuous ring |

Sample accuracy (ruling Q92) is exact frame alignment plus a bounded decoded-output error. PCM16 and IMA ADPCM are stateless from a seek point (a decode there depends on no earlier sample or block), so a seek or a loop wrap is bit-equal to a continuous decode at the same frames. A codec seek lands on frame f exactly through the seek table's pre-roll discard, but a stateful lossy decoder reset at the entry only converges to the continuous decode during the pre-roll, so its output is judged over the 1,024 frames from f: alignment, the lag in [−8, 8] frames that maximises the cross-correlation of the seek decode with the continuous decode from f is 0, where a tie between lags resolves to the lag of smallest absolute value, and between −k and +k to −k (so 0 wins every tie it takes part in), and a window whose continuous decode has an RMS level below −90 dBFS is aligned by definition (there is nothing to align; its error level is still computed and reported); error, with e the per-sample difference of the two decodes in int16 units over every channel, the level 20·log10(RMS(e) / 32,768) is at most −60 dBFS (an e of all zeros reports −200 dBFS). A codec loop wrap through its loop entry is judged the same way against the concatenation of the loop's end and start. Sample accuracy is asserted by `Test_AudioDecode_SeekIsSampleAccurate` (seeking to 100 positions and comparing with a full decode at the same frames: bit-equal for PCM16 and IMA ADPCM, aligned and within −60 dBFS for the codec) and `Test_AudioDecode_LoopIsSeamless` (the frames around a loop point equal the concatenation of the loop's end and start, under the same rule per decoder).

## Streaming

### Stream slots

At most `audio.streams.max` (32) slots. A slot's state and generation belong to `Audio.Mix`, which changes them in M7 (its decisions) and M1 (the decode thread's events) only. An open slot owns a stream block: a stream ring, a chunk buffer pair (two buffers, each an 8,208-byte carry area followed by a 64 KiB chunk area: 73,744 bytes), a packet scratch buffer of (the header's packet frames, at most `uAUDIO_MAX_PACKET_FRAMES` = 8,192) × channels int16 frames, a decoder (codec sounds) and the asset handle of its sound. M7 allocates the block from AUDIO's heap when it opens the slot and hands it to the decode thread in the OPEN request; M1 retires it when the decode thread answers CLOSED (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels").

<!-- elaboration -->
The ring is a `Zenith_SpscRing<sint16>` of N samples, N = the smallest power of two ≥ F × C, with F = `audio.streams.ringFrames` (a power of two in [16,384, 65,536]) and C the sound's channel count, so `Zenith_SpscRing::Init` receives a power of two for every channel count the format admits:

| Channels C | F × C (F = 16,384) | N samples | Bytes | Frame capacity ⌊N / C⌋ |
|---|---|---|---|---|
| 1 | 16,384 | 16,384 | 32 KiB | 16,384 |
| 2 | 32,768 | 32,768 | 64 KiB | 16,384 |
| 4 | 65,536 | 65,536 | 128 KiB | 16,384 |
| 6 (5.1) | 98,304 | 131,072 | 256 KiB | 21,845 |
| 8 (7.1) | 131,072 | 131,072 | 256 KiB | 16,384 |

Both sides move whole frames only: the decode thread pushes k frames as one `PushSpan` of k × C samples and only when `GetWritableCount() ≥ k × C + (N mod C)`, so at most ⌊N / C⌋ frames are ever held (the ring's writable space in frames is (`GetWritableCount()` − (N mod C)) / C, the quantity step 2 of § "Stream refill algorithm" compares with P), and the device pops j frames as one `PopSpan` of j × C samples. The read and write indices are free-running `uint32`; both advance by multiples of C, and their difference (the readable count, below N ≤ 2^20) is exact modulo 2^32, so it is always a multiple of C even when 2^32 is not; storage positions are `index & (N − 1)`, so a frame may straddle the end of storage and `PushSpan`/`PopSpan` copy it in two runs. The thresholds of the table below are counted in frames of F, not of the frame capacity, so a 5.1 stream behaves exactly as a stereo one and its 5,461 extra frames are slack. With F at its maximum 65,536 and C = 8, N = 524,288 samples (1 MiB), within `Zenith_SpscRing`'s limit of 2^20.

The slot state machine. Every event is either a decision of `Audio.Mix` M7 or a decode event M1 drains (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"); the decode thread never changes a state:

| State | Event | Next | Action |
|---|---|---|---|
| IDLE | M7: open (a voice of a STREAMED or memory-streamed sound became a start candidate) | OPENING | allocate the stream block (ring, chunk buffers, packet scratch, decoder, a copy of the sound's handle); push OPEN with the start frame, the prebuffer target and the read priority (the decode thread seeks and submits the first read) |
| OPENING | OPENED | PREBUFFERING | — |
| PREBUFFERING | PREBUFFERED (the ring holds the request's prebuffer target: the effective prebuffer, 4,096 by default, or 2,048 at memory WARNING) | PLAYING | the voice becomes eligible for real selection |
| PLAYING | (decode thread only: ring below low water, F / 2 frames) | PLAYING | the decode thread submits the next read; no state changes |
| PLAYING | M1: a device underrun record for the slot | STARVED | push STARVE (the decode thread raises its reads to CRITICAL) |
| STARVED | STARVED_RECOVERED (the ring is back at the prebuffer target; the decode thread put its reads back to HIGH) | PLAYING | — |
| STARVED | M7: starved for 500 ms | PLAYING with the voice virtual | the voice goes virtual (CONTINUE behaviour), the stream keeps filling |
| PLAYING or STARVED | M7: seek (loop wrap handled inline; a SeekInstance, a virtual voice resuming) | SEEKING | push SEEK with the start frame and the prebuffer target: the decode thread cancels the outstanding read, flushes the ring at a block boundary (the voice fades 5 ms) and seeks |
| SEEKING | PREBUFFERED | PLAYING | 10 ms fade-in |
| OPENING, PREBUFFERING, PLAYING, STARVED or SEEKING | READ_FAILED or DECODE_FAILED | CLOSING | the voice ends with a 5 ms fade (`AUDIO_ERROR_IO`, `AUDIO_ERROR_DECODE`), one `audio.stream` warning; record E and push CLOSE |
| any but IDLE and CLOSING | M7: close (voice ended, instance stopped, virtual for 500 ms, owner world detached) | CLOSING | record E (the epoch of this frame's snapshot) and push CLOSE (the decode thread requests the read's cancellation) |
| CLOSING | CLOSED (the read reached a terminal status, or none was outstanding) | IDLE | append the stream block to the retire list with E, release the handle, increment the slot's generation |

A seek decided while the slot is OPENING or PREBUFFERING, or while an earlier SEEK is outstanding, is held in the slot and pushed when the PREBUFFERED that answers the outstanding request is drained (rule 1 of `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels"). Opening when every slot is busy closes the slot of the lowest-ranked streamed voice that is virtual (CLOSING); the new voice stays virtual until CLOSED returns that slot to IDLE and the next M7 that finds the slot IDLE opens it. If no streamed voice is virtual, the new voice stays virtual (counted under the proposed streamed-voice row, BR-DEFER).

### Stream refill algorithm

<!-- elaboration -->
A 64 KiB chunk holds about 5.5 s of 96 kbit/s stereo codec audio (≈ 264,000 frames) or 0.34 s of stereo PCM16 at 48 kHz, while the ring holds 16,384 frames (341 ms), so a chunk is never decoded whole: it is decoded incrementally, one packet at a time, as the ring drains. The two chunk buffers have fixed roles that swap: the CURRENT buffer is owned by the decode thread and holds the chunk being decoded with a byte cursor; the NEXT buffer is owned by an outstanding `ReadRange` operation (its destination) from submission until the operation reports a terminal status, then by the decode thread as the filled next chunk. On the decode thread (under the offline backend: on the owner thread inside `ServiceBeforeFrame`, `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing"), per slot whose OPEN it has popped and whose CLOSED it has not yet pushed (its private refill state, `Zenith2/Docs/Design/AUDIO/Device.md` → "Decode thread loop"), each wake:

1. Read completion: if a read is outstanding and the read source's `GetStatus` is SUCCEEDED, the NEXT buffer is filled; FAILED, or CANCELLED by anything other than the slot's own CLOSE or SEEK: push READ_FAILED (`Audio.Mix` ends the voice with `AUDIO_ERROR_IO` and logs once) and stop refilling the slot; a terminal read is then released (`Release`).
2. Decode while room: while the ring's writable space ≥ P (the header's packet frames for a codec sound, 256 for PCM16) and the CURRENT buffer holds a complete unit at its cursor (a codec packet: its `u16` length prefix and that many bytes; PCM16: 256 frames, or the remaining whole frames at the end of the sound): decode that unit into the packet scratch buffer (at most P frames by the header's rule, checked: a packet that decodes to more ends the voice with `AUDIO_ERROR_DECODE`), drop codec pre-roll and loop pre-roll as the seek entry says, `PushSpan` the frames into the ring (they always fit, because writable ≥ P was checked), advance the cursor. Nothing decoded is ever held outside the ring, so no frame is lost or duplicated and no overflow buffer exists.
3. Chunk switch: when the CURRENT buffer has no complete unit left and the NEXT buffer is filled, copy the incomplete tail into the NEXT buffer's carry area immediately before its chunk bytes (the tail is at most 8,193 bytes for a codec — a 2-byte length prefix plus at most 8,191 of its at most 8,192 packet bytes — and at most 15 bytes for PCM16, a partial frame of 8 channels × 2 bytes; the carry area is 8,208 bytes = 8 KiB + 16, so it holds the longest tail with 15 bytes to spare, and the copy lands at offset 8,208 − tail so the carried bytes end exactly where the chunk bytes begin), swap the roles, set the cursor to the start of the carried bytes, and continue step 2. When the NEXT buffer is not yet filled, wait for the next wake (the device plays from the ring meanwhile).
4. Read ahead: when no read is outstanding and the NEXT buffer is free (just swapped in step 3, or never used), submit `Submit(handle, offset, 64 KiB or the remaining bytes, the NEXT buffer's chunk area, priority)` on the read source at once (production: `Zenith_AssetReader::ReadRange`), with the slot's read priority: HIGH, or CRITICAL after a STARVE until recovery and for an OPEN carrying CRITICAL (a music transition due within 500 ms); a submission error QUEUE_FULL retries next wake, any other submission error is a READ_FAILED. One chunk is therefore always read ahead of the one being decoded (5.5 s of codec audio, 0.34 s of PCM16).
5. Loop handling: when the bytes of the loop end have been decoded and the voice loops, the decoder is reset, the next read starts at the loop entry's byte offset (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Seek table section") and its pre-roll is discarded in step 2, so the ring holds continuous frames; the read ahead of step 4 is cancelled and reissued at that offset when it was submitted past the loop end.
6. Close: on the CLOSE request, the read source's `RequestCancel` for the outstanding read; keep both buffers until the read reports a terminal status ([Z2-N-CONTRACT-091]); then drop the private state and push CLOSED. The decode thread frees, retires and releases nothing: `Audio.Mix` retires the stream block when it drains CLOSED.

With reads at HIGH priority and one chunk read ahead, PCM16 streams have 0.34 s of read lead plus the ring's 341 ms, which covers ASSET's in-flight latency on the S24 (the proposed underrun row measures that it does). `Test_AudioStream_ChunkLargerThanRingDecodesIncrementally` streams a codec sound whose single chunk decodes to more than 16 times the ring and asserts the output equals a full decode frame for frame, the ring never holds more than its capacity, and exactly one read is outstanding at any time.

### Stream priorities and starvation

Reads use `ASSET_PRIORITY_HIGH`, raised to `ASSET_PRIORITY_CRITICAL` for a starved stream and for a music transition's prebuffer; memory streams (RESIDENT_COMPRESSED) do no I/O. The device never waits: an empty ring yields silence for the missing frames and one underrun record per underrun episode (`audio.streams.underruns`). A STREAMED voice is not eligible for real selection until its prebuffer is full, so a new voice starts late rather than glitching (`audio.sync.late_delta` for scheduled starts).

### Stream start latency

From a post to sound for a streamed voice: one `Audio.Update` (≤ one frame), the read (ASSET's HIGH-priority latency), the decode of 4,096 frames, the next snapshot and the next block. `PrepareDialogue` and the music transition's lead time (step 4 of `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Transition scheduling") hide it where timing matters. `Test_AudioStream_StartAfterPrebuffer` asserts that a streamed voice's first frame equals the sound's start frame (no truncated attack) with the scripted read source delaying every read by 50 ms (`Zenith2/Docs/Design/AUDIO/Tests.md` → "Scripted read source").

### Virtual streamed voices

A streamed voice virtual for 500 ms closes its slot; its playhead continues virtually; when selected real again it reopens at the playhead (a sample-accurate seek) and starts after its prebuffer with a 10 ms fade-in. Music segments and dialogue lines are exempt from the 500 ms close (their virtual behaviour is CONTINUE with the stream kept), because their resumption must be immediate.

## Coordination with world streaming

### Region-bound emitters

In a streamed world (STREAM's design, pending), audio content of a cell lives in cells of the audio grid role (`STREAMING_GRID_ROLE_AUDIO`): ambience emitters, zones, portals, occluders and the cell's `Zenith.Audio.Bank` rows, whose banks are hard dependencies of the cell scene and load with it like any other component reference. Entities carrying STREAM's region flag `Zenith.Streaming.RegionBound` are read by stable name, property 2 `InActiveRegion` (a bool written by `Streaming.Update` in phase 1; assumption pending STREAM in the entry, row ST10): an emitter whose flag is false keeps its instance but every voice of it is held virtual (not eligible for real selection, its playhead advancing), so it resumes in place when the region becomes active again; zones, portals and occluders whose flag is false are ignored. Entities without the component are always active. AUDIO calls no STREAM API ([Z2-N-ARCH-850]).

### Cells unloading under playing voices

When a cell unloads, its entities disappear at a sync point; emitters vanish and their instances stop with their stop fade (default 50 ms), never cut. Banks referenced only by that cell are released by ECS's handle release; voices still fading hold their sounds' handles until their end feedback, so no sample memory is freed under a playing voice (retirement). Script-posted instances following an entity that vanished keep playing at its last position until they end or are stopped (one-shots finish naturally; loops stop with a 200 ms fade after 1 s without their entity, counted).

### Origin shifts

When the world origin shifts (the world resource `Zenith.WorldOrigin`, `Zenith_WorldOrigin` in `ZenithScene/Zenith_WorldOrigin.h`, which `Audio.Update` declares a read of when the world has it), every world position changes by the same offset in one tick: `Audio.Update` compares the resource's `m_uRebaseCount` with the value it saw in the previous frame and, when it changed, resets every velocity estimate and the listener's for that frame, so no Doppler artefact appears; positions are otherwise used as given, and the listener frame follows the origin with the world.

## Memory accounting

### Memory consumer

`Zenith_AudioMemoryConsumer` (`Internal/Memory/`) implements `Zenith_MemoryConsumer` with `GetSubsystem() = MEMORY_SUBSYSTEM_AUDIO` and `GetResidentBytes()` = sample memory of resident sounds + bank records + the decoded-sample cache + stream rings and read buffers + the mixer snapshots, buffers and effect state + the retire list's pending bytes (relaxed atomic sums maintained by the control side). It is registered through the start context (`RegisterMemoryConsumer`). The sum is reported as `audio.memory.bytes` and measured by MP-SUBMEM for Z2-B-AUDIO-001.

### Shedding

`OnPressureLevelChanged` (main thread, frame prologue) records the new level in an atomic the next `Audio.Mix` applies (step M10) (the shedding work runs on the control side, never on the main thread beyond the store):

| Level | Action (Architecture's Audio row) |
|---|---|
| NORMAL | prebuffers back to `audio.streams.prebufferFrames`; banks AUDIO released at CRITICAL are not re-requested (they load again when something references them: a component, the always-loaded list at the next start, a locale switch) |
| WARNING | stream prebuffers to their minimum (2,048 frames); the decoded-sample cache purged to 50% of its cap (least recently used, unreferenced entries first) |
| CRITICAL | stop every virtual voice (instances whose voices are all virtual finish; others lose their virtual voices); release AUDIO's own handles on banks with no playing voice: the retention cache and the always-loaded banks other than the master bank (banks held by components stay, as ECS holds them); purge the cache to 0 unreferenced entries |

<!-- elaboration -->
The CRITICAL row is AUDIO's interpretation of Architecture's Audio cell "unload banks with no playing voice": AUDIO can release only the handles it holds, because a bank held by an entity's component is owned by that entity and is released when the entity is; the cell says so since cross-doc item X234 (b) (`Zenith2/Docs/Architecture.md` → "Shedding per subsystem": AUDIO-held bank references with no playing voice are released, component-held banks stay).

Shedding changes presentation only ([Z2-N-ARCH-812]): no command is refused because of the level, and a post of an event whose bank was released at CRITICAL is simply unknown until the bank is referenced again. `Test_AudioMemory_SheddingAtEachLevel` forces each level through the governor's forced-level API and asserts each action and the resident-byte drop.
