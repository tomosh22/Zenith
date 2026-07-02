#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPOrbitCamera_Component - Bird's-eye orbit camera centred on the village,
 * with a possession-driven third-person mode (2026-07-01).
 *
 * BirdsEye (default): the whole playable area (~(0..100, 0..100) in world
 * space, centre ~(50, 0, 50)) is visible so the player can see every
 * villager, priest, door, and item simultaneously. The orbit target is
 * pinned at the map centre and never moves. Q/E rotate; wheel zooms.
 *
 * ThirdPerson: camera sits behind + above the possessed villager, looking
 * at it (pose derivation mirrors RenderTest_FollowCameraComponent). The
 * mode auto-engages while a villager is possessed and auto-releases when
 * possession is lost; C manually overrides either way until the next
 * possession change. Transitions BLEND over ~0.4 s by lerping the camera
 * position and look-at point together and re-solving yaw/pitch from the
 * blended pair — no yaw-angle lerp, so there is no ±π wraparound case.
 *
 * Possession state is POLLED from DP_Player each frame rather than
 * subscribed via DP_OnPossessionChanged: the blend target is pure state
 * (not an edge), and polling keeps this component free of this-capturing
 * subscriptions (no move-rewiring hazard across pool relocation).
 *
 * Regression guarantee: while the blend is fully at BirdsEye (t == 0 —
 * always true in never-possessed scenes: FrontEnd, gyms, headless tests)
 * the ORIGINAL orbit code path runs unchanged, byte-identical to the
 * pre-third-person behaviour.
 *
 * Zenith_CameraComponent is FPS-style (position + pitch + yaw + FOV);
 * the orbit math is implemented as a game component on the camera entity.
 */

#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPInputActions.h"
#include "Source/DPTelemetry.h"

enum class DPCameraMode : uint8_t
{
	BirdsEye    = 0,
	ThirdPerson = 1,
};

class DPOrbitCamera_Component ZENITH_FINAL
{
public:
	DPOrbitCamera_Component() = delete;
	DPOrbitCamera_Component(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{}

	void OnAwake()
	{
		// Pin to the GameLevel's playable centre; gym scenes (which place
		// their action at world origin) override the target via
		// SetOrbitTarget below.
		m_xOrbitTarget = Zenith_Maths::Vector3(50.0f, 0.0f, 50.0f);
		m_fOrbitDistance = m_fDefaultDistance;
		// Default yaw of +π/2 puts the camera west of centre looking east
		// (toward +world.X). This rotates the rendered top-down view 90°
		// CW from Zenith's natural top-down orientation, so what shows up
		// "up on screen" is world +X — matching the UE editor's top-down
		// view (where UE.X is up on screen). The DP coordinate conversion
		// maps UE.X straight onto Zenith.X for mesh-axis alignment, so
		// without this camera offset the rendered view would look 90°
		// rotated from UE. Q/E player input still rotates relative to
		// this default; the player can spin the view freely.
		m_fOrbitYaw = glm::half_pi<float>();
	}

	// Component contract: version-only payload (orbit pose is runtime state).
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel() {}
#endif

	void OnUpdate(const float fDt)
	{
		// Q/E yaw, mouse wheel zoom — the only player controls over the
		// bird's-eye camera. The orbit target NEVER moves toward the
		// possessed villager: this is by design — the player needs the
		// whole village in view to pick which villager to possess next
		// and to keep tabs on the priest's pursuit.
		m_fOrbitYaw += DP_Input::ReadCameraRotate() * m_fRotateSpeed * fDt;
		const float fWheel = g_xEngine.Input().GetMouseWheelDelta();
		if (fWheel != 0.0f)
		{
			m_fOrbitDistance = glm::clamp(m_fOrbitDistance - fWheel * m_fZoomSpeed,
				m_fMinDistance, m_fMaxDistance);
		}

		UpdateModeAndBlend(fDt);

		// Recompute camera transform from orbit params. Initial yaw bakes
		// in a +π so the default view sits south of centre and looks
		// north into the map (matching the authored scene camera).
		const float fEffectiveYaw = m_fOrbitYaw + glm::pi<float>();
		const float fSinY = std::sin(fEffectiveYaw);
		const float fCosY = std::cos(fEffectiveYaw);
		const float fSinP = std::sin(m_fOrbitPitch);
		const float fCosP = std::cos(m_fOrbitPitch);

		const Zenith_Maths::Vector3 xOffset(
			fSinY * fCosP,
			fSinP,
			fCosY * fCosP);
		const Zenith_Maths::Vector3 xCamPos = m_xOrbitTarget + xOffset * m_fOrbitDistance;

		if (Zenith_CameraComponent* pxCam = m_xParentEntity.TryGetComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xCam = *pxCam;

			// Fully birds-eye: run the ORIGINAL orbit path unchanged (the
			// regression guarantee for never-possessed scenes).
			Zenith_Maths::Vector3 xTPCamPos, xTPLookAt;
			if (m_fBlendT <= 0.0f || !ComputeThirdPersonPose(xTPCamPos, xTPLookAt))
			{
				// Keep the blend path's yaw hold-over primed with the live
				// birds-eye yaw so a blend that starts next frame continues
				// from the current view instead of a stale value.
				m_fLastBlendYaw = std::atan2(xOffset.x, -xOffset.z);
				xCam.SetPosition(xCamPos);
				// Camera component computes forward as
				//   fwd = (-sin(yaw)*cos(pitch), sin(pitch), cos(yaw)*cos(pitch))
				// We want fwd = -xOffset (camera-to-target direction). Solving
				// for yaw: sin(yaw) = sin(eff), cos(yaw) = -cos(eff), so
				// yaw = atan2(xOffset.x, -xOffset.z). The original
				// atan2(-xOffset.x, ...) sign was wrong for non-default
				// m_fOrbitYaw — at m_fOrbitYaw = ±π/2 the camera ended up
				// looking AWAY from the target. That bug only manifests when
				// the orbit yaw isn't 0 or π (where sin(eff) = 0 lets the sign
				// error vanish), which is why the default m_fOrbitYaw = 0
				// view rendered correctly regardless.
				xCam.SetYaw(static_cast<double>(std::atan2(xOffset.x, -xOffset.z)));
				xCam.SetPitch(static_cast<double>(-m_fOrbitPitch));
				return;
			}

			// Blend the camera position and look-at point together, then
			// re-solve yaw/pitch from the blended pair. The yaw solve is the
			// same atan2(o.x, -o.z) as the birds-eye branch (generalised to
			// an arbitrary offset); pitch generalises -m_fOrbitPitch to
			// -atan2(vertical, horizontal). Because the POSE is blended (not
			// the angles) there is no ±π yaw-wraparound case to handle.
			const Zenith_Maths::Vector3 xBlendPos =
				glm::mix(xCamPos, xTPCamPos, m_fBlendT);
			const Zenith_Maths::Vector3 xBlendLookAt =
				glm::mix(m_xOrbitTarget, xTPLookAt, m_fBlendT);
			const Zenith_Maths::Vector3 xO = xBlendPos - xBlendLookAt;
			const float fHoriz = std::sqrt(xO.x * xO.x + xO.z * xO.z);

			xCam.SetPosition(xBlendPos);
			// Degenerate-pinch guard: mid-blend the horizontal offset can
			// pass through ~zero when the birds-eye and third-person
			// azimuths oppose each other; atan2 on a near-zero vector would
			// snap the yaw arbitrarily (a one-frame 180° whirl while looking
			// straight down). Hold the last well-defined yaw through the
			// pinch — pitch stays continuous either way.
			if (fHoriz > 0.25f)
			{
				m_fLastBlendYaw = std::atan2(xO.x, -xO.z);
			}
			xCam.SetYaw(static_cast<double>(m_fLastBlendYaw));
			xCam.SetPitch(static_cast<double>(-std::atan2(xO.y, std::max(fHoriz, 0.0001f))));
		}
	}

	void SetOrbitTarget(const Zenith_Maths::Vector3& xTarget) { m_xOrbitTarget = xTarget; }
	void SetOrbitDistance(float f) { m_fOrbitDistance = glm::clamp(f, m_fMinDistance, m_fMaxDistance); }

	// Telemetry-v3 accessors: per-frame camera state sampled by
	// Test_PersonalityPlaythrough's recorder hook. Read-only.
	const Zenith_Maths::Vector3& GetOrbitTarget()   const { return m_xOrbitTarget; }
	float                        GetOrbitDistance() const { return m_fOrbitDistance; }
	float                        GetOrbitYaw()      const { return m_fOrbitYaw; }
	float                        GetOrbitPitch()    const { return m_fOrbitPitch; }

	// Camera-mode observability (tests + telemetry). m_eMode is the blend
	// TARGET; m_fBlendT is where the pose currently sits (0 = birds-eye,
	// 1 = third-person).
	DPCameraMode GetCameraMode() const { return m_eMode; }
	float        GetBlendT()     const { return m_fBlendT; }

	// Raise the upper clamp on orbit distance. DPProcLevelBootstrap calls
	// this so its auto-fit camera distance for large procgen levels
	// isn't silently clamped to the default 150 m cap. Also re-clamps
	// the current distance against the new range so an in-flight zoom
	// stays valid.
	void SetMaxOrbitDistance(float f)
	{
		m_fMaxDistance = f;
		m_fOrbitDistance = glm::clamp(m_fOrbitDistance, m_fMinDistance, m_fMaxDistance);
	}

private:
	// Advance the mode state machine + blend scalar. Auto rule: possessed
	// -> ThirdPerson, unpossessed -> BirdsEye. C toggles a manual override
	// that survives until the next possession CHANGE (edge), then auto
	// resumes. ThirdPerson additionally requires a resolvable villager
	// (the possessed one, else the last one we saw possessed) — without
	// one the target is forced back to BirdsEye.
	void UpdateModeAndBlend(const float fDt)
	{
		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
		const bool bPossessed = xPossessed.IsValid();
		if (bPossessed) m_xLastPossessed = xPossessed;

		if (bPossessed != m_bWasPossessed)
		{
			m_bManualOverrideActive = false; // possession edge: auto resumes
			m_bWasPossessed = bPossessed;
		}

		if (DP_Input::ReadCameraTogglePressed())
		{
			const bool bAutoWantsTP = bPossessed;
			const bool bCurrentTP = m_bManualOverrideActive ? m_bManualWantsThirdPerson : bAutoWantsTP;
			m_bManualOverrideActive   = true;
			m_bManualWantsThirdPerson = !bCurrentTP;
		}

		bool bWantThirdPerson = m_bManualOverrideActive ? m_bManualWantsThirdPerson : bPossessed;
		if (bWantThirdPerson && !ResolveFollowVillager().IsValid())
		{
			bWantThirdPerson = false;
		}

		const DPCameraMode eNewMode = bWantThirdPerson ? DPCameraMode::ThirdPerson : DPCameraMode::BirdsEye;
		if (eNewMode != m_eMode)
		{
			m_eMode = eNewMode;
			// Telemetry: mode edges are rare + meaningful (mode-thrash or a
			// stuck blend shows up in recordings). No-op outside a recording.
			DPTelemetry::EmitEvent(DPTelemetry::DPEventType::CameraModeChanged,
				ResolveFollowVillager(), Zenith_EntityID{},
				static_cast<int32_t>(m_eMode), m_fBlendT, nullptr, "DPOrbitCamera");
		}

		const float fTarget = (m_eMode == DPCameraMode::ThirdPerson) ? 1.0f : 0.0f;
		const float fStep = (m_fBlendDurationS > 0.0001f) ? (fDt / m_fBlendDurationS) : 1.0f;
		if      (m_fBlendT < fTarget) m_fBlendT = std::min(m_fBlendT + fStep, fTarget);
		else if (m_fBlendT > fTarget) m_fBlendT = std::max(m_fBlendT - fStep, fTarget);
	}

	// The villager the third-person pose anchors on: the possessed one,
	// falling back to the last villager we saw possessed (covers the
	// blend-out frames right after possession is lost).
	Zenith_EntityID ResolveFollowVillager() const
	{
		const Zenith_EntityID xPossessed = DP_Player::GetPossessedVillager();
		if (xPossessed.IsValid())
		{
			Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xPossessed);
			if (xEnt.IsValid()) return xPossessed;
		}
		if (m_xLastPossessed.IsValid())
		{
			Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(m_xLastPossessed);
			if (xEnt.IsValid()) return m_xLastPossessed;
		}
		return INVALID_ENTITY_ID;
	}

	// Behind-and-above the followed villager, looking at its chest height.
	// Pose derivation mirrors RenderTest_FollowCameraComponent (rotated
	// back-offset behind the target's facing), with the facing taken from
	// the villager's transform as quat * +Z — NOT glm::eulerAngles, whose
	// yaw collapses for rotations >90° off +Z (documented engine gotcha).
	// Returns false when no villager resolves (caller falls back to the
	// pure birds-eye path).
	bool ComputeThirdPersonPose(Zenith_Maths::Vector3& xCamPosOut,
		Zenith_Maths::Vector3& xLookAtOut) const
	{
		const Zenith_EntityID xVillagerId = ResolveFollowVillager();
		if (!xVillagerId.IsValid()) return false;
		Zenith_Entity xEnt = g_xEngine.Scenes().ResolveEntity(xVillagerId);
		if (!xEnt.IsValid()) return false;
		Zenith_TransformComponent* pxT = xEnt.TryGetComponent<Zenith_TransformComponent>();
		if (pxT == nullptr) return false;

		Zenith_Maths::Vector3 xPos;
		pxT->GetPosition(xPos);
		Zenith_Maths::Quat xRot;
		pxT->GetRotation(xRot);

		Zenith_Maths::Vector3 xForward = xRot * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
		xForward.y = 0.0f;
		const float fLen = glm::length(xForward);
		xForward = (fLen > 0.0001f) ? (xForward / fLen) : Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);

		xCamPosOut = xPos - xForward * m_fThirdPersonDistance
			+ Zenith_Maths::Vector3(0.0f, m_fThirdPersonHeight, 0.0f);
		xLookAtOut = xPos + Zenith_Maths::Vector3(0.0f, m_fThirdPersonLookAtHeight, 0.0f);
		return true;
	}

	Zenith_Entity m_xParentEntity;

	// Pinned at the GameLevel's playable centre (~(50,0,50)). Set via
	// SetOrbitTarget for gym scenes whose action sits at world origin.
	Zenith_Maths::Vector3 m_xOrbitTarget   = Zenith_Maths::Vector3(50.0f, 0.0f, 50.0f);

	// Distance + bounds tuned so the default 80 m radius at ~69° pitch
	// frames the entire 100 × 100 m playable area with a comfortable
	// margin. Mouse wheel can zoom in to 30 m or out to 150 m.
	float m_fDefaultDistance = 80.0f;
	float m_fOrbitDistance   = 80.0f;
	float m_fMinDistance     = 30.0f;
	float m_fMaxDistance     = 150.0f;
	float m_fOrbitYaw        = 1.5707963f;   // π/2 default — see OnAwake comment
	float m_fOrbitPitch      = 1.20f;   // ~69° down — strong bird's-eye tilt
	float m_fRotateSpeed     = 1.5f;    // rad/s — slower since the view is wider
	float m_fZoomSpeed       = 5.0f;    // metres per wheel tick

	// Third-person mode state (2026-07-01). All trivially-movable members —
	// this component deliberately has no this-capturing subscriptions, so
	// the implicit moves stay correct.
	DPCameraMode    m_eMode                   = DPCameraMode::BirdsEye;
	float           m_fBlendT                 = 0.0f;  // 0 = birds-eye pose, 1 = third-person pose
	float           m_fBlendDurationS         = 0.4f;
	float           m_fLastBlendYaw           = 0.0f;  // hold-over for the degenerate-pinch guard
	float           m_fThirdPersonDistance    = 4.5f;  // metres behind the villager
	float           m_fThirdPersonHeight      = 2.5f;  // metres above its feet
	float           m_fThirdPersonLookAtHeight = 1.5f; // look-at at chest height
	Zenith_EntityID m_xLastPossessed          = INVALID_ENTITY_ID;
	bool            m_bWasPossessed           = false;
	bool            m_bManualOverrideActive   = false;
	bool            m_bManualWantsThirdPerson = false;
};
