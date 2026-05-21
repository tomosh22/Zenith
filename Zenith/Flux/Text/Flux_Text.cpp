#include "Zenith.h"

#include "Flux/Text/Flux_Text.h"
#include "Flux/Text/Flux_TextImpl.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "UI/Zenith_UICanvas.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif


static constexpr uint32_t s_uMaxCharsPerFrame = 65536;

// fCHAR_ASPECT_RATIO and fCHAR_SPACING defined in Flux_Text.h

struct TextVertex
{
	Zenith_Maths::Vector2 m_xPos;
	Zenith_Maths::Vector2 m_xUV;
	Zenith_Maths::UVector2 m_xTextRoot;
	float m_fTextSize;
	Zenith_Maths::Vector4 m_xColour;
};

// Pinned via TextureHandle so UnloadUnused never frees the font atlas while the
// engine is running. Cleared in Flux_Text::ReleaseAssetReferences.

// Overlay clip rect state (set during canvas render, consumed during Flux_Text::Render)

DEBUGVAR float dbg_fTextSize = 100.f;

void Flux_Text::BuildPipelines()
{
	g_xEngine.Text().m_xShader.Initialise(FluxShaderProgram::Text);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);//position
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT2);//offset into font texture atlas
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_UINT2);//text root
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT);//text size
	xVertexDesc.m_xPerInstanceLayout.GetElements().PushBack(SHADER_DATA_TYPE_FLOAT4);//colour
	xVertexDesc.m_xPerInstanceLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = FINAL_RT_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_pxShader = &g_xEngine.Text().m_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	g_xEngine.Text().m_xShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(g_xEngine.Text().m_xPipeline, xPipelineSpec);
}

void Flux_Text::Initialise()
{
	BuildPipelines();

	//#TO_TODO: need to experiment with this, not sure which will be faster
	constexpr bool bDeviceLocal = false;
	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, s_uMaxCharsPerFrame * sizeof(TextVertex), g_xEngine.Text().m_xInstanceBuffer, bDeviceLocal);

	if (Zenith_TextureAsset* pxFontAtlas = Zenith_AssetRegistry::Get<Zenith_TextureAsset>(ENGINE_ASSETS_DIR "Textures/Font/FontAtlas" ZENITH_TEXTURE_EXT))
	{
		g_xEngine.Text().m_xFontAtlasTexture.Set(pxFontAtlas);
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_TEXT, "Warning: Failed to load font atlas texture, using white texture");
		g_xEngine.Text().m_xFontAtlasTexture = g_xEngine.FluxGraphics().m_xWhiteTexture;  // copy: AddRefs the white default
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddFloat({ "Text", "Size" }, dbg_fTextSize, 0, 1000);
#endif

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Text,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&Flux_Text::BuildPipelines,
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

	Zenith_Log(LOG_CATEGORY_TEXT, "Flux_Text initialised");
}

void Flux_Text::Reset()
{
	// Clear pending text entries to prevent stale text from destroyed scenes persisting
	Zenith_UI::Zenith_UICanvas::ClearPendingTextEntries();

	Zenith_Log(LOG_CATEGORY_TEXT, "Flux_Text::Reset() - Cleared pending text entries");
}

void Flux_Text::ReleaseAssetReferences()
{
	g_xEngine.Text().m_xFontAtlasTexture.Clear();
}

void Flux_Text::Shutdown()
{
	Flux_MemoryManager::DestroyDynamicVertexBuffer(g_xEngine.Text().m_xInstanceBuffer);
	Zenith_Log(LOG_CATEGORY_TEXT, "Flux_Text shut down");
}

void Flux_Text::SetOverlayClipRect(const Zenith_Maths::Vector4& xRect, int iSortOrder)
{
	g_xEngine.Text().m_bOverlayClipActive = true;
	g_xEngine.Text().m_xOverlayClipRect = xRect;
	g_xEngine.Text().m_iOverlayClipSortOrder = iSortOrder;
}

void Flux_Text::ClearOverlayClipRect()
{
	g_xEngine.Text().m_bOverlayClipActive = false;
	g_xEngine.Text().m_xOverlayClipRect = {-1.f, -1.f, -1.f, -1.f};
}

// Helper: process a text entry's characters into the vertex buffer
static void ProcessTextEntry(const Zenith_UI::UITextEntry& xEntry, Zenith_Vector<TextVertex>& xVertices, uint32_t& uCharCount)
{
	float fCursorX = 0.f;
	float fCursorY = 0.f;
	for (uint32_t u = 0; u < xEntry.m_strText.size(); u++)
	{
		char cChar = xEntry.m_strText.at(u);

		// Handle newline: advance to next line
		if (cChar == '\n')
		{
			fCursorX = 0.f;
			fCursorY += 1.f;
			continue;
		}

		// Skip non-printable characters (ASCII < 32) to prevent index underflow
		// Font atlas starts at space (ASCII 32), so characters below that are invalid
		if (cChar < 32 || cChar > 126)
		{
			continue;
		}

		TextVertex xVertex;
		xVertex.m_xTextRoot = { static_cast<uint32_t>(xEntry.m_xPosition.x), static_cast<uint32_t>(xEntry.m_xPosition.y) };
		xVertex.m_fTextSize = xEntry.m_fSize;
		xVertex.m_xPos = Zenith_Maths::Vector2(fCursorX, fCursorY);
		xVertex.m_xColour = xEntry.m_xColor;

		const uint32_t uIndex = static_cast<uint32_t>(cChar - 32);

		const Zenith_Maths::UVector2 xTextureOffsets = { (uIndex % 10), (uIndex / 10) };
		xVertex.m_xUV = { xTextureOffsets.x, xTextureOffsets.y };
		xVertex.m_xUV /= 10.f;
		uCharCount++;

		xVertices.PushBack(xVertex);
		fCursorX += fCHAR_SPACING;
	}
}

//#TO returns number of chars to render
uint32_t Flux_Text::UploadChars()
{
	Zenith_Vector<TextVertex> xVertices(s_uMaxCharsPerFrame);
	uint32_t uCharCount = 0;

	Zenith_Vector<Zenith_UI::UITextEntry>& xUITextEntries = Zenith_UI::Zenith_UICanvas::GetPendingTextEntries();

	if (g_xEngine.Text().m_bOverlayClipActive)
	{
		// Two-pass partitioning: background text first, then overlay text
		// This allows the renderer to draw them with different clip rect push constants
		g_xEngine.Text().m_uBgCharCount = 0;
		g_xEngine.Text().m_uFgCharCount = 0;

		// Pass 1: Background text (sort order below overlay threshold)
		for (Zenith_Vector<Zenith_UI::UITextEntry>::Iterator xIt(xUITextEntries); !xIt.Done(); xIt.Next())
		{
			const Zenith_UI::UITextEntry& xEntry = xIt.GetData();
			if (xEntry.m_iSortOrder < g_xEngine.Text().m_iOverlayClipSortOrder)
			{
				ProcessTextEntry(xEntry, xVertices, g_xEngine.Text().m_uBgCharCount);
			}
		}

		// Pass 2: Overlay/foreground text (sort order at or above overlay threshold)
		for (Zenith_Vector<Zenith_UI::UITextEntry>::Iterator xIt(xUITextEntries); !xIt.Done(); xIt.Next())
		{
			const Zenith_UI::UITextEntry& xEntry = xIt.GetData();
			if (xEntry.m_iSortOrder >= g_xEngine.Text().m_iOverlayClipSortOrder)
			{
				ProcessTextEntry(xEntry, xVertices, g_xEngine.Text().m_uFgCharCount);
			}
		}

		uCharCount = g_xEngine.Text().m_uBgCharCount + g_xEngine.Text().m_uFgCharCount;
	}
	else
	{
		// No overlay active: process all entries in a single pass
		g_xEngine.Text().m_uBgCharCount = 0;
		g_xEngine.Text().m_uFgCharCount = 0;

		for (Zenith_Vector<Zenith_UI::UITextEntry>::Iterator xIt(xUITextEntries); !xIt.Done(); xIt.Next())
		{
			ProcessTextEntry(xIt.GetData(), xVertices, uCharCount);
		}
	}

	// Clear UI text entries after processing
	Zenith_UI::Zenith_UICanvas::ClearPendingTextEntries();

	Flux_MemoryManager::UploadBufferData(g_xEngine.Text().m_xInstanceBuffer.GetBuffer().m_xVRAMHandle, xVertices.GetDataPointer(), sizeof(TextVertex) * xVertices.GetSize());

	g_xEngine.Text().m_uTotalCharCount = uCharCount;
	return uCharCount;
}

void Flux_Text::Render(void*)
{
	if (!Zenith_GraphicsOptions::Get().m_bTextEnabled)
	{
		return;
	}

	UploadChars();
}

static void ExecuteText(Flux_CommandList* pxCommandList, void* pUserData)
{
	(void)pUserData;
	if (!Zenith_GraphicsOptions::Get().m_bTextEnabled)
	{
		return;
	}

	uint32_t uNumChars = Flux_Text::UploadChars();
	if (uNumChars == 0)
	{
		g_xEngine.Text().m_bOverlayClipActive = false;
		return;
	}

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&g_xEngine.Text().m_xPipeline);

	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer(), 0);
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.Text().m_xInstanceBuffer, 1);

	pxCommandList->AddCommand<Flux_CommandBeginBind>(0);
	pxCommandList->AddCommand<Flux_CommandBindCBV>(&g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV(), 0);
	pxCommandList->AddCommand<Flux_CommandBindSRV>(&g_xEngine.Text().m_xFontAtlasTexture.GetDirect()->m_xSRV, 1);

	if (g_xEngine.Text().m_bOverlayClipActive && g_xEngine.Text().m_uBgCharCount > 0)
	{
		// Draw background text with overlay clip rect active
		pxCommandList->AddCommand<Flux_CommandBindDrawConstants>(&g_xEngine.Text().m_xOverlayClipRect, static_cast<u_int>(sizeof(Zenith_Maths::Vector4)), 2);
		pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6, g_xEngine.Text().m_uBgCharCount, 0, 0, 0);

		if (g_xEngine.Text().m_uFgCharCount > 0)
		{
			// Draw overlay text without clip rect
			Zenith_Maths::Vector4 xNoClip = {-1.f, -1.f, -1.f, -1.f};
			pxCommandList->AddCommand<Flux_CommandBindDrawConstants>(&xNoClip, static_cast<u_int>(sizeof(Zenith_Maths::Vector4)), 2);
			pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6, g_xEngine.Text().m_uFgCharCount, 0, 0, g_xEngine.Text().m_uBgCharCount);
		}
	}
	else
	{
		// No overlay clipping: single draw, clip rect disabled
		Zenith_Maths::Vector4 xNoClip = {-1.f, -1.f, -1.f, -1.f};
		pxCommandList->AddCommand<Flux_CommandBindDrawConstants>(&xNoClip, static_cast<u_int>(sizeof(Zenith_Maths::Vector4)), 2);
		pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6, uNumChars);
	}

	// Clear overlay clip rect for next frame
	g_xEngine.Text().m_bOverlayClipActive = false;
}

void Flux_Text::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	xGraph.AddPass("Text", ExecuteText)
		.Writes(Flux_Graphics::GetFinalRenderTarget(), RESOURCE_ACCESS_WRITE_RTV);
}