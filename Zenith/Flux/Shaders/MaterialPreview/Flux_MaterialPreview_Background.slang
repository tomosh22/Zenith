// =====================================================================
// Flux_MaterialPreview_Background — Slang module.
//
// Subsystem: MaterialPreview (TOOLS-only editor material preview).
// Fills the preview HDR target with the scene's environment cubemap
// seen through the preview orbit camera — the UE-style "environment
// visible behind the preview mesh". The pass binds a subsystem-owned
// VIEW set built from the orbit camera (the preview FrameConstants
// clone, sourced from m_xPreviewFrameConstantsBuffer onto hg_xView), so
// the spine's RayDir reconstruction works unmodified.
//
// ParameterBlock spine: the camera arrives through the VIEW set (set 1)
// from the PREVIEW camera buffer; this pass's only own resource (the
// cubemap) lives in PassParams (set 3).
// =====================================================================

#include "Common/Bindings.slang"   // spine: GLOBAL/VIEW/BINDLESS at sets 0/1/2
import Common.Fullscreen;

// This pass's own resources land at set 3 (after the persistent spine sets).
struct PassParams
{
	SamplerCube g_xCubemap;
};
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut i) : SV_Target
{
	float3 xRayDir = RayDir(i.uv);
	return float4(g_xPassSet.g_xCubemap.Sample(xRayDir).rgb, 1.0);
}
