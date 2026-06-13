// =====================================================================
// Flux_AnimatedMesh_ToGBuffer — Slang module.
//
// Subsystem: AnimatedMeshes. Skeletal-animated G-buffer pass. Reads
// per-vertex bone IDs + weights and accumulates the bone-weighted
// position / normal / tangent / bitangent. Writes the full TBN matrix
// to varyings (vs static, which sends tangent + bitangent sign).
//
// Bones array lives in a separate CB at set=1 binding=1, which pushes
// every material texture binding up by 1 vs the static layout.
// =====================================================================

import Common.Frame;
import Common.DrawConstants;
import Common.Material;
import Common.MaterialSurface;
import Common.GBuffer;

struct VsIn
{
	float3 a_xPosition    : POSITION;
	float2 a_xUV          : TEXCOORD0;
	float3 a_xNormal      : NORMAL;
	float3 a_xTangent     : TANGENT;
	float3 a_xBitangent   : BINORMAL;
	float4 a_xColor       : COLOR;
	uint4  a_xBoneIDs     : TEXCOORD1;
	float4 a_xBoneWeights : TEXCOORD2;
};

struct VsOut
{
	float4 position    : SV_Position;
	float2 uv          : TEXCOORD0;
	float3 worldNormal : TEXCOORD1;
	float3 worldPos    : TEXCOORD2;
	// Full TBN — bone-weighted, so the fragment can't reconstruct from
	// a tangent + bitangent sign like the static path does.
	float3 tbn0        : TEXCOORD3;
	float3 tbn1        : TEXCOORD4;
	float3 tbn2        : TEXCOORD5;
	float4 vertexColor : TEXCOORD6;
};

struct BonesLayout
{
	float4x4 g_xBones[100];
};
[[vk::binding(1, 1)]] ConstantBuffer<BonesLayout> Bones;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;

	// Bone-weighted accumulation. ~0u sentinel terminates the influence
	// list early — most vertices use 1-2 bones, not the full 4.
	float4 xFinalPosition  = float4(0.0, 0.0, 0.0, 0.0);
	float3 xFinalNormal    = float3(0.0, 0.0, 0.0);
	float3 xFinalTangent   = float3(0.0, 0.0, 0.0);
	float3 xFinalBitangent = float3(0.0, 0.0, 0.0);

	for (uint u = 0; u < 4; u++)
	{
		if (input.a_xBoneIDs[u] == 0xFFFFFFFFu) break;

		float4x4 xBoneTransform = Bones.g_xBones[input.a_xBoneIDs[u]];
		float    fWeight        = input.a_xBoneWeights[u];

		xFinalPosition  += mul(xBoneTransform, float4(input.a_xPosition, 1.0)) * fWeight;
		xFinalNormal    += mul((float3x3)xBoneTransform, input.a_xNormal)    * fWeight;
		xFinalTangent   += mul((float3x3)xBoneTransform, input.a_xTangent)   * fWeight;
		xFinalBitangent += mul((float3x3)xBoneTransform, input.a_xBitangent) * fWeight;
	}

	float3 xWorldPos       = mul(DrawConstants.g_xModelMatrix, xFinalPosition).xyz;
	float3x3 xNormalMatrix = NormalMatrixFromModel(DrawConstants.g_xModelMatrix);

	o.uv          = input.a_xUV;
	o.vertexColor = input.a_xColor;
	o.worldNormal = normalize(mul(xNormalMatrix, normalize(xFinalNormal)));
	float3 xT = normalize(mul(xNormalMatrix, normalize(xFinalTangent)));
	float3 xB = normalize(mul(xNormalMatrix, normalize(xFinalBitangent)));
	// Pack the TBN matrix as columns through three vec3 varyings so the
	// fragment shader can rebuild it without an interpolated mat3 attr
	// (which Vulkan's varying-location accounting can be picky about).
	o.tbn0 = float3(xT.x, xB.x, o.worldNormal.x);
	o.tbn1 = float3(xT.y, xB.y, o.worldNormal.y);
	o.tbn2 = float3(xT.z, xB.z, o.worldNormal.z);
	o.worldPos = xWorldPos;
	o.position = mul(FrameConstants.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

// Material textures offset by 1 vs static (Bones occupies slot 1).
// Slot order matches MaterialTextureSlot in Zenith_MaterialParamTable.h.
[[vk::binding( 2, 1)]] Sampler2D g_xBaseColorTex;
[[vk::binding( 3, 1)]] Sampler2D g_xNormalTex;
[[vk::binding( 4, 1)]] Sampler2D g_xRoughnessMetallicTex;
[[vk::binding( 5, 1)]] Sampler2D g_xOcclusionTex;
[[vk::binding( 6, 1)]] Sampler2D g_xEmissiveTex;
[[vk::binding( 7, 1)]] Sampler2D g_xHeightTex;
[[vk::binding( 8, 1)]] Sampler2D g_xDetailAlbedoTex;
[[vk::binding( 9, 1)]] Sampler2D g_xDetailNormalTex;
[[vk::binding(10, 1)]] Sampler2D g_xDetailMaskTex;

[shader("fragment")]
GBufferOut fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace)
{
	// Rebuild TBN from the packed varying triple. Note: stored as columns,
	// so reconstructed matrix's mul(M, v) takes a tangent-space vector to
	// world space — same convention as Common.Material.BuildTBN.
	float3x3 TBN = float3x3(
		input.tbn0.x, input.tbn0.y, input.tbn0.z,
		input.tbn1.x, input.tbn1.y, input.tbn1.z,
		input.tbn2.x, input.tbn2.y, input.tbn2.z);

	float3 xViewDir = normalize(FrameConstants.g_xCamPos_Pad.xyz - input.worldPos);

	MaterialSurface xSurface = EvaluateMaterialSurface(
		g_xBaseColorTex, g_xNormalTex, g_xRoughnessMetallicTex,
		g_xOcclusionTex, g_xEmissiveTex, g_xHeightTex,
		g_xDetailAlbedoTex, g_xDetailNormalTex, g_xDetailMaskTex,
		input.uv, TBN, xViewDir, input.vertexColor, bFrontFace);

	return MakeGBuffer(xSurface);
}
