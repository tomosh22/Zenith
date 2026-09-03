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
constexpr u_int uZM_TEXTURESYNTH_VERSION       = 3u;   // v3: albedo bakes stamped BC1_RGB_SRGB (sampler-decoded), not UNORM
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

// Normal map from a height field, for the human/building generators.
// Reads xHeightSrc's R channel as height; central differences,
// n = normalize(-dX, -dY, 1), encodes (n*0.5+0.5) into RGB. Gradient scale
// tracks resolution (2.2f * width / 1024.0f). Zero-length -> flat (0.5,0.5,1).
// Returns an RGBA image ready for PackRGBA8(non-sRGB) + BC5 export.
//
// bWrap selects the neighbour rule at the image border: CLAMPED (an atlas with
// gutters -- creatures, humans) or WRAPPED (a TILING surface -- buildings,
// interiors, the shared micro-detail). A tiling map built with the clamped rule
// carries a one-texel line of wrong slope along every repeat, which at eight
// repeats per metre is a visible grid.
ZM_GenImage ZM_SynthNormalFromHeight(const ZM_GenImage& xHeightSrc, float fStrength,
	bool bWrap = false);

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
// LINEAR data (roughness/metallic, occlusion): non-sRGB bytes into BC1. See the
// implementation for why sharing the albedo path would be a silent gamma bug.
bool ZM_SynthBakeLinearBC1 (const ZM_GenImage& xImg, const char* szPath);
bool ZM_SynthBakeNormalBC5 (const ZM_GenImage& xNormalImg, const char* szPath);
bool ZM_SynthBakeIconBC1   (const ZM_GenImage& xImg, const char* szPath);
#else
inline bool ZM_SynthBakeAlbedoBC1 (const ZM_GenImage&, const char*) { return false; }
inline bool ZM_SynthBakeAlbedoSRGB(const ZM_GenImage&, const char*) { return false; }
inline bool ZM_SynthBakeLinearBC1 (const ZM_GenImage&, const char*) { return false; }
inline bool ZM_SynthBakeNormalBC5 (const ZM_GenImage&, const char*) { return false; }
inline bool ZM_SynthBakeIconBC1   (const ZM_GenImage&, const char*) { return false; }
#endif

// ===========================================================================
// TILEABLE PROCEDURAL PRIMITIVES -- shared by every ARCHITECTURAL generator
// (ZM_BuildingGen's four surface classes, ZM_InteriorGen's room shells).
//
// ★ EVERY TEXEL IS A PURE FUNCTION OF ITS COORDINATE, never of an RNG draw. A
// per-texel draw makes the image depend on iteration order, hence on the
// resolution, which is exactly how a "deterministic" generator stops being one.
// Callers draw a SALT once, from their own domain RNG, and pass it in.
//
// ★ AND EVERYTHING WRAPS: the lattices are periodic in texel space, so a
// repeated surface shows no seam. That is what makes the world-scaled tiling UVs
// of ZM_StaticMesh::AppendWorldBox usable at all.
// ===========================================================================

// Integer hash -> [0,1). Pure u_int arithmetic, so no float reassociation can
// make two compilers disagree.
float ZM_SynthTexHash01(u_int uX, u_int uY, u_int uSalt);

// Smoothstepped value noise on a WRAPPING lattice.
// ★ uPeriod MUST be > 1: a period-1 lattice is CONSTANT, which renders as a flat
// surface and is the trap this repo has already paid for once on a wrapped
// scatter. Asserted.
float ZM_SynthValueNoise(float fU, float fV, u_int uPeriod, u_int uSalt);

// Two octaves of the above -- enough for an architectural surface, and cheap.
float ZM_SynthFbm(float fU, float fV, u_int uPeriod, u_int uSalt);

// One masonry / tile / board course lattice.
struct ZM_SynthCourseSample
{
	float m_fJoint = 0.0f;   // 1 inside a joint, 0 on the unit face
	float m_fUnit  = 0.0f;   // per-unit hash in [0,1), so units vary from each other
};
ZM_SynthCourseSample ZM_SynthSampleCourses(float fU, float fV, u_int uRows, u_int uCols,
	float fJointWidth, bool bStagger, u_int uSalt);

// ===========================================================================
// THE PBR MAP SET -- one builder, shared by every generator that ships a
// material.
//
// ★★ WHY THIS IS SHARED RATHER THAN COPIED FOR A FOURTH TIME. Buildings,
// interiors, props, humans and creatures all need the same three derived maps
// from the same one input: a HEIGHT field. Normal is its gradient, occlusion is
// its cavity, and roughness is a constant modulated by that cavity. Only the
// height field and the response differ per family, so only those stay local.
//
// ★ AND THE THREE ARE DERIVED FROM ONE SOURCE ON PURPOSE. A normal map and an
// AO map built from different fields disagree about where the surface is, and
// the disagreement reads as dirt that does not line up with the relief -- the
// most common way a procedurally-generated material looks synthetic.
// ===========================================================================
struct ZM_SynthPbrResponse
{
	float m_fRoughness      = 0.85f;   // the base, before cavity modulation
	float m_fMetallic       = 0.0f;    // 0 for every dielectric; 1 only for real metal
	float m_fNormalStrength = 1.0f;
	float m_fRoughnessJitter = 0.0f;   // one per-asset offset, drawn by the caller
	float m_fCavityRoughness = 0.16f;  // how much a cavity roughens (0 disables)
	float m_fCavityOcclusion = 0.50f;  // how dark a full cavity gets
	bool  m_bWrap            = false;  // tiling surface: wrap the gradient at the border

	// ★ EDGE WEAR -- the second thing after cavities that weather does to a
	// surface, and the one that reads as AGE rather than dirt. Anything proud and
	// sharp -- an arris, the lip of a stone, the edge of a board -- gets rubbed,
	// rained on and knocked; it loses its paint or its patina and polishes. The
	// mask is derived from the height field's own gradient (steep AND high = a
	// proud edge), so it lands exactly where the normal map says the edge is.
	// m_fEdgeWearStrength scales the mask (0 disables it and leaves the RM/AO
	// bytes untouched); m_fEdgeWearRoughness is how much a fully-worn texel
	// smooths. The albedo response is the caller's -- lighten plaster, expose
	// timber under paint -- because it differs per material.
	float m_fEdgeWearStrength  = 0.0f;
	float m_fEdgeWearRoughness = 0.20f;
};

struct ZM_SynthPbrSet
{
	ZM_GenImage m_xNormal;              // tangent-space, from the height gradient
	ZM_GenImage m_xRoughnessMetallic;   // G = roughness, B = metallic (glTF packing)
	ZM_GenImage m_xOcclusion;           // R = AO
	ZM_GenImage m_xEdgeWear;            // R = edge-wear mask in [0,1] (1 = fully worn arris)

	bool Equals(const ZM_SynthPbrSet& xOther) const;
	bool NonEmpty() const;
};

// The edge-wear mask on its own: gradient magnitude (wrapping central
// differences, normalised so the SAME content gives the same mask at any
// resolution) gated by height-above-mean, so only the PROUD side of a step is
// worn. A pure function of the height field; exposed so a unit can prove where
// it lands.
ZM_GenImage ZM_SynthEdgeWearFromHeight(const ZM_GenImage& xHeight);

// Build all three from one height field. The result is the same size as the
// input, and is a PURE function of (height, response) -- no RNG is drawn here;
// the caller passes any jitter in through m_fRoughnessJitter.
ZM_SynthPbrSet ZM_SynthBuildPbrSet(const ZM_GenImage& xHeight,
	const ZM_SynthPbrResponse& xResponse);

// ===========================================================================
// THE SHARED MICRO-DETAIL PAIR -- one tiling grain that every architectural
// material overlays at 8-12 repeats per tile.
//
// ★ WHY IT EXISTS. A 512^2 map on a 3 m tile is 170 px/m; a player standing at
// a wall sees a texel every 6 mm and the surface goes soft. The engine's detail
// slots (Flux_MaterialGPU: DETAIL_ALBEDO / DETAIL_NORMAL, x2 mid-grey overlay +
// UDN normal blend) exist for exactly this, and the grain of plaster, stone grit
// and weathered slate is close enough at that scale that ONE pair serves them
// all. It is mean-neutral (the albedo averages 0.5 so the x2 overlay is identity
// on average) and it WRAPS, or the detail tiling would draw its own grid.
// ===========================================================================
constexpr u_int uZM_SYNTH_MICRODETAIL_RESOLUTION = 512u;

struct ZM_SynthDetailPair
{
	ZM_GenImage m_xAlbedo;   // linear grey around 0.5 -- bake with ZM_SynthBakeLinearBC1, NOT the sRGB path
	ZM_GenImage m_xNormal;   // tangent-space, wrapped, BC5

	bool Equals(const ZM_SynthDetailPair& xOther) const;
	bool NonEmpty() const;
};

// The grain height field (exposed so its statistics can be asserted) and the pair.
ZM_GenImage         ZM_SynthMicroDetailHeight(u_int uRes, u_int uSalt);
ZM_SynthDetailPair  ZM_SynthBuildMicroDetail(u_int uRes, u_int uSalt);

// A height field from an ALBEDO image's luminance, for the families whose
// surface detail lives in their colour rather than in a separate pattern (skin,
// fur, scales, painted props).
//
// ★ IT IS A HEURISTIC AND IS DOCUMENTED AS ONE. Luminance is not height: a dark
// marking on flat skin becomes a dent. fFlatten pulls the field back toward the
// mean so the derived relief stays subtle, which is the honest use -- it buys
// grain and pore-scale break-up, not sculpted form. Where a family HAS a real
// height source (masonry courses, board joints) it should pass that instead.
ZM_GenImage ZM_SynthHeightFromAlbedoLuma(const ZM_GenImage& xAlbedo, float fFlatten);
