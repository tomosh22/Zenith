#pragma once
#include "Maths/Zenith_Maths.h"
#include "RenderTest/Components/RenderTest_TennisTypes.h"
#include "RenderTest/Components/RenderTest_TennisCourtGeometry.h"
#include "RenderTest/Components/RenderTest_TennisRng.h"
#include "RenderTest/Components/RenderTest_TennisSpin.h"

#include <cmath>

// Pure decision core for the tennis testbed. Engine-free (Zenith_Maths only):
// shot selection, serve selection, ballistic interception + launch solving,
// jitter, balance, the rules classifiers, and the footwork/contact seams. Every
// function is deterministic (RNG passed explicitly) and headless-unit-testable.
//
// The interception predictor and the launch solver share ONE integrator
// (IntegrateStep) so a shot the brain decides is reachable is launched with a
// velocity that actually lands where it aimed.

namespace RenderTest_Tennis
{
	// ---- Tuning (decision-layer; the referee owns its own sim constants) -----
	inline constexpr float k_fDecisionMagnusK = 0.012f;   // matches the referee's Magnus k
	inline constexpr float k_fLineInset       = 0.6f;     // keep aim this far inside the lines
	inline constexpr float k_fStrikeAboveSurf = 1.0f;     // contact height above the surface
	// Shot pace band. DEVIATES from the plan's k_fPaceMin/Max=9.0/16.0: capped much
	// gentler so the post-bounce ball stays slow enough for the opponent to run down
	// before its second bounce — at higher paces the receiver can't reach the serve /
	// groundstroke in time and points die in one shot (no rallies). Tuned for emergent
	// multi-shot rallies, not realistic ball speed.
	inline constexpr float k_fDecPaceMin      = 7.0f;
	inline constexpr float k_fDecPaceMax      = 9.5f;
	inline constexpr float k_fAimJitterBase   = 1.2f;     // metres of aim scatter at full risk+difficulty
	inline constexpr float k_fPaceJitterBase  = 0.18f;    // fractional pace scatter at full risk+difficulty
	inline constexpr float k_fBaseAggression  = 0.35f;

	inline float StrikeHeight(const TennisCourt& xCourt) { return xCourt.m_fSurfaceY + k_fStrikeAboveSurf; }

	// ---- Shared ballistic integrator -----------------------------------------
	// Advance one step under gravity (-Y) + Magnus. Spin is held constant over
	// the short prediction/solve horizon (its decay over <2.5 s is negligible and
	// keeping it fixed makes predictor and solver agree).
	inline void IntegrateStep(Zenith_Maths::Vector3& xPos, Zenith_Maths::Vector3& xVel,
		const Zenith_Maths::Vector3& xSpin, float fDt, float fMagnusK)
	{
		xVel += MagnusDeltaV(xVel, xSpin, fMagnusK, fDt);
		xVel.y -= k_fGravity * fDt;
		xPos += xVel * fDt;
	}

	// Closed-form arc velocity ignoring Magnus (the solver's seed).
	inline Zenith_Maths::Vector3 BallisticArcVelocity(const Zenith_Maths::Vector3& xFrom,
		const Zenith_Maths::Vector3& xAim, float fT)
	{
		const Zenith_Maths::Vector3 d = xAim - xFrom;
		return Zenith_Maths::Vector3(d.x / fT, d.y / fT + 0.5f * k_fGravity * fT, d.z / fT);
	}

	inline float HorizDist(const Zenith_Maths::Vector3& xA, const Zenith_Maths::Vector3& xB)
	{
		const float fDx = xA.x - xB.x;
		const float fDz = xA.z - xB.z;
		return std::sqrt(fDx * fDx + fDz * fDz);
	}

	// ---- Interception ---------------------------------------------------------
	// Forward-integrate the live ball until it next descends through the strike
	// height on MY side of the net; report the strike point + time. Reachable iff
	// I can cover the horizontal gap in that time (run speed * t + reach).
	inline TennisInterceptResult PredictIntercept(const TennisCourt& xCourt,
		const Zenith_Maths::Vector3& xBallPos, const Zenith_Maths::Vector3& xBallVel,
		const Zenith_Maths::Vector3& xBallSpin, TennisSide eMySide, float fStrikeHeight,
		float fReachRadius, const Zenith_Maths::Vector3& xMyPos, float fRunSpeed)
	{
		TennisInterceptResult xOut;
		const float fDt = 1.0f / 120.0f;
		Zenith_Maths::Vector3 xPos = xBallPos;
		Zenith_Maths::Vector3 xVel = xBallVel;
		float fT = 0.0f;
		for (int i = 0; i < 300; ++i)   // ~2.5 s horizon
		{
			const Zenith_Maths::Vector3 xPrev = xPos;
			const float fPrevT = fT;
			IntegrateStep(xPos, xVel, xBallSpin, fDt, k_fDecisionMagnusK);
			fT += fDt;

			const bool bDescending = xVel.y < 0.0f;
			const bool bOnMySide = xCourt.SideOfZ(xPos.z) == eMySide;
			if (bDescending && bOnMySide)
			{
				// High ball: crosses DOWN through the strike height -> hit at exactly
				// the strike height (interpolated).
				if (xPrev.y >= fStrikeHeight && xPos.y < fStrikeHeight)
				{
					const float fDy = xPos.y - xPrev.y;
					const float fFrac = (std::fabs(fDy) > 1e-6f) ? (fStrikeHeight - xPrev.y) / fDy : 0.0f;
					const Zenith_Maths::Vector3 xStrike = xPrev + (xPos - xPrev) * fFrac;
					const float fStrikeT = fPrevT + (fT - fPrevT) * fFrac;
					xOut.m_xStrikePoint = xStrike;
					xOut.m_fTimeToStrike = fStrikeT;
					xOut.m_bReachable = HorizDist(xMyPos, xStrike) <= fRunSpeed * fStrikeT + fReachRadius;
					return xOut;
				}
				// Low flat drive: already in the strikeable band (above the surface,
				// at/below the strike height) and descending -> hittable here. Without
				// this, a flat drive that never rises above the strike height was wrongly
				// judged unreachable.
				const float fFloor = xCourt.m_fSurfaceY + xCourt.m_fBallRadius;
				if (xPos.y <= fStrikeHeight && xPos.y >= fFloor)
				{
					xOut.m_xStrikePoint = xPos;
					xOut.m_fTimeToStrike = fT;
					xOut.m_bReachable = HorizDist(xMyPos, xPos) <= fRunSpeed * fT + fReachRadius;
					return xOut;
				}
			}
		}
		return xOut;   // never reached the strike plane on my side
	}

	// ---- Launch solving -------------------------------------------------------
	// Velocity (incl. spin-aware correction) to land the ball at xAim from xFrom.
	// Seeds a closed-form arc, then 3 Magnus-aware correction passes converge the
	// integrated landing onto xAim. Reports the flight time used.
	inline Zenith_Maths::Vector3 ComputeLaunchVelocity(const Zenith_Maths::Vector3& xFrom,
		const Zenith_Maths::Vector3& xAim, const Zenith_Maths::Vector3& xSpinAngVel,
		float fPace, float& fFlightTOut)
	{
		const float fHoriz = HorizDist(xFrom, xAim);
		const float fT = glm::clamp(fHoriz / glm::max(fPace, 0.5f), 0.4f, 2.5f);
		fFlightTOut = fT;

		Zenith_Maths::Vector3 xV0 = BallisticArcVelocity(xFrom, xAim, fT);
		const float fDt = 1.0f / 120.0f;
		for (int iPass = 0; iPass < 3; ++iPass)
		{
			Zenith_Maths::Vector3 xPos = xFrom;
			Zenith_Maths::Vector3 xVel = xV0;
			float fElapsed = 0.0f;
			while (fElapsed < fT)
			{
				IntegrateStep(xPos, xVel, xSpinAngVel, fDt, k_fDecisionMagnusK);
				fElapsed += fDt;
			}
			const Zenith_Maths::Vector3 xErr = xAim - xPos;
			xV0 += xErr / fT;   // first-order velocity correction (map v0->landing ~ linear)
		}
		return xV0;
	}

	// ---- Sub-scores -----------------------------------------------------------
	// In-bounds margin: distance from the nearest singles line, normalised. 0 if
	// outside (illegal/very risky), ->1 deep inside the box.
	inline float InBoundsMargin(const TennisCourt& xCourt, const Zenith_Maths::Vector3& xAim, TennisSide eTargetSide)
	{
		const TennisBox xBox = xCourt.SinglesHalf(eTargetSide);
		const float fMx = glm::min(xAim.x - xBox.m_fMinX, xBox.m_fMaxX - xAim.x);
		const float fMz = glm::min(xAim.z - xBox.m_fMinZ, xBox.m_fMaxZ - xAim.z);
		return glm::clamp(glm::min(fMx, fMz) / glm::max(xCourt.m_fSinglesHalfWidth, 0.01f), 0.0f, 1.0f);
	}

	// How far inside their own baseline the player is standing (0 at/behind the
	// baseline, ->1 up at the net): enables aggression.
	inline float CourtPositionFactor(const TennisCourt& xCourt, const TennisPlayerState& xState)
	{
		const float fDistFromNet = std::fabs(xState.m_xMyPos.z - xCourt.m_fNetZ);
		return glm::clamp(1.0f - fDistFromNet / glm::max(xCourt.m_fHalfLength, 0.01f), 0.0f, 1.0f);
	}

	// Aggression rises with a points lead and with court position, falls with low
	// balance. 0..1.
	inline float ComputeAggression(const TennisPlayerState& xState, float fCourtPosFactor)
	{
		float fLead = 0.0f;
		if (xState.m_iMyPoints > xState.m_iOppPoints) fLead = 0.15f;
		else if (xState.m_iMyPoints < xState.m_iOppPoints) fLead = -0.10f;
		const float fAgg = k_fBaseAggression + 0.30f * fCourtPosFactor + fLead + 0.20f * xState.m_fBalance;
		return glm::clamp(fAgg, 0.0f, 1.0f);
	}

	// ---- Shot / serve selection ----------------------------------------------
	// Situational, realistic shot selection (a tactical decision tree, not an
	// open-court argmax). Reads my balance + court position and the opponent's position,
	// and picks the shot a real player would: a deep topspin staple to work the point,
	// a defensive slice when stretched, a lob over a net-rusher, a drop against a deep
	// opponent, and a flat winner to the open court when in control + the opponent is
	// pulled wide. The neutral rally shot is placed MODERATELY to the open side — enough
	// to move the opponent but still reachable, so rallies sustain instead of ending on
	// the first untouchable corner. Risk (-> per-hit jitter) scales with how aggressive
	// the shot is, so winners occasionally miss (unforced errors) while rally balls land.
	inline TennisShotDecision SelectShot(const TennisCourt& xCourt, const TennisPlayerState& xState, TennisRng& xRng)
	{
		const TennisSide eOpp = OtherSide(xState.m_eMySide);
		const float fOppSign = (eOpp == TENNIS_SIDE_FAR) ? 1.0f : -1.0f;   // net -> opponent court direction
		const float fCx      = xCourt.m_fCenterX;
		const float fSurf    = xCourt.m_fSurfaceY;
		const float fEdge    = xCourt.m_fSinglesHalfWidth - k_fLineInset;
		// "Deep" lands at ~78% of the way to the baseline, NOT on the line: the floating
		// slab has little run-back room, so a ball landing on the baseline bounces straight
		// off the slab edge before the receiver can run it down (ending the rally at one
		// shot). Landing it short of the baseline keeps the ball in play + reachable.
		const float fDeepZ   = xCourt.m_fNetZ + fOppSign * (xCourt.m_fHalfLength * 0.78f);
		const float fMidZ    = xCourt.m_fNetZ + fOppSign * (xCourt.m_fHalfLength * 0.55f);
		const float fShortZ  = xCourt.m_fNetZ + fOppSign * (xCourt.m_fServiceLineOffset * 0.75f);

		const float fOppDepthFromNet = std::fabs(xState.m_xOppPos.z - xCourt.m_fNetZ);
		const float fOppWide = xState.m_xOppPos.x - fCx;   // opponent's lateral offset (signed)
		const float fBalance = xState.m_fBalance;
		const float fCourtPos = CourtPositionFactor(xCourt, xState);
		const float fAggr = ComputeAggression(xState, fCourtPos);
		const float fJitX = xRng.NextSigned() * 0.6f;      // deterministic placement noise

		TennisShotType eType = TENNIS_SHOT_TYPE_TOPSPIN;
		Zenith_Maths::Vector3 xAim(fCx, fSurf, fDeepZ);
		float fPace = k_fDecPaceMax * 0.9f;
		float fRisk = 0.30f;

		if (fOppDepthFromNet < xCourt.m_fServiceLineOffset)
		{
			// Opponent crept inside the service line -> LOB deep over them.
			eType = TENNIS_SHOT_TYPE_LOB;
			xAim  = Zenith_Maths::Vector3(fCx + fJitX, fSurf, fDeepZ);
			fPace = k_fDecPaceMin * 1.15f;
			fRisk = 0.35f;
		}
		else if (fBalance < 0.40f)
		{
			// Stretched / scrambling -> defensive SLICE deep + central to reset.
			eType = TENNIS_SHOT_TYPE_SLICE;
			xAim  = Zenith_Maths::Vector3(fCx + fJitX, fSurf, fDeepZ);
			fPace = k_fDecPaceMin;
			fRisk = 0.20f;
		}
		else if (fBalance > 0.70f && std::fabs(fOppWide) > fEdge * 0.45f)
		{
			// In control + opponent pulled wide -> FLAT winner into the OPEN court.
			const float fOpenX = fCx - (fOppWide > 0.0f ? fEdge : -fEdge);
			eType = TENNIS_SHOT_TYPE_FLAT;
			xAim  = Zenith_Maths::Vector3(fOpenX, fSurf, fMidZ);
			fPace = k_fDecPaceMax;
			fRisk = 0.60f;
		}
		else if (fBalance > 0.60f && fOppDepthFromNet > xCourt.m_fHalfLength * 0.90f && xRng.NextUnit() < 0.30f)
		{
			// Opponent parked deep behind the baseline + I'm set -> occasional DROP shot.
			eType = TENNIS_SHOT_TYPE_DROP;
			xAim  = Zenith_Maths::Vector3(fCx + fJitX, fSurf, fShortZ);
			fPace = k_fDecPaceMin;
			fRisk = 0.45f;
		}
		else
		{
			// Neutral rally -> TOPSPIN deep, placed MODERATELY to the open side (mirror the
			// opponent's position): moves them but stays reachable so the rally continues.
			const float fPlaceX = glm::clamp(fCx - fOppWide, fCx - fEdge * 0.6f, fCx + fEdge * 0.6f);
			eType = TENNIS_SHOT_TYPE_TOPSPIN;
			xAim  = Zenith_Maths::Vector3(fPlaceX + fJitX, fSurf, fDeepZ);
			fPace = k_fDecPaceMax * 0.9f;
			fRisk = 0.25f + 0.15f * fAggr;
		}

		const float fMargin = InBoundsMargin(xCourt, xAim, eOpp);
		const Zenith_Maths::Vector3 xFrom(xState.m_xMyPos.x, StrikeHeight(xCourt), xState.m_xMyPos.z);
		const Zenith_Maths::Vector3 xDir = Zenith_Maths::Normalize(
			Zenith_Maths::Vector3(xAim.x - xFrom.x, 0.0f, xAim.z - xFrom.z));

		TennisShotDecision xOut;
		xOut.m_xAim = xAim;
		xOut.m_eType = eType;
		xOut.m_fPace = fPace;
		xOut.m_fRisk = glm::clamp(fRisk + 0.30f * (1.0f - fMargin), 0.0f, 1.0f);
		xOut.m_xSpinAngVel = SpinAngVelForShot(eType, xDir, fPace);
		xOut.m_bArmed = false;
		xOut.m_uEpoch = 0u;
		return xOut;
	}

	// Deuce court (server's right) when the point total is even; ad court (left)
	// when odd — covers deuce/advantage by parity.
	inline bool ServeCourtIsDeuce(int iServerPoints, int iReceiverPoints)
	{
		return ((iServerPoints + iReceiverPoints) % 2) == 0;
	}

	inline TennisShotDecision SelectServe(const TennisCourt& xCourt, const TennisPlayerState& xState,
		ServeResult /*eUnusedAttemptTag*/, bool bServeFromDeuceCourt, bool bIsSecondServe, TennisRng& xRng)
	{
		// Aim into the correct diagonal service box, DEEP (toward the service line) so the
		// post-bounce ball carries to the deep receiver (a short serve dies before they
		// reach it). Real serve PLACEMENT varies — wide (pull the receiver off court), down
		// the T (centre line), or at the body. The first serve goes for a corner (wide/T,
		// deep + risky -> it occasionally faults long/wide via the contact jitter); the
		// second serve is safe (body, shallower, slower topspin) so double faults stay rare
		// — realistic first-serve-in around two thirds.
		const TennisBox xBox = xCourt.ServiceBox(xState.m_eMySide, bServeFromDeuceCourt);
		const float fCx = xCourt.m_fCenterX;
		const float fCenterZ = (xBox.m_fMinZ + xBox.m_fMaxZ) * 0.5f;
		const float fDeepEdgeZ = (std::fabs(xBox.m_fMinZ - xCourt.m_fNetZ) > std::fabs(xBox.m_fMaxZ - xCourt.m_fNetZ))
			? xBox.m_fMinZ : xBox.m_fMaxZ;
		const float fInnerX = (std::fabs(xBox.m_fMinX - fCx) < std::fabs(xBox.m_fMaxX - fCx)) ? xBox.m_fMinX : xBox.m_fMaxX;
		const float fOuterX = (fInnerX == xBox.m_fMinX) ? xBox.m_fMaxX : xBox.m_fMinX;

		// Placement (fraction inner->outer): first serve goes T or wide; second to the body.
		float fPlaceT;
		float fDepth;
		if (bIsSecondServe)
		{
			fPlaceT = 0.50f;   // body / centre of the box
			fDepth  = 0.55f;   // safely inside the service line
		}
		else
		{
			// Go for the corners. The launch solver lands the serve mid-box in DEPTH
			// regardless of aim (it undershoots a deep target), so long faults never happen
			// — the realistic fault comes from going WIDE: aim close to the sideline so the
			// risk-scaled contact jitter occasionally pushes the serve past it for a wide
			// fault (~a fifth of first serves), giving a realistic first-serve-in rate.
			fPlaceT = (xRng.NextUnit() < 0.5f) ? 0.95f : 0.05f;   // out wide or tight down the T
			fDepth  = 0.82f;   // deep enough to carry to the receiver, comfortably inside the line
		}
		Zenith_Maths::Vector3 xAim(
			fInnerX + fPlaceT * (fOuterX - fInnerX),
			xCourt.m_fSurfaceY,
			fCenterZ + fDepth * (fDeepEdgeZ - fCenterZ));

		// Tiny deterministic placement scatter — kept small enough that the AIM always stays
		// inside the box (the first serve aims close to the line, so a larger scatter here
		// would fault on intent). The real fault-causing scatter is the risk-scaled jitter
		// applied at contact by the referee, which pushes the aggressive first serve long/wide.
		const Zenith_Maths::Vector2 xJit = xRng.NextInDisc(0.10f);
		xAim.x += xJit.x;
		xAim.z += xJit.y;

		const Zenith_Maths::Vector3 xFrom(xState.m_xMyPos.x, StrikeHeight(xCourt) + 1.4f, xState.m_xMyPos.z);
		const Zenith_Maths::Vector3 xDir = Zenith_Maths::Normalize(
			Zenith_Maths::Vector3(xAim.x - xFrom.x, 0.0f, xAim.z - xFrom.z));

		TennisShotDecision xOut;
		xOut.m_xAim = xAim;
		// First serve flat + risky (wide faults occasionally); second a safe topspin kick.
		// Pace stays gentle so the post-bounce serve is returnable (a faster serve doesn't
		// land any deeper — the solver caps depth — it only arrives too fast to return).
		xOut.m_eType = bIsSecondServe ? TENNIS_SHOT_TYPE_TOPSPIN : TENNIS_SHOT_TYPE_FLAT;
		xOut.m_fPace = bIsSecondServe ? (k_fDecPaceMax * 0.60f) : (k_fDecPaceMax * 0.80f);
		// High first-serve risk widens the contact jitter so the corner-seeking serve faults
		// wide often enough for a realistic first-serve-in rate; the second serve's low risk
		// keeps it safe so double faults stay rare.
		xOut.m_fRisk = bIsSecondServe ? 0.15f : 0.70f;
		xOut.m_xSpinAngVel = SpinAngVelForShot(xOut.m_eType, xDir, xOut.m_fPace);
		xOut.m_bArmed = false;
		xOut.m_uEpoch = 0u;
		return xOut;
	}

	// ---- Jitter ---------------------------------------------------------------
	// Aim scatter; magnitude rises monotonically with risk AND difficulty. For a
	// fixed RNG state the offset scales linearly with the disc radius, so the
	// scatter is deterministic and monotonic.
	inline Zenith_Maths::Vector3 JitterAim(const Zenith_Maths::Vector3& xAim, float fRisk, float fDifficulty, TennisRng& xRng)
	{
		const float fAmt = k_fAimJitterBase * glm::clamp(0.5f * fRisk + 0.5f * fDifficulty, 0.0f, 1.0f);
		const Zenith_Maths::Vector2 xOff = xRng.NextInDisc(fAmt);
		return Zenith_Maths::Vector3(xAim.x + xOff.x, xAim.y, xAim.z + xOff.y);
	}

	inline float JitterPace(float fPace, float fRisk, float fDifficulty, TennisRng& xRng)
	{
		const float fScale = k_fPaceJitterBase * glm::clamp(0.5f * fRisk + 0.5f * fDifficulty, 0.0f, 1.0f);
		return fPace * (1.0f + xRng.NextSigned() * fScale);
	}

	// ---- Balance / footwork / contact -----------------------------------------
	inline float ComputeBalance(const Zenith_Maths::Vector3& xMyPos, const Zenith_Maths::Vector3& xReadyPos, float fHalfWidth)
	{
		const float fDist = HorizDist(xMyPos, xReadyPos);
		return glm::clamp(1.0f - fDist / glm::max(fHalfWidth, 0.01f), 0.0f, 1.0f);
	}

	inline bool IsWithinContactRange(const Zenith_Maths::Vector3& xBallPos,
		const Zenith_Maths::Vector3& xSweetSpot, float fRadius)
	{
		return Zenith_Maths::LengthSq(xBallPos - xSweetSpot) <= fRadius * fRadius;
	}

	// The referee's contact decision, factored out of HandleEligibleContact so the
	// launch / opponent-point / discard branching unit-tests WITHOUT a ball physics
	// body or a posed skeleton (neither of which the headless fixture can supply).
	//   bInRange      = the ball is within racket reach of the posed sweet spot
	//   bHasArmedShot = the striker's brain holds an epoch-valid armed decision
	// Out-of-range is a genuine miss -> the opponent scores (no "magical" launch
	// regardless of racket position). In-range but unarmed is a stale/mistimed swing
	// -> discard it and keep the ball live. In-range AND armed is a real strike.
	inline ContactOutcome ClassifyContactOutcome(bool bInRange, bool bHasArmedShot)
	{
		if (!bInRange)
			return CONTACT_OUTCOME_OPPONENT_POINT;
		if (!bHasArmedShot)
			return CONTACT_OUTCOME_DISCARD;
		return CONTACT_OUTCOME_LAUNCH;
	}

	// Racket-head world position from the POSED hand-bone world matrix. The racket
	// is mounted at the RightHand bone with a 180deg-about-X twist so its head
	// extends along the hand bone's local -Y; the sweet spot sits k_fRacketReach up
	// that shaft. Pure (a Matrix4 in, a point out) so it unit-tests against
	// synthetic hand poses without booting a skeleton — the engine-bound
	// GetRacketSweetSpotPos() just resolves the hand matrix and calls this.
	inline Zenith_Maths::Vector3 ComputeRacketSweetSpot(const Zenith_Maths::Matrix4& xHandWorld, float fRacketReach)
	{
		const Zenith_Maths::Vector3 xHandPos(xHandWorld[3]);
		Zenith_Maths::Vector3 xShaft(xHandWorld * Zenith_Maths::Vector4(0.0f, -1.0f, 0.0f, 0.0f));
		const float fLen = Zenith_Maths::Length(xShaft);
		if (fLen > 1e-5f)
			xShaft /= fLen;
		return xHandPos + xShaft * fRacketReach;
	}

	// Proportional X-slide toward the footwork target, capped at run speed.
	inline float ComputeFootworkVelocityX(float fCurrentX, float fTargetX, float fRunSpeed)
	{
		return glm::clamp((fTargetX - fCurrentX) * 4.0f, -fRunSpeed, fRunSpeed);
	}

	// The body drives its own footwork only when the nav agent isn't.
	inline bool ShouldDriveFootwork(bool bExternalMovementDriven)
	{
		return !bExternalMovementDriven;
	}

	// Where a player recovers to between shots (the RecoverToReady fallback target Z). A
	// receiver AWAITING a serve stands UP at the service line — the gently-paced serve
	// bounces short in the box and dies before a baseline receiver can run it down, so
	// from the baseline the serve goes unreturned and no rally starts; standing in puts
	// the receiver where the serve arrives. Everyone else (the server, and both players
	// during a LIVE rally) holds the baseline for deep groundstrokes.
	inline float ComputeReadyZ(const TennisCourt& xCourt, TennisSide eMySide, PointPhase ePhase, bool bIsServer)
	{
		if (ePhase == POINT_PHASE_SERVING && !bIsServer)
		{
			const float fSign = (xCourt.BaselineZ(eMySide) - xCourt.m_fNetZ) >= 0.0f ? 1.0f : -1.0f;
			return xCourt.m_fNetZ + fSign * xCourt.m_fServiceLineOffset;   // ~the service line
		}
		return xCourt.BaselineZ(eMySide);
	}

	// How far the PLAYER moved toward its destination this frame: the player's actual
	// horizontal displacement projected onto the direction (from the previous position)
	// to the goal. Crucially this uses PLAYER displacement, NOT change-in-distance-to-goal
	// — the BT rewrites the destination every tick, so a goal sliding toward a STATIONARY
	// (blocked) agent would otherwise fake positive "distance reduction". A stationary
	// player scores 0 here no matter how the goal moves; sideways/backward motion scores
	// <=0. Result is in metres/frame along the goal direction.
	inline float ProgressTowardDestination(const Zenith_Maths::Vector3& xPrevPos,
		const Zenith_Maths::Vector3& xCurPos, const Zenith_Maths::Vector3& xDest)
	{
		const float fToDestX = xDest.x - xPrevPos.x;
		const float fToDestZ = xDest.z - xPrevPos.z;
		const float fLen = HorizDist(xDest, xPrevPos);
		if (fLen < 1e-4f)
			return 0.0f;
		const float fDispX = xCurPos.x - xPrevPos.x;
		const float fDispZ = xCurPos.z - xPrevPos.z;
		return (fDispX * fToDestX + fDispZ * fToDestZ) / fLen;
	}

	// A nav agent is stalled if it hasn't reached its destination yet AND made no
	// meaningful progress TOWARD that destination this frame (fProgressTowardDest, from
	// ProgressTowardDestination — player displacement projected at the goal, immune to a
	// moving destination). Deliberately independent of HasPath(): an agent can hold a
	// VALID path yet never approach its goal (its dynamic body wedged against geometry/
	// another body), which a path-presence, a raw-motion, OR a distance-reduction check
	// all miss. Sustained non-progress (k frames) hands the agent to footwork.
	inline bool IsNavStalled(bool bReachedDestination, float fProgressTowardDest, float fEps)
	{
		return !bReachedDestination && fProgressTowardDest < fEps;
	}

	// ---- Rules classifiers ----------------------------------------------------
	inline ServeResult ClassifyServe(const TennisCourt& xCourt, const Zenith_Maths::Vector3& xLand,
		TennisSide eServerSide, bool bServeFromDeuceCourt, bool bIsSecondServe, bool bCrossedNetLegally)
	{
		const bool bGood = bCrossedNetLegally && IsInServiceBox(xCourt, xLand, eServerSide, bServeFromDeuceCourt);
		if (bGood)
			return SERVE_RESULT_GOOD;
		return bIsSecondServe ? SERVE_RESULT_DOUBLE_FAULT : SERVE_RESULT_FAULT;
	}

	inline PointOutcome ClassifyBounceOutcome(const TennisCourt& xCourt, const Zenith_Maths::Vector3& xLand,
		TennisSide eLandSide, TennisSide eHitterSide, int iBounceIdxSinceHit, bool bCrossedNetLegally)
	{
		if (iBounceIdxSinceHit >= 2)
			return POINT_OUTCOME_HITTER_WINS;   // a legal rally ball reached a 2nd bounce
		// First bounce:
		if (!bCrossedNetLegally || eLandSide == eHitterSide)
			return POINT_OUTCOME_HITTER_LOSES;  // netted / landed on own side
		if (!IsInBounds(xCourt, xLand, OtherSide(eHitterSide)))
			return POINT_OUTCOME_HITTER_LOSES;  // out (vs singles lines)
		return POINT_OUTCOME_CONTINUE;
	}

	// Phase-aware stall resolution: a ball that never crossed legally (still on
	// the hitter's side / netted) loses for the hitter; one that crossed and then
	// stalled on the receiver's side is the hitter's point. Returns the winning
	// side index (0/1).
	inline int ResolveStallWinner(int iLastHitter, bool bCrossedNetLegally)
	{
		return bCrossedNetLegally ? iLastHitter : OtherSideIndex(iLastHitter);
	}

	// Pure winner+cause for SettleCheck's two cases (extracted so the side-math — the
	// "wrong side scores" class — is headless-unit-tested, not just windowed).

	// Ball left the floating slab. If it had bounced IN first, the OPPONENT failed to
	// return a good ball (the hitter wins; iBallSide is where the ball exited, so the
	// hitter is OtherSideIndex of it); a clean overshoot that never bounced is the
	// hitter's own error (opponent wins).
	inline TennisSettleResolution ResolveOffSlabSettle(int iBounceCount, int iBallSide, int iLastHitter, int iRallyShots)
	{
		TennisSettleResolution xR;
		if (iBounceCount >= 1)
		{
			xR.m_iWinnerSide = OtherSideIndex(iBallSide);
			xR.m_eCause = (iRallyShots <= 1) ? TENNIS_SETTLE_SERVE_UNRETURNED : TENNIS_SETTLE_DOUBLE_BOUNCE;
		}
		else
		{
			xR.m_iWinnerSide = OtherSideIndex(iLastHitter);
			xR.m_eCause = TENNIS_SETTLE_LANDED_OUT;
		}
		return xR;
	}

	// Ball died low on the slab after at least one bounce: whoever's side it settled on
	// loses — either their own netted/short shot (into-net/own-side), or they failed to
	// return the opponent's ball.
	inline TennisSettleResolution ResolveDeadLowSettle(int iSettleSide, int iLastHitter, int iRallyShots)
	{
		TennisSettleResolution xR;
		xR.m_iWinnerSide = OtherSideIndex(iSettleSide);
		if (iSettleSide == iLastHitter)
			xR.m_eCause = TENNIS_SETTLE_INTO_NET_OR_OWN_SIDE;
		else
			xR.m_eCause = (iRallyShots <= 1) ? TENNIS_SETTLE_SERVE_UNRETURNED : TENNIS_SETTLE_DOUBLE_BOUNCE;
		return xR;
	}

	// Which side may legally strike the ball right now (-1 = nobody): the server in
	// SERVING but ONLY before the serve is struck (iLastHitter<0 — once struck the
	// in-flight serve must not be re-hit), or the expected receiver in LIVE.
	inline int ComputeEligibleStriker(PointPhase ePhase, int iLastHitter, int iServer, int iExpectedReceiver)
	{
		if (ePhase == POINT_PHASE_SERVING)
			return (iLastHitter < 0) ? iServer : -1;
		if (ePhase == POINT_PHASE_LIVE)
			return iExpectedReceiver;
		return -1;
	}
}
