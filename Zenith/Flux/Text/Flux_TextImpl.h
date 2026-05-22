#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"

class Flux_RenderGraph;

/// Character width as fraction of height (typical monospace ratio is ~0.5-0.6).
static constexpr float fCHAR_ASPECT_RATIO = 0.5f;

/// Monospace character spacing.
static constexpr float fCHAR_SPACING = fCHAR_ASPECT_RATIO * 1.1f;

// Fraction of font height consumed by the ascender in the SDF atlas
static constexpr float fFONT_ASCENDER_RATIO = 0.25f;

// Phase 9: state + behaviour for Text subsystem.
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
	uint32_t UploadChars();

	void SetOverlayClipRect(const Zenith_Maths::Vector4& xRect, int iSortOrder);
	void ClearOverlayClipRect();

	Flux_Shader              m_xShader;
	Flux_Pipeline            m_xPipeline;
	Flux_DynamicVertexBuffer m_xInstanceBuffer;
	TextureHandle            m_xFontAtlasTexture;

	bool                     m_bOverlayClipActive    = false;
	Zenith_Maths::Vector4    m_xOverlayClipRect      = { -1.f, -1.f, -1.f, -1.f };
	int                      m_iOverlayClipSortOrder = 100;
	uint32_t                 m_uBgCharCount          = 0;
	uint32_t                 m_uFgCharCount          = 0;
	uint32_t                 m_uTotalCharCount       = 0;
};
