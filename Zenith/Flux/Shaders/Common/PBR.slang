// =====================================================================
// Common.PBR — shared Slang module.
//
// Slang port of PBRConstants.fxh: GGX NDF, Smith geometry (direct +
// IBL forms), Schlick Fresnel, multi-scatter compensation, and
// importance-sampling utilities (Hammersley + ImportanceSampleGGX).
//
// All values derived from physics or industry standards (UE4, Unity,
// Frostbite, Disney, Karis, Burley, Fdez-Aguera). See PBRConstants.fxh
// for the original references and rationale; the constants and
// derivations are kept verbatim.
// =====================================================================

// ---------- Mathematical constants ----------
public static const float PI           = 3.14159265359;
public static const float TWO_PI       = 6.28318530718;
public static const float INV_PI       = 0.31830988618;

// ---------- PBR material constants ----------
public static const float3 PBR_DIELECTRIC_F0           = float3(0.04, 0.04, 0.04);
public static const float  PBR_FRESNEL_POWER           = 5.0;
public static const float  PBR_MIN_ROUGHNESS           = 0.015;
public static const float  PBR_GGX_MIN_ROUGHNESS       = 0.005;
public static const float  PBR_BRDF_LUT_MIN_ROUGHNESS  = 0.01;
public static const float  PBR_BRDF_LUT_MAX_ROUGHNESS  = 0.99;

public static const float  IBL_REFERENCE_HEIGHT_METERS = 100.0;

public static const float  PBR_GEOMETRY_K_DIRECT_DIVISOR = 8.0;
public static const float  PBR_GEOMETRY_K_IBL_DIVISOR    = 2.0;

public static const float  IBL_PREFILTER_MIP_COUNT       = 7.0;
public static const float  IBL_SMOOTH_BLEND_THRESHOLD    = 0.05;

public static const float  PBR_EPSILON           = 0.0001;
public static const float  PBR_EPSILON_SMALL     = 0.0000001;
public static const float  PBR_DIRECTION_EPSILON = 0.0001;

// ---------- Fresnel ----------

public float3 FresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), PBR_FRESNEL_POWER);
}

public float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
	float3 oneMinusRough = float3(1.0 - roughness, 1.0 - roughness, 1.0 - roughness);
	return F0 + (max(oneMinusRough, F0) - F0) * pow(saturate(1.0 - cosTheta), PBR_FRESNEL_POWER);
}

// ---------- GGX / Trowbridge-Reitz NDF ----------

public float DistributionGGX(float NdotH, float roughness)
{
	float a   = roughness * roughness;
	float a2  = a * a;
	float NH2 = NdotH * NdotH;
	float denom = NH2 * (a2 - 1.0) + 1.0;
	denom = PI * denom * denom;
	return a2 / max(denom, PBR_EPSILON);
}

// ---------- Smith Schlick-GGX geometry ----------

public float GeometrySchlickGGX_Direct(float NdotV, float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / PBR_GEOMETRY_K_DIRECT_DIVISOR;
	float denom = NdotV * (1.0 - k) + k;
	return NdotV / max(denom, PBR_EPSILON);
}

public float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
	float k = (roughness * roughness) / PBR_GEOMETRY_K_IBL_DIVISOR;
	float denom = NdotV * (1.0 - k) + k;
	return NdotV / max(denom, PBR_EPSILON);
}

public float GeometrySmith_Direct(float NdotV, float NdotL, float roughness)
{
	return GeometrySchlickGGX_Direct(NdotV, roughness) *
		   GeometrySchlickGGX_Direct(NdotL, roughness);
}

public float GeometrySmith_IBL(float NdotV, float NdotL, float roughness)
{
	return GeometrySchlickGGX_IBL(NdotV, roughness) *
		   GeometrySchlickGGX_IBL(NdotL, roughness);
}

// ---------- Multi-scatter BRDF compensation ----------
// Recovers single-scatter GGX energy loss at high roughness — see
// Fdez-Aguera 2019 for the derivation kept inline in PBRConstants.fxh.
public float3 MultiscatterBRDF(float3 F0, float NdotV, float roughness, float2 brdfLUT)
{
	float3 FssEss = F0 * brdfLUT.x + brdfLUT.y;
	float  Ess    = brdfLUT.x + brdfLUT.y;
	float  Ems    = 1.0 - Ess;
	float3 Favg   = F0 + (1.0 - F0) / 21.0;
	float3 Fms    = FssEss * Favg / (1.0 - Ems * Favg);
	return FssEss + Fms * Ems;
}

// ---------- Importance sampling ----------

public float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return (float)bits * 2.3283064365386963e-10;
}

public float2 Hammersley(uint i, uint N)
{
	return float2((float)i / (float)N, RadicalInverse_VdC(i));
}

public float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
	float a = roughness * roughness;

	float phi      = TWO_PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

	float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

	float3 up      = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangent = normalize(cross(up, N));
	float3 bitan   = cross(N, tangent);

	return normalize(tangent * H.x + bitan * H.y + N * H.z);
}
