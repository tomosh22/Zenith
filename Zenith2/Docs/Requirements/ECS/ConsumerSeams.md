# ECS requirement catalogue: consumer seams

## Purpose

The second file of the ECS catalogue: the requirements Z2-R-ECS-176 onward, in the same format and the same `Z2-R-ECS-NNN` numbering as `Zenith2/Docs/Requirements/ECS.md`, which holds the purpose of the whole catalogue, the requirements Z2-R-ECS-001 to Z2-R-ECS-175, the coverage of inventory items (over both files) and the exclusions. Each requirement here is a seam other areas consume: the edit-world flag (MOD sets it, EDIT reports it), container writes without a merge reservation (AI's lifecycle listener, CINE's tick-boundary restore), per-ordinal event writers (CINE), play-last command kinds and the playback version (SCENE's origin shift) and entity handles as reflected C++ members (every area that registers a component holding a `Zenith_EntityHandle`). ECS owns every requirement here; the consuming area's own requirements cover its use. Every named test is specified in `Zenith2/Docs/Design/ECS/Tests.md`.

## Requirements

### Z2-R-ECS-176 Edit-world flag owned by ECS and set from the world's purpose

- **Statement:** `Zenith_WorldDesc::m_bEditWorld` (false by default) is copied by every backend factory and reported unchanged for the world's whole life by `Zenith_World::IsEditWorld()`; it is the only edit-world marker, ECS alone gives it meaning (`Zenith_WorldSimulation::ApplySchemaRevision` refuses a world whose flag is false), the world's creator sets it from the world's purpose (true exactly for `WORLD_PURPOSE_EDIT`), and two worlds that differ only in the flag hold, play back and hash identically.
- **Covers:** Z2-I-ECS-03, Z2-I-ECS-08
- **Stage:** S3
- **Classification:** required
- **Decisions:** Z2-D-039, Z2-D-048
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_EditWorldFlagReported` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "The edit-world flag"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_World header"; Zenith2/Docs/Design/ECS/Backends.md → "Factory construction steps"

### Z2-R-ECS-177 In-place container rewrite without a reservation

- **Statement:** `Zenith_WorldPlaybackAccess::RewriteContainer` replaces, on every backend, the values of a container whose element type carries no lifetime inside the block the cell already owns, reserving, allocating and freeing nothing, so it never fails for capacity; it refuses a different element count with `ECS_ERROR_VALUE_SIZE` and a non-container property or a lifetime-carrying element type with `ECS_ERROR_UNKNOWN_PROPERTY`, changing nothing; and every `SetContainer` of non-zero block bytes runs within a reservation its caller holds.
- **Covers:** Z2-I-ECS-04, Z2-I-ECS-15
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-040, Z2-D-051
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_RewriteContainerInPlace` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "In-place container rewrite"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Container writes without a merge reservation"; Zenith2/Docs/Design/ECS/Backends.md → "Container heap"

### Z2-R-ECS-178 Tick-boundary container writes reserve their own block

- **Statement:** `Zenith_TickBoundaryAccess::SetContainer` reserves the new block itself, returns `ECS_ERROR_CAPACITY` with nothing changed and no reservation outstanding when the container heap refuses it, and otherwise writes through the playback access's `SetContainer`, committed after tick-boundary step 3 at V(t, 0) like every tick-boundary property write; the access still exposes no structural member.
- **Covers:** Z2-I-ECS-05, Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-080, Z2-D-053
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_TickBoundarySetContainer` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Tick-boundary container writes"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_ECSObservers header"; Zenith2/Docs/Design/ECS/Scheduling.md → "Tick boundary and end-of-tick"

### Z2-R-ECS-179 Event writers per logical invocation ordinal

- **Statement:** `Zenith_SystemContext::Events(ulSimulationID, uInvocationOrdinal)` returns the event writer of the logical invocation `Commands(ulSimulationID, uInvocationOrdinal)` opens and records into that invocation's one buffer, so the events of each ordinal are accepted or rejected at the merge with that buffer alone; `Events(ulSimulationID)` is `Events(ulSimulationID, 0)`; and every such call sets the context's current simulation ID.
- **Covers:** Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_ECS_EventsPerInvocationOrdinal` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/SystemAPI.md → "Zenith_SystemContext header"; Zenith2/Docs/Design/ECS/SystemAPI.md → "The current logical invocation"; Zenith2/Docs/Design/ECS/Scheduling.md → "Recording"

### Z2-R-ECS-180 Play-last command kinds and the playback version

- **Statement:** `Zenith_CommandKindTable::Register` accepts a `Zenith_CommandKindDesc::m_uFlags` of 0 or `COMMAND_KIND_FLAG_PLAY_LAST` and refuses any other bit with `ECS_ERROR_INVALID_DESC`; step 7 of every sync point and of out-of-tick playback plays every record of a play-last kind after every other `MODULE` record, each group in merge order, identically for any worker count; and `Zenith_WorldPlaybackAccess::GetPlaybackVersion()` returns the version of the playback in progress (a sync point's merge version, V(t, M) at the end of tick t, V(t, 0) at its tick boundary, `GetChangeVersion()` out of tick) and 0 outside every playback.
- **Covers:** Z2-I-ECS-05, Z2-I-ECS-09
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-019, Z2-D-080
- **Budgets:** none
- **Acceptance evidence:** `Test_CommandKindTable_PlayLastFlag` in the `Test` step (win-debug, win-release); `Test_ECS_PlayLastKindsPlayAfterOtherRecords` and `Test_ECS_PlaybackVersionPerPlayback` in `ZenithECS_Conformance` on the Custom, flecs and EnTT targets, in the `Test` step and the `Zenith2ECS` category's `EcsConformance` line
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Play-last command kinds"; Zenith2/Docs/Design/ECS/WorldAPI.md → "Playback version"; Zenith2/Docs/Design/ECS/Scheduling.md → "Steps"

### Z2-R-ECS-181 Entity handles as reflected C++ members

- **Statement:** `Zenith_ReflectValueTraits<Zenith_EntityHandle>`, declared in `Zenith_EntityHandle.h`, makes a C++ component member of that type a `PROPERTY_TYPE_ENTITY` property of 8 bytes that carries no lifetime, so a trivially copyable component holding handles and plain values registers through `Zenith_TypeBuilder::Property` with null hooks and a default object of null handles, and a `Default` of any other size fails `Freeze` with `REFLECT_ERROR_BAD_DEFAULT`.
- **Covers:** Z2-I-ECS-12, Z2-I-ECS-16
- **Stage:** S2
- **Classification:** required
- **Decisions:** Z2-D-020, Z2-D-038
- **Budgets:** none
- **Acceptance evidence:** `Test_EntityHandle_ReflectValueTraits` in the `Test` step (win-debug, win-release)
- **Design:** Zenith2/Docs/Design/ECS/WorldAPI.md → "Zenith_EntityHandle header"; Zenith2/Docs/Design/ECS/Reflect.md → "Zenith_ReflectValueTraits header"
