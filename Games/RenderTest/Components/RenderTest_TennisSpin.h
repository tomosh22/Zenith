#pragma once
#include "Maths/Zenith_Maths.h"
#include "RenderTest/Components/RenderTest_TennisTypes.h"

#include <cmath>

// Pure spin physics for the tennis testbed. Engine-free (Zenith_Maths only).
//
// One source of truth for Magnus + spin-decay + spin-aware bounce. The referee
// applies MagnusDeltaV each frame to the live ball, decays the angular velocity,
// and on a detected bounce overrides the ball velocity with BounceVelocity()
// computed from the stored PRE-impact velocity (the Jolt body restitution is set
// near zero so this model owns the bounce; see D6 in the plan).
//
// Frame: +Y up. "topspin" for travel direction d has angular-velocity axis
// cross(up, d): for d = +Z that is +X, and Magnus = k*cross(omega, v) then points
// -Y (the ball dips) — verified by the Phase-0 sign tests.

namespace RenderTest_Tennis
{
	// Velocity change from the Magnus effect over dt. Linear in dt, independent
	// of mass, and zero when the spin axis is parallel to the velocity.
	inline Zenith_Maths::Vector3 MagnusDeltaV(const Zenith_Maths::Vector3& xVel,
		const Zenith_Maths::Vector3& xSpinAngVel, float fK, float fDt)
	{
		return Zenith_Maths::Cross(xSpinAngVel, xVel) * (fK * fDt);
	}

	// Exponential-ish spin decay; never flips sign, never grows.
	inline Zenith_Maths::Vector3 ApplySpinDecay(const Zenith_Maths::Vector3& xSpin,
		float fDecayPerSec, float fDt)
	{
		const float fScale = glm::clamp(1.0f - fDecayPerSec * fDt, 0.0f, 1.0f);
		return xSpin * fScale;
	}

	// Angular velocity (rad/s) imparted by a shot of the given type, launched in
	// horizontal direction xHitDir, at the given pace. Topspin spins about
	// +cross(up,dir) (Magnus dips the ball); slice/drop spin the opposite way
	// (the ball floats / skids); flat carries a small topspin for stability; lob
	// carries moderate topspin. The magnitude scales mildly with pace.
	inline Zenith_Maths::Vector3 SpinAngVelForShot(TennisShotType eType,
		const Zenith_Maths::Vector3& xHitDir, float fPace)
	{
		const Zenith_Maths::Vector3 xUp(0.0f, 1.0f, 0.0f);
		Zenith_Maths::Vector3 xDir = xHitDir;
		xDir.y = 0.0f;
		const float fLen = Zenith_Maths::Length(xDir);
		if (fLen < 1e-5f)
			return Zenith_Maths::Vector3(0.0f);
		xDir /= fLen;

		const Zenith_Maths::Vector3 xTopAxis = Zenith_Maths::Cross(xUp, xDir);   // unit (xUp,xDir orthogonal)

		float fMag = 0.0f;   // signed: + topspin, - backspin
		switch (eType)
		{
		case TENNIS_SHOT_TYPE_FLAT:    fMag =  2.0f;  break;
		case TENNIS_SHOT_TYPE_TOPSPIN: fMag = 12.0f;  break;
		case TENNIS_SHOT_TYPE_SLICE:   fMag = -9.0f;  break;
		case TENNIS_SHOT_TYPE_DROP:    fMag = -7.0f;  break;
		case TENNIS_SHOT_TYPE_LOB:     fMag =  9.0f;  break;
		}

		// Mild pace scaling (pace ~9..16 in the testbed).
		const float fPaceScale = 0.6f + 0.04f * fPace;
		return xTopAxis * (fMag * fPaceScale);
	}

	// Post-bounce velocity, computed from the pre-impact velocity. vy flips and
	// scales by restitution; topspin kicks the ball forward and higher; slice
	// lowers the bounce and shortens the horizontal carry; sidespin deflects
	// laterally. friction is the horizontal energy LOSS fraction (retain 1-f).
	inline Zenith_Maths::Vector3 BounceVelocity(const Zenith_Maths::Vector3& xVelIn,
		const Zenith_Maths::Vector3& xSpinAngVel, float fRestitution, float fFriction,
		float fTopspinKick, float fSliceSkid)
	{
		const Zenith_Maths::Vector3 xUp(0.0f, 1.0f, 0.0f);

		float fVyOut = -xVelIn.y * fRestitution;   // xVelIn.y < 0 on a descending impact

		Zenith_Maths::Vector3 xHoriz(xVelIn.x, 0.0f, xVelIn.z);
		const float fHorizSpeed = Zenith_Maths::Length(xHoriz);
		Zenith_Maths::Vector3 xHorizDir(0.0f);
		if (fHorizSpeed > 1e-5f)
			xHorizDir = xHoriz / fHorizSpeed;

		// Retain (1 - friction) of the horizontal carry.
		Zenith_Maths::Vector3 xHorizOut = xHoriz * glm::clamp(1.0f - fFriction, 0.0f, 1.0f);

		if (fHorizSpeed > 1e-5f)
		{
			const Zenith_Maths::Vector3 xTopAxis = Zenith_Maths::Cross(xUp, xHorizDir);
			// Signed topspin amount about the travel-relative top axis (+ topspin).
			const float fSpinTop = glm::clamp(Zenith_Maths::Dot(xSpinAngVel, xTopAxis) * 0.02f, -1.0f, 1.0f);
			if (fSpinTop > 0.0f)
			{
				// Topspin: extra forward bite + a higher, livelier kick.
				xHorizOut += xHorizDir * (fTopspinKick * fSpinTop * fHorizSpeed);
				fVyOut += fTopspinKick * fSpinTop * fHorizSpeed * 0.5f;
			}
			else if (fSpinTop < 0.0f)
			{
				// Slice/backspin: lower bounce, shorter carry (skids low).
				const float fSlice = -fSpinTop;
				fVyOut    *= glm::clamp(1.0f - fSliceSkid * fSlice, 0.0f, 1.0f);
				xHorizOut *= glm::clamp(1.0f - fSliceSkid * fSlice * 0.5f, 0.0f, 1.0f);
			}

			// Sidespin (about the vertical axis) deflects laterally on the bounce.
			const float fSideSpin = Zenith_Maths::Dot(xSpinAngVel, xUp);
			if (std::fabs(fSideSpin) > 1e-5f)
				xHorizOut += xTopAxis * (glm::clamp(fSideSpin * 0.01f, -0.5f, 0.5f) * fHorizSpeed);
		}

		return Zenith_Maths::Vector3(xHorizOut.x, fVyOut, xHorizOut.z);
	}
}
