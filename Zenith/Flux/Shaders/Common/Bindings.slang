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
// (The sun moved into ViewConstantsLayout: each render view owns its sun so a
// secondary view — the editor material preview — can light itself independently.)
public struct GlobalConstantsLayout
{
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
	// Per-view sun (moved from GlobalConstantsLayout): the main view carries the
	// scene sun; the preview view carries its own orbit light.
	public float4   g_xSunDir_Pad;     // xyz = sun direction
	public float4   g_xSunColour_Pad;  // rgb = sun colour, a = intensity
	// Per-view feature flags (FLUX_VIEW_FLAG_* below) + this view's registry slot.
	public uint     g_uViewFlags;
	public uint     g_uViewSlot;
	// Rcp OUTPUT (swapchain) dims. Under temporal upscaling g_xRcpScreenDims (above) is the RENDER
	// res, but the UI/text overlay (Quads/Text) draws onto the output-res FinalRT — they read THIS
	// for pixel->NDC. Equals g_xRcpScreenDims for non-upscaled views. (Repurposed pad @472.)
	public float2   g_xRcpOutputDims;
	// TAA temporal foundation (Stage 2): the UNJITTERED current + previous view-proj
	// (g_xViewProjMat above carries the sub-pixel jitter; these never do). The velocity
	// pass reprojects with them so motion vectors are jitter-free at the source. GPU
	// cull reads g_xViewProjMatNoJitter, never the jittered g_xViewProjMat. Non-main
	// views (cascades/preview) stage NoJitter == their own unjittered view-proj.
	// g_xJitterUV_PrevJitterUV: xy = this frame's jitter offset in UV space, zw = the
	// previous frame's (both zero while jitter is disabled).
	public float4x4 g_xViewProjMatNoJitter;
	public float4x4 g_xPrevViewProjMatNoJitter;
	public float4   g_xJitterUV_PrevJitterUV;
};

// Per-view feature-flag bits in g_uViewFlags — mirror FLUX_VIEW_FLAG_* in
// Flux/RenderViews/Flux_RenderViews.h (change in lockstep). Lit shaders gate
// CSM + clustered-light sampling per view (the preview view disables both).
public static const uint FLUX_VIEW_FLAG_SHADOWS_ENABLED        = 1u << 0;
public static const uint FLUX_VIEW_FLAG_CLUSTER_LIGHTS_ENABLED = 1u << 1;
public static const uint FLUX_VIEW_FLAG_SCENE_CONTENT          = 1u << 2;
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

// ============================================================================
// Spine accessor facade (Flux Shader System Overhaul — D1).
//
// Feature shaders reach the GLOBAL / VIEW / BINDLESS sets ONLY through these free
// functions; the raw g_xGlobalSet / g_xViewSet / g_xBindlessSet instances never
// appear outside THIS file (enforced by the FluxCompiler spine lint, D6). Each is a
// trivial pure forwarder — substituting it for the former direct poke is referentially
// transparent (analytically behaviour-preserving). Slang re-numbers SSA ids / re-
// materialises pointer types for the refactored source, so the .spv regenerate
// (semantically identical, reflection byte-identical) rather than staying byte-identical.
//
// Sampler accessors return the SAMPLER (not a pre-baked sample) so each call site
// keeps its exact .Sample / .SampleLevel / .Gather form (Stage-0 probe E1d).
// ============================================================================

// --- GLOBAL (set 0) ---
float    GetTimeSeconds()  { return g_xGlobalSet.g_xGlobal.g_fTimeSeconds; }
uint     GetFrameIndex()   { return g_xGlobalSet.g_xGlobal.g_uFrameIndex; }
MaterialGPU GetMaterial(uint uIndex) { return g_xGlobalSet.g_axMaterials[uIndex]; }

// --- VIEW (set 1) matrices ---
float4x4 GetViewMat()                 { return g_xViewSet.g_xView.g_xViewMat; }
float4x4 GetProjMat()                 { return g_xViewSet.g_xView.g_xProjMat; }
float4x4 GetViewProjMat()             { return g_xViewSet.g_xView.g_xViewProjMat; }
float4x4 GetInvViewProjMat()          { return g_xViewSet.g_xView.g_xInvViewProjMat; }
float4x4 GetInvViewMat()              { return g_xViewSet.g_xView.g_xInvViewMat; }
float4x4 GetInvProjMat()              { return g_xViewSet.g_xView.g_xInvProjMat; }
float4x4 GetViewProjMatNoJitter()     { return g_xViewSet.g_xView.g_xViewProjMatNoJitter; }
float4x4 GetPrevViewProjMatNoJitter() { return g_xViewSet.g_xView.g_xPrevViewProjMatNoJitter; }
float4   GetJitterUV_PrevJitterUV()   { return g_xViewSet.g_xView.g_xJitterUV_PrevJitterUV; }

// --- VIEW camera / screen ---
float4   GetCamPos_Pad()      { return g_xViewSet.g_xView.g_xCamPos_Pad; }
float3   GetCamPos()          { return g_xViewSet.g_xView.g_xCamPos_Pad.xyz; }
uint2    GetScreenDims()      { return g_xViewSet.g_xView.g_xScreenDims; }
float2   GetRcpScreenDims()   { return g_xViewSet.g_xView.g_xRcpScreenDims; }
float2   GetRcpOutputDims()   { return g_xViewSet.g_xView.g_xRcpOutputDims; }
float2   GetCameraNearFar()   { return g_xViewSet.g_xView.g_xCameraNearFar; }
uint     GetQuadUtilisationAnalysis() { return g_xViewSet.g_xView.g_bQuadUtilisationAnalysis; }
uint     GetTargetPixelsPerTri()      { return g_xViewSet.g_xView.g_uTargetPixelsPerTri; }

// --- VIEW sun (per-view) ---
float4   GetSunDir_Pad()         { return g_xViewSet.g_xView.g_xSunDir_Pad; }
float4   GetSunColour_Pad()      { return g_xViewSet.g_xView.g_xSunColour_Pad; }
float3   GetSunDir()             { return g_xViewSet.g_xView.g_xSunDir_Pad.xyz; }
float3   GetSunColour()          { return g_xViewSet.g_xView.g_xSunColour_Pad.rgb; }
float    GetSunIntensity()       { return g_xViewSet.g_xView.g_xSunColour_Pad.a; }
float4   GetSunColourIntensity() { return g_xViewSet.g_xView.g_xSunColour_Pad; }

// --- VIEW feature flags ---
uint     GetViewFlags()             { return g_xViewSet.g_xView.g_uViewFlags; }
uint     GetViewSlot()              { return g_xViewSet.g_xView.g_uViewSlot; }
bool     ViewShadowsEnabled()       { return (g_xViewSet.g_xView.g_uViewFlags & FLUX_VIEW_FLAG_SHADOWS_ENABLED) != 0u; }
bool     ViewClusterLightsEnabled() { return (g_xViewSet.g_xView.g_uViewFlags & FLUX_VIEW_FLAG_CLUSTER_LIGHTS_ENABLED) != 0u; }
bool     ViewHasSceneContent()      { return (g_xViewSet.g_xView.g_uViewFlags & FLUX_VIEW_FLAG_SCENE_CONTENT) != 0u; }

// --- VIEW shadows / cluster lights (structured-buffer element loads) ---
float4x4 GetShadowMatrix(int iCascade)      { return g_xViewSet.g_xShadowMatrices[iCascade]; }
LightInstance GetLight(uint uIndex)         { return g_xViewSet.g_xLightBuffer[uIndex]; }
uint     GetClusterLightCount(uint uCluster){ return g_xViewSet.g_xClusterLightCounts[uCluster]; }
uint     GetClusterLightIndex(uint uIndex)  { return g_xViewSet.g_xClusterLightIndices[uIndex]; }

// --- VIEW / BINDLESS samplers (return the sampler; call sites keep their form) ---
Sampler2DArray GetCSM()            { return g_xViewSet.g_xCSM; }
Sampler2D      GetBRDFLUT()        { return g_xViewSet.g_xBRDFLUT; }
SamplerCube    GetIrradianceMap()  { return g_xViewSet.g_xIrradianceMap; }
SamplerCube    GetPrefilteredMap() { return g_xViewSet.g_xPrefilteredMap; }
Sampler2D      GetBindlessTexture(uint uIndex) { return g_xBindlessSet.g_axTextures[uIndex]; }

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
