// =====================================================================
// Common.Noise — shared Slang module.
//
// GPU mirror of the deterministic integer-hash value noise in
// Zenith/Maths/Zenith_Noise.h (namespace Zenith_TerrainNoise). The CPU header is
// the SOURCE OF TRUTH; this file is a structural transcription of it, function for
// function, so a reader can diff the two side by side.
//
// WHAT AGREEMENT IS CLAIMED, AND WHAT IS NOT. The three integer entry points —
// HashUInt / HashCoords / HashToFloat01 — are bit-exact against the CPU:
// they are pure u32 shift/xor/multiply (wrapping identically in both languages)
// plus one exactly-representable scale (a 24-bit integer times 2^-24 rounds in
// neither language). So any DECISION keyed off the integer hash — which lattice
// node exists, which grass type a node picks, which fade class it lands in — is
// reproducible on the CPU for tests and tooling.
//
// The float noise built on top (ValueNoise / FBM) is NOT claimed bit-exact:
// floor / fused-multiply-add contraction / precise-vs-fast float modes all differ
// per target. Never gate a CPU/GPU consistency check on those values; hash the
// integers instead.
//
// PURE MODULE — no resources, no bindings, no ParameterBlock. Callers pass
// every constant (seed, frequency, octave count) as an argument, so this file is
// `import`-able from any stage without perturbing the descriptor-set spine.
// =====================================================================

// Finalizer-style 32-bit integer hash (xxhash/murmur-like avalanche).
// Constants are byte-identical to Zenith_TerrainNoise::HashUInt.
public uint Flux_NoiseHashUInt(uint uX)
{
    uX ^= uX >> 16;
    uX *= 0x7feb352du;
    uX ^= uX >> 15;
    uX *= 0x846ca68bu;
    uX ^= uX >> 16;
    return uX;
}

// Lattice-node hash. The int->uint casts are a bit reinterpretation on both
// sides (two's complement), so negative lattice coordinates agree with the CPU.
public uint Flux_NoiseHashCoords(int iX, int iY, uint uSeed)
{
    return Flux_NoiseHashUInt(((uint)iX * 0x9E3779B1u)
        ^ ((uint)iY * 0x85EBCA77u)
        ^ (uSeed * 0xC2B2AE3Du));
}

// Uniform [0,1) from a hash. Exact: the masked value is < 2^24 (representable in
// a float without rounding) and the scale is a power of two.
public float Flux_NoiseHashToFloat01(uint uHash)
{
    return (float)(uHash & 0x00FFFFFFu) * (1.0 / 16777216.0);
}

public float Flux_NoiseValueAt(int iX, int iY, uint uSeed)
{
    return Flux_NoiseHashToFloat01(Flux_NoiseHashCoords(iX, iY, uSeed));
}

// Smoothly-interpolated 2D value noise, [0,1]. Smoothstep weights, bilinear blend.
public float Flux_NoiseValueNoise(float fX, float fY, uint uSeed)
{
    int   iX0 = (int)floor(fX);
    int   iY0 = (int)floor(fY);
    float fTX = fX - (float)iX0;
    float fTY = fY - (float)iY0;
    float fSX = fTX * fTX * (3.0 - 2.0 * fTX);
    float fSY = fTY * fTY * (3.0 - 2.0 * fTY);

    float fV00 = Flux_NoiseValueAt(iX0,     iY0,     uSeed);
    float fV10 = Flux_NoiseValueAt(iX0 + 1, iY0,     uSeed);
    float fV01 = Flux_NoiseValueAt(iX0,     iY0 + 1, uSeed);
    float fV11 = Flux_NoiseValueAt(iX0 + 1, iY0 + 1, uSeed);

    float fTop    = fV00 + (fV10 - fV00) * fSX;
    float fBottom = fV01 + (fV11 - fV01) * fSX;
    return fTop + (fBottom - fTop) * fSY;
}

// Fractal Brownian motion over ValueNoise, [0,1] (normalized by total amplitude).
// uOctaves is clamped to [1,12] exactly as the CPU does — an unclamped count would
// unroll differently per target and is a pointless divergence risk.
public float Flux_NoiseFBM(float fX, float fY, uint uSeed, uint uOctaves, float fLacunarity, float fGain)
{
    uOctaves = clamp(uOctaves, 1u, 12u);
    float fAmplitude = 1.0;
    float fFrequency = 1.0;
    float fSum       = 0.0;
    float fTotal     = 0.0;
    for (uint u = 0u; u < uOctaves; u++)
    {
        fSum   += Flux_NoiseValueNoise(fX * fFrequency, fY * fFrequency, uSeed + u * 1013u) * fAmplitude;
        fTotal += fAmplitude;
        fAmplitude *= fGain;
        fFrequency *= fLacunarity;
    }
    return fTotal > 0.0 ? fSum / fTotal : 0.0;
}
