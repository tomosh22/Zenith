// =====================================================================
// Flux_InstancedMesh_ToShadowmap — Slang module.
//
// Subsystem: InstancedMeshes. Depth-only pass for instanced meshes.
// Per-instance transform from the same TransformBuffer as the G-buffer
// pass, and — crucially — the SAME VAT (vertex-animation-texture)
// deformation. The shadow geometry MUST match the lit G-buffer geometry:
// if the shadow map holds the rest pose while the G-buffer holds the
// swayed pose, the self-shadow test is misaligned, and because the swayed
// pose changes every frame the misalignment oscillates -> flickering
// shadow acne across all animated instances (the tree-flicker bug). The
// extra VAT sample per shadow vertex is the cost of correct animated
// self-shadowing.
//
// Bindings (mirror the G-buffer pass so the same buffers/params bind):
//   set=1 binding=0  DrawConstants (g_xEmissiveParams = VAT texW/texH/enable)
//   set=1 binding=1  ShadowMatrix
//   set=1 binding=6  TransformBuffer
//   set=1 binding=7  AnimDataBuffer
//   set=1 binding=8  VisibleIndexBuffer
//   set=1 binding=9  g_xAnimationTex (VAT positions per frame)
// =====================================================================

import Common.DrawConstants;

struct VsIn
{
	float3 a_xPosition  : POSITION;
	float2 a_xUV        : TEXCOORD0;
	float3 a_xNormal    : NORMAL;
	float3 a_xTangent   : TANGENT;
	float3 a_xBitangent : BINORMAL;
	float4 a_xColor     : COLOR;
};

struct ShadowMatrixLayout { float4x4 g_xSunViewProjMat; };
[[vk::binding(1, 1)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix;

// Read-only in the graphics pipeline (see Flux_InstancedMesh_ToGBuffer
// for the vertexPipelineStoresAndAtomics rationale).
[[vk::binding(6, 1)]] StructuredBuffer<float4x4> TransformBuffer;
[[vk::binding(7, 1)]] StructuredBuffer<uint4>    AnimDataBuffer;
[[vk::binding(8, 1)]] StructuredBuffer<uint>     VisibleIndexBuffer;
[[vk::binding(9, 1)]] Sampler2D                  g_xAnimationTex;

// VAT lookup — IDENTICAL to Flux_InstancedMesh_ToGBuffer::SampleVAT so the
// shadow vertex lands at exactly the same world position as the G-buffer
// vertex. (Bound with the POINT sampler — adjacent columns are different
// vertices; a linear blend would corrupt positions.)
float3 SampleVAT(uint uVertexID, uint uAnimIndex, float fAnimTime,
				   uint uFrameCount, uint uTextureWidth, uint uTextureHeight)
{
	float fFrame = fAnimTime * (float)(uFrameCount - 1u);
	uint  uFrame = (uint)fFrame;
	float fBlend = frac(fFrame);

	uFrame          = min(uFrame, uFrameCount - 1u);
	uint uNextFrame = min(uFrame + 1u, uFrameCount - 1u);

	uint uRow0 = uAnimIndex * uFrameCount + uFrame;
	uint uRow1 = uAnimIndex * uFrameCount + uNextFrame;

	float2 xUV0 = float2(((float)uVertexID + 0.5) / (float)uTextureWidth,
						   ((float)uRow0     + 0.5) / (float)uTextureHeight);
	float2 xUV1 = float2(((float)uVertexID + 0.5) / (float)uTextureWidth,
						   ((float)uRow1     + 0.5) / (float)uTextureHeight);

	float3 xPos0 = g_xAnimationTex.SampleLevel(xUV0, 0).xyz;
	float3 xPos1 = g_xAnimationTex.SampleLevel(xUV1, 0).xyz;
	return lerp(xPos0, xPos1, fBlend);
}

[shader("vertex")]
float4 vsMain(VsIn input, uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID) : SV_Position
{
	uint     uInstanceID = VisibleIndexBuffer[instanceId];
	float4x4 xModelMatrix = TransformBuffer[uInstanceID];
	uint4    xAnimData    = AnimDataBuffer[uInstanceID];

	// Same VAT deformation as the G-buffer pass (matched bit-for-bit).
	float3 xLocalPos = input.a_xPosition;
	bool   bUseVAT  = (DrawConstants.g_xEmissiveParams.z > 0.0) && ((xAnimData.w & 2u) != 0u);
	if (bUseVAT)
	{
		uint  uAnimIndex  = xAnimData.x & 0xFFFFu;
		uint  uFrameCount = (xAnimData.x >> 16u) & 0xFFFFu;
		float fAnimTime   = asfloat(xAnimData.y);
		uint  uTexWidth   = (uint)DrawConstants.g_xEmissiveParams.x;
		uint  uTexHeight  = (uint)DrawConstants.g_xEmissiveParams.y;
		xLocalPos = SampleVAT(vertexId, uAnimIndex, fAnimTime,
								uFrameCount, uTexWidth, uTexHeight);
	}

	float3 xWorldPos = mul(xModelMatrix, float4(xLocalPos, 1.0)).xyz;
	return mul(ShadowMatrix.g_xSunViewProjMat, float4(xWorldPos, 1.0));
}

[shader("fragment")]
void fsMain() {}
