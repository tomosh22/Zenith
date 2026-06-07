#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"

class Flux_RenderGraph;
class Zenith_FontAsset;

// Dependencies injected into Initialise (Wave-15 DI seam, built on the WS9.2
// Flux_HiZImpl / Wave-11 Flux_SSAOImpl / Wave-14 Flux_QuadsImpl template; Wave-4
// extends it to ALSO inject VulkanMemory per the aggressive g_xEngine-shrink
// directive). Forward-declared here; full headers are pulled in by Flux_Text.cpp.
// (DebugVariables() stays a direct g_xEngine lookup — it is a ZENITH_TOOLS-only
// accessor reached once under #ifdef ZENITH_DEBUG_VARIABLES, so injecting it would
// force conditional compilation into the Initialise signature. The
// Zenith_UI::Zenith_UICanvas statics + the FontHandle asset are not g_xEngine
// subsystems, so they are out of scope.)
class Flux_GraphicsImpl;
class Zenith_Vulkan_MemoryManager;

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
static_assert(sizeof(Flux_TextVertex) == 56, "Flux_TextVertex layout drift will silently break the vertex input description");

// Push-constant block bound to the text fragment shader. Layout MUST match
// TextConstantsLayout in Flux_Text.slang. 32 bytes total (was 16 in the legacy
// bitmap pipeline) — a size mismatch with shader reflection causes silent
// rendering failures in some drivers.
struct Flux_TextDrawConstants
{
	Zenith_Maths::Vector4 m_xClipRect;        // (-1,-1,-1,-1) = clip off
	Zenith_Maths::Vector2 m_xAtlasSizePx;     // e.g. (256, 256)
	float                 m_fAtlasPxRange;    // msdfgen "range" in atlas pixels
	float                 m_fPad;             // pad to 16-byte alignment
};
static_assert(sizeof(Flux_TextDrawConstants) == 32, "Flux_TextDrawConstants must be 32 bytes to match shader push-constant range");

class Flux_TextImpl
{
public:
	Flux_TextImpl() = default;
	~Flux_TextImpl() = default;

	Flux_TextImpl(const Flux_TextImpl&) = delete;
	Flux_TextImpl& operator=(const Flux_TextImpl&) = delete;

	// Cross-subsystem dep is injected here and stored into m_pxGraphics below.
	// This is the WS9.2 DI template: explicit ref param -> stored member pointer.
	void Initialise(Flux_GraphicsImpl& xGraphics, Zenith_Vulkan_MemoryManager& xVulkanMemory);
	void BuildPipelines();
	void ReleaseAssetReferences();
	void Shutdown();
	void Reset();
	void Render(void*);
	void SetupRenderGraph(Flux_RenderGraph& xGraph);
	uint32_t UploadChars();

	void SetOverlayClipRect(const Zenith_Maths::Vector4& xRect, int iSortOrder);
	void ClearOverlayClipRect();

	// Accessor for the active font asset. Returns the handle (which Resolves on first
	// use) so callers can grab metrics or null-check. Used by
	// Zenith_FontAsset::GetActiveOrDefaultMetrics and by UI layout call sites.
	FontHandle& GetFontHandle() { return m_xFontAsset; }
	const FontHandle& GetFontHandle() const { return m_xFontAsset; }

	Flux_Shader              m_xShader;
	Flux_Pipeline            m_xPipeline;
	Flux_DynamicVertexBuffer m_xInstanceBuffer;
	FontHandle               m_xFontAsset;

	bool                     m_bOverlayClipActive    = false;
	Zenith_Maths::Vector4    m_xOverlayClipRect      = { -1.f, -1.f, -1.f, -1.f };
	int                      m_iOverlayClipSortOrder = 100;
	uint32_t                 m_uBgCharCount          = 0;
	uint32_t                 m_uFgCharCount          = 0;
	uint32_t                 m_uTotalCharCount       = 0;

	// Injected cross-subsystem dependency (stored by Initialise). Default nullptr
	// so a default-constructed instance is headless-safe; the real boot path wires
	// it in via the Text init trampoline (Flux_FeatureRegistry.cpp).
	Flux_GraphicsImpl*       m_pxGraphics            = nullptr;
	Zenith_Vulkan_MemoryManager* m_pxVulkanMemory    = nullptr;
};
