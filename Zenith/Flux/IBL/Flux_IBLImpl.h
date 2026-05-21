#pragma once

#include "Flux/IBL/Flux_IBL.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

// Phase 7f: per-Engine state for IBL subsystem.
class Flux_IBLImpl
{
public:
	Flux_IBLImpl() = default;
	~Flux_IBLImpl() = default;

	Flux_IBLImpl(const Flux_IBLImpl&) = delete;
	Flux_IBLImpl& operator=(const Flux_IBLImpl&) = delete;

	// State flags.
	bool                       m_bBRDFLUTGenerated = false;
	bool                       m_bSkyIBLDirty      = true;
	bool                       m_bIBLReady         = false;
	bool                       m_bFirstGeneration  = true;

	// Render attachments.
	Flux_RenderAttachment      m_xBRDFLUT;
	Flux_RenderAttachmentCube  m_xIrradianceMap;
	Flux_RenderAttachmentCube  m_xPrefilteredMap;

	// Pipelines + shaders.
	Flux_Pipeline              m_xBRDFLUTPipeline;
	Flux_Pipeline              m_xIrradianceConvolvePipeline;
	Flux_Pipeline              m_xPrefilterPipeline;
	Flux_Shader                m_xBRDFLUTShader;
	Flux_Shader                m_xIrradianceConvolveShader;
	Flux_Shader                m_xPrefilterShader;

	// Configuration.
	float                      m_fIntensity = 1.0f;

	// Regen state machine.
	IBL_RegenState             m_eRegenState = IBL_REGEN_IDLE;
	u_int                      m_uRegenFace  = 0;
	u_int                      m_uRegenMip   = 0;

	// Pass handles.
	Flux_PassHandle            m_xBRDFLUTPassHandle = {};
	Flux_PassHandle            m_axIrradianceFacePassHandles[6] = {};
	Flux_PassHandle            m_axPrefilterMipFacePassHandles[IBLConfig::uPREFILTER_MIP_COUNT][6] = {};
};
