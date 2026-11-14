# CINE public API

## Scope of this file

This file holds every public header of the CINE area: the root headers of `ZenithCinematics`, the CINE-owned root header of `ZenithFlux` (`Flux_Video.h`), CINE's root header of the `ZenithAuthoring` tool library (`Zenith2/Tools/Authoring/Zenith_AuthoringCinematics.h`) and the one ZenithCook-internal header CINE owns. Every declaration is in Zenith Hungarian, every fallible call returns `Zenith_Result<T, E>`, and every type and every group of constants or free functions carries one contract block in the grammar of `Zenith2/Docs/Contracts.md` → "Block grammar" (a `functions` block names its group). The entry file `Zenith2/Docs/Design/CINE.md` → "Public API" holds the header map and the rules across the API; the algorithms behind each call are in `Zenith2/Docs/Design/CINE/Playback.md`, the formats in `Zenith2/Docs/Design/CINE/Formats.md`, the video path in `Zenith2/Docs/Design/CINE/Video.md`.

CINE never includes a ZenithAudio, ZenithVFX, ZenithUI or ZenithGameplay header ([Z2-N-ARCH-074]). It reaches AUDIO and VFX through their registered command kinds by name and their reflected payload types by stable name, and UI through the caption event types of ZenithRuntime (rows UI4, CN20). It includes ZenithAnimation headers, because `ZenithCinematics` declares `ZenithAnimation` in `DEPENDS` (`Zenith2/Docs/Architecture.md` → "Library dependency graph").

Every value a script can read is exact in a Luau number: no script-visible property or return value is a 64-bit hash (`Zenith2/Docs/Scripting/Bindings.md` → "Value marshalling": `uint64` is exact only up to 2⁵³, beyond which ZS1504 is raised). Names are `Zenith_StringID` (a Luau string), tickets are four `uint32` fields, and instance IDs are per-world serials below 2⁵³. 64-bit hashes remain only in C++-only types no generated `.d.luau` type exposes: `Zenith_CinematicBinding::m_ulNameHash` (C++ recorder input), the command payloads (never read by scripts) and `Zenith_CinematicLifecyclePayload` (delivered to C++ lifecycle listeners only).

## Zenith_Module_Cinematics header

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_Module_Cinematics.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithReflect/Zenith_TypeRegistry.h"
#include "ZenithRuntime/Zenith_Module.h"

/// @contract functions Zenith_Module_Cinematics
/// Ownership: RegisterTypes registers CINE's reflected components, structs, events, world resources, command payload
///   types and configuration keys; Start creates the module instance the runtime owns until shutdown.
/// Thread affinity: MAIN_THREAD, at the startup steps of Zenith2/Docs/Architecture/CompositionRoot.md "Startup sequence".
/// Invalidation: None.
/// Units: None.
/// Errors: MODULE_ERROR_REGISTRATION when a registration is rejected (logged by the context first);
///   MODULE_ERROR_OUT_OF_MEMORY when the instance, a world record or a pool cannot be allocated;
///   MODULE_ERROR_BACKEND_CREATE when the video decoder backend cannot be created.
/// Version: since Z2 Stage 4. Module entry points per Zenith2/Docs/Architecture.md "Module entry points".
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_Cinematics_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_Cinematics_Start(
	Zenith_ModuleStartContext& xContext);

/// @contract functions Zenith_CinematicsFaultPoints
/// Ownership: constexpr constants; no state.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None. Fault point IDs of [Z2-N-CONTRACT-255]: 'C' 'I' (0x4349) then the step.
/// Errors: None; consulted only when a test passes a Zenith_FaultInjector through Zenith_CinematicsModuleDesc.
/// Version: since Z2 Stage 4.
/// @endcontract
inline constexpr uint32 uFAULT_POINT_CINEMATICS_START_FIRST = 0x43490001u;    // + step − 1, steps 1 … 7
inline constexpr uint32 uFAULT_POINT_CINEMATICS_ATTACH_FIRST = 0x43490101u;   // + step − 1, steps 1 … 4
```

| Entry point step | Action | Failure |
|---|---|---|
| RegisterTypes 1 | the struct types `Zenith.Cinematics.BindingEntry`, `Zenith.Cinematics.Ticket`, `Zenith.Cinematics.InstanceID`, `Zenith.Cinematics.LifecyclePayload`, the six command payload types of "Command payloads", the component `Zenith.Cinematics.Player`, the events `Zenith.Cinematics.Started`, `Zenith.Cinematics.Ended`, `Zenith.Cinematics.Marker`, `Zenith.Cinematics.StartFailed`, the world resource types `Zenith.Cinematics.World` and `Zenith.Cinematics.Emission` | `MODULE_ERROR_REGISTRATION` |
| RegisterTypes 2 | the `cine.*` configuration keys of "Configuration keys" through `GetConfigSchema`; the `cine.capture.*` keys only when `!Zenith_IsShippingBuild()` | `MODULE_ERROR_REGISTRATION` |
| Start 1 | allocate the `Zenith_CinematicsModule` instance from the general heap; read and validate every `cine.*` key and `sim.tickHz`; convert each `f32` seconds key once to integer µs with `llround(double(v) × 1e6)` | `MODULE_ERROR_OUT_OF_MEMORY` |
| Start 2 | register the kind loaders of `ASSET_KIND_CINEMATIC` (14) and `ASSET_KIND_VIDEO` (21) (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md` → "Zenith_AssetKindLoader header") with the module as user data; the loaders intern names through the process string table (`GetStringTable()`) | `MODULE_ERROR_REGISTRATION` |
| Start 3 | register the eleven command kinds of "Zenith_CinematicCommands header" through `RegisterCommandKind` | `MODULE_ERROR_REGISTRATION` |
| Start 4 | register the `Cinematics` script binding table through `RegisterScriptBindings` (no side-effect consumer: the request functions use the ECS-held kind `SIDE_EFFECT_KIND_FIELD_COMMAND`, [Z2-N-SCRIPT-219], [Z2-N-SCRIPT-226]; no asynchronous source: it is declared per world at AttachWorld 1) | `MODULE_ERROR_REGISTRATION` |
| Start 5 | register the memory consumer `Cinematics` (`MEMORY_SUBSYSTEM_ASSETS`, budget row `Z2-B-CINE-001`) | `MODULE_ERROR_REGISTRATION` |
| Start 6 | register the frame listener `Zenith_CinematicsFrameListener` (`RegisterFrameListener`: Android suspend and resume, "Android lifecycle" of `Zenith2/Docs/Design/CINE/Video.md`; the capture's tick gate, "Capture algorithm"); then, when `GetRenderer()` is non-null and `cine.video.enabled` is true: create the decoder backend through the desc's factory ("Zenith_VideoDecoder interface"), start the thread `Zenith.VideoDec`, and call `Flux_Renderer::EnableVideo` with the module's `Flux_VideoFrameSource` | `MODULE_ERROR_BACKEND_CREATE` (decoder), `MODULE_ERROR_OUT_OF_MEMORY`, `MODULE_ERROR_REGISTRATION` |
| Start 7 | when `!Zenith_IsShippingBuild()` (an `if constexpr`, no layout-changing conditional): register the console commands and the profiler panel provider of `Zenith2/Docs/Design/CINE.md` → "Diagnostics and profiling", and create `Zenith_CinematicCapture` (held by the module as `Zenith_Owned`, null in Shipping and `ShippingTest`) | `MODULE_ERROR_REGISTRATION`, `MODULE_ERROR_OUT_OF_MEMORY` |
| AttachWorld 1 | allocate the world record (`Zenith_CinematicsWorld` with its outbox of 256 + 10 × `cine.maxInstancesPerWorld` + 64 entries, plus the snapshot pool of `cine.snapshotPoolBytes`), resolve CINE's kinds and the eleven ANIM and AUDIO kinds by name with `FindCommandKind` into `Zenith_CinematicCommandKinds` and `Zenith_CinematicForeignKinds`, add the world resources `Zenith.Cinematics.World` owned by `PHASE_INPUT` and `Zenith.Cinematics.Emission` owned by `PHASE_TRANSFORM_PROPAGATION`, take `GetSceneState()`, take (or reuse, by world tag) the module's `Zenith_CinematicsPublicationHandler` entry, then on `GetPublicationSink()`: `DeclareSource(ASYNC_SOURCE_CINEMATICS, Zenith_ModuleSourceDesc{ 64, 24, MODULE_SOURCE_SITE_MERGE_CHAIN })` and `SetSourceHandler(ASYNC_SOURCE_CINEMATICS, xHandler)` (`Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "Source declaration and handler registration"; `Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding") | `MODULE_ERROR_OUT_OF_MEMORY`, `MODULE_ERROR_REGISTRATION` (a rejected declaration, reported by the sink) |
| AttachWorld 2 | add the five systems of `Zenith2/Docs/Design/CINE/Playback.md` → "Systems" | `MODULE_ERROR_REGISTRATION` |
| AttachWorld 3 | add the lifecycle listener (`AddLifecycleListener`), the tick-boundary handler (`AddTickBoundaryHandler`) and the save participant `Zenith.Cinematics.Playback` (`RegisterSaveParticipant`) | `MODULE_ERROR_REGISTRATION` |
| AttachWorld 4 | add the extractor `Cinematics.ExtractRender` (every configuration, headless included; it writes nothing when the renderer is absent) | `MODULE_ERROR_REGISTRATION` |

`Zenith_WorldAttachContext::GetPublicationSink()` and `FindCommandKind(const char*)` are core text (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces", [Z2-N-ARCH-879]; MOD's halves in `Zenith2/Docs/Design/MOD/RuntimeSeams.md` → "World attach context lookups"), and `ASYNC_SOURCE_CINEMATICS` is 12 (`Zenith2/Docs/Scripting.md` → "Asynchronous source kind registry").

`Zenith_WorldAttachContext::AddTickBoundaryHandler` (X313 (c)) and `Zenith_ModuleStartContext::GetStringTable()` (X339 (a); the process string table the composition root owns and passes by reference, valid for the process's lifetime, so the kind loaders keep it past `Start` and intern at every load until the module's shutdown, `Zenith2/Docs/Design/CORE/Text.md` → "String interning") are in `Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces", with `GetSceneState()`, `RegisterSaveParticipant`, `RegisterFrameListener` and `RegisterScriptBindings`.

**Test construction seam.** `Zenith_Module_Cinematics_Start` calls `Zenith_CinematicsModule::Start(xContext, Zenith_CinematicsModuleDesc{})`, the internal constructor of `Engine/ZenithCinematics/Internal/Module/Zenith_CinematicsModule.h`, whose desc carries `Zenith_FaultInjector* m_pxFaults = nullptr` (consulted at every Start and Attach step's fault point) and `Zenith_PfnCreateVideoDecoder m_pfnCreateVideoDecoder = &Zenith_CreatePlatformVideoDecoder` (`using Zenith_PfnCreateVideoDecoder = Zenith_Result<Zenith_Owned<Zenith_VideoDecoder>, Zenith_CinematicError> (*)(Zenith_Heap&)`). `ZenithCinematics_Tests`, which may include `Internal/`, constructs the module with a fault injector and with `Zenith_CreateTestVideoDecoder` (`Tests/Support/Zenith_TestVideoDecoder.h`, header-only): a declared seam with a production implementation, never a double of a concrete class (FINAL-DOUBLES). Every step has a failure-injection test, `Test_CinematicsModule_Start_FailsAtStep<N>_Unwinds` (seven) and `Test_CinematicsModule_Attach_FailsAtStep<N>_Unwinds` (four), `Zenith2/Docs/Design/CINE/Tests.md` → "Module and registration tests"; the runtime undoes the earlier registrations in reverse.

## Zenith_CinematicErrors header

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_CinematicErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_CinematicErrorCode
/// Ownership: Plain enumeration; the m_eCode of Zenith_CinematicError ([Z2-N-CONTRACT-480], struct form, as
///   Zenith_ModuleErrorCode is to Zenith_ModuleError).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: The meaning of each value is its comment. The same uint16 value is what a kind loader passes to Fail and what
///   Zenith.Cinematics.StartFailed and Zenith.Cinematics.Player LastError carry.
/// Version: since Z2 Stage 4. Values keep their meaning once shipped ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_CinematicErrorCode : uint8
{
	CINEMATIC_ERROR_OUT_OF_MEMORY = 1,        // an allocation failed (world record, snapshot pool, instance pool, decoder)
	CINEMATIC_ERROR_CORRUPT,                  // a cooked .zcine or .zvid violated a rule of Formats.md "Load validation"
	CINEMATIC_ERROR_VERSION,                  // a cooked format version this build does not read
	CINEMATIC_ERROR_INVALID_ASSET,            // the handle is null, of the wrong kind, or FAILED
	CINEMATIC_ERROR_NOT_RESIDENT,             // an operation that needs a resident asset found it still loading
	CINEMATIC_ERROR_INSTANCE_LIMIT,           // cine.maxInstancesPerWorld instances already occupy the world's pool
	CINEMATIC_ERROR_NO_INSTANCE,              // the instance ID names no live instance of the world
	CINEMATIC_ERROR_TICKET_PENDING,           // the ticket's start command has not been played back yet
	CINEMATIC_ERROR_TICKET_EXPIRED,           // the ticket's resolve window has passed, or its start was dropped (ticket table full)
	CINEMATIC_ERROR_BINDING_UNKNOWN,          // a binding names no binding slot of the sequence
	CINEMATIC_ERROR_BINDING_DUPLICATE,        // two bindings name the same slot, or a no-takeover start found the entity held
	CINEMATIC_ERROR_BINDING_REQUIRED,         // a slot the sequence marks required has no live entity
	CINEMATIC_ERROR_BINDING_TYPE,             // the bound entity lacks a component a track of that slot requires
	CINEMATIC_ERROR_SNAPSHOT_LIMIT,           // the instance block exceeds the per-instance limits or the world pool
	CINEMATIC_ERROR_PREPARE_TIMEOUT,          // preparation did not finish within cine.prepareTimeoutTicks
	CINEMATIC_ERROR_KIND_MISSING,             // a track names a command kind or an event type the running build does not register
	CINEMATIC_ERROR_PAYLOAD,                  // a command or event payload field does not match the registered type
	CINEMATIC_ERROR_INVALID_ARGUMENT,         // a time, rate or blend outside its documented range, or a rejected sentinel
	CINEMATIC_ERROR_WRONG_STATE,              // the instance state does not accept the request (Playback.md "Instance state machine")
	CINEMATIC_ERROR_CANCELLED,                // a pending start was cancelled by interrupt, skip, world detach or a re-cooked save
	CINEMATIC_ERROR_VIDEO_DECODE,             // the decoder backend reported a stream error
	CINEMATIC_ERROR_VIDEO_UNSUPPORTED,        // the codec, profile, level, bit depth or resolution is outside the decoder's capability
	CINEMATIC_ERROR_VIDEO_DEVICE,             // the decoder device was lost (D3D11 removal, a reclaimed MediaCodec)
	CINEMATIC_ERROR_CAPTURE_IO,               // a capture or export file could not be written
	CINEMATIC_ERROR_CAPTURE_STATE,            // a capture operation was issued in the wrong capture state or configuration
	CINEMATIC_ERROR_COUNT,
};
constexpr const char* Zenith_ToString(Zenith_CinematicErrorCode eCode);   // the enumerator name, "CINEMATIC_ERROR_OUT_OF_MEMORY" for 1

/// @contract value Zenith_CinematicError
/// Ownership: Plain value (4 bytes), trivially copyable ([Z2-N-CONTRACT-480]).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: m_uDetail is the error's fixed-size context: the check number for CORRUPT (L1 … L12, V1 … V7), the binding or
///   slot index for BINDING_*, the track index for KIND_MISSING and PAYLOAD; 0xFFFF when none.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicError
{
	const Zenith_CinematicErrorCode m_eCode = CINEMATIC_ERROR_OUT_OF_MEMORY;
	const uint8 m_uReserved = 0;
	const uint16 m_uDetail = 0xFFFFu;
};
```

## Zenith_CinematicTypes header

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_CinematicTypes.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Hash.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithECS/Zenith_EntityHandle.h"

/// @contract functions Zenith_CinematicLimits
/// Ownership: constexpr constants; no state.
/// Thread affinity: IMMUTABLE. Compile-time constants.
/// Invalidation: None.
/// Units: Counts, bytes, microseconds and ticks as named.
/// Errors: None; the cook, the loader and the command playbacks check against them.
/// Version: since Z2 Stage 4. Raising a limit is a format-compatible change; lowering one is a cooked re-cook.
/// @endcontract
inline constexpr uint32 uCINE_MAX_INSTANCES_PER_WORLD_LIMIT = 64;     // cine.maxInstancesPerWorld upper bound (default 16)
inline constexpr uint32 uCINE_MAX_BINDINGS = 64;                      // binding slots per sequence
inline constexpr uint32 uCINE_MAX_TRACKS = 512;                       // tracks per sequence after subsequence flattening
inline constexpr uint32 uCINE_MAX_CHANNELS_PER_TRACK = 64;            // curve channels per track
inline constexpr uint32 uCINE_MAX_KEYS_PER_CHANNEL = 65536;
inline constexpr uint32 uCINE_MAX_SECTIONS_PER_TRACK = 4096;
inline constexpr uint32 uCINE_MAX_SECTIONS = 65536;                   // sections per sequence, all tracks
inline constexpr uint32 uCINE_MAX_EVENT_KEYS = 65536;                 // event keys per sequence
inline constexpr uint32 uCINE_MAX_FIRE_ON_SKIP_KEYS = 8;              // keys and sections flagged fireOnSkip per sequence
inline constexpr uint32 uCINE_MAX_EVENT_PAYLOAD_BYTES = 256;          // one event or command payload
inline constexpr uint32 uCINE_MAX_SUBSEQUENCE_DEPTH = 4;
inline constexpr uint64 ulCINE_MAX_DURATION_US = 3600ull * 1000000ull;   // one hour
inline constexpr uint32 uCINE_MAX_SNAPSHOT_ENTRIES = 4096;            // per instance
inline constexpr uint32 uCINE_MAX_SNAPSHOT_BYTES = 262144;            // per instance block (256 KiB)
inline constexpr uint32 uCINE_MAX_EVENTS_PER_TICK = 256;              // outbox entries Cinematics.Evaluate may hold (its admission)
inline constexpr uint32 uCINE_OUTBOX_INSTANCE_RESERVE = 10;           // outbox entries reserved per instance (outcome, Ended, 8 skip events)
inline constexpr uint32 uCINE_OUTBOX_START_SLACK = 64;                // = uCINE_MAX_TICKET_ENTRIES; outbox = 256 + 10 × M + 64
inline constexpr uint32 uCINE_EMISSION_GROUP_ENTRIES = 64;            // entries per emission group of Cinematics.EmitEvents
inline constexpr uint32 uCINE_EMISSION_ORDINAL_BASE = 1;              // event buffer ordinal of emission group g: 1 + g
inline constexpr uint32 uCINE_CURRENT_ORDINAL = 0;                    // the ordinal CINE opens to make an entity current
inline constexpr uint32 uCINE_MAX_BOUND_SEQUENCES = 96;               // per world: bound-set entries (64 records + 32 preloads)
inline constexpr uint32 uCINE_MAX_CAPTIONS_PER_INSTANCE = 16;         // caption list; also the cook's caption overlap limit
inline constexpr uint32 uCINE_MAX_LIPSYNC_WINDOWS = 16;               // window records per dialogue face clip (cooked table)
inline constexpr uint64 ulCINE_MIN_LOOP_SCALED = 8000000ull;          // a looping start needs D × H ≥ this (rate 8's advance)
inline constexpr uint32 uCINE_CAPTURE_VIDEO_WAIT_FRAMES = 300;        // a capture fails after this many frames waiting for video
inline constexpr uint64 ulCINE_CAPTURE_MAX_AUDIO_FRAMES = 536870907ull;   // (2^32 − 1 − 36) / 8: the u32 WAV size limit
inline constexpr uint32 uCINE_MAX_COMMANDS_PER_INSTANCE_TICK = 512;   // records one instance may emit in one tick buffer
inline constexpr uint32 uCINE_TICKET_WINDOW_TICKS = 2;                // a ticket resolves until the end of tick t + 2
inline constexpr uint32 uCINE_MAX_TICKET_ENTRIES = 64;                // per world
inline constexpr uint32 uCINE_MAX_PLAYER_STARTS = 64;                 // per world: autostarts awaiting their outcome
inline constexpr uint32 uCINE_MAX_LOADS_IN_FLIGHT = 64;               // per world: asset needs between allocation and publication
inline constexpr uint32 uCINE_MAX_PRELOADS = 32;                      // per world: distinct preloaded sequences
inline constexpr uint32 uCINE_AUDIO_LIST_CAPACITY = 1024;             // per world: audio instances CINE posted
inline constexpr uint32 uCINE_MAX_AUDIO_PER_INSTANCE = 256;
inline constexpr uint32 uCINE_MAX_PREPARES_PER_INSTANCE = 32;         // dialogue lines in one preparation window
inline constexpr uint32 uCINE_SIDE_ORDINAL_BASE = 1;                  // side buffer ordinal of instance index i: 1 + i
inline constexpr uint32 uCINE_TICK_ORDINAL_BASE = 65;                 // tick buffer ordinal of instance index i: 65 + i
inline constexpr uint32 uCINE_RATE_ONE_Q16 = 65536;                   // rate 1.0 in Q16.16
inline constexpr uint32 uCINE_RATE_MAX_Q16 = 8u * 65536u;             // rate 8.0
inline constexpr uint32 uCINE_REJECTED_SENTINEL = 0xFFFFFFFFu;        // a NaN or infinite float recorded into a u32 field
inline constexpr uint64 ulCINE_MAX_BLEND_US = 60ull * 1000000ull;     // blend-in and blend-out upper bound (60 s)
inline constexpr uint32 uCINE_MAX_VIDEO_WIDTH = 3840;
inline constexpr uint32 uCINE_MAX_VIDEO_HEIGHT = 2160;
inline constexpr uint64 ulCINE_MAX_VIDEO_DURATION_US = 7200ull * 1000000ull;   // two hours
inline constexpr uint32 uCINE_MAX_VIDEO_PLAYERS = 4;                  // per renderer

/// @contract value Zenith_CinematicInstanceID
/// Ownership: Plain value (8 bytes). The world's cinematic record owns the instance it names.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: Names no instance once the instance's record is freed (Playback.md "Instance state machine", state
///   RELEASED); an ID is never reused within one world's lifetime, so a stale ID resolves to CINEMATIC_ERROR_NO_INSTANCE.
/// Units: None. m_ulValue is the world's instance serial, starting at 1; 0 is the null ID. A serial stays below 2^53
///   (2^53 starts at 1,000 per second take 285,000 years), so a script reads it exactly; reflected struct
///   "Zenith.Cinematics.InstanceID": 1 Value (U64).
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized as the 64-bit value in saves, replay logs and command payloads.
/// @endcontract
struct Zenith_CinematicInstanceID
{
	uint64 m_ulValue = 0;
	constexpr bool IsNull() const { return m_ulValue == 0; }
	friend constexpr bool operator==(Zenith_CinematicInstanceID, Zenith_CinematicInstanceID) = default;
};

/// @contract value Zenith_CinematicTicket
/// Ownership: Plain value (16 bytes) returned by Zenith_RecordStartCinematic.
/// Thread affinity: IMMUTABLE; members non-const: a value type its holders assign and reassign.
/// Invalidation: Resolves (Zenith_CinematicsWorld::ResolveTicket) from the playback of its start command until the end of
///   tick t + uCINE_TICKET_WINDOW_TICKS, t being the playback tick; afterwards CINEMATIC_ERROR_TICKET_EXPIRED.
/// Units: The 64-bit key hash is Zenith_Hash64 of the 20 bytes u32 system order, u64 simulation ID, u32 invocation
///   ordinal, u32 sequence of the recording buffer's base merge key, each little-endian, in that order, with the sequence
///   replaced by the buffer's record count at record time; it is stored as two uint32 halves so that every field is
///   exact in Luau. m_uRecordTick is the low 32 bits of the recording tick. Reflected struct "Zenith.Cinematics.Ticket":
///   1 KeyHashLo, 2 KeyHashHi, 3 Sequence, 4 RecordTick (all U32).
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized field by field in the start payload, the events and the save section.
/// @endcontract
struct Zenith_CinematicTicket
{
	uint32 m_uKeyHashLo = 0;
	uint32 m_uKeyHashHi = 0;
	uint32 m_uSequence = 0xFFFFFFFFu;
	uint32 m_uRecordTick = 0;   // the recording buffer's tick (Zenith_CommandBuffer tick accessor, row CN28, pending ECS)
	constexpr bool IsNull() const { return m_uSequence == 0xFFFFFFFFu; }
	friend constexpr bool operator==(const Zenith_CinematicTicket&, const Zenith_CinematicTicket&) = default;
};

/// @contract value Zenith_CinematicBinding
/// Ownership: Plain value (16 bytes); the caller owns the span of bindings it passes in a start description.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: m_ulNameHash is Zenith_Hash64 of the binding slot's UTF-8 name as authored ("Hero", "ShotCamera"). C++ only:
///   scripts pass binding names as strings, which the binding hashes.
/// Errors: None; the start playback validates names and entities (CINEMATIC_ERROR_BINDING_*).
/// Version: since Z2 Stage 4. Serialized field by field in the start payload.
/// @endcontract
struct Zenith_CinematicBinding
{
	uint64 m_ulNameHash = 0;
	Zenith_EntityHandle m_xEntity;
};

/// @contract functions Zenith_CinematicBindingFunctions
/// Ownership: A pure inline function; no state.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
inline Zenith_CinematicBinding Zenith_MakeCinematicBinding(Zenith_StringView strName, Zenith_EntityHandle xEntity)
{
	return Zenith_CinematicBinding{ Zenith_Hash64(strName), xEntity };
}

/// @contract enum Zenith_CinematicState
/// Ownership: Plain enumeration; the states of Playback.md "Instance state machine".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Values are serialized in the save section; values keep their meaning once shipped.
/// @endcontract
enum Zenith_CinematicState : uint8
{
	CINE_STATE_NONE = 0,          // no instance (resolve of a null or freed ID; a player that never started)
	CINE_STATE_BINDING,           // the sequence asset (and its video assets) are loading
	CINE_STATE_PREPARING,         // resident; clips and dialogue are being prefetched for the first lookahead window
	CINE_STATE_BLENDING_IN,       // begun; weight rising from 0 to 1
	CINE_STATE_PLAYING,           // weight 1, time advancing
	CINE_STATE_PAUSED,            // weight held, time held
	CINE_STATE_BLENDING_OUT,      // natural end or interrupt; weight falling to 0
	CINE_STATE_ENDED,             // weight 0 reached, skip applied, or (on a player row) the start failed; restore pending
	CINE_STATE_RELEASED,          // restored and released; the ID resolves until the record is freed
};

/// @contract enum Zenith_CinematicEndReason
/// Ownership: Plain enumeration; the Reason property of Zenith.Cinematics.Ended and of the lifecycle payload.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. Serialized; values keep their meaning once shipped.
/// @endcontract
enum Zenith_CinematicEndReason : uint8
{
	CINE_END_REASON_NONE = 0,
	CINE_END_REASON_COMPLETED,    // the sequence reached its duration and blended out
	CINE_END_REASON_INTERRUPTED,  // Zenith_RecordInterruptCinematic, or a take-over of every binding by a subsequent instance
	CINE_END_REASON_SKIPPED,      // Zenith_RecordSkipCinematic
	CINE_END_REASON_CANCELLED,    // interrupt or skip while BINDING or PREPARING, world detach, a re-cooked save; no lifecycle event
	CINE_END_REASON_FAILED,       // a binding or preparation failure, the last required binding destroyed, a re-cooked save
};

/// @contract functions Zenith_CinematicStartFlagBits
/// Ownership: constexpr bit constants of Zenith_CinematicStartDesc::m_uFlags and Zenith_CinematicStartPayload::m_uFlags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; unknown bits are rejected at playback (CINEMATIC_ERROR_INVALID_ARGUMENT); FROM_PLAYER is accepted only
///   from a buffer of Cinematics.Evaluate (Playback.md "Start playback" step 1).
/// Version: since Z2 Stage 4.
/// @endcontract
inline constexpr uint32 uCINE_START_FLAG_LOOP = 1u << 0;            // wrap to 0 at the duration instead of blending out
inline constexpr uint32 uCINE_START_FLAG_PAUSED = 1u << 1;          // begin in PAUSED at weight 1 (editor preview, tests)
inline constexpr uint32 uCINE_START_FLAG_NO_TAKEOVER = 1u << 2;     // refuse (CINEMATIC_ERROR_BINDING_DUPLICATE) instead of taking over
inline constexpr uint32 uCINE_START_FLAG_KNOWN_MASK = 0x7u;         // the bits a public recorder may set
inline constexpr uint32 uCINE_START_FLAG_FROM_PLAYER = 1u << 3;     // internal: a player autostart; bindings read from the player
inline constexpr uint32 uCINE_START_FLAG_BINDING_OVERFLOW = 1u << 31;   // internal: more than 64 bindings were given (rejected)
```

## Zenith_CinematicCommands header

This header carries the three lifecycle recording functions of `Zenith2/Docs/Architecture/LifecycleCommands.md` → "Cinematic start, interrupt and skip" and CINE's further recording functions. Each recording function takes the world's `Zenith_CinematicCommandKinds` after the buffer (the PH6 pattern; the lifecycle signature change is cross-doc row X290 of the entry file's "Cross-area rows"), because a kind ID is assigned at registration and a recorder must know it without a lookup service.

`Zenith2/Docs/Architecture/LifecycleCommands.md` → "Cinematic start, interrupt and skip" carries the `const Zenith_CinematicCommandKinds& xKinds` parameter on the three lifecycle recorders and `Zenith_CinematicStartDesc::m_uFlags`, `m_fRate`, `m_fStartSeconds`, `m_xTag` (a `Zenith_StringID`) and `m_xVoiceLocale` (X290, X339 (b)).

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_CinematicCommands.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithECS/Zenith_CommandBuffer.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithCinematics/Zenith_CinematicTypes.h"

class Zenith_Cinematic;   // Zenith_CinematicAsset.h

/// @contract enum Zenith_CinematicCommandKind
/// Ownership: Plain enumeration; the index into Zenith_CinematicCommandKinds::m_auKindIDs.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4. The registered names below are stable; the runtime kind IDs are not.
/// @endcontract
enum Zenith_CinematicCommandKind : uint8
{
	CINE_COMMAND_START = 0,      // "Cinematics.Start"       Zenith.Cinematics.StartPayload
	CINE_COMMAND_INTERRUPT,      // "Cinematics.Interrupt"   Zenith.Cinematics.InstancePayload
	CINE_COMMAND_SKIP,           // "Cinematics.Skip"        Zenith.Cinematics.InstancePayload
	CINE_COMMAND_SEEK,           // "Cinematics.Seek"        Zenith.Cinematics.SeekPayload
	CINE_COMMAND_SET_RATE,       // "Cinematics.SetRate"     Zenith.Cinematics.RatePayload
	CINE_COMMAND_PAUSE,          // "Cinematics.Pause"       Zenith.Cinematics.InstancePayload
	CINE_COMMAND_RESUME,         // "Cinematics.Resume"      Zenith.Cinematics.InstancePayload
	CINE_COMMAND_PRELOAD,        // "Cinematics.Preload"     Zenith.Cinematics.PreloadPayload
	CINE_COMMAND_RELEASE,        // "Cinematics.Release"     Zenith.Cinematics.PreloadPayload
	CINE_COMMAND_BEGIN,          // "Cinematics.Begin"       Zenith.Cinematics.InstancePayload   (CINE-internal)
	CINE_COMMAND_END,            // "Cinematics.End"         Zenith.Cinematics.EndPayload        (CINE-internal)
	CINE_COMMAND_COUNT,
};

/// @contract value Zenith_CinematicCommandKinds
/// Ownership: Plain value; filled once per world at AttachWorld from Zenith_CommandKindTable::FindByName.
/// Thread affinity: IMMUTABLE after AttachWorld; members non-const: the module fills it at AttachWorld.
/// Invalidation: None while the module is started.
/// Units: None. A kind ID is the command kind table's registration index + 1.
/// Errors: A missing CINE kind at attach is ZENITH_CHECK (CINE registered it at Start).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicCommandKinds
{
	uint32 m_auKindIDs[CINE_COMMAND_COUNT] = {};
};

/// @contract value Zenith_CinematicStartDesc
/// Ownership: Plain value; borrows m_axBindings for the duration of the recording call only (the recorder copies them).
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Seconds for blends and the start time; rate is a dimensionless multiplier of world time.
/// Errors: Out-of-range, NaN or infinite values are detected at playback (CINEMATIC_ERROR_INVALID_ARGUMENT, StartFailed
///   event), never asserted.
/// Version: since Z2 Stage 4. The first four members are those of Zenith2/Docs/Architecture/LifecycleCommands.md.
/// @endcontract
struct Zenith_CinematicStartDesc
{
	Zenith_AssetHandle<Zenith_Cinematic> m_xSequence;
	Zenith_Span<const Zenith_CinematicBinding> m_axBindings;   // at most uCINE_MAX_BINDINGS
	float m_fBlendInSeconds = 0.25f;                           // [0, 60]
	float m_fBlendOutSeconds = 0.25f;                          // [0, 60]
	float m_fRate = 1.0f;                                      // [0, 8]; quantised to Q16.16 at record
	float m_fStartSeconds = 0.0f;                              // [0, duration]
	Zenith_StringID m_xTag;                                    // a caller tag; the empty ID for none
	uint32 m_uFlags = 0;                                      // uCINE_START_FLAG_* within uCINE_START_FLAG_KNOWN_MASK
	Zenith_StringID m_xVoiceLocale;                            // the simulation voice-locale tag (AUDIO's Zenith.Audio.DialogueLocale EffectiveTag);
	                                                           // empty: lip-sync window 0 (Playback.md "Dialogue tracks")
};

/// @contract functions Zenith_CinematicRecording
/// Ownership: Each function appends one module record of the named kind to xBuffer; the buffer owns the bytes.
/// Thread affinity: OWNER_THREAD — the buffer's.
/// Invalidation: None.
/// Units: Seconds or microseconds as named; rates are multipliers.
/// Errors: Recording cannot fail beyond the buffer's own capacity (merge-time rejection, [Z2-N-CONTRACT-346]); semantic
///   errors (a stale instance, an unknown binding) are detected at playback, counted and reported through
///   Zenith.Cinematics.StartFailed or the cine.commands.rejected_delta counter, never asserted. A start with more than
///   uCINE_MAX_BINDINGS bindings records the first 64 and sets uCINE_START_FLAG_BINDING_OVERFLOW, which playback rejects.
///   A float that is NaN or infinite records uCINE_REJECTED_SENTINEL, which playback rejects.
/// Version: since Z2 Stage 4.
/// @endcontract
// Copies the bindings sorted by name hash (stable, so a duplicate name keeps its caller order) and returns the ticket
// computed from xBuffer.GetBaseKey() and xBuffer.GetRecordCount() before the append.
Zenith_CinematicTicket Zenith_RecordStartCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	const Zenith_CinematicStartDesc& xDesc);
void Zenith_RecordInterruptCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	Zenith_CinematicInstanceID xInstance);                                   // blends out from the current weight
void Zenith_RecordSkipCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	Zenith_CinematicInstanceID xInstance);                                   // jumps to the end, then restores
void Zenith_RecordSeekCinematicUs(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	Zenith_CinematicInstanceID xInstance, uint64 ulTimeUs, bool bFireEvents); // exact; clamped to [0, duration] at playback
void Zenith_RecordSeekCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	Zenith_CinematicInstanceID xInstance, float fSeconds, bool bFireEvents); // records llround(double(fSeconds) × 1e6)
void Zenith_RecordSetCinematicRate(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	Zenith_CinematicInstanceID xInstance, float fRate);                      // [0, 8]
void Zenith_RecordPauseCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	Zenith_CinematicInstanceID xInstance);
void Zenith_RecordResumeCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	Zenith_CinematicInstanceID xInstance);
void Zenith_RecordPreloadCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	const Zenith_AssetHandle<Zenith_Cinematic>& xSequence);                  // pins the sequence and its video
void Zenith_RecordReleaseCinematic(Zenith_CommandBuffer& xBuffer, const Zenith_CinematicCommandKinds& xKinds,
	const Zenith_AssetHandle<Zenith_Cinematic>& xSequence);                  // drops one preload pin
```

Float conversion at record, in `double`: blends and the start time are `llround(double(v) × 1e6)`, the rate `llround(double(v) × 65536)`, each clamped to [0, 0xFFFFFFFE]; a NaN or infinite input records `uCINE_REJECTED_SENTINEL` (0xFFFFFFFF) instead, so `llround` is never called on a non-finite value.

### Command payloads

Every payload is a registered `TYPE_KIND_STRUCT`, so command buffers serialise it by reflection and replays and saves never depend on the C++ layout (Z2-D-038); a `Zenith_StringID` field serialises as its text. Stable names are `Zenith.Cinematics.<Name>Payload`. Scripts never read a command payload.

```cpp
/// @contract value Zenith_CinematicStartPayload
/// Ownership: Plain value, the payload of Cinematics.Start; followed in the record by m_uBindingCount
///   Zenith_CinematicBinding records, reflected as the container property 11 Bindings.
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: Microseconds; rate in Q16.16. Size 72 + 16 × binding count bytes, at most 1,096.
/// Errors: None; Playback.md "Start playback" validates every field as untrusted.
/// Version: since Z2 Stage 4. Property IDs are the numbers in the comments.
/// @endcontract
struct Zenith_CinematicStartPayload           // "Zenith.Cinematics.StartPayload"
{
	Zenith_PersistentID m_xSequence;          // 1 Sequence (GUID, 16)
	Zenith_CinematicTicket m_xTicket;         // 2 Ticket (STRUCT, 16)
	Zenith_EntityHandle m_xPlayer;            // 3 Player (ENTITY, 8): the player row of an autostart, null otherwise
	Zenith_StringID m_xTag;                   // 4 Tag (STRING_ID, 4)
	uint32 m_uBlendInUs = 250000;            // 5 BlendInUs
	uint32 m_uBlendOutUs = 250000;           // 6 BlendOutUs
	uint32 m_uRateQ16 = uCINE_RATE_ONE_Q16;  // 7 RateQ16
	uint32 m_uStartUs = 0;                   // 8 StartUs
	uint32 m_uFlags = 0;                     // 9 Flags: uCINE_START_FLAG_*
	uint16 m_uBindingCount = 0;              // 10 BindingCount, ≤ 64
	uint16 m_uReserved = 0;
	Zenith_StringID m_xVoiceLocale;           // 12 VoiceLocale (STRING_ID, 4): the start description's m_xVoiceLocale, or the
	                                          //    player's property 11 for an autostart
};

/// @contract value Zenith_CinematicInstancePayload
/// Ownership: Plain value, the payload of Cinematics.Interrupt, Skip, Pause, Resume and Begin.
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: None. 8 bytes.
/// Errors: None; an unknown instance is counted at playback (cine.commands.rejected_delta).
/// Version: since Z2 Stage 4. 1 Instance (STRUCT Zenith.Cinematics.InstanceID).
/// @endcontract
struct Zenith_CinematicInstancePayload { Zenith_CinematicInstanceID m_xInstance; };

/// @contract value Zenith_CinematicSeekPayload
/// Ownership: Plain value, the payload of Cinematics.Seek.
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: Microseconds. 24 bytes.
/// Errors: None; the target is clamped to [0, duration] at playback.
/// Version: since Z2 Stage 4. 1 Instance, 2 TimeUs (U64), 3 Flags (U32: bit 0 fire events).
/// @endcontract
struct Zenith_CinematicSeekPayload { Zenith_CinematicInstanceID m_xInstance; uint64 m_ulTimeUs = 0; uint32 m_uFlags = 0; uint32 m_uReserved = 0; };

/// @contract value Zenith_CinematicRatePayload
/// Ownership: Plain value, the payload of Cinematics.SetRate.
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: Q16.16. 16 bytes.
/// Errors: None; a rate above uCINE_RATE_MAX_Q16 or equal to the sentinel is rejected at playback.
/// Version: since Z2 Stage 4. 1 Instance, 2 RateQ16 (U32).
/// @endcontract
struct Zenith_CinematicRatePayload { Zenith_CinematicInstanceID m_xInstance; uint32 m_uRateQ16 = uCINE_RATE_ONE_Q16; uint32 m_uReserved = 0; };

/// @contract value Zenith_CinematicPreloadPayload
/// Ownership: Plain value, the payload of Cinematics.Preload and Release.
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: None. 16 bytes.
/// Errors: None; a null GUID is counted at playback.
/// Version: since Z2 Stage 4. 1 Sequence (GUID).
/// @endcontract
struct Zenith_CinematicPreloadPayload { Zenith_PersistentID m_xSequence; };

/// @contract value Zenith_CinematicEndPayload
/// Ownership: Plain value, the payload of Cinematics.End.
/// Thread affinity: IMMUTABLE; members non-const: a registered payload struct that command recording and playback write
///   field by field.
/// Invalidation: None.
/// Units: None. 16 bytes.
/// Errors: None.
/// Version: since Z2 Stage 4. 1 Instance, 2 Reason (U8, Zenith_CinematicEndReason).
/// @endcontract
struct Zenith_CinematicEndPayload { Zenith_CinematicInstanceID m_xInstance; uint8 m_eReason = 0; uint8 m_auReserved[7] = {}; };
```

| Kind | Registered name | Payload | Allowed phases (mask) | Max payload bytes | Playback |
|---|---|---|---|---|---|
| `CINE_COMMAND_START` | `Cinematics.Start` | `Zenith.Cinematics.StartPayload` | 1–10 (`0x7FE`) | 1,096 | Playback.md "Start playback" |
| `CINE_COMMAND_INTERRUPT` | `Cinematics.Interrupt` | `Zenith.Cinematics.InstancePayload` | 1–10 | 8 | Playback.md "Interrupt playback" |
| `CINE_COMMAND_SKIP` | `Cinematics.Skip` | `Zenith.Cinematics.InstancePayload` | 1–10 | 8 | Playback.md "Skip playback" |
| `CINE_COMMAND_SEEK` | `Cinematics.Seek` | `Zenith.Cinematics.SeekPayload` | 1–10 | 24 | Playback.md "Seek playback" |
| `CINE_COMMAND_SET_RATE` | `Cinematics.SetRate` | `Zenith.Cinematics.RatePayload` | 1–10 | 16 | Playback.md "Rate pause and resume playback" |
| `CINE_COMMAND_PAUSE` | `Cinematics.Pause` | `Zenith.Cinematics.InstancePayload` | 1–10 | 8 | same |
| `CINE_COMMAND_RESUME` | `Cinematics.Resume` | `Zenith.Cinematics.InstancePayload` | 1–10 | 8 | same |
| `CINE_COMMAND_PRELOAD` | `Cinematics.Preload` | `Zenith.Cinematics.PreloadPayload` | 1–10 | 16 | Playback.md "Asset binding" |
| `CINE_COMMAND_RELEASE` | `Cinematics.Release` | `Zenith.Cinematics.PreloadPayload` | 1–10 | 16 | Playback.md "Asset binding" |
| `CINE_COMMAND_BEGIN` | `Cinematics.Begin` | `Zenith.Cinematics.InstancePayload` | 1 (`0x2`) | 8 | Playback.md "Begin" (recorded only by `Cinematics.Evaluate`) |
| `CINE_COMMAND_END` | `Cinematics.End` | `Zenith.Cinematics.EndPayload` | 1 (`0x2`) | 16 | Playback.md "End playback" (recorded only by `Cinematics.Evaluate`) |

The two internal kinds, and a start with `uCINE_START_FLAG_FROM_PLAYER`, are refused at playback (counted in `cine.commands.rejected_delta`, logged once per world) when the recording merge key's system order is not the world record's `m_uEvaluateSystemOrder` (written by every `Cinematics.Evaluate` run from `Zenith_SystemContext::GetSystemOrder()`), so a script or another module cannot forge a begin, an end or an autostart. Each playback function is CINE's, registered with `m_pUserData` = the `Zenith_CinematicsModule`; it finds the world record by `Zenith_WorldPlaybackAccess::GetWorldTag()`, and a tag with no cinematic world drops the command and counts `cine.commands.no_world_delta`.

## Zenith_CinematicAsset header

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_CinematicAsset.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithCinematics/Zenith_CinematicTypes.h"
#include "ZenithCinematics/Zenith_CinematicErrors.h"

/// @contract enum Zenith_CinematicTrackKind
/// Ownership: Plain enumeration; the kind byte of a cooked track record (Formats.md "Track table section").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown kind in a cooked file is CINEMATIC_ERROR_CORRUPT.
/// Version: since Z2 Stage 4 (VIDEO at Stage 4 behind D-SEL-VIDEO; COMMAND sections naming VFX.Spawn evaluate from Stage 5).
/// @endcontract
enum Zenith_CinematicTrackKind : uint8
{
	CINE_TRACK_KIND_TRANSFORM = 1,    // local transform of a bound entity (Playback.md "Transform tracks")
	CINE_TRACK_KIND_CAMERA,           // pose, lens, cuts and camera lighting of a bound camera entity
	CINE_TRACK_KIND_ANIMATION,        // clip sections played through ANIM's cinematic-tagged slots
	CINE_TRACK_KIND_AUDIO,            // audio event sections posted through AUDIO's command kinds
	CINE_TRACK_KIND_EVENT,            // ECS events emitted at keys
	CINE_TRACK_KIND_DIALOGUE,         // dialogue lines: prepare, time-locked play, caption cue, optional face clip
	CINE_TRACK_KIND_SUBTITLE,         // caption cues without audio
	CINE_TRACK_KIND_PROPERTY,         // reflected component properties of a bound entity, curve or step
	CINE_TRACK_KIND_COMMAND,          // any registered module command kind at keys, payload by property name
	CINE_TRACK_KIND_OVERLAY,          // screen fade colour and letterbox
	CINE_TRACK_KIND_VIDEO,            // video sections composited or bound to a texture surface
	CINE_TRACK_KIND_MARKER,           // named markers raising Zenith.Cinematics.Marker
	CINE_TRACK_KIND_COUNT,
};

/// @contract enum Zenith_CinematicTransformSpace
/// Ownership: Plain enumeration; the space byte of a cooked transform or camera track (Formats.md "Track table section").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a value above 2 in a cooked file is CINEMATIC_ERROR_CORRUPT (check L6).
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CinematicTransformSpace : uint8
{
	CINE_TRANSFORM_SPACE_PARENT = 0,   // the sampled value is the local transform
	CINE_TRANSFORM_SPACE_WORLD = 1,    // a world transform converted into the parent's space
	CINE_TRANSFORM_SPACE_ANCHOR = 2,   // relative to an anchor slot's world transform
};

/// @contract enum Zenith_CinematicInterpolation
/// Ownership: Plain enumeration; the interpolation byte of a cooked key (Formats.md "Key table section").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a value above 2 in a cooked file is CINEMATIC_ERROR_CORRUPT (check L7).
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CinematicInterpolation : uint8
{
	CINE_INTERP_CONSTANT = 0,
	CINE_INTERP_LINEAR = 1,
	CINE_INTERP_CUBIC = 2,
};

/// @contract value Zenith_CinematicBindingInfo
/// Ownership: Plain value describing one binding slot of a resident sequence; strings point into the asset.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: m_strName is invalid once the asset's last handle is released.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicBindingInfo
{
	Zenith_StringView m_strName;
	uint64 m_ulNameHash = 0;
	uint32 m_uFlags = 0;               // uCINE_BINDING_FLAG_*
	uint32 m_uRequiredTypeCount = 0;   // component types the slot's tracks require
};

/// @contract functions Zenith_CinematicBindingFlagBits
/// Ownership: constexpr bit constants of Zenith_CinematicBindingInfo::m_uFlags and the cooked binding record.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; a set reserved bit in a cooked file is CINEMATIC_ERROR_CORRUPT (check L12).
/// Version: since Z2 Stage 4. SPAWNABLE is set from Stage 5 for a binding whose document names a "prefab"; always 0 in Stage 4.
/// @endcontract
inline constexpr uint32 uCINE_BINDING_FLAG_REQUIRED = 1u << 0;      // a start without a live entity for it fails
inline constexpr uint32 uCINE_BINDING_FLAG_CAMERA = 1u << 1;        // the slot carries a camera track
inline constexpr uint32 uCINE_BINDING_FLAG_SPAWNABLE = 1u << 2;     // Stage 5: the slot's entity is spawned from the binding's prefab

/// @contract type Zenith_Cinematic
/// Ownership: The runtime object of ASSET_KIND_CINEMATIC (14), created by CINE's kind loader in the asset slot and freed
///   by the registry when the last handle is released; immutable after load.
/// Thread affinity: IMMUTABLE; members non-const: its loader writes them after constructing the object. Every member is
///   a const read of loaded data.
/// Invalidation: Pointers and views into it are valid while the caller holds a handle.
/// Units: Microseconds for times; durations are whole microseconds.
/// Errors: None; the loader rejects every malformed file (Formats.md "Load validation").
/// Version: since Z2 Stage 4. Cooked format version 1 (Formats.md "Cooked cinematic file").
/// @endcontract
class Zenith_Cinematic
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_CINEMATIC;
	uint64 GetDurationUs() const;
	uint32 GetBindingCount() const;
	Zenith_CinematicBindingInfo GetBinding(uint32 uIndex) const;       // uIndex < count: ZENITH_ASSERT
	uint32 FindBinding(uint64 ulNameHash) const;                    // 0xFFFFFFFF when absent; O(log n)
	uint32 GetTrackCount() const;
	Zenith_CinematicTrackKind GetTrackKind(uint32 uIndex) const;
	Zenith_StringView GetTrackName(uint32 uIndex) const;
	uint32 GetSectionCount() const;                                  // all tracks, ≤ uCINE_MAX_SECTIONS
	uint32 GetLatePropertyCount() const;   // (track, property) pairs Cinematics.ApplyLate writes
	uint32 GetMarkerCount() const;
	Zenith_StringView GetMarkerName(uint32 uIndex) const;
	Zenith_StringID GetMarkerNameID(uint32 uIndex) const;            // interned at load
	uint64 GetMarkerTimeUs(uint32 uIndex) const;
	uint32 FindMarker(Zenith_StringID xName) const;                  // 0xFFFFFFFF when absent; O(log n)
	Zenith_Span<const Zenith_PersistentID> GetDependencies() const;   // clips, sounds, videos, dialogue
	uint64 GetContentHash() const;                                   // the cooked header's content hash
private:
	// elaboration: the loaded section views of Formats.md "Runtime layout"
};

/// @contract functions Zenith_CinematicValidation
/// Ownership: A pure function over caller-owned bytes; allocates only from xScratch.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: CINEMATIC_ERROR_CORRUPT with m_uDetail = the failing check (ASSET's C1–C12 as 101–112, then L1–L12 as 1–12),
///   CINEMATIC_ERROR_VERSION; never asserts on the bytes (they are untrusted).
/// Version: since Z2 Stage 4. The same checks, in the same order, as CINE's kind loader (Formats.md "Load validation").
/// @endcontract
Zenith_Result<void, Zenith_CinematicError> Zenith_ValidateCinematicFile(Zenith_ByteSpan xFile, Zenith_Arena& xScratch);
```

`Zenith_Cinematic` is the runtime object type of kind 14. ASSET's kind table names `Zenith_CinematicAsset`; Architecture's "Assumed types" names `Zenith_Cinematic`, which `Zenith2/Docs/Architecture/LifecycleCommands.md` uses in `Zenith_AssetHandle<Zenith_Cinematic>`. CINE adopts `Zenith_Cinematic` and asks ASSET to rename the kind-table entry (row CN1 of the entry file's "Cross-area rows"; KIND-TABLE-OWNER: ASSET alone edits the table). The header file keeps the name `Zenith_CinematicAsset.h`, the convention of `Zenith_SkeletonAsset.h`. `Zenith_ValidateCinematicFile` and `Zenith_ValidateVideoFile` are what ZenithCook's validators call ("Cook steps" of `Zenith2/Docs/Design/CINE/Authoring.md`), so no tool compiles a ZenithCinematics `Internal/` source.

## Zenith_VideoAsset header

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_VideoAsset.h
#pragma once
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithCinematics/Zenith_CinematicTypes.h"
#include "ZenithCinematics/Zenith_CinematicErrors.h"

/// @contract enum Zenith_VideoCodec
/// Ownership: Plain enumeration; the codec ID of a cooked video header (Formats.md "Cooked video file").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; an unknown value in a cooked file is CINEMATIC_ERROR_CORRUPT, a known value the decoder cannot play is
///   CINEMATIC_ERROR_VIDEO_UNSUPPORTED.
/// Version: since Z2 Stage 4. Codec IDs are never reused; a new codec takes the next value.
/// @endcontract
enum Zenith_VideoCodec : uint8
{
	VIDEO_CODEC_NONE = 0,
	VIDEO_CODEC_H264 = 1,     // ITU-T H.264, Annex B start codes stripped, length-prefixed NAL units (4-byte big-endian)
	VIDEO_CODEC_HEVC = 2,     // ITU-T H.265, same framing; decoded only where the backend reports support
};

/// @contract value Zenith_VideoInfo
/// Ownership: Plain value copied out of a resident video asset.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: Pixels; microseconds; frames per second as a rational m_uFrameRateNum / m_uFrameRateDen.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_VideoInfo
{
	Zenith_VideoCodec m_eCodec = VIDEO_CODEC_NONE;
	uint8 m_uColourPrimaries = 1;      // ITU-T H.273 code points: 1 = BT.709
	uint8 m_uTransfer = 1;             // 1 = BT.709, 16 = PQ, 18 = HLG
	uint8 m_uMatrix = 1;               // 1 = BT.709, 9 = BT.2020 non-constant
	uint8 m_bFullRange = 0;
	uint8 m_uBitDepth = 8;             // 8 or 10
	uint8 m_auReserved[2] = {};
	uint32 m_uWidth = 0;               // the display width (the decoded frame is cropped to it)
	uint32 m_uHeight = 0;              // the display height
	uint32 m_uFrameRateNum = 0;
	uint32 m_uFrameRateDen = 1;
	uint32 m_uFrameCount = 0;
	uint64 m_ulDurationUs = 0;
	Zenith_PersistentID m_xAudioSound;  // the AUDIO sound sub-asset cooked from the video's audio, null when silent
};

/// @contract type Zenith_VideoAsset
/// Ownership: The runtime object of ASSET_KIND_VIDEO (21), created by CINE's kind loader; holds the header, decoder
///   configuration and frame index sections resident; the bitstream section stays on disk and is read through
///   Zenith_AssetReader::ReadRange by the video thread.
/// Thread affinity: IMMUTABLE; members non-const: its loader writes them after constructing the object. Members are
///   const reads; the video thread reads the bitstream through its own reader.
/// Invalidation: Views are valid while the caller holds a handle.
/// Units: Microseconds; bytes.
/// Errors: None; the loader rejects malformed files (CINEMATIC_ERROR_CORRUPT); a codec the decoder cannot play is reported
///   by the player (CINEMATIC_ERROR_VIDEO_UNSUPPORTED), not by the loader, because the cooked file is platform-neutral.
/// Version: since Z2 Stage 4. Cooked format version 1.
/// @endcontract
class Zenith_VideoAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_VIDEO;
	const Zenith_VideoInfo& GetInfo() const;
	uint32 FindFrameAtOrBefore(uint64 ulPtsUs) const;   // presentation-order index, O(log n)
	uint32 FindKeyframeAtOrBefore(uint32 uFrame) const; // O(log k)
private:
	// elaboration: Formats.md "Cooked video file" section views
};

/// @contract functions Zenith_VideoValidation
/// Ownership: A pure function over caller-owned bytes; allocates only from xScratch.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: CINEMATIC_ERROR_CORRUPT with m_uDetail = the failing check (ASSET's C1–C12 as 101–112, then V1–V7 as 1–7),
///   CINEMATIC_ERROR_VERSION; never asserts on the bytes.
/// Version: since Z2 Stage 4. The same checks as CINE's kind loader (Formats.md "Video load validation").
/// @endcontract
Zenith_Result<void, Zenith_CinematicError> Zenith_ValidateVideoFile(Zenith_ByteSpan xFile, Zenith_Arena& xScratch);
```

## Zenith_CinematicComponents header

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_CinematicComponents.h
#pragma once
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithECS/Zenith_ContainerRef.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithCinematics/Zenith_CinematicTypes.h"

/// @contract value Zenith_CinematicBindingEntry
/// Ownership: Plain value; an element of the CONTAINER property 7 Bindings of Zenith_CinematicPlayerComponent.
/// Thread affinity: IMMUTABLE; members non-const: a registered struct its containing component, payload or result writes
///   field by field.
/// Invalidation: None.
/// Units: m_xName is the binding slot's name, interned; the start playback hashes its text with Zenith_Hash64.
/// Errors: None.
/// Version: since Z2 Stage 4. Reflected struct "Zenith.Cinematics.BindingEntry": 1 Name (STRING_ID), 2 Entity (ENTITY).
/// @endcontract
struct Zenith_CinematicBindingEntry
{
	Zenith_StringID m_xName;
	Zenith_EntityHandle m_xEntity;
};

/// @contract type Zenith_CinematicPlayerComponent
/// Ownership: An ECS component ("Zenith.Cinematics.Player") stored in its column; scenes author it. The Bindings container
///   is stored out of line in the world's container heap (Zenith_ContainerRef).
/// Thread affinity: PHASE(PHASE_INPUT) for properties 8, 9 and 10, which Cinematics.Evaluate owns; properties 1–7 are
///   authored and changed through SetProperty and SetContainer commands at sync points.
/// Invalidation: As every component (Zenith2/Docs/Contracts.md "Handle kinds").
/// Units: Seconds for blends; rate is a multiplier.
/// Errors: An invalid authored value is clamped at the start it triggers and counted (cine.player.clamped_delta); a failed
///   start is reported in State (ENDED) and LastError.
/// Version: since Z2 Stage 4. Property IDs below are stable; a removed property's ID is never reused.
/// @endcontract
struct Zenith_CinematicPlayerComponent
{
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;   // holds a Zenith_AssetHandle and a Zenith_ContainerRef
	Zenith_AssetHandle<Zenith_Cinematic> m_xSequence;          // 1 Sequence (ASSET_REF, kind 14)
	bool m_bPlayOnStart = false;                               // 2 PlayOnStart
	bool m_bLoop = false;                                      // 3 Loop
	float m_fBlendInSeconds = 0.25f;                           // 4 BlendInSeconds
	float m_fBlendOutSeconds = 0.25f;                          // 5 BlendOutSeconds
	float m_fRate = 1.0f;                                      // 6 Rate
	Zenith_ContainerRef m_xBindings;                           // 7 Bindings (CONTAINER of Zenith.Cinematics.BindingEntry)
	Zenith_CinematicInstanceID m_xInstance;                    // 8 Instance (written by Cinematics.Evaluate)
	uint8 m_eState = CINE_STATE_NONE;                         // 9 State (written by Cinematics.Evaluate)
	uint8 m_uReserved = 0;
	uint16 m_uLastError = 0;                                  // 10 LastError (written by Cinematics.Evaluate; 0 none)
	Zenith_StringID m_xVoiceLocale;                            // 11 VoiceLocale (the voice-locale tag an autostart passes)
};
```

| Property | ID | Kind | Authority (owning phase, writer source) | Flags |
|---|---|---|---|---|
| Sequence | 1 | `ASSET_REF` | authored; `SetProperty` | none |
| PlayOnStart | 2 | `BOOL` | authored | none |
| Loop | 3 | `BOOL` | authored | none |
| BlendInSeconds | 4 | `F32` | authored | none |
| BlendOutSeconds | 5 | `F32` | authored | none |
| Rate | 6 | `F32` | authored | none |
| Bindings | 7 | `CONTAINER` of `Zenith.Cinematics.BindingEntry` (`Zenith_ContainerRef`, 8 bytes in memory, `Zenith2/Docs/Design/ECS/Reflect.md` → "Zenith_TypeLayout header") | authored; `SetContainer` | none |
| Instance | 8 | `STRUCT` `Zenith.Cinematics.InstanceID` | `PHASE_INPUT`, `Cinematics.Evaluate`, single writer | `PROPERTY_FLAG_TRANSIENT` is not set: saved, so a restored player does not start twice |
| State | 9 | `U8` | `PHASE_INPUT`, `Cinematics.Evaluate`, single writer | saved |
| LastError | 10 | `U16` (`Zenith_CinematicErrorCode`) | `PHASE_INPUT`, `Cinematics.Evaluate`, single writer | saved |
| VoiceLocale | 11 | `STRING_ID` | authored; `SetProperty` (a script may mirror AUDIO's recorded `Zenith.Audio.DialogueLocale` `EffectiveTag` into it; ruling GmB33-DIALOGUE-LOCALE) | none; the autostart copies it into the start payload's property 12 |

A player with `PlayOnStart`, `State == NONE`, a null `Instance` and no pending autostart records one `Cinematics.Start` from `Cinematics.Evaluate` in the tick after it becomes visible (spawn or scene activation), with its blends, rate and the loop flag; the pending autostart is committed only when the merge accepts the record's buffer, so a rejected buffer autostarts again in the next tick (`Zenith2/Docs/Design/CINE/Playback.md` → "Cinematics.Evaluate" steps 1 and 2). The start playback reads the player's `Bindings` with `ReadContainer` and binds the entity carrying the player to the slot named `Self` when the sequence has one (`Zenith2/Docs/Design/CINE/Playback.md` → "Start playback"). From the Evaluate after the start's playback, `Instance` names the instance and `State` mirrors its state. A start that fails writes `State = CINE_STATE_ENDED` and `LastError` = the error code, and a start that was dropped (no ticket entry once its window has passed) writes `State = CINE_STATE_ENDED` and `LastError = CINEMATIC_ERROR_TICKET_EXPIRED`; the player never retries (`State` is no longer `NONE`). Scene documents author `Bindings` by entity name: SCENE's reference fields resolve the names to entity references at cook (`Zenith2/Docs/Design/SCENE/SceneAPI.md` → "Zenith_SceneDocument header").

## Zenith_CinematicEvents header

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_CinematicEvents.h
#pragma once
#include "ZenithCore/Zenith_StringID.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCinematics/Zenith_CinematicTypes.h"

/// @contract value Zenith_CinematicStartedEvent
/// Ownership: Plain value; the C++ mirror of the TYPE_KIND_EVENT "Zenith.Cinematics.Started", emitted by
///   Cinematics.EmitEvents (phase 8) and delivered in phase 9 of its emission tick ([Z2-N-ARCH-478]).
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None.
/// Units: None. Every property is exact in Luau.
/// Errors: None.
/// Version: since Z2 Stage 4. Property IDs are the numbers in the comments.
/// @endcontract
struct Zenith_CinematicStartedEvent       // "Zenith.Cinematics.Started"
{
	Zenith_CinematicInstanceID m_xInstance;   // 1 Instance (STRUCT Zenith.Cinematics.InstanceID)
	Zenith_PersistentID m_xSequence;          // 2 Sequence (GUID)
	Zenith_StringID m_xTag;                   // 3 Tag (STRING_ID)
	Zenith_CinematicTicket m_xTicket;         // 4 Ticket (STRUCT Zenith.Cinematics.Ticket) — matches the ticket a script holds
};

/// @contract value Zenith_CinematicEndedEvent
/// Ownership: Plain value; the C++ mirror of "Zenith.Cinematics.Ended", emitted and delivered as Started.
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None.
/// Units: Microseconds for TimeUs (≤ 3.6 × 10^9, exact in Luau).
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicEndedEvent         // "Zenith.Cinematics.Ended"
{
	Zenith_CinematicInstanceID m_xInstance;   // 1 Instance
	uint8 m_eReason = 0;                     // 2 Reason (U8, Zenith_CinematicEndReason)
	Zenith_StringID m_xTag;                   // 3 Tag
	uint64 m_ulTimeUs = 0;                   // 4 TimeUs: the sequence time at the end
};

/// @contract value Zenith_CinematicMarkerEvent
/// Ownership: Plain value; the C++ mirror of "Zenith.Cinematics.Marker", emitted and delivered as Started.
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None.
/// Units: Microseconds for TimeUs.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicMarkerEvent        // "Zenith.Cinematics.Marker"
{
	Zenith_CinematicInstanceID m_xInstance;   // 1 Instance
	Zenith_StringID m_xMarker;                // 2 Marker (STRING_ID, the marker name; a script compares it with a string)
	uint32 m_uMarkerIndex = 0;               // 3 MarkerIndex (U32)
	uint64 m_ulTimeUs = 0;                   // 4 TimeUs: the marker's authored time
	Zenith_StringID m_xTag;                   // 5 Tag
};

/// @contract value Zenith_CinematicStartFailedEvent
/// Ownership: Plain value; the C++ mirror of "Zenith.Cinematics.StartFailed", emitted and delivered as Started.
/// Thread affinity: IMMUTABLE; members non-const: an ECS event the event queue constructs from its default object and
///   writes property by property.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicStartFailedEvent   // "Zenith.Cinematics.StartFailed"
{
	Zenith_CinematicTicket m_xTicket;         // 1 Ticket (STRUCT)
	uint16 m_uError = 0;                     // 2 Error (U16, Zenith_CinematicErrorCode)
	uint16 m_uDetail = 0;                    // 3 Detail: the binding or slot or track index the error names, 0xFFFF none
	Zenith_PersistentID m_xSequence;          // 4 Sequence
	Zenith_StringID m_xTag;                   // 5 Tag
};

/// @contract value Zenith_CinematicLifecyclePayload
/// Ownership: Plain value; Zenith_LifecycleEvent::m_pPayload points at one for the four cinematic lifecycle events.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: The pointer is valid for the listener call only.
/// Units: Microseconds. C++ only: lifecycle events reach C++ listeners, never a script, so the 64-bit name hash is safe.
/// Errors: None.
/// Version: since Z2 Stage 4. Reflected struct "Zenith.Cinematics.LifecyclePayload".
/// @endcontract
struct Zenith_CinematicLifecyclePayload
{
	Zenith_CinematicInstanceID m_xInstance;
	uint64 m_ulBindingNameHash = 0;   // the slot the event's m_xEntity is bound to; 0 for the null-entity event
	uint64 m_ulTimeUs = 0;            // the sequence time at the dispatch
	uint8 m_eReason = 0;              // Zenith_CinematicEndReason for END, 0 otherwise
	uint8 m_auReserved[7] = {};
};
```

## Zenith_CinematicsWorld header

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_CinematicsWorld.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCinematics/Zenith_CinematicErrors.h"
#include "ZenithCinematics/Zenith_CinematicCommands.h"

/// @contract value Zenith_CinematicInstanceInfo
/// Ownership: Plain value copied out of the world record.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None (a copy).
/// Units: Microseconds; rate in Q16.16; weight in [0, 1].
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicInstanceInfo
{
	Zenith_CinematicInstanceID m_xInstance;
	Zenith_PersistentID m_xSequence;
	Zenith_CinematicState m_eState = CINE_STATE_NONE;
	Zenith_CinematicEndReason m_eEndReason = CINE_END_REASON_NONE;
	uint16 m_uBindingCount = 0;
	uint32 m_uRateQ16 = uCINE_RATE_ONE_Q16;
	uint64 m_ulTimeUs = 0;
	uint64 m_ulDurationUs = 0;
	float m_fWeight = 0.0f;
	uint32 m_uFlags = 0;
	Zenith_StringID m_xTag;
	uint64 m_ulBeginTick = 0;
};

/// @contract type Zenith_CinematicsWorld
/// Ownership: CINE's world resource "Zenith.Cinematics.World", owned by the module's world record from AttachWorld to
///   DetachWorld; registered with owning phase PHASE_INPUT.
/// Thread affinity: PHASE(PHASE_INPUT) for writes, which only CINE's own systems, merge functions, playbacks and handlers
///   perform. The const members below may be called by any system that declares ReadResource("Zenith.Cinematics.World")
///   in a phase after PHASE_INPUT, and on the sync-point node.
/// Invalidation: The reference handed out by the system context is valid for the node's run.
/// Units: Microseconds; ticks.
/// Errors: ResolveTicket returns CINEMATIC_ERROR_TICKET_PENDING (no entry and the ticket's record tick is the world record's
///   current tick, the tick of the latest Cinematics.Evaluate run),
///   CINEMATIC_ERROR_TICKET_EXPIRED (no entry and it was recorded in an earlier tick: its window passed or its start was
///   dropped), or the start failure's code; GetInstanceInfo returns CINEMATIC_ERROR_NO_INSTANCE.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_CinematicsWorld
{
public:
	const Zenith_CinematicCommandKinds& GetCommandKinds() const;
	Zenith_Result<Zenith_CinematicInstanceID, Zenith_CinematicError> ResolveTicket(const Zenith_CinematicTicket& xTicket) const;
	Zenith_Result<Zenith_CinematicInstanceInfo, Zenith_CinematicError> GetInstanceInfo(Zenith_CinematicInstanceID xInstance) const;
	uint32 GetInstanceCount() const;                               // instances not RELEASED
	Zenith_CinematicInstanceID GetInstanceAt(uint32 uIndex) const;  // ascending instance ID
	Zenith_CinematicInstanceID FindByTag(Zenith_StringID xTag) const;  // lowest live ID with the tag
	Zenith_CinematicInstanceID FindByEntity(Zenith_EntityHandle xEntity) const;   // the instance writing it
	bool IsAnyPlaying() const;                                      // a state in BLENDING_IN..BLENDING_OUT
private:
	// elaboration: Playback.md "World record layout"
};
```

## Zenith_CinematicEvaluator header

The evaluator samples a resident sequence at a time with no world: the editor's scrub preview, the unit tests and the cook use it, and `Cinematics.Evaluate` uses the same functions, so a preview and a played tick evaluate identically.

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_CinematicEvaluator.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithCore/Zenith_Vector3.h"
#include "ZenithCinematics/Zenith_CinematicAsset.h"

/// @contract value Zenith_CinematicTransformSample
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: Metres; unit quaternion; scale is dimensionless.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicTransformSample
{
	Zenith_Vector3 m_xPosition;
	Zenith_Quaternion m_xRotation;
	Zenith_Vector3 m_xScale{ 1.0f, 1.0f, 1.0f };
	uint32 m_uChannelMask = 0;                                  // bit 0 position, 1 rotation, 2 scale: driven at this time
	Zenith_CinematicTransformSpace m_eSpace = CINE_TRANSFORM_SPACE_PARENT;
	uint8 m_auReserved[3] = {};
};

/// @contract value Zenith_CinematicCameraSample
/// Ownership: Plain value.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: None.
/// Units: Degrees for the vertical field of view; metres; millimetres for focal length and sensor width; seconds for
///   shutter; EV100.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicCameraSample
{
	Zenith_CinematicTransformSample m_xPose;
	float m_fFovYDegrees = 60.0f;
	float m_fNearMetres = 0.1f;
	float m_fFarMetres = 1000.0f;
	float m_fFocalLengthMm = 35.0f;
	float m_fSensorWidthMm = 36.0f;
	float m_fFocusDistanceMetres = 10.0f;
	float m_fAperture = 2.8f;
	float m_fExposureCompensation = 0.0f;
	uint32 m_uCutIndex = 0;       // the index of the camera-cut key at or before the time; a change between ticks is a cut
	uint32 m_uLensMask = 0;       // bit per lens channel the track animates (FovY, Near, Far, Focal, Sensor, Focus, Aperture, Exposure)
};

/// @contract functions Zenith_CinematicEvaluation
/// Ownership: Pure functions over a resident Zenith_Cinematic; no allocation; the caller owns every output.
/// Thread affinity: ANY_THREAD. The asset is immutable.
/// Invalidation: None.
/// Units: Microseconds for times.
/// Errors: An index outside the asset's track count or a track of another kind is ZENITH_ASSERT (programmer error); a
///   time beyond the duration clamps to the duration.
/// Version: since Z2 Stage 4. The evaluation is bit-identical for the same asset, track and time on one build.
/// @endcontract
Zenith_CinematicTransformSample Zenith_EvaluateCinematicTransform(const Zenith_Cinematic& xSequence, uint32 uTrack,
	uint64 ulTimeUs);
Zenith_CinematicCameraSample Zenith_EvaluateCinematicCamera(const Zenith_Cinematic& xSequence, uint32 uTrack,
	uint64 ulTimeUs);
float Zenith_EvaluateCinematicChannel(const Zenith_Cinematic& xSequence, uint32 uTrack, uint32 uChannel,
	uint64 ulTimeUs);
// Writes the indices of the sections of uTrack active at ulTimeUs (ascending start, at most axOut.GetSize()) and their
// section weights (ease curves of Playback.md "Section blending"); returns the count written.
uint32 Zenith_GetActiveCinematicSections(const Zenith_Cinematic& xSequence, uint32 uTrack, uint64 ulTimeUs,
	Zenith_Span<uint32> auOutSections, Zenith_Span<float> afOutWeights);
// Writes the indices of the event keys of uTrack with time in (ulFromUs, ulToUs] (both bounds in sequence time,
// ulFromUs == ulToUs writes none; ulFromUs = UINT64_MAX means "from before 0", which includes keys at 0); returns the count.
uint32 Zenith_GetCinematicKeysInRange(const Zenith_Cinematic& xSequence, uint32 uTrack, uint64 ulFromUs,
	uint64 ulToUs, Zenith_Span<uint32> auOutKeys);
```

## Zenith_CinematicCapture header

```cpp
// Zenith2/Engine/ZenithCinematics/Zenith_CinematicCapture.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithCinematics/Zenith_CinematicErrors.h"

class Zenith_DevFileWriter;   // ZenithProfiling (Zenith2/Docs/Design/DEVT/Console.md "Zenith_DevFileWriter header")

/// @contract value Zenith_CinematicCaptureDesc
/// Ownership: Plain value; borrows the strings for the Begin call only.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: Pixels; frames per second.
/// Errors: Validated by Begin (CINEMATIC_ERROR_INVALID_ARGUMENT).
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicCaptureDesc
{
	Zenith_StringView m_strName;            // [A-Za-z0-9_-]{1,64}; the capture directory user:/devtools/captures/<name>/
	uint32 m_uWidth = 1920;                // [64, 3840], even (a 32-bit frame stays below uDEV_FILE_MAX_BYTES)
	uint32 m_uHeight = 1080;               // [64, 2160], even
	uint32 m_uFrameRate = 60;              // must equal sim.tickHz (one tick per captured frame)
	bool m_bCaptureAudio = true;            // per-second PCM chunks through the audio capture bridge (offline backend)
	bool m_bCaptureSubtitles = true;        // subtitles.json from the caption cues
	bool m_bIncludeUI = false;              // read back after UIComposite instead of after VideoComposite
};

/// @contract enum Zenith_CinematicCaptureState
/// Ownership: Plain enumeration; Video.md "Capture state machine".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Zenith_CinematicCaptureState : uint8
{
	CINE_CAPTURE_STATE_IDLE = 0, CINE_CAPTURE_STATE_RECORDING, CINE_CAPTURE_STATE_DRAINING, CINE_CAPTURE_STATE_FINISHED,
	CINE_CAPTURE_STATE_FAILED,
};

/// @contract type Zenith_CinematicCapture
/// Ownership: Owned by the Cinematics module instance (one per process), created at Start step 7 only when
///   !Zenith_IsShippingBuild() (null in Shipping and ShippingTest); reached by the console commands and the editor's
///   capture child process through the module instance.
/// Thread affinity: MAIN_THREAD. Begin, End and GetState run between frames; the per-frame work runs in the module's
///   frame listener and on the render-submit thread (Video.md "Capture algorithm").
/// Invalidation: None.
/// Units: Frames.
/// Errors: Begin returns CINEMATIC_ERROR_CAPTURE_STATE when not IDLE, FINISHED or FAILED, when pxWriter is null, when
///   cine.capture.enabled is false, when sim.ticksPerFrame is not 1, when no capture tick gate exists, or when
///   m_bCaptureAudio is set and audio.device.backend is not offline; CINEMATIC_ERROR_INVALID_ARGUMENT for a bad
///   description. A write failure while recording (a refused write, or a completion failure counted by the development
///   file writer), or a video wait above uCINE_CAPTURE_VIDEO_WAIT_FRAMES frames, moves the capture to FAILED and logs it.
/// Version: since Z2 Stage 4.
/// @endcontract
class Zenith_CinematicCapture
{
public:
	// pxWriter: the development file writer, borrowed until FINISHED or FAILED; the cine.capture.begin handler passes
	// Zenith_DevCommandOutput::GetFileWriter() (owned by Zenith_DevTools for the process's life).
	Zenith_Result<void, Zenith_CinematicError> Begin(const Zenith_CinematicCaptureDesc& xDesc,
		Zenith_DevFileWriter* pxWriter);
	void End();                                            // RECORDING -> DRAINING; FINISHED once every frame is written
	Zenith_CinematicCaptureState GetState() const;
	uint32 GetFramesWritten() const;
	uint32 GetFramesPending() const;        // readbacks and writes in flight (0 or 1, lockstep)
};
```

## Flux_Video header

`Flux_Video.h` is a root header of `ZenithFlux` that CINE owns (one owner per file; `Zenith2/Docs/Design/CINE.md` → "ZenithFlux files owned by CINE"). It declares the interface Flux calls upward to obtain decoded frames, so `ZenithFlux` never includes a `ZenithCinematics` header (`Zenith2/Docs/Contracts.md` → "Callback direction").

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Video.h
#pragma once
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithFlux/Flux_RenderGraph.h"

/// @contract enum Flux_VideoPixelLayout
/// Ownership: Plain enumeration of the decoded-frame layouts the conversion pass reads.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
enum Flux_VideoPixelLayout : uint8
{
	FLUX_VIDEO_PIXEL_LAYOUT_NV12 = 1,       // 8-bit Y plane, then interleaved CbCr at half width and height
	FLUX_VIDEO_PIXEL_LAYOUT_P010 = 2,       // 16-bit containers, 10 significant high bits, same plane layout
	FLUX_VIDEO_PIXEL_LAYOUT_I420 = 3,       // 8-bit Y, Cb, Cr planes (Android flexible YUV copied into this layout)
};

/// @contract value Flux_VideoFrameView
/// Ownership: Plain value; the plane spans point into a frame buffer the frame source owns until ReleaseFrame.
/// Thread affinity: IMMUTABLE; members non-const: its producer default-constructs it and fills it member by member.
/// Invalidation: The spans are invalid after Flux_VideoFrameSource::ReleaseFrame(m_ulFrameToken).
/// Units: Pixels; bytes for strides; microseconds for the presentation time.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_VideoFrameView
{
	Flux_VideoPixelLayout m_eLayout = FLUX_VIDEO_PIXEL_LAYOUT_NV12;
	uint8 m_uMatrix = 1;            // H.273 matrix coefficients
	uint8 m_uTransfer = 1;          // H.273 transfer characteristics
	uint8 m_bFullRange = 0;
	uint32 m_uWidth = 0;            // the luma plane's stored width
	uint32 m_uHeight = 0;           // the luma plane's stored height
	uint32 m_auCrop[4] = {};        // x, y, width, height of the displayed region in luma pixels, all even; conversion reads only it
	uint32 m_auStrides[3] = {};
	Zenith_Span<const uint8> m_axPlanes[3];
	uint64 m_ulPtsUs = 0;
	uint64 m_ulFrameToken = 0;
};

/// @contract interface Flux_VideoFrameSource
/// Ownership: Implemented by the Cinematics module (Zenith_VideoFrameSourceImpl in ZenithCinematics/Internal);
///   borrowed by Flux_VideoSystem from EnableVideo to DisableVideo.
/// Thread affinity: RENDER_SUBMIT_THREAD. Flux calls every member from the render-submit thread while recording the
///   frame's upload; the implementation receives frames from the video thread through a Zenith_SpscRing and returns
///   released buffers to it through a second one (Video.md "Video thread").
/// Invalidation: None.
/// Units: Microseconds.
/// Errors: AcquireFrame returns false when no frame newer than the last one it returned is ready; nothing is asserted.
/// Reentrancy: NONE. A source never calls back into Flux_VideoSystem from a member.
/// Version: since Z2 Stage 4. Single-release rule: every frame AcquireFrame returns is released exactly once, by Flux's
///   ReleaseFrame, in the same frame, after Flux copied its planes into the upload ring (or skipped the upload); the
///   source itself releases only frames it popped and never returned (Video.md "Frame selection").
/// @endcontract
class Flux_VideoFrameSource
{
public:
	virtual ~Flux_VideoFrameSource() = default;
	// The newest decoded frame of player uPlayer whose presentation time is ≤ ulMediaTimeUs, when it is newer than the
	// last frame returned for the player; false otherwise (the previous upload stays on screen).
	virtual bool AcquireFrame(uint32 uPlayer, uint64 ulMediaTimeUs, Flux_VideoFrameView& xOutFrame) = 0;
	// Pushes the returned frame's buffer index into the player's return ring (never full: its capacity covers the pool).
	virtual void ReleaseFrame(uint32 uPlayer, uint64 ulFrameToken) = 0;
};

/// @contract value Flux_VideoDrawDesc
/// Ownership: Plain value; one per active video section, copied from the render world.
/// Thread affinity: IMMUTABLE; members non-const: the player table writes m_uPlayer on the render-submit thread after
///   the extractor fills the rest.
/// Invalidation: None.
/// Units: Normalised output coordinates [0, 1]; opacity [0, 1].
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Flux_VideoDrawDesc
{
	uint32 m_uPlayer = 0;                            // written by the player table on the render-submit thread; the extractor leaves 0
	uint64 m_ulMediaTimeUs = 0;
	float m_afRect[4] = { 0.0f, 0.0f, 1.0f, 1.0f };   // x, y, width, height on the output
	float m_fOpacity = 1.0f;
	uint32 m_uFitMode = 0;                           // 0 letterbox, 1 fill-crop, 2 stretch
	uint64 m_ulSurfaceNameHash = 0;                  // nonzero: write into the named texture surface, not the composite
	uint32 m_uLayer = 0;                             // draw order, ascending
};

/// @contract type Flux_VideoSystem
/// Ownership: Owned by Flux_Renderer from EnableVideo to DisableVideo (Flux_FrameShared::m_pxVideo borrows it).
/// Thread affinity: RENDER_SUBMIT_THREAD for the frame members; MAIN_THREAD for the surface queries between frames.
/// Invalidation: A surface texture handle is invalid after the player it names stops or after DisableVideo.
/// Units: Pixels.
/// Errors: A player that cannot get its textures (the per-player VRAM limit of Video.md "GPU side") draws nothing and
///   counts flux.video.alloc_failed_delta.
/// Version: since Z2 Stage 4.
/// @endcontract
class Flux_VideoSystem
{
public:
	uint32 GetActivePlayerCount() const;
	Flux_TextureHandle FindSurface(uint64 ulSurfaceNameHash) const;   // null when none
private:
	// elaboration: Video.md "GPU side"
};

/// @contract functions Flux_VideoPasses
/// Ownership: Graph-recording functions; the graph owns the passes they add.
/// Thread affinity: RENDER_SUBMIT_THREAD, while the frame's graph is recorded.
/// Invalidation: None.
/// Units: None.
/// Errors: None; with no active player and no overlay record the function records nothing.
/// Version: since Z2 Stage 4.
/// @endcontract
// The epilogue pass of FrameAnatomy row 6b, after TonemapAndGrade and before TWOD's FLUX_EXTENSION_POINT_BEFORE_UI passes
// and UIComposite (Video.md "VideoComposite pass").
void Flux_AddVideoComposite(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_GraphTexture xOutput);
```

<!-- assumption: pending RPROD (rows CN8, X292, X313 (e)) -->
`Flux_Renderer` gains `Zenith_Result<void, Flux_RendererError> EnableVideo(Flux_VideoFrameSource& xSource)`, `void DisableVideo()` and `Flux_VideoSystem* GetVideoSystem()` in the pattern of the ENABLE-FEATURES ruling (RPROD declares the members; CINE writes their bodies in RPROD's renderer files), and `Flux_FrameShared::m_pxVideo` (null when disabled); the common epilogue gains row 6b `VideoComposite` (`Flux_AddVideoComposite`, reads output and the video textures, writes output) after `TonemapAndGrade` and before TWOD's before-UI extension point and `UIComposite`.
<!-- /assumption -->

## Tool-side headers

CINE's tool-side code follows EDIT's contract for `ZenithAuthoring` (`Zenith2/Docs/Design/EDIT.md` → "ZenithAuthoring contract rules"; the AUTHORING-CONTRACT ruling): one public root header in the library's root, sources under `Zenith2/Tools/Authoring/Internal/Cinematics/`, no ZenithCook or ZenithEditor type, no other library's `Internal/` source. Cook steps, the importer and the MP4 reader stay in ZenithCook (`Zenith2/Tools/ZenithCook/Internal/Cinematics/`), which calls these functions with plain inputs.

```cpp
// Zenith2/Tools/Authoring/Zenith_AuthoringCinematics.h   (included as "Authoring/Zenith_AuthoringCinematics.h")
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithDocument/Zenith_DocumentErrors.h"
#include "ZenithDocument/Zenith_MigrationRegistry.h"
#include "ZenithCinematics/Zenith_CinematicErrors.h"

/// @contract functions Zenith_CinematicDocumentRegistration
/// Ownership: A registration function; the registry owns what it registers.
/// Thread affinity: MAIN_THREAD, at tool start (ZenithCook and ZenithEditor both call it).
/// Invalidation: None.
/// Units: None.
/// Errors: The registry's own registration errors, forwarded.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<void, Zenith_DocumentError> Zenith_RegisterCinematicDocumentTypes(Zenith_MigrationRegistry& xRegistry);

/// @contract value Zenith_CinematicExportDesc
/// Ownership: Plain value; borrows its strings for the call.
/// Thread affinity: IMMUTABLE; members non-const: callers default-construct it and set the members they need.
/// Invalidation: None.
/// Units: None.
/// Errors: Validated by Zenith_ExportCinematicCapture.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicExportDesc
{
	Zenith_StringView m_strCaptureDirectory;   // a host path of a capture directory written by Zenith_CinematicCapture
	Zenith_StringView m_strOutputDirectory;    // created if absent; must be empty
	bool m_bWritePng = true;                   // frames as PNG (zlib, COMPRESS) instead of copying the TGA frames
	bool m_bWriteWebVtt = true;                // subtitles.vtt from subtitles.json
	bool m_bEncodeVideo = false;               // MP4 through the encoder D-SEL-VIDEO admits; refused while unruled
	Zenith_StringView m_strLocalizationDirectory;   // the game's Localization/<locale>/ host directory; empty: cues keep their keys
};

/// @contract functions Zenith_CinematicExportFunctions
/// Ownership: Reads a capture directory, writes an export directory; owns nothing after return.
/// Thread affinity: ANY_THREAD. Pure file processing; one call at a time per output directory.
/// Invalidation: None.
/// Units: None.
/// Errors: CINEMATIC_ERROR_CAPTURE_IO for unreadable inputs or unwritable outputs; CINEMATIC_ERROR_CORRUPT for a manifest
///   that fails its schema or a missing frame or audio chunk (m_uDetail = the first missing frame index modulo 65,536);
///   CINEMATIC_ERROR_VIDEO_UNSUPPORTED for m_bEncodeVideo while no encoder is admitted.
/// Version: since Z2 Stage 4.
/// @endcontract
Zenith_Result<void, Zenith_CinematicError> Zenith_ExportCinematicCapture(const Zenith_CinematicExportDesc& xDesc);

/// @contract value Zenith_CinematicCaptionKey
/// Ownership: Plain value; the views point into the document bytes passed to Zenith_ListCinematicCaptionKeys.
/// Thread affinity: IMMUTABLE; members non-const: an element its producer writes in place in an arena or Zenith_Array.
/// Invalidation: Invalid once those bytes are freed.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 4.
/// @endcontract
struct Zenith_CinematicCaptionKey
{
	Zenith_StringView m_strKey;            // a subtitleKey, speakerKey or subtitle key
	Zenith_StringView m_strJsonPointer;    // e.g. /tracks/3/sections/0/subtitleKey
};

/// @contract functions Zenith_CinematicLocalizationFunctions
/// Ownership: A pure function over caller-owned document bytes.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: CINEMATIC_ERROR_CORRUPT when the bytes are not a current-version cinematic document.
/// Version: since Z2 Stage 4.
/// @endcontract
// Writes up to axOut.GetSize() caption keys in document order and returns the total count found.
Zenith_Result<uint32, Zenith_CinematicError> Zenith_ListCinematicCaptionKeys(Zenith_ByteSpan xDocument,
	Zenith_Span<Zenith_CinematicCaptionKey> axOut);
```

```cpp
// Zenith2/Tools/ZenithCook/Internal/Cinematics/Zenith_CinematicCookSteps.h   (ZenithCook-internal)
#pragma once
#include "ZenithCore/Zenith_Result.h"

class Zenith_CookStepRegistry;   // ASSET's cook step registry (Zenith2/Docs/Design/ASSET/Cook.md "Zenith_CookStepRegistry")
class Zenith_CookServices;       // ASSET's cook services
enum Zenith_CookError : uint8;  // ASSET's cook error enumeration

/// @contract functions Zenith_CinematicCookRegistration
/// Ownership: A registration function; the registry owns the steps, importer and validators it registers.
/// Thread affinity: MAIN_THREAD, at cook start, in ASSET's canonical module order.
/// Invalidation: None.
/// Units: None.
/// Errors: The registry's own registration errors, forwarded.
/// Version: since Z2 Stage 4. The signature of Zenith2/Docs/Design/ASSET/Cook.md "Step registration functions".
/// @endcontract
Zenith_Result<void, Zenith_CookError> Zenith_RegisterCinematicCookSteps(Zenith_CookStepRegistry& xRegistry,
	Zenith_CookServices& xServices);
```

## Luau surface

The `Cinematics` binding table is registered by `Zenith_Module_Cinematics_Start` through `RegisterScriptBindings` (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces"), in the pattern of `Zenith2/Docs/Design/GAME/Scripting.md` → "Binding registration": request functions are `SCRIPT_FUNCTION_EFFECT_BUFFERED` with `uSIDE_EFFECT_MASK_FIELD_COMMAND` (one record of a CINE command kind, appended with `AppendSideEffect`, capacity demand one record of the payload's size; the ECS-held kind needs no consumer, [Z2-N-SCRIPT-219], [Z2-N-SCRIPT-226]); queries are `SCRIPT_FUNCTION_EFFECT_READ_WORLD` with the resource `Zenith.Cinematics.World` in `Uses`. Every function takes `ctx` first except the pure one. Contexts: system and behaviour, except the pure helper (load too).

| Function | Class | Signature (Luau) | Effect |
|---|---|---|---|
| `Start` | R | `Cinematics.Start(ctx, sequence: AssetRef, bindings: {[string]: Entity}, options: StartOptions?) -> CinematicTicket` | records `Cinematics.Start`; options keys `blendIn`, `blendOut`, `rate`, `startAt`, `loop`, `paused`, `tag` (string), `noTakeover`, `voiceLocale` (string: the simulation voice-locale tag of GAME's component, row CH18; absent: window 0). The bindings are converted to `(Zenith_Hash64(name), entity)` and sorted by hash before recording (Luau table iteration order is not stable); the returned ticket is computed at the call from the invocation's command buffer (`GetBaseKey()`, `GetRecordCount()` before the append), exactly as `Zenith_RecordStartCinematic` computes it |
| `Preload` | R | `Cinematics.Preload(ctx, sequence: AssetRef) -> ()` | records `Cinematics.Preload` |
| `Release` | R | `Cinematics.Release(ctx, sequence: AssetRef) -> ()` | records `Cinematics.Release` |
| `Interrupt` | R | `Cinematics.Interrupt(ctx, instance: CinematicInstance) -> ()` | records `Cinematics.Interrupt` |
| `Skip` | R | `Cinematics.Skip(ctx, instance: CinematicInstance) -> ()` | records `Cinematics.Skip` |
| `Seek` | R | `Cinematics.Seek(ctx, instance: CinematicInstance, seconds: number, fireEvents: boolean?) -> ()` | records `Cinematics.Seek` with `llround(seconds × 1e6)` computed in `double` (exact for every whole microsecond below 2⁵³ µs) |
| `SetRate` | R | `Cinematics.SetRate(ctx, instance: CinematicInstance, rate: number) -> ()` | records `Cinematics.SetRate` |
| `Pause` / `Resume` | R | `Cinematics.Pause(ctx, instance: CinematicInstance) -> ()` | records `Cinematics.Pause` / `Cinematics.Resume` |
| `Resolve` | Q | `Cinematics.Resolve(ctx, ticket: CinematicTicket) -> CinematicInstance?` | nil while pending or failed; a failure's code through `GetStartError` |
| `GetStartError` | Q | `Cinematics.GetStartError(ctx, ticket: CinematicTicket) -> string?` | the `CINEMATIC_ERROR_*` name of a failed or expired start, nil otherwise |
| `GetState` | Q | `Cinematics.GetState(ctx, instance: CinematicInstance) -> string` | `"none"`, `"binding"`, `"preparing"`, `"blendingIn"`, `"playing"`, `"paused"`, `"blendingOut"`, `"ended"`, `"released"` |
| `GetTime` / `GetDuration` | Q | `Cinematics.GetTime(ctx, instance: CinematicInstance) -> number` | seconds (µs / 1e6) |
| `GetWeight` | Q | `Cinematics.GetWeight(ctx, instance: CinematicInstance) -> number` | [0, 1] |
| `GetMarkerTime` | Q | `Cinematics.GetMarkerTime(ctx, instance: CinematicInstance, name: string) -> number?` | the marker's time in seconds (`FindMarker` of the interned name), nil when absent; a seek to it with `Seek` is exact |
| `IsPlaying` | Q | `Cinematics.IsPlaying(ctx, instance: CinematicInstance?) -> boolean` | the instance, or any instance when nil |
| `FindByTag` | Q | `Cinematics.FindByTag(ctx, tag: string) -> CinematicInstance?` | `Zenith_CinematicsWorld::FindByTag` of the interned tag |
| `FindByEntity` | Q | `Cinematics.FindByEntity(ctx, entity: Entity) -> CinematicInstance?` | the instance binding it |
| `SameTicket` | P | `Cinematics.SameTicket(a: CinematicTicket, b: CinematicTicket) -> boolean` | field-by-field equality of two tickets (a script matches `event.Ticket` of `Started` or `StartFailed` against the ticket `Start` returned) |

`CinematicTicket` and `CinematicInstance` are the generated frozen table types of the reflected structs `Zenith.Cinematics.Ticket` (four `uint32` fields) and `Zenith.Cinematics.InstanceID` (one serial below 2⁵³) (`Zenith2/Docs/Scripting/Bindings.md` → "Value marshalling": a reflected value struct is a frozen table of its fields), so no field a script reads can raise ZS1504. Events carry names as strings: `event.Tag` and `event.Marker` are `Zenith_StringID` properties, which a script compares with a string literal (`if event.Marker == "Shot02" then`). Scripts react to `Zenith.Cinematics.Started`, `Ended`, `Marker` and `StartFailed` as ordinary ECS events (`Zenith2/Docs/Scripting.md` → "Event order"); no CINE binding is asynchronous, because a start's outcome is an event delivered in phase 9 of the tick its start command played back (or of the tick its preparation finished), and the ticket resolves from the playback on. `Test_CinematicScript_MarkerAndTagMatchInLuau` asserts a Luau behaviour matching a marker by name, an instance by tag and a `Started` event by `SameTicket`, with no ZS1504.

### Misuse codes

CINE's block is **ZS2700–ZS2799**, assigned by the orchestrator's MISUSE-BLOCKS-2 ruling and reserved in `Zenith2/Docs/Scripting/MisuseCatalogue.md` → "Code ranges" by cross-doc item X313 (b) (which amends X291 (b)); CINE's bindings diff adds these rows to the catalogue ([Z2-N-SCRIPT-926]).

| Code | Raised by | Condition | Message tail |
|---|---|---|---|
| ZS2701 | `Start` | `bindings` is not a table of string keys to `Entity` values | `bindings must map binding names to entities` |
| ZS2702 | `Start` | more than 64 bindings | `at most 64 bindings, got <n>` |
| ZS2703 | `Start` | a binding name is empty or longer than 128 bytes | `binding name '<name>' is invalid` |
| ZS2704 | `Start` | `blendIn` or `blendOut` outside [0, 60] seconds | `blend <value> outside [0, 60]` |
| ZS2705 | `Start`, `SetRate` | rate outside [0, 8] | `rate <value> outside [0, 8]` |
| ZS2706 | `Seek`, `Start` (`startAt`) | time negative | `time <value> is negative` |
| ZS2707 | `Interrupt`, `Skip`, `Seek`, `SetRate`, `Pause`, `Resume`, `Get*` | the instance is nil or its `Value` is 0 | `instance is nil or null` |
| ZS2708 | `Start`, `Preload`, `Release` | the asset reference is null or not of kind `Cinematics.Sequence` | `asset is not a cinematic sequence` |
| ZS2709 | `Start` | an unknown `options` key | `unknown option '<key>'` |
| ZS2710 | `FindByTag`, `Start` (`tag`, `voiceLocale`), `GetMarkerTime` | tag, voice locale or marker name empty or longer than 128 bytes | `tag is invalid` |
| ZS2711 | any | a number argument is NaN or infinite | `argument <i> is not finite` |
| ZS2712 | `Resolve`, `GetStartError`, `SameTicket` | the ticket is nil or not a `CinematicTicket` | `ticket is nil or null` |

A time beyond the sequence's duration is not misuse: the seek playback clamps it. A stale but non-null instance is not misuse either: the request is recorded and the playback counts and drops it (`cine.commands.rejected_delta`), because staleness is a world fact the script cannot check without racing the merge.

## Configuration keys

Registered by `Zenith_Module_Cinematics_RegisterTypes` (`Zenith2/Docs/Design/MOD.md` → "Configuration-key registration"). Simulation-affecting keys are recorded in the replay header by MOD's rule for `sim.*`-class keys; the column says which. Every `f32` seconds key is converted once at Start to integer microseconds (`llround(double(v) × 1e6)`), and simulation uses only the integer.

| Key | Type | Default | Range | Affects simulation | Meaning |
|---|---|---|---|---|---|
| `cine.maxInstancesPerWorld` | u32 | 16 | [1, 64] | yes | instance pool size per world; also sizes the outbox (256 + 10 × M + 64 entries, `Zenith2/Docs/Design/CINE/Playback.md` → "Outbox") |
| `cine.snapshotPoolBytes` | u32 | 1,048,576 | [65,536, 16,777,216] | yes | per-world instance-block pool |
| `cine.prepareLookaheadSeconds` | f32 | 2.0 | [0, 30] | yes | how far ahead of the start time preparation prefetches |
| `cine.prepareTimeoutTicks` | u32 | 300 | [1, 36,000] | yes | ticks a PREPARING instance waits before beginning anyway (`cine.prepare.timeout_delta`) or failing (`cine.prepareTimeoutFails`) |
| `cine.prepareTimeoutFails` | bool | false | — | yes | true: a timeout fails the start with `CINEMATIC_ERROR_PREPARE_TIMEOUT`; false: it begins with what is resident |
| `cine.audioLeadSeconds` | f32 | 1.0 | [0, 10] | yes | how far before a dialogue section `Audio.PrepareDialogue` is recorded |
| `cine.video.enabled` | bool | true | — | no | create the decoder and enable Flux video |
| `cine.video.decodeAheadFrames` | u32 | 6 | [2, 32] | no | decoded frames buffered per player; the frame ring's capacity is the next power of two (6 → 8), the bound itself is an in-flight counter (Video.md "Video thread") |
| `cine.video.backend` | string | `"platform"` | `"platform"`, `"ffmpeg"` | no | the decoder backend; only values D-SEL-VIDEO admits for the running platform are accepted, others are a configure error |
| `cine.capture.enabled` | bool | false | — | no | development: allow `cine.capture.begin` (absent from Shipping's schema) |
| `cine.debug.overlay` | bool | false | — | no | development: draw the timeline overlay of "On-screen panel" |
