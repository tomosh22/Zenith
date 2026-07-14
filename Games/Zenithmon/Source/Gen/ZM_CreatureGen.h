#pragma once

// ============================================================================
// ZM_CreatureGen -- the S4 procedural creature-asset generator: it turns a
// ZM_SPECIES_ID into a complete, deterministic creature bundle (skinned mesh +
// skeleton, BC1 albedo, hue-rotated shiny albedo, and a flat dex icon) and, in
// tools builds, bakes that bundle to disk under the ZM_CreatureAssetPath scheme.
//
// ARCHITECTURE: a species is resolved into a ZM_CreatureRecipe (family seed +
// per-domain PCG seeds + size scale + elaboration tier + the texture recipe),
// then each output is a pure function of that recipe. The mesh is produced by
// the archetype builder for the species' body plan (the 8 ZM_ARCHETYPE builders
// each live in their own ZM_CreatureArchetype_<Name>.cpp and compose the shared
// ZM_CreatureArchetypeCommon appendage kit); all 8 archetype builders are wired,
// so ZM_GetArchetypeBuilder returns a non-null builder for every archetype.
//
// DETERMINISM (AssetManifest 6.2, the load-bearing S4 invariant): every output
// byte is a pure function of the species id. Randomness reaches a builder ONLY
// through ZM_MakeGenRNG over the recipe's pre-derived m_aulDomainSeed[] (one
// independent PCG stream per ZM_GEN_DOMAIN). No clock / pointer / global RNG /
// container-iteration entropy; fixed draw order. Same species => byte-identical
// bundle, proved by ZM_CreatureBuildEqual / ZM_CreatureContentHash.
//
// GUARD MODEL (mirrors ZM_GenCommon / ZM_TextureSynth): the pure generation API
// below compiles in ALL configs so the in-memory ZM_Gen unit gate exercises it
// headless. Only the disk bake at the very end is #ifdef ZENITH_TOOLS, with a
// non-tools no-op so _False builds link.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"      // ZM_GenMesh, ZM_GEN_DOMAIN, ZM_GenRNG, bake bridge
#include "Zenithmon/Source/Gen/ZM_TextureSynth.h"    // ZM_GenImage, ZM_CreatureTexRecipe, synth + bake bridges
#include "Zenithmon/Source/Data/ZM_SpeciesData.h"    // ZM_SPECIES_ID, ZM_ARCHETYPE, ZM_SIZE_CLASS

// ZM_BakeManifest (a later box) stamps this; bump it whenever this module's
// generation algorithms change so stale bakes self-invalidate.
constexpr u_int uZM_CREATUREGEN_VERSION      = 1u;

// Flat dex/party/box icon resolution (AssetManifest 1.2). BC1 128x128.
constexpr u_int uZM_CREATURE_ICON_RESOLUTION = 128u;

// Shiny hue-rotation band, in degrees, keyed on the SHINY domain. Bounded well
// away from 0 and 360 so a shiny is always visibly distinct from its base albedo
// (ZM_SynthHueRotate additionally guarantees a differing output on achromatic
// sources). GOLDEN-PINNED (a change re-tints every shiny).
constexpr float fZM_CREATURE_SHINY_MIN_DEG   = 80.0f;
constexpr float fZM_CREATURE_SHINY_MAX_DEG   = 280.0f;

// ---------------------------------------------------------------------------
// ZM_CreatureRecipe -- the fully resolved per-species generation inputs. Pure
// data; ZM_ResolveCreatureRecipe fills it deterministically from ZM_SpeciesData.
// ---------------------------------------------------------------------------
struct ZM_CreatureRecipe
{
	ZM_SPECIES_ID  m_eSpecies     = ZM_SPECIES_NONE;
	ZM_ARCHETYPE   m_eArchetype   = ZM_ARCHETYPE_QUADRUPED;
	u_int          m_uEvoStage    = 1u;                     // 1..3 (1 for single-stage/legendary)
	u_int          m_uFamilySeed  = 0u;                     // shared across an evolution line
	ZM_SIZE_CLASS  m_eSizeClass   = ZM_SIZE_MEDIUM;
	float          m_fSizeScale   = 1.0f;                   // ZM_SizeClassScale(m_eSizeClass)
	u_int          m_uElaboration = 0u;                     // evo stage - 1 (0,1,2): +1 detail tier per stage

	// One independent 64-bit PCG seed per generation domain (ZM_GenDeriveSeed).
	// The SOLE randomness source every builder draws from; index with ZM_GEN_DOMAIN_*.
	u_int64        m_aulDomainSeed[ZM_GEN_DOMAIN_COUNT] = {};

	// The texture-synthesis inputs (types + pattern + eye placement), consumed by
	// ZM_SynthCreatureAlbedo with an ALBEDO-domain rng.
	ZM_CreatureTexRecipe m_xTex;
};

// Resolve a species id into its full generation recipe (bounds-asserted id).
ZM_CreatureRecipe ZM_ResolveCreatureRecipe(ZM_SPECIES_ID eId);

// Seed a domain's generation RNG from a resolved recipe. THE single entry point
// through which randomness reaches any builder (keeps the determinism invariant
// auditable: every stream comes from a pre-derived domain seed).
inline ZM_GenRNG ZM_MakeGenRNG(const ZM_CreatureRecipe& xRecipe, ZM_GEN_DOMAIN eDomain)
{
	return ZM_GenRNG(xRecipe.m_aulDomainSeed[eDomain]);
}

// The shiny hue-rotation angle for a recipe (SHINY domain), in
// [fZM_CREATURE_SHINY_MIN_DEG, fZM_CREATURE_SHINY_MAX_DEG).
float ZM_CreatureShinyAngle(const ZM_CreatureRecipe& xRecipe);

// ---------------------------------------------------------------------------
// Archetype builder dispatch. A builder appends the full mesh + skeleton for one
// body plan into xMesh (already Reset) using ZM_CreatureArchetypeCommon; it does
// NOT run the finalise pass (ZM_BuildCreatureMesh owns that).
// ---------------------------------------------------------------------------
typedef void (*ZM_ArchetypeBuilderFn)(ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe);

// The 8 archetype builders (each defined ALONE in ZM_CreatureArchetype_<Name>.cpp
// as it lands). Declared together so the ZM_GetArchetypeBuilder switch can take
// each one's address; an un-authored builder is merely a declaration (never
// referenced until its switch case + .cpp land, so no link error). This block is
// the ONE sanctioned append to the frozen seam, finalised at SC2.
void ZM_BuildArchetype_Quadruped      (ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe);   // SC1
void ZM_BuildArchetype_Biped          (ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe);   // SC2
void ZM_BuildArchetype_Avian          (ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe);   // SC2
void ZM_BuildArchetype_Serpent        (ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe);   // SC3
void ZM_BuildArchetype_Aquatic        (ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe);   // SC3
void ZM_BuildArchetype_Insectoid      (ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe);   // SC4
void ZM_BuildArchetype_Blob           (ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe);   // SC4
void ZM_BuildArchetype_FloaterPlantoid(ZM_GenMesh& xMesh, const ZM_CreatureRecipe& xRecipe);   // SC5

// Explicit archetype -> builder mapping. All 8 archetype builders are wired, so
// every archetype value routes to a non-null builder; only the out-of-range
// ZM_ARCHETYPE_COUNT sentinel returns nullptr.
ZM_ArchetypeBuilderFn ZM_GetArchetypeBuilder(ZM_ARCHETYPE eArchetype);

// ---------------------------------------------------------------------------
// Per-output builders (pure functions of the recipe). Each is separately
// unit-testable.
// ---------------------------------------------------------------------------

// Build mesh + skeleton via the archetype builder, then the ONE finalise order:
// ZM_GenGenerateTangents -> ZM_GenNormalizeSkinWeights (the archetype builders
// emit analytic loft normals, so normals are NOT regenerated). Asserts a
// non-null builder; in debug also asserts ZM_ValidateGenMesh at the creature
// bone cap (30).
void ZM_BuildCreatureMesh(const ZM_CreatureRecipe& xRecipe, ZM_GenMesh& xMesh);

// Base albedo: ZM_SynthCreatureAlbedo driven by an ALBEDO-domain rng.
ZM_GenImage ZM_BuildCreatureAlbedo(const ZM_CreatureRecipe& xRecipe);

// Shiny albedo: the base albedo hue-rotated by ZM_CreatureShinyAngle. Same dims
// as the base; guaranteed to differ (ZM_SynthHueRotate contract).
ZM_GenImage ZM_BuildCreatureShiny(const ZM_CreatureRecipe& xRecipe, const ZM_GenImage& xAlbedo);

// Flat dex icon (128x128): a deterministic box-downsample of the base albedo
// blended over a flat primary-type-tinted background, keyed on the DEX_ICON
// domain. Never empty for a non-empty albedo. It carries >= 2 distinct texels NOT
// unconditionally but because the source albedo is non-constant -- it always bears
// a varying eye/pattern decal, so the box-downsample preserves at least two texels.
// A perfectly flat albedo would legitimately collapse to a single-texel icon.
ZM_GenImage ZM_BuildCreatureIcon(const ZM_CreatureRecipe& xRecipe, const ZM_GenImage& xAlbedo);

// ---------------------------------------------------------------------------
// ZM_Creature -- the full in-memory bundle SC1 produces (mesh + skeleton + the
// three images). The .zmtrl / .zmodel bundle is deferred to SC5.
// ---------------------------------------------------------------------------
struct ZM_Creature
{
	ZM_SPECIES_ID m_eSpecies = ZM_SPECIES_NONE;
	ZM_GenMesh    m_xMesh;      // positions/normals/uvs/tangents/skin + bones
	ZM_GenImage   m_xAlbedo;
	ZM_GenImage   m_xShiny;
	ZM_GenImage   m_xIcon;
};

// Build the complete bundle for a species (resolve -> mesh -> albedo -> shiny ->
// icon), in that fixed order.
void ZM_BuildCreature(ZM_SPECIES_ID eId, ZM_Creature& xOut);

// ---------------------------------------------------------------------------
// Determinism helpers (the same-seed byte-identity gate machinery).
// ---------------------------------------------------------------------------

// Byte-exact SoA compare over every ZM_GenMesh buffer (sizes then memcmp).
bool  ZM_CreatureMeshEqual(const ZM_GenMesh& xA, const ZM_GenMesh& xB);

// Byte-exact compare of two bundles: mesh + all three images.
bool  ZM_CreatureBuildEqual(const ZM_Creature& xA, const ZM_Creature& xB);

// FNV-1a content hash folding the mesh SoA buffers and the three image hashes.
u_int ZM_CreatureContentHash(const ZM_Creature& xCreature);

// ---------------------------------------------------------------------------
// ZM_ValidateCreature -- the S4 creature test contract in one pure call; fills
// EVERY flag. m_bAllValid is the conjunction of the structural flags.
// ---------------------------------------------------------------------------
struct ZM_CreatureValidation
{
	// Mesh structure (from ZM_ValidateGenMesh at the creature bone cap).
	bool  m_bMeshValid           = false;   // winding && bounds && weights && <=2 infl && cap
	bool  m_bWindingOutward      = false;
	bool  m_bBoundsNonDegen      = false;
	bool  m_bWeightsSumToOne     = false;
	bool  m_bWeightsAtMostTwo    = false;
	bool  m_bBonesWithinCap      = false;
	// Skeleton topology.
	bool  m_bHasSingleRoot       = false;   // exactly one bone with parent -1
	bool  m_bParentsBeforeChildren = false; // every parent index < child index
	// Textures.
	bool  m_bAlbedoNonEmpty      = false;
	bool  m_bShinyDimsMatch      = false;   // shiny dims == albedo dims
	bool  m_bShinyDiffers        = false;   // shiny content != albedo content
	bool  m_bIconNonEmpty        = false;
	bool  m_bIconDistinctTexels  = false;   // icon carries >= 2 distinct texels
	// Rollup.
	bool  m_bAllValid            = false;
	u_int m_uFirstBadVertex      = 0xFFFFFFFFu;
	u_int m_uFirstBadTriangle    = 0xFFFFFFFFu;
};
ZM_CreatureValidation ZM_ValidateCreature(const ZM_Creature& xCreature);

// ---------------------------------------------------------------------------
// Asset-path scheme (THE per-species file layout, AssetManifest 1.2 slot). The
// canonical asset REF is:
//     game:Creatures/<SpeciesName>/<SpeciesName><suffix>.<ext>
// with one directory per species. ZM_CreatureAssetPath writes that ref and
// returns false on buffer overflow (truncation). The MATERIAL / MODEL kinds are
// reserved for SC5 (their bake is deferred); their ref strings resolve now so
// callers can reference them ahead of the bake.
// ---------------------------------------------------------------------------
enum ZM_CREATURE_ASSET_KIND : u_int
{
	ZM_CREATURE_ASSET_MESH,            // <Name>.zmesh
	ZM_CREATURE_ASSET_SKELETON,        // <Name>.zskel
	ZM_CREATURE_ASSET_ALBEDO,          // <Name>_albedo.ztxtr
	ZM_CREATURE_ASSET_SHINY,           // <Name>_shiny.ztxtr
	ZM_CREATURE_ASSET_ICON,            // <Name>_icon.ztxtr
	ZM_CREATURE_ASSET_MATERIAL,        // <Name>.zmtrl        (SC5)
	ZM_CREATURE_ASSET_MATERIAL_SHINY,  // <Name>_shiny.zmtrl  (SC5)
	ZM_CREATURE_ASSET_MODEL,           // <Name>.zmodel       (SC5)
	ZM_CREATURE_ASSET_MODEL_SHINY,     // <Name>_shiny.zmodel (SC5)

	ZM_CREATURE_ASSET_KIND_COUNT
};

// Write the canonical "game:" asset ref for (species, kind) into szOut. Returns
// false (and leaves szOut best-effort NUL-terminated) if uCap is too small.
bool ZM_CreatureAssetPath(ZM_SPECIES_ID eId, ZM_CREATURE_ASSET_KIND eKind,
	char* szOut, u_int uCap);

// ---------------------------------------------------------------------------
// Disk bake (TOOLS ONLY) -- ZM_BakeCreature writes the full bundle to the
// ZM_CreatureAssetPath scheme under GAME_ASSETS_DIR: mesh + skeleton
// (ZM_GenBakeMesh), albedo / shiny / icon (ZM_SynthBakeAlbedoBC1 x2 +
// ZM_SynthBakeIconBC1), and (SC5b) the .zmtrl (base + shiny) + .zmodel (base +
// shiny) bundle. NOT exercised by the in-memory ZM_Gen gate. Non-tools no-ops
// keep _False configs linking.
// ---------------------------------------------------------------------------
#ifdef ZENITH_TOOLS
bool ZM_BakeCreature(ZM_SPECIES_ID eId);
bool ZM_BakeAllCreatures();
#else
inline bool ZM_BakeCreature(ZM_SPECIES_ID)  { return false; }
inline bool ZM_BakeAllCreatures()           { return false; }
#endif
