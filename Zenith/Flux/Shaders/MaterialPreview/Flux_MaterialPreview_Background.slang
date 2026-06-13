// =====================================================================
// Flux_MaterialPreview_Background — Slang module.
//
// Subsystem: MaterialPreview (TOOLS-only editor material preview).
// Fills the preview HDR target with the scene's environment cubemap
// seen through the preview orbit camera — the UE-style "environment
// visible behind the preview mesh". The pass binds a subsystem-owned
// FrameConstants clone built from the orbit camera, so Common.Frame's
// RayDir reconstruction works unmodified.
//
// Bindings:
//   FrameConstants  set=0 binding=0  (preview-camera clone, Common.Frame)
//   g_xCubemap      set=0 binding=1
// =====================================================================

import Common.Frame;
import Common.Fullscreen;

[[vk::binding(1, 0)]] SamplerCube g_xCubemap;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut i) : SV_Target
{
	float3 xRayDir = RayDir(i.uv);
	return float4(g_xCubemap.Sample(xRayDir).rgb, 1.0);
}
