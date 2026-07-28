# S7 item 3 SC6 -- APPROVED IMPLEMENTATION PLAN (trainer sight FSM + occlusion glue)

**Status: PLANNED, NOT IMPLEMENTED.** The authoring pass for this sub-commit was cut off by a session usage limit on 2026-07-28; the survey and planning passes completed and are preserved here verbatim so the next session resumes from the plan rather than re-deriving it. Delete this file in the commit that lands SC6.

## Settled question 1 -- is the occlusion raycast real?

SETTLED: the raycast is a REAL filter here, not decoration. Ship it for real (option "a" is not needed — there is nothing to apologise for).

Three facts, each verified against the tree rather than the survey text:

1. PHYSICS IS LIVE HEADLESS. `Zenith_Engine::InitialiseRendererAndPhysics` (C:\dev\Zenith\Zenith\Core\Zenith_Engine.cpp:538-550) creates and initialises `Zenith_Physics` unconditionally — no backend gate — and `Zenith_Core.cpp:171-183` ticks `Physics().Update` gated only on play-mode. The Null Sharpmake arm (C:\dev\Zenith\Build\Sharpmake_Common.cs:104-111) excludes only `Vulkan\`, `D3D12\` and `imgui_impl_vulkan`. The only headless harness skip is `m_bRequiresGraphics` (C:\dev\Zenith\Zenith\Core\Zenith_AutomatedTest.h:98-104), which is about READING PIXELS. Empirical proof already in-tree: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_Overworld.cpp:653-664 asserts `xWalkableHit.m_bHit` / `xSteepHit.m_bHit` from a plain BOOT unit with no `#ifdef` guard, so those raycasts must return real hits in the Null boot the CI unit gate runs.

2. DAWNMERE HAS REAL, COMMITTED OCCLUDERS. `Zenithmon.cpp:1915-1922` authors `DawnmereHomeShell` (16x6x40), `DawnmereHomeDoorLeft/Right`, `DawnmereHomeDoorLintel`, each `AddStep_SetEntityTransient(false)` + `AddStep_AddColliderShape(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_STATIC)`; `ZM_QueueDawnmereNpc` (`:1298-1300`) gives every stationary NPC a static AABB; the player is a dynamic capsule (`:1908-1909`); `HomeDoorTrigger` is a static AABB (`:1934-1936`). All of these are ENTITIES IN THE COMMITTED `Assets/Scenes/Dawnmere.zscen`, so they exist on a fresh CI checkout and produce Jolt bodies on load. The ray is therefore blockable in every environment.

3. THE ONE HONEST LIMITATION — TERRAIN. `Zenith_ColliderComponent::CreateTerrainShape` (Zenith_ColliderComponent.cpp:372-385) returns nullptr with a warning when `!xTerrain.HasPhysicsGeometry()`, and that geometry is loaded from `Assets/Terrain/Dawnmere/Physics_*.zmesh`, which are gitignored and NOT committed. So on a fresh CI checkout TERRAIN is not an occluder (a hill does not block a trainer's view there; it does on a machine with baked terrain). CONSEQUENCE FOR TESTS, and this is binding: NO test may assert occlusion against terrain — it would pass locally and be vacuous or red in CI. Every occlusion assertion in this plan is made against an EXPLICITLY CREATED static AABB inside a hermetic `PhysicsSceneScope` fixture (the ZM_Tests_Overworld.cpp:87-154 idiom), which needs no assets at all.

API and its two sharp edges, both designed around rather than ignored:
- The only filter the engine offers is a SINGLE-body ignore: `Zenith_PhysicsQuery::RaycastIgnoring(origin, dir, maxDist, Zenith_EntityID)` (Zenith\EntityComponent\Zenith_PhysicsQuery.h:20-24) -> `JPH::IgnoreSingleBodyFilter` (Zenith_Physics.cpp:854-864). There is NO layer/mask/channel argument: `RaycastImpl` hard-codes `JPH::BroadPhaseLayerFilter()` / `JPH::ObjectLayerFilter()`.
- Only one body can be ignored, and Jolt treats convex shapes as solid from inside, so (i) the TRAINER is filtered by entity id, and (ii) the PLAYER's own capsule, which necessarily terminates the ray at the far end, is recognised EXACTLY by comparing `RaycastResult::m_xHitEntity` against the resolved player `Zenith_EntityID`. This is strictly better than `Zenith_PerceptionSystem::CheckLineOfSight`'s 0.5 m magic tolerance (Zenith_PerceptionSystem.cpp:819-849) and introduces no constant.
- There is NO raycast budget or throttle anywhere in the engine (the claim in Zenith/AI/CLAUDE.md is aspirational; `UpdateSightPerception` walks every agent every call). Cost is controlled the only way the tree actually does it: the PURE cone runs first, and the ray is cast ONLY for a trainer that already passed it. In Dawnmere that is <= 1 ray/frame.

We do NOT adopt `Zenith_PerceptionSystem`: it is private, point-to-point, imports awareness ramps/eye height/target registration, and Zenithmon never ticks it (`Perception::Update` is called only from DevilsPlayground and RenderTest). The reusable asset is the idiom, not the class.

Ray geometry decision (recorded because it is an open design point, not a fact in the tree): the ray runs from the trainer's TRANSFORM POSITION to the player's TRANSFORM POSITION, full 3D, with `fMaxDistance` = the exact separation. No eye-height constant is invented, because Zenithmon authors every NPC body at the PLAYER's own scale and centre height (Zenithmon.cpp:1943-1946), so centre-to-centre already is a chest-height ray. `Zenith_SightConfig::m_fEyeHeight` belongs to a system this game does not use.

FAIL POLARITY, stated explicitly: no live simulation (`!Physics().HasActiveSimulation()`) FAILS OPEN (line reported clear). A world with no physics has no occluders; failing closed would blind every trainer in any physics-less context. Non-finite input FAILS CLOSED (line reported blocked), matching `ZM_IsTargetInTrainerSight`'s own totality table. The probe result carries `m_bPhysicsAvailable` precisely so a test can prove it did NOT take the fail-open branch — that is the anti-vacuity handle for the whole occlusion feature.

## Settled question 2 -- does this change committed .zscen bytes?

EXACT ANSWER: SC6 CHANGES ZERO SCENE BYTES, BY CONSTRUCTION. `ZM_Interactable::uSERIALIZATION_VERSION` STAYS `2u`, `WriteToDataStream` gains not one field, and a windowed `_True` boot must still leave all four `.zscen` files unmodified in `git status`.

Why this matters and what would have happened otherwise. `Dawnmere.zscen` carries FIVE `ZM_Interactable` payloads and is COMMITTED and TRACKED (as are Battle/FrontEnd/PlayerHome, ZM-D-148). The per-component size prefix (Zenith_ComponentMeta.cpp:253-269) is computed from what is actually written, so there is no framing that hides a write-side growth: adding ONE serialized field would grow five payloads, change five size prefixes, bump the leading version to `3u`, and a windowed boot (which re-authors and re-saves Dawnmere whenever every terrain recipe is warm — Zenithmon.cpp:1861) would leave `Dawnmere.zscen` modified. Games/Zenithmon/CLAUDE.md states that a boot leaving a scene modified is a REGRESSION of the boot-shape-independence property. Paying that cost in SC6 would have meant re-baking and committing `Dawnmere.zscen` in the same commit AND blunting the one tripwire that detects the ZM-D-148 slot-index regression, for a feature whose authored placement does not land until SC8.

HOW THE PLAN AVOIDS IT. Both new members are RUNTIME-ONLY, exactly like the walker's `m_xWalkerState` / `m_bOwnsInteractionMenu` / `m_xConfiguredWanderBodyID`, which `WriteToDataStream` already never touches:
  ZM_TRAINER_ID      m_eTrainerId = ZM_TRAINER_NONE;   // installed by ConfigureTrainerSight, NEVER serialized
  ZM_TrainerSightFsm m_xSightFsm;                      // session state, reset in OnStart
Both are PODs, so the header's `= default` noexcept moves stay correct under the pool's swap-and-pop relocation. `ReadFromDataStream` clears both in its unconditional reset block (alongside the existing clears), so a reloaded scene starts with no trainer and a cold watcher — deterministic, and identical to a fresh component.

This is guarded by a TEST, not by intention: `Interactable_TrainerSightIsNotSerialized` (see tests) configures a trainer, writes to a `Zenith_DataStream`, and asserts (a) the byte length is IDENTICAL to a stream written by an unconfigured component, (b) `uSERIALIZATION_VERSION == 2u`, and (c) a second component read from that stream has `ZM_TRAINER_NONE`. Anyone who later appends a field or bumps the version reds that unit before they ever notice a dirty `.zscen`.

THE DEBT THIS CREATES, STATED PLAINLY AND OWNED BY SC8. Because `m_eTrainerId` does not persist, a trainer configured during the tools authoring pass does NOT survive `AddStep_SaveScene` -> reload. SC6 therefore ships BEHAVIOUR ONLY, exercised by tests that configure a trainer at runtime; it cannot yet place a live trainer in Dawnmere, which is precisely what the sub-commit plan already assigns to SC8. SC8 has two routes, and the header comment on `m_eTrainerId` must name both:
  (1) ZERO-BYTE ROUTE (recommended): add a `ZM_TRAINER_ID m_eTrainer` column AT THE END of `ZM_NpcData`'s row struct and derive `m_eTrainerId` from the already-serialized `m_eNpcId` in `OnStart`. A compiled-const table column costs nothing on disk, so `Dawnmere.zscen` still never moves. Cost: it touches every positional `ZM_NpcData` row initializer and its units (see Status.md's "adding a data row can disarm an existing test" warning).
  (2) VERSION ROUTE: bump `uSERIALIZATION_VERSION` to `3u`, append the trainer block after the walker block, gate the read with `if (uVersion == 2u) { return; }` exactly as v1->v2 already does — and then SC8 owns the re-bake + same-commit re-commit of `Dawnmere.zscen`, plus the obligation to verify that ONLY Dawnmere goes modified (Battle/FrontEnd/PlayerHome moving would be a different, real defect).

Also unchanged: no new ECS order. `ZENITH_REGISTER_COMPONENT(ZM_Interactable, "ZM_Interactable", 113u)` (Zenithmon.cpp:161) and the `ZENITH_TOOLS` editor registration (Zenithmon.cpp:1411) are both untouched; game order 114 stays next-free; the existing `GateRoster_InteractableIsRegisteredExactlyOnce` unit keeps passing unmodified.

## Files to touch

### [create] C:\dev\Zenith\Games\Zenithmon\Source\Interaction\ZM_TrainerSightFsm.h

PURE layer. The sight state machine (ZM_TrainerSightFsm: states, transitions, the raise-confirm timer), the pure re-engagement gate ZM_MayTrainerEngage(row, flagSet, latchSet) implementing the adopted Q-2026-07-28-001 answer, and the process-global ZM_TrainerEngagementLatch. No ECS, no scene, no physics, no g_xEngine, no UI, no RNG, no allocation. Every function TOTAL and assert-free.

### [create] C:\dev\Zenith\Games\Zenithmon\Source\Interaction\ZM_TrainerSightFsm.cpp

Implementation of the pure FSM, the pure gate and the latch's single static u_int mask.

### [create] C:\dev\Zenith\Games\Zenithmon\Source\Interaction\ZM_TrainerSightProbe.h

The OCCLUSION FILTER, declared as one free function over two positions + two entity ids. Impure (it casts a ray) but scene-free and component-free, so a boot unit can drive the REAL raycast against a hermetically created static box. Documents the fail-open (no simulation) / fail-closed (non-finite) polarity and the single-body-ignore constraint.

### [create] C:\dev\Zenith\Games\Zenithmon\Source\Interaction\ZM_TrainerSightProbe.cpp

Implements ZM_ProbeTrainerSightLine via Zenith_PhysicsQuery::RaycastIgnoring, ignoring the trainer's body by id and treating a hit on the target's own entity as CLEAR.

### [modify] C:\dev\Zenith\Games\Zenithmon\Components\ZM_Interactable.h

Adds the two RUNTIME-ONLY members (m_eTrainerId, m_xSightFsm), the ConfigureTrainerSight setter, the IsTrainerSightEnabled/GetTrainerId observations, the two test seams (GetTrainerSightState/GetTrainerSightRaiseCount), and the private TickTrainerSight/UpdateWander split. Includes ZM_TrainerData.h + ZM_TrainerSightFsm.h. uSERIALIZATION_VERSION untouched at 2u.

### [modify] C:\dev\Zenith\Games\Zenithmon\Components\ZM_Interactable.cpp

The IMPURE GLUE, and the only impure part of SC6: OnUpdate becomes TickTrainerSight(dt) + UpdateWander(dt) (the walker body moved verbatim, so its behaviour is byte-identical). TickTrainerSight reads the two transforms, resolves the unique active-scene player, runs the pure cone FIRST, casts the occlusion ray ONLY on a cone pass, feeds the pure FSM, and on ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER marks the session latch and dispatches ZM_OnTrainerEncounter. OnStart resets the FSM and re-validates m_eTrainerId; ReadFromDataStream clears both new members. ConfigureTrainerSight fails closed.

### [modify] C:\dev\Zenith\Games\Zenithmon\Source\Interaction\ZM_InteractionRuntime.h

Promotes the private static TryResolveActivePlayerPose to a PUBLIC static TryResolveActivePlayer that also yields the player's Zenith_EntityID (which the occlusion probe needs to recognise the target's own capsule). The old surface is DELETED in the same commit, not kept alongside (no-legacy mandate). This is the one player-resolve seam in the game; SC6 does not hand-write a second.

### [modify] C:\dev\Zenith\Games\Zenithmon\Source\Interaction\ZM_InteractionRuntime.cpp

Renames/extends the resolver body (it already computes xPlayerID internally — it just did not hand it back) and updates its one internal caller at line 97.

### [modify] C:\dev\Zenith\Games\Zenithmon\Zenithmon.cpp

Adds ZM_TrainerEngagementLatch::ResetRuntimeStateForTests() to the between-tests hook (convention C3: the latch is ownerless process-global state, so batched tests would otherwise inherit it) plus the #include. NOTHING else: no new component, no new order, no editor registration, no scene authoring.

### [create] C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp

16 boot units, category ZM_Interaction: the FSM driven step by step with no engine, the pure re-engagement gate's two arms plus its roster anti-vacuity guard, the latch, and the Zenith_AssertCaptureScope totality proof.

### [create] C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightProbe.cpp

6 boot units, category ZM_Interaction: the REAL occlusion raycast against hermetically created static/dynamic bodies in a local PhysicsSceneScope fixture. Never touches terrain or any baked asset, so it is CI-valid.

### [modify] C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_Interactable.cpp

3 added boot units on the existing DetachedInteractable fixture: the trainer-sight defaults, the fail-closed ConfigureTrainerSight, and the SCENE-BYTE GUARD (Interactable_TrainerSightIsNotSerialized).

### [create] C:\dev\Zenith\Games\Zenithmon\Tests\ZM_AutoTests_TrainerSight.cpp

ZM_TrainerSightWalkUp_Test, m_bRequiresGraphics = false: walk into a runtime-placed trainer's cone in Dawnmere -> battle -> prize + defeat flag -> the defeated trainer never re-spots; then the flagless-latch arm proven end to end.

## Declarations (verbatim C++, paste-ready)

```cpp
// =============================================================================
// FILE 1 (new): Games/Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h
// =============================================================================
#pragma once

#include "Zenithmon/Source/Data/ZM_StoryFlags.h"    // ZM_STORY_FLAG_COUNT (the "row has a flag" test)
#include "Zenithmon/Source/Data/ZM_TrainerData.h"   // ZM_TrainerData / ZM_TRAINER_ID / ZM_IsRegisteredTrainer

// ============================================================================
// ZM_TrainerSightFsm (S7 item 3 SC6) -- the PURE trainer sight state machine and
// the PURE re-engagement gate. Plain values in, one action out: NO ECS, NO scene,
// NO physics, NO raycast, NO UI, NO g_xEngine, NO RNG, NO allocation, NO I/O.
// The impure half is exactly three things and lives in ZM_Interactable.cpp:
// reading the two transforms, casting the occlusion ray, and dispatching the
// event.
//
// It does NOT re-implement the sight cone. ZM_IsTargetInTrainerSightFromRotation
// (SC3) is the ONE cone in this game; its answer arrives here as a bool.
//
// EVERY function below is TOTAL and NEVER calls Zenith_Assert: the boot units
// feed it NaN deltas, negative tunings and the ZM_TRAINER_NONE sentinel on
// purpose, and an assert on a unit-supplied input does not fail one test -- it
// ends the whole boot unit run.
// ============================================================================

enum ZM_TRAINER_SIGHT_STATE : u_int
{
	// Eyes open. The only state that may raise.
	ZM_TRAINER_SIGHT_WATCHING = 0u,
	// Has raised once for THIS spotting and is deliberately silent. Re-arms only
	// when the target leaves sight, or when the raise is judged to have been
	// dropped (see m_fRaiseConfirmSeconds).
	ZM_TRAINER_SIGHT_ENGAGED,

	// NOT a state -- the walkable bound the totality unit iterates to.
	ZM_TRAINER_SIGHT_STATE_COUNT
};

enum ZM_TRAINER_SIGHT_ACTION : u_int
{
	ZM_TRAINER_SIGHT_ACTION_NONE = 0u,
	// Dispatch ZM_OnTrainerEncounter. Returned on EXACTLY ONE Step per spotting.
	ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER,

	ZM_TRAINER_SIGHT_ACTION_COUNT
};

// One tick's worth of world, BY VALUE. Each bool is answered by exactly one
// already-shipped seam, named here so the glue cannot invent a second source.
struct ZM_TrainerSightInputs
{
	// ZM_MayTrainerEngage(row, defeatFlagSet, sessionLatchSet).
	bool  m_bMayEngage      = false;
	// ZM_IsTargetInTrainerSightFromRotation(...) -- the SC3 pure cone. UNCHANGED.
	bool  m_bTargetInSight  = false;
	// ZM_ProbeTrainerSightLine(...).m_bClear -- the occlusion filter, consulted
	// ONLY when m_bTargetInSight already held (there is no raycast budget in this
	// engine, so cheap-gate-first IS the cost control).
	bool  m_bSightLineClear = false;
	// Something already owns the screen: ZM_BattleTransition::IsTransitionActive()
	// || ZM_GameStateManager::IsWarpInProgress() || ZM_UI_MenuStack::IsMenuOpen().
	// A busy channel DEFERS the raise (the trainer keeps staring); it never
	// consumes it, because Zenith_EventDispatcher::Dispatch returns void and a
	// refused raise is indistinguishable from an accepted one at the call site.
	bool  m_bChannelBusy    = false;
	float m_fDeltaSeconds   = 0.0f;
};

struct ZM_TrainerSightFsmTuning
{
	// How long an ENGAGED trainer waits for the battle channel to go BUSY before
	// concluding its raise was SILENTLY DROPPED and re-arming. This exists because
	// ZM_BattleTransition::IsTransitionActive() reports only s_bTransitionActive:
	// there is NO public accessor for s_bPendingEncounter /
	// s_bPendingTrainerEncounter, so a raise made in the same frame a wild
	// encounter latched is discarded with no trace. Without this window the
	// trainer would fall permanently silent while the player stood in the cone.
	// A non-finite or non-positive value disables the re-arm entirely (fail
	// closed: stay silent rather than raise every frame).
	float m_fRaiseConfirmSeconds = 0.5f;
};

class ZM_TrainerSightFsm
{
public:
	// The whole machine. Returns the action the caller must perform THIS tick.
	ZM_TRAINER_SIGHT_ACTION Step(const ZM_TrainerSightInputs& xInputs,
		const ZM_TrainerSightFsmTuning& xTuning);

	// Back to a cold watcher. Called from ZM_Interactable::OnStart and from
	// ConfigureTrainerSight, exactly where the walker resets m_xWalkerState.
	void Reset();

	ZM_TRAINER_SIGHT_STATE GetState() const { return m_eState; }
	// MONOTONIC count of raises actually emitted. Assert on THIS rather than on
	// the state alone: a machine stubbed to sit in ENGAGED would satisfy a state
	// check while never having raised anything.
	u_int GetRaiseCount() const { return m_uRaiseCount; }
	float GetConfirmElapsedSeconds() const { return m_fConfirmElapsed; }
	bool  IsRaiseConfirmed() const { return m_bRaiseConfirmed; }

private:
	ZM_TRAINER_SIGHT_STATE m_eState          = ZM_TRAINER_SIGHT_WATCHING;
	float                  m_fConfirmElapsed = 0.0f;
	bool                   m_bRaiseConfirmed = false;
	u_int                  m_uRaiseCount     = 0u;
};

// A stable short name for a state / action, for log lines and unit failure
// messages. TOTAL: never null, never indexes out of bounds ("UNKNOWN").
const char* ZM_TrainerSightStateName(ZM_TRAINER_SIGHT_STATE eState);
const char* ZM_TrainerSightActionName(ZM_TRAINER_SIGHT_ACTION eAction);

// ---- The re-engagement gate (the adopted answer to Q-2026-07-28-001) --------
//
// "May this trainer force a battle at all right now?", as a PURE function of the
// row and the two observations, so both arms are unit-testable with no game state.
//
//   * row NOT registered (the shared UNKNOWN row, or garbage) -> FALSE, closed.
//   * row HAS a defeat flag (m_eDefeatFlag < ZM_STORY_FLAG_COUNT) -> !bDefeatFlagSet.
//   * row has NO defeat flag (ZM_STORY_FLAG_NONE, as ZM_TRAINER_ROUTE1_RAMBLER's
//     is) -> !bSessionLatchSet.
//
// The second arm is the WHOLE point. ZM_IsStoryFlagSet(state, ZM_STORY_FLAG_NONE)
// returns false forever, silently (ZM_StoryFlags.cpp:100-117), so a flagless row
// gated on its flag alone is permanently "not yet defeated" and its prize is
// farmable. Do NOT reach for ZM_StoryGatePasses here: its NONE convention is the
// opposite ("never gated" == always passes), which is the wrong answer twice over.
//
// ASYMMETRY, DELIBERATE AND DOCUMENTED: a FLAGGED row ignores the latch, so
// losing to Vesper (a loss writes no flag -- ZM_BattleWriteBack.cpp:124-127 fails
// closed on anything but a WIN) leaves them re-battleable. That is the intended
// RPG behaviour: heal up and come back. A FLAGLESS row's latch is set on the
// RAISE, not on a win, because SC6 observes no battle outcome; it therefore means
// "this trainer has already forced a battle this session", which is exactly what
// its name says and is not farmable in either direction.
//
// TOTAL. Never asserts.
bool ZM_MayTrainerEngage(const ZM_TrainerData& xRow,
	bool bDefeatFlagSet,
	bool bSessionLatchSet);

// ---- The session latch -------------------------------------------------------
//
// OWNERLESS process-global state, one bit per ZM_TRAINER_ID, with static
// accessors -- NOT a per-component bool. A per-component latch would die with the
// scene, and Dawnmere is re-loaded SINGLE on every door round trip, so a flagless
// trainer would be re-farmable by walking into the player's house and back out.
//
// Because it is ownerless it MUST be cleared from the between-tests hook in
// Zenithmon.cpp (convention C3); the harness's scene-0 force-reload cannot reach it.
class ZM_TrainerEngagementLatch
{
public:
	// TOTAL and SILENT: an unregistered id (including the ZM_TRAINER_NONE
	// sentinel) is inert -- no bit is set, nothing is logged, nothing asserts.
	static void MarkEngaged(ZM_TRAINER_ID eTrainer);
	static bool HasEngaged(ZM_TRAINER_ID eTrainer);
	static void ResetRuntimeStateForTests();
	// Raw mask, for the unit that proves an unregistered MarkEngaged sets NO bit
	// at all (asserting only on HasEngaged could not see a stray high bit).
	static u_int GetEngagedMaskForTests();

private:
	static u_int s_uEngagedMask;
};

static_assert((u_int)ZM_TRAINER_COUNT <= 32u,
	"ZM_TrainerEngagementLatch stores one bit per trainer in a single u_int");


// =============================================================================
// FILE 2 (new): Games/Zenithmon/Source/Interaction/ZM_TrainerSightFsm.cpp
// The three load-bearing bodies, verbatim. (Zenith.h include + the two Name
// switches omitted here only for brevity; they follow the ZM_NpcRaiseKindName
// shape exactly.)
// =============================================================================

u_int ZM_TrainerEngagementLatch::s_uEngagedMask = 0u;

void ZM_TrainerEngagementLatch::MarkEngaged(ZM_TRAINER_ID eTrainer)
{
	if (!ZM_IsRegisteredTrainer(eTrainer))
	{
		return;
	}
	s_uEngagedMask |= (1u << (u_int)eTrainer);
}

bool ZM_TrainerEngagementLatch::HasEngaged(ZM_TRAINER_ID eTrainer)
{
	if (!ZM_IsRegisteredTrainer(eTrainer))
	{
		return false;
	}
	return (s_uEngagedMask & (1u << (u_int)eTrainer)) != 0u;
}

void ZM_TrainerEngagementLatch::ResetRuntimeStateForTests()
{
	s_uEngagedMask = 0u;
}

u_int ZM_TrainerEngagementLatch::GetEngagedMaskForTests()
{
	return s_uEngagedMask;
}

bool ZM_MayTrainerEngage(const ZM_TrainerData& xRow,
	bool bDefeatFlagSet,
	bool bSessionLatchSet)
{
	// The UNKNOWN row ZM_GetTrainerData hands back for a bad id carries
	// ZM_TRAINER_NONE, so this one comparison rejects it and every hand-built
	// garbage row together. Fail CLOSED: an unauthored trainer never battles.
	if (!ZM_IsRegisteredTrainer(xRow.m_eId))
	{
		return false;
	}
	// Spelled inline rather than via ZM_StoryFlags' registered-flag helper, which
	// is file-local to ZM_StoryFlags.cpp. ZM_STORY_FLAG_NONE aliases
	// ZM_STORY_FLAG_COUNT, so this rejects the sentinel and garbage together.
	if ((u_int)xRow.m_eDefeatFlag < (u_int)ZM_STORY_FLAG_COUNT)
	{
		return !bDefeatFlagSet;
	}
	return !bSessionLatchSet;
}

void ZM_TrainerSightFsm::Reset()
{
	m_eState = ZM_TRAINER_SIGHT_WATCHING;
	m_fConfirmElapsed = 0.0f;
	m_bRaiseConfirmed = false;
	m_uRaiseCount = 0u;
}

ZM_TRAINER_SIGHT_ACTION ZM_TrainerSightFsm::Step(const ZM_TrainerSightInputs& xInputs,
	const ZM_TrainerSightFsmTuning& xTuning)
{
	// SIGHT is the CONJUNCTION: inside the pure cone AND with an unblocked line.
	// This single expression is where occlusion enters the decision.
	const bool bSees = xInputs.m_bTargetInSight && xInputs.m_bSightLineClear;

	switch (m_eState)
	{
	case ZM_TRAINER_SIGHT_WATCHING:
		if (!bSees)
		{
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		if (!xInputs.m_bMayEngage)
		{
			// Already defeated (flagged row) or already battled this session
			// (flagless row). Stay WATCHING: the gate is re-read every tick, so a
			// new game or a test reset re-arms without any extra transition.
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		if (xInputs.m_bChannelBusy)
		{
			// DEFER, do not consume. Raising into a busy channel is silently
			// dropped, and the trainer would then sit ENGAGED having achieved
			// nothing.
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		m_eState = ZM_TRAINER_SIGHT_ENGAGED;
		m_fConfirmElapsed = 0.0f;
		m_bRaiseConfirmed = false;
		++m_uRaiseCount;
		return ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER;

	case ZM_TRAINER_SIGHT_ENGAGED:
		if (xInputs.m_bChannelBusy)
		{
			// The screen went busy after our raise: it TOOK. Latched, because the
			// channel goes idle again at the end of the round trip and we must not
			// then read that idleness as "the raise was dropped".
			m_bRaiseConfirmed = true;
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		if (!bSees)
		{
			// The target left the cone (or stepped behind cover): re-arm. This is
			// the rising-edge discipline ZM_WarpTrigger's overlap latch and
			// ZM_TallGrassSystem's tile transition already use -- one raise per
			// continuous spotting, never one per frame.
			m_eState = ZM_TRAINER_SIGHT_WATCHING;
			return ZM_TRAINER_SIGHT_ACTION_NONE;
		}
		if (!m_bRaiseConfirmed)
		{
			// A non-finite or non-positive dt contributes NOTHING, so the
			// accumulator can never go NaN and the re-arm can never fire on a
			// garbage frame.
			if (std::isfinite(xInputs.m_fDeltaSeconds) && xInputs.m_fDeltaSeconds > 0.0f)
			{
				m_fConfirmElapsed += xInputs.m_fDeltaSeconds;
			}
			// A degenerate window disables the re-arm entirely (fail closed).
			if (std::isfinite(xTuning.m_fRaiseConfirmSeconds)
				&& xTuning.m_fRaiseConfirmSeconds > 0.0f
				&& m_fConfirmElapsed >= xTuning.m_fRaiseConfirmSeconds)
			{
				m_eState = ZM_TRAINER_SIGHT_WATCHING;
			}
		}
		return ZM_TRAINER_SIGHT_ACTION_NONE;

	default:
		// Unreachable for a value this class can hold; fail closed rather than
		// assert (this runs inside the boot unit suite).
		m_eState = ZM_TRAINER_SIGHT_WATCHING;
		return ZM_TRAINER_SIGHT_ACTION_NONE;
	}
}


// =============================================================================
// FILE 3 (new): Games/Zenithmon/Source/Interaction/ZM_TrainerSightProbe.h
// =============================================================================
#pragma once

#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Entity.h"   // Zenith_EntityID

// ============================================================================
// ZM_TrainerSightProbe (S7 item 3 SC6) -- THE OCCLUSION FILTER. One free
// function, deliberately NOT a member of anything: it takes two world positions
// and two entity ids, so a boot unit can drive the REAL raycast against a
// hermetically created static box with no scene, no player and no component.
//
// It is consulted ONLY AFTER ZM_IsTargetInTrainerSightFromRotation has already
// passed. That ordering is the entire cost control: there is NO raycast budget or
// throttle anywhere in this engine (Zenith/AI/CLAUDE.md's "raycast budget" claim
// is documentation-only and not implemented).
//
// THE SINGLE-BODY-IGNORE CONSTRAINT, and how it is handled. The engine offers
// exactly one filter -- Zenith_PhysicsQuery::RaycastIgnoring's one entity id --
// and no layer/mask. So the TRAINER is filtered by id, and the PLAYER's own
// capsule (which necessarily terminates the ray at the far end, because Jolt
// treats convex shapes as solid) is recognised EXACTLY by comparing
// RaycastResult::m_xHitEntity against the target id. That is exact and needs no
// magic distance tolerance, unlike Zenith_PerceptionSystem::CheckLineOfSight's
// hard-coded 0.5 m.
//
// TERRAIN CAVEAT, binding on every test: Dawnmere's greybox shell, door jambs,
// lintel, NPC AABBs and the warp trigger are real static bodies EVERYWHERE
// (they live in the committed Dawnmere.zscen). The TERRAIN collider is built
// from gitignored, uncommitted Assets/Terrain/Dawnmere/Physics_*.zmesh, so on a
// fresh CI checkout terrain occludes NOTHING. Never assert occlusion against
// terrain -- assert it against an explicitly created box.
//
// TOTAL. Never calls Zenith_Assert.
// ============================================================================

struct ZM_TrainerSightProbeResult
{
	// The answer the FSM consumes.
	bool            m_bClear            = false;
	// False when there is no live simulation. Exposed so a test can PROVE a
	// "clear" answer did not come from the fail-open branch below -- without it
	// every occlusion assertion would be vacuous in a physics-less context.
	bool            m_bPhysicsAvailable = false;
	// A body OTHER than the target stopped the ray.
	bool            m_bBlockerHit       = false;
	Zenith_EntityID m_xBlockerEntityID  = INVALID_ENTITY_ID;
	float           m_fBlockerDistance  = 0.0f;
};

// ANSWER TABLE, in evaluation order (the order IS the specification):
//   1. Any non-finite position component      -> m_bClear = FALSE. FAIL CLOSED,
//      matching ZM_IsTargetInTrainerSight's own totality rule: one body that goes
//      non-finite must not hand every trainer a free line of sight.
//   2. Coincident (separation^2 <= fZM_INTERACT_DEGENERATE_LEN_SQ, the ONE
//      degenerate epsilon this game has) -> m_bClear = TRUE, no ray cast. There
//      is nothing between two points at the same place, and this matches the
//      cone predicate's own coincident carve-out.
//   3. !Zenith_Physics::HasActiveSimulation() -> m_bPhysicsAvailable = FALSE and
//      m_bClear = TRUE. FAIL OPEN, deliberately: a world with no physics has no
//      occluders, and failing closed would blind every trainer in every
//      physics-less context. This is the ONE place the two polarities differ, and
//      it is why m_bPhysicsAvailable is reported.
//   4. No hit                                  -> m_bClear = TRUE.
//   5. Hit whose m_xHitEntity == xTargetEntityID (and the id is valid)
//                                              -> m_bClear = TRUE (that is the
//      target's own body, not an occluder).
//   6. Otherwise -> m_bClear = FALSE, m_bBlockerHit = TRUE, blocker id + distance
//      recorded for the failure message.
ZM_TrainerSightProbeResult ZM_ProbeTrainerSightLine(
	const Zenith_Maths::Vector3& xTrainerPosition,
	Zenith_EntityID xTrainerEntityID,
	const Zenith_Maths::Vector3& xTargetPosition,
	Zenith_EntityID xTargetEntityID);


// =============================================================================
// FILE 4 (new): Games/Zenithmon/Source/Interaction/ZM_TrainerSightProbe.cpp
// The one body, verbatim.
// =============================================================================
ZM_TrainerSightProbeResult ZM_ProbeTrainerSightLine(
	const Zenith_Maths::Vector3& xTrainerPosition,
	Zenith_EntityID xTrainerEntityID,
	const Zenith_Maths::Vector3& xTargetPosition,
	Zenith_EntityID xTargetEntityID)
{
	ZM_TrainerSightProbeResult xResult;

	if (!ZM_IsFiniteVector3(xTrainerPosition) || !ZM_IsFiniteVector3(xTargetPosition))
	{
		xResult.m_bClear = false;
		return xResult;
	}

	const Zenith_Maths::Vector3 xSeparation = xTargetPosition - xTrainerPosition;
	const float fSeparationSq = xSeparation.x * xSeparation.x
		+ xSeparation.y * xSeparation.y
		+ xSeparation.z * xSeparation.z;
	if (fSeparationSq <= fZM_INTERACT_DEGENERATE_LEN_SQ)
	{
		xResult.m_bClear = true;
		return xResult;
	}

	Zenith_Physics& xPhysics = g_xEngine.Physics();
	xResult.m_bPhysicsAvailable = xPhysics.HasActiveSimulation();
	if (!xResult.m_bPhysicsAvailable)
	{
		xResult.m_bClear = true;   // FAIL OPEN -- see the header's answer table
		return xResult;
	}

	const float fSeparation = std::sqrt(fSeparationSq);
	const Zenith_Physics::RaycastResult xHit = Zenith_PhysicsQuery::RaycastIgnoring(
		xTrainerPosition, xSeparation, fSeparation, xTrainerEntityID);
	if (!xHit.m_bHit)
	{
		xResult.m_bClear = true;
		return xResult;
	}
	if (xTargetEntityID != INVALID_ENTITY_ID && xHit.m_xHitEntity == xTargetEntityID)
	{
		// The ray terminated on the TARGET's own body. Only one body can be
		// filtered per cast, so this comparison is how the far end is excused.
		xResult.m_bClear = true;
		return xResult;
	}

	xResult.m_bClear = false;
	xResult.m_bBlockerHit = true;
	xResult.m_xBlockerEntityID = xHit.m_xHitEntity;
	xResult.m_fBlockerDistance = xHit.m_fDistance;
	return xResult;
}


// =============================================================================
// FILE 5 (modify): Games/Zenithmon/Components/ZM_Interactable.h
// =============================================================================
// -- added includes (beside the existing four) --
#include "Zenithmon/Source/Data/ZM_TrainerData.h"                 // ZM_TRAINER_ID
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"      // the by-value FSM

// -- added to the PUBLIC section, after the wander block --

	// Install (or clear) this NPC's trainer identity. Fails CLOSED exactly as
	// SetNpcId does: an unregistered id stores ZM_TRAINER_NONE rather than keeping
	// the previous row, so a bad authoring value yields a blind NPC and never the
	// WRONG trainer's battle. Always resets the sight machine.
	//
	// ★ RUNTIME-ONLY, AND THAT IS DELIBERATE. m_eTrainerId is NOT serialized and
	// uSERIALIZATION_VERSION deliberately stays at 2u, so SC6 changes ZERO bytes
	// in the five committed ZM_Interactable payloads inside Dawnmere.zscen and a
	// boot still must not leave any scene modified in git status. SC6 ships the
	// BEHAVIOUR; SC8, which places the authored trainer, owns persistence and
	// picks one of two routes: (1) ZERO-BYTE -- add a ZM_TRAINER_ID column AT THE
	// END of ZM_NpcData's row and derive this from the already-serialized
	// m_eNpcId in OnStart; or (2) bump uSERIALIZATION_VERSION to 3u, append the
	// block after the walker block gated by `if (uVersion == 2u) { return; }`
	// exactly as v1->v2 did, and re-bake + re-commit Dawnmere.zscen IN THE SAME
	// COMMIT. Do not do either here.
	bool ConfigureTrainerSight(ZM_TRAINER_ID eTrainer);
	ZM_TRAINER_ID GetTrainerId() const { return m_eTrainerId; }
	// The live "is this NPC a trainer that can spot anyone?" answer.
	// ZM_IsRegisteredTrainer collapses the sentinel and every garbage value into
	// one comparison, so unlike IsInteractable there is no second flag to keep in
	// sync -- the id IS the enable.
	bool IsTrainerSightEnabled() const { return ZM_IsRegisteredTrainer(m_eTrainerId); }

	// ---- test/tools observation ----
	ZM_TRAINER_SIGHT_STATE GetTrainerSightState() const { return m_xSightFsm.GetState(); }
	// Monotonic raises emitted by THIS component. The windowed gate asserts on
	// this, not on "a battle happened", so a trainer that fired twice cannot pass.
	u_int GetTrainerSightRaiseCount() const { return m_xSightFsm.GetRaiseCount(); }

// -- added to the PRIVATE section --

	// OnUpdate is now these two, in this order. The walker body is UNCHANGED --
	// it moved verbatim into UpdateWander -- so its behaviour is byte-identical.
	// The sight tick must run FIRST and OUTSIDE the walker's `if (!m_bWanderEnabled)
	// return;` early-out, or a stationary trainer (which is what SC8 authors) would
	// never see anything.
	void TickTrainerSight(float fDeltaTime);
	void UpdateWander(float fDeltaTime);

	// Session-only, NEVER serialized -- the ZM_WalkerState precedent. A scene
	// reload restarts a cold watcher deterministically.
	ZM_TRAINER_ID      m_eTrainerId = ZM_TRAINER_NONE;
	ZM_TrainerSightFsm m_xSightFsm;


// =============================================================================
// FILE 6 (modify): Games/Zenithmon/Components/ZM_Interactable.cpp
// The new impure glue, verbatim. (New includes: ZM_BattleTransition.h,
// ZM_UI_MenuStack.h [already present], ZenithECS/Zenith_EventSystem.h,
// Zenithmon/Source/World/ZM_EncounterEvents.h, ZM_WorldSpec.h,
// ZM_InteractionRuntime.h, ZM_TrainerSightLogic.h, ZM_TrainerSightProbe.h.)
// =============================================================================

// -- in the anonymous namespace. The SIXTH local instance of this three-line
// -- lookup (ZM_TallGrassSystem, ZM_UI_MenuStack, ZM_BattleTransition x2,
// -- ZM_GameStateManager already each hold one); it is a lookup, not a decision,
// -- and promoting it would be a new API for no gain.
	ZM_SCENE_ID ZM_ResolveActiveSceneIdForSight()
	{
		const Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
		const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(xActiveScene);
		if (!xInfo.m_bLoaded || xInfo.m_iBuildIndex < 0)
		{
			return ZM_SCENE_NONE;
		}
		return ZM_FindSceneByBuildIndex(static_cast<u_int>(xInfo.m_iBuildIndex));
	}

void ZM_Interactable::OnUpdate(float fDeltaTime)
{
	TickTrainerSight(fDeltaTime);
	UpdateWander(fDeltaTime);
}

bool ZM_Interactable::ConfigureTrainerSight(ZM_TRAINER_ID eTrainer)
{
	m_xSightFsm.Reset();
	if (!ZM_IsRegisteredTrainer(eTrainer))
	{
		m_eTrainerId = ZM_TRAINER_NONE;
		return false;
	}
	m_eTrainerId = eTrainer;
	return true;
}

void ZM_Interactable::TickTrainerSight(float fDeltaTime)
{
	if (!IsTrainerSightEnabled())
	{
		return;
	}

	const ZM_TrainerData& xRow = ZM_GetTrainerData(m_eTrainerId);

	// The gate's two observations. No reachable game state means NOTHING HAS
	// HAPPENED YET -- the identical ruling Interact() already makes for story-gated
	// dialogue. Every defeat flag then reads clear, so a flagged trainer IS
	// engageable, which is exactly the answer a fresh save gives.
	ZM_GameState* pxGameState = nullptr;
	const bool bHasGameState =
		ZM_GameStateManager::TryGetGameState(pxGameState) && pxGameState != nullptr;
	const bool bDefeatFlagSet =
		bHasGameState && ZM_IsStoryFlagSet(*pxGameState, xRow.m_eDefeatFlag);
	const bool bLatchSet = ZM_TrainerEngagementLatch::HasEngaged(m_eTrainerId);

	ZM_TrainerSightInputs xInputs;
	xInputs.m_fDeltaSeconds = fDeltaTime;
	xInputs.m_bMayEngage = ZM_MayTrainerEngage(xRow, bDefeatFlagSet, bLatchSet);
	// THE PRECONDITION ACCESSORS. IsTransitionActive() is the only public window
	// onto the battle machine (there is NO accessor for its two pending latches),
	// so the FSM additionally treats a raise as unconfirmed until the channel is
	// observed BUSY -- see ZM_TrainerSightFsmTuning::m_fRaiseConfirmSeconds.
	xInputs.m_bChannelBusy = ZM_BattleTransition::IsTransitionActive()
		|| ZM_GameStateManager::IsWarpInProgress()
		|| ZM_UI_MenuStack::IsMenuOpen();

	Zenith_TransformComponent* pxTransform =
		m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
	Zenith_EntityID xPlayerID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 xPlayerPosition(0.0f);
	Zenith_Maths::Quat xPlayerRotation(1.0f, 0.0f, 0.0f, 0.0f);
	const bool bHavePlayer = ZM_InteractionRuntime::TryResolveActivePlayer(
		xPlayerID, xPlayerPosition, xPlayerRotation);
	if (pxTransform != nullptr && bHavePlayer)
	{
		Zenith_Maths::Vector3 xTrainerPosition(0.0f);
		Zenith_Maths::Quat xTrainerRotation(1.0f, 0.0f, 0.0f, 0.0f);
		pxTransform->GetPosition(xTrainerPosition);
		pxTransform->GetRotation(xTrainerRotation);

		// The SC3 PURE cone, unmodified and unduplicated. Rotation form, never
		// glm::eulerAngles(quat).y.
		xInputs.m_bTargetInSight = ZM_IsTargetInTrainerSightFromRotation(
			xTrainerPosition, xTrainerRotation, xPlayerPosition,
			ZM_TrainerSightTuning{});
		if (xInputs.m_bTargetInSight)
		{
			// The ray enters HERE and only here, AFTER the pure cone passed.
			const ZM_TrainerSightProbeResult xProbe = ZM_ProbeTrainerSightLine(
				xTrainerPosition, m_xParentEntity.GetEntityID(),
				xPlayerPosition, xPlayerID);
			xInputs.m_bSightLineClear = xProbe.m_bClear;
		}
	}

	if (m_xSightFsm.Step(xInputs, ZM_TrainerSightFsmTuning{})
		!= ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER)
	{
		return;
	}

	// Latch BEFORE dispatching: Dispatch is SYNCHRONOUS (the subscriber runs
	// inside this stack frame), so the latch must already be true if anything
	// downstream ever re-enters this component.
	ZM_TrainerEngagementLatch::MarkEngaged(m_eTrainerId);
	// This component does NOT freeze the player and does NOT push dialogue. The
	// shipped subscriber owns both: ZM_BattleTransition::OnTrainerEncounterEvent
	// latches, and TryParkOverworldPlayer zeroes velocity, drops gravity and calls
	// SetMovementEnabled(false). Adding a second freeze owner here would fight the
	// menu/warp/battle protocol those three already coordinate over.
	Zenith_EventDispatcher::Get().Dispatch(
		ZM_OnTrainerEncounter{ m_eTrainerId, ZM_ResolveActiveSceneIdForSight() });
}

// -- inside OnStart, beside the existing `m_xWalkerState = ZM_WalkerState{};` --
	m_xSightFsm.Reset();
	if (!ZM_IsRegisteredTrainer(m_eTrainerId))
	{
		m_eTrainerId = ZM_TRAINER_NONE;
	}

// -- inside ReadFromDataStream, in the unconditional reset block --
	m_eTrainerId = ZM_TRAINER_NONE;
	m_xSightFsm.Reset();


// =============================================================================
// FILE 7 (modify): Games/Zenithmon/Source/Interaction/ZM_InteractionRuntime.h
// The private static is PROMOTED and DELETED in the same edit (no-legacy
// mandate). It already resolved the id internally -- it just did not hand it back.
// =============================================================================
// -- moved into the PUBLIC section, replacing the private TryResolveActivePlayerPose --

	// The unique active-scene player's id and pose. THE one player-resolve seam
	// this feature area has: SC6's occlusion probe needs the ID (to recognise the
	// player's own capsule as the ray's legitimate terminator) as well as the pose.
	//
	// Deliberately NOT ZM_GameStateManager::TryGetUniqueActiveScenePlayerEntityID:
	// that seam additionally demands a live physics body, which would make the
	// whole thing unobservable headlessly.
	static bool TryResolveActivePlayer(Zenith_EntityID& xEntityIDOut,
		Zenith_Maths::Vector3& xPositionOut,
		Zenith_Maths::Quat& xRotationOut);


// =============================================================================
// FILE 8 (modify): Games/Zenithmon/Zenithmon.cpp -- between-tests hook only
// =============================================================================
		// SC6's session latch is ownerless process-global state, so the harness's
		// scene-0 force-reload cannot clear it: without this, one test's engaged
		// flagless trainer would silence him for every later batched test.
		ZM_TrainerEngagementLatch::ResetRuntimeStateForTests();
```

## Tests to author

### `Fsm_ColdWatcherRaisesExactlyOnceOnFirstSighting`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Build inputs {mayEngage=true, inSight=true, lineClear=true, channelBusy=false, dt=1/60}. Step 1 returns ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER, GetState()==ZM_TRAINER_SIGHT_ENGAGED, GetRaiseCount()==1u. Steps 2..30 with the IDENTICAL inputs each return ZM_TRAINER_SIGHT_ACTION_NONE and GetRaiseCount() stays 1u. This is the 'spotted once, not once per frame' clause.
* MUTATION THAT MUST RED IT: In ZM_TrainerSightFsm::Step's WATCHING arm, delete the line `m_eState = ZM_TRAINER_SIGHT_ENGAGED;`. Compiles cleanly (no parameter goes unreferenced, nothing becomes unreachable); step 2 then raises again and GetRaiseCount()==31u.

### `Fsm_DefeatedTrainerIsNeverSpotted`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Inputs {mayEngage=FALSE, inSight=true, lineClear=true, channelBusy=false}. Drive 30 Steps: every one returns ACTION_NONE, GetState() stays ZM_TRAINER_SIGHT_WATCHING, GetRaiseCount()==0u. Then flip mayEngage=true and Step once: RAISE_ENCOUNTER and count 1 -- so the test proves the gate blocked, not that the fixture was inert.
* MUTATION THAT MUST RED IT: Change the WATCHING guard `if (!xInputs.m_bMayEngage)` to `if (!xInputs.m_bMayEngage && xInputs.m_bChannelBusy)`. Both members stay referenced so it compiles under warnings-as-errors; the defeated trainer then raises on step 1.

### `Fsm_OccludedTargetInsideTheConeIsNotSpotted`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Inputs {mayEngage=true, inSight=TRUE, lineClear=FALSE}: 30 Steps all ACTION_NONE, count 0, state WATCHING. Then lineClear=true: RAISE on the next Step, count 1. Pins that occlusion is ANDed into sight rather than decorative.
* MUTATION THAT MUST RED IT: Change `const bool bSees = xInputs.m_bTargetInSight && xInputs.m_bSightLineClear;` to `... || xInputs.m_bSightLineClear;`. Compiles; the blocked trainer raises immediately.

### `Fsm_LeavingSightRearmsTheWatcher`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Raise once (count 1, ENGAGED). Set inSight=false and Step: state returns to ZM_TRAINER_SIGHT_WATCHING, action NONE, count still 1. Set inSight=true and Step: RAISE_ENCOUNTER, count 2. One raise per continuous spotting, and a genuinely new spotting still fires.
* MUTATION THAT MUST RED IT: In the ENGAGED arm delete `m_eState = ZM_TRAINER_SIGHT_WATCHING;` from the `if (!bSees)` branch (keep the `return`). Compiles; the second entry never raises and count stays 1.

### `Fsm_BusyChannelDefersTheRaiseRatherThanConsumingIt`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Inputs all passing but channelBusy=TRUE: 30 Steps all ACTION_NONE, state stays WATCHING (NOT ENGAGED), count 0. Then channelBusy=false: RAISE, count 1. Pins that a raise into a busy screen is never emitted AND never silently burnt.
* MUTATION THAT MUST RED IT: Delete the `if (xInputs.m_bChannelBusy) { return ZM_TRAINER_SIGHT_ACTION_NONE; }` guard from the WATCHING arm. Compiles (the member is still read in the ENGAGED arm); the FSM raises on step 1 while the screen is busy.

### `Fsm_UnconfirmedRaiseRearmsAfterTheConfirmWindow`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Raise once with channelBusy=false. Keep sight true and channelBusy FALSE (simulating a silently DROPPED dispatch) and Step with dt=1/60 until elapsed exceeds the default 0.5s: state returns to WATCHING and the next Step raises again (count 2). Also assert that before the window elapses the count is still 1 -- the re-arm must be timed, not immediate.
* MUTATION THAT MUST RED IT: Change `if (xInputs.m_bChannelBusy) { m_bRaiseConfirmed = true; ... }` to `if (!xInputs.m_bChannelBusy) { m_bRaiseConfirmed = true; ... }`. Compiles; the raise is confirmed on the very frame that should have started the drop timer, so count stays 1 forever.

### `Fsm_ConfirmedRaiseNeverRearmsWhileTheTargetStaysInSight`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Raise; then ONE Step with channelBusy=true (the round trip started, so the raise is confirmed); then 600 Steps with channelBusy=false, sight still true, dt=1/60 (10 simulated seconds -- the player restored inside the cone after the battle). GetRaiseCount() stays 1u and IsRaiseConfirmed() is true throughout. This is the 'a completed battle does not immediately re-fire' clause.
* MUTATION THAT MUST RED IT: Delete the `m_bRaiseConfirmed = true;` assignment from the ENGAGED arm. Compiles (m_bRaiseConfirmed is still read below and cleared on raise); the confirm timer then expires and count reaches 2.

### `Fsm_DegenerateDeltaNeverAccumulatesOrRearms`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Raise, leave unconfirmed, then Step 1000 times feeding dt values NaN, +inf, -inf, -1.0f and 0.0f in rotation: GetConfirmElapsedSeconds() is std::isfinite AND exactly 0.0f, GetState() is still ENGAGED, GetRaiseCount() is still 1u.
* MUTATION THAT MUST RED IT: Remove the finite/positive guard so the line reads `m_fConfirmElapsed += xInputs.m_fDeltaSeconds;` unconditionally. Compiles; elapsed goes NaN and the isfinite assertion reds (note: asserting only on the state would NOT have redded, because `NaN >= 0.5f` is false -- which is exactly why the assertion is on finiteness).

### `Fsm_ResetReturnsAColdWatcher`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Raise (count 1, ENGAGED, elapsed advanced), call Reset(): GetState()==WATCHING, GetRaiseCount()==0u, GetConfirmElapsedSeconds()==0.0f, IsRaiseConfirmed()==false. Non-vacuous because it asserts the populated state first.
* MUTATION THAT MUST RED IT: In Reset(), delete `m_uRaiseCount = 0u;`. Compiles; the post-reset count assertion reds.

### `Fsm_StepNeverAssertsOnAnyDegenerateInput`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Inside a Zenith_AssertCaptureScope (with NO ZENITH_ASSERT_* inside the scope, and the hit count copied to a local BEFORE the closing brace), drive Step over the cross product of all four bools, dt in {NaN, +inf, -inf, -1, 0, 1/60} and tunings with m_fRaiseConfirmSeconds in {NaN, -1, 0, 0.5}. Outside the scope: ZENITH_ASSERT_EQ(uHits, 0u, ...) plus DEFINED-answer checks (a fully-passing cold Step still returns RAISE; a mayEngage=false Step still returns NONE) so this is not merely 'nothing crashed'.
* MUTATION THAT MUST RED IT: Add `Zenith_Assert(std::isfinite(xInputs.m_fDeltaSeconds), "dt");` at the top of Step. Compiles; the hit count becomes non-zero. (This unit's real job is to stop such a line ever landing -- one assert here would end the entire boot unit run, not fail one test.)

### `Gate_FlaggedRowKeysOnItsDefeatFlagAndIgnoresTheLatch`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: xRow = ZM_GetTrainerData(ZM_TRAINER_RIVAL_VESPER). ZM_MayTrainerEngage(xRow, false, false)==true; (xRow, false, TRUE)==true (the latch must NOT gate a flagged row); (xRow, TRUE, false)==false; (xRow, TRUE, TRUE)==false. All four corners.
* MUTATION THAT MUST RED IT: Swap the two arms of ZM_MayTrainerEngage so the flagged branch returns `!bSessionLatchSet` and the flagless branch returns `!bDefeatFlagSet`. Both parameters stay referenced, so it compiles; the (false, TRUE) and (TRUE, false) corners both flip.

### `Gate_FlaglessRowKeysOnTheSessionLatchAndIgnoresTheFlag`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: xRow = ZM_GetTrainerData(ZM_TRAINER_ROUTE1_RAMBLER), whose m_eDefeatFlag IS ZM_STORY_FLAG_NONE. ZM_MayTrainerEngage(xRow, false, false)==true; (xRow, TRUE, false)==true (a flag input is meaningless for this row and must not gate it); (xRow, false, TRUE)==FALSE -- the 'already battled this session -> no spot' clause; (xRow, TRUE, TRUE)==false. Plus an explicit ZENITH_ASSERT_EQ that ZM_IsStoryFlagSet(ZM_StoryFlagSet{}, xRow.m_eDefeatFlag) is false, documenting in the test itself WHY a flag-only gate could never work here.
* MUTATION THAT MUST RED IT: The same swapped-arms mutation as the previous unit (it is two-sided). A per-test alternative: change the flagless branch to `return true;` -- the parameter list is unchanged so it compiles, and the (false, TRUE) corner reds.

### `Gate_UnregisteredRowFailsClosed`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: ZM_MayTrainerEngage(ZM_GetTrainerData(ZM_TRAINER_NONE), false, false)==false and (..., false, true)==false. ZM_GetTrainerData logs a non-fatal Zenith_Error for the sentinel and returns the shared UNKNOWN row -- it does NOT assert (ZM_TrainerData.h:36-39), so this unit is safe to run at boot.
* MUTATION THAT MUST RED IT: Delete the `if (!ZM_IsRegisteredTrainer(xRow.m_eId)) { return false; }` guard. Compiles (xRow is still read below); the UNKNOWN row carries ZM_STORY_FLAG_NONE, so it falls into the flagless arm and answers true.

### `Gate_ShippedRosterStillExercisesBothArms`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Walk the whole roster 0..ZM_GetTrainerCount(): assert ZM_GetTrainerData(ZM_TRAINER_RIVAL_VESPER).m_eDefeatFlag == ZM_STORY_FLAG_RIVAL1_DEFEATED (a REGISTERED flag), and ZM_GetTrainerData(ZM_TRAINER_ROUTE1_RAMBLER).m_eDefeatFlag == ZM_STORY_FLAG_NONE, and that at least one row of each kind exists. The anti-vacuity guard demanded by Status.md's 'adding a data row can disarm an existing test' rule: if a future author gives the rambler a flag, the two arm units above silently stop testing different arms, and this unit says so loudly instead.
* MUTATION THAT MUST RED IT: In ZM_TrainerData.cpp change the ZM_TRAINER_ROUTE1_RAMBLER row's flag column from ZM_STORY_FLAG_NONE to ZM_STORY_FLAG_ROUTE1_OPEN. Compiles; this unit reds with a message telling the author to point the flagless fixture at a genuinely flagless row.

### `Latch_MarkIsPerTrainerAndResetClearsEverything`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: ResetRuntimeStateForTests(); HasEngaged(VESPER)==false && HasEngaged(RAMBLER)==false && GetEngagedMaskForTests()==0u. MarkEngaged(RAMBLER): HasEngaged(RAMBLER)==true AND HasEngaged(VESPER)==FALSE (one trainer's engagement must not silence another). ResetRuntimeStateForTests(): both false, mask 0u. The test resets on entry AND exit so it cannot leak into the rest of the boot suite.
* MUTATION THAT MUST RED IT: Change MarkEngaged's body to `s_uEngagedMask |= (1u << ((u_int)eTrainer + 1u));`. The parameter stays referenced so it compiles; marking the RAMBLER (id 1) then sets bit 2 and the HasEngaged(RAMBLER)==true assertion reds.

### `Latch_UnregisteredIdIsInertAndSilent`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightFsm.cpp
* ASSERTS: Inside a Zenith_AssertCaptureScope: MarkEngaged(ZM_TRAINER_NONE) and MarkEngaged((ZM_TRAINER_ID)9999u), plus HasEngaged on both. Outside the scope: hit count == 0u, GetEngagedMaskForTests()==0u (asserting on the raw mask, not just HasEngaged, so a stray high bit cannot hide), and HasEngaged(ZM_TRAINER_NONE)==false.
* MUTATION THAT MUST RED IT: Delete the `if (!ZM_IsRegisteredTrainer(eTrainer)) { return; }` guard from MarkEngaged. Compiles; the sentinel sets bit ZM_TRAINER_COUNT and the mask assertion reds.

### `Probe_ClearLineIsClearAndPhysicsWasActuallyConsulted`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightProbe.cpp
* ASSERTS: In a local PhysicsSceneScope (the ZM_Tests_Overworld.cpp:87-154 fixture, copied file-locally because the original is in that file's anonymous namespace) with NO bodies created: ZM_ProbeTrainerSightLine({0,0,0}, INVALID_ENTITY_ID, {0,0,6}, INVALID_ENTITY_ID) gives m_bClear==true, m_bBlockerHit==false, m_xBlockerEntityID==INVALID_ENTITY_ID, and -- the anti-vacuity clause -- m_bPhysicsAvailable==TRUE, proving the 'clear' answer did NOT come from the no-simulation fail-open branch.
* MUTATION THAT MUST RED IT: Change the no-simulation branch from `xResult.m_bClear = true;` to `xResult.m_bClear = false;`. Compiles. This unit stays GREEN (physics IS available), which is itself the point: pair it with the mutation below. The per-test mutation that DOES red it: initialise `ZM_TrainerSightProbeResult xResult; xResult.m_bBlockerHit = true;` at the top of the function -- compiles, and the m_bBlockerHit==false assertion reds.

### `Probe_StaticBoxBetweenTrainerAndTargetBlocksTheLine`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightProbe.cpp
* ASSERTS: Create a static AABB box (scale {4,4,0.5}) at {0,0,3} between trainer {0,0,0} and target {0,0,6}. m_bClear==FALSE, m_bBlockerHit==true, m_xBlockerEntityID == the box entity's id, m_fBlockerDistance is finite and in (2.0f, 4.0f). This is THE occlusion proof, made against an explicitly created box and never against terrain (Assets/Terrain/Dawnmere/Physics_*.zmesh is uncommitted, so a terrain-based assertion would be vacuous on CI).
* MUTATION THAT MUST RED IT: In ZM_ProbeTrainerSightLine transpose the separation to `const Zenith_Maths::Vector3 xSeparation = xTrainerPosition - xTargetPosition;`. Both parameters stay referenced so it compiles; the ray points away from the target, hits nothing, and the line reports clear.

### `Probe_TheTargetsOwnBodyIsNotAnOccluder`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightProbe.cpp
* ASSERTS: Create a dynamic capsule at the target position {0,0,6} (gravity disabled so it stays put) with NOTHING between it and the trainer. Passing its Zenith_EntityID as xTargetEntityID gives m_bClear==true and m_bBlockerHit==false. Anti-vacuity: re-run the SAME probe passing INVALID_ENTITY_ID as xTargetEntityID and assert m_bClear==FALSE -- proving the capsule really was hit and the clear answer came from the id comparison, not from an empty world.
* MUTATION THAT MUST RED IT: Change `if (xTargetEntityID != INVALID_ENTITY_ID && xHit.m_xHitEntity == xTargetEntityID)` to `... && xHit.m_xHitEntity != xTargetEntityID)`. Both operands stay referenced so it compiles; the target's own capsule is then reported as a blocker.

### `Probe_TheTrainersOwnBodyIsIgnored`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightProbe.cpp
* ASSERTS: Create a static AABB AT the trainer origin {0,0,0} (so the ray starts inside it -- Jolt treats convex shapes as solid and returns fraction 0) and leave the line to {0,0,6} otherwise empty. Passing the box entity's id as xTrainerEntityID gives m_bClear==true and m_bBlockerHit==false. Anti-vacuity: the same probe with INVALID_ENTITY_ID as xTrainerEntityID gives m_bClear==FALSE with m_fBlockerDistance ~ 0.0f, proving the self-hit is real and the ignore is what excused it.
* MUTATION THAT MUST RED IT: Transpose the ignore argument: `Zenith_PhysicsQuery::RaycastIgnoring(xTrainerPosition, xSeparation, fSeparation, xTargetEntityID)`. Both id parameters remain referenced so it compiles under warnings-as-errors; the trainer self-hits at distance 0 and the line reports blocked.

### `Probe_CoincidentPositionsAreClearWithoutCasting`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightProbe.cpp
* ASSERTS: Trainer and target at the SAME position {2,0,2}, empty world: m_bClear==true, m_bBlockerHit==false. Matches ZM_IsTargetInTrainerSight's own coincident carve-out, so the cone and the filter cannot disagree about a player standing on a trainer.
* MUTATION THAT MUST RED IT: In the coincident early-out change `xResult.m_bClear = true;` to `xResult.m_bClear = false;`. Compiles; reds. (Deliberately NOT 'delete the coincident guard': that would feed a zero-length direction into Zenith_Maths::Normalize and then a NaN ray into Jolt, which is a crash risk rather than a clean red.)

### `Probe_NonFinitePositionFailsClosedAndNeverAsserts`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_TrainerSightProbe.cpp
* ASSERTS: Inside a Zenith_AssertCaptureScope, probe with NaN and +-inf in each of the six position components in turn. Outside the scope: hit count == 0u, and every one of those probes returned m_bClear==FALSE (fail closed -- one body that goes non-finite must not hand every trainer a free sight line), with m_bBlockerHit==false.
* MUTATION THAT MUST RED IT: In the non-finite arm change `xResult.m_bClear = false;` to `xResult.m_bClear = true;`. Compiles; every non-finite probe reports clear and the unit reds. (Again deliberately not 'delete the guard', which would push NaN into Jolt.)

### `Interactable_TrainerSightDefaultsOff`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_Interactable.cpp
* ASSERTS: On the existing DetachedInteractable fixture: GetTrainerId()==ZM_TRAINER_NONE, IsTrainerSightEnabled()==false, GetTrainerSightState()==ZM_TRAINER_SIGHT_WATCHING, GetTrainerSightRaiseCount()==0u. A freshly added, unconfigured component is blind by construction -- the same doctrine as m_bInteractable defaulting false.
* MUTATION THAT MUST RED IT: Change the member default to `ZM_TRAINER_ID m_eTrainerId = ZM_TRAINER_RIVAL_VESPER;` in ZM_Interactable.h. Compiles; both the id and the IsTrainerSightEnabled assertions red.

### `Interactable_ConfigureTrainerSightFailsClosedOnABadId`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_Interactable.cpp
* ASSERTS: ConfigureTrainerSight(ZM_TRAINER_RIVAL_VESPER) returns true, GetTrainerId()==ZM_TRAINER_RIVAL_VESPER, IsTrainerSightEnabled()==true. THEN ConfigureTrainerSight(ZM_TRAINER_NONE) returns false and GetTrainerId()==ZM_TRAINER_NONE with IsTrainerSightEnabled()==false -- it CLEARS rather than keeping the previous row, so a bad authoring value yields a blind NPC and never the WRONG trainer's battle. Same for (ZM_TRAINER_ID)9999u. Also: after OnStart() on a component whose id was force-set out of range, the id is back to ZM_TRAINER_NONE.
* MUTATION THAT MUST RED IT: Change the reject arm to `if (!ZM_IsRegisteredTrainer(eTrainer)) { return false; }` (i.e. drop the `m_eTrainerId = ZM_TRAINER_NONE;` line, keeping the parameter referenced by the guard). Compiles; the stale Vesper row survives the bad call and the post-reject assertions red.

### `Interactable_TrainerSightIsNotSerialized`  [unit]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_Tests_Interactable.cpp
* ASSERTS: THE SCENE-BYTE GUARD. Component A: default. Component B: ConfigureTrainerSight(ZM_TRAINER_RIVAL_VESPER) then Step its FSM to a non-default state. Write each to its own Zenith_DataStream. Assert (1) the two streams have IDENTICAL byte length, (2) ZM_Interactable::uSERIALIZATION_VERSION == 2u, (3) reading B's stream into a third component yields GetTrainerId()==ZM_TRAINER_NONE, IsTrainerSightEnabled()==false and GetTrainerSightRaiseCount()==0u. Any future serialized trainer field, or a version bump, reds this BEFORE anyone notices a dirty Dawnmere.zscen -- which is the whole reason SC6 changes no scene bytes.
* MUTATION THAT MUST RED IT: Append `xStream << (u_int)m_eTrainerId;` to ZM_Interactable::WriteToDataStream (and the matching read). Compiles; the two stream lengths diverge and clause (1) reds. Equivalently, bumping uSERIALIZATION_VERSION to 3u reds clause (2).

### `ZM_TrainerSightWalkUp_Test`  [automated]

* FILE: C:\dev\Zenith\Games\Zenithmon\Tests\ZM_AutoTests_TrainerSight.cpp
* ASSERTS: m_bRequiresGraphics = FALSE (it reads no pixels, so it runs FOR REAL on the Null backend rather than skipping as a pass), maxFrames 4000 (above the SUM of the named phase deadlines: 420 ready + 1 place + 30 basis + 900 approach + 600 in-battle + 900 drive + 8 settle + 200 hold + 200 hold2). Setup guards FIRST -- RequiredDawnmereAssetsPresent() && the Battle scene on disk && the prop bake warm, else RequestSkip -- and only then installs SetFixedDt(1/30), ZM_SetInstantBattlesForTests(true), ZM_BattleTransition::ResetRuntimeStateForTests(), ZM_TrainerEngagementLatch::ResetRuntimeStateForTests() and LoadSceneByIndex(2). Per-phase driver functions, never a monolithic Step (ZM-D-141 stack rule); every singleton and component pointer re-resolved each frame (pools relocate on swap-and-pop).

PHASES AND CLAUSES.
(1) AwaitReady: player grounded with a valid body, follow camera bound, the ZM_BattleTransition singleton resolves.
(2) PlaceTrainer: install a level-60 lead so the battle is WON; capture money + ZM_STORY_FLAG_RIVAL1_DEFEATED baselines (flag must be CLEAR before -- else the transition is vacuous -- and money + 500 must be below uZM_MONEY_CAP, else the credit delta is a saturation artefact). Then CREATE a trainer entity at runtime in the loaded Dawnmere scene, positioned on a clear line ~12 m from the player and ROTATED to face the player's approach, add ZM_Interactable, ConfigureTrainerSight(ZM_TRAINER_RIVAL_VESPER). Assert IsTrainerSightEnabled()==true, GetTrainerSightRaiseCount()==0u, and -- the SPAWN-CAMP GUARD -- that the initial separation EXCEEDS fZM_SIGHT_MAX_DISTANCE (8.0) by a clear margin, so the approach phase cannot be vacuous.
(3) BasisProbe: 30 frames of held W must move the player and +Z must dominate, so a broken movement basis fails in half a second WITH the measured deltas instead of grinding out the approach deadline.
(4) Approach: closed loop on the live position using a file-local copy of ZM_AutoTests_NpcTalk.cpp's CAMERA-RELATIVE DriveTowardXZ (movement is camera-relative and the follow camera is a lagging spring -- a world-space chooser was MEASURED settling on a 45-degree wrong heading), with a 60-frame stall watchdog on best-distance improvement. Ends the instant ZM_BattleTransition::IsTransitionActive() goes true. No SetPosition anywhere: the player walks on Jolt velocity.
(5) AwaitInBattle: capture GetBattleTrainer(). Assert it == ZM_TRAINER_RIVAL_VESPER. THE CHANNEL DISCRIMINATOR: if it is ZM_TRAINER_NONE the round trip was a WILD grass encounter that stole the screen, and the failure message says so and names the walk line as the thing to move.
(6) DriveMenu: ENTER presses until the transition returns to IDLE.
(7) Settle + HoldInCone: the player is restored INSIDE the cone. Hold still 200 frames and assert IsTransitionActive() stays false, GetObservedEncounterCount() does not increase, and the re-resolved component's GetTrainerSightRaiseCount() is EXACTLY 1u. That is 'spotted once, not once per frame' AND the defeat-flag gate, proven end to end.
(8) FlaglessArm: reconfigure the SAME entity to ZM_TRAINER_ROUTE1_RAMBLER (which resets its FSM to a cold watcher, count 0) and call ZM_TrainerEngagementLatch::MarkEngaged(ZM_TRAINER_ROUTE1_RAMBLER). Hold in the cone 200 frames: GetTrainerSightRaiseCount() stays 0u and no transition starts -- the flagless 'already battled this session' arm, end to end, for ~200 frames. Anti-vacuity: assert first that the reconfigure took (IsTrainerSightEnabled() && GetTrainerId()==ZM_TRAINER_ROUTE1_RAMBLER) and that the geometry still puts the player in the cone (poll GetTrainerSightState()/the live separation), so 'no raise' cannot be 'nothing was watching'.
VERIFY: money increased by exactly 500, RIVAL1_DEFEATED newly SET, no whiteout, the transition back at IDLE, the Battle scene unloaded. One mega Zenith_Log line with every capture, then per-clause Zenith_Error. Teardown on EVERY exit path: ClearFixedDt, ZM_SetInstantBattlesForTests(false), ZM_BattleTransition::ResetRuntimeStateForTests(), ZM_TrainerEngagementLatch::ResetRuntimeStateForTests(), ZM_GameStateManager::ResetGameStateForTests(), force-unload any lingering Battle scene, LoadSceneByIndex(0), ResetAllInputState. Returns bPassed || !g_bPrereqsPresent.
* MUTATION THAT MUST RED IT: In ZM_Interactable::TickTrainerSight, delete the `ZM_TrainerEngagementLatch::MarkEngaged(m_eTrainerId);` line before the Dispatch. Compiles (m_eTrainerId is still read by the Dispatch on the next line); phase (8) then raises for the latched rambler and reds. For the primary end-to-end path instead: replace the FSM's action check with `if (m_xSightFsm.Step(xInputs, ZM_TrainerSightFsmTuning{}) == ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER) { return; }` -- an inverted returned condition, compiles cleanly, and the walk-in never starts a battle so phases (5)-(7) all red.

## Risks

WHAT THE PLAYER OBSERVABLY EXPERIENCES, and who ships each beat.

SC6's actual delta is ONE beat: walk into a trainer's forward cone with an unblocked line of sight, within 8 m and inside a 60-degree cone (the SC3 shipped tuning), and the battle fade starts immediately.
- SPOTTED -> SC6. New.
- INPUT FROZEN -> ALREADY SHIPPED, and SC6 must not re-implement it. ZM_BattleTransition::OnTrainerEncounterEvent latches, and TryParkOverworldPlayer (ZM_BattleTransition.cpp:811-880) zeroes linear+angular velocity, drops gravity and calls SetMovementEnabled(false). Three owners (menu, warp, battle) already coordinate over that one seam with an explicit yield rule; a fourth freeze owner in the trainer component would fight them.
- TRAINER WALKS UP TO YOU -> NOT SHIPPED AND NOT SC6. There is no approach behaviour; the battle starts from where the player stands.
- "!" BARK / CHALLENGE DIALOGUE -> NOT SC6. That is SC7's first .bgraph. ZM_UI_MenuStack::TryPushDialogue is not called from this path.
- BATTLE, PRIZE, DEFEAT FLAG -> ALREADY SHIPPED by SC5 and covered by ZM_TrainerBattle_Test; SC6 only raises the event into it.
So the honest one-line description of SC6 in-game: "a trainer who sees you starts a battle." Anyone expecting the full Pokemon spot-bark-walk-talk-fight sequence should be pointed at SC7/SC8.

RISKS, ranked.

1. WILD GRASS CAN STEAL THE ROUND TRIP (the automated test's main flake surface). The walk-in happens in Dawnmere, where ZM_TallGrassSystem rolls a wild encounter on every tile transition over grass density. A wild encounter latching first means ZM_BattleTransition::OnTrainerEncounterEvent's fail-closed guard silently DROPS the trainer raise (Dispatch returns void, there is no error). Mitigations, all present: the FSM's raise-confirm window re-arms after 0.5 s so the trainer tries again; the test asserts GetBattleTrainer() == ZM_TRAINER_RIVAL_VESPER and names grass explicitly in its failure text; and the walk line should follow the same town-centre corridor ZM_NpcTalk_Test and ZM_PlayerHomeRoundTrip_Test already traverse without wild interference. If it still flakes, the fix is to move the walk line, NOT to weaken the channel assertion.

2. THE RAISE-CONFIRM WINDOW IS A HEURISTIC, not a handshake. There is genuinely no public accessor for s_bPendingEncounter / s_bPendingTrainerEncounter, so the FSM cannot know whether its dispatch landed; 0.5 s is a chosen number. Worst case if it is too short: a second dispatch that the latch harmlessly drops. Worst case if too long: a half-second of silence after a dropped raise. Neither is a correctness failure. If SC7+ wants determinism here, the right fix is a public `ZM_BattleTransition::HasPendingEncounter()` accessor, not a longer timer.

3. NO OCCLUSION COVERAGE IN DAWNMERE ITSELF. The occlusion proof is a hermetic boot unit against a created box. The end-to-end test walks a CLEAR line, so it does not exercise blocking in the real scene. That is deliberate (terrain is not an occluder on CI, and asserting against the greybox home shell would couple the test to Dawnmere's authoring), but it means "a wall blocks a trainer in the actual town" is reasoned, not measured. Say so; do not claim otherwise.

4. NPC AABBs AND THE DOOR TRIGGER ARE OPAQUE TO THE RAY. Another NPC's static AABB, or the HomeDoorTrigger box, standing between a trainer and the player counts as an occluder. That is arguably correct, but it is a gameplay consequence SC8 must keep in mind when placing Vesper: a trainer placed behind the door trigger will appear inexplicably blind.

5. HEADER WEIGHT. ZM_Interactable.h now transitively pulls ZM_TrainerData.h -> ZM_BattleAI.h / ZM_BattleTypes.h / ZM_SpeciesData.h / ZM_StoryFlags.h -> ZM_GameState.h. All are pure data headers, but ZM_Interactable.h is included by Zenithmon.cpp and ZM_InteractionRuntime.cpp. Acceptable; noted so nobody is surprised by the compile-time delta.

6. ZM_InteractionRuntime API CHANGE. TryResolveActivePlayerPose is DELETED and replaced by the public TryResolveActivePlayer in the same commit (no-legacy mandate). It has exactly one internal caller (ZM_InteractionRuntime.cpp:97), so the blast radius is one line -- but if any test file references the old name the build breaks loudly, which is the desired failure mode.

7. THE FLAGGED/FLAGLESS ASYMMETRY IS DELIBERATE AND WILL LOOK LIKE A BUG. Losing to Vesper leaves him re-battleable (no flag is written on a loss); losing to the rambler does not (his latch is set on the raise). Both follow the adopted Q-2026-07-28-001 answer verbatim. It is documented in the gate's header comment so a future reader does not "fix" it.

8. OnUpdate ORDERING. TickTrainerSight MUST run before the walker's `if (!m_bWanderEnabled) return;` early-out. SC8's Vesper is stationary, so getting this wrong makes the entire feature silently inert with every unit still green -- the units drive the FSM directly, not through OnUpdate. The reviewer should check this line specifically.

9. TEST-COUNT BOOKKEEPING. This adds 25 ZENITH_TEST cases and 1 ZENITH_AUTOMATED_TEST_REGISTER. The baselines to bump are ZM boot units 2657 (in .github/workflows/zm-tests.yml) and the automated registry 45 -- but ONLY to the numbers actually OBSERVED after the build, never to a predicted number. A regen that silently did not take shows up here and nowhere else. I am forbidden from editing .github/*, so the orchestrator owns that edit.

PROPOSED DOC TEXT for Games/Zenithmon/Docs/DecisionLog.md (I edited no docs):
"ZM-D-15x (S7 item 3 SC6) -- trainer sight glue. The FSM is a PURE by-value member of ZM_Interactable (order 113, no new order): ZM_TrainerSightFsm in Source/Interaction/, driven step by step by boot units with no ECS/scene/physics. The occlusion ray enters ONLY in ZM_Interactable::TickTrainerSight, AFTER the SC3 pure cone has already passed -- there is no raycast budget in this engine, so cheap-gate-first IS the cost control. The ray is real, not decorative: physics is live on the Null backend and Dawnmere's greybox shell, door jambs, NPC AABBs and warp trigger are committed static bodies; TERRAIN is NOT an occluder on a fresh CI checkout (Physics_*.zmesh is uncommitted), so every occlusion assertion is made against an explicitly created box. Only ONE body can be ignored per cast, so the trainer is filtered by id and the player's own capsule is excused by comparing RaycastResult::m_xHitEntity -- no distance tolerance. Re-engagement implements Q-2026-07-28-001: ZM_MayTrainerEngage keys on the row's defeat flag when it has one and on the process-global ZM_TrainerEngagementLatch when it is ZM_STORY_FLAG_NONE, because ZM_IsStoryFlagSet(state, NONE) reads false forever and a flagless row's prize would otherwise be farmable. SC6 SERIALIZES NOTHING: uSERIALIZATION_VERSION stays 2u, Dawnmere.zscen's five ZM_Interactable payloads are byte-identical, and Interactable_TrainerSightIsNotSerialized pins that. SC8 owns persistence and should prefer the zero-byte route (a ZM_TRAINER_ID column at the END of ZM_NpcData, derived in OnStart) over a v3 payload bump."

## Regen needed: True
