#include "Zenith.h"

#include "Flux/Text/Flux_Text.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "UI/Zenith_UICanvas.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Zenith_OS_Include.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_TEXT, Flux_Text::Render, nullptr);

static Flux_CommandList g_xCommandList("Text");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static constexpr uint32_t s_uMaxCharsPerFrame = 65536;

// Character width as fraction of height (typical monospace ratio is ~0.5-0.6)
// Must match CHAR_ASPECT_RATIO in Flux_Text.vert
static constexpr float fCHAR_ASPECT_RATIO = 0.5f;

// Character spacing
static constexpr float fCHAR_SPACING = fCHAR_ASPECT_RATIO * 0.5f;

struct TextVertex
{
	Zenith_Maths::Vector2 m_xPos;
	Zenith_Maths::Vector2 m_xUV;
	Zenith_Maths::UVector2 m_xTextRoot;
	float m_fTextSize;
	Zenith_Maths::Vector4 m_xColour;
};
static Flux_DynamicVertexBuffer s_xInstanceBuffer;

static Zenith_TextureAsset* s_pxFontAtlasTexture = nullptr;

DEBUGVAR bool dbg_bEnable = true;
DEBUGVAR float dbg_fTextSize = 100.f;

void Flux_Text::Initialise()
{
	s_xShader.Initialise("Text/Flux_Text.vert", "Text/Flux_Text.frag");

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
	xPipelineSpec.m_pxTargetSetup = &Flux_Graphics::s_xFinalRenderTarget;
	xPipelineSpec.m_pxShader = &s_xShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_TEXTURE;

	xPipelineSpec.m_bDepthTestEnabled = false;
	xPipelineSpec.m_bDepthWriteEnabled = false;

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	//#TO_TODO: need to experiment with this, not sure which will be faster
	constexpr bool bDeviceLocal = false;
	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, s_uMaxCharsPerFrame * sizeof(TextVertex), s_xInstanceBuffer, bDeviceLocal);

	s_pxFontAtlasTexture = Zenith_AssetRegistry::Get().Get<Zenith_TextureAsset>(ENGINE_ASSETS_DIR "Textures/Font/FontAtlas" ZENITH_TEXTURE_EXT);
	if (!s_pxFontAtlasTexture)
	{
		Zenith_Log(LOG_CATEGORY_TEXT, "Warning: Failed to load font atlas texture, using white texture");
		s_pxFontAtlasTexture = Flux_Graphics::s_pxWhiteTexture;
	}

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Text" }, dbg_bEnable);
	Zenith_DebugVariables::AddFloat({ "Text", "Size" }, dbg_fTextSize, 0, 1000);
#endif

	Zenith_Log(LOG_CATEGORY_TEXT, "Flux_Text initialised");
}

void Flux_Text::Reset()
{
	// Reset command list to ensure no stale GPU resource references, including descriptor bindings
	// This is called when the scene is reset (e.g., Play/Stop transitions in editor)
	g_xCommandList.Reset(true);

	// Clear pending text entries to prevent stale text from destroyed scenes persisting
	Zenith_UI::Zenith_UICanvas::ClearPendingTextEntries();

	Zenith_Log(LOG_CATEGORY_TEXT, "Flux_Text::Reset() - Reset command list and cleared pending text entries");
}

void Flux_Text::Shutdown()
{
	Flux_MemoryManager::DestroyDynamicVertexBuffer(s_xInstanceBuffer);
	Zenith_Log(LOG_CATEGORY_TEXT, "Flux_Text shut down");
}

//#TO returns number of chars to render
uint32_t Flux_Text::UploadChars()
{
	Zenith_Vector<TextVertex> xVertices(s_uMaxCharsPerFrame);
	uint32_t uCharCount = 0;

	// Process UI text entries from Zenith_UICanvas
	Zenith_Vector<Zenith_UI::UITextEntry>& xUITextEntries = Zenith_UI::Zenith_UICanvas::GetPendingTextEntries();
	for (Zenith_Vector<Zenith_UI::UITextEntry>::Iterator xIt(xUITextEntries); !xIt.Done(); xIt.Next())
	{
		const Zenith_UI::UITextEntry& xEntry = xIt.GetData();
		for (uint32_t u = 0; u < xEntry.m_strText.size(); u++)
		{
			TextVertex xVertex;
			xVertex.m_xTextRoot = { static_cast<uint32_t>(xEntry.m_xPosition.x), static_cast<uint32_t>(xEntry.m_xPosition.y) };
			xVertex.m_fTextSize = xEntry.m_fSize;
			// Character spacing includes small gap for natural appearance
			xVertex.m_xPos = Zenith_Maths::Vector2(u * fCHAR_SPACING, 0.f);
			xVertex.m_xColour = xEntry.m_xColor;

			char cChar = xEntry.m_strText.at(u);

			// Skip non-printable characters (ASCII < 32) to prevent index underflow
			// Font atlas starts at space (ASCII 32), so characters below that are invalid
			if (cChar < 32 || cChar > 126)
			{
				continue;
			}

			const uint32_t uIndex = static_cast<uint32_t>(cChar - 32);

			const Zenith_Maths::UVector2 xTextureOffsets = { (uIndex % 10), (uIndex / 10) };
			xVertex.m_xUV = { xTextureOffsets.x, xTextureOffsets.y };
			xVertex.m_xUV /= 10.f;
			uCharCount++;

			xVertices.PushBack(xVertex);
		}
	}
	// Clear UI text entries after processing
	Zenith_UI::Zenith_UICanvas::ClearPendingTextEntries();

	Flux_MemoryManager::UploadBufferData(s_xInstanceBuffer.GetBuffer().m_xVRAMHandle, xVertices.GetDataPointer(), sizeof(TextVertex) * xVertices.GetSize());

	return uCharCount;
}

void Flux_Text::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Text::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Text::Render(void*)
{
	if (!dbg_bEnable)
	{
		return;
	}

	uint32_t uNumChars = UploadChars();

	g_xCommandList.Reset(false);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xPipeline);

	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&Flux_Graphics::s_xQuadMesh.GetVertexBuffer(), 0);
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xInstanceBuffer, 1);

	g_xCommandList.AddCommand<Flux_CommandBeginBind>(0);
	g_xCommandList.AddCommand<Flux_CommandBindCBV>(&Flux_Graphics::s_xFrameConstantsBuffer.GetCBV(), 0);
	g_xCommandList.AddCommand<Flux_CommandBindSRV>(&s_pxFontAtlasTexture->m_xSRV, 1);

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6, uNumChars);

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget, RENDER_ORDER_TEXT);
}