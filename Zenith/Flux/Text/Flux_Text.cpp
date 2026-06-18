#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Text/Flux_TextImpl.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Text/Flux_TextQueue.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_FontAsset.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

// Cross-subsystem deps (FluxGraphics + VulkanMemory) are reached via g_xEngine
// at point of use. The non-capturing ExecuteText / hot-reload fn-pointer
// trampolines below re-enter via g_xEngine.Text() to recover the singleton
// instance. The Flux::Flux_TextQueue statics + the FontHandle asset are not
// g_xEngine subsystems.

static constexpr uint32_t s_uMaxCharsPerFrame = 65536;

DEBUGVAR float dbg_fTextSize = 100.f;

void Flux_TextImpl::BuildPipelines()
{
	this->m_xShader.Initialise(FluxShaderProgram::Text);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	// Per-vertex: unit quad (position + uv) — unchanged from legacy pipeline.
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();
	// Per-instance: matches Flux_TextVertex layout (56 bytes, static_asserted).
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);  // quadOffsetPx
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);  // quadSizePx
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);  // atlasUVOrigin
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);  // atlasUVSize
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);  // textRoot (float, was uint)
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);  // colour
	xVertexDesc.m_xPerInstanceLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = FINAL_RT_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &this->m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	this->m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(this->m_xPipeline, xPipelineSpec);
}

void Flux_TextImpl::Initialise()
{
	BuildPipelines();

	constexpr bool bDeviceLocal = false;
	g_xEngine.FluxMemory().InitialiseDynamicVertexBuffer(nullptr, s_uMaxCharsPerFrame * sizeof(Flux_TextVertex), this->m_xInstanceBuffer, bDeviceLocal);

	// Load the MSDF font asset. Path resolved via Zenith_AssetRegistry; the
	// asset itself reads the .zfont header and constructs a procedural texture
	// for the atlas (no mips — see Zenith_FontAsset::LoadFromFile).
	this->m_xFontAsset = FontHandle("engine:Fonts/LiberationMono.zfont");
	if (!this->m_xFontAsset.Resolve())
	{
		Zenith_Warning(LOG_CATEGORY_TEXT, "Flux_Text: failed to load engine:Fonts/LiberationMono.zfont — text rendering will be no-op");
	}

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddFloat({ "Text", "Size" }, dbg_fTextSize, 0, 1000);
#endif

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Text,
	};
	Flux_ShaderHotReload::RegisterSubsystem([](){ g_xEngine.Text().BuildPipelines(); },
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_TEXT, "Flux_Text initialised");
}

void Flux_TextImpl::Reset()
{
	Flux::Flux_TextQueue::ClearPending();
	Zenith_Log(LOG_CATEGORY_TEXT, "Flux_TextImpl::Reset() - Cleared pending text entries");
}

void Flux_TextImpl::ReleaseAssetReferences()
{
	this->m_xFontAsset.Clear();
}

void Flux_TextImpl::Shutdown()
{
	g_xEngine.FluxMemory().DestroyDynamicVertexBuffer(this->m_xInstanceBuffer);
	Zenith_Log(LOG_CATEGORY_TEXT, "Flux_Text shut down");
}

void Flux_TextImpl::SetOverlayClipRect(const Zenith_Maths::Vector4& xRect, int iSortOrder)
{
	this->m_bOverlayClipActive = true;
	this->m_xOverlayClipRect = xRect;
	this->m_iOverlayClipSortOrder = iSortOrder;
}

void Flux_TextImpl::ClearOverlayClipRect()
{
	this->m_bOverlayClipActive = false;
	this->m_xOverlayClipRect = {-1.f, -1.f, -1.f, -1.f};
}

// Per-glyph emission. For each char in the text:
//   - look up the GlyphMetric (advance + plane bounds + atlas UV rect)
//   - ink-less glyphs (space, etc.): advance cursor, emit no quad
//   - missing glyphs (stray non-ASCII): substitute space's advance, no quad,
//     don't `continue` silently — that collapses words on garbage input
//   - newlines: reset cursor X, step baseline by lineHeight
//   - everything else: emit one quad
//
// Baseline-Y semantics: fBaselineYPx = fAscentPx for the first line. The plane
// bounds in GlyphMetric are RELATIVE TO BASELINE (Y-down, see Zenith_FontAsset.h),
// so they're added to (cursorX, baselineY) without further sign-flips.
static void ProcessTextEntry(const Flux::Flux_TextEntry& xEntry,
							 const Zenith_FontAsset* pxFont,
							 Zenith_Vector<Flux_TextVertex>& xVertices,
							 uint32_t& uCharCount)
{
	if (!pxFont)
	{
		// Font wasn't loaded (boot order, missing .zfont). Skip silently — UI
		// layout still works via Zenith_FontAsset::GetDefaultMetrics().
		return;
	}

	const float fAscentPx     = pxFont->GetMetrics().fAscent     * xEntry.m_fSize;
	const float fLineHeightPx = pxFont->GetMetrics().fLineHeight * xEntry.m_fSize;
	float fCursorXPx   = 0.f;
	float fBaselineYPx = fAscentPx;

	for (size_t u = 0; u < xEntry.m_strText.size(); ++u)
	{
		// Use unsigned char so codepoints > 127 don't sign-extend to large
		// negatives on signed-char builds.
		const unsigned char uc = static_cast<unsigned char>(xEntry.m_strText[u]);

		if (uc == '\n')
		{
			fCursorXPx = 0.f;
			fBaselineYPx += fLineHeightPx;
			continue;
		}

		const Zenith_FontGlyphMetric* pxG = pxFont->FindGlyph(static_cast<u_int32>(uc));
		if (!pxG)
		{
			// Stray non-ASCII or out-of-charset. Substitute space's advance so
			// text retains approximate horizontal extent (better than silent collapse).
			if (const Zenith_FontGlyphMetric* pxSpace = pxFont->FindGlyph(' '))
			{
				fCursorXPx += pxSpace->m_fAdvance * xEntry.m_fSize;
			}
			continue;
		}

		// Ink-less glyph (space, etc.): advance only, no quad emitted.
		// Branch on zero-area, not codepoint, so any future ink-less glyph just works.
		const float fW = (pxG->m_fPlaneR - pxG->m_fPlaneL) * xEntry.m_fSize;
		const float fH = (pxG->m_fPlaneB - pxG->m_fPlaneT) * xEntry.m_fSize;
		if (fW <= 0.f || fH <= 0.f)
		{
			fCursorXPx += pxG->m_fAdvance * xEntry.m_fSize;
			continue;
		}

		Flux_TextVertex xVertex;
		xVertex.m_xQuadOffsetPx  = { fCursorXPx   + pxG->m_fPlaneL * xEntry.m_fSize,
									 fBaselineYPx + pxG->m_fPlaneT * xEntry.m_fSize };
		xVertex.m_xQuadSizePx    = { fW, fH };
		xVertex.m_xAtlasUVOrigin = { pxG->m_fAtlasU0, pxG->m_fAtlasV0 };
		xVertex.m_xAtlasUVSize   = { pxG->m_fAtlasU1 - pxG->m_fAtlasU0,
									 pxG->m_fAtlasV1 - pxG->m_fAtlasV0 };
		// xEntry.m_xPosition is already Vector2 (float). The legacy path floored
		// it via uint cast (bug #6) — keeping it float fixes subpixel jitter.
		xVertex.m_xTextRoot      = xEntry.m_xPosition;
		xVertex.m_xColour        = xEntry.m_xColor;
		xVertices.PushBack(xVertex);
		++uCharCount;

		fCursorXPx += pxG->m_fAdvance * xEntry.m_fSize;
	}
}

uint32_t Flux_TextImpl::UploadChars()
{
	Zenith_Vector<Flux_TextVertex> xVertices(s_uMaxCharsPerFrame);
	uint32_t uCharCount = 0;

	const Zenith_FontAsset* pxFont = this->m_xFontAsset.Resolve();

	Zenith_Vector<Flux::Flux_TextEntry>& xTextEntries = Flux::Flux_TextQueue::GetPending();

	if (this->m_bOverlayClipActive)
	{
		this->m_uBgCharCount = 0;
		this->m_uFgCharCount = 0;

		for (Zenith_Vector<Flux::Flux_TextEntry>::Iterator xIt(xTextEntries); !xIt.Done(); xIt.Next())
		{
			const Flux::Flux_TextEntry& xEntry = xIt.GetData();
			if (xEntry.m_iSortOrder < this->m_iOverlayClipSortOrder)
			{
				ProcessTextEntry(xEntry, pxFont, xVertices, this->m_uBgCharCount);
			}
		}

		for (Zenith_Vector<Flux::Flux_TextEntry>::Iterator xIt(xTextEntries); !xIt.Done(); xIt.Next())
		{
			const Flux::Flux_TextEntry& xEntry = xIt.GetData();
			if (xEntry.m_iSortOrder >= this->m_iOverlayClipSortOrder)
			{
				ProcessTextEntry(xEntry, pxFont, xVertices, this->m_uFgCharCount);
			}
		}

		uCharCount = this->m_uBgCharCount + this->m_uFgCharCount;
	}
	else
	{
		this->m_uBgCharCount = 0;
		this->m_uFgCharCount = 0;

		for (Zenith_Vector<Flux::Flux_TextEntry>::Iterator xIt(xTextEntries); !xIt.Done(); xIt.Next())
		{
			ProcessTextEntry(xIt.GetData(), pxFont, xVertices, uCharCount);
		}
	}

	Flux::Flux_TextQueue::ClearPending();

	if (xVertices.GetSize() > 0)
	{
		g_xEngine.FluxMemory().UploadBufferData(this->m_xInstanceBuffer.GetBuffer().m_xVRAMHandle, xVertices.GetDataPointer(), sizeof(Flux_TextVertex) * xVertices.GetSize());
	}

	this->m_uTotalCharCount = uCharCount;
	return uCharCount;
}

void Flux_TextImpl::Render(void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTextEnabled)
	{
		return;
	}

	UploadChars();
}

static void ExecuteText(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bTextEnabled)
	{
		return;
	}

	// Non-capturing graph callback (void(*)(Flux_CommandBuffer*, void*)) — it cannot
	// capture, so it re-enters via g_xEngine.Text() to reach the singleton
	// instance; FluxGraphics is reached via g_xEngine at point of use
	// (mirrors ExecuteQuads).
	Flux_TextImpl& xText = g_xEngine.Text();

	const uint32_t uNumChars = xText.UploadChars();
	if (uNumChars == 0)
	{
		xText.m_bOverlayClipActive = false;
		return;
	}

	const Zenith_FontAsset* pxFont = xText.m_xFontAsset.Resolve();
	if (!pxFont)
	{
		// No font loaded — nothing to draw.
		xText.m_bOverlayClipActive = false;
		return;
	}

	const Zenith_TextureAsset* pxAtlas = pxFont->GetAtlasTexture().Resolve();
	if (!pxAtlas)
	{
		Zenith_Warning(LOG_CATEGORY_TEXT, "Flux_Text: font asset has no atlas texture");
		xText.m_bOverlayClipActive = false;
		return;
	}

	pxCommandList->SetPipeline(&xText.m_xPipeline);

	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	pxCommandList->SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer(), 0);
	pxCommandList->SetIndexBuffer(xGraphics.m_xQuadMesh.GetIndexBuffer());
	pxCommandList->SetVertexBuffer(xText.m_xInstanceBuffer, 1);

	pxCommandList->BindCBV(&xGraphics.m_xFrameConstantsBuffer.GetCBV(), Flux_BindingSlot{ 0, 0, true });
	// Explicit clamp sampler at the bind site — MSDF AA assumes no wrap.
	pxCommandList->BindSRV(&pxAtlas->m_xSRV, 1);

	// Build the 32-byte push-constant block. Atlas size + pxRange feed the
	// shader's fwidth-based ScreenPxRange() helper for derivative-based AA.
	Flux_TextDrawConstants xConstants{};
	xConstants.m_xAtlasSizePx   = pxFont->GetAtlasSize();
	xConstants.m_fAtlasPxRange  = pxFont->GetAtlasPxRange();
	xConstants.m_fPad           = 0.f;

	if (xText.m_bOverlayClipActive && xText.m_uBgCharCount > 0)
	{
		// Background text draw: clip rect active.
		xConstants.m_xClipRect = xText.m_xOverlayClipRect;
		pxCommandList->BindDrawConstants(&xConstants, static_cast<u_int>(sizeof(Flux_TextDrawConstants)), 2);
		pxCommandList->DrawIndexed(6, xText.m_uBgCharCount, 0, 0, 0);

		if (xText.m_uFgCharCount > 0)
		{
			// Foreground/overlay text draw: clip off.
			xConstants.m_xClipRect = { -1.f, -1.f, -1.f, -1.f };
			pxCommandList->BindDrawConstants(&xConstants, static_cast<u_int>(sizeof(Flux_TextDrawConstants)), 2);
			pxCommandList->DrawIndexed(6, xText.m_uFgCharCount, 0, 0, xText.m_uBgCharCount);
		}
	}
	else
	{
		// No overlay clipping: single draw, clip off.
		xConstants.m_xClipRect = { -1.f, -1.f, -1.f, -1.f };
		pxCommandList->BindDrawConstants(&xConstants, static_cast<u_int>(sizeof(Flux_TextDrawConstants)), 2);
		pxCommandList->DrawIndexed(6, uNumChars);
	}

	xText.m_bOverlayClipActive = false;
}

void Flux_TextImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	xGraph.AddPass("Text", ExecuteText)
		.Writes(g_xEngine.FluxGraphics().GetFinalRenderTarget(), RESOURCE_ACCESS_WRITE_RTV);
}
