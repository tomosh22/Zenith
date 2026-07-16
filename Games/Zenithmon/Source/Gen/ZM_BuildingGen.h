#pragma once

// ============================================================================
// ZM_BuildingGen -- the S4 procedural BUILDING-asset generator: it turns a
// ZM_BUILDING_ID into a deterministic building bundle (a STATIC mesh + a
// placeholder facade) and (in tools builds, SC5) bakes it to disk under the
// ZM_BuildingAssetPath scheme.
//
// STATIC, unlike creatures/humans: buildings have NO skeleton and NO animation.
// The mesh carries zero bones and byte-empty skin buffers; it validates via
// ZM_ValidateGenMeshStatic (NOT the skinned ZM_ValidateGenMesh, whose weight-sum
// check fails a no-weight mesh) and lofts through ZM_StaticMesh::AppendBox (NOT
// the bone-binding ring loft). No .zskel, no .zanim.
//
// DETERMINISM (AssetManifest 6.2): every output byte is a pure function of the
// building id. Randomness reaches a builder ONLY through ZM_MakeGenRNG over the
// recipe's pre-derived m_aulDomainSeed[] (SC1 draws none -- the box shell + solid
// facade are seed-independent; the seeds are derived for SC2/SC3). Same id =>
// byte-identical bundle, proved by ZM_BuildingBuildEqual / ZM_BuildingContentHash.
//
// GUARD MODEL (mirrors ZM_HumanGen / ZM_GenCommon): the pure generation API below
// compiles in ALL configs so the in-memory ZM_Gen unit gate exercises it headless.
// Only the disk bake at the very end is #ifdef ZENITH_TOOLS, with a non-tools
// no-op so _False builds link. SC1 declares the bake as STUBS returning false;
// the real bodies land in SC5.
//
// STAGED SCOPE: SC1 lands the frozen seam + roster + a minimal (real-dimensioned)
// box shell + a flat facade fill. SC2 replaces the box with the real parametric
// shell (roof/storeys/windows); SC3 paints the facade decals; SC5 bakes to disk.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_GenCommon.h"       // ZM_GenMesh, ZM_GEN_DOMAIN, ZM_GenRNG, ZM_StaticMesh, static validator
#include "Zenithmon/Source/Gen/ZM_TextureSynth.h"     // ZM_GenImage (the placeholder facade type)
#include "Zenithmon/Source/Data/ZM_BuildingData.h"    // ZM_BUILDING_ID + the roster enums

// ZM_BakeManifest (a later box) stamps this per-family version; bump it whenever
// this module's generation algorithms change so stale bakes self-invalidate.
constexpr u_int uZM_BUILDINGGEN_VERSION           = 1u;

// The placeholder facade resolution SC1 fills with a flat palette colour. SC3
// replaces it with the real synthesised texture; the value is not golden.
constexpr u_int uZM_BUILDING_FACADE_RESOLUTION    = 256u;

// Buildings have no evolution, so the seed-derivation evo-stage slot is a fixed
// constant (keeps ZM_GenDeriveSeed's signature shared with creatures/humans).
constexpr u_int uZM_BUILDING_SYNTHETIC_EVO_STAGE  = 1u;

// ---------------------------------------------------------------------------
// ZM_BuildingRecipe -- the fully resolved per-building generation inputs. Pure
// data; ZM_ResolveBuildingRecipe fills it deterministically from ZM_BuildingData.
// ---------------------------------------------------------------------------
struct ZM_BuildingRecipe
{
	ZM_BUILDING_ID      m_eId            = ZM_BUILDING_HOUSE_COTTAGE_WARM;
	u_int               m_uSyntheticSeed = 0u;   // family seed == ZM_GenHashName(m_szName)

	// One independent 64-bit PCG seed per generation domain (ZM_GenDeriveSeed).
	// The full array is derived so a builder can index any domain without a gap;
	// SC1 draws none, SC2/SC3 draw MESH + ALBEDO.
	u_int64             m_aulDomainSeed[ZM_GEN_DOMAIN_COUNT] = {};

	ZM_BUILDING_STYLE   m_eStyle         = ZM_BUILDING_STYLE_COTTAGE;
	ZM_BUILDING_PALETTE m_ePalette       = ZM_BUILDING_PALETTE_WARM;
	ZM_ROOF_KIND        m_eRoof          = ZM_ROOF_GABLE;
	float               m_fWidth = 6.0f, m_fDepth = 5.0f, m_fStoreyHeight = 3.0f;
	u_int               m_uStoreys = 1u, m_uWindowCols = 2u, m_uWindowRows = 1u;
	ZM_TYPE             m_eThemeType     = ZM_TYPE_NONE;
};

// Resolve a building id into its full generation recipe (bounds-asserted id).
ZM_BuildingRecipe ZM_ResolveBuildingRecipe(ZM_BUILDING_ID eId);

// Seed a domain's generation RNG from a resolved recipe. THE single entry point
// through which randomness reaches any builder (keeps the determinism invariant
// auditable). SC1 does not call this; the SC2/SC3 builders will.
ZM_GenRNG ZM_MakeGenRNG(const ZM_BuildingRecipe& xR, ZM_GEN_DOMAIN eDomain);

// ---------------------------------------------------------------------------
// Per-output builders (pure functions of the recipe). Each is separately
// unit-testable.
// ---------------------------------------------------------------------------

// Build the static box shell (NO bones): one real-dimensioned axis-aligned box
// (footprint m_fWidth x m_fDepth, grounded at y=0, height m_fStoreyHeight *
// m_uStoreys) via ZM_StaticMesh::AppendBox, then finalise tangents. SC1 body is
// intentionally MINIMAL -- enough that ZM_ValidateGenMeshStatic passes; SC2
// replaces it with the real parametric shell.
void        ZM_BuildBuildingMesh  (const ZM_BuildingRecipe& xR, ZM_GenMesh& xMesh);

// Build the placeholder facade: a uZM_BUILDING_FACADE_RESOLUTION-square image
// filled with a per-palette solid colour. SC3 paints the real decals.
ZM_GenImage ZM_BuildBuildingFacade(const ZM_BuildingRecipe& xR);

// ---------------------------------------------------------------------------
// ZM_Building -- the full in-memory bundle SC1 produces (mesh + placeholder
// facade). The .zmtrl / .zmodel bundle bake is deferred to SC5.
// ---------------------------------------------------------------------------
struct ZM_Building
{
	ZM_BUILDING_ID m_eId = ZM_BUILDING_NONE;
	ZM_GenMesh     m_xMesh;      // static: positions/normals/uvs/tangents/colours, zero bones
	ZM_GenImage    m_xFacade;    // SC1: a flat palette placeholder; SC3 does the real texture
};

// Build the complete bundle for a building (resolve -> mesh -> facade), in that
// fixed order.
void ZM_BuildBuilding(ZM_BUILDING_ID eId, ZM_Building& xOut);

// ---------------------------------------------------------------------------
// Determinism helpers (the same-id byte-identity gate machinery).
// ---------------------------------------------------------------------------

// Byte-exact SoA compare over every ZM_GenMesh buffer (sizes then memcmp).
bool  ZM_BuildingMeshEqual (const ZM_GenMesh& xA, const ZM_GenMesh& xB);

// Byte-exact compare of two bundles: mesh + facade.
bool  ZM_BuildingBuildEqual(const ZM_Building& xA, const ZM_Building& xB);

// FNV-1a content hash folding the mesh SoA buffers and the facade bytes.
u_int ZM_BuildingContentHash(const ZM_Building& xBuilding);

// ---------------------------------------------------------------------------
// ZM_ValidateBuilding -- the S4 building test contract in one pure call; wraps
// ZM_ValidateGenMeshStatic (the bone-free structural validator) and adds the
// facade non-empty check. m_bAllValid is the conjunction.
// ---------------------------------------------------------------------------
struct ZM_BuildingValidation
{
	ZM_GenStaticMeshValidation m_xMesh;
	bool m_bFacadeNonEmpty = false;
	bool m_bAllValid = false;
};
ZM_BuildingValidation ZM_ValidateBuilding(const ZM_Building& xBuilding);

// ---------------------------------------------------------------------------
// Asset-path scheme (AssetManifest section 2). PER-MODEL only (buildings are
// static: NO shared skeleton/anim set):
//   game:Buildings/<Name>/<Name>.zmesh / _facade.ztxtr / .zmtrl / .zmodel
// Writes the canonical "game:" ref and returns false on buffer overflow
// (truncation), mirroring ZM_HumanAssetPath.
// ---------------------------------------------------------------------------
enum ZM_BUILDING_ASSET_KIND : u_int
{
	ZM_BUILDING_ASSET_MESH,       // <Name>.zmesh
	ZM_BUILDING_ASSET_FACADE,     // <Name>_facade.ztxtr
	ZM_BUILDING_ASSET_MATERIAL,   // <Name>.zmtrl
	ZM_BUILDING_ASSET_MODEL,      // <Name>.zmodel

	ZM_BUILDING_ASSET_KIND_COUNT
};

// Write the canonical per-model "game:" asset ref for (building, kind) into szOut.
// Returns false (leaving szOut best-effort NUL-terminated) if uCap is too small.
bool ZM_BuildingAssetPath(ZM_BUILDING_ID eId, ZM_BUILDING_ASSET_KIND eKind, char* szOut, u_int uCap);

// ---------------------------------------------------------------------------
// Disk bake (TOOLS ONLY) -- ZM_BakeBuilding writes one model's mesh/facade/
// material/model bundle; ZM_BakeAllBuildings bakes every model. NOT exercised by
// the in-memory ZM_Gen gate. Bodies land in SC5; SC1 declares them as STUBS
// returning false. Non-tools no-ops keep _False configs linking.
// ---------------------------------------------------------------------------
#ifdef ZENITH_TOOLS
bool ZM_BakeBuilding(ZM_BUILDING_ID eId);   // STUB in SC1 (return false); real bake = SC5
bool ZM_BakeAllBuildings();                  // STUB in SC1 (return false); real bake = SC5
#else
inline bool ZM_BakeBuilding(ZM_BUILDING_ID) { return false; }
inline bool ZM_BakeAllBuildings()           { return false; }
#endif
