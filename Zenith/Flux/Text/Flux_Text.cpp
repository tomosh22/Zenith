#include "Zenith.h"

#include "Flux/Text/Flux_Text.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Zenith_OS_Include.h"
#include "TaskSystem/Zenith_TaskSystem.h"

static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_TEXT, Flux_Text::Render, nullptr);

static Flux_CommandList g_xCommandList("Text");

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static constexpr uint32_t s_uMaxCharsPerFrame = 65536;
struct TextVertex
{
	Zenith_Maths::Vector2 m_xPos;
	Zenith_Maths::Vector2 m_xUV;
	Zenith_Maths::UVector2 m_xTextRoot;
	float m_fTextSize;
};
static Flux_DynamicVertexBuffer s_xInstanceBuffer;

static Flux_Texture s_xFontAtlasTexture;

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

	Zenith_AssetHandler::TextureData xTexData = Zenith_AssetHandler::LoadTexture2DFromFile("C:/dev/Zenith/Zenith/Assets/FontAtlas.ztx");
	Zenith_AssetHandler::AddTexture("Font_Atlas", xTexData);
	xTexData.FreeAllocatedData();
	s_xFontAtlasTexture = Zenith_AssetHandler::GetTexture("Font_Atlas");

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Text" }, dbg_bEnable);
	Zenith_DebugVariables::AddFloat({ "Text", "Size" }, dbg_fTextSize, 0, 1000);
#endif

	Zenith_Log("Flux_Text initialised");
}

//#TO returns number of chars to render
uint32_t Flux_Text::UploadChars()
{
	Zenith_Vector<Zenith_TextComponent*> xComponents;
	Zenith_Vector<TextVertex> xVertices(s_uMaxCharsPerFrame);
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TextComponent>(xComponents);

	uint32_t uCharCount = 0;
	for (Zenith_Vector<Zenith_TextComponent*>::Iterator xIt(xComponents); !xIt.Done(); xIt.Next())
	{
		Zenith_TextComponent* pxComponent = xIt.GetData();

		for (TextEntry& xText : pxComponent->m_xEntries)
		{
			for (uint32_t u = 0; u < xText.m_strText.size(); u++)
			{
				TextVertex xVertex;
				xVertex.m_xTextRoot = xText.m_xPosition;
				xVertex.m_fTextSize = dbg_fTextSize;
				const float fSpacing = xVertex.m_fTextSize / 200.f;
				xVertex.m_xPos = Zenith_Maths::Vector2(u * fSpacing, 0.f);

				char cChar = xText.m_strText.at(u);

				//#TO font atlas starts at unicode 20, we take away 11 to shift where we sample up/left one character, and take off one more to account for off by one error
				const uint32_t uIndex = cChar - 32;

				const Zenith_Maths::UVector2 xTextureOffsets = { (uIndex % 10), (uIndex / 10) };
				xVertex.m_xUV = { xTextureOffsets.x, xTextureOffsets.y };
				xVertex.m_xUV /= 10.f;
				uCharCount++;

				xVertices.PushBack(xVertex);
			}
		}

		for (TextEntry_World& xText : pxComponent->m_xEntries_World)
		{
			for (uint32_t u = 0; u < xText.m_strText.size(); u++)
			{
				TextVertex xVertex;
				Zenith_Maths::Vector4 xTextRoot(xText.m_xPosition.x, xText.m_xPosition.y, xText.m_xPosition.z, 1);
				Zenith_Maths::Vector4 xClipSpace = Flux_Graphics::GetViewProjMatrix() * xTextRoot;
				Zenith_Maths::Vector4 xScreenSpace = xClipSpace / xClipSpace.w;
				if (xScreenSpace.x > 1 || xScreenSpace.x < -1 ||
					xScreenSpace.y > 1 || xScreenSpace.y < -1 ||
					xScreenSpace.z > 1 || xScreenSpace.z < -1)
				{
					continue;
				}
				xScreenSpace = (xScreenSpace + Zenith_Maths::Vector4(1.f)) / Zenith_Maths::Vector4(2.f);
				int32_t iWindowWidth, iWindowHeight;
				Zenith_Window::GetInstance()->GetSize(iWindowWidth, iWindowHeight);
				xVertex.m_xTextRoot = { xScreenSpace.x * iWindowWidth, xScreenSpace.y * iWindowHeight};
				xVertex.m_fTextSize = dbg_fTextSize;
				const float fSpacing = xVertex.m_fTextSize / 200.f;
				xVertex.m_xPos = Zenith_Maths::Vector2(u * fSpacing, 0.f);

				char cChar = xText.m_strText.at(u);

				//#TO font atlas starts at unicode 20, we take away 11 to shift where we sample up/left one character, and take off one more to account for off by one error
				const uint32_t uIndex = cChar - 32;

				const Zenith_Maths::UVector2 xTextureOffsets = { (uIndex % 10), (uIndex / 10) };
				xVertex.m_xUV = { xTextureOffsets.x, xTextureOffsets.y };
				xVertex.m_xUV /= 10.f;
				uCharCount++;

				xVertices.PushBack(xVertex);
			}
		}

		
	}

	Flux_MemoryManager::UploadBufferData(s_xInstanceBuffer.GetBufferVRAM().m_xVRAMHandle, xVertices.GetDataPointer(), sizeof(TextVertex) * xVertices.GetSize());

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
	g_xCommandList.AddCommand<Flux_CommandBindBuffer>(&Flux_Graphics::s_xFrameConstantsBuffer.GetBufferVRAM(), 0);
	g_xCommandList.AddCommand<Flux_CommandBindSRV>(&s_xFontAtlasTexture.m_xSRV, 1);

	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6, uNumChars);

	Flux::SubmitCommandList(&g_xCommandList, Flux_Graphics::s_xFinalRenderTarget, RENDER_ORDER_TEXT);
}