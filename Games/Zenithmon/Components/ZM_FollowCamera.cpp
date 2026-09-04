#include "Zenith.h"

#include "Zenithmon/Source/World/ZM_HumanBody.h"   // the body contract the pivot is measured from
#include "Zenithmon/Components/ZM_FollowCamera.h"

#include "Core/Zenith_Engine.h"
#include "DataStream/Zenith_DataStream.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_PhysicsQuery.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Components/ZM_PlayerController.h"   // the target is resolved BY COMPONENT
#include "Zenithmon/Source/Gen/ZM_InteriorGen.h"        // ZM_GetInteriorRoomSpec -- the ceiling height
#include "Zenithmon/Source/World/ZM_InteriorDressing.h" // the shell entity names this scene may wear

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

#include <cmath>

namespace
{
	bool IsFiniteVector(const Zenith_Maths::Vector3& xValue)
	{
		return std::isfinite(xValue.x)
			&& std::isfinite(xValue.y)
			&& std::isfinite(xValue.z);
	}

	float FiniteOr(float fValue, float fFallback)
	{
		return std::isfinite(fValue) ? fValue : fFallback;
	}
}

ZM_FollowCamera::ZM_FollowCamera(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_FollowCamera::OnStart()
{
	ResetRuntimeState();
	CaptureAuthoredYaw();
	ResolveCeiling();
	ResolveTarget();
}

// Which room, if any, this camera's own scene is wearing -- and therefore how
// high its ceiling is.
//
// ★ RESOLVED BY WALKING THIS SCENE FOR AN INTERIOR SHELL, not by asking which
// scene this is. ZM_RoomForShellEntity is TOTAL over entity names and answers
// ZM_INTERIOR_ROOM_COUNT for anything that is not a shell, so an outdoor scene
// resolves to "no ceiling" by construction rather than by a list of scene names
// somebody has to remember to extend. A future interior gets the fix for free the
// moment it authors a shell, which is the same thing that gives it a ceiling to
// begin with.
//
// ★ ONCE, AT OnStart. The shell is authored, so it exists the instant the scene
// activates, and a room's height does not change while it is loaded.
void ZM_FollowCamera::ResolveCeiling()
{
	m_fCeilingY = fNO_CEILING;

	Zenith_SceneData* pxSceneData =
		g_xEngine.Scenes().GetSceneDataForEntity(m_xParentEntity.GetEntityID());
	if (pxSceneData == nullptr)
	{
		return;
	}

	for (u_int uRoom = 0u; uRoom < (u_int)ZM_INTERIOR_ROOM_COUNT; ++uRoom)
	{
		const ZM_INTERIOR_ROOM eRoom = (ZM_INTERIOR_ROOM)uRoom;
		const char* szShell = (eRoom == ZM_INTERIOR_ROOM_PROF_LAB)
			? szZM_PROFLAB_SHELL_ENTITY_NAME
			: szZM_PLAYERHOME_SHELL_ENTITY_NAME;

		const Zenith_Entity xShell = pxSceneData->FindEntityByName(szShell);
		if (!xShell.IsValid())
		{
			continue;
		}

		// The room's own wall height, read from the generator's spec rather than
		// re-spelled: the ceiling slab is built at exactly m_fWallHeight
		// (ZM_IGBuildCeiling), so this is the plane the lens must stay under.
		const float fWallHeight = ZM_GetInteriorRoomSpec(eRoom).m_fWallHeight;
		if (std::isfinite(fWallHeight) && fWallHeight > 0.0f)
		{
			m_fCeilingY = fWallHeight;
			Zenith_Log(LOG_CATEGORY_GAMEPLAY,
				"[ZM_FollowCamera] interior '%s' -> ceiling %.2f m, lens capped at "
				"%.2f m (outdoors it would sit %.2f m above the player)",
				szShell, (double)m_fCeilingY,
				(double)(m_fCeilingY - fCEILING_CLEARANCE), (double)fCAMERA_HEIGHT);
		}
		return;
	}
}

void ZM_FollowCamera::OnLateUpdate(float fDeltaTime)
{
	Zenith_CameraComponent* pxCamera =
		m_xParentEntity.TryGetComponent<Zenith_CameraComponent>();
	if (pxCamera == nullptr)
	{
		return;
	}

	if (!m_bAuthoredYawCaptured)
	{
		CaptureAuthoredYaw();
	}

	Zenith_Entity xTarget = ResolveTarget();
	if (!xTarget.IsValid())
	{
		return;
	}

	Zenith_TransformComponent* pxTargetTransform =
		xTarget.TryGetComponent<Zenith_TransformComponent>();
	if (pxTargetTransform == nullptr)
	{
		return;
	}

	Zenith_Maths::Vector3 xPlayerPosition;
	pxTargetTransform->GetPosition(xPlayerPosition);
	if (!IsFiniteVector(xPlayerPosition))
	{
		return;
	}

	// ★ DERIVED ONCE, AND FED TO BOTH. The transform is the player's FEET; both
	// helpers below measure from the body CENTRE (fPIVOT_HEIGHT and fCAMERA_HEIGHT
	// are both stated against it, and every authored camera constant in the
	// placement headers was derived that way). Converting for one and not the other
	// is the exact bug this line exists to make impossible: the pivot would hold
	// while the eye dropped half a body, changing every camera's height AND its
	// pitch, with no test on the arm LENGTH able to see it.
	const Zenith_Maths::Vector3 xBodyCentre = ZM_HumanBodyCentre(xPlayerPosition);

	const Zenith_Maths::Vector3 xPivot = ComputePivot(xBodyCentre);
	// The outdoor boom, then the indoor cap. Outdoors the second call is the
	// identity; indoors it is the difference between seeing the room and seeing
	// the top of its ceiling slab. See ClampBoomBelowCeiling in the header.
	const Zenith_Maths::Vector3 xDesiredPosition = ClampBoomBelowCeiling(
		ComputeDesiredPosition(xBodyCentre, m_fAuthoredYaw),
		xPivot,
		m_fCeilingY);

	bool bSnap = !m_bSpringInitialised || !m_bHasPreviousTargetPosition;
	if (!bSnap)
	{
		const Zenith_Maths::Vector3 xTargetDelta =
			xPlayerPosition - m_xPreviousTargetPosition;
		const float fSnapDistanceSq =
			fTELEPORT_SNAP_DISTANCE * fTELEPORT_SNAP_DISTANCE;
		bSnap = glm::dot(xTargetDelta, xTargetDelta) > fSnapDistanceSq;
	}

	if (bSnap)
	{
		m_xSpringPosition = xDesiredPosition;
		m_xSpringVelocity = Zenith_Maths::Vector3(0.0f);
		m_bSpringInitialised = true;
	}
	else
	{
		m_xSpringPosition = StepCriticalSpring(
			m_xSpringPosition, xDesiredPosition, m_xSpringVelocity, fDeltaTime);
	}

	m_bCollisionConstrained = false;
	Zenith_Maths::Vector3 xArm = m_xSpringPosition - xPivot;
	float fArmDistance = glm::length(xArm);
	if (std::isfinite(fArmDistance) && fArmDistance > 0.0001f)
	{
		const Zenith_Maths::Vector3 xArmDirection = xArm / fArmDistance;
		const Zenith_Maths::Vector3 xDesiredArm = xDesiredPosition - xPivot;
		const float fDesiredArmDistance = glm::length(xDesiredArm);
		float fClampedDistance = fArmDistance;
		if (std::isfinite(fDesiredArmDistance) && fDesiredArmDistance > 0.0001f)
		{
			// Query the full authored arm so an obstruction remains reported while
			// the spring sits inside its limit. Query the spring candidate as well
			// because target motion can temporarily put it on a different ray.
			const Zenith_Maths::Vector3 xDesiredArmDirection =
				xDesiredArm / fDesiredArmDistance;
			const Zenith_Physics::RaycastResult xDesiredHit =
				Zenith_PhysicsQuery::RaycastIgnoring(
					xPivot, xDesiredArmDirection, fDesiredArmDistance,
					m_xTargetEntityID);
			const float fDesiredLimit = ClampArmDistance(
				fDesiredArmDistance, xDesiredHit.m_bHit,
				xDesiredHit.m_fDistance);
			m_bCollisionConstrained =
				fDesiredLimit + 0.0001f < fDesiredArmDistance;
			fClampedDistance = glm::min(fClampedDistance, fDesiredLimit);
		}

		const Zenith_Physics::RaycastResult xCandidateHit =
			Zenith_PhysicsQuery::RaycastIgnoring(
				xPivot, xArmDirection, fArmDistance, m_xTargetEntityID);
		const float fCandidateLimit = ClampArmDistance(
			fArmDistance, xCandidateHit.m_bHit, xCandidateHit.m_fDistance);
		m_bCollisionConstrained = m_bCollisionConstrained
			|| fCandidateLimit + 0.0001f < fArmDistance;
		fClampedDistance = glm::min(fClampedDistance, fCandidateLimit);
		if (fClampedDistance + 0.0001f < fArmDistance)
		{
			// Obstructions move the camera inward immediately so it never eases
			// through geometry. The spring resumes from this clamped state and
			// therefore recovers outward smoothly when the obstruction clears.
			m_xSpringPosition = xPivot + xArmDirection * fClampedDistance;
			m_xSpringVelocity = Zenith_Maths::Vector3(0.0f);
			fArmDistance = fClampedDistance;
			m_bCollisionConstrained = true;
		}
	}
	else
	{
		// A corrupt/non-finite spring state must never reach the Camera.
		m_xSpringPosition = xDesiredPosition;
		m_xSpringVelocity = Zenith_Maths::Vector3(0.0f);
		fArmDistance = glm::length(m_xSpringPosition - xPivot);
	}

	const ZM_FollowCameraPose xPose = BuildLookAtPose(
		m_xSpringPosition,
		xPivot,
		m_fAuthoredYaw,
		static_cast<float>(pxCamera->GetPitch()));
	pxCamera->SetPosition(xPose.m_xPosition);
	pxCamera->SetYaw(xPose.m_fYaw);
	pxCamera->SetPitch(xPose.m_fPitch);
	pxCamera->SetFOV(glm::radians(fFOV_DEGREES));

	m_fCurrentArmDistance = std::isfinite(fArmDistance) ? fArmDistance : 0.0f;
	m_xPreviousTargetPosition = xPlayerPosition;
	m_bHasPreviousTargetPosition = true;
}

void ZM_FollowCamera::OnDestroy()
{
	ResetRuntimeState();
}

void ZM_FollowCamera::WriteToDataStream(Zenith_DataStream& xStream) const
{
	const u_int uComponentVersion = 1u;
	xStream << uComponentVersion;
}

void ZM_FollowCamera::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uComponentVersion = 0u;
	xStream >> uComponentVersion;
	ResetRuntimeState();
}

#ifdef ZENITH_TOOLS
void ZM_FollowCamera::RenderPropertiesPanel()
{
	if (m_xTargetEntityID == INVALID_ENTITY_ID)
	{
		ImGui::TextUnformatted("Target: unresolved");
	}
	else
	{
		ImGui::Text("Target: %u:%u", m_xTargetEntityID.m_uIndex,
			m_xTargetEntityID.m_uGeneration);
	}
	ImGui::Text("Authored yaw: %.3f rad", m_fAuthoredYaw);
	ImGui::Text("Arm distance: %.2f m", m_fCurrentArmDistance);
	ImGui::Text("Collision constrained: %s",
		m_bCollisionConstrained ? "true" : "false");
}
#endif

// Takes the body CENTRE, exactly as ComputeDesiredPosition does -- the two are a
// pair and a caller must not have to remember that they disagree. A caller holding
// a player TRANSFORM (which is the feet) converts with ZM_HumanBodyCentre first,
// and OnLateUpdate does that once for both.
Zenith_Maths::Vector3 ZM_FollowCamera::ComputePivot(
	const Zenith_Maths::Vector3& xPlayerBodyCentre)
{
	return xPlayerBodyCentre + Zenith_Maths::Vector3(0.0f, fPIVOT_HEIGHT, 0.0f);
}

Zenith_Maths::Vector3 ZM_FollowCamera::ComputeDesiredPosition(
	const Zenith_Maths::Vector3& xPlayerPosition, float fAuthoredYaw)
{
	const Zenith_Maths::Vector3 xSafePlayerPosition = IsFiniteVector(xPlayerPosition)
		? xPlayerPosition
		: Zenith_Maths::Vector3(0.0f);
	const float fSafeYaw = FiniteOr(fAuthoredYaw, 0.0f);
	const Zenith_Maths::Vector3 xForward(
		-std::sin(fSafeYaw), 0.0f, std::cos(fSafeYaw));
	return xSafePlayerPosition
		+ Zenith_Maths::Vector3(0.0f, fCAMERA_HEIGHT, 0.0f)
		- xForward * fCAMERA_ARM_LENGTH;
}

Zenith_Maths::Vector3 ZM_FollowCamera::ClampBoomBelowCeiling(
	const Zenith_Maths::Vector3& xDesired,
	const Zenith_Maths::Vector3& xPivot,
	float fCeilingY)
{
	if (!IsFiniteVector(xDesired) || !IsFiniteVector(xPivot)
		|| !std::isfinite(fCeilingY))
	{
		return xDesired;
	}

	const float fCapY = fCeilingY - fCEILING_CLEARANCE;
	if (xDesired.y <= fCapY)
	{
		return xDesired;   // outdoors (fNO_CEILING), or already under the slab
	}

	// ★ THE PIVOT IS THE FLOOR OF THIS CLAMP, NOT THE CAP. A player standing on a
	// mezzanine, on a prop, or simply not in the room the shell describes can have
	// a pivot ABOVE the cap; dropping the lens to the cap there would bury it in
	// the floor and point the camera up at the player's feet. In that case the
	// boom is left alone and the ordinary arm raycast -- which CAN see the walls --
	// remains the only constraint.
	if (xPivot.y >= fCapY)
	{
		return xDesired;
	}

	// ★★ THE WHOLE BOOM IS SHORTENED, NOT JUST FLATTENED. Capping only the height
	// and keeping the horizontal offset does clear the ceiling, and it costs the
	// shot its pitch: at PlayerHome's numbers the lens ends up 1.15 m above a pivot
	// 5.5 m away, an 11-degree downward angle that fills the top third of the frame
	// with ceiling and pushes the floor -- where the room and its furniture are --
	// into the bottom. Sliding the lens ALONG the boom instead preserves the angle
	// the camera was designed at and simply brings it closer, which is what a
	// third-person boom does under any other constraint. It is also exactly what
	// the arm raycast below already does when a wall is in the way, so indoors and
	// against-a-wall behave the same way instead of two different ways.
	const float fRise = xDesired.y - xPivot.y;
	if (!(fRise > 1.0e-4f))
	{
		return xDesired;
	}
	const float fScale = (fCapY - xPivot.y) / fRise;
	return xPivot + (xDesired - xPivot) * fScale;
}

Zenith_Maths::Vector3 ZM_FollowCamera::StepCriticalSpring(
	const Zenith_Maths::Vector3& xCurrent,
	const Zenith_Maths::Vector3& xTarget,
	Zenith_Maths::Vector3& xVelocityInOut,
	float fDeltaTime)
{
	if (!IsFiniteVector(xTarget))
	{
		xVelocityInOut = Zenith_Maths::Vector3(0.0f);
		return IsFiniteVector(xCurrent) ? xCurrent : Zenith_Maths::Vector3(0.0f);
	}
	if (!IsFiniteVector(xCurrent) || !IsFiniteVector(xVelocityInOut))
	{
		xVelocityInOut = Zenith_Maths::Vector3(0.0f);
		return xTarget;
	}
	if (!std::isfinite(fDeltaTime) || fDeltaTime <= 0.0f)
	{
		return xCurrent;
	}

	// Exact critically-damped solution for a constant target. Unlike an Euler
	// integrator this remains finite for a long hitch and is frame-partition
	// invariant up to floating-point rounding.
	const Zenith_Maths::Vector3 xDisplacement = xCurrent - xTarget;
	const Zenith_Maths::Vector3 xCoefficient =
		xVelocityInOut + fSPRING_OMEGA * xDisplacement;
	const float fDecay = std::exp(-fSPRING_OMEGA * fDeltaTime);
	const Zenith_Maths::Vector3 xNext = xTarget
		+ (xDisplacement + xCoefficient * fDeltaTime) * fDecay;
	xVelocityInOut = (xVelocityInOut
		- fSPRING_OMEGA * xCoefficient * fDeltaTime) * fDecay;

	if (!IsFiniteVector(xNext) || !IsFiniteVector(xVelocityInOut))
	{
		xVelocityInOut = Zenith_Maths::Vector3(0.0f);
		return xTarget;
	}
	return xNext;
}

float ZM_FollowCamera::ClampArmDistance(
	float fDesiredDistance, bool bHit, float fHitDistance)
{
	if (!std::isfinite(fDesiredDistance) || fDesiredDistance <= 0.0f)
	{
		return 0.0f;
	}
	if (!bHit || !std::isfinite(fHitDistance))
	{
		return fDesiredDistance;
	}

	const float fMinimumForThisArm =
		glm::min(fMINIMUM_ARM_LENGTH, fDesiredDistance);
	const float fCollisionDistance = glm::max(
		fMinimumForThisArm, fHitDistance - fCOLLISION_PADDING);
	return glm::min(fDesiredDistance, fCollisionDistance);
}

ZM_FollowCameraPose ZM_FollowCamera::BuildLookAtPose(
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Vector3& xPivot,
	float fFallbackYaw,
	float fFallbackPitch)
{
	ZM_FollowCameraPose xPose;
	xPose.m_xPosition = IsFiniteVector(xPosition)
		? xPosition
		: Zenith_Maths::Vector3(0.0f);
	xPose.m_fYaw = FiniteOr(fFallbackYaw, 0.0f);
	xPose.m_fPitch = glm::clamp(
		FiniteOr(fFallbackPitch, 0.0f), -fMAX_ABS_PITCH, fMAX_ABS_PITCH);

	if (!IsFiniteVector(xPosition) || !IsFiniteVector(xPivot))
	{
		return xPose;
	}

	const Zenith_Maths::Vector3 xDirection = xPivot - xPosition;
	const float fLengthSq = glm::dot(xDirection, xDirection);
	if (!std::isfinite(fLengthSq) || fLengthSq <= 0.00000001f)
	{
		return xPose;
	}

	const float fHorizontalLength =
		std::sqrt(xDirection.x * xDirection.x + xDirection.z * xDirection.z);
	const float fYaw = std::atan2(-xDirection.x, xDirection.z);
	const float fPitch = std::atan2(xDirection.y, fHorizontalLength);
	if (std::isfinite(fYaw))
	{
		xPose.m_fYaw = fYaw;
	}
	if (std::isfinite(fPitch))
	{
		xPose.m_fPitch = glm::clamp(fPitch, -fMAX_ABS_PITCH, fMAX_ABS_PITCH);
	}
	return xPose;
}

void ZM_FollowCamera::ResetRuntimeState()
{
	m_xTargetEntityID = INVALID_ENTITY_ID;
	m_xSpringPosition = Zenith_Maths::Vector3(0.0f);
	m_xSpringVelocity = Zenith_Maths::Vector3(0.0f);
	m_xPreviousTargetPosition = Zenith_Maths::Vector3(0.0f);
	m_fAuthoredYaw = 0.0f;
	m_fCurrentArmDistance = 0.0f;
	m_bAuthoredYawCaptured = false;
	m_bSpringInitialised = false;
	m_bHasPreviousTargetPosition = false;
	m_bCollisionConstrained = false;
}

void ZM_FollowCamera::CaptureAuthoredYaw()
{
	Zenith_CameraComponent* pxCamera =
		m_xParentEntity.TryGetComponent<Zenith_CameraComponent>();
	if (pxCamera == nullptr)
	{
		return;
	}
	m_fAuthoredYaw = FiniteOr(static_cast<float>(pxCamera->GetYaw()), 0.0f);
	m_bAuthoredYawCaptured = true;
	pxCamera->SetFOV(glm::radians(fFOV_DEGREES));
}

Zenith_Entity ZM_FollowCamera::ResolveTarget()
{
	Zenith_SceneData* pxSceneData =
		g_xEngine.Scenes().GetSceneDataForEntity(m_xParentEntity.GetEntityID());
	if (pxSceneData == nullptr)
	{
		m_xTargetEntityID = INVALID_ENTITY_ID;
		m_bSpringInitialised = false;
		m_bHasPreviousTargetPosition = false;
		return Zenith_Entity();
	}

	if (m_xTargetEntityID != INVALID_ENTITY_ID)
	{
		// EntityExists/TryGetEntity operate on the process-global entity store;
		// by themselves they do not prove that a still-live ID remains owned by
		// this camera's scene after MoveToScene/DontDestroyOnLoad. Resolve the
		// generation-checked owner before accepting the cached target.
		Zenith_SceneData* pxTargetSceneData =
			g_xEngine.Scenes().GetSceneDataForEntity(m_xTargetEntityID);
		if (pxTargetSceneData == pxSceneData)
		{
			Zenith_Entity xCached = pxTargetSceneData->TryGetEntity(
				m_xTargetEntityID);
			if (xCached.IsValid())
			{
				return xCached;
			}
		}
	}

	const Zenith_EntityID xPreviousTarget = m_xTargetEntityID;

	// ---- ACQUISITION IS BY COMPONENT, NEVER BY NAME. ------------------------
	//
	// ★ WHY THIS IS NOT pxSceneData->FindEntityByName("Player") ANY MORE.
	// It was, from this component's first commit until now -- and it was the ONLY
	// FindEntityByName call in the whole game layer. That made a bare string
	// literal load-bearing in every scene ever authored, with nothing able to
	// enforce it: rename the entity in ONE scene and this returns an invalid
	// entity, ZM_GameStateManager::PollForCameraAndBeginFadeIn then bare-returns
	// on a camera that has no target, and the warp sat on a barrier that back then
	// had NO TIMEOUT AT ALL -- a permanent black screen behind an opaque fade, with
	// every unit still green. (That barrier is bounded now, ZM-D-200: it escapes
	// with a Zenith_Error naming the state. Still a broken warp, but a loud one.)
	// The entities are all still NAMED "Player" (see
	// Project_RegisterEditorAutomationSteps); the point is that NOTHING DEPENDS ON
	// THAT any more. The name was also a substring of the serialized type name
	// "ZM_PlayerController", so a committed-bytes needle on it could never be a
	// clean equality.
	//
	// ★ WHY NOT ZM_GameStateManager::FindUniquePlayerInScene / its
	// TryGetUniqueActiveScenePlayerEntityID wrapper -- DO NOT "SIMPLIFY" INTO IT.
	// That seam early-outs on !g_xEngine.Physics().HasActiveSimulation() and
	// additionally demands a live body, because it exists to authenticate a
	// COLLIDING player. A camera that cannot acquire its subject until physics is
	// simulating is a NEW hang, not a fix, and it would make this unobservable
	// headlessly. ZM_InteractionRuntime::TryResolveActivePlayer avoids that seam
	// for exactly the same reason and says so at its definition.
	//
	// ★ WHY THE SCENE'S OWN QUERY AND NOT QueryActiveScene.
	// This camera follows the player in ITS OWN scene, not in whatever scene
	// happens to be active -- S5 loads Battle additively and makes it active while
	// this overworld camera is still alive. Zenith_SceneData::Query walks THIS
	// scene's component pools, and Zenith_SceneSystem::MoveEntityInternal
	// transfers an entity's components into the destination scene's pools, so pool
	// membership IS the ownership property the cached-target path above has to
	// spell out longhand (the global entity store cannot prove ownership after a
	// MoveToScene / DontDestroyOnLoad).
	//
	// ★ EXACTLY ONE, OR NOTHING. Zero players and MANY players both produce the
	// same "no target" outcome the old not-found branch produced. Silently taking
	// the first of several would hide an authoring defect behind a camera that
	// follows an arbitrary one of them.
	u_int uPlayerCount = 0u;
	Zenith_EntityID xPlayerEntityID = INVALID_ENTITY_ID;
	pxSceneData->Query<ZM_PlayerController>().ForEach(
		[&](Zenith_EntityID xEntityID, ZM_PlayerController&)
		{
			++uPlayerCount;
			if (uPlayerCount == 1u)
			{
				xPlayerEntityID = xEntityID;
			}
		});

	Zenith_Entity xPlayer = uPlayerCount == 1u
		? pxSceneData->TryGetEntity(xPlayerEntityID)
		: Zenith_Entity();
	if (!xPlayer.IsValid())
	{
		m_xTargetEntityID = INVALID_ENTITY_ID;
		m_bSpringInitialised = false;
		m_bHasPreviousTargetPosition = false;
		return Zenith_Entity();
	}

	m_xTargetEntityID = xPlayer.GetEntityID();
	if (m_xTargetEntityID != xPreviousTarget)
	{
		m_bSpringInitialised = false;
		m_bHasPreviousTargetPosition = false;
		m_bCollisionConstrained = false;
	}
	return xPlayer;
}
