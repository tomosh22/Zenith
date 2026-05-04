#pragma once

class Flux_RenderGraph;

/// Character width as fraction of height (typical monospace ratio is ~0.5-0.6).
/// Must also match CHAR_ASPECT_RATIO in Flux_Text.vert.
static constexpr float fCHAR_ASPECT_RATIO = 0.5f;

/// Monospace character spacing: width = fontSize * fCHAR_SPACING.
/// The Flux_Text system uses a monospace SDF font atlas (10x10 glyph grid).
/// Proportional fonts are not supported. UI text metrics
/// (Zenith_UIText::GetTextWidth/GetTextHeight) rely on this constant.
static constexpr float fCHAR_SPACING = fCHAR_ASPECT_RATIO * 1.1f;

// Fraction of font height consumed by the ascender in the SDF atlas
// Used by layout groups to correct text vertical centering
static constexpr float fFONT_ASCENDER_RATIO = 0.25f;

class Flux_Text
{
public:
	static void Initialise();
	static void BuildPipelines();

	// Drop refs to font atlas texture before the asset registry shuts down.
	// Called from Flux::ReleaseAssetReferences.
	static void ReleaseAssetReferences();

	static void Shutdown();

	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void Render(void*);

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

	static uint32_t UploadChars();

	// Overlay clip rect: text entries with sort order below the threshold
	// will have fragments inside the clip rect discarded in the fragment shader.
	// Must be called each frame during canvas render (before Flux_Text::Render).
	static void SetOverlayClipRect(const Zenith_Maths::Vector4& xRect, int iSortOrder);
	static void ClearOverlayClipRect();
};