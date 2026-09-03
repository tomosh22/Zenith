#pragma once

#include "Maths/Zenith_Maths.h"

// =====================================================================
// Renderer-neutral scene-environment boundary.
//
// The engine resolves ONE coherent environment per frame: a Sun
// (celestial geometry, Zenith_SunComponent) and an atmosphere (physical
// medium, Zenith_AtmosphereComponent) co-authored on a single environment
// entity. EntityComponent owns the concrete components and publishes the
// resolved snapshot through this POD + function pointer; Flux consumes it
// without a reverse dependency on ECS component types.
//
// The Sun and the atmosphere are deliberately SEPARATE components (the Sun is
// a celestial object, not an atmosphere property) but they are resolved TOGETHER
// from one authoritative environment entity, so a Sun from one loaded scene is
// never combined with an atmosphere from another.
//
// The engine's single radiometric anchor (AtmosphereConfig::fSUN_INTENSITY) is
// POLICY, not a scene look control: it is intentionally absent from this struct
// and from Zenith_AtmosphereComponent. Exposure remains responsible for
// presentation brightness.
// =====================================================================

// The default global sun-travel direction — the fallback used when no loaded
// scene authors a Zenith_SunComponent. ~30 degrees of elevation (was ~46): a
// lower sun gives every surface a lit and a shaded side, long readable shadows
// and warmer transmitted colour, which is the single strongest photographic cue
// an outdoor frame has. A scene that wants noon authors it.
inline Zenith_Maths::Vector3 Zenith_GetDefaultSunDirection()
{
	return Zenith_Maths::Vector3(-0.55f, -0.50f, -0.67f);
}

// Physical atmosphere-model defaults. These mirror the runtime-tunable medium
// parameters the renderer historically initialised from AtmosphereConfig
// (Rayleigh/Mie density scales, the Henyey-Greenstein asymmetry, the two
// exponential scale heights) plus the environment-capture ground albedo that
// used to be the Atmosphere.slang constant IBL_GROUND_ALBEDO. They are
// duplicated here, in the neutral base layer, so the ECS component can
// default-construct without depending on Flux. They MUST track the
// AtmosphereConfig / Atmosphere.slang values; the unit tests pin both ends.
inline float Zenith_GetDefaultAtmosphereRayleighScale()      { return 1.0f; }
inline float Zenith_GetDefaultAtmosphereMieScale()           { return 4.0f; }   // 4x the textbook "very clear" aerosol load: a real horizon is pale and the sun wears an aureole; 1.0 read as a vacuum-clean navy sky
inline float Zenith_GetDefaultAtmosphereMieG()               { return 0.76f; }
// Exponential density scale heights, metres. Earth: molecular ~8 km, aerosol
// ~1.2 km. Lowering the Mie scale height concentrates haze near the ground.
inline float Zenith_GetDefaultAtmosphereRayleighScaleHeight() { return 8000.0f; }
inline float Zenith_GetDefaultAtmosphereMieScaleHeight()      { return 1200.0f; }
// Lambertian albedo of the virtual ground used by ENVIRONMENT CAPTURES only
// (the IBL irradiance/prefilter convolutions). 0.25 is the measured mean
// shortwave albedo of vegetated land. Passes that render real terrain in front
// of the sky pass 0 -- the scene's own ground supplies the bounce there.
inline float Zenith_GetDefaultAtmosphereGroundAlbedo()        { return 0.25f; }

// One resolved environment snapshot. Flux reads exactly this and never the
// ECS component types.
struct Zenith_EnvironmentAuthorityData
{
	// --- Sun (celestial geometry). m_xSunDirection is the direction the light
	// travels INTO the scene. When no environment entity authored a Sun, this is
	// Zenith_GetDefaultSunDirection() and m_bSunAuthored is false.
	bool                  m_bSunAuthored             = false;
	Zenith_Maths::Vector3 m_xSunDirection            = Zenith_GetDefaultSunDirection();
	bool                  m_bSunSourceIsInActiveScene = false;

	// --- Atmosphere (physical medium). When no environment entity authored an
	// atmosphere, the fields are the physical defaults and m_bAtmosphereAuthored
	// is false (identical to the pre-component engine behaviour). These are the
	// values AFTER local blend volumes have been applied (see
	// Zenith_BlendAtmosphereLayer below), so the renderer consumes one resolved
	// medium and knows nothing about volumes.
	bool   m_bAtmosphereAuthored       = false;
	float  m_fRayleighScale             = Zenith_GetDefaultAtmosphereRayleighScale();
	float  m_fMieScale                  = Zenith_GetDefaultAtmosphereMieScale();
	float  m_fMieG                      = Zenith_GetDefaultAtmosphereMieG();
	float  m_fRayleighScaleHeight       = Zenith_GetDefaultAtmosphereRayleighScaleHeight();
	float  m_fMieScaleHeight            = Zenith_GetDefaultAtmosphereMieScaleHeight();
	float  m_fGroundAlbedo              = Zenith_GetDefaultAtmosphereGroundAlbedo();

	// Diagnostics: how many LOCAL blend volumes contributed a non-zero weight to
	// the medium above, and their summed weight (0 = the global base only).
	u_int  m_uBlendVolumesApplied       = 0u;
	float  m_fBlendWeightTotal          = 0.0f;

	// --- Authority. The one environment entity the snapshot was gathered from
	// (the entity carrying the Sun and/or the atmosphere). m_uEnvironmentEntity*
	// are 0xFFFFFFFF when no environment entity exists in any loaded scene.
	u_int m_uEnvironmentEntityIndex      = 0xFFFFFFFFu;
	u_int m_uEnvironmentEntityGeneration = 0xFFFFFFFFu;
	bool  m_bEnvironmentSourceIsInActiveScene = false;

	// Diagnostics: per-component authored counts + the union environment-entity
	// count. A count > 1 is a conflict and emits a throttled warning.
	u_int m_uSunAuthoredCount        = 0u;
	u_int m_uAtmosphereAuthoredCount  = 0u;
	u_int m_uEnvironmentEntityCount   = 0u;
};

using Zenith_EnvironmentAuthorityGatherFn = void (*)(Zenith_EnvironmentAuthorityData& xOut);
extern Zenith_EnvironmentAuthorityGatherFn g_pfnZenithEnvironmentAuthorityGather;

// The snapshot the LAST gather produced. Diagnostics only -- the editor
// property panels read it to tell an author whether the entity they are looking
// at actually won the environment. Reading it never re-runs the gather, so a
// panel can never trip the conflict assert or the warning throttle.
const Zenith_EnvironmentAuthorityData& Zenith_GetLastResolvedEnvironmentAuthority();

// =====================================================================
// Local atmosphere blend volumes (PURE -- no ECS, no engine state).
//
// The environment resolves in two layers, which is Unity's Volume model with
// the parts that do not apply here removed:
//
//   BASE   -- exactly one GLOBAL environment entity, chosen by the existing
//             precedence rule (active scene first, then lowest stable entity
//             ID). More than one global is still a conflict.
//   LOCAL  -- any number of atmosphere entities with m_fBlendRadius > 0. Each
//             is a sphere around its own transform; its weight falls from 1
//             inside (radius - falloff) to 0 at radius, evaluated at the main
//             camera. They are applied in ascending (priority, entity ID) and
//             each LERPS the accumulated medium toward its own values by its
//             weight, so overlapping volumes compose predictably.
//
// The SUN is deliberately NOT blended. It is a celestial object: its direction
// cannot depend on where the camera stands, and making it do so would break the
// shadow fit and the direct/ambient agreement the whole system is built on.
// Only the medium is local -- a dusty basin, a humid valley, a smoggy district.
//
// A scene with no local volumes produces bit-identical results to the
// single-winner rule that predates this, which is why it is safe by default.
// =====================================================================

// The medium half of the resolved environment -- the part a local volume may
// override. Split out so the blend is a pure function over a small POD.
struct Zenith_AtmosphereMedium
{
	float m_fRayleighScale       = Zenith_GetDefaultAtmosphereRayleighScale();
	float m_fMieScale            = Zenith_GetDefaultAtmosphereMieScale();
	float m_fMieG                = Zenith_GetDefaultAtmosphereMieG();
	float m_fRayleighScaleHeight = Zenith_GetDefaultAtmosphereRayleighScaleHeight();
	float m_fMieScaleHeight      = Zenith_GetDefaultAtmosphereMieScaleHeight();
	float m_fGroundAlbedo        = Zenith_GetDefaultAtmosphereGroundAlbedo();
};

// Weight of a spherical blend volume at a given distance from its centre.
// 1 inside the solid core, smoothstepped to 0 across the falloff band, 0
// outside. fFalloff <= 0 gives a hard-edged sphere (a step at fRadius).
inline float Zenith_ComputeBlendVolumeWeight(float fDistance, float fRadius, float fFalloff)
{
	if (fRadius <= 0.0f)      return 0.0f;   // not a volume (global, or degenerate)
	if (fDistance >= fRadius) return 0.0f;
	const float fBand = (fFalloff > 0.0f && fFalloff < fRadius) ? fFalloff : 0.0f;
	if (fBand <= 0.0f)        return 1.0f;
	const float fInner = fRadius - fBand;
	if (fDistance <= fInner)  return 1.0f;
	// smoothstep from the inner edge (1) out to the radius (0).
	const float t = (fDistance - fInner) / fBand;
	const float s = t * t * (3.0f - 2.0f * t);
	return 1.0f - s;
}

// Apply ONE local volume on top of the accumulated medium. Straight lerp by
// weight -- identical semantics to a Unity volume override at that weight, and
// associative enough that adding a zero-weight volume is exactly a no-op.
inline Zenith_AtmosphereMedium Zenith_BlendAtmosphereLayer(
	const Zenith_AtmosphereMedium& xBase, const Zenith_AtmosphereMedium& xLayer, float fWeight)
{
	if (fWeight <= 0.0f) return xBase;
	const float w = (fWeight >= 1.0f) ? 1.0f : fWeight;
	Zenith_AtmosphereMedium x;
	x.m_fRayleighScale       = xBase.m_fRayleighScale       + (xLayer.m_fRayleighScale       - xBase.m_fRayleighScale)       * w;
	x.m_fMieScale            = xBase.m_fMieScale            + (xLayer.m_fMieScale            - xBase.m_fMieScale)            * w;
	x.m_fMieG                = xBase.m_fMieG                + (xLayer.m_fMieG                - xBase.m_fMieG)                * w;
	x.m_fRayleighScaleHeight = xBase.m_fRayleighScaleHeight + (xLayer.m_fRayleighScaleHeight - xBase.m_fRayleighScaleHeight) * w;
	x.m_fMieScaleHeight      = xBase.m_fMieScaleHeight      + (xLayer.m_fMieScaleHeight      - xBase.m_fMieScaleHeight)      * w;
	x.m_fGroundAlbedo        = xBase.m_fGroundAlbedo        + (xLayer.m_fGroundAlbedo        - xBase.m_fGroundAlbedo)        * w;
	return x;
}

// =====================================================================
// Conflict-warning signature (PURE -- no ECS, no engine state).
//
// The gatherer warns once per DISTINCT multi-environment-entity conflict, not
// once per frame, and it decides "distinct" by this signature. The signature
// must therefore cover everything that changes what the warning MEANS:
//   - every candidate's stable packed entity ID,
//   - every candidate's Sun and Atmosphere presence (a LOSING candidate gaining
//     or losing either changes the reported counts and which halves are being
//     ignored -- hashing only the winner's mask silently suppressed that),
//   - every candidate's active-scene membership (the first tiebreak, so it
//     materially decides the winner),
//   - the winner's identity and whether any scene is active at all.
// It must NOT depend on the order the ECS query happened to visit candidates
// in -- the per-candidate hashes are therefore combined COMMUTATIVELY.
//
// Extracted here, in the neutral base layer, so it is unit-testable directly
// without building scenes (mirrors Flux_IBLRegen / Flux_AtmosphereTransmittance).
// =====================================================================

struct Zenith_EnvironmentConflictCandidate
{
	uint64_t m_ulPackedEntityID = 0u;
	bool     m_bHasSun          = false;
	bool     m_bHasAtmosphere   = false;
	bool     m_bInActiveScene   = false;
};

// One FNV-1a round. Named (not a lambda) so the mixing is identical at every
// site and reads the same in the tests that pin the signature's coverage.
inline void Zenith_EnvironmentSignatureMix(uint64_t& ulHash, uint64_t ulValue)
{
	ulHash ^= ulValue;
	ulHash *= 1099511628211ull;
}

inline uint64_t Zenith_ComputeEnvironmentConflictSignature(
	const Zenith_EnvironmentConflictCandidate* paxCandidates,
	u_int uCandidateCount,
	uint64_t ulWinnerPackedEntityID,
	bool bHasActiveScene)
{
	// Per-candidate hashes are folded together with ADDITION -- commutative, so
	// the result cannot depend on query order. (XOR is also commutative but
	// cancels equal entries; candidate IDs are unique today, yet addition costs
	// nothing and cannot degenerate.)
	uint64_t ulCandidateSet = 0u;
	for (u_int u = 0u; u < uCandidateCount; u++)
	{
		const Zenith_EnvironmentConflictCandidate& rC = paxCandidates[u];
		uint64_t ulEntry = 1469598103934665603ull;
		Zenith_EnvironmentSignatureMix(ulEntry, rC.m_ulPackedEntityID);
		Zenith_EnvironmentSignatureMix(ulEntry,
			  (rC.m_bHasSun         ? 1ull : 0ull)
			| (rC.m_bHasAtmosphere  ? 2ull : 0ull)
			| (rC.m_bInActiveScene  ? 4ull : 0ull));
		ulCandidateSet += ulEntry;
	}

	uint64_t ulSignature = 1469598103934665603ull;
	Zenith_EnvironmentSignatureMix(ulSignature, ulCandidateSet);
	Zenith_EnvironmentSignatureMix(ulSignature, static_cast<uint64_t>(uCandidateCount));
	Zenith_EnvironmentSignatureMix(ulSignature, ulWinnerPackedEntityID);
	Zenith_EnvironmentSignatureMix(ulSignature, bHasActiveScene ? 1ull : 0ull);

	// 0 is the gatherer's "no conflict outstanding" sentinel. A real conflict
	// that hashed to 0 would look like "already warned about nothing" and
	// suppress the very next warning, so fold it off the sentinel.
	return ulSignature == 0u ? 1ull : ulSignature;
}

#ifdef ZENITH_TESTING
// Test seam for the conflict-warning throttle: returns the FNV-1a signature of
// the last distinct multi-environment-entity conflict the gatherer saw. The
// gatherer only re-warns when this changes, so a test can prove the throttle
// fires once per distinct conflict set and not on every repeated gather.
// Defined in Zenith_LightComponent.cpp (the gather host TU).
uint64_t Zenith_GetLastEnvironmentConflictSignatureForTest();

// Direct observation of the warn SITE (not a proxy): how many times the
// gatherer has actually called Zenith_Warning for an environment conflict.
// A signature that stays put proves the key is stable; only this proves the
// warning itself did not fire again.
u_int Zenith_GetEnvironmentConflictWarningCountForTest();
void  Zenith_ResetEnvironmentConflictThrottleForTest();
#endif

#ifdef ZENITH_TOOLS
// In a tools build a multi-global-environment conflict is a hard authoring
// error (Zenith_Assert), not just a log line -- it is silent data loss
// otherwise: one entity's atmosphere is dropped on the floor. Tests that
// deliberately construct conflicts scope this off.
//
// RAII rather than a bare setter so an early return or a failing assertion
// inside the scope cannot leave the engine permanently non-fatal.
class Zenith_ScopedEnvironmentConflictAssertSuppression
{
public:
	Zenith_ScopedEnvironmentConflictAssertSuppression();
	~Zenith_ScopedEnvironmentConflictAssertSuppression();
	Zenith_ScopedEnvironmentConflictAssertSuppression(const Zenith_ScopedEnvironmentConflictAssertSuppression&) = delete;
	Zenith_ScopedEnvironmentConflictAssertSuppression& operator=(const Zenith_ScopedEnvironmentConflictAssertSuppression&) = delete;
private:
	bool m_bPrevious;
};
#endif