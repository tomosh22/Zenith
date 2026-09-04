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
// player target; neither survives a SINGLE scene load. Target resolution uses
// an EntityID (index + generation), never a component/entity pointer.
//
// ★ THE TARGET IS THE UNIQUE ZM_PlayerController IN THIS CAMERA'S OWN SCENE --
// a COMPONENT lookup, not the name lookup this component shipped with. The old
// FindEntityByName("Player") made a string literal load-bearing in every
// authored scene with no way to enforce it, and a miss stalls
// ZM_GameStateManager's fade-in on a barrier until its frame budget expires and
// a Zenith_Error names the state (ZM-D-200) -- bounded now, still broken. Zero
// players or several both mean "no target" rather than a guess. ResolveTarget in
// the .cpp carries the full argument, including why this is deliberately NOT
// routed through ZM_GameStateManager's player seam (it demands live physics).
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

	// ★★ THE INDOOR FIX. ComputeDesiredPosition puts the lens fCAMERA_HEIGHT above
	// the player's transform unconditionally, which is right outdoors and wrong in
	// every room this game has: PlayerHome's ceiling slab starts at 3.0 m and a
	// player standing on its floor has a transform at ~0.9 m, so the desired lens
	// sits at ~3.9 m -- most of a metre ABOVE the ceiling, looking down through it.
	// The room is not visible at all, which is exactly what it looked like.
	//
	// ★ AND NO RAYCAST CAN FIND THAT CEILING. The arm is already swept for
	// obstructions, and it sweeps right through this one: the interior shell is a
	// VISUAL-ONLY entity (ZM_InteriorDressing.h -- the seven blockout blocks own
	// all the collision and there is no ceiling block among them), so the slab the
	// camera ends up above has no collider to hit. Adding one would change what the
	// PLAYER can do to fix what the CAMERA sees.
	//
	// So the ceiling is a NUMBER the camera resolves once, at OnStart, from the
	// room its own scene is wearing -- and this is the pure clamp that applies it.
	// Returns xDesired unchanged when fCeilingY is fNO_CEILING (every outdoor
	// scene), when the pivot is already above the ceiling (a player who is not in
	// the room the shell describes), or when the desired lens already clears it.
	// Otherwise the lens SLIDES ALONG THE BOOM until it sits at
	// fCeilingY - fCEILING_CLEARANCE -- so the camera keeps the pitch it was
	// designed at and simply comes closer, the same thing the arm raycast does
	// when a wall is in the way. See the .cpp for why capping the height alone
	// (which also clears the ceiling) is the wrong shape.
	static Zenith_Maths::Vector3 ClampBoomBelowCeiling(
		const Zenith_Maths::Vector3& xDesired,
		const Zenith_Maths::Vector3& xPivot,
		float fCeilingY);

	// The interior ceiling height this camera resolved from its own scene, or
	// fNO_CEILING outdoors. Exposed so a test can state which case it is exercising
	// rather than inferring it from a pose.
	float GetCeilingY() const { return m_fCeilingY; }
	static constexpr float GetNoCeiling() { return fNO_CEILING; }
	static constexpr float GetCeilingClearance() { return fCEILING_CLEARANCE; }
	static ZM_FollowCameraPose BuildLookAtPose(
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xPivot,
		float fFallbackYaw,
		float fFallbackPitch);

	static constexpr float GetPivotHeight() { return fPIVOT_HEIGHT; }

	// THE pivot, in ONE place. Takes the player's body CENTRE -- the same input
	// ComputeDesiredPosition takes, deliberately, so the pair cannot be fed two
	// different conventions. A caller holding a player TRANSFORM (which is the FEET;
	// see ZM_HumanBody.h) converts with ZM_HumanBodyCentre first.
	//
	// ★ IT EXISTS BECAUSE THE DUPLICATE BIT. fPIVOT_HEIGHT is measured from the body
	// CENTRE, and three camera units re-spelled the derivation as
	// "playerPosition + GetPivotHeight()". That was correct only while the transform
	// WAS the centre; when the origin moved to the feet, production moved and the
	// three copies did not. Both sides call this now, so the same mistake cannot be
	// made twice.
	static Zenith_Maths::Vector3 ComputePivot(
		const Zenith_Maths::Vector3& xPlayerBodyCentre);
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

	// The heading captured from the co-located Camera component at OnStart, which
	// this camera then keeps for the whole scene. READ-ONLY on purpose: the fixed
	// heading IS the shipped design, so there is deliberately no setter and no
	// rotation input. It is exposed so the real-scene clearance guard can drive
	// ComputeDesiredPosition with the yaw the SCENE authored rather than with a
	// hard-coded 0 that a later scene edit could silently invalidate.
	float GetAuthoredYaw() const { return m_fAuthoredYaw; }

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

	// "This scene has no ceiling." A sentinel rather than an optional because the
	// clamp is a pure static a test drives with plain floats. Any real ceiling is
	// a room height in metres, so a large positive value can never collide with
	// one, and it also makes the "already clears it" comparison do the right
	// thing with no special case.
	static constexpr float fNO_CEILING = 1.0e9f;

	// How far below the ceiling the lens is parked. The near plane authored on
	// every interior camera is 0.1 m, so this is comfortably more than enough to
	// keep the slab out of the frustum's front face while staying high enough to
	// look down at the room.
	static constexpr float fCEILING_CLEARANCE = 0.35f;

	void ResetRuntimeState();
	void CaptureAuthoredYaw();
	void ResolveCeiling();
	Zenith_Entity ResolveTarget();

	Zenith_Entity m_xParentEntity;
	Zenith_EntityID m_xTargetEntityID = INVALID_ENTITY_ID;
	Zenith_Maths::Vector3 m_xSpringPosition = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xSpringVelocity = Zenith_Maths::Vector3(0.0f);
	Zenith_Maths::Vector3 m_xPreviousTargetPosition = Zenith_Maths::Vector3(0.0f);
	float m_fAuthoredYaw = 0.0f;
	// Resolved at OnStart from the interior shell in this camera's own scene, and
	// NOT serialized: it is a function of the room, and the room is already in the
	// scene. Authoring it as well would put a second copy of the wall height in the
	// committed bytes for nothing to reconcile it against.
	float m_fCeilingY = fNO_CEILING;
	float m_fCurrentArmDistance = 0.0f;
	bool m_bAuthoredYawCaptured = false;
	bool m_bSpringInitialised = false;
	bool m_bHasPreviousTargetPosition = false;
	bool m_bCollisionConstrained = false;
};
