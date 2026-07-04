// =====================================================================
// DP_Fog — DevilsPlayground game-side fog Slang module.
//
// Subsystem: Fog. Replaces the engine fog with exponential distance
// fog and circular "holes" around villagers/lights. Holes carve out
// the fog inside their world-space radius (linear falloff at the
// boundary) so the player can see in a small bubble around themselves
// and any dynamic light. Up to DP_FOG_MAX_HOLES (60) holes per frame —
// see the constexpr below; the matching C++ cap lives in
// Games/DevilsPlayground/Source/DPFogPass.cpp (DP_FOG_MAX_HOLES) and
// PublicInterfaces.cpp's per-frame upload truncates beyond that.
//
// Imports Common.Frame for FrameConstants + GetWorldPosFromDepthTex.
// Output is premultiplied (fogColor * fogAlpha, fogAlpha) for blend
// state One/InvSrcAlpha (set on the C++ pipeline spec).
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Fullscreen;

// Hole table cap. Sized to fit every authored light (26) plus every
// villager (17, all of which register fog holes regardless of possession)
// plus headroom for future content. The DPFogConstants_CB payload is
// 60 * 16 + 32 = 992 bytes; engine's per-command scratch
// (Flux_CommandBindDrawConstants::uMAX_SIZE) was widened to 2048 B to
// accommodate it.
static const uint DP_FOG_MAX_HOLES = 60u;

struct DPFogConstantsLayout
{
	float4 g_xFogColor_Density; // RGB = fog colour, W = density / falloff
	float4 g_axHoles[DP_FOG_MAX_HOLES]; // xyz = world-space center, w = radius (0 = unused)
	uint   g_uHoleCount;        // active hole count, 0..DP_FOG_MAX_HOLES
	uint   _pad0;
	uint   _pad1;
	uint   _pad2;
	// Memory-fog window: xy = world-space XZ of g_xMemoryTex texel (0,0)'s
	// corner, zw = 1 / world-space extent the texture covers. zw == 0
	// disables the memory term (nothing revealed yet / non-gameplay scene).
	float4 g_xMemoryRect;
};
// PASS frequency — declared AFTER the spine #include so it lands at set 3.
struct PassParams
{
	ConstantBuffer<DPFogConstantsLayout> DPFogConstants;
	Sampler2D g_xDepthTex;
	Sampler2D g_xMemoryTex; // R8: per-1m-cell remembered visibility (1 = clear)
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float3 xWorldPos      = GetWorldPosFromDepthTex(g_xPassSet.g_xDepthTex, input.uv);
	float3 xCameraToPixel = xWorldPos - GetCamPos_Pad().xyz;
	float  fDist          = length(xCameraToPixel);

	// Beer-Lambert exponential fog amount (scalar form).
	float fFogAlpha = 1.0 - exp(-fDist * g_xPassSet.DPFogConstants.g_xFogColor_Density.w);

	// Fog holes — for each registered hole compute a smooth attenuation
	// based on the distance from the world-space sample to the hole
	// center vs the hole radius. We take the *minimum* attenuation across
	// all holes (i.e. the hole that clears the most fog wins). A linear
	// falloff at the boundary keeps the bubble edge soft.
	uint uHoleCount = min(g_xPassSet.DPFogConstants.g_uHoleCount, DP_FOG_MAX_HOLES);
	float fAttenuation = 1.0;
	for (uint u = 0u; u < uHoleCount; ++u)
	{
		float4 xHole = g_xPassSet.DPFogConstants.g_axHoles[u];
		float fRadius = xHole.w;
		if (fRadius <= 0.0) continue;

		float fHoleDist = length(xWorldPos - xHole.xyz);
		// Smooth linear ramp: 0 at center -> 1 at radius edge -> stays 1
		// outside. Multiplying fog alpha by this clears fog inside the
		// bubble and leaves fog untouched outside.
		float fLocal = saturate(fHoleDist / max(fRadius, 0.0001));
		fAttenuation = min(fAttenuation, fLocal);
	}

	// Memory fog — cells a villager revealed earlier stay (partially) clear
	// while their memory ages through VisitedVisible -> VisitedDim ->
	// VisitedHidden. The R8 texture holds per-cell visibility rasterized on
	// the CPU each frame (1 = remembered-visible, 0 = forgotten/never seen);
	// bilinear filtering softens the 1 m cell edges. Combined with holes via
	// min() so whichever clears the most fog wins.
	float4 xMemRect = g_xPassSet.DPFogConstants.g_xMemoryRect;
	if (xMemRect.z > 0.0)
	{
		float2 xMemUV = (xWorldPos.xz - xMemRect.xy) * xMemRect.zw;
		if (all(xMemUV >= 0.0) && all(xMemUV <= 1.0))
		{
			float fMemoryVis = g_xPassSet.g_xMemoryTex.SampleLevel(xMemUV, 0.0).r;
			fAttenuation = min(fAttenuation, 1.0 - fMemoryVis);
		}
	}

	fFogAlpha *= fAttenuation;

	// Premultiplied output for blend One/InvSrcAlpha.
	float3 xFogColor = g_xPassSet.DPFogConstants.g_xFogColor_Density.xyz;
	return float4(xFogColor * fFogAlpha, fFogAlpha);
}
