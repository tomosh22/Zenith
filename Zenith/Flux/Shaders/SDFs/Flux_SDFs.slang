// =====================================================================
// Flux_SDFs — Slang module.
//
// Subsystem: SDFs. Sphere-tracing pass that ray marches a list of
// signed-distance spheres with smooth-min blending. Output is the
// blended sphere colour (weighted by per-sphere distance contribution)
// shaded by a single Lambertian dot against the sun direction.
//
// Uses the Bindings spine for the VIEW/GLOBAL data + RayDir.
// =====================================================================

// Spine via preprocessor #include (declares GLOBAL/VIEW/BINDLESS at sets 0/1/2);
// Common.Fullscreen is pure helpers, so `import` is fine for it.
#include "Common/Bindings.slang"
import Common.Fullscreen;

static const uint  kMaxSteps     = 100u;
static const float kHitDistance  = 0.1;
static const float kMissDistance = 10000.0;

struct Sphere
{
	float4 m_xPosition_Radius;
	float4 m_xColour;
};

struct SphereDataLayout
{
	uint4  g_uNumSpheres_Pad;
	uint4  g_uMorePadding;
	Sphere g_axSpheres[1000];
};
// PASS frequency (this shader's own sphere list). Declared AFTER the spine
// #include, so it lands at set 3 (the first set past the persistent 0/1/2).
struct PassParams { ConstantBuffer<SphereDataLayout> SphereData; };
ParameterBlock<PassParams> g_xPassSet;

[shader("vertex")]
FullscreenVsOut vsMain(uint vertexId : SV_VertexID) { return FullscreenVS(vertexId); }

float smoothmin(float d1, float d2, float k)
{
	float h = max(k - abs(d1 - d2), 0.0) / k;
	return min(d1, d2) - h * h * h * k * (1.0 / 6.0);
}

// Distance to the union of all spheres + colour weighted by each
// sphere's contribution to the field.
float SDF(float3 xOrigin, out float4 xColour)
{
	float  fRet         = 1.0 / 0.0;
	float4 finalColour  = float4(0.0, 0.0, 0.0, 0.0);
	float  totalWeight  = 0.0;

	// Spatial vs colour blend factors. Colour blend is much wider than
	// the geometry blend so material transitions are softer than the
	// distance field's smooth-min.
	const float kDistance = 50.0;
	const float kColor    = 50.0;

	for (uint u = 0; u < g_xPassSet.SphereData.g_uNumSpheres_Pad.x; u++)
	{
		Sphere xSphere = g_xPassSet.SphereData.g_axSpheres[u];

		float fNewDistance = length(xOrigin - xSphere.m_xPosition_Radius.xyz)
							 - xSphere.m_xPosition_Radius.w;

		if (u == 0) fRet = fNewDistance;
		else        fRet = smoothmin(fRet, fNewDistance, kDistance);

		float weight = exp(-fNewDistance / kColor);
		finalColour += weight * xSphere.m_xColour;
		totalWeight += weight;
	}

	if (totalWeight > 0.0) finalColour /= totalWeight;

	xColour = finalColour;
	return fRet;
}

// Numerical normal via central differences. Slang's `out` parameter
// requires a temp on each call; matches the GLSL pattern.
float3 GetNormal(float3 p)
{
	float4 xColour;
	float d = SDF(p, xColour);
	float2 e = float2(0.01, 0.0);

	float dx, dy, dz;
	float4 xUnused;
	dx = SDF(p - e.xyy, xUnused);
	dy = SDF(p - e.yxy, xUnused);
	dz = SDF(p - e.yyx, xUnused);

	float3 n = d - float3(dx, dy, dz);
	return normalize(n);
}

float4 RayMarch(float3 xRayDir)
{
	float fDistanceFromOrigin = 0.0;
	for (uint u = 0; u < kMaxSteps; u++)
	{
		float3 xSamplePoint = GetCamPos_Pad().xyz + fDistanceFromOrigin * xRayDir;
		float4 xColour;
		float  fNearestDistance = SDF(xSamplePoint, xColour);
		fDistanceFromOrigin += fNearestDistance;

		if (fNearestDistance < kHitDistance)
		{
			float3 xNormal = GetNormal(GetCamPos_Pad().xyz + fDistanceFromOrigin * xRayDir);
			return xColour + dot(-GetSunDir_Pad().xyz, xNormal) / 2.0;
		}
		if (fDistanceFromOrigin > kMissDistance) return float4(0.0, 0.0, 0.0, 0.0);
	}
	// Step budget exhausted — flag with red so it's visible if it ever happens.
	return float4(1.0, 0.0, 0.0, 1.0);
}

[shader("fragment")]
float4 fsMain(FullscreenVsOut input) : SV_Target
{
	float3 xRayDir = RayDir(input.uv);
	return RayMarch(xRayDir);
}
