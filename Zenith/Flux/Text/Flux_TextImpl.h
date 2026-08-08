#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"
#include "Flux/Shaders/Generated/Text.h"   // the baked vertex layout the pins below compare against

#include <cstddef>   // offsetof

class Flux_RenderGraph;
class Zenith_FontAsset;

// Per-glyph instance vertex pushed to the GPU. One per visible glyph quad.
// Layout matches the vertex input description in Flux_TextImpl::BuildPipelines.
//
// Position math, evaluated in the vertex shader:
//   screen_px = m_xTextRoot + m_xQuadOffsetPx + quad_uv * m_xQuadSizePx
//   atlas_uv  = m_xAtlasUVOrigin + quad_uv * m_xAtlasUVSize
//
// m_xTextRoot is float2 (subpixel-tolerant) — historically uint2, which floored
// text positions to whole pixels and was the source of jitter at small sizes.
struct Flux_TextVertex
{
	Zenith_Maths::Vector2 m_xQuadOffsetPx;
	Zenith_Maths::Vector2 m_xQuadSizePx;
	Zenith_Maths::Vector2 m_xAtlasUVOrigin;
	Zenith_Maths::Vector2 m_xAtlasUVSize;
	Zenith_Maths::Vector2 m_xTextRoot;
	Zenith_Maths::Vector4 m_xColour;
};
// The 56 is not a magic number any more: binding 1 of the Text program's baked layout
// IS this struct, so the pins below compare field-by-field against what the VS fetches.
// (The literal stays as the human-readable anchor; the generated comparison is what
// actually catches a reordered field, which a size check alone never could.)
static_assert(sizeof(Flux_TextVertex) == 56, "Flux_TextVertex layout drift will silently break the vertex input description");
namespace Flux_TextVertexPins
{
	using Layout = decltype(Flux_Generated_Text::Text::kVertexLayout);
	inline constexpr const Layout& kxLayout = Flux_Generated_Text::Text::kVertexLayout;
	// Elements 0/1 are the shared unit quad on binding 0; the per-instance stream starts at 2.
	inline constexpr u_int uINSTANCE_FIRST = 2u;
}
static_assert(sizeof(Flux_TextVertex) == Flux_TextVertexPins::kxLayout.m_auStrides[1],
	"Flux_TextVertex must match the per-instance stride the Text VS fetches");
static_assert(offsetof(Flux_TextVertex, m_xQuadOffsetPx)  == Flux_Generated_Text::Text::kaxVertexAttribs[Flux_TextVertexPins::uINSTANCE_FIRST + 0].m_uOffset,
	"Flux_TextVertex.m_xQuadOffsetPx drifted from the generated per-instance layout");
static_assert(offsetof(Flux_TextVertex, m_xQuadSizePx)    == Flux_Generated_Text::Text::kaxVertexAttribs[Flux_TextVertexPins::uINSTANCE_FIRST + 1].m_uOffset,
	"Flux_TextVertex.m_xQuadSizePx drifted from the generated per-instance layout");
static_assert(offsetof(Flux_TextVertex, m_xAtlasUVOrigin) == Flux_Generated_Text::Text::kaxVertexAttribs[Flux_TextVertexPins::uINSTANCE_FIRST + 2].m_uOffset,
	"Flux_TextVertex.m_xAtlasUVOrigin drifted from the generated per-instance layout");
static_assert(offsetof(Flux_TextVertex, m_xAtlasUVSize)   == Flux_Generated_Text::Text::kaxVertexAttribs[Flux_TextVertexPins::uINSTANCE_FIRST + 3].m_uOffset,
	"Flux_TextVertex.m_xAtlasUVSize drifted from the generated per-instance layout");
static_assert(offsetof(Flux_TextVertex, m_xTextRoot)      == Flux_Generated_Text::Text::kaxVertexAttribs[Flux_TextVertexPins::uINSTANCE_FIRST + 4].m_uOffset,
	"Flux_TextVertex.m_xTextRoot drifted from the generated per-instance layout");
static_assert(offsetof(Flux_TextVertex, m_xColour)        == Flux_Generated_Text::Text::kaxVertexAttribs[Flux_TextVertexPins::uINSTANCE_FIRST + 5].m_uOffset,
	"Flux_TextVertex.m_xColour drifted from the generated per-instance layout");

// Full-table pin: offsets alone are invariant under a shader-side reorder of
// equal-width fields (five float2s here), so the WHOLE expected table — semantic,
// index, storage type, binding and offset per element — is spelled out and
// compared element-wise. A VsIn permutation that preserves the offset sequence
// now fails this line instead of silently swapping meaning on the GPU.
namespace Flux_TextVertexPins
{
	inline constexpr Flux_VertexLayoutElement kaxEXPECTED[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3, 0u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2, 0u, 12u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 1u, SHADER_DATA_TYPE_FLOAT2, 1u,  0u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 2u, SHADER_DATA_TYPE_FLOAT2, 1u,  8u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 3u, SHADER_DATA_TYPE_FLOAT2, 1u, 16u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 4u, SHADER_DATA_TYPE_FLOAT2, 1u, 24u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 5u, SHADER_DATA_TYPE_FLOAT2, 1u, 32u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 6u, SHADER_DATA_TYPE_FLOAT4, 1u, 40u },
	};
}
static_assert(Flux_Generated_Text::Text::kVertexLayout == Flux_VertexLayoutDesc{ Flux_TextVertexPins::kaxEXPECTED, 8u, { 20u, 56u } },
	"The Text program's vertex layout drifted from the pinned contract — re-derive kaxEXPECTED consciously if the VsIn really changed");

// Push-constant block bound to the text fragment shader. Layout MUST match
// TextConstantsLayout in Flux_Text.slang. 32 bytes total (was 16 in the legacy
// bitmap pipeline) — a size mismatch with shader reflection causes silent
// rendering failures in some drivers.
struct Flux_TextDrawConstants
{
	Zenith_Maths::Vector4 m_xClipRect;        // (-1,-1,-1,-1) = clip off
	Zenith_Maths::Vector2 m_xAtlasSizePx;     // e.g. (256, 256)
	float                 m_fAtlasPxRange;    // msdfgen "range" in atlas pixels
	u_int                 m_uAtlasIdx;        // bindless slot of the MSDF atlas in g_axTextures[]
};
static_assert(sizeof(Flux_TextDrawConstants) == 32, "Flux_TextDrawConstants must be 32 bytes to match shader push-constant range");

class Flux_TextImpl
{
public:
	Flux_TextImpl() = default;
	~Flux_TextImpl() = default;

	Flux_TextImpl(const Flux_TextImpl&) = delete;
	Flux_TextImpl& operator=(const Flux_TextImpl&) = delete;

	void Initialise();
	void BuildPipelines();
	void ReleaseAssetReferences();
	void Shutdown();
	void Reset();
	void Render(void*);
	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	// The graph registers this exact callback; keeping it addressable also lets
	// headless tests cover early-out frame boundaries without fabricating targets.
	static void ExecuteRenderGraphPass(
		Flux_CommandBuffer* pxCommandList, void* pUserData);
	uint32_t UploadChars();

	void SetOverlayClipRect(const Zenith_Maths::Vector4& xRect, int iSortOrder);
	void ClearOverlayClipRect();
	void DiscardPendingFrame();

	// THE font accessor. Returns nullptr once the font is known-unavailable
	// instead of re-attempting the load, so a per-frame caller costs nothing.
	//
	// ALWAYS prefer this over GetFontHandle().Resolve() from anything that runs
	// per frame: Zenith_AssetHandle caches only SUCCESS, so a bare Resolve() on a
	// missing asset re-enters the registry (failed disk open + 3 log lines) EVERY
	// call. With Zenith/Assets absent -- a fresh CI checkout, since it is
	// gitignored -- UI layout drove ~25k load attempts in 180 s and reduced the
	// headless batch to a crawl, once the Null backend made the text path
	// actually execute.
	Zenith_FontAsset* TryResolveFont() { return m_bFontUnavailable ? nullptr : m_xFontAsset.Resolve(); }

	// Raw handle. For non-per-frame use (setup, teardown, tooling) only.
	FontHandle& GetFontHandle() { return m_xFontAsset; }
	const FontHandle& GetFontHandle() const { return m_xFontAsset; }

	Flux_Shader              m_xShader;
	Flux_Pipeline            m_xPipeline;
	Flux_DynamicVertexBuffer m_xInstanceBuffer;
	FontHandle               m_xFontAsset;
	// Latched at Initialise when the font fails to load. Zenith_AssetHandle
	// caches only SUCCESS, so an unresolved handle re-attempts the registry load
	// on EVERY Resolve() -- and the two text sites below run per frame. With the
	// font absent (a fresh CI checkout: Zenith/Assets is gitignored) that is a
	// failed disk open + three log lines per frame, forever, which reduced a
	// CityBuilder headless batch to a crawl once the Null backend made the text
	// path actually execute. Ask once; then stay a no-op.
	bool                     m_bFontUnavailable = false;

	bool                     m_bOverlayClipActive    = false;
	Zenith_Maths::Vector4    m_xOverlayClipRect      = { -1.f, -1.f, -1.f, -1.f };
	int                      m_iOverlayClipSortOrder = 100;
	uint32_t                 m_uBgCharCount          = 0;
	uint32_t                 m_uFgCharCount          = 0;
	uint32_t                 m_uTotalCharCount       = 0;
};
