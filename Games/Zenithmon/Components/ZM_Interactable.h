#pragma once

#include "Physics/Zenith_Physics_Fwd.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Zenithmon/Source/Data/ZM_NpcData.h"   // ZM_NPC_ID / ZM_NPC_ROLE -- the row this component IS
#include "Zenithmon/Source/Data/ZM_TrainerData.h"                 // ZM_TRAINER_ID
#include "Zenithmon/Source/Interaction/ZM_NpcWalkerLogic.h"
#include "Zenithmon/Source/Interaction/ZM_TrainerSightFsm.h"      // the by-value FSM

class Zenith_ColliderComponent;
class Zenith_DataStream;
class Zenith_TransformComponent;

// ============================================================================
// ZM_Interactable (S6 item 3 SC4) -- the ECS component that makes an authored NPC
// TALKABLE: it says WHICH ZM_NpcData row an entity is, how far its reach extends,
// whether it is currently a candidate at all, and it owns the ONE dispatch point
// that turns "the player pressed E at me" into a raised screen.
//
// Serialization order 113 (registered in Zenithmon.cpp, twice: the
// ZENITH_REGISTER_COMPONENT macro AND the ZENITH_TOOLS editor "Add Component"
// registry -- miss the second and the component silently vanishes from the editor
// menu). The order is a WITHIN-ENTITY tiebreak only: it decides the order this
// component's hooks run relative to OTHER components ON THE SAME ENTITY. It does
// NOT order entities against each other -- cross-entity dispatch follows scene-slot
// / entity-creation order, and the DontDestroyOnLoad ZM_MenuRoot (ZM_UI_MenuStack,
// order 112) lives in the PERSISTENT scene and therefore always updates ahead of a
// scene-owned NPC regardless of numbering. DO NOT try to "fix" an input-ordering
// question by renumbering this component; the ZM_ShouldInteract menu-open gate is
// what serialises the interact edge against the menu, not the component order.
//
// Interact() is deliberately the SINGLE dispatch point. S7 adds a Behaviour-Graph
// branch to it in roughly a dozen lines: one more ZM_NPC_RAISE_KIND arm plus a
// graph-run call in the switch below -- no other file has to move.
// ============================================================================

// Which already-shipped ZM_UI_MenuStack raise seam a role talks through. Split out
// of Interact() as a PURE enum + pure mapping function so the role -> seam decision
// is unit-testable headlessly, with no singleton, no scene and no screen raised.
// APPEND-only (units walk it).
enum ZM_NPC_RAISE_KIND : u_int
{
	ZM_NPC_RAISE_NONE = 0u,     // unknown / unmapped role: raises nothing, and LOGS
	ZM_NPC_RAISE_DIALOGUE,      // -> ZM_UI_MenuStack::TryPushDialogue(row lines)
	ZM_NPC_RAISE_SHOP,          // -> ZM_UI_MenuStack::TryOpenShop(row stock)
	ZM_NPC_RAISE_CARE_CENTER,   // -> ZM_UI_MenuStack::TryOpenCareCenterPrompt()

	// NOT a kind -- the walkable bound the totality unit iterates to.
	ZM_NPC_RAISE_COUNT
};

// The role -> seam map. TOTAL: every ZM_NPC_ROLE below ZM_NPC_ROLE_COUNT maps to a
// real seam, and anything outside the enumerated range yields ZM_NPC_RAISE_NONE
// (which Interact() reports as a warning rather than a silent no-op).
ZM_NPC_RAISE_KIND ZM_RaiseKindForRole(ZM_NPC_ROLE eRole);

// A stable short name for a raise kind, for log lines and unit failure messages.
// TOTAL: never returns nullptr, never indexes out of bounds ("UNKNOWN" otherwise).
const char* ZM_NpcRaiseKindName(ZM_NPC_RAISE_KIND eKind);

// S7 item 1 SC3. The five observations the DYNAMIC-CAPSULE body contract is
// decided from, carried together as ONE value.
//
// ★ IT EXISTS SO THERE IS EXACTLY ONE BODY-VALIDATION PATH. The patrol
// (TryConfigureWanderBody) and the walk-up both ask this same question of the same
// body; two hand-rolled observation blocks would be free to drift into asking
// subtly different ones, and the drift would surface as "the trainer walks but the
// patrol asserts" (or the reverse) with nothing naming the divergence. The
// DECISION over these fields is the pure ZM_Interactable::IsDrivableBodyContractMet
// below; this struct is only the reading.
//
// The two enum sentinels are chosen to FAIL the contract, so a component with no
// collider at all can never read as one carrying a legal body.
struct ZM_InteractableBodyObservation
{
	bool m_bEntityValid     = false;
	bool m_bColliderPresent = false;
	bool m_bBodyValid       = false;
	// Derived from the two enums below and kept only so the patrol's strict assert
	// can name it; the contract itself re-derives it from the enums.
	bool m_bDynamicCapsule  = false;
	bool m_bPhysicsActive   = false;

	CollisionVolumeType  m_eVolumeType    = COLLISION_VOLUME_TYPE_AABB;
	RigidBodyType        m_eRigidBodyType = RIGIDBODY_TYPE_STATIC;

	// Both are only meaningful when m_bColliderPresent / m_bBodyValid hold; a
	// default-constructed Zenith_PhysicsBodyID is the INVALID sentinel.
	Zenith_ColliderComponent* m_pxCollider = nullptr;
	Zenith_PhysicsBodyID      m_xBodyID;
};

class ZM_Interactable
{
public:
	static constexpr u_int uSERIALIZATION_VERSION = 2u;

	// Reach BONUS added to fZM_INTERACT_MAX_DISTANCE by the picker, so a physically
	// large interactable (a shop counter, a sign post) can be addressed from its edge.
	// Default zero: an unconfigured component gets exactly the global reach, never more.
	static constexpr float fDEFAULT_RADIUS = 0.0f;
	// Upper bound on that bonus. A mis-authored huge radius would let one NPC swallow
	// every interact press in the town, so the setter clamps rather than trusting.
	static constexpr float fMAX_RADIUS = 8.0f;

	ZM_Interactable() = delete;
	explicit ZM_Interactable(Zenith_Entity& xParentEntity);

	// Component pools relocate their elements (move-construct + destruct the source),
	// so moves must exist; copies are deleted. Every member is a POD or a movable
	// handle, so defaulted moves are correct.
	ZM_Interactable(const ZM_Interactable&) = delete;
	ZM_Interactable& operator=(const ZM_Interactable&) = delete;
	ZM_Interactable(ZM_Interactable&&) noexcept = default;
	ZM_Interactable& operator=(ZM_Interactable&&) noexcept = default;

	void OnStart();
	void OnUpdate(float fDeltaTime);

	ZM_NPC_ID GetNpcId() const { return m_eNpcId; }
	// Rejects an out-of-range id by storing ZM_NPC_NONE (which makes the component
	// non-interactable) rather than keeping a stale row. Returns whether it took.
	bool SetNpcId(ZM_NPC_ID eId);

	float GetRadius() const { return m_fRadius; }
	// Clamps into [0, fMAX_RADIUS]; a non-finite value resets to fDEFAULT_RADIUS.
	// Returns whether the requested value was taken verbatim.
	bool SetRadius(float fRadius);

	// The live candidacy answer that feeds ZM_InteractProbe::m_bEnabled: the authored
	// flag AND a real NPC row behind it. The conjunction is deliberate -- an entity
	// that carries the component but was never configured must not absorb the interact
	// press and leave the player standing in front of a mute object.
	bool IsInteractable() const { return m_bInteractable && m_eNpcId < ZM_NPC_COUNT; }
	void SetInteractable(bool bInteractable) { m_bInteractable = bInteractable; }

	// Install the authored waypoint loop and its deterministic tuning. A malformed
	// configuration fails closed and disables wandering; successful configuration
	// resets the runtime cursor/dwell state and enables the patrol.
	bool ConfigureWander(const ZM_WalkerWaypoints& xWaypoints,
		const ZM_WalkerTuning& xTuning = ZM_WalkerTuning{});
	// These are observations, not transient "currently moving" flags. In particular,
	// IsWanderEnabled remains true while this NPC is deliberately halted for its own
	// dialogue, and GetWaypointIndex exposes the pure walker's live target cursor.
	bool IsWanderEnabled() const { return m_bWanderEnabled; }
	u_int GetWaypointCount() const { return m_xWalkerWaypoints.m_uCount; }
	u_int GetWaypointIndex() const { return m_xWalkerState.m_uTargetIndex; }

	// Install (or clear) this NPC's trainer identity. Fails CLOSED exactly as
	// SetNpcId does: an unregistered id stores ZM_TRAINER_NONE rather than keeping
	// the previous row, so a bad authoring value yields a blind NPC and never the
	// WRONG trainer's battle. Always resets the sight machine.
	//
	// ★ RUNTIME-ONLY, AND PERMANENTLY SO. m_eTrainerId is NOT serialized and
	// uSERIALIZATION_VERSION stays at 2u, so the five committed ZM_Interactable
	// payloads inside Dawnmere.zscen carry no trainer and a boot must never leave a
	// scene modified in git status.
	//
	// S7 item 3 SC8 TOOK THE ZERO-BYTE ROUTE. An AUTHORED trainer's identity comes
	// from a ZM_TRAINER_ID column at the END of its ZM_NpcData row, re-derived by
	// DeriveTrainerFromNpcRow() in OnStart off the already-serialized m_eNpcId. That
	// is strictly stronger than persisting the id: it is recomputed from committed
	// scene bytes plus a compiled table on EVERY load, including the resume warp and
	// the door round trip. Bump uSERIALIZATION_VERSION to 3u ONLY if a trainer ever
	// needs per-ENTITY identity independent of its roster row -- and then the bump,
	// the version-gated read, and the re-bake + re-commit of Dawnmere.zscen all
	// belong in ONE commit.
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
	// MONOTONIC count of challenge beats STARTED, as distinct from encounters
	// raised. The walk-up test asserts on both: the beat must run exactly once and
	// the battle must still happen exactly once.
	u_int GetTrainerChallengeCount() const { return m_xSightFsm.GetChallengeCount(); }
	// W3 presentation observations. The FSM count proves a genuine SPOTTED entry;
	// the submit count is fed from the helper's MEASURED Flux queue growth, so it
	// proves this live component's line+sphere actually reached the renderer once on
	// every update that ended in SPOTTED. Both are runtime-only.
	u_int GetTrainerSpottedCount() const { return m_xSightFsm.GetSpottedCount(); }
	float GetTrainerSpottedElapsedSeconds() const
	{
		return m_xSightFsm.GetSpottedElapsedSeconds();
	}
	u_int GetTrainerSpottedIndicatorSubmitCount() const
	{
		return m_uSpottedIndicatorSubmitCount;
	}

	// The ONE renderer submission seam for W3's asset-free exclamation mark. Public
	// so a boot unit can inspect the exact CPU primitive payload synchronously; live
	// code calls the same function, and the per-component counter above is fed from
	// this return value.
	//
	// RETURNS 1 only when BOTH primitives were observed to land in Flux's CPU
	// instance queues, and 0 otherwise (including a refused non-finite centre). The
	// caller adds the result rather than incrementing unconditionally, which is what
	// stops the live contract from being satisfiable with the submission removed.
	static u_int SubmitTrainerSpottedIndicator(
		const Zenith_Maths::Vector3& xTrainerCenter,
		const Zenith_Maths::Vector3& xTrainerScale);

	// ---- S7 item 1 SC3: the walk-up -----------------------------------------

	// ★ THE FLOOR ON HOW MUCH OF THE COMMANDED WALK SPEED THE BODY ACTUALLY
	// ACHIEVES, AND IT IS AN ASSUMPTION -- THE COMPILER CANNOT KNOW IT.
	//
	// The walk is driven by writing a velocity onto a dynamic capsule, so what the
	// trainer covers per second is NOT the number handed to SetLinearVelocity. Two
	// losses are structural: the velocity written during Scenes().Update is not
	// integrated until the NEXT frame's Physics().Update (one frame of start-up
	// latency, a fixed cost that matters most on short walks), and Coulomb friction
	// can bleed up to mu * g * dt off the body inside each step before the next tick
	// overwrites it. Terrain slope and contact against the player's capsule can take
	// more.
	//
	// MEASURED: 3.816 m/s achieved against 4.0 m/s commanded -- 95.4% -- over a
	// 5.469 m walk on the committed Dawnmere, headless Null backend, dt 1/30
	// (2026-07-29). This floor is deliberately well under that so ordinary terrain
	// variation cannot trip it, while still failing loudly if someone halves the
	// speed or doubles the friction.
	//
	// ★ IT IS PINNED BY A RUNTIME OBSERVATION, NOT BY ITS OWN EXISTENCE.
	// ZM_RivalVesperAuthored_Test logs achievedSpeed on every run, and
	// Approach_WalkConvergesIntoTheStandoffRingBeforeTheTimeout simulates the walk
	// at THIS derated speed rather than the nominal one -- because a boot unit that
	// integrates a frictionless point mass at the commanded speed certifies a timing
	// margin the live path does not have, which is exactly what happened the first
	// time this was written.
	static constexpr float fAPPROACH_SPEED_EFFICIENCY = 0.85f;

	// THE ONE BODY CONTRACT, as a PURE predicate over the observations, so a boot
	// unit can walk its whole truth table with no entity, no collider, no physics
	// world and nothing created. Both the patrol and the walk-up route their
	// decision through exactly this function.
	//
	// "This component owns a valid DYNAMIC CAPSULE body on an active simulation."
	// The two enum comparisons live HERE rather than at the reading site precisely
	// so inverting either of them is visible to a headless unit -- a wiring defect
	// that only a windowed test could see would be a contract with no unit.
	//
	// TOTAL: every combination of arguments is defined, and it never asserts.
	static bool IsDrivableBodyContractMet(bool bEntityValid,
		bool bColliderPresent,
		bool bBodyValid,
		CollisionVolumeType eVolumeType,
		RigidBodyType eRigidBodyType,
		bool bPhysicsActive);

	// The facing that points a body along xDirectionXZ, as
	// AngleAxis(atan2(dir.x, dir.z), +Y) -- the +Z-forward convention
	// ZM_ForwardFromRotation rotates against.
	//
	// ★ NEVER glm::eulerAngles(quat).y. That decomposition COLLAPSES past 90
	// degrees off +Z and has already cost this repo a full debugging cycle
	// (ZM-D-156); Approach_YawFacesTravelDirectionAcrossAllFourQuadrants exists to
	// red on exactly that respelling, which is why the four quadrants are walked
	// rather than one representative direction.
	//
	// TOTAL, and REFUSES rather than inventing: a zero-length or non-finite XZ
	// direction returns FALSE and leaves xFacingOut untouched. atan2(0, 0) is 0,
	// which would silently snap the body to face +Z -- i.e. a trainer who arrived
	// would pivot north on the frame he stopped, and nothing would name the cause.
	static bool TryBuildApproachFacing(const Zenith_Maths::Vector3& xDirectionXZ,
		Zenith_Maths::Quat& xFacingOut);

	// MONOTONIC count of walk-ups actually ENTERED by this component. Same doctrine
	// as the three counters above: a machine stubbed into APPROACHING would satisfy
	// a state check having taken no step.
	u_int GetTrainerApproachCount() const { return m_xSightFsm.GetApproachCount(); }
	float GetTrainerApproachElapsedSeconds() const
	{
		return m_xSightFsm.GetApproachElapsedSeconds();
	}
	// LAST TICK'S contract answer, for the automated fail-open clause: the
	// collider-less runtime trainer must report FALSE and take the shipped
	// SPOTTED exit unchanged.
	bool IsTrainerApproachPossible() const { return m_bApproachPossible; }
	// True while THIS component is holding the cinematic freeze. Runtime-only, and
	// deliberately observable: a test that asserted only on the player's movement
	// bool could not tell "released" from "never taken".
	bool IsTrainerCinematicHoldActive() const { return m_bCinematicHoldActive; }

	// Fire this NPC's role: ONE switch over ZM_RaiseKindForRole(row.m_eRole) onto the
	// three shipped ZM_UI_MenuStack seams. Returns whether a screen was actually
	// raised. A refusal (no menu singleton, a full dialogue queue, a rejected stock
	// list) and an unknown / unconfigured role both LOG a warning naming the NPC and
	// the role -- Shortfalls 1.6: a mis-authored NPC must never be silently mute.
	bool Interact();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	// Configure (or reconfigure after a body replacement) the physics half of an
	// enabled patrol. Non-strict calls are used while editor construction may still
	// be assembling the entity; the first runtime update is strict and fails closed.
	bool TryConfigureWanderBody(bool bRequireRuntimeReady);

	// ---- S7 item 1 SC3: the shared body half --------------------------------

	// READ the body contract's five observations off this entity. TOTAL: a missing
	// entity / collider / body / simulation yields an observation whose sentinels
	// all FAIL the contract. It decides NOTHING -- IsDrivableBodyContractMet does.
	ZM_InteractableBodyObservation ObserveDrivableBody();

	// Apply the shared NPC body properties, at most once per body IDENTITY (the
	// m_xConfiguredWanderBodyID cache the patrol already keeps). Shared by the
	// patrol and the walk-up so a trainer who walks and a patrol who walks are
	// standing on the same physics, and so the ONE difference between them -- the
	// YAW LOCK -- is spelled in exactly one place.
	void ApplyDrivenBodySetup(const ZM_InteractableBodyObservation& xBody);

	// Drive ONE tick of the walk: the two-call velocity idiom UpdateWander uses,
	// then the travel-direction facing. Writes NO position.
	void DriveTrainerApproach(const ZM_InteractableBodyObservation& xBody,
		const ZM_TrainerApproachStep& xStep,
		Zenith_TransformComponent* pxTransform);

	// The anti-shove half of the same seam: hold XZ station and repair the authored
	// yaw while this trainer is NOT walking. See the R1 block in the .cpp.
	void HoldTrainerStation(const ZM_InteractableBodyObservation& xBody,
		Zenith_TransformComponent* pxTransform);

	// Take / release the cinematic freeze. BOTH are idempotent, and the release is
	// TOTAL: releasing a hold nobody took is legal and inert.
	//
	// ★ THE HOLD IS TIED TO THE OPERATION, NOT TO A CALL SITE. Acquire re-validates
	// on EVERY tick the machine is APPROACHING (so re-entering this function inside
	// one logical walk re-applies a no-op rather than stacking a second claim), and
	// release is driven by "am I still holding one?" rather than by having spotted a
	// particular transition -- arrival, timeout and cancel all leave through it.
	void AcquireTrainerCinematicHold(Zenith_EntityID xPlayerID);
	void ReleaseTrainerCinematicHold();

	// OnUpdate is now these two, in this order. The walker body is UNCHANGED --
	// it moved verbatim into UpdateWander -- so its behaviour is byte-identical.
	// The sight tick must run FIRST and OUTSIDE the walker's `if (!m_bWanderEnabled)
	// return;` early-out, or a stationary trainer (which is what SC8 authors) would
	// never see anything.
	void TickTrainerSight(float fDeltaTime);
	void UpdateWander(float fDeltaTime);

	// S7 item 3 SC8. The AUTHORED trainer's identity, recovered from the compiled
	// ZM_NpcData row named by the already-serialized m_eNpcId.
	//
	// FILL-IF-EMPTY, and that is mandatory rather than defensive: the shipped
	// windowed sight gate builds a trainer with NO npc row and calls
	// ConfigureTrainerSight BEFORE OnStart dispatches, so an unconditional assignment
	// would wipe it. Runtime configuration WINS over the authored row.
	//
	// Called from OnStart ONLY. ReadFromDataStream provably runs FIRST on every
	// shipped load path (Zenith_SceneData_Serialization.cpp:196 deserializes and
	// marks pending-start; Zenith_SceneSystem_Lifecycle.cpp:399-407 drains
	// DispatchPendingStarts at the TOP of the next Update), so there is nothing for a
	// second call site to repair. If a tools-only editor re-read of a LIVE component
	// ever appears, the fix is one line: call this again at the tail of
	// ReadFromDataStream inside `if (m_bLifecycleStarted)`.
	void DeriveTrainerFromNpcRow();

	// Attaches the shared challenge graph, at most once per session, the first time
	// this component ticks as a trainer WITH lines.
	//
	// ★ CALLED ONLY FROM TickTrainerSight -- an OnUpdate-only path. Zenith_Core.cpp:138
	// gates Scenes().Update (the sole driver of OnUpdate AND of the pending-OnStart
	// queue) on EditorMode::Playing, and the boot authoring pass runs with the editor
	// Stopped. A Zenith_GraphComponent therefore CANNOT exist on this entity during
	// AddStep_SaveScene and CANNOT reach committed scene bytes. This is what keeps
	// SC7 at zero .zscen bytes, and it BINDS SC8: do NOT AddStep_AttachGraph the
	// authored Vesper -- he picks the graph up here for free.
	void EnsureTrainerChallengeGraph();
	// Fires szZM_GRAPH_EVENT_TRAINER_SPOTTED at this entity's own graph, payload =
	// the trainer id. Silently does nothing when there is no resolved graph, which is
	// the FAIL-OPEN path: the FSM's challenge window then raises the encounter.
	void RunTrainerChallenge();

	// Stored BY VALUE (never a reference): a reference member would dangle on the
	// temporary ctor handle and break the pool's move-construct.
	Zenith_Entity m_xParentEntity;
	ZM_NPC_ID     m_eNpcId        = ZM_NPC_NONE;
	float         m_fRadius       = fDEFAULT_RADIUS;
	// Defaults FALSE. A freshly added, unconfigured component is inert by
	// construction -- SC5 authoring turns each NPC on explicitly.
	bool          m_bInteractable = false;

	// Authored patrol data is serialized with the NPC. Cursor/dwell and the ownership
	// latch are session-only: loading always restarts deterministically at point zero,
	// and only a screen raised by THIS component is allowed to halt THIS walker.
	ZM_WalkerWaypoints m_xWalkerWaypoints;
	ZM_WalkerTuning    m_xWalkerTuning;
	ZM_WalkerState     m_xWalkerState;
	bool               m_bWanderEnabled = false;
	bool               m_bOwnsInteractionMenu = false;
	// RUNTIME-ONLY, exactly like m_bOwnsInteractionMenu and m_xConfiguredWanderBodyID:
	// NOT serialized, uSERIALIZATION_VERSION STAYS 2u, and
	// Interactable_TrainerSightIsNotSerialized must stay green WITH NO EDIT.
	// "Attempted", not "attached": one load attempt per session whether it succeeded
	// or not, so a missing asset costs one failed lookup rather than one per frame.
	bool               m_bChallengeGraphAttempted = false;
	// Runtime-only lifecycle/body identity. A body-id change causes the shared setup
	// path to apply gravity/upright/material properties to the replacement exactly once.
	Zenith_PhysicsBodyID m_xConfiguredWanderBodyID;
	bool                  m_bLifecycleStarted = false;

	// Session-only, NEVER serialized -- the ZM_WalkerState precedent. A scene
	// reload restarts a cold watcher deterministically.
	ZM_TRAINER_ID      m_eTrainerId = ZM_TRAINER_NONE;
	ZM_TrainerSightFsm m_xSightFsm;
	u_int              m_uSpottedIndicatorSubmitCount = 0u;

	// ---- S7 item 1 SC3, ALL RUNTIME-ONLY -------------------------------------
	// They ride in the same never-serialized block as m_eTrainerId and the watcher,
	// for the same reason and one sharper one: a cinematic hold that survived a save
	// would restore a player who can never move again. uSERIALIZATION_VERSION STAYS
	// 2u and Interactable_TrainerSightIsNotSerialized must stay green WITH NO EDIT.
	bool m_bApproachPossible    = false;
	bool m_bCinematicHoldActive = false;
	// WHICH player this component froze, so the release re-enables the controller it
	// actually took -- the ZM_UI_MenuStack::m_xFrozenPlayerEntityID precedent.
	Zenith_EntityID m_xCinematicHeldPlayerID = INVALID_ENTITY_ID;
	// The AUTHORED facing, captured once per component start, and the flag that says
	// a capture happened. See the R1 block in the .cpp: a DYNAMIC body can be yawed
	// by a collision, and a yawed trainer is a PERMANENTLY BLIND one that no boot
	// unit can see (the units reason about compiled constants; the damage is live
	// state). Never serialized: it is re-read from the committed transform on every
	// load, which is strictly stronger than persisting it.
	Zenith_Maths::Quat m_xWatchFacing = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	bool               m_bWatchFacingCaptured = false;
};
