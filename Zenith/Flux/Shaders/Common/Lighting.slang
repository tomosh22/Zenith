// =====================================================================
// Common.Lighting — shared Slang module.
//
// Subsystem: DynamicLights / DeferredShading. Single source of truth
// for the dynamic-light data layout, the clustered-deferred grid math,
// and the per-light Cook-Torrance evaluator. Used by:
//   - DynamicLights/Flux_LightClustering.slang  (compute, builds the
//     per-cluster light index lists)
//   - DeferredShading/Flux_DeferredShading.slang (fragment, iterates
//     each pixel's cluster and accumulates light contribution)
//
// LightInstance is a unified 64-byte POD. Point/spot/directional all
// share the layout — fields irrelevant to a given type are unused but
// kept zeroed by the CPU staging path. The C++ side has a matching POD
// in DynamicLights/Flux_DynamicLights.cpp with a static_assert on size.
//
// Cluster grid is 16x9x24 (3456 clusters total) with exponential Z
// distribution. Resolution is independent of the 160x90x64 froxel-fog
// grid — using that resolution here would produce 921600 clusters and
// defeat the point. Max 64 lights per cluster; clamp silently on
// overflow (acceptable for v1, cap is well above realistic counts
// given uMAX_LIGHTS = 256).
//
// Imports Common.Frame for FrameConstants (g_xInvProjMat etc) and
// Common.PBR for the BRDF helpers.
// =====================================================================

// (Common.Frame no longer needed — ComputeClusterAABB takes the inverse-proj as
// a parameter, so this shared module has no global per-frame dependency.)
import Common.PBR;

// ---------- Light type tags (kept in m_xTypeOuter.y as a float) ----------
public static const uint LIGHT_TYPE_POINT       = 0u;
public static const uint LIGHT_TYPE_SPOT        = 1u;
public static const uint LIGHT_TYPE_DIRECTIONAL = 2u;

// ---------- Cluster grid ----------
// Keep these as compile-time constants so the bound is a literal in
// inner loops. CPU-side mirror lives in Flux_LightClustering.cpp.
public static const uint CLUSTER_DIM_X      = 16u;
public static const uint CLUSTER_DIM_Y      = 9u;
public static const uint CLUSTER_DIM_Z      = 24u;
public static const uint CLUSTER_COUNT      = CLUSTER_DIM_X * CLUSTER_DIM_Y * CLUSTER_DIM_Z; // 3456
public static const uint MAX_LIGHTS_PER_CLUSTER = 64u;

// Numerical guards.
public static const float LIGHT_CONTRIBUTION_THRESHOLD = 0.001;
public static const float MIN_LIGHT_DISTANCE           = 0.01; // 1 cm minimum to avoid 1/d² blowup
public static const float CLUSTER_NEAR_Z               = 0.5;  // matches typical scene scale
public static const float CLUSTER_FAR_Z                = 500.0;

// ---------- Light data ----------
// 64 bytes — sized to spot's worst-case footprint so all light types
// share one struct. Padding fields stay zero on CPU staging.
public struct LightInstance
{
    public float4 m_xPositionRange;   // xyz = world position (point/spot), w = range
    public float4 m_xColorIntensity;  // xyz = linear RGB, w = intensity (lumens or lux)
    public float4 m_xDirectionInner;  // xyz = direction (spot/dir, normalized), w = cos(inner)
    public float4 m_xTypeOuter;       // x = cos(outer), y = light type tag, z/w = pad
};

// ---------- Cluster <-> screen mapping ----------
// Slice index from view-space Z. Exponential distribution is the same
// formula the froxel fog uses (Flux_FroxelFog_Inject.slang) — stays
// consistent with the engine's depth conventions (+Z forward).
public uint ClusterSliceFromViewZ(float fViewZ)
{
    float fNearZ = CLUSTER_NEAR_Z;
    float fFarZ  = CLUSTER_FAR_Z;
    float fLogRatio = log(fFarZ / fNearZ);
    // viewZ is positive in this engine's convention; clamp for safety.
    float fClampedZ = max(fViewZ, fNearZ);
    float fSlice = log(fClampedZ / fNearZ) / fLogRatio * float(CLUSTER_DIM_Z);
    return clamp(uint(fSlice), 0u, CLUSTER_DIM_Z - 1u);
}

// Cluster coord from a screen UV + view-space Z. Used by the deferred
// fragment shader to look up the per-pixel light list.
public uint3 ClusterCoordFromUVDepth(float2 xUV, float fViewZ)
{
    uint uX = clamp(uint(xUV.x * float(CLUSTER_DIM_X)), 0u, CLUSTER_DIM_X - 1u);
    uint uY = clamp(uint(xUV.y * float(CLUSTER_DIM_Y)), 0u, CLUSTER_DIM_Y - 1u);
    uint uZ = ClusterSliceFromViewZ(fViewZ);
    return uint3(uX, uY, uZ);
}

// Linear cluster index — the buffers use this layout:
//   ClusterLightCounts[idx]
//   ClusterLightIndices[idx * MAX_LIGHTS_PER_CLUSTER + i]
public uint ClusterIndex(uint3 xCoord)
{
    return xCoord.z * (CLUSTER_DIM_X * CLUSTER_DIM_Y) +
           xCoord.y *  CLUSTER_DIM_X +
           xCoord.x;
}

// Z range (in view space) for a given Z slice. Used by the clustering
// compute to build a slab-AABB per cluster. Inverse of ClusterSliceFromViewZ.
public float2 ClusterZRange(uint uSlice)
{
    float fNearZ = CLUSTER_NEAR_Z;
    float fFarZ  = CLUSTER_FAR_Z;
    float fNorm0 = float(uSlice    ) / float(CLUSTER_DIM_Z);
    float fNorm1 = float(uSlice + 1u) / float(CLUSTER_DIM_Z);
    float fZ0 = fNearZ * pow(fFarZ / fNearZ, fNorm0);
    float fZ1 = fNearZ * pow(fFarZ / fNearZ, fNorm1);
    return float2(fZ0, fZ1);
}

// Cluster's view-space AABB. Built by reconstructing the four screen-
// space corners at the slice's near and far planes through the inverse
// projection. Conservative — we just min/max the eight resulting points.
// xInvProj is passed in (the VIEW set's inverse-projection) so this shared
// helper has no global-constant dependency — it works under the binding spine.
public void ComputeClusterAABB(uint3 xCoord, float4x4 xInvProj, out float3 xMin, out float3 xMax)
{
    float2 xUVMin = float2(xCoord.xy) / float2(CLUSTER_DIM_X, CLUSTER_DIM_Y);
    float2 xUVMax = float2(xCoord.xy + uint2(1u, 1u)) / float2(CLUSTER_DIM_X, CLUSTER_DIM_Y);
    float2 xRange = ClusterZRange(xCoord.z);

    xMin = float3( 1e30,  1e30,  1e30);
    xMax = float3(-1e30, -1e30, -1e30);

    // Eight corners of the cluster frustum in view space.
    [unroll]
    for (int iZ = 0; iZ < 2; ++iZ)
    {
        float fZ = (iZ == 0) ? xRange.x : xRange.y;
        [unroll]
        for (int iY = 0; iY < 2; ++iY)
        {
            float fV = (iY == 0) ? xUVMin.y : xUVMax.y;
            [unroll]
            for (int iX = 0; iX < 2; ++iX)
            {
                float fU = (iX == 0) ? xUVMin.x : xUVMax.x;
                float2 xNDC = float2(fU, fV) * 2.0 - 1.0;
                float4 xClip = float4(xNDC, 0.0, 1.0);
                float4 xView = mul(xInvProj, xClip);
                xView /= xView.w;
                // Engine uses +Z forward; scale the unit-depth ray to fZ.
                float fViewZ = max(abs(xView.z), 1e-4);
                float3 xScaled = xView.xyz * (fZ / fViewZ);
                xMin = min(xMin, xScaled);
                xMax = max(xMax, xScaled);
            }
        }
    }
}

// Sphere-vs-AABB squared-distance test. Returns true if the sphere
// intersects (or contains, or is contained by) the AABB.
public bool SphereIntersectsAABB(float3 xCenter, float fRadius, float3 xMin, float3 xMax)
{
    float3 xClosest = clamp(xCenter, xMin, xMax);
    float3 xDelta = xCenter - xClosest;
    return dot(xDelta, xDelta) <= fRadius * fRadius;
}

// ---------- Per-light evaluators ----------
// Lifted verbatim from the now-deleted Flux_DynamicLights.slang. Same
// energy formulation as the deferred sun light + IBL — multiscatter
// compensation via the BRDF LUT keeps brightness consistent across
// all lighting paths.

// Karis 2013 windowed inverse-square. Flux→irradiance via 1/(4πd²),
// multiplied by saturate(1 - (d/r)⁴)² so contribution reaches zero
// exactly at the range limit.
public float ComputePointAttenuation(float fDistance, float fRange)
{
    float d2 = fDistance * fDistance;
    float r2 = fRange    * fRange;
    float fDistanceRatio = d2 / max(r2, 1e-6);
    float fWindow        = saturate(1.0 - fDistanceRatio * fDistanceRatio);
    fWindow             *= fWindow;
    float fClampedDist  = max(fDistance, MIN_LIGHT_DISTANCE);
    return (1.0 / (4.0 * PI * fClampedDist * fClampedDist)) * fWindow;
}

// Smoothstep penumbra. fCosOuter < fCosInner.
public float ComputeSpotConeFalloff(float3 xLightToFrag, float3 xSpotDir, float fCosInner, float fCosOuter)
{
    float fCosAngle = dot(xLightToFrag, xSpotDir);
    const float fConeEpsilon = 0.001;
    return smoothstep(fCosOuter - fConeEpsilon, fCosInner, fCosAngle);
}

// Cook-Torrance BRDF + multiscatter compensation. Same recipe as
// CookTorrance_Directional / ComputeIBLAmbient in the deferred shader,
// so static and dynamic lights agree on energy.
public float3 EvaluateCookTorrance(float3 xAlbedo,
                                    float3 xNormal,
                                    float3 xViewDir,
                                    float3 xLightDir,
                                    float3 xLightColor,
                                    float  fMetallic,
                                    float  fRoughness,
                                    float  fSpecular,
                                    float  fAttenuation,
                                    Sampler2D xBRDFLUT)
{
    float3 xHalfDir = normalize(xLightDir + xViewDir);
    float NdotL = max(dot(xNormal, xLightDir), 0.0);
    float NdotV = max(dot(xNormal, xViewDir),  PBR_EPSILON);
    float NdotH = max(dot(xNormal, xHalfDir),  0.0);
    float HdotV = max(dot(xHalfDir, xViewDir), 0.0);
    if (NdotL <= PBR_EPSILON) return float3(0.0, 0.0, 0.0);

    float fClampedRoughness = clamp(fRoughness, PBR_MIN_ROUGHNESS, 1.0);
    float3 F0 = ComputeF0(xAlbedo, fMetallic, fSpecular);

    float  D = DistributionGGX(NdotH, fClampedRoughness);
    float  G = GeometrySmith_Direct(NdotV, NdotL, fClampedRoughness);
    float3 F = FresnelSchlick(HdotV, F0);

    float3 numerator   = D * G * F;
    float  denom       = 4.0 * NdotV * NdotL;
    float3 specular    = numerator / max(denom, PBR_EPSILON);

    // Multi-scatter compensation — keeps brightness in line with IBL
    // on rough surfaces.
    float fRoughnessForLUT = clamp(fClampedRoughness, PBR_BRDF_LUT_MIN_ROUGHNESS, PBR_BRDF_LUT_MAX_ROUGHNESS);
    float2 xBRDF = xBRDFLUT.Sample(float2(NdotV, fRoughnessForLUT)).rg;
    float3 xMulti = MultiscatterBRDF(F0, NdotV, fRoughnessForLUT, xBRDF);
    float3 xSingle = F0 * xBRDF.x + xBRDF.y;
    specular *= xMulti / max(xSingle, float3(PBR_EPSILON, PBR_EPSILON, PBR_EPSILON));

    float3 kS = F;
    float3 kD = (1.0 - kS) * (1.0 - fMetallic);

    float3 diffuse  = kD * xAlbedo / PI;
    float3 radiance = xLightColor * fAttenuation;
    return (diffuse + specular) * radiance * NdotL;
}

// Single-call evaluator dispatched on light type. Branch is uniform per
// pixel within a wave only when a wave maps to a single cluster — across
// cluster boundaries the type can vary, but the cost is bounded by the
// per-cluster light count cap (MAX_LIGHTS_PER_CLUSTER) and any divergence
// is much cheaper than the redundant G-buffer sampling we replaced.
public float3 EvaluateLight(LightInstance xLight,
                            float3 xWorldPos,
                            float3 xNormal,
                            float3 xViewDir,
                            float3 xAlbedo,
                            float  fMetallic,
                            float  fRoughness,
                            float  fSpecular,
                            Sampler2D xBRDFLUT)
{
    uint   uType        = uint(xLight.m_xTypeOuter.y);
    float3 xLightPos    = xLight.m_xPositionRange.xyz;
    float  fRange       = xLight.m_xPositionRange.w;
    float3 xLightColor  = xLight.m_xColorIntensity.xyz;
    float  fIntensity   = xLight.m_xColorIntensity.w;

    float3 xLightDir;
    float  fAttenuation = fIntensity;

    if (uType == LIGHT_TYPE_DIRECTIONAL)
    {
        // m_xDirectionInner.xyz points FROM the light INTO the scene —
        // BRDF wants fragment-to-light, so negate.
        xLightDir = -xLight.m_xDirectionInner.xyz;
        // No distance falloff — directional intensity is already lux.
    }
    else
    {
        float3 xToLight = xLightPos - xWorldPos;
        float  fDistance = length(xToLight);
        if (fDistance > fRange) return float3(0.0, 0.0, 0.0);
        xLightDir    = xToLight / max(fDistance, MIN_LIGHT_DISTANCE);
        fAttenuation *= ComputePointAttenuation(fDistance, fRange);

        if (uType == LIGHT_TYPE_SPOT)
        {
            float fCosInner = xLight.m_xDirectionInner.w;
            float fCosOuter = xLight.m_xTypeOuter.x;
            float3 xSpotDir = xLight.m_xDirectionInner.xyz; // points light → scene
            // Cone test wants light-to-fragment, which is -xLightDir.
            fAttenuation *= ComputeSpotConeFalloff(-xLightDir, xSpotDir, fCosInner, fCosOuter);
        }
    }

    if (fAttenuation < LIGHT_CONTRIBUTION_THRESHOLD) return float3(0.0, 0.0, 0.0);

    return EvaluateCookTorrance(xAlbedo, xNormal, xViewDir, xLightDir,
                                xLightColor, fMetallic, fRoughness, fSpecular,
                                fAttenuation, xBRDFLUT);
}
