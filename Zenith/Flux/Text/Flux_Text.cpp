#include "Zenith.h"

#include "Flux/Text/Flux_Text.h"

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "EntityComponent/Components/Zenith_TextComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"


static Flux_CommandBuffer s_xCommandBuffer;

static Flux_Shader s_xShader;
static Flux_Pipeline s_xPipeline;

static constexpr uint32_t s_uMaxCharsPerFrame = 4096;
struct TextVertex
{
	Zenith_Maths::Vector2 m_xPos;
	Zenith_Maths::Vector2 m_xUV;
	Zenith_Maths::UVector2 m_xTextRoot;
	float m_fTextSize;
};
static Flux_DynamicVertexBuffer s_xInstanceBuffer;

static Flux_Texture* s_pxFontAtlas = nullptr;

DEBUGVAR bool dbg_bEnable = true;
DEBUGVAR Zenith_Maths::Vector2 dbg_xPosition = {0,0};
DEBUGVAR float dbg_fTextSize = 100.f;

void Flux_Text::Initialise()
{
	s_xCommandBuffer.Initialise();

	s_xShader.Initialise("Text/Flux_Text.vert", "Text/Flux_Text.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT3);
	xVertexDesc.m_xPerVertexLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();
	xVertexDesc.m_xPerInstanceLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);//position
	xVertexDesc.m_xPerInstanceLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT2);//offset into font texture atlas
	xVertexDesc.m_xPerInstanceLayout.GetElements().push_back(SHADER_DATA_TYPE_UINT2);//text root
	xVertexDesc.m_xPerInstanceLayout.GetElements().push_back(SHADER_DATA_TYPE_FLOAT);//text size
	xVertexDesc.m_xPerInstanceLayout.CalculateOffsetsAndStrides();

	std::vector<Flux_BlendState> xBlendStates;
	xBlendStates.push_back({ BLEND_FACTOR_SRCALPHA, BLEND_FACTOR_ONE, true });

	
	Flux_PipelineSpecification xPipelineSpec(
		xVertexDesc,
		&s_xShader,
		xBlendStates,
		false,
		false,
		DEPTH_COMPARE_FUNC_ALWAYS,
		DEPTHSTENCIL_FORMAT_D32_SFLOAT,
		true,
		false,
		{1,1},
		{0,0},
		Flux_Graphics::s_xFinalRenderTarget
	);

	Flux_PipelineBuilder::FromSpecification(s_xPipeline, xPipelineSpec);

	//#TO_TODO: need to experiment with this, not sure which will be faster
	constexpr bool bDeviceLocal = false;
	Flux_MemoryManager::InitialiseDynamicVertexBuffer(nullptr, s_uMaxCharsPerFrame * sizeof(TextVertex), s_xInstanceBuffer, bDeviceLocal);

	Zenith_AssetHandler::AddTexture2D(Zenith_GUID(), "Font_Atlas", "C:/dev/Zenith/Zenith/Assets/FontAtlas.ztx");
	s_pxFontAtlas = &Zenith_AssetHandler::GetTexture("Font_Atlas");

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Enable", "Text" }, dbg_bEnable);
	Zenith_DebugVariables::AddVector2( { "Text", "Position" }, dbg_xPosition, 0, 4000);
	Zenith_DebugVariables::AddFloat( { "Text", "Size" }, dbg_fTextSize, 0, 1000);
#endif

	Zenith_Log("Flux_Text initialised");
}

//#TO returns number of chars to render
uint32_t Flux_Text::UploadChars()
{
	std::vector<Zenith_TextComponent*> xComponents;
	std::vector<TextVertex> xVertices(s_uMaxCharsPerFrame);
	Zenith_Scene::GetCurrentScene().GetAllOfComponentType<Zenith_TextComponent>(xComponents);

	uint32_t uCharCount = 0;
	for (Zenith_TextComponent* pxComponent : xComponents)
	{
		for (TextEntry& xText : pxComponent->m_xEntries)
		{
			for (uint32_t u = 0; u < xText.m_strText.size(); u++)
			{
				TextVertex& xVertex = xVertices.at(uCharCount++);
				xVertex.m_xTextRoot = dbg_xPosition;
				xVertex.m_fTextSize = dbg_fTextSize;
				const float fSpacing = xVertex.m_fTextSize / 500.f;
				xVertex.m_xPos = Zenith_Maths::Vector2(uCharCount * fSpacing, 0.f);

				char cChar = xText.m_strText.at(u);

				//#TO font atlas starts at unicode 20, we take away 11 to shift where we sample up/left one character, and take off one more to account for off by one error
				const uint32_t uIndex = cChar - 32;

				const Zenith_Maths::UVector2 xTextureOffsets = { (uIndex % 10), (uIndex / 10) };
				xVertex.m_xUV = { xTextureOffsets.x, xTextureOffsets.y };
				xVertex.m_xUV /= 10.f;
				uCharCount++;
			}
		}
	}

	Flux_MemoryManager::UploadData(&s_xInstanceBuffer.GetBuffer(), xVertices.data(), sizeof(TextVertex) * xVertices.size());

	return uCharCount;
}

void Flux_Text::Render()
{
	if (!dbg_bEnable)
	{
		return;
	}

	uint32_t uNumChars = UploadChars();

	s_xCommandBuffer.BeginRecording();

	s_xCommandBuffer.SubmitTargetSetup(Flux_Graphics::s_xFinalRenderTarget);

	s_xCommandBuffer.SetPipeline(&s_xPipeline);

	s_xCommandBuffer.SetVertexBuffer(Flux_Graphics::s_xQuadMesh.GetVertexBuffer(), 0);
	s_xCommandBuffer.SetIndexBuffer(Flux_Graphics::s_xQuadMesh.GetIndexBuffer());
	s_xCommandBuffer.SetVertexBuffer(s_xInstanceBuffer, 1);

	s_xCommandBuffer.BeginBind(BINDING_FREQUENCY_PER_FRAME);
	s_xCommandBuffer.BindBuffer(&Flux_Graphics::s_xFrameConstantsBuffer.GetBuffer(), 0);
	s_xCommandBuffer.BindTexture(s_pxFontAtlas, 1);

	s_xCommandBuffer.DrawIndexed(6, uNumChars);

	s_xCommandBuffer.EndRecording(RENDER_ORDER_TEXT);
}
