#include "Zenith.h"

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
	ResolveTarget();
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

	const Zenith_Maths::Vector3 xPivot = xPlayerPosition
		+ Zenith_Maths::Vector3(0.0f, fPIVOT_HEIGHT, 0.0f);
	const Zenith_Maths::Vector3 xDesiredPosition =
		ComputeDesiredPosition(xPlayerPosition, m_fAuthoredYaw);

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
	// on a camera that has no target, and the warp sits on a barrier with NO
	// TIMEOUT -- a permanent black screen behind an opaque fade, with every unit
	// still green. The entities are all still NAMED "Player" (see
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
