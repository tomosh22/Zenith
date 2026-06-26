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

#include "Common/Bindings.slang"
import Common.DrawConstants;
import Common.MaterialTable;
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

// Per-draw block (set 3): skinning bones + per-draw constants (model matrix +
// material-table index). Material textures are bindless now (set 2), resolved in
// fsMain. Member names preserved so the codegen handles (hBones, hDrawConstants)
// — and thus the C++ binds — are stable.
struct DrawParams
{
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
	ConstantBuffer<BonesLayout>         Bones;
};
ParameterBlock<DrawParams> g_xDrawSet;

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

		float4x4 xBoneTransform = g_xDrawSet.Bones.g_xBones[input.a_xBoneIDs[u]];
		float    fWeight        = input.a_xBoneWeights[u];

		xFinalPosition  += mul(xBoneTransform, float4(input.a_xPosition, 1.0)) * fWeight;
		xFinalNormal    += mul((float3x3)xBoneTransform, input.a_xNormal)    * fWeight;
		xFinalTangent   += mul((float3x3)xBoneTransform, input.a_xTangent)   * fWeight;
		xFinalBitangent += mul((float3x3)xBoneTransform, input.a_xBitangent) * fWeight;
	}

	float3 xWorldPos       = mul(g_xDrawSet.DrawConstants.g_xModelMatrix, xFinalPosition).xyz;
	float3x3 xNormalMatrix = NormalMatrixFromModel(g_xDrawSet.DrawConstants.g_xModelMatrix);

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
	o.position = mul(g_xViewSet.g_xView.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

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

	float3 xViewDir = normalize(g_xViewSet.g_xView.g_xCamPos_Pad.xyz - input.worldPos);

	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;
	MaterialGPU m = g_xGlobalSet.g_axMaterials[dc.g_uMaterialIndex];

	// Bindless material textures (draw-uniform index → no NonUniformResourceIndex).
	Sampler2D xBaseColorTex         = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.x];
	Sampler2D xNormalTex            = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.y];
	Sampler2D xRoughnessMetallicTex = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.z];
	Sampler2D xOcclusionTex         = g_xBindlessSet.g_axTextures[m.g_xTexIdx0.w];
	Sampler2D xEmissiveTex          = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.x];
	Sampler2D xHeightTex            = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.y];
	Sampler2D xDetailAlbedoTex      = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.z];
	Sampler2D xDetailNormalTex      = g_xBindlessSet.g_axTextures[m.g_xTexIdx1.w];
	Sampler2D xDetailMaskTex        = g_xBindlessSet.g_axTextures[m.g_xTexIdx2.x];

	MaterialSurface xSurface = EvaluateMaterialSurface(
		xBaseColorTex, xNormalTex, xRoughnessMetallicTex,
		xOcclusionTex, xEmissiveTex, xHeightTex,
		xDetailAlbedoTex, xDetailNormalTex, xDetailMaskTex,
		input.uv, TBN, xViewDir, input.vertexColor, bFrontFace, m);

	return MakeGBuffer(xSurface);
}
