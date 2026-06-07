#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"

#define FLUX_MAX_QUADS_PER_FRAME 1024

class Flux_CommandList;
class Flux_RenderGraph;

// Dependencies injected into Initialise (Wave-14 DI seam, built on the WS9.2
// Flux_HiZImpl / Wave-11 Flux_SSAOImpl template; Wave-4 extends it to ALSO inject
// the engine-infra singleton VulkanMemory that was previously a direct g_xEngine
// lookup, per the aggressive g_xEngine-shrink directive). Forward-declared here;
// full headers are pulled in by Flux_Quads.cpp.
class Flux_GraphicsImpl;
class Zenith_Vulkan_MemoryManager;

// Phase 9: state + behaviour for Quads subsystem -- shader/pipeline/instance
// buffer + per-frame quad upload ring. The nested Quad struct is the on-wire
// per-quad vertex format used by UI and game-side overlay drawing.
//
// Wave-14 DI seam (mirrors Flux_SSAOImpl): the lone cross-subsystem dependency
// (Flux_GraphicsImpl) is INJECTED through Initialise as an explicit reference and
// stored as a member pointer, rather than reached for via g_xEngine.FluxGraphics()
// inside the instance methods. The only place g_xEngine self-lookup survives is
// the non-capturing fn-pointer trampoline (the ExecuteQuads graph callback, and
// the ZENITH_TOOLS hot-reload callback) — those cannot capture state, so they
// re-enter via g_xEngine.Quads() to reach this singleton instance and then route
// their FluxGraphics reach-ins through the injected member.
class Flux_QuadsImpl
{
public:
	struct Quad
	{
		Quad() = default;
		Quad(Zenith_Maths::UVector4 xPosition_Size, Zenith_Maths::Vector4 xColour, uint32_t uTexture, Zenith_Maths::Vector2 xUVMult_UVAdd,
			float fCornerRadius = 0.0f, Zenith_Maths::Vector2 xSizePixels = {0,0}, Zenith_Maths::Vector4 xColour2 = {-1,-1,-1,-1})
			: m_xPosition_Size(xPosition_Size)
			, m_xColour(xColour)
			, m_uTexture(uTexture)
			, m_xUVMult_UVAdd(xUVMult_UVAdd)
			, m_fCornerRadius(fCornerRadius)
			, m_xSizePixels(xSizePixels)
			, m_xColour2(xColour2)
		{
		}
		Zenith_Maths::UVector4 m_xPosition_Size;
		Zenith_Maths::Vector4  m_xColour;
		uint32_t               m_uTexture;
		Zenith_Maths::Vector2  m_xUVMult_UVAdd;
		float                  m_fCornerRadius = 0.0f;
		Zenith_Maths::Vector2  m_xSizePixels = {0, 0};
		Zenith_Maths::Vector4  m_xColour2 = {-1, -1, -1, -1};
	};

	Flux_QuadsImpl() = default;
	~Flux_QuadsImpl() = default;

	Flux_QuadsImpl(const Flux_QuadsImpl&) = delete;
	Flux_QuadsImpl& operator=(const Flux_QuadsImpl&) = delete;

	// Cross-subsystem dep is injected here and stored into m_pxGraphics below.
	// This is the WS9.2 DI template: explicit ref param -> stored member pointer.
	void Initialise(Flux_GraphicsImpl& xGraphics, Zenith_Vulkan_MemoryManager& xVulkanMemory);
	void BuildPipelines();
	void Shutdown();

	void Render(void*);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	void UploadQuad(const Quad& xQuad);
	void UploadInstanceData();

	Flux_Shader              m_xShader;
	Flux_Pipeline            m_xPipeline;
	Flux_DynamicVertexBuffer m_xInstanceBuffer;

	Quad                     m_axQuadsToRender[FLUX_MAX_QUADS_PER_FRAME];
	uint32_t                 m_uQuadRenderIndex = 0;

	// Injected cross-subsystem dependency (stored by Initialise). Default nullptr
	// so a default-constructed instance is headless-safe; the real boot path wires
	// it in via the Quads init trampoline (Flux_FeatureRegistry.cpp).
	Flux_GraphicsImpl*       m_pxGraphics = nullptr;
	Zenith_Vulkan_MemoryManager* m_pxVulkanMemory = nullptr;
};
