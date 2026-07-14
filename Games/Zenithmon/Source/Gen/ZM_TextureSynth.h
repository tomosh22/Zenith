#pragma once

// ============================================================================
// ZM_TextureSynth -- S4 procedural texture synthesis: creature albedo (BC1,
// 512^2) + shiny (hue-rotated) + dex-icon, plus reusable palette / pattern /
// decal / normal-from-height primitives shaped from the outset for the (later,
// out-of-scope) ZM_HumanGen and ZM_BuildingGen. Determinism: texels are a pure
// function of a species/family-derived seed -- no global RNG, no clock
// (AssetManifest 6.2).
//
// GUARD MODEL: the synthesis library is compiled in ALL configs (headless, no
// GPU, no disk) so the ZM_Gen unit gate can byte-compare texels; only the
// .ztxtr bake bridges at the end are #ifdef ZENITH_TOOLS, with non-tools no-ops
// so _False builds link. Conventions copied verbatim from the StickFigure
// texture path: sRGB albedo (no BC sRGB format exists), BC5 normal (R,G only;
// the shader rebuilds Z), glTF G=roughness/B=metallic packing.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"   // ZM_GenRNG (+ ZM_GenNoise for mottle)
#include "Zenithmon/Source/Data/ZM_Types.h"        // ZM_TYPE (18 elemental types)
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

// ZM_BakeManifest (a later box) stamps this; bump when synthesis changes.
// v2 (SC5d): creature-albedo palette-saturation boost (punchier type colours).
constexpr u_int uZM_TEXTURESYNTH_VERSION       = 2u;
constexpr u_int uZM_CREATURE_ALBEDO_RESOLUTION = 512u;   // BC1 512x512 (AssetManifest 1.2)

// ---------------------------------------------------------------------------
// ZM_GenImage -- interleaved RGBA float CPU image (engine Zenith_Image is
// single-channel, unusable for albedo). Row-major: texel (uY,uX) at
// (uY*w + uX)*4. Channels in [0,1]. Pure and headless; its Zenith_Vector moves,
// so synth functions return it by value. A height field for the normal-from-
// height primitive is passed as a source image's R channel (no dedicated
// channel), keeping this buffer a plain RGBA image.
// ---------------------------------------------------------------------------
class ZM_GenImage
{
public:
	ZM_GenImage() = default;
	ZM_GenImage(u_int uWidth, u_int uHeight);      // zero-filled RGB, alpha 1

	u_int GetWidth()  const { return m_uWidth; }
	u_int GetHeight() const { return m_uHeight; }
	bool  IsEmpty()   const { return m_uWidth == 0u || m_uHeight == 0u; }

	Zenith_Maths::Vector4 Get(u_int uY, u_int uX) const;
	void                  Set(u_int uY, u_int uX, const Zenith_Maths::Vector4& xRGBA);

	// Pack to a tightly-packed RGBA8 buffer (4 bytes/texel) for the exporter.
	// bSRGBEncode applies the sRGB OETF to R/G/B (albedo path). Deterministic
	// quantisation: (u_int8)(clamp01(c) * 255.0f + 0.5f).
	void  PackRGBA8(Zenith_Vector<u_int8>& xOut, bool bSRGBEncode) const;

	// Byte-exact content compare + FNV-1a hash over the packed (non-sRGB) texels
	// -- the machinery for the determinism / shiny-differs tests.
	bool  Equals(const ZM_GenImage& xOther) const;
	u_int ContentHash() const;

private:
	Zenith_Vector<float> m_xRGBA;   // interleaved, size == w*h*4
	u_int m_uWidth = 0u, m_uHeight = 0u;
};

// ---------------------------------------------------------------------------
// Type identity -> palette (all 18 ZM_TYPE mapped; GDD 5). Compiled const
// table, no disk read (AssetManifest 0.1).
// ---------------------------------------------------------------------------
struct ZM_TypePalette
{
	Zenith_Maths::Vector3 m_xBase;     // dominant body colour
	Zenith_Maths::Vector3 m_xAccent;   // pattern / detail colour
	Zenith_Maths::Vector3 m_xBelly;    // underside
};
ZM_TypePalette ZM_SynthTypePalette(ZM_TYPE eType);
// Dual-typed species: a fixed 60/40 primary-weighted base with secondary ->
// accent. Order-stable: (primary,secondary) differs from (secondary,primary).
ZM_TypePalette ZM_SynthBlendPalette(ZM_TYPE ePrimary, ZM_TYPE eSecondary);

// ---------------------------------------------------------------------------
// Pattern layers + reusable decals. Fills write into an already-base-filled
// image; colours are explicit linear Vector3. ApplySpots is the ONLY RNG-driven
// fill: its draw order is FIXED (i = 0..uCount-1, then centreU, centreV, radius,
// softness per spot) and unit-tested, because draw order is what makes a PCG
// stream reproducible (the catch/flee draw-order class of bug).
// ---------------------------------------------------------------------------
enum ZM_PATTERN_KIND : u_int
{
	ZM_PATTERN_NONE,
	ZM_PATTERN_STRIPES,
	ZM_PATTERN_SPOTS,
	ZM_PATTERN_GRADIENT,
	ZM_PATTERN_BELLY,
};

struct ZM_PatternParams
{
	ZM_PATTERN_KIND m_eKind = ZM_PATTERN_NONE;
	float           m_fFrequency = 6.0f;   // stripe/spot density
	float           m_fContrast  = 1.0f;   // ink strength [0,1]
	float           m_fJitter    = 0.0f;   // positional jitter [0,1]
	u_int           m_uCount     = 12u;    // stripe bands / spot count (fixed up front)
};

void ZM_SynthFillSolid   (ZM_GenImage& xImg, const Zenith_Maths::Vector3& xColour);
void ZM_SynthFillGradient(ZM_GenImage& xImg, const Zenith_Maths::Vector3& xTop,
	const Zenith_Maths::Vector3& xBottom);
void ZM_SynthApplyStripes(ZM_GenImage& xImg, const ZM_PatternParams& xParams,
	const Zenith_Maths::Vector3& xInk);
void ZM_SynthApplySpots  (ZM_GenImage& xImg, const ZM_PatternParams& xParams,
	const Zenith_Maths::Vector3& xInk, ZM_GenRNG& xRng);
void ZM_SynthApplyBelly  (ZM_GenImage& xImg, const Zenith_Maths::Vector3& xBelly,
	float fSplitV, float fSoftness);
// Reusable decals: eye (creatures) + axis-aligned rect (building windows/doors).
void ZM_SynthStampEyeDecal (ZM_GenImage& xImg, float fCentreU, float fCentreV,
	float fRadius, const Zenith_Maths::Vector3& xIris, const Zenith_Maths::Vector3& xPupil);
void ZM_SynthStampRectDecal(ZM_GenImage& xImg, float fU0, float fV0, float fU1, float fV1,
	const Zenith_Maths::Vector3& xColour);

// ---------------------------------------------------------------------------
// Shiny: hue-rotate the RGB of a source albedo into a NEW image. GUARANTEES a
// differing output at identical dimensions -- an achromatic (grey/white) source
// is nudged in saturation/lightness instead, so "shiny differs" always holds;
// re-running the same rotation is byte-identical. Mesh + skeleton are untouched
// (shiny is texture-only), so "shiny differs, mesh/skeleton identical" is
// structural.
// ---------------------------------------------------------------------------
ZM_GenImage ZM_SynthHueRotate(const ZM_GenImage& xSrc, float fDegrees);

// Normal map from a height field, for the (later) human/building generators.
// Reads xHeightSrc's R channel as height; central differences,
// n = normalize(-dX, -dY, 1), encodes (n*0.5+0.5) into RGB. Gradient scale
// tracks resolution (2.2f * width / 1024.0f). Zero-length -> flat (0.5,0.5,1).
// Returns an RGBA image ready for PackRGBA8(non-sRGB) + BC5 export.
ZM_GenImage ZM_SynthNormalFromHeight(const ZM_GenImage& xHeightSrc, float fStrength);

// ---------------------------------------------------------------------------
// Top-level creature albedo synthesis -- the ZM_CreatureGen entry. Fully a pure
// function of (recipe, rng): draws only from xRng (seeded via
// ZM_GenDeriveSeed(..., ZM_GEN_DOMAIN_ALBEDO)). Same recipe+seed => byte-
// identical texels.
// ---------------------------------------------------------------------------
// SC5d palette-saturation boost: ZM_SynthCreatureAlbedo multiplies each RESOLVED
// palette colour's HSV saturation by this factor (hue + value preserved, saturation
// clamped to [0,1]) so every creature reads with a clearly-saturated type colour
// instead of the earlier soft/pastel look. Creature-scoped -- the raw
// ZM_SynthTypePalette / ZM_SynthBlendPalette tables are untouched. Tunable; a fixed
// factor keeps generation deterministic (1.0 == the pre-SC5d look).
constexpr float fZM_CREATURE_ALBEDO_SATURATION_BOOST = 1.6f;

struct ZM_CreatureTexRecipe
{
	ZM_TYPE          m_ePrimaryType   = ZM_TYPE_NORMAL;
	ZM_TYPE          m_eSecondaryType = ZM_TYPE_NONE;   // == NONE for mono-type
	ZM_PatternParams m_xPattern;
	float            m_fEyeU = 0.5f, m_fEyeV = 0.35f, m_fEyeRadius = 0.06f;
	u_int            m_uWidth  = uZM_CREATURE_ALBEDO_RESOLUTION;
	u_int            m_uHeight = uZM_CREATURE_ALBEDO_RESOLUTION;
};
ZM_GenImage ZM_SynthCreatureAlbedo(const ZM_CreatureTexRecipe& xRecipe, ZM_GenRNG& xRng);

// ---------------------------------------------------------------------------
// .ztxtr bake bridges (TOOLS ONLY). Albedo -> BC1 (creature) or sRGB v2
// uncompressed (colour that must stay sRGB); normal -> BC5; icon -> BC1. Reuse
// Zenith_Tools_TextureExport verbatim. Non-tools no-ops keep _False linking.
// ---------------------------------------------------------------------------
#ifdef ZENITH_TOOLS
bool ZM_SynthBakeAlbedoBC1 (const ZM_GenImage& xImg, const char* szPath);
bool ZM_SynthBakeAlbedoSRGB(const ZM_GenImage& xImg, const char* szPath);
bool ZM_SynthBakeNormalBC5 (const ZM_GenImage& xNormalImg, const char* szPath);
bool ZM_SynthBakeIconBC1   (const ZM_GenImage& xImg, const char* szPath);
#else
inline bool ZM_SynthBakeAlbedoBC1 (const ZM_GenImage&, const char*) { return false; }
inline bool ZM_SynthBakeAlbedoSRGB(const ZM_GenImage&, const char*) { return false; }
inline bool ZM_SynthBakeNormalBC5 (const ZM_GenImage&, const char*) { return false; }
inline bool ZM_SynthBakeIconBC1   (const ZM_GenImage&, const char*) { return false; }
#endif
