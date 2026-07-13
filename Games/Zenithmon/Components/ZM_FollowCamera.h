#pragma once

#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Entity.h"

class Zenith_DataStream;

// Pure camera pose returned by BuildLookAtPose. Keeping this as a game-local
// POD makes the follow math unit-testable without a live scene or renderer.
struct ZM_FollowCameraPose
{
	Zenith_Maths::Vector3 m_xPosition = Zenith_Maths::Vector3(0.0f);
	float m_fYaw = 0.0f;
	float m_fPitch = 0.0f;
};

// Scene-owned, fixed-heading overworld follow camera.
//
// The component owns only runtime follow state. The co-located engine Camera
// component owns the rendered pose and the scene owns both this entity and the
// Player target; neither survives a SINGLE scene load. Target resolution uses
// an EntityID (index + generation), never a component/entity pointer.
class ZM_FollowCamera
{
public:
	ZM_FollowCamera() = delete;
	explicit ZM_FollowCamera(Zenith_Entity& xParentEntity);

	ZM_FollowCamera(const ZM_FollowCamera&) = delete;
	ZM_FollowCamera& operator=(const ZM_FollowCamera&) = delete;
	ZM_FollowCamera(ZM_FollowCamera&&) noexcept = default;
	ZM_FollowCamera& operator=(ZM_FollowCamera&&) noexcept = default;

	void OnStart();
	void OnLateUpdate(float fDeltaTime);
	void OnDestroy();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

	// Deterministic, side-effect-free seams shared by production and tests.
	static Zenith_Maths::Vector3 ComputeDesiredPosition(
		const Zenith_Maths::Vector3& xPlayerPosition, float fAuthoredYaw);
	static Zenith_Maths::Vector3 StepCriticalSpring(
		const Zenith_Maths::Vector3& xCurrent,
		const Zenith_Maths::Vector3& xTarget,
		Zenith_Maths::Vector3& xVelocityInOut,
		float fDeltaTime);
	static float ClampArmDistance(float fDesiredDistance, bool bHit, float fHitDistance);
	static ZM_FollowCameraPose BuildLookAtPose(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xPivot,
		float fFallbackYaw,
		float fFallbackPitch);

	static constexpr float GetPivotHeight() { return fPIVOT_HEIGHT; }
	static constexpr float GetCameraHeight() { return fCAMERA_HEIGHT; }
	static constexpr float GetArmLength() { return fCAMERA_ARM_LENGTH; }
	static constexpr float GetSpringOmega() { return fSPRING_OMEGA; }
	static constexpr float GetCollisionPadding() { return fCOLLISION_PADDING; }
	static constexpr float GetMinimumArmLength() { return fMINIMUM_ARM_LENGTH; }
	static constexpr float GetFOVDegrees() { return fFOV_DEGREES; }
	static constexpr float GetTeleportSnapDistance() { return fTELEPORT_SNAP_DISTANCE; }

	float GetCurrentArmDistance() const { return m_fCurrentArmDistance; }
	bool IsCollisionConstrained() const { return m_bCollisionConstrained; }
	Zenith_EntityID GetTargetEntityID() const { return m_xTargetEntityID; }

private:
	static constexpr float fPIVOT_HEIGHT = 0.60f;
	static constexpr float fCAMERA_HEIGHT = 3.0f;
	static constexpr float fCAMERA_ARM_LENGTH = 5.5f;
	static constexpr float fSPRING_OMEGA = 8.0f;
	static constexpr float fCOLLISION_PADDING = 0.20f;
	static constexpr float fMINIMUM_ARM_LENGTH = 1.0f;
	static constexpr float fFOV_DEGREES = 65.0f;
	static constexpr float fTELEPORT_SNAP_DISTANCE = 20.0f;
	static constexpr float fMAX_ABS_PITCH = 1.55334306f; // pi/2 - 1 degree

	void ResetRuntimeState();
	void CaptureAuthoredYaw();
	Zenith_Entity ResolveTarget();

	Zenith_Entity m_xParentEntity;
	Zenith_EntityID m_xTargetEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 m_xSpringPosition = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xSpringVelocity = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xPreviousTargetPosition = Zenith_Maths::Vector3(0.0f);
	float m_fAuthoredYaw = 0.0f;
	float m_fCurrentArmDistance = 0.0f;
	bool m_bAuthoredYawCaptured = false;
	bool m_bSpringInitialised = false;
	bool m_bHasPreviousTargetPosition = false;
	bool m_bCollisionConstrained = false;
};
