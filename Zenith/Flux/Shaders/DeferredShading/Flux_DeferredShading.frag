#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

//#TO_TODO: these should really all be in one buffer
layout(std140, set = 0, binding=1) uniform ShadowMatrix0{
	mat4 g_xShadowMat0;
};
layout(std140, set = 0, binding=2) uniform ShadowMatrix1{
	mat4 g_xShadowMat1;
};
layout(std140, set = 0, binding=3) uniform ShadowMatrix2{
	mat4 g_xShadowMat2;
};
layout(std140, set = 0, binding=4) uniform ShadowMatrix3{
	mat4 g_xShadowMat3;
};

layout(set = 0, binding = 5) uniform sampler2D g_xDiffuseTex;
layout(set = 0, binding = 6) uniform sampler2D g_xNormalsAmbientTex;
layout(set = 0, binding = 7) uniform sampler2D g_xMaterialTex;
layout(set = 0, binding = 8) uniform sampler2D g_xDepthTex;

//#TO_TODO: texture arrays
layout(set = 0, binding = 9) uniform sampler2D g_xCSM0;
layout(set = 0, binding = 10) uniform sampler2D g_xCSM1;
layout(set = 0, binding = 11) uniform sampler2D g_xCSM2;
layout(set = 0, binding = 12) uniform sampler2D g_xCSM3;

layout(push_constant) uniform DeferredShadingConstants
{
	uint g_bVisualiseCSMs;
};


const float PI = 3.14159265359;

// Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with roughness for ambient lighting
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX/Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(float NdotH, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH2 = NdotH * NdotH;
	
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	
	return a2 / max(denom, 0.0000001);
}

// Smith's Schlick-GGX Geometry Function (single direction)
float GeometrySchlickGGX(float NdotV, float roughness) {
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;
	
	return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith's method combining view and light directions
float GeometrySmith(float NdotV, float NdotL, float roughness) {
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	
	return ggx1 * ggx2;
}

void CookTorrance_Directional(inout vec4 xFinalColor, vec4 xDiffuse, DirectionalLight xLight, vec3 xNormal, float fMetal, float fRough, float fReflectivity, vec3 xWorldPos) {
	// Light direction is FROM the sun (already pointing away from sun towards surface)
	vec3 xLightDir = normalize(xLight.m_xDirection.xyz);
	vec3 xViewDir = normalize(g_xCamPos_Pad.xyz - xWorldPos);
	vec3 xHalfDir = normalize(xLightDir + xViewDir);

	float NdotL = max(dot(xNormal, xLightDir), 0.0);
	float NdotV = max(dot(xNormal, xViewDir), 0.0001); // Prevent division by zero
	float NdotH = max(dot(xNormal, xHalfDir), 0.0);
	float HdotV = max(dot(xHalfDir, xViewDir), 0.0);
	
	// Early exit if no light contribution
	if(NdotL <= 0.0001) {
		return;
	}
	
	// Clamp roughness to prevent artifacts
	float roughness = max(fRough, 0.04);
	
	// Calculate F0 (base reflectivity at normal incidence)
	// Dielectrics have ~0.04, metals use albedo as F0
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, xDiffuse.rgb, fMetal);
	
	// Cook-Torrance BRDF components
	float D = DistributionGGX(NdotH, roughness);
	float G = GeometrySmith(NdotV, NdotL, roughness);
	vec3 F = FresnelSchlick(HdotV, F0);
	
	// Specular BRDF
	vec3 numerator = D * G * F;
	float denominator = 4.0 * NdotV * NdotL;
	vec3 specular = numerator / max(denominator, 0.001);
	
	// Energy conservation - what isn't reflected is refracted
	vec3 kS = F; // Specular contribution
	vec3 kD = vec3(1.0) - kS; // Diffuse contribution
	
	// Metals don't have diffuse lighting
	kD *= (1.0 - fMetal);
	
	// Improved diffuse: Disney/Burley diffuse for more realistic subsurface scattering
	// Fall back to Lambertian for better performance, but with proper normalization
	vec3 diffuse = kD * xDiffuse.rgb / PI;
	
	// Combine diffuse and specular with incoming radiance
	vec3 radiance = xLight.m_xColour.rgb * xLight.m_xColour.a;
	vec3 Lo = (diffuse + specular) * radiance * NdotL;
	
	xFinalColor.rgb += Lo;
	xFinalColor.a = 1.0;
}

// ACES Filmic Tone Mapping
vec3 ACESFilm(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Reinhard Tone Mapping (simpler alternative)
vec3 ReinhardToneMapping(vec3 color) {
	return color / (color + vec3(1.0));
}

// Uncharted 2 Tone Mapping
vec3 Uncharted2Tonemap(vec3 x) {
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

void main()
{
	vec4 xDiffuse = texture(g_xDiffuseTex, a_xUV);
	
	if(texture(g_xDepthTex, a_xUV).r == 1.0f)
	{
		o_xColour = xDiffuse;
		return;
	}
	
	vec4 xMaterial = texture(g_xMaterialTex, a_xUV);
	vec4 xNormalAmbient = texture(g_xNormalsAmbientTex, a_xUV);
	vec3 xNormal = xNormalAmbient.xyz;
	float fAmbient = xNormalAmbient.w;
	
	vec3 xWorldPos = GetWorldPosFromDepthTex(g_xDepthTex, a_xUV);
	
	float fRoughness = xMaterial.x;
	float fMetallic = xMaterial.y;
	
	// Calculate view direction for ambient Fresnel
	vec3 xViewDir = normalize(g_xCamPos_Pad.xyz - xWorldPos);
	float NdotV = max(dot(xNormal, xViewDir), 0.0);
	
	// Calculate F0 for ambient term
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, xDiffuse.rgb, fMetallic);
	
	// Ambient with energy conservation
	vec3 kS_ambient = FresnelSchlickRoughness(NdotV, F0, fRoughness);
	vec3 kD_ambient = (1.0 - kS_ambient) * (1.0 - fMetallic);
	
	// Apply ambient diffuse
	vec3 ambientDiffuse = kD_ambient * xDiffuse.rgb * fAmbient;
	
	// Apply ambient specular (approximation for metals and reflective surfaces)
	// This simulates environment reflection without an actual environment map
	// Use roughness to modulate the ambient specular strength
	float specularStrength = 1.0 - fRoughness * 0.7; // Rougher = less specular reflection
	vec3 ambientSpecular = kS_ambient * F0 * fAmbient * specularStrength;
	
	o_xColour.rgb = ambientDiffuse + ambientSpecular;

	DirectionalLight xLight;
	xLight.m_xColour = g_xSunColour;
	xLight.m_xDirection = vec4(-g_xSunDir_Pad.xyz, 0.);
	
	const uint uNumCSMs = 4;
	mat4 axShadowMats[uNumCSMs] = {g_xShadowMat0, g_xShadowMat1, g_xShadowMat2, g_xShadowMat3};
	
	bool bInShadow = false;
	
	// Try cascades in order from highest quality (0) to lowest (2)
	// Break when we find one that contains the fragment
	for(int iCascade = 0; iCascade < int(uNumCSMs); iCascade++)
	{
		// Transform to shadow space for this cascade
		vec4 xShadowSpace = axShadowMats[iCascade] * vec4(xWorldPos, 1);
		vec2 xSamplePos = xShadowSpace.xy / xShadowSpace.w * 0.5 + 0.5;
		float fCurrentDepth = xShadowSpace.z / xShadowSpace.w;
		
		// Check if sample position is within valid bounds [0, 1]
		if(xSamplePos.x < 0 || xSamplePos.x > 1 || xSamplePos.y < 0 || xSamplePos.y > 1)
		{
			continue; // Try next cascade
		}
		
		// Check if depth is within valid range
		if(fCurrentDepth < 0 || fCurrentDepth > 1.0)
		{
			continue; // Try next cascade
		}

		if(g_bVisualiseCSMs > 0)
		{
			o_xColour = vec4(1.f / int(uNumCSMs)) * iCascade;
			return;
		}
		
		// Sample the appropriate shadow map
		float fShadowDepth = 0.0;
		if(iCascade == 0)
		{
			fShadowDepth = texture(g_xCSM0, xSamplePos).x;
		}
		else if(iCascade == 1)
		{
			fShadowDepth = texture(g_xCSM1, xSamplePos).x;
		}
		else if(iCascade == 2)
		{
			fShadowDepth = texture(g_xCSM2, xSamplePos).x;
		}
		else if(iCascade == 3)
		{
			fShadowDepth = texture(g_xCSM3, xSamplePos).x;
		}
		
		
		if(fCurrentDepth > fShadowDepth)
		{
			bInShadow = true;
		}
		
		// Found a valid cascade, stop searching
		break;
	}
	
	if(bInShadow)
	{
		o_xColour.w = 1.f;
		return;
	}
	
	

	float fReflectivity = 0.5f;
	if(xLight.m_xColour.a > 0.1)
	{
		CookTorrance_Directional(o_xColour, xDiffuse, xLight, xNormal, fMetallic, fRoughness, fReflectivity, xWorldPos);
	}
	
	o_xColour.w = 1.f;
}