#pragma once
#include "Flux/Flux.h"
#include "Flux/Flux_Enums.h"


struct Flux_RenderAttachment {
	ColourFormat m_eColourFormat = COLOUR_FORMAT_NONE;
	DepthStencilFormat m_eDepthStencilFormat = DEPTHSTENCIL_FORMAT_NONE;

	uint32_t m_uWidth = 0;
	uint32_t m_uHeight = 0;

	//one per frame in flight
	Flux_Texture* m_pxTargetTexture;
};

struct Flux_TargetSetup {
	Flux_RenderAttachment m_axColourAttachments[FLUX_MAX_TARGETS];

	//#TO not owned by this
	Flux_RenderAttachment* m_pxDepthStencil = nullptr;

	std::string m_strName;

	void AssignDepthStencil(Flux_RenderAttachment* pxDS);

	const uint32_t GetNumColourAttachments();
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