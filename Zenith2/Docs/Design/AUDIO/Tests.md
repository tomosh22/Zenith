# AUDIO design: test catalogue

## Scope of this file

This file is part of the AUDIO design family (entry: `Zenith2/Docs/Design/AUDIO.md`). It lists every named AUDIO test with the case it asserts, its file and the gate step that runs it, the test support code, the integration suite, the fuzz targets, the benchmarks and the device run records. Unit tests live in `Zenith2/Engine/ZenithAudio/Tests/` (target `ZenithAudio_Tests`, doctest; ZenithAudio is `ZENITH_GPU`, so the target is built in win-debug, win-release, win-asan, android-arm64 and android-x86_64 and never in linux-tsan, [Z2-N-ARCH-173]; the `Test` step runs it in win-debug and win-release and `Sanitizers -Preset win-asan` in win-asan; each test case is its own CTest, [Z2-N-ARCH-179], and every name below is unique programme-wide); cook tests in `Zenith2/Tools/ZenithCook/Tests/Zenith_AudioCook_Tests.cpp`; the document-type test in `Zenith2/Tools/Authoring/Tests/Zenith_AuthoringAudio_Tests.cpp` (`ZenithAuthoring_Tests`, ruling Q84 (a)). Test names map `Test_<Type>_<Behaviour>` ↔ `"ZenithAudio/<Type>/<Behaviour>"` (`Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates"). No test reads a committed audio clip (Q55); fixtures (cooked sounds and banks from generated signals) are generated at test time under `${ZENITH_OUT}/test-fixtures/AUDIO/` ([Z2-N-ARCH-257]); expected values are derived in the test from the specification (signals, formulas, the reference renderer), never recorded from a run, so no golden file needs a recorded refresh (GOLDEN-FILES); no test doubles a concrete class (only AUDIO's own two internal interfaces are substituted, the device backend and the read source; `Zenith_AssetReader` and `Zenith_AssetLoader` are always the real types; FINAL-DOUBLES); no test needs a GPU or an audio endpoint (X170: none of them is `needs-gpu`); no test asks anyone to record a count or a digest (WORKER-EVIDENCE: the gate runs them).

## Test support

### Generated test signals

`Zenith2/Engine/ZenithAudio/Tests/Support/Zenith_AudioTestSignals.h`, header-only (tests may use the STL there):

| Function | Output |
|---|---|
| `Zenith_AudioTestSine(fFrequencyHz, fAmplitude, uFrames, uRate, uChannels)` | a sine, phase 0, every channel equal |
| `Zenith_AudioTestSweep(fStartHz, fEndHz, uFrames, uRate)` | an exponential sine sweep |
| `Zenith_AudioTestWhiteNoise(ulSeed, uFrames)` | uniform noise from `Zenith_RandomSource(ulSeed, 0)` |
| `Zenith_AudioTestPinkNoise(ulSeed, uFrames)` | Voss–McCartney 16-row pink noise |
| `Zenith_AudioTestImpulse(uFrames, uAt)` | a unit impulse |
| `Zenith_AudioTestClickTrain(uPeriodFrames, uFrames)` | impulses every period (latency and scheduling checks) |
| `Zenith_AudioTestFormants(ulSeed, uFrames)` | synthetic vowel formants (three resonators over a glottal pulse train) as a speech-like signal for codec tests |
| `Zenith_AudioTestDecodedIntermediate(signal, uRate, uChannels, loop, markers)` | the `decoded-audio` blob of `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio importer boundary" |
| `Zenith_AudioTestWavBytes(signal, uRate, uChannels)` | an in-memory WAV file (for the walkthrough's generated source files and CONTENT importer tests) |
| `Zenith_AudioTestCookSound(intermediate, settings)` | cooks a `.zsnd` in memory by calling the `audio.sound` step's implementation functions |

### Test heap

`Tests/Support/Zenith_AudioTestHeap.h` (header-only, [Z2-N-ARCH-255]): `Zenith_Heap` is a concrete class, not an interface, so nothing wraps it; the helper constructs a dedicated real `Zenith_Heap` per test (`Zenith2/Docs/Contracts/Memory.md` → "Zenith_Heap", configured through its desc: `m_ulFailAfterAllocations` for fault injection, no emergency reserve) and every `Zenith_AudioSystemDesc` in tests points at it, so `GetStats()` (live allocations, total allocations, failed allocations) shows leaks, and an unchanged total allocation count across a rendered span proves the device thread allocated nothing (the no-allocation scope asserts as well).

### Scripted test backend

`Tests/Support/Zenith_AudioScriptedBackend.h`, specified in `Zenith2/Docs/Design/AUDIO/Device.md` → "Test backend".

### Scripted read source

`Tests/Support/Zenith_AudioScriptedReadSource.h` (header-only, X184) implements `Zenith_AudioReadSource` (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioReadSource") for the streaming and offline tests, injected with `Zenith_AudioSystemTestAccess::SetReadSource` before `Start`. It serves stream reads from in-memory copies of the cooked fixture files (`AddFile(const Zenith_AssetGUID&, Zenith_ByteSpan xCookedFile)`, the same bytes the real registry loaded the sound's prefix from), keyed by the handle's `GetPersistentID()`, and lets a test script each read:

| Member | Effect |
|---|---|
| `SetDelay(uint32 uMinMs, uint32 uMaxMs, uint64 ulSeed)` | every read completes after a delay drawn from `Zenith_RandomSource(ulSeed, 0)` in [uMinMs, uMaxMs] of wall time; completion order then follows the draws, not the submission order |
| `WithholdNext(uint32 uCount)` / `ReleaseWithheld()` | the next uCount reads stay RUNNING until released (the underrun, starvation and timeout tests) |
| `FailNext(Zenith_AssetErrorCode eCode)` / `FailEvery(uint32 uPeriod)` | the next read, or every uPeriod-th, completes FAILED with that code |
| `CancelNextOnSubmit()` | the next read completes CANCELLED although nobody requested it (the READ_FAILED path) |
| `GetOutstandingCount()`, `GetMaxOutstandingPerAsset()`, `GetPriorityOf(Zenith_AudioReadID)` | what the tests assert: one read outstanding per stream, CRITICAL while starved |

`ServiceUntilComplete` waits for the scripted completion (or returns `ASSET_ERROR_TIMEOUT` for a withheld read once the timeout passes), so the offline servicing protocol runs unchanged over it. It is not used for asset loads: those go through the real `Zenith_AssetLoader` over a real registry on the generated fixtures.

### Reference renderer

`Tests/Support/Zenith_AudioReferenceRenderer.h`: double-precision implementations of the resampler, biquads, panning laws, attenuation curves and the reverb's line gains, used as oracles with tolerances (never as the code under test).

## Unit tests

### Types and maths tests

`Zenith_AudioMath_Tests.cpp`, `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioMath_DbGainRoundTrip` | \|GainToDb(DbToGain(x)) − x\| ≤ 2.5 × 10^-5 dB over [−140, +40] dB in 0.01 dB steps; −144 dB ↔ 0 |
| `Test_AudioMath_AgainstDoubleReference` | Exp2 relative error ≤ 1.5 × 10^-7 and Log2's stated bound at the sample sets of `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Decibel and gain conversions" |
| `Test_AudioMath_TrigErrorBounds` | sin and cos ≤ 1.5 × 10^-7, atan2 ≤ 4 × 10^-7 rad, asin and acos ≤ 6 × 10^-7 rad at the sample sets of `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Deterministic trigonometry" |
| `Test_AudioMath_CoefficientBitsMatchTable` | every polynomial coefficient and reduction constant has the bit pattern of the tables in PublicAPI.md |
| `Test_AudioMath_BitIdenticalAcrossCalls` | the same input bits give the same output bits in 10^6 calls on 4 threads |
| `Test_AudioTypes_IdsAreHash64OfNames` | event, bus, parameter IDs equal `Zenith_Hash64` (low 32 bits for 32-bit IDs) of the UTF-8 name |
| `Test_AudioTypes_NativeInstanceIdNeverScriptId` | over 10^6 inputs `Zenith_AudioMakeNativeInstanceID` has bit 0 and bit 63 clear and is never 0; script IDs `(draw \| 1) & ~(1 << 63)` have bit 0 set and bit 63 clear; anonymous internal IDs have bit 63 set and bit 0 clear; no value is in two classes |
| `Test_AudioConfig_KeysPresentationOnly` | every `audio.*` key is registered, non-simulation-affecting, with the default, range and layers of `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Configuration keys" |
| `Test_AudioConfig_PrebufferClampedToRingFill` | `audio.streams.prebufferFrames` 16,384 with `audio.streams.ringFrames` 16,384 reads back an effective 8,192 with one warning; a STREAMED codec fixture declaring 8,192 packet frames then reaches PLAYING with no start timeout; a hand-built desc with a 16,384 prebuffer and a 16,384 ring is rejected by `Create` with `AUDIO_ERROR_INVALID_DESC` (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioSettings") |
| `Test_AudioComponents_RegisteredWithAuthority` | the six components register with their stable names, property IDs and `PHASE_COMMANDS_ONLY` authority, and serialise round-trip |

### Format and loader tests

`Zenith_AudioFormats_Tests.cpp`, `Zenith_AudioLoaders_Tests.cpp`, `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioSoundFile_ParsesEveryCodecAndLayout` | cooked sounds of each codec valid in the build × each channel count parse and match their header |
| `Test_AudioSoundFile_RejectsEachCheck` | one mutated file per check SND1 … SND11 fails with that check number |
| `Test_AudioFileValidate_MatchesLoaderChecks` | the public `Zenith_AudioValidateSoundFile` and `Zenith_AudioValidateBankFile` pass valid sounds of each codec and a valid master bank, fail each mutated file of SND1 … SND11 and BNK1 … BNK11 with `AUDIO_ERROR_CORRUPT_DATA` and the check number the loader reports for the same bytes, and allocate nothing (test heap count unchanged) |
| `Test_AudioBankFile_RejectsEachCheck` | one mutated bank per check BNK1 … BNK11 fails with that check number |
| `Test_AudioSoundLoader_ActivatesResidentPcm` | a resident sound activates, reports its resident bytes, and its samples equal the cooked int16 data |
| `Test_AudioSoundLoader_CorruptBecomesNullObject` | a corrupt sound activates as the NULL_OBJECT fallback and plays silence |
| `Test_AudioSoundLoader_StreamedLoadsPrefixOnly` | a STREAMED sound requests no samples section |
| `Test_AudioSoundLoader_UnloadRetiresAfterEpoch` | Unload acknowledges immediately; memory is freed only after the epoch |
| `Test_AudioBankLoader_EventIndexSorted` | the process event index finds every event of 3 banks in O(log n) |
| `Test_AudioBankLoader_DuplicateEventKeepsFirst` | a second bank with a duplicate ID keeps the first bank's event and counts it |
| `Test_AudioBankLoader_LocaleVariantSwitch` | a base bank with `fr` and `de` variants: with locale `fr` the `fr` variant is requested and its events replace the base's; switching to `de` requests `de`, keeps `fr` active until `de` activates (no event unknown in between), then releases `fr`; with an unmatched locale the base events apply |
| `Test_AudioBankLoader_ConcurrentActivationsHandOff` | two banks and 64 sounds activated from four threads at once all appear after the next `Audio.Mix`, each exactly once; the hand-off queue reports no full event |
| `Test_AudioEventIndex_ReaderKeepsOldIndexUntilRetired` | an index pointer loaded before a swap stays valid across one `Audio.Mix` (win-asan) and is freed at the second |
| `Test_AudioSystem_MasterBankFailureUsesBuiltInLayout` | a failing master bank leaves the built-in layout and the start succeeds |

### Mixer and DSP tests

`Zenith_AudioMixer_Tests.cpp`, `Zenith_AudioDsp_Tests.cpp`, `Test` step (all through the offline renderer or direct DSP calls):

| Test | Asserts |
|---|---|
| `Test_AudioMixer_BusTreeSumsInPostOrder` | a 3-level tree sums each bus into its parent once, with gains applied in order |
| `Test_AudioMixer_SendsReachAuxReturns` | post-fader sends at −6 dB reach the return at −6 dB |
| `Test_AudioMixSnapshot_BlendsOverrides` | two active snapshots combine volume additively and cutoffs by min/max in log frequency with fade weights |
| `Test_AudioDucking_AttackHoldRelease` | a sidechain burst ducks the target by the reduction with the attack, hold and release times within one block |
| `Test_AudioMixer_UserVolumeRampsCategory` | a user volume change ramps its category bus over 50 ms |
| `Test_AudioMixer_DownmixMatricesUnitPower` | 5.1 and 7.1 to stereo and 7.1 to 5.1 matrices equal ITU-R BS.775 coefficients |
| `Test_AudioMixer_OutputLayoutFollowsDevice` | `auto` picks stereo, 5.1 or 7.1 from the device channel count |
| `Test_AudioMixer_FinalClampClearsNaN` | a NaN injected into a bus buffer becomes 0 and is counted; output stays in [−1, 1] |
| `Test_AudioMixer_OverrunCounted` | a callback whose scripted duration exceeds `audio.mixer.overrunSharePercent` of the audio it rendered increments `audio.mixer.overruns` and sets the share maxima |
| `Test_AudioMixer_CallbackShareWithShortBursts` | 96-frame callbacks with a scripted clock: a block costing 3 ms reports a share of 56.25% and no overrun; a block costing 6 ms reports 112.5% and one overrun |
| `Test_AudioResampler_HermiteMatchesReference` | a 1 kHz sine at ratios 0.5, 1, 1.5, 2 within 10^-6 of the reference |
| `Test_AudioResampler_LoopCrossesSeamlessly` | interpolation across a loop point reads loop-start samples |
| `Test_AudioBiquad_ResponseMatchesRbj` | low-pass and high-pass magnitude at 20 frequencies within 0.05 dB of the RBJ formula |
| `Test_AudioEq_BandsMatchRbj` | shelf and peaking bands within 0.05 dB |
| `Test_AudioCompressor_StaticCurve` | steady-state gain reduction matches the soft-knee curve within 0.2 dB |
| `Test_AudioLimiter_NeverExceedsCeiling` | a +12 dB burst never exceeds the ceiling on any frame |
| `Test_AudioDelay_TimeAndFeedback` | an impulse repeats at the delay time with feedback decay |
| `Test_AudioReverb_DecayMatchesRt60` | the impulse response's energy decays 60 dB within ±10% of RT60 for 0.5, 1, 2, 4 s at both quality levels |
| `Test_AudioReverb_PresetChangeCrossfades` | a size change crossfades over 500 ms with no discontinuity above −60 dB |

### Voice management tests

`Zenith_AudioVoices_Tests.cpp`, `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioVoices_WorkedExampleSelection` | the worked example of `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Voice selection worked example" |
| `Test_AudioVoices_RealLimitDefersExcess` | (a) with the limit at 8 and 20 equal audible looping voices of 20 instances posted in one tick, 8 are real and 12 counted in `audio.voices.deferred_delta` in every frame for 60 frames, and no real voice changes slot (no flapping: hysteresis and minimum real time keep them); (b) limit 8, eight equal looping voices real from frame 1 and a ninth instance T, the newest, deferred from frame 1; instance A, older than all nine, beyond its maximum distance (inaudible, no candidate) until frame 5, audible with deferral 0 from then; at frame 6 one real voice ends: T takes the freed slot, not A, although A is the older instance, and A stays deferred (priority rule 4 orders deferred candidates for free slots, BR-DEFER) |
| `Test_AudioVoices_HysteresisPreventsFlapping` | two voices oscillating within 2 dB do not swap slots; at 4 dB they swap once |
| `Test_AudioVoices_MinRealTimeHonoured` | a voice made real keeps its slot for 100 ms against equal candidates |
| `Test_AudioVoices_PerBusLimit` | a bus limit of 4 caps its voices independently of the global limit |
| `Test_AudioVoices_StealModes` | OLDEST, QUIETEST, FARTHEST pick the documented victim; NONE refuses and counts |
| `Test_AudioVoices_CooldownRefusesRepeat` | a post within the cooldown is refused |
| `Test_AudioVoices_VirtualPlayheadAdvances` | a voice virtual for 1.0 s resumes 48,000 × pitch frames further, wrapping loops |
| `Test_AudioVoices_VirtualBehaviours` | PAUSE, RESTART and KILL behave as specified |
| `Test_AudioVoices_StartStopFadesNoClick` | starts, stops and real/virtual changes produce no sample step above −60 dBFS |
| `Test_AudioVoices_InstanceTableFullRefuses` | the 1,025th instance of a world is refused and counted |
| `Test_AudioVoices_SecondaryWorldShare` | the world with the highest session ordinal is primary; a secondary world's voices take at most `audio.voices.secondaryWorldRealMax` slots out of L; a secondary world's music command is ignored and counted |
| `Test_AudioVoices_AuditionSlotsReserved` | with 240 game voices real, an audition still gets real voices from its 16 reserved slots and displaces no game voice |
| `Test_AudioEvents_RandomNodeDeterministicPerInstance` | the same instance ID yields the same voices; no-repeat windows hold; two consecutive anonymous posts of one event in one tick differ in pick or randomisation, and a replay reproduces both; a recorded run at 60 Hz ticks and 30 fps (two ticks drained per frame) replayed at 60 fps (one per frame) gives every anonymous post the same seed, because the seed's ordinal counts within the post's own tick |
| `Test_AudioEvents_NodeKinds` | SEQUENCE step, continuous and shuffle; SWITCH ranges; BLEND crossfade; LAYERED voice count capped at 8 |
| `Test_AudioDeterminism_ReplayProducesSameInstances` | a recorded command sequence replayed gives identical instance and voice tables |

### Spatialisation tests

`Zenith_AudioSpatial_Tests.cpp`, `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioSpatial_AttenuationCurves` | the five curves against their formulas at 20 distances, the end fade and virtualisation beyond max |
| `Test_AudioSpatial_AirAbsorptionCutoff` | the cutoff formula and its 1 kHz floor |
| `Test_AudioSpatial_ConeGain` | inner, outer and interpolated gains |
| `Test_AudioSpatial_DopplerRatio` | approaching and receding ratios, the factor, the clamp, the teleport reset |
| `Test_AudioSpatial_DopplerSteadyAt30HzTicks144HzFrames` | an emitter at 20 m/s with 30 Hz ticks and 144 Hz frames: the per-frame Doppler ratio stays within 0.1% of the analytic value while the geometry is steady |
| `Test_AudioSpatial_PositionsInterpolatedWithAlpha` | an emitter's presented position equals the lerp of its previous and current tick positions at the frame's alpha |
| `Test_AudioSpatial_StereoPanConstantPower` | L² + R² = 1 at every azimuth; rear attenuation; listener forward −Z and right +X |
| `Test_AudioSpatial_VbapSurround` | 5.1 and 7.1 pair selection and gains at speaker and mid-pair azimuths |
| `Test_AudioSpatial_HeadphoneInterauralDelay` | the far-ear delay reaches 14 frames at 90° |
| `Test_AudioSpatial_SpreadReachesUniform` | spread 360° gives equal power to every speaker |
| `Test_AudioSpatial_AttenuationEntityBlend` | distances measured from the blended attenuation position |
| `Test_AudioSpatial_OriginShiftNoDoppler` | a world-origin shift resets velocity estimates for that frame: the pitch ratio stays 1 |

### Occlusion zone and reverb tests

`Zenith_AudioAcoustics_Tests.cpp`, `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioOcclusion_SegmentThroughBoxes` | losses add over crossed boxes, the floor at −48 dB, the minimum cutoff |
| `Test_AudioOcclusion_RaysPerFrameCap` | at most `audio.occlusion.raysPerFrame` tests run; untested voices keep their last result (BR-DEFER) |
| `Test_AudioOcclusion_SmoothingTime` | targets settle with the 150 ms one-pole |
| `Test_AudioZones_PrioritySelection` | nested zones pick the highest priority, then the smallest volume |
| `Test_AudioZones_PortalPath` | a voice behind an open portal is heard from the portal's direction with the openness loss |
| `Test_AudioZones_WallLossWithoutPortal` | no portal path applies the wall loss and cutoff |
| `Test_AudioZones_ReverbCrossfadeOnZoneChange` | the return crossfades presets over 500 ms |
| `Test_AudioZones_AmbienceFollowsListener` | a zone's ambience starts on entry and fades on exit |

### Music and dialogue tests

`Zenith_AudioMusic_Tests.cpp`, `Zenith_AudioDialogue_Tests.cpp`, `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioMusic_TransitionLandsOnBarFrame` | a NEXT_BAR transition's downbeat lands on the exact computed frame in offline output |
| `Test_AudioMusic_EveryRuleComputesBoundary` | IMMEDIATE, NEXT_BEAT, NEXT_BAR, NEXT_GRID, NEXT_CUE, SEGMENT_END boundaries, including the too-soon skip |
| `Test_AudioMusic_StingerOnBeat` | a stinger lands on the next beat frame |
| `Test_AudioMusic_RequestDuringPendingReplaces` | a new state request during PENDING replaces the pending transition |
| `Test_AudioDialogue_LocaleFallback` | exact tag, language subtag, then the default entry |
| `Test_AudioDialogue_LocaleSourceOrder` | `audio.dialogue.locale`, then `ui.locale`, then the platform locale |
| `Test_AudioDialogue_DucksMusic` | a ducking line reduces `Music` and `SFX` by its amount for its duration |
| `Test_AudioDialogue_SameSpeakerReplaces` | a second line on one entity stops the first with a 100 ms fade |
| `Test_AudioDialogue_LateCounted` | a line delayed past 300 ms counts late |

### Clock sync tests

`Zenith_AudioClock_Tests.cpp`, `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioClock_LinearFitTracksDevice` | the 64-point fit predicts the scripted device's frame within 1 block |
| `Test_AudioClock_SubTickTriggersKeepSpacing` | 30 Hz footsteps rendered at 120 Hz frames are evenly spaced within 1 frame of 1,600 frames |
| `Test_AudioClock_TriggerAlignsWithPresentedTime` | with the convention of Mixer.md "Scheduling a trigger", a trigger at τ = α·Δ + 20 ms in the last tick starts 960 frames after F(now), on the offline clock |
| `Test_AudioClock_ScheduleClampAndLateCount` | targets clamp to [1 block, 100 ms] ahead; late starts are counted |
| `Test_AudioClock_AvSyncOffsetShiftsStarts` | `audio.avSyncOffsetMs` = 20 shifts starts by 960 frames |
| `Test_AudioClock_TimeLockedDriftCorrection` | drift ≤ 240 ignored, ≤ 2,048 nudged by ±0.5%, beyond resynced by seek |
| `Test_AudioClock_PauseAndTimeScale` | a paused world pauses its world-paused buses only; time scale 0.5 halves time-scaled bus pitch |

### Device and state machine tests

`Zenith_AudioDevice_Tests.cpp`, `Test` step, on the scripted backend unless noted:

| Test | Asserts |
|---|---|
| `Test_AudioDevice_TransitionTable` | every one of the 150 cells of `Zenith2/Docs/Design/AUDIO/Device.md` → "Transition table" (next state, backend calls, who mixes, the ignored-event counter), one sub-case per cell |
| `Test_AudioDevice_PermanentFocusLossRecoversOnWindowFocus` | after FOCUS_LOSS while foreground, WINDOW_FOCUS_GAINED re-requests focus and a GRANTED answer reaches RUNNING; a FAILED answer stays INTERRUPTED; with no window focus change, the first input transition re-requests focus, a second within 2 s does not |
| `Test_AudioFocusBridge_JavaMatchesNativeRegistration` | the Java source's class name, constructor and native method signatures equal the native registration table's strings |
| `Test_AudioDevice_RetryScheduleAfterLoss` | retries at 0, 1, 2, 4, 8 s, then every 8 s, with a fake clock |
| `Test_AudioDevice_LossHandsOverToPacer` | a STOPPED notification starts the pacer within one block period and the frame counter stays continuous |
| `Test_AudioDevice_RerouteSameFormatContinues` | a REROUTED with the same channel count keeps RUNNING with no reopen |
| `Test_AudioDevice_FormatChangeReopens` | a reroute to 6 channels reopens with the 5.1 layout and fades in |
| `Test_AudioDevice_FocusStates` | the four focus changes give the states and ducking of `Zenith2/Docs/Design/AUDIO/Device.md` → "Audio focus" |
| `Test_AudioDevice_StopResumeAudio` | STOP_AUDIO fades and stops within 30 ms, the mixer clock pauses, RESUME_AUDIO reopens and voices continue from their positions |
| `Test_AudioDevice_RequiredDeviceFailsStart` | with `audio.device.required` and a failing open, `Start` returns `AUDIO_ERROR_DEVICE_OPEN_FAILED` after 2,000 ms |
| `Test_AudioDevice_LatencyReported` | the scripted latency appears in the status and the counter |
| `Test_AudioDevice_MuteWhenUnfocused` | with `audio.muteWhenUnfocused`, window focus loss fades the master to silence over 100 ms and focus gain restores it |
| `Test_AudioDevice_MiniaudioNullBackendRunsCallback` | with `audio.device.backend = miniaudioNull` the real miniaudio device path opens, calls the mixer, reports frames, and closes on the control thread (no endpoint needed) |

### Streaming tests

`Zenith_AudioStream_Tests.cpp`, `Test` step (stream reads through the scripted read source of § "Scripted read source" over cooked generated sounds, unless noted):

| Test | Asserts |
|---|---|
| `Test_AudioStream_StateMachine` | every transition of `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots", run for a stereo and a 6-channel (5.1) STREAMED sound; the 5.1 ring opens with N = 131,072 samples (no `ZENITH_CHECK`) and plays frame for frame equal to a full decode |
| `Test_AudioStream_SurroundRingPowerOfTwo` | for C in {1, 2, 4, 6, 8} and F in {16,384, 32,768, 65,536}, N is the smallest power of two ≥ F × C, at most ⌊N / C⌋ frames are ever held, every push and pop moves whole frames, and a frame that straddles the end of storage reads back intact across the 2^32 index wrap (indices seeded near 2^32) |
| `Test_AudioStream_CarryAreaHoldsLongestTail` | a chunk boundary that cuts a codec unit after its 2-byte prefix and 8,191 of its 8,192 bytes (an 8,193-byte tail) carries it into the 8,208-byte carry area and decodes the packet whole; a PCM16 8-channel boundary carrying a 15-byte partial frame does the same |
| `Test_AudioStream_StartAfterPrebuffer` | the first output frame of a streamed voice is the sound's start frame with every read delayed 50 ms by the scripted read source (`SetDelay(50, 50, …)`) |
| `Test_AudioStream_ChunkLargerThanRingDecodesIncrementally` | a codec chunk decoding to more than 16 rings of frames plays equal to a full decode frame for frame, the ring never exceeds its capacity, and the scripted read source's `GetMaxOutstandingPerAsset()` is 1 throughout |
| `Test_AudioStream_UnderrunOutputsSilence` | a read withheld by the scripted read source (`WithholdNext(1)`) yields silence and one underrun record per episode; after `ReleaseWithheld` playback resumes in sync |
| `Test_AudioStream_StarvationRaisesPriority` | after an underrun the slot goes STARVED, M7 pushes STARVE, and the scripted read source's `GetPriorityOf` reports CRITICAL for the next read; STARVED_RECOVERED returns the slot to PLAYING and its reads to HIGH |
| `Test_AudioStream_SlotStealing` | the 33rd stream closes the lowest-ranked virtual stream's slot and opens it once CLOSED returns it to IDLE, else stays virtual |
| `Test_AudioStream_CancelOnClose` | closing with a read withheld by the scripted read source requests its cancellation, and CLOSED, the stream block's retirement and its free all come only after the read reports CANCELLED (win-asan) |
| `Test_AudioStream_MixDecodeChannelStress` | win-asan: 10^5 open, seek and close cycles at frame rate against a decode thread at full speed, with seeded read delays and a 1-in-50 FAILED injection: every OPEN answered by exactly one PREBUFFERED or one failure, every CLOSE by exactly one CLOSED and never before the read's terminal status, every stream block retired exactly once and freed only after the released epoch passed its E, request and event high water within 128 and 256 with no drop, and no stale-generation event applied (`Zenith2/Docs/Design/AUDIO/Device.md` → "Audio.Mix and decode thread channels") |
| `Test_AudioStream_AssetReadSourceReadsCookedFile` | the production `Zenith_AudioAssetReadSource` over a real `Zenith_AssetRegistry` on generated fixtures: ranges read back equal to the file's bytes, a 33rd concurrent submission is `ASSET_ERROR_QUEUE_FULL`, a cancelled read reports CANCELLED and is released, `ServiceUntilComplete` returns once the read completes |
| `Test_AudioDecode_SeekIsSampleAccurate` | 100 seeks against a full decode at the same frames: bit-equal for PCM16 and IMA ADPCM; for the codec, frame-aligned (cross-correlation lag 0) with a decoded-output error of at most −60 dBFS over the 1,024 frames after the pre-roll (ruling Q92; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Seeking and loop points") |
| `Test_AudioDecode_LoopIsSeamless` | the frames across a loop point equal the loop end followed by the loop start: bit-equal for PCM16 and IMA ADPCM, aligned and within −60 dBFS for the codec (ruling Q92) |
| `Test_AudioAdpcm_DecodeMatchesReferenceEncoderRoundTrip` | decoder output equals the encoder's reconstruction bit for bit and is within 0.5% RMS of the source |
| `Test_AudioCache_HitMissEviction` | a miss plays from a memory stream and fills the cache; LRU eviction skips referenced entries |

### Real-time contract tests

`Zenith_AudioRealTime_Tests.cpp`, `Test` step and `Sanitizers -Preset win-asan`:

| Test | Asserts |
|---|---|
| `Test_AudioRealTime_NoAllocationInCallback` | 10,000 blocks with every feature active make zero allocations or frees on the device thread (the test heap's allocation count and the no-allocation scope) |
| `Test_AudioRealTime_SourceSetIncludeWhitelist` | the include and identifier scan of `Zenith2/Docs/Design/AUDIO/Device.md` → "Real-time source set enforcement" |
| `Test_AudioRealTime_FloatEnvironmentAppliedOnNewThread` | FTZ and DAZ are set inside the callback after a device thread change (detected by `Zenith_IsExpectedFloatEnvironment`) |
| `Test_AudioRealTime_CallbackSizesAnyFrameCount` | callbacks of 1, 96, 191, 256, 480, 1,024 and 4,096 frames (and a sequence alternating 480 and 1,024) produce the same output stream as 256-frame callbacks; FIFO occupancy never exceeds 256 frames (asserted per step under win-asan, the FIFO being exactly one block of storage) and each callback renders exactly ⌈max(0, uFrames − b0) / 256⌉ blocks |
| `Test_AudioSystem_FirstBlockBeforeFirstMix` | the device renders 100 blocks before the first `Audio.Mix`: every block reads the empty epoch-0 snapshot of all three slots, dereferences the built-in layout, starts no voice and outputs silence; the first `Audio.Mix` publishes epoch 1 |
| `Test_AudioRtChannels_StressSnapshotAndFeedback` | 10^6 frames of concurrent publish and apply: every start applied once, every end reported once |
| `Test_AudioRetirement_FreesOnlyAfterEpochAcknowledged` | nothing is freed before the device applies the epoch |
| `Test_AudioRetirement_NothingFreedDuringFade` | a stopped voice held fading across many applied snapshots keeps its samples, cache entry and stream ring until its fade ends and the released epoch passes them (win-asan) |
| `Test_AudioRetirement_SupersededSnapshotKeepsFadingMemory` | the normative worked case of `Zenith2/Docs/Design/AUDIO/Device.md` → "Sample memory retirement": epoch 5 lists v, 6 drops it (E = 6), 6 is superseded unread (`WithholdSnapshotAcquire`), the device applies 7; R reads 5 for the whole held fade, v's sample buffer, cache entry and stream ring stay allocated (win-asan), and they are freed at the first M9 after R reaches 7 |
| `Test_AudioRetirement_ReconcilesDroppedEndRecord` | with the feedback ring full, a voice end is recovered from the slot-state table, counted, and the slot reused |
| `Test_AudioRetirement_FullListDefersRelease` | with the retire list shrunk to 4 entries, 10 sound buffers retired in one frame: 4 appended, 6 counted in `audio.retire.deferred` and kept allocated by their owners (win-asan), every one freed after the released epoch passes its E, none reused while pending |
| `Test_AudioRtChannels_MeterFloodKeepsMarkers` | 64 buses metering, one voice crossing a marker, `Audio.Mix` stalled for 300 ms of 96-frame callbacks: the marker record arrives, `audio.feedback.dropped` stays 0, meters reach `Audio.Mix` only through the meter-frame triple buffer and the superseded frames are counted in `audio.meters.dropped` |
| `Test_AudioRetirement_ReconcilesDroppedStartRecord` | with the feedback ring full when a voice starts, the slot-state table showing the assigned generation playing clears the start flag in the next `Audio.Mix` (counted), so the next snapshot lists the voice without its start flag and the device does not restart it |
| `Test_AudioRealTime_SnapshotApplyClampsIndices` | a snapshot with out-of-range slots, counts and bus indices is clamped without an out-of-bounds read (asan) |

### Command and script tests

`Zenith_AudioCommands_Tests.cpp`, `Zenith_AudioScript_Tests.cpp`, `Test` step (with an ECS world and the Luau runtime):

| Test | Asserts |
|---|---|
| `Test_AudioCommands_PlaybackValidatesPayload` | a payload with an invalid enum or range is dropped and counted, never asserted |
| `Test_AudioCommands_QueueOrderIsMergeOrder` | commands of two systems and script triggers across two ticks are drained in merge order |
| `Test_AudioCommandQueue_FullDropsNewest` | a full queue drops new commands and counts them (the pending BR-DROP row) |
| `Test_AudioCommands_PlaybackFindsWorldByTag` | playback in two worlds appends to each world's own queue by `GetWorldTag()`; a tag with audio disabled drops and counts |
| `Test_AudioScript_MisuseCodesRaised` | each binding with each invalid value raises its ZS2201–ZS2208 code and rolls the transaction's other side effects back |
| `Test_AudioScript_NotificationTickExactOffline` | under the offline backend `WhenFinished` publishes at the same tick in two runs and a replay publishes the recorded payload at the recorded tick |
| `Test_AudioBridges_ConstantValueWeatherBridge` | a `WeatherChanged` event matching `ToNameHash` sets the global parameter to the bridge's constant with its ramp; a `TimeOfDay` value bridge converts `m_dHours` to the nearest float |
| `Test_AudioBridges_LiveEditChangingTypesRefused` | while a world is attached, a live layout edit adding a bridged event type returns `AUDIO_ERROR_MIXER_LAYOUT` with `m_uDetail` 1 and applies nothing of the type change; an edit renaming a bridge's match, value, time or entity property returns it with `m_uDetail` 2 and the bridge keeps reading its attach-time properties; an edit of a match value or a ramp applies |
| `Test_AudioScript_PostReturnsDeterministicInstance` | `Post` returns `(NextRandomDraw() \| 1) & ~(1 << 63)`; a replay returns the same IDs |
| `Test_AudioScript_RollbackDropsTrigger` | a failing entity transaction's triggers never reach the consumer |
| `Test_AudioScript_TriggerCapacityRejectsAtMerge` | exceeding Z2-B-SCRIPT-023's capacity rejects the entity at the merge (BR-MERGE) |
| `Test_AudioScript_ConsumerRegisteredBeforeSeal` | `Seal` succeeds with AUDIO's table and refuses it without the consumer (`SCRIPT_BINDING_ERROR_MISSING_CONSUMER`) |
| `Test_AudioScript_NotificationsPublishAtTickBoundary` | `WhenFinished` results publish at a tick boundary in completion-ID order and replay from the record (Stage 4) |
| `Test_AudioBridge_PostsMatchedEvents` | a bridged event type posts the mapped audio event with the entity and sub-tick time |
| `Test_AudioBridge_UnknownTypeDeactivates` | an unknown event type or property deactivates its bridge with one warning |
| `Test_AudioEmitter_AutoPlayAndStopOnRemove` | an auto-play emitter plays; removing the row stops it with its fade |
| `Test_AudioEmitter_RegionBoundInactive` | an emitter whose `InActiveRegion` flag is false keeps its instance with voices virtual and resumes in place when the flag becomes true |
| `Test_FrameGraph_AudioUpdateOncePerFrame` | one `Audio.Update@W<tag>` per world per frame, for frames with 0, 1, 3 and 4 ticks (the 3-tick case is the one `Zenith2/Docs/Architecture/CPUFrameGraph.md` → "Required tests" names: once, after the last tick), running concurrently with that world's extraction (overlapping zones on a 4-worker run) |
| `Test_FrameGraph_AudioMixAfterEveryWorldUpdate` | `Audio.Mix` runs once per frame after every world's `Audio.Update`, and with no world attached |

### Cook tests

`Zenith2/Tools/ZenithCook/Tests/Zenith_AudioCook_Tests.cpp`, `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioCook_SoundDeterministic` | cooking one intermediate twice gives identical bytes (dither seeded by GUID) |
| `Test_AudioCook_LoadModeRule` | durations at each threshold get the mode and codec of the rule |
| `Test_AudioCook_ResamplerStopband` | ≥ 90 dB stopband on a generated sweep |
| `Test_AudioCook_LoudnessOfReferenceSine` | a 997 Hz full-scale stereo sine measures −3.01 LUFS ± 0.05 |
| `Test_AudioCook_NormaliseRespectsTruePeak` | normalisation never pushes the true peak above −1 dBTP |
| `Test_AudioCook_BankFlattensNodes` | node trees become contiguous child ranges with correct parents |
| `Test_AudioCook_DuplicateEventAcrossBanksFails` | `audio.eventIndex` fails on a duplicate path, passes for locale variants |
| `Test_AudioCook_ValidatorMatchesLoader` | the cook validator accepts exactly what the loaders accept on the format test corpus |
| `Test_AudioCook_SeekTableLoopEntryPassesSnd8` | a looping codec sound whose loop start is not a multiple of the interval gets one extra entry with flag bit 0 and passes SND8; one whose loop start is a multiple gets none extra and passes |
| `Test_AudioCook_BankContributesSubtitleKeys` | a bank with two dialogue lines and one captioned event contributes three keys to `Zenith_UIExtractionSink` with the document path, JSON line and context |
| `Test_AudioCook_RecordLayoutsSumToSizes` | every cooked record and the snapshot header is written at its documented offsets and size (bank header 64, curve 80, segment 48, transition 40, line 40, attenuation preset 96, bridge 48, locale variant 24, snapshot header 256) with zero reserved bytes; in a master bank with every mixer table non-empty (one mix snapshot with an odd override count included), each of the nine `u64` counts and each table starts at an offset ≡ 0 (mod 8) of the file, so both `u64` fields of every bridge record are naturally aligned |
| `Test_AudioCook_EventEnumFieldsRoundTrip` | a bank with one event for each of the 4 × 4 combinations of steal mode and virtual behaviour cooks and loads, and every event reads back its two enumerators from offset 29 (NONE and KILL, value 4, included); a file with 0 or 5 … 7 in either 3-bit field fails BNK5 |
| `Test_AudioCook_MasterBankEmbedsMixer` | the master bank carries the mixer section from the mixer document |
| `Test_AudioCook_ValidationRules` | every row of `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook validation rules" produces its error |
| `Test_AudioAuthoring_PreviewCookMatchesCook` | a bank with every node kind, music, dialogue and the master mixer cooked by `audio.bank` and by `Zenith_AudioCookBankPreview` (same documents, an in-memory property source returning the same properties) gives byte-identical sections except the bank header's flags word, which differs exactly in bit 3 (audition-only); `Zenith_AudioCookMixerSection` gives exactly the master bank's mixer section bytes (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Registration of document types") |
| `Test_AudioCook_ContentRulesMeasureBanks` | the kind-11 `residentMegabytes` and kind-10 `durationSeconds` measurements equal the cooked sizes and durations |

### Authoring library tests

`Zenith2/Tools/Authoring/Tests/Zenith_AuthoringAudio_Tests.cpp` (`ZenithAuthoring_Tests`), `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioDocuments_RegisteredAndMigrate` | both document types register through `Zenith_RegisterAudioDocumentTypes` of `Zenith_AuthoringAudio.h` and round-trip with unknown members preserved |

### Editor tests

`Zenith2/Tools/ZenithEditor/Tests/Audio/Zenith_AudioEditor_Tests.cpp` (`ZenithEditor_Tests`; the area-owned test directory of `Zenith2/Docs/Design/EDIT/Panels.md` → "Area-owned panels"), `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioEditor_BankEditUndoRoundTrip` | creating an event, a node tree and a curve in the bank editor, undoing and redoing, and saving produce the expected bank document |

### Memory and shedding tests

`Zenith_AudioMemory_Tests.cpp`, `Zenith_AudioSystem_Tests.cpp`, `Zenith_AudioOffline_Tests.cpp`, `Test` step:

| Test | Asserts |
|---|---|
| `Test_AudioMemory_ResidentBytesMatchAllocations` | `GetResidentBytes` equals the test heap's live AUDIO bytes within the documented exclusions |
| `Test_AudioMemory_SheddingAtEachLevel` | WARNING and CRITICAL perform the actions of `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Shedding" (BR-GOVERNOR), and nothing simulation-visible changes |
| `Test_AudioSystem_Create_FailsAtEachStep_Unwinds` | a fault at each creation step unwinds with zero outstanding allocations |
| `Test_AudioSystem_HeadlessUsesNullPacer` | a headless start creates no miniaudio context; voices, events and counters run as with the null backend |
| `Test_AudioSystem_ShutdownJoinsThreads` | destruction joins every AUDIO thread and frees every allocation |
| `Test_AudioSystem_DetachFreesTablesAtOnce` | `DestroyWorld` returns with the world's instance, voice and request tables freed (test heap statistics), with no wait; the world's sounds, released with its handles, stay allocated (win-asan) until the released epoch passes the first snapshot that omits them, and its device slots are freed in `Audio.Mix` as their fades end |
| `Test_AudioSystem_NinthWorldDisabled` | a ninth world attaches with audio disabled (its queue and `Audio.Update` registered, no voice tables; its commands counted in `audio.commands.no_world_delta`), and the `DetachWorld` of one of the eight enables it at once, so it is mixed from its next update |
| `Test_AudioSystem_FollowsWorldTransition` | GAME's swap order on the offline backend (N attached while O updates; N's `SessionOrdinal` one above O's; in one prologue `DetachWorld(O)`, then N's first update): every frame before the swap mixes only O's voices, the swap frame's snapshot lists only N's and the audition's, O's slots are freed after their fades, the music player keeps its state and obeys N; a failed transition (N detached before its first update) leaves O primary |
| `Test_AudioSystem_MainChannelsOrderAndBounds` | 37 outstanding main requests with no mix (high water 37, no drop), then one `Audio.Mix` applies them in sequence order and stores the acknowledgements; 100 state publishes between two mixes apply once with the last values (99 counted in `audio.main.state.dropped`); a fifth unacknowledged layout swap returns `AUDIO_ERROR_CAPACITY`; a 17th audition returns `AUDIO_ERROR_CAPACITY`; `StopAudition` called 1,000 times for one instance between two mixes pushes one AUDITION_STOP, and for an ended or an unknown instance pushes nothing (no assert, high water unchanged); an audition slot is freed and its handles released only after its end is acknowledged |
| `Test_AudioCapture_WritesWav` | a capture writes a valid float WAV whose samples equal the offline output |
| `Test_AudioCounters_PublishedPerFrame` | every counter of the entry's table is published once per frame with its kind and `_delta` naming |
| `Test_AudioDiagnostics_WarnOncePerId` | an unknown event posted 100 times logs one warning and counts 100 |
| `Test_AudioDiagnostics_FrameDumpMember` | the frame dump's `audio` member lists the device state, voices with audibility, bus gains, snapshots and streams |
| `Test_AudioSystem_VoiceInspectorListsVoices` | the inspector lists every live voice with its real flag, audibility, distance and position |
| `Test_AudioSystem_MetersPeakRms` | a −6 dBFS sine on a bus reads −6 dB peak and −9 dB RMS within 0.1 dB; master momentary LUFS of a reference sine is within 0.1 LU |
| `Test_AudioSystem_ApplyMixerLayoutLive` | a live layout change applies within one frame, retires the old layout by epoch and keeps playing voices |
| `Test_AudioSystem_AuditionWithoutWorld` | an audition plays on the `Audition` bus with no game world attached (non-silent output: the rendered RMS is above −40 dBFS), with live parameters and a positional pad; an audition-only preview of a loaded bank (header bit 3) activates, plays its events by record index, joins neither the bank list nor the event index, and counts no `audio.events.duplicate_delta` |
| `Test_AudioSystem_AuditionInContextRoutesToAuditionBus` | an audition with `m_uContextWorldTag` naming a running world, moved every frame by `SetAuditionPosition`, is spatialised against that world's listener at the positions written (its pan follows them), routed to `Audition`, and bypasses snapshots and ducking; after that world detaches it falls back to the virtual origin listener and keeps playing |
| `Test_AudioOffline_BitExactAcrossRuns` | two offline renders of the 10 s scenario of `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing" (a streamed music transition, an on-demand dialogue line, two cache voices posted before their fills, a locale variant switch) hash identically and report `IsDeterministic`, with the scripted read source jittered from a different seed in each run (`SetDelay(0, 20, seed)`: stream reads completing 0 to 20 ms after submission and out of request order) |
| `Test_AudioOffline_ServiceTimeoutReported` | a read the scripted read source withholds past 5,000 ms in `ServiceBeforeFrame` ends its voice with `AUDIO_ERROR_IO`, counts `audio.offline.service_timeouts` and clears `IsDeterministic` |
| `Test_AudioOffline_BitExactAcrossWorkerCounts` | 1, 2 and 8 job workers hash identically |
| `Test_AudioUpdate_ThreeWorldsBitExactAcrossWorkerCounts` | three attached worlds with overlapping voice demand render one output hash with 1, 2 and 8 job workers |
| `Test_AudioUpdate_WorldWithoutNodeDoesNotStall` | a world attached after the schedule compiled contributes nothing until its first update, and `Audio.Mix` publishes a snapshot every frame meanwhile |
| `Test_AudioOffline_MatchesDevicePathGivenConsumptionLog` | the null-pacer run of a scenario with a streamed voice and one forced underrun, its consumption log (epochs and underruns) and snapshot archive replayed offline block by block with retirement suspended and the stream voice rewritten to the replay feed (source kind 5: available frames, then silence, then the missing frames skipped once), hashes identically |
| `Test_AudioOffline_FramesPerTickExact` | `FramesForNextTick` sums to exactly 48,000 × seconds for 30, 50, 60 and 144 Hz |

## Integration suite

`Zenith2/Tests/Audio/` (`zenith_add_integration_tests(Audio DEPENDS ZenithAudio ZenithRuntime)`), headless (null pacer) game boots under the `Test` step:

| Test | Stage | Asserts |
|---|---|---|
| `Test_AudioIntegration_HeadlessGame300Ticks` | 3 | a minimal game with Audio selected runs 300 ticks headless with posts, emitters and a bank; `Test_Runtime_HeadlessSkipsDevice` stays green |
| `Test_AudioIntegration_DoorScriptPostsAndChains` | 4 | `Scripts/door.luau` posts, chains on `WhenFinished` and plays the slam |
| `Test_AudioIntegration_EmitterInScene` | 3 | a scene's emitter plays at load and stops at unload |
| `Test_AudioIntegration_AnimNotifyFootsteps` | 4 | an animated character's footstep notifies post footsteps through the bridge (with ANIM) |
| `Test_AudioIntegration_WeatherPrefabAmbience` | 4 | a weather state's effect prefab plays its ambience emitter, and a constant-value bridge on `ToNameHash` sets `Rain` (with ENV, EN13) |
| `Test_AudioIntegration_StreamedCellUnloadFades` | 4 | a streamed cell's emitters fade on unload and no sample memory is freed under a playing voice (with STREAM) |
| `Test_AudioIntegration_ReplayIdenticalCommands` | 3 | a recorded run's replay drains identical audio command sequences |

## Fuzz targets

| Target | Seeds | Asserts |
|---|---|---|
| `Zenith_Fuzz_SoundFile` (`Tests/Fuzz/SoundFile.cpp`) | cooked generated sounds of every codec | the parser never crashes, hangs or reads out of bounds |
| `Zenith_Fuzz_BankFile` (`Tests/Fuzz/BankFile.cpp`) | cooked test banks | as above |
| `Zenith_Fuzz_MixerSnapshot` (`Tests/Fuzz/MixerSnapshot.cpp`) | snapshots from the offline scenario | the device apply step clamps every field |
| `Zenith_Fuzz_TriggerPayload` (`Tests/Fuzz/TriggerPayload.cpp`) | valid payloads of every kind | command playback and the script consumer validate without asserting |

Run over their seed corpora in `Sanitizers -Preset win-asan`; seeds are generated by a test-time step from `Zenith_AudioTestSignals.h`, so no binary corpus is committed.

## Benchmarks

| Benchmark | Measures | Runs |
|---|---|---|
| `Zenith_Bench_AudioMixer` (`Zenith2/Benchmarks/AudioMixer/`) | callback CPU per period at 32, 64, 128 and 240 real voices (the game cap) with every effect class, at 192-frame and 480-frame periods; `Audio.Update` CPU at 256 and 1,024 instances with occlusion and zones | `--smoke` in `Test`; full runs in the budget runs of Z2-B-AUDIO-002 and Z2-B-AUDIO-003 on the reference desktop and the S24 |
| `Zenith_Bench_AudioCodec` (`Zenith2/Benchmarks/AudioCodec/`) | the D-SEL-CODEC harness of `Zenith2/Docs/Design/AUDIO.md` → "D-SEL-CODEC candidates and criteria" | `--smoke` in `Test`; the full run by the ruling ticket |

## Device run records

Recorded on the stage acceptance ticket (`needs-human`, Z2-D-127), each with the build, the device, the date and the observed result:

| Record | Stage | Platform | Procedure |
|---|---|---|---|
| `AUDIO-DR-01` output and latency | 3 | S24 speaker and wired headset; reference desktop | play the click train; record the measured `audio.device.latency_us`; a loopback recording confirms it within 5 ms |
| `AUDIO-DR-02` route changes | 3 | S24 | plug and unplug a wired headset and connect and disconnect Bluetooth during playback: playback continues within 500 ms on the new route, no crash |
| `AUDIO-DR-03` audio interruption | 3 | S24 | receive a phone call, trigger an alarm, start another media app: INTERRUPTED, silence, resume on focus gain; start a media app from the notification shade while the game stays in the foreground (permanent loss), then touch the game: focus re-requested and audio restored; start a playlist with a Bluetooth headset's play button while the game keeps window focus (permanent loss with no window focus change), then touch the game: the input-transition re-request and its outcome recorded, as the Stage 3 focus ruling's evidence; the build's APK contains the focus class (the bridge initialised, no `audio.device` JNI warning) (the S24 audio-interruption acceptance of Z2-I-AUDIO-13) |
| `AUDIO-DR-04` background and foreground | 3 | S24 | home and return: silence while backgrounded, resume with voices in position |
| `AUDIO-DR-05` endpoint change and unplug | 3 | reference desktop | change the default endpoint, unplug the only device and replug: REROUTING, LOST with the null pacer, recovery on replug |
| `AUDIO-DR-06` soak | 6 | S24 | 30 min at the slice's voice load: zero callback overruns (Z2-B-AUDIO-003), zero stream underruns |
| `AUDIO-DR-07` memory pressure | 6 | S24 | the memory-pressure device run with audio shedding observed in the counters |
