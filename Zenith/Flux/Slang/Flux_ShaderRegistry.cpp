#include "Zenith.h"
#include "Flux/Slang/Flux_ShaderRegistry.h"
#include "Flux/Slang/Flux_SlangCompiler.h"

// Registry source of truth. Each newly-ported subsystem appends its programs
// here. Order in this array does NOT need to match FluxShaderProgram enum
// order — GetProgram(eId) does an O(N) search keyed on m_eId. N is small
// enough for the next several years that this isn't worth a side table.
static const Flux_ShaderRegistryEntry s_axRegistry[] =
{
	// Pilot: fullscreen textured-quad blit. Used by Zenith_Vulkan_Swapchain
	// to copy the final frame to the backbuffer. Single combined sampler
	// at set=0 binding=0; no constant buffers, no parameter blocks.
	{
		FluxShaderProgram::TexturedQuad,
		"TexturedQuad",
		"Quads/Flux_TexturedQuad",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Present",
	},

	// HiZ depth-pyramid generator. Compute pilot — proves the compute path
	// through CompileProgram + InitialiseCompute(FluxShaderProgram). Combined
	// sampler g_xInputTex at b0, RWTexture2D g_xOutputTex at b1, CB
	// pushConstants at b2.
	{
		FluxShaderProgram::HiZ_Generate,
		"HiZ_Generate",
		"HiZ/Flux_HiZ_Generate",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"HiZ",
	},

	// HDR Bloom — three-pass chain (threshold → downsample mips → upsample
	// mips additively). All three share Common.Fullscreen for vsMain so the
	// bloom passes prove `import` / cross-module composition through
	// CompileProgram. Same binding layout: BloomConstants CB at b0,
	// source texture at b1.
	{
		FluxShaderProgram::BloomThreshold,
		"BloomThreshold",
		"HDR/Flux_BloomThreshold",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"HDR",
	},
	{
		FluxShaderProgram::BloomDownsample,
		"BloomDownsample",
		"HDR/Flux_BloomDownsample",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"HDR",
	},
	{
		FluxShaderProgram::BloomUpsample,
		"BloomUpsample",
		"HDR/Flux_BloomUpsample",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"HDR",
	},

	// Auto-exposure compute pair: Luminance builds a 256-bin histogram in
	// log space, Adaptation reduces it (single 256-thread workgroup) into
	// an adapted exposure scalar. Both compute-only.
	{
		FluxShaderProgram::HDR_Luminance,
		"HDR_Luminance",
		"HDR/Flux_Luminance",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"HDR",
	},
	{
		FluxShaderProgram::HDR_Adaptation,
		"HDR_Adaptation",
		"HDR/Flux_Adaptation",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"HDR",
	},

	// Skybox solid-colour fallback. Writes the override colour to the
	// G-buffer (MRT0=diffuse, MRT1=normals+ambient=0, MRT2=material).
	// Doesn't depend on FrameConstants — the cubemap / atmosphere
	// variants need Common.slang and land in a follow-up batch.
	{
		FluxShaderProgram::SkyboxSolidColour,
		"SkyboxSolidColour",
		"Skybox/Flux_SkyboxSolidColour",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Skybox",
	},

	// Cubemap skybox — first shader to depend on Common.Frame (the Slang
	// port of Common.fxh's FrameConstants UBO + RayDir / depth helpers).
	// Proves the import path for shaders that reconstruct world directions
	// from screen UVs.
	{
		FluxShaderProgram::SkyboxCubemap,
		"SkyboxCubemap",
		"Skybox/Flux_Skybox",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Skybox",
	},

	// HDR tone-mapping composite. Five tone curves + a battery of debug
	// overlays. Reads HDR scene + bloom + auto-exposure histogram /
	// exposure data; standalone (no .fxh dependency).
	{
		FluxShaderProgram::HDR_ToneMapping,
		"HDR_ToneMapping",
		"HDR/Flux_ToneMapping",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"HDR",
	},

	// IBL split-sum BRDF LUT generator. First shader to depend on
	// Common.PBR — exercises Hammersley, ImportanceSampleGGX,
	// GeometrySmith_IBL, and the PBR_FRESNEL_POWER constant.
	{
		FluxShaderProgram::IBL_BRDFIntegration,
		"IBL_BRDFIntegration",
		"IBL/Flux_BRDFIntegration",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"IBL",
	},

	// SSAO half-res-to-full-res bilateral upsample. Standalone — no
	// FrameConstants / PBR / GBuffer dependencies. First shader in the
	// SSAO/SSGI/SSR cluster to land.
	{
		FluxShaderProgram::SSAO_Upsample,
		"SSAO_Upsample",
		"SSAO/Flux_SSAO_Upsample",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSAO",
	},

	// SSAO joint bilateral blur (half-res). Edge-stopping by depth +
	// normal weights to preserve geometry boundaries.
	{
		FluxShaderProgram::SSAO_Blur,
		"SSAO_Blur",
		"SSAO/Flux_SSAO_Blur",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSAO",
	},

	// SSAO main pass — half-res ray-marched horizon-based occlusion.
	// Imports Common.Frame for view+proj matrices and view-space
	// depth reconstruction.
	{
		FluxShaderProgram::SSAO_Main,
		"SSAO_Main",
		"SSAO/Flux_SSAO",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSAO",
	},

	// SSGI half-res-to-full-res bilateral upsample. Standalone — depth-
	// weighted, no normals/albedo (the temporal accumulator already cleaned).
	{
		FluxShaderProgram::SSGI_Upsample,
		"SSGI_Upsample",
		"SSGI/Flux_SSGI_Upsample",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSGI",
	},

	// SSGI separable joint bilateral denoise — horizontal half. Pairs with
	// SSGI_DenoiseV; together they approximate a 2D joint bilateral with
	// O(2r) cost instead of O(r²). Strict bilateral non-separability is
	// negligible at this kernel size.
	{
		FluxShaderProgram::SSGI_DenoiseH,
		"SSGI_DenoiseH",
		"SSGI/Flux_SSGI_DenoiseH",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSGI",
	},

	// SSGI separable joint bilateral denoise — vertical half. Reads the
	// horizontal-blurred intermediate and writes the final denoised SSGI.
	{
		FluxShaderProgram::SSGI_DenoiseV,
		"SSGI_DenoiseV",
		"SSGI/Flux_SSGI_DenoiseV",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSGI",
	},

	// SSGI ray-march pass. Multi-ray cosine-weighted hemisphere sampling
	// with HiZ-accelerated traversal. Imports Common.Frame for view-space
	// helpers and Common.PBR for TWO_PI / direction epsilon.
	{
		FluxShaderProgram::SSGI_RayMarch,
		"SSGI_RayMarch",
		"SSGI/Flux_SSGI_RayMarch",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSGI",
	},

	// SSR ray-march pass. GGX-perturbed reflection rays + HiZ traversal +
	// world-space contact-hardening confidence. Imports Common.Frame and
	// Common.PBR (ImportanceSampleGGX).
	{
		FluxShaderProgram::SSR_RayMarch,
		"SSR_RayMarch",
		"SSR/Flux_SSR_RayMarch",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSR",
	},

	// SSR denoise — separable joint-bilateral horizontal pass. Replaces the
	// retired SSR_Resolve. Roughness gating runs at the top so smooth/rough
	// pixels pass through without cost. Push constants drive sigma/radius
	// (mirrors SSGI's denoise pattern). Imports Common.Frame for screen dims.
	{
		FluxShaderProgram::SSR_DenoiseH,
		"SSR_DenoiseH",
		"SSR/Flux_SSR_DenoiseH",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSR",
	},

	// SSR denoise — separable joint-bilateral vertical pass. Reads the H
	// intermediate; same gates and weights as H. Outputs the canonical
	// denoised RGBA16F that the deferred shader consumes.
	{
		FluxShaderProgram::SSR_DenoiseV,
		"SSR_DenoiseV",
		"SSR/Flux_SSR_DenoiseV",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSR",
	},

	// SSR bilateral upsample pass. Half-res ray-march output + full-res
	// depth → full-res RGBA16F (RGB = reflection colour, A = confidence).
	// Texture dimensions come from the SSR CBV — no GetDimensions inside
	// the shader.
	{
		FluxShaderProgram::SSR_Upsample,
		"SSR_Upsample",
		"SSR/Flux_SSR_Upsample",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SSR",
	},

	// Simple distance-based fog with Henyey-Greenstein sun scatter. First
	// shader to depend on Common.Volumetric — proves the volumetric module
	// import path before tackling the heavier froxel/raymarch fog.
	{
		FluxShaderProgram::Fog_Simple,
		"Fog_Simple",
		"Fog/Flux_Fog",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Fog",
	},

	// Screen-space god-rays — radial blur from the light source position,
	// gated on sky pixels. Artistic decay model (no Beer-Lambert).
	{
		FluxShaderProgram::Fog_GodRays,
		"Fog_GodRays",
		"Fog/Flux_GodRays",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Fog",
	},

	// Froxel-fog compositing pass. Reads precomputed lighting + scattering
	// 3D textures and accumulates Beer-Lambert in-scatter along the view
	// ray. First shader to use Sampler3D in the Slang port.
	{
		FluxShaderProgram::Fog_FroxelApply,
		"Fog_FroxelApply",
		"Fog/Flux_FroxelFog_Apply",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Fog",
	},

	// Froxel-fog density injection compute. 8x8x8 workgroup, one thread per
	// froxel cell. Combines uniform base density, height fog falloff, and
	// animated 3D noise into the density volume.
	{
		FluxShaderProgram::Fog_FroxelInject,
		"Fog_FroxelInject",
		"Fog/Flux_FroxelFog_Inject",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"Fog",
	},

	// Froxel-fog directional-light + ambient compute. Henyey-Greenstein on
	// the sun direction, CSM-sampled volumetric shadows, sky ambient term;
	// writes both lighting and scattering volumes for the Apply pass.
	{
		FluxShaderProgram::Fog_FroxelLight,
		"Fog_FroxelLight",
		"Fog/Flux_FroxelFog_Light",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"Fog",
	},

	// Raymarched volumetric fog (technique 2). Per-pixel ray walks the
	// camera ray through 3D-noise-modulated density, with HG scattering +
	// Beer-Lambert extinction + CSM-shadowed direct light contribution.
	{
		FluxShaderProgram::Fog_Raymarch,
		"Fog_Raymarch",
		"Fog/Flux_RaymarchFog",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Fog",
	},

	// Temporal froxel fog resolve (compute). 3D current/history blend with
	// neighbourhood clamping. No active call site yet — registered so it
	// builds through the same path as the other fog passes.
	{
		FluxShaderProgram::Fog_TemporalResolve,
		"Fog_TemporalResolve",
		"Fog/Flux_TemporalFog_Resolve",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"Fog",
	},

	// IBL diffuse irradiance convolution. Cosine-weighted hemisphere
	// importance sampling with Hammersley; one face per dispatch. First
	// shader to depend on Common.Atmosphere (used when sampling a
	// procedural sky for the IBL source).
	{
		FluxShaderProgram::IBL_IrradianceConvolution,
		"IBL_IrradianceConvolution",
		"IBL/Flux_IrradianceConvolution",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"IBL",
	},

	// IBL specular prefilter. GGX importance sampling, one mip × face per
	// dispatch, with smooth-roughness blend toward a direct mirror to
	// hide GGX delta-function artefacts at near-zero roughness.
	{
		FluxShaderProgram::IBL_PrefilterEnvMap,
		"IBL_PrefilterEnvMap",
		"IBL/Flux_PrefilterEnvMap",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"IBL",
	},

	// Procedural physically-based sky (Rayleigh + Mie). Writes the
	// G-buffer with emissive luminance equal to the sky colour
	// magnitude.
	{
		FluxShaderProgram::SkyboxAtmosphere,
		"SkyboxAtmosphere",
		"Skybox/Flux_Atmosphere",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Skybox",
	},

	// Deferred shading composite. Reads the G-buffer + shadow maps + IBL
	// (BRDF LUT, irradiance, prefiltered env) + SSR + SSGI and produces
	// the final lit HDR colour. Heaviest shader so far — exercises the
	// matrix-layout fix, every helper in Common.PBR, and 4-cascade PCF
	// shadow gather.
	{
		FluxShaderProgram::DeferredShading,
		"DeferredShading",
		"DeferredShading/Flux_DeferredShading",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"DeferredShading",
	},

	// Sphere-traced SDF debug pass.
	{
		FluxShaderProgram::SDFs,
		"SDFs",
		"SDFs/Flux_SDFs",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"SDFs",
	},

	// Instanced UI quads — first shader using an unbounded bindless
	// sampler array (set=1 binding=0).
	{
		FluxShaderProgram::Quads,
		"Quads",
		"Quads/Flux_Quads",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Quads",
	},

	// Instanced glyph rasterisation with overlay clip-rect discard.
	{
		FluxShaderProgram::Text,
		"Text",
		"Text/Flux_Text",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Text",
	},

	// Debug primitives writing the G-buffer with matte material.
	{
		FluxShaderProgram::Primitives,
		"Primitives",
		"Primitives/Flux_Primitives",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Primitives",
	},

	// Editor gizmos — unlit per-vertex colour with hover-highlight.
	{
		FluxShaderProgram::Gizmos,
		"Gizmos",
		"Gizmos/Flux_Gizmos",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Gizmos",
	},

	// Static mesh G-buffer pass. First entry to import Common.Material +
	// Common.DrawConstants; gateway shader for the engine's mesh path.
	{
		FluxShaderProgram::StaticMesh_ToGBuffer,
		"StaticMesh_ToGBuffer",
		"StaticMeshes/Flux_StaticMesh_ToGBuffer",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"StaticMeshes",
	},

	// Static mesh shadow-map pass — depth-only, no colour output.
	{
		FluxShaderProgram::StaticMesh_ToShadowmap,
		"StaticMesh_ToShadowmap",
		"StaticMeshes/Flux_StaticMesh_ToShadowmap",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"StaticMeshes",
	},

	// Skeletal-animated mesh G-buffer pass — bone-weighted TBN.
	{
		FluxShaderProgram::AnimatedMesh_ToGBuffer,
		"AnimatedMesh_ToGBuffer",
		"AnimatedMeshes/Flux_AnimatedMesh_ToGBuffer",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"AnimatedMeshes",
	},
	// Skeletal-animated mesh shadow pass.
	{
		FluxShaderProgram::AnimatedMesh_ToShadowmap,
		"AnimatedMesh_ToShadowmap",
		"AnimatedMeshes/Flux_AnimatedMesh_ToShadowmap",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"AnimatedMeshes",
	},

	// GPU-instanced mesh G-buffer pass with VAT vertex animation and
	// indirection-table-driven per-instance transforms.
	{
		FluxShaderProgram::InstancedMesh_ToGBuffer,
		"InstancedMesh_ToGBuffer",
		"InstancedMeshes/Flux_InstancedMesh_ToGBuffer",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"InstancedMeshes",
	},
	// GPU-instanced mesh shadow pass — VAT skipped for cost.
	{
		FluxShaderProgram::InstancedMesh_ToShadowmap,
		"InstancedMesh_ToShadowmap",
		"InstancedMeshes/Flux_InstancedMesh_ToShadowmap",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"InstancedMeshes",
	},

	// GPU frustum culling for instanced meshes — atomic compaction into
	// VisibleIndexBuffer + indirect-draw instanceCount update.
	{
		FluxShaderProgram::InstanceCulling,
		"InstanceCulling",
		"InstancedMeshes/Flux_InstanceCulling",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"InstancedMeshes",
	},

	// Terrain G-buffer pass — splatmap-blended 4-material PBR. 22 bindings
	// in set 1 (per-draw CB + LOD SSBO + splatmap + 4 materials × 5 textures).
	{
		FluxShaderProgram::Terrain_ToGBuffer,
		"Terrain_ToGBuffer",
		"Terrain/Flux_Terrain_ToGBuffer",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Terrain",
	},
	// Terrain shadow pass — depth-only.
	{
		FluxShaderProgram::Terrain_ToShadowmap,
		"Terrain_ToShadowmap",
		"Terrain/Flux_Terrain_ToShadowmap",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Terrain",
	},
	// GPU-driven terrain chunk culling + 2-level LOD selection.
	{
		FluxShaderProgram::TerrainCulling,
		"TerrainCulling",
		"Terrain/Flux_TerrainCulling",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"Terrain",
	},
	// Single-thread compute that zeroes the per-terrain visible-count
	// buffer. Runs as its own render-graph pass (DependsOn: culling) so
	// the graph can emit the UAV→UAV barrier against the culling
	// dispatch's atomic increments. See the slang module for why this
	// can't be folded into the culling shader as a thread-0 trick.
	{
		FluxShaderProgram::TerrainResetCounters,
		"TerrainResetCounters",
		"Terrain/Flux_TerrainResetCounters",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"Terrain",
	},

	// Per-instance grass blade rendering with layered-wave wind, distance
	// fade, wrap lighting + translucency. Forward (HDR-blended).
	{
		FluxShaderProgram::Grass,
		"Grass",
		"Vegetation/Flux_Grass",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Vegetation",
	},

	// Clustered deferred light culling — compute. One thread per cluster
	// (16x9x24 = 3456), tests sphere-vs-AABB for each light, writes per-
	// cluster index lists consumed by the deferred-shading fragment shader.
	// Replaces the old per-light-volume rasterisation pass (which was
	// GPU-bound from disabled depth testing + redundant G-buffer sampling).
	{
		FluxShaderProgram::LightClustering,
		"LightClustering",
		"DynamicLights/Flux_LightClustering",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"DynamicLights",
	},

	// GPU particle billboards — camera-facing quads with per-instance
	// position/size/colour from the ParticleUpdate compute pass.
	{
		FluxShaderProgram::Particles,
		"Particles",
		"Particles/Flux_Particles",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Particles",
	},
	// GPU particle integration: gravity + drag + per-particle turbulence;
	// emits compacted render instances with atomic alive-count.
	{
		FluxShaderProgram::ParticleUpdate,
		"ParticleUpdate",
		"Particles/Flux_ParticleUpdate",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"Particles",
	},

	// Simple water plane — single sun lambert against a tangent-space
	// normal map, alpha-blended over the terrain.
	{
		FluxShaderProgram::Water,
		"Water",
		"Water/Flux_Water",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Water",
	},

	// Compute-pipeline validation kernel.
	{
		FluxShaderProgram::ComputeTest,
		"ComputeTest",
		"ComputeTest/ComputeTest",
		nullptr,
		nullptr,
		"csMain",
		"spirv_1_3",
		"ComputeTest",
	},
	// Fullscreen-triangle blit that displays the compute test's output.
	{
		FluxShaderProgram::ComputeTest_Display,
		"ComputeTest_Display",
		"ComputeTest/ComputeTest_Display",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"ComputeTest",
	},

	// Deferred screen-space box decals — pre-Apply normalsAmbient clone
	// pass. Required because Apply both reads and writes the live
	// normalsAmbient MRT; Flux's render graph doesn't expose subpass
	// self-dependencies, so the standard pattern is a transient copy.
	{
		FluxShaderProgram::Decals_NormalsCopy,
		"Decals_NormalsCopy",
		"Decals/Flux_Decals_NormalsCopy",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Decals",
	},
	// Decal apply pass — instanced unit cube; per-fragment box test +
	// surface-alignment discard; alpha-blends a procedural bullet-hole
	// pattern into all three G-buffer MRTs.
	{
		FluxShaderProgram::Decals_Apply,
		"Decals_Apply",
		"Decals/Flux_Decals_Apply",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Decals",
	},

	// DevilsPlayground game-side fog (EXT-1). Replaces the engine fog
	// (which is disabled via the render graph force-disable overlay,
	// SetOwnerForceDisabled("Fog")) with
	// exponential distance fog and circular "holes" around villagers /
	// lights. Up to DP_FOG_MAX_HOLES (60) holes per frame uploaded as a CBV
	// array — see DPFogPass.cpp for the matching C++ cap and
	// Zenith/Flux/Shaders/Fog/DP_Fog.slang for the shader-side constant.
	{
		FluxShaderProgram::DevilsPlayground_DPFog,
		"DevilsPlayground_DPFog",
		"Fog/DP_Fog",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Fog",
	},

	// Forward-lit translucent mesh pass (Translucent/Additive blend modes).
	// Also reused by the editor material preview against its own targets.
	{
		FluxShaderProgram::Translucent_Forward,
		"Translucent_Forward",
		"Translucency/Flux_Translucent_Forward",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Translucency",
	},

	// Editor material-preview offscreen renderer (TOOLS): environment-cubemap
	// background + fixed-exposure tonemap to the ImGui-visible LDR target.
	{
		FluxShaderProgram::MaterialPreview_Background,
		"MaterialPreview_Background",
		"MaterialPreview/Flux_MaterialPreview_Background",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"MaterialPreview",
	},
	{
		FluxShaderProgram::MaterialPreview_Tonemap,
		"MaterialPreview_Tonemap",
		"MaterialPreview/Flux_MaterialPreview_Tonemap",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"MaterialPreview",
	},

	// Atmosphere transmittance LUT generator (256x64). Precomputes sun-ray
	// transmittance to the atmosphere top, sampled by the atmosphere solver to
	// replace the per-pixel inner light-ray march. Appended at the array END so
	// its FluxShaderProgram id stays the last value (matches a FluxCompiler regen).
	{
		FluxShaderProgram::SkyboxTransmittanceLUT,
		"SkyboxTransmittanceLUT",
		"Skybox/Flux_TransmittanceLUT",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Skybox",
	},

	// Atmosphere sky-view LUT generator (low-res lat-long). Runs the single-
	// scatter raymarch once per frame per LUT texel (sampling the transmittance
	// LUT), so the fullscreen sky pass samples it instead of raymarching per
	// pixel. Appended at the array END to keep its id last (FluxCompiler-regen safe).
	{
		FluxShaderProgram::SkyboxSkyViewLUT,
		"SkyboxSkyViewLUT",
		"Skybox/Flux_SkyViewLUT",
		"vsMain",
		"fsMain",
		nullptr,
		"spirv_1_3",
		"Skybox",
	},
};

static constexpr u_int kRegistryCount = sizeof(s_axRegistry) / sizeof(s_axRegistry[0]);
static_assert(kRegistryCount == static_cast<u_int>(FluxShaderProgram::COUNT),
	"Flux_ShaderRegistry: registry size does not match FluxShaderProgram::COUNT — "
	"either the generated enum is stale or a registry entry is missing.");

u_int Flux_ShaderRegistry::GetProgramCount()
{
	return kRegistryCount;
}

const Flux_ShaderRegistryEntry& Flux_ShaderRegistry::GetProgramByIndex(u_int uIndex)
{
	Zenith_Assert(uIndex < kRegistryCount, "Flux_ShaderRegistry: index %u out of bounds (count %u)", uIndex, kRegistryCount);
	return s_axRegistry[uIndex];
}

const Flux_ShaderRegistryEntry& Flux_ShaderRegistry::GetProgram(FluxShaderProgram eId)
{
	for (u_int u = 0; u < kRegistryCount; u++)
	{
		if (s_axRegistry[u].m_eId == eId) return s_axRegistry[u];
	}
	Zenith_Assert(false, "Flux_ShaderRegistry: program id %u not registered", static_cast<u_int>(eId));
	return s_axRegistry[0];
}

void Flux_ShaderRegistry::DescribeProgram(FluxShaderProgram eId, Flux_SlangProgramDesc& xDescOut)
{
	const Flux_ShaderRegistryEntry& xEntry = GetProgram(eId);
	xDescOut.m_szModuleName    = xEntry.m_szModuleName;
	xDescOut.m_szVertexEntry   = xEntry.m_szVertexEntry;
	xDescOut.m_szFragmentEntry = xEntry.m_szFragmentEntry;
	xDescOut.m_szComputeEntry  = xEntry.m_szComputeEntry;
	xDescOut.m_szTargetProfile = xEntry.m_szTargetProfile;
}

// Per-stage artifact stems disambiguate vertex vs fragment SPIR-V from the
// same module — two stages share a module file but produce two .spv blobs.
std::string Flux_ShaderRegistry::GetVertexArtifactStem(FluxShaderProgram eId)
{
	const Flux_ShaderRegistryEntry& xEntry = GetProgram(eId);
	Zenith_Assert(xEntry.m_szVertexEntry, "GetVertexArtifactStem: program '%s' has no vertex stage", xEntry.m_szName);
	return std::string(xEntry.m_szModuleName) + "." + xEntry.m_szVertexEntry;
}

std::string Flux_ShaderRegistry::GetFragmentArtifactStem(FluxShaderProgram eId)
{
	const Flux_ShaderRegistryEntry& xEntry = GetProgram(eId);
	Zenith_Assert(xEntry.m_szFragmentEntry, "GetFragmentArtifactStem: program '%s' has no fragment stage", xEntry.m_szName);
	return std::string(xEntry.m_szModuleName) + "." + xEntry.m_szFragmentEntry;
}

std::string Flux_ShaderRegistry::GetComputeArtifactStem(FluxShaderProgram eId)
{
	const Flux_ShaderRegistryEntry& xEntry = GetProgram(eId);
	Zenith_Assert(xEntry.m_szComputeEntry, "GetComputeArtifactStem: program '%s' has no compute stage", xEntry.m_szName);
	return std::string(xEntry.m_szModuleName) + "." + xEntry.m_szComputeEntry;
}
