// =====================================================================
// Flux_GrassCommon — shared Slang include for the GPU-driven grass family
// (Reset / Placement / IndirectFixup / Displacement / ToGBuffer /
// ToGBufferVelocity / ToShadowmap).
//
// TEXTUAL #include (NOT `import`): it declares the spine ParameterBlocks via
// Common/Bindings.slang, whose fixed set 0/1/2 assignment depends on declaration
// order, and its shading core calls the spine accessor free functions. Every
// consumer #includes THIS file first, then declares its own PASS block, so the
// PASS block lands at space 3.
//
// BINARY CONTRACT. Flux_GrassBladeInstance (64 B) and Flux_GrassTypeParamsGPU
// (144 B) are hand-mirrored twins of the structs in
// Flux/Vegetation/Flux_GrassTypes.h, which is the source of truth and carries the
// static_asserts. Field ORDER and TYPE must match there field-for-field: a silent
// drift reads every field after it at the wrong offset and produces plausible
// garbage rather than a crash. The bit-pack helpers below are transcriptions of
// the constexpr helpers in that header and must stay statement-for-statement
// diffable against them.
//
// DETERMINISM. Blades are regenerated every frame and never persisted; all
// per-blade randomness keys off Flux_NoiseHashCoords(latticeX, latticeZ, seed),
// so a blade is a function of its world position alone. Nothing here may read
// wall-clock state or accumulate across frames — a blade that changes identity
// between frames flickers under TAA.
// =====================================================================

#ifndef FLUX_VEGETATION_GRASSCOMMON_SLANG
#define FLUX_VEGETATION_GRASSCOMMON_SLANG

#include "Common/Bindings.slang"   // spine sets 0/1/2 (declared FIRST) + accessor facade
import Common.Noise;               // Flux_NoiseHashUInt / HashCoords / HashToFloat01 / ValueNoise
import Common.Wind;                // Flux_WindConstants / Flux_SampleWindStrength / Flux_SampleWindDetail
import Common.GBuffer;             // GBufferOut / MakeGBuffer / GBUFFER_SHADING_SUBSURFACE

// =====================================================================
// Partition contract — PINNED. Mirrored by the C++ side; changing a base or a
// cap here without changing it there silently writes one partition's survivors
// into the next partition's range.
// =====================================================================

// VkDrawIndexedIndirectCommand is 5 uints; the args buffer is a flat uint array
// indexed slot*5 + word, so the reset can write and the placement CS can
// InterlockedAdd without agreeing on a struct layout.
static const uint kGRASS_INDIRECT_WORDS      = 5u;
static const uint kGRASS_INDIRECT_SLOT_COUNT = 16u;   // total args slots (4 live + 12 reserved)
static const uint kGRASS_LIVE_SLOT_COUNT     = 4u;    // slots the placement CS may append into

static const uint kGRASS_SLOT_CAMERA_HI = 0u;   // camera view, HI mesh
static const uint kGRASS_SLOT_CAMERA_LO = 1u;   // camera view, LO mesh
static const uint kGRASS_SLOT_CASCADE_0 = 2u;   // shadow cascade 0, LO mesh
static const uint kGRASS_SLOT_CASCADE_1 = 3u;   // shadow cascade 1, LO mesh

// Base offsets (in UINTS) of each live slot's slice of the visible-index buffer,
// and the number of entries each slice may hold. base[s] + cap[s] == base[s+1],
// so the four slices tile the buffer with no gap and no overlap.
static const uint kauGRASS_PARTITION_BASE[4] = { 0u, 524288u, 1048576u, 1310720u };
static const uint kauGRASS_PARTITION_CAP[4]  = { 524288u, 524288u, 262144u, 262144u };
static const uint kGRASS_VISIBLE_INDEX_CAPACITY = 1572864u;   // base[3] + cap[3]

// Which frustum each live slot is culled against. Slots 0 and 1 share the camera
// frustum and are separated by the HI-radius distance band, not by a plane set.
static const uint kGRASS_FRUSTUM_COUNT    = 3u;
static const uint kGRASS_FRUSTUM_CAMERA   = 0u;
static const uint kGRASS_FRUSTUM_CASCADE0 = 1u;
static const uint kGRASS_FRUSTUM_CASCADE1 = 2u;
static const uint kauGRASS_SLOT_FRUSTUM[4] = { 0u, 0u, 1u, 2u };

// =====================================================================
// Blade mesh — AUTHORITATIVE tables. There is NO vertex buffer: the draw is
// indexed, so SV_VertexID delivers the fetched index value, which IS the logical
// vertex id 0..14. The uint32 index buffer the C++ side uploads must be
// kauGRASS_INDEX_TABLE verbatim, or firstIndex/indexCount below address the wrong
// triangles.
//
// 15 logical vertices across two PERMANENTLY DISCONNECTED pieces:
//   piece A — verts 0..7,  4 rows x 2 columns, 3 quads, 18 indices
//   piece B — verts 8..14, 3 rows x 2 columns + a tip vertex, 2 quads + 1
//             triangle, 15 indices
// A's top row (6,7) and B's bottom row (8,9) sit at the SAME height fraction, so
// an unfolded blade welds into one continuous surface. A folded blade (typeFlags
// bit 8) rigid-offsets ALL of piece B, which separates the two pieces at that
// seam — that is why they are separate primitives and why there is no per-mode
// index table.
// =====================================================================

static const uint kGRASS_VERTEX_COUNT     = 15u;
static const uint kGRASS_INDEX_TABLE_SIZE = 48u;
static const uint kGRASS_HI_FIRST_INDEX   = 0u;
static const uint kGRASS_HI_INDEX_COUNT   = 33u;   // piece A (18) + piece B (15)
static const uint kGRASS_LO_FIRST_INDEX   = 33u;
static const uint kGRASS_LO_INDEX_COUNT   = 15u;   // 2 quads + tip, over a subset of the same verts

// Triangle winding follows the engine rule cross(C-A, B-A) = outward, i.e. a quad
// spanning rows r (lower) and r+1 (upper) emits
//   { L(r), L(r+1), R(r) } and { R(r), L(r+1), R(r+1) }.
static const uint kauGRASS_INDEX_TABLE[48] =
{
	// --- piece A, firstIndex 0, 18 indices ---
	 0u,  2u,  1u,    1u,  2u,  3u,   // rows 0 -> 1
	 2u,  4u,  3u,    3u,  4u,  5u,   // rows 1 -> 2
	 4u,  6u,  5u,    5u,  6u,  7u,   // rows 2 -> 3 (A's clamped top edge)
	// --- piece B, continues the HI range to 33 indices ---
	 8u, 10u,  9u,    9u, 10u, 11u,   // seam row -> row 1
	10u, 12u, 11u,   11u, 12u, 13u,   // row 1 -> row 2
	12u, 14u, 13u,                    // row 2 -> tip
	// --- LO strip, firstIndex 33, 15 indices ---
	// Spans base -> tip in three steps using verts 0,1 / 4,5 / 8,9 / 14. Its upper
	// row is a PIECE-B row, so a folded blade offsets the whole upper half of the
	// strip together and the strip skews instead of tearing.
	 0u,  4u,  1u,    1u,  4u,  5u,   // u 0.00 -> 0.50
	 4u,  8u,  5u,    5u,  8u,  9u,   // u 0.50 -> 0.75 (seam row)
	 8u, 14u,  9u,                    // seam row -> tip
};

// Per-vertex height fraction in UNIFORM space, before the per-type vertex
// distribution exponent remaps it (Flux_GrassRemapHeightT below). Piece A covers
// 0 -> 0.75 in three equal steps; piece B covers 0.75 -> 1 in three equal steps,
// so the upper quarter carries as many rows as the lower three quarters and the
// tip curve stays smooth.
static const float kafGRASS_VERT_U[15] =
{
	0.0,        0.0,          // row 0 (piece A base)
	0.25,       0.25,         // row 1
	0.5,        0.5,          // row 2
	0.75,       0.75,         // row 3 (piece A top edge)
	0.75,       0.75,         // row 0 (piece B seam row — coincides with row 3 unfolded)
	0.83333333, 0.83333333,   // row 1
	0.91666667, 0.91666667,   // row 2
	1.0,                      // tip
};

// Lateral side of the blade spine: -1 = left edge, +1 = right edge, 0 = the tip
// vertex (which sits on the spine).
static const float kafGRASS_VERT_SIDE[15] =
{
	-1.0, 1.0,  -1.0, 1.0,  -1.0, 1.0,  -1.0, 1.0,
	-1.0, 1.0,  -1.0, 1.0,  -1.0, 1.0,
	 0.0,
};

// 0 = piece A (lower), 1 = piece B (upper). Only piece B receives the fold offset.
static const uint kauGRASS_VERT_PIECE[15] =
{
	0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u,
	1u, 1u, 1u, 1u, 1u, 1u, 1u,
};

// =====================================================================
// Lattice + fade bands — mirror of Flux_GrassConfig / Flux_GrassLatticeClass /
// Flux_GrassLatticeFade in Flux_GrassTypes.h.
// =====================================================================

static const float kGRASS_HI_LATTICE_STEP  = 0.25;
static const uint  kGRASS_LO_LATTICE_STRIDE = 2u;
static const uint  kGRASS_TILE_CELLS       = 64u;
static const uint  kGRASS_TILE_CELL_COUNT  = 4096u;   // kGRASS_TILE_CELLS squared
static const float kGRASS_HI_RADIUS        = 64.0;

// Class 0 is the (even,even) set the LO lattice reproduces and must NEVER fade —
// fading it punches holes the LO tiles do not fill. Equal-width, shifted bands
// mean the three ramps are translates of each other, so density falls
// monotonically and no two classes pop together.
static const float kafGRASS_FADE_BAND_START[4] = { 0.0, 0.70, 0.74, 0.80 };
static const float kafGRASS_FADE_BAND_END[4]   = { 0.0, 0.82, 0.86, 0.92 };

uint Flux_GrassLatticeClass(int iLatticeX, int iLatticeZ)
{
	return ((uint)iLatticeX & 1u) | (((uint)iLatticeZ & 1u) << 1u);
}

float Flux_GrassLatticeFade(uint uClass, float fDistance, float fHiRadius)
{
	if (uClass == 0u || uClass > 3u) return 1.0;
	float fStart = kafGRASS_FADE_BAND_START[uClass] * fHiRadius;
	float fEnd   = kafGRASS_FADE_BAND_END[uClass]   * fHiRadius;
	if (fDistance <= fStart) return 1.0;
	if (fDistance >= fEnd)   return 0.0;
	return 1.0 - (fDistance - fStart) / (fEnd - fStart);
}

// A lattice node's world position. ONE mapping lattice->world on both LODs: a LO
// tile addresses every kGRASS_LO_LATTICE_STRIDE-th HI node, so its nodes are all
// (even,even) — class 0 — which is what makes the HI->LO transition a fade rather
// than a reshuffle.
float2 Flux_GrassLatticeToWorld(int2 xLattice)
{
	return float2(xLattice) * kGRASS_HI_LATTICE_STEP;
}

// Purely geometric HI-region test, mirroring Flux_GrassIsInsideHiRegion. A LO tile
// skips nodes that answer true here because a HI tile already emitted them; if the
// two answers could differ the seam would double-draw or gap.
bool Flux_GrassIsInsideHiRegion(float3 xCameraPosWS, float2 xWorldXZ, float fHiRadius)
{
	float2 xDelta = xWorldXZ - xCameraPosWS.xz;
	return dot(xDelta, xDelta) <= fHiRadius * fHiRadius;
}

// =====================================================================
// Bit packing — transcription of the constexpr helpers in Flux_GrassTypes.h.
// =====================================================================

uint Flux_GrassPackUnorm16(float fValue)   { return (uint)(saturate(fValue) * 65535.0 + 0.5); }
uint Flux_GrassPackUnorm2x16(float fX, float fY)
{
	return Flux_GrassPackUnorm16(fX) | (Flux_GrassPackUnorm16(fY) << 16u);
}
float Flux_GrassUnpackUnorm2x16X(uint uPacked) { return (float)(uPacked & 0xFFFFu) * (1.0 / 65535.0); }
float Flux_GrassUnpackUnorm2x16Y(uint uPacked) { return (float)((uPacked >> 16u) & 0xFFFFu) * (1.0 / 65535.0); }

uint  Flux_GrassPackClump(float fClumpHash01, float fDistToCentre01) { return Flux_GrassPackUnorm2x16(fClumpHash01, fDistToCentre01); }
float Flux_GrassUnpackClumpHash01(uint uPacked) { return Flux_GrassUnpackUnorm2x16X(uPacked); }
float Flux_GrassUnpackClumpDist01(uint uPacked) { return Flux_GrassUnpackUnorm2x16Y(uPacked); }

float3 Flux_GrassUnpackColourRGB(uint uPacked)
{
	return float3(
		(float)((uPacked >> 0u)  & 0xFFu) * (1.0 / 255.0),
		(float)((uPacked >> 8u)  & 0xFFu) * (1.0 / 255.0),
		(float)((uPacked >> 16u) & 0xFFu) * (1.0 / 255.0));
}

// The blade record's typeFlags slot. Bits 0-9 and 16-31 are the layout declared in
// Flux_GrassTypes.h. Bits 10-11 additionally carry the LATTICE CLASS: the vertex
// stage applies the class fade and cannot recover the class from posWS (the base is
// jittered and clump-pulled), and the record has no spare field. Bits 12-15 stay
// reserved. The CPU decoders mask their own bits, so writing 10-11 is additive.
static const uint kGRASS_FLAG_TYPE_INDEX_MASK   = 0x000000FFu;   // bits 0-7
static const uint kGRASS_FLAG_FOLDED_BIT        = 0x00000100u;   // bit 8
static const uint kGRASS_FLAG_LO_MESH_BIT       = 0x00000200u;   // bit 9
static const uint kGRASS_FLAG_LATTICE_CLASS_SHIFT = 10u;         // bits 10-11
static const uint kGRASS_FLAG_LATTICE_CLASS_MASK  = 0x00000003u;
static const uint kGRASS_FLAG_FS_HASH_SHIFT     = 16u;           // bits 16-31
static const uint kGRASS_FLAG_FS_HASH_MASK      = 0x0000FFFFu;

uint Flux_GrassPackTypeFlags(uint uTypeIndex, bool bFolded, bool bLOMesh, uint uLatticeClass, uint uFragmentHash)
{
	return (uTypeIndex & kGRASS_FLAG_TYPE_INDEX_MASK)
		| (bFolded ? kGRASS_FLAG_FOLDED_BIT : 0u)
		| (bLOMesh ? kGRASS_FLAG_LO_MESH_BIT : 0u)
		| ((uLatticeClass & kGRASS_FLAG_LATTICE_CLASS_MASK) << kGRASS_FLAG_LATTICE_CLASS_SHIFT)
		| ((uFragmentHash & kGRASS_FLAG_FS_HASH_MASK) << kGRASS_FLAG_FS_HASH_SHIFT);
}

uint Flux_GrassTypeFlagsIndex(uint uFlags)        { return uFlags & kGRASS_FLAG_TYPE_INDEX_MASK; }
bool Flux_GrassTypeFlagsIsFolded(uint uFlags)     { return (uFlags & kGRASS_FLAG_FOLDED_BIT) != 0u; }
bool Flux_GrassTypeFlagsIsLOMesh(uint uFlags)     { return (uFlags & kGRASS_FLAG_LO_MESH_BIT) != 0u; }
uint Flux_GrassTypeFlagsLatticeClass(uint uFlags) { return (uFlags >> kGRASS_FLAG_LATTICE_CLASS_SHIFT) & kGRASS_FLAG_LATTICE_CLASS_MASK; }
uint Flux_GrassTypeFlagsFragmentHash(uint uFlags) { return (uFlags >> kGRASS_FLAG_FS_HASH_SHIFT) & kGRASS_FLAG_FS_HASH_MASK; }

// =====================================================================
// GPU-mirror records.
// =====================================================================

// 16 x 32-bit slots, 64 bytes. Twin of Flux_GrassBladeInstance in
// Flux_GrassTypes.h — order and types are the contract; the byte offsets in the
// comments are the ones that header's layout produces.
struct Flux_GrassBladeInstance
{
	float3 m_xPosWS;           //  0 : world position of the blade base
	uint   m_uHashBits;        // 12 : lattice hash (blade identity)
	float2 m_xFacingXZ;        // 16 : unit facing in world XZ
	float  m_fHeight;          // 24
	float  m_fWidth;           // 28
	float  m_fTiltRad;         // 32 : lean away from vertical
	float  m_fBend;            // 36 : quadratic bend along height
	float  m_fSideCurve;       // 40 : lateral curl of the blade plane
	float  m_fWindStrength;    // 44 : raw Flux_SampleWindStrength at the base
	float2 m_xClumpNormalXZ;   // 48 : direction from the clump centre
	uint   m_uClumpPacked;     // 56 : Flux_GrassPackClump
	uint   m_uTypeFlags;       // 60 : Flux_GrassPackTypeFlags
};

// 36 x 32-bit scalars = 144 bytes. Twin of Flux_GrassTypeParamsGPU in
// Flux_GrassTypes.h. All-scalar by construction: it packs identically under every
// buffer layout rule, so no field may become a vector.
struct Flux_GrassTypeParamsGPU
{
	// --- shape ---
	float m_fHeightMin;               //   0
	float m_fHeightMax;               //   4
	float m_fWidthMin;                //   8
	float m_fWidthMax;                //  12
	float m_fTiltMinRad;              //  16
	float m_fTiltMaxRad;              //  20
	float m_fBendMin;                 //  24
	float m_fBendMax;                 //  28
	float m_fSideCurve;               //  32
	float m_fVertexDistributionPow;   //  36 : >1 packs vertex rows toward the tip
	float m_fFoldThreshold;           //  40 : blade hash above this folds along its spine
	float m_fFoldPushApart;           //  44
	float m_fTipWidthFrac;            //  48
	// --- placement ---
	float m_fDensity;                 //  52 : lattice-node acceptance probability [0,1]
	float m_fSlopeMax;                //  56 : sine of the steepest ground that grows grass
	float m_fMaxDrawDistance;         //  60
	float m_fSlopeAlign;              //  64 : 0 = always upright, 1 = follow the ground normal
	// --- clump ---
	float m_fClumpScale;              //  68 : metres per clump cell
	float m_fClumpHeightBoost;        //  72
	float m_fClumpFacingWeight;       //  76
	float m_fClumpOutwardWeight;      //  80
	float m_fClumpPullToCentre;       //  84
	float m_fClumpNormalBlendMax;     //  88
	// --- appearance ---
	uint  m_uBaseColourPacked;        //  92
	uint  m_uTipColourPacked;         //  96
	uint  m_uVeinTextureIndex;        // 100 : bindless; 0xFFFFFFFF = unbound
	uint  m_uGlossTextureIndex;       // 104
	uint  m_uRampTextureIndex;        // 108
	float m_fGlossRepeat;             // 112
	float m_fRoughnessBase;           // 116
	float m_fSpecular;                // 120
	uint  m_uColourJitterNormalTiltPacked; // 124 : jitter, normal tilt as a fraction of a quarter turn
	uint  m_uTranslucencyPacked;      // 128 : base, tip
	uint  m_uAOPacked;                // 132 : base occlusion, height at which it releases
	// --- dynamics ---
	float m_fWindResponse;            // 136
	float m_fStiffness;               // 140
};

static const uint kGRASS_BINDLESS_UNBOUND = 0xFFFFFFFFu;

// One dispatched placement tile. Lattice coordinates are stored in GLOBAL HI-lattice
// units rather than as a world origin, so both LODs share one lattice->world mapping
// and a LO tile's nodes are exactly the (even,even) subset of the HI lattice.
struct Flux_GrassTileGPU
{
	int  m_iLatticeBaseX;    //  0 : global HI-lattice coordinate of cell (0,0)
	int  m_iLatticeBaseZ;    //  4
	uint m_uLatticeStride;   //  8 : 1 for a HI tile, kGRASS_LO_LATTICE_STRIDE for a LO tile
	uint m_uIsLoLOD;         // 12 : 1 => emit the LO mesh and skip nodes inside the HI region
};

// A culling plane with an INWARD-pointing normal; xyz = normal, w = distance.
struct Flux_GrassFrustumPlane
{
	float4 m_xNormalAndDistance;
};

// Wind constants as three float4s. Flux_WindConstants (Common/Wind.slang) is an
// all-scalar struct whose std140 size in a constant buffer differs from its C++
// sizeof; three float4s are unambiguous under every layout rule and mirror the C++
// upload one-for-one.
struct Flux_GrassWindBlock
{
	float4 m_xDirXZ_Strength_Time;    //  0 : xy = unit heading, z = strength (m), w = time (s)
	float4 m_xFreq_Scroll_Gust_Seed;  // 16 : x = 1/m, y = m/s, z = pow exponent >= 1, w = asfloat(seed)
	float4 m_xDetailFreq_Speed_Amp;   // 32 : x = 1/m, y = rad/s, z = tip amplitude (m), w unused
};

Flux_WindConstants Flux_GrassDecodeWind(Flux_GrassWindBlock xBlock)
{
	Flux_WindConstants xWind;
	xWind.m_xDirectionXZ    = xBlock.m_xDirXZ_Strength_Time.xy;
	xWind.m_fStrength       = xBlock.m_xDirXZ_Strength_Time.z;
	xWind.m_fTime           = xBlock.m_xDirXZ_Strength_Time.w;
	xWind.m_fFrequency      = xBlock.m_xFreq_Scroll_Gust_Seed.x;
	xWind.m_fScrollSpeed    = xBlock.m_xFreq_Scroll_Gust_Seed.y;
	xWind.m_fGustSharpness  = xBlock.m_xFreq_Scroll_Gust_Seed.z;
	xWind.m_uSeed           = asuint(xBlock.m_xFreq_Scroll_Gust_Seed.w);
	xWind.m_fDetailFrequency= xBlock.m_xDetailFreq_Speed_Amp.x;
	xWind.m_fDetailSpeed    = xBlock.m_xDetailFreq_Speed_Amp.y;
	xWind.m_fDetailAmplitude= xBlock.m_xDetailFreq_Speed_Amp.z;
	return xWind;
}

// Per-draw constants shared VERBATIM by ToGBuffer, ToGBufferVelocity and
// ToShadowmap. One layout, three consumers: the shadow pose must be built from the
// same camera position and the same fade/LOD bands as the lit pose or a blade
// casts a shadow it is not standing in.
struct Flux_GrassDrawConstantsLayout
{
	Flux_GrassWindBlock m_xWind;                          //  0 : 48 B, current frame
	float4 m_xCameraPosWS_HiRadius;                       // 48 : xyz = MAIN camera WS, w = HI radius (m)
	float4 m_xMinPixelWidth_Thicken_NormalBlend_LODBand;  // 64 : min blade width in pixels, thicken blend [0,1],
	                                                      //      clump-normal blend distance (m), LOD-blend band fraction
	float4 m_xMaxDrawDistance_GlossCut_DebugMode_Pad;     // 80 : m, m, asfloat(debug mode), 0
};

// =====================================================================
// Vertex construction.
// =====================================================================

// Everything the blade pose needs that does not come from the record or the type.
// Each shell fills it from its own PASS constants + its own VIEW set, so the shadow
// path thickens against the CASCADE projection (which is what keeps a thin blade
// from falling through the cascade raster) while sharing one pose path.
struct Flux_GrassVertexContext
{
	Flux_WindConstants m_xWind;
	float3   m_xCameraPosWS;
	float    m_fHiRadius;
	float4x4 m_xViewMat;
	float4x4 m_xProjMat;
	float2   m_xRcpScreenDims;
	float    m_fMinPixelWidth;
	float    m_fThickenBlend;         // [0,1]: 0 = no min-pixel clamp, 1 = full clamp
	float    m_fNormalBlendDistance;  // metres over which the normal reaches m_fClumpNormalBlendMax
	float    m_fLODBlendBand;         // fraction of the HI radius over which a HI blade converges on the LO silhouette
};

// Build the context from the shared draw constants + the CURRENTLY BOUND view set.
// The view/proj come from the accessors, so a cascade pass thickens against its own
// projection, while the camera position and the fade/LOD bands come from the
// constants and stay the MAIN camera's in every pass — the shadow pose has to be the
// pose the lit blade is actually holding.
Flux_GrassVertexContext Flux_GrassMakeVertexContext(Flux_GrassDrawConstantsLayout xConstants)
{
	Flux_GrassVertexContext xCtx;
	xCtx.m_xWind                = Flux_GrassDecodeWind(xConstants.m_xWind);
	xCtx.m_xCameraPosWS         = xConstants.m_xCameraPosWS_HiRadius.xyz;
	xCtx.m_fHiRadius            = xConstants.m_xCameraPosWS_HiRadius.w;
	xCtx.m_xViewMat             = GetViewMat();
	xCtx.m_xProjMat             = GetProjMat();
	xCtx.m_xRcpScreenDims       = GetRcpScreenDims();
	xCtx.m_fMinPixelWidth       = xConstants.m_xMinPixelWidth_Thicken_NormalBlend_LODBand.x;
	xCtx.m_fThickenBlend        = xConstants.m_xMinPixelWidth_Thicken_NormalBlend_LODBand.y;
	xCtx.m_fNormalBlendDistance = xConstants.m_xMinPixelWidth_Thicken_NormalBlend_LODBand.z;
	xCtx.m_fLODBlendBand        = xConstants.m_xMinPixelWidth_Thicken_NormalBlend_LODBand.w;
	return xCtx;
}

struct Flux_GrassVertexOut
{
	float3 m_xWorldPos;
	float3 m_xNormalWS;
	float  m_fHeightT;         // 0 base -> 1 tip, AFTER the per-type distribution remap
	float  m_fSide;            // -1 / +1 edge, 0 at the tip
	float  m_fLatticeFade;     // presence weight this blade was drawn at
	float  m_fCameraDistance;
};

// Uniform row parameter -> height fraction. An exponent > 1 shortens the steps near
// the tip, so the curvature the Bezier puts there is actually tessellated.
float Flux_GrassRemapHeightT(float fU, float fDistributionPow)
{
	return pow(saturate(fU), 1.0 / max(fDistributionPow, 0.01));
}

float3 Flux_GrassBezier(float3 xP0, float3 xP1, float3 xP2, float3 xP3, float fT)
{
	float fInv = 1.0 - fT;
	return fInv * fInv * fInv * xP0
		 + 3.0 * fInv * fInv * fT * xP1
		 + 3.0 * fInv * fT * fT * xP2
		 + fT * fT * fT * xP3;
}

float3 Flux_GrassBezierTangent(float3 xP0, float3 xP1, float3 xP2, float3 xP3, float fT)
{
	float fInv = 1.0 - fT;
	return 3.0 * fInv * fInv * (xP1 - xP0)
		 + 6.0 * fInv * fT * (xP2 - xP1)
		 + 3.0 * fT * fT * (xP3 - xP2);
}

// The blade's local frame. facing is the direction the blade leans and bends
// toward; side is the width axis; grow is the root growth direction after tilt.
struct Flux_GrassBladeFrame
{
	float3 m_xFacing;
	float3 m_xSide;
	float3 m_xGrow;
};

Flux_GrassBladeFrame Flux_GrassMakeBladeFrame(float2 xFacingXZ, float fTiltRad)
{
	Flux_GrassBladeFrame xFrame;
	float2 xFacing2 = normalize(xFacingXZ + float2(1e-5, 0.0));
	xFrame.m_xFacing = float3(xFacing2.x, 0.0, xFacing2.y);
	xFrame.m_xSide   = normalize(cross(float3(0.0, 1.0, 0.0), xFrame.m_xFacing));
	xFrame.m_xGrow   = normalize(float3(0.0, 1.0, 0.0) * cos(fTiltRad) + xFrame.m_xFacing * sin(fTiltRad));
	return xFrame;
}

// Minimum on-screen width. A blade thinner than one pixel aliases into a crawling
// dotted line under any temporal filter, so the width is widened (never narrowed)
// until it covers m_fMinPixelWidth. The clip w of the blade centre is the only
// depth term needed: pixels-per-metre = proj[0][0] / w * 0.5 * screenWidth.
float Flux_GrassThickenWidth(Flux_GrassVertexContext xCtx, float3 xCentreWS, float fWidth)
{
	float4 xView = mul(xCtx.m_xViewMat, float4(xCentreWS, 1.0));
	float4 xClip = mul(xCtx.m_xProjMat, xView);
	float  fW    = max(abs(xClip.w), 1e-4);
	float  fPixelsPerMetre = abs(xCtx.m_xProjMat._m00) / fW * 0.5 / max(xCtx.m_xRcpScreenDims.x, 1e-8);
	float  fWidthPixels    = max(fWidth * fPixelsPerMetre, 1e-4);
	float  fClamp          = max(1.0, xCtx.m_fMinPixelWidth / fWidthPixels);
	return fWidth * lerp(1.0, fClamp, saturate(xCtx.m_fThickenBlend));
}

// THE shared blade vertex. ToGBuffer, ToGBufferVelocity and ToShadowmap all call
// this and differ only in what they do with the result — one pose path, three
// callers, so a swaying blade can never shadow itself out of alignment. The
// velocity shell calls it a second time with the PREVIOUS frame's wind constants;
// everything time-dependent must therefore enter through xCtx.m_xWind and nowhere
// else, or the motion vector reports a pose the blade never held.
Flux_GrassVertexOut Flux_GrassBuildBladeVertex(Flux_GrassBladeInstance xBlade,
	Flux_GrassTypeParamsGPU xType, uint uVertexId, Flux_GrassVertexContext xCtx)
{
	Flux_GrassVertexOut o;

	uint  uSafeVert = min(uVertexId, kGRASS_VERTEX_COUNT - 1u);
	float fU        = kafGRASS_VERT_U[uSafeVert];
	float fSide     = kafGRASS_VERT_SIDE[uSafeVert];
	bool  bPieceB   = kauGRASS_VERT_PIECE[uSafeVert] != 0u;
	float fT        = Flux_GrassRemapHeightT(fU, xType.m_fVertexDistributionPow);

	float3 xRoot     = xBlade.m_xPosWS;
	float  fDistance = distance(xRoot, xCtx.m_xCameraPosWS);

	// Lattice-class fade: classes 1-3 shrink AND sink over their staggered bands so
	// they leave without popping. Class 0 never fades (the LO lattice reproduces it).
	uint  uClass = Flux_GrassTypeFlagsLatticeClass(xBlade.m_uTypeFlags);
	float fFade  = Flux_GrassLatticeFade(uClass, fDistance, xCtx.m_fHiRadius);
	float fHeight = xBlade.m_fHeight * fFade;
	xRoot.y -= (1.0 - fFade) * xBlade.m_fHeight * 0.5;

	// Convergence on the LO silhouette over the last band of the HI radius. The LO
	// strip is drawn from the same 15 verts, so the silhouette difference is the fold
	// break and the lateral curl — both are annealed to zero here, which is what makes
	// the HI->LO swap invisible instead of a shape pop.
	float fBandStart = xCtx.m_fHiRadius * (1.0 - saturate(xCtx.m_fLODBlendBand));
	float fLODBlend  = saturate((fDistance - fBandStart) / max(xCtx.m_fHiRadius - fBandStart, 1e-3));
	if (Flux_GrassTypeFlagsIsLOMesh(xBlade.m_uTypeFlags)) fLODBlend = 1.0;

	Flux_GrassBladeFrame xFrame = Flux_GrassMakeBladeFrame(xBlade.m_xFacingXZ, xBlade.m_fTiltRad);

	// Wind: the record carries the RAW gust strength sampled at the base, so the
	// per-type response and stiffness apply here and the record stays a pure mirror of
	// Flux_SampleWindStrength for the CPU-side test.
	float  fWindGain = xBlade.m_fWindStrength * xType.m_fWindResponse / (1.0 + max(xType.m_fStiffness, 0.0));
	float3 xWindDir  = float3(xCtx.m_xWind.m_xDirectionXZ.x, 0.0, xCtx.m_xWind.m_xDirectionXZ.y);
	float3 xWindTip  = xWindDir * fWindGain;

	float  fSideCurve = xBlade.m_fSideCurve * (1.0 - fLODBlend);

	float3 xP0 = xRoot;
	float3 xP3 = xRoot + xFrame.m_xGrow * fHeight + xWindTip;
	float3 xP1 = xRoot + xFrame.m_xGrow * (fHeight * (1.0 / 3.0));
	float3 xP2 = xRoot + xFrame.m_xGrow * (fHeight * (2.0 / 3.0))
			   + xFrame.m_xFacing * (xBlade.m_fBend * fHeight)
			   + xFrame.m_xSide * (fSideCurve * fHeight)
			   + xWindTip * (4.0 / 9.0);   // (2/3)^2 — the tip deflection weighted by height squared

	float3 xSpine   = Flux_GrassBezier(xP0, xP1, xP2, xP3, fT);
	float3 xTangent = normalize(Flux_GrassBezierTangent(xP0, xP1, xP2, xP3, fT) + float3(0.0, 1e-5, 0.0));

	// Per-vertex bob. Phase is keyed off the blade hash, so neighbouring blades never
	// beat in unison, and the amplitude is height-squared weighted inside the helper.
	float fDetail = Flux_SampleWindDetail(xCtx.m_xWind, xRoot.xz, xBlade.m_uHashBits, fT);
	xSpine += xFrame.m_xSide * fDetail;

	// Fold: a rigid offset of piece B ONLY, so the two disconnected pieces separate at
	// the seam row. It anneals to zero as the blade converges on the LO silhouette,
	// which is also why a LO blade never shows a break.
	if (bPieceB && Flux_GrassTypeFlagsIsFolded(xBlade.m_uTypeFlags))
	{
		float fFold = xType.m_fFoldPushApart * fHeight * (1.0 - fLODBlend);
		xSpine += xFrame.m_xFacing * fFold - float3(0.0, fFold * 0.5, 0.0);
	}

	float fWidth = lerp(xBlade.m_fWidth, xBlade.m_fWidth * xType.m_fTipWidthFrac, fT);
	fWidth = Flux_GrassThickenWidth(xCtx, xSpine, fWidth);

	o.m_xWorldPos = xSpine + xFrame.m_xSide * (fSide * fWidth * 0.5);

	// Outward normal tilt: rotating each edge's normal away from the spine rounds a
	// flat quad into a cylinder under lighting without adding geometry.
	float  fNormalTilt = Flux_GrassUnpackUnorm2x16Y(xType.m_uColourJitterNormalTiltPacked) * 1.57079633;
	float3 xFlat       = normalize(cross(xTangent, xFrame.m_xSide));
	float3 xNormal     = normalize(xFlat * cos(fNormalTilt * abs(fSide)) + xFrame.m_xSide * (fSide * sin(fNormalTilt)));

	// Distant blades borrow the clump's normal so a field reads as one soft surface
	// rather than a million individually-lit slivers.
	float3 xClumpNormal = normalize(float3(xBlade.m_xClumpNormalXZ.x, 1.0, xBlade.m_xClumpNormalXZ.y));
	float  fBlend       = saturate(fDistance / max(xCtx.m_fNormalBlendDistance, 1e-3)) * xType.m_fClumpNormalBlendMax;
	o.m_xNormalWS = normalize(lerp(xNormal, xClumpNormal, fBlend));

	o.m_fHeightT        = fT;
	o.m_fSide           = fSide;
	o.m_fLatticeFade    = fFade;
	o.m_fCameraDistance = fDistance;
	return o;
}

// =====================================================================
// Fragment shading.
// =====================================================================

struct Flux_GrassFragmentIn
{
	float3 m_xWorldPos;
	float3 m_xNormalWS;
	float  m_fHeightT;
	float  m_fSide;
	float  m_fCameraDistance;
	uint   m_uTypeFlags;
	uint   m_uClumpPacked;
};

static const uint kGRASS_DEBUG_NONE           = 0u;
static const uint kGRASS_DEBUG_TYPE_INDEX     = 1u;
static const uint kGRASS_DEBUG_CLUMP          = 2u;
static const uint kGRASS_DEBUG_HEIGHT_T       = 3u;
static const uint kGRASS_DEBUG_LOD_MESH       = 4u;
static const uint kGRASS_DEBUG_LATTICE_CLASS  = 5u;
static const uint kGRASS_DEBUG_NORMALS        = 6u;

float3 Flux_GrassDebugColour(uint uMode, Flux_GrassFragmentIn xIn)
{
	if (uMode == kGRASS_DEBUG_TYPE_INDEX)
	{
		uint uHash = Flux_NoiseHashUInt(Flux_GrassTypeFlagsIndex(xIn.m_uTypeFlags) + 1u);
		return float3((float)(uHash & 0xFFu), (float)((uHash >> 8u) & 0xFFu), (float)((uHash >> 16u) & 0xFFu)) / 255.0;
	}
	if (uMode == kGRASS_DEBUG_CLUMP)
	{
		return float3(Flux_GrassUnpackClumpHash01(xIn.m_uClumpPacked), Flux_GrassUnpackClumpDist01(xIn.m_uClumpPacked), 0.0);
	}
	if (uMode == kGRASS_DEBUG_HEIGHT_T)      return float3(xIn.m_fHeightT, xIn.m_fHeightT, xIn.m_fHeightT);
	if (uMode == kGRASS_DEBUG_LOD_MESH)      return Flux_GrassTypeFlagsIsLOMesh(xIn.m_uTypeFlags) ? float3(1.0, 0.4, 0.0) : float3(0.0, 0.8, 0.2);
	if (uMode == kGRASS_DEBUG_LATTICE_CLASS)
	{
		uint uClass = Flux_GrassTypeFlagsLatticeClass(xIn.m_uTypeFlags);
		if (uClass == 0u) return float3(1.0, 1.0, 1.0);
		if (uClass == 1u) return float3(1.0, 0.0, 0.0);
		if (uClass == 2u) return float3(0.0, 1.0, 0.0);
		return float3(0.0, 0.0, 1.0);
	}
	return xIn.m_xNormalWS * 0.5 + 0.5;
}

// THE shared grass G-buffer write. Both lit shells call this; the velocity shell
// promotes the result with MakeGBufferVelocity so the shading can never diverge
// between the two MRT counts.
//
// Textures come from BINDLESS indices on the type record and are OPTIONAL —
// kGRASS_BINDLESS_UNBOUND must short-circuit before the table lookup, because
// GetBindlessTexture(0xFFFFFFFF) is an out-of-range descriptor read.
GBufferOut Flux_GrassShadeBlade(Flux_GrassFragmentIn xIn, Flux_GrassTypeParamsGPU xType,
	float fGlossCutDistance, uint uDebugMode, bool bFrontFace)
{
	// Grass is drawn double-sided; the lighting normal must face the viewer or every
	// back-facing blade in a field reads as a black sliver.
	float3 xNormal = bFrontFace ? xIn.m_xNormalWS : -xIn.m_xNormalWS;

	float fT           = saturate(xIn.m_fHeightT);
	float fClumpHash01 = Flux_GrassUnpackClumpHash01(xIn.m_uClumpPacked);
	float fClumpDist01 = Flux_GrassUnpackClumpDist01(xIn.m_uClumpPacked);
	uint  uFSHash      = Flux_GrassTypeFlagsFragmentHash(xIn.m_uTypeFlags);

	// Base -> tip colour with a per-blade tint jitter. 8-bit type colours are below
	// the noise floor of this jitter, which is why the type block stores them packed.
	float3 xBase   = Flux_GrassUnpackColourRGB(xType.m_uBaseColourPacked);
	float3 xTip    = Flux_GrassUnpackColourRGB(xType.m_uTipColourPacked);
	float3 xAlbedo = lerp(xBase, xTip, fT);

	float fJitterAmount = Flux_GrassUnpackUnorm2x16X(xType.m_uColourJitterNormalTiltPacked);
	float fJitter       = (Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uFSHash + 7919u)) - 0.5) * 2.0 * fJitterAmount;
	xAlbedo *= (1.0 + fJitter);

	// 2D colour ramp: u picks the clump's variant, v walks base -> tip. Sampling by
	// clump hash (not blade hash) is what makes a clump read as one plant.
	if (xType.m_uRampTextureIndex != kGRASS_BINDLESS_UNBOUND)
	{
		xAlbedo *= GetBindlessTexture(xType.m_uRampTextureIndex).Sample(float2(fClumpHash01, fT)).rgb;
	}

	// Vein/streak detail across the blade width.
	if (xType.m_uVeinTextureIndex != kGRASS_BINDLESS_UNBOUND)
	{
		float2 xVeinUV = float2(xIn.m_fSide * 0.5 + 0.5, fT);
		xAlbedo *= GetBindlessTexture(xType.m_uVeinTextureIndex).Sample(xVeinUV).rgb;
	}

	// Gloss streaks run ALONG the blade, so the 1D lookup is indexed by height only.
	// Beyond the cut distance the streak is sub-pixel and only produces sparkle, so it
	// is annealed out rather than filtered.
	float fRoughness = xType.m_fRoughnessBase;
	if (xType.m_uGlossTextureIndex != kGRASS_BINDLESS_UNBOUND)
	{
		float fGloss    = GetBindlessTexture(xType.m_uGlossTextureIndex).Sample(float2(fT * xType.m_fGlossRepeat, 0.5)).r;
		float fDistCut  = 1.0 - saturate(xIn.m_fCameraDistance / max(fGlossCutDistance, 1e-3));
		fRoughness      = saturate(fRoughness - fGloss * 0.45 * fDistCut);
	}

	// Analytic AO: the blade base sits inside the canopy and is occluded; the occlusion
	// releases by the height the type declares.
	float fAOBase = Flux_GrassUnpackUnorm2x16X(xType.m_uAOPacked);
	float fAOTipT = Flux_GrassUnpackUnorm2x16Y(xType.m_uAOPacked);
	float fAmbient = lerp(1.0 - fAOBase, 1.0, saturate(fT / max(fAOTipT, 1e-3)));
	fAmbient *= lerp(1.0, 0.85, fClumpDist01);   // clump interiors are shaded by their neighbours

	// Analytic translucency into the emissive target: SUBSURFACE shading gives the
	// wrap diffuse, but the back-lit glow through a thin blade is view-dependent and
	// has to be evaluated here.
	float  fTransBase = Flux_GrassUnpackUnorm2x16X(xType.m_uTranslucencyPacked);
	float  fTransTip  = Flux_GrassUnpackUnorm2x16Y(xType.m_uTranslucencyPacked);
	float  fTrans     = lerp(fTransBase, fTransTip, fT);
	float3 xToLight   = normalize(-GetSunDir());
	float3 xViewDir   = normalize(GetCamPos() - xIn.m_xWorldPos);
	float  fBackLit   = saturate(-dot(xNormal, xToLight));
	float  fThrough   = pow(saturate(dot(-xViewDir, xToLight)), 4.0);
	float3 xEmissive  = xAlbedo * GetSunColour() * GetSunIntensity() * fTrans * (fBackLit * 0.35 + fThrough * 0.65);

	if (uDebugMode != kGRASS_DEBUG_NONE)
	{
		return MakeGBuffer(float4(Flux_GrassDebugColour(uDebugMode, xIn), 1.0), xNormal, 1.0, 1.0, 0.0);
	}

	return MakeGBuffer(float4(xAlbedo, 1.0), xNormal, fAmbient, fRoughness, 0.0,
		xType.m_fSpecular, xEmissive, 0.0, 0.0, GBUFFER_SHADING_SUBSURFACE);
}

#endif // FLUX_VEGETATION_GRASSCOMMON_SLANG
