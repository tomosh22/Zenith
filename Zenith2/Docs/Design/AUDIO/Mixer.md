# AUDIO design: mixer, voices and spatialisation

## Scope of this file

This file is part of the AUDIO design family (entry: `Zenith2/Docs/Design/AUDIO.md`). It specifies the mixer graph, voice management (instances, audibility, priorities, the real-voice limit, stealing, virtual voices), event node evaluation, the DSP chain with every filter and effect, spatialisation with its formulas, occlusion, acoustic zones and reverb, music transitions, dialogue and localized audio, the audio clock and its synchronisation with animation and simulation, and the determinism rules of the mixer. Per-world control-side algorithms run in `Audio.Update@W<tag>` (`Zenith2/Docs/Design/AUDIO.md` → "Audio.Update step by step"), process-wide ones (real-voice selection, mix snapshots, ducking, the snapshot build) in `Audio.Mix` (`Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"); device-side ones in the mixer callback (`Zenith2/Docs/Design/AUDIO.md` → "Mixer callback step by step").

## Mixer graph

### Bus hierarchy

The mixer layout is a tree of at most 64 buses rooted at `Master`, depth ≤ 8 below it, loaded from the master bank's embedded mixer section (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Embedded mixer section"). The control side flattens it into the immutable real-time layout `Zenith_AudioRtLayout`: buses in a topological order where every child precedes its parent (post-order), each with its parent index, insert effects, aux kind and preset. A voice outputs to exactly one bus; a send feeds an aux-return bus. A layout change (the editor's live edit, `Zenith_AudioSystem::ApplyMixerLayout`) builds a new layout object, publishes it in the next snapshot and retires the old one by epoch; the device crossfades bus gains over one block when buses are added or removed and resets the state of effects whose kind changed.

### Built-in buses

The built-in master-only layout, used when no master bank loads, has `Master` (limiter at −1 dB), `Master/Audition`, and one bus per volume category directly under `Master` (`Music`, `SFX`, `Dialogue`, `Ambience`, `UI`), with no effects. The engine default mixer document extends it with `Master/Reverb/Zone` (the zone reverb return) and the attenuation and reverb presets of `Zenith2/Docs/Design/AUDIO/Formats.md` → "Reverb presets".

### Signal flow per block

For each 256-frame block:

1. Clear every bus buffer and send buffer (planar float, `channels × 256` each, preallocated: 64 buses × 8 channels × 256 × 4 bytes = 512 KiB plus 4 aux sends).
2. Render every real voice into its bus buffer and, scaled by its send gains, into the send buffers of its two aux buses (§ "Per-voice chain").
3. Walk buses in the layout's post-order: apply the bus's insert effects in order, its low-pass and high-pass (when set by a mix snapshot), its gain ramp (the bus record's target, ramped linearly across the block from the previous block's value), its duck gain; add the result into its parent's buffer. An aux-return bus first processes its send buffer through its aux processor (reverb or delay) and adds the output into its own buffer before its inserts run.
4. `Master` applies its inserts (the limiter last), the final clamp and the output conversion (§ "Limiter and final clamp", § "Output layouts and downmix").

Cost: O(real voices × 256 × output channels) for step 2 plus O(buses × channels × 256) for step 3.

### Aux sends and returns

A voice has at most two sends (event fields send 0 and send 1 with their aux bus IDs), plus the zone reverb send, which replaces send 1 when the event enables zone reverb and the listener is in a zone with a preset (§ "Reverb crossfade"). Sends are post-fader (after the voice gain, before the pan): a send buffer is mono-summed per voice and panned by the voice's pan matrix at half width, so reverb tails keep the source's direction loosely. At most 4 aux returns exist (`uAUDIO_MAX_AUX_REVERBS`); their processors are preallocated at layout build.

### Mix snapshots

A mix snapshot is a set of bus overrides (volume offset dB, low-pass Hz, high-pass Hz) with a priority and fade times. Up to 8 are active at once. Each active snapshot has a weight w ∈ [0, 1] ramping linearly to 1 over its fade-in and to 0 over its fade-out (then it deactivates). Per bus the control side combines: volume offset = Σ w_i · offset_i over active snapshots (dB, additive); low-pass = min over active snapshots of lerp(20,000, cutoff_i, w_i) in log-frequency space (`exp2(lerp(log2 20000, log2 c_i, w_i))`); high-pass = max over snapshots of the same form from 10 Hz. Priority orders ties only for display (the combination is commutative). Snapshots never touch the `Audition` bus.

### Ducking

A ducking rule ducks bus D while sidechain bus S is active. The device measures S's level each block as the RMS over the last two blocks (512 frames, 10.67 ms: the sum of squared samples of the mono sum of the current block plus the stored sum of the previous block, divided by 512); when the level exceeds the threshold, the target reduction is the rule's reduction (dB) and the hold timer restarts; after the hold expires the target returns to 0 dB. The applied duck gain follows the target with the rule's attack (going down) and release (going up) as linear-in-dB ramps per block. Several rules on one bus multiply (their dB values add), floored at −60 dB. Dialogue lines with `ducks` add a transient rule targeting `Music` and `SFX` with the line's duck amount for the line's duration (§ "Dialogue ducking and captions").

### User volume categories

Six categories (`Zenith_AudioVolumeCategory`): master, music, SFX, dialogue, ambience, UI. Each maps to one bus (the mixer document's `category` member); the user volume of `audio.<category>Volume` (dB) multiplies that bus's gain, and master multiplies the output. A change ramps over 50 ms. `audio.muteWhenUnfocused` adds a −144 dB master target with a 100 ms ramp on window focus loss (Windows only; Android pauses through focus and lifecycle instead).

### Output layouts and downmix

The mixer renders internally in the output layout: stereo (2), headphones (2, with the headphone pan law of § "Panning to stereo and headphones"), 5.1 (6) or 7.1 (8). `audio.outputLayout = auto` picks the layout from the device's channel count (2 → stereo, 6 → 5.1, 8 → 7.1, any other count → stereo with zeros on the extra channels). Multichannel sources are folded to the output by static matrices (ITU-R BS.775): to stereo, L' = L + 0.7071·C + 0.7071·Ls (+ 0.7071·Lb for 7.1), R' symmetric, LFE dropped; to 5.1 from 7.1, Ls' = 0.7071·Ls + 0.7071·Lb (and R symmetric). After the downmix the master limiter keeps the sum below its ceiling.

## Voice management

### Instances voices and slots

An instance is created by a post (a command, a script trigger, a bridged event, an emitter, an audition) and owns 1 to 8 voices, one per SOUND node its event evaluation selected. Instance states:

| State | Event | Next | Action |
|---|---|---|---|
| STARTING | the event's sounds resolve (resident) or the streams prebuffer | PLAYING | voices become candidates for real selection |
| STARTING | a stop | FINISHED | release prepared streams, no sound |
| STARTING | prebuffer exceeds 2,000 ms | FINISHED | count `audio.streams.start_timeouts_delta` |
| PLAYING | all voices ended (non-looping) | FINISHED | release the instance ID |
| PLAYING | stop with fade f | STOPPING | voices fade to silence over f ms |
| PLAYING | stop IMMEDIATE | FINISHED | voices end at the next block with a 5 ms fade (click suppression floor) |
| PLAYING | stop ALLOW_TAIL | STOPPING | loops are released: each looping voice plays to its loop end and then to the sound's end |
| PLAYING | every voice virtual | VIRTUAL | — |
| VIRTUAL | any voice selected real | PLAYING | — |
| VIRTUAL | virtual behaviour KILL | FINISHED | — |
| STOPPING | fade complete | FINISHED | — |

Instances live in per-world tables of `audio.voices.instanceMax` entries; a post when the table is full is refused (`AUDIO_ERROR_CAPACITY`, counted). Voices live in per-world tables of `uAUDIO_MAX_VOICES_PER_WORLD` (2,048) entries, allocated at attach, so `Audio.Update` of one world never touches another world's tables. Each world has its own start-sequence counter (incremented per accepted post in drain order). Device voice slots (256, of which 16 are the audition world's) are process resources assigned only by `Audio.Mix`, only to real voices.

### Event node evaluation

<!-- elaboration -->
A post evaluates the event's node tree depth-first from the root, with the instance's deterministic random stream `Zenith_RandomSource(seed = S, stream = node index)` (`Zenith2/Docs/Design/CORE/Maths.md` → "Zenith_RandomSource"). S is the instance ID for an identified instance. An anonymous post (instance 0: every `PostOneShot`, every native post without an ID, every bridged post) gets S = `Zenith_Hash64` of the 20-byte record (world tag as `uint16`, 6 bytes 0, recording tick `m_ulTick` as `uint64`, then the 4-byte tick ordinal: the command's index, counted from 0 in drain order, among the drained commands of this world that carry the same `m_ulTick`), and the instance's internal ID is (S with bit 0 cleared) with bit 63 set. The tick ordinal depends only on the tick's own commands, which a replay reproduces in merge order, and never on how many ticks a frame drained (one per frame at 60 Hz ticks and 60 fps, two at 30 fps, one per rendered frame offline), so a replay at another frame cadence draws the same. The three instance-ID classes are disjoint by their two marker bits: native IDs have bit 0 clear and bit 63 clear, script IDs bit 0 set and bit 63 clear (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Identifiers", `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Function table"), internal anonymous IDs bit 0 clear and bit 63 set. Two consecutive anonymous posts therefore draw different randomisation, and a replay draws the same:

| Node kind | Evaluation |
|---|---|
| SOUND | produces one voice: the sound reference, the accumulated volume (sum of dB down the path, plus a uniform draw in [−volumeRandom, 0] dB per node that randomises), the accumulated pitch (sum of cents plus a uniform draw in [−r, +r] cents), the accumulated start delay, loop settings |
| RANDOM | picks one child by weight among those not among the last `noRepeat` picks of this event in this world (a ring of up to 16 recent picks per RANDOM node per world); if every child is excluded, the window shrinks to `child count − 1` |
| SEQUENCE step | picks child (n mod child count) where n is the per-world post counter of the node |
| SEQUENCE continuous | plays children in order as one voice chain: each child starts when the previous one's voice ends, crossfaded by the node's crossfade ms |
| SEQUENCE shuffle | a per-world permutation of the children (Fisher–Yates with the node's random stream), reshuffled at its end, never repeating the last child first |
| SWITCH | picks the child whose [min, max) contains the switch parameter's current value (instance parameter, else global, else the parameter's default); none → no voice |
| BLEND | evaluates every child; each child's voice gain is multiplied by its crossfade curve evaluated at the parameter value, re-evaluated every frame |
| LAYERED | evaluates every child |

Evaluation stops adding voices at 8 per instance (counted). Event-level curves apply to every voice of the instance; node-scoped curves to the voices under that node. Because random draws are keyed by the instance seed and node index, the same post in a replay yields the same voices (`Test_AudioEvents_RandomNodeDeterministicPerInstance`, which also asserts that two consecutive anonymous posts of one event in one tick differ in pick or randomisation and that a replay of both reproduces them).

### Audibility estimate

Each frame the control side computes, per voice, the audibility A in dB: A = event volume + node volume + curve volume + attenuation(distance) + cone gain + occlusion loss + bus gain chain (category, snapshot, duck target, user volumes) + the sound's integrated loudness offset (loudness − (−18 LUFS), so a quiet recording counts as quiet) + fade gain. Positional voices with no valid listener get −144 dB. A voice is audible when A > `audio.voices.audibilityThresholdDb` (default −60 dB).

### Priority ordering

Voices compete in this total order (strict weak, ties broken deterministically):

1. audible before inaudible;
2. higher event priority (0 … 255) first;
3. higher audibility first, quantised to 0.5 dB steps (so tiny movements do not reorder);
4. longer deferral first: the number of consecutive frames the voice has wanted a slot and been deferred (`uint32`, saturating; 0 for a voice that is real or was not deferred last frame);
5. older instance first: lower world tag, then lower per-world start sequence number;
6. lower voice index within the instance first.

Rule 4 orders deferred candidates among themselves for the slots that are free in a frame, after hysteresis and minimum real time (selection step 4) have kept the real voices: the longest-deferred candidate takes the next free slot whatever its instance's age. It never displaces a real voice that step 4 protects, because the deferral count is not one of the two reasons step 4 lets a candidate take an occupied slot (a margin of `audio.voices.stealHysteresisDb` or a higher priority). It sits between audibility and age because rule 5 totally orders different instances, so an age term after it would never act across instances and the oldest deferred instance would always take a freed slot. The deferral count is a function of the previous frames' selection results, which are functions of the frame records, so the order stays deterministic.

All game worlds' candidates are ordered together and compete for L = `audio.voices.realMax` slots (at most 240: 256 device slots less the audition world's 16); the primary world has no cap of its own within L, and each secondary world takes at most `audio.voices.secondaryWorldRealMax` of them. The audition world competes only for its own 16 reserved slots, by the same order, so an audition is never displaced by game voices and never displaces them.

### Real voice selection algorithm

Once per frame across all worlds, in `Audio.Mix` step M6 (`Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"), over the frame records the worlds' `Audio.Update` wrote:

1. Gather candidate voices: every voice of a PLAYING or STOPPING instance that is audible, whose instance is not paused, and whose sound data is available (resident, cached, or a stream slot in state PLAYING, which only a PREBUFFERED event drained in M1 sets, `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots").
2. Sort the candidates by the priority order (introsort on an index array of every world's candidates, at most 8 × 2,048, preallocated; O(V log V)); the order is total, so the result does not depend on the order the candidates were gathered in.
3. Walk the order assigning real slots, respecting in order: the per-bus real-voice limit (the bus record's `realVoiceLimit`, counted on the voice's bus and all its ancestors), a secondary world's share, and the limit L = min(`audio.voices.realMax`, 240); then the audition world's candidates for its 16 slots. Device slots are taken from the free list in ascending slot index, and a slot's generation increments on each assignment.
4. Hysteresis: a voice that is real now and would lose its slot to a candidate keeps it unless the candidate's audibility exceeds the real voice's by `audio.voices.stealHysteresisDb` (3 dB) or has a higher priority; a voice made real in the last `audio.voices.minRealMs` (100 ms) keeps its slot unless its instance stopped. A longer deferral (priority rule 4) is never a reason to take an occupied slot: it only decides which candidate takes a slot no protected real voice holds.
5. Voices that were real and are not selected start FADING_TO_VIRTUAL; voices that were virtual and are selected start FADING_TO_REAL (§ "Virtual voices").
6. Every candidate that wanted a slot and did not get one because of the global or world limit increments `audio.voices.deferred_delta` (BR-DEFER of Z2-B-AUDIO-004) and its deferral count; a voice that gets a slot, stops being a candidate or becomes inaudible resets it to 0. Through priority rule 4, when a slot becomes free (a real voice ended, stopped, became inaudible or was virtualised), the deferred candidate deferred longest takes it, whatever its instance's age; that is the round-robin of BR-DEFER (`Zenith2/Docs/Budgets.md` → "Breach consequences": "The request is deferred to a subsequent frame (round-robin)"), which orders the deferred requests served and never displaces a playing voice. With limit 8 and 20 equal audible looping voices, 8 are real and 12 deferred in every frame, and no real voice is replaced while it plays (no flapping); when one ends, the longest-deferred of the 12 takes its slot.

### Stealing and instance limits

Per event, at most `instanceLimit` instances per world (0 = unlimited). A post beyond the limit applies the event's steal mode:

| Steal mode | Victim |
|---|---|
| OLDEST | the instance with the lowest start sequence |
| QUIETEST | the instance with the lowest maximum voice audibility this frame (ties → oldest) |
| FARTHEST | the instance with the largest listener distance (2D instances count as distance 0; ties → oldest) |
| NONE | none: the new post is refused and counted |

The victim stops with a 20 ms fade (`audio.voices.stolen_delta`); the new instance starts in the same frame. A per-world cooldown refuses posts of an event within its `cooldownMs` of the previous accepted post.

### Virtual voices

Residency states per voice:

| State | Event | Next | Action |
|---|---|---|---|
| VIRTUAL | selected real | FADING_TO_REAL | assign a device slot; start at the virtual playhead with a 10 ms fade-in |
| FADING_TO_REAL | fade complete (device feedback) | REAL | — |
| REAL | not selected | FADING_TO_VIRTUAL | 10 ms fade-out; the slot is freed when the device reports the voice ended (the feedback record, or the slot-state table when the record was dropped) |
| FADING_TO_VIRTUAL | voice-ended feedback | VIRTUAL | the playhead is the device's reported end frame |
| any | instance finished | released | — |

A virtual voice advances its playhead by the elapsed presentation time × pitch ratio (wrapping at loop points), so it resumes in sync when made real again. The event's virtual behaviour: CONTINUE (as above), PAUSE (the playhead does not advance while virtual), RESTART (made real at frame 0), KILL (the instance finishes when every voice is virtual for 500 ms). A STREAMED voice that stays virtual for 500 ms releases its stream slot; when it becomes real again the stream reopens at the playhead with a seek (sample-accurate for codecs through the seek table) and the voice fades in when the prebuffer is full. Virtual voices cost O(1) control-side work and no device work.

### Fades and click suppression

Every start, stop, real/virtual change and seek is faded: start and resume 10 ms (480 frames) unless the sound starts at frame 0 with the event's `noFadeIn` (one-shot transients start unfaded), stop at least 5 ms, steal 20 ms. Gain, filter and pan targets ramp linearly across each block from the previous block's value; a pan change larger than 90° in one frame ramps over 4 blocks. Fades are linear in amplitude for ≤ 20 ms and equal-power (sin/cos) for longer crossfades (music, sequences).

### Voice selection worked example

Limit L = 4, hysteresis 3 dB, no voice deferred in the previous frame (every deferral count 0). Candidates this frame (priority, audibility, real now?): A (200, −10, yes), B (128, −20, yes), C (128, −24, yes), D (128, −30, yes), E (128, −22, no), F (64, −5, no). Order: A, B, E, C, D, F. The first four are A, B, E, C; D would lose to E, but E (−22) is louder than D (−30) by 8 dB ≥ 3, so D fades to virtual and E fades to real; C keeps its slot (it is in the top four). F has lower priority than all four, so it stays virtual and counts as deferred despite being the loudest: priority dominates audibility by design (authors raise a sound's priority to protect it). `Test_AudioVoices_WorkedExampleSelection` asserts exactly this outcome.

## DSP chain

### Per-voice chain

Device side, per real voice per block: (1) fetch source frames for the block from the source position through the resampler's reach (block × ratio + 3 frames); (2) Hermite-resample to 256 frames at the ramped pitch ratio; (3) the voice high-pass biquad then low-pass biquad, each skipped when its cutoff is at the off value (10 Hz, 20,000 Hz); (4) multiply by the gain ramp; (5) apply the pan matrix ramp into the bus buffer; (6) accumulate sends. Silence detection: a voice whose gain ramp starts and ends below −144 dB skips steps 2–6 but still advances its source position.

### Resampler

Cubic Hermite (Catmull-Rom) interpolation on a 32.32 fixed-point source position (`uint64`: integer frame in the high 32 bits, fraction in the low 32). For output frame n, with position p = p0 + n·step (step = ratio × 2^32, ramped linearly across the block from the previous step to the new one), integer i, fraction t = (p mod 2^32) / 2^32, samples y−1, y0, y1, y2:

y = y0 + 0.5·t·(y1 − y−1 + t·(2·y−1 − 5·y0 + 4·y1 − y2 + t·(3·(y0 − y1) + y2 − y−1)))

Loop wrapping reads across the loop point (samples beyond loop end come from loop start). The resampler's 2 frames of history live in the voice state. Pitch ratios are clamped to [1/16, 16]. Aliasing above Nyquist at ratios > 1 is accepted (game assets are cooked at 48 kHz; pitch-up beyond an octave is rare); `Test_AudioResampler_HermiteMatchesReference` checks the output against the double-precision reference within 10^-6 for a 1 kHz sine at ratios 0.5, 1, 1.5 and 2.

### Biquad filters

RBJ cookbook biquads (transposed direct form II), coefficients recomputed per block when the cutoff target changes by more than 0.5% and otherwise kept; coefficients ramp by recomputing from the block's start and end cutoffs and interpolating the five coefficients linearly across the block in 4 sub-steps of 64 frames (stable for the range used: cutoff changes are smoothed to ≤ 1 octave per block by the control side). Low-pass and high-pass use Q = 0.7071; the per-coefficient formulas with ω = 2π·fc/48000, α = sin ω / (2Q) are the RBJ definitions, computed with `Zenith_AudioSin` and `Zenith_AudioCos`. State is flushed to zero when |state| < 10^-20 (denormal guard, beside FTZ/DAZ).

### Gain and pan ramps

Gain: g(n) = g_prev + (g_target − g_prev)·(n+1)/256 for n in 0 … 255. Pan matrix: the same per element. Targets are written by the snapshot; when no new snapshot arrives the ramp target equals the current value (no ramp).

### Bus effects

| Kind | Parameters (in order, the 12-float table) | Algorithm |
|---|---|---|
| EQ | band 0..3: frequency Hz, gain dB, Q (12 floats) | § "Equaliser" |
| COMPRESSOR | threshold dB, ratio, attack ms, release ms, knee dB, makeup dB, sidechain bus index (−1 self) | § "Compressor" |
| LIMITER | ceiling dB, release ms | § "Limiter and final clamp" |
| LOW_PASS | cutoff Hz, Q | biquad |
| HIGH_PASS | cutoff Hz, Q | biquad |
| DELAY | time ms (1 … 1000), feedback (0 … 0.95), wet dB, damping Hz | § "Delay effect" |

Effects are applied in place on the bus buffer; bypass ramps the wet/dry over 10 ms.

### Equaliser

Four bands: band 0 low shelf, bands 1–2 peaking, band 3 high shelf (RBJ formulas; shelves with S = 1). A band with gain 0 dB is skipped. Frequencies 20 … 20,000 Hz, gain −24 … +24 dB, Q 0.1 … 10.

### Compressor

Feed-forward, stereo-linked (the level is the maximum absolute sample across channels per frame). Level detection in dB with a branching attack/release one-pole smoother on the gain computer output: coefficient a = exp(−1 / (τ·48000)) computed per block from attack and release with `Zenith_AudioExp2`. Gain computer with soft knee W: for x < T − W/2, y = x; for |x − T| ≤ W/2, y = x + (1/R − 1)(x − T + W/2)² / (2W); above, y = T + (x − T)/R. Gain reduction g = y − x dB, smoothed, converted to linear with `Zenith_AudioDbToGain`, multiplied with makeup. Evaluated per 16-frame sub-block (the gain is linearly interpolated within it), which keeps the cost at 16 dB conversions per block per bus.

### Limiter and final clamp

The master limiter is a zero-lookahead peak limiter (lookahead would add latency): per frame the peak p = max |x_c|; if p·g > ceiling, g drops instantly to ceiling/p; otherwise g recovers toward 1 with the release one-pole (default 80 ms). After it, every sample is clamped to [−1, 1] (the final clamp, which only acts on inter-frame overshoot and NaN: a NaN or infinity in the final buffer becomes 0, and `audio.mixer.nan_frames` counts it). The limiter reduction is reported in the meters.

### Delay effect

A stereo delay line per aux bus (preallocated for 1,000 ms), read with linear interpolation at the ramped delay time, feedback through a one-pole low-pass at the damping cutoff, wet level applied on output.

### Reverb

<!-- elaboration -->
An 8-line feedback delay network per reverb aux return (`audio.reverb.quality = high`; `low` uses the first 4 lines and a 4 × 4 Hadamard):

- Base delays in frames at 48 kHz: 1153, 1327, 1559, 1801, 2063, 2287, 2539, 2803 (mutually prime), each multiplied by the preset's size s ∈ [0.5, 1.5] and rounded to an integer; delay memory preallocated for s = 1.5 (8 × 4,205 floats).
- Per line gain from RT60: g_i = 10^(−3·d_i / (RT60 · 48000)), computed with `Zenith_AudioDbToGain(−60·d_i / (RT60·48000))`.
- Mixing: the 8 × 8 normalised Hadamard matrix (entries ±1/√8) applied to the line outputs each frame (a fast Walsh–Hadamard transform: 24 additions).
- Damping: a one-pole low-pass per line at the preset's damping cutoff, inside the feedback loop.
- Input: the mono sum of the send buffer through the pre-delay (≤ 200 ms, preallocated), a low cut (one-pole high-pass at the preset's low cut), then distributed to the lines with alternating signs.
- Diffusion: two series Schroeder all-passes (delays 142 and 379 frames, gain = 0.6 × diffusion) before the network.
- Early reflections: 6 taps of the pre-delay line at 7, 11, 17, 23, 31, 41 ms × size with gains 0.8^k, summed at the early level.
- Output: lines 0, 2, 4, 6 summed to left and 1, 3, 5, 7 to right (scaled by 1/2), for surround layouts lines alternate across L/R/Ls/Rs (and Lb/Rb), at the preset's wet level times the snapshot's wet target.

A preset change crossfades two network states over 500 ms only when the size changes (delay lengths change); otherwise the gains ramp over one block. The cost is fixed per return (about 40 operations per frame per line); `Benchmarks/AudioMixer` reports it.

## Spatialisation

### Listener space

For a voice at world position e, the listener at position l with rotation q and attenuation position a:

- relative vector v = q⁻¹ · (e − l) (listener space: +X right, +Y up, −Z forward, `Zenith2/Docs/Design/CORE/Maths.md` → "Coordinate systems");
- distance for attenuation r = |e − a|; for direction, d = v / |v| (|v| < 10^-4 m → straight ahead, d = (0, 0, −1));
- azimuth θ = atan2(d.x, −d.z) (0 ahead, +90° right), elevation φ = asin(d.y) clamped (used only for the elevation fold below).

<!-- elaboration -->
Positions and rotations are the presented ones: `Audio.Update` interpolates every entity it reads between `Zenith.PreviousWorldTransform` (the end of tick K − 1) and `Zenith.WorldTransform` (the end of tick K) with the frame's interpolation alpha α (`Zenith_Lerp` for positions, `Zenith_Slerp` for rotations), the interpolation extraction applies ([Z2-N-ARCH-415]), so a sound sits where its mesh is drawn at any display rate. Velocities are not frame differences: v = (p_K − p_{K−1}) / Δ with Δ the tick duration, the same for every frame between two ticks. At 60 Hz ticks and a 144 Hz display a per-frame difference would alternate between 0 and two to three times the true speed (ripple at the tick rate that the 50 ms smoother cannot remove, audible as Doppler flutter); the tick difference is exactly the motion the simulation produced. A teleport flag on the entity sets its velocity to 0 for the frame. The listener orientation is the listener entity's interpolated world rotation.

### Distance attenuation curves

With r0 the minimum distance, r1 the maximum, ρ the rolloff, r clamped to [r0, r1] (beyond r1 the voice is inaudible and virtual):

| Curve | Gain |
|---|---|
| NONE | 0 dB |
| INVERSE | g = r0 / (r0 + ρ·(r − r0)) (the inverse distance clamped model) |
| LINEAR | g = 1 − ρ·(r − r0)/(r1 − r0), floored at 0 |
| LOGARITHMIC | dB = −6·ρ·log2(r / r0) (6 dB per doubling at ρ = 1) |
| CUSTOM | dB by piecewise-linear interpolation over the preset's points at u = (r − r0)/(r1 − r0) |

For INVERSE and LOGARITHMIC a fade to −144 dB is applied over the last 10% of [r0, r1] so a voice reaching r1 vanishes without a step.

### Air absorption

With the preset's absorption distance D > 0, the voice low-pass cutoff is fc = max(1000, 20000 · 2^(−r / D)) Hz, combined with the event's and occlusion's cutoffs by taking the minimum.

### Cones

With the emitter forward f (its +Z in world space, model front +Z) and the direction from emitter to listener u = (l − e)/|l − e|, the angle β = acos(f·u) in degrees. With inner angle I and outer angle O (full angles) and outer gain G dB: β ≤ I/2 → 0 dB; β ≥ O/2 → G dB; between, linear in dB. Cone attenuation also lowers the cutoff toward 4 kHz in proportion (outer → 4 kHz) when the event's flags 2 bit 3 (cone filtering, `coneFilter`) is set; occlusion keeps its own bit (flags bit 2).

### Doppler

With speed of sound c = 343 m/s, unit vector n from listener to emitter, listener velocity v_L, emitter velocity v_E (the tick-to-tick velocities of § "Listener space", clamped to 100 m/s each):

ratio = (c + v_L·n) / (c + v_E·n)

raised to the event's Doppler factor k (ratio^k via `Zenith_AudioExp2(k·Zenith_AudioLog2(ratio))`), clamped to [0.5, 2], smoothed with a 50 ms one-pole on the control side, and multiplied into the pitch ratio. A teleport flag on either entity resets the smoother to 1 for that frame. Doppler is off for 2D events and when the event's Doppler flag is clear. Because v_L and v_E change only when a tick runs, the ratio is constant across the frames between ticks; `Test_AudioSpatial_DopplerSteadyAt30HzTicks144HzFrames` moves an emitter at 20 m/s past a listener with 30 Hz ticks and 144 Hz frames and asserts the per-frame Doppler ratio stays within 0.1% of the analytic value while the geometry is steady.

### Panning to stereo and headphones

Stereo uses constant-power panning on the azimuth folded to the front: θf = θ for |θ| ≤ 90°, else sign(θ)·(180° − |θ|); pan p = θf / 90° ∈ [−1, 1]; gains L = cos((p + 1)·π/4), R = sin((p + 1)·π/4). Sources behind the listener (|θ| > 90°) are attenuated by up to 3 dB (linear in (|θ| − 90°)/90°) and low-passed toward 8 kHz, so front and rear stay distinguishable. Headphones use the same law with a narrower rear attenuation (1.5 dB) and a 0.3 ms interaural delay toward the far ear (14 frames at maximum, a fractional delay line of 32 frames per voice), which is the whole of v1's headphone virtualisation (HRTF is excluded). Elevation reduces the pan width by cos φ.

### Panning to 5.1 and 7.1

Pairwise constant-power panning (2D VBAP) over the speaker ring, ignoring LFE: 5.1 speakers at azimuths C 0°, L −30°, R +30°, Ls −110°, Rs +110°; 7.1 at C 0°, L −30°, R +30°, Ls −90°, Rs +90°, Lb −150°, Rb +150°. For azimuth θ, find the adjacent pair (a, b) bracketing it, t = (θ − θa)/(θb − θa), gains cos(t·π/2) and sin(t·π/2). The centre speaker gets at most 0.7 of the front pair's power for non-dialogue voices (dialogue events may route to C at full weight through the event's flags 2 bit 2, `centreFull`, set on their template). LFE gets the event's LFE send (default −144 dB, off).

### Spread and source size

Spread σ (degrees) widens a voice from a point: the voice is rendered as K = 3 virtual points at θ − σ/2, θ, θ + σ/2 with weights 0.5, 1, 0.5 normalised to unit power; σ = 360° gives uniform power to every speaker. The event's spread applies at minimum distance and interpolates to the preset's "spread at maximum distance" at r1. A stereo source is two points at θ ± (15° + σ/2) carrying its L and R channels respectively.

### Multichannel sources

Sources with 4, 6 or 8 channels are beds: never positional; folded to the output by the static matrices of § "Output layouts and downmix", with the bus gain chain applied. A positional event playing a multichannel sound downmixes it to mono first (equal-power sum, LFE dropped) and pans the mono point.

### Secondary worlds and listeners

Each audio world has at most one listener (the highest-priority active one), and its voices are spatialised against that listener only. Every updated world's real voices are mixed into the one output. The primary world is chosen by `Audio.Mix` each frame (the updated world with the highest `SessionOrdinal` of `Zenith.Runtime.WorldState`, ties to the lowest world tag; in a game run that is the world host's current world, and through a world transition the primary changes in the swap frame exactly, `Zenith2/Docs/Design/AUDIO.md` → "Following the presenting world"); it is limited only by L, while each secondary world (for example an outgoing world still fading, or a preview world) takes at most `audio.voices.secondaryWorldRealMax` real voices (§ "Priority ordering") and its music commands are ignored. Merging several listeners into one view (split-screen) is out of scope (Z2-D-076: one local player). Worlds that run without a listener (a server-style world, an AI test world) have their positional voices treated as inaudible and virtual; their 2D voices still play.

## Occlusion

### Occluder geometry

Occluders are oriented boxes from `Zenith.Audio.Occluder` components. Each frame the control side rebuilds a uniform hash grid of 8 m cells over the occluders (each box inserted into every cell its world AABB overlaps; at most 4,096 occluders, cells in an open-addressing table of 8,192 entries keyed by the cell's integer coordinates).

### Occlusion ray test

For each positional voice with occlusion enabled and audible before occlusion, a segment from the listener's attenuation position to the emitter is tested at most every 4 frames (round-robin over voices, at most `audio.occlusion.raysPerFrame` segments per frame; voices not tested keep their last result, which is the BR-DEFER behaviour of the proposed occlusion row). The segment walks the grid by 3D DDA (Amanatides–Woo); in each cell it tests each occluder (deduplicated by a per-test stamp) by the slab method in the box's local space. The total loss is the sum of transmission losses of the boxes crossed (dB, floored at −48 dB); the cutoff is the minimum of their cutoffs. Cost: O(cells crossed + boxes in those cells) per segment.

### Occlusion filter mapping

Targets (loss dB, cutoff Hz) are smoothed with a 150 ms one-pole on the control side and applied as voice gain and the voice low-pass. An unoccluded voice targets 0 dB and 20,000 Hz. Occlusion never changes audibility ranking abruptly: the 0.5 dB quantisation and the hysteresis of the selection apply.

## Acoustic zones

### Zone selection

Zones are oriented boxes (`Zenith.Audio.Zone`). For a point p, the containing zone is the zone with the highest priority whose box contains p (ties → smallest volume, then lowest simulation ID); p in no zone is in the default (outdoor) zone. The listener's zone is evaluated each frame at its attenuation position; each positional voice's zone once per frame at its emitter (O(zones), at most 256; a 16 m grid accelerates it when more than 32 zones exist). The blend band: inside a zone within `fadeDistance` of its boundary, the listener is between that zone (weight w = depth / fadeDistance) and the zone it would otherwise be in (1 − w).

### Portals and propagation

When a voice's zone Ze differs from the listener's zone Zl:

1. if both are outdoor zones (or default), no extra loss;
2. else if a portal connects Ze and Zl directly, with openness o > 0: the voice's apparent position becomes the portal's nearest point to the emitter projected on the portal rectangle (direction from the listener to that point; distance = |e − portal| + |portal − l|), with a loss of 20·log10(max(o, 10^-3)) dB, and a low-pass at lerp(Zl's wall cutoff, 20,000, o);
3. else (no portal, or a closed one): the loss is Zl's wall loss (default −36 dB) and the cutoff its wall cutoff (default 1 kHz), at the true position.

Only one portal hop is evaluated (zones two portals away use rule 3). The results combine with occlusion (losses add, cutoffs take the minimum) after the same 150 ms smoothing.

### Reverb crossfade

The zone reverb return uses the listener zone's preset: on a zone change the return crossfades between two FDN instances (the old and the new preset) over 500 ms, equal power. Voices send to the return with the event's zone reverb send at the zone's `reverbWetDb`; a voice in another zone sends with its own zone's preset weighting only when a portal path exists (rule 2), at half level. Zone ambience events start (2D, looping, on `Ambience`) when the listener enters the zone and fade out over 1 s when it leaves; within the blend band both zones' ambiences play at w and 1 − w.

## Music

### Music model

A bank's music section defines segments (a sound with tempo, beats per bar, grid, entry and exit cues), states (a playlist of segments) and transitions (from state, to state, rule, fades, an optional transition segment and stinger). Music plays in one music player per process (the primary world's), on the `Music` bus; it is driven by `Audio.MusicSetState` and `Audio.MusicStinger`. Music segments are STREAMED (two streams: current and next) so a transition never waits on a resident load.

### Transition scheduling

<!-- elaboration -->
States: STABLE (one segment playing), PENDING (a transition is scheduled at a future device frame), CROSSFADING. On a state request:

1. Find the transition rule: the most specific (exact from and to, then any → to); none → IMMEDIATE with a 1,000 ms crossfade.
2. Compute the current segment's musical position from the device clock: frames since the segment's entry cue s; beat length b = 48000 × 60 / BPM frames (a real number, not an integer in general: 48,000 × 60 / 97 is not; positions are kept as `double` frames, which is deterministic though not exact); beat index = floor(s / b); every boundary T is rounded to the nearest integer frame once, by `llround`-equivalent arithmetic on the `double`.
3. The transition frame T by rule: IMMEDIATE → the next block; NEXT_BEAT → entry + (beat index + 1)·b; NEXT_BAR → the next multiple of b·beatsPerBar; NEXT_GRID → of b·grid; NEXT_CUE → the next music-cue marker after s; SEGMENT_END → the exit cue. If T is less than the stream prebuffer time (85 ms at 4,096 frames) after now, take the following boundary instead.
4. Prepare: open the target segment's stream (and the transition segment's) seeked to its entry cue minus the transition's fade-in (sample-accurate seek), so its downbeat lands at T exactly; schedule the voice with `m_ulStartFrame = T − fadeIn frames`.
5. At T − fadeOut the current segment's fade-out begins (equal power or the curve exponent), and the stinger, if any, is scheduled at T on the state's stinger bus.
6. When the fade completes the state is STABLE on the new segment; a new request during PENDING replaces the pending one if its T is not within 2 blocks.

Sample accuracy: a scheduled start lands on its device frame exactly (the device starts the voice at offset T − block start within the block), asserted by `Test_AudioMusic_TransitionLandsOnBarFrame` with the offline renderer.

### Stingers

A stinger is an event scheduled on the next beat (default) or at the transition frame; it plays over the music (the `Music` bus, ducking the current segment by 3 dB for its duration when the stinger's event sets flags 2 bit 1, `musicDuck`).

## Dialogue and localized audio

### Dialogue lines

A dialogue line is a row of a bank's dialogue table: line ID, speaker, subtitle key, priority, ducking, a template event (bus, attenuation, sends) and one sound per locale (soft references, STREAMED). `Audio.PlayDialogue` resolves the line's sound for the active locale, requests its load on demand (`Zenith_AssetLoader::RequestLoad` at HIGH priority, with the completion polled by the control side), opens its stream when active, and starts the voice when the prebuffer is full; `Audio.PrepareDialogue` performs the load and prebuffer without playing (kept 10 s). A line whose start is delayed more than 300 ms past its post counts `audio.dialogue.late_delta`. Lines of one speaker entity never overlap: a new line on the same entity stops the previous with a 100 ms fade.

### Locale selection

The active dialogue locale is `audio.dialogue.locale` when non-empty; else the value of the key `ui.locale` when that key is registered (UI's; read by name through the configuration store); else `Zenith_Platform::GetPreferredLocale()`. For a line, the sound is chosen by: exact tag match; else the language subtag match (`fr-CA` → `fr`); else the bank's first locale entry (the authoring default). A locale change takes effect for lines posted after it; playing lines continue. Dialogue resolution reads the world's recorded effective tag (§ "Recorded dialogue locale"), of which this source order gives the derived part.

<!-- elaboration -->
Localized events beyond dialogue lines live in locale variant banks. A locale-neutral base bank lists its variants (`localeVariants`, `Zenith2/Docs/Design/AUDIO/Formats.md` → "Locale variant section") as soft references. When a base bank with variants becomes active, and again on every locale change, `Audio.Mix` resolves the active locale against the variant tags by the line rule above and requests that variant through `Zenith_AssetLoader::RequestLoad` (NORMAL priority), holding its handle in AUDIO's retention set for as long as the base bank is active. The variant's events replace the base bank's events of the same ID in the event index while it is active (the base bank's events are the authoring default). On a locale change the old variant's handle is released only after the new variant activated, so a localized event is never unknown in between; with no matching variant the base bank's events stay in effect. No bank is ever loaded "dormant" and no loader is re-run for an active asset: a variant switch is an ordinary load of another asset (`Test_AudioBankLoader_LocaleVariantSwitch`).

### Recorded dialogue locale

Ruling GmB33-DIALOGUE-LOCALE: AUDIO owns the effective dialogue locale as recorded simulation state, so a character's lips (CHAR), a cinematic's face clip (CINE) and the voice AUDIO plays agree in every replay and after every load, and no simulation script reads a user setting.

- **State.** Each audio world has the world resource `Zenith.Audio.DialogueLocale` (`Zenith_AudioDialogueLocaleResource`, declared in the public header `Zenith2/Engine/ZenithAudio/Zenith_AudioDialogueLocale.h`, registered by `Zenith_Module_Audio_RegisterTypes`, version 1, serialized, authority `PHASE_COMMANDS_ONLY`): property 1 `EffectiveTag` (`Zenith_StringID`, the BCP 47 tag of the voice locale), 2 `DerivedTag` (the presentation-side tag of "Locale selection": `audio.dialogue.locale`, else `ui.locale`, else the preferred locale of `Zenith_AudioSystemDesc::m_acPreferredLocale` and `SetPreferredLocale`), 3 `OverrideTag` (the game's voice-locale override; the empty ID means none) and 4 `ChangedTick` (`uint64`, the tick `EffectiveTag` last changed). `EffectiveTag` is `OverrideTag` when it is non-empty, else `DerivedTag`.
- **Recorded input.** The derived tag never reaches simulation directly. When it changes, and at the first tick after a restore whose saved `DerivedTag` differs from the current one, the module parks one `Zenith.Audio.DialogueLocaleChanged` record (world tag, new derived tag) through the module publication sink (`Zenith_WorldAttachContext::GetPublicationSink()`, ruling Q76 (a)) as `PUBLICATION_KIND_GENERIC_RECORDED`. It is recorded with the tick's inputs and applied at the next tick boundary, once per world; a replay applies it from the log and never reads the user setting or the platform.
- **The game's override.** The game's settings script sets the override with the BUFFERED binding `Zenith.Audio.SetVoiceLocale(tag)` (the empty string clears it), an audio trigger of payload kind `AUDIO_TRIGGER_SET_VOICE_LOCALE` that commits or rolls back with its transaction and writes `OverrideTag` when AUDIO's script consumer applies it; a tag that is not 1 to 15 bytes of BCP 47 characters raises ZS2209.
- **Readers.** Dialogue resolution (line selection of "Locale selection", locale variant banks) reads `EffectiveTag` of the line's world. CHAR (the voice locale of its lip-sync clips), CINE (`Zenith_CinematicStartDesc::m_xVoiceLocale`) and GAME read it through AUDIO's accessors without a module edge: by stable name `Zenith.Audio.DialogueLocale` property 1 through reflection, from Luau through the PURE binding `Zenith.Audio.DialogueLocale()`, or in C++ through `Zenith_StringID Zenith_AudioGetEffectiveDialogueLocale(const Zenith_AudioDialogueLocaleResource&)`.
- **Save.** The resource is serialized with its world, so a load restores the effective tag before the first tick, and the restore rule above records any difference the current settings make.

### Dialogue ducking and captions

A line with `ducks` adds a transient ducking target on `Music` and `SFX` at the line's duck amount, with 50 ms attack and 400 ms release, for the line's duration. Captions: AUDIO does not draw them. For each line start, AUDIO records (line ID, speaker string, subtitle key, duration, the start device frame) in the per-world caption feed; caption-cue markers in the line's sound add timed entries. The feed is exposed to Luau as the notification `Zenith.Audio.WhenCaption` (Stage 4, `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script notifications"), which the game's UI script forwards to UI's caption API (`UI.ShowCaption(ctx, key, speaker, kind, seconds, priority)` with kind `"subtitle"` or `"sound"`, `Zenith2/Docs/Design/UI/Accessibility.md` → "Caption cues"); the `audio.bank` cook step contributes every dialogue line's subtitle key and every event caption key to UI's localization extraction through `Zenith_UIExtractionSink` (row UI20, `Zenith2/Docs/Design/UI/Localization.md` → "Extraction sources"; `Zenith2/Docs/Design/AUDIO/Authoring.md` → "audio.bank step"); sound captions (`[door slams]`) come from events with a caption string, posted to the same feed when the event's voice becomes audible above −40 dB.

## Clock and synchronisation

### The device clock

<!-- elaboration -->
The device thread advances a frame counter by 256 per block and pushes, at the start of each callback, one clock-sample feedback record carrying the pair (frame counter, `Zenith_GetMonotonicTimeNs()`) (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Feedback record"), so the pair cannot tear and the control side receives every callback's sample, not one per frame; the output latency estimate comes from the control thread (`Zenith2/Docs/Design/AUDIO/Device.md` → "Latency measurement"). `Audio.Mix` keeps a linear fit of frame against time over the last 64 clock samples (least squares, recomputed each frame; a dropped sample only shortens the window), which gives the current playback frame F(t) = frame at time t minus the output latency in frames. The null pacer and the offline backend produce exact clocks (the offline clock is the rendered frame count).

### Scheduling a trigger

Convention (tied to [Z2-N-ARCH-415]): tick k spans simulation time [(k − 1)·Δ, k·Δ), with Δ the tick duration; its end state is what `Zenith.WorldTransform` holds after it, and `Zenith.PreviousWorldTransform` holds the end state of tick k − 1. A command or trigger recorded in tick k at time τ into that tick (`m_fTimeIntoTick`, seconds; 0 when unknown) has simulation time S = (k − 1)·Δ + τ. The frame being presented interpolates between the ends of ticks K − 1 and K with alpha α, K the world's last executed tick, so it shows simulation time S_r = (K − 1 + α)·Δ. When the payload carries `uAUDIO_POST_FLAG_SCHEDULE_SUBTICK`, the target start is

frame = F(now) + round((S − S_r) · 48000) + round(audio.avSyncOffsetMs · 48)

clamped to [F(now) + 256, F(now) + 4,800] (between one block and 100 ms ahead). Without the flag, the voice starts at the next block. The late counter increments when the device reports a start more than 256 frames after its target. Because every trigger of one tick is shifted by the same S_r, the relative timing of triggers inside a frame is kept: footsteps from a 30 Hz simulation rendered at 120 Hz stay evenly spaced (`Test_AudioClock_SubTickTriggersKeepSpacing`), and a trigger recorded at τ = α·Δ + 20 ms in the last tick starts 960 frames after F(now) (`Test_AudioClock_TriggerAlignsWithPresentedTime`, the absolute alignment).

### Time-locked voices

A voice posted with `uAUDIO_POST_FLAG_TIME_LOCKED` (cinematic tracks, cut-scene dialogue, music synced to a scripted sequence) has a target position: the source frame it should be at equals (S_r − S_start) · 48000 · pitch, where S_start is its post's simulation time. Each frame the control side compares the device's reported position with the target: a drift |e| ≤ 240 frames (5 ms) is ignored; 240 < |e| ≤ 2,048 frames is corrected by nudging the pitch ratio by ±0.5% until the error is below 48 frames; |e| > 2,048 frames (43 ms) resyncs by a seek with a 10 ms crossfade. Pausing the world pauses time-locked voices; `Audio.SeekInstance` jumps them (a cinematic skip).

### Pause and time scale

When a world is paused (`m_bPaused`), every voice on a bus with "pauses with world" pauses (gain fades to 0 over 20 ms, then the source position stops advancing; real slots are kept for up to 10 s, then the voices go virtual with PAUSE behaviour); UI, Music and Dialogue buses keep playing by default. A simulation time scale s (slow motion) multiplies the pitch of buses marked "time-scaled" by s (clamped to [0.25, 2]) through the bus record's time-scale factor, which the device multiplies into every voice of the bus.

## Determinism

### Determinism contract

Audio is presentation (Z2-D-019): no simulation-visible value depends on the device, the mixer, voice selection, timing or meters. Scripts can post and stop but cannot query voice state; notifications (Stage 4) are recorded asynchronous publications, so a replay publishes the recorded results rather than re-measuring. Within audio itself, the control-side state after frame n is a pure function of the queued commands, the component inputs and the elapsed presentation times; the device output is a pure function of the snapshot sequence applied per block. This is what the offline renderer exploits (`Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing").

### Mixer arithmetic rules

- No transcendental call from `<cmath>` in the mixer, the spatialiser, the reverb, the filters or the dynamics: only `Zenith_AudioMath` (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioMath header"); `std::sqrt` alone is allowed.
- No fused multiply-add and no reassociation (the project's floating-point rules, `Zenith2/Docs/Contracts.md` → "Floating-point environment"); FTZ and DAZ set on every AUDIO thread.
- Accumulation order is fixed: voices in device slot order within a bus, buses in post-order, sends in voice order.
- No dependence on addresses, thread IDs or wall-clock time inside the block computation; the snapshot's publish time is diagnostic only.
- Random numbers only from `Zenith_RandomSource` keyed by the instance seed and node index (§ "Event node evaluation").
- Process-wide decisions (real-voice selection, slot and stream assignment, mixer control) are made once per frame in `Audio.Mix`, iterating worlds in world-tag order; per-world work touches only its world, so no result depends on worker scheduling.
- Given those rules, two runs of one build on one platform are bit-identical; across Windows x64 and the S24's arm64 the results are expected to match too (both IEEE 754 binary32 with the same operation sequence), and the Stage 3 cross-platform comparison records whether they do in `Zenith2/Docs/Research/Audio/CrossPlatformHash.md`, which gates nothing.
