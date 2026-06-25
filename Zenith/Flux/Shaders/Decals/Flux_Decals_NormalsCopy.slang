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

// Spine via preprocessor #include (declares GLOBAL/VIEW/BINDLESS at sets 0/1/2);
// Common.Fullscreen is pure helpers, so `import` is fine for it. This shader
// uses no FrameConstants, but still includes the spine so its own resources
// land at set 3.
#include "Common/Bindings.slang"
import Common.Fullscreen;

// PASS frequency — the live normalsAmbient MRT clone source. Declared AFTER the
// spine #include, so it lands at set 3 (the first set past the persistent 0/1/2).
struct PassParams
{
	Sampler2D g_xNormalsTex;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	return g_xPassSet.g_xNormalsTex.Sample(input.uv);
}
