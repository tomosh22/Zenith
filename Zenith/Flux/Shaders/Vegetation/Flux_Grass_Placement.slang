// =====================================================================
// Flux_Grass_Placement — Slang module (GPU-driven grass).
//
// ONE thread per LATTICE NODE across every dispatched tile. A node either grows a
// blade or it does not; there is no per-tile sample count, so the node's hash fully
// determines it and a blade is a pure function of its world position. Nothing here
// may carry state across frames or read wall-clock time: a blade that changes
// identity between frames flickers under TAA.
//
// Thread decode is TILE-MAJOR: tile = tid / kGRASS_TILE_CELL_COUNT,
// cell = tid % kGRASS_TILE_CELL_COUNT, so one dispatch covers every selected tile
// and the tile record supplies the global lattice base + stride.
//
// The chain, in order:
//   hash + jitter -> coverage -> type gather + dithered pick -> distance/band cull
//   -> height + central-difference slope -> Voronoi clump (9 jittered cells)
//   -> facing blend -> shape -> slope lean -> wind field -> displacement push
//   -> fold decision -> per-view sphere/frustum -> pool append + per-partition
//      visible-index append
//
// The pose written to the blade record is VIEW-INDEPENDENT: it is computed once
// here and every partition that accepts the blade draws that same pose. Only
// oscillation, LOD blend, thickening and normal blend are per-vertex.
// =====================================================================

#include "Vegetation/Flux_GrassCommon.slang"   // spine sets 0/1/2 + the records, tables and partition contract

struct GrassPlacementConstantsLayout
{
	// View-major: camera, cascade 0, cascade 1, six INWARD-pointing planes each.
	Flux_GrassFrustumPlane g_axFrustumPlanes[18];  //   0 : 288 B
	Flux_GrassWindBlock    g_xWind;                // 288 :  48 B
	float4 g_xCameraPosWS_HiRadius;                // 336 : xyz = camera WS, w = HI radius (m)
	// Map footprints are supplied as constants — never GetDimensions.
	float4 g_xCoverageMapParams;                   // 352 : x = world size (m), y = 1/world size, z = density scale, w = coverage bias
	float4 g_xTypeMapParams;                       // 368 : x = world size (m), y = 1/world size, z = texel size in UV, w = type count
	float4 g_xHeightMapParams;                     // 384 : x = world size (m), y = 1/world size, z = height scale (m), w = height bias (m)
	float4 g_xSlopeStep_Pad;                       // 400 : x = central-difference step (m), yzw = 0
	float4 g_xDisplacementParams;                  // 416 : xy = map origin XZ (m), z = map world size (m), w = push scale (m)
	float4 g_xLoRadius_MaxDist_Density_Pad;        // 432 : LO outer radius (m), global max draw distance (m), global density scale, 0
	uint4  g_xTileCount_Seed_ActiveSlots_PoolCap;  // 448 : tile count, hash seed, active-slot bitmask, blade-pool capacity
};                                                 // 464 B

// PASS block (set 3). The three maps are sampled with SampleLevel because implicit
// derivatives are not available in a compute thread. g_xTypeMap MUST be bound with a
// POINT sampler: its texel value IS a type index, and a lerp between type 0 and
// type 4 selects type 2 — a type the author never placed there.
struct PassParams
{
	ConstantBuffer<GrassPlacementConstantsLayout> GrassPlacementConstants;
	StructuredBuffer<Flux_GrassTypeParamsGPU>     TypeParams;       // [type count]
	StructuredBuffer<Flux_GrassTileGPU>           Tiles;            // [tile count]
	Sampler2D g_xHeightMap;                                         // R: terrain height, normalised
	Sampler2D g_xCoverageMap;                                       // R: painted grass density [0,1]
	Sampler2D g_xTypeMap;                                           // R: grass type index / 255 (POINT)
	Sampler2D g_xDisplacementMap;                                   // RG: mover push in world XZ (m)
	RWStructuredBuffer<Flux_GrassBladeInstance>   BladePool;        // [pool capacity]
	RWStructuredBuffer<uint>                      BladeCounter;     // [1] append cursor
	RWStructuredBuffer<uint>                      VisibleIndices;   // [kGRASS_VISIBLE_INDEX_CAPACITY]
	RWStructuredBuffer<uint>                      IndirectArgs;     // [16 * 5]
};
ParameterBlock<PassParams> g_xPassSet;

static const float kGRASS_TAU = 6.28318530718;

// ---------------------------------------------------------------------
// Map sampling
// ---------------------------------------------------------------------

// Maps cover [0, worldSize] on both axes from the world origin — the same
// convention as the painted density map the terrain editor writes.
float2 Flux_GrassMapUV(float2 xWorldXZ, float fInvWorldSize)
{
	return saturate(xWorldXZ * fInvWorldSize);
}

float Flux_GrassSampleHeight(float2 xWorldXZ)
{
	float4 xParams = g_xPassSet.GrassPlacementConstants.g_xHeightMapParams;
	float  fRaw = g_xPassSet.g_xHeightMap.SampleLevel(Flux_GrassMapUV(xWorldXZ, xParams.y), 0.0).r;
	return fRaw * xParams.z + xParams.w;
}

// Central differences over the height map. A forward difference biases every slope
// half a step downhill, which tilts a whole field consistently toward +X/+Z.
float3 Flux_GrassGroundNormal(float2 xWorldXZ)
{
	float fStep = max(g_xPassSet.GrassPlacementConstants.g_xSlopeStep_Pad.x, 1e-3);
	float fXm = Flux_GrassSampleHeight(xWorldXZ - float2(fStep, 0.0));
	float fXp = Flux_GrassSampleHeight(xWorldXZ + float2(fStep, 0.0));
	float fZm = Flux_GrassSampleHeight(xWorldXZ - float2(0.0, fStep));
	float fZp = Flux_GrassSampleHeight(xWorldXZ + float2(0.0, fStep));
	return normalize(float3(fXm - fXp, 2.0 * fStep, fZm - fZp));
}

float Flux_GrassSampleCoverage(float2 xWorldXZ)
{
	float4 xParams = g_xPassSet.GrassPlacementConstants.g_xCoverageMapParams;
	float  fRaw = g_xPassSet.g_xCoverageMap.SampleLevel(Flux_GrassMapUV(xWorldXZ, xParams.y), 0.0).r;
	return saturate(fRaw * xParams.z + xParams.w);
}

// Dithered type pick over the four texels of the bilinear footprint. Filtering an
// index map is meaningless, so the bilinear WEIGHTS are used as a probability
// distribution and one whole texel is chosen — the boundary between two types
// dissolves into a stochastic mix of the two instead of inventing a third.
uint Flux_GrassPickType(float2 xWorldXZ, uint uHash)
{
	float4 xParams = g_xPassSet.GrassPlacementConstants.g_xTypeMapParams;
	float  fTexel  = max(xParams.z, 1e-6);
	float2 xUV     = Flux_GrassMapUV(xWorldXZ, xParams.y);
	float2 xTexelSpace = xUV / fTexel - 0.5;
	float2 xBase   = floor(xTexelSpace);
	float2 xFrac   = xTexelSpace - xBase;

	float afWeight[4];
	afWeight[0] = (1.0 - xFrac.x) * (1.0 - xFrac.y);
	afWeight[1] = xFrac.x * (1.0 - xFrac.y);
	afWeight[2] = (1.0 - xFrac.x) * xFrac.y;
	afWeight[3] = xFrac.x * xFrac.y;

	// Weights sum to 1, so falling out of the loop lands on the last bucket.
	float fPick  = Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uHash + 5431u));
	float fAccum = 0.0;
	uint  uChosen = 3u;
	for (uint u = 0u; u < 3u; u++)
	{
		fAccum += afWeight[u];
		if (fPick <= fAccum) { uChosen = u; break; }
	}

	float2 xOffset = float2((float)(uChosen & 1u), (float)(uChosen >> 1u));
	float2 xSampleUV = (xBase + xOffset + 0.5) * fTexel;
	float  fIndex = g_xPassSet.g_xTypeMap.SampleLevel(saturate(xSampleUV), 0.0).r;

	uint uTypeCount = max((uint)xParams.w, 1u);
	return min((uint)(fIndex * 255.0 + 0.5), uTypeCount - 1u);
}

// ---------------------------------------------------------------------
// Clump
// ---------------------------------------------------------------------

struct Flux_GrassClump
{
	float2 m_xCentre;
	float2 m_xNormalXZ;    // unit direction from the centre toward the blade
	float  m_fDist01;      // distance to the centre, normalised by the cell size
	float  m_fHash01;
};

// Voronoi over the 3x3 neighbourhood of jittered clump sites. Nine cells is the
// minimum that guarantees the true nearest site is found for any jitter inside a
// cell; a 2x2 search misses the nearest whenever the blade sits near a corner and
// produces visible clump seams along the cell grid.
Flux_GrassClump Flux_GrassFindClump(float2 xWorldXZ, float fClumpScale, uint uSeed)
{
	float fScale = max(fClumpScale, 0.01);
	int2  xCell  = int2(floor(xWorldXZ / fScale));

	Flux_GrassClump xClump;
	xClump.m_xCentre = xWorldXZ;
	xClump.m_fHash01 = 0.0;
	float fBestDistSq = 1e30;
	uint  uBestHash = 0u;

	for (int iDZ = -1; iDZ <= 1; iDZ++)
	{
		for (int iDX = -1; iDX <= 1; iDX++)
		{
			int2 xNeighbour = xCell + int2(iDX, iDZ);
			uint uHash = Flux_NoiseHashCoords(xNeighbour.x, xNeighbour.y, uSeed + 7717u);
			float2 xJitter = float2(Flux_NoiseHashToFloat01(uHash),
									 Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uHash)));
			float2 xSite = (float2(xNeighbour) + xJitter) * fScale;
			float2 xDelta = xWorldXZ - xSite;
			float  fDistSq = dot(xDelta, xDelta);
			if (fDistSq < fBestDistSq)
			{
				fBestDistSq = fDistSq;
				xClump.m_xCentre = xSite;
				uBestHash = uHash;
			}
		}
	}

	float2 xOut = xWorldXZ - xClump.m_xCentre;
	float  fLen = length(xOut);
	xClump.m_xNormalXZ = fLen > 1e-5 ? xOut / fLen : float2(1.0, 0.0);
	xClump.m_fDist01   = saturate(fLen / fScale);
	xClump.m_fHash01   = Flux_NoiseHashToFloat01(uBestHash);
	return xClump;
}

// ---------------------------------------------------------------------
// Culling
// ---------------------------------------------------------------------

bool Flux_GrassTestSphereFrustum(uint uFrustum, float3 xCentre, float fRadius)
{
	const int kaPlaneOrder[6] = { 4, 5, 0, 1, 2, 3 };   // near, far, then sides
	uint uBase = uFrustum * 6u;
	for (int i = 0; i < 6; i++)
	{
		Flux_GrassFrustumPlane xPlane = g_xPassSet.GrassPlacementConstants.g_axFrustumPlanes[uBase + (uint)kaPlaneOrder[i]];
		if (dot(xPlane.m_xNormalAndDistance.xyz, xCentre) + xPlane.m_xNormalAndDistance.w < -fRadius) return false;
	}
	return true;
}

// Reserve this survivor's slot in (partition uSlot) and record the blade index.
// The InterlockedAdd is unconditional so instanceCount doubles as the output cursor
// AND the draw's instance count; an over-budget frame therefore leaves the count
// above the cap, which Flux_Grass_IndirectFixup clamps before the draw reads it.
void Flux_GrassAppendVisible(uint uSlot, uint uBladeIndex)
{
	uint uLocal;
	InterlockedAdd(g_xPassSet.IndirectArgs[uSlot * kGRASS_INDIRECT_WORDS + 1u], 1u, uLocal);
	if (uLocal >= kauGRASS_PARTITION_CAP[uSlot]) return;
	g_xPassSet.VisibleIndices[kauGRASS_PARTITION_BASE[uSlot] + uLocal] = uBladeIndex;
}

// ============================================================================
// Generate, shape and cull one grass blade per lattice node.
// Workgroup: 64x1x1 (64 threads — one wave/half-wave, and one row of a 64-cell tile)
// Shared memory: none
//
// Decodes the thread to a (tile, lattice node), runs the acceptance/shape chain and,
// on survival, appends the blade record to the shared pool and its index to every
// partition whose frustum wants it.
// ============================================================================
[shader("compute")]
[numthreads(64, 1, 1)]
void csMain(uint3 xDispatchId : SV_DispatchThreadID)
{
	uint uTileCount = g_xPassSet.GrassPlacementConstants.g_xTileCount_Seed_ActiveSlots_PoolCap.x;
	uint uSeed      = g_xPassSet.GrassPlacementConstants.g_xTileCount_Seed_ActiveSlots_PoolCap.y;
	uint uSlotMask  = g_xPassSet.GrassPlacementConstants.g_xTileCount_Seed_ActiveSlots_PoolCap.z;
	uint uPoolCap   = g_xPassSet.GrassPlacementConstants.g_xTileCount_Seed_ActiveSlots_PoolCap.w;

	uint uThread = xDispatchId.x;
	if (uThread >= uTileCount * kGRASS_TILE_CELL_COUNT) return;

	uint uTile = uThread / kGRASS_TILE_CELL_COUNT;
	uint uCell = uThread - uTile * kGRASS_TILE_CELL_COUNT;
	Flux_GrassTileGPU xTile = g_xPassSet.Tiles[uTile];

	int2 xCellXZ = int2((int)(uCell % kGRASS_TILE_CELLS), (int)(uCell / kGRASS_TILE_CELLS));
	int2 xLattice = int2(xTile.m_iLatticeBaseX, xTile.m_iLatticeBaseZ) + xCellXZ * (int)xTile.m_uLatticeStride;

	// --- hash + jitter ---
	uint   uHash = Flux_NoiseHashCoords(xLattice.x, xLattice.y, uSeed);
	float  fStep = kGRASS_HI_LATTICE_STEP * (float)xTile.m_uLatticeStride;
	float2 xNode = Flux_GrassLatticeToWorld(xLattice);
	float2 xJitter = float2(Flux_NoiseHashToFloat01(uHash),
							 Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uHash))) - 0.5;
	float2 xWorldXZ = xNode + xJitter * fStep;

	float3 xCameraPos = g_xPassSet.GrassPlacementConstants.g_xCameraPosWS_HiRadius.xyz;
	float  fHiRadius  = g_xPassSet.GrassPlacementConstants.g_xCameraPosWS_HiRadius.w;
	bool   bLoTile    = xTile.m_uIsLoLOD != 0u;

	// A LO tile skips whatever a HI tile already emitted. Both sides answer with the
	// same purely-geometric test, or the seam double-draws or gaps.
	if (bLoTile && Flux_GrassIsInsideHiRegion(xCameraPos, xWorldXZ, fHiRadius)) return;
	if (!bLoTile && !Flux_GrassIsInsideHiRegion(xCameraPos, xWorldXZ, fHiRadius)) return;

	// --- coverage ---
	float fCoverage = Flux_GrassSampleCoverage(xWorldXZ)
					* g_xPassSet.GrassPlacementConstants.g_xLoRadius_MaxDist_Density_Pad.z;
	if (Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uHash + 1301u)) >= fCoverage) return;

	// --- type gather + dithered pick ---
	uint uTypeIndex = Flux_GrassPickType(xWorldXZ, uHash);
	Flux_GrassTypeParamsGPU xType = g_xPassSet.TypeParams[uTypeIndex];
	if (Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uHash + 2609u)) >= xType.m_fDensity) return;

	// --- distance / band cull ---
	float fMaxDistance = min(xType.m_fMaxDrawDistance,
							 g_xPassSet.GrassPlacementConstants.g_xLoRadius_MaxDist_Density_Pad.y);
	float2 xToCameraXZ = xWorldXZ - xCameraPos.xz;
	float  fPlanarDist = length(xToCameraXZ);
	if (fPlanarDist > fMaxDistance) return;

	// The lattice-class fade is evaluated here as well as in the vertex stage: a blade
	// whose class has already faded out must not consume a pool slot or an indirect
	// instance, and the vertex stage must reach zero size at exactly the same distance.
	uint  uLatticeClass = Flux_GrassLatticeClass(xLattice.x, xLattice.y);
	float fClassFade    = Flux_GrassLatticeFade(uLatticeClass, fPlanarDist, fHiRadius);
	if (fClassFade <= 0.0) return;

	// --- height + central-difference slope ---
	float  fGroundY = Flux_GrassSampleHeight(xWorldXZ);
	float3 xGroundNormal = Flux_GrassGroundNormal(xWorldXZ);
	float  fSlopeSin = length(xGroundNormal.xz);
	if (fSlopeSin > xType.m_fSlopeMax) return;

	// --- Voronoi clump ---
	Flux_GrassClump xClump = Flux_GrassFindClump(xWorldXZ, xType.m_fClumpScale, uSeed);
	xWorldXZ = lerp(xWorldXZ, xClump.m_xCentre, saturate(xType.m_fClumpPullToCentre));

	// --- facing blend ---
	float  fBladeAngle = Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uHash + 3413u)) * kGRASS_TAU;
	float2 xBladeFacing = float2(cos(fBladeAngle), sin(fBladeAngle));
	float  fClumpAngle = xClump.m_fHash01 * kGRASS_TAU;
	float2 xClumpFacing = float2(cos(fClumpAngle), sin(fClumpAngle));
	float2 xFacing = lerp(xBladeFacing, xClumpFacing, saturate(xType.m_fClumpFacingWeight))
				   + xClump.m_xNormalXZ * xType.m_fClumpOutwardWeight;
	xFacing = normalize(xFacing + float2(1e-5, 0.0));

	// --- shape ---
	float fShapeA = Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uHash + 4703u));
	float fShapeB = Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uHash + 6151u));
	float fShapeC = Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uHash + 8171u));
	float fHeight = lerp(xType.m_fHeightMin, xType.m_fHeightMax, fShapeA);
	float fWidth  = lerp(xType.m_fWidthMin,  xType.m_fWidthMax,  fShapeB);
	float fTilt   = lerp(xType.m_fTiltMinRad, xType.m_fTiltMaxRad, fShapeC);
	float fBend   = lerp(xType.m_fBendMin,   xType.m_fBendMax,   fShapeA * fShapeC);
	float fSideCurve = xType.m_fSideCurve * (fShapeB * 2.0 - 1.0);
	// Clump centres grow tallest, which is what reads as a tuft rather than a lawn.
	fHeight *= 1.0 + xType.m_fClumpHeightBoost * (1.0 - xClump.m_fDist01);

	// --- slope lean ---
	// Lean toward the ground normal by the type's alignment weight. A fully-aligned
	// blade on a steep face grows perpendicular to the slope; an unaligned one stays
	// vertical no matter what it is standing on.
	float fSlopeTilt = acos(clamp(xGroundNormal.y, -1.0, 1.0));
	fTilt = fTilt + fSlopeTilt * saturate(xType.m_fSlopeAlign);
	if (fSlopeSin > 1e-4)
	{
		float2 xDownhill = -normalize(xGroundNormal.xz);
		xFacing = normalize(lerp(xFacing, xDownhill, saturate(xType.m_fSlopeAlign) * fSlopeSin));
	}

	// --- wind field ---
	Flux_WindConstants xWind = Flux_GrassDecodeWind(g_xPassSet.GrassPlacementConstants.g_xWind);
	float fWindStrength = Flux_SampleWindStrength(xWind, xWorldXZ);

	// --- displacement push ---
	// The map is neutral (zero) until movers are submitted, so this is inert rather
	// than conditional — a branch here would diverge across a whole wave for nothing.
	float4 xDispParams = g_xPassSet.GrassPlacementConstants.g_xDisplacementParams;
	float2 xDispUV = saturate((xWorldXZ - xDispParams.xy) / max(xDispParams.z, 1e-3));
	float2 xPush = g_xPassSet.g_xDisplacementMap.SampleLevel(xDispUV, 0.0).rg * xDispParams.w;
	float  fPushMag = length(xPush);
	if (fPushMag > 1e-4)
	{
		float2 xPushDir = xPush / fPushMag;
		float  fPush01 = saturate(fPushMag);
		xFacing = normalize(lerp(xFacing, xPushDir, fPush01));
		fTilt  += fPush01 * 1.2;
		fHeight *= 1.0 - fPush01 * 0.35;   // a pushed blade is flattened, not stretched
	}

	// --- fold decision ---
	// A LO blade never folds: the break would be sub-pixel at LO distance and the LO
	// strip straddles the seam, so the offset would only shear the strip.
	float fFoldHash = Flux_NoiseHashToFloat01(Flux_NoiseHashUInt(uHash + 9973u));
	bool  bFolded = !bLoTile && (fFoldHash > xType.m_fFoldThreshold);

	// --- per-view sphere/frustum ---
	float3 xBase = float3(xWorldXZ.x, fGroundY, xWorldXZ.y);
	float3 xSphereCentre = xBase + float3(0.0, fHeight * 0.5, 0.0);
	float  fSphereRadius = fHeight * 0.6 + fWidth;

	uint uCameraSlot = bLoTile ? kGRASS_SLOT_CAMERA_LO : kGRASS_SLOT_CAMERA_HI;
	uint uAcceptMask = 0u;
	if ((uSlotMask & (1u << uCameraSlot)) != 0u
		&& Flux_GrassTestSphereFrustum(kauGRASS_SLOT_FRUSTUM[uCameraSlot], xSphereCentre, fSphereRadius))
	{
		uAcceptMask |= 1u << uCameraSlot;
	}
	for (uint uCascade = kGRASS_SLOT_CASCADE_0; uCascade <= kGRASS_SLOT_CASCADE_1; uCascade++)
	{
		if ((uSlotMask & (1u << uCascade)) == 0u) continue;
		if (!Flux_GrassTestSphereFrustum(kauGRASS_SLOT_FRUSTUM[uCascade], xSphereCentre, fSphereRadius)) continue;
		uAcceptMask |= 1u << uCascade;
	}
	if (uAcceptMask == 0u) return;

	// --- pool append ---
	uint uBladeIndex;
	InterlockedAdd(g_xPassSet.BladeCounter[0], 1u, uBladeIndex);
	if (uBladeIndex >= uPoolCap) return;   // over budget: the counter over-reads so the host can see it

	Flux_GrassBladeInstance xBlade;
	xBlade.m_xPosWS         = xBase;
	xBlade.m_uHashBits      = uHash;
	xBlade.m_xFacingXZ      = xFacing;
	xBlade.m_fHeight        = fHeight;
	xBlade.m_fWidth         = fWidth;
	xBlade.m_fTiltRad       = fTilt;
	xBlade.m_fBend          = fBend;
	xBlade.m_fSideCurve     = fSideCurve;
	xBlade.m_fWindStrength  = fWindStrength;
	xBlade.m_xClumpNormalXZ = xClump.m_xNormalXZ;
	xBlade.m_uClumpPacked   = Flux_GrassPackClump(xClump.m_fHash01, xClump.m_fDist01);
	xBlade.m_uTypeFlags     = Flux_GrassPackTypeFlags(uTypeIndex, bFolded, bLoTile, uLatticeClass,
													  Flux_NoiseHashUInt(uHash + 12289u));
	g_xPassSet.BladePool[uBladeIndex] = xBlade;

	// --- per-partition visible append ---
	for (uint uSlot = 0u; uSlot < kGRASS_LIVE_SLOT_COUNT; uSlot++)
	{
		if ((uAcceptMask & (1u << uSlot)) == 0u) continue;
		Flux_GrassAppendVisible(uSlot, uBladeIndex);
	}
}
