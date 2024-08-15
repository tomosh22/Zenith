#version 450 core

#include "../Common.fxh"

layout(location = 0) out vec4 o_xColour;

layout(location = 0) in vec2 a_xUV;

uint g_uMaxSteps = 100;
float g_fHitDistance = 0.1f;
float g_fMissDistance = 10000.f;

struct Sphere
{
	vec4 m_xPosition_Radius;
	vec4 m_xColour;
};

layout(std140, set = 0, binding = 1) uniform SphereData{
	uvec4 g_uNumSpheres_Pad;
	uvec4 g_uMorePadding;
	Sphere g_axSpheres[1000];
};

float smoothmin(float d1, float d2, float k) {
    float h = max(k - abs(d1 - d2), 0.0) / k;
    return min(d1, d2) - h * h * h * k * (1.0 / 6.0);
}

float SDF(vec3 xOrigin, out vec4 xColour) {
    float fRet = 1. / 0.;
    vec4 finalColour = vec4(0.0);
    float totalWeight = 0.0;

    // Smoothness factors
    float kDistance = 50.;  // Blending factor for the spatial transition
    float kColor = 50.;     // Blending factor for the color transition (higher for smoother color blend)

    for(uint u = 0; u < g_uNumSpheres_Pad.x; u++) {
        Sphere xSphere = g_axSpheres[u];
        
        float fNewDistance = length(xOrigin - xSphere.m_xPosition_Radius.xyz) - xSphere.m_xPosition_Radius.w;

        if (u == 0) {
            // Initialize fRet with the first sphere
            fRet = fNewDistance;
        } else {
            // Use smoothmin to combine distances with kDistance factor
            fRet = smoothmin(fRet, fNewDistance, kDistance);
        }

        // Calculate weight for current sphere's color based on its distance contribution with kColor factor
        float weight = exp(-fNewDistance / kColor);
        finalColour += weight * xSphere.m_xColour;
        totalWeight += weight;
    }

    // Normalize the final color by the total weight
    if (totalWeight > 0.0) {
        finalColour /= totalWeight;
    }

    xColour = finalColour;
    return fRet;
}

vec3 GetNormal(vec3 p) {
	vec4 xColour; //#TO_TODO: delete me
	float d = SDF(p, xColour);
    vec2 e = vec2(.01, 0);
    
    vec3 n = d - vec3(
        SDF(p-e.xyy, xColour),
        SDF(p-e.yxy, xColour),
        SDF(p-e.yyx, xColour));
    
    return normalize(n);
}

vec4 RayMarch(vec3 xRayDir)
{
	float fDistanceFromOrigin = 0;
	for (uint u = 0; u < g_uMaxSteps; u++)
	{
		vec3 xSamplePoint = g_xCamPos_Pad.xyz + fDistanceFromOrigin * xRayDir;
		
		vec4 xColour;
		float fNearestDistance = SDF(xSamplePoint, xColour);
		fDistanceFromOrigin += fNearestDistance;
		if (fNearestDistance < g_fHitDistance) {
		
			vec3 xNormal = GetNormal(g_xCamPos_Pad.xyz + fDistanceFromOrigin * xRayDir);
			return xColour + dot(-g_xSunDir_Pad.xyz, xNormal) / 2;
		}
		if (fDistanceFromOrigin > g_fMissDistance) {
			return vec4(0.);
		}
	}
	return vec4(1., 0., 0., 1);
}

void main()
{
	vec3 xRayDir = RayDir(a_xUV);
	
	o_xColour = RayMarch(xRayDir);
}