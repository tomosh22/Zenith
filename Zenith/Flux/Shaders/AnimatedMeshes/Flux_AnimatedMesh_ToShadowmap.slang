// =====================================================================
// Flux_AnimatedMesh_ToShadowmap — Slang module.
//
// Subsystem: AnimatedMeshes. Depth-only pass for skeletal animation, now
// with masked alpha cutout so cut-out skinned materials cast cut-out
// shadows. Same bone-weighted position accumulation as the G-buffer path;
// the fragment runs the SHARED masked cutout (SampleAlbedoWithCutout,
// identical to the G-buffer path), guarded on GetAlphaCutoff() > 0 so
// opaque casters stay depth-only.
//
// The Bones UBO occupies binding 1, so the shadow matrix lives at binding 2
// and the base-colour texture at binding 3 (matches the legacy Mesh_Vert
// layout offsets).
// =====================================================================

#include "Common/Bindings.slang"
import Common.DrawConstants;
import Common.Material;
import Common.MaterialSurface;

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
	float4 vertexColor : TEXCOORD1;
};

struct BonesLayout      { float4x4 g_xBones[100]; };
struct ShadowMatrixLayout { float4x4 g_xSunViewProjMat; };

// Per-draw block (set 3): skinning bones + shadow view-proj + per-draw material
// constants + base colour (for the masked cutout). Member names preserved so the
// codegen handle names (hBones, hShadowMatrix, hDrawConstants, hg_xBaseColorTex)
// — and thus the C++ binds — are stable.
struct DrawParams
{
	ConstantBuffer<BonesLayout>         Bones;
	ConstantBuffer<ShadowMatrixLayout>  ShadowMatrix;
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
	Sampler2D g_xBaseColorTex;
};
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;

	float4 xFinalPosition = float4(0.0, 0.0, 0.0, 0.0);
	for (uint u = 0; u < 4; u++)
	{
		if (input.a_xBoneIDs[u] == 0xFFFFFFFFu) break;
		float4x4 xBone = g_xDrawSet.Bones.g_xBones[input.a_xBoneIDs[u]];
		xFinalPosition += mul(xBone, float4(input.a_xPosition, 1.0)) * input.a_xBoneWeights[u];
	}

	float3 xWorldPos = mul(g_xDrawSet.DrawConstants.g_xModelMatrix, xFinalPosition).xyz;
	o.position    = mul(g_xDrawSet.ShadowMatrix.g_xSunViewProjMat, float4(xWorldPos, 1.0));
	o.uv          = input.a_xUV;
	o.vertexColor = input.a_xColor;
	return o;
}

[shader("fragment")]
void fsMain(VsOut input)
{
	// Uniform branch: opaque casters (cutoff 0) skip the sample entirely → depth-only
	// cost preserved. Masked casters run the SAME cutout as the G-buffer path.
	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;
	if (GetAlphaCutoff(dc) > 0.0)
	{
		SampleAlbedoWithCutout(g_xDrawSet.g_xBaseColorTex,
			TransformUV(input.uv, GetUVTiling(dc), GetUVOffset(dc)), input.vertexColor, dc);
	}
}
