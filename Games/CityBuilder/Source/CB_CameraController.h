#pragma once

#include "Maths/Zenith_Maths.h"
#include <cmath>

// ============================================================================
// CB_CameraController — pure orbit/pan/zoom camera math for the city builder.
//
// Holds the orbit state (target point on the ground plane, distance, yaw,
// pitch) and derives an FPS-style camera transform (position + yaw + pitch)
// consumable by Zenith_CameraComponent. Deliberately free of engine/ECS/input
// types so it can be unit-tested in isolation (Phase 1 ships no camera unit
// test, but the seam is here for later).
//
// The transform math mirrors the proven DPOrbitCamera_Behaviour: the camera
// sits on a sphere around the target; the +pi yaw offset and the
// atan2(offset.x, -offset.z) facing solve match how Zenith_CameraComponent
// derives its forward vector.
// ============================================================================
struct CB_CameraController
{
	// Orbit state. Target defaults to the centre of the 4096x4096 terrain
	// footprint (terrain mesh spans 0..TERRAIN_SIZE in X/Z, not centred).
	Zenith_Maths::Vector3 m_xTarget   = Zenith_Maths::Vector3(2048.0f, 0.0f, 2048.0f);
	float                 m_fDistance = 400.0f;
	float                 m_fYaw      = 0.0f;    // radians, orbit azimuth
	float                 m_fPitch    = 0.95f;   // radians downward tilt (~54 deg)

	// Limits.
	float m_fMinDistance = 30.0f;
	float m_fMaxDistance = 2500.0f;
	float m_fMinPitch    = 0.20f;   // near-horizontal
	float m_fMaxPitch    = 1.45f;   // near-top-down (just under pi/2)

	// Zoom in/out. Positive fDelta moves the camera closer.
	void Zoom(float fDelta)
	{
		m_fDistance = glm::clamp(m_fDistance - fDelta, m_fMinDistance, m_fMaxDistance);
	}

	// Rotate the orbit. fPitchDelta is clamped to keep the view above the ground.
	void Rotate(float fYawDelta, float fPitchDelta)
	{
		m_fYaw += fYawDelta;
		m_fPitch = glm::clamp(m_fPitch + fPitchDelta, m_fMinPitch, m_fMaxPitch);
	}

	// Pan the orbit target across the ground plane. fRight / fForward are
	// world-unit deltas along the camera's right and (XZ-projected) forward
	// axes, so panning always tracks the current view direction.
	void Pan(float fRight, float fForward)
	{
		const float fEffectiveYaw = m_fYaw + glm::pi<float>();
		const float fSinY = std::sin(fEffectiveYaw);
		const float fCosY = std::cos(fEffectiveYaw);
		// Camera-to-target horizontal forward = -normalize(offset.xz) = (-sinY, -cosY).
		const Zenith_Maths::Vector3 xForward(-fSinY, 0.0f, -fCosY);
		// Right = forward rotated -90 deg about +Y.
		const Zenith_Maths::Vector3 xRight(-fCosY, 0.0f, fSinY);
		m_xTarget += xForward * fForward + xRight * fRight;
	}

	// Derive the camera transform. fOutYaw / fOutPitch are in the convention
	// Zenith_CameraComponent expects (doubles, radians).
	void ComputeCamera(Zenith_Maths::Vector3& xOutPos, double& fOutYaw, double& fOutPitch) const
	{
		const float fEffectiveYaw = m_fYaw + glm::pi<float>();
		const float fSinY = std::sin(fEffectiveYaw);
		const float fCosY = std::cos(fEffectiveYaw);
		const float fSinP = std::sin(m_fPitch);
		const float fCosP = std::cos(m_fPitch);

		const Zenith_Maths::Vector3 xOffset(fSinY * fCosP, fSinP, fCosY * fCosP);
		xOutPos   = m_xTarget + xOffset * m_fDistance;
		fOutYaw   = static_cast<double>(std::atan2(xOffset.x, -xOffset.z));
		fOutPitch = static_cast<double>(-m_fPitch);
	}
};
