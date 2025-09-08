#pragma once

#include "Collections/Zenith_Vector.h"
#include "Zenith_PlatformGraphics_Include.h"
#include "Flux/Flux_Enums.h"
#include "Flux/Flux_CommandList.h"

struct Flux_RenderAttachment {
	ColourFormat m_eColourFormat = COLOUR_FORMAT_NONE;
	DepthStencilFormat m_eDepthStencilFormat = DEPTHSTENCIL_FORMAT_NONE;

	uint32_t m_uWidth = 0;
	uint32_t m_uHeight = 0;

	//one per frame in flight
	Flux_Texture* m_pxTargetTexture = nullptr;
};

class Flux_RenderAttachmentBuilder {
public:
	Flux_RenderAttachmentBuilder() = default;
	ColourFormat m_eColourFormat = COLOUR_FORMAT_NONE;
	DepthStencilFormat m_eDepthStencilFormat = DEPTHSTENCIL_FORMAT_NONE;
	uint32_t m_uWidth;
	uint32_t m_uHeight;

	void Build(Flux_RenderAttachment& xAttachment, RenderTargetType eType, const std::string& strName);
};

struct Flux_TargetSetup {
	Flux_RenderAttachment m_axColourAttachments[FLUX_MAX_TARGETS];

	//#TO not owned by this
	Flux_RenderAttachment* m_pxDepthStencil = nullptr;

	std::string m_strName;

	void AssignDepthStencil(Flux_RenderAttachment* pxDS);

	const uint32_t GetNumColourAttachments();

	bool operator==(const Flux_TargetSetup& xOther)
	{
		for (u_int u = 0; u < FLUX_MAX_TARGETS; u++)
		{
			if (m_axColourAttachments[u].m_pxTargetTexture != xOther.m_axColourAttachments[u].m_pxTargetTexture)
			{
				return false;
			}
		}
		return m_pxDepthStencil == xOther.m_pxDepthStencil;
	}

	bool operator!=(const Flux_TargetSetup& xOther)
	{
		return !(*this == xOther);
	}
};

class Flux
{
public:
	Flux() = delete;
	~Flux() = delete;
	static void EarlyInitialise();
	static void LateInitialise();

	static const uint32_t GetFrameCounter() { return s_uFrameCounter; }

	static void SubmitCommandList(const Flux_CommandList* pxCmdList, const Flux_TargetSetup& xTargetSetup, RenderOrder eOrder){s_xPendingCommandLists[eOrder].PushBack({pxCmdList, xTargetSetup}); }

	static void AddResChangeCallback(void(*pfnCallback)()) { s_xResChangeCallbacks.push_back(pfnCallback); }
	static void OnResChange();

	static void RegisterBindlessTexture(Flux_Texture* pxTex, uint32_t uIndex);
private:
	friend class Flux_PlatformAPI;
	static void Platform_RegisterBindlessTexture(Flux_Texture* pxTex, uint32_t uIndex);

	static uint32_t s_uFrameCounter;
	static std::vector<void(*)()> s_xResChangeCallbacks;
	static Zenith_Vector<std::pair<const Flux_CommandList*, Flux_TargetSetup>> s_xPendingCommandLists[RENDER_ORDER_MAX];
};

struct Flux_PipelineSpecification
{
	Flux_PipelineSpecification() = default;

	Flux_Shader* m_pxShader;

	Flux_BlendState m_axBlendStates[FLUX_MAX_TARGETS];

	bool m_bDepthTestEnabled = true;
	bool m_bDepthWriteEnabled = true;
	DepthCompareFunc m_eDepthCompareFunc = DEPTH_COMPARE_FUNC_LESSEQUAL;
	DepthStencilFormat m_eDepthStencilFormat;
	bool m_bUsePushConstants = true;
	bool m_bUseTesselation = false;

	Flux_PipelineLayout m_xPipelineLayout;

	Flux_VertexInputDescription m_xVertexInputDesc;

	struct Flux_TargetSetup* m_pxTargetSetup;
	LoadAction m_eColourLoadAction;
	StoreAction m_eColourStoreAction;
	LoadAction m_eDepthStencilLoadAction;
	StoreAction m_eDepthStencilStoreAction;
	bool m_bWireframe = false;
};
