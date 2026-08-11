#pragma once
#include "Core/Zenith_Engine.h"

#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"

#include "CityBuilder/CB_Bindings.h"
#include "CityBuilder/Source/CB_CameraController.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// CB_CityCameraComponent — the player's RTS-style camera.
//
//   ORBIT_MODIFIER + ORBIT_DELTA    : right-drag orbits (yaw + pitch)
//   ORBIT_RATE                      : pad right stick orbits
//   ROTATE                          : Q / E rotate yaw
//   PAN_DRAG_MODIFIER + _DELTA      : middle-drag pans across the ground plane
//   PAN                             : WASD / pad left stick pan
//   ZOOM_DELTA / ZOOM_RATE          : wheel / pad trigger pair zoom
//
// Every one of those is an ACTION from CB_Bindings.h — there is no raw key,
// mouse button or pad code in this file. It mutates a CB_CameraController and
// writes the result onto the entity's Zenith_CameraComponent every frame.
// Pan/zoom speeds scale with the orbit distance so the feel stays consistent
// across zoom levels.
//
// ★ DISPLACEMENT AND RATE ARE APPLIED SEPARATELY, AND THAT IS THE WHOLE REASON
// ORBIT AND ZOOM ARE TWO ACTIONS EACH. A mouse/wheel value is already integrated
// over the frame, so multiplying it by dt would make the camera frame-rate
// dependent; a stick/trigger value is a deflection that only becomes an angle or
// a distance once multiplied by dt. Both halves are applied every frame and a
// resting device contributes exactly zero, so there is no mode to switch.
// ============================================================================
class CB_CityCameraComponent
{
public:
	CB_CityCameraComponent() = delete;
	CB_CityCameraComponent(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	// Component pools relocate components on resize / swap-and-pop (move-construct
	// + destruct the source), so the moves are hand-written: the published static
	// instance pointer must follow the live object. Copies deleted.
	CB_CityCameraComponent(const CB_CityCameraComponent&) = delete;
	CB_CityCameraComponent& operator=(const CB_CityCameraComponent&) = delete;

	CB_CityCameraComponent(CB_CityCameraComponent&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_xController(xOther.m_xController)
		, m_fZoomSpeed(xOther.m_fZoomSpeed)
		, m_fZoomRateSpeed(xOther.m_fZoomRateSpeed)
		, m_fKeyRotateSpeed(xOther.m_fKeyRotateSpeed)
		, m_fMouseRotateSpeed(xOther.m_fMouseRotateSpeed)
		, m_fPadRotateSpeed(xOther.m_fPadRotateSpeed)
		, m_fPanSpeed(xOther.m_fPanSpeed)
		, m_fMouseDragPanSpeed(xOther.m_fMouseDragPanSpeed)
	{
		if (s_pxActive == &xOther)
		{
			s_pxActive = this;
		}
	}

	CB_CityCameraComponent& operator=(CB_CityCameraComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity      = xOther.m_xParentEntity;
			m_xController        = xOther.m_xController;
			m_fZoomSpeed         = xOther.m_fZoomSpeed;
			m_fZoomRateSpeed     = xOther.m_fZoomRateSpeed;
			m_fKeyRotateSpeed    = xOther.m_fKeyRotateSpeed;
			m_fMouseRotateSpeed  = xOther.m_fMouseRotateSpeed;
			m_fPadRotateSpeed    = xOther.m_fPadRotateSpeed;
			m_fPanSpeed          = xOther.m_fPanSpeed;
			m_fMouseDragPanSpeed = xOther.m_fMouseDragPanSpeed;
			if (s_pxActive == &xOther)
			{
				s_pxActive = this;
			}
		}
		return *this;
	}

	// Component contract. The orbit state is runtime-only (reset fresh each play
	// session); only the version tag persists.
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
	void RenderPropertiesPanel()
	{
		ImGui::Text("Distance: %.1f", m_xController.m_fDistance);
		ImGui::Text("Yaw: %.3f  Pitch: %.3f", m_xController.m_fYaw, m_xController.m_fPitch);
		ImGui::Text("Target: %.1f, %.1f, %.1f", m_xController.m_xTarget.x, m_xController.m_xTarget.y, m_xController.m_xTarget.z);
	}
#endif

	void OnAwake()
	{
		// Fresh controller each play session (static-state discipline: component
		// state persists across editor Play/Stop cycles otherwise).
		m_xController = CB_CameraController();
	}

	void OnUpdate(const float fDt)
	{
		s_pxActive = this;   // expose the live controller (tests/automation tilt the view)

		// ---- Zoom: wheel DISPLACEMENT + pad-trigger RATE ----
		const float fZoomDelta = CB_Bindings::ReadZoomDelta();
		if (fZoomDelta != 0.0f)
		{
			m_xController.Zoom(fZoomDelta * m_fZoomSpeed);
		}
		const float fZoomRate = CB_Bindings::ReadZoomRate();
		if (fZoomRate != 0.0f)
		{
			m_xController.Zoom(fZoomRate * m_fZoomRateSpeed * fDt);
		}

		// ---- Rotate: ROTATE keys + right-drag ORBIT_DELTA + pad ORBIT_RATE ----
		// The two mouse signs are this camera's own convention (drag right turns
		// right, drag down tilts toward top-down); the binding rows pass the
		// device value through at scale 1 because the pan-drag rows below want
		// the OPPOSITE x sign from the same source.
		float fYawDelta = CB_Bindings::ReadRotate() * m_fKeyRotateSpeed * fDt;
		float fPitchDelta = 0.0f;
		if (CB_Bindings::IsOrbitModifierHeld())
		{
			const Zenith_Maths::Vector2 xOrbit = CB_Bindings::ReadOrbitDelta();
			fYawDelta   += xOrbit.x * m_fMouseRotateSpeed;
			fPitchDelta += xOrbit.y * m_fMouseRotateSpeed;
		}
		{
			const Zenith_Maths::Vector2 xOrbitRate = CB_Bindings::ReadOrbitRate();
			fYawDelta   += xOrbitRate.x * m_fPadRotateSpeed * fDt;
			fPitchDelta += xOrbitRate.y * m_fPadRotateSpeed * fDt;
		}
		if (fYawDelta != 0.0f || fPitchDelta != 0.0f)
		{
			m_xController.Rotate(fYawDelta, fPitchDelta);
		}

		// ---- Pan (PAN + middle-drag), speed proportional to zoom distance ----
		// PAN is +y FORWARD / +x RIGHT with opposite keys cancelling, which is
		// exactly what the retired four IsKeyDown polls summed to; the pad's left
		// stick additionally gives an analog magnitude.
		const float fKeyPan = m_xController.m_fDistance * m_fPanSpeed * fDt;
		const Zenith_Maths::Vector2 xPan = CB_Bindings::ReadPan();
		float fRight   = xPan.x * fKeyPan;
		float fForward = xPan.y * fKeyPan;
		if (CB_Bindings::IsPanDragModifierHeld())
		{
			const Zenith_Maths::Vector2 xDrag = CB_Bindings::ReadPanDragDelta();
			const float fDragPan = m_xController.m_fDistance * m_fMouseDragPanSpeed;
			fRight   -= xDrag.x * fDragPan;
			fForward += xDrag.y * fDragPan;
		}
		if (fRight != 0.0f || fForward != 0.0f)
		{
			m_xController.Pan(fRight, fForward);
		}

		// ---- Drive the camera component ----
		if (Zenith_CameraComponent* pxCam = m_xParentEntity.TryGetComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xCam = *pxCam;
			Zenith_Maths::Vector3 xPos;
			double fYaw = 0.0;
			double fPitch = 0.0;
			m_xController.ComputeCamera(xPos, fYaw, fPitch);
			xCam.SetPosition(xPos);
			xCam.SetYaw(fYaw);
			xCam.SetPitch(fPitch);
		}
	}

	void OnDestroy()
	{
		// The static points at `this` (a pooled component) — never leave it dangling.
		if (s_pxActive == this)
		{
			s_pxActive = nullptr;
		}
	}

	CB_CameraController& GetController() { return m_xController; }

	// The live camera component (set each frame in OnUpdate). Lets tests/automation
	// drive the orbit (e.g. an oblique angle to show the terrain relief).
	static CB_CityCameraComponent* GetActive() { return s_pxActive; }

private:
	static inline CB_CityCameraComponent* s_pxActive = nullptr;
	Zenith_Entity m_xParentEntity;
	CB_CameraController m_xController;

	float m_fZoomSpeed         = 20.0f;    // world units per wheel notch (ZOOM_DELTA)
	float m_fZoomRateSpeed     = 240.0f;   // world units per second at full trigger (ZOOM_RATE)
	float m_fKeyRotateSpeed    = 1.5f;     // rad/s for ROTATE (Q/E)
	float m_fMouseRotateSpeed  = 0.005f;   // rad per pixel of ORBIT_DELTA (right-drag)
	float m_fPadRotateSpeed    = 1.8f;     // rad/s at full stick for ORBIT_RATE
	float m_fPanSpeed          = 0.6f;     // fraction of distance per second at full PAN
	float m_fMouseDragPanSpeed = 0.0015f;  // per pixel of PAN_DRAG_DELTA, scaled by distance
};
