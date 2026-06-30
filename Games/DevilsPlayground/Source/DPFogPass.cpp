#include "Zenith.h"
#include "Flux/Slang/Flux_UnownedEngineShaders.h"
#include "Core/Zenith_Engine.h"

#include "Source/DPFogPass.h"
#include "Source/PublicInterfaces.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Zenith_GameRenderFeatures.h"
#include "Flux/Fog/Flux_FogImpl.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Flux/Slang/Flux_ShaderBinder.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

#include "Flux/Shaders/Generated/Fog.h"

#include "DebugVariables/Zenith_DebugVariables.h"

#include <cstring>

namespace
{
	// Game render-feature trampolines (registered with Zenith_GameRenderFeatures).
	// InitialiseDPFog: one-time init (debug vars + shader hot-reload reg).
	// SetupDPFog: declares the DP_Fog pass + force-disables engine fog each rebuild.
	// ShutdownDPFog: lifts the force-disable + tears down the pipeline.
	void InitialiseDPFog();
	void SetupDPFog(Flux_RenderGraph& xGraph);
	void ShutdownDPFog();

	// Pass record callback: assembles and binds the per-frame fog CBV,
	// then issues a fullscreen draw against the HDR scene target.
	void ExecuteDPFog(Flux_CommandBuffer* pxCommandList, void* pUserData);

	// Build / rebuild the DP_Fog pipeline. Hooked into the shader hot-reload
	// path in tools builds so editing the .slang file refreshes the pipeline.
	void BuildPipelines();

	bool s_bInitialised = false;

	// Owning shader + pipeline. HEAP-allocated (raw pointers, not file-scope value
	// objects) so Shutdown() can delete them DURING the engine shutdown sequence,
	// while the Vulkan device is still alive. As static value objects, their
	// device-touching dtors ran at C++ static-exit -- after
	// Zenith_Engine::Shutdown() freed g_xEngine.FluxBackend() -- which is the
	// 0xC0000005 (hit even by --list-automated-tests: the pipeline is never
	// built, but the static dtor still ran Reset(), which dereferences
	// g_xEngine.FluxBackend() before its null-handle guard). Built lazily on the
	// first ExecuteDPFog call once the engine's Flux subsystems have inited.
	Flux_Shader*   s_pxShader = nullptr;
	Flux_Pipeline* s_pxPipeline = nullptr;
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
		if (!s_pxShader)   s_pxShader   = new Flux_Shader();
		if (!s_pxPipeline) s_pxPipeline = new Flux_Pipeline();

		s_pxShader->Initialise(Flux_UnownedEngineShaders::xDevilsPlayground_DPFog);

		Flux_VertexInputDescription xVertexDesc;
		xVertexDesc.m_eTopology = MESH_TOPOLOGY_NONE;

		Flux_PipelineSpecification xPipelineSpec;
		xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
		xPipelineSpec.m_uNumColourAttachments = 1;
		xPipelineSpec.m_pxShader = s_pxShader;
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

		s_pxShader->GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(*s_pxPipeline, xPipelineSpec);
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

	// Register DP_Fog as a generic game render feature. It anchors runAfter="Fog"
	// so its pass is declared right after the engine fog step — the exact slot the
	// old hardcoded @GameHook:PostFog occupied — keeping DP_Fog's HDR write-chain
	// position unchanged. The registry owns the lifecycle: because Flux is already
	// up when this runs (Project_RegisterGameComponents fires after
	// FluxRenderer().LateInitialise()), Register() calls InitialiseDPFog()
	// immediately and requests a graph rebuild so SetupDPFog runs before the first
	// frame — no manual RequestGraphRebuild() needed here anymore.
	Zenith_GameRenderFeatureDesc xDesc;
	xDesc.m_szName             = "DP_Fog";
	xDesc.m_pfnInitialise      = &InitialiseDPFog;
	xDesc.m_pfnSetupRenderGraph = &SetupDPFog;
	xDesc.m_pfnShutdown        = &ShutdownDPFog;
	xDesc.m_szRunAfter         = "Fog";
	Zenith_GameRenderFeatures::Register(xDesc);
}

void DPFogPass::Shutdown()
{
	if (!s_bInitialised) return;
	s_bInitialised = false;
	// The registry calls ShutdownDPFog() (lift override + tear down pipeline) if
	// the feature was initialised. Order-preserving unregister.
	Zenith_GameRenderFeatures::Unregister("DP_Fog");
}

namespace
{
	void InitialiseDPFog()
	{
#ifdef ZENITH_TOOLS
		g_xEngine.DebugVariables().AddFloat({ "DevilsPlayground", "Fog", "Density"          }, s_fDebugDensity,         0.0f, 10.0f);
		g_xEngine.DebugVariables().AddFloat({ "DevilsPlayground", "Fog", "ColorR"           }, s_fDebugColorR,          0.0f,  1.0f);
		g_xEngine.DebugVariables().AddFloat({ "DevilsPlayground", "Fog", "ColorG"           }, s_fDebugColorG,          0.0f,  1.0f);
		g_xEngine.DebugVariables().AddFloat({ "DevilsPlayground", "Fog", "ColorB"           }, s_fDebugColorB,          0.0f,  1.0f);
		g_xEngine.DebugVariables().AddFloat({ "DevilsPlayground", "Fog", "HoleRadiusScale"  }, s_fDebugHoleRadiusScale, 0.0f,  2.0f);

		// Re-build the DP_Fog pipeline whenever the shader hot-reloads.
		static const Flux_ShaderDecl* s_axPrograms[] = {
			&Flux_UnownedEngineShaders::xDevilsPlayground_DPFog,
		};
		Flux_ShaderHotReload::RegisterSubsystem(&BuildPipelines,
			s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif
	}

	void SetupDPFog(Flux_RenderGraph& xGraph)
	{
		// Kill the 6 engine fog passes generically — DP ships its own. The overlay
		// force-disables every pass owned by the "Fog" setup step (owner == engine
		// fog feature name) WITHOUT touching their base enable bits, and persists
		// across graph rebuilds; ShutdownDPFog lifts it. Engine fog returns intact
		// (at the active technique) the moment the override is lifted.
		xGraph.SetOwnerForceDisabled("Fog", true);

		// Declare the actual DP_Fog pass. Reads scene depth so the shader can
		// reconstruct world position; writes the HDR scene target. Pass builder
		// is &&-qualified so the chain MUST be consumed inline. RunSetup tags this
		// pass with owner "DP_Fog" (the feature name) around this call.
		xGraph.AddPass("DP_Fog", &ExecuteDPFog)
			.Reads (g_xEngine.FluxGraphics().GetDepthAttachment(), RESOURCE_ACCESS_READ_SRV)
			.Writes(g_xEngine.FluxGraphics().GetHDRSceneTarget(),       RESOURCE_ACCESS_WRITE_RTV);
	}

	void ShutdownDPFog()
	{
		// Lift the generic fog override so a follow-on project (or Editor Stop
		// without restart) doesn't boot with engine fog silently masked. Guarded
		// against teardown order — when Flux is already gone the graph is invalid
		// and there's nothing to restore.
		if (g_xEngine.FluxRenderer().IsRenderGraphValid())
		{
			g_xEngine.FluxRenderer().GetRenderGraph().SetOwnerForceDisabled("Fog", false);
		}
		// delete (not Reset) so the device-touching destructors run HERE, in the
		// shutdown sequence with the Vulkan device still alive -- not at C++
		// static-exit, by which time g_xEngine.FluxBackend() has been freed (the
		// 0xC0000005). delete on nullptr (pipeline never built, e.g.
		// --list-automated-tests) is a safe no-op.
		delete s_pxPipeline;
		s_pxPipeline = nullptr;
		delete s_pxShader;
		s_pxShader = nullptr;
		s_bPipelineBuilt = false;
	}

	void ExecuteDPFog(Flux_CommandBuffer* pxCommandList, void* /*pUserData*/)
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

		pxCommandList->SetPipeline(s_pxPipeline);

		pxCommandList->SetVertexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
		pxCommandList->SetIndexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

		Flux_ShaderBinder xBinder(*pxCommandList);
		namespace NS = Flux_Generated_Fog::DevilsPlayground_DPFog;
		xBinder.BindSRV(NS::hg_xDepthTex, g_xEngine.FluxGraphics().GetDepthStencilSRV());
		xBinder.BindDrawConstants(NS::hDPFogConstants, &s_xPayload, sizeof(s_xPayload));

		pxCommandList->DrawIndexed(6);
	}
}
