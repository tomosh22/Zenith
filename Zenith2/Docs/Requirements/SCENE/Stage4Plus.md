# SCENE requirement catalogue: the Stage 4 module seams

## Purpose

The second file of the SCENE catalogue: the Stage 4 requirements Z2-R-SCENE-166 to -182 of the module seams specified in `Zenith2/Docs/Design/SCENE/ModuleSeams.md` (the activation hook STREAM implements, the world origin and its shift, the origin-shifted event, the fluid query), added by the post-signoff patch of 2026-10-09, in the same format and the same `Z2-R-SCENE-NNN` numbering as `Zenith2/Docs/Requirements/SCENE.md`, which holds the purpose of the whole catalogue, every earlier requirement (Stage 4 ones included), the coverage of inventory items over both files and the exclusions.

## Requirements

### Z2-R-SCENE-166 Activation hook declarations

- **Statement:** ZenithScene declares `Zenith_SceneActivationHook` (the eight members `OnLoadCommitted`, `OnDiscardedPublication`, `IsPreparationGateOpen`, `ShouldSkipEntity`, `AmendComponents`, `OnPreparationEnded`, `AdmitActivation`, `GetRootTranslation`), `Zenith_SceneComponentAmender`, `Zenith_AdditiveLoadDesc::m_ulHookTag`, `Zenith_SceneWorldState::SetActivationHook`, `Zenith_AdditiveSceneSet::GetLiveInstances`, `GetErrorDetail` and `IsCaptureCoherentOnly`, `Zenith_PfnScenePublicationResolve` with `Zenith_ScenePublicationSink::SetPublicationResolver`, and the preparation probe seam, each with its contract block, and every existing implementer of the sink keeps compiling.
- **Covers:** Z2-I-SCENE-06, Z2-I-SCENE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-047, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_SceneApi_ModuleSeamDeclarations` (Test step, `ZenithScene_Tests`); `HeaderSelfContained` and `ApiContractCheck` steps
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Activation hook declarations"

### Z2-R-SCENE-167 Activation hook call points

- **Statement:** For a tagged load SCENE calls `OnLoadCommitted` at the `Scene.LoadAdditive` playback for the commit or its refusal (`SCENE_ERROR_LIMIT`, `SCENE_ERROR_DUPLICATE_INSTANCE`), `AdmitActivation` and then `GetRootTranslation` on the `Scene.ActivateAdditive` node before recording an instance's spawns (a refusal leaves it QUEUED without advancing its retry count; the translation is added to every root's local position and every recorded world and previous position and translation column), and the preparation members on the preparation thread; an untagged load and a null hook make no call.
- **Covers:** Z2-I-SCENE-06, Z2-I-SCENE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_HookCallPoints` (Test step, `ZenithScene_Tests`); `Test_SceneSeams_TaggedDiscardReplaysAtRecordedTick` (Test step, `Zenith_Integration_Scene`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Hook call points"

### Z2-R-SCENE-168 Gated preparation state for tagged loads

- **Statement:** A tagged preparation passes through a GATED state between DEPENDENCIES and BUILDING that polls `IsPreparationGateOpen` once per slice, never blocking and cancellable at every slice, and runs BUILDING in two passes, so `ShouldSkipEntity` (offered in cooked order to every entity whose ancestors were kept) and `AmendComponents` (once per kept entity) are called only after the gate opened, a skipped entity's descendants are removed with it, and every ENTITY value naming a removed entity is null and counted in `scene.references.skipped`.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_HookCallPoints` (Test step, `ZenithScene_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Gated preparation state"; Zenith2/Docs/Design/SCENE/Loading.md → "Preparation state machine"

### Z2-R-SCENE-169 One discarded publication per tagged load unloaded while loading

- **Statement:** Every committed tagged load whose instance becomes UNLOADED while LOADING, whoever recorded the unload, yields exactly one published publication of its completion ID whatever the operation's outcome (succeeded, failed, cancelled, or a pending preparation removed before it started, which the `Scene.ForgetInstance` playback parks at once), and the tick-boundary handler keeps that record until the publication handler has called `OnDiscardedPublication` and released it.
- **Covers:** Z2-I-SCENE-06, Z2-I-SCENE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_DiscardResolverRecordsCancelled` (Test step, `ZenithScene_Tests`); `Test_SceneSeams_TaggedDiscardReplaysAtRecordedTick` (Test step, `Zenith_Integration_Scene`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Discarded publications of tagged loads"; Zenith2/Docs/Design/SCENE/Loading.md → "Tick-boundary handler"

### Z2-R-SCENE-170 Discarded publications are recorded and decided at publication

- **Statement:** SCENE's resolver, installed through `SetPublicationResolver` in `Attach`, rewrites in live mode every drained kind-8 completion whose record carries "discard requested" (written only by the `Scene.ForgetInstance` playback) to `PUBLICATION_KIND_GENERIC_RECORDED` `{ instance, SCENE_ERROR_CANCELLED, 0 }` and leaves every other completion unchanged, so a result parked before the unload's playback, after it, or never is logged identically and a replay publishes it at the recorded tick without holding a tick; a late local completion after the replayed discard is released by `StepActivations` and counted in `scene.prepare.orphaned`.
- **Covers:** Z2-I-SCENE-06, Z2-I-SCENE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_DiscardResolverRecordsCancelled` (Test step, `ZenithScene_Tests`); `Test_SceneSeams_TaggedDiscardReplaysAtRecordedTick` (Test step, `Zenith_Integration_Scene`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Discarded publications of tagged loads"; Zenith2/Docs/Design/SCENE/Loading.md → "Publication service"

### Z2-R-SCENE-171 Preparation end reported once per tagged load

- **Statement:** `OnPreparationEnded` is called exactly once per committed tagged load, before any park of its publication: on the preparation thread after its last other call whatever its status, on the main thread in `StepActivations` for a refused start, and on the sync-point node in the `Scene.ForgetInstance` playback for a pending preparation removed before it started.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_HookCallPoints`, `Test_AdditiveScenes_ComponentAmenderEdits` (Test step, `ZenithScene_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Hook captured per load"

### Z2-R-SCENE-172 The hook is captured per load

- **Statement:** A committed tagged load's pending-preparation record captures the world's hook current at its commit and every subsequent call of that load (gate, skip, amend, end, discarded publication) uses the captured pointer, so `SetActivationHook(nullptr)` affects only commits made after it, while `OnLoadCommitted`, `AdmitActivation` and `GetRootTranslation` use the world's current hook.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_HookCallPoints` (Test step, `ZenithScene_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Hook captured per load"

### Z2-R-SCENE-173 Component amendment before the init record is final

- **Statement:** `AmendComponents` is called once per kept entity in BUILDING pass 2 with a `Zenith_SceneComponentAmender` whose `AddComponent` appends a registered type's default init block (a no-op for a present type, ignored and counted in `scene.amend.unknown_types` for an unregistered one) and whose `RemoveComponent` drops a present type keeping the others' order (a no-op for an absent one); an arena failure fails the preparation with `SCENE_ERROR_OUT_OF_MEMORY`, and the entity spawns with exactly the amended set.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-045
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_ComponentAmenderEdits` (Test step, `ZenithScene_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Component amendment"

### Z2-R-SCENE-174 Live instances in ascending order

- **Statement:** `Zenith_AdditiveSceneSet::GetLiveInstances` returns the IDs of every LOADING, QUEUED, ACTIVE and FAILED record in ascending order and never an UNLOADED one, as a span valid until the set's next change.
- **Covers:** Z2-I-SCENE-06
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_LiveInstancesAndCoherence` (Test step, `ZenithScene_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Live instances and capture coherence"

### Z2-R-SCENE-175 Capture coherence input for the additive-set participant

- **Statement:** `Zenith_AdditiveSceneSet::IsCaptureCoherentOnly` is true exactly when the set holds a LOADING or QUEUED instance, an UNLOADED tagged record whose discarded publication is unpublished (which `ForEachInstance` never reports), or a tagged commit the `Scene.LoadAdditive` playback refused during the current tick (a flag cleared at the next tick boundary), so SAVE's `Zenith.Scene.AdditiveSet` participant reports such a capture COHERENT.
- **Covers:** Z2-I-SCENE-06, Z2-I-SAVE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_LiveInstancesAndCoherence`, `Test_AdditiveScenes_AssetErrorDetailPublished` (Test step, `ZenithScene_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Live instances and capture coherence"

### Z2-R-SCENE-176 Cancelled-preparation counters per state and the preparation hold

- **Statement:** SCENE counts every cancelled preparation in `scene.prepare.cancelled.<state>` for the state the cancellation reached (`REQUESTED`, `READING`, `DECODING`, `DEPENDENCIES`, `GATED`, `BUILDING`, `PENDING`), and `Zenith_SceneLoader::SetPreparationProbe` with the header-only `Tests/Support/Zenith_ScenePreparationHold.h` parks a named instance's preparation at the start of DECODING, DEPENDENCIES or BUILDING, before any work of that state, until released, each re-poll a cancellation point.
- **Covers:** Z2-I-SCENE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_ScenePreparationHold_ParksNamedStage` (Test step, `ZenithScene_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Preparation probe and hold"; Zenith2/Docs/Design/SCENE/Loading.md → "Cancellation"

### Z2-R-SCENE-177 The ASSET cause of an unavailable asset is published

- **Statement:** Every preparation failure with `SCENE_ERROR_ASSET_UNAVAILABLE` carries the causing `Zenith_AssetErrorCode`, never 0, in `Zenith_SceneError::m_uDetail` and the published `Zenith_SceneLoadResult::m_uDetail` (READING: the registry's load error; DEPENDENCIES: `AcquireAny`'s error; for `ASSET_ERROR_DEPENDENCY_FAILED` the dependency's root code once ASSET supplies it, else that code itself), stored in the FAILED record and returned by `GetErrorDetail` (0 for no detail or an unknown instance).
- **Covers:** Z2-I-SCENE-07
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-045, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_AssetErrorDetailPublished` (Test step, `ZenithScene_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Asset error detail"

### Z2-R-SCENE-178 Restore re-requests are reported to the hook

- **Statement:** For a LOADING saved instance with a non-zero hook tag, `RestoreInstances` calls `OnLoadCommitted(tag, instance, 0)` on the main thread after creating the record and capturing the hook, and `OnLoadCommitted(tag, 0, code)` after parking the failure of a re-request it refuses; with a null hook it makes neither call.
- **Covers:** Z2-I-SCENE-06, Z2-I-SAVE-01
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_AdditiveScenes_RestoreReRequestReportsToHook` (Test step, `ZenithScene_Tests`); `Test_SceneSeams_RestoredLoadingInstanceCompletes` (Test step, `Zenith_Integration_Scene`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Restore re-requests and the hook"

### Z2-R-SCENE-179 World origin resource

- **Statement:** ZenithScene registers the world resource `Zenith.WorldOrigin` (`TYPE_KIND_RESOURCE`, owning phase `PHASE_INPUT`, no system writer, five serialized properties: X, Y, Z in binary64, the rebase count and the last rebase tick) and the command kind `Scene.ShiftWorldOrigin` with `Zenith_RecordShiftWorldOrigin`; every world's SCENE state owns one origin, zero at creation, written only by that command's playback and saved through SAVE's resources section.
- **Covers:** Z2-I-SCENE-08, Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldOrigin_ResourceRegistration` (Test step, `ZenithScene_Tests`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Zenith_WorldOrigin header"

### Z2-R-SCENE-180 Origin shift plays last in its sync point

- **Statement:** The `Scene.ShiftWorldOrigin` playback runs after every structural record of step 6 and every other step-7 record of its sync point whatever its merge key, applies at most one shift per sync point (a second, or one with a non-finite, non-integral or out-of-range component, is counted and ignored), adds S to the origin in binary64, and sets every world position and translation column, every previous world position, every root local position and every `PROPERTY_FLAG_WORLD_POSITION` property to `(float)((double)p − S)` in ascending simulation ID, allocating nothing.
- **Covers:** Z2-I-SCENE-08, Z2-I-STREAM-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-019
- **Budgets:** Z2-B-SHIP-016
- **Acceptance evidence:** `Test_WorldOrigin_ShiftPlaysLastInSyncPoint`, `Test_WorldOrigin_ShiftsTransformsAndFlaggedProperties`, `Test_WorldOrigin_SecondAndInvalidShiftsIgnored` (Test step, `ZenithScene_Tests`); `Test_SceneSeams_OriginShiftThroughRuntime` (Test step, `Zenith_Integration_Scene`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Origin shift playback"

### Z2-R-SCENE-181 Origin-shifted lifecycle event

- **Statement:** After an applied shift, still inside step 7 of its sync point, the playback dispatches `LIFECYCLE_EVENT_ORIGIN_SHIFTED` through the world's `Zenith_LifecycleDispatcher` with a null entity, the current tick and a payload pointing at the played `Zenith_WorldOriginShift`, to every listener whose interest mask has the event's bit; an ignored shift dispatches nothing.
- **Covers:** Z2-I-SCENE-08, Z2-I-PHYS-28
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_WorldOrigin_OriginShiftedEventDispatched` (Test step, `ZenithScene_Tests`); `Test_SceneSeams_OriginShiftThroughRuntime` (Test step, `Zenith_Integration_Scene`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Origin shifted lifecycle event"

### Z2-R-SCENE-182 Fluid query interface in ZenithScene

- **Statement:** ZenithScene declares `Zenith2/Engine/ZenithScene/Zenith_FluidQuery.h` (`Zenith_FluidSample`; `Zenith_FluidQuery` with `virtual bool Sample(float fX, float fZ, Zenith_FluidSample& xSampleOut) const = 0` and `xTYPE_ID`) as an upward-callback interface and registers the resource type `Zenith.FluidQuery` (`TYPE_KIND_RESOURCE`, owning phase `PHASE_INPUT`, no property), never adding the resource to a world itself.
- **Covers:** Z2-I-SCENE-08, Z2-I-ENV-05
- **Stage:** S4
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-046
- **Budgets:** none
- **Acceptance evidence:** `Test_FluidQuery_ResourceTypeRegistered` (Test step, `ZenithScene_Tests`); `Test_SceneSeams_FluidQueryResourceThroughRuntime` (Test step, `Zenith_Integration_Scene`)
- **Design:** Zenith2/Docs/Design/SCENE/ModuleSeams.md → "Zenith_FluidQuery header"
