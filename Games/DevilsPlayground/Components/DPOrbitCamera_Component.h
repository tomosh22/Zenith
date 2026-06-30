#pragma once
#include "Core/Zenith_Engine.h"
/**
 * DPOrbitCamera_Component - Bird's-eye orbit camera centred on the village.
 *
 * The camera does NOT follow the possessed villager. The whole playable
 * area (~(0..100, 0..100) in world space, centre ~(50, 0, 50)) is meant
 * to be visible at all times so the player can see every villager,
 * priest, door, and item simultaneously. The orbit target is therefore
 * pinned at the map centre and never moves. Q/E rotate the bird's-eye
 * view around the centre; mouse wheel zooms the radius.
 *
 * Zenith_CameraComponent is FPS-style (position + pitch + yaw + FOV);
 * the orbit math is implemented as a game component on the camera entity.
 */

#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "DataStream/Zenith_DataStream.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

#include "Source/PublicInterfaces.h"
#include "Source/DPInputActions.h"

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
};
