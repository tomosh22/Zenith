#pragma once

#include "Maths/Zenith_Maths.h"

// Pure, deterministic authored-waypoint patrol state. Runtime glue owns the
// physics body and supplies its current position/velocity; this layer has no
// ECS, scene, input, UI, navmesh, RNG, or process-global dependencies.
struct ZM_WalkerWaypoints
{
	static constexpr u_int uMAX_WAYPOINTS = 4u;

	Zenith_Maths::Vector3 m_axPoints[uMAX_WAYPOINTS] = {};
	u_int m_uCount = 0u;
};

struct ZM_WalkerState
{
	u_int m_uTargetIndex = 0u;
	float m_fDwellRemaining = 0.0f;
};

struct ZM_WalkerTuning
{
	float m_fSpeed = 1.6f;
	float m_fArriveRadius = 0.35f;
	float m_fDwellSeconds = 3.0f;
};

struct ZM_WalkerStep
{
	Zenith_Maths::Vector3 m_xDirXZ = Zenith_Maths::Vector3(0.0f);
	float m_fSpeed = 0.0f;
	bool m_bArrivedThisStep = false;
};

// Advances only the explicit cursor/dwell state. Direction and arrival distance
// are XZ-only; an arrival is inclusive and installs the full dwell without
// consuming this call's dt. A halted walker pauses both arrival and dwell.
ZM_WalkerStep ZM_StepWalker(const ZM_WalkerWaypoints& xWaypoints,
	ZM_WalkerState& xState,
	const Zenith_Maths::Vector3& xCurrentPosition,
	float fDt,
	bool bHalted,
	const ZM_WalkerTuning& xTuning);

// Converts a patrol direction/speed into a body velocity while leaving gravity
// and terrain response in sole ownership of the existing vertical velocity.
Zenith_Maths::Vector3 ZM_BuildPatrolVelocity(
	const Zenith_Maths::Vector3& xDirectionXZ,
	float fSpeed,
	const Zenith_Maths::Vector3& xCurrentVelocity);
