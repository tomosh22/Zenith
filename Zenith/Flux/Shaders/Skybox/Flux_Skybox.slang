// =====================================================================
// Flux_Skybox — Slang module.
//
// Subsystem: Skybox. Cubemap-driven sky pass. Reads RayDir(uv) from
// Common.Frame to derive a world-space view direction and samples the
// cubemap, writing the result to the G-buffer's diffuse target.
//
// Bindings:
//   spine VIEW set (set 1)        — view/proj inverse for RayDir()
//   g_xPassSet.g_xCubemap (set 3) — combined cube sampler
// =====================================================================

// Spine via preprocessor #include (declares GLOBAL/VIEW/BINDLESS at sets 0/1/2);
// replaces `import Common.Frame;` (RayDir now sourced from the VIEW set).
#include "Common/Bindings.slang"
import Common.Fullscreen;

// PASS frequency. Declared AFTER the spine #include, so it lands at set 3.
struct PassParams { SamplerCube g_xCubemap; };
ParameterBlock<PassParams> g_xPassSet;

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
	o.diffuse        = g_xPassSet.g_xCubemap.Sample(xRayDir);
	o.normalsAmbient = float4(0.0, 0.0, 0.0, 0.0);
	o.material       = float4(0.0, 0.0, 0.0, 0.0);
	o.emissive       = float4(0.0, 0.0, 0.0, 0.0);
	return o;
}
