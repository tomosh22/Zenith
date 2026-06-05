#pragma once

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Input/Zenith_Input.h"
#include "Maths/Zenith_Maths.h"

#include "CityBuilder/Source/CB_CameraController.h"

// ============================================================================
// CB_CityCamera_Behaviour — the player's RTS-style camera.
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
class CB_CityCamera_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(CB_CityCamera_Behaviour)

	CB_CityCamera_Behaviour() = delete;
	CB_CityCamera_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnAwake() ZENITH_FINAL override
	{
		// Fresh controller each play session (static-state discipline: behaviours
		// persist their object across editor Play/Stop cycles otherwise).
		m_xController = CB_CameraController();
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
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
		if (xInput.IsKeyHeld(ZENITH_KEY_Q)) { fYawDelta -= m_fKeyRotateSpeed * fDt; }
		if (xInput.IsKeyHeld(ZENITH_KEY_E)) { fYawDelta += m_fKeyRotateSpeed * fDt; }
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
		if (xInput.IsKeyHeld(ZENITH_KEY_W)) { fForward += fKeyPan; }
		if (xInput.IsKeyHeld(ZENITH_KEY_S)) { fForward -= fKeyPan; }
		if (xInput.IsKeyHeld(ZENITH_KEY_D)) { fRight   += fKeyPan; }
		if (xInput.IsKeyHeld(ZENITH_KEY_A)) { fRight   -= fKeyPan; }
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
		if (m_xParentEntity.HasComponent<Zenith_CameraComponent>())
		{
			Zenith_CameraComponent& xCam = m_xParentEntity.GetComponent<Zenith_CameraComponent>();
			Zenith_Maths::Vector3 xPos;
			double fYaw = 0.0;
			double fPitch = 0.0;
			m_xController.ComputeCamera(xPos, fYaw, fPitch);
			xCam.SetPosition(xPos);
			xCam.SetYaw(fYaw);
			xCam.SetPitch(fPitch);
		}
	}

	CB_CameraController& GetController() { return m_xController; }

private:
	CB_CameraController m_xController;

	float m_fZoomSpeed         = 20.0f;    // world units per wheel tick
	float m_fKeyRotateSpeed    = 1.5f;     // rad/s for Q/E
	float m_fMouseRotateSpeed  = 0.005f;   // rad per pixel of right-drag
	float m_fPanSpeed          = 0.6f;     // fraction of distance per second at full key
	float m_fMouseDragPanSpeed = 0.0015f;  // per pixel of middle-drag, scaled by distance
};
