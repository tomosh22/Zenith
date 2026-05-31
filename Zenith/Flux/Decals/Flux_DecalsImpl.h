#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Maths/Zenith_Maths.h"

class Zenith_TextureAsset;
class Flux_RenderGraph;

// Cross-subsystem dependencies injected into Initialise (Wave-17 DI seam, the
// SSAO leaf-seam shape: explicit ref params -> stored member pointers).
// Forward-declared here; full headers are pulled in by Flux_Decals.cpp.
class Flux_GraphicsImpl;
class Zenith_Vulkan_Swapchain;

// Phase 9: state + behaviour for Decals subsystem.
//
// Wave-17 DI seam (mirrors Flux_SSAOImpl): cross-subsystem dependencies are
// INJECTED through Initialise as explicit references and stored as member
// pointers, rather than reached for via g_xEngine.X() inside SetupRenderGraph.
// The only place g_xEngine self-lookup survives is the non-capturing fn-pointer
// trampolines (the Execute* graph callbacks and the ZENITH_DEBUG_VARIABLES-gated
// Prepare callback) — those cannot capture state, so they re-enter via
// g_xEngine.Decals() to reach this singleton instance and then route their
// FluxGraphics reach-ins through the injected member.
class Flux_DecalsImpl
{
public:
	Flux_DecalsImpl() = default;
	~Flux_DecalsImpl() = default;

	Flux_DecalsImpl(const Flux_DecalsImpl&) = delete;
	Flux_DecalsImpl& operator=(const Flux_DecalsImpl&) = delete;

	// Cross-subsystem deps are injected here and stored into the member pointers
	// below. This is the SSAO DI template: explicit ref params -> stored member
	// pointers.
	void Initialise(Flux_GraphicsImpl& xGraphics, Zenith_Vulkan_Swapchain& xSwapchain);
	void Shutdown();
	void BuildPipelines();
	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	void SpawnDecal(const Zenith_Maths::Vector3& xPosition,
	                const Zenith_Maths::Vector3& xNormal,
	                Zenith_TextureAsset*         pxTexture,
	                float                        fSize,
	                float                        fLifetime);

	bool IsInitialised() const { return m_bInitialised; }

	static constexpr u_int uMAX_DECALS = 64;

#ifdef ZENITH_TESTING
	struct TestSlotView
	{
		Zenith_Maths::Vector3 m_xPosition;
		Zenith_Maths::Vector3 m_xNormal;
		float                 m_fRemainingLifetime;
		bool                  m_bActive;
	};
	u_int        GetActiveCountForTest();
	TestSlotView GetSlotForTest(u_int uSlotIndex);
	void         ResetForTest();
#endif

	bool                        m_bInitialised      = false;
	u_int                       m_uNextSlot         = 0;
	u_int                       m_uActiveDecalCount = 0;

	Flux_RenderGraph*           m_pxGraph = nullptr;
	Flux_TransientHandle        m_xNormalsCopyHandle;
	Flux_PassHandle             m_xNormalsCopyPass;
	Flux_PassHandle             m_xApplyPass;

	Flux_Shader                 m_xNormalsCopyShader;
	Flux_Shader                 m_xApplyShader;
	Flux_Pipeline               m_xNormalsCopyPipeline;
	Flux_Pipeline               m_xApplyPipeline;

	Flux_DynamicReadWriteBuffer m_xDecalBuffer;
	Flux_IndexBuffer            m_xDecalIndexBuffer;

	// Injected cross-subsystem dependencies (stored by Initialise). Default
	// nullptr so a default-constructed instance is headless-safe; the real boot
	// path wires them in Flux_FeatureRegistry's Decals init trampoline.
	Flux_GraphicsImpl*       m_pxGraphics  = nullptr;
	Zenith_Vulkan_Swapchain* m_pxSwapchain = nullptr;
};
