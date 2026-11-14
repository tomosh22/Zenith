# VFX design: public API

Section file of `Zenith2/Docs/Design/VFX.md`. It declares every public type and free function of the VFX area in full: the ZenithVFX module headers (simulation side: components, events, commands, script bindings, the module entry points and errors), the ZenithFlux headers VFX owns (the effect asset, the render-world emitter command, the particle system, the particle random functions), the renderer-internal declarations the frame build uses, the `[FluxShared]` GPU structs, and the Slang interfaces through which effects are code-authored. Every block follows `Zenith2/Docs/Contracts.md → "Contract blocks per type"`; every fallible function returns `Zenith_Result<…>`.

## Scope of this file

| Topic | Heading |
|---|---|
| which header declares what, and who includes it | "Header map" |
| the module's two entry points | "Zenith_Module_VFX header" |
| the simulation-side error enumeration | "Zenith_VFXErrors header" |
| the components VFX registers | "Zenith_VFXComponents header" |
| the events VFX emits | "Zenith_VFXEvents header" |
| the command kind and its recording helper | "Zenith_VFXCommands header" |
| the CPU reference simulation for tools and tests | "Zenith_VFXReference header" |
| the Luau surface and the trigger record | "Zenith_VFXScriptBindings" |
| the effect asset and its loader | "Flux_ParticleEffect header" |
| the render-world record, the particle system and the random functions | "Flux_Particles header" |
| what `Flux_Renderer` calls inside ZenithFlux | "Internal Flux particle declarations" |
| the GPU structs shared by C++ and Slang | "Shared GPU structs" |
| the interfaces of code-authored update and shading types | "Slang interfaces for code-authored modules" |

## Header map

| Header | Library | Declares | Included by |
|---|---|---|---|
| `Zenith2/Engine/ZenithVFX/Zenith_Module_VFX.h` | ZenithVFX | `Zenith_Module_VFX_RegisterTypes`, `Zenith_Module_VFX_Start` | the generated composition TU (`Zenith2/Docs/Design/MOD/GameBuild.md → "Module to target mapping"`) |
| `Zenith2/Engine/ZenithVFX/Zenith_VFXErrors.h` | ZenithVFX | `Zenith_VFXError`, `Zenith_ToString(Zenith_VFXError)` | every ZenithVFX header that returns a result |
| `Zenith2/Engine/ZenithVFX/Zenith_VFXComponents.h` | ZenithVFX | `Zenith_VFXEffectComponent`, `Zenith_VFXEffectStateComponent`, `Zenith_VFXEffectState`, `Zenith_VFXColliderComponent`, `Zenith_VFXColliderShape`, `Zenith_VFXViewerComponent`, `Zenith_VFXPreviewControlComponent`, the property-ID constants, `Zenith_RegisterVFXComponentTypes` | games' scripts through the generated `.d.luau`; EDIT's inspector; ENV's weather prefab content (by stable name only) |
| `Zenith2/Engine/ZenithVFX/Zenith_VFXEvents.h` | ZenithVFX | `Zenith_VFXEffectStartedEvent`, `Zenith_VFXEffectStoppedEvent` | scripts; any module that subscribes by stable name |
| `Zenith2/Engine/ZenithVFX/Zenith_VFXCommands.h` | ZenithVFX | `Zenith_VFXSpawnPayload`, `Zenith_VFXSpawnFlags`, `Zenith_RecordVFXSpawn` | C++ systems of the game's modules |
| `Zenith2/Engine/ZenithVFX/Zenith_VFXReference.h` | ZenithVFX | `Zenith_VFXReferenceDesc`, `Zenith_VFXReferenceEmitterResult`, `Zenith_VFXRunReference` | ZenithCook's `vfx.effect` step (the bounds bake), the ZenithVFX tests |
| `Zenith2/Engine/ZenithFlux/Flux_ParticleEffect.h` | ZenithFlux | the effect enumerations and limits, `Flux_ParticleError`, `Flux_ParticleEmitterDesc` and its member types, `Zenith_ParticleEffectAsset`, `Flux_CreateParticleEffectKindLoader`, `Flux_ParseParticleEffect` | ZenithVFX, ZenithCook's VFX steps, the editor |
| `Zenith2/Engine/ZenithFlux/Flux_Particles.h` | ZenithFlux | `Flux_ParticleInstanceKey` and its functions, `Flux_EmitterCommandOp`, `Flux_EmitterCommandFlags`, `Flux_RenderEmitterCommand`, `Flux_RenderParticleCollider`, `Flux_ParticleSortAlgorithm`, `Flux_ParticleSystemDesc`, `Flux_ReadParticleSystemDesc`, `Flux_ParticleSystem`, `Flux_ParticleStats`, `Flux_ParticleInstanceStats`, the particle random functions, `Flux_ParticleWindBound`, `Flux_ParticleWindExpansion` | `Flux_RenderWorld.h` (the section's element type), ZenithVFX, the editor |
| `Zenith2/Engine/ZenithFlux/Internal/Particles/Flux_ParticleInternal.h` | ZenithFlux (internal) | `Flux_ParticleFrame`, `Flux_ParticleStep`, `Flux_ParticleDispatchBatch`, `Flux_ParticleViewDraws`, `Flux_ParticleDrawRun`, `Flux_ParticleSystemDeps`, `Flux_CreateParticleSystem`, `Flux_ReleaseParticleSystem`, the renderer-facing members | `Internal/Passes/Flux_PrologueFeatures.h` (RPROD's), `Internal/Renderer/` |
| `Zenith2/Shaders/Common/Particles.slang` | ZenithShadersGenerated (generated C++ into `Flux_SharedTypes.generated.h`) | the `[FluxShared]` structs of "Shared GPU structs" | every particle program, the C++ of ZenithFlux and ZenithVFX |
| `Zenith2/Shaders/Particles/ParticleInterfaces.slang` | shader module | `Flux_ParticleUpdateType`, `Flux_ParticleShadingType` | code-authored particle types |

<!-- elaboration -->
`Flux_` types in ZenithFlux headers are renderer types; `Zenith_ParticleEffectAsset` keeps the `Zenith_` prefix that `Zenith2/Docs/Design/ASSET.md → "Asset kinds"` gives the kind's runtime object type (the precedent is GEOM's `Zenith_ProceduralGeometrySource` in a ZenithFlux header). ZenithVFX includes the ZenithFlux headers through its declared `ZenithFlux` edge; ZenithFlux includes no ZenithVFX header ([Z2-N-ARCH-420] forbids Flux edges to ZenithScene, ZenithECS and ZenithScript; the layer DAG forbids the edge to a layer-7 module).

## Zenith_Module_VFX header

```cpp
// Zenith2/Engine/ZenithVFX/Zenith_Module_VFX.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithRuntime/Zenith_Module.h"

class Zenith_TypeRegistry;

/// @contract functions Zenith_Module_VFX
/// Ownership: RegisterTypes registers VFX's reflected types (the five components, the two events, the spawn payload, the
///   trigger-queue world resource type) and the vfx.* configuration keys through the registry's configuration schema; Start
///   creates the module instance the runtime owns, the effect kind loader, the command kind, the script bindings, the VFX
///   trigger consumer and, when a renderer exists, the renderer's particle system.
/// Thread affinity: MAIN_THREAD, at startup steps 6 and 15.
/// Invalidation: None.
/// Units: None.
/// Errors: MODULE_ERROR_REGISTRATION when a registration is rejected (logged by the context with the VFX error that caused
///   it); MODULE_ERROR_OUT_OF_MEMORY when the instance, the loader or the particle system cannot be allocated;
///   MODULE_ERROR_DEVICE when the renderer refuses EnableParticles with FLUX_PARTICLE_ERROR_DEVICE.
/// Version: since Z2 Stage 5. Module entry points per [Z2-N-ARCH-168].
/// @endcontract
Zenith_Result<void, Zenith_ModuleError> Zenith_Module_VFX_RegisterTypes(Zenith_TypeRegistry& xTypes);
Zenith_Result<Zenith_Owned<Zenith_ModuleInstance>, Zenith_ModuleError> Zenith_Module_VFX_Start(
	Zenith_ModuleStartContext& xContext);
```

`Zenith_Module_VFX_Start` performs these steps in order; each fallible step has its own failure-injection test `Test_VFXModule_Start_FailsAt<Step>_Unwinds` ([Z2-N-CONTRACT-341]) and a failure unwinds every earlier step in reverse:

| # | Step | Fault point | Failure |
|---|---|---|---|
| 1 | read the `vfx.*` keys of `Zenith2/Docs/Design/VFX.md → "Configuration keys"` from `xContext.GetConfig()` into the module's settings value | `0x56460001` | a value outside its domain: the key's default, one `vfx` warning |
| 2 | `Zenith_TryNew<Zenith_VFXModuleInstance>` | `0x56460002` | `MODULE_ERROR_OUT_OF_MEMORY` |
| 3 | `Flux_CreateParticleEffectKindLoader(xContext.GetHeap())`, then `xContext.GetAssets().RegisterKindLoader(ASSET_KIND_PARTICLE_EFFECT, xDesc, *pxLoader)` with `xDesc = { "VFX.EffectLoader", uFLUX_PARTICLE_EFFECT_FORMAT_VERSION, 0 }` | `0x56460003` | `MODULE_ERROR_OUT_OF_MEMORY` or `MODULE_ERROR_REGISTRATION` |
| 4 | `xContext.RegisterCommandKind` for `VFX.Spawn` ("Zenith_VFXCommands header") | `0x56460004` | `MODULE_ERROR_REGISTRATION` |
| 5 | `xContext.RegisterScriptBindings(xTable)` with the `VFX` namespace of "Luau surface", then `xContext.SetScriptSideEffectConsumer(SIDE_EFFECT_KIND_VFX_TRIGGER, xInstance.GetTriggerConsumer())` | `0x56460005` | `MODULE_ERROR_REGISTRATION` |
| 6 | when `xContext.GetRenderer()` is non-null: `GetRenderer()->EnableParticles(xParticleDesc)` with the render-side settings of step 1 | `0x56460006` | `MODULE_ERROR_OUT_OF_MEMORY` for `FLUX_PARTICLE_ERROR_OUT_OF_MEMORY`, `MODULE_ERROR_DEVICE` for `FLUX_PARTICLE_ERROR_DEVICE` |
| 7 | `xContext.RegisterMemoryConsumer(xInstance.GetMemoryConsumer())` (`MEMORY_SUBSYSTEM_RENDERING`: the CPU particle storage and the trigger queues; the GPU pool is reported by the particle system through RHI's rendering consumer) | — | none |
| 8 | `ConfigListener`: add the instance's change callback to `xContext.GetConfig().GetChangeRegistry()` (`Zenith2/Docs/Design/PLAT/Configuration.md → "Zenith_Config API"`); the instance holds the returned token ("Runtime configuration changes" of `Zenith2/Docs/Design/VFX.md`) | `0x56460007` | `MODULE_ERROR_REGISTRATION` |
| 9 | `ProfilerPanel`: `xContext.GetProfilerOverlay().AddProvider(xInstance.GetPanelProvider())`; the instance holds the returned `Zenith_CallbackToken` ("On-screen profiler rows" of `Zenith2/Docs/Design/VFX.md`) | `0x56460008` | `MODULE_ERROR_REGISTRATION` for `PROFILER_ERROR_INVALID_DESC` |

<!-- assumption: pending MOD (DEVT cross-area row DV12) -->
`Zenith_ModuleStartContext::GetProfilerOverlay()` returns `Zenith_ProfilerOverlay&` (MAIN_THREAD during `Start`; the overlay outlives every module instance), the member DEVT requests for module panel providers (`Zenith2/Docs/Design/DEVT/Overlay.md → "Panel providers"`).
<!-- /assumption -->

Fault-injection values carry the library-distinct high 16 bits `0x5646` ('VF'). A failure at step 8 or 9 unwinds step 6 through `Flux_Renderer::DisableParticles()`, then steps 5 … 2 in reverse. Step 6 and the unwind link against RPROD's declarations of the two members (Z2T-RPROD-014) and the bodies of VFX's ZenithFlux API slice ("Flux_ParticleSystem"), so slice 7 needs neither the full particle system nor slice 11.

The module instance's `AttachWorld` registers, for the attached world, in this order: the world resource `Zenith.VFX.TriggerQueue` (`Zenith_WorldAttachContext::AddWorldResource`, owning phase `PHASE_LATE_SCRIPTS`), the lifecycle listener (entity destruction, teleport), the system `VFX.Lifecycle` (phase 10), the epilogue stage `VFX.Update` and the extractor `VFX.Extract` (`Zenith2/Docs/Design/VFX.md → "Frame-graph placement"`); it records `GetWorldPurpose()` and `GetSimulationClock()` in the world's presentation state (cross-doc item X243). `DetachWorld` releases the world's presentation state, which drops every effect reference it holds.

### Module shutdown order

The module instance is destroyed in step 6 of `Zenith2/Docs/Contracts.md → "Shutdown ownership order"`: after every world was destroyed (so every component's and presentation state's effect handle is gone), after the GPU went idle and the deferred deletions retired (step 5), and before the renderer, the RHI device (step 9) and the asset registry (step 10) are destroyed. Its destructor runs, on the main thread, in this order:

| # | Action | Why here |
|---|---|---|
| 1 | destroy the profiler-panel token, then the configuration-listener token | callbacks are unregistered before their owner dies ([Z2-N-CONTRACT-357]'s callback-lifetime rule) |
| 2 | when the particle system exists, `Flux_Renderer::DisableParticles()` | the render side's instance table holds effect handles acquired by persistent ID; they must be released while the kind loader still serves the kind, and this point is render-idle (no `Flux.BuildGraph` runs after step 2 of the shutdown order) |
| 3 | `Zenith_AssetRegistry::UnregisterKindLoader(ASSET_KIND_PARTICLE_EFFECT)`, which unloads every ACTIVE and CACHED effect through the loader and services until each unload is acknowledged (`Zenith2/Docs/Design/ASSET/RuntimeAPI.md → "Zenith_AssetRegistry"`) | no handle of the kind exists any more, so no unloaded effect is still resolved anywhere |
| 4 | destroy the kind loader, then the instance's remaining state (trigger-queue pools, settings) | the loader is unregistered before destruction |

The registrations made through the start and attach contexts (command kind, bindings, side-effect consumer, memory consumer) are undone by the runtime ([Z2-N-ARCH-851]). The order is observable through one test seam, since the tests use the real concrete objects (the FINAL-DOUBLES ruling): the destructor appends one `Zenith_VFXShutdownAction` enumerator per action of the table (`VFX_SHUTDOWN_ACTION_PANEL_TOKEN`, `VFX_SHUTDOWN_ACTION_CONFIG_TOKEN`, `VFX_SHUTDOWN_ACTION_DISABLE_PARTICLES`, `VFX_SHUTDOWN_ACTION_UNREGISTER_KIND_LOADER`, `VFX_SHUTDOWN_ACTION_DESTROY_LOADER`, in the order it performs them) to a test-installed `Zenith_VFXShutdownTrace*`, a fixed array of 8 enumerators with a count, held by the module instance, set through `Zenith2/Engine/ZenithVFX/Internal/Zenith_VFXModuleTestAccess.h` and null outside tests, so it appends nothing then; both shutdown tests read that trace. Slice 7 lands this order against slice 2a's empty particle system, and `Test_VFXModule_ShutdownOrder` asserts it with a renderer on WARP: the two tokens are destroyed first, `DisableParticles` is called once and leaves `GetParticleSystem()` null, then `UnregisterKindLoader` runs, then the loader is destroyed. Slice 11, whose instance table holds effect references, lands `Test_VFXModule_ShutdownReleasesRenderSideFirst`, which asserts the release with a renderer on WARP: the trace shows `DisableParticles` before `UnregisterKindLoader`, the registry reports no referenced particle-effect slot when the loader is unregistered, and every effect asset reaches `AcknowledgeUnload`.

## Zenith_VFXErrors header

```cpp
// Zenith2/Engine/ZenithVFX/Zenith_VFXErrors.h
#pragma once
#include "ZenithCore/Zenith_Types.h"

/// @contract enum Zenith_VFXError
/// Ownership: Plain enumeration of the ZenithVFX failure categories.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; it is the error. VFX_ERROR_OUT_OF_MEMORY: a Zenith_TryNew or arena allocation of the module failed.
///   VFX_ERROR_REGISTRATION: a context registration was refused. VFX_ERROR_INVALID_ARGUMENT: a spawn payload, binding argument
///   or component value outside its domain (non-finite transform, time scale outside [0, 8], parameter index above 7).
///   VFX_ERROR_UNKNOWN_EFFECT: a null persistent ID, or an ID that names no particle-effect asset in the catalogue.
///   VFX_ERROR_UNKNOWN_PARAMETER: a parameter name the effect does not declare. VFX_ERROR_LIMIT: a per-world cap (detached effects, effect lights, effect decals, colliders,
///   viewers) is reached. VFX_ERROR_STALE_ENTITY: an attach target that no longer resolves. VFX_ERROR_SHUTTING_DOWN: a call
///   after the module began shutdown.
/// Version: since Z2 Stage 5. Enumerators are appended, never renumbered ([Z2-N-CONTRACT-467]).
/// @endcontract
enum Zenith_VFXError : uint8
{
	VFX_ERROR_OUT_OF_MEMORY = 1,
	VFX_ERROR_REGISTRATION,
	VFX_ERROR_INVALID_ARGUMENT,
	VFX_ERROR_UNKNOWN_EFFECT,
	VFX_ERROR_UNKNOWN_PARAMETER,
	VFX_ERROR_LIMIT,
	VFX_ERROR_STALE_ENTITY,
	VFX_ERROR_SHUTTING_DOWN,
};

constexpr const char* Zenith_ToString(Zenith_VFXError eError);   // "VFX_ERROR_OUT_OF_MEMORY", …
```

## Zenith_VFXComponents header

```cpp
// Zenith2/Engine/ZenithVFX/Zenith_VFXComponents.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithAssets/Zenith_AssetHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"
#include "ZenithFlux/Flux_ParticleEffect.h"     // Zenith_ParticleEffectAsset: complete, because Zenith_AssetHandle checks its kind

class Zenith_TypeRegistry;
enum Zenith_ReflectError : uint8;
```

### Zenith_VFXEffectComponent

```cpp
/// @contract value Zenith_VFXEffectComponent
/// Ownership: Plain component value stored by the ECS backend through the reflected type Zenith.VFX.Effect; m_xEffect holds one
///   reference, copied and destroyed through the type's lifecycle hooks ([Z2-N-CONTRACT-087]).
/// Thread affinity: As every component: written directly in PHASE_LATE_SCRIPTS (its authority) and through commands from any
///   other phase; read by VFX.Lifecycle (phase 10), VFX.Update (epilogue stage 12) and nothing else.
/// Invalidation: None.
/// Units: Seconds for m_fDestroyAfterSeconds; a dimensionless multiplier for m_fTimeScale; parameter units are the effect's.
/// Errors: None stored; VFX.Lifecycle treats a value outside its domain as "VFX_ERROR_INVALID_ARGUMENT" for that entity: the
///   effect does not start, vfx.lifecycle.invalid_components counts it and one vfx warning is logged per entity per run.
/// Version: since Z2 Stage 5; type version 1. Properties 1–17 serialized; IDs never reused ([Z2-N-ARCH-641]).
/// @endcontract
struct Zenith_VFXEffectComponent
{
	Zenith_AssetHandle<Zenith_ParticleEffectAsset> m_xEffect;   // 1  Effect (asset VFX.Effect)
	bool m_bAutoPlay = true;                                    // 2  AutoPlay: start when the component first appears
	uint32 m_uSeed = 0;                                        // 3  Seed: 0 derives it from the entity's stable simulation ID and the generation
	uint32 m_uPlayRequest = 0;                                 // 4  PlayRequest: any change of value (re)starts the effect
	uint32 m_uStopRequest = 0;                                 // 5  StopRequest: any change of value stops spawning; particles live out
	bool m_bPaused = false;                                     // 6  Paused: presentation time stops; particles freeze and stay drawn
	float m_fTimeScale = 1.0f;                                  // 7  TimeScale, [0, 8]
	float m_fDestroyAfterSeconds = 0.0f;                        // 8  DestroyAfterSeconds: > 0 destroys the entity that long after the start
	float m_fParameter0 = 0.0f;                                 // 9  Parameter0 … the effect's user parameters, in declaration order
	float m_fParameter1 = 0.0f;                                 // 10 Parameter1
	float m_fParameter2 = 0.0f;                                 // 11 Parameter2
	float m_fParameter3 = 0.0f;                                 // 12 Parameter3
	float m_fParameter4 = 0.0f;                                 // 13 Parameter4
	float m_fParameter5 = 0.0f;                                 // 14 Parameter5
	float m_fParameter6 = 0.0f;                                 // 15 Parameter6
	float m_fParameter7 = 0.0f;                                 // 16 Parameter7
	uint32 m_uLayerMask = 0x01000001u;                         // 17 LayerMask: render layers bits 0–23, lighting channels bits 24–31
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	static const Zenith_TypeID xTYPE_ID;                        // Zenith_MakeTypeID("Zenith.VFX.Effect"), constexpr in the .inl
};
```

| ID | Name | Reflected type | Default | Range | Authority | Serialized |
|---|---|---|---|---|---|---|
| 1 | `Effect` | asset `VFX.Effect` | null | — | `PHASE_LATE_SCRIPTS` | yes |
| 2 | `AutoPlay` | BOOL | true | — | `PHASE_LATE_SCRIPTS` | yes |
| 3 | `Seed` | U32 | 0 | — | `PHASE_LATE_SCRIPTS` | yes |
| 4 | `PlayRequest` | U32 | 0 | — | `PHASE_LATE_SCRIPTS` | yes |
| 5 | `StopRequest` | U32 | 0 | — | `PHASE_LATE_SCRIPTS` | yes |
| 6 | `Paused` | BOOL | false | — | `PHASE_LATE_SCRIPTS` | yes |
| 7 | `TimeScale` | F32 | 1.0 | [0, 8] | `PHASE_LATE_SCRIPTS` | yes |
| 8 | `DestroyAfterSeconds` | F32 | 0.0 | [0, 86,400] | `PHASE_LATE_SCRIPTS` | yes |
| 9–16 | `Parameter0` … `Parameter7` | F32 | 0.0 | finite | `PHASE_LATE_SCRIPTS` | yes |
| 17 | `LayerMask` | U32 | `0x01000001` | — | `PHASE_LATE_SCRIPTS` | yes |

The editor's inspector shows parameters 9–16 under the names the effect's `parameters` array declares (`Zenith2/Docs/Design/VFX/EffectFormat.md → "Top-level keys"`); a parameter the effect does not declare is hidden and ignored. A newly assigned effect's declared defaults are written into the parameter properties by the inspector's assignment command, never at run time, so runtime values are always the component's.

```cpp
/// @contract enum Zenith_VFXEffectState
/// Ownership: Plain enumeration; the values of Zenith_VFXEffectStateComponent::m_uState (property 6).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; any other stored value is treated as VFX_EFFECT_STATE_IDLE and counted in vfx.lifecycle.invalid_components.
/// Version: since Z2 Stage 5. Serialized as a uint8; values never move.
/// @endcontract
enum Zenith_VFXEffectState : uint8
{
	VFX_EFFECT_STATE_IDLE = 0,       // never started, or stopped and its stop handled
	VFX_EFFECT_STATE_PLAYING = 1,    // started; spawning per the effect's timeline
	VFX_EFFECT_STATE_STOPPED = 2,    // a StopRequest change was observed; spawning has ended, particles live out
};

/// @contract value Zenith_VFXEffectStateComponent
/// Ownership: Plain component value (Zenith.VFX.EffectState); added and written by VFX.Lifecycle alone, through the command
///   buffer for the add and through its declared direct write afterwards.
/// Thread affinity: Written in PHASE_LATE_SCRIPTS by VFX.Lifecycle; read by VFX.Update.
/// Invalidation: None.
/// Units: Ticks for m_ulStartTick and m_ulStopTick (the world's tick index).
/// Errors: None.
/// Version: since Z2 Stage 5; type version 1. Every property serialized, so a save restores timers exactly.
/// @endcontract
struct Zenith_VFXEffectStateComponent
{
	uint64 m_ulStartTick = 0;          // 1 StartTick: the tick whose VFX.Lifecycle observed the start
	uint64 m_ulStopTick = 0;           // 2 StopTick: the tick whose VFX.Lifecycle observed the stop; 0 while playing
	uint32 m_uObservedPlay = 0;        // 3 ObservedPlay: the PlayRequest value last acted on
	uint32 m_uObservedStop = 0;        // 4 ObservedStop: the StopRequest value last acted on
	uint32 m_uGeneration = 0;          // 5 Generation: incremented by every (re)start; part of the instance key
	uint8 m_uState = VFX_EFFECT_STATE_IDLE;   // 6 State
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	static const Zenith_TypeID xTYPE_ID;      // Zenith_MakeTypeID("Zenith.VFX.EffectState")
};
```

The state is a separate component so that a behaviour declaring a write of `Zenith.VFX.Effect` (behaviours declare whole component types, [Z2-N-ARCH-487]) never collides with VFX.Lifecycle's direct writes in the same phase ([Z2-N-ARCH-484]).

### Zenith_VFXColliderComponent

```cpp
/// @contract enum Zenith_VFXColliderShape
/// Ownership: Plain enumeration; the values of Zenith_VFXColliderComponent::m_uShape (property 1).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; another value disables the collider and counts vfx.colliders.invalid.
/// Version: since Z2 Stage 5. Serialized as a uint8; values never move.
/// @endcontract
enum Zenith_VFXColliderShape : uint8
{
	VFX_COLLIDER_SHAPE_PLANE = 1,     // the entity's local XZ plane (+Y normal), infinite
	VFX_COLLIDER_SHAPE_SPHERE = 2,    // radius m_fRadius about the entity origin
	VFX_COLLIDER_SHAPE_BOX = 3,       // half extents m_xHalfExtents, oriented by the entity's rotation
	VFX_COLLIDER_SHAPE_CAPSULE = 4,   // segment of half length m_xHalfExtents.y along local +Y, radius m_fRadius
};

/// @contract value Zenith_VFXColliderComponent
/// Ownership: Plain component value (Zenith.VFX.Collider), authored in scenes and prefabs.
/// Thread affinity: Written in PHASE_LATE_SCRIPTS (authority) or by commands; read by VFX.Update.
/// Invalidation: None.
/// Units: Metres.
/// Errors: None stored; a non-finite or negative dimension disables the collider (vfx.colliders.invalid).
/// Version: since Z2 Stage 5; type version 1; every property serialized.
/// @endcontract
struct Zenith_VFXColliderComponent
{
	uint8 m_uShape = VFX_COLLIDER_SHAPE_PLANE;                  // 1 Shape
	Zenith_Vector3 m_xHalfExtents{ 0.5f, 0.5f, 0.5f };           // 2 HalfExtents (box; capsule uses .y)
	float m_fRadius = 0.5f;                                      // 3 Radius (sphere, capsule)
	uint32 m_uCollisionMask = 0xFFFFFFFFu;                      // 4 CollisionMask: AND with the collision module's mask
	bool m_bEnabled = true;                                      // 5 Enabled
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	static const Zenith_TypeID xTYPE_ID;                         // Zenith_MakeTypeID("Zenith.VFX.Collider")
};
```

```cpp
/// @contract value Zenith_VFXViewerComponent
/// Ownership: Plain component value (Zenith.VFX.Viewer): an extra LOD viewpoint at its entity's world transform (editor
///   viewports in the edit world, a game's secondary camera); the world's main camera is always a viewer without it.
/// Thread affinity: Written in PHASE_LATE_SCRIPTS (authority) or by commands; read by VFX.Update.
/// Invalidation: None.
/// Units: Radians for m_fFovY.
/// Errors: None stored; m_fFovY outside (0, π) is clamped into [0.01, 3.1] by the reader and counted in vfx.viewers.clamped.
/// Version: since Z2 Stage 5; type version 1; serialized.
/// @endcontract
struct Zenith_VFXViewerComponent
{
	float m_fFovY = 1.0471976f;     // 1 FovY (60 degrees)
	bool m_bEnabled = true;         // 2 Enabled
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	static const Zenith_TypeID xTYPE_ID;    // Zenith_MakeTypeID("Zenith.VFX.Viewer")
};

/// @contract value Zenith_VFXPreviewControlComponent
/// Ownership: Plain component value (Zenith.VFX.PreviewControl) that EDIT's effect preview writes on the preview entity
///   (Zenith2/Docs/Design/VFX/Authoring.md "Preview session"); a game never needs it.
/// Thread affinity: Written in PHASE_LATE_SCRIPTS or by commands; read by VFX.Update.
/// Invalidation: None.
/// Units: Seconds.
/// Errors: None stored; m_fSeekSeconds is clamped into [0, fVFX_MAX_SEEK_SECONDS].
/// Version: since Z2 Stage 5; type version 1; not serialized (a preview control never reaches a saved scene).
/// @endcontract
struct Zenith_VFXPreviewControlComponent
{
	float m_fSeekSeconds = 0.0f;    // 1 SeekSeconds: the effect time a seek re-simulates to
	uint32 m_uSeekRequest = 0;     // 2 SeekRequest: any change of value re-simulates the instance to m_fSeekSeconds
	bool m_bFreeze = false;         // 3 Freeze: presentation time stops after the seek
	bool m_bShowBounds = false;     // 4 ShowBounds: draws the instance's CPU bounds and its GPU-measured bounds
	static constexpr bool bZENITH_TRIVIALLY_RELOCATABLE = true;
	static const Zenith_TypeID xTYPE_ID;    // Zenith_MakeTypeID("Zenith.VFX.PreviewControl")
};

inline constexpr float fVFX_MAX_SEEK_SECONDS = 30.0f;
inline constexpr float fVFX_DETACHED_ENDLESS_SECONDS = 10.0f;   // the spawn span of an endless effect played detached
```

### Component registration

```cpp
/// @contract functions Zenith_VFXComponentRegistration
/// Ownership: Registers the five component types above with the registry the module's RegisterTypes receives.
/// Thread affinity: MAIN_THREAD, startup step 6 (inside Zenith_Module_VFX_RegisterTypes).
/// Invalidation: None.
/// Units: None.
/// Errors: The registry's Zenith_ReflectError, returned unchanged.
/// Version: since Z2 Stage 5.
/// @endcontract
Zenith_Result<void, Zenith_ReflectError> Zenith_RegisterVFXComponentTypes(Zenith_TypeRegistry& xTypes);
```

Each type is registered with `Zenith_TypeRegistry::BeginComponent<T>(szStableName, 1)`, `DefaultAuthority(PHASE_LATE_SCRIPTS)`, one `Property(uPROP_…, szName, &T::m_…)` per row with `Flags(PROPERTY_FLAG_SERIALIZED)` exactly where the tables say "serialized", the ranges and defaults of the tables, `Zenith_MakeLifecycleHooks<T>()` for `Zenith_VFXEffectComponent` (its asset handle), then `End()` (`Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_TypeBuilder"`). Property-ID constants follow the RPROD pattern: `uPROP_VFX_EFFECT_EFFECT = 1` … `uPROP_VFX_EFFECT_LAYER_MASK = 17`, `uPROP_VFX_EFFECT_STATE_START_TICK = 1` … `uPROP_VFX_EFFECT_STATE_STATE = 6`, `uPROP_VFX_COLLIDER_SHAPE = 1` … `uPROP_VFX_COLLIDER_ENABLED = 5`, `uPROP_VFX_VIEWER_FOV_Y = 1`, `uPROP_VFX_VIEWER_ENABLED = 2`, `uPROP_VFX_PREVIEW_SEEK_SECONDS = 1` … `uPROP_VFX_PREVIEW_SHOW_BOUNDS = 4`.

### Authority census rows

The registering diff (slice 7 of `Zenith2/Docs/Design/VFX.md → "Stage 5 slices"`) adds these rows to `Zenith2/Docs/Research/Phases/AuthorityCensus.md` in the twelve columns of `Zenith2/Docs/Design/GAME/PhaseContract.md → "Census format"` (change-control rule 3 of `Zenith2/Docs/Design/GAME/PhaseContract.md → "Census rows added by subsequent designs"`). ZenithVFX is a module no Lifecycle suite links, so that heading's table assigns its rows no census test: they are checked by review in the registering diff and by the D-PHASES (open, Stage 1) ruling, and VFX's own registration-shape test `Test_VFXEffectComponent_Registration` asserts every authority, writer and property ID below against the registry.

| Area | Library | Stage | Stable name | Kind | Properties | Owning phase | Writer sources | Arbitration | Writers (system names) | Readers outside the owning phase | Source |
|---|---|---|---|---|---|---|---|---|---|---|---|
| VFX | ZenithVFX | S5 | `Zenith.VFX.Effect` | component | 1 Effect, 2 AutoPlay, 3 Seed, 4 PlayRequest, 5 StopRequest, 6 Paused, 7 TimeScale, 8 DestroyAfterSeconds, 9–16 Parameter0 … Parameter7, 17 LayerMask | LateScripts | — | none | behaviours (10); commands from any other phase; the editor's assignment and play commands | `VFX.Update` (epilogue, 12) | `Zenith2/Docs/Design/VFX/PublicAPI.md` → "Zenith_VFXEffectComponent" |
| VFX | ZenithVFX | S5 | `Zenith.VFX.EffectState` | component | 1 StartTick, 2 StopTick, 3 ObservedPlay, 4 ObservedStop, 5 Generation, 6 State | LateScripts | — | none | `VFX.Lifecycle` (10; the add by command) | `VFX.Update` (12); saves | same |
| VFX | ZenithVFX | S5 | `Zenith.VFX.Collider` | component | 1 Shape, 2 HalfExtents, 3 Radius, 4 CollisionMask, 5 Enabled | LateScripts | — | none | behaviours (10); commands | `VFX.Update` (12) | `Zenith2/Docs/Design/VFX/PublicAPI.md` → "Zenith_VFXColliderComponent" |
| VFX | ZenithVFX | S5 | `Zenith.VFX.Viewer` | component | 1 FovY, 2 Enabled | LateScripts | — | none | behaviours (10); commands; the editor | `VFX.Update` (12) | same |
| VFX | ZenithVFX | S5 | `Zenith.VFX.PreviewControl` | component | 1 SeekSeconds, 2 SeekRequest, 3 Freeze, 4 ShowBounds | LateScripts | — | none | commands of EDIT's preview panel | `VFX.Update` (12) | same |
| VFX | ZenithVFX | S5 | `Zenith.VFX.TriggerQueue` | resource | — (no reflected property) | LateScripts | — | none | `VFX.Spawn` playback (sync points); the VFX trigger consumer (script merges) | `VFX.Update` (12), its sole consumer | `Zenith2/Docs/Design/VFX/Simulation.md` → "Detached effects and the trigger queue" |
| VFX | ZenithVFX | S5 | `Zenith.VFX.EffectStarted` | event | 1 Entity, 2 Effect, 3 Generation | — | — | none | `VFX.Lifecycle` (10) | scripts and subscribers (phase 9 of the next tick) | `Zenith2/Docs/Design/VFX/PublicAPI.md` → "Zenith_VFXEvents header" |
| VFX | ZenithVFX | S5 | `Zenith.VFX.EffectStopped` | event | 1 Entity, 2 Effect, 3 Generation, 4 Reason | — | — | none | `VFX.Lifecycle` (10) | scripts and subscribers (phase 9 of the next tick) | same |

## Zenith_VFXEvents header

```cpp
// Zenith2/Engine/ZenithVFX/Zenith_VFXEvents.h
#pragma once
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithReflect/Zenith_ReflectIDs.h"   // Zenith_TypeID

/// @contract value Zenith_VFXEffectStartedEvent
/// Ownership: Plain event payload of the registered TYPE_KIND_EVENT "Zenith.VFX.EffectStarted", emitted by VFX.Lifecycle in
///   phase 10 and delivered in phase 9 of the next tick ([Z2-N-ARCH-478]).
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: m_xEntity may be stale when the event is read if the entity was destroyed at the end of the emitting tick.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5; type version 1. Field IDs: 1 Entity, 2 Effect, 3 Generation.
/// @endcontract
struct Zenith_VFXEffectStartedEvent
{
	Zenith_EntityHandle m_xEntity;      // 1 Entity
	Zenith_PersistentID m_xEffect;      // 2 Effect: the asset's persistent ID
	uint32 m_uGeneration = 0;          // 3 Generation
	static const Zenith_TypeID xTYPE_ID;    // Zenith_MakeTypeID("Zenith.VFX.EffectStarted")
};

/// @contract value Zenith_VFXEffectStoppedEvent
/// Ownership: Payload of "Zenith.VFX.EffectStopped", emitted by VFX.Lifecycle when it observes a StopRequest change, a
///   restart of a playing effect, or the DestroyAfterSeconds expiry (in the tick whose commands destroy the entity).
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: As Zenith_VFXEffectStartedEvent.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5; type version 1. Field IDs: 1 Entity, 2 Effect, 3 Generation, 4 Reason.
/// @endcontract
struct Zenith_VFXEffectStoppedEvent
{
	Zenith_EntityHandle m_xEntity;      // 1 Entity
	Zenith_PersistentID m_xEffect;      // 2 Effect
	uint32 m_uGeneration = 0;          // 3 Generation: the generation that stopped
	uint8 m_uReason = 0;               // 4 Reason: 1 stop request, 2 restart, 3 destroy-after expiry
	static const Zenith_TypeID xTYPE_ID;    // Zenith_MakeTypeID("Zenith.VFX.EffectStopped")
};
```

A module that reacts to effects (AUDIO attaching a sound cue, GAME counting impacts) declares `ReadsEvent(Zenith_MakeTypeID("Zenith.VFX.EffectStarted"))` by stable name and needs no ZenithVFX header, so no module-to-module edge exists ([Z2-N-ARCH-850]).

## Zenith_VFXCommands header

```cpp
// Zenith2/Engine/ZenithVFX/Zenith_VFXCommands.h
#pragma once
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Quaternion.h"
#include "ZenithECS/Zenith_EntityHandle.h"
#include "ZenithVFX/Zenith_VFXErrors.h"

class Zenith_CommandBuffer;

/// @contract enum Zenith_VFXSpawnFlags
/// Ownership: Plain bit enumeration of Zenith_VFXSpawnPayload::m_uFlags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; unknown bits make the payload VFX_ERROR_INVALID_ARGUMENT at playback.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Zenith_VFXSpawnFlags : uint32
{
	VFX_SPAWN_FLAG_NONE = 0,
	VFX_SPAWN_FLAG_ATTACH = 1u << 0,          // follow m_xAttachTo's interpolated world transform, the payload transform an offset
	VFX_SPAWN_FLAG_ATTACH_ROTATION = 1u << 1, // with ATTACH: follow its rotation too (else world-aligned)
	VFX_SPAWN_FLAG_ATTACH_SCALE = 1u << 2,    // with ATTACH: follow its scale too
};

/// @contract value Zenith_VFXSpawnPayload
/// Ownership: Plain value; the payload of the command kind "VFX.Spawn" (a registered TYPE_KIND_STRUCT "Zenith.VFX.SpawnPayload",
///   96 bytes) and the body of a VFX trigger record ("VFX trigger record").
/// Thread affinity: IMMUTABLE. Members non-const: a reflected type, which the type registry default-constructs and
///   writes property by property.
/// Invalidation: None; m_xAttachTo is resolved at playback.
/// Units: Metres; unit quaternion; seconds.
/// Errors: Validated at playback: a null effect ID, a non-finite transform, a non-unit rotation (|q|² outside [0.98, 1.02]),
///   a time scale outside [0, 8], a lead outside [0, 1] seconds or unknown flag bits drop the record with
///   vfx.triggers.invalid + 1.
/// Version: since Z2 Stage 5; type version 1. Field IDs 1–10 in member order; field 10 occupies what was tail padding, so
///   the size stays 96 bytes.
/// @endcontract
struct Zenith_VFXSpawnPayload
{
	Zenith_PersistentID m_xEffect;              // 1 Effect: the asset's persistent ID
	Zenith_Vector3 m_xPosition;                 // 2 Position (world, or an offset with ATTACH)
	Zenith_Quaternion m_xRotation;              // 3 Rotation
	Zenith_Vector3 m_xScale{ 1.0f, 1.0f, 1.0f };   // 4 Scale
	Zenith_EntityHandle m_xAttachTo;            // 5 AttachTo (VFX_SPAWN_FLAG_ATTACH)
	uint32 m_uSeed = 0;                        // 6 Seed: 0 derives it from the trigger sequence
	uint32 m_uFlags = VFX_SPAWN_FLAG_NONE;     // 7 Flags
	float m_fTimeScale = 1.0f;                  // 8 TimeScale
	float m_afParameters[4] = {};               // 9 Parameters: the effect's first four user parameters; the rest take defaults
	float m_fLeadSeconds = 0.0f;                // 10 LeadSeconds: how long before the end of the recording tick the trigger happened,
	                                            //    0 … 1 s (an animation notify's sub-tick time); 0 = the tick's end
};
static_assert(sizeof(Zenith_VFXSpawnPayload) == 96);

/// @contract functions Zenith_VFXCommandRecording
/// Ownership: Records into a command buffer the caller owns (its system invocation's).
/// Thread affinity: OWNER_THREAD (the recording invocation); recordable in phases 1–10 (m_uAllowedPhaseMask 0x7FE).
/// Invalidation: None.
/// Units: As Zenith_VFXSpawnPayload.
/// Errors: VFX_ERROR_INVALID_ARGUMENT for a payload failing the checks of its block (checked at record time as well, so a
///   C++ caller learns at once); nothing else: recording into a Zenith_CommandBuffer returns nothing and its arena
///   exhaustion is fatal as that type defines (Zenith2/Docs/Design/ECS/WorldAPI.md "Zenith_CommandBuffer header").
/// Version: since Z2 Stage 5. Source API.
/// @endcontract
Zenith_Result<void, Zenith_VFXError> Zenith_RecordVFXSpawn(Zenith_CommandBuffer& xBuffer,
	const Zenith_VFXSpawnPayload& xPayload);
```

The command kind is `{ "VFX.Spawn", Zenith_MakeTypeID("Zenith.VFX.SpawnPayload"), 96, 0x7FE, &Zenith_VFXSpawnPlayback, pxInstance }`. Its playback (sync-point step 6 order, merge order) appends one trigger record to the world's trigger queue within the queue's C++ share (the capacity minus the script reserve; beyond it the record is dropped and counted, `Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"`); it writes no ECS state.

## Zenith_VFXReference header

```cpp
// Zenith2/Engine/ZenithVFX/Zenith_VFXReference.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Bounds.h"
#include "ZenithCore/Zenith_Arena.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithFlux/Flux_ParticleEffect.h"
#include "ZenithFlux/Flux_Particles.h"
#include "ZenithVFX/Zenith_VFXErrors.h"

/// @contract value Zenith_VFXReferenceTransform
/// Ownership: Plain value; one step of Zenith_VFXReferenceDesc::m_axEmitterTrack.
/// Thread affinity: IMMUTABLE. Members non-const: callers fill arrays of it element by element before passing them.
/// Invalidation: None.
/// Units: Metres; the top three rows of a column-vector emitter-to-world matrix.
/// Errors: A non-finite row makes Zenith_VFXRunReference return VFX_ERROR_INVALID_ARGUMENT.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Zenith_VFXReferenceTransform
{
	Zenith_Vector4 m_axRows[3];                 // the transform at the end of the step; the previous step's (or the first) at its start
};

/// @contract value Zenith_VFXReferenceDesc
/// Ownership: Plain value; borrows the effect and the spans for the duration of one Zenith_VFXRunReference call.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Seconds, metres, metres per second.
/// Errors: Zenith_VFXRunReference refuses a null effect, a non-finite or non-positive step or duration, more parameter
///   values than the effect declares slots, or a non-empty track whose length is not ceil(m_fSeconds / m_fStepSeconds)
///   (VFX_ERROR_INVALID_ARGUMENT).
/// Version: since Z2 Stage 5.
/// @endcontract
struct Zenith_VFXReferenceDesc
{
	const Zenith_ParticleEffectAsset* m_pxEffect = nullptr;          // a loaded or parsed effect (Flux_ParseParticleEffect)
	uint32 m_uInstanceSeed = 0;                                     // the instance seed every emitter seed derives from
	float m_fSeconds = 0.0f;                                         // effect time to simulate from 0
	float m_fStepSeconds = 1.0f / 60.0f;                             // the fixed presentation step
	uint64 m_ulMaxParticleSteps = 100000000ull;                     // the work budget: VFX_ERROR_LIMIT beyond it
	uint32 m_uLODLevel = 0;
	Zenith_Span<const float> m_afParameters;                         // empty: the declared defaults
	Zenith_Span<const Flux_RenderParticleCollider> m_axColliders;    // empty for the cook's bake
	Zenith_Vector3 m_xWind{ 0.0f, 0.0f, 0.0f };                      // a uniform wind for the wind module; zero for the bake
	Zenith_Span<const Zenith_VFXReferenceTransform> m_axEmitterTrack; // one emitter-to-world transform per step; empty: identity
	bool m_bExcludeWorldTerms = false;                               // the bake: gravity, world-space force, a world-space emitter's
	                                                                 // emitter-space force and turbulence contribute zero; wind
	                                                                 // relaxes towards zero wind (m_xWind ignored); distance
	                                                                 // spawning runs as its reference rate (Authoring.md "Bounds baking")
	bool m_bKeepFinalStates = false;                                 // copy every emitter's slots into the result
};

/// @contract value Zenith_VFXReferenceEmitterResult
/// Ownership: Plain value filled by Zenith_VFXRunReference; m_axFinalStates points into the arena the call received.
/// Thread affinity: IMMUTABLE once returned. Members non-const: an element of an output array that its producer fills
///   in place.
/// Invalidation: m_axFinalStates is invalid when that arena is reset.
/// Units: Metres (bounds in emitter space with an empty track, else world space; draw extents included), particles,
///   particle-steps.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Zenith_VFXReferenceEmitterResult
{
	Zenith_AABB m_xDrawBounds;                      // every live particle after every step and every event record's position,
	                                                // expanded by its draw extent
	Zenith_AABB m_xSpawnRelativeBounds;             // every live particle's position minus its own spawn position, expanded by
	                                                // its draw extent (the bake's S_t of an event target; the simulator keeps
	                                                // each slot's spawn position in the call's arena)
	uint32 m_uPeakLive = 0;
	uint32 m_uOverwrites = 0;                      // live particles overwritten by a newer spawn (event children included)
	uint64 m_ulParticleSteps = 0;
	Zenith_Span<const Flux_ParticleState> m_axFinalStates;   // slot order, capacity entries, when m_bKeepFinalStates
};

/// @contract functions Zenith_VFXReference
/// Ownership: Zenith_VFXRunReference allocates its working state and the final states from xArena only.
/// Thread affinity: ANY_THREAD; concurrent calls share nothing.
/// Invalidation: None.
/// Units: As the two values above.
/// Errors: VFX_ERROR_INVALID_ARGUMENT (the desc's block; axResults not exactly the effect's emitter count),
///   VFX_ERROR_LIMIT (m_ulMaxParticleSteps exceeded; the results hold the measurements up to that point),
///   VFX_ERROR_OUT_OF_MEMORY (the arena).
/// Version: since Z2 Stage 5. Source API for tools and tests.
/// @endcontract
Zenith_Result<void, Zenith_VFXError> Zenith_VFXRunReference(const Zenith_VFXReferenceDesc& xDesc,
	Zenith_Arena& xArena, Zenith_Span<Zenith_VFXReferenceEmitterResult> axResults);
```

The reference simulation is ZenithVFX's CPU simulator (`Zenith2/Docs/Design/VFX/Simulation.md → "CPU simulation"`) run single-threaded over every emitter of the effect, `gpu` ones included: the simulator implements every module the catalogue marks for either target, the GPU-only inputs (depth collision, precipitation occlusion) reading as empty. A code-authored update type has no C++ form, so an emitter with one is simulated without it and its result carries the bounds of the built-in modules alone; the cook therefore requires `fixed` bounds for such an emitter (`Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"`). With an empty `m_axEmitterTrack` the emitter stands at the origin with identity transform, so results are in emitter space; with a track the emitter follows it step by step (distance spawning, inherited velocity, world-space forces and local-space mapping all run as at run time), which is how `Test_VFXBounds_AttachedAndWorldSpace` produces the particles the runtime bounds must contain. The bake (`m_bExcludeWorldTerms`) measures the emitter-frame part only, and the cook adds the world terms analytically (`Zenith2/Docs/Design/VFX/Authoring.md → "Bounds baking"`). The same function backs the CPU reference parity tests (`Zenith2/Docs/Design/VFX/Tests.md → "CPU reference parity tests"`), so the arithmetic the cook bakes bounds with is the arithmetic the GPU is held to.

## Zenith_VFXScriptBindings

### Luau surface

The VFX module registers one `Zenith_ScriptBindingTable` namespace, `VFX` (Luau `Zenith.VFX`), through `Zenith_ModuleStartContext::RegisterScriptBindings` (`Zenith2/Docs/Design/SCRIPT/Bindings.md → "Side-effect consumer registration"`). Every function is `SCRIPT_FUNCTION_EFFECT_BUFFERED` with side-effect mask `1 << SIDE_EFFECT_KIND_VFX_TRIGGER` and contexts `SCRIPT_FUNCTION_CONTEXT_SYSTEM | SCRIPT_FUNCTION_CONTEXT_BEHAVIOUR`, except `ParameterIndex`, which reads the calling world's published load result (below) and is never `PURE`:

| Luau signature (generated `.d.luau`) | Function ID | Record | Misuse |
|---|---|---|---|
| `Zenith.VFX.Spawn(effect: Asset<ParticleEffect>, position: ZenithVec3, rotation: ZenithQuat?, scale: ZenithVec3?, parameters: {number}?, leadSeconds: number?) -> ()` | `0x56460101` | a trigger record with flags NONE; `leadSeconds` (default 0) is `m_fLeadSeconds` | `ZS5601` |
| `Zenith.VFX.SpawnAttached(effect: Asset<ParticleEffect>, entity: ZenithEntity, offset: ZenithVec3?, followRotation: boolean?, parameters: {number}?, leadSeconds: number?) -> ()` | `0x56460102` | a trigger record with `ATTACH` (+ `ATTACH_ROTATION` when `followRotation` is true, the default) | `ZS5602` |
| `Zenith.VFX.ParameterIndex(effect: Asset<ParticleEffect>, name: string) -> number?` | `0x56460103` | none (reads the effect only through the calling world's published load result for the handle, the state `Asset:IsLoaded()` reads, `Zenith2/Docs/Design/ASSET/Loading.md → "Publication to simulation"`: `nil` until that result is loaded, then the name's first slot from the cooked effect's parameter table) | none: an unknown name, or an effect whose load the world has not published as loaded, returns `nil` |

`ParameterIndex` never reads the live registry (`GetState`, `Resolve`), whose answer depends on when a load completes: it takes the effect class and contexts SCRIPT gives the `Asset` accessors that read published state, so its result is a function of simulation state alone and a behaviour that branches on `nil` takes the same branch at the same tick on every run, whatever the load's wall-clock completion (Z2-D-019). It is not `SCRIPT_FUNCTION_EFFECT_PURE`, which `Zenith2/Docs/Scripting/Bindings.md → "Public binding types"` defines as no world access with a result that depends on the arguments alone.

<!-- assumption: pending SCRIPT (cross-area row VX17) -->
SCRIPT names the accessor through which a module's binding reads a handle's published load result in the calling world (the state `Asset:IsLoaded()` reads), with the effect class and contexts a binding using it registers under; `ParameterIndex` uses that accessor and that class.
<!-- /assumption -->

A wrong argument count or type to any of the three is SCRIPT's dispatcher error (`ZS1510`, `ZS1511`, `Zenith2/Docs/Scripting/MisuseCatalogue.md → "Catalogue"`), raised before the binding runs. VFX's own codes are the rows of its block ZS5600–ZS5699 (`Zenith2/Docs/Scripting/MisuseCatalogue.md → "Code ranges"`), each a controlled Luau error, never an assert ([Z2-N-CONTRACT-473]), in the catalogue's column format:

| Code | Kind | Trigger | Text |
|---|---|---|---|
| ZS5601 | MISUSE | `VFX.Spawn` with a null effect, a non-finite position, rotation, scale or parameter, a non-unit rotation, more than 4 parameters, or a `leadSeconds` outside [0, 1] | `VFX.Spawn: <reason>` |
| ZS5602 | MISUSE | `VFX.SpawnAttached` with a null effect or entity, a stale entity, a non-finite offset or parameter, more than 4 parameters, or a `leadSeconds` outside [0, 1] | `VFX.SpawnAttached: <reason>` |

An animation notify reaches an effect through a script that subscribes to `Zenith.Animation.Notify` and calls `VFX.Spawn` with `leadSeconds = (k − m_fTimeIntoTick) × tickDuration` for the notify's k-tick evaluation span (`Zenith_AnimNotifyEvent`, `Zenith2/Docs/Design/ANIM/Runtime.md`), so the effect starts at the notify's sub-tick time (start time of `Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"`); ANIM's `ANIM_EVENT_FLAG_VFX` is advisory for scripts and no module consumes it (cross-area row VX16).

Attached effects that live on an entity (looping fire, torch smoke, weather emitters) are not spawned through bindings: a script adds or edits the `Zenith.VFX.Effect` component through the ordinary component paths (writing `PlayRequest`, `StopRequest`, `Parameter0..7` directly in Late scripts or by command elsewhere, [Z2-N-ARCH-483]).

### VFX trigger record

A binding serializes one `Zenith_VFXSpawnPayload` (96 bytes, little-endian, field by field) as the payload of a `Zenith_SideEffectRecord` of kind `SIDE_EFFECT_KIND_VFX_TRIGGER` with capacity demand one `SCRIPT_CAPACITY_CLASS_VFX_TRIGGERS` unit (`Zenith2/Docs/Scripting.md → "Merge-time capacity and asynchronous results"`, row `Z2-B-SCRIPT-024`). At commit the script runtime calls the module's `Zenith_ScriptSideEffectConsumer::Consume(uWorldTag, SIDE_EFFECT_KIND_VFX_TRIGGER, 0, xPayload)` (`uint16 uWorldTag`) in merge order; the consumer decodes the payload, validates it as the command playback does, and appends it to the world's trigger queue within the queue's script reserve, which is sized so that every trigger the script merge accepted fits (`Zenith2/Docs/Design/VFX/Simulation.md → "Detached effects and the trigger queue"`). The consumer never fails; a record beyond the queue's total capacity, which only a misconfigured reserve permits, is dropped and counted in `vfx.triggers.dropped` (BR-DROP), because the trigger is presentation and losing it changes no simulation state.

## Flux_ParticleEffect header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_ParticleEffect.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Owned.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Bounds.h"
#include "ZenithCore/Zenith_StringView.h"
#include "ZenithAssets/Zenith_AssetTypes.h"
#include "ZenithAssets/Zenith_AssetKindLoader.h"
#include "ZenithRHI/Zenith_RHIShaderInterface.h"

class Zenith_Heap;
```

### Effect enumerations

```cpp
inline constexpr uint16 uFLUX_PARTICLE_EFFECT_FORMAT_VERSION = 1;
inline constexpr uint32 uFLUX_PARTICLE_MAX_EMITTERS = 16;            // per effect
inline constexpr uint32 uFLUX_PARTICLE_MAX_MODULES = 47;             // per emitter: 18 spawn, 8 initialisation, 13 update, 5 over-life,
                                                                      // 1 render, 1 light, 1 decal (EffectFormat.md "Document limits")
inline constexpr uint32 uFLUX_PARTICLE_MAX_CURVES = 64;              // per effect
inline constexpr uint32 uFLUX_PARTICLE_CURVE_SAMPLES = 32;
inline constexpr uint32 uFLUX_PARTICLE_MAX_USER_PARAMETERS = 8;      // floats per effect
inline constexpr uint32 uFLUX_PARTICLE_MAX_LOD_LEVELS = 4;
inline constexpr uint32 uFLUX_PARTICLE_MAX_EVENT_EDGES = 16;         // per effect
inline constexpr uint32 uFLUX_PARTICLE_MAX_EVENTS_PER_PARTICLE = 4;  // E ≤ 4 compact event slots: collisions 0 … m − 1, death m = E − 1
                                                                      // (death-only source: death 0; Simulation.md "Event spawning")
inline constexpr uint32 uFLUX_PARTICLE_MAX_CHILDREN_PER_EVENT = 16;
inline constexpr uint32 uFLUX_PARTICLE_MAX_EVENT_RECORDS = 262144;   // per source emitter: C_src × E (Simulation.md "Event spawning")
inline constexpr uint32 uFLUX_PARTICLE_MAX_TRAIL_SAMPLES = 8;
inline constexpr uint32 uFLUX_PARTICLE_MAX_TEXTURES = 8;             // per effect
inline constexpr uint32 uFLUX_PARTICLE_MAX_MESHES = 4;               // per effect
inline constexpr uint32 uFLUX_PARTICLE_MAX_GPU_CAPACITY = 262144;    // per emitter
inline constexpr uint32 uFLUX_PARTICLE_MAX_CPU_CAPACITY = 4096;      // per emitter
inline constexpr uint32 uFLUX_PARTICLE_MAX_EMITTER_CONSTANTS = 4096; // bytes of one emitter's constants block
inline constexpr uint32 uFLUX_PARTICLE_THREADS_PER_GROUP = 64;
inline constexpr uint32 uFLUX_PARTICLE_CAPACITY_GRANULE = 64;        // capacities are multiples of the group size
inline constexpr float fFLUX_PARTICLE_MAX_STEP_SECONDS = 1.0f / 15.0f;   // the per-frame simulation step clamp
inline constexpr float fFLUX_PARTICLE_MAX_PREWARM_SECONDS = 2.0f;        // a preview seek allows fVFX_MAX_SEEK_SECONDS
inline constexpr float fFLUX_PARTICLE_PREWARM_STEP_SECONDS = 1.0f / 30.0f;
inline constexpr uint32 uFLUX_PARTICLE_MAX_SORTED_EMITTERS = 1024;    // depth-sorted emitters per frame (segments of the sort)
inline constexpr uint32 uFLUX_PARTICLE_SORT_PARTITION_KEYS = 2048;    // pairs per ParticleSort partition (256 threads x 8)
inline constexpr uint32 uFLUX_PARTICLE_MAX_DRAWS_PER_VIEW = 4096;     // draw commands per view
inline constexpr uint32 uFLUX_PARTICLE_MAX_COLLIDERS_PER_WORLD = 64;  // analytic colliders per world per frame
inline constexpr uint32 uFLUX_PARTICLE_MAX_PENDING_CATCH_UP_STEPS = 64; // kept steps per emitter (Simulation.md "Deferred and failed frames")
inline constexpr uint32 uFLUX_PARTICLE_MAX_STEPS_PER_FRAME = 128;       // steps of one emitter in one frame: 64 kept + 64 of the frame

/// @contract enum Flux_ParticleTarget
/// Ownership: Plain enumeration (effect document key "target").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the reader refuses any other value (FLUX_PARTICLE_ERROR_CORRUPT).
/// Version: since Z2 Stage 5. Cooked as uint8; values never move.
/// @endcontract
enum Flux_ParticleTarget : uint8 { FLUX_PARTICLE_TARGET_CPU = 1, FLUX_PARTICLE_TARGET_GPU = 2 };

/// @contract enum Flux_ParticleSpace
/// Ownership: Plain enumeration ("space").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Flux_ParticleTarget.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_ParticleSpace : uint8 { FLUX_PARTICLE_SPACE_WORLD = 1, FLUX_PARTICLE_SPACE_LOCAL = 2 };

/// @contract enum Flux_ParticleRenderKind
/// Ownership: Plain enumeration (the render module of an emitter).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Flux_ParticleTarget.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_ParticleRenderKind : uint8
{
	FLUX_PARTICLE_RENDER_NONE = 0,      // simulated, never drawn (an event source, a light or decal carrier)
	FLUX_PARTICLE_RENDER_SPRITE = 1,
	FLUX_PARTICLE_RENDER_RIBBON = 2,    // the emitter's particles in spawn order form one strip
	FLUX_PARTICLE_RENDER_MESH = 3,
	FLUX_PARTICLE_RENDER_TRAIL = 4,     // each particle draws a strip through its own trail samples
};

/// @contract enum Flux_ParticleBlend
/// Ownership: Plain enumeration ("blend").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Flux_ParticleTarget.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_ParticleBlend : uint8 { FLUX_PARTICLE_BLEND_ALPHA = 1, FLUX_PARTICLE_BLEND_PREMULTIPLIED = 2, FLUX_PARTICLE_BLEND_ADDITIVE = 3 };

/// @contract enum Flux_ParticleSortMode
/// Ownership: Plain enumeration ("sort").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Flux_ParticleTarget; the cook refuses DEPTH with ADDITIVE blending and with a ribbon render kind.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_ParticleSortMode : uint8 { FLUX_PARTICLE_SORT_DEPTH = 1, FLUX_PARTICLE_SORT_OLDEST_FIRST = 2, FLUX_PARTICLE_SORT_NEWEST_FIRST = 3 };

/// @contract enum Flux_ParticleFacing
/// Ownership: Plain enumeration (sprite and ribbon "facing").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Flux_ParticleTarget.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_ParticleFacing : uint8
{
	FLUX_PARTICLE_FACING_CAMERA_PLANE = 1,     // parallel to the view plane
	FLUX_PARTICLE_FACING_CAMERA_POSITION = 2,  // towards the camera position
	FLUX_PARTICLE_FACING_VELOCITY = 3,         // stretched along velocity, rotated about it towards the camera
	FLUX_PARTICLE_FACING_AXIS = 4,             // rotated about a fixed emitter-space axis towards the camera
	FLUX_PARTICLE_FACING_WORLD = 5,            // a fixed emitter-space orientation (ground decals, shockwave rings)
};

/// @contract enum Flux_ParticleLighting
/// Ownership: Plain enumeration ("lighting").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Flux_ParticleTarget.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_ParticleLighting : uint8 { FLUX_PARTICLE_LIGHTING_UNLIT = 0, FLUX_PARTICLE_LIGHTING_VERTEX = 1, FLUX_PARTICLE_LIGHTING_PIXEL = 2 };

/// @contract enum Flux_ParticleCullBehaviour
/// Ownership: Plain enumeration ("cullBehaviour").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Flux_ParticleTarget.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_ParticleCullBehaviour : uint8 { FLUX_PARTICLE_CULL_KILL = 1, FLUX_PARTICLE_CULL_PAUSE = 2 };

/// @contract enum Flux_ParticleEventKind
/// Ownership: Plain enumeration (an event edge's "on").
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: As Flux_ParticleTarget.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_ParticleEventKind : uint8 { FLUX_PARTICLE_EVENT_DEATH = 1, FLUX_PARTICLE_EVENT_COLLISION = 2 };

/// @contract enum Flux_ParticleModuleID
/// Ownership: Plain enumeration; the closed module vocabulary of Zenith2/Docs/Design/VFX/EffectFormat.md "Module identifiers".
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the reader refuses an identifier outside the table (FLUX_PARTICLE_ERROR_CORRUPT).
/// Version: since Z2 Stage 5. Cooked as uint16; values never move; a new module takes a new value and bumps the format version.
/// @endcontract
enum Flux_ParticleModuleID : uint16
{
	FLUX_PARTICLE_MODULE_SPAWN_RATE = 0x0101, FLUX_PARTICLE_MODULE_SPAWN_BURST = 0x0102,
	FLUX_PARTICLE_MODULE_SPAWN_DISTANCE = 0x0103, FLUX_PARTICLE_MODULE_SPAWN_EVENT = 0x0104,
	FLUX_PARTICLE_MODULE_INIT_SHAPE = 0x0201, FLUX_PARTICLE_MODULE_INIT_VELOCITY = 0x0202,
	FLUX_PARTICLE_MODULE_INIT_LIFETIME = 0x0203, FLUX_PARTICLE_MODULE_INIT_SIZE = 0x0204,
	FLUX_PARTICLE_MODULE_INIT_ROTATION = 0x0205, FLUX_PARTICLE_MODULE_INIT_COLOUR = 0x0206,
	FLUX_PARTICLE_MODULE_INIT_SUB_UV = 0x0207, FLUX_PARTICLE_MODULE_INIT_INHERIT_VELOCITY = 0x0208,
	FLUX_PARTICLE_MODULE_UPDATE_GRAVITY = 0x0301, FLUX_PARTICLE_MODULE_UPDATE_FORCE = 0x0302,
	FLUX_PARTICLE_MODULE_UPDATE_DRAG = 0x0303, FLUX_PARTICLE_MODULE_UPDATE_TURBULENCE = 0x0304,
	FLUX_PARTICLE_MODULE_UPDATE_VORTEX = 0x0305, FLUX_PARTICLE_MODULE_UPDATE_ATTRACTOR = 0x0306,
	FLUX_PARTICLE_MODULE_UPDATE_WIND = 0x0307, FLUX_PARTICLE_MODULE_UPDATE_SPEED_LIMIT = 0x0308,
	FLUX_PARTICLE_MODULE_UPDATE_COLLISION = 0x0309, FLUX_PARTICLE_MODULE_UPDATE_KILL_VOLUME = 0x030A,
	FLUX_PARTICLE_MODULE_UPDATE_TRAIL = 0x030B, FLUX_PARTICLE_MODULE_UPDATE_PRECIPITATION_OCCLUSION = 0x030C,
	FLUX_PARTICLE_MODULE_UPDATE_CUSTOM = 0x030D,
	FLUX_PARTICLE_MODULE_LIFE_COLOUR = 0x0401, FLUX_PARTICLE_MODULE_LIFE_SIZE = 0x0402,
	FLUX_PARTICLE_MODULE_LIFE_SUB_UV = 0x0403, FLUX_PARTICLE_MODULE_LIFE_COLOUR_BY_SPEED = 0x0404,
	FLUX_PARTICLE_MODULE_LIFE_SIZE_BY_SPEED = 0x0405,
	FLUX_PARTICLE_MODULE_RENDER_SPRITE = 0x0501, FLUX_PARTICLE_MODULE_RENDER_RIBBON = 0x0502,
	FLUX_PARTICLE_MODULE_RENDER_MESH = 0x0503, FLUX_PARTICLE_MODULE_RENDER_TRAIL = 0x0504,
	FLUX_PARTICLE_MODULE_EFFECT_LIGHT = 0x0601, FLUX_PARTICLE_MODULE_EFFECT_DECAL = 0x0602,
};

/// @contract enum Flux_ParticleError
/// Ownership: Plain enumeration of the ZenithFlux particle failures.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; it is the error. CORRUPT: a cooked effect fails a reader check of Zenith2/Docs/Design/VFX/EffectFormat.md
///   "Reader validation". VERSION: the format version is not uFLUX_PARTICLE_EFFECT_FORMAT_VERSION. LIMIT: a count above a
///   limit of "Format limits". OUT_OF_MEMORY: a heap allocation failed. POOL_FULL: the particle pool's region is at its
///   maximum. DEVICE: a device call failed (the effect constants buffer or the quad index buffer). INVALID_DESC: a
///   Flux_ParticleSystemDesc outside its domain. ALREADY_ENABLED: EnableParticles called twice.
/// Version: since Z2 Stage 5. Appended, never renumbered; the kind loader's Fail code is this value.
/// @endcontract
enum Flux_ParticleError : uint8
{
	FLUX_PARTICLE_ERROR_CORRUPT = 1,
	FLUX_PARTICLE_ERROR_VERSION,
	FLUX_PARTICLE_ERROR_LIMIT,
	FLUX_PARTICLE_ERROR_OUT_OF_MEMORY,
	FLUX_PARTICLE_ERROR_POOL_FULL,
	FLUX_PARTICLE_ERROR_DEVICE,
	FLUX_PARTICLE_ERROR_INVALID_DESC,
	FLUX_PARTICLE_ERROR_ALREADY_ENABLED,
};

constexpr const char* Zenith_ToString(Flux_ParticleError eError);
```

### Flux_ParticleEmitterDesc

The decoded, CPU-side view of one cooked emitter record (`Zenith2/Docs/Design/VFX/EffectFormat.md → "Emitter record section"`). Every member is read from the cooked bytes by the reader; nothing in it is computed at run time.

```cpp
/// @contract value Flux_ParticleSpawnDesc
/// Ownership: Plain value inside Flux_ParticleEmitterDesc.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Particles per second, seconds, metres.
/// Errors: None; validated by the reader.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleSpawnDesc
{
	uint32 m_uRateModule = 0xFFFFFFFFu;       // module index of SPAWN_RATE, or none
	uint32 m_uDistanceModule = 0xFFFFFFFFu;   // SPAWN_DISTANCE
	uint32 m_uEventEdge = 0xFFFFFFFFu;        // index into the effect's event edges for an event-spawned emitter
	uint32 m_uFirstBurst = 0;                 // into the effect's burst table
	uint32 m_uBurstCount = 0;
	float m_fMaxRate = 0.0f;                   // the curve maximum times the largest parameter binding scale (cook-computed)
	float m_fSpacingMetres = 0.0f;             // distance spawning
	float m_fPrewarmSeconds = 0.0f;            // 0 … fFLUX_PARTICLE_MAX_PREWARM_SECONDS
};

/// @contract value Flux_ParticleBurst
/// Ownership: Plain value of the effect's burst table.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Seconds; particles.
/// Errors: None; validated by the reader.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleBurst
{
	float m_fTime = 0.0f;          // effect time of the first cycle
	uint32 m_uCountMin = 0;
	uint32 m_uCountMax = 0;       // the count is drawn per cycle with draw index 0x2000 + cycle (Simulation.md "Burst spawning")
	uint32 m_uCycles = 1;         // 0 = every loop of the effect, without a cycle limit
	float m_fInterval = 0.0f;      // seconds between cycles
};

/// @contract value Flux_ParticleLODLevel
/// Ownership: Plain value of an effect's LOD table.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Screen size is a fraction of the viewport height (the bounding sphere's projected diameter / viewport height).
/// Errors: None; the reader refuses levels that are not strictly decreasing in m_fScreenSize.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleLODLevel
{
	float m_fScreenSize = 0.0f;           // the level applies at or above this screen size
	float m_fSpawnScale = 1.0f;           // multiplies every spawn count of the level
	uint32 m_auDisabledModuleMask[16] = {};   // per emitter index: bit i disables update module i of that emitter
	                                           // (Flux_ParticleEmitterDesc::m_auUpdateModules); decoded from the level's mask
	                                           // block in the constants image (EffectFormat.md "Event edge section", LOD records)
};

/// @contract value Flux_ParticleEventEdge
/// Ownership: Plain value of an effect's event-edge table.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Particles.
/// Errors: None; the reader refuses a source equal to the target, a cycle, or an index outside the emitter count.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleEventEdge
{
	uint8 m_uSourceEmitter = 0;
	uint8 m_uTargetEmitter = 0;
	Flux_ParticleEventKind m_eOn = FLUX_PARTICLE_EVENT_DEATH;
	uint8 m_uChildrenPerEvent = 1;           // 1 … uFLUX_PARTICLE_MAX_CHILDREN_PER_EVENT
	uint8 m_uMaxCollisionEvents = 1;         // COLLISION: the first n collisions of a particle raise events (1 … 3)
	uint8 m_auReserved[3] = {};
	float m_fInheritVelocity = 0.0f;          // the child's initial velocity adds this × the event velocity
};

/// @contract value Flux_ParticleRenderDesc
/// Ownership: Plain value inside Flux_ParticleEmitterDesc.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: None.
/// Units: Metres for the soft distance; dimensionless otherwise.
/// Errors: None; validated by the reader.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleRenderDesc
{
	Flux_ParticleRenderKind m_eKind = FLUX_PARTICLE_RENDER_NONE;
	Flux_ParticleBlend m_eBlend = FLUX_PARTICLE_BLEND_ALPHA;
	Flux_ParticleSortMode m_eSort = FLUX_PARTICLE_SORT_OLDEST_FIRST;
	Flux_ParticleFacing m_eFacing = FLUX_PARTICLE_FACING_CAMERA_PLANE;
	Flux_ParticleLighting m_eLighting = FLUX_PARTICLE_LIGHTING_UNLIT;
	uint8 m_uTextureIndex = 0xFF;            // into the effect's texture dependencies; 0xFF: none (white)
	uint8 m_uMeshIndex = 0xFF;               // MESH: into the effect's mesh dependencies
	uint8 m_uSubUVColumns = 1;
	uint8 m_uSubUVRows = 1;
	uint8 m_uTrailSamples = 0;               // TRAIL: the samples drawn (2 … the trail module's count)
	uint8 m_auReserved[2] = {};
	uint32 m_uRenderFlags = 0;               // the render block's flags word (EffectFormat.md "Module parameter blocks", render flags)
	Zenith_Vector3 m_xAxis{ 0.0f, 1.0f, 0.0f };   // emitter space, unit: the axis of AXIS facing and the normal of WORLD facing
	float m_fSoftDistance = 0.0f;             // 0 disables soft particles
	float m_fSortBias = 0.0f;                 // metres added to the emitter's rank depth
	uint64 m_ulShadingTypeID = 0;            // 0: the engine type ParticleShading_Standard; else a code-authored type ("Slang interfaces …")
	uint32 m_uRenderConstantsOffset = 0;     // into the effect constants image
	uint32 m_uRenderConstantsBytes = 0;
};

/// @contract value Flux_ParticleEmitterDesc
/// Ownership: Owned by its Zenith_ParticleEffectAsset; borrowed by reference.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: Valid while a handle to the asset is held.
/// Units: Seconds, metres, particles.
/// Errors: None; every field was validated by the reader.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleEmitterDesc
{
	Zenith_StringView m_strName;                     // into the asset's string table
	Flux_ParticleTarget m_eTarget = FLUX_PARTICLE_TARGET_GPU;
	Flux_ParticleSpace m_eSpace = FLUX_PARTICLE_SPACE_WORLD;
	Flux_ParticleCullBehaviour m_eCull = FLUX_PARTICLE_CULL_KILL;
	uint8 m_uIndex = 0;                             // the emitter's index in its effect, 0 … 15
	uint32 m_uCapacity = 0;                         // slots; a multiple of uFLUX_PARTICLE_CAPACITY_GRANULE
	uint32 m_uEventCapacity = 0;                    // event-ring records when an edge has this emitter as its source; else 0
	uint32 m_uTrailSamples = 0;                     // 0, or 2 … 8 with the trail module
	float m_fMaxLifetime = 0.0f;                     // the lifetime module's maximum (cook-computed)
	float m_fDuration = 0.0f;                        // the emitter's spawn timeline; 0 = the effect's
	float m_fStartDelay = 0.0f;                      // seconds of the timeline before the first spawn (record offset 32)
	uint32 m_uSeedOffset = 0;                       // mixed into the emitter seed (record offset 200; the index by default)
	Zenith_AABB m_xLocalBounds;                      // emitter-frame part B_e, fixed or baked; an event target's spawn-relative
	                                                 // box S_t (bounds flag bit 1) (EffectFormat.md "Emitter record section")
	Zenith_AABB m_xWorldDisplacement;                // world-term displacement box D, cook-computed and, for an event target,
	                                                 // composed over its chain (Authoring.md "Bounds baking")
	float m_fInheritVelocityGainSeconds = 0.0f;      // J: the inherited-velocity box is ±J × v_max (0: none)
	float m_fWindGain = 0.0f;                        // G, metres per m/s: the render side's wind cube is G × the wind bound
	float m_fBoundsHistorySeconds = 0.0f;            // H: the effect-time span the runtime bounds buckets cover (≥ m_fMaxLifetime)
	uint32 m_uBoundsFlags = 0;                      // bit 0 compose D as a rotation-invariant cube; bit 1 event target (local
	                                                 // bounds are S_t, composed from the chain root); bit 2 estimate class
	uint8 m_uBoundsRootEmitter = 0;                 // the chain root whose B_e an event target's bounds start from; else m_uIndex
	Flux_ParticleSpawnDesc m_xSpawn;
	Flux_ParticleRenderDesc m_xRender;
	uint32 m_uModuleMask = 0;                       // bit i: update module i is present (i indexes m_auUpdateModules)
	uint16 m_auUpdateModules[16] = {};              // Flux_ParticleModuleID of each present update module, in the fixed order
	uint32 m_uFirstModule = 0;                      // into the effect's module records
	uint32 m_uModuleCount = 0;
	uint32 m_uConstantsOffset = 0;                  // the emitter's block in the effect constants image (16-byte aligned)
	uint32 m_uConstantsBytes = 0;                   // ≤ uFLUX_PARTICLE_MAX_EMITTER_CONSTANTS
	uint64 m_ulUpdateTypeID = 0;                    // 0: none; else the code-authored update type (GPU only)
	uint32 m_uLightModule = 0xFFFFFFFFu;            // EFFECT_LIGHT module index, or none
	uint32 m_uDecalModule = 0xFFFFFFFFu;            // EFFECT_DECAL module index, or none
};
```

### Zenith_ParticleEffectAsset

```cpp
/// @contract value Flux_ParticleParameterDesc
/// Ownership: Plain value of the effect's user-parameter table.
/// Thread affinity: IMMUTABLE. Members non-const: the loader default-constructs the asset's record array and decodes
///   each record into it from the cooked bytes.
/// Invalidation: Valid while a handle to the asset is held (m_strName points into the asset's string table).
/// Units: The effect's.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleParameterDesc
{
	Zenith_StringView m_strName;
	uint8 m_uFirstSlot = 0;          // the first of the eight float slots it occupies
	uint8 m_uSlotCount = 1;          // 1 float, 2, 3, or 4 (colour)
	uint8 m_auReserved[2] = {};
	float m_afDefault[4] = {};
};

/// @contract type Zenith_ParticleEffectAsset
/// Ownership: Created by the VFX effect kind loader (Flux_CreateParticleEffectKindLoader) when the asset system loads a .zvfx;
///   shared through Zenith_AssetHandle<Zenith_ParticleEffectAsset>; owns its decoded tables and the CPU copy of the effect
///   constants image in one general-heap block; destroyed through the loader's Unload. It owns no GPU resource: the particle
///   system creates the effect's constants buffer on first use (Zenith2/Docs/Design/VFX/Simulation.md "Emitter pool ranges").
/// Thread affinity: IMMUTABLE after activation: every member is a const read callable from any thread.
/// Invalidation: Pointers, spans and string views it returns are valid while a handle to the asset is held.
/// Units: Seconds, metres.
/// Errors: None; construction failures are the loader's Fail codes (Flux_ParticleError).
/// Version: since Z2 Stage 5. Cooked data re-cooks on any format change; the document migrates.
/// @endcontract
class Zenith_ParticleEffectAsset
{
public:
	static constexpr Zenith_AssetKind eASSET_KIND = ASSET_KIND_PARTICLE_EFFECT;
	Zenith_PersistentID GetPersistentID() const;
	uint64 GetContentHash() const;                        // the cooked header's content hash, low word
	uint32 GetEmitterCount() const;
	const Flux_ParticleEmitterDesc& GetEmitter(uint32 uIndex) const;   // ZENITH_ASSERT uIndex < count
	float GetDuration() const;                             // seconds; 0 = endless
	bool IsLooping() const;
	float GetLingerSeconds() const;                        // the longest particle lifetime of any emitter
	float GetPriority() const;
	Zenith_AABB GetLocalBounds() const;                    // the union of every emitter's local bounds
	Zenith_Span<const Flux_ParticleLODLevel> GetLODLevels() const;
	float GetCullScreenSize() const;
	float GetMaxDistance() const;                          // metres; 0 = unlimited
	float GetLODHysteresis() const;                        // fraction, 0 … 0.5
	Zenith_Span<const Flux_ParticleParameterDesc> GetParameters() const;
	Zenith_Result<uint32, Flux_ParticleError> FindParameter(Zenith_StringView strName) const;   // first slot; LIMIT when absent
	Zenith_Span<const Flux_ParticleEventEdge> GetEventEdges() const;
	Zenith_Span<const Flux_ParticleBurst> GetBursts() const;
	Zenith_ByteSpan GetConstantsImage() const;             // the GPU constants image, bit-identical to the GPU copy
	Zenith_Span<const Zenith_AnyAssetHandle> GetDependencies() const;   // textures then meshes, in cooked order
	uint32 GetTextureCount() const;
	uint32 GetMeshCount() const;
	uint64 GetResidentBytes() const;
private:
	friend class Flux_ParticleEffectReader;     // Internal/Particles/Flux_ParticleEffectReader.h
	Zenith_ParticleEffectAsset() = default;
};
```

### Effect kind loader

```cpp
/// @contract functions Flux_ParticleEffectLoading
/// Ownership: Flux_CreateParticleEffectKindLoader returns a loader the caller owns (the VFX module instance) and registers
///   with the asset registry; the caller keeps it alive until every particle-effect asset is unloaded and unregisters it
///   before destroying it (Zenith2/Docs/Design/ASSET/RuntimeAPI.md "Zenith_AssetKindLoader").
/// Thread affinity: ANY_THREAD for the factory; the loader's members follow Zenith_AssetKindLoader's block.
/// Invalidation: None.
/// Units: Bytes.
/// Errors: The factory returns FLUX_PARTICLE_ERROR_OUT_OF_MEMORY; the loader reports a corrupt, mismatched or over-limit file
///   through Zenith_AssetLoadContext::Fail with the Flux_ParticleError value as the loader code, never an assert.
/// Version: since Z2 Stage 5. The kind is CPU-resident (Zenith2/Docs/Design/VFX/EffectFormat.md "Cooked effect format").
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_AssetKindLoader>, Flux_ParticleError> Flux_CreateParticleEffectKindLoader(
	Zenith_Heap& xHeap);

/// @contract functions Flux_ParticleEffectParsing
/// Ownership: Flux_ParseParticleEffect returns an effect object the caller owns, built from a whole cooked file in memory,
///   for tools and tests (the effect cook's bounds bake, the reference-simulation tests); its dependency handles are null.
/// Thread affinity: ANY_THREAD; the result is IMMUTABLE.
/// Invalidation: None; the object copies everything it keeps out of xFileBytes.
/// Units: Bytes.
/// Errors: The reader's codes of Zenith2/Docs/Design/VFX/EffectFormat.md "Reader validation" (FLUX_PARTICLE_ERROR_CORRUPT,
///   _VERSION, _LIMIT), and FLUX_PARTICLE_ERROR_OUT_OF_MEMORY; never an assert on file content.
/// Version: since Z2 Stage 5.
/// @endcontract
Zenith_Result<Zenith_Owned<Zenith_ParticleEffectAsset>, Flux_ParticleError> Flux_ParseParticleEffect(
	Zenith_ByteSpan xFileBytes, Zenith_Heap& xHeap);
```

The loader: `BeginLoad` requests every section of the file in one round with empty destinations (ZenithAssets decodes into its buffers); `OnSectionsLoaded` runs `Flux_ParticleEffectReader::Read` over the sections and the load's hard dependencies (the effect's textures and meshes, each ACTIVE or FAILED with its fallback), which allocates one general-heap block of `GetResidentBytes()` holding the decoded tables, the string table and the constants image, then calls `SetResidentBytes(MEMORY_SUBSYSTEM_RENDERING, bytes, 0)` and `Activate(pAsset)`; a reader failure is `Fail(code)` with the block released. `Cancel` releases nothing that was not yet allocated and ends in `AcknowledgeCancel`; `Unload` frees the block and ends in `AcknowledgeUnload`; `DropCpuCopies` returns 0, because the CPU copy is the asset's only copy (the kind is not GPU-resident). The loader works identically in a headless run, where the particle system does not exist and the simulation side alone reads the asset.

## Flux_Particles header

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Particles.h
#pragma once
#include "ZenithCore/Zenith_Types.h"
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Span.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Bounds.h"
#include "ZenithCore/Zenith_PersistentID.h"
#include "ZenithFlux/Flux_ParticleEffect.h"
#include "ZenithShadersGenerated/Flux/Shaders/Flux_SharedTypes.generated.h"   // Flux_ParticleState

class Zenith_Config;
struct Flux_EnvironmentBlocks;   // ENV's (Zenith2/Engine/ZenithFlux/Flux_Environment.h), read by Flux_ParticleWindBound
```

### Instance keys

```cpp
/// @contract value Flux_ParticleInstanceKey
/// Ownership: Plain value naming one effect instance of one world for its lifetime.
/// Thread affinity: IMMUTABLE. Members non-const: a key is copy-assigned wherever it is stored (records, containers,
///   sort keys).
/// Invalidation: None; a restarted attached effect gets a new key (its generation is an input). Two entities of one world
///   never share an attached key, whether authored or spawned at run time (distinct simulation IDs), barring a 63-bit hash
///   collision.
/// Units: None. Bit 63 clear: an attached instance; bit 63 set: a detached instance.
/// Errors: None.
/// Version: since Z2 Stage 5. Never serialized, so a D-SEL-HASH (open, Stage 1) change moves every key at once and breaks nothing.
/// @endcontract
struct Flux_ParticleInstanceKey
{
	uint64 m_ulValue = 0;
	friend constexpr bool operator==(Flux_ParticleInstanceKey, Flux_ParticleInstanceKey) = default;
	friend constexpr auto operator<=>(Flux_ParticleInstanceKey, Flux_ParticleInstanceKey) = default;
};

/// @contract functions Flux_ParticleInstanceKeys
/// Ownership: Pure functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
// Zenith_Hash64 (Zenith2/Docs/Design/CORE/Maths.md "Zenith_Hash64") of the 8 bytes of ulSimulationID (LE) then uGeneration
// (u32 LE), seed uWorldTag, with bit 63 cleared. ulSimulationID is the entity's stable simulation ID
// (Zenith2/Docs/Architecture/ECSModel.md "Entity identity"): unique within the world session, never reused, assigned to
// runtime-spawned entities as to authored ones, serialized in saves and replays. The persistent ID is never an input: it is
// null for every runtime-spawned entity (Zenith_SpawnDesc of Zenith2/Docs/Design/ECS/WorldAPI.md).
Flux_ParticleInstanceKey Flux_MakeAttachedParticleKey(uint64 ulSimulationID, uint16 uWorldTag,
	uint32 uGeneration);
// Zenith_Hash64 of the 8 bytes of ulTriggerSequence (LE), seed uWorldTag ^ 0x56465800, with bit 63 set.
Flux_ParticleInstanceKey Flux_MakeDetachedParticleKey(uint16 uWorldTag, uint64 ulTriggerSequence);
// The default instance seed of an attached effect (component Seed 0): the low word XOR the high word of Zenith_Hash64 of the
// 8 bytes of ulSimulationID (LE) then uGeneration (u32 LE), seed 0x56465300. It omits the world tag, so a saved and loaded
// world (whose tag may differ) shows the same particles for the same generation.
uint32 Flux_MakeAttachedParticleSeed(uint64 ulSimulationID, uint32 uGeneration);
// The 32-bit instance seed of a detached effect whose payload seed is 0: the low word of the key's value XOR its high word.
constexpr uint32 Flux_DeriveParticleSeed(Flux_ParticleInstanceKey xKey)
{
	return static_cast<uint32>(xKey.m_ulValue) ^ static_cast<uint32>(xKey.m_ulValue >> 32);
}
// The emitter seed: Flux_ParticleHash(uInstanceSeed ^ (uSeedOffset × 0x9E3779B9)), uSeedOffset the emitter's cooked seed
// offset (Flux_ParticleEmitterDesc::m_uSeedOffset; the emitter index unless the document sets seedOffset).
constexpr uint32 Flux_DeriveEmitterSeed(uint32 uInstanceSeed, uint32 uSeedOffset);
```

### Flux_RenderEmitterCommand

The element type of `Flux_RenderWorldSection::m_axEmitterCommands` (`Zenith2/Docs/Rendering.md → "Purpose and scope"`), appended through `Flux_RenderWorldWriter::AddEmitterCommand` by `VFX.Extract` alone.

```cpp
/// @contract enum Flux_EmitterCommandOp
/// Ownership: Plain enumeration.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; the particle system ignores any other value and counts Flux.Particles.CommandsRejected.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_EmitterCommandOp : uint8
{
	FLUX_EMITTER_COMMAND_OP_UPDATE = 1,    // the emitter's full per-frame state; an unknown key creates the emitter instance
	FLUX_EMITTER_COMMAND_OP_RELEASE = 2,   // no further spawns; the render side destroys the emitter when its last particle dies
	FLUX_EMITTER_COMMAND_OP_DESTROY = 3,   // remove now (world detach, effect change, preview seek reset)
};

/// @contract enum Flux_EmitterCommandFlags
/// Ownership: Plain bit enumeration of Flux_RenderEmitterCommand::m_uFlags.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; unknown bits are cleared by the writer.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_EmitterCommandFlags : uint8
{
	FLUX_EMITTER_FLAG_ACTIVE = 1u << 0,       // simulate this frame (LOD-admitted, not paused, not culled)
	FLUX_EMITTER_FLAG_VISIBLE = 1u << 1,      // draw this frame in the views whose frustum the bounds intersect
	FLUX_EMITTER_FLAG_RESET = 1u << 2,        // clear every particle before this frame's spawns (a kill cull, a restart, a seek)
	FLUX_EMITTER_FLAG_PREWARM = 1u << 3,      // this frame's rate spawns are the prewarm set (Simulation.md "Prewarm")
	FLUX_EMITTER_FLAG_TELEPORTED = 1u << 4,   // the previous transform equals the current one; distance spawns reset
	FLUX_EMITTER_FLAG_PAUSED = 1u << 5,       // presentation time stopped: no simulation, particles drawn as they are
	FLUX_EMITTER_FLAG_CATCH_UP = 1u << 6,     // a preview catch-up step: simulated, never drawn; the emitter's next command follows in the
	                                          // same frame and is its next step (steps run in arrival order, Simulation.md "Frame building")
	FLUX_EMITTER_FLAG_SEEK = 1u << 7,         // a command of a preview seek in progress, every one of its steps (the last, unflagged for
	                                          // CATCH_UP, included): kept as its own step and never merged by a deferred or failed frame
	                                          // (Simulation.md "Deferred and failed frames")
};

/// @contract value Flux_RenderEmitterCommand
/// Ownership: Plain value in the sealed render world's slot arena; m_axCpuParticles points into the same arena (the writer
///   copies the span there and re-points it, as for ENV's records, Zenith2/Docs/Rendering.md "Render-world record types").
/// Thread affinity: IMMUTABLE after Render.Seal. Members non-const: the extractor writes it into the frame's record
///   arrays before Render.Seal.
/// Invalidation: The span is invalid after the render subgraph that read the slot completes.
/// Units: Metres, seconds, particles; transforms map emitter space to world space as the top three rows of a column-vector
///   matrix (Zenith2/Docs/Design/CORE/Maths.md "Coordinate systems").
/// Errors: The particle system validates each command: a non-finite transform, an emitter index outside the effect, a span
///   on a GPU emitter, or a span longer than the emitter's capacity drops the command and counts
///   Flux.Particles.CommandsRejected; the emitter keeps its previous state.
/// Version: since Z2 Stage 5. Never serialized. 256 bytes.
/// @endcontract
struct Flux_RenderEmitterCommand
{
	Flux_ParticleInstanceKey m_xInstance;            //   0  8
	Flux_EmitterCommandOp m_eOp;                     //   8  1
	uint8 m_uEmitterIndex;                          //   9  1
	uint8 m_uLODLevel;                              //  10  1  0 … 3
	uint8 m_uFlags;                                 //  11  1  Flux_EmitterCommandFlags
	uint32 m_uSeed;                                 //  12  4  the emitter seed (Flux_DeriveEmitterSeed)
	Zenith_PersistentID m_xEffect;                   //  16 16  the effect asset; resolved and referenced by the render side
	Zenith_Vector4 m_axEmitterToWorld[3];            //  32 48  presentation-interpolated, this frame
	Zenith_Vector4 m_axPrevEmitterToWorld[3];        //  80 48  last frame's (equal to the current with TELEPORTED)
	Zenith_Vector3 m_xEmitterVelocity;               // 128 12  world m/s, for inherited velocity
	float m_fDeltaTime;                              // 140  4  the simulation step, ≤ fFLUX_PARTICLE_MAX_STEP_SECONDS
	uint32 m_uSpawnOrdinalBase;                     // 144  4  the first new particle's spawn ordinal
	uint32 m_uSpawnCount;                           // 148  4  rate and distance spawns this frame
	uint32 m_uBurstCount;                           // 152  4  burst spawns this frame (ordinals after the rate spawns)
	float m_fBurstAgeOffset;                         // 156  4  the burst particles' age at the end of the frame
	float m_fSpawnAgeFirst;                          // 160  4  the first rate spawn's age at the end of the frame
	float m_fSpawnAgeStep;                           // 164  4  the age difference between consecutive rate spawns
	float m_fEffectTime;                             // 168  4  seconds since the start (wrapped by the loop)
	float m_fSignificance;                           // 172  4  the GPU admission key ("Budgets and admission")
	float m_afUserParameters[8];                     // 176 32
	Zenith_AABB m_xWorldBounds;                      // 208 24  the CPU-side conservative bounds this frame
	uint32 m_uLayerMask;                            // 232  4  render layers bits 0–23, lighting channels bits 24–31
	uint32 m_uPrewarmCount;                         // 236  4  with PREWARM: the number of prewarm spawns (m_uSpawnCount carries them)
	Zenith_Span<const Flux_ParticleState> m_axCpuParticles;   // 240 16  CPU target: the live particles in spawn order; else empty
};
static_assert(sizeof(Flux_RenderEmitterCommand) == 256);
```

### Flux_RenderParticleCollider

```cpp
/// @contract value Flux_RenderParticleCollider
/// Ownership: Plain value in the sealed render world's section array m_axParticleColliders, written by VFX.Extract through
///   Flux_RenderWorldWriter::AddParticleCollider (Zenith2/Docs/Rendering.md "The render world hand-off", cross-doc item X240).
/// Thread affinity: IMMUTABLE after Render.Seal. Members non-const: the extractor writes it into the frame's record
///   arrays before Render.Seal.
/// Invalidation: None.
/// Units: Metres; the rows map world space to collider space (the inverse of the entity's rigid transform, scale removed).
/// Errors: The particle system drops a record with a non-finite row or extent, or a shape outside 1 … 4, and counts
///   Flux.Particles.CollidersRejected.
/// Version: since Z2 Stage 5. Never serialized. 64 bytes, field for field the GPU struct Flux_ParticleCollider.
/// @endcontract
struct Flux_RenderParticleCollider
{
	Zenith_Vector4 m_axWorldToCollider[3];   //  0 48
	Zenith_Vector3 m_xHalfExtents;           // 48 12  box half extents; capsule half length in y; sphere and capsule radius in x
	uint32 m_uShapeAndMask;                 // 60  4  bits 0–2 the shape (1 plane, 2 sphere, 3 box, 4 capsule), bits 8–31 the low 24 bits of the mask
};
static_assert(sizeof(Flux_RenderParticleCollider) == 64);
```

### Flux_ParticleSystem

```cpp
/// @contract enum Flux_ParticleSortAlgorithm
/// Ownership: Plain enumeration (vfx.sort.algorithm).
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
enum Flux_ParticleSortAlgorithm : uint8
{
	FLUX_PARTICLE_SORT_ALGORITHM_ONESWEEP = 1,          // single-pass partitioned scatter with decoupled look-back
	FLUX_PARTICLE_SORT_ALGORITHM_REDUCE_THEN_SCAN = 2,  // three dispatches per digit pass, no inter-group wait
};

/// @contract value Flux_ParticleSystemDesc
/// Ownership: Plain value passed to Flux_Renderer::EnableParticles; filled from the vfx.* keys by Flux_ReadParticleSystemDesc.
/// Thread affinity: IMMUTABLE. Members non-const: callers default-construct it and set the fields they need before
///   passing it.
/// Invalidation: None.
/// Units: Particles, emitters, records, samples.
/// Errors: EnableParticles returns FLUX_PARTICLE_ERROR_INVALID_DESC for a zero value, a maximum below its initial value, or
///   m_uSortedEmittersPerFrame above uFLUX_PARTICLE_MAX_SORTED_EMITTERS.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleSystemDesc
{
	uint32 m_uGpuActiveCapacity = 0;          // vfx.gpuActiveCapacity: Σ admitted work in one frame (Simulation.md "GPU admission per frame")
	uint32 m_uGpuEmittersPerFrame = 0;        // vfx.gpuEmittersPerFrame
	uint32 m_uSortedParticlesPerFrame = 0;    // vfx.sortedParticlesPerFrame
	uint32 m_uSortedEmittersPerFrame = 0;     // vfx.sortedEmittersPerFrame (≤ 1,024)
	uint32 m_uPoolInitialParticles = 0;       // vfx.pool.initialParticles
	uint32 m_uPoolMaxParticles = 0;           // vfx.pool.maxParticles (from Z2-B-VFX-001)
	uint32 m_uPoolMaxTrailSamples = 0;        // vfx.pool.maxTrailSamples
	uint32 m_uPoolMaxEventRecords = 0;        // vfx.pool.maxEventRecords
	uint32 m_uMaxInstances = 0;               // vfx.maxRenderInstances: the render side's instance table capacity
	Flux_ParticleLighting m_eMaxLighting = FLUX_PARTICLE_LIGHTING_PIXEL;   // vfx.maxLightingMode: the lighting-mode clamp
	Flux_ParticleSortAlgorithm m_eSortAlgorithm = FLUX_PARTICLE_SORT_ALGORITHM_ONESWEEP;   // vfx.sort.algorithm
	bool m_bValidateBounds = false;            // vfx.debug.validateBounds (development builds; refused true in Shipping)
};

/// @contract functions Flux_ParticleSystemDescReading
/// Ownership: Pure read of the configuration store.
/// Thread affinity: MAIN_THREAD (the module's Start, and Flux_Renderer::ApplyConfigChanges in prologue step 3).
/// Invalidation: None.
/// Units: As Flux_ParticleSystemDesc.
/// Errors: None; a key outside its domain was already replaced by its default when the store resolved it
///   (Zenith2/Docs/Design/VFX.md "Configuration keys").
/// Version: since Z2 Stage 5.
/// @endcontract
// Reads the render-side vfx.* keys of Zenith2/Docs/Design/VFX.md "Configuration keys" (the four admission budgets, the pool
// and instance-table sizes, vfx.maxLightingMode, vfx.sort.algorithm, vfx.debug.validateBounds) into one desc.
Flux_ParticleSystemDesc Flux_ReadParticleSystemDesc(const Zenith_Config& xConfig);

/// @contract value Flux_ParticleStats
/// Ownership: Plain value snapshot published once per built frame (copied out by Flux_ParticleSystem::GetStats).
/// Thread affinity: IMMUTABLE. Members non-const: its producer resets it and accumulates its counters in place.
/// Invalidation: None.
/// Units: Particles, emitters, bytes.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleStats
{
	uint32 m_uInstances = 0;                  // render-side effect instances
	uint32 m_uGpuEmitters = 0;                // GPU emitters with pool ranges
	uint32 m_uGpuEmittersSimulated = 0;       // admitted this frame
	uint32 m_uGpuEmittersBudgetCulled = 0;    // refused by the frame budget this frame
	uint64 m_ulGpuActiveWork = 0;             // Σ admitted work: simulate threads plus event-consumer threads
	uint32 m_uCpuEmitters = 0;
	uint32 m_uCpuParticles = 0;               // uploaded this frame
	uint32 m_uSortedParticles = 0;            // keys sorted this frame
	uint32 m_uSortedEmitters = 0;
	uint32 m_uDrawCommands = 0;               // over every view
	uint32 m_uGpuActiveCapacity = 0;          // the frame's admission budget: m_uGpuActiveCapacity × the pressure factor
	uint64 m_ulPoolBytesUsed = 0;
	uint64 m_ulPoolBytesCapacity = 0;
	uint64 m_ulConstantsBytes = 0;            // Σ effect constants buffers
};

/// @contract value Flux_ParticleInstanceStats
/// Ownership: Plain value filled by Flux_ParticleSystem::GetInstanceStats.
/// Thread affinity: IMMUTABLE. Members non-const: its producer writes it field by field into the caller's output.
/// Invalidation: None.
/// Units: Particles; metres.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleInstanceStats
{
	uint32 m_uEmitterCount = 0;
	uint32 m_auCapacity[16] = {};
	uint32 m_auSpawnedThisFrame[16] = {};
	uint8 m_auLODLevel[16] = {};
	bool m_abSimulated[16] = {};
	bool m_abBoundsExceeded[16] = {};          // GPU-measured bounds outside the CPU bounds (development builds)
	Zenith_AABB m_axMeasuredBounds[16];        // the newest GPU-measured world bounds (development builds; empty otherwise)
	uint32 m_auLiveUpperBound[16] = {};       // CPU-known: min(capacity, ordinals in the possibly-live span); exact for CPU emitters
	uint32 m_auMeasuredLive[16] = {};         // GPU emitters: the validation readback's live count; 0xFFFFFFFF when unmeasured
	uint64 m_ulMeasuredFrameIndex = 0;        // the frame the measurement describes (one to F frames behind, F the frames in flight)
	bool m_bSeekInterrupted = false;           // a preview seek's kept steps overflowed; true until a RESET command (Simulation.md
	                                           // "Deferred and failed frames"); the instance draws nothing meanwhile
};

/// @contract type Flux_ParticleSystem
/// Ownership: Owned by Flux_Renderer, created by Flux_Renderer::EnableParticles at startup step 15 (from the VFX module's
///   Start) and destroyed in the renderer's shutdown before the GPU scene whose pool it created; owns the instance table, the
///   particle pool handle and its ranges, every effect constants buffer, the quad index buffer, the deferred-command list
///   and the bounds readbacks.
/// Thread affinity: MAIN_THREAD for GetStats, GetInstanceStats and SetDebugFlags (the frame prologue, the editor, the
///   profiler panel provider). Every mutation runs on the Flux.BuildGraph node through the internal members of "Internal
///   Flux particle declarations". The stats are a three-slot ring indexed by frame index mod 3: the build of frame k writes
///   slot k mod 3, and a main-thread reader during frame k copies slot (k − 2) mod 3, the newest frame whose render subgraph
///   completed before frame k began (the render side runs one frame behind, Z2-D-050, and render subgraphs run in order),
///   so no slot is read while it is written and the frame graph's completion edges order the write before the read; no
///   atomic or lock is involved.
/// Invalidation: None.
/// Units: As Flux_ParticleStats.
/// Errors: Flux_ParticleError; per-frame failures never fail the frame (each is counted, "Render-side failure behaviour" of
///   Zenith2/Docs/Design/VFX/Rendering.md).
/// Reentrancy: NOT_APPLICABLE. It invokes no callbacks.
/// Version: since Z2 Stage 5.
/// @endcontract
class Flux_ParticleSystem
{
public:
	Flux_ParticleStats GetStats() const;   // a copy of the published snapshot
	bool GetInstanceStats(Flux_ParticleInstanceKey xKey, Flux_ParticleInstanceStats& xOut) const;   // a copy; false: unknown key
	void SetDebugFlags(uint32 uFlags);          // Flux_ParticleDebugFlags (Zenith2/Docs/Design/VFX.md "Debug views")
	~Flux_ParticleSystem();
private:
	friend class Flux_ParticleSystemAccess;               // Internal/Particles/Flux_ParticleInternal.h
	Flux_ParticleSystem() = default;
};
```

```cpp
/// @contract enum Flux_ParticleDebugFlags
/// Ownership: Plain bit enumeration passed to Flux_ParticleSystem::SetDebugFlags by the debug UI.
/// Thread affinity: IMMUTABLE.
/// Invalidation: None.
/// Units: None.
/// Errors: None; unknown bits are ignored, and every bit is ignored in Shipping builds.
/// Version: since Z2 Stage 5. The views are Zenith2/Docs/Design/VFX.md "Debug views".
/// @endcontract
enum Flux_ParticleDebugFlags : uint32
{
	FLUX_PARTICLE_DEBUG_BOUNDS = 1u << 0,      // every drawn emitter's CPU bounds; with validation, the GPU-measured bounds too
	FLUX_PARTICLE_DEBUG_OVERDRAW = 1u << 1,    // every particle draws additive grey 1/32: an overdraw heat map
	FLUX_PARTICLE_DEBUG_LOD = 1u << 2,         // tint by LOD level: 0 white, 1 green, 2 yellow, 3 red
	FLUX_PARTICLE_DEBUG_SORT_RANK = 1u << 3,   // tint partition-0 emitters by rank, blue (far) to red (near)
	FLUX_PARTICLE_DEBUG_BUDGET = 1u << 4,      // tint emitters refused by the frame budget (drawn frozen) magenta
};
```

`Flux_Renderer` gains three members and one owned member (`Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"`; cross-area row VX2), the counterpart of ENV's `EnableEnvironment` and UI's `EnableUI`. Under the ENABLE-FEATURES ruling, extended to particles, RPROD declares all of them in its renderer API ticket Z2T-RPROD-014 (and the design text of that heading), and VFX defines the three bodies in RPROD's `Zenith2/Engine/ZenithFlux/Internal/Renderer/Flux_Renderer.cpp`, in its ZenithFlux API slice (slice 2a of `Zenith2/Docs/Design/VFX.md → "Stage 5 slices"`), so the module start that calls them links from the slice that lands it:

```cpp
// Zenith2/Engine/ZenithFlux/Flux_Renderer.h (additions, declared by Z2T-RPROD-014; Zenith2/Docs/Design/RPROD/Renderer.md "Flux_Renderer header")
class Flux_ParticleSystem; struct Flux_ParticleSystemDesc; enum Flux_ParticleError : uint8;   // forward declarations
// MAIN_THREAD at startup step 15, before the first frame. A second call returns FLUX_PARTICLE_ERROR_ALREADY_ENABLED.
Zenith_Result<Flux_ParticleSystem*, Flux_ParticleError> EnableParticles(const Flux_ParticleSystemDesc& xDesc);
// MAIN_THREAD at a render-idle point (the VFX module's shutdown, shutdown step 6; the unwinding of a failed module start):
// releases every effect handle of the instance table, frees every pool range, effect constants buffer and the quad index
// buffer through deferred deletion, destroys the particle system; GetParticleSystem() is null afterwards. No-op when null.
void DisableParticles();
Flux_ParticleSystem* GetParticleSystem();   // null until EnableParticles succeeds, and after DisableParticles
// private: Zenith_Owned<Flux_ParticleSystem> m_xParticles;          // destroyed in the renderer's shutdown before the GPU scene
```
RPROD's renderer design (`Zenith2/Docs/Design/RPROD/Renderer.md → "Flux_Renderer header"`) and Z2T-RPROD-014 carry these declarations, with the same signatures, and leave the two `Flux_FrameShared` members to Rendering (cross-doc item X240 (5)).

`Flux_FrameShared` (Rendering's struct, declared in code in `Zenith2/Engine/ZenithFlux/Flux_ViewContext.h` by RPROD's view-surface ticket Z2T-RPROD-011) has `const Flux_ParticleFrame* m_pxParticles = nullptr` (null when particles are not enabled or the frame has no particle work) and `Flux_RGBuffer m_xParticleSorted`, the core text of cross-doc item X240 (5) in `Zenith2/Docs/Rendering.md → "Shared producers"`. VFX's slice 2a adds the two members to `Flux_ViewContext.h` in its own diff, after Z2T-RPROD-011 has created the header, unless Z2T-RPROD-011 already copied them from the applied core text; their default initialisers keep `m_pxParticles` null until VFX slice 11's `BuildGraph` call sets it to the frame `Flux_ParticleBuildFrame` returned when that frame has work.

The bodies VFX writes, all three in slice 2a and unchanged afterwards: `EnableParticles` returns `FLUX_PARTICLE_ERROR_ALREADY_ENABLED` when `m_xParticles` is set, else calls `Flux_CreateParticleSystem(xDesc, xDeps)` ("Flux_ParticleSystem internals") with the renderer's heap, GPU scene, persistent resources and readback pool, moves the result into `m_xParticles` and returns the borrowed pointer (an error leaves `m_xParticles` null); `DisableParticles`, when `m_xParticles` is set, calls `Flux_ReleaseParticleSystem(*m_xParticles)` (the releases above, through deferred deletion) and then resets `m_xParticles`, which destroys the system; `GetParticleSystem` returns `m_xParticles`'s pointer. Slice 2a's `Flux_CreateParticleSystem` performs the desc validation, the second-call refusal is the body's, and step 1 (`Allocate`) of the construction table below; slice 11 adds steps 2 … 6 to the same function, each with its fault test, so neither the bodies nor the module start change when the full system lands.

`Flux_Renderer::ApplyConfigChanges` (RPROD's, MAIN_THREAD in prologue step 3, `Zenith2/Docs/Design/RPROD/Renderer.md → "Quality tier selection"`) gains one action, also row VX2: when the particle system exists and a changed key starts with `vfx.` or the active tier changed, it calls `Flux_ParticleApplyConfigChanges(xSystem, Flux_ReadParticleSystemDesc(xConfig))`. The particle system takes the non-restart members (the four admission budgets, `m_eMaxLighting`, `m_eSortAlgorithm`) as its pending budgets and ignores the restart members; `Flux_ParticleApplyRenderWorld` adopts the pending budgets at step 4 of the next `BuildGraph`, the same hand-off at which the renderer's new `Flux_QualitySettings` take effect (that heading's step 3). The admission of that frame, the lighting-mode clamp of its draw runs and the sort dispatch use the new values; the memory-pressure factor of "Memory pressure response" (`Zenith2/Docs/Design/VFX/Simulation.md`) multiplies the new budgets. `Test_ParticleSystem_TierChangeAppliesBudgets` switches a WARP runtime from the desktop default tier to the S24 default tier and asserts that the next built frame admits at most the S24 `vfx.gpuActiveCapacity`, draws a pixel-lit emitter with the vertex-lit permutation, and that no restart member changed.

`EnableParticles` validates the desc (`FLUX_PARTICLE_ERROR_INVALID_DESC`), refuses a second call (`FLUX_PARTICLE_ERROR_ALREADY_ENABLED`), then constructs in this order; each step has its failure-injection test `Test_ParticleSystem_Create_FailsAt<Step>_Unwinds` ([Z2-N-CONTRACT-341]) and a failure undoes the earlier steps in reverse, leaving `GetParticleSystem()` null:

| # | Step | Fault point | Failure |
|---|---|---|---|
| 1 | `Allocate`: `Zenith_TryNew<Flux_ParticleSystem>` on the renderer's heap | `0x56461001` | `FLUX_PARTICLE_ERROR_OUT_OF_MEMORY` |
| 2 | `InstanceTable`: reserve the instance table (`m_uMaxInstances`) and the per-frame command scratch | `0x56461002` | `FLUX_PARTICLE_ERROR_OUT_OF_MEMORY` |
| 3 | `Pool`: create the particle pool with its three regions (state, trail samples, event records) at their initial sizes through RHI's pool allocation (`Zenith2/Docs/Design/RHI/Memory.md → "Pool allocation"`), and publish it as `Flux_FrameShared::m_xParticlePool` | `0x56461003` | `FLUX_PARTICLE_ERROR_OUT_OF_MEMORY`, `FLUX_PARTICLE_ERROR_DEVICE` |
| 4 | `QuadIndexBuffer`: the six-index persistent read-only buffer, uploaded through the renderer's upload path | `0x56461004` | `FLUX_PARTICLE_ERROR_OUT_OF_MEMORY`, `FLUX_PARTICLE_ERROR_DEVICE` |
| 5 | `BoundsReadback`: the development builds' validation readback ring (bounds and live counts, `Flux_ParticleValidationRecord`; no-op otherwise) | `0x56461005` | `FLUX_PARTICLE_ERROR_OUT_OF_MEMORY`, `FLUX_PARTICLE_ERROR_DEVICE` |
| 6 | `PendingList`: the deferred-command list sized for one frame's commands | `0x56461006` | `FLUX_PARTICLE_ERROR_OUT_OF_MEMORY` |

### Flux_ParticleStats

The counters of `Zenith2/Docs/Design/VFX.md → "Counters"` named `Flux.Particles.*` are published from the same snapshot; the editor's preview and inspector and VFX's profiler panel provider (`Zenith2/Docs/Design/VFX.md → "On-screen profiler rows"`) read `GetStats` on the main thread (`Zenith2/Docs/Design/VFX/Authoring.md → "Preview statistics"`). No runtime decision reads a GPU-written count: the CPU learns a GPU live count only through the development builds' validation readback, which feeds these statistics alone.

### Particle random functions

One integer hash, identical bit for bit in C++ and Slang (`Zenith2/Shaders/Particles/ParticleRandom.slang`), so CPU emitters, GPU emitters, the cook's bounds bake and the reference tests draw identical numbers:

```cpp
/// @contract functions Flux_ParticleRandom
/// Ownership: Pure constexpr functions.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: Unit results in [0, 1) with 24 bits of resolution.
/// Errors: None.
/// Version: since Z2 Stage 5. The constants and the composition are fixed; changing them changes every effect's look and
///   bumps uFLUX_PARTICLE_EFFECT_FORMAT_VERSION (the cooked bounds depend on them).
/// @endcontract
constexpr uint32 Flux_ParticleHash(uint32 uX)
{
	uX ^= uX >> 16; uX *= 0x7FEB352Du; uX ^= uX >> 15; uX *= 0x846CA68Bu; uX ^= uX >> 16;
	return uX;
}
constexpr uint32 Flux_ParticleRandomU32(uint32 uSeed, uint32 uOrdinal, uint32 uDraw)
{
	return Flux_ParticleHash(uSeed ^ Flux_ParticleHash(uOrdinal ^ Flux_ParticleHash(uDraw ^ 0x9E3779B9u)));
}
constexpr float Flux_ParticleRandomUnit(uint32 uSeed, uint32 uOrdinal, uint32 uDraw)
{
	return static_cast<float>(Flux_ParticleRandomU32(uSeed, uOrdinal, uDraw) >> 8) * 5.9604644775390625e-8f;   // × 2⁻²⁴, exact
}
```

The draw indices each module uses are fixed by `Zenith2/Docs/Design/VFX/Simulation.md → "Particle random numbers"`.

### Particle wind bound

```cpp
/// @contract functions Flux_ParticleWind
/// Ownership: Pure functions over a world's render-environment blocks.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: m/s for the bound; metres for the expansion.
/// Errors: None; a non-finite or negative input field is read as 0.
/// Version: since Z2 Stage 5.
/// @endcontract
// An upper bound of |Env_WindSample| at every position and time of the frame (Zenith2/Docs/Design/ENV/Foliage.md
// "Gusts and turbulence": the wind is dir × speed × (1 + g + turbulence) with g = gustStrength × G, G in [0, 1], and the
// turbulence term in [−0.5, 0.5] × m_fTurbulence; each local source adds at most its m_fStrength):
//   m_xWind.m_fSpeed × (1 + m_xWind.m_fGustStrength + 0.5 × m_xWind.m_fTurbulence)
//     + Σ_{i < m_uLocalWindSourceCount} m_axLocalWindSources[i].m_fStrength
// of xBlocks; 0 when xBlocks.m_bValid is false (no Environment module: no wind is sampled).
float Flux_ParticleWindBound(const Flux_EnvironmentBlocks& xBlocks);
// The half extent of the wind cube an emitter's bounds gain: fWindGain (Flux_ParticleEmitterDesc::m_fWindGain) × fWindBound.
constexpr float Flux_ParticleWindExpansion(float fWindGain, float fWindBound)
{
	return fWindGain * fWindBound;
}
```

The render side calls `Flux_ParticleWindBound` once per world per built frame on `Flux_RenderWorldSection::m_xEnvironment.m_xBlocks` (the world's render environment, `Zenith2/Docs/Rendering.md → "Render environment"`, written by `Render.ExtractEnvironment` from ENV's `Zenith.Environment.RenderBlocks`), and keeps, per GPU emitter with a non-zero `m_fWindGain`, a ring of nine buckets of `m_fBoundsHistorySeconds / 8` seconds of effect time (the current bucket plus the eight full buckets before it, a bucket dropped once its end is older than `τ − m_fBoundsHistorySeconds`, so the kept buckets always cover at least the last `m_fBoundsHistorySeconds`), each the largest bound of the frames it covers; the emitter's `W_e` is the largest kept bucket. `Test_VFXBounds_AttachedAndWorldSpace` calls both functions to compose the runtime box the reference simulation's windy fixture must stay inside.

## Internal Flux particle declarations

### Flux_ParticleFrame

```cpp
// Zenith2/Engine/ZenithFlux/Internal/Particles/Flux_ParticleInternal.h
#include "ZenithFlux/Flux_Particles.h"
#include "ZenithFlux/Internal/Upload/Flux_UploadRing.h"

/// @contract value Flux_ParticleDispatchBatch
/// Ownership: Plain value inside a Flux_ParticleFrame.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: None.
/// Units: Commands; thread groups of uFLUX_PARTICLE_THREADS_PER_GROUP.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleDispatchBatch
{
	Zenith_RHIShaderProgramID m_xProgram;     // the permutation's program (Rendering.md "PSO keys and permutations")
	uint32 m_uFirstCommand = 0;              // into the frame's Flux_EmitterCommand array (every command of one step)
	uint32 m_uCommandCount = 0;
	uint32 m_uGroupCount = 0;                // Σ ceil(threads / 64) over the batch's commands
	uint32 m_uStep = 0;                      // the step its commands belong to (0 … m_uStepCount − 1); sort-key batches: the last
};

/// @contract value Flux_ParticleStep
/// Ownership: Plain value inside a Flux_ParticleFrame; one per simulate step of the frame.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: As Flux_ParticleFrame.
/// Units: Commands and batches.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleStep
{
	uint32 m_uFirstCommand = 0;              // the step's contiguous range of the frame's Flux_EmitterCommand array
	uint32 m_uCommandCount = 0;
	uint32 m_uFirstEmitBatch = 0;            // into m_axEmitBatches; m_uEmitBatchCount 0: no emission in this step
	uint32 m_uEmitBatchCount = 0;
	uint32 m_uFirstEventBatch = 0;           // into m_axEventBatches
	uint32 m_uEventBatchCount = 0;
	uint32 m_uFirstSimulateBatch = 0;        // into m_axSimulateBatches
	uint32 m_uSimulateBatchCount = 0;
};

/// @contract value Flux_ParticleViewDraws
/// Ownership: Plain value inside a Flux_ParticleFrame; one per view that draws particles.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: Invalid after the frame's graph is compiled.
/// Units: Draw commands.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleViewDraws
{
	Flux_ViewID m_xView;
	Flux_FrameDataRange m_xCommands;          // Flux_IndirectDrawIndexed × m_uCommandCount
	Flux_FrameDataRange m_xRecords;           // Flux_ParticleDrawRecord × m_uCommandCount
	uint32 m_uCommandCount = 0;
	uint32 m_uFirstRun = 0;                  // into m_axRuns
	uint32 m_uRunCount = 0;
};

/// @contract value Flux_ParticleDrawRun
/// Ownership: Plain value: consecutive draw commands of one view sharing one PSO.
/// Thread affinity: IMMUTABLE. Members non-const: an element of an output array that its producer fills in place.
/// Invalidation: As Flux_ParticleViewDraws.
/// Units: Draw commands.
/// Errors: None.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleDrawRun
{
	Zenith_RHIPermutationKey m_xPermutation;  // the draw program's permutation (Rendering.md "PSO keys and permutations")
	uint32 m_uFirstCommand = 0;
	uint32 m_uCommandCount = 0;
	uint8 m_uPartition = 0;                  // 0 sorted (premultiplied blend), 1 additive
	uint8 m_uMeshIndex = 0xFF;               // MESH runs: the mesh whose streams the run binds
	uint8 m_auReserved[2] = {};
};

/// @contract value Flux_ParticleFrame
/// Ownership: Frame-scoped; built by Flux_ParticleBuildFrame in the frame's arena; its frame-data tickets belong to the
///   frame's upload ring and retire with it.
/// Thread affinity: OWNER_THREAD (the Flux.BuildGraph node).
/// Invalidation: Invalid after the frame's graph is compiled.
/// Units: As its members.
/// Errors: None; Flux_AddParticleSimulation and Flux_AddParticleDraw cannot fail.
/// Version: since Z2 Stage 5.
/// @endcontract
struct Flux_ParticleFrame
{
	Zenith_Span<const Flux_ParticleStep> m_axSteps;                     // m_uStepCount entries in step order; empty: no particle pass
	Zenith_Span<const Flux_ParticleDispatchBatch> m_axEmitBatches;      // ascending (step, permutation); a step's slice: m_axSteps
	Zenith_Span<const Flux_ParticleDispatchBatch> m_axEventBatches;     // event spawns, ascending (step, permutation)
	Zenith_Span<const Flux_ParticleDispatchBatch> m_axSimulateBatches;  // ascending (step, permutation)
	Zenith_Span<const Flux_ParticleDispatchBatch> m_axSortKeyBatches;   // the depth-sorted emitters' last steps; empty: no sort passes
	uint32 m_uStepCount = 0;                                            // 0 … uFLUX_PARTICLE_MAX_STEPS_PER_FRAME
	Flux_FrameDataRange m_xEmitterCommands;                              // Flux_EmitterCommand × m_uCommandCount, every step's
	Flux_FrameDataRange m_xCpuParticles;                                 // Flux_ParticleState × m_uCpuParticleCount
	Flux_FrameDataRange m_xColliders;                                    // Flux_ParticleCollider × m_uColliderCount
	uint32 m_uCommandCount = 0;
	uint32 m_uCpuParticleCount = 0;
	uint32 m_uColliderCount = 0;
	uint32 m_uSortedKeyCount = 0;                                       // Σ capacity of the depth-sorted emitters admitted
	Flux_ViewID m_xSortView;                                             // the view whose depth keys the sort uses; null: none
	Zenith_Span<const Flux_ParticleViewDraws> m_axViews;
	Zenith_Span<const Flux_ParticleDrawRun> m_axRuns;
	uint64 m_ulFrameIndex = 0;
};
```

### Flux_ParticleSystem internals

```cpp
// Called by the bodies of Flux_Renderer::EnableParticles and DisableParticles ("Flux_ParticleSystem"; VFX's, in RPROD's file)
struct Flux_ParticleSystemDeps
{
	Zenith_Heap* m_pxHeap = nullptr;                       // the renderer's heap
	Flux_GPUScene* m_pxGPUScene = nullptr;                 // CreatePool, AllocatePoolRange, FreePoolRange (RHI's pool allocation)
	Flux_PersistentResources* m_pxPersistent = nullptr;    // the quad index buffer, effect constants buffers, deferred deletion
	Flux_ReadbackPool* m_pxReadbacks = nullptr;            // the development builds' validation readbacks
};
// Validates xDesc (FLUX_PARTICLE_ERROR_INVALID_DESC), then runs the construction steps of "Flux_ParticleSystem" in order,
// unwinding in reverse on failure: step 1 from slice 2a, steps 2 … 6 from slice 11.
Zenith_Result<Zenith_Owned<Flux_ParticleSystem>, Flux_ParticleError> Flux_CreateParticleSystem(
	const Flux_ParticleSystemDesc& xDesc, const Flux_ParticleSystemDeps& xDeps);
// Releases every effect handle of the instance table and hands every pool range, constants buffer and the quad index
// buffer to deferred deletion; the owner then destroys the system (MAIN_THREAD, render-idle).
void Flux_ReleaseParticleSystem(Flux_ParticleSystem& xSystem);

// Called by Flux_Renderer::BuildGraph (Zenith2/Docs/Design/RPROD/Renderer.md "Flux.BuildGraph"; cross-area row VX2)
void Flux_ParticleApplyRenderWorld(Flux_ParticleSystem& xSystem, const Flux_RenderWorld& xWorld, const Zenith_AssetRegistry& xAssets);   // step 4
const Flux_ParticleFrame& Flux_ParticleBuildFrame(Flux_ParticleSystem& xSystem, const Flux_ViewList& xViews,
	Flux_UploadRing& xRing, Zenith_Arena& xFrameArena);                                                                // step 7c
void Flux_ParticleWriteFrameData(Flux_ParticleSystem& xSystem, Flux_UploadRing& xRing);                                // step 11, after FITS or EXTENDED
void Flux_ParticleEndFrame(Flux_ParticleSystem& xSystem, bool bSubmitted);                                            // step 13, every path
void Flux_ParticleOnDeviceRecovered(Flux_ParticleSystem& xSystem);                                                     // the renderer's recovery step
void Flux_ParticleApplyConfigChanges(Flux_ParticleSystem& xSystem, const Flux_ParticleSystemDesc& xNew);              // ApplyConfigChanges, prologue step 3

// Declared in Zenith2/Engine/ZenithFlux/Internal/Passes/Flux_PrologueFeatures.h (RPROD's header; VFX implements it)
void Flux_AddParticleSimulation(Flux_RenderGraph& xGraph, const Flux_ParticleFrame& xFrame, Flux_FrameShared& xShared);
// Declared in Zenith2/Engine/ZenithFlux/Flux_Features.h (RPROD's header; VFX implements it)
void Flux_AddParticleDraw(Flux_RenderGraph& xGraph, const Flux_ViewContext& xView, Flux_ViewTargets& xTargets);
```

`Flux_AddParticleDraw` reaches the frame's particle data through `Flux_FrameShared::m_pxParticles` (a `const Flux_ParticleFrame*`, null when particles are not enabled or the frame has no particle work) and the sorted pairs through `Flux_FrameShared::m_xParticleSorted`; both members are cross-doc item X240 (5) against `Zenith2/Docs/Rendering.md → "Shared producers"`, added in code to `Flux_ViewContext.h` by slice 2a unless RPROD's view-surface ticket already carries them ("Flux_ParticleSystem").

The ZenithFlux API slice (`Zenith2/Docs/Design/VFX.md → "Stage 5 slices"`, slice 2a, scheduled in the Stage 3 sprint) ships these declarations with these definitions: `Flux_AddParticleSimulation` and `Flux_AddParticleDraw` declare no pass (so RPROD's path functions, which call `Flux_AddParticleDraw`, link and run before the particle passes exist, [Z2-N-REND-281]); the six `Flux_Particle*` functions return without effect (`Flux_ParticleBuildFrame` returns a reference to an empty frame), so whichever of RPROD's renderer ticket and VFX slice 11 adds the `BuildGraph` calls first links; `Flux_CreateParticleSystem` validates the desc and allocates the system (step 1), `Flux_ReleaseParticleSystem` releases nothing yet, and the three `Flux_Renderer` bodies, `Flux_ParticleSystem::GetStats` (a zero snapshot), `GetInstanceStats` (false) and `SetDebugFlags` (stores the bits) are complete. So the module start and shutdown of slice 7 link and run against a real, empty particle system. `Test_ParticleDraw_NoParticleFrameDeclaresNoPass` asserts that a view with the particles feature bit and a null `m_pxParticles` declares no pass, and `Test_ParticleSystem_EnableDisableLifecycle` that `EnableParticles` refuses an invalid desc and a second call, that `GetParticleSystem` is non-null after it and null after `DisableParticles`, that `DisableParticles` with no system does nothing, and that the `Allocate` fault point unwinds to a null system. The algorithms of the five internal frame functions are `Zenith2/Docs/Design/VFX/Simulation.md` ("GPU simulation", "Render-world command stream") and `Zenith2/Docs/Design/VFX/Rendering.md` ("Draw records and indirect commands").

## Shared GPU structs

Declared `[FluxShared]` in `Zenith2/Shaders/Common/Particles.slang` (the module Rendering's "Shared GPU structs" table already lists for `Flux_EmitterCommand`, `Zenith2/Docs/Rendering/ShaderSystem.md → "Shared GPU structs"`; the added structs are in that table since cross-doc item X240), scalar layout, generated into `Flux_SharedTypes.generated.h` with a `static_assert(offsetof)` per field. Half values are stored as packed `uint` words written with GEOM's integer half codec on the GPU (`Flux_F32ToF16Bits`, `Zenith2/Shaders/Common/VertexFormats.slang`) and `Zenith_F32ToF16` on the CPU (ZenithVertexFormat), so CPU-written particles and GPU-written particles have one encoding.

### Flux_ParticleState

64 bytes per particle slot; the state region of the particle pool holds one per slot, and a CPU emitter's ring holds the same records.

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 12 | `float3 m_xPosition` | metres; world space, or emitter space for a local-space emitter |
| 12 | 4 | `float m_fAge` | seconds since spawn, ≥ 0 |
| 16 | 12 | `float3 m_xVelocity` | m/s, the position's space |
| 28 | 4 | `float m_fLifetime` | seconds; 0 marks a slot that never held a particle or whose particle died |
| 32 | 8 | `uint2 m_xColour` | half4: the spawn colour, linear Rec.709, pre-exposure-free, alpha in w |
| 40 | 4 | `uint m_uSize` | half2: the spawn size x, y in metres |
| 44 | 4 | `uint m_uRotation` | half2: rotation (radians, about the facing normal) and rotation rate (rad/s) |
| 48 | 4 | `uint m_uSpawnOrdinal` | the spawn ordinal; the slot is `ordinal mod capacity` |
| 52 | 4 | `uint m_uFlags` | bits 0–7 the emitter generation that spawned it; 8–9 collisions raised (0–3); 10 born this frame; 11 died this frame; 12 collided this frame; 13–15 trail head (0–7); 16–22 zero; 23 local space; 24–31 the sub-UV start frame (0–255, so every frame of a `subUV` module's at most 256 frames fits) |
| 56 | 4 | `float m_fCustom0` | the code-authored update type's first scalar (0 otherwise) |
| 60 | 4 | `float m_fCustom1` | its second scalar |

A slot is live when `m_fLifetime > 0`, its generation equals the emitter's current generation, and `m_fAge < m_fLifetime`. Colour and size over life are evaluated at draw time from the spawn values and `m_fAge / m_fLifetime`, so the update writes neither.

### Flux_EmitterCommand

256 bytes; one per admitted emitter per frame in the frame chunk, built from the render-world command and the render-side instance state.

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 48 | `float4 m_axEmitterToWorld[3]` | this frame's emitter-to-world rows |
| 48 | 48 | `float4 m_axPrevEmitterToWorld[3]` | the previous frame's rows |
| 96 | 4 | `uint m_uStateBase` | the emitter's first slot in the state region (element index) |
| 100 | 4 | `uint m_uCapacity` | slots |
| 104 | 4 | `uint m_uSpawnOrdinalBase` | rate, distance and burst emitters: the first new particle's ordinal; an event-spawned emitter (which has no CPU-scheduled spawn, `Zenith2/Docs/Design/VFX/EffectFormat.md → "Spawn modules"`): the source step tag whose event records this step consumes, `0xFFFFFFFF` for none ("Event spawning" of `Zenith2/Docs/Design/VFX/Simulation.md`) |
| 108 | 4 | `uint m_uSpawnCount` | rate and distance spawns (prewarm spawns with the prewarm flag) |
| 112 | 4 | `uint m_uBurstCount` | |
| 116 | 4 | `uint m_uEmitGroupBase` | prefix of emit groups within its batch |
| 120 | 4 | `uint m_uSimulateGroupBase` | prefix of simulate groups within its batch |
| 124 | 4 | `uint m_uSeed` | emitter seed |
| 128 | 4 | `uint m_uModuleMask` | update modules present, minus the LOD level's disabled mask |
| 132 | 4 | `uint m_uEffectConstants` | `FLUX_DESCRIPTOR_SPACE_BUFFER_SRV` index of the effect constants buffer |
| 136 | 4 | `uint m_uConstantsOffset` | byte offset of the emitter's block |
| 140 | 4 | `float m_fDeltaTime` | |
| 144 | 4 | `float m_fEffectTime` | |
| 148 | 4 | `float m_fSpawnAgeFirst` | |
| 152 | 4 | `float m_fSpawnAgeStep` | |
| 156 | 4 | `float m_fBurstAgeOffset` | |
| 160 | 4 | `uint m_uFlags` | bit 0 local space, 1 prewarm, 2 teleported, 3 reset, 4 depth collision enabled, 5 GPU bounds validation (on the emitter's last step of the frame only), 6–31 zero |
| 164 | 4 | `uint m_uGeneration` | the emitter's current generation (8 bits used) |
| 168 | 4 | `uint m_uTrailBase` | first trail sample (element index) or 0xFFFFFFFF |
| 172 | 4 | `uint m_uTrailSamples` | 0, 2 … 8 |
| 176 | 4 | `uint m_uEventBase` | this emitter's event ring (as a source) or 0xFFFFFFFF |
| 180 | 4 | `uint m_uEventCapacity` | records |
| 184 | 4 | `uint m_uEventSourceCommand` | for an event-spawned emitter: the source emitter's command index this frame, else 0xFFFFFFFF |
| 188 | 4 | `uint m_uEventEdge` | for an event target, its incoming edge packed (else 0): bits 0–1 `e_base` (0 for a collision edge, `m` for a death edge), bits 2–3 `E_edge` (1 … 3), bits 4–6 the source's `E` (1 … 4), bit 7 the edge's `on` (0 death, 1 collision), bits 8–11 `k − 1` (children per event 1 … 16), bits 12–15 zero, bits 16–31 the edge's `inheritVelocity` as a half (`Zenith_F32ToF16` on the CPU, the value both simulators use) ("Event spawning" of `Zenith2/Docs/Design/VFX/Simulation.md`) |
| 192 | 32 | `float4 m_axUserParameters[2]` | the eight user parameters |
| 224 | 12 | `float3 m_xEmitterVelocity` | |
| 236 | 4 | `uint m_uStepIndex` | the emitter's simulate-step tag: the render side's per-emitter step counter before this step; the frame build advances it by one per step it builds (catch-up and kept steps included, in step order) and `Flux_ParticleEndFrame(false)` restores it after a deferred or failed frame; a source writes it into its event records |
| 240 | 4 | `uint m_uColliderFirst` | the world's first collider in the frame's collider array |
| 244 | 4 | `uint m_uColliderCount` | |
| 248 | 4 | `uint m_uPrewarmCount` | |
| 252 | 4 | `uint m_uBoundsSlot` | GPU bounds validation slot, or 0xFFFFFFFF |

### Flux_ParticleDrawRecord

32 bytes; one per draw command, indexed by `Flux_DrawRecordIndex(m_uRecordBase)` ([Z2-N-REND-071]).

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | `uint m_uStateBase` | GPU: the emitter's first slot; CPU: the first record in the frame's CPU-particle range |
| 4 | 4 | `uint m_uCount` | slots (GPU) or live particles (CPU) drawn |
| 8 | 4 | `uint m_uSortedBase` | sprite and mesh draws: the first sorted pair of the emitter, or 0xFFFFFFFF for an age-ordered draw; ribbon draws (never depth-sorted): the length of the CPU-known possibly-live ordinal span `ordinalEnd − q_lo` ("Emitter ribbons" of `Zenith2/Docs/Design/VFX/Rendering.md`) |
| 12 | 4 | `uint m_uOrderStart` | age order: the slot of the oldest possible particle, `(ordinalEnd − capacity) mod capacity`; newest-first mode stores `ordinalEnd − 1` |
| 16 | 4 | `uint m_uCommandIndex` | the emitter's `Flux_EmitterCommand` (transform, constants, parameters) |
| 20 | 4 | `uint m_uTexture` | `FLUX_DESCRIPTOR_SPACE_TEXTURE_SRV` index of the sprite texture this frame (`Flux_TextureResource::GetSRV()`, or the white fallback) |
| 24 | 4 | `uint m_uFlags` | bit 0 CPU source, 1 newest first, 2 ribbon closed at dead particles, 3 local space, 4–7 the lighting mode, 8–15 the trail samples, 16–31 zero |
| 28 | 4 | `uint m_uLayerMask` | the instance's layer mask (lighting channels bits 24–31) |

### Flux_ParticleSortPair

8 bytes: `uint m_uKey`, `uint m_uPayload` (the state element index; bit 31 set for a CPU-particle index into the frame chunk range). The key layout is `Zenith2/Docs/Design/VFX/Simulation.md → "ParticleCompactAndSortKeys dispatch"`.

### Flux_ParticleEventRecord

48 bytes; the event ring of a source emitter holds `m_uEventCapacity` of them.

| Offset | Size | Field |
|---|---|---|
| 0 | 12 | `float3 m_xPosition` (world space) |
| 12 | 4 | `uint m_uStepIndex` (the source's `m_uStepIndex` of the step whose simulate wrote it; a target accepts exactly the tag its command names) |
| 16 | 12 | `float3 m_xVelocity` (world space) |
| 28 | 4 | `uint m_uSourceOrdinal` |
| 32 | 8 | `uint2 m_xColour` (half4, the source particle's spawn colour) |
| 40 | 4 | `uint m_uKindAndIndex` (bits 0–1 the compact event index `e`: collisions 0 … m − 1, death m = E − 1, so a death-only source writes 0; bit 2 death, bit 3 collision, exactly one set in a raised record and neither in a cleared one; bits 4–31 zero; "Event spawning" of `Zenith2/Docs/Design/VFX/Simulation.md`) |
| 44 | 4 | `uint m_uGeneration` (the source emitter's generation) |

### Flux_ParticleTrailSample

16 bytes: `float3 m_xPosition` (the particle's space), `uint m_uWidthAlpha` (half2: width scale and alpha scale at the sample).

### Flux_ParticleValue

48 bytes; one numeric module parameter in the effect constants image. Its fields and its evaluation `Flux_EvaluateValue` are `Zenith2/Docs/Design/VFX/EffectFormat.md → "Effect constants image"`; it is `[FluxShared]` here so that the C++ simulator (`Zenith2/Engine/ZenithVFX/Internal/Simulation/Zenith_VFXValues.cpp`) and the Slang simulator read one generated layout.

### Flux_ParticleValidationRecord

32 bytes; one per simulated GPU emitter in the development builds' validation buffer (`vfx.debug.validateBounds`), cleared by `AddClearBuffer` before the frame's first step, accumulated by the emitter's last step alone (the drawn state) and read back through the readback pool.

| Offset | Size | Field |
|---|---|---|
| 0 | 12 | `uint3 m_xMinBits` (world AABB minimum: order-preserving float encodings, `InterlockedMin`; cleared to `0xFFFFFFFF`) |
| 12 | 12 | `uint3 m_xMaxBits` (maximum, `InterlockedMax`; cleared to 0) |
| 24 | 4 | `uint m_uLiveCount` (`InterlockedAdd` of 1 per live slot after the step) |
| 28 | 4 | `uint m_uReserved` (zero) |

Integer atomics give an order-independent result, and nothing but diagnostics reads it ("GPU bounds validation" of `Zenith2/Docs/Design/VFX/Simulation.md`).

### Flux_ParticleCollider

64 bytes; the frame's collider array holds every admitted world's colliders.

| Offset | Size | Field |
|---|---|---|
| 0 | 48 | `float4 m_axWorldToCollider[3]` (world to collider space, unit scale removed: the inverse of the entity's rigid transform) |
| 48 | 12 | `float3 m_xHalfExtents` (box; capsule half length in y) |
| 60 | 4 | `uint m_uShapeAndMask` (bits 0–2 the shape, bits 8–31 the low 24 bits of the collision mask; radius below) |

The radius of a sphere or capsule is `m_xHalfExtents.x` for those shapes (the extractor writes it there), and a plane uses no extent.

## Slang interfaces for code-authored modules

VFX effects are authored in code with previews (Z2-D-079): an effect document names built-in modules and their parameters, and an emitter may name a Slang type for its GPU update and for its shading. Both interfaces live in `Zenith2/Shaders/Particles/ParticleInterfaces.slang`; engine types live under `Zenith2/Shaders/Particles/Types/`, a game's under `Zenith2/Games/<Name>/Assets/VFX/`. Each type is selected inside its program by link-time specialisation, so every (program, type) pair is its own compiled permutation, enumerated offline from the cooked effects (`Zenith2/Docs/Design/VFX/Authoring.md → "Particle type set step"`).

Cross-doc item X241 changed three normative ShaderSystem rules for this mechanism: [Z2-N-REND-105] names `Zenith2/Shaders/Particles/` and `Particles/Types/` as module directories beside `Common/` and `Lighting/`; [Z2-N-REND-106] lets games contribute code-authored particle update and shading types under `Zenith2/Games/<Name>/Assets/VFX/` beside material modules; [Z2-N-REND-123] and [Z2-N-REND-124] compile the particle programs at cook time multiplied by the game's particle type set, carried in `Zenith_RHIPermutationKey::m_ulMaterialVariantID`. Engine programs with variant 0 are compiled at build time.

### Flux_ParticleUpdateType

```slang
// Zenith2/Shaders/Particles/ParticleInterfaces.slang
struct Flux_ParticleUpdateInput
{
	Flux_ParticleState m_xState;        // after every built-in module of the frame ran
	float m_fDeltaTime;
	float m_fEffectTime;
	float m_fNormalisedAge;             // age / lifetime
	float4 m_axUserParameters[2];
	uint m_uSeed;
	uint m_uRandomBase;                 // draw index base 0x1000 reserved for the type
};

interface Flux_ParticleUpdateType
{
	associatedtype Params;                                   // ≤ 256 bytes, scalar layout, plain data only
	static Flux_ParticleState Update(Flux_ParticleUpdateInput xIn, Params xParams);
	static const bool bKILLS;                                // true: Update may set m_fLifetime to 0 (a death event fires)
};

extern struct Flux_ActiveParticleUpdate : Flux_ParticleUpdateType;   // bound per permutation
```

`Update` is a pure function: it reads no resource, writes nothing but its return value, has no derivative and no groupshared access; it may change position, velocity, colour, size, rotation, the two custom scalars and (with `bKILLS`) the lifetime, and the simulate program writes the returned state. An emitter with a code-authored update type is GPU-only (the cook refuses `target: cpu`, `VFX_COOK_ERROR_TARGET_MODULE`).

### Flux_ParticleShadingType

```slang
struct Flux_ParticleShadeInput
{
	float3 m_xPositionWS;
	float3 m_xViewDirWS;               // unit, particle to camera
	float3 m_xNormalWS;                // unit: the facing normal (a sprite), the strip normal (a ribbon), the mesh normal
	float2 m_xUV;                      // the flipbook frame's UV
	float2 m_xUVNext;                  // the next frame's UV (flipbook blending)
	float m_fFrameBlend;               // 0 … 1
	half4 m_xColour;                   // spawn colour × over-life × by-speed modules
	float m_fNormalisedAge;
	float4 m_axUserParameters[2];
	uint m_uTexture;                   // the draw record's texture index
	float2 m_xScreenUV;
	float m_fLinearDepth;
};

struct Flux_ParticleShadeOutput
{
	half3 m_xColour;                   // linear, not pre-exposed
	half m_fOpacity;
	half3 m_xEmissive;                 // linear, not pre-exposed; never lit
	half m_fLitFraction;               // 0 … 1: how much of m_xColour is lit when the emitter's lighting is not UNLIT
};

interface Flux_ParticleShadingType
{
	associatedtype Params;                                   // ≤ 256 bytes, plain data and DescriptorHandle<Texture2D<…>> fields
	static Flux_ParticleShadeOutput Shade(Flux_ParticleShadeInput xIn, Params xParams);
};

extern struct Flux_ActiveParticleShading : Flux_ParticleShadingType;
```

`Shade` samples textures only through `Flux_SampleParticle(xTexture, uSampler, xUV)` (a `SampleLevel` at a level the vertex stage computes from the particle's screen size, so no implicit derivative is used and the same code runs in the vertex-lit path), writes nothing, and never discards (transparent particles blend; [Z2-N-REND-242]'s purity rule applied to particles). The engine type `ParticleShading_Standard` (`Zenith2/Shaders/Particles/Types/Standard.slang`) is `colour = texture × input colour`, opacity the texture's alpha × input alpha, emissive `colour × m_fEmissiveScale`, lit fraction 1.
