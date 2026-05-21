#include "Zenith.h"

#include "Source/DPFogPass.h"
#include "Source/PublicInterfaces.h"

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Zenith_GameRenderHook.h"
#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Particles/Flux_ParticleEmitterConfig.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

#include "Flux/Shaders/Generated/Fog.h"

#include "DebugVariables/Zenith_DebugVariables.h"

#include <cstring>

namespace
{
	// Forward decl — body below uses Flux_RenderGraph internals.
	void SetupDPFog(Flux_RenderGraph& xGraph);

	// Pass record callback: assembles and binds the per-frame fog CBV,
	// then issues a fullscreen draw against the HDR scene target.
	void ExecuteDPFog(Flux_CommandList* pxCommandList, void* pUserData);

	// Build / rebuild the DP_Fog pipeline. Hooked into the shader hot-reload
	// path in tools builds so editing the .slang file refreshes the pipeline.
	void BuildPipelines();

	bool s_bInitialised = false;

	// Owning shader + pipeline. Built lazily on the first ExecuteDPFog call so
	// the engine's Flux subsystems have completed their own Initialise().
	Flux_Shader   s_xShader;
	Flux_Pipeline s_xPipeline;
	bool          s_bPipelineBuilt = false;

	// CBV layout constants. DP_FOG_HOLE_CAP must match DP_FOG_MAX_HOLES in
	// the .slang module; the static_assert below trips if codegen and this
	// TU disagree. Sized for 17 villagers + 26 lights + headroom; engine's
	// Flux_CommandBindDrawConstants::uMAX_SIZE was bumped to 2048 B to fit.
	constexpr uint32_t DP_FOG_HOLE_CAP = 60;
	constexpr uint32_t DP_FOG_CBV_SIZE = sizeof(Flux_Generated_Fog::DevilsPlayground_DPFog::DPFogConstants_CB);
	static_assert(DP_FOG_CBV_SIZE == 992,
		"DP_Fog CBV size drift — keep aligned with DP_FOG_MAX_HOLES (60) "
		"× 16 B + 16 B colour/density + 16 B count/pad = 992 B. "
		"Update Flux_CommandBindDrawConstants::uMAX_SIZE if growing the cap further.");

	struct DPFogPayload
	{
		Zenith_Maths::Vector4 m_xFogColor_Density   = { 0.55f, 0.58f, 0.65f, 1.0f };
		Zenith_Maths::Vector4 m_axHoles[DP_FOG_HOLE_CAP] = {};
		uint32_t              m_uHoleCount          = 0;
		uint32_t              m_u_pad0              = 0;
		uint32_t              m_u_pad1              = 0;
		uint32_t              m_u_pad2              = 0;
	};
	static_assert(sizeof(DPFogPayload) == DP_FOG_CBV_SIZE,
		"DPFogPayload size drift — keep aligned with reflected DPFogConstants_CB layout");

	DPFogPayload s_xPayload;

	void BuildPipelines()
	{
		s_xShader.Initialise(FluxShaderProgram::DevilsPlayground_DPFog);

		Flux_VertexInputDescription xVertexDesc;
		xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
		xPipelineSpec.m_uNumColourAttachments = 1;
		xPipelineSpec.m_pxShader = &s_xShader;
		xPipelineSpec.m_xVertexInputDesc = xVertexDesc;
		xPipelineSpec.m_bDepthTestEnabled = false;
		xPipelineSpec.m_bDepthWriteEnabled = false;

		// Premultiplied alpha blend: shader output is (color * fogAlpha, fogAlpha)
		// so One/InvSrcAlpha gives `dst * (1 - fogAlpha) + color * fogAlpha`.
		xPipelineSpec.m_axBlendStates[0].m_bBlendEnabled       = true;
		xPipelineSpec.m_axBlendStates[0].m_eSrcBlendFactor     = BLEND_FACTOR_ONE;
		xPipelineSpec.m_axBlendStates[0].m_eDstBlendFactor     = BLEND_FACTOR_ONEMINUSSRCALPHA;
		xPipelineSpec.m_axBlendStates[0].m_eSrcAlphaBlendFactor = BLEND_FACTOR_ONE;
		xPipelineSpec.m_axBlendStates[0].m_eDstAlphaBlendFactor = BLEND_FACTOR_ONEMINUSSRCALPHA;

		s_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);
	}

	// Runtime-tunable fog knobs. Bound at Init time and read each frame in
	// ExecuteDPFog. Defaults match the "fog of war" target — opaque grey
	// outside hole radii. Useful when triaging "fog not visible" reports:
	// crank density to 5+ to confirm the pass is rendering at all, and
	// crank holeRadiusScale below 1.0 to shrink/eliminate the clear bubbles.
	float s_fDebugDensity         = 1.0f;
	float s_fDebugColorR          = 0.55f;
	float s_fDebugColorG          = 0.58f;
	float s_fDebugColorB          = 0.65f;
	float s_fDebugHoleRadiusScale = 1.0f;
}

void DPFogPass::Init()
{
	if (s_bInitialised) return;
	s_bInitialised = true;
	Zenith_GameRenderHook::RegisterPostFogPass(&SetupDPFog);

	// CRITICAL ORDERING NOTE — boot sequence is:
	//   1. Flux::LateInitialise() → SetupRenderGraph() → InvokePostFogRegistrations()
	//   2. Project_RegisterScriptBehaviours() → DPFogPass::Init() → RegisterPostFogPass()
	// i.e. the post-fog callback list is empty when SetupRenderGraph runs the
	// first time. Without an explicit rebuild request, our SetupDPFog hook
	// would NEVER fire in non-tools builds (which is why the fog rendered in
	// tools — Terrain/SSR/etc trigger incidental rebuilds during gameplay
	// init that pick up the late-registered hook — but didn't render at all
	// in non-tools where no rebuild ever happened). Force the rebuild here so
	// the graph picks up our callback before the first frame ticks.
	Flux::RequestGraphRebuild();

#ifdef ZENITH_TOOLS
	Zenith_DebugVariables::AddFloat({ "DevilsPlayground", "Fog", "Density"          }, s_fDebugDensity,         0.0f, 10.0f);
	Zenith_DebugVariables::AddFloat({ "DevilsPlayground", "Fog", "ColorR"           }, s_fDebugColorR,          0.0f,  1.0f);
	Zenith_DebugVariables::AddFloat({ "DevilsPlayground", "Fog", "ColorG"           }, s_fDebugColorG,          0.0f,  1.0f);
	Zenith_DebugVariables::AddFloat({ "DevilsPlayground", "Fog", "ColorB"           }, s_fDebugColorB,          0.0f,  1.0f);
	Zenith_DebugVariables::AddFloat({ "DevilsPlayground", "Fog", "HoleRadiusScale"  }, s_fDebugHoleRadiusScale, 0.0f,  2.0f);
#endif

#ifdef ZENITH_TOOLS
	// Re-build the DP_Fog pipeline whenever the shader hot-reloads.
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::DevilsPlayground_DPFog,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&BuildPipelines,
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif
}

void DPFogPass::Shutdown()
{
	if (!s_bInitialised) return;
	s_bInitialised = false;
	Zenith_GameRenderHook::UnregisterPostFogPass(&SetupDPFog);
	// Re-engage normal fog technique selection so a follow-on project (or
	// Editor Stop without restart) doesn't boot with the engine fog system
	// silently disabled. Guarded against teardown order — see EXT-1.
	if (Flux::IsRenderGraphValid())
	{
		g_xEngine.Fog().SetExternallyOverridden(false);
	}
	s_bPipelineBuilt = false;
}

namespace
{
	void SetupDPFog(Flux_RenderGraph& xGraph)
	{
		// EXT-1: kill the 6 engine fog passes — DP ships its own.
		g_xEngine.Fog().SetExternallyOverridden(true);

		// Register the actual DP_Fog pass. Reads scene depth so the shader can
		// reconstruct world position; writes the HDR scene target. Pass builder
		// is &&-qualified so the chain MUST be consumed inline.
		xGraph.AddPass("DP_Fog", &ExecuteDPFog)
			.Reads (Flux_Graphics::GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV)
			.Writes(Flux_HDR::GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV);
	}

	void ExecuteDPFog(Flux_CommandList* pxCommandList, void* /*pUserData*/)
	{
		// Lazy pipeline init — keeps Init/Shutdown lightweight and tolerates the
		// engine still booting Flux_Fog when DPFogPass::Init runs.
		if (!s_bPipelineBuilt)
		{
			BuildPipelines();
			s_bPipelineBuilt = true;
		}

		// Pack the per-frame CBV. Color/density default; fog hole table from
		// the DP_Fog public side-table. Tail entries stay zero so the shader's
		// `for (uHoleCount)` loop never reads stale data.
		std::memset(s_xPayload.m_axHoles, 0, sizeof(s_xPayload.m_axHoles));
		const uint32_t uWritten = DP_Fog::GatherFogHolePositions(
			s_xPayload.m_axHoles, DP_FOG_HOLE_CAP);
		s_xPayload.m_uHoleCount = uWritten;

		// Debug-knob hole-radius scaling. Set to 0 in the debug panel to
		// kill ALL holes (entire screen becomes opaque fog) — useful to
		// confirm the pass is reaching the screen when fog appears missing.
		if (s_fDebugHoleRadiusScale != 1.0f)
		{
			for (uint32_t i = 0; i < uWritten; ++i)
			{
				s_xPayload.m_axHoles[i].w *= s_fDebugHoleRadiusScale;
			}
		}
		// Fog of war: density 1.0 makes (1 - exp(-density*dist)) saturate
		// to ~1.0 at any plausible view depth (>= 5 m), so outside the
		// hole radii the fog is a HARD WALL of cool grey. Inside a hole,
		// the linear `saturate(dist/radius)` attenuation drives fFogAlpha
		// to 0 at the center and ramps linearly back up to full fog at
		// the radius edge — a soft, radius-wide "clear bubble" effect
		// (e.g. a 3 m villager hole gives ~67 % scene visibility 1 m
		// from the villager and full fog 3 m+ away).
		//
		// Earlier values failed in two opposite ways:
		//   - 0.04 with near-black (0.05, 0.04, 0.06) → looked like
		//     missing geometry, not fog.
		//   - 0.025 with grey (0.55, 0.58, 0.65)     → fog was visible
		//     but only ~85 % opaque outside holes; the player could
		//     still see most of the map → no fog-of-war effect.
		// Now: grey colour stays (visible as fog), density jumps to 1.0
		// (effectively opaque outside holes). Wave-4 polish exposes both
		// knobs via debug variables.
		s_xPayload.m_xFogColor_Density = Zenith_Maths::Vector4(
			s_fDebugColorR, s_fDebugColorG, s_fDebugColorB, s_fDebugDensity);

		pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

		pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
		pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

		Flux_ShaderBinder xBinder(*pxCommandList);
		xBinder.BindCBV(s_xShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
		xBinder.BindSRV(s_xShader, "g_xDepthTex",     Flux_Graphics::GetDepthStencilSRV());
		xBinder.BindDrawConstants(s_xShader, "DPFogConstants", &s_xPayload, sizeof(s_xPayload));

		pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
	}
}

// =====================================================================
// Particle config registration
//
// Called from DevilsPlayground::InitializeResources via the project's
// Project_RegisterScriptBehaviours hook. Registers the PFX_Witch
// emitter config for runtime instantiation by ParticleEmitterComponent
// or scripted spawners. Pattern mirrors Combat::g_pxHitSparkConfig
// (Combat.cpp:648-664).
// =====================================================================
namespace
{
	Flux_ParticleEmitterConfig* g_pxWitchConfig = nullptr;
}

namespace DPFogPass
{
	void RegisterParticleConfigs()
	{
		if (g_pxWitchConfig != nullptr) return; // idempotent

		g_pxWitchConfig = new Flux_ParticleEmitterConfig();
		g_pxWitchConfig->m_fSpawnRate            = 15.0f;
		g_pxWitchConfig->m_uBurstCount           = 0;
		g_pxWitchConfig->m_uMaxParticles         = 30;
		g_pxWitchConfig->m_fSpawnRadius          = 0.1f;
		g_pxWitchConfig->m_fLifetimeMin          = 1.5f;
		g_pxWitchConfig->m_fLifetimeMax          = 2.0f;
		g_pxWitchConfig->m_fSpeedMin             = 0.4f;
		g_pxWitchConfig->m_fSpeedMax             = 0.9f;
		g_pxWitchConfig->m_fSpreadAngleDegrees   = 18.0f;
		g_pxWitchConfig->m_xEmitDirection        = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
		g_pxWitchConfig->m_xGravity              = Zenith_Maths::Vector3(0.0f, 0.6f, 0.0f); // light updraft
		g_pxWitchConfig->m_fDrag                 = 0.5f;
		g_pxWitchConfig->m_xColorStart           = Zenith_Maths::Vector4(0.6f, 0.2f, 0.8f, 0.9f); // witch purple
		g_pxWitchConfig->m_xColorEnd             = Zenith_Maths::Vector4(0.2f, 0.0f, 0.4f, 0.0f);
		g_pxWitchConfig->m_fSizeStart            = 0.20f;
		g_pxWitchConfig->m_fSizeEnd              = 0.04f;
		g_pxWitchConfig->m_bAdditiveBlending     = true;
		g_pxWitchConfig->m_fTurbulence           = 0.8f;
		g_pxWitchConfig->m_bUseGPUCompute        = false;
		Flux_ParticleEmitterConfig::Register("PFX_Witch", g_pxWitchConfig);
	}

	void UnregisterParticleConfigs()
	{
		if (g_pxWitchConfig == nullptr) return;
		Flux_ParticleEmitterConfig::Unregister("PFX_Witch");
		delete g_pxWitchConfig;
		g_pxWitchConfig = nullptr;
	}
}

// =====================================================================
// Scene-authoring hook for the VisualWiring agent.
//
// VisualWiring owns Project_RegisterEditorAutomationSteps so we don't
// touch it from this TU. Instead we expose a stand-alone function the
// VisualWiring agent calls from the GameLevel scene-author block. The
// witch position is taken from the L_GameLevel.json NiagaraActor_2:
//   UE loc (cm)  = (8582.63, 2575.46, 200.00)
//   Engine (m)   ≈ (85.83,    25.75,    2.00)  (UE: X-fwd, Y-right, Z-up
//                                               -> Zenith: X, Y=Z, Z=Y)
// We also register the PFX_Witch fog hole on the same entity so the
// witch silhouette is visible through the fog at gameplay distance.
// =====================================================================
namespace DPFogPass
{
	// Matches DP_LevelData::kNiagara[0] for PFX_Witch (already in
	// Zenith X-Y-Z metres from generate_level_data.py).
	float GetWitchSpawnX() { return 85.8263f; }
	float GetWitchSpawnY() { return 2.0000f;  }
	float GetWitchSpawnZ() { return 25.7546f; }
}
