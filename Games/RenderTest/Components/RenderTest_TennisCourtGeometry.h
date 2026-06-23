#pragma once
#include "Maths/Zenith_Maths.h"
#include "RenderTest/Components/RenderTest_TennisTypes.h"

#include <cmath>

// Pure court geometry for the tennis testbed. Engine-free (Zenith_Maths only).
//
// Coordinate frame (matches RenderTest_Tennis.h): +Z is the long axis (baseline
// to baseline), +X is the court width, +Y up. The net sits at the centre Z. The
// near half is Z < netZ (near player faces +Z); the far half is Z > netZ.
//
// The numeric defaults below are the regulation values; a Phase-0 unit test
// cross-checks DefaultCourt() against the authored RenderTest_Tennis:: constants
// so the two independent sources cannot silently drift apart.

namespace RenderTest_Tennis
{
	// Axis-aligned XZ rectangle (a court region: full half, or a service box).
	struct TennisBox
	{
		float m_fMinX = 0.0f;
		float m_fMaxX = 0.0f;
		float m_fMinZ = 0.0f;
		float m_fMaxZ = 0.0f;

		bool Contains(float fX, float fZ, float fTol) const
		{
			return fX >= m_fMinX - fTol && fX <= m_fMaxX + fTol
			    && fZ >= m_fMinZ - fTol && fZ <= m_fMaxZ + fTol;
		}
	};

	struct TennisCourt
	{
		float m_fCenterX        = 256.0f;
		float m_fNetZ           = 200.0f;   // == court centre Z
		float m_fSurfaceY       = 70.0f;

		float m_fSinglesHalfWidth = 4.115f; // fHALF_WIDTH (5.485) - 1.37
		float m_fDoublesHalfWidth = 5.485f;
		float m_fHalfLength       = 11.885f;

		float m_fSlabHalfWidth    = 7.985f; // includes the 2.5 m apron
		float m_fSlabHalfLength   = 14.385f;

		float m_fServiceLineOffset = 6.40f; // service line distance from the net
		float m_fNetHeight         = 0.91f;
		float m_fBallRadius        = 0.12f;

		// Baseline Z for a side.
		float BaselineZ(TennisSide eSide) const
		{
			return eSide == TENNIS_SIDE_NEAR ? (m_fNetZ - m_fHalfLength)
			                                 : (m_fNetZ + m_fHalfLength);
		}
		float BaselineZ(int iSide) const { return BaselineZ(static_cast<TennisSide>(iSide)); }

		// Which side a Z lies on (the net plane counts as the far side; exact-net
		// landings are vanishingly rare and harmless either way).
		TennisSide SideOfZ(float fZ) const
		{
			return fZ < m_fNetZ ? TENNIS_SIDE_NEAR : TENNIS_SIDE_FAR;
		}

		// The full singles half a side defends (net line to its baseline).
		TennisBox SinglesHalf(TennisSide eSide) const
		{
			TennisBox xBox;
			xBox.m_fMinX = m_fCenterX - m_fSinglesHalfWidth;
			xBox.m_fMaxX = m_fCenterX + m_fSinglesHalfWidth;
			if (eSide == TENNIS_SIDE_NEAR)
			{
				xBox.m_fMinZ = m_fNetZ - m_fHalfLength;
				xBox.m_fMaxZ = m_fNetZ;
			}
			else
			{
				xBox.m_fMinZ = m_fNetZ;
				xBox.m_fMaxZ = m_fNetZ + m_fHalfLength;
			}
			return xBox;
		}

		// The diagonal service box a serve from serverSide must land in. The box
		// is on the RECEIVER's half, between the net and the receiver's service
		// line, on the X-half diagonally opposite the server's stance.
		TennisBox ServiceBox(TennisSide eServerSide, bool bDeuceCourt) const
		{
			const TennisSide eReceiver = OtherSide(eServerSide);
			TennisBox xBox;

			// Z: between the net and the receiver's service line.
			if (eReceiver == TENNIS_SIDE_NEAR)
			{
				xBox.m_fMinZ = m_fNetZ - m_fServiceLineOffset;
				xBox.m_fMaxZ = m_fNetZ;
			}
			else
			{
				xBox.m_fMinZ = m_fNetZ;
				xBox.m_fMaxZ = m_fNetZ + m_fServiceLineOffset;
			}

			// X: the server's deuce court is +X for the near server (faces +Z,
			// right = +X) and -X for the far server. A deuce serve crosses to the
			// opposite X-half; an ad serve stays on the server's deuce X-sign.
			const float fServerDeuceSign = (eServerSide == TENNIS_SIDE_NEAR) ? 1.0f : -1.0f;
			const float fTargetSign = bDeuceCourt ? -fServerDeuceSign : fServerDeuceSign;
			if (fTargetSign > 0.0f)
			{
				xBox.m_fMinX = m_fCenterX;
				xBox.m_fMaxX = m_fCenterX + m_fSinglesHalfWidth;
			}
			else
			{
				xBox.m_fMinX = m_fCenterX - m_fSinglesHalfWidth;
				xBox.m_fMaxX = m_fCenterX;
			}
			return xBox;
		}
	};

	inline TennisCourt DefaultCourt()
	{
		return TennisCourt();   // member initialisers carry the regulation values
	}

	// A landing point is in bounds for the receiving side if it sits within that
	// side's singles half, with a ball-radius tolerance on the lines.
	inline bool IsInBounds(const TennisCourt& xCourt, const Zenith_Maths::Vector3& xLand, TennisSide eReceiverSide)
	{
		return xCourt.SinglesHalf(eReceiverSide).Contains(xLand.x, xLand.z, xCourt.m_fBallRadius);
	}

	// A serve lands in the correct diagonal service box (ball-radius tolerance).
	inline bool IsInServiceBox(const TennisCourt& xCourt, const Zenith_Maths::Vector3& xLand,
		TennisSide eServerSide, bool bDeuceCourt)
	{
		return xCourt.ServiceBox(eServerSide, bDeuceCourt).Contains(xLand.x, xLand.z, xCourt.m_fBallRadius);
	}

	// The prev->cur segment straddles the net plane (Z = netZ).
	inline bool CrossedNetPlane(float fPrevZ, float fCurZ, float fNetZ)
	{
		const bool bPrevFar = fPrevZ >= fNetZ;
		const bool bCurFar  = fCurZ  >= fNetZ;
		return bPrevFar != bCurFar;
	}

	// The ball height at the net is above the tape.
	inline bool ClearsNet(float fHeightAtNet, float fNetHeight)
	{
		return fHeightAtNet > fNetHeight;
	}

	// Clamp an XZ destination to the walkable slab (court + apron), keeping Y. Used to
	// keep nav destinations on the generated navmesh. fMargin erodes the slab inward
	// (the plan's "eroded slab" — pass agent radius + clearance) so a destination never
	// lands on the very edge where SetDestination can fail-stop. The eroded half-extents
	// are floored at 0 so a large margin can't invert the bounds.
	inline Zenith_Maths::Vector3 ProjectToSlab(const TennisCourt& xCourt, const Zenith_Maths::Vector3& xPos, float fMargin = 0.0f)
	{
		const float fHalfW = glm::max(xCourt.m_fSlabHalfWidth  - fMargin, 0.0f);
		const float fHalfL = glm::max(xCourt.m_fSlabHalfLength - fMargin, 0.0f);
		Zenith_Maths::Vector3 xOut = xPos;
		xOut.x = glm::clamp(xPos.x, xCourt.m_fCenterX - fHalfW, xCourt.m_fCenterX + fHalfW);
		xOut.z = glm::clamp(xPos.z, xCourt.m_fNetZ    - fHalfL, xCourt.m_fNetZ    + fHalfL);
		return xOut;
	}

	// Interpolate the prev->cur segment to the net plane and report the crossing
	// height. m_bCrossed is false (height 0) when the segment doesn't straddle
	// the net or is parallel to it.
	struct TennisNetCrossing
	{
		bool  m_bCrossed = false;
		float m_fHeightAtCross = 0.0f;
	};

	inline TennisNetCrossing NetCrossingClearance(const Zenith_Maths::Vector3& xPrev,
		const Zenith_Maths::Vector3& xCur, float fNetZ)
	{
		TennisNetCrossing xOut;
		if (!CrossedNetPlane(xPrev.z, xCur.z, fNetZ))
			return xOut;
		const float fDz = xCur.z - xPrev.z;
		if (std::fabs(fDz) < 1e-6f)
			return xOut;   // parallel to the plane within the straddle test's noise
		const float fT = (fNetZ - xPrev.z) / fDz;
		xOut.m_bCrossed = true;
		xOut.m_fHeightAtCross = xPrev.y + (xCur.y - xPrev.y) * fT;
		return xOut;
	}
}
