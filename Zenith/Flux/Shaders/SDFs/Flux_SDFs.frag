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

float SDF(vec3 xOrigin)
{
	float fRet = 1. / 0.;
	
	Sphere xSphere;
	xSphere.m_xPosition_Radius = vec4(2000., 1500., 2000., 500.);
	
	float fNewDistance = length(xOrigin - xSphere.m_xPosition_Radius.xyz) - (xSphere.m_xPosition_Radius.w);
	if (fNewDistance < fRet) {
		fRet = fNewDistance;
	}
	
	return fRet;
}

vec4 RayMarch(vec3 xRayDir)
{
	float fDistanceFromOrigin = 0;
	for (uint u = 0; u < g_uMaxSteps; u++)
	{
		vec3 xSamplePoint = g_xCamPos_Pad.xyz + fDistanceFromOrigin * xRayDir;
		
		float fNearestDistance = SDF(xSamplePoint);
		fDistanceFromOrigin += fNearestDistance;
		if (fNearestDistance < g_fHitDistance) {
			return vec4(0.,1.,0., 1);
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