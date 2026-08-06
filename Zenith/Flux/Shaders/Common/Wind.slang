// =====================================================================
// Common.Wind — shared Slang module.
//
// The vegetation wind field, split into the two frequencies that are evaluated at
// DIFFERENT rates and must therefore not live in one function:
//
//   * Flux_SampleWindStrength — ONE scalar per blade, evaluated in the placement
//     compute shader and baked into the blade record. A scrolled two-octave value
//     noise sharpened by a pow() so the field spends most of its time near calm and
//     the gusts read as discrete waves crossing the field rather than a uniform
//     shimmer. Cost is per-blade, not per-vertex, and the CPU mirror in
//     Flux/Vegetation/Flux_GrassTypes.h reproduces it for tests and tooling.
//
//   * Flux_SampleWindDetail — per-VERTEX bob, evaluated in the vertex stage. A
//     travelling wave whose phase is offset per blade from the blade's lattice hash,
//     so neighbouring blades never beat in unison (the tell-tale of a single global
//     sine). Height-squared weighted: the base is rooted, the tip carries the motion.
//
// This generalizes the layered-sine CalculateWind that lived inline in
// Flux_Grass.slang. That version summed three fixed sines of dot(worldPos, windDir)
// plus a quadratic gust — cheap, but coherent along the whole wind axis (every blade
// on a line perpendicular to the wind moved identically) and evaluated per vertex.
// The feel it produced (a slow large wave, a faster small one, and a gust envelope
// riding over both) is preserved; the mechanism is not.
//
// PURE MODULE — no resources, no bindings, no ParameterBlock. The caller owns
// where Flux_WindConstants comes from (a pass constant buffer, a push constant),
// which is what lets the CPU mirror consume the identical struct.
// =====================================================================

import Common.Noise;

// Mirrored field-for-field by Flux_WindConstants in Flux_GrassTypes.h. Not yet a
// pinned GPU layout contract — nothing uploads it until the placement CS lands.
public struct Flux_WindConstants
{
    public float2 m_xDirectionXZ;    // unit heading in world XZ
    public float  m_fStrength;       // metres of tip deflection at full gust
    public float  m_fTime;           // seconds; scrolls the gust field
    public float  m_fFrequency;      // gust-field spatial frequency (1/m)
    public float  m_fScrollSpeed;    // m/s the gust field travels along the heading
    public float  m_fGustSharpness;  // pow exponent, >= 1; higher = rarer, harder gusts
    public uint   m_uSeed;           // gust-field hash seed
    public float  m_fDetailFrequency;// per-vertex bob spatial frequency (1/m)
    public float  m_fDetailSpeed;    // per-vertex bob temporal frequency (rad/s)
    public float  m_fDetailAmplitude;// per-vertex bob amplitude, metres at the tip
};

// Per-blade gust strength in [0, m_fStrength]. Evaluated ONCE per blade in the
// placement CS and stored in the blade record's windStrength field.
public float Flux_SampleWindStrength(Flux_WindConstants xWind, float2 xWorldXZ)
{
    // Scrolling the SAMPLE POINT upwind (rather than adding time to the noise seed)
    // is what makes a gust a coherent patch that travels — the field shape is frozen
    // and the world slides through it.
    float2 xScrolled = xWorldXZ - xWind.m_xDirectionXZ * (xWind.m_fScrollSpeed * xWind.m_fTime);
    float2 xP        = xScrolled * xWind.m_fFrequency;

    // Two octaves only: the blade record stores one scalar, so anything finer than
    // the second octave is invisible by the time it reaches a vertex.
    float fOctave0 = Flux_NoiseValueNoise(xP.x, xP.y, xWind.m_uSeed);
    float fOctave1 = Flux_NoiseValueNoise(xP.x * 2.17 + 13.7, xP.y * 2.17 - 7.3, xWind.m_uSeed + 101u);
    float fField   = fOctave0 * 0.65 + fOctave1 * 0.35;

    // Sharpen: pow() on a [0,1] field pulls the mid-range toward calm and leaves the
    // peaks, which is the difference between "gusty" and "everything vibrating".
    float fGust = pow(saturate(fField), max(xWind.m_fGustSharpness, 1.0));
    return xWind.m_fStrength * fGust;
}

// Per-vertex bob, in metres along the blade's bend axis. uBladeHash is the blade
// record's lattice hash (world-stable), so a blade's phase is fixed for as long as
// it exists no matter which tile regenerated it. Height-squared weighted so the
// base stays rooted.
public float Flux_SampleWindDetail(Flux_WindConstants xWind, float2 xWorldXZ, uint uBladeHash, float fHeightFrac)
{
    float fPhase  = Flux_NoiseHashToFloat01(uBladeHash) * 6.28318530718;
    float fTravel = dot(xWorldXZ, xWind.m_xDirectionXZ) * xWind.m_fDetailFrequency
                  - xWind.m_fTime * xWind.m_fDetailSpeed;
    return sin(fTravel + fPhase) * xWind.m_fDetailAmplitude * fHeightFrac * fHeightFrac;
}
