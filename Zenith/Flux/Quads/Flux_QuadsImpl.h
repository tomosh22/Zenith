#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"

#define FLUX_MAX_QUADS_PER_FRAME 1024

class Flux_RenderGraph;

// Phase 9: state + behaviour for Quads subsystem -- shader/pipeline/instance
// buffer + per-frame quad upload ring. The nested Quad struct is the on-wire
// per-quad vertex format used by UI and game-side overlay drawing.
//
// Cross-subsystem deps (FluxGraphics / VulkanMemory) are reached via g_xEngine
// at point of use. The non-capturing fn-pointer trampolines (the ExecuteQuads
// graph callback, and the ZENITH_TOOLS hot-reload callback) cannot capture
// state, so they re-enter via g_xEngine.Quads() to reach this singleton
// instance.
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

	void Initialise();
	void BuildPipelines();
	void Shutdown();

	void Render(void*);

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Sort order is global across UI canvases. Raw/non-UI callers keep the
	// historical order-0 behaviour by omitting the second argument.
	void UploadQuad(const Quad& xQuad, int iSortOrder = 0);
	void SortQueuedQuadsForUpload();
	void UploadInstanceData();

	Flux_Shader              m_xShader;
	Flux_Pipeline            m_xPipeline;
	Flux_DynamicVertexBuffer m_xInstanceBuffer;

	Quad                     m_axQuadsToRender[FLUX_MAX_QUADS_PER_FRAME];
	int                      m_aiQuadSortOrders[FLUX_MAX_QUADS_PER_FRAME] = {};
	uint32_t                 m_uQuadRenderIndex = 0;
	bool                     m_bCapacityWarningIssued = false;
};
