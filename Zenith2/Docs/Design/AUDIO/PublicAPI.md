# AUDIO design: public API

## Scope of this file

This file is part of the AUDIO design family (entry: `Zenith2/Docs/Design/AUDIO.md`). It writes out every public root header of `Zenith2/Engine/ZenithAudio/` in full, with the contract block of each type (`Zenith2/Docs/Contracts.md` → "Contract blocks per type"), and the internal interfaces the tests and the device layer implement. The algorithms behind the members are in `Zenith2/Docs/Design/AUDIO/Mixer.md`, `Zenith2/Docs/Design/AUDIO/Device.md` and `Zenith2/Docs/Design/AUDIO/Streaming.md`.

## Zenith_Module_Audio header

### Module entry points

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_Module_Audio.h
#pragma once
#include "ZenithRuntime/Zenith_Module.h"        // Zenith_ModuleInstance, Zenith_ModuleStartContext, Zenith_ModuleError
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"

class Zenith_TypeRegistry;
class Zenith_AudioSystem;

/// @contract functions Zenith_Module_Audio
/// Ownership: Start returns the module instance, owned by ZenithRuntime; the instance owns the Zenith_AudioSystem.
/// Thread affinity: MAIN_THREAD. RegisterTypes at startup step 6; Start at step 15 in canonical module order.
/// Invalidation: The system pointer GetSystem returns is valid until the instance is destroyed (shutdown step 6).
/// Units: None.
/// Errors: RegisterTypes returns MODULE_ERROR_REGISTRATION when a type or key registration is rejected. Start returns
///   MODULE_ERROR_OUT_OF_MEMORY, MODULE_ERROR_REGISTRATION (a loader, command kind, binding or consumer registration
///   rejected; also a master bank that fails to load AND the built-in fallback layout failing, which cannot happen for
///   valid code), or MODULE_ERROR_DEVICE only when audio.device.required is true and no device opened within 2,000 ms.
///   Every failure unwinds what Start created.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Audio_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Audio_Start(
	Zenith_ModuleStartContext& xContext);
```

`RegisterTypes` calls, in order: `Zenith_RegisterAudioComponents(xTypes)`, the registration of the payload structs of § "Command payloads", the script value types of § "Script value types", the enums of § "Enumerations" that properties or scripts use, and `Zenith_RegisterAudioConfigKeys(xTypes.GetConfigSchema(), Zenith_GetBuildPlatform())`. `Start` follows `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach". Audio has no backend family, so `Start` takes the start context alone (`Zenith2/Docs/Design/MOD/GameBuild.md` → "Module to target mapping").

### Reaching the system from a tool

```cpp
/// @contract function Zenith_Module_Audio_GetSystem
/// Ownership: Borrowed; the instance keeps ownership.
/// Thread affinity: MAIN_THREAD.
/// Invalidation: As the instance.
/// Units: None.
/// Errors: Precondition: xInstance is the object Zenith_Module_Audio_Start returned (a tool obtains it from the runtime by
///   module name, cross-area row to MOD). Passing another module's instance is a programmer error (ZENITH_ASSERT on the
///   instance's module tag, which every Zenith_AudioModule carries as its first member after the vtable).
/// Version: since Z2 Stage 4 (the editor's audio panels).
/// @endcontract
Zenith_AudioSystem& Zenith_Module_Audio_GetSystem(Zenith_ModuleInstance& xInstance);
```

Runtime code never calls it ([Z2-N-ARCH-850]); the editor's audio panels and the integration suite do.

## Zenith_AudioTypes header

### Constants

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_StringView.h"

inline constexpr uint32 uAUDIO_MIX_SAMPLE_RATE = 48000;
inline constexpr uint32 uAUDIO_MIX_BLOCK_FRAMES = 256;
inline constexpr uint32 uAUDIO_MAX_OUTPUT_CHANNELS = 8;
inline constexpr uint32 uAUDIO_MAX_SOURCE_CHANNELS = 8;
inline constexpr uint32 uAUDIO_MAX_REAL_VOICES = 256;
inline constexpr uint32 uAUDIO_AUDITION_REAL_VOICES = 16;      // device slots reserved for the audition world
inline constexpr uint32 uAUDIO_MAX_VOICES_PER_WORLD = 2048;     // each audio world owns its voice table
inline constexpr uint32 uAUDIO_MAX_INSTANCES_PER_WORLD = 1024;
inline constexpr uint32 uAUDIO_MAX_VOICES_PER_INSTANCE = 8;
inline constexpr uint32 uAUDIO_MAX_WORLDS = 8;
inline constexpr uint32 uAUDIO_MAX_BUSES = 64;
inline constexpr uint32 uAUDIO_MAX_BUS_DEPTH = 8;
inline constexpr uint32 uAUDIO_MAX_EFFECTS_PER_BUS = 4;
inline constexpr uint32 uAUDIO_MAX_AUX_REVERBS = 4;
inline constexpr uint32 uAUDIO_MAX_SENDS_PER_VOICE = 2;
inline constexpr uint32 uAUDIO_MAX_STREAMS = 32;
inline constexpr uint32 uAUDIO_MAX_MIX_SNAPSHOTS = 32;
inline constexpr uint32 uAUDIO_MAX_ACTIVE_MIX_SNAPSHOTS = 8;
inline constexpr uint32 uAUDIO_MAX_DUCKING_RULES = 16;
inline constexpr uint32 uAUDIO_MAX_GLOBAL_PARAMETERS = 64;
inline constexpr uint32 uAUDIO_MAX_INSTANCE_PARAMETERS = 8;
inline constexpr uint32 uAUDIO_MAX_EVENT_NODES = 64;
inline constexpr uint32 uAUDIO_MAX_EVENT_DEPTH = 8;
inline constexpr uint32 uAUDIO_MAX_CURVE_POINTS = 8;
inline constexpr uint32 uAUDIO_MAX_BANKS = 256;
inline constexpr uint32 uAUDIO_MAX_ZONES_PER_WORLD = 256;
inline constexpr uint32 uAUDIO_MAX_PORTALS_PER_WORLD = 512;
inline constexpr uint32 uAUDIO_MAX_OCCLUDERS_PER_WORLD = 4096;
inline constexpr uint32 uAUDIO_MAX_MARKERS_PER_SOUND = 64;
inline constexpr uint32 uAUDIO_MAX_EVENT_PATH_BYTES = 255;
inline constexpr uint32 uAUDIO_COMMAND_QUEUE_CAPACITY_MAX = 65536;
inline constexpr uint32 uAUDIO_FEEDBACK_RING_CAPACITY = 1024;
inline constexpr uint32 uAUDIO_MAX_SOUND_RECORDS = 16384;
inline constexpr uint32 uAUDIO_LOADER_HANDOFF_CAPACITY = 65536;
inline constexpr uint32 uAUDIO_NOTIFY_RESULT_BYTES = 16;
inline constexpr uint32 uAUDIO_MAIN_REQUEST_CAPACITY = 64;     // main request ring (AUDIO.md "Main thread to Audio.Mix channels")
inline constexpr uint32 uAUDIO_MAX_AUDITIONS = 16;             // audition slots
inline constexpr uint32 uAUDIO_MAX_GAME_REAL_VOICES = uAUDIO_MAX_REAL_VOICES - uAUDIO_AUDITION_REAL_VOICES;   // 240: the most L can be
inline constexpr uint32 uAUDIO_DECODE_REQUEST_CAPACITY = 256;  // Audio.Mix → decode thread (Device.md "Audio.Mix and decode thread channels")
inline constexpr uint32 uAUDIO_DECODE_EVENT_CAPACITY = 512;    // decode thread → Audio.Mix
inline constexpr uint32 uAUDIO_MAX_CACHE_FILLS = 32;           // cache-fill requests outstanding at once
inline constexpr uint32 uAUDIO_MAX_PACKET_FRAMES = 8192;       // the largest packet frames a sound header admits (check SND9)
inline constexpr uint32 uAUDIO_COUNTER_COUNT = 65;            // rows of AUDIO.md "Counters"
inline constexpr uint16 uAUDIO_AUDITION_WORLD_TAG = 0xFFFFu;
inline constexpr float fAUDIO_SILENCE_DB = -144.0f;
inline constexpr float fAUDIO_SPEED_OF_SOUND = 343.0f;   // metres per second
```

### Identifiers

```cpp
/// @contract value Zenith_AudioEventID
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE. Members non-const: an ID is copy-assigned wherever it is stored (records, command
///   payloads, containers).
/// Invalidation: None; stable across builds and cooks.
/// Units: None. m_ulValue is Zenith_Hash64 of the event's UTF-8 path bytes (no terminator); 0 is the null event.
/// Errors: None.
/// Version: since Z2 Stage 3. Serialized as the 64-bit value in cooked banks, commands and components.
/// @endcontract
struct Zenith_AudioEventID { uint64 m_ulValue = 0; };
struct Zenith_AudioInstanceID { uint64 m_ulValue = 0; };        // 0 = none; bit 0 set = script-assigned; bit 63 set = AUDIO-internal (anonymous); both clear = native
struct Zenith_AudioDialogueLineID { uint64 m_ulValue = 0; };    // Zenith_Hash64 of the line ID text
struct Zenith_AudioBusID { uint32 m_uValue = 0; };              // low 32 bits of Zenith_Hash64 of the bus path ("Master/SFX/Weapons")
struct Zenith_AudioParameterID { uint32 m_uValue = 0; };        // low 32 bits of Zenith_Hash64 of the parameter name
struct Zenith_AudioMixSnapshotID { uint32 m_uValue = 0; };
struct Zenith_AudioMusicStateID { uint32 m_uValue = 0; };
struct Zenith_AudioMarkerID { uint32 m_uValue = 0; };           // low 32 bits of Zenith_Hash64 of the marker name

constexpr bool operator==(Zenith_AudioEventID xA, Zenith_AudioEventID xB) { return xA.m_ulValue == xB.m_ulValue; }
// operator== likewise for every identifier above (constexpr, inline)

Zenith_AudioEventID Zenith_AudioMakeEventID(Zenith_StringView strPath);
Zenith_AudioDialogueLineID Zenith_AudioMakeDialogueLineID(Zenith_StringView strLine);
Zenith_AudioBusID Zenith_AudioMakeBusID(Zenith_StringView strPath);
Zenith_AudioParameterID Zenith_AudioMakeParameterID(Zenith_StringView strName);
Zenith_AudioMixSnapshotID Zenith_AudioMakeMixSnapshotID(Zenith_StringView strName);
Zenith_AudioMusicStateID Zenith_AudioMakeMusicStateID(Zenith_StringView strName);
Zenith_AudioMarkerID Zenith_AudioMakeMarkerID(Zenith_StringView strName);
// A native recorder's deterministic instance ID: v = Zenith_Hash64 of the 12 little-endian bytes (ulSimulationID, uSequence)
// with bit 0 and bit 63 cleared (v & ~(1ull | (1ull << 63))); v = 2 when that yields 0. Script IDs are
// (NextRandomDraw() | 1) & ~(1ull << 63) (Authoring.md "Function table"); AUDIO's internal IDs for anonymous posts have bit 63
// set and bit 0 clear (Mixer.md "Event node evaluation"). The two marker bits make the three classes disjoint.
Zenith_AudioInstanceID Zenith_AudioMakeNativeInstanceID(uint64 ulSimulationID, uint32 uSequence);
```

A 32-bit ID that collides inside one mixer layout is a cook error (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook validation rules"); a 64-bit event ID colliding across banks is refused at load (`AUDIO_ERROR_DUPLICATE_EVENT`).

### Enumerations

```cpp
enum Zenith_AudioCodec : uint8               // Formats.md "Codec identifiers"; 0 is invalid
{
	AUDIO_CODEC_PCM16 = 1,
	AUDIO_CODEC_IMA_ADPCM = 2,
	AUDIO_CODEC_OPUS = 3,                     // valid only if D-SEL-CODEC rules Opus
	AUDIO_CODEC_VORBIS = 4,                   // valid only if D-SEL-CODEC rules Vorbis
};
enum Zenith_AudioLoadMode : uint8 { AUDIO_LOAD_MODE_RESIDENT_PCM = 1, AUDIO_LOAD_MODE_RESIDENT_COMPRESSED, AUDIO_LOAD_MODE_STREAMED };
enum Zenith_AudioChannelLayout : uint8       // value = channel order of Formats.md "Sound header section"
{
	AUDIO_CHANNEL_LAYOUT_MONO = 1, AUDIO_CHANNEL_LAYOUT_STEREO, AUDIO_CHANNEL_LAYOUT_QUAD,
	AUDIO_CHANNEL_LAYOUT_SURROUND_51, AUDIO_CHANNEL_LAYOUT_SURROUND_71,
};
enum Zenith_AudioOutputLayout : uint8 { AUDIO_OUTPUT_LAYOUT_STEREO = 1, AUDIO_OUTPUT_LAYOUT_HEADPHONES, AUDIO_OUTPUT_LAYOUT_SURROUND_51, AUDIO_OUTPUT_LAYOUT_SURROUND_71 };
enum Zenith_AudioSpatialMode : uint8 { AUDIO_SPATIAL_MODE_NONE = 1, AUDIO_SPATIAL_MODE_POSITIONAL };
enum Zenith_AudioAttenuationCurve : uint8
{
	AUDIO_ATTENUATION_CURVE_NONE = 1, AUDIO_ATTENUATION_CURVE_INVERSE, AUDIO_ATTENUATION_CURVE_LINEAR,
	AUDIO_ATTENUATION_CURVE_LOGARITHMIC, AUDIO_ATTENUATION_CURVE_CUSTOM,
};
enum Zenith_AudioStealMode : uint8 { AUDIO_STEAL_MODE_OLDEST = 1, AUDIO_STEAL_MODE_QUIETEST, AUDIO_STEAL_MODE_FARTHEST, AUDIO_STEAL_MODE_NONE };
enum Zenith_AudioVirtualBehaviour : uint8 { AUDIO_VIRTUAL_BEHAVIOUR_CONTINUE = 1, AUDIO_VIRTUAL_BEHAVIOUR_PAUSE, AUDIO_VIRTUAL_BEHAVIOUR_RESTART, AUDIO_VIRTUAL_BEHAVIOUR_KILL };
enum Zenith_AudioNodeKind : uint8 { AUDIO_NODE_KIND_SOUND = 1, AUDIO_NODE_KIND_RANDOM, AUDIO_NODE_KIND_SEQUENCE, AUDIO_NODE_KIND_SWITCH, AUDIO_NODE_KIND_BLEND, AUDIO_NODE_KIND_LAYERED };
enum Zenith_AudioEffectKind : uint8 { AUDIO_EFFECT_KIND_EQ = 1, AUDIO_EFFECT_KIND_COMPRESSOR, AUDIO_EFFECT_KIND_LIMITER, AUDIO_EFFECT_KIND_LOW_PASS, AUDIO_EFFECT_KIND_HIGH_PASS, AUDIO_EFFECT_KIND_DELAY };
enum Zenith_AudioCurveTarget : uint8
{
	AUDIO_CURVE_TARGET_VOLUME = 1, AUDIO_CURVE_TARGET_PITCH, AUDIO_CURVE_TARGET_LOW_PASS, AUDIO_CURVE_TARGET_HIGH_PASS,
	AUDIO_CURVE_TARGET_SEND_0, AUDIO_CURVE_TARGET_SEND_1, AUDIO_CURVE_TARGET_BLEND_WEIGHT,
};
enum Zenith_AudioVolumeCategory : uint8
{
	AUDIO_VOLUME_CATEGORY_MASTER = 1, AUDIO_VOLUME_CATEGORY_MUSIC, AUDIO_VOLUME_CATEGORY_SFX, AUDIO_VOLUME_CATEGORY_DIALOGUE,
	AUDIO_VOLUME_CATEGORY_AMBIENCE, AUDIO_VOLUME_CATEGORY_UI, AUDIO_VOLUME_CATEGORY_COUNT,
};
enum Zenith_AudioMusicTransitionRule : uint8
{
	AUDIO_MUSIC_TRANSITION_IMMEDIATE = 1, AUDIO_MUSIC_TRANSITION_NEXT_BEAT, AUDIO_MUSIC_TRANSITION_NEXT_BAR,
	AUDIO_MUSIC_TRANSITION_NEXT_GRID, AUDIO_MUSIC_TRANSITION_NEXT_CUE, AUDIO_MUSIC_TRANSITION_SEGMENT_END,
};
enum Zenith_AudioDeviceState : uint8
{
	AUDIO_DEVICE_STATE_CLOSED = 1, AUDIO_DEVICE_STATE_OPENING, AUDIO_DEVICE_STATE_RUNNING, AUDIO_DEVICE_STATE_REROUTING,
	AUDIO_DEVICE_STATE_LOST, AUDIO_DEVICE_STATE_SUSPENDED, AUDIO_DEVICE_STATE_INTERRUPTED, AUDIO_DEVICE_STATE_CLOSING,
	AUDIO_DEVICE_STATE_NULL_DEVICE, AUDIO_DEVICE_STATE_OFFLINE,
};
enum Zenith_AudioFocusState : uint8
{
	AUDIO_FOCUS_STATE_GAINED = 1, AUDIO_FOCUS_STATE_LOST_TRANSIENT, AUDIO_FOCUS_STATE_LOST_TRANSIENT_CAN_DUCK, AUDIO_FOCUS_STATE_LOST,
};
enum Zenith_AudioBackendKind : uint8 { AUDIO_BACKEND_AUTO = 1, AUDIO_BACKEND_MINIAUDIO_NULL, AUDIO_BACKEND_NULL_PACER, AUDIO_BACKEND_OFFLINE };
enum Zenith_AudioStopMode : uint8 { AUDIO_STOP_MODE_FADE = 1, AUDIO_STOP_MODE_IMMEDIATE, AUDIO_STOP_MODE_ALLOW_TAIL };
constexpr const char* Zenith_ToString(Zenith_AudioDeviceState eState);   // and for every enum above
```

Every enum is registered with reflection by `Zenith_Module_Audio_RegisterTypes` under the stable name `Zenith.Audio.<Name>` (`Zenith.Audio.StealMode`, …) so properties, payloads and Luau can use it.

### Zenith_AudioError

`Zenith_AudioErrorCode`, `Zenith_AudioError` and `Zenith_ToString(Zenith_AudioErrorCode)` are declared in this header exactly as `Zenith2/Docs/Design/AUDIO.md` → "Error enumerations" writes them, with the contract block:

```cpp
/// @contract value Zenith_AudioError
/// Ownership: Plain value, 8 bytes, trivially copyable; m_eCode first ([Z2-N-CONTRACT-480] record form).
/// Thread affinity: IMMUTABLE. Members non-const: held as the error of a Zenith_Result, whose move assignment assigns
///   it.
/// Invalidation: None.
/// Units: m_uOsCode is the backend's or OS's result code (HRESULT, aaudio_result_t, ma_result) or 0.
/// Errors: None.
/// Version: since Z2 Stage 3. Codes are append-only.
/// @endcontract
```

## Zenith_AudioMath header

### Decibel and gain conversions

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioMath.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract functions Zenith_AudioMath
/// Ownership: None.
/// Thread affinity: ANY_THREAD, including the device thread (no allocation, no libm call other than std::sqrt, no state).
/// Invalidation: None.
/// Units: dB relative to full scale; gain linear; cents; radians.
/// Errors: None. Inputs outside the stated domain are clamped, never asserted (the functions see authored and computed values).
/// Version: since Z2 Stage 3. Results are bit-identical across runs on one build and platform (Mixer.md "Mixer arithmetic rules").
/// @endcontract
float Zenith_AudioDbToGain(float fDb);         // fDb ≤ -144 → 0; clamped to +48 dB; Exp2(fDb · K_DB_TO_LOG2)
float Zenith_AudioGainToDb(float fGain);       // fGain ≤ 10^-7.2 → -144; Log2(fGain) · K_LOG2_TO_DB
float Zenith_AudioCentsToRatio(float fCents);  // clamped to ±4800 cents; Exp2(fCents / 1200)
float Zenith_AudioExp2(float fX);              // fX clamped to [-125, 126] (the result stays normal under FTZ)
float Zenith_AudioLog2(float fX);              // fX ≤ 2^-126 (or not a number) → -126
```

<!-- elaboration -->
Every function is a fixed sequence of IEEE binary32 operations evaluated in the order written, with no fused multiply-add and no reassociation (`Zenith2/Docs/Contracts.md` → "Floating-point environment"), so the result is a pure function of the input bits. The coefficients below are the binary32 values given by their bit patterns (the decimal is informative; the source writes the bit pattern through `std::bit_cast<float>(0x…u)`). They are near-minimax fits computed by the Remez exchange algorithm in double precision and rounded to binary32; each stated bound is the sum of two parts: the approximation error E_a of the polynomial with its binary32 coefficients evaluated in exact arithmetic (the Remez equioscillation level, re-measured after rounding on 2 × 10^5 points of the interval), and the evaluation error E_e of the binary32 Horner chain (each operation contributes at most half an ulp of its result, propagated through the remaining operations; the derivation's maximum is stated, and a margin of at least 30% is added).

| Constant | Bits | Value |
|---|---|---|
| K_DB_TO_LOG2 = log2(10) / 20 | `0x3E2A152D` | 0.1660964042 |
| K_LOG2_TO_DB = 20 · log10(2) | `0x40C0A8C1` | 6.020599842 |

**`Zenith_AudioExp2(x)`**: clamp x to [−125, 126]; n = floor(x + 0.5) (as an integer, exact); f = x − n (exact, Sterbenz), f ∈ [−0.5, 0.5]; p = 1 + f·(e1 + f·(e2 + f·(e3 + f·(e4 + f·(e5 + f·e6))))) evaluated innermost first; the result is p · 2^n with 2^n built from the exponent bits `(n + 127) << 23` (one exact multiplication).

| Coefficient | Bits | Value |
|---|---|---|
| e1 | `0x3F317219` | 6.931472421e-01 |
| e2 | `0x3E75FDF1` | 2.402265221e-01 |
| e3 | `0x3D635739` | 5.550310388e-02 |
| e4 | `0x3C1D9387` | 9.617692791e-03 |
| e5 | `0x3AAFB93B` | 1.340664341e-03 |
| e6 | `0x392385A5` | 1.559467782e-04 |

Relative error: E_a = 2.2 × 10^-8, E_e ≤ 1.1 × 10^-7 (dominated by the final addition 1 + t, half an ulp of [0.70, 1.42]); stated bound **1.5 × 10^-7 relative** over [−125, 126] (the scaling by 2^n is exact).

**`Zenith_AudioLog2(x)`**: for x ≤ 2^-126 return −126; read the exponent e and the mantissa m ∈ [1, 2) from the bits; when m > √2 (the binary32 `0x3FB504F3`), m = m / 2 (exact) and e = e + 1, so m ∈ [√2/2, √2]; t = (m − 1) / (m + 1) (one rounded subtraction is exact by Sterbenz, the addition and the division round), t ∈ [−0.1716, 0.1716]; u = t·t; result = e + t·(l1 + u·(l3 + u·(l5 + u·l7))).

| Coefficient | Bits | Value |
|---|---|---|
| l1 | `0x4038AA3B` | 2.885390043e+00 |
| l3 | `0x3F763893` | 9.618007541e-01 |
| l5 | `0x3F139B0B` | 5.765845180e-01 |
| l7 | `0x3EDE56CB` | 4.342559278e-01 |

Absolute error of the kernel: E_a = 5.3 × 10^-9, E_e ≤ 1.1 × 10^-7; with the final addition of e (half an ulp of the result) the stated bound is **1.5 × 10^-7 + 6 × 10^-8 · |result|** absolute (1.2 × 10^-6 at |result| = 17).

Conversions built from them: `Zenith_AudioDbToGain(d) = Exp2(d · K_DB_TO_LOG2)` and `Zenith_AudioGainToDb(g) = Log2(g) · K_LOG2_TO_DB`; `Zenith_AudioCentsToRatio(c) = Exp2(c / 1200)` (one correctly rounded division). Their round trip has a derived bound of 2.5 × 10^-5 dB over [−140, +40] (the rounding of d · K, Exp2's relative error times 20/ln 10, Log2's error times K_LOG2_TO_DB and the final rounding of a product up to 140 dB; the simulated maximum is 1.53 × 10^-5 dB).

Tests: `Test_AudioMath_DbGainRoundTrip` asserts |GainToDb(DbToGain(x)) − x| ≤ 2.5 × 10^-5 dB over [−140, +40] in 0.01 dB steps; `Test_AudioMath_AgainstDoubleReference` asserts Exp2's relative error ≤ 1.5 × 10^-7 at 2^20 uniformly spaced points of [−125, 126] and at every binary32 value of [−0.5, 0.5] whose low 8 mantissa bits are 0, and Log2's bound at 2^20 uniformly spaced points of [2^-20, 2^20] plus every binary32 value of [√2/2, √2] whose low 8 mantissa bits are 0, against `std::exp2` and `std::log2` in double precision (tests may use `<cmath>`); `Test_AudioMath_CoefficientBitsMatchTable` asserts every coefficient's bit pattern equals this table, so a rounding change in a refit cannot slip in.

### Deterministic trigonometry

```cpp
float Zenith_AudioSin(float fRadians);              // Cody–Waite reduction by π/2, quadrant kernels
float Zenith_AudioCos(float fRadians);              // the same reduction, quadrant shifted by one
float Zenith_AudioAtan2(float fY, float fX);        // octant reduction + the tan(π/8) identity; (0,0) → 0
float Zenith_AudioAsin(float fX);                   // fX clamped to [-1, 1]; Atan2(fX, Sqrt((1 - fX)·(1 + fX)))
float Zenith_AudioAcos(float fX);                   // π/2 − Zenith_AudioAsin(fX)
float Zenith_AudioSqrt(float fX);                   // std::sqrt (IEEE correctly rounded on both platforms); fX ≤ 0 → 0
```

<!-- elaboration -->
**Reduction for sin and cos.** q = floor(x · TWO_OVER_PI + 0.5) as an integer; r = ((x − q·C1) − q·C2) − q·C3, each product and difference rounded to binary32 in that order (C1 = 1.1001001 in binary has 8 significant bits, so q·C1 needs at most 8 + 16 = 24 bits and is exact for |q| < 2^16, which covers every |q| ≤ 652 the stated bounds use); r ∈ [−π/4, π/4] up to rounding. With k = q mod 4 (two's-complement `q & 3`) and s = sign(r): sin x = {S(r), C(|r|), −S(r), −C(|r|)} and cos x = {C(|r|), −S(r), −C(|r|), S(r)} for k = 0, 1, 2, 3, where S is odd (S(−r) = −S(r)) and both kernels are evaluated on |r|. The bounds hold for |x| ≤ 1024; beyond that the result is finite and in [−1, 1] but unbounded in error (AUDIO's callers pass angles in [−2π, 2π] and wrap oscillator phases to [0, 2π) themselves).

| Constant | Bits | Value |
|---|---|---|
| TWO_OVER_PI | `0x3F22F983` | 6.366197467e-01 |
| C1 | `0x3FC90000` | 1.5703125 |
| C2 | `0x39FDAA22` | 4.838267923e-04 |
| C3 | `0x2C346000` | 2.563282919e-12 |

**Sine kernel** S(r) = r + r·u·(s3 + u·(s5 + u·s7)), u = r·r, on [0, π/4]; **cosine kernel** C(r) = (1 − 0.5·u) + u·u·(c4 + u·(c6 + u·c8)).

| Coefficient | Bits | Value |
|---|---|---|
| s3 | `0xBE2AAAA9` | −1.666666418e-01 |
| s5 | `0x3C0885A8` | 8.332647383e-03 |
| s7 | `0xB94D2C8D` | −1.956691995e-04 |
| c4 | `0x3D2AAAA5` | 4.166664556e-02 |
| c6 | `0xBAB60646` | −1.388736768e-03 |
| c8 | `0x37CD012D` | 2.443845187e-05 |

Errors: sine kernel E_a = 9.2 × 10^-9, E_e ≤ 7 × 10^-8; cosine kernel E_a = 5.1 × 10^-10, E_e ≤ 9.2 × 10^-8; the reduction adds at most 1.5 × 10^-8 for |q| ≤ 652 (the rounding of q·C2). Stated bound for sin and cos: **1.5 × 10^-7 absolute** for |x| ≤ 1024 (simulated maximum 9.3 × 10^-8). The odd degree-9 polynomial on [−π, π] of the earlier draft could not reach its stated 2 × 10^-6 with a minimax error near 6 × 10^-6; reducing to [−π/4, π/4] with two kernels is what makes the bound attainable.

**Arctangent.** atan2(y, x): (0, 0) → 0; with a = |y|, b = |x|: when b ≥ a, θ = A(a / b), else θ = π/2 − A(b / a); then x < 0 → θ = π − θ; y < 0 → θ = −θ (π, π/2 and π/4 as the binary32 `0x40490FDB`, `0x3FC90FDB`, `0x3F490FDB`). A(t) for t ∈ [0, 1]: when t > tan(π/8) (`0x3ED413CD`), A(t) = π/4 − T((1 − t) / (1 + t)), else A(t) = T(t), where the kernel T(z) = z + z·u·(a3 + u·(a5 + u·(a7 + u·a9))), u = z·z, on [0, tan(π/8)].

| Coefficient | Bits | Value |
|---|---|---|
| a3 | `0xBEAAA9E9` | −3.333275616e-01 |
| a5 | `0x3E4C8315` | 1.997187883e-01 |
| a7 | `0xBE0D8FFA` | −1.382445395e-01 |
| a9 | `0x3DA1D860` | 7.902598381e-02 |

Errors: kernel E_a = 5.3 × 10^-9, E_e ≤ 3.4 × 10^-8; the divisions, the identity's subtraction from π/4 and the final π − θ add at most 2.4 × 10^-7 (half an ulp of results up to π). Stated bound for atan2: **4 × 10^-7 rad absolute** (simulated maximum 2.7 × 10^-7 over 4 × 10^5 angles at magnitudes 10^-3 … 10^3). Asin and acos inherit it plus the rounding of the square root's argument: **6 × 10^-7 rad**.

Tests: `Test_AudioMath_TrigErrorBounds` asserts the bounds against double-precision `std::sin`, `std::cos`, `std::atan2` and `std::asin` at 2^20 uniformly spaced points of [−1024, 1024] (sin, cos), every binary32 value of [0, π/4] whose low 6 mantissa bits are 0 (the kernels), 2^20 angles × 7 magnitudes (atan2) and 2^20 points of [−1, 1] (asin, acos); `Test_AudioMath_BitIdenticalAcrossCalls` asserts identical bits for repeated and interleaved calls. The mixer, the spatialiser, the reverb and the filters use these functions and never `<cmath>` transcendental functions; `std::sqrt` is exempt because IEEE 754 requires it correctly rounded.

## Zenith_AudioAssets header

### Zenith_SoundAsset

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioAssets.h
#pragma once
#include "ZenithAssets/Zenith_AssetTypes.h"     // Zenith_AssetKind, ASSET_KIND_SOUND, ASSET_KIND_SOUND_BANK
#include "ZenithAudio/Zenith_AudioTypes.h"
#include "ZenithCore/Zenith_Span.h"

/// @contract type Zenith_SoundAsset
/// Ownership: Created and destroyed by AUDIO's kind-10 loader; reached through Zenith_AssetHandle<Zenith_SoundAsset>
///   and Zenith_AssetRegistry::Resolve. Sample memory is AUDIO's (MEMORY_SUBSYSTEM_AUDIO); after Unload it is freed only
///   when the device epoch passes its last use (Device.md "Sample memory retirement").
/// Thread affinity: IMMUTABLE after activation; any thread may read through a resolved pointer within the frame.
///   Members non-const: the loader sets its storage pointer when it activates the asset.
/// Invalidation: ASSET's: a resolved pointer is valid until the next frame boundary while a handle is held.
/// Units: Frames at the sound's own rate (48,000 or 24,000 Hz); loudness in LUFS; marker positions in frames.
/// Errors: None; a corrupt file activates the NULL_OBJECT fallback (zero frames, silence).
/// Version: since Z2 Stage 3. Cooked format version 1 (Formats.md "Sound file").
/// @endcontract
class Zenith_SoundAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_SOUND;
	uint64 GetFrameCount() const;
	uint32 GetSampleRate() const;
	uint8 GetChannelCount() const;
	Zenith_AudioChannelLayout GetChannelLayout() const;
	Zenith_AudioCodec GetCodec() const;
	Zenith_AudioLoadMode GetLoadMode() const;
	bool HasLoop() const;
	uint64 GetLoopStartFrame() const;
	uint64 GetLoopEndFrame() const;          // exclusive
	float GetIntegratedLoudnessLufs() const;
	float GetPeakDb() const;
	uint32 GetMarkerCount() const;
	Zenith_AudioMarkerID GetMarkerID(uint32 uIndex) const;    // uIndex < GetMarkerCount, else null ID
	uint64 GetMarkerFrame(uint32 uIndex) const;
	uint64 GetResidentBytes() const;
	bool IsNullObject() const;
private:
	friend class Zenith_AudioSoundLoader;    // Internal
	void* m_pData = nullptr;                 // the Internal sound record (Streaming.md "Sound loader")
};
```

### Zenith_SoundBankAsset

```cpp
/// @contract value Zenith_AudioEventInfo
/// Ownership: Owned by the bank; returned by pointer.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: With the bank (ASSET's resolved-pointer rule).
/// Units: m_fMaxDistance metres; m_uDurationMs 0 = indefinite (a loop or a sequence without end).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AudioEventInfo
{
	Zenith_AudioEventID m_xID;
	Zenith_AudioBusID m_xBus;
	uint8 m_uPriority = 128;                 // 0 lowest … 255 highest
	Zenith_AudioSpatialMode m_eSpatial = AUDIO_SPATIAL_MODE_POSITIONAL;
	bool m_bLooping = false;
	bool m_bStreamed = false;                 // any SOUND node of the event plays a STREAMED sound
	float m_fMaxDistance = 0.0f;
	uint32 m_uDurationMs = 0;
};

/// @contract type Zenith_SoundBankAsset
/// Ownership: Created and destroyed by AUDIO's kind-11 loader; reached through Zenith_AssetHandle<Zenith_SoundBankAsset>.
///   A bank holds hard references to its resident sounds (catalogue dependencies) and soft references to streamed and
///   dialogue sounds.
/// Thread affinity: IMMUTABLE after activation. Members non-const: the loader sets its storage pointer when it
///   activates the asset.
/// Invalidation: ASSET's resolved-pointer rule.
/// Units: None.
/// Errors: None; a bank that fails a check fails to load (kind fallback policy FAIL).
/// Version: since Z2 Stage 3. Cooked format version 1 (Formats.md "Sound bank file").
/// @endcontract
class Zenith_SoundBankAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_SOUND_BANK;
	uint32 GetEventCount() const;
	const Zenith_AudioEventInfo* GetEvent(uint32 uIndex) const;       // null when out of range
	const Zenith_AudioEventInfo* FindEvent(Zenith_AudioEventID xID) const;   // binary search, O(log n)
	bool IsMasterBank() const;           // carries the embedded mixer layout
	Zenith_StringView GetLocale() const; // "" for a locale-neutral bank; BCP 47 tag otherwise
	uint32 GetDialogueLineCount() const;
	Zenith_StringView GetEventPath(uint32 uIndex) const;   // the authored path from the bank string table; "" when out of range
	uint64 GetResidentBytes() const;
private:
	friend class Zenith_AudioBankLoader;
	void* m_pData = nullptr;
};
```

The kind table rows (`Zenith2/Docs/Design/ASSET.md` → "Asset kinds") name these runtime object types exactly: kind 10 `Zenith_SoundAsset`, kind 11 `Zenith_SoundBankAsset` (AS26).

### Event and parameter lookup

Event lookup across banks is the system's (§ "Zenith_AudioSystem"): `Zenith_AudioSystem::FindEvent(Zenith_AudioEventID)` binary-searches the process event index, a sorted array of (event ID, bank slot, event index) that `Audio.Mix` rebuilds when the bank list changes and publishes by an atomic pointer swap (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Event index publication"). The pointer a reader loads stays valid until the end of the second `Audio.Mix` after it was replaced, so readers in `Audio.Update` (stage 12) and on the main thread outside the EPILOGUE subgraph never see it freed. Global parameters, buses and mix snapshots are looked up by their 32-bit IDs in the active `Zenith_AudioControlLayout` (open-addressing tables, 2× capacity, linear probing), an immutable control-side object published by the same release pointer swap as the event index and retired by frame count two `Audio.Mix` runs after it is replaced (never by epoch: no device thread reads it).

### Cooked file validation

The kind loaders and the cook's validators run one parser: `Internal/Loading/Zenith_AudioFileParse.cpp`. ZenithCook cannot link another library's `Internal/` (non-negotiable 1), so the parser's two checks are public here, declared in `Zenith_AudioAssets.h` and implemented in `Internal/Loading/Zenith_AudioFileValidate.cpp` as thin wrappers over the same functions the loaders call. A cook validator and a load therefore fail on the same rule with the same check number (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Step registration").

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioAssets.h (continued)
#include "ZenithCore/Zenith_Result.h"         // Zenith_ByteSpan comes from Zenith_Span.h, included above

/// @contract value Zenith_AudioCookedSection
/// Ownership: Borrowed; m_xBytes points into the caller's buffer for the duration of the call.
/// Thread affinity: The calling thread.
/// Invalidation: After the call.
/// Units: m_uKind is the section kind of Formats.md "Sound file sections" or "Bank sections"; m_xBytes the section's decoded
///   bytes (AUDIO stores every section with section codec 0, so they are the stored bytes).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AudioCookedSection
{
	uint32 m_uKind = 0;
	Zenith_ByteSpan m_xBytes;
};

/// @contract function Zenith_AudioValidateSoundFile
/// Ownership: None. Reads only axSections; allocates nothing (no heap, at most 4 KiB of stack); keeps no state.
/// Thread affinity: ANY_THREAD; reentrant (a pure function of its input).
/// Invalidation: None.
/// Units: None.
/// Errors: AUDIO_ERROR_CORRUPT_DATA for the first failing check of Formats.md "Sound file checks" in table order, with
///   m_uDetail = the check number n of SNDn (1 … 11) and m_uIndex = the index into axSections of the offending section
///   (0xFFFF when the failure is a missing or duplicate section). SND12 is a decode-time check and is not run. Untrusted
///   input never asserts. Success means the kind-10 loader's OnSectionsLoaded accepts exactly these sections.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_AudioError> Zenith_AudioValidateSoundFile(Zenith_Span<const Zenith_AudioCookedSection> axSections);

/// @contract function Zenith_AudioValidateBankFile
/// Ownership: As Zenith_AudioValidateSoundFile.
/// Thread affinity: ANY_THREAD; reentrant.
/// Invalidation: None.
/// Units: None.
/// Errors: AUDIO_ERROR_CORRUPT_DATA for the first failing check of Formats.md "Bank file checks" in table order, with
///   m_uDetail = n of BNKn (1 … 11) and m_uIndex as above. Success means the kind-11 loader accepts these sections.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_AudioError> Zenith_AudioValidateBankFile(Zenith_Span<const Zenith_AudioCookedSection> axSections);
```

The functions see sections only, never ASSET's 64-byte header or section table: the caller has already parsed those (the cook through its `Zenith_CookedFileView`, a test from its own fixture), so the public header names no ASSET format type. `Test_AudioFileValidate_MatchesLoaderChecks` gives both functions a valid cooked sound of each codec valid in the build, a valid master bank, and one mutated file per check SND1 … SND11 and BNK1 … BNK11. It asserts that the valid files pass, that each mutated file fails with `AUDIO_ERROR_CORRUPT_DATA` and exactly the check number the loader reports for the same bytes, and that the test heap's allocation count is unchanged across the calls.

## Zenith_AudioComponents header

### Component conventions

Every audio component is registered with `BeginComponent<T>(stableName, 1)` (`Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeBuilder"), with `DefaultAuthority(PHASE_COMMANDS_ONLY)`: no system writes them; game code changes them through field commands (Luau writes, scene authoring) at sync points. Every property is serialized; none is replicated. Property IDs are fixed below and never reused.

### Zenith_AudioListenerComponent

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioComponents.h
#pragma once
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithAudio/Zenith_AudioAssets.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithECS/Zenith_EntityTypes.h"       // Zenith_Entity

/// @contract component Zenith_AudioListenerComponent ("Zenith.Audio.Listener", version 1)
/// Ownership: An ECS row. Usually on the camera entity (GAME places it).
/// Thread affinity: ECS rules; read by Audio.Update.
/// Invalidation: ECS rules.
/// Units: m_fAttenuationBlend in [0, 1].
/// Errors: Out-of-range values are clamped by Audio.Update and counted (audio.content warning once per entity).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AudioListenerComponent
{
	bool m_bActive = true;                           // property 1 "Active" BOOL
	uint8 m_uPriority = 0;                          // property 2 "Priority" U8: the highest-priority active listener of a world wins; ties → lowest entity index
	Zenith_Entity m_xAttenuationEntity;              // property 3 "AttenuationEntity" ENTITY: distances measured from this entity's position when set
	float m_fAttenuationBlend = 0.0f;                // property 4 "AttenuationBlend" F32 [0, 1]: 0 = listener position, 1 = the attenuation entity's position
};
```

The listener's orientation is always its own entity's world rotation (view forward −Z); the attenuation position is the third-person compromise: distance and occlusion measured between the camera and the character.

### Zenith_AudioEmitterComponent

```cpp
/// @contract component Zenith_AudioEmitterComponent ("Zenith.Audio.Emitter", version 1)
/// Ownership: An ECS row; its instance is owned by the audio world and stopped when the row disappears.
/// Thread affinity: ECS rules; read by Audio.Update.
/// Invalidation: ECS rules.
/// Units: dB, cents, milliseconds.
/// Errors: Unknown event or parameter IDs are counted, never asserted.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AudioEmitterComponent
{
	uint64 m_ulEvent = 0;                           // property 1 "Event" U64: a Zenith_AudioEventID value (editor: event picker)
	Zenith_AssetHandle<Zenith_SoundBankAsset> m_xBank;  // property 2 "Bank" ASSET: optional; keeps the bank loaded with the entity
	bool m_bAutoPlay = true;                         // property 3 "AutoPlay" BOOL: play while the row exists (and its region is active)
	float m_fVolumeDb = 0.0f;                        // property 4 "VolumeDb" F32 [-96, 12]
	float m_fPitchCents = 0.0f;                      // property 5 "PitchCents" F32 [-2400, 2400]
	uint32 m_auParameterIDs[4] = {};                // property 6 "ParameterIDs" FIXED_ARRAY<U32, 4>: Zenith_AudioParameterID values, 0 = unused
	float m_afParameterValues[4] = {};               // property 7 "ParameterValues" FIXED_ARRAY<F32, 4>
	uint32 m_uStopFadeMs = 50;                      // property 8 "StopFadeMs" U32 [0, 10000]
	bool m_bRestartOnChange = false;                 // property 9 "RestartOnChange" BOOL: a change of Event restarts the instance
};
```

`Audio.Update` reconciles emitters every frame (`Zenith2/Docs/Design/AUDIO.md` → "Audio.Update step by step" step 3): parameter values are pushed to the emitter's instance each frame with a 50 ms smoothing ramp; a changed `m_ulEvent` stops the old instance with `m_uStopFadeMs` and posts the new one when `m_bRestartOnChange` or `m_bAutoPlay`.

### Zenith_AudioBankComponent

```cpp
/// @contract component Zenith_AudioBankComponent ("Zenith.Audio.Bank", version 1)
/// Ownership: An ECS row holding one bank reference; the bank stays resident while any row (or an emitter's m_xBank,
///   or the always-loaded list) references it.
/// Thread affinity: ECS rules.
/// Invalidation: ECS rules.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AudioBankComponent
{
	Zenith_AssetHandle<Zenith_SoundBankAsset> m_xBank;   // property 1 "Bank" ASSET
};
```

A scene that needs a bank for script-posted events carries one entity with this component; its cook declares the hard reference, so the bank loads with the scene (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank dependencies and residency").

### Zenith_AudioZoneComponent

```cpp
/// @contract component Zenith_AudioZoneComponent ("Zenith.Audio.Zone", version 1)
/// Ownership: An ECS row; an oriented box in the entity's local space centred on its origin.
/// Thread affinity: ECS rules.
/// Invalidation: ECS rules.
/// Units: metres, dB, Hz.
/// Errors: Negative extents are clamped to 0 (the zone is then never selected) and counted.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AudioZoneComponent
{
	Zenith_Vector3 m_xHalfExtents = { 5.0f, 3.0f, 5.0f };   // property 1 "HalfExtents" VEC3
	sint32 m_iPriority = 0;                         // property 2 "Priority" I32: nested zones: highest priority wins
	uint32 m_uReverbPreset = 0;                     // property 3 "ReverbPreset" U32: low 32 bits of the preset name's hash, 0 = none
	float m_fReverbWetDb = -6.0f;                    // property 4 "ReverbWetDb" F32 [-96, 0]
	float m_fFadeDistance = 2.0f;                    // property 5 "FadeDistance" F32 [0, 50]: blend band inside the box boundary
	uint64 m_ulAmbienceEvent = 0;                   // property 6 "AmbienceEvent" U64: a 2D loop played while the listener is inside
	float m_fAmbienceVolumeDb = 0.0f;                // property 7 "AmbienceVolumeDb" F32 [-96, 12]
	float m_fWallLossDb = -36.0f;                    // property 8 "WallLossDb" F32 [-96, 0]: loss for sources in another zone without a portal path
	float m_fWallCutoffHz = 1000.0f;                 // property 9 "WallCutoffHz" F32 [100, 20000]
	bool m_bOutdoor = false;                         // property 10 "Outdoor" BOOL: outdoor zones apply no wall loss between each other
};
```

### Zenith_AudioPortalComponent

```cpp
/// @contract component Zenith_AudioPortalComponent ("Zenith.Audio.Portal", version 1)
/// Ownership: An ECS row; a rectangle in the entity's local XY plane centred on its origin, normal +Z.
/// Thread affinity: ECS rules.
/// Invalidation: ECS rules.
/// Units: metres; m_fOpenness in [0, 1].
/// Errors: A portal naming an entity without a zone component is ignored and counted.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AudioPortalComponent
{
	float m_fHalfWidth = 0.5f;                       // property 1 "HalfWidth" F32
	float m_fHalfHeight = 1.0f;                      // property 2 "HalfHeight" F32
	Zenith_Entity m_xZoneA;                          // property 3 "ZoneA" ENTITY
	Zenith_Entity m_xZoneB;                          // property 4 "ZoneB" ENTITY (null = the outdoor or default zone)
	float m_fOpenness = 1.0f;                        // property 5 "Openness" F32 [0, 1]: a door; 0 behaves as a wall
};
```

### Zenith_AudioOccluderComponent

```cpp
/// @contract component Zenith_AudioOccluderComponent ("Zenith.Audio.Occluder", version 1)
/// Ownership: An ECS row; an oriented box in the entity's local space.
/// Thread affinity: ECS rules.
/// Invalidation: ECS rules.
/// Units: metres, dB, Hz.
/// Errors: Values outside the ranges are clamped and counted.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_AudioOccluderComponent
{
	Zenith_Vector3 m_xHalfExtents = { 0.5f, 0.5f, 0.5f };   // property 1 "HalfExtents" VEC3
	float m_fTransmissionLossDb = -18.0f;            // property 2 "TransmissionLossDb" F32 [-60, 0]
	float m_fCutoffHz = 2000.0f;                     // property 3 "CutoffHz" F32 [100, 20000]
};
```

Occluders are AUDIO's own geometry so the module needs no physics dependency ([Z2-N-ARCH-074]); the editor's "generate occluders from colliders" action (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Bank and event editor") authors them from scene geometry.

### Component registration

```cpp
/// @contract function Zenith_RegisterAudioComponents
/// Ownership: None.
/// Thread affinity: MAIN_THREAD, startup step 6.
/// Invalidation: None.
/// Units: None.
/// Errors: Registration errors surface at Freeze (Zenith_TypeRegistry's rule); the function itself cannot fail.
/// Version: since Z2 Stage 3.
/// @endcontract
void Zenith_RegisterAudioComponents(Zenith_TypeRegistry& xTypes);
```

It registers the six components above with their properties, ranges (`Range`) and editor info (category "Audio"), plus the struct `Zenith.Audio.EventRef` used by the editor's event picker.

## Zenith_AudioCommands header

### Command kinds

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioCommands.h
#pragma once
#include "ZenithAudio/Zenith_AudioTypes.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithECS/Zenith_CommandKinds.h"      // Zenith_CommandKindTable
#include "ZenithECS/Zenith_CommandBuffer.h"

enum Zenith_AudioCommandKind : uint8         // index into Zenith_AudioCommandKinds; the registered names are in the table below
{
	AUDIO_COMMAND_POST_EVENT = 1, AUDIO_COMMAND_STOP_INSTANCE, AUDIO_COMMAND_STOP_ENTITY, AUDIO_COMMAND_SET_PARAMETER,
	AUDIO_COMMAND_SET_GLOBAL_PARAMETER, AUDIO_COMMAND_SET_MIX_SNAPSHOT, AUDIO_COMMAND_MUSIC_SET_STATE,
	AUDIO_COMMAND_MUSIC_STINGER, AUDIO_COMMAND_PLAY_DIALOGUE, AUDIO_COMMAND_PREPARE_DIALOGUE, AUDIO_COMMAND_SET_BUS_PAUSED,
	AUDIO_COMMAND_SEEK_INSTANCE, AUDIO_COMMAND_COUNT,
};
```

| Kind | Registered name | Payload struct (stable name) | Allowed phases | Stage |
|---|---|---|---|---|
| `AUDIO_COMMAND_POST_EVENT` | `Audio.PostEvent` | `Zenith_AudioPostEventPayload` (`Zenith.Audio.PostEventPayload`) | 1–10 | 3 |
| `AUDIO_COMMAND_STOP_INSTANCE` | `Audio.StopInstance` | `Zenith_AudioStopInstancePayload` | 1–10 | 3 |
| `AUDIO_COMMAND_STOP_ENTITY` | `Audio.StopEntity` | `Zenith_AudioStopEntityPayload` | 1–10 | 3 |
| `AUDIO_COMMAND_SET_PARAMETER` | `Audio.SetParameter` | `Zenith_AudioSetParameterPayload` | 1–10 | 3 |
| `AUDIO_COMMAND_SET_GLOBAL_PARAMETER` | `Audio.SetGlobalParameter` | `Zenith_AudioSetGlobalParameterPayload` | 1–10 | 3 |
| `AUDIO_COMMAND_SET_MIX_SNAPSHOT` | `Audio.SetMixSnapshot` | `Zenith_AudioSetMixSnapshotPayload` | 1–10 | 4 |
| `AUDIO_COMMAND_MUSIC_SET_STATE` | `Audio.MusicSetState` | `Zenith_AudioMusicSetStatePayload` | 1–10 | 4 |
| `AUDIO_COMMAND_MUSIC_STINGER` | `Audio.MusicStinger` | `Zenith_AudioMusicStingerPayload` | 1–10 | 4 |
| `AUDIO_COMMAND_PLAY_DIALOGUE` | `Audio.PlayDialogue` | `Zenith_AudioPlayDialoguePayload` | 1–10 | 4 |
| `AUDIO_COMMAND_PREPARE_DIALOGUE` | `Audio.PrepareDialogue` | `Zenith_AudioPrepareDialoguePayload` | 1–10 | 4 |
| `AUDIO_COMMAND_SET_BUS_PAUSED` | `Audio.SetBusPaused` | `Zenith_AudioSetBusPausedPayload` | 1–10 | 3 |
| `AUDIO_COMMAND_SEEK_INSTANCE` | `Audio.SeekInstance` | `Zenith_AudioSeekInstancePayload` | 1–10 | 4 |

Every kind is registered by `Zenith_Module_Audio_Start` through `Zenith_ModuleStartContext::RegisterCommandKind` with `m_uMaxPayloadBytes = 64`, `m_uAllowedPhaseMask = 0x7FE` (bits 1–10), the payload's registered struct type, `m_pUserData` = the `Zenith_AudioSystem`, and AUDIO's playback function, which validates the payload (every enum and range, as untrusted data), finds the world's queue in AUDIO's world table by `Zenith_WorldPlaybackAccess::GetWorldTag()` ([Z2-N-ARCH-716]; `Zenith2/Docs/Design/AUDIO.md` → "Startup and attach"; a tag with no audio world, or an audio-disabled one, drops the command and counts `audio.commands.no_world_delta`) and appends one `Zenith_AudioQueuedCommand`. A payload that fails validation is dropped and counted (`audio.commands.invalid_delta`), never asserted.

Command kinds serve code that links ZenithAudio (AUDIO's own systems, tools, tests, the runtime's Luau bindings). Other modules never include this header ([Z2-N-ARCH-074]); they reach AUDIO through components, bridged ECS events (`Zenith2/Docs/Design/AUDIO/Authoring.md` → "Event bridges") or Luau.

### Command payloads

Every payload starts with the 16-byte header and carries at most 48 bytes after it:

```cpp
struct Zenith_AudioCommandHeader              // "Zenith.Audio.CommandHeader"
{
	uint64 m_ulTick = 0;                     // the recording tick (Zenith_SystemContext::GetTickIndex)
	float m_fTimeIntoTick = 0.0f;             // seconds into that tick; 0 when the recorder has no sub-tick time
	uint16 m_uFlags = 0;                     // kind-specific flags below
	uint16 m_uReserved = 0;
};
inline constexpr uint16 uAUDIO_POST_FLAG_FOLLOW_ENTITY = 1u << 0;   // position from m_xEntity's transform every frame
inline constexpr uint16 uAUDIO_POST_FLAG_AT_POSITION = 1u << 1;     // fixed world position m_xPosition
inline constexpr uint16 uAUDIO_POST_FLAG_TIME_LOCKED = 1u << 2;     // Mixer.md "Time-locked voices"
inline constexpr uint16 uAUDIO_POST_FLAG_SCHEDULE_SUBTICK = 1u << 3;  // honour m_fTimeIntoTick (Mixer.md "Scheduling a trigger")

struct Zenith_AudioPostEventPayload           // "Zenith.Audio.PostEventPayload", 64 bytes
{
	Zenith_AudioCommandHeader m_xHeader;
	Zenith_AudioEventID m_xEvent;
	Zenith_AudioInstanceID m_xInstance;       // 0: anonymous (cannot be stopped or parameterised individually; seeded at drain, Mixer.md "Event node evaluation")
	Zenith_EntityHandle m_xEntity;            // the entity the instance follows or stops with; null none (8 bytes, reflected as ENTITY)
	Zenith_Vector3 m_xPosition;               // world position with uAUDIO_POST_FLAG_AT_POSITION
	float m_fVolumeDb = 0.0f;                 // [-96, 12]
	float m_fPitchCents = 0.0f;               // [-2400, 2400]
	uint32 m_uReserved = 0;
};
struct Zenith_AudioStopInstancePayload { Zenith_AudioCommandHeader m_xHeader; Zenith_AudioInstanceID m_xInstance; uint32 m_uFadeMs = 50; Zenith_AudioStopMode m_eMode = AUDIO_STOP_MODE_FADE; };
struct Zenith_AudioStopEntityPayload { Zenith_AudioCommandHeader m_xHeader; Zenith_EntityHandle m_xEntity; uint32 m_uFadeMs = 50; };
struct Zenith_AudioSetParameterPayload { Zenith_AudioCommandHeader m_xHeader; Zenith_AudioInstanceID m_xInstance; Zenith_AudioParameterID m_xParameter; float m_fValue = 0.0f; uint32 m_uRampMs = 0; };
struct Zenith_AudioSetGlobalParameterPayload { Zenith_AudioCommandHeader m_xHeader; Zenith_AudioParameterID m_xParameter; float m_fValue = 0.0f; uint32 m_uRampMs = 0; };
struct Zenith_AudioSetMixSnapshotPayload { Zenith_AudioCommandHeader m_xHeader; Zenith_AudioMixSnapshotID m_xSnapshot; uint32 m_uFadeMs = 0; bool m_bActive = true; };
struct Zenith_AudioMusicSetStatePayload { Zenith_AudioCommandHeader m_xHeader; Zenith_AudioMusicStateID m_xState; bool m_bImmediate = false; };
struct Zenith_AudioMusicStingerPayload { Zenith_AudioCommandHeader m_xHeader; Zenith_AudioEventID m_xStinger; };
struct Zenith_AudioPlayDialoguePayload { Zenith_AudioCommandHeader m_xHeader; Zenith_AudioDialogueLineID m_xLine; Zenith_AudioInstanceID m_xInstance; Zenith_EntityHandle m_xEntity; };
struct Zenith_AudioPrepareDialoguePayload { Zenith_AudioCommandHeader m_xHeader; Zenith_AudioDialogueLineID m_xLine; };
struct Zenith_AudioSetBusPausedPayload { Zenith_AudioCommandHeader m_xHeader; Zenith_AudioBusID m_xBus; bool m_bPaused = true; };
struct Zenith_AudioSeekInstancePayload { Zenith_AudioCommandHeader m_xHeader; Zenith_AudioInstanceID m_xInstance; uint32 m_uPositionMs = 0; };
static_assert(sizeof(Zenith_AudioPostEventPayload) == 64);   // and ≤ 64 for every other payload
```

Each payload is a registered `TYPE_KIND_STRUCT` (stable names `Zenith.Audio.<Name>Payload`), so command buffers serialise it by reflection and replays and saves never depend on the C++ layout (Z2-D-038). Entity references are `Zenith_EntityHandle` values, exactly what native recorders, script bindings and event bridges hold, so no recorder converts a handle to a simulation ID and `Audio.Update` resolves the handle directly (`Zenith2/Docs/Design/AUDIO.md` → "Frame-graph placement").

### Recording helpers

```cpp
/// @contract value Zenith_AudioCommandKinds
/// Ownership: Plain value; filled once per world at AttachWorld from Zenith_CommandKindTable::FindByName.
/// Thread affinity: IMMUTABLE after AttachWorld. Members non-const: default-constructed, then filled from the command
///   registry at AttachWorld.
/// Invalidation: None while the module is started.
/// Units: None.
/// Errors: A kind the table lacks is a programmer error (ZENITH_CHECK at attach), since AUDIO registered it.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AudioCommandKinds
{
	uint32 m_auKindIDs[AUDIO_COMMAND_COUNT] = {};    // index by Zenith_AudioCommandKind
};
Zenith_Result<Zenith_AudioCommandKinds, Zenith_AudioError> Zenith_AudioResolveCommandKinds(
	const Zenith_CommandKindTable& xTable);   // AUDIO_ERROR_INVALID_DESC naming the first missing kind in m_uIndex

/// @contract functions Zenith_AudioRecord
/// Ownership: None; the payload is copied into the command buffer.
/// Thread affinity: OWNER_THREAD: the recording system's invocation.
/// Invalidation: None.
/// Units: As the payloads.
/// Errors: A malformed payload from engine code is ZENITH_ASSERT (engine code is not untrusted); the command buffer's
///   own capacity rules apply (ECS's).
/// Version: since Z2 Stage 3.
/// @endcontract
void Zenith_AudioRecordPostEvent(Zenith_CommandBuffer& xCommands, const Zenith_AudioCommandKinds& xKinds,
	const Zenith_AudioPostEventPayload& xPayload);
void Zenith_AudioRecordStopInstance(Zenith_CommandBuffer& xCommands, const Zenith_AudioCommandKinds& xKinds,
	const Zenith_AudioStopInstancePayload& xPayload);
void Zenith_AudioRecordSetParameter(Zenith_CommandBuffer& xCommands, const Zenith_AudioCommandKinds& xKinds,
	const Zenith_AudioSetParameterPayload& xPayload);
// one helper per kind, same shape: Zenith_AudioRecord<KindName>(xCommands, xKinds, xPayload)
```

Each helper calls `xCommands.Record(xKinds.m_auKindIDs[kind], Zenith_ByteSpan of the payload)` (`Zenith2/Docs/Design/ECS/WorldAPI.md` → "Zenith_CommandKinds header").

## Zenith_AudioCommandQueue header

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioCommandQueue.h
#pragma once
#include "ZenithAudio/Zenith_AudioCommands.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"

struct Zenith_AudioQueuedCommand              // 64 bytes; Zenith2/Docs/Design/AUDIO.md "Command record layout"
{
	Zenith_AudioCommandKind m_eKind;
	uint8 m_uSource;                         // 0 native command, 1 script trigger, 2 event bridge, 3 audition
	uint16 m_uFlags;
	float m_fTimeIntoTick;
	uint64 m_ulTick;
	alignas(8) uint8 m_auBody[48];           // the payload after its header
};
static_assert(sizeof(Zenith_AudioQueuedCommand) == 64);

/// @contract type Zenith_AudioCommandQueue
/// Ownership: One per audio world, created by the module at AttachWorld and registered as the world resource
///   "Zenith.Audio.CommandQueue" (owning phase PHASE_COMMANDS_ONLY); destroyed at DetachWorld. One heap block of
///   capacity × 64 bytes at Create, nothing afterwards.
/// Thread affinity: WORLD_SERIAL: Append is called by the world's sync-point playback (command kinds) and by the world's
///   merge nodes (Zenith_AudioScriptConsumer); Drain by Audio.Update@W<tag>. The frame graph runs none of these
///   concurrently for one world (playback and merges are ordered within the world's tick chain, and the epilogue
///   follows the last tick); a debug build asserts the single-writer rule with an atomic owner flag.
/// Invalidation: The span Drain returns is valid until Clear.
/// Units: Commands.
/// Errors: Create returns AUDIO_ERROR_INVALID_DESC (capacity 0, above uAUDIO_COMMAND_QUEUE_CAPACITY_MAX or not a power of
///   two) or AUDIO_ERROR_OUT_OF_MEMORY. Append on a full queue drops the new command (QUEUE_FULL_POLICY_DROP_NEWEST) and
///   counts it; it never fails otherwise.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AudioCommandQueue
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_AudioCommandQueue>, Zenith_AudioError> Create(Zenith_Heap& xHeap,
		uint32 uCapacity, uint16 uWorldTag);
	~Zenith_AudioCommandQueue();
	bool Append(const Zenith_AudioQueuedCommand& xCommand);      // false: dropped (full)
	Zenith_Span<const Zenith_AudioQueuedCommand> Drain() const;  // every command since the last Clear, in append order
	void Clear();
	uint32 GetCapacity() const;
	uint32 GetHighWater() const;      // since Create
	uint64 GetDroppedCount() const;   // cumulative
	uint16 GetWorldTag() const;
};
```

The capacity is `audio.commands.capacity` (default 4096). The rule that sizes it: at least (Z2-B-SCRIPT-023's per-tick trigger capacity + the native per-tick estimate of the slice) × Z2-B-JOBS-002's catch-up tick bound, rounded up to a power of two; the Stage 1 ruling records the value in AUDIO's platform fragment once both rows are adopted.

## Zenith_AudioSystem header

### Zenith_AudioSettings

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioSystem.h
#pragma once
#include "ZenithAudio/Zenith_AudioAssets.h"
#include "ZenithAudio/Zenith_AudioCommandQueue.h"
#include "ZenithAudio/Zenith_AudioComponents.h"
#include "ZenithCore/Zenith_Quaternion.h"

class Zenith_AssetRegistry;
class Zenith_Config;
class Zenith_Platform;

/// @contract value Zenith_AudioSettings
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE once passed to Create. Members non-const: callers default-construct it and set the fields
///   they need before passing it.
/// Invalidation: None.
/// Units: As each member's comment.
/// Errors: Validated by Create (AUDIO_ERROR_INVALID_DESC, m_uIndex = the member's ordinal).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AudioSettings
{
	Zenith_AudioBackendKind m_eBackend = AUDIO_BACKEND_AUTO;
	Zenith_AudioOutputLayout m_eOutputLayout = AUDIO_OUTPUT_LAYOUT_STEREO;   // after "auto" resolution
	bool m_bOutputLayoutAuto = true;
	bool m_bDeviceRequired = false;
	bool m_bAndroidExclusive = false;
	bool m_bMuteWhenUnfocused = false;
	uint32 m_uPeriodFrames = 480;               // requested device period at 48 kHz; 0 = backend default
	uint32 m_uRealVoiceLimit = uAUDIO_MAX_GAME_REAL_VOICES;   // audio.voices.realMax: default and maximum 240 (the 16 audition slots are outside it)
	uint32 m_uSecondaryWorldRealVoiceLimit = 16;
	uint32 m_uInstanceLimitPerWorld = uAUDIO_MAX_INSTANCES_PER_WORLD;
	float m_fAudibilityThresholdDb = -60.0f;
	float m_fStealHysteresisDb = 3.0f;
	uint32 m_uMinRealMs = 100;
	uint32 m_uStreamLimit = uAUDIO_MAX_STREAMS;
	uint32 m_uStreamPrebufferFrames = 4096;      // the effective prebuffer: at most m_uStreamRingFrames − uAUDIO_MAX_PACKET_FRAMES
	uint32 m_uStreamRingFrames = 16384;          // a power of two in [16384, 65536]
	uint64 m_ulDecodedCacheBytes = 16ull << 20;
	uint32 m_uCommandQueueCapacity = 4096;
	uint32 m_uOcclusionRaysPerFrame = 64;
	bool m_bReverbHighQuality = true;
	float m_fAvSyncOffsetMs = 0.0f;
	float m_afUserVolumeDb[AUDIO_VOLUME_CATEGORY_COUNT] = {};   // index 0 unused
	Zenith_AssetGUID m_xMasterBank;               // null: the engine default master bank
	uint32 m_uCaptureSeconds = 0;                // development only; 0 = off
};

/// @contract function Zenith_AudioReadSettings
/// Ownership: None.
/// Thread affinity: MAIN_THREAD (Zenith_Config is).
/// Invalidation: None.
/// Units: None.
/// Errors: None: every key has a registered default and range, so the store always yields a valid value; the one cross-key
///   rule the schema cannot express (the prebuffer must fit the ring's guaranteed fill) is applied by clamping, below.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_AudioSettings Zenith_AudioReadSettings(const Zenith_Config& xConfig, bool bHeadless);
```

`bHeadless` forces `m_eBackend = AUDIO_BACKEND_NULL_PACER` and `m_bDeviceRequired = false` ([Z2-N-ARCH-867]).

The prebuffer must fit the ring. Refill stops when fewer than P frames of room remain (P the sound's packet frames, at most `uAUDIO_MAX_PACKET_FRAMES` = 8,192; `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream refill algorithm" step 2), so a ring of F frames is guaranteed to hold only F − P frames, and a prebuffer above F − 8,192 might never complete: every streamed start would then time out at 2,000 ms. `Zenith_AudioReadSettings` therefore sets `m_uStreamPrebufferFrames` = min(`audio.streams.prebufferFrames`, `audio.streams.ringFrames` − 8,192) and, when that clamps, logs one `audio.stream` warning naming both keys and the effective value; the clamp is a property of the start-time configuration, not an event, so it has no counter (the frame dump's `audio` member reports the effective value). With F ≥ 16,384 (the key's range) the clamped value is ≥ 8,192, above the 2,048-frame minimum. `Create` rejects a desc whose prebuffer exceeds `m_uStreamRingFrames` − 8,192 with `AUDIO_ERROR_INVALID_DESC` (`m_uIndex` = the prebuffer member's ordinal), because only a test or a tool builds settings without `Zenith_AudioReadSettings`. `Test_AudioConfig_PrebufferClampedToRingFill` sets the prebuffer to 16,384 with a 16,384-frame ring, asserts the effective 8,192, the one warning, and a STREAMED codec fixture whose header declares the maximum 8,192 packet frames reaching PLAYING without a start timeout (the scripted read source serving it), and asserts that a hand-built desc with 16,384 is rejected.

### Zenith_AudioSystemDesc

```cpp
struct Zenith_AudioSystemDesc
{
	Zenith_Heap* m_pxHeap = nullptr;                   // the root heap or a test heap; every AUDIO allocation goes here
	Zenith_AssetRegistry* m_pxAssets = nullptr;        // null only for unit tests that load nothing
	Zenith_Platform* m_pxPlatform = nullptr;           // null in unit tests and on the offline backend (no locale, no JNI)
	Zenith_AudioSettings m_xSettings;
	Zenith_FaultInjector* m_pxFaults = nullptr;        // tests: the creation fault points of "Creation steps"
};
```

### Zenith_AudioSystem

```cpp
/// @contract type Zenith_AudioSystem
/// Ownership: Created by Create as Zenith_Owned<Zenith_AudioSystem>; owned by Zenith_AudioModule (or a test or tool).
///   Owns the device backend, the control, decode and null-pacer threads, the mixer, voices, streams and caches.
/// Thread affinity: MAIN_THREAD for Create, Start, Stop, CreateWorld, DestroyWorld, the audition and override members,
///   ApplyMixerLayout, the lifecycle and setting members, GetVoiceInspector and the destructor; the members that change the
///   mix reach Audio.Mix only through the main request ring and the main-state triple buffer (AUDIO.md "Main thread to
///   Audio.Mix channels"), so they may run while the EPILOGUE subgraph runs. UpdateWorld is ANY_THREAD with one call per world per frame (Audio.Update@W<tag>), concurrent across
///   worlds; RunMix is ANY_THREAD once per frame after every UpdateWorld of the frame (Audio.Mix). FindEvent is callable
///   from AUDIO's epilogue nodes and from the main thread outside the EPILOGUE subgraph. GetStatus, GetMeters and
///   GetCounters are ANY_THREAD.
/// Invalidation: World handles are invalid after DestroyWorld; the system after its destructor.
/// Units: Frames at 48 kHz, milliseconds, dB.
/// Errors: Zenith_Result<…, Zenith_AudioError>; see each member.
/// Reentrancy: NONE. No member calls back into caller code; loaders, the script consumer and the memory consumer are
///   called by their owners and touch only AUDIO state.
/// Version: since Z2 Stage 3. Source API.
/// Example: Zenith2/Engine/ZenithAudio/Tests/Examples/Zenith_AudioSystem_Example.cpp
/// @endcontract
class Zenith_AudioSystem
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_AudioSystem>, Zenith_AudioError> Create(const Zenith_AudioSystemDesc& xDesc);
	~Zenith_AudioSystem();           // Stop if running; joins every AUDIO thread; frees everything

	Zenith_Result<void, Zenith_AudioError> Start();   // opens the device (asynchronously unless required)
	void Stop();                                                     // closes the device; the mixer stops advancing

	// Kind loaders for Zenith_AssetRegistry::RegisterKindLoader (Streaming.md "Sound and bank loaders").
	Zenith_AssetKindLoader& GetSoundLoader();
	Zenith_AssetKindLoader& GetBankLoader();
	Zenith_MemoryConsumer& GetMemoryConsumer();
	Zenith_Result<void, Zenith_AudioError> LoadMasterBank(uint32 uTimeoutMs);   // MAIN_THREAD, outside frames

	// Audio worlds.
	Zenith_Result<Zenith_AudioWorldHandle, Zenith_AudioError> CreateWorld(uint16 uWorldTag);  // AUDIO_ERROR_CAPACITY above 8
	void DestroyWorld(Zenith_AudioWorldHandle xWorld);
	Zenith_Result<void, Zenith_AudioError> UpdateWorld(Zenith_AudioWorldHandle xWorld, const Zenith_AudioWorldFrame& xFrame);   // U1–U7, this world only
	void RunMix(const Zenith_AudioMixFrame& xFrame);                                                                   // M1–M11, every world

	// Lifecycle (forwarded from the frame listener; MAIN_THREAD; never block).
	void OnStopAudio();
	void OnResumeAudio();
	void OnWindowFocusChanged(bool bFocused);            // audio.muteWhenUnfocused
	void OnUserVolumeChanged(Zenith_AudioVolumeCategory eCategory, float fVolumeDb);   // config user-change callback

	// Queries.
	Zenith_AudioStatus GetStatus() const;
	Zenith_AudioMeters GetMeters() const;
	Zenith_AudioCounters GetCounters() const;
	const Zenith_AudioEventInfo* FindEvent(Zenith_AudioEventID xEvent) const;   // valid until the second Audio.Mix after the next index swap

	// Audition and live mixing (editor; Stage 4). MAIN_THREAD.
	Zenith_Result<Zenith_AudioInstanceID, Zenith_AudioError> Audition(const Zenith_AudioAuditionDesc& xDesc);
	void StopAudition(Zenith_AudioInstanceID xInstance, uint32 uFadeMs);   // pushes at most once per active slot; else a no-op
	void StopAllAuditions();
	void SetAuditionParameter(Zenith_AudioInstanceID xInstance, Zenith_AudioParameterID xParameter, float fValue);
	void SetAuditionPosition(Zenith_AudioInstanceID xInstance, const Zenith_Vector3& xPosition);   // context world's space, or relative to the virtual listener
	void SetBusOverride(Zenith_AudioBusID xBus, const Zenith_AudioBusOverride& xOverride);   // mute, solo, volume trim
	void ClearBusOverrides();
	Zenith_Result<void, Zenith_AudioError> ApplyMixerLayout(Zenith_ByteSpan xCookedMixerSection);  // live edit
	void SetVoiceInspectorEnabled(bool bEnabled);
	uint32 GetVoiceInspector(Zenith_MutableSpan<Zenith_AudioVoiceInfo> axOut);  // MAIN_THREAD: the inspector triple buffer's one consumer

	// Development capture.
	Zenith_Result<void, Zenith_AudioError> BeginCapture(uint32 uSeconds);
	Zenith_Result<void, Zenith_AudioError> EndCapture(const Zenith_VirtualPath& xUserPath);
};
```

### World frame inputs

`UpdateWorld` takes the world's state as plain data, so the ECS stage function and the tests feed the same core:

```cpp
struct Zenith_AudioWorldHandle { uint16 m_uIndex = 0xFFFFu; uint16 m_uGeneration = 0; };

struct Zenith_AudioListenerState
{
	bool m_bValid = false;                    // false: no active listener; positional voices go virtual
	Zenith_Vector3 m_xPosition;               // listener (camera) position, world space
	Zenith_Quaternion m_xRotation;            // listener orientation; view forward −Z
	Zenith_Vector3 m_xVelocity;               // metres per second: (current tick − previous tick position) / tick duration, constant between ticks
	Zenith_Vector3 m_xAttenuationPosition;    // m_xPosition blended toward the attenuation entity
};
struct Zenith_AudioEmitterState
{
	uint64 m_ulSimulationID = 0;
	Zenith_Vector3 m_xPosition; Zenith_Quaternion m_xRotation;   // interpolated with the frame's alpha (Mixer.md "Listener space")
	Zenith_Vector3 m_xVelocity;                                  // tick-to-tick difference over the tick duration
	bool m_bTeleported = false;               // WORLD_TRANSFORM_FLAG_TELEPORTED this frame: Doppler and smoothing reset
	bool m_bRegionActive = true;              // false while the region is not ACTIVE (Streaming.md "Region-bound emitters")
	const Zenith_AudioEmitterComponent* m_pxEmitter = nullptr;   // valid for the call
};
struct Zenith_AudioZoneState { uint64 m_ulSimulationID; Zenith_Matrix4 m_xWorldFromLocal; const Zenith_AudioZoneComponent* m_pxZone; };
struct Zenith_AudioPortalState { uint64 m_ulSimulationID; Zenith_Matrix4 m_xWorldFromLocal; uint64 m_ulZoneA, m_ulZoneB; const Zenith_AudioPortalComponent* m_pxPortal; };
struct Zenith_AudioOccluderState { Zenith_Matrix4 m_xWorldFromLocal; const Zenith_AudioOccluderComponent* m_pxOccluder; };
struct Zenith_AudioEntityPosition { Zenith_EntityHandle m_xEntity; Zenith_Vector3 m_xPosition; Zenith_Vector3 m_xVelocity; bool m_bTeleported; bool m_bResolved; };   // m_bResolved false: the entity vanished

/// @contract value Zenith_AudioWorldFrame
/// Ownership: Borrowed spans; valid for the UpdateWorld call.
/// Thread affinity: The calling thread.
/// Invalidation: After the call.
/// Units: Seconds, metres, metres per second.
/// Errors: UpdateWorld validates counts against the capacities (AUDIO_ERROR_CAPACITY counts and truncates, never fails the frame).
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AudioWorldFrame
{
	Zenith_AudioListenerState m_xListener;
	Zenith_Span<const Zenith_AudioEmitterState> m_axEmitters;        // sorted by simulation ID (the stage sorts)
	Zenith_Span<const Zenith_AudioEntityPosition> m_axFollowed;      // entities instances follow, sorted by handle bits
	Zenith_Span<const Zenith_AudioZoneState> m_axZones;
	Zenith_Span<const Zenith_AudioPortalState> m_axPortals;
	Zenith_Span<const Zenith_AudioOccluderState> m_axOccluders;
	Zenith_AudioCommandQueue* m_pxCommands = nullptr;                // drained and cleared by the call
	double m_dFrameSeconds = 0.0;             // presentation time since the previous frame (wall clock, clamped to [0, 0.25])
	uint64 m_ulLastTick = 0;                 // the world's tick index after this frame's ticks
	float m_fTickSeconds = 0.0f;              // fixed tick duration
	float m_fInterpolationAlpha = 0.0f;       // the render interpolation alpha (CPUFrameGraph.md "Simulation clock")
	bool m_bPaused = false;                   // the world is paused: its non-UI voices pause
	float m_fTimeScale = 1.0f;                // simulation time scale: pitch of time-scaled buses (Mixer.md "Pause and time scale")
	uint32 m_uSessionOrdinal = 0;            // Zenith.Runtime.WorldState SessionOrdinal; Audio.Mix makes the highest the primary world
	uint64 m_ulFrameIndex = 0;               // the frame index the record belongs to
};

/// @contract value Zenith_AudioMixFrame
/// Ownership: Plain value.
/// Thread affinity: The calling thread (Audio.Mix).
/// Invalidation: None.
/// Units: Seconds; frames.
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AudioMixFrame
{
	uint64 m_ulFrameIndex = 0;               // worlds whose record carries this index were updated this frame
	double m_dFrameSeconds = 0.0;             // as Zenith_AudioWorldFrame; advances the audition world
};
```

The primary world is not set by the caller: `Audio.Mix` derives it each frame from the updated worlds' session ordinals (the world host's current world has the highest ordinal, `Zenith2/Docs/Design/GAME/WorldHost.md` → "World state resource"), ties to the lowest world tag.

### Status and meters

```cpp
struct Zenith_AudioStatus
{
	Zenith_AudioDeviceState m_eDeviceState = AUDIO_DEVICE_STATE_CLOSED;
	Zenith_AudioFocusState m_eFocus = AUDIO_FOCUS_STATE_GAINED;
	bool m_bFocusLostPermanently = false;    // Android: INTERRUPTED after AUDIOFOCUS_LOSS (Device.md "Audio focus")
	Zenith_AudioBackendKind m_eBackend = AUDIO_BACKEND_AUTO;
	Zenith_AudioOutputLayout m_eLayout = AUDIO_OUTPUT_LAYOUT_STEREO;
	uint32 m_uDeviceSampleRate = 0;          // the device's rate; the mixer is always 48 kHz (miniaudio resamples otherwise)
	uint32 m_uDeviceChannels = 0;
	uint32 m_uPeriodFrames = 0;
	uint32 m_uLatencyUs = 0;
	uint32 m_uRealVoices = 0, m_uVirtualVoices = 0, m_uStreams = 0;
	uint64 m_ulResidentBytes = 0;
	uint64 m_ulDeviceFrame = 0;              // frames mixed since Start
	char m_acDeviceName[64] = {};             // UTF-8, truncated
};
struct Zenith_AudioBusMeter { Zenith_AudioBusID m_xBus; float m_fPeakDb[2]; float m_fRmsDb[2]; };   // left/right (or the first two channels)
struct Zenith_AudioMeters
{
	Zenith_AudioBusMeter m_axBuses[uAUDIO_MAX_BUSES];
	uint32 m_uBusCount = 0;
	float m_fMomentaryLufs = -144.0f;         // master, 400 ms window (EBU R128 momentary)
	float m_fShortTermLufs = -144.0f;         // master, 3 s window
	float m_fLimiterReductionDb = 0.0f;
};
struct Zenith_AudioCounters { uint64 m_aulValues[uAUDIO_COUNTER_COUNT]; };   // the counters of AUDIO.md "Counters", in table order
struct Zenith_AudioVoiceInfo
{
	Zenith_AudioInstanceID m_xInstance; Zenith_AudioEventID m_xEvent; Zenith_AudioBusID m_xBus;
	uint16 m_uWorldTag; uint8 m_uPriority; bool m_bReal; bool m_bStreamed;
	float m_fAudibilityDb; float m_fDistance; float m_fOcclusionDb; uint64 m_ulPlayFrame;
};
```

Meters are computed by the device thread every 4th block (peak and RMS over the 1,024 frames) and published in a meter frame through their own triple buffer, never through the feedback ring (`Zenith2/Docs/Design/AUDIO/Formats.md` → "Meter frame record"); `Audio.Mix` M1 acquires the latest frame and smooths it on the control side (peak hold 1.5 s, fall 20 dB/s). With 64 buses the meters were 16 records per block, so a main-thread stall of about 300 ms (56 blocks) filled the 1,024-record feedback ring and dropped marker records; out of the ring, a stall drops at most superseded meter frames (`audio.meters.dropped`). `Test_AudioRtChannels_MeterFloodKeepsMarkers` stalls `Audio.Mix` for 300 ms of 96-frame callbacks with 64 buses metering and one voice crossing a marker, and asserts that the marker record is delivered, `audio.feedback.dropped` is 0 and the superseded meter frames are counted.

### Audition API

```cpp
struct Zenith_AudioAuditionDesc
{
	Zenith_AudioEventID m_xEvent;
	Zenith_AssetHandle<Zenith_SoundBankAsset> m_xBank;    // the bank under edit; may differ from the loaded one (a preview cook)
	Zenith_AssetHandle<Zenith_SoundAsset> m_xSound;       // alternatively, a raw sound (m_xEvent null)
	bool m_bPositional = false;               // false: 2D audition on the "Audition" bus
	Zenith_Vector3 m_xPosition;               // with m_bPositional: in the context world's space, else relative to a virtual listener at the origin facing −Z
	uint16 m_uContextWorldTag = 0xFFFFu;     // the world whose listener spatialises the audition (audition in context); 0xFFFF none
	float m_afParameterValues[uAUDIO_MAX_INSTANCE_PARAMETERS] = {};
	Zenith_AudioParameterID m_axParameterIDs[uAUDIO_MAX_INSTANCE_PARAMETERS] = {};
	bool m_bLoop = false;                     // force loop for sounds
};
struct Zenith_AudioBusOverride { bool m_bMute = false; bool m_bSolo = false; float m_fTrimDb = 0.0f; };
```

Audition instances live in the internal audition world (world tag `uAUDIO_AUDITION_WORLD_TAG`), which `Audio.Mix` advances itself (step M3) from the main request ring and the main-state triple buffer (`Zenith2/Docs/Design/AUDIO.md` → "Main thread to Audio.Mix channels": AUDITION_START, AUDITION_STOP, AUDITION_STOP_ALL, and the audition parameters of the main state; at most `uAUDIO_MAX_AUDITIONS` auditions at once, a 17th `Audition` returns `AUDIO_ERROR_CAPACITY`), so it needs no frame-graph node of its own; it owns the `uAUDIO_AUDITION_REAL_VOICES` reserved device slots and is mixed into the `Audition` bus that bypasses mix snapshots and ducking; audition works while no game world exists and, in play-in-editor, beside the game world ("audition in context", `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Audition in context"). A positional audition with `m_uContextWorldTag` naming an attached world is spatialised against that world's listener whenever the world was updated in the frame, and against the virtual origin listener otherwise (`Zenith2/Docs/Design/AUDIO.md` → "Audio.Mix step by step" M3); `SetAuditionPosition` (MAIN_THREAD, through the main state, a no-op for an unknown or ended instance) moves it each frame. The context changes only the listener: the voice stays in the audition world, on its reserved slots and the `Audition` bus, so it is never captured into game logic.

### Creation steps

`Create` performs, in order, each with a fault point `uFAULT_POINT_AUDIO_CREATE_<STEP>` (`0x41550001` + step index): (1) validate the desc; (2) allocate the system block, the audition world's tables, the device voice and slot-ownership tables (256), the sound and bank record pools (`uAUDIO_MAX_SOUND_RECORDS` + `uAUDIO_MAX_BANKS`), the loader hand-off queue, the snapshot triple buffer's three slots, the feedback ring, the meter-frame triple buffer's three slots (3 × 1,040 bytes), the main request ring (`uAUDIO_MAIN_REQUEST_CAPACITY` × 128 bytes) and the main-state triple buffer's three slots (3 × 2,304 bytes), the decode request ring (`uAUDIO_DECODE_REQUEST_CAPACITY` × 64 bytes) and the decode event ring (`uAUDIO_DECODE_EVENT_CAPACITY` × 32 bytes), the stream slot table (each slot's ring, N = the smallest power of two ≥ `m_uStreamRingFrames` × channels samples, is allocated when the slot opens, `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Stream slots") and the decoded-sample cache arena; (3) build the built-in master-only mixer layout (both its `Zenith_AudioRtLayout` and its `Zenith_AudioControlLayout`), then write the empty snapshot into all three slots of the snapshot triple buffer (epoch 0, voice count 0, bus count = the built-in layout's, master and category gain targets 1.0, layout pointer = the built-in real-time layout, every other byte 0) the initial main state into all three main-state slots (sequence 0, user volumes from the settings, no override) and a meter frame with bus count 0 into all three meter slots, so the device, which may start before the first `Audio.Mix` (start step 11), reads a valid snapshot from its first block and treats epoch 0 as "no snapshot yet" (it starts no voice and renders silence); (4) create the decode thread (skipped for `AUDIO_BACKEND_OFFLINE`, whose stream servicing runs on the owner thread, `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing"); (5) create the control thread; (6) create the backend object (null pacer, offline, or miniaudio wrapper; miniaudio's context is initialised by the control thread at `Start`, not here). Game worlds' voice and instance tables are allocated by `CreateWorld` (attach time). A failure at step k destroys steps k−1 … 1 in reverse and returns the step's error; `Test_AudioSystem_Create_FailsAtEachStep_Unwinds` asserts every step with the test heap's statistics showing zero live allocations.

## Zenith_AudioOfflineRenderer header

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioOfflineRenderer.h
#pragma once
#include "ZenithAudio/Zenith_AudioSystem.h"

/// @contract type Zenith_AudioOfflineRenderer
/// Ownership: Zenith_Owned from Create; borrows a Zenith_AudioSystem created with AUDIO_BACKEND_OFFLINE; destroyed first.
/// Thread affinity: OWNER_THREAD: a thread that is not a job worker (the main thread outside frames, or a test or tool
///   thread). Per frame, in this order: ServiceBeforeFrame; the frame's UpdateWorld and RunMix (directly, or through the frame
///   graph's Audio.Update and Audio.Mix nodes); Render for that frame's frames.
/// Invalidation: None.
/// Units: Frames at 48 kHz; interleaved float samples in the output layout's channel order.
/// Errors: Create returns AUDIO_ERROR_INVALID_DESC when the system's backend is not OFFLINE; Render returns
///   AUDIO_ERROR_INVALID_DESC when the span is smaller than uFrames × channels.
/// Version: since Z2 Stage 3.
/// @endcontract
class Zenith_AudioOfflineRenderer
{
public:
	static Zenith_Result<Zenith_Owned<Zenith_AudioOfflineRenderer>, Zenith_AudioError> Create(Zenith_AudioSystem& xSystem);
	// Completes, on the calling thread, every asset load AUDIO requested, every stream slot's outstanding read and refill and
	// every pending cache fill (AUDIO.md "Bit-exact offline mixing" step 2), each wait bounded by 5,000 ms. Returns
	// AUDIO_ERROR_INVALID_DESC on a job worker thread; a timeout ends the affected voice and clears IsDeterministic.
	Zenith_Result<void, Zenith_AudioError> ServiceBeforeFrame();
	bool IsDeterministic() const;   // false once any servicing wait timed out or failed
	// Renders uFrames frames by calling the mixer exactly as the device callback does (block FIFO included).
	Zenith_Result<void, Zenith_AudioError> Render(uint32 uFrames, Zenith_MutableSpan<float> axInterleaved);
	// Frames a simulation tick of fTickSeconds corresponds to, carrying the remainder: round-half-even of the exact
	// accumulated product, so 1,000 ticks at 60 Hz render exactly 800,000 frames.
	uint32 FramesForNextTick(float fTickSeconds);
	uint64 GetOutputHash() const;   // Zenith_Hash64Stream over every rendered sample's bits
	uint64 GetFramesRendered() const;
};
```

The offline backend never starts a thread that calls the mixer; the device state is `AUDIO_DEVICE_STATE_OFFLINE`. The editor's "render to file" action and the bit-exact tests use it (`Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing").

## Zenith_AudioStreamDecoder header

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioStreamDecoder.h
#pragma once
#include "ZenithAudio/Zenith_AudioTypes.h"
#include "ZenithCore/Zenith_Heap.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"

struct Zenith_AudioDecoderDesc
{
	Zenith_AudioCodec m_eCodec = AUDIO_CODEC_PCM16;
	uint32 m_uSampleRate = uAUDIO_MIX_SAMPLE_RATE;
	uint8 m_uChannels = 2;
	Zenith_ByteSpan m_xCodecSetup;            // the sound's codec setup bytes (Formats.md "Compressed codec section"); empty for PCM16 and ADPCM
	Zenith_Heap* m_pxHeap = nullptr;          // the decoder's state block (and the codec library's allocations, routed here)
};
struct Zenith_AudioDecodeResult
{
	uint32 m_uConsumedBytes = 0;             // whole packets (or ADPCM blocks) consumed
	uint32 m_uFramesWritten = 0;
	bool m_bNeedMoreInput = false;            // the input ended inside a packet
};

/// @contract interface Zenith_AudioStreamDecoder
/// Ownership: Zenith_Owned from Zenith_CreateAudioStreamDecoder; owned by one stream slot or one cache-fill job.
/// Thread affinity: OWNER_THREAD: a stream slot's decoder is created by Audio.Mix (M7) and used only by the decode thread
///   from the OPEN request to the CLOSED event (the decode rings hand it over), then destroyed by Audio.Mix's retirement;
///   a cache-fill job's decoder lives on the decode thread alone (Device.md "Audio.Mix and decode thread channels").
/// Invalidation: None.
/// Units: Frames; bytes; output interleaved int16 in the sound's channel order.
/// Errors: Decode returns AUDIO_ERROR_DECODE for a packet the codec rejects (untrusted data: never an assert) and
///   AUDIO_ERROR_INVALID_DESC when the output span is not a multiple of the channel count. Reset returns AUDIO_ERROR_DECODE
///   when the setup bytes are invalid.
/// Reentrancy: NONE. No callback.
/// Version: since Z2 Stage 3. The codec implementation is selected by D-SEL-CODEC (open, Stage 2).
/// @endcontract
class Zenith_AudioStreamDecoder
{
public:
	virtual ~Zenith_AudioStreamDecoder() = default;
	virtual Zenith_Result<Zenith_AudioDecodeResult, Zenith_AudioError> Decode(Zenith_ByteSpan xInput,
		Zenith_MutableSpan<sint16> axOut) = 0;
	// Discards decoder state after a seek: the next packet decoded starts a new run; the first uDiscardFrames frames it
	// produces are dropped (codec pre-roll from the seek table entry).
	virtual Zenith_Result<void, Zenith_AudioError> Reset(uint32 uDiscardFrames) = 0;
	virtual uint32 GetCodecDelayFrames() const = 0;   // frames the codec emits before frame 0 (pre-skip)
};

/// @contract function Zenith_CreateAudioStreamDecoder
/// Ownership: Returns an owned decoder allocated from xDesc.m_pxHeap.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: AUDIO_ERROR_UNSUPPORTED_FORMAT for a codec this build does not decode (the unruled D-SEL-CODEC candidate,
///   channels > 8, rate other than 48,000 or 24,000); AUDIO_ERROR_OUT_OF_MEMORY; AUDIO_ERROR_DECODE for bad setup bytes.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_AudioStreamDecoder>, Zenith_AudioError> Zenith_CreateAudioStreamDecoder(
	const Zenith_AudioDecoderDesc& xDesc);
```

`Zenith_AudioStreamDecoder` is the D-SEL-CODEC noun (`Zenith2/Docs/DecisionLog.md` → "D-SEL-CODEC"); it is an interface because the ruling picks its implementation, and three implementations exist (`Internal/Decode/`): PCM16 (a copy), IMA ADPCM (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "IMA ADPCM decoding") and the ruled codec.

## Zenith_AudioScriptApi header

### Script value types

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioScriptApi.h
#pragma once
#include "ZenithAudio/Zenith_AudioTypes.h"
#include "ZenithReflect/Zenith_ScriptBinding.h"

struct Zenith_AudioEventRef { uint32 m_uLow = 0; uint32 m_uHigh = 0; };          // "Zenith.Audio.EventRef" → Luau "AudioEvent"
struct Zenith_AudioInstanceRef { uint32 m_uLow = 0; uint32 m_uHigh = 0; };       // "Zenith.Audio.InstanceRef" → Luau "AudioInstance"
struct Zenith_AudioLineRef { uint32 m_uLow = 0; uint32 m_uHigh = 0; };           // "Zenith.Audio.LineRef" → Luau "AudioLine"
```

The three value structs are reflected `TYPE_KIND_STRUCT`s with two `U32` properties each (Luau numbers are doubles, so a 64-bit ID is never exposed as one number) and are exposed with `AddValueType<T>` as value userdata with `==` and `tostring` (`"AudioEvent(0x…)"`). Bus, parameter, snapshot and music-state IDs are 32-bit and exposed as plain numbers.

### Binding registration

```cpp
/// @contract function Zenith_RegisterAudioScriptBindings
/// Ownership: xTable is owned by the module instance; xConsumer by the audio system; both outlive every world.
/// Thread affinity: MAIN_THREAD, inside Zenith_Module_Audio_Start (step 15), before the composition root seals the table.
/// Invalidation: None.
/// Units: None.
/// Errors: Cannot fail; descriptor errors surface at Seal as Zenith_ScriptBindingError (exit code 3).
/// Version: since Z2 Stage 3.
/// @endcontract
void Zenith_RegisterAudioScriptBindings(Zenith_ScriptBindingTable& xTable);
```

The function adds the functions of `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Function table" under the namespace `Audio` (Luau `Zenith.Audio`), the three value types, and the enums `Zenith.Audio.StopMode`; the start function then calls `xContext.RegisterScriptBindings(xTable)` and `xContext.SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_AUDIO_TRIGGER, xConsumer)` (SB22; `Zenith2/Docs/Design/SCRIPT/Bindings.md` → "Side-effect consumer registration"). Every BUFFERED function declares `uSideEffects = 1u << SIDE_EFFECT_KIND_AUDIO_TRIGGER`, so `Seal` would refuse the table without the consumer (`SCRIPT_BINDING_ERROR_MISSING_CONSUMER`). From Stage 4 the function also registers the result decoder `xTable.SetAsyncResultDecoder(ASYNC_SOURCE_AUDIO, &Zenith_AudioDecodeNotification)` (`Zenith2/Docs/Design/SCRIPT/PublicAPI.md` → "Zenith_ScriptBinding header"), the `When…` functions declare `1u << SIDE_EFFECT_KIND_ASYNC_REQUEST`, and the start function calls `xContext.DeclareScriptAsyncSource(ASYNC_SOURCE_AUDIO, uAUDIO_NOTIFY_RESULT_BYTES)` and `xContext.SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_ASYNC_REQUEST, xSubmitter)` (the routing of `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Script notifications"; the value of `ASYNC_SOURCE_AUDIO` is SCRIPT's registry's, `Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry").

### Script side-effect payloads

Each BUFFERED binding appends one record with `AppendSideEffect(SIDE_EFFECT_KIND_AUDIO_TRIGGER, payload, demand)` where `demand.m_auCount[SCRIPT_CAPACITY_CLASS_AUDIO_TRIGGERS] = 1`. The payload is exactly a command payload of § "Command payloads" preceded by one byte, the `Zenith_AudioCommandKind`, and padded to 72 bytes; the header's `m_ulTick` is `GetTick()` of the invocation context and `m_fTimeIntoTick` 0. `Zenith_AudioScriptConsumer::Consume(uWorldTag, SIDE_EFFECT_KIND_AUDIO_TRIGGER, 0, xPayload)` validates the record exactly as a command playback does (it is untrusted in principle: a corrupted replay could carry it) and appends it to that world's queue with `m_uSource = 1`. `Consume` cannot fail: a full queue drops and counts.

## Configuration keys header

### Configuration keys

```cpp
// Zenith2/Engine/ZenithAudio/Zenith_AudioConfig.h
#pragma once
#include "ZenithCore/Zenith_Config.h"           // Zenith_ConfigSchema, Zenith_ConfigError, Zenith_BuildPlatform

/// @contract function Zenith_RegisterAudioConfigKeys
/// Ownership: None.
/// Thread affinity: MAIN_THREAD, startup step 6 (from Zenith_Module_Audio_RegisterTypes).
/// Invalidation: None.
/// Units: As each key.
/// Errors: CONFIG_ERROR_* from the schema (a duplicate key is a programmer error the schema reports).
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_ConfigError> Zenith_RegisterAudioConfigKeys(Zenith_ConfigSchema& xSchema,
	Zenith_BuildPlatform ePlatform);
inline constexpr const char* szAUDIO_KEY_MASTER_VOLUME = "audio.masterVolume";   // one constant per key below
```

Every key is presentation-only (`m_bSimulationAffecting = false`): no audio key may change simulation (Z2-D-019). Layers: E engine default, P platform, G game, Q quality tier, U user, C command line.

| Key | Type | Default | Range or values | Layers | Restart | Meaning |
|---|---|---|---|---|---|---|
| `audio.masterVolume` | FLOAT | 0.0 | [−96, 0] dB | E P G U C | no | user master volume |
| `audio.musicVolume` | FLOAT | 0.0 | [−96, 0] dB | E P G U C | no | the `Music` category |
| `audio.sfxVolume` | FLOAT | 0.0 | [−96, 0] dB | E P G U C | no | the `SFX` category |
| `audio.dialogueVolume` | FLOAT | 0.0 | [−96, 0] dB | E P G U C | no | the `Dialogue` category |
| `audio.ambienceVolume` | FLOAT | 0.0 | [−96, 0] dB | E P G U C | no | the `Ambience` category |
| `audio.uiVolume` | FLOAT | 0.0 | [−96, 0] dB | E P G U C | no | the `UI` category |
| `audio.muteWhenUnfocused` | BOOL | false | — | E P G U C | no | Windows: fade master to silence over 100 ms on window focus loss |
| `audio.outputLayout` | ENUM | `auto` | `auto`, `stereo`, `headphones`, `surround51`, `surround71` | E P G U C | no | `auto` follows the device's channel count (2 → stereo, 6 → 5.1, 8 → 7.1, other → stereo downmix) |
| `audio.device.backend` | ENUM | `auto` | `auto`, `miniaudioNull`, `null`, `offline` | E P C | yes | the backend (`Zenith2/Docs/Design/AUDIO/Device.md` → "Backend selection") |
| `audio.device.required` | BOOL | false | — | E G C | yes | true: no device at start is MODULE_ERROR_DEVICE (exit code 4) |
| `audio.device.periodFrames` | INT | 480 | 0, [64, 4096] | E P C | yes | requested period; 0 = backend default |
| `audio.android.exclusive` | BOOL | false | — | E P C | yes | AAudio exclusive sharing mode request |
| `audio.voices.realMax` | INT | 240 | [8, 240] | E P G Q C | no | the real-voice limit L for game worlds (BR-DEFER); the 16 audition slots are outside it; its shipping value comes from Z2-B-AUDIO-004 through AUDIO's fragments |
| `audio.voices.secondaryWorldRealMax` | INT | 16 | [0, 64] | E P G Q C | no | real voices a secondary world may claim |
| `audio.voices.instanceMax` | INT | 1024 | [64, 1024] | E G C | no | instances per world |
| `audio.voices.audibilityThresholdDb` | FLOAT | −60.0 | [−96, −20] | E P G Q C | no | below this a voice is inaudible (virtual) |
| `audio.voices.stealHysteresisDb` | FLOAT | 3.0 | [0, 12] | E G C | no | a real voice is displaced only by one louder by this margin |
| `audio.voices.minRealMs` | INT | 100 | [0, 1000] | E G C | no | a voice stays real at least this long once made real |
| `audio.streams.max` | INT | 32 | [1, 32] | E P G Q C | no | concurrent streams |
| `audio.streams.prebufferFrames` | INT | 4096 | [2048, 16384] | E P G C | no | frames decoded before a stream's voice starts; effective value min(this, `audio.streams.ringFrames` − 8,192) (§ "Zenith_AudioSettings") |
| `audio.streams.ringFrames` | INT | 16384 | [16384, 65536], power of two | E P C | yes | stream ring size in frames; at least 16,384 so that the ring's guaranteed fill F − 8,192 holds every prebuffer from the 2,048 minimum up |
| `audio.cache.decodedBytes` | INT | 16 MiB | [0, 256 MiB] | E P G Q C | no | the decoded-sample cache cap |
| `audio.commands.capacity` | INT | 4096 | [256, 65536], power of two | E P G C | yes | per-world command queue capacity |
| `audio.masterBank` | STRING | "" | a GUID or empty | E G C | yes | the master bank; empty = the engine default |
| `audio.banks.alwaysLoaded` | STRING | "" | comma-separated GUIDs | E G C | yes | banks kept resident from start |
| `audio.banks.retainSeconds` | FLOAT | 10.0 | [0, 600] | E G C | no | an unreferenced bank AUDIO loaded on demand stays this long |
| `audio.dialogue.locale` | STRING | "" | BCP 47 tag or empty | E G U C | no | dialogue locale; empty = `ui.locale` when registered, else the platform's preferred locale |
| `audio.occlusion.raysPerFrame` | INT | 64 | [0, 1024] | E P G Q C | no | occlusion segment tests per frame (BR-DEFER beyond) |
| `audio.reverb.quality` | ENUM | `high` | `low`, `high` | E P G Q C | no | 4-line or 8-line FDN |
| `audio.avSyncOffsetMs` | FLOAT | 0.0 | [−200, 200] | E P G U C | no | added to every scheduled start (positive delays audio) |
| `audio.debug.view` | ENUM | `off` | `off`, `emitters`, `occlusion`, `zones`, `voices` | E C | no | development debug view |
| `audio.capture.seconds` | INT | 0 | [0, 600] | E C | no | development output capture length; 0 = off |
| `audio.mixer.overrunSharePercent` | FLOAT | 100.0 | [10, 100] | E P C | no | the callback share above which a callback is an overrun and Z2-B-AUDIO-003 is breached (AUDIO.md "Measuring the mixer callback row"); the Stage 1 ruling writes the row's value into the platform fragments |

User volume keys are applied through `Zenith_ConfigStore::GetUserChangeRegistry()` (prologue step 3, main thread): the callback forwards the new value to `Zenith_AudioSystem::OnUserVolumeChanged`, which ramps the category over 50 ms. The game's settings screen writes the user layer through PLAT's `SetUserValue`; AUDIO stores nothing itself.

## Internal interfaces

### Zenith_AudioDeviceBackend

```cpp
// Zenith2/Engine/ZenithAudio/Internal/Device/Zenith_AudioDeviceBackend.h (Internal; tests implement it)
struct Zenith_AudioDeviceConfig
{
	uint32 m_uSampleRate = uAUDIO_MIX_SAMPLE_RATE;
	uint32 m_uChannels = 2;                  // 2, 6 or 8
	uint32 m_uPeriodFrames = 480;
	bool m_bLowLatency = true;
	bool m_bExclusive = false;
};
struct Zenith_AudioDeviceInfo { uint32 m_uSampleRate; uint32 m_uChannels; uint32 m_uPeriodFrames; uint32 m_uLatencyUs; char m_acName[64]; };
using Zenith_PfnAudioRender = void (*)(float* pfInterleaved, uint32 uFrames, uint32 uChannels, void* pUserData);
enum Zenith_AudioBackendNotification : uint8 { AUDIO_BACKEND_NOTIFY_STOPPED = 1, AUDIO_BACKEND_NOTIFY_REROUTED, AUDIO_BACKEND_NOTIFY_INTERRUPTION_BEGIN, AUDIO_BACKEND_NOTIFY_INTERRUPTION_END };
using Zenith_PfnAudioBackendNotify = void (*)(Zenith_AudioBackendNotification eNotification, void* pUserData);

/// @contract interface Zenith_AudioDeviceBackend
/// Ownership: Owned by Zenith_AudioSystem.
/// Thread affinity: Open, Close, Start, Stop and GetInfo on the control thread only; the render callback on the device
///   thread (whichever thread the backend uses, possibly a new one after a reroute); the notify callback on any backend
///   thread, concurrently (it only pushes to the multi-producer notification queue and posts the control semaphore).
/// Invalidation: The render callback is never called after Stop returns.
/// Units: Frames, Hz, microseconds.
/// Errors: Open and Start return AUDIO_ERROR_DEVICE_OPEN_FAILED with the backend's result in m_uOsCode.
/// Reentrancy: NONE. The callbacks never call a backend member.
/// Version: since Z2 Stage 3. Internal.
/// @endcontract
class Zenith_AudioDeviceBackend
{
public:
	virtual ~Zenith_AudioDeviceBackend() = default;
	virtual Zenith_Result<Zenith_AudioDeviceInfo, Zenith_AudioError> Open(const Zenith_AudioDeviceConfig& xConfig,
		Zenith_PfnAudioRender pfnRender, Zenith_PfnAudioBackendNotify pfnNotify, void* pUserData) = 0;
	virtual Zenith_Result<void, Zenith_AudioError> Start() = 0;
	virtual void Stop() = 0;         // returns after the last render callback has returned
	virtual void Close() = 0;
	virtual Zenith_AudioDeviceInfo GetInfo() const = 0;
	virtual bool IsRealTime() const = 0;   // false for the offline backend
};
```

### Zenith_AudioReadSource

<!-- elaboration -->
Every stream read goes through this AUDIO-internal interface, so tests can delay, reorder, withhold and fail single reads without a double of `Zenith_AssetReader`, which is a concrete class (FINAL-DOUBLES; `Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetReader header") and for which ASSET offers no per-read seam. It is AUDIO's own seam, like `Zenith_AudioDeviceBackend`. A read is named by AUDIO's own `Zenith_AudioReadID`, not by a `Zenith_OperationHandle`, which is a concrete move-only class issued only by CORE's operation table; the production implementation keeps the handle inside.

```cpp
// Zenith2/Engine/ZenithAudio/Internal/Streaming/Zenith_AudioReadSource.h (Internal; tests implement it)
struct Zenith_AudioReadID { uint16 m_uSlot = 0xFFFFu; uint16 m_uGeneration = 0; };   // 0xFFFF: none

/// @contract interface Zenith_AudioReadSource
/// Ownership: The production instance (Zenith_AudioAssetReadSource) is owned by Zenith_AudioSystem; a test's instance is
///   borrowed through Zenith_AudioSystemTestAccess::SetReadSource and outlives the system.
/// Thread affinity: OWNER_THREAD: the decode thread (under the offline backend, the thread calling ServiceBeforeFrame);
///   the role passes from the decode thread to the destructor's thread only after the decode thread has exited.
/// Invalidation: A Zenith_AudioReadID is valid from Submit until Release; xDestination stays the caller's and is not touched
///   by the caller until GetStatus reports a terminal status.
/// Units: Bytes; offsets from the start of the cooked file; milliseconds.
/// Errors: Submit returns ASSET's submission errors (Zenith2/Docs/Design/ASSET/RuntimeAPI.md "Zenith_AssetReader header"),
///   ASSET_ERROR_QUEUE_FULL also when the source's 32 read slots are in use; ServiceUntilComplete returns ASSET_ERROR_TIMEOUT
///   with the read still running and the read's error record on FAILED or CANCELLED.
/// Reentrancy: NONE. No member calls back into AUDIO.
/// Version: since Z2 Stage 3. Internal.
/// @endcontract
class Zenith_AudioReadSource
{
public:
	virtual ~Zenith_AudioReadSource() = default;
	virtual Zenith_Result<Zenith_AudioReadID, Zenith_AssetError> Submit(const Zenith_AnyAssetHandle& xAsset,
		uint64 ulOffset, uint32 uBytes, Zenith_MutableByteSpan xDestination, Zenith_AssetPriority ePriority) = 0;
	virtual Zenith_OperationStatus GetStatus(Zenith_AudioReadID xRead) const = 0;
	virtual void RequestCancel(Zenith_AudioReadID xRead) = 0;             // idempotent, never blocks
	virtual Zenith_AssetError GetError(Zenith_AudioReadID xRead) const = 0;   // FAILED or CANCELLED
	virtual void Release(Zenith_AudioReadID xRead) = 0;                   // after a terminal status; frees the read slot
	// OFFLINE servicing and the shutdown wait: blocks the calling non-worker thread until a terminal status or the timeout.
	virtual Zenith_Result<void, Zenith_AssetError> ServiceUntilComplete(Zenith_AudioReadID xRead, uint32 uTimeoutMs) = 0;
};
```

| Implementation | File | Behaviour |
|---|---|---|
| `Zenith_AudioAssetReadSource` (production) | `Internal/Streaming/Zenith_AudioAssetReadSource.h/.cpp` | 32 read slots (a stream has at most one read outstanding, so `uAUDIO_MAX_STREAMS` suffice), each holding the `Zenith_OperationHandle` of `Zenith_AssetReader::ReadRange(xAsset, ulOffset, uBytes, xDestination, ePriority)` and a generation; `GetStatus` and `RequestCancel` forward to the handle, `GetError` to `Zenith_AssetReader::GetError`, `Release` resets the handle and bumps the generation, `ServiceUntilComplete` to `Zenith_AssetReader::ServiceUntilComplete(handle, uTimeoutMs)` (row AU26; Z2T-ASSET-029, `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing") |
| `Zenith_AudioScriptedReadSource` (tests) | `Tests/Support/Zenith_AudioScriptedReadSource.h` | `Zenith2/Docs/Design/AUDIO/Tests.md` → "Scripted read source" |

Only stream reads use it. Asset loads AUDIO issues (dialogue lines, locale variant banks, audition banks, the master bank) still go through the real `Zenith_AssetLoader`, so a test that substitutes the read source jitters stream reads only. `Test_AudioStream_AssetReadSourceReadsCookedFile` drives the production source over a real `Zenith_AssetRegistry` on generated fixtures under `${ZENITH_OUT}/test-fixtures/AUDIO/`: ranges read back equal to the file's bytes, a 33rd concurrent submission is `ASSET_ERROR_QUEUE_FULL`, a cancelled read reports CANCELLED and is released, and `ServiceUntilComplete` returns once the read completes.

### Zenith_AudioMixer

```cpp
// Zenith2/Engine/ZenithAudio/Internal/RealTime/Zenith_AudioMixer.h
class Zenith_AudioMixer
{
public:
	// DEVICE_THREAD. Renders uFrames interleaved frames; never allocates, locks, waits or logs.
	void RenderBlocks(float* pfInterleaved, uint32 uFrames, uint32 uChannels);
	// Control side, before the device starts or while it is stopped: binds the channels and preallocated buffers.
	void Bind(Zenith_AudioRtChannels& xChannels, Zenith_AudioRtBuffers& xBuffers);
	uint64 GetFrameCounter() const;      // ANY_THREAD (acquire)
	uint64 GetAppliedEpoch() const;      // ANY_THREAD (acquire): the newest snapshot epoch applied
	uint64 GetReleasedEpoch() const;     // ANY_THREAD (acquire): Device.md "Sample memory retirement"
	uint32 GetSlotState(uint32 uSlot) const;   // ANY_THREAD (acquire): generation << 8 | state
};
```

### Zenith_AudioVoiceManager

```cpp
// Zenith2/Engine/ZenithAudio/Internal/Control/Zenith_AudioVoiceManager.h
class Zenith_AudioVoiceManager
{
public:
	Zenith_Result<Zenith_AudioInstanceID, Zenith_AudioError> Post(Zenith_AudioWorldIndex uWorld,
		const Zenith_AudioPostEventPayload& xPost, const Zenith_AudioEventRecord& xEvent);
	void Stop(Zenith_AudioInstanceID xInstance, uint32 uFadeMs, Zenith_AudioStopMode eMode);
	void Advance(Zenith_AudioWorldIndex uWorld, double dSeconds);                       // U4, one world
	void SelectRealVoices(const Zenith_AudioSettings& xSettings, uint16 uPrimaryWorldTag);   // M6, every world
	void WriteSnapshot(Zenith_AudioMixerSnapshot& xOut) const;
	void ApplyFeedback(Zenith_Span<const Zenith_AudioFeedbackRecord> axRecords);
};
```

### Test access

```cpp
// Zenith2/Engine/ZenithAudio/Internal/Zenith_AudioSystemTestAccess.h (Internal; included only by Tests/)
class Zenith_AudioSystemTestAccess
{
public:
	// Creates a system whose backend is xBackend (the scripted test backend); the system does not own it.
	static Zenith_Result<Zenith_Owned<Zenith_AudioSystem>, Zenith_AudioError> CreateWithBackend(
		const Zenith_AudioSystemDesc& xDesc, Zenith_AudioDeviceBackend& xBackend);
	static Zenith_AudioMixer& GetMixer(Zenith_AudioSystem& xSystem);
	static Zenith_AudioVoiceManager& GetVoices(Zenith_AudioSystem& xSystem);
	// Consumption log (Formats.md "Consumption log record"): record during a real-time run; replay through the offline path.
	static void EnableConsumptionLog(Zenith_AudioSystem& xSystem, Zenith_MutableSpan<Zenith_AudioConsumptionRecord> axLog);
	static void EnableSnapshotArchive(Zenith_AudioSystem& xSystem, Zenith_MutableSpan<Zenith_AudioMixerSnapshot> axArchive);
	static Zenith_Result<void, Zenith_AudioError> ReplayConsumptionLog(Zenith_AudioOfflineRenderer& xRenderer,
		Zenith_Span<const Zenith_AudioConsumptionRecord> axLog, Zenith_Span<const Zenith_AudioMixerSnapshot> axArchive);
	static void ForceDeviceEvent(Zenith_AudioSystem& xSystem, Zenith_AudioBackendNotification eEvent);
	static void ForceFocus(Zenith_AudioSystem& xSystem, Zenith_AudioFocusState eFocus);
	// The callback-timing instrument's clock (Zenith_GetMonotonicTimeNs in production); tests script durations with it.
	static void SetMixerClock(Zenith_AudioSystem& xSystem, uint64 (*pfnNowNs)(void*), void* pUserData);
	// Holds the device's per-voice fades open (the voice keeps rendering at its last gain) until released, for the retirement tests.
	static void HoldDeviceFades(Zenith_AudioSystem& xSystem, bool bHold);
	// Suspends M9's frees (entries stay on the retire list) so archived snapshots keep pointing at valid sample memory during a
	// consumption-log replay; resuming frees what is due at the next Audio.Mix.
	static void SuspendRetirement(Zenith_AudioSystem& xSystem, bool bSuspend);
	// Overrides the device's AcquireLatest for the scripted backend: while withheld, publishes accumulate (the newest wins).
	static void WithholdSnapshotAcquire(Zenith_AudioSystem& xSystem, bool bWithhold);
	// Replaces the production read source with xSource (the scripted read source) for every stream read; call before Start
	// (the decode thread, or ServiceBeforeFrame, reads the pointer only after Start). The system does not own xSource.
	static void SetReadSource(Zenith_AudioSystem& xSystem, Zenith_AudioReadSource& xSource);
};
```

`ReplayConsumptionLog` renders one block per `RenderBlocks` call, sets the mixer's replay flag, rewrites every archived stream voice to source kind 5 over a replay feed and sets each feed's underrun frames per block from the log (`Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing").

No test double replaces a concrete class (FINAL-DOUBLES): the tests substitute only AUDIO's own two internal interfaces, the device backend and the read source, which are interfaces by design, and drive everything else, `Zenith_AssetReader` and `Zenith_AssetLoader` included, through the real types.
