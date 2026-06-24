// =====================================================================
// Flux_InstancedMesh_ToShadowmap — Slang module.
//
// Subsystem: InstancedMeshes. Depth-only pass for instanced meshes, now
// with masked alpha cutout so foliage casts cut-out (not solid-quad)
// shadows. Per-instance transform from the same TransformBuffer as the
// G-buffer pass, and — crucially — the SAME VAT (vertex-animation-texture)
// deformation. The shadow geometry MUST match the lit G-buffer geometry:
// if the shadow map holds the rest pose while the G-buffer holds the
// swayed pose, the self-shadow test is misaligned, and because the swayed
// pose changes every frame the misalignment oscillates -> flickering
// shadow acne across all animated instances (the tree-flicker bug).
//
// The fragment shader runs the SHARED masked cutout (SampleAlbedoWithCutout
// in Common.MaterialSurface, identical to the G-buffer path), guarded on
// GetAlphaCutoff() > 0 so opaque casters never sample and keep their
// depth-only cost.
//
// Bindings (set=1):
//   binding 0   DrawConstants (alpha cutoff + UV transform + base-colour tint +
//               g_xVATParams: VAT texW/texH/enable)
//   binding 1   ShadowMatrix
//   binding 2   g_xBaseColorTex (masked cutout)
//   binding 10  TransformBuffer
//   binding 11  AnimDataBuffer
//   binding 12  VisibleIndexBuffer
//   binding 13  g_xAnimationTex (VAT positions per frame; POINT sampler)
// =====================================================================

import Common.DrawConstants;
import Common.Material;
import Common.MaterialSurface;

struct VsIn
{
	float3 a_xPosition  : POSITION;
	float2 a_xUV        : TEXCOORD0;
	float3 a_xNormal    : NORMAL;
	float3 a_xTangent   : TANGENT;
	float3 a_xBitangent : BINORMAL;
	float4 a_xColor     : COLOR;
};

struct VsOut
{
	float4 position    : SV_Position;
	float2 uv          : TEXCOORD0;
	float4 vertexColor : TEXCOORD1;
};

struct ShadowMatrixLayout { float4x4 g_xSunViewProjMat; };
[[vk::binding(1, 1)]] ConstantBuffer<ShadowMatrixLayout> ShadowMatrix;

// Masked cutout texture (matches the G-buffer's base-colour slot by name).
[[vk::binding(2, 1)]] Sampler2D g_xBaseColorTex;

// Instancing resources at bindings >= 10 (see Flux_InstancedMesh_ToGBuffer
// for the vertexPipelineStoresAndAtomics rationale).
[[vk::binding(10, 1)]] StructuredBuffer<float4x4> TransformBuffer;
[[vk::binding(11, 1)]] StructuredBuffer<uint4>    AnimDataBuffer;
[[vk::binding(12, 1)]] StructuredBuffer<uint>     VisibleIndexBuffer;
[[vk::binding(13, 1)]] Sampler2D                  g_xAnimationTex;

// VAT params live in DrawConstants.g_xVATParams (see Flux_InstancedMesh_ToGBuffer:
// one scratch/draw-constants buffer per descriptor set, so VAT can't be a 2nd CB).

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
VsOut vsMain(VsIn input, uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
	VsOut o;

	uint     uInstanceID = VisibleIndexBuffer[instanceId];
	float4x4 xModelMatrix = TransformBuffer[uInstanceID];
	uint4    xAnimData    = AnimDataBuffer[uInstanceID];

	// Same VAT deformation as the G-buffer pass (matched bit-for-bit).
	float3 xLocalPos = input.a_xPosition;
	bool   bUseVAT  = GetVATEnabled() && ((xAnimData.w & 2u) != 0u);
	if (bUseVAT)
	{
		uint  uAnimIndex  = xAnimData.x & 0xFFFFu;
		uint  uFrameCount = (xAnimData.x >> 16u) & 0xFFFFu;
		float fAnimTime   = asfloat(xAnimData.y);
		float2 xVATSize   = GetVATTextureSize();
		uint  uTexWidth   = (uint)xVATSize.x;
		uint  uTexHeight  = (uint)xVATSize.y;
		xLocalPos = SampleVAT(vertexId, uAnimIndex, fAnimTime,
								uFrameCount, uTexWidth, uTexHeight);
	}

	float3 xWorldPos = mul(xModelMatrix, float4(xLocalPos, 1.0)).xyz;
	o.position    = mul(ShadowMatrix.g_xSunViewProjMat, float4(xWorldPos, 1.0));
	o.uv          = input.a_xUV;
	o.vertexColor = input.a_xColor;
	return o;
}

[shader("fragment")]
void fsMain(VsOut input)
{
	// Uniform branch: opaque casters (cutoff 0) skip the sample entirely → depth-only
	// cost preserved. Masked casters run the SAME cutout as the G-buffer path.
	if (GetAlphaCutoff() > 0.0)
	{
		SampleAlbedoWithCutout(g_xBaseColorTex,
			TransformUV(input.uv, GetUVTiling(), GetUVOffset()), input.vertexColor);
	}
}
