#pragma once
#include "Maths/Zenith_Maths.h"
#include "RenderTest/Components/RenderTest_TennisTypes.h"

#include <cmath>
#include <cstdint>

// Deterministic LCG for the tennis cores. Engine-free. Every stochastic choice
// (shot tie-breaks, aim/pace jitter) flows through an explicit TennisRng so the
// whole match replays identically from a seed — no Math::Random.
//
// Same constants as the existing match LCG (Numerical Recipes), so behaviour is
// familiar; the value type is a plain struct so it copies/serialises trivially.

namespace RenderTest_Tennis
{
	struct TennisRng
	{
		uint32_t m_uState = 0x1234567u;

		explicit TennisRng(uint32_t uSeed = 0x1234567u) : m_uState(uSeed) {}

		uint32_t Next()
		{
			m_uState = m_uState * 1664525u + 1013904223u;
			return m_uState;
		}

		// Uniform in [0, 1) from the top 24 bits (matches the legacy Rng01()).
		float NextUnit()
		{
			return static_cast<float>((Next() >> 8) & 0xFFFFFFu) / 16777216.0f;
		}

		// Uniform in [-1, 1).
		float NextSigned()
		{
			return NextUnit() * 2.0f - 1.0f;
		}

		// Uniform point in a disc of the given radius (area-uniform, sqrt-warped).
		Zenith_Maths::Vector2 NextInDisc(float fRadius)
		{
			const float fR = fRadius * std::sqrt(NextUnit());
			const float fTheta = NextUnit() * static_cast<float>(Zenith_Maths::Pi * 2.0);
			return Zenith_Maths::Vector2(fR * std::cos(fTheta), fR * std::sin(fTheta));
		}
	};
}
