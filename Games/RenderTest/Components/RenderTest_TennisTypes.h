#pragma once
#include "Maths/Zenith_Maths.h"

// Pure value types for the autonomous tennis testbed (decision + spin cores).
//
// This header is DELIBERATELY engine-free: it depends only on Zenith_Maths
// (a GLM wrapper). No g_xEngine, no ECS, no physics — so every type here is
// headless-unit-testable, and the merge gate ("0 engine symbols in the logic
// headers") holds. All free functions are `inline` because these headers are
// included from multiple translation units.
//
// Enum convention matches the engine (prefixed SCREAMING_SNAKE_CASE values,
// like TEXTURE_FORMAT_*). Side indices are pinned to 0 (near) / 1 (far) so an
// int index and a TennisSide are interchangeable across the seam.

namespace RenderTest_Tennis
{
	// ---- Sides -----------------------------------------------------------
	enum TennisSide
	{
		TENNIS_SIDE_NEAR = 0,   // the -Z half (near baseline), faces +Z
		TENNIS_SIDE_FAR  = 1,   // the +Z half (far baseline), faces -Z
	};

	inline TennisSide OtherSide(TennisSide eSide)
	{
		return eSide == TENNIS_SIDE_NEAR ? TENNIS_SIDE_FAR : TENNIS_SIDE_NEAR;
	}

	// Int-index overload for the referee seam (it carries server/hitter as int).
	inline int OtherSideIndex(int iSide)
	{
		return 1 - iSide;
	}

	// ---- Shot taxonomy ---------------------------------------------------
	enum TennisShotType
	{
		TENNIS_SHOT_TYPE_FLAT = 0,
		TENNIS_SHOT_TYPE_TOPSPIN,
		TENNIS_SHOT_TYPE_SLICE,
		TENNIS_SHOT_TYPE_DROP,
		TENNIS_SHOT_TYPE_LOB,
	};

	enum ServeResult
	{
		SERVE_RESULT_GOOD = 0,
		SERVE_RESULT_FAULT,
		SERVE_RESULT_DOUBLE_FAULT,
	};

	// Referee match-state phases + serve attempt. These live in the neutral types
	// header (rather than the referee header) so the BT leaves can read the phase
	// the referee publishes into each NPC's blackboard WITHOUT including the
	// referee header — that one-directional flow (referee -> blackboard -> leaves)
	// keeps the brain/BT/referee headers acyclic.
	enum PointPhase
	{
		POINT_PHASE_WARMUP = 0,
		POINT_PHASE_SERVING,
		POINT_PHASE_LIVE,
		POINT_PHASE_POINT_OVER,
		POINT_PHASE_MATCH_OVER,
		POINT_PHASE_SHOWCASE,
	};

	enum ServeAttempt
	{
		SERVE_ATTEMPT_FIRST = 0,
		SERVE_ATTEMPT_SECOND,
	};

	// Outcome of a bounce, from the HITTER's perspective (the side that last
	// struck the ball). CONTINUE => rally is still live.
	enum PointOutcome
	{
		POINT_OUTCOME_CONTINUE = 0,
		POINT_OUTCOME_HITTER_WINS,
		POINT_OUTCOME_HITTER_LOSES,
	};

	// What the referee does with an eligible striker's anim contact: LAUNCH the
	// ball, award the OPPONENT (a genuine out-of-range miss), or DISCARD it (a
	// stale/unarmed decision — keep the ball live).
	enum ContactOutcome
	{
		CONTACT_OUTCOME_LAUNCH = 0,
		CONTACT_OUTCOME_OPPONENT_POINT,
		CONTACT_OUTCOME_DISCARD,
	};

	// Why a settling ball ended the point. A NEUTRAL cause (no telemetry dependency
	// in the pure headers) — the referee maps it to a telemetry PointReason. Lets
	// the SettleCheck winner/reason side-math be headless-unit-tested (it is exactly
	// the "wrong side scores" class), instead of only windowed.
	enum TennisSettleCause
	{
		TENNIS_SETTLE_SERVE_UNRETURNED = 0,    // <=1 rally shot: opponent never got the serve back
		TENNIS_SETTLE_DOUBLE_BOUNCE,           // a returnable ball the opponent failed to reach
		TENNIS_SETTLE_LANDED_OUT,              // overshot the slab without bouncing in (hitter's error)
		TENNIS_SETTLE_INTO_NET_OR_OWN_SIDE,    // the hitter's own shot died on their side
	};

	// Winner side (0/1) + cause for a SettleCheck resolution.
	struct TennisSettleResolution
	{
		int               m_iWinnerSide = 0;
		TennisSettleCause m_eCause      = TENNIS_SETTLE_SERVE_UNRETURNED;
	};

	// ---- PODs ------------------------------------------------------------

	// A fully-decided shot, parked on the brain by the BT and consumed by the
	// referee at contact. m_uEpoch stamps the ball-instance this decision was
	// armed for; the referee rejects a decision whose epoch no longer matches
	// (stale-ball guard). m_bArmed is set only once the body confirms the
	// stroke actually started (RequestSwing/RequestServe returned true).
	struct TennisShotDecision
	{
		Zenith_Maths::Vector3 m_xAim       = Zenith_Maths::Vector3(0.0f);  // world-space landing target
		Zenith_Maths::Vector3 m_xSpinAngVel = Zenith_Maths::Vector3(0.0f); // rad/s, applied at launch
		TennisShotType        m_eType      = TENNIS_SHOT_TYPE_FLAT;
		float                 m_fPace      = 0.0f;                          // launch pace scalar
		float                 m_fRisk      = 0.0f;                          // 0..1, drives jitter magnitude
		bool                  m_bArmed     = false;
		u_int                 m_uEpoch     = 0u;
	};

	// Result of forward-integrating the ball to a player's strike plane.
	struct TennisInterceptResult
	{
		bool                  m_bReachable   = false;
		Zenith_Maths::Vector3 m_xStrikePoint = Zenith_Maths::Vector3(0.0f);
		float                 m_fTimeToStrike = 0.0f;
	};

	// A snapshot of a player's situation, fed to the decision core. Pure data
	// (the brain fills it from live engine state each frame).
	struct TennisPlayerState
	{
		Zenith_Maths::Vector3 m_xMyPos  = Zenith_Maths::Vector3(0.0f);
		Zenith_Maths::Vector3 m_xOppPos = Zenith_Maths::Vector3(0.0f);
		TennisSide            m_eMySide = TENNIS_SIDE_NEAR;
		float                 m_fBalance = 1.0f;   // 0 (stretched) .. 1 (set at ready)
		int                   m_iMyPoints = 0;
		int                   m_iOppPoints = 0;
		bool                  m_bIsServer = false;
	};

	// Gravity used by every ballistic stepper in the tennis cores (m/s^2, -Y).
	inline constexpr float k_fGravity = 9.81f;
}
