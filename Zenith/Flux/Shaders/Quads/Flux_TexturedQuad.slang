// =====================================================================
// Flux_TexturedQuad — pilot Slang module.
//
// Subsystem: Quads. Replaces the original GLSL pair
//   Flux_Fullscreen_UV.vert + Flux_TexturedQuad.frag
//
// Entry points are co-located in this module so Slang sees the full
// pipeline interface during link, preserving varyings between stages.
// Used by Zenith_Vulkan_Swapchain to copy the final frame to the
// backbuffer.
//
// Pure blit: does not read the GLOBAL/VIEW sets. The spine is still
// #included so it reserves sets 0/1/2 and this shader's own PassParams
// lands at set 3 (consistent with every other converted shader).
// =====================================================================

#include "Common/Bindings.slang"

struct VsOut
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
};

[shader("vertex")]
VsOut vsMain(uint vertexId : SV_VertexID)
{
	uint xBit = vertexId & 1u;
	uint yBit = (vertexId >> 1u) & 1u;
	float xPos = float(xBit) * 2.0 - 1.0;
	float yPos = float(yBit) * 2.0 - 1.0;

	VsOut o;
	o.position = float4(xPos, yPos, 1.0, 1.0);
	o.uv       = float2((xPos + 1.0) * 0.5, (yPos + 1.0) * 0.5);
	return o;
}

// PASS frequency. Declared AFTER the spine #include, so it lands at set 3
// (the first set past the persistent 0/1/2). Member name preserved exactly —
// the C++ bind keys on it (hg_xTexture).
struct PassParams
{
	Sampler2D g_xTexture;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("fragment")]
float4 fsMain(VsOut i) : SV_Target
{
	return g_xPassSet.g_xTexture.Sample(i.uv);
}
