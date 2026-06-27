// =====================================================================
// Common/Bindings — the canonical frequency-set spine.
//
// TEXT-INCLUDED (preprocessor #include, NOT `import`) at the TOP of every
// shader so the spine ParameterBlocks are declared FIRST and occupy the fixed
// persistent descriptor sets:
//     set 0 = GLOBAL   (view-invariant: time / frame / global lighting)
//     set 1 = VIEW     (per-camera: view/proj matrices, + view-frequency SRVs)
//     set 2 = BINDLESS (the unbounded sampled-texture table)
// The shader then declares its OWN local PassParams (set 3) / DrawParams
// (set 4+) AFTER this include. `import` would reorder local-before-imported and
// break the spine — `#include` preserves declaration order. Declared-but-
// unreferenced blocks keep their space (Slang does not dead-strip them), so a
// shader that uses only VIEW still gets VIEW at set 1. (All three facts proven
// by the Phase-0/2 reflection experiments.)
//
// GROW the GLOBAL/VIEW blocks here as shaders are converted; because every
// converted shader #includes this one file, the layout stays identical across
// them (required for the persistent sets).
// =====================================================================

import Common.MaterialTable;   // MaterialGPU (no bindings — does not shift the spine spaces)
import Common.Lighting;        // LightInstance (no bindings — does not shift the spine spaces)

// Directional light data struct (moved from Common.Frame so spine shaders keep it).
public struct DirectionalLight
{
	public float4 m_xDirection; // 4 bytes of padding
	public float4 m_xColour;
};

// ---- GLOBAL (set 0) — view-invariant per-frame data ----
public struct GlobalConstantsLayout
{
	public float4 g_xSunDir_Pad;     // xyz = sun direction
	public float4 g_xSunColour_Pad;  // rgb = sun colour
	public float  g_fTimeSeconds;
	public uint   g_uFrameIndex;
	public float2 g_xGlobalPad;
};
struct GlobalParams
{
	ConstantBuffer<GlobalConstantsLayout> g_xGlobal;       // binding 0
	StructuredBuffer<MaterialGPU>         g_axMaterials;   // binding 1 — GPU material table
};
ParameterBlock<GlobalParams> g_xGlobalSet;

// g_axMaterials (the GPU material table) lives in the GLOBAL set (Phase 5.3). A
// multi-member spine ParameterBlock forces its binding-0 (g_xGlobal) to be statically
// used by EVERY shader that #includes this file, so the GLOBAL set MUST be bound in
// every shader — which it now is, PERSISTENTLY (Phase 5.1: g_xGlobalSet is written once
// per frame in PreparePersistentSets and bound for every pipeline). Material shaders read
// their record via g_xGlobalSet.g_axMaterials[DrawConstants.g_uMaterialIndex]; non-material
// shaders never reference g_axMaterials (reflected-but-unused → no per-draw cost). This
// killed the fragile per-draw hg_axMaterials re-stage the old DRAW-block placement needed.

// ---- VIEW (set 1) — per-camera data + view-frequency resources ----
public struct ViewConstantsLayout
{
	public float4x4 g_xViewMat;
	public float4x4 g_xProjMat;
	public float4x4 g_xViewProjMat;
	public float4x4 g_xInvViewProjMat;
	public float4x4 g_xInvViewMat;
	public float4x4 g_xInvProjMat;
	public float4   g_xCamPos_Pad;
	public uint2    g_xScreenDims;
	public float2   g_xRcpScreenDims;
	public uint     g_bQuadUtilisationAnalysis;
	public uint     g_uTargetPixelsPerTri;
	public float2   g_xCameraNearFar;
};
struct ViewParams
{
	ConstantBuffer<ViewConstantsLayout> g_xView;            // binding 0
	Sampler2DArray                      g_xCSM;             // binding 1 — cascaded shadow-map array (Phase 5.4)
	StructuredBuffer<float4x4>          g_xShadowMatrices;      // binding 2 — all-cascade sun view×proj SSBO (Phase 5.4)
	StructuredBuffer<LightInstance>     g_xLightBuffer;         // binding 3 — dynamic light list (Phase 5.4)
	StructuredBuffer<uint>              g_xClusterLightCounts;  // binding 4 — per-cluster light count (Phase 5.4)
	StructuredBuffer<uint>              g_xClusterLightIndices; // binding 5 — per-cluster light index list (Phase 5.4)
	Sampler2D                           g_xBRDFLUT;             // binding 6 — IBL BRDF integration LUT (Phase 5.4)
	SamplerCube                         g_xIrradianceMap;       // binding 7 — IBL diffuse irradiance cube (Phase 5.4)
	SamplerCube                         g_xPrefilteredMap;      // binding 8 — IBL prefiltered specular cube (Phase 5.4)
	// (further view-frequency resources are added here as the consuming shaders
	//  are converted, in lockstep with the VIEW layout in
	//  Zenith_Vulkan::CreatePersistentDescriptorSets + the canonical layout check
	//  in Flux_PersistentSetLayouts::ValidateCanonicalGroup + the kaszViewMemberNames
	//  manifest + the Flux_ViewSetBinding registry row.)
};
ParameterBlock<ViewParams> g_xViewSet;

// ---- BINDLESS (set 2) — unbounded sampled-texture table ----
struct BindlessParams
{
	Sampler2D g_axTextures[];
};
ParameterBlock<BindlessParams> g_xBindlessSet;

// ---- View-space helpers (spine versions of Common.Frame's, reading g_xViewSet) ----
// Converted shaders #include this instead of importing Common.Frame, so the same
// reconstruction helpers must be available here, sourced from the VIEW set.
float3 RayDir(float2 xUV)
{
	float2 xNDC = xUV * 2.0 - 1.0;
	float4 xClip = float4(xNDC, 1.0, 1.0);
	float4 xView = mul(g_xViewSet.g_xView.g_xInvProjMat, xClip);
	xView.w = 0.0;
	return normalize(mul(g_xViewSet.g_xView.g_xInvViewMat, xView).xyz);
}
float3 GetWorldPosFromDepthTex(Sampler2D xDepthTex, float2 xUV)
{
	float fDepth = xDepthTex.Sample(xUV).x;
	float2 xNDC = xUV * 2.0 - 1.0;
	float4 xClip = float4(xNDC, fDepth, 1.0);
	float4 xView = mul(g_xViewSet.g_xView.g_xInvProjMat, xClip);
	xView /= xView.w;
	return mul(g_xViewSet.g_xView.g_xInvViewMat, xView).xyz;
}
float3 GetViewPosFromDepth(float2 xUV, float fDepth)
{
	float2 xNDC = xUV * 2.0 - 1.0;
	float4 xClip = float4(xNDC, fDepth, 1.0);
	float4 xView = mul(g_xViewSet.g_xView.g_xInvProjMat, xClip);
	return xView.xyz / xView.w;
}
float DepthToViewZ(float2 xUV, float fDepth)
{
	float2 xNDC = xUV * 2.0 - 1.0;
	float4 xClip = float4(xNDC, fDepth, 1.0);
	float4 xView = mul(g_xViewSet.g_xView.g_xInvProjMat, xClip);
	return xView.z / xView.w;
}
float3 ViewToScreen(float3 xViewPos)
{
	float4 xClip = mul(g_xViewSet.g_xView.g_xProjMat, float4(xViewPos, 1.0));
	xClip.xyz /= xClip.w;
	return float3(xClip.x * 0.5 + 0.5, xClip.y * 0.5 + 0.5, xClip.z);
}
float ComputeEdgeFade(float2 xUV, float fMargin)
{
	float2 xFade = smoothstep(0.0, fMargin, xUV) * smoothstep(0.0, fMargin, 1.0 - xUV);
	return xFade.x * xFade.y;
}
static const int   BLUE_NOISE_SIZE = 64;
static const float GOLDEN_RATIO    = 0.618033988749;
