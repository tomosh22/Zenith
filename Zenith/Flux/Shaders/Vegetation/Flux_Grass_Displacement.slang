// =====================================================================
// Flux_Grass_Displacement — Slang module (GPU-driven grass).
//
// Maintains the camera-anchored grass displacement field: a 256x256 RG16F
// ping-pong holding a world-XZ push vector per texel, which the placement pass
// samples bilinearly at each blade base and turns into a facing/tilt/height push.
//
// Two things happen here and nothing else:
//   RE-ANCHOR — the field is anchored to a scrolling origin, so a texel of the NEXT
//     map is resolved to a world position and read back out of the PREV map at that
//     world position. Texels the previous anchor did not cover read as zero, which
//     is why the scroll must be at most one map per frame or the whole field blanks.
//   DECAY — the field relaxes toward zero so a mover leaves a trail that closes
//     behind it rather than a permanent scar.
//
// No mover is splatted yet. A mover splat belongs immediately after the decay below
// and must be ADDITIVE on top of the decayed value and clamped to the same range, or
// re-splatting a stationary mover each frame integrates to an unbounded push.
// g_uMoverCount is 0 until movers are submitted.
// =====================================================================

#include "Vegetation/Flux_GrassCommon.slang"   // spine sets 0/1/2

static const uint kGRASS_DISPLACEMENT_RESOLUTION = 256u;

struct GrassDisplacementConstantsLayout
{
	float4 g_xPrevOriginXZ_Size_Pad;   //  0 : xy = previous anchor origin (m), z = map world size (m), w = 0
	float4 g_xNextOriginXZ_Size_Pad;   // 16 : xy = this frame's anchor origin (m), z = map world size (m), w = 0
	float4 g_xDecay_Texel_Pad;         // 32 : x = per-frame retention factor, y = texel size in UV, zw = 0
	uint4  g_xResolution_MoverCount;   // 48 : xy = map resolution in texels, z = mover count, w = 0
};                                     // 64 B

// PASS block (set 3). Ping-pong: the previous field is an SRV, this frame's is a
// UAV. They must be distinct resources — reading and writing one texture across a
// dispatch has no defined ordering between texels.
struct PassParams
{
	ConstantBuffer<GrassDisplacementConstantsLayout> GrassDisplacementConstants;
	Sampler2D           g_xPrevDisplacementTex;   // RG16F, bilinear, clamp
	RWTexture2D<float4> g_xNextDisplacementTex;   // RG16F storage image
};
ParameterBlock<PassParams> g_xPassSet;

// ============================================================================
// Decay and re-anchor the grass displacement field.
// Workgroup: 8x8x1 (64 threads — one texel each, a 32x32 group grid over 256x256)
// Shared memory: none
//
// Resolves each texel of the new field to a world position, reads the previous
// field there, decays it and writes the result. Texels outside the previous anchor
// resolve to zero.
// ============================================================================
[shader("compute")]
[numthreads(8, 8, 1)]
void csMain(uint3 xDispatchId : SV_DispatchThreadID)
{
	uint2 xResolution = g_xPassSet.GrassDisplacementConstants.g_xResolution_MoverCount.xy;
	uint2 xTexel = xDispatchId.xy;
	if (xTexel.x >= xResolution.x || xTexel.y >= xResolution.y) return;

	float4 xNext = g_xPassSet.GrassDisplacementConstants.g_xNextOriginXZ_Size_Pad;
	float4 xPrev = g_xPassSet.GrassDisplacementConstants.g_xPrevOriginXZ_Size_Pad;

	// Texel centre -> world position under THIS frame's anchor.
	float2 xUV = (float2(xTexel) + 0.5) / float2(xResolution);
	float2 xWorldXZ = xNext.xy + xUV * xNext.z;

	// ...and back to a UV under the PREVIOUS anchor.
	float2 xPrevUV = (xWorldXZ - xPrev.xy) / max(xPrev.z, 1e-3);
	bool   bInsidePrev = all(xPrevUV >= 0.0) && all(xPrevUV <= 1.0);

	float2 xPush = bInsidePrev ? g_xPassSet.g_xPrevDisplacementTex.SampleLevel(xPrevUV, 0.0).rg : float2(0.0, 0.0);
	xPush *= g_xPassSet.GrassDisplacementConstants.g_xDecay_Texel_Pad.x;

	g_xPassSet.g_xNextDisplacementTex[xTexel] = float4(xPush, 0.0, 0.0);
}
