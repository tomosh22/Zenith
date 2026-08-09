#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_VertexCodec.h"   // the ONE place the packed lanes below are quantised
#include "Maths/Zenith_Maths.h"
#include "Flux/Shaders/Generated/Quads.h"   // the baked vertex layout the pins below compare against

#include <cstddef>   // offsetof
#include <cstdint>   // uint16_t — the packed rect lanes

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
	// The on-wire per-quad instance vertex (52 B). Two of its seven lanes are packed;
	// the rest deliberately are not:
	//   * m_auPosition_Size is uint16x4. The pixel rect ARRIVES as integers — every
	//     producer casts screen floats to u32 before constructing a Quad — so u16 is
	//     EXACT for any coordinate a real display carries. Flux_PackUint16x4 saturates
	//     rather than wraps, so an over-range u32 lane can never fold back into view
	//     at (value mod 65536). (What a NEGATIVE screen float becomes upstream is UB
	//     and target-specific — ~4.29e9 on x64, 0 on AArch64's saturating FCVTZU —
	//     pre-existing producer behaviour either way; the packer only guarantees it
	//     never makes such a rect MORE visible than the u32 lane did.)
	//   * m_uColour is unorm8x4 — the authoring precision of every UI colour in the
	//     engine (pickers emit 8-bit; gradients interpolate in float in the FS after
	//     the fetch widens it). The output chain is NOT the justification: the final
	//     RT is RGBA16_UNORM into an sRGB swapchain, so this is a real narrowing
	//     below ~0.004 linear — invisible for authored UI colours.
	//   * m_xColour2 STAYS a float4 and is NOT a spare 12 bytes. Its (-1,-1,-1,-1)
	//     default is the "no vertical gradient" SENTINEL the fragment shader tests
	//     with `colour2.x >= 0`, and no unorm encoding can hold a negative: packing it
	//     would make every plain quad a black-bottomed gradient. Moving the flag out
	//     of the colour lane is a wire-format redesign, not a format flip.
	//   * m_xUVMult_UVAdd is a multiply/add pair, not a [0,1] rect — a caller tiling a
	//     texture passes a multiplier above 1 — so it has no bounded unorm range.
	//   * m_fCornerRadius / m_xSizePixels are pixel-space floats, and the rounded-corner
	//     SDF specifically wants the sub-pixel size the integer rect above discarded.
	struct Quad
	{
		Quad() = default;
		Quad(Zenith_Maths::UVector4 xPosition_Size, Zenith_Maths::Vector4 xColour, uint32_t uTexture, Zenith_Maths::Vector2 xUVMult_UVAdd,
			float fCornerRadius = 0.0f, Zenith_Maths::Vector2 xSizePixels = {0,0}, Zenith_Maths::Vector4 xColour2 = {-1,-1,-1,-1})
			: m_uColour(Flux_PackUnorm8x4(xColour))
			, m_uTexture(uTexture)
			, m_xUVMult_UVAdd(xUVMult_UVAdd)
			, m_fCornerRadius(fCornerRadius)
			, m_xSizePixels(xSizePixels)
			, m_xColour2(xColour2)
		{
			Flux_PackUint16x4(xPosition_Size, m_auPosition_Size);
		}

		void SetPositionSize(const Zenith_Maths::UVector4& xPosition_Size) { Flux_PackUint16x4(xPosition_Size, m_auPosition_Size); }
		void SetColour(const Zenith_Maths::Vector4& xColour)               { m_uColour = Flux_PackUnorm8x4(xColour); }

		Zenith_Maths::UVector4 GetPositionSize() const { return Flux_UnpackUint16x4(m_auPosition_Size); }
		Zenith_Maths::Vector4  GetColour()       const { return Flux_UnpackUnorm8x4(m_uColour); }

		uint16_t               m_auPosition_Size[4] = {0, 0, 0, 0};   // uint16x4
		uint32_t               m_uColour = 0u;                        // unorm8x4
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
	static_assert(offsetof(Quad, m_auPosition_Size) == Flux_Generated_Quads::Quads::kaxVertexAttribs[uQUAD_INSTANCE_FIRST_ELEMENT + 0].m_uOffset,
		"Quad.m_auPosition_Size drifted from the generated per-instance layout");
	static_assert(offsetof(Quad, m_uColour)        == Flux_Generated_Quads::Quads::kaxVertexAttribs[uQUAD_INSTANCE_FIRST_ELEMENT + 1].m_uOffset,
		"Quad.m_uColour drifted from the generated per-instance layout");
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
	// equal-width fields (this table has uint16x4/float4 both 8-and-16 wide, uint/float
	// at 12/24 and two float2 at 16/28 — all silently swappable), so the whole expected
	// table is spelled out and compared element-wise, semantics and types included.
	// Pinning the STORAGE TYPES is also what catches a dropped [VtxFmt] tag, which would
	// silently re-widen the stream to 72 B and produce no other symptom.
	static constexpr Flux_VertexLayoutElement kaxQUAD_EXPECTED_LAYOUT[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3,   0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2,   0u, 12u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 1u, SHADER_DATA_TYPE_UINT16X4, 1u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 2u, SHADER_DATA_TYPE_UNORM8X4, 1u,  8u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 3u, SHADER_DATA_TYPE_UINT,     1u, 12u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 4u, SHADER_DATA_TYPE_FLOAT2,   1u, 16u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 5u, SHADER_DATA_TYPE_FLOAT,    1u, 24u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 6u, SHADER_DATA_TYPE_FLOAT2,   1u, 28u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 7u, SHADER_DATA_TYPE_FLOAT4,   1u, 36u },
	};
	static_assert(Flux_Generated_Quads::Quads::kVertexLayout == Flux_VertexLayoutDesc{ kaxQUAD_EXPECTED_LAYOUT, 9u, { 20u, 52u } },
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
