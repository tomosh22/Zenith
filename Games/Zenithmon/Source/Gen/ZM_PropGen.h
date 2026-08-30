#pragma once

// ============================================================================
// ZM_PropGen -- the S4 procedural PROP-asset generator: it turns a ZM_PROP_ID
// into a deterministic prop bundle (a STATIC mesh + a placeholder albedo) and (in
// tools builds, SC5) bakes it to disk under the ZM_PropAssetPath scheme.
//
// STATIC, unlike creatures/humans: props have NO skeleton and NO animation. The
// mesh carries zero bones and byte-empty skin buffers; it validates via
// ZM_ValidateGenMeshStatic (NOT the skinned ZM_ValidateGenMesh, whose weight-sum
// check fails a no-weight mesh) and composes through ZM_StaticMesh::AppendBox
// (NOT the bone-binding ring loft). No .zskel, no .zanim. Colliders are
// scene-authored, NOT part of this generator.
//
// DETERMINISM (AssetManifest 6.2): every output byte is a pure function of the
// prop id. Randomness reaches a builder ONLY through ZM_MakeGenRNG over the
// recipe's pre-derived m_aulDomainSeed[] -- the MESH domain for the box jitter,
// the ALBEDO domain for the colour jitter. Same id => byte-identical bundle,
// proved by ZM_PropBuildEqual / ZM_PropContentHash.
//
// GUARD MODEL (mirrors ZM_BuildingGen / ZM_GenCommon): the pure generation API
// below compiles in ALL configs so the in-memory ZM_Gen unit gate exercises it
// headless. Only the disk bake at the very end is #ifdef ZENITH_TOOLS, with a
// non-tools no-op so _False builds link. SC4 declares the bake as STUBS returning
// false; the real bodies land in SC5.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"       // ZM_GenMesh, ZM_GEN_DOMAIN, ZM_GenRNG, ZM_StaticMesh, static validator
#include "Zenithmon/Source/Gen/ZM_TextureSynth.h"     // ZM_GenImage (the placeholder albedo type)
#include "Zenithmon/Source/Data/ZM_PropData.h"        // ZM_PROP_ID + the roster enums

// ZM_BakeManifest (a later box) stamps this per-family version; bump it whenever
// this module's generation algorithms change so stale bakes self-invalidate.
//
// 2 (ZM-67): the box composition gained ZM_PROP_KIND_ITEM_PICKUP and
// ZM_PROP_KIND_ITEM_SPENT. NO existing prop's bytes moved -- every pre-existing
// kind's switch arm and every pre-existing roster row is untouched -- but the
// algorithm did change, which is what this number is for.
// 3: props gained the full four-map PBR set (normal / roughness-metallic /
// occlusion) beside the albedo, derived from a per-palette height field.
constexpr u_int uZM_PROPGEN_VERSION          = 3u;

// The placeholder albedo resolution SC4 fills with a flat palette colour + accent
// band. Not golden.
constexpr u_int uZM_PROP_ALBEDO_RESOLUTION   = 128u;

// Props have no evolution, so the seed-derivation evo-stage slot is a fixed
// constant (keeps ZM_GenDeriveSeed's signature shared with creatures/humans).
constexpr u_int uZM_PROP_SYNTHETIC_EVO_STAGE = 1u;

// ★★ THE ONE PLACE THE ITEM KINDS' GROUND PLANE IS SPELLED, AND THE ONLY EXCEPTION
// TO "grounded at y = 0" IN THIS MODULE.
//
// A scenery prop is dropped onto an identity transform, so its composition starts
// at y = 0 and that IS the ground. A GROUND-ITEM prop is worn by an entity the
// scene authored at "measured surface + half the cube edge", scaled uniformly by
// that same edge (ZM_Route1PropCentreY / fZM_ROUTE1_PROP_CUBE_EDGE) -- so its local
// origin sits half a unit ABOVE the ground and a composition grounded at 0 would
// hover there. The two ZM_PROP_KIND_ITEM_* arms therefore start here instead.
//
// ★ IT IS -0.5 BECAUSE THE TRANSFORM IS FROZEN, NOT BECAUSE -0.5 IS TIDY. The
// anchors, their measured ground columns and the 2.5 m reach budget that holds
// each prop beside the walked lane are all fixed by ZM-D-207; the picture had to
// move to the transform rather than the other way round.
constexpr float fZM_PROP_ITEM_BASE_Y         = -0.5f;

// ---------------------------------------------------------------------------
// ZM_PropRecipe -- the fully resolved per-prop generation inputs. Pure data;
// ZM_ResolvePropRecipe fills it deterministically from ZM_PropData.
// ---------------------------------------------------------------------------
struct ZM_PropRecipe
{
	ZM_PROP_ID      m_eId            = ZM_PROP_FENCE_WOOD;
	u_int           m_uSyntheticSeed = 0u;   // family seed == ZM_GenHashName(m_szName)

	// One independent 64-bit PCG seed per generation domain (ZM_GenDeriveSeed).
	// The full array is derived so a builder can index any domain without a gap;
	// SC4 draws MESH + ALBEDO.
	u_int64         m_aulDomainSeed[ZM_GEN_DOMAIN_COUNT] = {};

	ZM_PROP_KIND    m_eKind    = ZM_PROP_KIND_FENCE;
	ZM_PROP_BIOME   m_eBiome   = ZM_PROP_BIOME_NONE;
	ZM_PROP_PALETTE m_ePalette = ZM_PROP_PALETTE_WOOD;
	float           m_fWidth = 1.0f, m_fDepth = 1.0f, m_fHeight = 1.0f;
};

// Resolve a prop id into its full generation recipe (bounds-asserted id).
ZM_PropRecipe ZM_ResolvePropRecipe(ZM_PROP_ID eId);

// Seed a domain's generation RNG from a resolved recipe. THE single entry point
// through which randomness reaches any builder (keeps the determinism invariant
// auditable).
ZM_GenRNG ZM_MakeGenRNG(const ZM_PropRecipe& xR, ZM_GEN_DOMAIN eDomain);

// ---------------------------------------------------------------------------
// Per-output builders (pure functions of the recipe). Each separately unit-testable.
// ---------------------------------------------------------------------------

// Build the static box mesh (NO bones): a fixed set of axis-aligned boxes composed
// per m_eKind (grounded at y=0), then finalise tangents. ALL randomness (the box
// jitter) is drawn from the MESH domain ONLY, up-front in a FIXED order before the
// kind branch, so a kind change never alters the draw count and an ALBEDO-seed
// change can never perturb the mesh.
void        ZM_BuildPropMesh   (const ZM_PropRecipe& xR, ZM_GenMesh& xMesh);

// Build the placeholder albedo: a uZM_PROP_ALBEDO_RESOLUTION-square image whose
// base is the palette colour (biome-tinted for the dressing sets) plus a per-
// channel jitter, with an accent band. ALL randomness is drawn from the ALBEDO
// domain ONLY, so a MESH-seed change can never perturb the texture.
ZM_GenImage ZM_BuildPropTexture(const ZM_PropRecipe& xR);

// The palette's BASE colour, before any biome tint or per-prop jitter. Public
// since ZM-67 for exactly one reason: a caller that must present a prop with no
// baked albedo available -- the cold-start shape ZM_GreyboxVisual falls back to on
// a tree that has never run a tools build -- has to paint the same family colour
// the bake would have produced. A second copy of these five constants at that call
// site could drift, and the drift would show up as a prop that changes colour the
// moment a bake appears. Bounds-asserted on a garbage palette, like the roster
// accessors.
Zenith_Maths::Vector3 ZM_PropPaletteColour(ZM_PROP_PALETTE ePalette);

// The per-PALETTE height field the normal / roughness / occlusion maps are all
// derived from. Wood runs a stretched grain, stone is isotropic pitting, metal a
// fine brushed direction, painted a near-flat skim, foliage a leafy break-up.
// Deterministic and tileable: every texel is a pure function of its coordinate
// and the prop's own seed (see the ZM_Synth* primitives).
ZM_GenImage ZM_BuildPropHeight(const ZM_PropRecipe& xR);

// The material response one palette answers to. Metal is the only arm that
// returns m_fMetallic = 1, and it is correct there for the same reason it is
// wrong everywhere else: a lamp post really is a conductor.
ZM_SynthPbrResponse ZM_PropPbrResponse(ZM_PROP_PALETTE ePalette);

// ---------------------------------------------------------------------------
// ZM_Prop -- the full in-memory bundle SC4 produces (mesh + placeholder albedo).
// The .zmtrl / .zmodel bundle bake is deferred to SC5.
// ---------------------------------------------------------------------------
struct ZM_Prop
{
	ZM_PROP_ID     m_eId = ZM_PROP_NONE;
	ZM_GenMesh     m_xMesh;      // static: positions/normals/uvs/tangents/colours, zero bones
	ZM_GenImage    m_xTexture;   // the base colour
	// ★ THE FULL PBR SET, not an albedo alone. A prop lit by the same lights and
	// the same SSGI as the room around it, wearing only a base colour, reads as
	// painted card next to a wall that has relief -- and the interiors gave it
	// exactly that comparison to lose. Derived from a per-PALETTE height field
	// (ZM_BuildPropHeight), so wood gets grain, stone gets pitting and metal a
	// brushed direction.
	ZM_SynthPbrSet m_xPbr;
};

// Build the complete bundle for a prop (resolve -> mesh -> texture), in that
// fixed order.
void ZM_BuildProp(ZM_PROP_ID eId, ZM_Prop& xOut);

// ---------------------------------------------------------------------------
// Determinism helpers (the same-id byte-identity gate machinery).
// ---------------------------------------------------------------------------

// Byte-exact SoA compare over every ZM_GenMesh buffer (sizes then memcmp).
bool  ZM_PropMeshEqual (const ZM_GenMesh& xA, const ZM_GenMesh& xB);

// Byte-exact compare of two bundles: mesh + texture.
bool  ZM_PropBuildEqual(const ZM_Prop& xA, const ZM_Prop& xB);

// FNV-1a content hash folding the mesh SoA buffers and the texture bytes.
u_int ZM_PropContentHash(const ZM_Prop& xProp);

// ---------------------------------------------------------------------------
// ZM_ValidateProp -- the S4 prop test contract in one pure call; wraps
// ZM_ValidateGenMeshStatic (the bone-free structural validator) and adds the
// texture non-empty check. m_bAllValid is the conjunction.
// ---------------------------------------------------------------------------
struct ZM_PropValidation
{
	ZM_GenStaticMeshValidation m_xMesh;
	bool m_bTextureNonEmpty = false;
	bool m_bAllValid = false;
};
ZM_PropValidation ZM_ValidateProp(const ZM_Prop& xProp);

// ---------------------------------------------------------------------------
// Asset-path scheme (AssetManifest section 2). PER-MODEL only (props are static:
// NO shared skeleton/anim set):
//   game:Props/<Name>/<Name>.zmesh / _albedo.ztxtr / .zmtrl / .zmodel
// Writes the canonical "game:" ref and returns false on buffer overflow
// (truncation), mirroring ZM_BuildingAssetPath.
// ---------------------------------------------------------------------------
enum ZM_PROP_ASSET_KIND : u_int
{
	ZM_PROP_ASSET_MESH,       // <Name>.zmesh
	ZM_PROP_ASSET_ALBEDO,     // <Name>_albedo.ztxtr
	ZM_PROP_ASSET_NORMAL,     // <Name>_normal.ztxtr   (BC5)
	ZM_PROP_ASSET_ROUGH_METAL,// <Name>_rm.ztxtr
	ZM_PROP_ASSET_OCCLUSION,  // <Name>_ao.ztxtr
	ZM_PROP_ASSET_MATERIAL,   // <Name>.zmtrl
	ZM_PROP_ASSET_MODEL,      // <Name>.zmodel

	ZM_PROP_ASSET_KIND_COUNT
};

// Write the canonical per-model "game:" asset ref for (prop, kind) into szOut.
// Returns false (leaving szOut best-effort NUL-terminated) if uCap is too small.
bool ZM_PropAssetPath(ZM_PROP_ID eId, ZM_PROP_ASSET_KIND eKind, char* szOut, u_int uCap);

// ---------------------------------------------------------------------------
// Disk bake (TOOLS ONLY) -- ZM_BakeProp writes one model's STATIC
// mesh/albedo/material/model bundle (skeleton-less .zmesh via ZM_GenBakeStaticMesh
// + .zmtrl + a .zmodel with NO skeleton/anim); ZM_BakeAllProps bakes every model.
// NOT exercised by the in-memory ZM_Gen gate (see the tools-only bake smoke
// ZM_Tests_PropBake.cpp). Non-tools no-ops keep _False configs linking.
// ---------------------------------------------------------------------------
#ifdef ZENITH_TOOLS
bool ZM_BakeProp(ZM_PROP_ID eId);    // static bundle: .zmesh/_albedo.ztxtr/.zmtrl/.zmodel
bool ZM_BakeAllProps();               // bakes every prop in the roster

// Is this ONE prop's bundle loadable right now -- and if not, bake just it and
// re-ask. Returns whether all four per-model files are present non-empty after the
// attempt. Idempotent and cheap when warm (four stats, no build).
//
// ★ DELIBERATELY NOT ZM_BakeAllProps, AND NOT A FAMILY STAMP. ZM_BakeManifest.h
// records why no boot bakes a whole family synchronously ("wiring a synchronous
// cold full-family bake into the unconditional tools boot would slow every gate/CI
// boot"), and that reasoning holds for a scene load too. This bakes ONE model and
// writes NO stamp, so ZM_BakeManifestCheck keeps telling the truth about whether
// the FAMILY is warm -- a stamp written here would make every family-guarded test
// skip its own bake and then miss files.
bool ZM_EnsurePropBaked(ZM_PROP_ID eId);
#else
inline bool ZM_BakeProp(ZM_PROP_ID) { return false; }
inline bool ZM_BakeAllProps()       { return false; }
// NOT "the bundle is absent" -- "this build cannot bake". A caller must still try
// to LOAD the model: on Android whatever shipped in the APK IS the bake (the same
// ruling ZM_HumanAssetPolicy.cpp records), so treating false as absent would
// permanently ignore assets that are right there.
inline bool ZM_EnsurePropBaked(ZM_PROP_ID) { return false; }
#endif
