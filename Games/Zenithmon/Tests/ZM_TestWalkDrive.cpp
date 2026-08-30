#include "Zenith.h"

#include "Zenithmon/Tests/ZM_TestWalkDrive.h"

#include "EntityComponent/Components/Zenith_CameraComponent.h"   // GetFacingDir
#include "EntityComponent/Zenith_CameraResolve.h"                // Zenith_GetMainCameraAcrossScenes
#include "Input/Zenith_InputSimulator.h"

#include <cmath>   // std::isfinite / std::sqrt

// ============================================================================
// See the header for the contract and for WHY this is one function rather than
// the eight copies it replaces. This file is the ORDER, and the arithmetic below
// is unchanged from those copies, deliberately and to the operation -- the
// consolidation must not move a single measured walk.
// ============================================================================

namespace
{
	bool ZM_IsFiniteVector3XZ(const Zenith_Maths::Vector3& xValue)
	{
		return std::isfinite(xValue.x) && std::isfinite(xValue.z);
	}
}

ZM_WalkDriveKeys ZM_ComputeWalkDriveKeys(
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Vector3& xTarget,
	const Zenith_Maths::Vector3& xCameraForward)
{
	ZM_WalkDriveKeys xKeys;

	// FAIL CLOSED. See the header's totality table: this is what the eight
	// copies already did by accident (every comparison against a NaN is false),
	// stated on purpose so m_bArrived cannot inherit "no key held" as "arrived".
	if (!ZM_IsFiniteVector3XZ(xPosition)
		|| !ZM_IsFiniteVector3XZ(xTarget)
		|| !ZM_IsFiniteVector3XZ(xCameraForward))
	{
		return xKeys;
	}

	// The camera's XZ basis. ZM_PlayerController::BuildCameraRelativeDirection
	// interprets W/A/S/D against exactly this, so a driver that chose keys from
	// raw world dx/dz would be correct only for a single leg walked from rest --
	// which is the defect ZM-D-130/131 fixed, in three copies.
	Zenith_Maths::Vector3 xForward(xCameraForward.x, 0.0f, xCameraForward.z);
	const float fForwardLengthSq = xForward.x * xForward.x + xForward.z * xForward.z;
	if (fForwardLengthSq <= 0.000001f)
	{
		// A straight-down (or absent) camera has no XZ facing. +Z is
		// ZM_FollowCamera's resting orientation, so this degenerates to the
		// pre-ZM-D-130 world-space behaviour rather than to nothing.
		xForward = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	}
	else
	{
		xForward /= std::sqrt(fForwardLengthSq);
	}
	const Zenith_Maths::Vector3 xRight(xForward.z, 0.0f, -xForward.x);

	const Zenith_Maths::Vector3 xToTarget(
		xTarget.x - xPosition.x, 0.0f, xTarget.z - xPosition.z);
	const float fForwardAmount = xToTarget.x * xForward.x + xToTarget.z * xForward.z;
	const float fRightAmount   = xToTarget.x * xRight.x   + xToTarget.z * xRight.z;

	// ★★ HERE IS THE QUANTISATION, AND IT IS THE WHOLE SUBJECT OF THIS FILE.
	// Each axis independently answers one of three things -- key, opposite key,
	// or nothing -- so the eight reachable outputs are the eight compass
	// directions of the CAMERA's basis, never the true bearing to the target. On
	// a bearing that falls between two of them the driver alternates, and since
	// the camera is meanwhile rotating to follow the player, the path bows away
	// from the straight line instead of averaging onto it. See the header.
	if (fRightAmount < -fZM_WALK_DRIVE_DEAD_ZONE)
	{
		xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_A] = true;
	}
	else if (fRightAmount > fZM_WALK_DRIVE_DEAD_ZONE)
	{
		xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_D] = true;
	}
	if (fForwardAmount < -fZM_WALK_DRIVE_DEAD_ZONE)
	{
		xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_S] = true;
	}
	else if (fForwardAmount > fZM_WALK_DRIVE_DEAD_ZONE)
	{
		xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_W] = true;
	}

	xKeys.m_bArrived = !xKeys.AnyHeld();
	return xKeys;
}

ZM_WalkDriveKeys ZM_DriveWalkTowardXZ(
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Vector3& xTarget,
	bool bRun)
{
	Zenith_Maths::Vector3 xCameraForward(0.0f, 0.0f, 1.0f);
	if (Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes())
	{
		pxCamera->GetFacingDir(xCameraForward);
	}

	const ZM_WalkDriveKeys xKeys =
		ZM_ComputeWalkDriveKeys(xPosition, xTarget, xCameraForward);

	// Submitted in the SAME ORDER the eight copies used (strafe, then forward,
	// then run). Nothing should depend on it -- they are all held for the same
	// frame -- but a consolidation is not the place to find out.
	if (xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_A])
	{
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_A);
	}
	if (xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_D])
	{
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_D);
	}
	if (xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_S])
	{
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_S);
	}
	if (xKeys.m_abKeys[ZM_WALK_DRIVE_KEY_W])
	{
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_W);
	}
	if (bRun)
	{
		// UNCONDITIONALLY, exactly as before -- the seven copies that ran held
		// SHIFT even on the frame they held no direction key.
		Zenith_InputSimulator::SimulateKeyDown(ZENITH_KEY_LEFT_SHIFT);
	}
	return xKeys;
}
