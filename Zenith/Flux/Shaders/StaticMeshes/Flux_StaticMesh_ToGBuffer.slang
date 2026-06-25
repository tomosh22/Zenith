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
// the VIEW set g_xViewSet.g_xView). Material textures are bindless: the per-draw
// constants carry only the model matrix + a material-table index; the fragment
// shader loads its MaterialGPU record from g_xGlobalSet.g_axMaterials and samples
// g_xBindlessSet.g_axTextures[...]. The DRAW ParameterBlock (set 3) now holds only
// the DrawConstants CB. Common.DrawConstants / Common.MaterialTable / Common.Material
// / Common.MaterialSurface / Common.GBuffer are pure helper modules (no bindings).
// =====================================================================

#include "Common/Bindings.slang"
import Common.DrawConstants;
import Common.MaterialTable;
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

// DRAW frequency (per-draw model matrix + material-table index + VAT). Declared
// AFTER the spine #include, so it lands at set 3 (the first set past the persistent
// 0/1/2). Material textures are no longer here — they are bindless (set 2). Member
// name preserved so the generated handle stays hDrawConstants.
struct DrawParams
{
	ConstantBuffer<DrawConstantsLayout> DrawConstants;
	StructuredBuffer<MaterialGPU>       g_axMaterials;   // GPU material table (per-frame, bound per pass)
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
	MaterialGPU m = g_xDrawSet.g_axMaterials[dc.g_uMaterialIndex];

	// Bindless material textures. The material index is draw-uniform (one material
	// per draw), so the array index is uniform → no NonUniformResourceIndex needed.
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
