#include "Zenith.h"

#include "Zenithmon/Components/ZM_PlayerController.h"
#include "Zenithmon/Source/ZM_InputActions.h"

#include "Core/Zenith_Engine.h"
#include "EntityComponent/Components/Zenith_AnimatorComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Zenith_PhysicsQuery.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Scene.h"

#include <cmath>

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

namespace
{
	constexpr float fCOLLIDER_MIN_SCALE = 0.001f;
}

ZM_PlayerController::ZM_PlayerController(Zenith_Entity& xParentEntity)
	: m_xParentEntity(xParentEntity)
{
}

void ZM_PlayerController::OnStart()
{
	ResetRuntimeState();
	EnsureAndConfigureBody();
}

void ZM_PlayerController::OnUpdate(float fDeltaTime)
{
	// A bad frame delta must not reach either the step impulse or the facing
	// blend. In particular, glm::clamp does not sanitize NaN, so allowing a
	// non-finite value through RotateTowardsMovement would write a non-finite
	// transform. This is deliberately the first operation: zero/negative/non-
	// finite frames are true no-ops for controller observables, animation,
	// physics, and facing.
	if (!std::isfinite(fDeltaTime) || fDeltaTime <= 0.0f)
	{
		return;
	}

	m_xMoveDirection = Zenith_Maths::Vector3(0.0f);
	m_fRequestedSpeed = 0.0f;
	m_bGrounded = false;

	if (!m_xParentEntity.IsValid()
		|| !m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		return;
	}

	Zenith_ColliderComponent* pxCollider =
		m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	if (pxCollider == nullptr
		|| !pxCollider->HasValidBody()
		|| !xPhysics.HasActiveSimulation())
	{
		DriveAnimatorSpeed();
		return;
	}

	const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
	xPhysics.EnforceUpright(xBodyID);

	Zenith_Maths::Vector3 xGroundNormal(0.0f, 1.0f, 0.0f);
	Zenith_Maths::Vector3 xGroundPoint(0.0f);
	bool bGroundWalkable = false;
	const bool bGroundSurfaceHit = ProbeGround(
		xGroundNormal, xGroundPoint, bGroundWalkable);
	m_bGrounded = bGroundSurfaceHit && bGroundWalkable;

	Zenith_Maths::Vector3 xCameraForward(0.0f, 0.0f, 1.0f);
	if (Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes())
	{
		pxCamera->GetFacingDir(xCameraForward);
	}

	const Zenith_Maths::Vector2 xInput = ZM_InputActions::ReadMove();
	m_xMoveDirection = BuildCameraRelativeDirection(xInput, xCameraForward);
	if (m_bGrounded && glm::dot(m_xMoveDirection, m_xMoveDirection) > fEPSILON)
	{
		m_xMoveDirection = ProjectOntoGround(m_xMoveDirection, xGroundNormal);
	}
	else if (bGroundSurfaceHit
		&& !bGroundWalkable
		&& glm::dot(m_xMoveDirection, m_xMoveDirection) > fEPSILON)
	{
		// A steep surface's horizontal normal points downhill. Remove only the
		// opposing (uphill) input component so >45-degree faces cannot be driven
		// up, while contouring and moving away/downhill remain available.
		Zenith_Maths::Vector3 xDownhillNormal(
			xGroundNormal.x, 0.0f, xGroundNormal.z);
		const float fDownhillLengthSquared =
			glm::dot(xDownhillNormal, xDownhillNormal);
		if (fDownhillLengthSquared > fEPSILON)
		{
			xDownhillNormal /= std::sqrt(fDownhillLengthSquared);
			const float fDownhillInput =
				glm::dot(m_xMoveDirection, xDownhillNormal);
			if (fDownhillInput < 0.0f)
			{
				m_xMoveDirection -= xDownhillNormal * fDownhillInput;
			}
		}
	}

	const bool bHasMove = glm::dot(m_xMoveDirection, m_xMoveDirection) > fEPSILON;
	m_fRequestedSpeed = SelectRequestedSpeed(bHasMove, ZM_InputActions::ReadRunHeld());

	const Zenith_Maths::Vector3 xCurrentVelocity = xPhysics.GetLinearVelocity(xBodyID);
	Zenith_Maths::Vector3 xRequestedVelocity = BuildHorizontalVelocity(
		m_xMoveDirection, m_fRequestedSpeed, xCurrentVelocity);
	// ProbeGround classified the current contact before desired velocity is
	// built. On a walkable downslope, gravity alone may not drop a velocity-
	// driven capsule as quickly as the surface falls away. Add only the exact
	// downward tangent rate needed to retain contact; never motor upward, and
	// never replace either a stronger existing fall or an active upward step
	// assist.
	xRequestedVelocity.y = CalculateGroundedSlopeVerticalVelocity(
		m_xMoveDirection,
		m_fRequestedSpeed,
		xCurrentVelocity.y,
		m_bGrounded);
	xPhysics.SetLinearVelocity(xBodyID, xRequestedVelocity);

	if (m_bGrounded && bHasMove)
	{
		TryApplyStep(m_xMoveDirection, xGroundPoint, fDeltaTime);
		RotateTowardsMovement(m_xMoveDirection, fDeltaTime);
	}

	DriveAnimatorSpeed();
}

void ZM_PlayerController::OnDisable()
{
	StopHorizontalMotion();
	ResetRuntimeState();
	DriveAnimatorSpeed();
}

void ZM_PlayerController::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << uSERIALIZATION_VERSION;
}

void ZM_PlayerController::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uComponentVersion = 0u;
	xStream >> uComponentVersion;
	(void)uComponentVersion;
	ResetRuntimeState();
}

#ifdef ZENITH_TOOLS
void ZM_PlayerController::RenderPropertiesPanel()
{
	ImGui::Text("Velocity-driven capsule controller");
	ImGui::Text("Grounded: %s", m_bGrounded ? "true" : "false");
	ImGui::Text("Requested speed: %.2f m/s", m_fRequestedSpeed);
	ImGui::Text("Walk / run: %.1f / %.1f m/s", fWALK_SPEED, fRUN_SPEED);
	ImGui::Text("Slope / step: %.0f deg / %.2f m", fMAX_SLOPE_DEGREES, fMAX_STEP_HEIGHT);
}
#endif

Zenith_Maths::Vector3 ZM_PlayerController::BuildCameraRelativeDirection(
	const Zenith_Maths::Vector2& xInput,
	const Zenith_Maths::Vector3& xCameraForward)
{
	if (glm::dot(xInput, xInput) <= fEPSILON)
	{
		return Zenith_Maths::Vector3(0.0f);
	}

	Zenith_Maths::Vector3 xForward(xCameraForward.x, 0.0f, xCameraForward.z);
	const float fForwardLengthSquared = glm::dot(xForward, xForward);
	if (fForwardLengthSquared <= fEPSILON)
	{
		xForward = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		xForward /= std::sqrt(fForwardLengthSquared);
	}

	const Zenith_Maths::Vector3 xRight(xForward.z, 0.0f, -xForward.x);
	Zenith_Maths::Vector3 xDirection = xForward * xInput.y + xRight * xInput.x;
	const float fDirectionLengthSquared = glm::dot(xDirection, xDirection);
	if (fDirectionLengthSquared <= fEPSILON)
	{
		return Zenith_Maths::Vector3(0.0f);
	}
	return xDirection / std::sqrt(fDirectionLengthSquared);
}

float ZM_PlayerController::SelectRequestedSpeed(bool bHasMove, bool bRunHeld)
{
	if (!bHasMove)
	{
		return 0.0f;
	}
	return bRunHeld ? fRUN_SPEED : fWALK_SPEED;
}

Zenith_Maths::Vector3 ZM_PlayerController::BuildHorizontalVelocity(
	const Zenith_Maths::Vector3& xDirection,
	float fRequestedSpeed,
	const Zenith_Maths::Vector3& xCurrentVelocity)
{
	Zenith_Maths::Vector3 xHorizontalDirection(xDirection.x, 0.0f, xDirection.z);
	const float fHorizontalLengthSquared = glm::dot(xHorizontalDirection, xHorizontalDirection);
	if (fHorizontalLengthSquared > fEPSILON)
	{
		// ProjectOntoGround returns a unit three-dimensional tangent. Its XZ
		// projection shortens by cos(slope), but horizontal travel speed is the
		// controller contract. Re-normalise that projection before applying the
		// requested walk/run speed. Y remains body-owned here; the separate
		// grounded-downslope selector may make it more negative for adhesion,
		// while Jolt contact response handles uphill motion.
		xHorizontalDirection /= std::sqrt(fHorizontalLengthSquared);
	}
	else
	{
		xHorizontalDirection = Zenith_Maths::Vector3(0.0f);
	}

	const float fSafeSpeed = glm::max(fRequestedSpeed, 0.0f);
	return Zenith_Maths::Vector3(
		xHorizontalDirection.x * fSafeSpeed,
		xCurrentVelocity.y,
		xHorizontalDirection.z * fSafeSpeed);
}

float ZM_PlayerController::CalculateGroundedSlopeVerticalVelocity(
	const Zenith_Maths::Vector3& xMovementTangent,
	float fRequestedHorizontalSpeed,
	float fCurrentVerticalVelocity,
	bool bGrounded)
{
	if (!bGrounded
		|| fCurrentVerticalVelocity > fEPSILON
		|| xMovementTangent.y >= 0.0f
		|| !std::isfinite(xMovementTangent.x)
		|| !std::isfinite(xMovementTangent.y)
		|| !std::isfinite(xMovementTangent.z)
		|| !std::isfinite(fRequestedHorizontalSpeed)
		|| fRequestedHorizontalSpeed <= 0.0f)
	{
		return fCurrentVerticalVelocity;
	}

	const float fHorizontalLength = std::sqrt(
		xMovementTangent.x * xMovementTangent.x
		+ xMovementTangent.z * xMovementTangent.z);
	if (fHorizontalLength <= fEPSILON)
	{
		return fCurrentVerticalVelocity;
	}

	const float fSlopeFollowVelocity =
		(xMovementTangent.y / fHorizontalLength)
		* fRequestedHorizontalSpeed;
	return glm::min(fCurrentVerticalVelocity, fSlopeFollowVelocity);
}

bool ZM_PlayerController::IsWalkableSlope(const Zenith_Maths::Vector3& xNormal)
{
	const float fLengthSquared = glm::dot(xNormal, xNormal);
	if (fLengthSquared <= fEPSILON)
	{
		return false;
	}

	const float fNormalY = xNormal.y / std::sqrt(fLengthSquared);
	const float fMinimumY = std::cos(glm::radians(fMAX_SLOPE_DEGREES));
	return fNormalY + fEPSILON >= fMinimumY;
}

Zenith_Maths::Vector3 ZM_PlayerController::ProjectOntoGround(
	const Zenith_Maths::Vector3& xDirection,
	const Zenith_Maths::Vector3& xNormal)
{
	const float fNormalLengthSquared = glm::dot(xNormal, xNormal);
	if (fNormalLengthSquared <= fEPSILON)
	{
		return Zenith_Maths::Vector3(0.0f);
	}

	const Zenith_Maths::Vector3 xNormalisedNormal =
		xNormal / std::sqrt(fNormalLengthSquared);
	Zenith_Maths::Vector3 xProjected =
		xDirection - xNormalisedNormal * glm::dot(xDirection, xNormalisedNormal);
	const float fProjectedLengthSquared = glm::dot(xProjected, xProjected);
	if (fProjectedLengthSquared <= fEPSILON)
	{
		return Zenith_Maths::Vector3(0.0f);
	}
	return xProjected / std::sqrt(fProjectedLengthSquared);
}

bool ZM_PlayerController::IsStepCandidateValid(
	bool bLowerBlocked,
	bool bUpperBlocked,
	bool bLandingFound,
	float fRise,
	const Zenith_Maths::Vector3& xLandingNormal)
{
	return bLowerBlocked
		&& !bUpperBlocked
		&& bLandingFound
		&& fRise >= fSTEP_MIN_RISE
		&& fRise <= fMAX_STEP_HEIGHT + fEPSILON
		&& IsWalkableSlope(xLandingNormal);
}

float ZM_PlayerController::CalculateStepAssistVelocity(
	float fCurrentVerticalVelocity,
	float fRise,
	float fDeltaTime)
{
	if (fRise <= 0.0f
		|| !std::isfinite(fDeltaTime)
		|| fDeltaTime <= 0.0f)
	{
		return fCurrentVerticalVelocity;
	}

	// Size the one-shot assist to the qualified rise, not the frame length. A
	// rise/dt impulse would hit the 3 m/s cap for almost every curb at 60 Hz and
	// visibly hop over small steps. The ballistic estimate reaches the landing
	// height plus one probe skin under normal gravity, while remaining bounded.
	const float fAssistHeight = fRise + fSTEP_PROBE_SKIN;
	const float fRequiredVelocity = std::sqrt(
		2.0f * fSTEP_GRAVITY * fAssistHeight);
	const float fBoundedAssist = glm::min(fRequiredVelocity, fSTEP_ASSIST_SPEED);
	return glm::max(fCurrentVerticalVelocity, fBoundedAssist);
}

void ZM_PlayerController::ResetRuntimeState()
{
	m_xMoveDirection = Zenith_Maths::Vector3(0.0f);
	m_fRequestedSpeed = 0.0f;
	m_bGrounded = false;
}

void ZM_PlayerController::EnsureAndConfigureBody()
{
	if (!m_xParentEntity.IsValid()
		|| !m_xParentEntity.HasComponent<Zenith_TransformComponent>())
	{
		return;
	}

	Zenith_Physics& xPhysics = g_xEngine.Physics();
	if (!xPhysics.HasActiveSimulation())
	{
		return;
	}

	Zenith_ColliderComponent* pxCollider =
		m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
	if (pxCollider != nullptr
		&& pxCollider->HasValidBody()
		&& (pxCollider->GetCollisionVolumeType() != COLLISION_VOLUME_TYPE_CAPSULE
			|| pxCollider->GetRigidBodyType() != RIGIDBODY_TYPE_DYNAMIC))
	{
		// The controller contract owns the player body shape. Replacing an
		// incompatible authored collider is reversible and avoids reaching into
		// Jolt or adding an engine-side character-controller abstraction.
		m_xParentEntity.RemoveComponent<Zenith_ColliderComponent>();
		pxCollider = nullptr;
	}

	if (pxCollider == nullptr)
	{
		pxCollider = &m_xParentEntity.AddComponent<Zenith_ColliderComponent>();
	}
	if (!pxCollider->HasValidBody())
	{
		// Generic AddCollider deliberately derives the capsule dimensions from
		// Transform scale, so the serialized collider round-trips exactly.
		pxCollider->AddCollider(COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
	}
	if (!pxCollider->HasValidBody())
	{
		return;
	}

	const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
	pxCollider->SetIsSensor(false);
	xPhysics.SetGravityEnabled(xBodyID, true);
	xPhysics.LockRotation(xBodyID, true, false, true);
	xPhysics.EnforceUpright(xBodyID);
	xPhysics.SetFriction(xBodyID, fBODY_FRICTION);
	xPhysics.SetRestitution(xBodyID, fBODY_RESTITUTION);
}

void ZM_PlayerController::StopHorizontalMotion()
{
	if (!m_xParentEntity.IsValid())
	{
		return;
	}

	Zenith_ColliderComponent* pxCollider =
		m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
	Zenith_Physics& xPhysics = g_xEngine.Physics();
	if (pxCollider == nullptr
		|| !pxCollider->HasValidBody()
		|| !xPhysics.HasActiveSimulation())
	{
		return;
	}

	const Zenith_Maths::Vector3 xCurrentVelocity =
		xPhysics.GetLinearVelocity(pxCollider->GetBodyID());
	xPhysics.SetLinearVelocity(
		pxCollider->GetBodyID(),
		BuildHorizontalVelocity(Zenith_Maths::Vector3(0.0f), 0.0f, xCurrentVelocity));
}

void ZM_PlayerController::DriveAnimatorSpeed() const
{
	if (!m_xParentEntity.IsValid())
	{
		return;
	}
	if (Zenith_AnimatorComponent* pxAnimator =
		m_xParentEntity.TryGetComponent<Zenith_AnimatorComponent>())
	{
		pxAnimator->SetFloat("Speed", m_fRequestedSpeed);
	}
}

bool ZM_PlayerController::ProbeGround(
	Zenith_Maths::Vector3& xNormalOut,
	Zenith_Maths::Vector3& xPointOut,
	bool& bWalkableOut) const
{
	bWalkableOut = false;
	Zenith_Maths::Vector3 xPosition;
	m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetPosition(xPosition);

	const Zenith_Physics::RaycastResult xGroundHit =
		Zenith_PhysicsQuery::RaycastIgnoring(
			xPosition,
			Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f),
			GetCapsuleHalfExtent() + fGROUND_PROBE_EXTENSION,
			m_xParentEntity.GetEntityID());
	if (!xGroundHit.m_bHit)
	{
		return false;
	}

	const float fNormalLengthSquared = glm::dot(xGroundHit.m_xHitNormal, xGroundHit.m_xHitNormal);
	if (fNormalLengthSquared <= fEPSILON)
	{
		return false;
	}
	xNormalOut = xGroundHit.m_xHitNormal / std::sqrt(fNormalLengthSquared);
	xPointOut = xGroundHit.m_xHitPoint;
	bWalkableOut = IsWalkableSlope(xNormalOut);
	return true;
}

void ZM_PlayerController::TryApplyStep(
	const Zenith_Maths::Vector3& xMoveDirection,
	const Zenith_Maths::Vector3& xGroundPoint,
	float fDeltaTime)
{
	Zenith_ColliderComponent* pxCollider =
		m_xParentEntity.TryGetComponent<Zenith_ColliderComponent>();
	if (pxCollider == nullptr || !pxCollider->HasValidBody())
	{
		return;
	}

	Zenith_Maths::Vector3 xForward(xMoveDirection.x, 0.0f, xMoveDirection.z);
	const float fForwardLengthSquared = glm::dot(xForward, xForward);
	if (fForwardLengthSquared <= fEPSILON)
	{
		return;
	}
	xForward /= std::sqrt(fForwardLengthSquared);

	Zenith_Physics& xPhysics = g_xEngine.Physics();
	const Zenith_PhysicsBodyID xBodyID = pxCollider->GetBodyID();
	Zenith_Maths::Vector3 xVelocity = xPhysics.GetLinearVelocity(xBodyID);
	if (xVelocity.y > fEPSILON)
	{
		// One accepted step supplies one upward impulse. Re-boosting an already
		// rising capsule every frame would turn a 0.40m assist into a hop.
		return;
	}
	const Zenith_Maths::Vector3 xBodyPosition = xPhysics.GetBodyPosition(xBodyID);
	const Zenith_Maths::Vector3 xProbeBase(
		xBodyPosition.x,
		xGroundPoint.y + fSTEP_PROBE_SKIN,
		xBodyPosition.z);

	const Zenith_Physics::RaycastResult xLowerHit =
		Zenith_PhysicsQuery::RaycastIgnoring(
			xProbeBase,
			xForward,
			fSTEP_FORWARD_DISTANCE,
			m_xParentEntity.GetEntityID());
	const bool bLowerBlocked = xLowerHit.m_bHit
		&& !IsWalkableSlope(xLowerHit.m_xHitNormal);
	if (!bLowerBlocked)
	{
		return;
	}

	const Zenith_Maths::Vector3 xUpperOrigin =
		xProbeBase + Zenith_Maths::Vector3(0.0f, fMAX_STEP_HEIGHT + fSTEP_PROBE_SKIN, 0.0f);
	const Zenith_Physics::RaycastResult xUpperHit =
		Zenith_PhysicsQuery::RaycastIgnoring(
			xUpperOrigin,
			xForward,
			fSTEP_FORWARD_DISTANCE,
			m_xParentEntity.GetEntityID());

	const Zenith_Maths::Vector3 xLandingOrigin =
		xProbeBase
		+ xForward * fSTEP_FORWARD_DISTANCE
		+ Zenith_Maths::Vector3(0.0f, fMAX_STEP_HEIGHT + fSTEP_PROBE_SKIN, 0.0f);
	const Zenith_Physics::RaycastResult xLandingHit =
		Zenith_PhysicsQuery::RaycastIgnoring(
			xLandingOrigin,
			Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f),
			fMAX_STEP_HEIGHT + fSTEP_PROBE_SKIN * 4.0f,
			m_xParentEntity.GetEntityID());
	const float fRise = xLandingHit.m_bHit
		? xLandingHit.m_xHitPoint.y - xGroundPoint.y
		: 0.0f;
	if (!IsStepCandidateValid(
		bLowerBlocked,
		xUpperHit.m_bHit,
		xLandingHit.m_bHit,
		fRise,
		xLandingHit.m_xHitNormal))
	{
		return;
	}

	const float fAssistedVerticalVelocity = CalculateStepAssistVelocity(
		xVelocity.y, fRise, fDeltaTime);
	if (fAssistedVerticalVelocity <= xVelocity.y + fEPSILON)
	{
		return;
	}

	xVelocity.y = fAssistedVerticalVelocity;
	xPhysics.SetLinearVelocity(xBodyID, xVelocity);
}

void ZM_PlayerController::RotateTowardsMovement(
	const Zenith_Maths::Vector3& xMoveDirection,
	float fDeltaTime)
{
	if (!std::isfinite(fDeltaTime) || fDeltaTime <= 0.0f)
	{
		return;
	}

	Zenith_Maths::Vector3 xFacing(xMoveDirection.x, 0.0f, xMoveDirection.z);
	const float fFacingLengthSquared = glm::dot(xFacing, xFacing);
	if (fFacingLengthSquared <= fEPSILON)
	{
		return;
	}
	xFacing /= std::sqrt(fFacingLengthSquared);

	Zenith_TransformComponent& xTransform =
		m_xParentEntity.GetComponent<Zenith_TransformComponent>();
	Zenith_Maths::Quat xCurrentRotation;
	xTransform.GetRotation(xCurrentRotation);
	if (glm::dot(xCurrentRotation, xCurrentRotation) <= fEPSILON)
	{
		xCurrentRotation = Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f);
	}
	else
	{
		xCurrentRotation = glm::normalize(xCurrentRotation);
	}

	const float fTargetYaw = std::atan2(xFacing.x, xFacing.z);
	const Zenith_Maths::Quat xTargetRotation =
		glm::angleAxis(fTargetYaw, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	const float fBlend = glm::clamp(fDeltaTime * fTURN_SPEED, 0.0f, 1.0f);
	const Zenith_Maths::Quat xNewRotation = glm::normalize(
		glm::slerp(xCurrentRotation, xTargetRotation, fBlend));
	xTransform.SetRotation(xNewRotation);
}

float ZM_PlayerController::GetCapsuleHalfExtent() const
{
	Zenith_Maths::Vector3 xScale;
	m_xParentEntity.GetComponent<Zenith_TransformComponent>().GetScale(xScale);
	xScale.x = glm::max(xScale.x, fCOLLIDER_MIN_SCALE);
	xScale.y = glm::max(xScale.y, fCOLLIDER_MIN_SCALE);
	xScale.z = glm::max(xScale.z, fCOLLIDER_MIN_SCALE);

	const float fRadius = glm::max(xScale.x, xScale.z) * 0.5f;
	const float fHalfCylinder = glm::max(
		fCOLLIDER_MIN_SCALE,
		xScale.y * 0.5f - fRadius);
	return fRadius + fHalfCylinder;
}
