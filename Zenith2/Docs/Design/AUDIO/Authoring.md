# AUDIO design: authoring, cooking and scripting

## Scope of this file

This file is part of the AUDIO design family (entry: `Zenith2/Docs/Design/AUDIO.md`). It specifies AUDIO's side of import (the boundary with CONTENT's importer), the cook steps and validators, the document types and their migrations, the `Zenith.Audio` Luau surface including notifications, the data-driven event bridges, the editor surfaces (bank and event editing, the mixer console, auditioning, audition in context, the voice inspector) and the complete audio workflow with its timed walkthrough (Z2-I-AUDIO-16, -17, -21 to -25; Z2-B-AUDIO-005).

## Import

### Audio importer boundary

Source audio files (WAV, FLAC, OGG Vorbis, MP3, AIFF) are decoded by CONTENT's `audio` importer (`Zenith2/Docs/Design/ASSET/Import.md` → "Boundary with CONTENT importers"), which writes one intermediate blob named `decoded-audio` (`Zenith2/Docs/Design/ASSET/Import.md` → "Intermediates") that the `audio.sound` step reads with `ReadIntermediate("decoded-audio")`. AUDIO specifies the blob's layout (cross-area row to CONTENT):

| Offset | Type | Field |
|---|---|---|
| 0 | 4 bytes | magic `ZAPC` |
| 4 | u32 | version 1 |
| 8 | u32 | sample rate, 8,000 … 192,000 |
| 12 | u16 | channel count, 1 … 8 |
| 14 | u16 | 32 (bits per sample: float) |
| 16 | u64 | frame count |
| 24 | u64 | source loop start (`UINT64_MAX` none) |
| 32 | u64 | source loop end |
| 40 | u32 | marker count (≤ 64) |
| 44 | u32 | channel mask (WAVE extensible; 0 = default order for the count) |
| 48 | 64 × marker count | markers: `u64` frame, 8 bytes 0, 48-byte NUL-padded name |
| … | f32 × frames × channels | interleaved samples in [−1, 1] (values outside are clamped by the step and counted in the report) |

The importer also declares the import settings keys of `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound import settings" in its `Zenith_ImporterDesc`.

### Import settings keys

The settings are per asset (the asset's import sidecar), edited in the editor's sound inspector (§ "Bank and event editor"); a change re-cooks only that sound and the banks that published-read its properties. The editor shows, beside the settings, the cooked result: load mode, codec, duration, cooked size, loudness and true peak (read from the published properties).

## Cook steps

### Step registration

```cpp
// Zenith2/Tools/ZenithCook/Internal/Audio/Zenith_AudioCookSteps.h
Zenith_Result<void, Zenith_CookError> Zenith_RegisterAudioCookSteps(Zenith_CookStepRegistry& xRegistry,
	Zenith_CookServices& xServices);
```

Called by the cook services in canonical module order (`Zenith2/Docs/Design/ASSET/Cook.md` → "Step registration functions"); it registers three steps, two validators and two content rules:

| Step ID | Version | Phase | Kind | Runs after | Tool versions | Out of process |
|---|---|---|---|---|---|---|
| `audio.sound` | 1 | `COOK_PHASE_ASSET` | 10 | — | the D-SEL-CODEC encoder library version | yes |
| `audio.bank` | 1 | `COOK_PHASE_ASSET` | 11 | `audio.sound` | — | no (it opens source documents) |
| `audio.eventIndex` | 1 | `COOK_PHASE_GLOBAL_POST` | — | `audio.bank` | — | no; output prefix `Audio/` |

Validators (`RegisterValidator`): kind 10 and kind 11. Each builds a `Zenith_AudioCookedSection` span from its `Zenith_CookedFileView` (section kind and bytes, in table order) and calls ZenithAudio's public `Zenith_AudioValidateSoundFile` or `Zenith_AudioValidateBankFile` (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Cooked file validation"). Those functions wrap the parser the loaders run, so the cook's structural validation is exactly the loaders' checks, and ZenithCook includes no `Internal/` header of ZenithAudio (non-negotiable 1). A failure is `COOK_ERROR_STEP_FAILED`, with the message "SND<n>" or "BNK<n>" (`m_uDetail`), the section index and the asset. Content rules (`RegisterRule`): kind 11 quantity `residentMegabytes` (the bank plus its hard-referenced sounds), kind 10 quantity `durationSeconds`; their limits are the content-rule table's (ContentSpecs' audio row for bank memory), breaching is BR-COOK.

### audio.sound step

`Cook` for one sound:

1. Read the intermediate and validate it (magic, version, ranges, sizes); invalid → `COOK_ERROR_STEP_FAILED` naming the asset and the field.
2. Apply `audio.channels = mono` (equal-power sum: (Σ channels) / √n, LFE excluded).
3. Resample to `audio.sampleRate` (§ "Cook-time resampling") when the source rate differs.
4. Trim silence when `audio.trimSilence` (leading and trailing frames whose every channel is below −72 dBFS; loop points and markers shift accordingly; a loop region is never trimmed).
5. Measure loudness and true peak (§ "Loudness measurement"); normalise when `audio.normalise`: gain = target − measured LUFS, reduced so the true peak after gain is ≤ −1 dBTP.
6. Resolve the load mode and codec (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Load mode rule").
7. Quantise to int16 with TPDF dither (two uniform draws of ±0.5 LSB from `Zenith_RandomSource(seed = the low 64 bits of the asset GUID, stream = channel)`), so a cook is bit-reproducible; PCM16 stores it, ADPCM encodes it (the standard IMA encoder choosing each code by the exact decoder recurrence, so encoder and decoder never drift), the codec encodes the float signal at `audio.bitrateKbps` with the library's deterministic settings (fixed complexity, no variable-bitrate search seeded by time).
8. Build the seek table (codec: the regular entries at every multiple of the 0.5 s interval, plus, for a looping sound, the loop entry at the loop start with flag bit 0, merged into the regular entry when the loop start is a multiple of the interval; exactly what check SND8 accepts, `Zenith2/Docs/Design/AUDIO/Formats.md` → "Seek table section"), the markers section (source markers, then loop markers named `LoopStart`/`LoopEnd`), the header.
9. Publish properties for dependents: `audio.loadMode` (u8), `audio.durationFrames` (u64), `audio.channels` (u8), `audio.loudnessLufs` (f32), `audio.residentBytes` (u64), `audio.markers` (the marker ID and frame pairs).
10. Write the file through `MakeWriterDesc('ZSND', 1)` and `WriteAssetFile(bytes, residentEstimate)` (`Zenith2/Docs/Design/ASSET/Cook.md` → "Zenith_CookContext").

`AddKeyInputs` adds the intermediate's hash, every settings value in key order, the step version and the encoder's tool version.

### audio.bank step

1. `OpenSourceDocument(asset GUID)` for the bank document (`Zenith2/Docs/Design/ASSET/Cook.md` → "Source documents in cook steps"); for the master bank also the mixer document named by its `mixer` member. A newer document version than this build reads is a cook error (as SCENE's).
2. Call `Zenith_AudioWriteBankFile` (§ "Registration of document types"; `ZenithAuthoring`, ruling Q84 (a)) with the opened documents and the cook's sound-property source `Zenith_AudioCookSoundPropertySource`: it validates (§ "Cook validation rules"), flattens each event's node tree into contiguous child ranges (breadth-first per parent, children in authored order), computes event IDs, bus IDs, parameter IDs and the other 32-bit IDs, and returns the reference list, the event IDs and paths and the file bytes. A validation failure is `COOK_ERROR_MISSING_REFERENCE` for the sound-reference rule and `COOK_ERROR_STEP_FAILED` for every other rule, with the writer's message (the rule, the document path and the JSON line).
3. For every sound a node, music segment or dialogue line references, the writer reads the sound's published properties through the property source, whose cook implementation calls `ReadDependencyOutput(sound GUID, "audio.loadMode")` and the other properties it needs (duration for music cues and event durations, markers for cues); the step then declares each returned reference with `DeclareReference(guid, HARD)` for resident sounds and `SOFT` for streamed sounds, music segments and dialogue lines.
4. The writer has written the sections of `Zenith2/Docs/Design/AUDIO/Formats.md` → "Sound bank file" (a base bank's `localeVariants` become the locale variant section and soft references; for a master bank the mixer section through `Zenith_AudioWriteMixerSection`); the step publishes `audio.eventIDs` (the sorted u64 list) and `audio.eventPaths` for `audio.eventIndex`, and writes the file.
5. Contribute localization keys to UI's extraction (row UI20): for every dialogue line its subtitle key, and for every event with a caption its caption key, call `Zenith_UIExtractionSink::AddKey` with `m_strKey` the key, `m_strSourcePath` the bank document's repo-relative path, `m_uLine` the JSON line of the line or event, `m_strContext` `"audio.line:<id>"` or `"audio.event:<path>"` and `m_fMaxWidthEm` 0 (`Zenith2/Docs/Design/UI/Localization.md` → "Extraction sources"). The sink is obtained from the cook services when the UI module's cook steps are registered; with no UI module selected the step contributes nothing. `Test_AudioCook_BankContributesSubtitleKeys` asserts the keys, paths and lines for a bank with two lines and one captioned event.

### audio.eventIndex step

A global post step: for every kind-11 asset of the plan, `ReadDependencyOutput(bank, "audio.eventIDs")`; two banks defining one event ID fail the cook (`COOK_ERROR_RULE`, naming both banks and the path), unless one is a locale variant listed by the other (a variant replaces its base bank's events while active) or both are variants of one base bank with different locales. It writes `Audio/events.zevi`: a sorted table of (event ID, path offset) plus a string table, which development builds load (`ReadVerifiedFile`) so warnings about unknown events name the path; Shipping never reads it.

### Cook-time resampling

Polyphase windowed-sinc: 64 taps (32 each side), 256 phases, Kaiser window β = 8.6, cutoff 0.95 × min(source, target) / 2, coefficients normalised per phase to unit DC gain, linear interpolation between adjacent phases for the fractional position, computed in double precision. Stopband attenuation ≥ 90 dB; asserted by `Test_AudioCook_ResamplerStopband` on a generated swept sine. The runtime resampler (Hermite) is used only for pitch and the 24 kHz → 48 kHz playback of 24 kHz sounds.

### Loudness measurement

ITU-R BS.1770-4 integrated loudness at the cooked rate: the K-weighting pre-filter (the standard two biquads; coefficients derived for 48,000 and 24,000 Hz by the bilinear transform of the standard's analogue prototypes), mean square per channel with channel weights 1.0 (L, R, C) and 1.41 (Ls, Rs, Lb, Rb), LFE excluded, 400 ms blocks with 75% overlap, the absolute gate at −70 LUFS and the relative gate at −10 LU. True peak by 4× oversampling (a 48-tap interpolating filter). Silence yields −144 LUFS. `Test_AudioCook_LoudnessOfReferenceSine` asserts a 997 Hz full-scale stereo sine measures −3.01 LUFS ± 0.05 (the standard's calibration point).

### Cook validation rules

| Rule | Error |
|---|---|
| event path syntax and ≤ 255 bytes | `COOK_ERROR_STEP_FAILED` naming the path |
| an event path used twice in one bank | `COOK_ERROR_STEP_FAILED` |
| an event path used in two banks (not locale variants) | `COOK_ERROR_RULE` from `audio.eventIndex` |
| a 32-bit ID collision within the mixer (bus, parameter, snapshot, state, preset, marker within a sound) | `COOK_ERROR_STEP_FAILED` naming both names |
| a bus path, attenuation or reverb preset, parameter, snapshot or music state that the mixer does not define | `COOK_ERROR_STEP_FAILED` |
| a node tree deeper than 8 or larger than 64; more than 8 voices possible from one post (LAYERED and BLEND fan-out counted) | `COOK_ERROR_STEP_FAILED` |
| a sound reference to a GUID that is not a kind-10 asset | `COOK_ERROR_MISSING_REFERENCE` (hard) or the soft-reference warning |
| a music segment whose sound is not STREAMED, or whose cues exceed its duration | `COOK_ERROR_STEP_FAILED` |
| a dialogue line without a sound for the bank's default locale | `COOK_ERROR_STEP_FAILED` |
| `localeVariants` in a bank whose own `locale` is set; a variant GUID that is not a kind-11 asset or whose `locale` differs from its key | `COOK_ERROR_STEP_FAILED` naming the variant |
| mixer: no `Master`, a missing parent, depth > 8, > 64 buses, > 4 effects on a bus, > 4 aux returns, a category unmapped or mapped twice | `COOK_ERROR_STEP_FAILED` |
| any numeric field outside the range `Zenith2/Docs/Design/AUDIO/Formats.md` gives | `COOK_ERROR_STEP_FAILED` naming the member and its JSON line |

### Cook error codes

AUDIO adds no cook error enumerator: every failure is one of ASSET's codes with a message naming the asset, the document path and line, and the AUDIO rule. The runtime loaders' check numbers (SND1 … SND12, BNK1 … BNK11) are reused verbatim in validator messages, so a cook failure and a load failure name the same rule.

## Document types

### Registration of document types

Ruling Q84 (a) moves the document types and every function both ZenithCook and ZenithEditor call into the tool library `ZenithAuthoring`, in the layout `Zenith2/Docs/Design/EDIT.md` → "Tool-side code the editor needs" states (`Zenith2/Docs/Design/AUDIO.md` → "ZenithAuthoring AUDIO files"): a tool can neither include another tool's `Internal/` nor link a tool executable (row ED19), so the earlier `Tools/ZenithCook/Internal/Audio/Zenith_AudioDocumentTypes.h`, "linked by ZenithCook and ZenithEditor", was not buildable.

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringAudio.h (ZenithAuthoring; #include "Authoring/Zenith_AuthoringAudio.h")
#pragma once
#include "ZenithAudio/Zenith_AudioTypes.h"        // Zenith_AudioError, Zenith_AudioMarkerID, Zenith_AudioLoadMode

/// @contract function Zenith_RegisterAudioDocumentTypes
/// Ownership: None. Thread affinity: the caller's (before the registry is frozen). Invalidation: None. Units: None.
/// Errors: the registry's Zenith_DocumentError (a duplicate type name is a programmer error the registry reports).
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_RegisterAudioDocumentTypes(Zenith_MigrationRegistry& xRegistry);

struct Zenith_AudioMarkerProperty { Zenith_AudioMarkerID m_xMarker; uint32 m_uReserved = 0; uint64 m_ulFrame = 0; };
struct Zenith_AudioSoundProperties             // the published properties of audio.sound step 9 that the bank writer reads
{
	Zenith_AudioLoadMode m_eLoadMode;
	uint8 m_uChannels = 0;
	uint64 m_ulDurationFrames = 0;
	uint64 m_ulResidentBytes = 0;
	float m_fLoudnessLufs = -144.0f;
	Zenith_Span<const Zenith_AudioMarkerProperty> m_axMarkers;   // valid until the next call on the same source
};
/// @contract interface Zenith_AudioSoundPropertySource
/// Ownership: Borrowed for one writer call. Thread affinity: the writer's calling thread. Invalidation: m_axMarkers as noted.
/// Units: Frames at the sound's rate; LUFS. Errors: any error means "not a kind-10 asset of this plan or session"; the writer
///   reports the sound-reference rule. Reentrancy: NONE. Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AudioSoundPropertySource
{
public:
	virtual ~Zenith_AudioSoundPropertySource() = default;
	virtual Zenith_Result<Zenith_AudioSoundProperties, Zenith_AudioError> ReadSoundProperties(const Zenith_AssetGUID& xSound) = 0;
};
struct Zenith_AudioBankReference { Zenith_AssetGUID m_xAsset; bool m_bHard = false; };
struct Zenith_AudioBankWriteResult            // every array grows from the caller's heap
{
	Zenith_Array<uint8> m_axFileBytes;        // the complete .zbank (ASSET header, section table, sections)
	Zenith_Array<Zenith_AudioBankReference> m_axReferences;   // in section order, for DeclareReference
	Zenith_Array<uint64> m_aulEventIDs;       // sorted, for the audio.eventIDs property
	Zenith_Array<char> m_axEventPaths;         // NUL-separated, in ID order, for audio.eventPaths
	Zenith_Array<char> m_axMessage;            // on failure: the rule, the document path and the JSON line
};
/// @contract function Zenith_AudioWriteBankFile
/// Ownership: None; xOut's arrays are the caller's. Thread affinity: the caller's (the cook step's worker, or the editor's
///   main thread). Invalidation: None. Units: Bytes.
/// Errors: AUDIO_ERROR_INVALID_DESC with m_uDetail = the 1-based row of Authoring.md "Cook validation rules" that failed
///   (row 7 is the sound-reference rule) and xOut.m_axMessage filled; AUDIO_ERROR_OUT_OF_MEMORY.
/// Version: since Z2 Stage 3. Deterministic: equal documents and properties give equal bytes.
/// @endcontract
Zenith_Result<void, Zenith_AudioError> Zenith_AudioWriteBankFile(const Zenith_OpenedDocument& xBank,
	const Zenith_OpenedDocument* pxMixer, Zenith_AudioSoundPropertySource& xSounds, uint32 uHeaderFlags,
	Zenith_AudioBankWriteResult& xOut);   // pxMixer: the master bank's mixer document, else null; uHeaderFlags: 0, or bit 3
// The embedded mixer section alone (Formats.md "Embedded mixer section"); Zenith_AudioWriteBankFile calls it for a master bank.
Zenith_Result<void, Zenith_AudioError> Zenith_AudioWriteMixerSection(const Zenith_OpenedDocument& xMixer,
	Zenith_Array<uint8>& axSectionBytes, Zenith_Array<char>& axMessage);
// The editor's in-process preview cook (row AU18): Zenith_AudioWriteBankFile with uHeaderFlags = bit 3 (audition-only), the
// master bank's mixer when xBank is the master bank. The bytes become an audition preview bank through the editor's
// in-process asset path (Zenith2/Docs/Design/EDIT/AuthoringTools.md "Audio tools").
Zenith_Result<void, Zenith_AudioError> Zenith_AudioCookBankPreview(const Zenith_OpenedDocument& xBank,
	const Zenith_OpenedDocument* pxMixer, Zenith_AudioSoundPropertySource& xSounds, Zenith_AudioBankWriteResult& xOut);
// The mixer console's live apply: exactly the bytes Zenith_AudioSystem::ApplyMixerLayout takes.
Zenith_Result<void, Zenith_AudioError> Zenith_AudioCookMixerSection(const Zenith_OpenedDocument& xMixer,
	Zenith_Array<uint8>& axSectionBytes, Zenith_Array<char>& axMessage);
```

The cook's property source (`Zenith_AudioCookSoundPropertySource`, `Tools/ZenithCook/Internal/Audio/`) reads `ReadDependencyOutput`; the editor's (`Tools/ZenithEditor/Internal/Audio/`) reads the session's loaded `Zenith_SoundAsset`s (`GetLoadMode`, `GetChannelCount`, `GetFrameCount`, `GetResidentBytes`, `GetIntegratedLoudnessLufs`, the markers; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_SoundAsset"), so a preview needs no cook-server round trip for sounds already cooked. The `audio.sound` encoders, the cook resampler and the loudness measurement stay in ZenithCook. `Test_AudioAuthoring_PreviewCookMatchesCook` (in `Zenith2/Tools/ZenithCook/Tests/Zenith_AudioCook_Tests.cpp`, because it runs the real `audio.bank` step, which only ZenithCook's tests can reach; it links `ZenithAuthoring` as ZenithCook does) cooks a bank with every node kind, music, dialogue and the master mixer through `audio.bank`, cooks the same documents through `Zenith_AudioCookBankPreview` and `Zenith_AudioCookMixerSection` with an in-memory property source returning the same properties, and asserts that every section is byte-identical except the bank header's flags word, which differs exactly in bit 3, and that the mixer section bytes equal the master bank's mixer section.

`Zenith_RegisterAudioDocumentTypes` registers two `Zenith_DocumentTypeDesc`s: `Zenith.Audio.BankDocument` (suffix `.bank.json`, current version 1) and `Zenith.Audio.MixerDocument` (suffix `.mixer.json`, current version 1), each with its canonical-member order and schema tag (`zenith2.audio.bank`, `zenith2.audio.mixer`). The row for `Zenith2/Docs/Design/ASSET/Migration.md` → "Registered document types" is added by AUDIO's first document ticket (cross-area row to ASSET): stable names, the function (declared in `Zenith_AuthoringAudio.h`), ZenithCook and ZenithEditor as hosts, both through `ZenithAuthoring`. `Test_AudioDocuments_RegisteredAndMigrate` runs in `ZenithAuthoring_Tests` (`Zenith2/Tools/Authoring/Tests/Zenith_AuthoringAudio_Tests.cpp`).

### Document migrations

Each version bump adds one migration step (version n → n+1) in the same diff, previewed and journaled by the document framework with unknown members preserved (Z2-D-048). Planned migrations: none at v1. A rename of a bus or event path is an editor refactor (the editor rewrites every bank document and the emitter components that reference the old ID in scenes, through the document framework's batch write), never a migration step, because IDs are hashes of names.

## Luau surface

### Function table

All under the Luau namespace `Zenith.Audio` (binding namespace `Audio`), registered by `Zenith_RegisterAudioScriptBindings`. Contexts: S = system, B = behaviour, L = load. BUFFERED functions append one audio-trigger side effect (capacity class `SCRIPT_CAPACITY_CLASS_AUDIO_TRIGGERS`, one slot) that commits or rolls back with the entity transaction (Z2-D-016, Z2-D-051).

| Luau signature | Contexts | Effect | Records | Stage |
|---|---|---|---|---|
| `Event(path: string): AudioEvent` | S B L | PURE | — (hashes the path; a path over 255 bytes or empty is a bad-argument error) | 3 |
| `Line(id: string): AudioLine` | S B L | PURE | — | 4 |
| `Parameter(name: string): number` | S B L | PURE | — (the 32-bit ID) | 3 |
| `Bus(name: string): number` | S B L | PURE | — | 3 |
| `Snapshot(name: string): number` | S B L | PURE | — | 4 |
| `MusicState(name: string): number` | S B L | PURE | — | 4 |
| `Post(event: AudioEvent, entity: Entity?, volumeDb: number?, pitchCents: number?): AudioInstance` | S B | BUFFERED | `Audio.PostEvent`, following `entity` when given (else 2D or at the listener) | 3 |
| `PostAt(event: AudioEvent, position: vector, volumeDb: number?): AudioInstance` | S B | BUFFERED | `Audio.PostEvent` at a fixed position | 3 |
| `PostOneShot(event: AudioEvent, entity: Entity?)` | S B | BUFFERED | `Audio.PostEvent` with instance 0 (no handle; for fire-and-forget sounds, consuming no instance ID draw) | 3 |
| `Stop(instance: AudioInstance, fadeMs: number?)` | S B | BUFFERED | `Audio.StopInstance` | 3 |
| `StopEntity(entity: Entity, fadeMs: number?)` | S B | BUFFERED | `Audio.StopEntity` | 3 |
| `SetParameter(instance: AudioInstance, parameter: number, value: number, rampMs: number?)` | S B | BUFFERED | `Audio.SetParameter` | 3 |
| `SetGlobalParameter(parameter: number, value: number, rampMs: number?)` | S B | BUFFERED | `Audio.SetGlobalParameter` | 3 |
| `SetBusPaused(bus: number, paused: boolean)` | S B | BUFFERED | `Audio.SetBusPaused` | 3 |
| `SetMixSnapshot(snapshot: number, active: boolean, fadeMs: number?)` | S B | BUFFERED | `Audio.SetMixSnapshot` | 4 |
| `SetMusicState(state: number, immediate: boolean?)` | S B | BUFFERED | `Audio.MusicSetState` | 4 |
| `PlayStinger(event: AudioEvent)` | S B | BUFFERED | `Audio.MusicStinger` | 4 |
| `PlayDialogue(line: AudioLine, entity: Entity?): AudioInstance` | S B | BUFFERED | `Audio.PlayDialogue` | 4 |
| `PrepareDialogue(line: AudioLine)` | S B | BUFFERED | `Audio.PrepareDialogue` | 4 |
| `Seek(instance: AudioInstance, positionMs: number)` | S B | BUFFERED | `Audio.SeekInstance` | 4 |
| `PostTimeLocked(event: AudioEvent, entity: Entity?): AudioInstance` | S B | BUFFERED | `Audio.PostEvent` with `uAUDIO_POST_FLAG_TIME_LOCKED` (cinematic tracks and scripted sequences, `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Time-locked voices") | 4 |
| `WhenFinished(instance: AudioInstance): Ticket` | B | BUFFERED (asynchronous request) | § "Script notifications" | 4 |
| `WhenMarker(instance: AudioInstance, marker: string): Ticket` | B | BUFFERED (asynchronous request) | § "Script notifications" | 4 |
| `WhenCaption(): Ticket` | B | BUFFERED (asynchronous request) | § "Script notifications" | 4 |
| `WhenDeviceChanged(): Ticket` | B | BUFFERED (asynchronous request) | § "Script notifications" | 4 |

Instance IDs: a binding that returns an `AudioInstance` draws `NextRandomDraw()` from the invocation context, sets bit 0 and clears bit 63 (`id = (draw | 1) & ~(1ull << 63)`), so the ID is a deterministic function of the entity transaction and a replay assigns the same IDs; native IDs keep bit 0 and bit 63 clear (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Identifiers") and AUDIO's internal anonymous IDs have bit 63 set and bit 0 clear (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Event node evaluation"), so no two classes collide. Argument kinds and counts are checked by SCRIPT's dispatcher before the thunk runs (ZS1511, ZS1510; an entity of another world ZS1102, `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Catalogue"); AUDIO's thunk validates values and raises its own codes (§ "Audio misuse codes"). Scripts can query nothing about audio state (no voice counts, no playback position): everything audio exposes to simulation arrives through recorded notifications.

### Audio misuse codes

AUDIO's bindings raise codes from one block of `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" ([Z2-N-SCRIPT-926]). AUDIO's block is ZS2200–ZS2299, reserved in that table's row "ZS2200–ZS2299 | AUDIO (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audio misuse codes"), reserved before its bindings diff | ZS2201–ZS2208" (ruling Q81; applied by core-doc patch d as X252). The block is fixed: AUDIO's bindings diff only confirms the codes in use, and there is no renumbering fallback. Every code is kind MISUSE, a controlled Luau error in the caller's transaction, never an assert:

| Code | Raised when | Message |
|---|---|---|
| ZS2201 | `Event`, `Post…` with an event path that is empty, longer than 255 bytes or outside `[A-Za-z0-9_]+(/[A-Za-z0-9_]+)*` | `Audio.<Fn>: '<path>' is not a valid event path` |
| ZS2202 | a volume outside [−96, 12] dB | `Audio.<Fn>: volume <v> dB is outside [-96, 12]` |
| ZS2203 | a pitch outside [−2400, 2400] cents | `Audio.<Fn>: pitch <v> cents is outside [-2400, 2400]` |
| ZS2204 | a fade or ramp outside [0, 60,000] ms, or a seek position below 0 | `Audio.<Fn>: <name> <v> ms is outside [0, 60000]` |
| ZS2205 | a default-constructed `AudioEvent`, `AudioLine` or `AudioInstance` (ID 0) where one is required | `Audio.<Fn>: argument <index> is an empty <Type>` |
| ZS2206 | a name for `Parameter`, `Bus`, `Snapshot`, `MusicState` or `Line` that is empty or longer than 255 bytes | `Audio.<Fn>: '<name>' is not a valid name` |
| ZS2207 | a NaN or infinite number for any numeric argument | `Audio.<Fn>: argument <index> is not a finite number` |
| ZS2208 | a `WhenMarker` marker name that is empty or longer than 47 bytes | `Audio.WhenMarker: '<name>' is not a valid marker name` |

`Test_AudioScript_MisuseCodesRaised` calls each binding with each invalid value and asserts the code and that the transaction's other side effects are rolled back.

### Script trigger payloads

The record a BUFFERED function appends is § "Script side-effect payloads" of `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Zenith_AudioScriptApi header": one kind byte and the command payload. The consumer validates and enqueues it (`m_uSource = 1`); a rejected entity's records never reach it (SCRIPT's merge, Z2-B-SCRIPT-023's BR-MERGE).

### Script notifications

<!-- elaboration -->
Stage 4, with the asynchronous source `ASYNC_SOURCE_AUDIO` (value 10, provisionally assigned by `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry"; AUDIO cites the enumerator and never writes a value of its own, [Z2-N-SCRIPT-925]). The path, end to end:

1. Request. Each `When…` call appends a `SIDE_EFFECT_KIND_ASYNC_REQUEST` record whose payload is byte 0 `ASYNC_SOURCE_AUDIO`, byte 1 the request kind (1 finished, 2 marker, 3 caption, 4 device changed), 6 bytes 0, then the instance ID (`uint64`) and the marker ID (`uint32`, 0 when unused) and 4 bytes 0: 24 bytes. It returns a ticket.
2. Completion ID. At commit, in merge order, SCRIPT assigns the request its completion ID from the world's counter ([Z2-N-SCRIPT-334]); a rolled-back request consumes none. AUDIO never allocates a completion ID.
3. Routing. ZenithRuntime's `Zenith_AsyncRequestMultiplexer` routes the record by byte 0 to `Zenith_AudioAsyncSubmitter`, which AUDIO registered at start with `DeclareScriptAsyncSource(ASYNC_SOURCE_AUDIO, uAUDIO_NOTIFY_RESULT_BYTES)` followed by `SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, xSubmitter)` (`Zenith2/Docs/Design/MOD.md` → "SCENE and SCRIPT seams of the runtime"). `Consume(uWorldTag, eKind, ulCompletionID, xPayload)` validates the payload and appends (completion ID, kind, instance, marker) to the world's request list (capacity Z2-B-SCRIPT-026's audio value, default 32: SCRIPT's merge rejects more, BR-MERGE).
4. Watching. `Audio.Update` of the world evaluates its requests each frame against the world's instances and feedback (`Audio.Mix` routes feedback to the world before the next update); a request whose condition holds is completed.
5. Parking. AUDIO parks each result with the completion ID SCRIPT assigned, the publication kind `PUBLICATION_KIND_GENERIC_RECORDED` and a 16-byte payload, through the single module publication sink MOD's post-signoff patch specifies (ruling Q76 (a); `Zenith_WorldAttachContext::GetPublicationSink()`, `Park`; assumption pending MOD and SCRIPT in `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas"), from `Audio.Update` of that world. AUDIO has no C++-originated publication, so it never allocates on the sink and declares nothing on it beyond what `DeclareScriptAsyncSource` sizes.
6. Publication. The result becomes visible at the next tick boundary of the world in completion-ID order (Z2-D-053), and `ticket:Result()` decodes it through AUDIO's result decoder, registered on AUDIO's binding table with `SetAsyncResultDecoder(ASYNC_SOURCE_AUDIO, …)` before `Seal` (`Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptBinding header"), which fills the fields below and returns false (a world halt, SCRIPT's rule) only for a payload AUDIO's producer could not have written.

Clocks. Results are RECORDED because presentation timing is not recomputable: a replay publishes the recorded payloads at their recorded ticks and never re-measures. In a live run the publication tick depends on when the device or the null pacer reached the condition (wall-clock time), so two live headless runs may publish a result at different ticks; only the replay is tick-exact. Under the offline backend the device clock is the rendered frame count driven by simulation ticks (`Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing"), so offline tests complete on the rendered clock and are tick-exact run to run (`Test_AudioScript_NotificationTickExactOffline`).

Result fields, 16-byte payload (offset 0 `u8` request kind, 1 `u8` flags, 2 `u16` 0, 4 `u32` duration ms, 8 `u64` the line or event ID):

| Request | Completes when | Result fields (through AUDIO's decoder for `ASYNC_SOURCE_AUDIO`) |
|---|---|---|
| `WhenFinished(instance)` | the instance reaches FINISHED, or immediately if the ID is unknown | `finished: boolean` (false when unknown or refused), `stolen: boolean` |
| `WhenMarker(instance, marker)` | the instance's voice crosses the marker (feedback kind 6), or the instance finishes first | `reached: boolean` |
| `WhenCaption()` | the next dialogue line or captioned sound starts in this world | `line: AudioLine?`, `event: AudioEvent?`, `durationMs: number`, `sound: boolean` |
| `WhenDeviceChanged()` | the device state changes between running and not running (RUNNING ↔ LOST, SUSPENDED, INTERRUPTED), or the route changes | `running: boolean`, `interrupted: boolean`, `routeChanged: boolean` |

At most Z2-B-SCRIPT-026's audio value of requests are in flight per world (BR-MERGE at the merge point, SCRIPT's mechanism). The captions path: a game's UI script loops `WhenCaption()` and forwards each result to UI's caption API, whose key for a line is the line ID text (dialogue tables key subtitles by line ID).

### Script examples

```lua
-- a behaviour on a door: plays its creak, and a slam when the creak finishes
local Audio = Zenith.Audio
local CREAK = Audio.Event("Doors/Creak")
local SLAM = Audio.Event("Doors/Slam")
local SPEED = Audio.Parameter("Speed")

function Door:OnOpen(ctx)
	local instance = Audio.Post(CREAK, self.entity)
	Audio.SetParameter(instance, SPEED, self.openSpeed, 50)
	local finished = Audio.WhenFinished(instance)
	finished:Then(function() Audio.PostOneShot(SLAM, self.entity) end)
end
```

`Zenith2/Tests/Audio/Scripts/door.luau` is this example, run by the integration suite (`Test_AudioIntegration_DoorScriptPostsAndChains`).

## Event bridges

### Bridge declarations

Other modules cannot call AUDIO ([Z2-N-ARCH-074]), so their gameplay events reach audio as data: the mixer document's `bridges` list (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Mixer document schema"). A bridge names an ECS event type by stable name, optionally a property and value to match, and one of three actions (the `u8` action of the bridge record, `Zenith2/Docs/Design/AUDIO/Formats.md` → "Embedded mixer section"): 0 post an audio event (following the event's entity, with sub-tick scheduling from a named time property); 1 set a global parameter from a numeric property of the event, ramped over the record's ramp ms; 2 set a global parameter to the record's constant value, ramped over the record's ramp ms (the weather form, § "Engine event types bridged"). Up to 64 bridges; several may name one event type.

### Bridge system

<!-- elaboration -->
`Audio.EventBridge` is a C++ system in `PHASE_EVENTS` (9), registered at every AttachWorld whether or not the layout has bridges (with no bridges its declared `ReadsEvent` set is empty and it returns at once), so a layout edit never needs to add a system. At attach it resolves each bridge's event type (`FindTypeByStableName`; an unknown type deactivates that bridge with one warning) and property offsets (by property name through the type's `Zenith_PropertyInfo` list; a missing or mistyped property deactivates the bridge) and declares `ReadsEvent` for every distinct type plus `EmitsCommands`. At run it reads the tick's delivered events of those types ([Z2-N-ARCH-478]) in delivery order; for each match it records `Audio.PostEvent` (action 0: with `uAUDIO_POST_FLAG_FOLLOW_ENTITY` when set, the `Zenith_EntityHandle` from the entity property copied as is, `m_fTimeIntoTick` from the time property, `uAUDIO_POST_FLAG_SCHEDULE_SUBTICK`) or `Audio.SetGlobalParameter` (action 1: the value property converted to `float`: `F32` as is, `F64` rounded to nearest by `static_cast<float>`, integers converted exactly up to 2^24 and rounded to nearest above, `BOOL` 0 or 1; action 2: the record's constant; both with the record's ramp), with `m_uSource = 2` once queued. Live layout edits (`Zenith_AudioSystem::ApplyMixerLayout`) that change the set of bridged event types are refused with `AUDIO_ERROR_MIXER_LAYOUT` (`m_uDetail` 1) while worlds are attached, because the bridge's declared `ReadsEvent` set is fixed by the compiled schedule; the editor shows "bridged event types changed: restart play-in-editor to apply" and applies every other change of the same edit. Edits that change a bridge's match property, value property, time property or entity property are refused the same way (`m_uDetail` 2), because the bridge resolved those properties' offsets at attach and would read stale offsets; the editor shows "bridge properties changed: restart play-in-editor to apply". `ApplyMixerLayout` compares each bridge's four property names with the main thread's copy of the active layout's, alongside the type-set check (`Zenith2/Docs/Design/AUDIO.md` → "Main thread to Audio.Mix channels" rule 3). Edits that keep the type set and the property names (match values, targets, constants, ramps) apply live (`Test_AudioBridges_LiveEditChangingTypesRefused`). String matches compare the event's string ID with the match text resolved to a string ID at attach (the string table's lookup without interning; text the table lacks deactivates the bridge, because no event can carry it). The bridge reads only event payloads and records commands, so it is deterministic and its commands are replayed like any other.

### Engine event types bridged

| Event type (stable name) | Owner | Typical bridge |
|---|---|---|
| `Zenith.Animation.Notify` | ANIM (`Zenith2/Docs/Design/ANIM/Runtime.md` → "Zenith_AnimationEvents header"; match `Flags` bit 1, the audio notify) | footsteps and foley: match `NameHash` = the hash of `Footstep`, follow the entity, sub-tick from `TimeIntoTick`; the footstep event is a SWITCH on the `Surface` instance parameter, which a game script sets from ENV's terrain physical material (`Zenith2/Docs/Design/ENV/Terrain.md`, its `Sample` query) |
| `Zenith.Environment.WeatherChanged` | ENV | today: constant-value bridges (action 2) matching `ToNameHash` per weather state, each setting `Rain`, `Snow` or `Wind` to the state's authored value with a ramp of the transition time (the mixer document's example), and weather stingers posted by matching `ToNameHash` with `Arrived`; when ENV adds the `RainIntensity`, `SnowIntensity` and `WindSpeed` properties (assumption pending ENV in the entry, row AU14), value bridges (action 1) read them directly |
| `Zenith.Environment.TimeOfDay` | ENV | `SetGlobalParameter` `TimeOfDay` (action 1) from the reflected property of `Zenith_TimeOfDayEvent::m_dHours`, an `F64` converted to the nearest `float` (hours in [0, 24) keep about 7 significant digits, under 0.01 s of error) |
| cinematic cue events | CINE (pending) | posts on cue (CINE's audio tracks may also use time-locked posts through Luau) |
| physics contact events | PHYS (pending) | impact sounds by material pair, with the impulse as a parameter |

Weather ambience itself needs no bridge: ENV's weather effect prefabs carry `Zenith.Audio.Emitter` components, which AUDIO plays like any emitter (`Zenith2/Docs/Design/ENV/WaterWeather.md` → "Effect prefabs"; EN13). Wind intensity reaches audio as a global parameter set by a game script from `Zenith_EvaluateWindCPU`, or by a bridge when ENV emits a wind event.

## Editor surfaces

### Bank and event editor

A ZenithEditor panel (`Zenith2/Tools/ZenithEditor/Internal/Audio/Zenith_AudioBankEditor.cpp`) over bank documents, edited through the document framework (undo, journaled save, migration preview): a tree of events by path; per event the property sheet (bus, priority, spatial, attenuation with a curve graph, limits, cooldown, sends, virtual behaviour, caption); the node tree editor (add, reorder and nest container nodes; weights, ranges, blend curves drawn as graphs; drag sounds from the asset browser onto nodes); parameter curves as editable graphs; the music editor (segments on a timeline with bar and beat grid, entry and exit cues, states and the transition matrix); the dialogue table (lines by ID, per-locale sound slots with missing-locale highlighting). The sound inspector edits import settings and shows the cooked properties and a waveform with loop points and markers (draggable, writing the sidecar's `audio.loopStart`/`audio.loopEnd` and the bank's marker list). "Generate occluders from colliders" creates `Zenith.Audio.Occluder` entities from the selected entities' collider boxes (reading the physics component by stable name; an editor action, never a runtime dependency).

### Mixer console

A panel over the mixer document: one strip per bus in tree order with a fader (dB), mute, solo, meters (peak and RMS from `Zenith_AudioSystem::GetMeters`, master LUFS momentary and short-term), insert effect slots with parameter knobs, send levels, the snapshot list with live activation, ducking rules with a gain-reduction meter. Every edit writes the document (undoable) and, live, cooks the mixer section with `Zenith_AudioCookMixerSection` (§ "Registration of document types"; `ZenithAuthoring`) and applies it with `Zenith_AudioSystem::ApplyMixerLayout` within 100 ms, so changes are heard while the game plays; `SetBusOverride` serves mute and solo without touching the document.

### Audition panel

Select an event, a sound or a dialogue line and play it with `Zenith_AudioSystem::Audition`: transport (play, stop, loop), parameter sliders for the event's parameters (live `SetAuditionParameter`), a positional mode with a top-down pad that moves the source around a virtual listener (attenuation, cone and panning heard and drawn), the voice list of the audition, and the waveform playhead. Auditioning works with no game world (the audition world is internal) and plays edits before they are saved: the panel cooks the edited bank in memory with `Zenith_AudioCookBankPreview` (§ "Registration of document types"), which sets the bank header's audition-only bit (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Bank header section"), and auditions from that preview bank (`m_xBank` of the audition desc). The bit keeps the preview out of the bank list and the event index (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank loader"), so a preview never duplicates the events of the loaded bank it previews (no `audio.events.duplicate_delta`, no warning) and never shadows it in play-in-editor.

### Audition in context

In play-in-editor the game world runs with its real mix and the panels stay live: auditioning an event at the selected entity's position in the running world (the audition voice is spatialised by the game world's listener but routed to the `Audition` bus, so it is never captured into game logic), live mixer changes, and the voice inspector over the game's voices. The panel calls `Audition` with `m_bPositional` true and `m_uContextWorldTag` the play-in-editor world's tag (`Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Audition API"), and every frame writes the selected entity's interpolated world position through `SetAuditionPosition`. `Audio.Mix` M3 spatialises the audition against that world's listener, taken from the world's frame record, in every frame the world was updated, and against the virtual origin listener otherwise, for example after the world detaches (`Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step"). The walkthrough of § "Complete audio workflow" exercises it.

### Profiler and voice inspector

`Zenith_AudioSystem::GetVoiceInspector` feeds a sortable table (instance, event path from `events.zevi`, bus, world, real or virtual, audibility, distance, occlusion, play position), the device panel (backend, endpoint, state, latency, period, underruns, callback share maximum against Z2-B-AUDIO-003), the stream panel (slots, ring fill, read latency), the memory panel (resident bytes by category against Z2-B-AUDIO-001), and the debug views of `Zenith2/Docs/Design/AUDIO.md` → "Debug views". DEVT's on-screen profiler shows the "Audio" row group in game builds; the debug views draw through `Zenith_DebugDraw` (`Zenith2/Docs/Design/DEVT/DebugDraw.md` → "Zenith_DebugDraw header").

## Complete audio workflow

### Workflow steps

The complete audio workflow of Z2-I-AUDIO-21 to -24, all inside ZenithEditor with no text editing:

1. Import: drag WAV files into the asset browser; CONTENT's importer creates the sound assets; set import settings in the sound inspector (loop points, normalisation); the cooked result appears within the reimport row's time (Z2-B-EDIT-004).
2. Bank and event authoring: create a bank, create events (a random footstep container, a looping ambience, a music state pair with a bar-synced transition), assign buses and attenuation, add a parameter curve.
3. Mix: in the mixer console, set bus levels, add a compressor on `SFX`, a ducking rule from `Dialogue` to `Music`, a pause-menu snapshot.
4. Audition in context: place an emitter in the scene, enter play-in-editor, walk the camera past it, hear attenuation and occlusion, change a bus level live, trigger the music transition from the console, and adjust until satisfied; exit and save.

### Workflow walkthrough script

The MP-WALKTHROUGH script `Zenith2/Docs/Research/Walkthroughs/AudioWorkflow.md` (written by the Stage 4 walkthrough ticket in the playthrough-script format of `Zenith2/Docs/Budgets.md` → "Playthrough script format") lists the 4 steps above as 23 timed actions with generated source files supplied by the ticket (a script writes the WAV files from `Zenith_AudioTestSignals` into a scratch folder, so no clip is committed, Q55). A human runs it; the elapsed time is the measurement of Z2-B-AUDIO-005 (BR-ACCEPT), and the usability observations (Z2-I-AUDIO-25) are recorded on the stage acceptance ticket per `Zenith2/Docs/Budgets.md` → "Usability walkthrough targets".
