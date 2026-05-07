// =====================================================================
// Flux_Decals_NormalsCopy — Slang module.
//
// Subsystem: Decals. Clones the live G-buffer normalsAmbient MRT into a
// transient that the Apply pass can sample as an SRV. Required because
// Apply both reads and writes the normalsAmbient attachment, and Flux's
// render graph doesn't expose Vulkan input attachments / subpass
// self-dependencies — the standard pattern is a transient copy.
//
// Output format: RGBA16F (matches MRT_INDEX_NORMALSAMBIENT). Straight
// pass-through, no alpha blend.
// =====================================================================

import Common.Fullscreen;

[[vk::binding(0, 0)]] Sampler2D g_xNormalsTex;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	return g_xNormalsTex.Sample(input.uv);
}
