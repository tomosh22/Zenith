// =====================================================================
// Flux_Grass_Displacement — Slang module (GPU-driven grass).
//
// Maintains the camera-anchored grass displacement field: a 256x256 RG16F
// ping-pong holding a world-XZ push vector per texel, which the placement pass
// samples bilinearly at each blade base and turns into a facing/tilt/height push.
// This is the ONLY thing about grass that persists between frames.
//
// Three things happen here and nothing else:
//   RE-ANCHOR — the field is anchored to a scrolling origin, so a texel of the NEXT
//     map is filled from the PREV map at an INTEGER texel offset. The offset is
//     integral by construction (the host snaps the anchor to whole texels — see
//     Flux_GrassSnapDisplacementAnchor), so the scroll is a lossless copy rather
//     than a resample; a fractional offset would blur the field a little every
//     frame and a standing trail would dissolve itself. Texels the previous anchor
//     did not cover read as zero, which is why a scroll of more than one whole map
//     in a frame blanks the field.
//   DECAY — the field relaxes toward zero so a mover leaves a trail that closes
//     behind it rather than a permanent scar.
//   SPLAT — each submitted mover adds a radial push away from its centre, ADDITIVE
//     on top of the decayed value and clamped to kGRASS_DISPLACEMENT_MAX_PUSH. Both
//     halves of that are load-bearing: additive-over-decayed is what makes a moving
//     mover leave a fading tail, and the clamp is what stops a STATIONARY one — which
//     re-splats the same texels every frame — integrating amount/(1-decay) without
//     bound.
//
// PING-PONG. prev and next are two distinct committed images whose roles swap each
// frame, and both are bound as storage images: the prev read is a point LOAD at an
// integer offset, not a filtered sample. That is what lets the render graph declare
// BOTH of them read-modify-write on this pass — one static declaration that is
// correct under either parity, because the graph never has to know which is which.
// Both carry an explicit [vk::image_format] so the load needs no
// shaderStorageImageReadWithoutFormat device feature.
// =====================================================================

#include "Vegetation/Flux_GrassCommon.slang"   // spine sets 0/1/2

static const uint  kGRASS_DISPLACEMENT_RESOLUTION = 256u;
static const uint  kGRASS_MAX_MOVERS              = 64u;
static const float kGRASS_DISPLACEMENT_MAX_PUSH   = 1.0;

struct GrassDisplacementConstantsLayout
{
	float4 g_xPrevOriginXZ_Size_Pad;   //  0 : xy = previous anchor origin (m), z = map world size (m), w = 0
	float4 g_xNextOriginXZ_Size_Pad;   // 16 : xy = this frame's anchor origin (m), z = map world size (m), w = 0
	float4 g_xDecay_Texel_ShiftXZ;     // 32 : x = per-frame retention factor, y = texel size (m),
	                                   //      zw = prev->next scroll in texels (INTEGER-valued)
	uint4  g_xResolution_MoverCount;   // 48 : xy = map resolution in texels, z = mover count, w = 0
};                                     // 64 B

// PASS block (set 3). Ping-pong: they must be distinct resources — reading and
// writing one texture across a dispatch has no defined ordering between texels,
// and the re-anchor deliberately reads a DIFFERENT texel than it writes.
struct PassParams
{
	ConstantBuffer<GrassDisplacementConstantsLayout> GrassDisplacementConstants;
	// One mover per element: xy = world XZ centre (m), z = radius (m), w = [0,1] strength.
	// A bare float4 rather than a named record because that IS the whole mover — the
	// host mirror (Flux_GrassMoverGPU) is one float4 for the same reason.
	StructuredBuffer<float4> Movers;                                          // [kGRASS_MAX_MOVERS]
	[vk::image_format("rg16f")] RWTexture2D<float4> g_xPrevDisplacementTex;   // last frame's field (LOADED)
	[vk::image_format("rg16f")] RWTexture2D<float4> g_xNextDisplacementTex;   // this frame's field (STORED)
};
ParameterBlock<PassParams> g_xPassSet;

// ============================================================================
// Decay, re-anchor and splat the grass displacement field.
// Workgroup: 8x8x1 (64 threads — one texel each, a 32x32 group grid over 256x256)
// Shared memory: none
//
// One thread per texel of the NEXT map. Reads the previous field at this texel's
// scrolled position, decays it, adds every mover that reaches this texel and
// clamps the result. Texels outside the previous anchor start from zero.
// ============================================================================
[shader("compute")]
[numthreads(8, 8, 1)]
void csMain(uint3 xDispatchId : SV_DispatchThreadID)
{
	uint2 xResolution = g_xPassSet.GrassDisplacementConstants.g_xResolution_MoverCount.xy;
	uint2 xTexel = xDispatchId.xy;
	if (xTexel.x >= xResolution.x || xTexel.y >= xResolution.y) return;

	float4 xNext  = g_xPassSet.GrassDisplacementConstants.g_xNextOriginXZ_Size_Pad;
	float4 xDecay = g_xPassSet.GrassDisplacementConstants.g_xDecay_Texel_ShiftXZ;

	// --- re-anchor: an integer texel slide, never a resample ---
	int2 xShift = int2(xDecay.zw);
	int2 xPrevTexel = int2(xTexel) + xShift;
	bool bInsidePrev = all(xPrevTexel >= int2(0, 0)) && all(xPrevTexel < int2(xResolution));

	float2 xPush = bInsidePrev ? g_xPassSet.g_xPrevDisplacementTex[uint2(xPrevTexel)].rg : float2(0.0, 0.0);

	// --- decay ---
	xPush *= xDecay.x;

	// --- mover splat, additive over the decayed value ---
	// Texel CENTRE under THIS frame's anchor: a mover's falloff has to be measured
	// against the ground the texel actually covers, not against its corner.
	float2 xWorldXZ = xNext.xy + (float2(xTexel) + 0.5) * xDecay.y;

	uint uMoverCount = min(g_xPassSet.GrassDisplacementConstants.g_xResolution_MoverCount.z, kGRASS_MAX_MOVERS);
	for (uint uMover = 0u; uMover < uMoverCount; uMover++)
	{
		float4 xM = g_xPassSet.Movers[uMover];
		float2 xDelta = xWorldXZ - xM.xy;
		float  fRadius = max(xM.z, 1e-3);
		float  fDist = length(xDelta);
		if (fDist >= fRadius) continue;

		// Squared linear falloff: the push reaches zero AND its gradient reaches zero
		// at the rim, so the trail's edge fades instead of ending on a hard ring of
		// half-pushed blades.
		float fFalloff = 1.0 - fDist / fRadius;
		fFalloff *= fFalloff;
		// A mover sitting exactly on a texel centre has no direction to push it; the
		// degenerate case has to pick one rather than normalize a zero vector.
		float2 xDir = fDist > 1e-4 ? (xDelta / fDist) : float2(1.0, 0.0);
		xPush += xDir * (fFalloff * xM.w);
	}

	// --- clamp: the whole reason a stationary mover does not integrate forever ---
	float fMagnitude = length(xPush);
	if (fMagnitude > kGRASS_DISPLACEMENT_MAX_PUSH)
	{
		xPush *= kGRASS_DISPLACEMENT_MAX_PUSH / fMagnitude;
	}

	g_xPassSet.g_xNextDisplacementTex[xTexel] = float4(xPush, 0.0, 0.0);
}
