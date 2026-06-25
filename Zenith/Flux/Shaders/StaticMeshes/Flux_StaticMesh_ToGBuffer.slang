// =====================================================================
// Flux_StaticMesh_ToGBuffer — Slang module.
//
// Subsystem: StaticMeshes. Forward G-buffer write for static (non-
// animated, non-instanced) meshes. Reads vertex attributes (position,
// uv, normal, tangent, bitangent, vertex colour), applies the per-draw
// model matrix, evaluates the uber-material surface (POM, detail maps,
// normal strength, two-sided normal flip, emissive RGB, clear coat),
// and writes the 4-target G-buffer.
//
// Spine via #include (GLOBAL/VIEW/BINDLESS at sets 0/1/2 — camera lives in
// the VIEW set g_xViewSet.g_xView). The per-draw constants + the 9 uber-
// material textures are this shader's own DRAW ParameterBlock (set 3).
// Common.DrawConstants / Common.Material / Common.MaterialSurface / Common.GBuffer
// are pure helper modules (no bindings) — imported normally.
// =====================================================================

#include "Common/Bindings.slang"
import Common.DrawConstants;
import Common.Material;
import Common.MaterialSurface;
import Common.GBuffer;

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
	float4 position        : SV_Position;
	float2 uv              : TEXCOORD0;
	float3 worldNormal     : TEXCOORD1;
	float3 worldPos        : TEXCOORD2;
	float3 worldTangent    : TEXCOORD3;
	float  bitangentSign   : TEXCOORD4;
	float4 vertexColor     : TEXCOORD5;
};

// DRAW frequency (per-draw model+material constants + the 9 uber-material
// textures). Declared AFTER the spine #include, so it lands at set 3 (the
// first set past the persistent 0/1/2). Member names are preserved so the
// generated handles stay hDrawConstants / hg_x*Tex (FLUX_MATERIAL_TEXTURE_HANDLES).
struct DrawParams
{
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
	Sampler2D g_xBaseColorTex;
	Sampler2D g_xNormalTex;
	Sampler2D g_xRoughnessMetallicTex;
	Sampler2D g_xOcclusionTex;
	Sampler2D g_xEmissiveTex;
	Sampler2D g_xHeightTex;
	Sampler2D g_xDetailAlbedoTex;
	Sampler2D g_xDetailNormalTex;
	Sampler2D g_xDetailMaskTex;
};
ParameterBlock<DrawParams> g_xDrawSet;

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;

	float4x4 xModelMatrix = g_xDrawSet.DrawConstants.g_xModelMatrix;
	float3 xWorldPos = mul(xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;

	// Normal matrix: inverse-transpose of the 3x3 model rotation. Slang
	// has no built-in `inverse()` so this uses the cofactor helper from
	// Common.Material (matches the GLSL `transpose(inverse(mat3(M)))`).
	float3x3 xNormalMatrix = NormalMatrixFromModel(xModelMatrix);

	o.uv             = input.a_xUV;
	o.vertexColor    = input.a_xColor;
	o.worldNormal    = normalize(mul(xNormalMatrix, input.a_xNormal));
	o.worldTangent   = normalize(mul(xNormalMatrix, input.a_xTangent));
	float3 xBitangent = normalize(mul(xNormalMatrix, input.a_xBitangent));
	// Sign-pack the bitangent — fragment reconstructs full B = cross(N,T)*sign.
	o.bitangentSign  = sign(dot(xBitangent, cross(o.worldNormal, o.worldTangent)));
	o.worldPos       = xWorldPos;
	o.position       = mul(g_xViewSet.g_xView.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

[shader("fragment")]
GBufferOut fsMain(VsOut input, bool bFrontFace : SV_IsFrontFace)
{
	float3x3 TBN = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
	float3 xViewDir = normalize(g_xViewSet.g_xView.g_xCamPos_Pad.xyz - input.worldPos);

	DrawConstantsLayout dc = g_xDrawSet.DrawConstants;

	MaterialSurface xSurface = EvaluateMaterialSurface(
		g_xDrawSet.g_xBaseColorTex, g_xDrawSet.g_xNormalTex, g_xDrawSet.g_xRoughnessMetallicTex,
		g_xDrawSet.g_xOcclusionTex, g_xDrawSet.g_xEmissiveTex, g_xDrawSet.g_xHeightTex,
		g_xDrawSet.g_xDetailAlbedoTex, g_xDrawSet.g_xDetailNormalTex, g_xDrawSet.g_xDetailMaskTex,
		input.uv, TBN, xViewDir, input.vertexColor, bFrontFace, dc);

	return MakeGBuffer(xSurface);
}
