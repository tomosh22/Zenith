// =====================================================================
// Flux_Primitives — Slang module.
//
// Subsystem: Primitives. Debug and gameplay primitive (sphere/box/line/etc)
// rendering to the G-buffer with per-instance colour and a model matrix supplied
// via constant buffer. Zero-emissive debug draws retain the matte default-lit
// material; gameplay draws opt into unlit/emissive output through the final scalar.
//
// Imports Common.Frame for view×proj transform and Common.GBuffer for
// the MakeGBuffer helper.
// =====================================================================

// Spine via preprocessor #include (declares GLOBAL/VIEW/BINDLESS at sets 0/1/2);
// Common.GBuffer is pure helpers, so `import` is fine for it.
#include "Common/Bindings.slang"
import Common.GBuffer;

struct VsIn
{
	float3 a_xPosition    : POSITION;
	float3 a_xNormal      : NORMAL;
	float3 a_xVertexColor : COLOR; // unused — overridden by CB colour
};

struct VsOut
{
	float4 position    : SV_Position;
	float3 worldPos    : TEXCOORD0;
	float3 worldNormal : TEXCOORD1;
	float3 colour      : TEXCOORD2;
};

struct PrimitivePushConstantLayout
{
	float4x4 m_xModelMatrix;
	float3   m_xColor;
	float    m_fEmissiveIntensity;
};
// DRAW frequency (per-draw model matrix + colour). Declared AFTER the spine
// #include, so it lands at set 3 (the first set past the persistent 0/1/2).
struct DrawParams { ConstantBuffer<PrimitivePushConstantLayout> PrimitivePushConstant; };
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;

	float4 xWorldPos = mul(g_xDrawSet.PrimitivePushConstant.m_xModelMatrix, float4(input.a_xPosition, 1.0));
	o.worldPos       = xWorldPos.xyz;

	// Upper 3x3 of the model matrix — assumes uniform scaling, like the
	// GLSL original. Non-uniform scale would need inverse-transpose.
	float3x3 xNormalMatrix = (float3x3)g_xDrawSet.PrimitivePushConstant.m_xModelMatrix;
	o.worldNormal = normalize(mul(xNormalMatrix, input.a_xNormal));

	o.colour     = g_xDrawSet.PrimitivePushConstant.m_xColor;
	o.position   = mul(GetViewProjMat(), xWorldPos);
	return o;
}

[shader("fragment")]
GBufferOut fsMain(VsOut input)
{
	float4 xDiffuse  = float4(input.colour, 1.0);
	float3 xNormal   = normalize(input.worldNormal);
	float fEmissiveIntensity = max(
		g_xDrawSet.PrimitivePushConstant.m_fEmissiveIntensity, 0.0);
	uint uShadingModel = (fEmissiveIntensity > 0.0)
		? GBUFFER_SHADING_UNLIT
		: GBUFFER_SHADING_DEFAULT_LIT;
	// Debug primitives retain the old matte, default-lit path (intensity zero).
	// Production gameplay queues opt into unlit + HDR emissive output, keeping cues
	// readable under overhead light and in shadow without bypassing depth testing.
	return MakeGBuffer(xDiffuse, xNormal, 0.2, 0.8, 0.0, 0.5,
		input.colour * fEmissiveIntensity, 0.0, 0.0, uShadingModel);
}
