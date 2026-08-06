#pragma once

//=============================================================================
// Flux_GrassTypes — the pure half of the GPU-driven grass system.
//
// Everything here is a plain struct or a free function of its arguments: no
// engine singleton, no Flux runtime include, no device, no file IO. That is a
// hard constraint, not a preference — these are the definitions the placement
// compute shader, the vertex stage and the unit tests must all agree on, and a
// definition that can only be evaluated inside a booted renderer cannot be
// pinned by a headless test.
//
// Three kinds of thing live here:
//
//   1. GPU-MIRROR RECORDS (blade instance, indirect draw args, per-type params).
//      Their sizes are static_asserted because the shader-side declarations are
//      hand-written twins — a silent CPU-side size change reads every field
//      after the drift at the wrong offset and produces plausible garbage.
//
//   2. PURE DECISIONS the CPU and GPU both make (tile selection, lattice class,
//      fade bands, wind strength, map sampling). Stated once here so the two
//      sides cannot drift, and so the decision can be tested without a GPU.
//
//   3. The pipeline-variant selector — same shape as
//      Flux/Terrain/Flux_TerrainPipelineSelect.h.
//
// DETERMINISM. Blades are regenerated every frame and never persisted; all
// per-blade randomness keys off Zenith_TerrainNoise::HashCoords(latticeX,
// latticeZ, seed), so a blade is a function of its world position alone. Nothing
// here may introduce state, wall-clock reads, or float-order-dependent
// accumulation, or the same world position would grow different grass on two
// runs — and a blade that changes identity between frames flickers under TAA.
//=============================================================================

#include "Maths/Zenith_Maths.h"
#include "Maths/Zenith_Noise.h"
#include "Maths/Zenith_FrustumCulling.h"

#include <bit>   // std::bit_cast — the ONLY way to classify a float here; see below

//=============================================================================
// Bit packing. Every helper is pure and constexpr where the caller may need it
// in a default member initialiser.
//=============================================================================

// constexpr saturate — Zenith_Maths::Clamp is not usable in the default member
// initialisers below.
constexpr float Flux_GrassSaturate(float fValue)
{
	return fValue < 0.0f ? 0.0f : (fValue > 1.0f ? 1.0f : fValue);
}

// Is this float finite? INTEGER-ONLY, and it must stay that way.
//
// Every runtime FLOAT test for a NaN or an infinity — `f != f`, std::isnan,
// std::isfinite, a comparison against a huge magnitude — is folded to "it's fine"
// by the float optimizations this engine builds with: the compiler is permitted
// to assume those values never occur, so the test it is asked to emit is one it
// has already proved cannot fire. That is not a theoretical hazard; it is what
// let an authored NaN walk straight through Flux_GrassTypeTable::Validate.
// Arithmetic on the BIT PATTERN carries no such assumption.
//
// IEEE-754 binary32: an all-ones exponent field is a NaN (non-zero mantissa) or
// an infinity (zero mantissa); every other exponent is finite. Sign is
// irrelevant, so -inf classifies exactly like +inf.
constexpr u_int uFLUX_GRASS_FLOAT_EXPONENT_MASK = 0x7F800000u;

inline bool Flux_GrassIsFiniteFloat(float fValue)
{
	return (std::bit_cast<u_int>(fValue) & uFLUX_GRASS_FLOAT_EXPONENT_MASK) != uFLUX_GRASS_FLOAT_EXPONENT_MASK;
}

// packUnorm2x16 semantics, matching GLSL/Slang exactly: each component is
// saturated, scaled by 65535 and rounded to nearest; X occupies the low half.
constexpr u_int Flux_GrassPackUnorm16(float fValue)
{
	return static_cast<u_int>(Flux_GrassSaturate(fValue) * 65535.0f + 0.5f);
}

constexpr u_int Flux_GrassPackUnorm2x16(float fX, float fY)
{
	return Flux_GrassPackUnorm16(fX) | (Flux_GrassPackUnorm16(fY) << 16u);
}

constexpr float Flux_GrassUnpackUnorm2x16X(u_int uPacked)
{
	return static_cast<float>(uPacked & 0xFFFFu) * (1.0f / 65535.0f);
}

constexpr float Flux_GrassUnpackUnorm2x16Y(u_int uPacked)
{
	return static_cast<float>((uPacked >> 16u) & 0xFFFFu) * (1.0f / 65535.0f);
}

// The blade record's clumpPacked slot: the clump's own [0,1] hash in the low
// half, the blade's normalized distance to its clump centre in the high half.
constexpr u_int Flux_GrassPackClump(float fClumpHash01, float fDistToCentre01)
{
	return Flux_GrassPackUnorm2x16(fClumpHash01, fDistToCentre01);
}

constexpr float Flux_GrassUnpackClumpHash01(u_int uPacked) { return Flux_GrassUnpackUnorm2x16X(uPacked); }
constexpr float Flux_GrassUnpackClumpDist01(u_int uPacked) { return Flux_GrassUnpackUnorm2x16Y(uPacked); }

constexpr u_int Flux_GrassPackUnorm8(float fValue)
{
	return static_cast<u_int>(Flux_GrassSaturate(fValue) * 255.0f + 0.5f);
}

// RGB in the low three bytes (top byte reserved). Colours ride in one 32-bit
// slot because the per-type block is a pinned 144 bytes and 8-bit colour is
// below the noise floor of the per-blade tint jitter that is applied over it.
constexpr u_int Flux_GrassPackColourRGB(float fR, float fG, float fB)
{
	return Flux_GrassPackUnorm8(fR) | (Flux_GrassPackUnorm8(fG) << 8u) | (Flux_GrassPackUnorm8(fB) << 16u);
}

inline Zenith_Maths::Vector3 Flux_GrassUnpackColourRGB(u_int uPacked)
{
	return Zenith_Maths::Vector3(
		static_cast<float>((uPacked >> 0u) & 0xFFu) * (1.0f / 255.0f),
		static_cast<float>((uPacked >> 8u) & 0xFFu) * (1.0f / 255.0f),
		static_cast<float>((uPacked >> 16u) & 0xFFu) * (1.0f / 255.0f));
}

// The blade record's typeFlags slot. Bits 10-11 carry the blade's LATTICE CLASS:
// the vertex stage applies the class fade and cannot recover the class from posWS
// (the base is jittered and then pulled toward its clump centre), and the 64-byte
// record has no spare field. Bits 12-15 are reserved.
namespace Flux_GrassTypeFlags
{
	constexpr u_int uTYPE_INDEX_MASK    = 0x000000FFu;   // bits 0-7
	constexpr u_int uFOLDED_BIT         = 0x00000100u;   // bit 8
	constexpr u_int uLO_MESH_BIT        = 0x00000200u;   // bit 9
	constexpr u_int uLATTICE_CLASS_SHIFT = 10u;          // bits 10-11
	constexpr u_int uLATTICE_CLASS_MASK  = 0x00000003u;
	constexpr u_int uFS_HASH_SHIFT      = 16u;           // bits 16-31
	constexpr u_int uFS_HASH_MASK       = 0x0000FFFFu;
}

constexpr u_int Flux_GrassPackTypeFlags(u_int uTypeIndex, bool bFolded, bool bLOMesh, u_int uLatticeClass, u_int uFragmentHash)
{
	return (uTypeIndex & Flux_GrassTypeFlags::uTYPE_INDEX_MASK)
		| (bFolded ? Flux_GrassTypeFlags::uFOLDED_BIT : 0u)
		| (bLOMesh ? Flux_GrassTypeFlags::uLO_MESH_BIT : 0u)
		| ((uLatticeClass & Flux_GrassTypeFlags::uLATTICE_CLASS_MASK) << Flux_GrassTypeFlags::uLATTICE_CLASS_SHIFT)
		| ((uFragmentHash & Flux_GrassTypeFlags::uFS_HASH_MASK) << Flux_GrassTypeFlags::uFS_HASH_SHIFT);
}

constexpr u_int Flux_GrassTypeFlagsIndex(u_int uFlags)  { return uFlags & Flux_GrassTypeFlags::uTYPE_INDEX_MASK; }
constexpr bool  Flux_GrassTypeFlagsIsFolded(u_int uFlags) { return (uFlags & Flux_GrassTypeFlags::uFOLDED_BIT) != 0u; }
constexpr bool  Flux_GrassTypeFlagsIsLOMesh(u_int uFlags) { return (uFlags & Flux_GrassTypeFlags::uLO_MESH_BIT) != 0u; }
constexpr u_int Flux_GrassTypeFlagsLatticeClass(u_int uFlags)
{
	return (uFlags >> Flux_GrassTypeFlags::uLATTICE_CLASS_SHIFT) & Flux_GrassTypeFlags::uLATTICE_CLASS_MASK;
}
constexpr u_int Flux_GrassTypeFlagsFragmentHash(u_int uFlags)
{
	return (uFlags >> Flux_GrassTypeFlags::uFS_HASH_SHIFT) & Flux_GrassTypeFlags::uFS_HASH_MASK;
}

//=============================================================================
// GPU-mirror records.
//=============================================================================

// One blade: 16 x 32 bits. The three integer slots are declared as u_int here
// and may be declared float and asuint()-ed on the shader side — identical bits
// either way, so long as nothing does arithmetic on the float view.
//
// m_uHashBits is HashCoords(latticeX, latticeZ, seed) — the blade's identity.
// Everything else in the record is derivable from it plus the type params, and
// is precomputed only because the vertex stage runs per-vertex, not per-blade.
struct Flux_GrassBladeInstance
{
	Zenith_Maths::Vector3 m_xPosWS;           //   0 : world position of the blade base
	u_int                 m_uHashBits;        //  12 : lattice hash (blade identity)
	Zenith_Maths::Vector2 m_xFacingXZ;        //  16 : unit facing in world XZ
	float                 m_fHeight;          //  24
	float                 m_fWidth;           //  28
	float                 m_fTiltRad;         //  32 : lean away from vertical
	float                 m_fBend;            //  36 : quadratic bend along height
	float                 m_fSideCurve;       //  40 : lateral curl of the blade plane
	float                 m_fWindStrength;    //  44 : Flux_SampleWindStrength at the base
	Zenith_Maths::Vector2 m_xClumpNormalXZ;   //  48 : direction from the clump centre
	u_int                 m_uClumpPacked;     //  56 : Flux_GrassPackClump
	u_int                 m_uTypeFlags;       //  60 : Flux_GrassPackTypeFlags
};
static_assert(sizeof(Flux_GrassBladeInstance) == 64,
	"Flux_GrassBladeInstance is a hand-mirrored GPU record: 16 x 32-bit slots, 64 bytes");

// VkDrawIndexedIndirectCommand's five words. vertexOffset is int32 in the API
// struct; grass only ever writes non-negative offsets, and keeping all five
// unsigned lets the placement CS zero and atomically bump the block uniformly.
struct Flux_GrassDrawIndexedIndirectArgs
{
	u_int m_uIndexCount;
	u_int m_uInstanceCount;
	u_int m_uFirstIndex;
	u_int m_uVertexOffset;
	u_int m_uFirstInstance;
};
static_assert(sizeof(Flux_GrassDrawIndexedIndirectArgs) == 20,
	"indirect draw args must match VkDrawIndexedIndirectCommand's 5-word layout exactly");

// Per grass-type parameters, 36 x 32-bit scalars = 144 bytes (9 x 16, so an
// array of these is std140/std430-safe with no trailing pad). The size is the
// pinned contract: to add a field, repurpose or pack an existing slot rather
// than growing the block.
//
//   shape       0- 48  height/width/tilt/bend ranges, side curve, vertex
//                      distribution exponent, fold threshold + push-apart,
//                      tip width fraction
//   placement  52- 64  density, slope limit, draw distance, slope alignment
//   clump      68- 88  cell scale, height boost, facing/outward weights,
//                      pull-to-centre, normal blend cap
//   appearance 92-132  packed base/tip colour, three bindless indices, gloss
//                      repeat, roughness, specular, and three unorm16 PAIRS
//                      (colour jitter + normal tilt, translucency base + tip,
//                      AO base + tip-t) — pairs, not padding, is how the block
//                      lands on exactly 144
//   dynamics  136-140  wind response, stiffness
struct Flux_GrassTypeParamsGPU
{
	// --- shape ---
	float m_fHeightMin = 0.35f;
	float m_fHeightMax = 0.75f;
	float m_fWidthMin = 0.012f;
	float m_fWidthMax = 0.022f;
	float m_fTiltMinRad = 0.05f;
	float m_fTiltMaxRad = 0.35f;
	float m_fBendMin = 0.10f;
	float m_fBendMax = 0.35f;
	float m_fSideCurve = 0.15f;
	float m_fVertexDistributionPow = 1.6f;   // >1 packs vertices toward the tip
	float m_fFoldThreshold = 0.65f;          // blade hash above this folds along its spine
	float m_fFoldPushApart = 0.35f;
	float m_fTipWidthFrac = 0.15f;

	// --- placement ---
	float m_fDensity = 1.0f;                 // lattice-node acceptance probability [0,1]
	float m_fSlopeMax = 0.70f;               // sine of the steepest ground that grows grass
	float m_fMaxDrawDistance = 250.0f;
	float m_fSlopeAlign = 0.50f;             // 0 = always upright, 1 = follow the ground normal

	// --- clump ---
	float m_fClumpScale = 3.0f;              // metres per clump cell
	float m_fClumpHeightBoost = 0.25f;
	float m_fClumpFacingWeight = 0.60f;
	float m_fClumpOutwardWeight = 0.30f;
	float m_fClumpPullToCentre = 0.15f;
	float m_fClumpNormalBlendMax = 0.50f;

	// --- appearance ---
	u_int m_uBaseColourPacked = Flux_GrassPackColourRGB(0.15f, 0.28f, 0.09f);
	u_int m_uTipColourPacked = Flux_GrassPackColourRGB(0.42f, 0.55f, 0.18f);
	u_int m_uVeinTextureIndex = 0xFFFFFFFFu;   // bindless; 0xFFFFFFFF = unbound
	u_int m_uGlossTextureIndex = 0xFFFFFFFFu;
	u_int m_uRampTextureIndex = 0xFFFFFFFFu;
	float m_fGlossRepeat = 4.0f;
	float m_fRoughnessBase = 0.55f;
	float m_fSpecular = 0.35f;
	u_int m_uColourJitterNormalTiltPacked = Flux_GrassPackUnorm2x16(0.15f, 0.35f);  // jitter, tilt as a fraction of a quarter turn
	u_int m_uTranslucencyPacked = Flux_GrassPackUnorm2x16(0.15f, 0.65f);            // base, tip
	u_int m_uAOPacked = Flux_GrassPackUnorm2x16(0.25f, 0.75f);                      // base occlusion, height at which it releases

	// --- dynamics ---
	float m_fWindResponse = 1.0f;
	float m_fStiffness = 0.50f;
};
static_assert(sizeof(Flux_GrassTypeParamsGPU) == 144,
	"the per-type block is a pinned 144 bytes (36 x 32-bit scalars) — pack, never grow");

//=============================================================================
// Lattice, tiles and fade bands.
//=============================================================================

namespace Flux_GrassConfig
{
	// The placement lattice. A blade exists at a lattice node or it does not;
	// there is no jittered sample count, so the node's hash fully determines it.
	constexpr float fHI_LATTICE_STEP = 0.25f;
	constexpr u_int uLO_LATTICE_STRIDE = 2u;
	constexpr float fLO_LATTICE_STEP = fHI_LATTICE_STEP * static_cast<float>(uLO_LATTICE_STRIDE);

	// Tiles are the compute-dispatch unit. Both LODs are 64x64 nodes, so both
	// dispatch the same thread-group shape.
	constexpr float fHI_TILE_SIZE = 16.0f;
	constexpr float fLO_TILE_SIZE = 32.0f;
	constexpr u_int uTILE_CELLS = 64u;
	constexpr float fHI_RADIUS = 64.0f;

	constexpr float fDEFAULT_MAX_DISTANCE = 250.0f;
	constexpr float fMIN_MAX_DISTANCE = 50.0f;
	constexpr float fMAX_MAX_DISTANCE = 400.0f;

	// Hard cap on tiles selected per frame. Overflow keeps the nearest and is a
	// budget signal, not an error — the caller logs it.
	constexpr u_int uMAX_TILES = 256u;

	// Staggered lattice-class fade bands, as fractions of fHI_RADIUS, indexed by
	// lattice class. Class 0 is the (even,even) set that survives into the LO
	// lattice and NEVER fades — fading it would punch holes the LO tiles do not
	// fill. Classes 1 and 2 (the axis-interleaved nodes) go first, leaving a
	// half-density diagonal lattice; class 3 goes last. Equal widths and shifted
	// starts mean the three ramps are translates of each other, so density falls
	// monotonically and no two classes ever pop together.
	constexpr float afFADE_BAND_START[4] = { 0.0f, 0.70f, 0.74f, 0.80f };
	constexpr float afFADE_BAND_END[4] = { 0.0f, 0.82f, 0.86f, 0.92f };
}

static_assert(Flux_GrassConfig::fHI_TILE_SIZE / Flux_GrassConfig::fHI_LATTICE_STEP
	== static_cast<float>(Flux_GrassConfig::uTILE_CELLS),
	"a HI tile must be exactly uTILE_CELLS lattice cells across");
static_assert(Flux_GrassConfig::fLO_TILE_SIZE / Flux_GrassConfig::fLO_LATTICE_STEP
	== static_cast<float>(Flux_GrassConfig::uTILE_CELLS),
	"a LO tile must be exactly uTILE_CELLS lattice cells across — same dispatch shape as HI");
static_assert(Flux_GrassConfig::fLO_TILE_SIZE == Flux_GrassConfig::fHI_TILE_SIZE * 2.0f,
	"a LO tile must cover exactly four HI tiles, or the coverage test below is wrong");

//=============================================================================
// Blade mesh index table.
//
// There is NO vertex buffer: the blade draw is INDEXED, so SV_VertexID delivers
// the fetched index VALUE, which is the logical vertex id 0..14. The uint32 index
// buffer the host uploads must therefore be this table VERBATIM.
//
// This is a transcription of kauGRASS_INDEX_TABLE in
// Zenith/Flux/Shaders/Vegetation/Flux_GrassCommon.slang, which is the
// AUTHORITATIVE copy — as it is for the per-vertex height/side/piece tables
// mirrored below it. Entry order, the piece-A/piece-B split and the trailing LO
// strip must be diffed against that file, not re-derived: the first/count pairs
// below address ranges INSIDE this table, so a reordering here silently draws
// different triangles rather than failing to compile.
//=============================================================================

// One logical blade vertex, before any pose is built from it. Three fields, one
// per shader-side table (see axBLADE_VERTEX_TABLE below).
struct Flux_GrassBladeVertex
{
	float  m_fU;       // uniform height fraction, PRE-distribution remap
	float  m_fSide;    // -1 left edge, +1 right edge, 0 at the tip (which is on the spine)
	u_int8 m_uPiece;   // 0 = piece A (lower), 1 = piece B (upper — the half that folds)
};

namespace Flux_GrassConfig
{
	constexpr u_int uBLADE_VERTEX_COUNT = 15u;
	constexpr u_int uBLADE_INDEX_COUNT  = 48u;

	constexpr u_int uBLADE_HI_FIRST_INDEX = 0u;
	constexpr u_int uBLADE_HI_INDEX_COUNT = 33u;   // piece A (18) + piece B (15)
	constexpr u_int uBLADE_LO_FIRST_INDEX = 33u;
	constexpr u_int uBLADE_LO_INDEX_COUNT = 15u;   // 2 quads + tip, over a subset of the same verts

	// Per-vertex record. Transcription of kafGRASS_VERT_U / kafGRASS_VERT_SIDE /
	// kauGRASS_VERT_PIECE in Flux_GrassCommon.slang (AUTHORITATIVE — one array per
	// field there, one array of records here; the CONTENT is what must be diffed).
	//
	// m_fU is the UNIFORM height fraction, before the per-type distribution exponent
	// remaps it (Flux_GrassRemapHeightT). Piece A covers 0 -> 0.75 in three equal
	// steps and piece B covers 0.75 -> 1 in three more, so the upper quarter carries
	// as many rows as the lower three quarters.
	//
	// Vertices 6/7 (piece A's top edge) and 8/9 (piece B's seam row) sit at the SAME
	// u and the SAME side: unfolded they COINCIDE and the two pieces weld into one
	// surface. Folding rigid-offsets piece B alone, which is what separates them —
	// see Flux_GrassWeldOffset.
	constexpr Flux_GrassBladeVertex axBLADE_VERTEX_TABLE[uBLADE_VERTEX_COUNT] =
	{
		// --- piece A: 4 rows x 2 columns ---
		{ 0.0f,        -1.0f, 0u }, { 0.0f,        1.0f, 0u },   // row 0 (base)
		{ 0.25f,       -1.0f, 0u }, { 0.25f,       1.0f, 0u },   // row 1
		{ 0.5f,        -1.0f, 0u }, { 0.5f,        1.0f, 0u },   // row 2
		{ 0.75f,       -1.0f, 0u }, { 0.75f,       1.0f, 0u },   // row 3 (A's top edge)
		// --- piece B: 3 rows x 2 columns + the tip ---
		{ 0.75f,       -1.0f, 1u }, { 0.75f,       1.0f, 1u },   // seam row (coincides with row 3)
		{ 0.83333333f, -1.0f, 1u }, { 0.83333333f, 1.0f, 1u },   // row 1
		{ 0.91666667f, -1.0f, 1u }, { 0.91666667f, 1.0f, 1u },   // row 2
		{ 1.0f,         0.0f, 1u },                              // tip, on the spine
	};

	constexpr u_int auBLADE_INDEX_TABLE[uBLADE_INDEX_COUNT] =
	{
		// --- piece A, firstIndex 0, 18 indices ---
		 0u,  2u,  1u,    1u,  2u,  3u,
		 2u,  4u,  3u,    3u,  4u,  5u,
		 4u,  6u,  5u,    5u,  6u,  7u,
		// --- piece B, continues the HI range to 33 indices ---
		 8u, 10u,  9u,    9u, 10u, 11u,
		10u, 12u, 11u,   11u, 12u, 13u,
		12u, 14u, 13u,
		// --- LO strip, firstIndex 33, 15 indices ---
		 0u,  4u,  1u,    1u,  4u,  5u,
		 4u,  8u,  5u,    5u,  8u,  9u,
		 8u, 14u,  9u,
	};
}

static_assert(Flux_GrassConfig::uBLADE_HI_FIRST_INDEX + Flux_GrassConfig::uBLADE_HI_INDEX_COUNT
	== Flux_GrassConfig::uBLADE_LO_FIRST_INDEX,
	"the LO strip must start exactly where the HI range ends — the two ranges tile the table");
static_assert(Flux_GrassConfig::uBLADE_LO_FIRST_INDEX + Flux_GrassConfig::uBLADE_LO_INDEX_COUNT
	== Flux_GrassConfig::uBLADE_INDEX_COUNT,
	"the HI + LO ranges must consume the whole index table");
static_assert(sizeof(Flux_GrassConfig::auBLADE_INDEX_TABLE) / sizeof(u_int) == Flux_GrassConfig::uBLADE_INDEX_COUNT,
	"the mirrored index table must be exactly uBLADE_INDEX_COUNT entries");

// A transcription typo that lands out of range would index past the vertex tables
// on the GPU, which reads as garbage geometry rather than a crash.
constexpr bool Flux_GrassBladeIndexTableInRange()
{
	for (u_int u = 0; u < Flux_GrassConfig::uBLADE_INDEX_COUNT; u++)
	{
		if (Flux_GrassConfig::auBLADE_INDEX_TABLE[u] >= Flux_GrassConfig::uBLADE_VERTEX_COUNT)
		{
			return false;
		}
	}
	return true;
}
static_assert(Flux_GrassBladeIndexTableInRange(),
	"every blade index must address one of the uBLADE_VERTEX_COUNT logical vertices");

// A transcribed u outside [0,1], a side that is not an edge or the spine, or a
// piece id above 1 all still compile and all still pose a blade — just the wrong
// one, everywhere, forever.
constexpr bool Flux_GrassBladeVertexTableIsWellFormed()
{
	for (u_int u = 0; u < Flux_GrassConfig::uBLADE_VERTEX_COUNT; u++)
	{
		const Flux_GrassBladeVertex& xVertex = Flux_GrassConfig::axBLADE_VERTEX_TABLE[u];
		if (xVertex.m_fU < 0.0f || xVertex.m_fU > 1.0f)
		{
			return false;
		}
		if (xVertex.m_fSide != -1.0f && xVertex.m_fSide != 1.0f && xVertex.m_fSide != 0.0f)
		{
			return false;
		}
		if (xVertex.m_uPiece > 1u)
		{
			return false;
		}
	}
	return true;
}
static_assert(Flux_GrassBladeVertexTableIsWellFormed(),
	"the mirrored vertex table must stay in [0,1] height, +/-1 or 0 side, piece 0 or 1");

//=============================================================================
// Blade pose. CPU mirror of Flux_GrassBuildBladeVertex in
// Zenith/Flux/Shaders/Vegetation/Flux_GrassCommon.slang, which is AUTHORITATIVE:
// the functions below are transcribed from it statement for statement so the two
// can be diffed. Float agreement across the two languages is NOT claimed (FMA
// contraction and fast-math modes differ per target); the FORMULAS are.
//
// The pose is factored the same way the shader factors it — frame, then control
// points, then the curve — because that is the only split at which the CPU can
// evaluate it without a view matrix. Everything downstream of the curve in the
// shader (screen-space thickening, the normal blend) needs a projection and is
// deliberately NOT mirrored here.
//=============================================================================

// Uniform row parameter -> height fraction. An exponent > 1 shortens the steps
// near the tip, so the curvature the Bezier puts there is actually tessellated.
inline float Flux_GrassRemapHeightT(float fU, float fDistributionPow)
{
	return powf(Flux_GrassSaturate(fU), 1.0f / glm::max(fDistributionPow, 0.01f));
}

inline Zenith_Maths::Vector3 Flux_GrassBezierPoint(const Zenith_Maths::Vector3& xP0, const Zenith_Maths::Vector3& xP1,
	const Zenith_Maths::Vector3& xP2, const Zenith_Maths::Vector3& xP3, float fT)
{
	const float fInv = 1.0f - fT;
	return fInv * fInv * fInv * xP0
		+ 3.0f * fInv * fInv * fT * xP1
		+ 3.0f * fInv * fT * fT * xP2
		+ fT * fT * fT * xP3;
}

inline Zenith_Maths::Vector3 Flux_GrassBezierTangent(const Zenith_Maths::Vector3& xP0, const Zenith_Maths::Vector3& xP1,
	const Zenith_Maths::Vector3& xP2, const Zenith_Maths::Vector3& xP3, float fT)
{
	const float fInv = 1.0f - fT;
	return 3.0f * fInv * fInv * (xP1 - xP0)
		+ 6.0f * fInv * fT * (xP2 - xP1)
		+ 3.0f * fT * fT * (xP3 - xP2);
}

// The blade's local frame: facing is the direction it leans and bends toward,
// side is the width axis, grow is the root growth direction after tilt.
struct Flux_GrassBladeFrame
{
	Zenith_Maths::Vector3 m_xFacing{ 0.0f, 0.0f, 1.0f };
	Zenith_Maths::Vector3 m_xSide{ 1.0f, 0.0f, 0.0f };
	Zenith_Maths::Vector3 m_xGrow{ 0.0f, 1.0f, 0.0f };
};

inline Flux_GrassBladeFrame Flux_GrassMakeBladeFrame(const Zenith_Maths::Vector2& xFacingXZ, float fTiltRad)
{
	// The 1e-5 nudge is the shader's, not a CPU-side guard: a zero facing would
	// normalize to NaN and the two sides must degenerate identically.
	const Zenith_Maths::Vector2 xFacing2 = glm::normalize(xFacingXZ + Zenith_Maths::Vector2(1.0e-5f, 0.0f));
	const Zenith_Maths::Vector3 xUp(0.0f, 1.0f, 0.0f);

	Flux_GrassBladeFrame xFrame;
	xFrame.m_xFacing = Zenith_Maths::Vector3(xFacing2.x, 0.0f, xFacing2.y);
	xFrame.m_xSide = glm::normalize(glm::cross(xUp, xFrame.m_xFacing));
	xFrame.m_xGrow = glm::normalize(xUp * cosf(fTiltRad) + xFrame.m_xFacing * sinf(fTiltRad));
	return xFrame;
}

// The four cubic control points the spine is swept along.
struct Flux_GrassBladeCurve
{
	Zenith_Maths::Vector3 m_xP0{ 0.0f };
	Zenith_Maths::Vector3 m_xP1{ 0.0f };
	Zenith_Maths::Vector3 m_xP2{ 0.0f };
	Zenith_Maths::Vector3 m_xP3{ 0.0f };
};

// xWindTip is the tip deflection in METRES (the shader's windDir * windGain, where
// the gain already carries the type's wind response and stiffness). It enters P2 at
// 4/9 = (2/3)^2 — the deflection weighted by height squared, which is what keeps the
// blade's base still while its tip travels.
inline Flux_GrassBladeCurve Flux_GrassBuildBladeCurve(const Zenith_Maths::Vector3& xRoot,
	const Flux_GrassBladeFrame& xFrame, float fHeight, float fBend, float fSideCurve,
	const Zenith_Maths::Vector3& xWindTip)
{
	Flux_GrassBladeCurve xCurve;
	xCurve.m_xP0 = xRoot;
	xCurve.m_xP1 = xRoot + xFrame.m_xGrow * (fHeight * (1.0f / 3.0f));
	xCurve.m_xP2 = xRoot + xFrame.m_xGrow * (fHeight * (2.0f / 3.0f))
		+ xFrame.m_xFacing * (fBend * fHeight)
		+ xFrame.m_xSide * (fSideCurve * fHeight)
		+ xWindTip * (4.0f / 9.0f);
	xCurve.m_xP3 = xRoot + xFrame.m_xGrow * fHeight + xWindTip;
	return xCurve;
}

// The rigid FOLD offset, in the blade's own frame: x = metres along
// Flux_GrassBladeFrame::m_xFacing, y = metres along world +Y. Piece A never moves,
// which is exactly why an unfolded blade welds at the seam and a folded one breaks
// there instead of shearing.
struct Flux_GrassFoldOffset
{
	float m_fAlongFacing = 0.0f;
	float m_fAlongUp = 0.0f;
};

// Mirror of the fold branch in Flux_GrassBuildBladeVertex (Flux_GrassCommon.slang,
// AUTHORITATIVE). fPushApart is the ALREADY-SCALED push the shader computes —
// type push-apart * height * (1 - LOD blend) — so this stays a pure function of the
// vertex id and the fold bit.
inline Flux_GrassFoldOffset Flux_GrassWeldOffset(u_int uVertexId, bool bFolded, float fPushApart)
{
	Flux_GrassFoldOffset xOffset;
	const u_int uSafe = uVertexId < Flux_GrassConfig::uBLADE_VERTEX_COUNT
		? uVertexId : Flux_GrassConfig::uBLADE_VERTEX_COUNT - 1u;
	if (!bFolded || Flux_GrassConfig::axBLADE_VERTEX_TABLE[uSafe].m_uPiece == 0u)
	{
		return xOffset;
	}
	xOffset.m_fAlongFacing = fPushApart;
	xOffset.m_fAlongUp = -fPushApart * 0.5f;
	return xOffset;
}

//=============================================================================
// Clump Voronoi. CPU mirror of Flux_GrassFindClump in
// Zenith/Flux/Shaders/Vegetation/Flux_Grass_Placement.slang, which is
// AUTHORITATIVE.
//
// The site JITTER is integer-hash-derived, so the nine candidate positions are
// bit-exact across the two languages. The nearest-site choice is a float distance
// compare and is mirrored STRUCTURALLY: the same nine cells, visited in the same
// order (dz outer, dx inner), with the same strict-less test — so a tie keeps the
// FIRST cell visited on both sides.
//=============================================================================

namespace Flux_GrassConfig
{
	// The clump lattice must not collide with the blade lattice, which keys off the
	// bare seed. Mirrors the `uSeed + 7717u` in Flux_GrassFindClump.
	constexpr u_int uCLUMP_SEED_OFFSET = 7717u;
	// Mirrors the `uHash + 5431u` in Flux_GrassPickType.
	constexpr u_int uTYPE_DITHER_SALT = 5431u;
	// A zero cell size would divide the world position by zero and place every blade
	// in one clump at infinity.
	constexpr float fMIN_CLUMP_SCALE = 0.01f;
}

struct Flux_GrassClumpCell
{
	Zenith_Maths::Vector2 m_xSite{ 0.0f, 0.0f };
	u_int m_uHash = 0u;
};

inline Flux_GrassClumpCell Flux_GrassClumpSite(int iCellX, int iCellZ, float fScale, u_int uSeed)
{
	Flux_GrassClumpCell xCell;
	xCell.m_uHash = Zenith_TerrainNoise::HashCoords(iCellX, iCellZ, uSeed + Flux_GrassConfig::uCLUMP_SEED_OFFSET);
	const Zenith_Maths::Vector2 xJitter(Zenith_TerrainNoise::HashToFloat01(xCell.m_uHash),
		Zenith_TerrainNoise::HashToFloat01(Zenith_TerrainNoise::HashUInt(xCell.m_uHash)));
	xCell.m_xSite = (Zenith_Maths::Vector2(static_cast<float>(iCellX), static_cast<float>(iCellZ)) + xJitter) * fScale;
	return xCell;
}

struct Flux_GrassClump
{
	Zenith_Maths::Vector2 m_xCentre{ 0.0f, 0.0f };
	Zenith_Maths::Vector2 m_xNormalXZ{ 1.0f, 0.0f };   // unit, centre -> blade
	float m_fDist01 = 0.0f;                            // distance to the centre over the cell size
	float m_fHash01 = 0.0f;
};

// Nine cells is the MINIMUM that guarantees the true nearest site for any jitter
// inside a cell; a 2x2 search misses it whenever the blade sits near a corner and
// produces visible clump seams along the cell grid.
inline Flux_GrassClump Flux_GrassClumpPick(float fWorldX, float fWorldZ, float fClumpScale, u_int uSeed)
{
	const float fScale = glm::max(fClumpScale, Flux_GrassConfig::fMIN_CLUMP_SCALE);
	const Zenith_Maths::Vector2 xWorld(fWorldX, fWorldZ);
	const int iCellX = static_cast<int>(floorf(fWorldX / fScale));
	const int iCellZ = static_cast<int>(floorf(fWorldZ / fScale));

	Flux_GrassClump xClump;
	xClump.m_xCentre = xWorld;
	float fBestDistSq = 1.0e30f;
	u_int uBestHash = 0u;
	for (int iDZ = -1; iDZ <= 1; iDZ++)
	{
		for (int iDX = -1; iDX <= 1; iDX++)
		{
			const Flux_GrassClumpCell xCell = Flux_GrassClumpSite(iCellX + iDX, iCellZ + iDZ, fScale, uSeed);
			const Zenith_Maths::Vector2 xDelta = xWorld - xCell.m_xSite;
			const float fDistSq = glm::dot(xDelta, xDelta);
			if (fDistSq < fBestDistSq)
			{
				fBestDistSq = fDistSq;
				xClump.m_xCentre = xCell.m_xSite;
				uBestHash = xCell.m_uHash;
			}
		}
	}

	const Zenith_Maths::Vector2 xOut = xWorld - xClump.m_xCentre;
	const float fLength = glm::length(xOut);
	xClump.m_xNormalXZ = fLength > 1.0e-5f ? (xOut / fLength) : Zenith_Maths::Vector2(1.0f, 0.0f);
	xClump.m_fDist01 = Flux_GrassSaturate(fLength / fScale);
	xClump.m_fHash01 = Zenith_TerrainNoise::HashToFloat01(uBestHash);
	return xClump;
}

// The blade's lattice class. The LO lattice samples every uLO_LATTICE_STRIDE-th
// node on both axes, so class 0 — and only class 0 — is the set a LO tile
// reproduces from identical inputs. That is what makes the HI->LO transition a
// fade rather than a reshuffle.
constexpr u_int Flux_GrassLatticeClass(int iLatticeX, int iLatticeZ)
{
	return (static_cast<u_int>(iLatticeX) & 1u) | ((static_cast<u_int>(iLatticeZ) & 1u) << 1u);
}

// Presence weight of a lattice class at a given camera distance: 1 = full size,
// 0 = gone. Drives BOTH the vertex stage's shrink/sink and the CS's cull, which
// is why it is stated once here.
inline float Flux_GrassLatticeFade(u_int uClass, float fDistance, float fHiRadius)
{
	if (uClass == 0u || uClass > 3u)
	{
		return 1.0f;
	}
	const float fStart = Flux_GrassConfig::afFADE_BAND_START[uClass] * fHiRadius;
	const float fEnd = Flux_GrassConfig::afFADE_BAND_END[uClass] * fHiRadius;
	if (fDistance <= fStart)
	{
		return 1.0f;
	}
	if (fDistance >= fEnd)
	{
		return 0.0f;
	}
	return 1.0f - (fDistance - fStart) / (fEnd - fStart);
}

enum class Flux_GrassTileLOD : u_int8
{
	HI = 0,   // 16 m tile, 0.25 m lattice
	LO,       // 32 m tile, 0.50 m lattice (the class-0 survivors)
	COUNT
};

constexpr float Flux_GrassTileSize(Flux_GrassTileLOD eLOD)
{
	return eLOD == Flux_GrassTileLOD::HI ? Flux_GrassConfig::fHI_TILE_SIZE : Flux_GrassConfig::fLO_TILE_SIZE;
}

constexpr float Flux_GrassLatticeStep(Flux_GrassTileLOD eLOD)
{
	return eLOD == Flux_GrassTileLOD::HI ? Flux_GrassConfig::fHI_LATTICE_STEP : Flux_GrassConfig::fLO_LATTICE_STEP;
}

struct Flux_GrassTile
{
	float m_fWorldMinX = 0.0f;
	float m_fWorldMinZ = 0.0f;
	float m_fSize = 0.0f;
	float m_fDistanceSq = 0.0f;   // camera to the tile's NEAREST point, XZ, squared
	int m_iTileX = 0;             // tile-grid coordinate: floor(world / m_fSize)
	int m_iTileZ = 0;
	Flux_GrassTileLOD m_eLOD = Flux_GrassTileLOD::HI;
};

// Fixed-capacity output. Kept sorted nearest-first at all times, so truncation
// at the cap IS "keep the nearest" and the caller can walk it in draw order.
struct Flux_GrassTileList
{
	Flux_GrassTile m_axTiles[Flux_GrassConfig::uMAX_TILES];
	u_int m_uCount = 0;
	u_int m_uConsidered = 0;    // survived culling BEFORE the cap
	bool m_bOverflowed = false; // m_uConsidered > m_uCount: tiles were dropped

	void Clear()
	{
		m_uCount = 0;
		m_uConsidered = 0;
		m_bOverflowed = false;
	}

	const Flux_GrassTile& Get(u_int uIndex) const { return m_axTiles[uIndex]; }
};

// Per-tile vertical bounds. Grass is culled by an AABB, and a flat AABB over
// hilly ground culls tiles that are plainly visible, so the caller supplies a
// coarse min/max height grid (row-major, cell (0,0) at the origin corner).
// An absent or degenerate grid falls back to the fixed band.
struct Flux_GrassHeightGrid
{
	const float* m_pfMinY = nullptr;
	const float* m_pfMaxY = nullptr;
	u_int m_uCellsX = 0;
	u_int m_uCellsZ = 0;
	float m_fCellSize = 0.0f;
	float m_fOriginX = 0.0f;
	float m_fOriginZ = 0.0f;
	float m_fFallbackMinY = 0.0f;
	float m_fFallbackMaxY = 0.0f;

	bool IsValid() const
	{
		return m_pfMinY != nullptr && m_pfMaxY != nullptr && m_uCellsX > 0u && m_uCellsZ > 0u && m_fCellSize > 0.0f;
	}
};

// The world rectangle grass may occupy. Degenerate extents mean "unbounded" —
// a caller that has not loaded a map yet must not silently get zero tiles.
struct Flux_GrassMapExtents
{
	float m_fMinX = 0.0f;
	float m_fMinZ = 0.0f;
	float m_fMaxX = 0.0f;
	float m_fMaxZ = 0.0f;

	bool IsValid() const { return m_fMaxX > m_fMinX && m_fMaxZ > m_fMinZ; }
};

// Camera frustum plus every shadow cascade. A tile survives if ANY of them
// wants it — a tile behind the camera still has to fill the cascades it casts
// into. A zero count disables culling entirely.
struct Flux_GrassCullFrusta
{
	const Zenith_Frustum* m_pxFrusta = nullptr;
	u_int m_uCount = 0;
};

struct Flux_GrassTileSelectParams
{
	Zenith_Maths::Vector3 m_xCameraPos{ 0.0f, 0.0f, 0.0f };
	float m_fMaxDistance = Flux_GrassConfig::fDEFAULT_MAX_DISTANCE;
	float m_fBladeHeadroom = 1.0f;   // added to the tile AABB's max Y: blades stand ON the ground
	Flux_GrassMapExtents m_xExtents;
	Flux_GrassHeightGrid m_xHeights;
	Flux_GrassCullFrusta m_xFrusta;
};

inline float Flux_GrassClampMaxDistance(float fRequested)
{
	return Zenith_Maths::Clamp(fRequested, Flux_GrassConfig::fMIN_MAX_DISTANCE, Flux_GrassConfig::fMAX_MAX_DISTANCE);
}

inline int Flux_GrassFloorToTile(float fWorld, float fTileSize)
{
	return static_cast<int>(floorf(fWorld / fTileSize));
}

// Squared XZ distance from a point to the nearest point of an axis-aligned
// rectangle; zero when the point is inside it.
inline float Flux_GrassRectNearestDistSqXZ(float fPX, float fPZ, float fMinX, float fMinZ, float fMaxX, float fMaxZ)
{
	const float fDX = fPX < fMinX ? (fMinX - fPX) : (fPX > fMaxX ? (fPX - fMaxX) : 0.0f);
	const float fDZ = fPZ < fMinZ ? (fMinZ - fPZ) : (fPZ > fMaxZ ? (fPZ - fMaxZ) : 0.0f);
	return fDX * fDX + fDZ * fDZ;
}

// Is this world position inside the HI region? PURELY geometric — it must not
// consult culling or the tile cap, because the placement CS applies the same
// test per lattice node to skip nodes a HI tile already emitted. If the two
// answers could differ, the seam would either double-draw or gap.
inline bool Flux_GrassIsInsideHiRegion(const Zenith_Maths::Vector3& xCameraPos, float fWorldX, float fWorldZ)
{
	const float fDX = fWorldX - xCameraPos.x;
	const float fDZ = fWorldZ - xCameraPos.z;
	return (fDX * fDX + fDZ * fDZ) <= (Flux_GrassConfig::fHI_RADIUS * Flux_GrassConfig::fHI_RADIUS);
}

// A HI tile is wanted when any part of it is within the HI radius.
inline bool Flux_GrassHiTileIsWanted(const Zenith_Maths::Vector3& xCameraPos, int iTileX, int iTileZ)
{
	const float fSize = Flux_GrassConfig::fHI_TILE_SIZE;
	const float fMinX = static_cast<float>(iTileX) * fSize;
	const float fMinZ = static_cast<float>(iTileZ) * fSize;
	const float fDistSq = Flux_GrassRectNearestDistSqXZ(xCameraPos.x, xCameraPos.z,
		fMinX, fMinZ, fMinX + fSize, fMinZ + fSize);
	return fDistSq <= (Flux_GrassConfig::fHI_RADIUS * Flux_GrassConfig::fHI_RADIUS);
}

// A LO tile covers exactly the four HI tiles (2x,2z)..(2x+1,2z+1). Drop it only
// when all four are already being drawn at HI; a partially-covered LO tile is
// kept and the CS skips its nodes inside the HI region.
inline bool Flux_GrassLoTileIsFullyCoveredByHi(const Zenith_Maths::Vector3& xCameraPos, int iTileX, int iTileZ)
{
	for (int iDZ = 0; iDZ < 2; iDZ++)
	{
		for (int iDX = 0; iDX < 2; iDX++)
		{
			if (!Flux_GrassHiTileIsWanted(xCameraPos, iTileX * 2 + iDX, iTileZ * 2 + iDZ))
			{
				return false;
			}
		}
	}
	return true;
}

inline void Flux_GrassHeightBandForRect(const Flux_GrassHeightGrid& xGrid,
	float fMinX, float fMinZ, float fMaxX, float fMaxZ, float& fOutMinY, float& fOutMaxY)
{
	fOutMinY = xGrid.m_fFallbackMinY;
	fOutMaxY = xGrid.m_fFallbackMaxY;
	if (!xGrid.IsValid())
	{
		return;
	}

	const int iLastX = static_cast<int>(xGrid.m_uCellsX) - 1;
	const int iLastZ = static_cast<int>(xGrid.m_uCellsZ) - 1;
	const int iX0 = Zenith_Maths::Clamp(Flux_GrassFloorToTile(fMinX - xGrid.m_fOriginX, xGrid.m_fCellSize), 0, iLastX);
	const int iX1 = Zenith_Maths::Clamp(Flux_GrassFloorToTile(fMaxX - xGrid.m_fOriginX, xGrid.m_fCellSize), 0, iLastX);
	const int iZ0 = Zenith_Maths::Clamp(Flux_GrassFloorToTile(fMinZ - xGrid.m_fOriginZ, xGrid.m_fCellSize), 0, iLastZ);
	const int iZ1 = Zenith_Maths::Clamp(Flux_GrassFloorToTile(fMaxZ - xGrid.m_fOriginZ, xGrid.m_fCellSize), 0, iLastZ);

	fOutMinY = FLT_MAX;
	fOutMaxY = -FLT_MAX;
	for (int iZ = iZ0; iZ <= iZ1; iZ++)
	{
		for (int iX = iX0; iX <= iX1; iX++)
		{
			const u_int uIndex = static_cast<u_int>(iZ) * xGrid.m_uCellsX + static_cast<u_int>(iX);
			fOutMinY = glm::min(fOutMinY, xGrid.m_pfMinY[uIndex]);
			fOutMaxY = glm::max(fOutMaxY, xGrid.m_pfMaxY[uIndex]);
		}
	}
}

inline Zenith_AABB Flux_GrassTileAABB(const Flux_GrassTileSelectParams& xParams, const Flux_GrassTile& xTile)
{
	const float fMaxX = xTile.m_fWorldMinX + xTile.m_fSize;
	const float fMaxZ = xTile.m_fWorldMinZ + xTile.m_fSize;
	float fMinY = 0.0f;
	float fMaxY = 0.0f;
	Flux_GrassHeightBandForRect(xParams.m_xHeights, xTile.m_fWorldMinX, xTile.m_fWorldMinZ, fMaxX, fMaxZ, fMinY, fMaxY);
	return Zenith_AABB(
		Zenith_Maths::Vector3(xTile.m_fWorldMinX, fMinY, xTile.m_fWorldMinZ),
		Zenith_Maths::Vector3(fMaxX, fMaxY + xParams.m_fBladeHeadroom, fMaxZ));
}

inline bool Flux_GrassTilePassesFrusta(const Flux_GrassCullFrusta& xFrusta, const Zenith_AABB& xAABB)
{
	if (xFrusta.m_pxFrusta == nullptr || xFrusta.m_uCount == 0u)
	{
		return true;
	}
	for (u_int u = 0; u < xFrusta.m_uCount; u++)
	{
		if (Zenith_FrustumCulling::TestAABBFrustum(xFrusta.m_pxFrusta[u], xAABB))
		{
			return true;
		}
	}
	return false;
}

inline bool Flux_GrassRectOverlapsExtents(const Flux_GrassMapExtents& xExtents,
	float fMinX, float fMinZ, float fMaxX, float fMaxZ)
{
	if (!xExtents.IsValid())
	{
		return true;
	}
	return fMaxX > xExtents.m_fMinX && fMinX < xExtents.m_fMaxX
		&& fMaxZ > xExtents.m_fMinZ && fMinZ < xExtents.m_fMaxZ;
}

// Strict total order over tiles: nearest first, then coordinates. The tie-break
// is what makes the kept set independent of visit order — two runs that walk
// the rings differently still keep the same 256 tiles.
inline bool Flux_GrassTileOrderLess(const Flux_GrassTile& xA, const Flux_GrassTile& xB)
{
	if (xA.m_fDistanceSq != xB.m_fDistanceSq) return xA.m_fDistanceSq < xB.m_fDistanceSq;
	if (xA.m_eLOD != xB.m_eLOD)               return xA.m_eLOD < xB.m_eLOD;
	if (xA.m_iTileZ != xB.m_iTileZ)           return xA.m_iTileZ < xB.m_iTileZ;
	return xA.m_iTileX < xB.m_iTileX;
}

// Insertion into the sorted, capped list. A full list drops the candidate when
// it is not nearer than the current farthest entry, and evicts that entry when
// it is — which is what makes truncation "keep the nearest". The drop is not
// reported here: the caller learns about it from m_uConsidered vs m_uCount,
// which counts every drop including the evictions.
inline void Flux_GrassTileListInsert(Flux_GrassTileList& xList, const Flux_GrassTile& xTile)
{
	const bool bFull = xList.m_uCount >= Flux_GrassConfig::uMAX_TILES;
	if (bFull && !Flux_GrassTileOrderLess(xTile, xList.m_axTiles[Flux_GrassConfig::uMAX_TILES - 1u]))
	{
		return;
	}

	u_int uSlot = bFull ? (Flux_GrassConfig::uMAX_TILES - 1u) : xList.m_uCount;
	while (uSlot > 0u && Flux_GrassTileOrderLess(xTile, xList.m_axTiles[uSlot - 1u]))
	{
		xList.m_axTiles[uSlot] = xList.m_axTiles[uSlot - 1u];
		uSlot--;
	}
	xList.m_axTiles[uSlot] = xTile;
	if (!bFull)
	{
		xList.m_uCount++;
	}
}

inline void Flux_GrassConsiderTile(const Flux_GrassTileSelectParams& xParams, Flux_GrassTileLOD eLOD,
	float fRadius, int iTileX, int iTileZ, Flux_GrassTileList& xOut)
{
	Flux_GrassTile xTile;
	xTile.m_eLOD = eLOD;
	xTile.m_fSize = Flux_GrassTileSize(eLOD);
	xTile.m_iTileX = iTileX;
	xTile.m_iTileZ = iTileZ;
	xTile.m_fWorldMinX = static_cast<float>(iTileX) * xTile.m_fSize;
	xTile.m_fWorldMinZ = static_cast<float>(iTileZ) * xTile.m_fSize;

	const float fMaxX = xTile.m_fWorldMinX + xTile.m_fSize;
	const float fMaxZ = xTile.m_fWorldMinZ + xTile.m_fSize;
	xTile.m_fDistanceSq = Flux_GrassRectNearestDistSqXZ(xParams.m_xCameraPos.x, xParams.m_xCameraPos.z,
		xTile.m_fWorldMinX, xTile.m_fWorldMinZ, fMaxX, fMaxZ);

	if (xTile.m_fDistanceSq > fRadius * fRadius)
	{
		return;
	}
	if (eLOD == Flux_GrassTileLOD::LO && Flux_GrassLoTileIsFullyCoveredByHi(xParams.m_xCameraPos, iTileX, iTileZ))
	{
		return;
	}
	if (!Flux_GrassRectOverlapsExtents(xParams.m_xExtents, xTile.m_fWorldMinX, xTile.m_fWorldMinZ, fMaxX, fMaxZ))
	{
		return;
	}
	if (!Flux_GrassTilePassesFrusta(xParams.m_xFrusta, Flux_GrassTileAABB(xParams, xTile)))
	{
		return;
	}

	xOut.m_uConsidered++;
	Flux_GrassTileListInsert(xOut, xTile);
}

// Rings outward from the camera's own tile. Ring r's nearest point is at least
// (r-1) tiles away, so floor(radius / tileSize) + 1 rings is exactly enough.
inline void Flux_GrassAppendLODRings(const Flux_GrassTileSelectParams& xParams, Flux_GrassTileLOD eLOD,
	float fRadius, Flux_GrassTileList& xOut)
{
	const float fTileSize = Flux_GrassTileSize(eLOD);
	const int iCamX = Flux_GrassFloorToTile(xParams.m_xCameraPos.x, fTileSize);
	const int iCamZ = Flux_GrassFloorToTile(xParams.m_xCameraPos.z, fTileSize);
	const int iMaxRing = static_cast<int>(fRadius / fTileSize) + 1;

	for (int iRing = 0; iRing <= iMaxRing; iRing++)
	{
		for (int iDZ = -iRing; iDZ <= iRing; iDZ++)
		{
			const bool bPerimeterRow = (iDZ == -iRing || iDZ == iRing);
			for (int iDX = -iRing; iDX <= iRing; iDX++)
			{
				if (!bPerimeterRow && iDX != -iRing && iDX != iRing)
				{
					continue;   // interior of the ring belongs to a smaller ring
				}
				Flux_GrassConsiderTile(xParams, eLOD, fRadius, iCamX + iDX, iCamZ + iDZ, xOut);
			}
		}
	}
}

// THE tile scheduler. Pure: identical inputs give a byte-identical list,
// including which tiles the cap dropped.
inline void Flux_GrassSelectTiles(const Flux_GrassTileSelectParams& xParams, Flux_GrassTileList& xOut)
{
	xOut.Clear();
	Flux_GrassAppendLODRings(xParams, Flux_GrassTileLOD::HI, Flux_GrassConfig::fHI_RADIUS, xOut);
	Flux_GrassAppendLODRings(xParams, Flux_GrassTileLOD::LO, Flux_GrassClampMaxDistance(xParams.m_fMaxDistance), xOut);
	xOut.m_bOverflowed = xOut.m_uConsidered > xOut.m_uCount;
}

//=============================================================================
// Displacement field geometry — the camera-anchored trail map.
//
// A fixed 256^2 RG16F map covering 64 m, so one texel is 0.25 m: exactly one HI
// lattice step, which is the finest push a blade can resolve.
//
// Everything here is stated in WHOLE TEXELS rather than in metres, and that is
// the load-bearing part. The map's anchor follows the camera, so every frame the
// field has to be re-anchored: texel t of the new map is filled by reading the
// PREVIOUS map at t + (newAnchor - oldAnchor). An anchor free to land mid-texel
// would make that offset fractional, so the re-anchor would resample the whole
// field through a filter EVERY frame — a trail would blur itself away while the
// camera stood still, and a standing mover's splat would creep. Snapped to whole
// texels the re-anchor is a lossless integer copy and a stationary field is
// bit-stable.
//=============================================================================

namespace Flux_GrassConfig
{
	constexpr u_int uDISPLACEMENT_RESOLUTION = 256u;
	constexpr float fDISPLACEMENT_WORLD_SIZE = 64.0f;
	constexpr float fDISPLACEMENT_TEXEL_SIZE =
		fDISPLACEMENT_WORLD_SIZE / static_cast<float>(uDISPLACEMENT_RESOLUTION);

	// The field stores a push VECTOR whose magnitude is the [0,1] push amount, not
	// a distance. The clamp is not a nicety: the splat is additive over the decayed
	// value, so a stationary mover re-splatting every frame integrates the geometric
	// series amount/(1 - decay) and would saturate the map to an arbitrary magnitude.
	constexpr float fDISPLACEMENT_MAX_PUSH = 1.0f;
}

static_assert(Flux_GrassConfig::fDISPLACEMENT_TEXEL_SIZE == Flux_GrassConfig::fHI_LATTICE_STEP,
	"one displacement texel must be exactly one HI lattice step — a coarser field cannot push "
	"neighbouring blades differently, and a finer one stores detail no blade can sample");

// The map's origin, in WHOLE TEXELS from the world origin. This — not a metre
// position — is the stored anchor, so integrality is a property of the type
// rather than something the arithmetic has to preserve.
struct Flux_GrassDisplacementAnchor
{
	int m_iTexelX = 0;
	int m_iTexelZ = 0;

	bool operator==(const Flux_GrassDisplacementAnchor& xOther) const
	{
		return m_iTexelX == xOther.m_iTexelX && m_iTexelZ == xOther.m_iTexelZ;
	}
	bool operator!=(const Flux_GrassDisplacementAnchor& xOther) const { return !(*this == xOther); }
};

// Centre the map on the camera, then snap DOWN to a whole texel. floor, never
// round-to-nearest: floor is monotonic in the camera position with a step of
// exactly one texel, so a camera crossing a boundary moves the anchor by exactly
// one texel and never by two.
inline Flux_GrassDisplacementAnchor Flux_GrassSnapDisplacementAnchor(float fCameraX, float fCameraZ)
{
	const float fHalf = Flux_GrassConfig::fDISPLACEMENT_WORLD_SIZE * 0.5f;
	const float fInvTexel = 1.0f / Flux_GrassConfig::fDISPLACEMENT_TEXEL_SIZE;

	Flux_GrassDisplacementAnchor xAnchor;
	xAnchor.m_iTexelX = static_cast<int>(floorf((fCameraX - fHalf) * fInvTexel));
	xAnchor.m_iTexelZ = static_cast<int>(floorf((fCameraZ - fHalf) * fInvTexel));
	return xAnchor;
}

// World XZ of the map's (0,0) texel corner — always an exact multiple of the
// texel size, which is what the placement CS's world -> UV mapping relies on.
inline Zenith_Maths::Vector2 Flux_GrassDisplacementOriginWS(const Flux_GrassDisplacementAnchor& xAnchor)
{
	return Zenith_Maths::Vector2(
		static_cast<float>(xAnchor.m_iTexelX) * Flux_GrassConfig::fDISPLACEMENT_TEXEL_SIZE,
		static_cast<float>(xAnchor.m_iTexelZ) * Flux_GrassConfig::fDISPLACEMENT_TEXEL_SIZE);
}

// Texels the field slides when the anchor moves prev -> next: the CS reads the
// previous map at (texel + shift). Returned as floats because that is the slot
// the constant buffer carries, but the VALUE is always integral — an int
// difference widened, never a metre division.
inline Zenith_Maths::Vector2 Flux_GrassDisplacementTexelShift(const Flux_GrassDisplacementAnchor& xPrev,
	const Flux_GrassDisplacementAnchor& xNext)
{
	return Zenith_Maths::Vector2(
		static_cast<float>(xNext.m_iTexelX - xPrev.m_iTexelX),
		static_cast<float>(xNext.m_iTexelZ - xPrev.m_iTexelZ));
}

// Per-frame retention factor for an exponential decay with the given e-folding
// time. Derived from dt rather than authored per frame, or the trail would
// outlive its half-life on a fast machine and vanish on a slow one.
inline float Flux_GrassDisplacementDecay(float fDeltaSeconds, float fEFoldSeconds)
{
	// The divisor routes through a ternary with a nonzero fallback so the
	// division is provably safe to the compiler (C4723 ignores early-outs).
	const float fSafeEFold = fEFoldSeconds > 0.0f ? fEFoldSeconds : 1.0f;
	if (!(fDeltaSeconds > 0.0f) || !(fEFoldSeconds > 0.0f))
	{
		return 1.0f;
	}
	return expf(-fDeltaSeconds / fSafeEFold);
}

//=============================================================================
// Wind. CPU mirror of Zenith/Flux/Shaders/Common/Wind.slang — field for field
// and statement for statement, so the two can be diffed. Float agreement is NOT
// claimed (see Common/Noise.slang); structural agreement is.
//=============================================================================

struct Flux_WindConstants
{
	Zenith_Maths::Vector2 m_xDirectionXZ{ 1.0f, 0.0f };
	float m_fStrength = 0.35f;
	float m_fTime = 0.0f;
	float m_fFrequency = 0.02f;
	float m_fScrollSpeed = 6.0f;
	float m_fGustSharpness = 2.5f;
	u_int m_uSeed = 1337u;
	float m_fDetailFrequency = 1.5f;
	float m_fDetailSpeed = 3.0f;
	float m_fDetailAmplitude = 0.05f;
};

inline float Flux_SampleWindStrength(const Flux_WindConstants& xWind, float fWorldX, float fWorldZ)
{
	const float fScrolledX = fWorldX - xWind.m_xDirectionXZ.x * (xWind.m_fScrollSpeed * xWind.m_fTime);
	const float fScrolledZ = fWorldZ - xWind.m_xDirectionXZ.y * (xWind.m_fScrollSpeed * xWind.m_fTime);
	const float fPX = fScrolledX * xWind.m_fFrequency;
	const float fPZ = fScrolledZ * xWind.m_fFrequency;

	const float fOctave0 = Zenith_TerrainNoise::ValueNoise(fPX, fPZ, xWind.m_uSeed);
	const float fOctave1 = Zenith_TerrainNoise::ValueNoise(fPX * 2.17f + 13.7f, fPZ * 2.17f - 7.3f, xWind.m_uSeed + 101u);
	const float fField = fOctave0 * 0.65f + fOctave1 * 0.35f;

	const float fGust = powf(Zenith_Maths::Clamp(fField, 0.0f, 1.0f),
		xWind.m_fGustSharpness > 1.0f ? xWind.m_fGustSharpness : 1.0f);
	return xWind.m_fStrength * fGust;
}

//=============================================================================
// Map sampling. Coverage / height maps are scalar and bilinear; the type map is
// an index and is NEVER interpolated — a lerp between type 0 and type 4 would
// select type 2, a type the author never placed there.
//=============================================================================

enum class Flux_GrassMapFormat : u_int8
{
	U8 = 0,
	U16,
	F32,
	COUNT
};

// Row-major, covering [0, m_fWorldSize] on both axes from the world origin —
// the same convention as the painted grass-density map.
struct Flux_GrassMap
{
	const void* m_pData = nullptr;
	u_int m_uWidth = 0;
	u_int m_uHeight = 0;
	float m_fWorldSize = 0.0f;
	Flux_GrassMapFormat m_eFormat = Flux_GrassMapFormat::U8;
	float m_fScale = 1.0f;   // multiplied into the normalized texel value

	bool IsValid() const
	{
		return m_pData != nullptr && m_uWidth > 0u && m_uHeight > 0u && m_fWorldSize > 0.0f;
	}
};

inline float Flux_GrassMapTexel(const Flux_GrassMap& xMap, u_int uX, u_int uZ)
{
	const u_int uIndex = uZ * xMap.m_uWidth + uX;
	if (xMap.m_eFormat == Flux_GrassMapFormat::U8)
	{
		return static_cast<float>(static_cast<const u_int8*>(xMap.m_pData)[uIndex]) * (1.0f / 255.0f);
	}
	if (xMap.m_eFormat == Flux_GrassMapFormat::U16)
	{
		return static_cast<float>(static_cast<const u_int16*>(xMap.m_pData)[uIndex]) * (1.0f / 65535.0f);
	}
	if (xMap.m_eFormat == Flux_GrassMapFormat::F32)
	{
		return static_cast<const float*>(xMap.m_pData)[uIndex];
	}
	return 0.0f;
}

// Texel-space position with edge clamp. Texel k sits at world k * worldSize /
// dimension (no half-texel offset), matching the existing density-map sampler.
inline float Flux_GrassMapTexelCoord(float fWorld, u_int uDimension, float fWorldSize)
{
	const float fScale = static_cast<float>(uDimension) / fWorldSize;
	return Zenith_Maths::Clamp(fWorld * fScale, 0.0f, static_cast<float>(uDimension - 1u));
}

// Bilinear with edge clamp. An absent map samples 0, never a neutral 1: a
// caller with no map must decide what "no data" means, not inherit "full".
inline float Flux_GrassSampleMapBilinear(const Flux_GrassMap& xMap, float fWorldX, float fWorldZ)
{
	if (!xMap.IsValid())
	{
		return 0.0f;
	}
	const float fPX = Flux_GrassMapTexelCoord(fWorldX, xMap.m_uWidth, xMap.m_fWorldSize);
	const float fPZ = Flux_GrassMapTexelCoord(fWorldZ, xMap.m_uHeight, xMap.m_fWorldSize);
	const u_int uX0 = static_cast<u_int>(fPX);
	const u_int uZ0 = static_cast<u_int>(fPZ);
	const u_int uX1 = uX0 + 1u < xMap.m_uWidth ? uX0 + 1u : xMap.m_uWidth - 1u;
	const u_int uZ1 = uZ0 + 1u < xMap.m_uHeight ? uZ0 + 1u : xMap.m_uHeight - 1u;
	const float fTX = fPX - static_cast<float>(uX0);
	const float fTZ = fPZ - static_cast<float>(uZ0);

	const float fTop = Flux_GrassMapTexel(xMap, uX0, uZ0) * (1.0f - fTX) + Flux_GrassMapTexel(xMap, uX1, uZ0) * fTX;
	const float fBottom = Flux_GrassMapTexel(xMap, uX0, uZ1) * (1.0f - fTX) + Flux_GrassMapTexel(xMap, uX1, uZ1) * fTX;
	return (fTop * (1.0f - fTZ) + fBottom * fTZ) * xMap.m_fScale;
}

inline u_int Flux_GrassNearestTexel(float fTexelCoord, u_int uDimension)
{
	const u_int uRounded = static_cast<u_int>(fTexelCoord + 0.5f);
	return uRounded < uDimension ? uRounded : uDimension - 1u;
}

// Nearest-texel type pick. The raw byte IS the type index, so the map must be
// U8 and the result is always a byte that appears in the map.
inline u_int Flux_GrassSampleTypeIndex(const Flux_GrassMap& xMap, float fWorldX, float fWorldZ)
{
	if (!xMap.IsValid() || xMap.m_eFormat != Flux_GrassMapFormat::U8)
	{
		return 0u;
	}
	const u_int uX = Flux_GrassNearestTexel(Flux_GrassMapTexelCoord(fWorldX, xMap.m_uWidth, xMap.m_fWorldSize), xMap.m_uWidth);
	const u_int uZ = Flux_GrassNearestTexel(Flux_GrassMapTexelCoord(fWorldZ, xMap.m_uHeight, xMap.m_fWorldSize), xMap.m_uHeight);
	return static_cast<u_int>(static_cast<const u_int8*>(xMap.m_pData)[uZ * xMap.m_uWidth + uX]);
}

//=============================================================================
// Dithered type pick. CPU mirror of Flux_GrassPickType in
// Zenith/Flux/Shaders/Vegetation/Flux_Grass_Placement.slang, which is
// AUTHORITATIVE.
//
// Filtering an INDEX map is meaningless, so the four bilinear weights are used as
// a probability distribution and ONE WHOLE TEXEL is chosen: the boundary between
// two types dissolves into a stochastic mix of the two rather than inventing a
// third. The choice is integer-hash-decided and therefore bit-exact across the two
// languages; only the weights themselves are float.
//
// This is the placement CS's sampler. Flux_GrassSampleTypeIndex above is the
// QUERY sampler (nearest texel, no hash) — a caller asking "what type is here"
// must not get a different answer on every call.
//=============================================================================

// Texel-space position in the CS's convention: UV * dimension - 0.5, so texel k's
// CENTRE sits at k and the bilinear footprint is [floor, floor + 1].
inline float Flux_GrassMapTexelSpace(float fWorld, u_int uDimension, float fWorldSize)
{
	const float fUV = Flux_GrassSaturate(fWorld / fWorldSize);
	return fUV * static_cast<float>(uDimension) - 0.5f;
}

// The four bilinear footprint weights, in the CS's order: weight i sits at texel
// offset (i & 1, i >> 1).
inline void Flux_GrassTypeGatherWeights(float fFracX, float fFracZ, float afOutWeight[4])
{
	afOutWeight[0] = (1.0f - fFracX) * (1.0f - fFracZ);
	afOutWeight[1] = fFracX * (1.0f - fFracZ);
	afOutWeight[2] = (1.0f - fFracX) * fFracZ;
	afOutWeight[3] = fFracX * fFracZ;
}

// Which of the four gathered texels this blade takes. The weights sum to 1, so
// falling out of the loop lands on the last bucket — that IS the CS's behaviour,
// not a rounding guard bolted on afterwards.
inline u_int Flux_GrassPickGatherIndex(const float afWeight[4], u_int uHash)
{
	const float fPick = Zenith_TerrainNoise::HashToFloat01(
		Zenith_TerrainNoise::HashUInt(uHash + Flux_GrassConfig::uTYPE_DITHER_SALT));
	float fAccum = 0.0f;
	for (u_int u = 0; u < 3u; u++)
	{
		fAccum += afWeight[u];
		if (fPick <= fAccum)
		{
			return u;
		}
	}
	return 3u;
}

inline u_int Flux_GrassClampTexel(int iTexel, u_int uDimension)
{
	const int iLast = static_cast<int>(uDimension) - 1;
	return static_cast<u_int>(iTexel < 0 ? 0 : (iTexel > iLast ? iLast : iTexel));
}

// Gather the four texels of the bilinear footprint, choose one by the dither, and
// clamp the byte it holds to the live type count — exactly the CS's chain. The
// result is ONE gathered texel's byte, never an average of two.
inline u_int Flux_GrassSampleTypeDithered(const Flux_GrassMap& xMap, float fWorldX, float fWorldZ,
	u_int uHash, u_int uTypeCount)
{
	if (!xMap.IsValid() || xMap.m_eFormat != Flux_GrassMapFormat::U8)
	{
		return 0u;
	}
	const float fSpaceX = Flux_GrassMapTexelSpace(fWorldX, xMap.m_uWidth, xMap.m_fWorldSize);
	const float fSpaceZ = Flux_GrassMapTexelSpace(fWorldZ, xMap.m_uHeight, xMap.m_fWorldSize);
	const float fBaseX = floorf(fSpaceX);
	const float fBaseZ = floorf(fSpaceZ);

	float afWeight[4];
	Flux_GrassTypeGatherWeights(fSpaceX - fBaseX, fSpaceZ - fBaseZ, afWeight);
	const u_int uChosen = Flux_GrassPickGatherIndex(afWeight, uHash);

	const u_int uX = Flux_GrassClampTexel(static_cast<int>(fBaseX) + static_cast<int>(uChosen & 1u), xMap.m_uWidth);
	const u_int uZ = Flux_GrassClampTexel(static_cast<int>(fBaseZ) + static_cast<int>(uChosen >> 1u), xMap.m_uHeight);
	const u_int uIndex = static_cast<const u_int8*>(xMap.m_pData)[uZ * xMap.m_uWidth + uX];
	const u_int uLast = (uTypeCount > 0u ? uTypeCount : 1u) - 1u;
	return uIndex < uLast ? uIndex : uLast;
}

//=============================================================================
// Pipeline-variant selection — same house pattern as
// Flux/Terrain/Flux_TerrainPipelineSelect.h.
//
// Debug mode is deliberately NOT an axis: it is a uniform branch inside the
// fragment stage, not pipeline state, so it must not multiply the pipeline set.
// Only inputs that change a pipeline OBJECT belong in this signature.
//=============================================================================

enum class Flux_GrassPipelineVariant : u_int8
{
	GBUFFER = 0,        // 4 core MRTs
	GBUFFER_VELOCITY,   // 5 MRTs, TAA velocity latch on
	SHADOW_DEPTH,       // depth only, no colour attachment
	COUNT
};

// PURE. The shadow pass wins outright: a cascade target has no colour
// attachment at all, so a velocity variant there is not merely wasteful, it is
// render-pass-incompatible.
constexpr Flux_GrassPipelineVariant Flux_GrassSelectPipelineVariant(bool bVelocityActive, bool bShadowPass)
{
	if (bShadowPass)
	{
		return Flux_GrassPipelineVariant::SHADOW_DEPTH;
	}
	return bVelocityActive ? Flux_GrassPipelineVariant::GBUFFER_VELOCITY : Flux_GrassPipelineVariant::GBUFFER;
}

// The framebuffer contract each variant's pipeline declares. The render-graph
// pass's .Writes count must agree with this or the bind is invalid.
constexpr u_int Flux_GrassPipelineColourAttachmentCount(Flux_GrassPipelineVariant eVariant)
{
	if (eVariant == Flux_GrassPipelineVariant::GBUFFER) return 4u;
	if (eVariant == Flux_GrassPipelineVariant::GBUFFER_VELOCITY) return 5u;
	return 0u;
}

constexpr bool Flux_GrassPipelineVariantIsShadow(Flux_GrassPipelineVariant eVariant)
{
	return eVariant == Flux_GrassPipelineVariant::SHADOW_DEPTH;
}

constexpr bool Flux_GrassPipelineVariantIsVelocity(Flux_GrassPipelineVariant eVariant)
{
	return eVariant == Flux_GrassPipelineVariant::GBUFFER_VELOCITY;
}
