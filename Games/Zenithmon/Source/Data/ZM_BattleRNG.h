#pragma once

// ============================================================================
// ZM_BattleRNG -- the seeded, deterministic battle RNG (S1 data core). A PCG32
// generator (O'Neill's permuted congruential generator): 64-bit state, 32-bit
// output, statistically strong and exactly reproducible from a seed. Spec:
// Docs/GameDesignDocument.md section 7 (ZM_BattleRNG, PCG32); DecisionLog ZM-D-027.
//
// The battle engine (S2) is "headless, synchronous, and seeded" -- every random
// decision (accuracy, crit, damage roll, secondary-effect procs, AI tie-breaks,
// catch shakes) draws from an instance of this, so a battle replays bit-for-bit
// from its seed. This is the ONLY sanctioned randomness source in game logic;
// never call rand()/std::random in rule code.
// ============================================================================

class ZM_BattleRNG
{
public:
	// Default construction is deterministic (a fixed seed) so an unseeded RNG is
	// never a hidden source of nondeterminism.
	ZM_BattleRNG() { Seed(0x853C49E6748FEA9Bull, 0xDA3E39CB94B95BDBull); }
	explicit ZM_BattleRNG(u_int64 ulSeed, u_int64 ulSeq = 54ull) { Seed(ulSeed, ulSeq); }

	// (Re)seed the stream. ulSeq selects one of 2^63 distinct sequences.
	void Seed(u_int64 ulSeed, u_int64 ulSeq = 54ull)
	{
		m_ulState = 0ull;
		m_ulInc = (ulSeq << 1u) | 1u;
		Next();
		m_ulState += ulSeed;
		Next();
	}

	// The core step: advance the state and return the next 32-bit output.
	u_int32 Next()
	{
		const u_int64 ulOld = m_ulState;
		m_ulState = ulOld * ulPCG32_MULTIPLIER + m_ulInc;
		const u_int32 uXorShifted = (u_int32)(((ulOld >> 18u) ^ ulOld) >> 27u);
		const u_int32 uRot = (u_int32)(ulOld >> 59u);
		return (uXorShifted >> uRot) | (uXorShifted << ((0u - uRot) & 31u));
	}

	// Unbiased integer in [0, uBound) via rejection (removes modulo bias).
	u_int RandBelow(u_int uBound)
	{
		Zenith_Assert(uBound > 0u, "ZM_BattleRNG::RandBelow: bound must be > 0");
		const u_int32 uThreshold = (u_int32)(0u - uBound) % uBound;   // == 2^32 mod uBound
		for (;;)
		{
			const u_int32 uX = Next();
			if (uX >= uThreshold)
			{
				return (u_int)(uX % uBound);
			}
		}
	}

	// Inclusive integer in [uMin, uMax].
	u_int RandRange(u_int uMin, u_int uMax)
	{
		Zenith_Assert(uMax >= uMin, "ZM_BattleRNG::RandRange: max < min");
		return uMin + RandBelow(uMax - uMin + 1u);
	}

	// True with probability uNumerator / uDenominator. Chance(0, d) is never true;
	// Chance(d, d) is always true.
	bool Chance(u_int uNumerator, u_int uDenominator)
	{
		Zenith_Assert(uDenominator > 0u, "ZM_BattleRNG::Chance: denominator must be > 0");
		return RandBelow(uDenominator) < uNumerator;
	}

	// True with probability uPercent / 100 (uPercent >= 100 is always true).
	bool ChancePercent(u_int uPercent)
	{
		return Chance(uPercent, 100u);
	}

private:
	static constexpr u_int64 ulPCG32_MULTIPLIER = 6364136223846793005ull;

	u_int64	m_ulState = 0ull;
	u_int64	m_ulInc = 1ull;
};
