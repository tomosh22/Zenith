#pragma once
#include "Flux/Flux.h"
#include "Flux/Flux_Enums.h"

struct Flux_BlendState
{
	BlendFactor m_eSrcBlendFactor;
	BlendFactor m_eDstBlendFactor;
	bool m_bBlendEnabled;
};

struct Flux_RenderAttachment {

	ColourFormat m_eColourFormat = COLOUR_FORMAT_NONE;
	DepthStencilFormat m_eDepthStencilFormat = DEPTHSTENCIL_FORMAT_NONE;

	uint32_t m_uWidth = 0;
	uint32_t m_uHeight = 0;

	//one per frame in flight
	std::vector<Flux_Texture*> m_axTargetTextures;
};

struct Flux_TargetSetup {
	Flux_RenderAttachment m_axColourAttachments[FLUX_MAX_TARGETS];
	Flux_RenderAttachment m_xDepthStencil;
	std::string m_strName;
};

class Flux_RenderAttachmentBuilder {
public:
	Flux_RenderAttachmentBuilder() = default;
	ColourFormat m_eColourFormat = COLOUR_FORMAT_NONE;
	DepthStencilFormat m_eDepthStencilFormat = DEPTHSTENCIL_FORMAT_NONE;
	uint32_t m_uWidth;
	uint32_t m_uHeight;

	void Build(Flux_RenderAttachment& xAttachment, RenderTargetType eType);
};