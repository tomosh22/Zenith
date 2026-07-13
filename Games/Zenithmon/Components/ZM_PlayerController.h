#pragma once

#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Entity.h"

// Velocity-driven overworld controller. The component owns no persistent
// gameplay state: the serialized payload is version-only, while all movement,
// grounding, and step-assist state is rebuilt from the live body each run.
class ZM_PlayerController
{
public:
	static constexpr u_int uSERIALIZATION_VERSION = 1u;

	static constexpr float fWALK_SPEED = 4.0f;
	static constexpr float fRUN_SPEED = 7.0f;
	static constexpr float fMAX_SLOPE_DEGREES = 45.0f;
	static constexpr float fMAX_STEP_HEIGHT = 0.40f;
	static constexpr float fSTEP_FORWARD_DISTANCE = 0.55f;
	static constexpr float fSTEP_ASSIST_SPEED = 3.0f;

	ZM_PlayerController() = delete;
	explicit ZM_PlayerController(Zenith_Entity& xParentEntity);

	ZM_PlayerController(const ZM_PlayerController&) = delete;
	ZM_PlayerController& operator=(const ZM_PlayerController&) = delete;
	ZM_PlayerController(ZM_PlayerController&&) noexcept = default;
	ZM_PlayerController& operator=(ZM_PlayerController&&) noexcept = default;

	void OnStart();
	void OnUpdate(float fDeltaTime);
	void OnDisable();
	void SetMovementEnabled(bool bEnabled);
	bool IsMovementEnabled() const { return m_bMovementEnabled; }
	void ResetRuntimeState();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

	// Pure deterministic policy seams used by the runtime and unit tests.
	static Zenith_Maths::Vector3 BuildCameraRelativeDirection(
		const Zenith_Maths::Vector2& xInput,
		const Zenith_Maths::Vector3& xCameraForward);
	static float SelectRequestedSpeed(bool bHasMove, bool bRunHeld);
	static Zenith_Maths::Vector3 BuildHorizontalVelocity(
		const Zenith_Maths::Vector3& xDirection,
		float fRequestedSpeed,
		const Zenith_Maths::Vector3& xCurrentVelocity);
	static float CalculateGroundedSlopeVerticalVelocity(
		const Zenith_Maths::Vector3& xMovementTangent,
		float fRequestedHorizontalSpeed,
		float fCurrentVerticalVelocity,
		bool bGrounded);
	static bool IsWalkableSlope(const Zenith_Maths::Vector3& xNormal);
	static Zenith_Maths::Vector3 ProjectOntoGround(
		const Zenith_Maths::Vector3& xDirection,
		const Zenith_Maths::Vector3& xNormal);
	static bool IsStepCandidateValid(
		bool bLowerBlocked,
		bool bUpperBlocked,
		bool bLandingFound,
		float fRise,
		const Zenith_Maths::Vector3& xLandingNormal);
	static float CalculateStepAssistVelocity(
		float fCurrentVerticalVelocity,
		float fRise,
		float fDeltaTime);
	static float CalculateCapsuleHalfExtent(
		const Zenith_Maths::Vector3& xScale);

	float GetRequestedSpeed() const { return m_fRequestedSpeed; }
	bool IsGrounded() const { return m_bGrounded; }
	const Zenith_Maths::Vector3& GetMoveDirection() const { return m_xMoveDirection; }

private:
	static constexpr float fBODY_FRICTION = 0.8f;
	static constexpr float fBODY_RESTITUTION = 0.0f;
	static constexpr float fGROUND_PROBE_EXTENSION = 0.15f;
	static constexpr float fSTEP_PROBE_SKIN = 0.05f;
	static constexpr float fSTEP_MIN_RISE = 0.02f;
	static constexpr float fSTEP_GRAVITY = 9.81f;
	static constexpr float fTURN_SPEED = 12.0f;
	static constexpr float fEPSILON = 0.00001f;

	void ResetMovementObservables();
	void EnsureAndConfigureBody();
	void StopHorizontalMotion();
	void DriveAnimatorSpeed() const;
	bool ProbeGround(
		Zenith_Maths::Vector3& xNormalOut,
		Zenith_Maths::Vector3& xPointOut,
		bool& bWalkableOut) const;
	void TryApplyStep(
		const Zenith_Maths::Vector3& xMoveDirection,
		const Zenith_Maths::Vector3& xGroundPoint,
		float fDeltaTime);
	void RotateTowardsMovement(
		const Zenith_Maths::Vector3& xMoveDirection,
		float fDeltaTime);
	float GetCapsuleHalfExtent() const;

	Zenith_Entity m_xParentEntity;
	Zenith_Maths::Vector3 m_xMoveDirection = Zenith_Maths::Vector3(0.0f);
	float m_fRequestedSpeed = 0.0f;
	bool m_bGrounded = false;
	bool m_bMovementEnabled = true;
};
