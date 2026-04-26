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
// =====================================================================

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

// Combined image+sampler at set=0, binding=0 — matches the original
// GLSL `sampler2D g_xTexture` so Zenith_Vulkan_Swapchain's existing
// BindSRV layout still applies during the pilot.
[[vk::binding(0, 0)]] Sampler2D g_xTexture;

[shader("fragment")]
float4 fsMain(VsOut i) : SV_Target
{
	return g_xTexture.Sample(i.uv);
}
