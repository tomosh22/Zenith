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
// The masked-cutout base-colour texture is bindless now (set 2), resolved in
// fsMain from the material record.
//
// The all-cascade sun view×proj matrices are in the persistent VIEW set
// (g_xViewSet.g_xShadowMatrices, Phase 5.4), indexed by DrawConstants.g_uShadowCascade.
// DRAW block (set 3) bindings:
//   binding 0   DrawConstants (material-table index + g_xVATParams: VAT texW/texH/enable)
//   binding 1   TransformBuffer
//   binding 2   AnimDataBuffer
//   binding 3   VisibleIndexBuffer
//   binding 4   g_xAnimationTex (VAT positions per frame; POINT sampler)
// =====================================================================

#include "Common/Bindings.slang"
import Common.DrawConstants;
import Common.MaterialTable;
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

// DRAW block (set 3) — per-draw constants + the masked-cutout base-colour texture + the
// SAME instancing buffers and VAT animation texture as the G-buffer pass (the shadow
// geometry MUST match the lit pose; see header). The all-cascade sun view×proj matrices
// now live in the persistent VIEW set (g_xViewSet.g_xShadowMatrices, Phase 5.4); this
// caster selects element DrawConstants.g_uShadowCascade (set per cascade pass).
//
// Masked cutout texture (g_xBaseColorTex) matches the G-buffer's base-colour slot
// by name. VAT params live in DrawConstants.g_xVATParams (see Flux_InstancedMesh_ToGBuffer:
// one scratch/draw-constants buffer per descriptor set, so VAT can't be a 2nd CB).
struct DrawParams
{
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
	StructuredBuffer<float4x4> TransformBuffer;
	StructuredBuffer<uint4>    AnimDataBuffer;
	StructuredBuffer<uint>     VisibleIndexBuffer;
	Sampler2D                  g_xAnimationTex;
};
ParameterBlock<DrawParams> g_xDrawSet;

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

	float3 xPos0 = g_xDrawSet.g_xAnimationTex.SampleLevel(xUV0, 0).xyz;
	float3 xPos1 = g_xDrawSet.g_xAnimationTex.SampleLevel(xUV1, 0).xyz;
	return lerp(xPos0, xPos1, fBlend);
}

[shader("vertex")]
VsOut vsMain(VsIn input, uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
	VsOut o;

	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;

	uint     uInstanceID = g_xDrawSet.VisibleIndexBuffer[instanceId];
	float4x4 xModelMatrix = g_xDrawSet.TransformBuffer[uInstanceID];
	uint4    xAnimData    = g_xDrawSet.AnimDataBuffer[uInstanceID];

	// Same VAT deformation as the G-buffer pass (matched bit-for-bit).
	float3 xLocalPos = input.a_xPosition;
	bool   bUseVAT  = GetVATEnabled(dc) && ((xAnimData.w & 2u) != 0u);
	if (bUseVAT)
	{
		uint  uAnimIndex  = xAnimData.x & 0xFFFFu;
		uint  uFrameCount = (xAnimData.x >> 16u) & 0xFFFFu;
		float fAnimTime   = asfloat(xAnimData.y);
		float2 xVATSize   = GetVATTextureSize(dc);
		uint  uTexWidth   = (uint)xVATSize.x;
		uint  uTexHeight  = (uint)xVATSize.y;
		xLocalPos = SampleVAT(vertexId, uAnimIndex, fAnimTime,
								uFrameCount, uTexWidth, uTexHeight);
	}

	float3 xWorldPos = mul(xModelMatrix, float4(xLocalPos, 1.0)).xyz;
	o.position    = mul(g_xViewSet.g_xShadowMatrices[dc.g_uShadowCascade], float4(xWorldPos, 1.0));
	o.uv          = input.a_xUV;
	o.vertexColor = input.a_xColor;
	return o;
}

[shader("fragment")]
void fsMain(VsOut input)
{
	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;
	MaterialGPU m = g_xGlobalSet.g_axMaterials[dc.g_uMaterialIndex];

	// Uniform branch: opaque casters (cutoff 0) skip the sample entirely → depth-only
	// cost preserved. Masked casters run the SAME cutout as the G-buffer path.
	if (GetAlphaCutoff(m) > 0.0)
	{
		Sampler2D xBaseColorTex = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.x];
		SampleAlbedoWithCutout(xBaseColorTex,
			TransformUV(input.uv, GetUVTiling(m), GetUVOffset(m)), input.vertexColor, m);
	}
}
