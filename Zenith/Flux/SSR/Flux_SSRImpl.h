#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_ScreenSpaceEffectBase.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

class Flux_RenderGraph;

// Cross-subsystem dependencies injected into Initialise (aggressive DI pass:
// the engine-infra singletons VulkanMemory/VulkanSwapchain and the sibling
// Flux subsystems FluxGraphics/HiZ/VolumeFog/FluxRenderer are now stored member
// pointers instead of reached for via g_xEngine.X() inside instance methods).
// Forward-declared here; full headers are pulled in by Flux_SSR.cpp.
class Flux_GraphicsImpl;
class Flux_HiZImpl;
class Flux_VolumeFogImpl;
class Flux_RendererImpl;

enum SSR_DebugMode : u_int
{
	SSR_DEBUG_NONE = 0,
	SSR_DEBUG_RAY_DIRECTIONS,
	SSR_DEBUG_SCREEN_DIRECTIONS,
	SSR_DEBUG_HIT_POSITIONS,
	SSR_DEBUG_REFLECTION_UVS,
	SSR_DEBUG_CONFIDENCE,
	SSR_DEBUG_DEPTH_COMPARISON,
	SSR_DEBUG_EDGE_FADE,
	SSR_DEBUG_MARCH_DISTANCE,
	SSR_DEBUG_FINAL_RESULT,
	SSR_DEBUG_ROUGHNESS,
	SSR_DEBUG_WORLD_NORMAL_Y,
	SSR_DEBUG_RAY_COUNT,
	SSR_DEBUG_COUNT
};

// Phase 9: state + behaviour for SSR subsystem.
class Flux_SSRImpl : public Flux_ScreenSpaceEffectBase<Flux_SSRImpl>
{
public:
	Flux_SSRImpl() = default;
	~Flux_SSRImpl() = default;

	Flux_SSRImpl(const Flux_SSRImpl&) = delete;
	Flux_SSRImpl& operator=(const Flux_SSRImpl&) = delete;

	// Cross-subsystem deps injected here and stored into the member pointers
	// below (aggressive DI seam). Reached through the stored member pointers from
	// the instance methods; the non-capturing Execute* / hot-reload trampolines
	// recover this instance via g_xEngine.SSR() first, then route through these.
	void Initialise(Flux_MemoryManager& xVulkanMemory, Flux_Swapchain& xSwapchain,
	                Flux_GraphicsImpl& xGraphics, Flux_HiZImpl& xHiZ,
	                Flux_VolumeFogImpl& xVolumeFog, Flux_RendererImpl& xRenderer);
	void BuildPipelines();

	// CRTP hook called by Flux_ScreenSpaceEffectBase::Shutdown() — destroys the
	// SSR constants CBV. The base resets m_pxGraph / m_bInitialised afterwards.
	void ShutdownImpl();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	void ApplyBlurSelectionToGraph(Flux_RenderGraph& xGraph);

	// Promoted from a file-static helper so its HiZ + VulkanSwapchain reach-ins
	// route through the injected members. Public because the (former
	// free-function) call site sits inside the non-capturing ExecuteSSRRayMarch
	// trampoline, which recovers this instance via g_xEngine.SSR() first.
	void UpdateSSRConstants();

	Flux_TransientHandle GetReflectionHandle() const { return m_xReflectionSelector.GetCommittedHandle(); }
	Flux_ShaderResourceView& GetReflectionSRV();
	bool IsEnabled() const;

	Flux_RenderAttachment& GetRayMarchAttachment();
	Flux_RenderAttachment& GetRayMarchAuxAttachment();
	Flux_RenderAttachment& GetUpsampledAttachment();
	Flux_RenderAttachment& GetUpsampledAuxAttachment();
	Flux_RenderAttachment& GetDenoiseHAttachment();
	Flux_RenderAttachment& GetDenoiseHConfAttachment();
	Flux_RenderAttachment& GetDenoiseVAttachment();

	// SSR-specific dual-MRT handles (RT0 colour/confidence + RT1 aux/metadata).
	// These do NOT exist on SSGI — kept here to respect the divergence.
	Flux_TransientHandle m_xRayMarchHandle;
	Flux_TransientHandle m_xRayMarchAuxHandle;
	Flux_TransientHandle m_xUpsampledHandle;
	Flux_TransientHandle m_xUpsampledAuxHandle;
	Flux_TransientHandle m_xDenoiseHHandle;
	Flux_TransientHandle m_xDenoiseHConfHandle;
	Flux_TransientHandle m_xDenoiseVHandle;

	Flux_DynamicConstantBuffer m_xSSRConstantsBuffer;

	Flux_PassHandle      m_xDenoiseHPass;
	Flux_PassHandle      m_xDenoiseVPass;

	// Tracks which transient the deferred pass reads (DenoiseV when blur is on,
	// Upsampled otherwise) and triggers a graph rebuild when the live toggle
	// diverges from the committed selection.
	Flux_CommittedHandleSelector<bool> m_xReflectionSelector;

	// Injected cross-subsystem dependencies (stored by Initialise). Default
	// nullptr so a default-constructed instance is headless-safe; the real boot
	// path wires them in Flux_FeatureRegistry's SSR init trampoline.
	Flux_MemoryManager* m_pxVulkanMemory  = nullptr;
	Flux_Swapchain*     m_pxSwapchain     = nullptr;
	Flux_GraphicsImpl*           m_pxGraphics      = nullptr;
	Flux_HiZImpl*                m_pxHiZ           = nullptr;
	Flux_VolumeFogImpl*          m_pxVolumeFog     = nullptr;
	Flux_RendererImpl*           m_pxRenderer      = nullptr;
};
