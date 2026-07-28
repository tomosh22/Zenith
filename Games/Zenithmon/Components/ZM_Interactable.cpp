#include "Zenith.h"

#include "Zenithmon/Components/ZM_Interactable.h"

#include "Core/Multithreading/Zenith_Multithreading.h"   // Zenith_ScopedMutexLock (the primitive-queue sample)
#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_GraphComponent.h"   // the runtime graph host
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_EventSystem.h"            // Zenith_EventDispatcher -- the encounter channel
#include "ZenithECS/Zenith_SceneSystem.h"            // GetActiveScene / GetSceneInfo (the source-scene lookup)
#include "Zenithmon/Components/ZM_BattleTransition.h"   // IsTransitionActive -- the busy-channel input
#include "Zenithmon/Components/ZM_GameStateManager.h"   // TryGetGameState -- the live story flags
#include "Zenithmon/Components/ZM_UI_MenuStack.h"   // the three shipped raise seams
#include "Zenithmon/Source/Data/ZM_StoryFlags.h"    // ZM_StoryFlagSet (the gate's input)
#include "Zenithmon/Source/Data/ZM_WorldSpec.h"      // ZM_SCENE_ID / ZM_FindSceneByBuildIndex
#include "Zenithmon/Source/Graph/ZM_GraphAuthoring.h"             // the shared name constants
#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"   // TryResolveActivePlayer -- THE player seam
#include "Zenithmon/Source/Interaction/ZM_TrainerSightLogic.h"    // the SC3 pure cone (unmodified)
#include "Zenithmon/Source/Interaction/ZM_TrainerSightProbe.h"    // the occlusion filter
#include "Zenithmon/Source/World/ZM_EncounterEvents.h"            // ZM_OnTrainerEncounter

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
	constexpr float fZM_SPOTTED_LINE_START_OFFSET = 0.55f;
	constexpr float fZM_SPOTTED_LINE_END_OFFSET = 1.20f;
	constexpr float fZM_SPOTTED_LINE_THICKNESS = 0.10f;
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

ZM_Interactable::ZM_Interactable(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_Interactable::OnStart()
{
	m_bLifecycleStarted = true;
	m_xConfiguredWanderBodyID = Zenith_PhysicsBodyID{};
	m_xWalkerState = ZM_WalkerState{};
	m_bOwnsInteractionMenu = false;
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

bool ZM_Interactable::TryConfigureWanderBody(bool bRequireRuntimeReady)
{
	if (!m_bWanderEnabled)
	{
		return false;
	}

	const bool bEntityValid = m_xParentEntity.IsValid();
	Zenith_ColliderComponent* pxCollider =
		bEntityValid
			? m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>()
			: nullptr;
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	const bool bColliderPresent = pxCollider != nullptr;
	const bool bBodyValid = bColliderPresent && pxCollider->HasValidBody();
	const bool bDynamicCapsule = bColliderPresent
		&& pxCollider->GetCollisionVolumeType() == COLLISION_VOLUME_TYPE_CAPSULE
		&& pxCollider->GetRigidBodyType() == RIGIDBODY_TYPE_DYNAMIC;
	const bool bPhysicsActive = xPhysics.HasActiveSimulation();
	const bool bContractValid = bEntityValid
		&& bColliderPresent
		&& bBodyValid
		&& bDynamicCapsule
		&& bPhysicsActive;
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
				(u_int)m_eNpcId, (int)bEntityValid, (int)bColliderPresent,
				(int)bBodyValid, (int)bDynamicCapsule, (int)bPhysicsActive);
			// Release builds compile the assertion out, so the explicit state change is
			// the shipping fail-closed path rather than a debug-only diagnostic.
			m_bWanderEnabled = false;
		}
		return false;
	}

	const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
	if (m_xConfiguredWanderBodyID == xBodyID)
	{
		return true;
	}

	pxCollider->SetIsSensor(false);
	xPhysics.SetGravityEnabled(xBodyID, true);
	xPhysics.LockRotation(xBodyID, true, false, true);
	xPhysics.EnforceUpright(xBodyID);
	xPhysics.SetFriction(xBodyID, fZM_WANDER_BODY_FRICTION);
	xPhysics.SetRestitution(xBodyID, fZM_WANDER_BODY_RESTITUTION);
	m_xConfiguredWanderBodyID = xBodyID;
	return true;
}

void ZM_Interactable::OnUpdate(float fDeltaTime)
{
	// ORDER IS LOAD-BEARING. The sight tick runs FIRST and, crucially, OUTSIDE
	// UpdateWander's `if (!m_bWanderEnabled) { return; }` early-out: SC8 authors a
	// STATIONARY trainer, and folding this call into the walker body would leave
	// every such trainer permanently blind while every unit test still passed.
	TickTrainerSight(fDeltaTime);
	UpdateWander(fDeltaTime);
}

u_int ZM_Interactable::SubmitTrainerSpottedIndicator(
	const Zenith_Maths::Vector3& xTrainerCenter,
	const Zenith_Maths::Vector3& xTrainerScale)
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

	// The greybox NPC transform is centred on the model. Absolute Y scale handles
	// mirrored authoring, and a non-finite value falls back to unit height so a bad
	// presentation scale cannot poison Flux's instance buffers.
	const float fHeight = std::isfinite(xTrainerScale.y)
		? std::fabs(xTrainerScale.y)
		: 1.0f;
	const float fTop = xTrainerCenter.y + fHeight * 0.5f;
	const Zenith_Maths::Vector3 xDotCenter(
		xTrainerCenter.x, fTop + fZM_SPOTTED_DOT_OFFSET, xTrainerCenter.z);
	const Zenith_Maths::Vector3 xLineStart(
		xTrainerCenter.x, fTop + fZM_SPOTTED_LINE_START_OFFSET, xTrainerCenter.z);
	const Zenith_Maths::Vector3 xLineEnd(
		xTrainerCenter.x, fTop + fZM_SPOTTED_LINE_END_OFFSET, xTrainerCenter.z);

	Flux_PrimitivesImpl& xPrimitives = g_xEngine.Primitives();
	u_int uLineBefore = 0u;
	u_int uSphereBefore = 0u;
	{
		Zenith_ScopedMutexLock xLock(xPrimitives.m_xInstanceMutex);
		uLineBefore = xPrimitives.m_xLineInstances.GetSize();
		uSphereBefore = xPrimitives.m_xSphereInstances.GetSize();
	}

	xPrimitives.AddLine(
		xLineStart, xLineEnd, xZM_SPOTTED_COLOUR, fZM_SPOTTED_LINE_THICKNESS);
	xPrimitives.AddSphere(xDotCenter, fZM_SPOTTED_DOT_RADIUS, xZM_SPOTTED_COLOUR);

	// ★ THE RESULT IS MEASURED OFF FLUX, NOT ASSERTED. Every live test watches the
	// caller's per-frame counter, and that counter is fed from THIS return value --
	// so a submission that never reached the renderer cannot be reported as one.
	// A counter incremented merely BESIDE the two Add calls would have left the
	// whole live contract satisfiable with the calls deleted, which is exactly the
	// hole this closes. Both queues only ever GROW between the two samples, so a
	// concurrent producer can inflate the delta but can never hide a missing
	// primitive of ours.
	Zenith_ScopedMutexLock xLock(xPrimitives.m_xInstanceMutex);
	return (xPrimitives.m_xLineInstances.GetSize() > uLineBefore
			&& xPrimitives.m_xSphereInstances.GetSize() > uSphereBefore)
		? 1u
		: 0u;
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
	Zenith_Maths::Vector3 xTrainerScale(1.0f);
	bool bHaveTrainerTransform = false;
	const bool bHavePlayer = ZM_InteractionRuntime::TryResolveActivePlayer(
		xPlayerID, xPlayerPosition, xPlayerRotation);
	if (pxTransform != nullptr && bHavePlayer)
	{
		Zenith_Maths::Quat xTrainerRotation(1.0f, 0.0f, 0.0f, 0.0f);
		pxTransform->GetPosition(xTrainerPosition);
		pxTransform->GetRotation(xTrainerRotation);
		pxTransform->GetScale(xTrainerScale);
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
			const ZM_TrainerSightProbeResult xProbe = ZM_ProbeTrainerSightLine(
				xTrainerPosition, m_xParentEntity.GetEntityID(),
				xPlayerPosition, xPlayerID);
			xInputs.m_bSightLineClear = xProbe.m_bClear;
		}
	}

	const ZM_TRAINER_SIGHT_ACTION eAction =
		m_xSightFsm.Step(xInputs, ZM_TrainerSightFsmTuning{});
	if (bHaveTrainerTransform
		&& m_xSightFsm.GetState() == ZM_TRAINER_SIGHT_SPOTTED)
	{
		// The counter is the helper's OWN measurement of what reached Flux's CPU
		// queues, never a bare increment beside the call: deleting the submission
		// cannot leave this advancing, so a live test watching this counter is
		// watching the actual renderer payload rather than a proxy for it.
		m_uSpottedIndicatorSubmitCount +=
			SubmitTrainerSpottedIndicator(xTrainerPosition, xTrainerScale);
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
	// This component still does NOT freeze the player and still owns no dialogue: it
	// asks the GRAPH to speak and lets ZM_UI_MenuStack own that freeze, then lets
	// ZM_BattleTransition::TryParkOverworldPlayer own the battle freeze. The shipped
	// subscriber owns both halves of the battle side: OnTrainerEncounterEvent
	// latches, and TryParkOverworldPlayer zeroes velocity, drops gravity and calls
	// SetMovementEnabled(false). The two owners are strictly SEQUENTIAL -- ECS order
	// 112 (ZM_UI_MenuStack) closes, pops and UNFREEZES before order 113 (this
	// component) dispatches, in the SAME frame -- and SC7 added neither. Adding a
	// third freeze owner here would fight the menu/warp/battle protocol those
	// systems already coordinate over.
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
	m_xSightFsm.Reset();
	m_uSpottedIndicatorSubmitCount = 0u;
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

		const char* const* paszLines = nullptr;
		u_int uLineCount = 0u;
		ZM_SelectNpcLines(xRow, xFlags, paszLines, uLineCount);
		bRaised = ZM_UI_MenuStack::TryPushDialogue(paszLines, uLineCount);
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
}
#endif
