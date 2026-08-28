#pragma once

//=============================================================================
// Deterministic integer-hash gradient/value noise. Runtime-shared maths: the
// terrain editor's Noise brush, procedural generation and auto-splat jitter
// consume it, and so does any runtime caller that must reproduce the same
// field for the same seed.
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

	//-------------------------------------------------------------------------
	// 3D value noise. Same integer-hash determinism contract as the 2D pair
	// above — it exists because a field sampled on a DIRECTION (a rock's
	// surface displacement, a volumetric mask) has no 2D parameterisation that
	// is free of a seam or a pole. Projecting a sphere into 2D to reuse
	// ValueNoise puts a visible pinch at both poles and a discontinuity down
	// the wrap meridian; sampling the 3D field at the unit direction has
	// neither, which is the whole reason it is here rather than a local helper
	// in one generator.
	//-------------------------------------------------------------------------
	inline u_int HashCoords3(int iX, int iY, int iZ, u_int uSeed)
	{
		return HashUInt(static_cast<u_int>(iX) * 0x9E3779B1u
			^ static_cast<u_int>(iY) * 0x85EBCA77u
			^ static_cast<u_int>(iZ) * 0xC2B2AE35u
			^ uSeed * 0x27D4EB2Fu);
	}

	inline float ValueAt3(int iX, int iY, int iZ, u_int uSeed)
	{
		return HashToFloat01(HashCoords3(iX, iY, iZ, uSeed));
	}

	// Smoothly-interpolated 3D value noise, [0,1].
	inline float ValueNoise3D(float fX, float fY, float fZ, u_int uSeed)
	{
		const int iX0 = static_cast<int>(floorf(fX));
		const int iY0 = static_cast<int>(floorf(fY));
		const int iZ0 = static_cast<int>(floorf(fZ));
		const float fTX = fX - static_cast<float>(iX0);
		const float fTY = fY - static_cast<float>(iY0);
		const float fTZ = fZ - static_cast<float>(iZ0);
		const float fSX = fTX * fTX * (3.0f - 2.0f * fTX);
		const float fSY = fTY * fTY * (3.0f - 2.0f * fTY);
		const float fSZ = fTZ * fTZ * (3.0f - 2.0f * fTZ);

		const float fV000 = ValueAt3(iX0, iY0, iZ0, uSeed);
		const float fV100 = ValueAt3(iX0 + 1, iY0, iZ0, uSeed);
		const float fV010 = ValueAt3(iX0, iY0 + 1, iZ0, uSeed);
		const float fV110 = ValueAt3(iX0 + 1, iY0 + 1, iZ0, uSeed);
		const float fV001 = ValueAt3(iX0, iY0, iZ0 + 1, uSeed);
		const float fV101 = ValueAt3(iX0 + 1, iY0, iZ0 + 1, uSeed);
		const float fV011 = ValueAt3(iX0, iY0 + 1, iZ0 + 1, uSeed);
		const float fV111 = ValueAt3(iX0 + 1, iY0 + 1, iZ0 + 1, uSeed);

		const float fX00 = fV000 + (fV100 - fV000) * fSX;
		const float fX10 = fV010 + (fV110 - fV010) * fSX;
		const float fX01 = fV001 + (fV101 - fV001) * fSX;
		const float fX11 = fV011 + (fV111 - fV011) * fSX;

		const float fY0 = fX00 + (fX10 - fX00) * fSY;
		const float fY1 = fX01 + (fX11 - fX01) * fSY;
		return fY0 + (fY1 - fY0) * fSZ;
	}

	// Fractal Brownian motion over ValueNoise3D, [0,1] (normalized by total
	// amplitude). uOctaves clamped to [1,12], matching FBM.
	inline float FBM3D(float fX, float fY, float fZ, u_int uSeed, u_int uOctaves,
		float fLacunarity, float fGain)
	{
		uOctaves = uOctaves < 1 ? 1 : (uOctaves > 12 ? 12 : uOctaves);
		float fAmplitude = 1.0f;
		float fFrequency = 1.0f;
		float fSum = 0.0f;
		float fTotal = 0.0f;
		for (u_int u = 0; u < uOctaves; u++)
		{
			fSum += ValueNoise3D(fX * fFrequency, fY * fFrequency, fZ * fFrequency,
				uSeed + u * 1013u) * fAmplitude;
			fTotal += fAmplitude;
			fAmplitude *= fGain;
			fFrequency *= fLacunarity;
		}
		return fTotal > 0.0f ? fSum / fTotal : 0.0f;
	}

	//-------------------------------------------------------------------------
	// TILEABLE value noise — the lattice is WRAPPED, so the field repeats exactly
	// over [0,1)^2. ValueNoise above cannot do this: it samples an unbounded
	// lattice, so a texture generated from it shows a hard seam wherever it meets
	// its own edge. Anything sampled at world-scale UVs (a rock's box projection,
	// a log's cylindrical wrap) meets that edge constantly.
	//
	// The periods are per-axis because bark is ANISOTROPIC: ridges run along the
	// trunk, which is a high period across V and a low one along it. Passing the
	// same period twice gives the isotropic field a rock wants.
	//-------------------------------------------------------------------------
	inline int WrapLattice(int iCoord, int iPeriod)
	{
		return ((iCoord % iPeriod) + iPeriod) % iPeriod;
	}

	inline float TileableValueNoise(float fU, float fV, int iPeriodU, int iPeriodV, u_int uSeed)
	{
		const float fX = fU * static_cast<float>(iPeriodU);
		const float fY = fV * static_cast<float>(iPeriodV);
		const int iX0 = static_cast<int>(floorf(fX));
		const int iY0 = static_cast<int>(floorf(fY));
		const float fTX = fX - static_cast<float>(iX0);
		const float fTY = fY - static_cast<float>(iY0);
		const float fSX = fTX * fTX * (3.0f - 2.0f * fTX);
		const float fSY = fTY * fTY * (3.0f - 2.0f * fTY);

		const int iXA = WrapLattice(iX0, iPeriodU);
		const int iXB = WrapLattice(iX0 + 1, iPeriodU);
		const int iYA = WrapLattice(iY0, iPeriodV);
		const int iYB = WrapLattice(iY0 + 1, iPeriodV);

		const float fV00 = ValueAt(iXA, iYA, uSeed);
		const float fV10 = ValueAt(iXB, iYA, uSeed);
		const float fV01 = ValueAt(iXA, iYB, uSeed);
		const float fV11 = ValueAt(iXB, iYB, uSeed);

		const float fTop = fV00 + (fV10 - fV00) * fSX;
		const float fBottom = fV01 + (fV11 - fV01) * fSX;
		return fTop + (fBottom - fTop) * fSY;
	}

	// Isotropic convenience form.
	inline float TileableValueNoise(float fU, float fV, int iPeriod, u_int uSeed)
	{
		return TileableValueNoise(fU, fV, iPeriod, iPeriod, uSeed);
	}

	inline float TileableFBM(float fU, float fV, int iBasePeriodU, int iBasePeriodV,
		u_int uOctaves, u_int uSeed)
	{
		float fSum = 0.0f;
		float fAmp = 1.0f;
		float fTotal = 0.0f;
		int iPeriodU = iBasePeriodU;
		int iPeriodV = iBasePeriodV;
		for (u_int u = 0; u < uOctaves; u++)
		{
			fSum += TileableValueNoise(fU, fV, iPeriodU, iPeriodV, uSeed + u * 1013u) * fAmp;
			fTotal += fAmp;
			fAmp *= 0.5f;
			iPeriodU *= 2;
			iPeriodV *= 2;
		}
		return fTotal > 0.0f ? fSum / fTotal : 0.0f;
	}

	inline float TileableFBM(float fU, float fV, int iBasePeriod, u_int uOctaves, u_int uSeed)
	{
		return TileableFBM(fU, fV, iBasePeriod, iBasePeriod, uOctaves, uSeed);
	}

	// Ridged variant — sharp crests, which is what a crack network (or a bark
	// furrow) is once it is thresholded.
	inline float TileableRidged(float fU, float fV, int iBasePeriodU, int iBasePeriodV,
		u_int uOctaves, u_int uSeed)
	{
		float fSum = 0.0f;
		float fAmp = 1.0f;
		float fTotal = 0.0f;
		int iPeriodU = iBasePeriodU;
		int iPeriodV = iBasePeriodV;
		for (u_int u = 0; u < uOctaves; u++)
		{
			const float fN = TileableValueNoise(fU, fV, iPeriodU, iPeriodV, uSeed + u * 2027u);
			const float fRidge = 1.0f - fabsf(fN * 2.0f - 1.0f);
			fSum += fRidge * fRidge * fAmp;
			fTotal += fAmp;
			fAmp *= 0.5f;
			iPeriodU *= 2;
			iPeriodV *= 2;
		}
		return fTotal > 0.0f ? fSum / fTotal : 0.0f;
	}

	inline float TileableRidged(float fU, float fV, int iBasePeriod, u_int uOctaves, u_int uSeed)
	{
		return TileableRidged(fU, fV, iBasePeriod, iBasePeriod, uOctaves, uSeed);
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
