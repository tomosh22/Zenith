#pragma once

// ============================================================================
// ZM_GenCommon -- the S4 asset-generation FOUNDATION: the deterministic
// generation RNG (+ seed derivation + integer-hash noise) and the loft/mesh
// toolkit (the "rings along a bone chain" technique ported from the StickFigure
// precedent in Tools/Zenith_Tools_TestAssetExport.cpp, whose helpers live in an
// anonymous namespace in a Tools TU and are therefore un-linkable). Shared by
// every later S4 generator (ZM_CreatureGen / ZM_HumanGen / ZM_BuildingGen /
// ZM_PropGen), all of which are OUT of scope here.
//
// GUARD MODEL (mirrors ZM_TerrainAuthoring's pure-policy split): the pure
// deterministic library below is compiled in ALL configs so the ZM_Gen unit
// gate exercises it headless -- no GPU, no disk, no engine asset. Only the
// disk-bake bridge at the very end is #ifdef ZENITH_TOOLS, with a non-tools
// no-op so _False builds still link (Zenith_Tools_TreeAssetExport precedent).
//
// DETERMINISM (the load-bearing S4 invariant, AssetManifest 6.2): every byte a
// generator emits is a pure function of an explicit u_int64 seed derived ONLY
// from stable IDs/names (species id, family seed) -- never a clock, pointer
// value, global RNG, or container-iteration order. Same seed => byte-identical
// output, tested in-memory at S4 and hash-gated on disk at S9.
// ============================================================================

#include "Zenithmon/Source/Data/ZM_BattleRNG.h"   // reuse the golden-pinned PCG32 core (ZM-D-027)
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

// ---------------------------------------------------------------------------
// Generator version -- ZM_BakeManifest (a later box) stamps this into its
// per-family marker; bump it whenever this module's algorithms change so stale
// bakes self-invalidate (AssetManifest 7: "fix the generator, bump its version,
// re-bake").
// ---------------------------------------------------------------------------
constexpr u_int uZM_GENCOMMON_VERSION = 1u;

// ---------------------------------------------------------------------------
// Seed derivation -- stable IDs/names ONLY.
// ---------------------------------------------------------------------------

// FNV-1a 32-bit over a NUL-terminated C string. THE canonical name->seed hash
// for all generation. Byte-identical to ZM_TerrainAuthoring's ZM_Fnv1a32 (the
// Dawnmere terrain seed 0x7BF32CA4 comes from this exact algorithm); a unit
// test pins the two together so they can never drift.
u_int ZM_GenHashName(const char* szText);

// Order-stable 32-bit mix: ZM_GenHashCombine(a,b) != ZM_GenHashCombine(b,a).
u_int ZM_GenHashCombine(u_int uSeedA, u_int uSeedB);

// Independent per-output generation sub-streams for one species, so adding a
// draw to (say) the albedo stream can never perturb the mesh stream's bytes.
// This enum is PINNED for the whole S4 program: reordering or removing a value
// changes every derived seed and forces a full cold re-bake, so it is frozen in
// the first commit -- APPEND before ZM_GEN_DOMAIN_COUNT only, never reorder.
enum ZM_GEN_DOMAIN : u_int
{
	ZM_GEN_DOMAIN_MESH,
	ZM_GEN_DOMAIN_SKELETON,
	ZM_GEN_DOMAIN_ALBEDO,
	ZM_GEN_DOMAIN_SHINY,
	ZM_GEN_DOMAIN_PATTERN,
	ZM_GEN_DOMAIN_EYE,
	ZM_GEN_DOMAIN_DEX_ICON,
	ZM_GEN_DOMAIN_ANIM,       // reserved for the ZM_CreatureAnimGen box (scopeOut)

	ZM_GEN_DOMAIN_COUNT
};

// Fold a species recipe (family seed + species id + evolution stage) plus a
// domain into a 64-bit PCG seed. Pure and order-fixed. Family seed comes from
// ZM_GetSpeciesFamilySeed (shared across an evolution line); species id + evo
// stage drive per-stage elaboration. Humans/buildings pass a synthetic id.
u_int64 ZM_GenDeriveSeed(u_int uFamilySeed, u_int uSpeciesId, u_int uEvoStage,
	ZM_GEN_DOMAIN eDomain);

// ---------------------------------------------------------------------------
// ZM_GenRNG -- the deterministic generation RNG. A thin forward over the
// golden-pinned PCG32 of ZM_BattleRNG, but a DISTINCT type so generation
// streams can never alias the battle-logic streams (ZM-D-027 scopes
// ZM_BattleRNG to game logic). Adds the fixed integer->float helpers battle
// omits. Default construction is DELETED so an unseeded generation stream can
// never silently leak in -- every generator constructs one from an explicit
// derived seed.
// ---------------------------------------------------------------------------
class ZM_GenRNG
{
public:
	ZM_GenRNG() = delete;
	explicit ZM_GenRNG(u_int64 ulSeed, u_int64 ulSeq = 54ull) { m_xCore.Seed(ulSeed, ulSeq); }

	void    Seed(u_int64 ulSeed, u_int64 ulSeq = 54ull) { m_xCore.Seed(ulSeed, ulSeq); }

	// Integer core -- byte-identical to ZM_BattleRNG (same golden PCG32 step).
	u_int32 Next()                             { return m_xCore.Next(); }
	u_int   RandBelow(u_int uBound)            { return m_xCore.RandBelow(uBound); }
	u_int   RandRange(u_int uMin, u_int uMax)  { return m_xCore.RandRange(uMin, uMax); }
	bool    Chance(u_int uNum, u_int uDen)     { return m_xCore.Chance(uNum, uDen); }

	// DETERMINISTIC float in [0,1): the top 24 bits of one integer draw times a
	// fixed rational scale 2^-24. No std::uniform_real, no ldexp, no reinterpret
	// -- the bit pattern is identical on any conforming compiler (the codebase-
	// wide reason std::mt19937/std::random are banned).
	float NextFloat01()                          { return (float)(m_xCore.Next() >> 8u) * (1.0f / 16777216.0f); }
	float NextFloatRange(float fMin, float fMax) { return fMin + (fMax - fMin) * NextFloat01(); }
	float NextSignedUnit()                       { return NextFloat01() * 2.0f - 1.0f; }   // [-1,1)

private:
	ZM_BattleRNG m_xCore;
};

// ---------------------------------------------------------------------------
// ZM_GenNoise -- deterministic integer-hash value noise (a ZM-prefixed port of
// Zenith_TerrainNoise, decoupled from the TOOLS-only terrain editor). Integer
// hashing only: no std:: RNG, no float-accumulation-order ambiguity, so it is
// bit-reproducible and CI-hash-safe. All outputs are a pure function of
// (coords, seed). Octave seeding uses a fixed integer schedule, not float
// accumulation of the seed.
// ---------------------------------------------------------------------------
namespace ZM_GenNoise
{
	u_int HashCoords(int iX, int iY, u_int uSeed);
	float HashToFloat01(u_int uHash);
	float ValueNoise2D(float fX, float fY, u_int uSeed);                  // smooth [0,1]
	float FBM2D(float fX, float fY, u_int uSeed,
		u_int uOctaves, float fLacunarity, float fGain);                  // normalized [0,1]
	float RidgedFBM2D(float fX, float fY, u_int uSeed,
		u_int uOctaves, float fLacunarity, float fGain);
}

// ===========================================================================
// LOFT / MESH TOOLKIT
// ===========================================================================

constexpr u_int uZM_GEN_RING_SUBDIV       = 4u;   // Catmull-Rom densification (matches StickFigure uHUMAN_RING_SUBDIV)
constexpr u_int uZM_GEN_CREATURE_BONE_CAP = 30u;  // creatures cap here so clips transfer within an archetype
constexpr u_int uZM_GEN_BONE_NAME_MAX     = 48u;  // fixed bone-name buffer (keeps the result a memcmp-able POD)

// A normalized [0,1] UV island over a part's atlas (>=8px gutters, dilated
// post-paint). Generalizes StickFigure's HumanUVIsland.
struct ZM_GenUVIsland
{
	float m_fU0 = 0.0f, m_fV0 = 0.0f, m_fU1 = 1.0f, m_fV1 = 1.0f;
	float U(float fUNorm) const { return m_fU0 + (m_fU1 - m_fU0) * fUNorm; }
	float V(float fVNorm) const { return m_fV0 + (m_fV1 - m_fV0) * fVNorm; }
};

// A horizontal elliptical cross-section skinned to <=2 bones (generalizes
// HumanRing). Angle convention (load-bearing, copied verbatim from the port):
// ang=0 faces -Z (back seam), ang=pi faces +Z (front). Bone A weight = 1-m_fBlendB.
struct ZM_LoftRing
{
	float m_fY   = 0.0f;                 // sweep coordinate (height along the spine)
	float m_fCx  = 0.0f, m_fCz = 0.0f;   // ring centre in the cross-section plane
	float m_fRx  = 0.5f, m_fRz = 0.5f;   // elliptical half-extents
	u_int m_uBoneA = 0u, m_uBoneB = 0u;  // <=2-bone bind
	float m_fBlendB = 0.0f;              // weight of bone B, in [0,1]
	float m_fSuperEllipse = 1.0f;        // 1=ellipse; <1 rounds toward a box (belly/shoe profile)
};

// One bone as the loft built it -- a POD mirror of Zenith_SkeletonAsset::Bone,
// engine-free so tests and the (later) anim generator can inspect topology
// without a skeleton asset. Parent-before-child order is required (the AddBone
// precondition). Name is a zero-filled fixed buffer so the whole struct is
// deterministically memcmp-able.
struct ZM_GenBone
{
	char                  m_szName[uZM_GEN_BONE_NAME_MAX] = {};
	int                   m_iParent = -1;                        // -1 for root
	Zenith_Maths::Vector3 m_xLocalPos = Zenith_Maths::Vector3(0);
	Zenith_Maths::Quat    m_xLocalRot = glm::identity<Zenith_Maths::Quat>();
	Zenith_Maths::Vector3 m_xLocalScale = Zenith_Maths::Vector3(1);
};

// The pure CPU mesh+skeleton the loft produces and the tests dissect. The SoA
// layout MIRRORS Zenith_MeshAsset's public buffers so the tools bake bridge is
// a straight element-wise copy -- the bytes memcmp'd in-memory are exactly the
// bytes Export() serialises. NO GPU, NO file handles, NO engine asset object.
struct ZM_GenMesh
{
	Zenith_Vector<Zenith_Maths::Vector3> m_xPositions;
	Zenith_Vector<Zenith_Maths::Vector3> m_xNormals;
	Zenith_Vector<Zenith_Maths::Vector2> m_xUVs;
	Zenith_Vector<Zenith_Maths::Vector3> m_xTangents;
	Zenith_Vector<Zenith_Maths::Vector4> m_xColors;
	Zenith_Vector<u_int32>               m_xIndices;
	Zenith_Vector<glm::uvec4>            m_xBoneIndices;   // one per vertex
	Zenith_Vector<glm::vec4>             m_xBoneWeights;   // one per vertex; sums to 1 after normalise
	Zenith_Vector<ZM_GenBone>            m_xBones;

	u_int GetNumVerts() const { return m_xPositions.GetSize(); }
	u_int GetNumTris()  const { return m_xIndices.GetSize() / 3u; }
	u_int GetNumBones() const { return m_xBones.GetSize(); }
	void  Reset();
};

// Find a bone by exact name (the topology-binding contract for anim gen + the
// clip-channel test). Returns -1 if absent.
int ZM_GenMeshFindBone(const ZM_GenMesh& xMesh, const char* szName);

// Axis-aligned bounds over the positions.
Zenith_Maths::Vector3 ZM_GenMeshBoundsMin(const ZM_GenMesh& xMesh);
Zenith_Maths::Vector3 ZM_GenMeshBoundsMax(const ZM_GenMesh& xMesh);

// Structural validation -- the S4 test contract in one pure call. The winding
// check compares each triangle's face normal cross(C-A,B-A) against the average
// of its three vertex normals (dot > 0): a per-part-local restatement of the
// "cross(C-A,B-A) faces outward" rule that stays correct for offset limbs (a
// naive centroid-vs-world-origin test would false-negative there).
struct ZM_GenMeshValidation
{
	bool  m_bWindingOutward   = false;   // every tri cross(C-A,B-A) . avg(vertexNormal) > 0
	bool  m_bBoundsNonDegen   = false;   // extent on all 3 axes > fEps
	bool  m_bWeightsSumToOne  = false;   // per-vertex |sum(weights) - 1| <= fWeightTol
	bool  m_bWeightsAtMostTwo = false;   // <=2 non-zero influences per vertex (the ring bind)
	bool  m_bBonesWithinCap   = false;   // GetNumBones() <= uBoneCap
	u_int m_uFirstBadVertex   = 0xFFFFFFFFu;
	u_int m_uFirstBadTriangle = 0xFFFFFFFFu;
};
ZM_GenMeshValidation ZM_ValidateGenMesh(const ZM_GenMesh& xMesh, u_int uBoneCap,
	float fWeightTol = 1.0e-4f);

// ---- Loft primitives (append into xMesh; return the first-vertex index) ----
namespace ZM_MeshLoft
{
	// Emit uSegs+1 verts around the ellipse (the first column is DUPLICATED at
	// the -Z seam for a clean UV wrap). Writes analytic outward normals,
	// {ang->U, fVNorm->V} UVs, and 2-bone skin {A,B,0,0} / {1-blend,blend,0,0}.
	u_int EmitRing(ZM_GenMesh& xMesh, const ZM_LoftRing& xRing, u_int uSegs,
		const ZM_GenUVIsland& xIsland, float fVNorm);

	// Quad-stitch two emitted rings (ringA above ringB). Non-flip order makes
	// each triangle's cross(C-A,B-A) (== cross(v2-v0, v1-v0)) point OUTWARD --
	// the engine front-face rule (matches Zenith_MeshAsset::GenerateUnitCube).
	// bFlip reverses for an opposite march direction. Indices only, no new verts.
	void StitchRings(ZM_GenMesh& xMesh, u_int uRingAFirstVert, u_int uRingBFirstVert,
		u_int uSegs, bool bFlip = false);

	// Triangle-fan cap to a fresh centre vertex. bUpward selects +axis vs -axis
	// facing winding. Returns the centre vertex index.
	u_int CapRing(ZM_GenMesh& xMesh, u_int uRingFirstVert, u_int uSegs,
		const Zenith_Maths::Vector3& xCentre, const ZM_GenUVIsland& xIsland,
		float fVNorm, bool bUpward);

	// One full swept part.
	struct Part
	{
		const ZM_LoftRing* m_pxRings = nullptr;
		u_int              m_uNumRings = 0u;
		u_int              m_uSegs = 8u;                     // radial segments (>=3)
		ZM_GenUVIsland     m_xIsland;
		bool               m_bCapStart = false;
		bool               m_bCapEnd = false;
		u_int              m_uSubdiv = uZM_GEN_RING_SUBDIV;   // 0 = author rings only
	};

	// Sweep one part: Catmull-Rom densify the ring table (t==0 reproduces an
	// authored ring EXACTLY), spread V by cumulative sweep distance, emit +
	// stitch (+ optionally cap). Skin weights lerp across subdivided rings in
	// per-bone weight space, keeping the 2 heaviest and renormalising. Returns
	// the first vertex index. This is the workhorse the creature/human builders
	// call.
	u_int AppendPart(ZM_GenMesh& xMesh, const Part& xPart);
}

// ---- Skeleton + finalisation ----------------------------------------------

// Append a bone (parent-before-child). Asserts count <= Zenith_SkeletonAsset
// MAX_BONES (100). Returns the new bone index.
u_int ZM_GenAddBone(ZM_GenMesh& xMesh, const char* szName, int iParent,
	const Zenith_Maths::Vector3& xLocalPos, const Zenith_Maths::Quat& xLocalRot,
	const Zenith_Maths::Vector3& xLocalScale);

// Position-welded smooth normals (repo front-face rule; matches
// Zenith_MeshAsset::GenerateNormals, NOT the terrain inversion). Re-run after a
// direct vertex sculpt; the base loft already writes analytic ring normals.
void ZM_GenGenerateNormals(ZM_GenMesh& xMesh);

// Tangents from UVs (skips UV-degenerate triangles), ported from
// Zenith_MeshAsset::GenerateTangents so the tested buffer IS the baked buffer
// (the bake bridge does NOT re-derive tangents on the asset).
void ZM_GenGenerateTangents(ZM_GenMesh& xMesh);

// Renormalise every vertex's <=4 weights to sum to exactly 1, keeping the 2
// heaviest (the ring-bind contract). Idempotent.
void ZM_GenNormalizeSkinWeights(ZM_GenMesh& xMesh);

// ---------------------------------------------------------------------------
// Disk-bake bridge (TOOLS ONLY) -- the sole engine-asset reach point. Copies
// the pure buffers element-wise into a Zenith_MeshAsset + builds a
// Zenith_SkeletonAsset from m_xBones (AddBone parent-before-child +
// ComputeBindPoseMatrices), wires SetSkeletonPath(szSkeletonRef), ComputeBounds,
// then Exports both (.zmesh + .zskel). create_directories first; tolerates an
// absent Assets/ dir; returns false on any IO failure. NOT exercised by the
// in-memory ZM_Gen gate.
// ---------------------------------------------------------------------------
#ifdef ZENITH_TOOLS
bool ZM_GenBakeMesh(const ZM_GenMesh& xMesh, const char* szMeshPath,
	const char* szSkeletonPath, const char* szSkeletonRef);
#else
inline bool ZM_GenBakeMesh(const ZM_GenMesh&, const char*, const char*, const char*)
{
	return false;   // non-tools no-op keeps _False configs linking
}
#endif
