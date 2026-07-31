#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "Core/Zenith_EnvironmentAuthority.h"

#ifdef ZENITH_TOOLS
// Shared authority banner for the Sun + Atmosphere property panels: tells the
// author, in the panel they are editing, whether THIS entity actually won the
// environment. A losing entity's data is silently dropped by the resolver, so
// without this the only signal is a log line. Reads the LAST resolved snapshot
// (never re-gathers), so drawing it cannot trip the conflict assert.
void Zenith_RenderEnvironmentAuthorityBanner(const Zenith_Entity& xEntity, bool bIsLocalVolume);
#endif

// A scene atmosphere owns the physical medium that the renderer's sky, direct
// sun key and IBL convolution all derive from. It carries ONLY world-describing
// model inputs that are genuinely scene-authored: the Rayleigh + Mie density
// scales and the Henyey-Greenstein asymmetry parameter. It deliberately has NO
// field for the engine's single radiometric anchor
// (AtmosphereConfig::fSUN_INTENSITY) -- that is policy, not a look knob -- and
// no exposure, sample-count, LUT, debug-mode or quality-override field (those
// are renderer state and stay in Flux).
//
// Intended authoring shape: co-locate this with Zenith_SunComponent on one
// environment entity. The two are separate components because the Sun is a
// celestial object, not an atmosphere property; the environment authority
// resolves them together from that one entity.
//
// A SECOND authoring shape exists for regional variation: set m_fBlendRadius > 0
// and the component becomes a LOCAL volume that blends its medium over the
// global one wherever the camera is inside it (see the blend-volume accessors
// below). Local volumes never compete for authority and never carry a Sun.
class Zenith_AtmosphereComponent
{
public:
	explicit Zenith_AtmosphereComponent(Zenith_Entity& xEntity);
	~Zenith_AtmosphereComponent() = default;

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// Density multiplier on the Rayleigh (molecular) scattering coefficients.
	// >= 0; 1.0 is the physical default. A change invalidates the transmittance
	// LUT, the sky-view LUT, the direct sun key and the IBL convolution.
	float GetRayleighScale() const { return m_fRayleighScale; }
	void  SetRayleighScale(float fScale);

	// Density multiplier on the Mie (aerosol) scatter coefficient. >= 0; 1.0 is
	// the physical default. A change invalidates the transmittance LUT, the
	// sky-view LUT, the direct sun key and the IBL convolution.
	float GetMieScale() const { return m_fMieScale; }
	void  SetMieScale(float fScale);

	// Henyey-Greenstein aerosol phase-function asymmetry in [0, 0.99]. 0.76 is
	// the physical default (strong forward scatter). A change invalidates the
	// sky-view LUT + the IBL convolution; it does NOT affect transmittance.
	float GetMieG() const { return m_fMieG; }
	void  SetMieG(float fG);

	// Exponential density scale height of the molecular (Rayleigh) layer, in
	// metres. Earth is ~8000. Raising it thickens the upper atmosphere and
	// deepens the zenith blue. Affects transmittance, so a change invalidates
	// the transmittance LUT as well as the sky and the IBL capture.
	float GetRayleighScaleHeight() const { return m_fRayleighScaleHeight; }
	void  SetRayleighScaleHeight(float fMetres);

	// Exponential density scale height of the aerosol (Mie) layer, in metres.
	// Earth is ~1200. LOWERING it presses the haze down against the ground --
	// the usual knob for a humid valley or a dust bowl. Also a transmittance
	// invalidator.
	float GetMieScaleHeight() const { return m_fMieScaleHeight; }
	void  SetMieScaleHeight(float fMetres);

	// Lambertian albedo of the virtual ground the ENVIRONMENT CAPTURE integrates
	// (the IBL irradiance/prefilter cubes) in [0, 1]. 0.25 = vegetated land;
	// raise toward 0.7 for snow or desert, drop toward 0.05 for fresh basalt or
	// open ocean. It fills the cube's lower hemisphere, so it is what puts
	// bounce light on vertical surfaces. It does NOT affect the visible sky
	// (that pass passes 0 -- real terrain supplies the ground there) and does
	// NOT affect transmittance.
	float GetGroundAlbedo() const { return m_fGroundAlbedo; }
	void  SetGroundAlbedo(float fAlbedo);

	// --- Local blend volume (see Zenith_BlendAtmosphereLayer) -------------
	// 0 (the default) = a GLOBAL atmosphere: this entity is a candidate for the
	// one authoritative environment, and two globals are a conflict.
	// > 0 = a LOCAL volume centred on this entity's transform: it does not
	// compete for authority, it LERPS the resolved medium toward its own values
	// by its weight wherever the camera is inside it. The Sun is never blended.
	float GetBlendRadius() const { return m_fBlendRadius; }
	void  SetBlendRadius(float fRadius);

	// Width of the fade band just inside the radius, in world units. 0 = a hard
	// edge (a visible pop as the camera crosses). Clamped to < radius.
	float GetBlendFalloff() const { return m_fBlendFalloff; }
	void  SetBlendFalloff(float fFalloff);

	// Application order for overlapping local volumes: LOWER priority is applied
	// first, so a HIGHER priority volume wins where they overlap. Ties break on
	// the stable entity ID, so the result never depends on ECS query order.
	float GetBlendPriority() const { return m_fBlendPriority; }
	void  SetBlendPriority(float fPriority) { m_fBlendPriority = fPriority; }

	bool IsLocalBlendVolume() const { return m_fBlendRadius > 0.0f; }

	// The medium half, packed for the pure blend helpers.
	Zenith_AtmosphereMedium GetMedium() const;
	void SetMedium(const Zenith_AtmosphereMedium& xMedium);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel();
#endif

private:
	Zenith_Entity m_xParentEntity;
	float m_fRayleighScale       = Zenith_GetDefaultAtmosphereRayleighScale();
	float m_fMieScale            = Zenith_GetDefaultAtmosphereMieScale();
	float m_fMieG                = Zenith_GetDefaultAtmosphereMieG();
	float m_fRayleighScaleHeight = Zenith_GetDefaultAtmosphereRayleighScaleHeight();
	float m_fMieScaleHeight      = Zenith_GetDefaultAtmosphereMieScaleHeight();
	float m_fGroundAlbedo        = Zenith_GetDefaultAtmosphereGroundAlbedo();
	float m_fBlendRadius         = 0.0f;   // 0 = global (the back-compatible default)
	float m_fBlendFalloff        = 0.0f;
	float m_fBlendPriority       = 0.0f;
};