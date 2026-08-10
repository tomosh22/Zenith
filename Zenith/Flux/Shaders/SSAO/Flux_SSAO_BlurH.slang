// Horizontal half of the separable SSAO joint-bilateral blur.
#include "SSAO/Flux_SSAO_BlurCommon.slang"

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	return SSAOBlur1D(input.uv, float2(1.0, 0.0));
}
