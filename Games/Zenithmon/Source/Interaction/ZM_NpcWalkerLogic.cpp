#include "Zenith.h"

#include "Zenithmon/Source/Interaction/ZM_NpcWalkerLogic.h"

#include <cmath>

namespace
{
	float ZM_NonNegativeFiniteOrZero(float fValue)
	{
		return (std::isfinite(fValue) && fValue > 0.0f) ? fValue : 0.0f;
	}

	bool ZM_IsFiniteXZ(const Zenith_Maths::Vector3& xValue)
	{
		return std::isfinite(xValue.x) && std::isfinite(xValue.z);
	}
}

ZM_WalkerStep ZM_StepWalker(const ZM_WalkerWaypoints& xWaypoints,
	ZM_WalkerState& xState,
	const Zenith_Maths::Vector3& xCurrentPosition,
	float fDt,
	bool bHalted,
	const ZM_WalkerTuning& xTuning)
{
	ZM_WalkerStep xStep{};

	// A malformed serialized count must never make the fixed array unsafe.
	const u_int uCount = (xWaypoints.m_uCount <= ZM_WalkerWaypoints::uMAX_WAYPOINTS)
		? xWaypoints.m_uCount
		: ZM_WalkerWaypoints::uMAX_WAYPOINTS;
	if (uCount == 0u)
	{
		xState.m_uTargetIndex = 0u;
		xState.m_fDwellRemaining = 0.0f;
		return xStep;
	}

	// Normalise invalid persisted/runtime state before any early-out. Halting then
	// pauses the resulting valid state without consuming an arrival or dwell time.
	xState.m_uTargetIndex %= uCount;
	xState.m_fDwellRemaining = ZM_NonNegativeFiniteOrZero(xState.m_fDwellRemaining);
	if (bHalted)
	{
		return xStep;
	}

	if (xState.m_fDwellRemaining > 0.0f)
	{
		const float fSafeDt = ZM_NonNegativeFiniteOrZero(fDt);
		xState.m_fDwellRemaining = (fSafeDt < xState.m_fDwellRemaining)
			? (xState.m_fDwellRemaining - fSafeDt)
			: 0.0f;
		return xStep;
	}

	const Zenith_Maths::Vector3& xTarget = xWaypoints.m_axPoints[xState.m_uTargetIndex];
	if (!ZM_IsFiniteXZ(xCurrentPosition) || !ZM_IsFiniteXZ(xTarget))
	{
		return xStep;
	}

	const float fDeltaX = xTarget.x - xCurrentPosition.x;
	const float fDeltaZ = xTarget.z - xCurrentPosition.z;
	const float fDistance = std::hypot(fDeltaX, fDeltaZ);
	if (!std::isfinite(fDistance))
	{
		return xStep;
	}

	const float fArriveRadius = ZM_NonNegativeFiniteOrZero(xTuning.m_fArriveRadius);
	if (fDistance <= fArriveRadius)
	{
		xStep.m_bArrivedThisStep = true;
		xState.m_uTargetIndex = (xState.m_uTargetIndex + 1u) % uCount;
		xState.m_fDwellRemaining = ZM_NonNegativeFiniteOrZero(xTuning.m_fDwellSeconds);
		return xStep;
	}

	const float fSpeed = ZM_NonNegativeFiniteOrZero(xTuning.m_fSpeed);
	if (fSpeed == 0.0f)
	{
		return xStep;
	}

	const float fInverseDistance = 1.0f / fDistance;
	xStep.m_xDirXZ = Zenith_Maths::Vector3(
		fDeltaX * fInverseDistance,
		0.0f,
		fDeltaZ * fInverseDistance);
	xStep.m_fSpeed = fSpeed;
	return xStep;
}

Zenith_Maths::Vector3 ZM_BuildPatrolVelocity(
	const Zenith_Maths::Vector3& xDirectionXZ,
	float fSpeed,
	const Zenith_Maths::Vector3& xCurrentVelocity)
{
	float fDirectionX = 0.0f;
	float fDirectionZ = 0.0f;
	const float fSafeSpeed = ZM_NonNegativeFiniteOrZero(fSpeed);
	if ((fSafeSpeed > 0.0f) && ZM_IsFiniteXZ(xDirectionXZ))
	{
		const float fLength = std::hypot(xDirectionXZ.x, xDirectionXZ.z);
		if (std::isfinite(fLength) && (fLength > 0.0f))
		{
			fDirectionX = xDirectionXZ.x / fLength;
			fDirectionZ = xDirectionXZ.z / fLength;
		}
	}

	return Zenith_Maths::Vector3(
		fDirectionX * fSafeSpeed,
		xCurrentVelocity.y,
		fDirectionZ * fSafeSpeed);
}
