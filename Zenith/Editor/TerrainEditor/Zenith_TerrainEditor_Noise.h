#pragma once

#ifdef ZENITH_TOOLS

//=============================================================================
// Deterministic integer-hash gradient/value noise shared by the terrain
// editor's Noise brush, procedural generation, and auto-splat jitter.
//
// Determinism is load-bearing: RenderTest regenerates its terrain from a
// fixed seed and CI hash-compares the output across runs — so NO std::mt19937
// here (its stream is implementation-defined across standard libraries), and
// no float-order ambiguity: every value derives from integer hashing.
//=============================================================================

namespace Zenith_TerrainNoise
{
	// Finalizer-style 32-bit integer hash (xxhash/murmur-like avalanche).
	inline u_int HashUInt(u_int uX)
	{
		uX ^= uX >> 16;
		uX *= 0x7feb352du;
		uX ^= uX >> 15;
		uX *= 0x846ca68bu;
		uX ^= uX >> 16;
		return uX;
	}

	inline u_int HashCoords(int iX, int iY, u_int uSeed)
	{
		return HashUInt(static_cast<u_int>(iX) * 0x9E3779B1u
			^ static_cast<u_int>(iY) * 0x85EBCA77u
			^ uSeed * 0xC2B2AE3Du);
	}

	// Uniform [0,1) from a hash.
	inline float HashToFloat01(u_int uHash)
	{
		return static_cast<float>(uHash & 0x00FFFFFFu) * (1.0f / 16777216.0f);
	}

	inline float ValueAt(int iX, int iY, u_int uSeed)
	{
		return HashToFloat01(HashCoords(iX, iY, uSeed));
	}

	// Smoothly-interpolated 2D value noise, [0,1].
	inline float ValueNoise(float fX, float fY, u_int uSeed)
	{
		const int iX0 = static_cast<int>(floorf(fX));
		const int iY0 = static_cast<int>(floorf(fY));
		const float fTX = fX - static_cast<float>(iX0);
		const float fTY = fY - static_cast<float>(iY0);
		const float fSX = fTX * fTX * (3.0f - 2.0f * fTX);
		const float fSY = fTY * fTY * (3.0f - 2.0f * fTY);

		const float fV00 = ValueAt(iX0, iY0, uSeed);
		const float fV10 = ValueAt(iX0 + 1, iY0, uSeed);
		const float fV01 = ValueAt(iX0, iY0 + 1, uSeed);
		const float fV11 = ValueAt(iX0 + 1, iY0 + 1, uSeed);

		const float fTop = fV00 + (fV10 - fV00) * fSX;
		const float fBottom = fV01 + (fV11 - fV01) * fSX;
		return fTop + (fBottom - fTop) * fSY;
	}

	// Fractal Brownian motion over ValueNoise, [0,1] (normalized by total
	// amplitude). uOctaves clamped to [1,12].
	inline float FBM(float fX, float fY, u_int uSeed, u_int uOctaves, float fLacunarity, float fGain)
	{
		uOctaves = uOctaves < 1 ? 1 : (uOctaves > 12 ? 12 : uOctaves);
		float fAmplitude = 1.0f;
		float fFrequency = 1.0f;
		float fSum = 0.0f;
		float fTotal = 0.0f;
		for (u_int u = 0; u < uOctaves; u++)
		{
			fSum += ValueNoise(fX * fFrequency, fY * fFrequency, uSeed + u * 1013u) * fAmplitude;
			fTotal += fAmplitude;
			fAmplitude *= fGain;
			fFrequency *= fLacunarity;
		}
		return fTotal > 0.0f ? fSum / fTotal : 0.0f;
	}

	// Ridged multifractal variant, [0,1] — sharp crests, eroded-looking.
	inline float RidgedFBM(float fX, float fY, u_int uSeed, u_int uOctaves, float fLacunarity, float fGain)
	{
		uOctaves = uOctaves < 1 ? 1 : (uOctaves > 12 ? 12 : uOctaves);
		float fAmplitude = 1.0f;
		float fFrequency = 1.0f;
		float fSum = 0.0f;
		float fTotal = 0.0f;
		for (u_int u = 0; u < uOctaves; u++)
		{
			const float fN = ValueNoise(fX * fFrequency, fY * fFrequency, uSeed + u * 2027u);
			const float fRidge = 1.0f - fabsf(fN * 2.0f - 1.0f);   // tent: peaks at n=0.5
			fSum += fRidge * fRidge * fAmplitude;
			fTotal += fAmplitude;
			fAmplitude *= fGain;
			fFrequency *= fLacunarity;
		}
		return fTotal > 0.0f ? fSum / fTotal : 0.0f;
	}

	// Small deterministic PRNG for droplet simulation (xorshift32). Seeded
	// per-droplet from the hash so iteration order is the only order that
	// matters.
	struct XorShift32
	{
		u_int m_uState;
		explicit XorShift32(u_int uSeed) : m_uState(uSeed == 0 ? 0x12345678u : uSeed) {}
		u_int Next()
		{
			u_int uX = m_uState;
			uX ^= uX << 13;
			uX ^= uX >> 17;
			uX ^= uX << 5;
			m_uState = uX;
			return uX;
		}
		float NextFloat01() { return HashToFloat01(Next()); }
	};
}

#endif // ZENITH_TOOLS
