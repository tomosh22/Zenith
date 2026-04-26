// =====================================================================
// Flux_StaticMesh_ToGBuffer — Slang module.
//
// Subsystem: StaticMeshes. Forward G-buffer write for static (non-
// animated, non-instanced) meshes. Reads vertex attributes (position,
// uv, normal, tangent, bitangent, vertex colour), applies the per-draw
// model matrix, samples the standard PBR texture set, and writes the
// G-buffer (diffuse / normals+ambient / material).
//
// Imports Common.Frame (view×proj), Common.DrawConstants (model
// matrix + material params), Common.Material (texture helpers,
// BuildTBN), Common.GBuffer (MakeGBuffer struct + helper).
// =====================================================================

import Common.Frame;
import Common.DrawConstants;
import Common.Material;
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

[shader("vertex")]
VsOut vsMain(VsIn input)
{
	VsOut o;

	float3 xWorldPos = mul(DrawConstants.g_xModelMatrix, float4(input.a_xPosition, 1.0)).xyz;

	// Normal matrix: inverse-transpose of the 3x3 model rotation. Slang
	// has no built-in `inverse()` so this uses the cofactor helper from
	// Common.Material (matches the GLSL `transpose(inverse(mat3(M)))`).
	float3x3 xNormalMatrix = NormalMatrixFromModel(DrawConstants.g_xModelMatrix);

	o.uv             = input.a_xUV;
	o.vertexColor    = input.a_xColor;
	o.worldNormal    = normalize(mul(xNormalMatrix, input.a_xNormal));
	o.worldTangent   = normalize(mul(xNormalMatrix, input.a_xTangent));
	float3 xBitangent = normalize(mul(xNormalMatrix, input.a_xBitangent));
	// Sign-pack the bitangent — fragment reconstructs full B = cross(N,T)*sign.
	o.bitangentSign  = sign(dot(xBitangent, cross(o.worldNormal, o.worldTangent)));
	o.worldPos       = xWorldPos;
	o.position       = mul(FrameConstants.g_xViewProjMat, float4(xWorldPos, 1.0));
	return o;
}

// Material textures — bind at set=1 starting from slot 1 (DrawConstants
// occupies slot 0 within set 1).
[[vk::binding(1, 1)]] Sampler2D g_xDiffuseTex;
[[vk::binding(2, 1)]] Sampler2D g_xNormalTex;
[[vk::binding(3, 1)]] Sampler2D g_xRoughnessMetallicTex;
[[vk::binding(4, 1)]] Sampler2D g_xOcclusionTex;
[[vk::binding(5, 1)]] Sampler2D g_xEmissiveTex;

[shader("fragment")]
GBufferOut fsMain(VsOut input)
{
	float2 xUV = TransformUV(input.uv, GetUVTiling(), GetUVOffset());

	float4 xDiffuse = SampleDiffuseWithBaseColor(g_xDiffuseTex, xUV, DrawConstants.g_xBaseColor);

	// Vertex-colour modulation: only mix when alpha indicates "use me"
	// (legacy convention so meshes without per-vertex tinting can leave
	// colour at zero alpha and pass through unchanged).
	if (input.vertexColor.a > 0.0) xDiffuse *= input.vertexColor;

	float fAlphaCutoff = GetAlphaCutoff();
	if (xDiffuse.a < fAlphaCutoff) discard;

	float3x3 TBN          = BuildTBN(input.worldNormal, input.worldTangent, input.bitangentSign);
	float3   xWorldNormal = SampleNormalMap(g_xNormalTex, xUV, TBN);

	float fRoughness, fMetallic;
	SampleRoughnessMetallic(g_xRoughnessMetallicTex, xUV,
							 GetRoughnessMultiplier(), GetMetallicMultiplier(),
							 fRoughness, fMetallic);

	float fOcclusion = SampleOcclusion(g_xOcclusionTex, xUV, GetOcclusionStrength());
	float fEmissive  = CalculateEmissiveLuminance(g_xEmissiveTex, xUV,
													GetEmissiveColor(), GetEmissiveIntensity());

	return MakeGBuffer(xDiffuse, xWorldNormal, fOcclusion, fRoughness, fMetallic, fEmissive);
}
