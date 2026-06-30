#pragma once
#include "Core/Zenith_Engine.h"

#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DataStream/Zenith_DataStream.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

#include "CityBuilder/Source/CB_CameraController.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// ============================================================================
// CB_CityCameraComponent — the player's RTS-style camera.
//
//   Right-drag  : orbit (yaw + pitch)
//   Q / E       : rotate yaw
//   Middle-drag : pan across the ground plane
//   W A S D     : pan
//   Mouse wheel : zoom
//
// Reads input, mutates a CB_CameraController, and writes the result onto the
// entity's Zenith_CameraComponent every frame. Pan/zoom speeds scale with the
// orbit distance so the feel stays consistent across zoom levels.
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
		, m_fKeyRotateSpeed(xOther.m_fKeyRotateSpeed)
		, m_fMouseRotateSpeed(xOther.m_fMouseRotateSpeed)
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
			m_fKeyRotateSpeed    = xOther.m_fKeyRotateSpeed;
			m_fMouseRotateSpeed  = xOther.m_fMouseRotateSpeed;
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
		Zenith_Input& xInput = g_xEngine.Input();

		// ---- Zoom (mouse wheel) ----
		const float fWheel = xInput.GetMouseWheelDelta();
		if (fWheel != 0.0f)
		{
			m_xController.Zoom(fWheel * m_fZoomSpeed);
		}

		// ---- Rotate (Q/E + right-drag) ----
		float fYawDelta = 0.0f;
		float fPitchDelta = 0.0f;
		if (xInput.IsKeyDown(ZENITH_KEY_Q)) { fYawDelta -= m_fKeyRotateSpeed * fDt; }
		if (xInput.IsKeyDown(ZENITH_KEY_E)) { fYawDelta += m_fKeyRotateSpeed * fDt; }
		if (xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_RIGHT))
		{
			Zenith_Maths::Vector2_64 xDelta;
			xInput.GetMouseDelta(xDelta);
			fYawDelta   += static_cast<float>(xDelta.x) * m_fMouseRotateSpeed;
			fPitchDelta += static_cast<float>(xDelta.y) * m_fMouseRotateSpeed;
		}
		if (fYawDelta != 0.0f || fPitchDelta != 0.0f)
		{
			m_xController.Rotate(fYawDelta, fPitchDelta);
		}

		// ---- Pan (WASD + middle-drag), speed proportional to zoom distance ----
		const float fKeyPan = m_xController.m_fDistance * m_fPanSpeed * fDt;
		float fRight = 0.0f;
		float fForward = 0.0f;
		if (xInput.IsKeyDown(ZENITH_KEY_W)) { fForward += fKeyPan; }
		if (xInput.IsKeyDown(ZENITH_KEY_S)) { fForward -= fKeyPan; }
		if (xInput.IsKeyDown(ZENITH_KEY_D)) { fRight   += fKeyPan; }
		if (xInput.IsKeyDown(ZENITH_KEY_A)) { fRight   -= fKeyPan; }
		if (xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_MIDDLE))
		{
			Zenith_Maths::Vector2_64 xDelta;
			xInput.GetMouseDelta(xDelta);
			const float fDragPan = m_xController.m_fDistance * m_fMouseDragPanSpeed;
			fRight   -= static_cast<float>(xDelta.x) * fDragPan;
			fForward += static_cast<float>(xDelta.y) * fDragPan;
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

	float m_fZoomSpeed         = 20.0f;    // world units per wheel tick
	float m_fKeyRotateSpeed    = 1.5f;     // rad/s for Q/E
	float m_fMouseRotateSpeed  = 0.005f;   // rad per pixel of right-drag
	float m_fPanSpeed          = 0.6f;     // fraction of distance per second at full key
	float m_fMouseDragPanSpeed = 0.0015f;  // per pixel of middle-drag, scaled by distance
};
