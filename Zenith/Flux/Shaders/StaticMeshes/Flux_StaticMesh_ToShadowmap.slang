// =====================================================================
// Flux_StaticMesh_ToShadowmap — Slang module.
//
// Subsystem: StaticMeshes. Depth-only pass that writes static mesh
// geometry into a CSM cascade, now with masked alpha cutout so cut-out
// materials (foliage, fences, grates) cast cut-out shadows. Vertex shader
// transforms by the per-cascade sun view×proj; fragment runs the SHARED
// masked cutout (SampleAlbedoWithCutout, identical to the G-buffer path),
// guarded on GetAlphaCutoff() > 0 so opaque casters stay depth-only.
//
// Imports Common.DrawConstants for the model matrix + cutoff/UV params,
// and Common.Material/Common.MaterialSurface for the cutout helper. The
// cascade's view×proj is a separate CB at set=1 binding=1; the base-colour
// texture is at set=1 binding=2.
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
[[vk::binding(2, 1)]] Sampler2D g_xBaseColorTex;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;
	float3 xWorldPos = mul(DrawConstants.g_xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;
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
