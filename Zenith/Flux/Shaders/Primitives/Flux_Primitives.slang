// =====================================================================
// Flux_Primitives — Slang module.
//
// Subsystem: Primitives. Debug primitive (sphere/box/line/etc) rendering
// to the G-buffer with per-instance colour and a model matrix supplied
// via constant buffer. Matte material (roughness 0.8, no metallic, fixed
// 0.2 ambient) so primitives blend into the deferred lighting pass.
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
	float    m_fPadding;
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
	// Matte debug shading — same constants as the GLSL source.
	return MakeGBuffer(xDiffuse, xNormal, 0.2, 0.8, 0.0);
}
