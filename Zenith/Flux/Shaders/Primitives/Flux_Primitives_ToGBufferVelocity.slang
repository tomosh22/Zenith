// =====================================================================
// Flux_Primitives_ToGBufferVelocity — the velocity variant of Flux_Primitives.
//
// Recorded INSTEAD of the base primitives program inside the 5-attachment
// G-buffer pass when the velocity latch is on (TAA active). Identical shading to
// the base, plus it writes MRT_INDEX_VELOCITY (SV_Target4).
//
// ★ prevWorld == curWorld — CAMERA-ONLY MOTION, and that is a deliberate,
// documented limitation rather than an oversight. A primitive is submitted by
// world-space value every frame (centre + extents, or a start/end pair) with no
// identity the renderer can key on, so there is no Flux_PrevTransformCache entry
// to reproject through: nothing here knows that this frame's sphere is last
// frame's sphere moved. A primitive that MOVES therefore gets its background's
// motion — the camera term — which is strictly better than the (0,0) it used to
// leave behind, and exactly right for the overwhelming majority of primitive
// draws, which are static world annotations. See Flux/TAA/CLAUDE.md.
//
// Kept a copy of the base rather than a shared #include so the working base
// primitives shader is untouched; the only diffs are the two clip outputs, the
// velocity encode, and the GBufferVelocityOut return type. (Same call as
// Flux_Terrain_ToGBufferVelocity next door.)
// =====================================================================

#include "Common/Bindings.slang"
import Common.GBuffer;
import Common.Velocity;   // GBufferVelocityOut + Flux_EncodeVelocityUV + MakeGBufferVelocity

// Mirrors Flux_Primitives' VsIn exactly — the two programs fetch the SAME vertex
// buffers, so a layout that drifted between them would give one buffer two
// strides depending on whether the TAA velocity latch is on.
struct VsIn
{
	float3 a_xPosition    : POSITION;
	float3 a_xNormal      : NORMAL;
};

struct VsOut
{
	float4 position    : SV_Position;
	float3 worldPos    : TEXCOORD0;
	float3 worldNormal : TEXCOORD1;
	float3 colour      : TEXCOORD2;
	// Motion-vector clip positions from the UNJITTERED matrices (SV_Position stays
	// jittered so the raster is jittered; velocity is computed jitter-free).
	float4 clipCurNoJitter  : TEXCOORD3;
	float4 clipPrevNoJitter : TEXCOORD4;
};

struct PrimitivePushConstantLayout
{
	float4x4 m_xModelMatrix;
	float3   m_xColor;
	float    m_fEmissiveIntensity;
};
struct DrawParams { ConstantBuffer<PrimitivePushConstantLayout> PrimitivePushConstant; };
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;

	float4 xWorldPos = mul(g_xDrawSet.PrimitivePushConstant.m_xModelMatrix, float4(input.a_xPosition, 1.0));
	o.worldPos       = xWorldPos.xyz;

	float3x3 xNormalMatrix = (float3x3)g_xDrawSet.PrimitivePushConstant.m_xModelMatrix;
	o.worldNormal = normalize(mul(xNormalMatrix, input.a_xNormal));

	o.colour     = g_xDrawSet.PrimitivePushConstant.m_xColor;
	o.position   = mul(GetViewProjMat(), xWorldPos);

	// Prev world pos == current world pos (see the header note), so the motion
	// vector is pure camera reprojection through the unjittered current +
	// previous view-proj matrices.
	o.clipCurNoJitter  = mul(GetViewProjMatNoJitter(),     xWorldPos);
	o.clipPrevNoJitter = mul(GetPrevViewProjMatNoJitter(), xWorldPos);
	return o;
}

[shader("fragment")]
GBufferVelocityOut fsMain(VsOut input)
{
	float4 xDiffuse  = float4(input.colour, 1.0);
	float3 xNormal   = normalize(input.worldNormal);
	float fEmissiveIntensity = max(
		g_xDrawSet.PrimitivePushConstant.m_fEmissiveIntensity, 0.0);
	uint uShadingModel = (fEmissiveIntensity > 0.0)
		? GBUFFER_SHADING_UNLIT
		: GBUFFER_SHADING_DEFAULT_LIT;

	return MakeGBufferVelocity(
		MakeGBuffer(xDiffuse, xNormal, 1.0, 0.8, 0.0, 0.5,
			input.colour * fEmissiveIntensity, 0.0, 0.0, uShadingModel),
		Flux_EncodeVelocityUV(input.clipCurNoJitter, input.clipPrevNoJitter));
}
