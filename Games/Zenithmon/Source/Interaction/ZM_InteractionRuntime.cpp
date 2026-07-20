#include "Zenith.h"

#include "Zenithmon/Source/Interaction/ZM_InteractionRuntime.h"

#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_BattleTransition.h"   // IsTransitionActive -- the SAME predicate ShouldOpenMenu consults
#include "Zenithmon/Components/ZM_GameStateManager.h"   // IsWarpInProgress    -- ditto
#include "Zenithmon/Components/ZM_Interactable.h"
#include "Zenithmon/Components/ZM_PlayerController.h"   // IsMovementEnabled (the freeze flag)
#include "Zenithmon/Components/ZM_UI_MenuStack.h"       // IsMenuOpen / IsActiveSceneOverworld -- ditto
#include "Zenithmon/Source/ZM_InputActions.h"           // ReadInteractPressed

// ============================================================================
// ZM_InteractionRuntime (S6 item 3 SC4). See the header for the contract.
//
// Every world predicate below is the SAME one ZM_UI_MenuStack::OnUpdate feeds into
// ShouldOpenMenu -- IsMenuOpen / IsActiveSceneOverworld / IsWarpInProgress /
// IsTransitionActive. Reusing them (rather than hand-rolling a parallel
// build-index -> scene -> kind resolve here) is what stops the pause gate and the
// interaction gate from drifting apart.
// ============================================================================

ZM_INTERACT_REJECT ZM_InteractionRuntime::s_eLastResult = ZM_INTERACT_REJECT_NO_INPUT_EDGE;
Zenith_EntityID    ZM_InteractionRuntime::s_xLastTarget = INVALID_ENTITY_ID;
u_int              ZM_InteractionRuntime::s_uRaiseCount = 0u;
bool               ZM_InteractionRuntime::s_bHasLatchedResult = false;

void ZM_InteractionRuntime::Tick(const Zenith_Maths::Vector3& xPlayerPosition,
	const Zenith_Maths::Quat& xPlayerRotation)
{
	const bool bInteractPressed = ZM_InputActions::ReadInteractPressed();

	Zenith_EntityID xTarget = INVALID_ENTITY_ID;
	const ZM_INTERACT_REJECT eResult = Decide(
		/* bHavePose */ true,
		xPlayerPosition,
		xPlayerRotation,
		bInteractPressed,
		xTarget);

	// Latch the last ATTEMPT, not the last frame. Writing unconditionally would
	// clobber the result on every no-edge frame -- and during a walk-up nobody is
	// pressing E, so the latch would sit permanently at NO_INPUT_EDGE / INVALID.
	// A later windowed Step asserting a NEGATIVE ("no raise happened") would then
	// pass against those clobbered values whether or not the feature works, which is
	// this project's most-repeated failure mode. Continuous proximity polling is
	// EvaluateForTests' job; the latch answers "what did the last press DO?".
	//
	// s_bHasLatchedResult still moves every tick, so it remains an honest "the
	// runtime ran" signal (and keeps the reset unit non-vacuous).
	s_bHasLatchedResult = true;
	if (bInteractPressed)
	{
		s_eLastResult = eResult;
		s_xLastTarget = xTarget;
	}

	if (eResult != ZM_INTERACT_OK)
	{
		return;
	}

	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xTarget);
	ZM_Interactable* pxInteractable = xEntity.IsValid()
		? xEntity.TryGetComponent<ZM_Interactable>()
		: nullptr;
	if (pxInteractable == nullptr)
	{
		return;
	}

	// The raise count moves ONLY when a screen genuinely went up, so it cannot be
	// satisfied by a decision that merely said OK.
	if (pxInteractable->Interact())
	{
		++s_uRaiseCount;
	}
}

ZM_INTERACT_REJECT ZM_InteractionRuntime::EvaluateForTests(Zenith_EntityID& xTargetOut) const
{
	xTargetOut = INVALID_ENTITY_ID;

	// The pose is PASSED THROUGH rather than gated on here. This seam has no owner
	// transform, so it resolves the pose from the active scene -- which legitimately
	// fails on FrontEnd, mid-warp before the player spawns, and in the additive
	// battle scene. Rejecting here would report DEGENERATE_ORIGIN in all three,
	// masking the honest NOT_OVERWORLD / WARP_IN_PROGRESS that the LIVE path returns
	// and that SC5+ pollers wait on. Decide runs the world gate first and only then
	// consults bHavePose.
	Zenith_Maths::Vector3 xPosition(0.0f);
	Zenith_Maths::Quat xRotation(1.0f, 0.0f, 0.0f, 0.0f);
	const bool bHavePose = TryResolveActivePlayerPose(xPosition, xRotation);

	// bInteractPressed is ASSUMED true: the question this seam answers is "what would
	// pressing E right now do?", not "was E pressed?". Reading the real edge here
	// would make the seam report NO_INPUT_EDGE on every polling frame and be useless
	// for deciding WHEN to press.
	return Decide(bHavePose, xPosition, xRotation, /* bInteractPressed */ true, xTargetOut);
}

ZM_INTERACT_REJECT ZM_InteractionRuntime::GetLastResult() const
{
	return s_eLastResult;
}

Zenith_EntityID ZM_InteractionRuntime::GetLastTarget() const
{
	return s_xLastTarget;
}

u_int ZM_InteractionRuntime::GetRaiseCount() const
{
	return s_uRaiseCount;
}

bool ZM_InteractionRuntime::HasLatchedResult() const
{
	return s_bHasLatchedResult;
}

void ZM_InteractionRuntime::ResetRuntimeStateForTests()
{
	s_eLastResult = ZM_INTERACT_REJECT_NO_INPUT_EDGE;
	s_xLastTarget = INVALID_ENTITY_ID;
	s_uRaiseCount = 0u;
	s_bHasLatchedResult = false;
}

ZM_INTERACT_REJECT ZM_InteractionRuntime::Decide(bool bHavePose,
	const Zenith_Maths::Vector3& xPlayerPosition,
	const Zenith_Maths::Quat& xPlayerRotation,
	bool bInteractPressed,
	Zenith_EntityID& xTargetOut) const
{
	xTargetOut = INVALID_ENTITY_ID;

	// ---- 1. The world gate (SC1). All six inputs snapshotted here, in one place. --
	const ZM_INTERACT_REJECT eGate = ZM_ShouldInteract(
		bInteractPressed,
		ZM_UI_MenuStack::IsMenuOpen(),
		ZM_UI_MenuStack::IsActiveSceneOverworld(),
		ZM_GameStateManager::IsWarpInProgress(),
		ZM_BattleTransition::IsTransitionActive(),
		ResolveActivePlayerMovementEnabled());
	if (eGate != ZM_INTERACT_OK)
	{
		return eGate;
	}

	// AFTER the gate, never before: a blocked world is the more informative answer,
	// and the two are routinely true together (no unique player resolves precisely
	// when the scene is FrontEnd / mid-warp / the battle scene).
	if (!bHavePose)
	{
		return ZM_INTERACT_REJECT_DEGENERATE_ORIGIN;
	}

	// ---- 2. Build the by-value probe set from the ACTIVE scene. -------------------
	ZM_InteractProbe axProbes[uZM_MAX_INTERACT_PROBES];
	Zenith_EntityID  axProbeEntities[uZM_MAX_INTERACT_PROBES];
	u_int uProbeCount = 0u;
	u_int uSeenCount = 0u;
	g_xEngine.Scenes().QueryActiveScene<ZM_Interactable, Zenith_TransformComponent>().ForEach(
		[&](Zenith_EntityID xEntityID,
			ZM_Interactable& xInteractable,
			Zenith_TransformComponent& xTransform)
		{
			++uSeenCount;
			if (uProbeCount >= uZM_MAX_INTERACT_PROBES)
			{
				return;   // overflow reported once, below
			}
			Zenith_Maths::Vector3 xPosition(0.0f);
			xTransform.GetPosition(xPosition);
			axProbes[uProbeCount].m_xPosition = xPosition;
			axProbes[uProbeCount].m_fRadius   = xInteractable.GetRadius();
			axProbes[uProbeCount].m_bEnabled  = xInteractable.IsInteractable();
			axProbeEntities[uProbeCount] = xEntityID;
			++uProbeCount;
		});

	if (uSeenCount > uZM_MAX_INTERACT_PROBES)
	{
		// Taking the first 64 is a deliberate, LOUD degradation: a scene this dense is
		// an authoring mistake, and silently dropping candidates would make one NPC
		// unreachable for reasons nobody could see.
		Zenith_Assert(false,
			"ZM_InteractionRuntime: active scene holds more interactables than the probe cap");
		Zenith_Warning(LOG_CATEGORY_GAMEPLAY,
			"[ZM_InteractionRuntime] %u interactables in the active scene exceeds the cap of %u "
			"-- only the first %u are considered",
			uSeenCount, uZM_MAX_INTERACT_PROBES, uZM_MAX_INTERACT_PROBES);
	}

	// ---- 3. The candidate picker (SC2). -----------------------------------------
	ZM_InteractOrigin xOrigin;
	xOrigin.m_xPosition = xPlayerPosition;
	xOrigin.m_xForward  = ZM_ForwardFromRotation(xPlayerRotation);

	const ZM_InteractTuning xTuning;   // seeded from the shipped fZM_INTERACT_* constants
	u_int uBestIndex = uProbeCount;
	const ZM_INTERACT_REJECT ePick = ZM_PickInteractTarget(
		axProbes, uProbeCount, xOrigin, xTuning, uBestIndex);
	if (ePick == ZM_INTERACT_OK && uBestIndex < uProbeCount)
	{
		xTargetOut = axProbeEntities[uBestIndex];
	}
	return ePick;
}

bool ZM_InteractionRuntime::TryResolveActivePlayerPose(Zenith_Maths::Vector3& xPositionOut,
	Zenith_Maths::Quat& xRotationOut)
{
	// Deliberately NOT ZM_GameStateManager::TryGetUniqueActiveScenePlayerEntityID:
	// that seam additionally demands a live physics body (it exists to authenticate a
	// COLLIDING player), which is exactly the dependency interaction does not have and
	// must not acquire -- it would make the whole runtime unobservable headlessly.
	Zenith_EntityID xPlayerID = INVALID_ENTITY_ID;
	u_int uPlayerCount = 0u;
	g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController, Zenith_TransformComponent>().ForEach(
		[&](Zenith_EntityID xEntityID, ZM_PlayerController&, Zenith_TransformComponent&)
		{
			++uPlayerCount;
			if (uPlayerCount == 1u)
			{
				xPlayerID = xEntityID;
			}
		});
	if (uPlayerCount != 1u)
	{
		return false;
	}

	Zenith_Entity xPlayer = g_xEngine.Scenes().ResolveEntity(xPlayerID);
	Zenith_TransformComponent* pxTransform = xPlayer.IsValid()
		? xPlayer.TryGetComponent<Zenith_TransformComponent>()
		: nullptr;
	if (pxTransform == nullptr)
	{
		return false;
	}
	pxTransform->GetPosition(xPositionOut);
	pxTransform->GetRotation(xRotationOut);
	return true;
}

bool ZM_InteractionRuntime::ResolveActivePlayerMovementEnabled()
{
	ZM_PlayerController* pxController = nullptr;
	u_int uPlayerCount = 0u;
	g_xEngine.Scenes().QueryActiveScene<ZM_PlayerController>().ForEach(
		[&](Zenith_EntityID, ZM_PlayerController& xController)
		{
			++uPlayerCount;
			if (uPlayerCount == 1u)
			{
				pxController = &xController;
			}
		});
	if (uPlayerCount != 1u || pxController == nullptr)
	{
		// Fail OPEN -- see the header. The authoritative freeze guard is the CALL SITE
		// (Tick runs after ZM_PlayerController::OnUpdate's frozen early-out), so an
		// unresolvable controller must not fabricate a PLAYER_FROZEN reject that the
		// live game would never produce.
		return true;
	}
	return pxController->IsMovementEnabled();
}
