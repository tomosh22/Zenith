// =====================================================================
// Flux_SkyboxSolidColour — Slang module.
//
// Subsystem: Skybox. Fills the G-buffer with a solid override colour
// when no cubemap / atmosphere is configured. MRT layout matches the
// engine's deferred G-buffer (diffuse / normals+ambient / material).
// =====================================================================

// This shader uses no FrameConstants (pure fullscreen fill), but still #includes
// the spine so it reserves the persistent sets 0/1/2 and PassParams lands at set 3
// (keeps the descriptor-set layout identical across all converted shaders).
#include "Common/Bindings.slang"
import Common.Fullscreen;

struct SkyboxOverrideConstantsLayout
{
	float4 g_xOverrideColour;
};

// PASS frequency. Declared AFTER the spine #include, so it lands at set 3.
struct PassParams { ConstantBuffer<SkyboxOverrideConstantsLayout> SkyboxOverrideConstants; };
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
	GBufferOut o;
	o.diffuse        = g_xPassSet.SkyboxOverrideConstants.g_xOverrideColour;
	o.normalsAmbient = float4(0.0, 0.0, 0.0, 0.0);
	o.material       = float4(0.0, 0.0, 0.0, 0.0);
	o.emissive       = float4(0.0, 0.0, 0.0, 0.0);
	return o;
}
