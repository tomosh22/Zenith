// =====================================================================
// Common.Material — shared Slang module.
//
// Slang port of MaterialCommon.fxh. Pure functions only (UV transform,
// texture sampling helpers, TBN reconstruction) — no resource bindings
// here. Each consumer declares its own material textures at locally-
// appropriate slots since the Mesh path uses different binding offsets
// per variant (Animated offsets by 1 because its Bones UBO occupies
// slot 1).
// =====================================================================

public float2 TransformUV(float2 xUV, float2 xTiling, float2 xOffset)
{
	return xUV * xTiling + xOffset;
}

// Sample diffuse, multiplied by the per-draw base-colour tint.
public float4 SampleDiffuseWithBaseColor(Sampler2D xTex, float2 xUV, float4 xBaseColor)
{
	return xTex.Sample(xUV) * xBaseColor;
}

// Sample tangent-space normal map and transform to world-space using TBN.
public float3 SampleNormalMap(Sampler2D xTex, float2 xUV, float3x3 xTBN)
{
	float3 xNormalTangent = xTex.Sample(xUV).xyz * 2.0 - 1.0;
	return normalize(mul(xTBN, xNormalTangent));
}

// Standard glTF roughness/metallic texture layout: G=roughness, B=metallic.
// Multipliers apply per-channel for material parameter overrides.
public void SampleRoughnessMetallic(Sampler2D xTex, float2 xUV,
									  float fRoughnessMult, float fMetallicMult,
									  out float fRoughness, out float fMetallic)
{
	float2 xRM = xTex.Sample(xUV).gb;
	fRoughness = xRM.x * fRoughnessMult;
	fMetallic  = xRM.y * fMetallicMult;
}

// Strength blends the AO sample toward unity; 0 = no AO, 1 = full AO.
public float SampleOcclusion(Sampler2D xTex, float2 xUV, float fStrength)
{
	float fAO = xTex.Sample(xUV).r;
	return lerp(1.0, fAO, fStrength);
}

// Full-colour HDR emissive: texture x colour x intensity. Stored in the
// dedicated emissive G-buffer target (RGBA16F) — values above 1.0 survive
// into the HDR scene and drive bloom.
public float3 SampleEmissiveRGB(Sampler2D xTex, float2 xUV,
								  float3 xEmissiveColor, float fIntensity)
{
	float3 xEmissiveTex = xTex.Sample(xUV).rgb;
	return xEmissiveTex * xEmissiveColor * fIntensity;
}

// Legacy: emissive convolved to luminance. Superseded by SampleEmissiveRGB +
// the emissive MRT; kept only for shaders not yet migrated.
public float CalculateEmissiveLuminance(Sampler2D xTex, float2 xUV,
										  float3 xEmissiveColor, float fIntensity)
{
	float3 xEmissive = SampleEmissiveRGB(xTex, xUV, xEmissiveColor, fIntensity);
	return dot(xEmissive, float3(0.299, 0.587, 0.114));
}

// 3x3 matrix inverse — Slang/HLSL has no built-in `inverse()` (GLSL
// does; the difference matters for the mesh-path normal matrix). Cofactor
// formula; suitable for non-uniform-scale model matrices' upper-3x3.
public float3x3 Inverse3x3(float3x3 m)
{
	float3 c0 = float3(m._m00, m._m10, m._m20);
	float3 c1 = float3(m._m01, m._m11, m._m21);
	float3 c2 = float3(m._m02, m._m12, m._m22);
	float3 r0 = cross(c1, c2);
	float3 r1 = cross(c2, c0);
	float3 r2 = cross(c0, c1);
	float invDet = 1.0 / dot(c0, r0);
	return float3x3(r0.x, r0.y, r0.z,
					r1.x, r1.y, r1.z,
					r2.x, r2.y, r2.z) * invDet;
}

// Convenience: builds the transpose-of-inverse normal matrix from a
// model matrix's upper-3x3. Result is the matrix that correctly
// transforms normal vectors under non-uniform scale.
public float3x3 NormalMatrixFromModel(float4x4 modelMatrix)
{
	return transpose(Inverse3x3((float3x3)modelMatrix));
}

// Reconstruct a TBN matrix from the world-space normal/tangent and a
// bitangent sign (the engine packs sign rather than the full bitangent
// to save vertex bandwidth).
public float3x3 BuildTBN(float3 xNormal, float3 xTangent, float fBitangentSign)
{
	float3 N = normalize(xNormal);
	float3 T = normalize(xTangent);
	float3 B = cross(N, T) * fBitangentSign;
	// HLSL/Slang convention: matrix-mul treats v as column vector and
	// stores the basis as columns. Constructing float3x3 from 3 row
	// vectors gives the matrix whose ROWS are T, B, N — so mul(M, v)
	// yields (T·v, B·v, N·v) in tangent space, which is the inverse
	// transform of what we want. We need M such that mul(M, vTangent)
	// = vWorld; the columns of that matrix are T, B, N.
	return float3x3(T.x, B.x, N.x,
					T.y, B.y, N.y,
					T.z, B.z, N.z);
}
