#include "Zenith.h"

#include "Zenithmon/Components/ZM_Interactable.h"

#include "Core/Multithreading/Zenith_Multithreading.h"   // Zenith_ScopedMutexLock (the primitive-queue sample)
#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "AssetHandling/Zenith_FontAsset.h"   // GetActiveOrDefaultMetrics -- the name-tag centring width
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"   // SetFloat("Speed") -- the Idle<->Walk blend
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"   // the runtime graph host
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Flux_GraphicsImpl.h"    // GetViewProjMatrix -- the name-tag world-to-screen projection
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Physics/Zenith_Physics.h"
#include "UI/Zenith_UICanvas.h"        // SubmitText -- the name-tag draw call
#include "Zenith_OS_Include.h"         // Zenith_Window::GetInstance -- the name-tag pixel-space size
#include "ZenithECS/Zenith_EventSystem.h"            // Zenith_EventDispatcher -- the encounter channel
#include "ZenithECS/Zenith_SceneSystem.h"            // GetActiveScene / GetSceneInfo (the source-scene lookup)
#include "Zenithmon/Components/ZM_BattleTransition.h"   // IsTransitionActive -- the busy-channel input
#include "Zenithmon/Components/ZM_GameStateManager.h"   // TryGetGameState -- the live story flags
#include "Zenithmon/Components/ZM_PlayerController.h"   // SetMovementEnabled + fWALK_SPEED (the walk-up freeze)
#include "Zenithmon/Components/ZM_UI_MenuStack.h"   // the three shipped raise seams
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"    // ZM_StoryFlagSet (the gate's input)
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"      // ZM_SCENE_ID / ZM_FindSceneByBuildIndex
#include "Zenithmon/Source/Graph/ZM_GraphAuthoring.h"             // the shared name constants
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"   // TryResolveActivePlayer -- THE player seam
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"    // the SC3 pure cone (unmodified)
#include "Zenithmon/Source/Interaction/ZM_TrainerSightProbe.h"    // the occlusion filter
#include "Zenithmon/Source/Party/ZM_StarterChoice.h"              // ZM_CanEnterBattle -- the gate's fourth observation
#include "Zenithmon/Source/World/ZM_EncounterEvents.h"            // ZM_OnTrainerEncounter
#include "Zenithmon/Source/World/ZM_HumanBody.h"                  // THE body contract (head anchors)

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <cmath>     // std::isfinite (radius sanitising)
#include <cstring>   // std::strcmp (the idempotency scan)

// ============================================================================
// ZM_Interactable (S6 item 3 SC4). See the header for the contract. The role ->
// seam map is PURE and lives at the top so a unit can exercise it with no scene,
// no singleton and nothing raised; Interact() below is the only impure part.
// ============================================================================

namespace
{
	constexpr float fZM_WANDER_BODY_FRICTION = 0.8f;
	constexpr float fZM_WANDER_BODY_RESTITUTION = 0.0f;
	constexpr float fZM_SPOTTED_DOT_OFFSET = 0.25f;
	constexpr float fZM_SPOTTED_DOT_RADIUS = 0.13f;
	constexpr float fZM_SPOTTED_STEM_START_OFFSET = 0.55f;
	constexpr float fZM_SPOTTED_STEM_END_OFFSET = 1.20f;
	constexpr float fZM_SPOTTED_STEM_RADIUS = 0.10f;
	const Zenith_Maths::Vector3 xZM_SPOTTED_COLOUR(1.0f, 0.82f, 0.08f);

	bool ZM_IsFiniteVector3(const Zenith_Maths::Vector3& xValue)
	{
		return std::isfinite(xValue.x)
			&& std::isfinite(xValue.y)
			&& std::isfinite(xValue.z);
	}

	bool ZM_IsValidWanderConfiguration(const ZM_WalkerWaypoints& xWaypoints,
		const ZM_WalkerTuning& xTuning)
	{
		if (xWaypoints.m_uCount == 0u
			|| xWaypoints.m_uCount > ZM_WalkerWaypoints::uMAX_WAYPOINTS
			|| !std::isfinite(xTuning.m_fSpeed)
			|| xTuning.m_fSpeed <= 0.0f
			|| !std::isfinite(xTuning.m_fArriveRadius)
			|| xTuning.m_fArriveRadius <= 0.0f
			|| !std::isfinite(xTuning.m_fDwellSeconds)
			|| xTuning.m_fDwellSeconds < 0.0f)
		{
			return false;
		}

		for (u_int u = 0u; u < xWaypoints.m_uCount; ++u)
		{
			if (!ZM_IsFiniteVector3(xWaypoints.m_axPoints[u]))
			{
				return false;
			}
		}
		return true;
	}

	// ---- S7 item 1 SC3: the walk-up's own tuning ---------------------------
	//
	// The shipped sight-machine tuning, spelled ONCE for this whole file. The FSM
	// step, the standoff the approach maths stops on, and the timeout the static
	// assertion below is written against must all be the SAME instance, or the walk
	// would be measured against one set of numbers and judged against another.
	constexpr ZM_TrainerSightFsmTuning xZM_SIGHT_TUNING{};

	// ★ THE WALK-UP SPEED IS NOT A NEW TUNING KNOB. It is the PLAYER'S OWN WALK
	// SPEED -- the one human-body speed this game has already tuned -- so a trainer
	// closing the gap moves at a rate the player has spent the whole overworld
	// learning to read. Naming ZM_PlayerController::fWALK_SPEED rather than
	// re-typing 4.0f is what keeps them from drifting apart silently.
	constexpr float fZM_TRAINER_APPROACH_SPEED = ZM_PlayerController::fWALK_SPEED;

	// ★ WHAT THE WALK ACTUALLY HAS TO COVER, and it is NOT the whole sight range.
	// The trainer starts at most fZM_SIGHT_MAX_DISTANCE away (the cone admits no
	// further) and STOPS on the standoff ring, so the distance that governs arrival
	// is the difference. The first revision of this file asserted against the full
	// range, which is a different -- and slacker -- claim than the one that matters.
	constexpr float fZM_TRAINER_APPROACH_REACH =
		fZM_SIGHT_MAX_DISTANCE - xZM_SIGHT_TUNING.m_fApproachStandoffMetres;

	// ★ THE COUPLING IS A COMPILE ERROR, NOT A FLAKY FRAME BUDGET. The FSM's
	// approach timeout FAILS OPEN: when it expires the machine hands off to the
	// bark/encounter WHEREVER the trainer happens to be standing. So a speed that
	// cannot cross that reach inside the timeout does not produce a failing test --
	// it produces a walk that visibly stops short, in a build where every unit is
	// still green. Lower the speed, widen the cone, shrink the standoff or shorten
	// the timeout and THIS line is what tells you.
	//
	// ★ IT IS WRITTEN AGAINST THE **ACHIEVED** SPEED, NOT THE COMMANDED ONE, AND
	// THAT DISTINCTION IS THE WHOLE POINT. The previous form multiplied the nominal
	// fWALK_SPEED by the timeout and compared against the full sight range: it was
	// true at EXACTLY zero margin (4.0 * 2.0 >= 8.0) and it reasoned about a speed
	// the body never actually travels at, so it read as proof while proving nothing
	// about arrival. A compiler cannot know the achieved speed -- friction, contact
	// response, terrain slope and one frame of integration latency are all runtime
	// -- so the derating is declared as an ASSUMPTION
	// (ZM_Interactable::fAPPROACH_SPEED_EFFICIENCY, measured and documented on that
	// constant) and the assumption itself is pinned by a boot unit and logged live
	// on every windowed run. An assert that cannot be wrong about the right quantity
	// is worth less than one that CAN be wrong about it.
	//
	// Currently: 4.0 m/s * 0.85 * 2.0 s = 6.8 m against a 6.0 m reach -- 13% margin,
	// against 95.4% efficiency actually measured.
	static_assert(
		fZM_TRAINER_APPROACH_SPEED
				* ZM_Interactable::fAPPROACH_SPEED_EFFICIENCY
				* xZM_SIGHT_TUNING.m_fApproachTimeoutSeconds
			>= fZM_TRAINER_APPROACH_REACH,
		"the trainer walk-up cannot cross (fZM_SIGHT_MAX_DISTANCE - the standoff "
		"ring) inside ZM_TrainerSightFsmTuning::m_fApproachTimeoutSeconds at the "
		"DERATED walk speed -- the fail-open timeout would end the walk with the "
		"trainer still short of the player, and every downstream test would stay "
		"green because the timeout hands off to the bark regardless");

	// How far the live rotation may drift from the authored one before the WATCHING
	// repair writes it back. |dot| of two unit quaternions, so this is ~0.5 degrees:
	// tight enough that a collision-induced yaw is corrected on the frame it
	// happens, loose enough that float round-tripping through the body pose is not
	// a per-frame write.
	constexpr float fZM_WATCH_FACING_MIN_ABS_DOT = 0.99999f;

	// The player's controller, from the entity id the sight tick already resolved.
	// Re-resolved per use and never cached: component pools relocate on swap-and-pop.
	ZM_PlayerController* ZM_ResolvePlayerController(Zenith_EntityID xPlayerID)
	{
		if (xPlayerID == INVALID_ENTITY_ID)
		{
			return nullptr;
		}
		Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(xPlayerID);
		return xPlayer.IsValid()
			? xPlayer.TryGetComponent<ZM_PlayerController>()
			: nullptr;
	}

	// |dot| of two rotations. ABSOLUTE because q and -q are the SAME rotation, so a
	// sign-flipped-but-identical quaternion must not read as drift and trigger a
	// pointless body write every frame.
	float ZM_FacingAbsDot(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB)
	{
		const float fDot = xA.w * xB.w + xA.x * xB.x + xA.y * xB.y + xA.z * xB.z;
		return std::fabs(fDot);
	}

	// The SIXTH local instance of this three-line lookup (ZM_TallGrassSystem,
	// ZM_UI_MenuStack, ZM_BattleTransition x2, ZM_GameStateManager already each
	// hold one); it is a lookup, not a decision, and promoting it would be a new
	// API for no gain.
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
}

ZM_NPC_RAISE_KIND ZM_RaiseKindForRole(ZM_NPC_ROLE eRole)
{
	switch (eRole)
	{
	case ZM_NPC_ROLE_TALKER:    return ZM_NPC_RAISE_DIALOGUE;
	case ZM_NPC_ROLE_SHOPKEEP:  return ZM_NPC_RAISE_SHOP;
	case ZM_NPC_ROLE_CARETAKER: return ZM_NPC_RAISE_CARE_CENTER;
	// A switch, not a table lookup, precisely so ZM_NPC_ROLE_COUNT and anything
	// past it land here instead of reading off the end of an array.
	default:                    return ZM_NPC_RAISE_NONE;
	}
}

const char* ZM_NpcRaiseKindName(ZM_NPC_RAISE_KIND eKind)
{
	switch (eKind)
	{
	case ZM_NPC_RAISE_NONE:        return "NONE";
	case ZM_NPC_RAISE_DIALOGUE:    return "DIALOGUE";
	case ZM_NPC_RAISE_SHOP:        return "SHOP";
	case ZM_NPC_RAISE_CARE_CENTER: return "CARE_CENTER";
	default:                       return "UNKNOWN";
	}
}

bool ZM_NpcRaisesStarterChoice(ZM_NPC_ID eNpcId, const ZM_StoryFlagSet& xFlags)
{
	// ONE row, named explicitly. See the header: this is a routing rule keyed on an
	// NPC identity, not a role -- Aster is still a TALKER and still talks.
	if (eNpcId != ZM_NPC_PROF_ASTER)
	{
		return false;
	}
	// ...and only until the starter has actually been handed over. THE one-shot
	// guard; ZM_ApplyStarterChoice would happily grant a second monster and the
	// row's line gate would happily keep raising the screen while changing only
	// which words appear over it.
	return !ZM_IsStoryFlagSet(xFlags, ZM_STORY_FLAG_STARTER_RECEIVED);
}

ZM_STORY_FLAG_ID ZM_IntroStoryFlagForArrival(u_int uBuildIndex, const char* szSpawnTag)
{
	// The lab. Its ONLY door is the Dawnmere lab seam, so the build index alone is
	// the whole condition -- there is no second way in to distinguish.
	if (uBuildIndex == ZM_GetWorldSpec(ZM_SCENE_PROFLAB).m_uBuildIndex)
	{
		return ZM_STORY_FLAG_MET_PROFESSOR;
	}

	// Dawnmere is reachable four ways (town centre, the route, the lab return and
	// the front door), and only ONE of them is leaving home. The tag is read off the
	// compiled PlayerHome -> Dawnmere connection rather than spelled, so a world-table
	// edit that renames that edge moves this decision with it instead of silently
	// leaving the flag unset forever.
	if (uBuildIndex != ZM_GetWorldSpec(ZM_SCENE_DAWNMERE).m_uBuildIndex
		|| szSpawnTag == nullptr
		|| szSpawnTag[0] == '\0')
	{
		return ZM_STORY_FLAG_NONE;
	}
	const ZM_WorldSpec& xHome = ZM_GetWorldSpec(ZM_SCENE_PLAYERHOME);
	if (xHome.m_pxConnections == nullptr)
	{
		return ZM_STORY_FLAG_NONE;
	}
	for (u_int uEdge = 0u; uEdge < xHome.m_uConnectionCount; ++uEdge)
	{
		const ZM_SceneConnection& xEdge = xHome.m_pxConnections[uEdge];
		if (xEdge.m_eTarget != ZM_SCENE_DAWNMERE || xEdge.m_szSpawnTag == nullptr)
		{
			continue;
		}
		if (std::strcmp(xEdge.m_szSpawnTag, szSpawnTag) == 0)
		{
			return ZM_STORY_FLAG_INTRO_LEFT_HOME;
		}
	}
	return ZM_STORY_FLAG_NONE;
}

ZM_Interactable::ZM_Interactable(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_Interactable::OnStart()
{
	m_bLifecycleStarted = true;
	// FIRST, and before anything else is reset: a component that is being (re)started
	// is a COLD watcher by definition, so any cinematic freeze it was still holding
	// belongs to an operation that no longer exists. Releasing it here is what stops
	// a restart from stranding the player frozen. TOTAL -- inert on a fresh component.
	ReleaseTrainerCinematicHold();
	m_xConfiguredWanderBodyID = Zenith_PhysicsBodyID{};
	m_xWalkerState = ZM_WalkerState{};
	m_bOwnsInteractionMenu = false;
	m_bApproachPossible = false;
	// The sight machine is SESSION state, exactly like m_xWalkerState above: every
	// start is a cold watcher, and a trainer id that no longer resolves must not
	// survive as a live watcher either.
	m_xSightFsm.Reset();
	m_uSpottedIndicatorSubmitCount = 0u;
	// S7 item 3 SC7: the runtime graph-attach latch is session state too, cleared
	// exactly where the trainer id and the watcher are.
	m_bChallengeGraphAttempted = false;
	if (!ZM_IsRegisteredTrainer(m_eTrainerId))
	{
		m_eTrainerId = ZM_TRAINER_NONE;
	}

	// Re-validate what deserialization / authoring left behind. A row id that no
	// longer exists (the roster shrank) must not survive as a live candidate.
	if (m_eNpcId >= ZM_NPC_COUNT)
	{
		m_eNpcId = ZM_NPC_NONE;
		m_bInteractable = false;
	}
	// ★ ORDER IS LOAD-BEARING, in BOTH directions, and mirrors the warning on
	// OnUpdate below.
	//   * AFTER the clamp: ZM_GetNpcData (ZM_NpcData.cpp) ASSERTS on an out-of-range
	//     id, and an assert in a boot unit does not fail one test -- it ends the
	//     whole boot-unit run. Cited by SYMBOL, never by line: that file is an
	//     APPEND-ONLY roster, so every new row shifts its line numbers (SC8's own
	//     rival row already did).
	//   * BEFORE the `if (!m_bWanderEnabled) { return; }` early-out below: SC8's
	//     authored rival is STATIONARY, so folding this call past that return would
	//     leave every authored trainer permanently blind while every unit still
	//     passed. Interactable_OnStartDerivesTheTrainerFromItsNpcRow covers exactly
	//     that mistake.
	DeriveTrainerFromNpcRow();
	SetRadius(m_fRadius);

	// ★ CAPTURE THE AUTHORED FACING AT THE EARLIEST POSSIBLE INSTANT (S7 item 1 SC3,
	// risk R1). This runs at the TOP of the first Update after deserialization, so
	// what it reads is the rotation the committed scene bytes carry -- before the
	// first physics step of this session, and therefore before any contact could
	// have moved it. Capturing lazily on the first sight tick instead would risk
	// freezing an already-damaged yaw in as the thing to repair TO.
	//
	// Re-captured on every start on purpose: a re-authored scene must be able to
	// move him without a stale runtime value arguing.
	m_bWatchFacingCaptured = false;
	if (Zenith_TransformComponent* pxStartTransform = m_xParentEntity.IsValid()
			? m_xParentEntity.TryGetComponent<Zenith_TransformComponent>()
			: nullptr)
	{
		pxStartTransform->GetRotation(m_xWatchFacing);
		m_bWatchFacingCaptured = true;
	}
	if (m_bWanderEnabled
		&& !ZM_IsValidWanderConfiguration(m_xWalkerWaypoints, m_xWalkerTuning))
	{
		m_bWanderEnabled = false;
	}

	// Stationary NPCs never enter the physics-facing portion of the runtime contract:
	// their existing static AABBs and runtime behaviour remain unchanged. OnStart is
	// non-strict because editor construction may legitimately still be assembling a
	// body; a valid reloaded scene is configured here, and OnUpdate is the strict gate.
	if (!m_bWanderEnabled)
	{
		return;
	}
	TryConfigureWanderBody(false);
}

bool ZM_Interactable::IsDrivableBodyContractMet(bool bEntityValid,
	bool bColliderPresent,
	bool bBodyValid,
	CollisionVolumeType eVolumeType,
	RigidBodyType eRigidBodyType,
	bool bPhysicsActive)
{
	// PURE, and the ONLY place the contract is DECIDED. See the header for why the
	// two enum comparisons live here rather than at the reading site.
	return bEntityValid
		&& bColliderPresent
		&& bBodyValid
		&& eVolumeType == COLLISION_VOLUME_TYPE_CAPSULE
		&& eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC
		&& bPhysicsActive;
}

ZM_InteractableBodyObservation ZM_Interactable::ObserveDrivableBody()
{
	ZM_InteractableBodyObservation xBody;
	xBody.m_bEntityValid = m_xParentEntity.IsValid();
	xBody.m_pxCollider = xBody.m_bEntityValid
		? m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>()
		: nullptr;
	xBody.m_bColliderPresent = xBody.m_pxCollider != nullptr;
	xBody.m_bBodyValid =
		xBody.m_bColliderPresent && xBody.m_pxCollider->HasValidBody();
	if (xBody.m_bColliderPresent)
	{
		xBody.m_eVolumeType = xBody.m_pxCollider->GetCollisionVolumeType();
		xBody.m_eRigidBodyType = xBody.m_pxCollider->GetRigidBodyType();
	}
	xBody.m_bDynamicCapsule = xBody.m_bColliderPresent
		&& xBody.m_eVolumeType == COLLISION_VOLUME_TYPE_CAPSULE
		&& xBody.m_eRigidBodyType == RIGIDBODY_TYPE_DYNAMIC;
	xBody.m_bPhysicsActive = g_xEngine.Physics().HasActiveSimulation();
	if (xBody.m_bBodyValid)
	{
		xBody.m_xBodyID = xBody.m_pxCollider->GetBodyID();
	}
	return xBody;
}

void ZM_Interactable::ApplyDrivenBodySetup(
	const ZM_InteractableBodyObservation& xBody)
{
	// Once per body IDENTITY. A body-id change (a rebuilt collider) re-applies
	// everything below exactly once, which is the behaviour the patrol has always
	// had; this function is that block, moved verbatim and shared.
	if (xBody.m_pxCollider == nullptr || m_xConfiguredWanderBodyID == xBody.m_xBodyID)
	{
		return;
	}

	Zenith_Physics& xPhysics = g_xEngine.Physics();
	xBody.m_pxCollider->SetIsSensor(false);
	xPhysics.SetGravityEnabled(xBody.m_xBodyID, true);
	// ★ THE YAW LOCK IS THE ONE DIFFERENCE BETWEEN A PATROL BODY AND A STATIONARY
	// TRAINER'S, AND IT IS THE PRIMARY S7-item-1 R1 MITIGATION.
	//
	// RIGIDBODY_TYPE offers only DYNAMIC and STATIC -- there is no KINEMATIC -- so
	// the authored rival now stands on a body the player can physically shove. With
	// Y left free (which is what a patrol wants, because nothing writes a walker's
	// yaw) one shoulder-barge applies real torque about +Y, and a trainer turned a
	// few degrees off his authored bearing is PERMANENTLY BLIND: the 60-degree cone
	// stops covering the lane he was authored to watch. That is a live regression of
	// exactly the defect ZM-D-156 fixed, and NO BOOT UNIT COULD SEE IT -- the units
	// reason about compiled placement constants while the damage sits in runtime
	// body state.
	//
	// Zenith_Physics::LockRotation zeroes the INVERSE INERTIA on the locked axes, so
	// no contact can ever produce angular acceleration about them; it does NOT block
	// a direct pose write, which is what leaves DriveTrainerApproach free to author
	// the walk's facing. A stationary trainer therefore owns his yaw outright, and
	// physics owns none of it.
	xPhysics.LockRotation(xBody.m_xBodyID, true, !m_bWanderEnabled, true);
	xPhysics.EnforceUpright(xBody.m_xBodyID);
	xPhysics.SetFriction(xBody.m_xBodyID, fZM_WANDER_BODY_FRICTION);
	xPhysics.SetRestitution(xBody.m_xBodyID, fZM_WANDER_BODY_RESTITUTION);
	m_xConfiguredWanderBodyID = xBody.m_xBodyID;
}

bool ZM_Interactable::TryConfigureWanderBody(bool bRequireRuntimeReady)
{
	if (!m_bWanderEnabled)
	{
		return false;
	}

	// ONE observation, ONE decision -- both shared with the walk-up. The five
	// diagnostics the strict assert names are the fields of that same observation,
	// so the message can never describe a body other than the one that was judged.
	const ZM_InteractableBodyObservation xBody = ObserveDrivableBody();
	const bool bContractValid = IsDrivableBodyContractMet(
		xBody.m_bEntityValid, xBody.m_bColliderPresent, xBody.m_bBodyValid,
		xBody.m_eVolumeType, xBody.m_eRigidBodyType, xBody.m_bPhysicsActive);
	if (!bContractValid)
	{
		m_xConfiguredWanderBodyID = Zenith_PhysicsBodyID{};
		if (bRequireRuntimeReady)
		{
			Zenith_Assert(bContractValid,
				"[ZM_Interactable] enabled wander patrol requires an active physics "
				"simulation and a valid DYNAMIC CAPSULE body "
				"(npcId=%u entityValid=%d colliderPresent=%d bodyValid=%d "
				"dynamicCapsule=%d physicsActive=%d)",
				(u_int)m_eNpcId, (int)xBody.m_bEntityValid,
				(int)xBody.m_bColliderPresent, (int)xBody.m_bBodyValid,
				(int)xBody.m_bDynamicCapsule, (int)xBody.m_bPhysicsActive);
			// Release builds compile the assertion out, so the explicit state change is
			// the shipping fail-closed path rather than a debug-only diagnostic.
			m_bWanderEnabled = false;
		}
		return false;
	}

	ApplyDrivenBodySetup(xBody);
	return true;
}

bool ZM_Interactable::TryBuildApproachFacing(
	const Zenith_Maths::Vector3& xDirectionXZ,
	Zenith_Maths::Quat& xFacingOut)
{
	// REFUSE rather than invent. See the header: atan2(0, 0) is a perfectly finite
	// ZERO, so a degenerate direction would silently author a body facing +Z.
	if (!std::isfinite(xDirectionXZ.x) || !std::isfinite(xDirectionXZ.z))
	{
		return false;
	}
	const float fLength = std::hypot(xDirectionXZ.x, xDirectionXZ.z);
	if (!std::isfinite(fLength) || fLength <= 0.0f)
	{
		return false;
	}

	// X FIRST, Z SECOND -- the +Z-forward convention ZM_ForwardFromRotation rotates
	// against, and the same argument order ZM_DawnmereVesperYaw uses. NEVER
	// glm::eulerAngles(quat).y.
	const float fYaw = std::atan2(xDirectionXZ.x, xDirectionXZ.z);
	xFacingOut =
		Zenith_Maths::AngleAxis(fYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	return true;
}

// Report how fast this NPC is actually being COMMANDED to move, so the shared
// human animator can blend Idle <-> Walk. Reporting ZERO while stationary is the
// whole point: without it a standing trainer moon-walks. Silent when there is no
// animator, which is every blockout and every cold-start fallback block.
void ZM_Interactable::DriveAnimatorSpeed(float fSpeed)
{
	if (Zenith_AnimatorComponent* pxAnimator =
		m_xParentEntity.TryGetComponent<Zenith_AnimatorComponent>())
	{
		pxAnimator->SetFloat("Speed", fSpeed);
	}
}

void ZM_Interactable::DriveTrainerApproach(
	const ZM_InteractableBodyObservation& xBody,
	const ZM_TrainerApproachStep& xStep,
	Zenith_TransformComponent* pxTransform)
{
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	const Zenith_Maths::Vector3 xCurrentVelocity =
		xPhysics.GetLinearVelocity(xBody.m_xBodyID);
	// ★ THE SAME TWO-CALL IDIOM AS UpdateWander, and for the same reason: no
	// SetPosition and no teleport anywhere, so gravity and terrain response stay in
	// the SOLE ownership of the body's vertical component. ZM_BuildPatrolVelocity
	// replaces XZ and copies Y verbatim.
	xPhysics.SetLinearVelocity(xBody.m_xBodyID,
		ZM_BuildPatrolVelocity(xStep.m_xDirXZ, xStep.m_fSpeed, xCurrentVelocity));
	DriveAnimatorSpeed(xStep.m_fSpeed);

	// FACE THE TRAVEL DIRECTION. On the arrival tick the direction is exactly zero
	// and TryBuildApproachFacing REFUSES, which leaves the facing the previous tick
	// authored -- i.e. still looking at the player -- rather than snapping north.
	Zenith_Maths::Quat xFacing(1.0f, 0.0f, 0.0f, 0.0f);
	if (pxTransform != nullptr
		&& TryBuildApproachFacing(xStep.m_xDirXZ, xFacing))
	{
		// SetRotation mirrors onto the physics body (Zenith_TransformComponent
		// keeps the body authoritative when one exists), so this is the single
		// write that moves both.
		pxTransform->SetRotation(xFacing);
	}
}

void ZM_Interactable::HoldTrainerStation(
	const ZM_InteractableBodyObservation& xBody,
	Zenith_TransformComponent* pxTransform)
{
	// A PATROL OWNS ITS OWN VELOCITY. UpdateWander runs immediately after this tick
	// and would overwrite anything written here anyway; the station hold exists for
	// the AUTHORED STATIONARY trainer, which is what SC8 placed in Dawnmere.
	if (m_bWanderEnabled)
	{
		return;
	}

	Zenith_Physics& xPhysics = g_xEngine.Physics();
	const Zenith_Maths::Vector3 xCurrentVelocity =
		xPhysics.GetLinearVelocity(xBody.m_xBodyID);
	// ★ R1, THE TRANSLATION HALF. A DYNAMIC body can be shoved, and a rival nudged
	// out of his authored spot degrades every geometric claim the placement header
	// makes (corridor clearances, the whiteout spawn-camp margin) without redding
	// anything. Zeroing the XZ request every tick he is not walking means a push
	// costs one frame of contact response rather than an open-ended slide -- and it
	// is the SAME helper, with the same Y-preserving contract, that the walk itself
	// goes through, so there is no second velocity convention to keep in step.
	xPhysics.SetLinearVelocity(xBody.m_xBodyID,
		ZM_BuildPatrolVelocity(
			Zenith_Maths::Vector3(0.0f), 0.0f, xCurrentVelocity));
	DriveAnimatorSpeed(0.0f);

	// ★ R1, THE ROTATION HALF, and it is deliberately WATCHING-ONLY.
	//   * WATCHING is the only state that can raise, so it is the only state in
	//     which a stolen yaw makes the trainer permanently blind.
	//   * Repairing in SPOTTED / CHALLENGING / ENGAGED would fight the presentation
	//     beats (and, in the runtime-fixture tests, the deliberate rotations that
	//     break sight mid-beat).
	//   * It is gated on a live DYNAMIC CAPSULE by the caller, so a bodyless trainer
	//     -- the collider-less runtime fixture -- keeps its rotation entirely in the
	//     hands of whoever wrote it. Nothing repairs damage physics cannot do.
	// The write is conditional on measured drift, so the common case is a dot
	// product and no body write at all.
	if (pxTransform == nullptr
		|| !m_bWatchFacingCaptured
		|| m_xSightFsm.GetState() != ZM_TRAINER_SIGHT_WATCHING)
	{
		return;
	}
	Zenith_Maths::Quat xCurrentFacing(1.0f, 0.0f, 0.0f, 0.0f);
	pxTransform->GetRotation(xCurrentFacing);
	if (ZM_FacingAbsDot(xCurrentFacing, m_xWatchFacing)
		< fZM_WATCH_FACING_MIN_ABS_DOT)
	{
		pxTransform->SetRotation(m_xWatchFacing);
	}
}

void ZM_Interactable::AcquireTrainerCinematicHold(Zenith_EntityID xPlayerID)
{
	// RE-VALIDATED ON EVERY TICK OF THE WALK. Begin is last-writer-wins with no
	// counter, and SetMovementEnabled is a bare bool, so re-application inside one
	// logical operation is a no-op by construction -- which is exactly the property
	// the "latch consumed at the top of a re-enterable function" lesson demands.
	ZM_TrainerCinematicLatch::Begin(m_eTrainerId);
	m_bCinematicHoldActive = true;
	if (xPlayerID != INVALID_ENTITY_ID)
	{
		m_xCinematicHeldPlayerID = xPlayerID;
	}
	if (ZM_PlayerController* pxController =
			ZM_ResolvePlayerController(m_xCinematicHeldPlayerID))
	{
		pxController->SetMovementEnabled(false);
	}
}

void ZM_Interactable::ReleaseTrainerCinematicHold()
{
	// TOTAL: releasing a hold nobody took is legal and inert. The early-out is what
	// lets every exit -- arrival, timeout, cancel, a lost trainer row, a restarted
	// component -- funnel through ONE call without any of them having to know
	// whether one of the others got there first.
	if (!m_bCinematicHoldActive)
	{
		return;
	}
	m_bCinematicHoldActive = false;
	const Zenith_EntityID xPlayerID = m_xCinematicHeldPlayerID;
	m_xCinematicHeldPlayerID = INVALID_ENTITY_ID;

	// The latch goes down FIRST so the arbitration below reads the truth: with it
	// still armed, this function would always decline to hand movement back and the
	// player would be stranded frozen -- the one failure mode the latch is shaped to
	// make impossible.
	ZM_TrainerCinematicLatch::End();

	// THE SAME GUARD LIST ZM_UI_MenuStack::UnfreezePlayer CONSULTS, for the same
	// reason: m_bMovementEnabled is one bare bool with four owners and no refcount.
	// IsMenuOpen is included because a bark raised BY THIS WALK freezes through the
	// menu, and the menu's own close path is what must hand movement back then.
	if (ZM_GameStateManager::IsWarpInProgress()
		|| ZM_BattleTransition::IsTransitionActive()
		|| ZM_UI_MenuStack::IsMenuOpen())
	{
		return;
	}
	if (ZM_PlayerController* pxController = ZM_ResolvePlayerController(xPlayerID))
	{
		pxController->SetMovementEnabled(true);
	}
}

void ZM_Interactable::OnUpdate(float fDeltaTime)
{
	// ORDER IS LOAD-BEARING. The sight tick runs FIRST and, crucially, OUTSIDE
	// UpdateWander's `if (!m_bWanderEnabled) { return; }` early-out: SC8 authors a
	// STATIONARY trainer, and folding this call into the walker body would leave
	// every such trainer permanently blind while every unit test still passed.
	TickTrainerSight(fDeltaTime);
	UpdateWander(fDeltaTime);

	SubmitNameText(m_xParentEntity.IsValid()
		? m_xParentEntity.TryGetComponent<Zenith_TransformComponent>()
		: nullptr);
}

void ZM_Interactable::SubmitNameText(Zenith_TransformComponent* pxTransform)
{
	if (m_eNpcId >= ZM_NPC_COUNT || !pxTransform)
	{
		return;
	}

	Zenith_UI::Zenith_UICanvas* pxCanvas = Zenith_UI::Zenith_UICanvas::GetPrimaryCanvas();
	if (!pxCanvas)
	{
		return;
	}

	// Anchor a little above the BODY's top rather than the entity's own (centre)
	// position, so the tag floats above the NPC's head rather than through its
	// chest -- the same "anchor + fixed offset" idiom
	// RenderTest_TennisMatchComponent::SubmitScoreText uses for the net-height
	// score anchor.
	//
	// ★ THE BODY CONTRACT, NOT THE TRANSFORM SCALE. The scale describes how large
	// the MODEL is drawn, which is a uniform factor with no relationship to how
	// tall a person is; reading it here would drop the tag straight through the
	// chest. Every human is fZM_HUMAN_BODY_HEIGHT tall, STANDING ON its origin.
	constexpr float fHEAD_MARGIN = 0.35f;
	Zenith_Maths::Vector3 xPosition;
	pxTransform->GetPosition(xPosition);
	// The transform is the FEET, so the crown is a WHOLE body height up -- not a
	// half. Getting this wrong parks the tag in the navel rather than over the head.
	const Zenith_Maths::Vector3 xAnchor(xPosition.x,
		xPosition.y + fZM_HUMAN_BODY_HEIGHT + fHEAD_MARGIN, xPosition.z);

	const Zenith_Maths::Matrix4 xViewProj = g_xEngine.FluxGraphics().GetViewProjMatrix();
	const Zenith_Maths::Vector4 xClip = xViewProj * Zenith_Maths::Vector4(xAnchor, 1.0f);
	if (xClip.w <= 0.0001f)
	{
		return;
	}

	int iWidth = 1280;
	int iHeight = 720;
	if (Zenith_Window::GetInstance())
	{
		Zenith_Window::GetInstance()->GetSize(iWidth, iHeight);
	}

	const float fNdcX = xClip.x / xClip.w;
	const float fNdcY = xClip.y / xClip.w;
	const Zenith_Maths::Vector2 xPixelPos(
		(fNdcX * 0.5f + 0.5f) * static_cast<float>(iWidth),
		(fNdcY * 0.5f + 0.5f) * static_cast<float>(iHeight));

	const float fSize = glm::clamp(700.0f / xClip.w, 12.0f, 28.0f);
	const char* szName = ZM_GetNpcData(m_eNpcId).m_szDisplayName;

	// SubmitText's position is the LEFT edge of the string (Flux_TextQueue has no
	// alignment concept of its own -- Zenith_UIText::GetTextWidth does this same
	// per-glyph-advance sum to centre a UI label). Shift left by half the string's
	// width so xPixelPos lands in the MIDDLE of the name, matching the head anchor,
	// rather than the tag reading off to the right of the NPC.
	const float fTextWidth = static_cast<float>(std::strlen(szName))
		* fSize * Zenith_FontAsset::GetActiveOrDefaultMetrics().fEmAdvance;
	const Zenith_Maths::Vector2 xCenteredPixelPos(xPixelPos.x - fTextWidth * 0.5f, xPixelPos.y);

	pxCanvas->SubmitText(szName, xCenteredPixelPos, fSize, Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f));
}

u_int ZM_Interactable::SubmitTrainerSpottedIndicator(
	const Zenith_Maths::Vector3& xTrainerCenter)
{
	// A non-finite CENTRE is refused outright rather than fed to Flux. The live
	// path cannot produce one -- ZM_IsTargetInTrainerSight fails closed on any
	// non-finite position before the FSM can reach SPOTTED -- but this helper is
	// public and static precisely so a unit can call it, so it guards itself
	// instead of trusting every present and future caller.
	if (!ZM_IsFiniteVector3(xTrainerCenter))
	{
		return 0u;
	}

	// ★ THE BODY CONTRACT, NOT THE TRANSFORM SCALE. A human transform is centred on
	// its body, and the body's size is compiled -- the scale only says how large the
	// model is drawn. Deriving the head height from scale used to work only while
	// the two were the same number, and would now park the "!" inside the ribcage.
	// It also cannot be poisoned by a bad presentation scale, which is why the old
	// non-finite fallback is gone rather than replaced.
	// xTrainerCenter is a body CENTRE (see its derivation), so the top of the head
	// is one HALF height above it. This one is unchanged by the feet-origin move
	// precisely because it starts from a centre rather than from a transform.
	const float fTop = xTrainerCenter.y + fZM_HUMAN_BODY_HALF_HEIGHT;
	const Zenith_Maths::Vector3 xDotCenter(
		xTrainerCenter.x, fTop + fZM_SPOTTED_DOT_OFFSET, xTrainerCenter.z);
	const Zenith_Maths::Vector3 xStemStart(
		xTrainerCenter.x, fTop + fZM_SPOTTED_STEM_START_OFFSET, xTrainerCenter.z);
	const Zenith_Maths::Vector3 xStemEnd(
		xTrainerCenter.x, fTop + fZM_SPOTTED_STEM_END_OFFSET, xTrainerCenter.z);

	Flux_PrimitivesImpl& xPrimitives = g_xEngine.Primitives();
	// A solid cylinder is readable from every player-camera yaw. The old stem used
	// Flux's flat debug-line quad, whose fixed world-space face and start-anchored
	// transform made the exclamation mark collapse or read diagonally at oblique
	// angles. Both parts now use the dedicated gameplay queues, which stay visible
	// when a tools user unchecks Graphics/Primitives/Enabled.
	// ★ THE RESULT IS MEASURED OFF FLUX, NOT ASSERTED. The composite append and
	// both queue-size samples share Flux's instance mutex with the render-thread
	// drain, so a drain cannot split the payload or erase it before measurement.
	// Every live caller += this exact result; there is no proxy increment beside it.
	return xPrimitives.SubmitGameplayCylinderAndSphere(
		xStemStart, xStemEnd, fZM_SPOTTED_STEM_RADIUS,
		xDotCenter, fZM_SPOTTED_DOT_RADIUS, xZM_SPOTTED_COLOUR);
}

void ZM_Interactable::TickTrainerSight(float fDeltaTime)
{
	if (!IsTrainerSightEnabled())
	{
		// RELEASED BEFORE THE EARLY-OUT, never after it. The hold belongs to the
		// WALK, and a component that has lost its trainer row has no walk in flight
		// -- so a row cleared mid-approach (ConfigureTrainerSight failing closed on a
		// bad id) must not leave the player frozen with nobody able to name why.
		ReleaseTrainerCinematicHold();
		m_bApproachPossible = false;
		return;
	}

	const ZM_TrainerData& xRow = ZM_GetTrainerData(m_eTrainerId);

	// The gate's three observations. No reachable game state means NOTHING HAS
	// HAPPENED YET -- the identical ruling Interact() already makes for story-gated
	// dialogue. Every defeat flag then reads clear, so a flagged trainer IS
	// engageable, which is exactly the answer a fresh save gives.
	ZM_GameState* pxGameState = nullptr;
	const bool bHasGameState =
		ZM_GameStateManager::TryGetGameState(pxGameState) && pxGameState != nullptr;
	const bool bDefeatFlagSet =
		bHasGameState && ZM_IsStoryFlagSet(*pxGameState, xRow.m_eDefeatFlag);
	const bool bLatchSet = ZM_TrainerEngagementLatch::HasEngaged(m_eTrainerId);

	// The SAME "NOTHING HAS HAPPENED YET" ruling the two lines above make: with no
	// reachable game state the player is treated as ABLE to battle, which is the
	// pre-SC3 answer at every site. A `false` here would silently disable every
	// trainer in any scene that runs without a GameStateManager -- a live behaviour
	// change in exactly the windowed tests this commit must not move.
	//
	// ★ NO LONGER INERT -- THIS ARM IS NOW A REAL PLAYTHROUGH STATE (ZM-D-188).
	// SC3 landed this wiring with a note that "a later sub-commit makes production
	// partyless"; that sub-commit is the intro beat. The two PRODUCTION seed sites no
	// longer grant a starter (ZM_GameStateManager::OnStart and RequestNewGame), so a
	// real player walks out of PlayerHome with an EMPTY party and past rival Vesper's
	// 8 m cone on the way to the lab. ZM_CanEnterBattle is false for that whole stretch
	// and this is the clause that keeps him silent through it -- a partyless player
	// dragged into a battle has nothing to send out.
	//
	// The pure proof is IntroBeat_PartylessNewGamePlayerIsNeverEngagedByATrainer
	// (Tests/ZM_Tests_IntroBeat.cpp), which drives ZM_MayTrainerEngage with
	// ZM_CanEnterBattle(ZM_MakeNewGameState()) rather than a hand-written false; the
	// live proof is the Dawnmere leg of ZM_IntroBeat_Test.
	const bool bPlayerCanBattle = !bHasGameState || ZM_CanEnterBattle(*pxGameState);

	ZM_TrainerSightInputs xInputs;
	xInputs.m_fDeltaSeconds = fDeltaTime;
	xInputs.m_bMayEngage =
		ZM_MayTrainerEngage(xRow, bDefeatFlagSet, bLatchSet, bPlayerCanBattle);
	// THE PRECONDITION ACCESSORS. IsTransitionActive() is the only public window
	// onto the battle machine (there is NO accessor for its two pending latches),
	// so the FSM additionally treats a raise as unconfirmed until the channel is
	// observed BUSY -- see ZM_TrainerSightFsmTuning::m_fRaiseConfirmSeconds.
	xInputs.m_bChannelBusy = ZM_BattleTransition::IsTransitionActive()
		|| ZM_GameStateManager::IsWarpInProgress()
		|| ZM_UI_MenuStack::IsMenuOpen();

	// S7 item 3 SC7. Does this trainer have anything to SAY? After the shared W3
	// visual beat, a row with no challenge lines skips CHALLENGING and raises.
	const char* const* paszChallengeLines = nullptr;
	u_int uChallengeLineCount = 0u;
	ZM_SelectTrainerChallengeLines(xRow, paszChallengeLines, uChallengeLineCount);
	xInputs.m_bChallengeAvailable = (uChallengeLineCount > 0u);
	if (xInputs.m_bChallengeAvailable)
	{
		// EAGER, not lazy-at-the-dramatic-moment: the .bgraph load is synchronous and
		// Zenith_AssetRegistry caches it, so paying it on this trainer's first
		// play-mode tick puts any hitch at scene entry rather than on the frame the
		// player is spotted.
		EnsureTrainerChallengeGraph();
	}

	Zenith_TransformComponent* pxTransform =
		m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
	Zenith_EntityID xPlayerID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 xPlayerPosition(0.0f);
	Zenith_Maths::Quat xPlayerRotation(1.0f, 0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xTrainerPosition(0.0f);
	bool bHaveTrainerTransform = false;
	const bool bHavePlayer = ZM_InteractionRuntime::TryResolveActivePlayer(
		xPlayerID, xPlayerPosition, xPlayerRotation);
	if (pxTransform != nullptr && bHavePlayer)
	{
		Zenith_Maths::Quat xTrainerRotation(1.0f, 0.0f, 0.0f, 0.0f);
		pxTransform->GetPosition(xTrainerPosition);
		pxTransform->GetRotation(xTrainerRotation);
		bHaveTrainerTransform = true;

		// The SC3 PURE cone, unmodified and unduplicated. Rotation form, never
		// glm::eulerAngles(quat).y.
		xInputs.m_bTargetInSight = ZM_IsTargetInTrainerSightFromRotation(
			xTrainerPosition, xTrainerRotation, xPlayerPosition,
			ZM_TrainerSightTuning{});
		if (xInputs.m_bTargetInSight)
		{
			// The ray enters HERE and only here, AFTER the pure cone passed. There
			// is no raycast budget anywhere in this engine, so cheap-gate-first IS
			// the cost control.
			// ★ EYE TO EYE, NOT FOOT TO FOOT. Both transforms are the FEET, and a ray
			// cast between two points ON THE GROUND grazes the terrain for its whole
			// length -- the occlusion probe then reports every trainer permanently
			// blind, with the pure cone still passing and nothing else red. Both ends
			// lift to the body centre, which is where the old body-centre transforms
			// already put them and is the only height at which "can these two see
			// each other" is a question about the world rather than about the floor.
			const ZM_TrainerSightProbeResult xProbe = ZM_ProbeTrainerSightLine(
				ZM_HumanBodyCentre(xTrainerPosition), m_xParentEntity.GetEntityID(),
				ZM_HumanBodyCentre(xPlayerPosition), xPlayerID);
			xInputs.m_bSightLineClear = xProbe.m_bClear;
		}
	}

	// ---- S7 item 1 SC3: the two walk-up inputs, filled BEFORE the machine rules --
	//
	// ★ FAIL OPEN WITH NO BODY. m_bApproachPossible is the WHOLE compatibility
	// story: false means ZM_TrainerSightFsm skips APPROACHING entirely and this
	// component keeps the pre-SC1 SPOTTED exit byte for byte. It is therefore also
	// the gate on every physics write below -- a trainer this component cannot drive
	// is a trainer this component does not touch, which is what keeps the
	// collider-less runtime fixture meaningful rather than merely tolerated.
	const ZM_InteractableBodyObservation xBody = ObserveDrivableBody();
	xInputs.m_bApproachPossible = IsDrivableBodyContractMet(
		xBody.m_bEntityValid, xBody.m_bColliderPresent, xBody.m_bBodyValid,
		xBody.m_eVolumeType, xBody.m_eRigidBodyType, xBody.m_bPhysicsActive);
	m_bApproachPossible = xInputs.m_bApproachPossible;
	if (xInputs.m_bApproachPossible)
	{
		// The SAME once-per-body-identity setup the patrol gets, through the SAME
		// function. Applied here rather than at the moment the walk starts, because
		// the R1 yaw lock has to be in place BEFORE the first shove, not after the
		// first sighting.
		ApplyDrivenBodySetup(xBody);
	}

	// ★ ARRIVED IS THE DEFAULT, matching ZM_StepTrainerApproach's own fail-open
	// polarity: "I cannot measure the gap" and "I have walked far enough" must look
	// identical to the machine, because the only thing it does with this bool is
	// STOP WAITING. A walk whose body or transform disappears mid-flight therefore
	// ends on the next tick instead of burning the whole timeout on a dead screen.
	ZM_TrainerApproachStep xApproach;
	xApproach.m_bArrived = true;
	if (xInputs.m_bApproachPossible && bHaveTrainerTransform)
	{
		xApproach = ZM_StepTrainerApproach(xTrainerPosition, xPlayerPosition,
			xZM_SIGHT_TUNING.m_fApproachStandoffMetres,
			fZM_TRAINER_APPROACH_SPEED);
	}
	xInputs.m_bApproachArrived = xApproach.m_bArrived;

	const ZM_TRAINER_SIGHT_ACTION eAction =
		m_xSightFsm.Step(xInputs, xZM_SIGHT_TUNING);
	if (bHaveTrainerTransform
		&& m_xSightFsm.GetState() == ZM_TRAINER_SIGHT_SPOTTED)
	{
		// The counter is the helper's OWN measurement of what reached Flux's CPU
		// queues, never a bare increment beside the call: deleting the submission
		// cannot leave this advancing, so a live test watching this counter is
		// watching the actual renderer payload rather than a proxy for it.
		m_uSpottedIndicatorSubmitCount +=
			SubmitTrainerSpottedIndicator(xTrainerPosition);
	}

	// ---- S7 item 1 SC3: THE WALK, driven AFTER the machine has ruled ------------
	//
	// ★ KEYED ON THE STATE, NOT ON A TRANSITION, AND THAT IS THE WHOLE DESIGN. This
	// block asks "is the operation running?" every single tick, so re-entering
	// TickTrainerSight inside one logical walk re-applies a no-op instead of
	// stacking a second claim, and EVERY exit -- arrival, timeout, and cancel alike
	// -- lands in the same release. A release keyed on "did I just see the
	// APPROACHING -> X edge?" would have three sites to keep in step, and the one
	// that got forgotten would strand the player frozen. This project has already
	// been bitten by a latch consumed at the top of a function its own state machine
	// re-enters (PollForSpawnAndPlacePlayer re-ran its teleport on every pass); the
	// lesson is that lifetime belongs to the operation.
	if (m_xSightFsm.GetState() == ZM_TRAINER_SIGHT_APPROACHING)
	{
		AcquireTrainerCinematicHold(xPlayerID);
		if (m_bApproachPossible)
		{
			DriveTrainerApproach(xBody, xApproach, pxTransform);
		}
	}
	else
	{
		ReleaseTrainerCinematicHold();
		if (m_bApproachPossible)
		{
			HoldTrainerStation(xBody, pxTransform);
		}
	}

	switch (eAction)
	{
	case ZM_TRAINER_SIGHT_ACTION_RUN_CHALLENGE:
		// NO latch and NO dispatch on this arm. The bark is a presentation beat, not
		// the encounter: the engagement latch is still burnt on the RAISE alone, so an
		// abandoned or unheard bark never costs a flagless trainer his one session
		// shot.
		RunTrainerChallenge();
		return;

	case ZM_TRAINER_SIGHT_ACTION_RAISE_ENCOUNTER:
		break;

	default:
		return;
	}

	// ---- UNCHANGED FROM SC6 BELOW THIS LINE ----
	// Latch BEFORE dispatching: Dispatch is SYNCHRONOUS (the subscriber runs
	// inside this stack frame), so the latch must already be true if anything
	// downstream ever re-enters this component.
	ZM_TrainerEngagementLatch::MarkEngaged(m_eTrainerId);
	// This component owns no dialogue: it asks the GRAPH to speak and lets
	// ZM_UI_MenuStack own that freeze, then lets
	// ZM_BattleTransition::TryParkOverworldPlayer own the battle freeze. The shipped
	// subscriber owns both halves of the battle side: OnTrainerEncounterEvent
	// latches, and TryParkOverworldPlayer zeroes velocity, drops gravity and calls
	// SetMovementEnabled(false). The two owners are strictly SEQUENTIAL -- ECS order
	// 112 (ZM_UI_MenuStack) closes, pops and UNFREEZES before order 113 (this
	// component) dispatches, in the SAME frame.
	//
	// ★ S7 item 1 SC3 UPDATED THIS PARAGRAPH RATHER THAN CONTRADICTING IT. What SC6
	// forbade -- and it was right to -- was a THIRD freeze CONCEPT arbitrated by
	// nothing. The walk-up is instead the FOURTH OWNER of the one shared bool, and
	// it arbitrates through the one shared question SC2 added to
	// ZM_UI_MenuStack::UnfreezePlayer (ZM_TrainerCinematicLatch::IsActive()). By the
	// time control reaches THIS line the hold has already been released above, so
	// there is still exactly one freeze owner at every instant, and the dispatch
	// below still lands on a player nobody else has claimed.
	Zenith_EventDispatcher::Get().Dispatch(
		ZM_OnTrainerEncounter{ m_eTrainerId, ZM_ResolveActiveSceneIdForSight() });
}

void ZM_Interactable::DeriveTrainerFromNpcRow()
{
	// ZM_GetNpcData is NOT total -- it asserts -- so bounds-check first. Do NOT
	// mirror ZM_GetTrainerData's forgiving UNKNOWN-row shape here.
	if (m_eNpcId >= ZM_NPC_COUNT)
	{
		return;
	}
	// FILL-IF-EMPTY: a runtime-configured trainer WINS. See the header.
	if (ZM_IsRegisteredTrainer(m_eTrainerId))
	{
		return;
	}
	const ZM_TRAINER_ID eRowTrainer = ZM_GetNpcData(m_eNpcId).m_eTrainer;
	if (!ZM_IsRegisteredTrainer(eRowTrainer))
	{
		return;
	}
	// Routed through the ONE validating installer rather than assigning
	// m_eTrainerId directly: ConfigureTrainerSight fails closed, resets the sight
	// machine and re-arms the graph latch, and keeping a single assignment site is
	// what stops the authored and runtime paths drifting apart.
	ConfigureTrainerSight(eRowTrainer);
}

void ZM_Interactable::EnsureTrainerChallengeGraph()
{
	if (m_bChallengeGraphAttempted)
	{
		return;
	}
	// Set BEFORE the work, not after: a failed load must not be retried every frame.
	m_bChallengeGraphAttempted = true;

	if (!m_xParentEntity.IsValid())
	{
		return;
	}

	Zenith_GraphComponent* pxGraph =
		m_xParentEntity.TryGetComponent<Zenith_GraphComponent>();
	if (pxGraph == nullptr)
	{
		// Grows a DIFFERENT pool from this component's, so `this` cannot relocate;
		// the precedent for adding a component from inside a component body is
		// DPProcLevelBootstrap_Component.cpp:341-371 and Combat_GameComponent.h:713.
		pxGraph = &m_xParentEntity.AddComponent<Zenith_GraphComponent>();
	}

	// IDEMPOTENT BY PATH, not by flag alone: a scene reload or a future authored
	// slot could already carry this graph, and two live copies would double-bark.
	for (u_int u = 0u; u < pxGraph->GetGraphCount(); ++u)
	{
		const char* szPath = pxGraph->GetGraphAssetPathAt(u);
		if (szPath != nullptr
			&& std::strcmp(szPath, szZM_GRAPH_TRAINER_CHALLENGE_ASSET) == 0)
		{
			return;
		}
	}

	// A load failure appends an UNRESOLVED slot and returns null (the unresolved-slot
	// contract, Scripting/CLAUDE.md). That is the FAIL-OPEN case -- on a _False or
	// Android build, or a fresh clone with no tools boot, the asset simply is not
	// there. The trainer loses his line and still starts the battle.
	pxGraph->AddGraphByAssetPath(szZM_GRAPH_TRAINER_CHALLENGE_ASSET);
}

void ZM_Interactable::RunTrainerChallenge()
{
	Zenith_GraphComponent* pxGraph = m_xParentEntity.IsValid()
		? m_xParentEntity.TryGetComponent<Zenith_GraphComponent>()
		: nullptr;
	if (pxGraph == nullptr)
	{
		return;   // FAIL OPEN -- the FSM's challenge window raises the encounter.
	}

	// TARGETED, never Zenith_GraphComponent::BroadcastCustomEvent: that walks
	// QueryAllScenes<Zenith_GraphComponent> (Zenith_GraphComponent.cpp:210-215) and
	// would also reach the 31 engine UnitTest_*.bgraph components sitting in
	// Games/Zenithmon/Assets/Graphs/.
	Zenith_PropertyValue xPayload;
	xPayload.SetInt32((int32_t)(u_int)m_eTrainerId);
	pxGraph->FireCustomEvent(szZM_GRAPH_EVENT_TRAINER_SPOTTED, &xPayload);
}

void ZM_Interactable::UpdateWander(float fDeltaTime)
{
	if (!m_bWanderEnabled)
	{
		return;
	}
	// This is the runtime-ready point: unlike construction/OnStart, an enabled
	// walker reaching an update must already own the exact body it promises.
	if (!TryConfigureWanderBody(true))
	{
		return;
	}
	if (!std::isfinite(fDeltaTime) || fDeltaTime <= 0.0f)
	{
		return;
	}

	Zenith_TransformComponent* pxTransform =
		m_xParentEntity.TryGetComponent<Zenith_TransformComponent>();
	Zenith_ColliderComponent* pxCollider =
		m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	// TryConfigureWanderBody just proved the collider/simulation contract on this
	// same main-thread tick; the transform is the engine's mandatory entity pose.
	if (pxTransform == nullptr)
	{
		return;
	}

	// Interact() latches ownership only after THIS component successfully raises a
	// screen. Another NPC or the pause menu therefore cannot stop this patrol. Keep
	// the authored enabled observation true; halting is an input to the pure step.
	const bool bMenuOpen = ZM_UI_MenuStack::IsMenuOpen();
	const bool bHalted = m_bOwnsInteractionMenu && bMenuOpen;
	if (!bMenuOpen)
	{
		m_bOwnsInteractionMenu = false;
	}

	Zenith_Maths::Vector3 xPosition(0.0f);
	pxTransform->GetPosition(xPosition);
	const ZM_WalkerStep xStep = ZM_StepWalker(
		m_xWalkerWaypoints, m_xWalkerState, xPosition,
		fDeltaTime, bHalted, m_xWalkerTuning);

	const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
	const Zenith_Maths::Vector3 xCurrentVelocity =
		xPhysics.GetLinearVelocity(xBodyID);
	// No SetPosition / teleport: the dynamic capsule alone owns translation. The
	// helper replaces XZ while preserving Y verbatim for gravity/terrain response.
	xPhysics.SetLinearVelocity(xBodyID,
		ZM_BuildPatrolVelocity(xStep.m_xDirXZ, xStep.m_fSpeed, xCurrentVelocity));
	DriveAnimatorSpeed(xStep.m_fSpeed);
}

bool ZM_Interactable::SetNpcId(ZM_NPC_ID eId)
{
	if (eId >= ZM_NPC_COUNT)
	{
		// Fail CLOSED: clearing the row (rather than keeping the previous one) means a
		// bad authoring value produces an inert NPC, never a wrong conversation.
		m_eNpcId = ZM_NPC_NONE;
		m_bInteractable = false;
		return false;
	}
	m_eNpcId = eId;
	return true;
}

bool ZM_Interactable::SetRadius(float fRadius)
{
	if (!std::isfinite(fRadius))
	{
		m_fRadius = fDEFAULT_RADIUS;
		return false;
	}
	if (fRadius < 0.0f)
	{
		m_fRadius = 0.0f;
		return false;
	}
	if (fRadius > fMAX_RADIUS)
	{
		m_fRadius = fMAX_RADIUS;
		return false;
	}
	m_fRadius = fRadius;
	return true;
}

bool ZM_Interactable::ConfigureWander(const ZM_WalkerWaypoints& xWaypoints,
	const ZM_WalkerTuning& xTuning)
{
	m_xWalkerState = ZM_WalkerState{};
	m_bOwnsInteractionMenu = false;
	if (!ZM_IsValidWanderConfiguration(xWaypoints, xTuning))
	{
		m_xWalkerWaypoints = ZM_WalkerWaypoints{};
		m_xWalkerTuning = ZM_WalkerTuning{};
		m_bWanderEnabled = false;
		m_xConfiguredWanderBodyID = Zenith_PhysicsBodyID{};
		return false;
	}

	m_xWalkerWaypoints = xWaypoints;
	m_xWalkerTuning = xTuning;
	m_bWanderEnabled = true;
	// Configure immediately when this is a live, already-started entity with its
	// body ready. Missing pre-body editor state remains harmless until OnUpdate's
	// strict runtime gate; it is still serialized so the completed scene can reload.
	if (m_bLifecycleStarted)
	{
		TryConfigureWanderBody(false);
	}
	return true;
}

bool ZM_Interactable::ConfigureTrainerSight(ZM_TRAINER_ID eTrainer)
{
	// The machine is about to become a cold watcher, so any walk it was running is
	// over -- including the freeze that walk was holding. Released FIRST, and
	// through the one TOTAL release, so installing a new row can never be the thing
	// that strands the player. Inert unless this component was actually holding one.
	ReleaseTrainerCinematicHold();
	m_xSightFsm.Reset();
	m_uSpottedIndicatorSubmitCount = 0u;
	m_bApproachPossible = false;
	// Re-arm the one-shot attach with the machine: the incoming row may be the first
	// one on this component that has anything to say.
	m_bChallengeGraphAttempted = false;
	if (!ZM_IsRegisteredTrainer(eTrainer))
	{
		// Fail CLOSED, exactly as SetNpcId does: CLEAR the row rather than keeping
		// the previous one, so a bad authoring value yields a blind NPC and never
		// the WRONG trainer's battle.
		m_eTrainerId = ZM_TRAINER_NONE;
		return false;
	}
	m_eTrainerId = eTrainer;
	return true;
}

bool ZM_Interactable::Interact()
{
	if (m_eNpcId >= ZM_NPC_COUNT)
	{
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_Interactable] Interact on an UNCONFIGURED interactable (no NPC row) -- nothing raised");
		return false;
	}

	const ZM_NpcData& xRow = ZM_GetNpcData(m_eNpcId);
	const ZM_NPC_RAISE_KIND eKind = ZM_RaiseKindForRole(xRow.m_eRole);

	bool bRaised = false;
	switch (eKind)
	{
	case ZM_NPC_RAISE_DIALOGUE:
	{
		// Which lines this row says depends on the LIVE story flags. Gating selects
		// CONTENT only -- it never re-routes which seam a role talks through, so
		// ZM_RaiseKindForRole above is untouched by it.
		//
		// No reachable game state means an all-clear flag set -- a manager-less
		// context (a headless dispatch unit, or anything running before the
		// singleton exists) is treated as "NOTHING HAS HAPPENED YET". Every
		// require-SET gate therefore fails CLOSED under it, so an NPC guarding a
		// story beat says its GATED lines rather than leaking content the player has
		// not earned; a require-CLEAR gate PASSES under it by construction, which is
		// exactly the answer a fresh save would give.
		const ZM_StoryFlagSet xNoFlags{};
		ZM_GameState* pxGameState = nullptr;
		const bool bHasGameState =
			ZM_GameStateManager::TryGetGameState(pxGameState) && pxGameState != nullptr;
		const ZM_StoryFlagSet& xFlags =
			bHasGameState ? pxGameState->m_xStoryFlags : xNoFlags;

		// ---- S8 item 1: THE LAB BEAT --------------------------------------------
		//
		// ★★ THE PICKER IS PUSHED **FIRST** AND THE LINES **SECOND**, WHICH IS THE
		// REVERSE OF THE ORDER THE PLAYER EXPERIENCES THEM -- AND THAT IS THE WHOLE
		// TRICK. ZM_MenuScreenStack is a LIFO: the screen pushed first is the one
		// reached LAST. So this order gives, in play order, Aster's pre-starter lines
		// and THEN the picker they invite -- with no cutscene system, no pending-action
		// enum, no new ZM_MENU_SCREEN and no new raise seam. Swap the two calls and the
		// player picks a starter before being told why, then reads the invitation.
		//
		// ★ AND THE PICKER IS RAISED BESIDE THE DIALOGUE, NEVER INSTEAD OF IT. A row's
		// ZM_StoryGate selects CONTENT only -- ZM_RaiseKindForRole is untouched by any of
		// this and Aster remains a TALKER whose seam is TryPushDialogue.
		//
		// ★ EXACTLY ONE FREEZE OWNER IS TAKEN. OpenStarterChoiceScreen freezes only when
		// the stack was EMPTY; PushDialogueLines then sees a non-empty stack and does
		// not freeze again. m_bMovementEnabled is a bare bool with no refcount, so a
		// second claim here would be a bug, not redundancy.
		const bool bStarterRaised =
			ZM_NpcRaisesStarterChoice(m_eNpcId, xFlags)
			&& ZM_UI_MenuStack::TryOpenStarterChoiceScreen();

		const char* const* paszLines = nullptr;
		u_int uLineCount = 0u;
		ZM_SelectNpcLines(xRow, xFlags, paszLines, uLineCount);
		// The OR, not an AND: a refused dialogue (a full queue, an armed prompt) must
		// not report "nothing was raised" while a starter picker the player cannot
		// cancel out of is sitting on the stack.
		bRaised = ZM_UI_MenuStack::TryPushDialogue(paszLines, uLineCount)
			|| bStarterRaised;
		break;
	}
	case ZM_NPC_RAISE_SHOP:
		bRaised = ZM_UI_MenuStack::TryOpenShop(xRow.m_paeStock, xRow.m_uStockCount);
		break;
	case ZM_NPC_RAISE_CARE_CENTER:
		bRaised = ZM_UI_MenuStack::TryOpenCareCenterPrompt();
		break;
	default:
		// An unmapped role is CONTENT breakage, not a runtime condition: it must be
		// loud rather than a silent no-op (Shortfalls 1.6).
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_Interactable] NPC '%s' (id %u) has UNMAPPED role %u -- no seam to raise",
			xRow.m_szDisplayName, (u_int)m_eNpcId, (u_int)xRow.m_eRole);
		return false;
	}

	if (!bRaised)
	{
		// The seam refused. Previously this was silent, so a mis-authored NPC read as
		// a mute one with no diagnostic anywhere; name the NPC and the seam it tried.
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_Interactable] NPC '%s' (id %u, role %u) could not raise its %s screen "
			"-- the seam refused (no ZM_MenuRoot singleton, or the screen rejected its content)",
			xRow.m_szDisplayName, (u_int)m_eNpcId, (u_int)xRow.m_eRole,
			ZM_NpcRaiseKindName(eKind));
	}
	m_bOwnsInteractionMenu = bRaised;
	return bRaised;
}

void ZM_Interactable::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
	xStream << (u_int)m_eNpcId;
	xStream << m_fRadius;
	xStream << m_bInteractable;
	xStream << m_bWanderEnabled;
	xStream << m_xWalkerWaypoints.m_uCount;
	for (u_int u = 0u; u < ZM_WalkerWaypoints::uMAX_WAYPOINTS; ++u)
	{
		xStream << m_xWalkerWaypoints.m_axPoints[u].x;
		xStream << m_xWalkerWaypoints.m_axPoints[u].y;
		xStream << m_xWalkerWaypoints.m_axPoints[u].z;
	}
	xStream << m_xWalkerTuning.m_fSpeed;
	xStream << m_xWalkerTuning.m_fArriveRadius;
	xStream << m_xWalkerTuning.m_fDwellSeconds;
}

void ZM_Interactable::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0u;
	xStream >> uVersion;
	m_eNpcId = ZM_NPC_NONE;
	m_fRadius = fDEFAULT_RADIUS;
	m_bInteractable = false;
	m_xWalkerWaypoints = ZM_WalkerWaypoints{};
	m_xWalkerTuning = ZM_WalkerTuning{};
	m_xWalkerState = ZM_WalkerState{};
	m_bWanderEnabled = false;
	m_bOwnsInteractionMenu = false;
	m_xConfiguredWanderBodyID = Zenith_PhysicsBodyID{};
	// The trainer identity and its watcher are RUNTIME-ONLY (uSERIALIZATION_VERSION
	// deliberately stays at 2u and WriteToDataStream never touches either), so a
	// reloaded scene starts with NO trainer and a COLD watcher -- deterministic, and
	// identical to a fresh component. Guarded by
	// Interactable_TrainerSightIsNotSerialized.
	m_eTrainerId = ZM_TRAINER_NONE;
	m_xSightFsm.Reset();
	m_uSpottedIndicatorSubmitCount = 0u;
	// SC7's graph-attach latch rides in the SAME runtime-only block, for the same
	// reason: a reloaded scene starts with no trainer, a cold watcher and no graph.
	m_bChallengeGraphAttempted = false;
	// S7 item 1 SC3's walk-up state rides there too, and the HOLD is released rather
	// than merely cleared: dropping the flag alone would leave the process-global
	// latch armed with nobody left who could end it.
	ReleaseTrainerCinematicHold();
	m_bApproachPossible = false;
	m_bWatchFacingCaptured = false;
	if (uVersion != 1u && uVersion != uSERIALIZATION_VERSION)
	{
		return;
	}

	u_int uNpcId = (u_int)ZM_NPC_NONE;
	float fRadius = fDEFAULT_RADIUS;
	bool bInteractable = false;
	xStream >> uNpcId;
	xStream >> fRadius;
	xStream >> bInteractable;

	// Route every field through the validating setters, so a hand-edited or stale
	// scene file cannot install a state the live setters would have refused.
	SetNpcId((ZM_NPC_ID)uNpcId);
	SetRadius(fRadius);
	SetInteractable(bInteractable);

	// Version 1 scenes predate patrols; their NPCs deliberately remain stationary.
	if (uVersion == 1u)
	{
		return;
	}

	bool bWanderEnabled = false;
	u_int uWaypointCount = 0u;
	ZM_WalkerWaypoints xWaypoints;
	ZM_WalkerTuning xTuning;
	xStream >> bWanderEnabled;
	xStream >> uWaypointCount;
	for (u_int u = 0u; u < ZM_WalkerWaypoints::uMAX_WAYPOINTS; ++u)
	{
		xStream >> xWaypoints.m_axPoints[u].x;
		xStream >> xWaypoints.m_axPoints[u].y;
		xStream >> xWaypoints.m_axPoints[u].z;
	}
	xStream >> xTuning.m_fSpeed;
	xStream >> xTuning.m_fArriveRadius;
	xStream >> xTuning.m_fDwellSeconds;
	xWaypoints.m_uCount = uWaypointCount;

	if (ZM_IsValidWanderConfiguration(xWaypoints, xTuning))
	{
		m_xWalkerWaypoints = xWaypoints;
		m_xWalkerTuning = xTuning;
		m_bWanderEnabled = bWanderEnabled;
		if (m_bWanderEnabled && m_bLifecycleStarted)
		{
			TryConfigureWanderBody(false);
		}
	}
}

#ifdef ZENITH_TOOLS
void ZM_Interactable::RenderPropertiesPanel()
{
	const bool bHasRow = m_eNpcId < ZM_NPC_COUNT;
	ImGui::Text("NPC: %s (id %u)",
		bHasRow ? ZM_GetNpcData(m_eNpcId).m_szDisplayName : "<none>", (u_int)m_eNpcId);
	ImGui::Text("Role raise kind: %s", ZM_NpcRaiseKindName(bHasRow
		? ZM_RaiseKindForRole(ZM_GetNpcData(m_eNpcId).m_eRole)
		: ZM_NPC_RAISE_NONE));
	ImGui::Text("Reach bonus: %.2f", m_fRadius);
	ImGui::Text("Interactable: %s", IsInteractable() ? "true" : "false");
	ImGui::Text("Wander enabled: %s", IsWanderEnabled() ? "true" : "false");
	ImGui::Text("Waypoint: %u / %u", GetWaypointIndex(), GetWaypointCount());
	// S7 item 1 SC3. "Approach possible" is the DYNAMIC-CAPSULE body contract as of
	// the last sight tick, so an authored trainer who silently lost his capsule (the
	// exact regression the collider change here is about) is visible in the editor
	// rather than only in a windowed test's timeout.
	ImGui::Text("Trainer approach: possible=%s count=%u elapsed=%.2f hold=%s",
		IsTrainerApproachPossible() ? "true" : "false",
		GetTrainerApproachCount(), GetTrainerApproachElapsedSeconds(),
		IsTrainerCinematicHoldActive() ? "true" : "false");
}
#endif
