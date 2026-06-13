// =====================================================================
// Flux_Skybox — Slang module.
//
// Subsystem: Skybox. Cubemap-driven sky pass. Reads RayDir(uv) from
// Common.Frame to derive a world-space view direction and samples the
// cubemap, writing the result to the G-buffer's diffuse target.
//
// Bindings:
//   FrameConstants  set=0 binding=0  (from Common.Frame)
//   g_xCubemap      set=0 binding=1  (combined cube sampler)
// =====================================================================

import Common.Frame;
import Common.Fullscreen;

[[vk::binding(1, 0)]] SamplerCube g_xCubemap;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

struct GBufferOut
{
	float4 diffuse        : SV_Target0;
	float4 normalsAmbient : SV_Target1;
	float4 material       : SV_Target2;
	float4 emissive       : SV_Target3;
};

[shader("fragment")]
GBufferOut fsMain(FullscreenVsOut i)
{
	float3 xRayDir = RayDir(i.uv);

	GBufferOut o;
	o.diffuse        = g_xCubemap.Sample(xRayDir);
	o.normalsAmbient = float4(0.0, 0.0, 0.0, 0.0);
	o.material       = float4(0.0, 0.0, 0.0, 0.0);
	o.emissive       = float4(0.0, 0.0, 0.0, 0.0);
	return o;
}
