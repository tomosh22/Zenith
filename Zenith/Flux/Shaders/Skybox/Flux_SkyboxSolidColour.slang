// =====================================================================
// Flux_SkyboxSolidColour — Slang module.
//
// Subsystem: Skybox. Fills the G-buffer with a solid override colour
// when no cubemap / atmosphere is configured. MRT layout matches the
// engine's deferred G-buffer (diffuse / normals+ambient / material).
// =====================================================================

import Common.Fullscreen;

struct SkyboxOverrideConstantsLayout
{
	float4 g_xOverrideColour;
};
[[vk::binding(0, 0)]] ConstantBuffer<SkyboxOverrideConstantsLayout> SkyboxOverrideConstants;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

struct GBufferOut
{
	float4 diffuse        : SV_Target0;
	float4 normalsAmbient : SV_Target1;
	float4 material       : SV_Target2;
};

[shader("fragment")]
GBufferOut fsMain(FullscreenVsOut i)
{
	GBufferOut o;
	o.diffuse        = SkyboxOverrideConstants.g_xOverrideColour;
	o.normalsAmbient = float4(0.0, 0.0, 0.0, 0.0);
	o.material       = float4(0.0, 0.0, 0.0, 1.0);
	return o;
}
