#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/Shaders/Generated/Quads.h"   // the baked vertex layout the pins below compare against

#include <cstddef>   // offsetof

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

	// Quad WAS the one instanced vertex struct in Flux with no compile-time pin at all
	// (Flux_TextVertex and Flux_ParticleInstance both had a size assert; this had
	// nothing). It is binding 1 of the Quads program's baked layout, so the field-by-
	// field comparison below is what the hand-written per-instance description used to
	// be — except the build now fails instead of the UI rendering from the wrong offsets.
	// Elements 0/1 of that layout are the shared unit quad on binding 0; the per-instance
	// stream starts at index 2.
	static constexpr u_int uQUAD_INSTANCE_FIRST_ELEMENT = 2u;
	static_assert(sizeof(Quad) == Flux_Generated_Quads::Quads::kVertexLayout.m_auStrides[1],
		"Flux_QuadsImpl::Quad must match the per-instance stride the Quads VS fetches");
	static_assert(offsetof(Quad, m_xPosition_Size) == Flux_Generated_Quads::Quads::kaxVertexAttribs[uQUAD_INSTANCE_FIRST_ELEMENT + 0].m_uOffset,
		"Quad.m_xPosition_Size drifted from the generated per-instance layout");
	static_assert(offsetof(Quad, m_xColour)        == Flux_Generated_Quads::Quads::kaxVertexAttribs[uQUAD_INSTANCE_FIRST_ELEMENT + 1].m_uOffset,
		"Quad.m_xColour drifted from the generated per-instance layout");
	static_assert(offsetof(Quad, m_uTexture)       == Flux_Generated_Quads::Quads::kaxVertexAttribs[uQUAD_INSTANCE_FIRST_ELEMENT + 2].m_uOffset,
		"Quad.m_uTexture drifted from the generated per-instance layout");
	static_assert(offsetof(Quad, m_xUVMult_UVAdd)  == Flux_Generated_Quads::Quads::kaxVertexAttribs[uQUAD_INSTANCE_FIRST_ELEMENT + 3].m_uOffset,
		"Quad.m_xUVMult_UVAdd drifted from the generated per-instance layout");
	static_assert(offsetof(Quad, m_fCornerRadius)  == Flux_Generated_Quads::Quads::kaxVertexAttribs[uQUAD_INSTANCE_FIRST_ELEMENT + 4].m_uOffset,
		"Quad.m_fCornerRadius drifted from the generated per-instance layout");
	static_assert(offsetof(Quad, m_xSizePixels)    == Flux_Generated_Quads::Quads::kaxVertexAttribs[uQUAD_INSTANCE_FIRST_ELEMENT + 5].m_uOffset,
		"Quad.m_xSizePixels drifted from the generated per-instance layout");
	static_assert(offsetof(Quad, m_xColour2)       == Flux_Generated_Quads::Quads::kaxVertexAttribs[uQUAD_INSTANCE_FIRST_ELEMENT + 6].m_uOffset,
		"Quad.m_xColour2 drifted from the generated per-instance layout");

	// Full-table pin: offsets alone are invariant under a shader-side reorder of
	// equal-width fields (this table has uint4/float4 at 0/16, uint/float at 32/44
	// and two float2 at 36/48 — all silently swappable), so the whole expected
	// table is spelled out and compared element-wise, semantics and types included.
	static constexpr Flux_VertexLayoutElement kaxQUAD_EXPECTED_LAYOUT[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3, 0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2, 0u, 12u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 1u, SHADER_DATA_TYPE_UINT4,  1u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 2u, SHADER_DATA_TYPE_FLOAT4, 1u, 16u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 3u, SHADER_DATA_TYPE_UINT,   1u, 32u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 4u, SHADER_DATA_TYPE_FLOAT2, 1u, 36u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 5u, SHADER_DATA_TYPE_FLOAT,  1u, 44u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 6u, SHADER_DATA_TYPE_FLOAT2, 1u, 48u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 7u, SHADER_DATA_TYPE_FLOAT4, 1u, 56u },
	};
	static_assert(Flux_Generated_Quads::Quads::kVertexLayout == Flux_VertexLayoutDesc{ kaxQUAD_EXPECTED_LAYOUT, 9u, { 20u, 72u } },
		"The Quads program's vertex layout drifted from the pinned contract — re-derive kaxQUAD_EXPECTED_LAYOUT consciously if the VsIn really changed");

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
