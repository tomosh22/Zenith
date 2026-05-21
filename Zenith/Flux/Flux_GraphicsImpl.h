#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_AssetHandle.h"

class Flux_RenderGraph;

// Phase 6a-2: per-Engine state for the Flux_Graphics static facade.
// Replaces the 17 file-static data members on Flux_Graphics: samplers,
// shared geometry, frame-constants buffer + struct + layout, fallback
// texture / material handles, scene textures, MRT format array, and
// the transient render-graph handles + graph ref.
//
// Static facade methods on Flux_Graphics keep their signatures; bodies
// reach this Impl via g_xEngine.FluxGraphics().m_xXxx.
class Flux_GraphicsImpl
{
public:
	Flux_GraphicsImpl() = default;
	~Flux_GraphicsImpl() = default;

	Flux_GraphicsImpl(const Flux_GraphicsImpl&) = delete;
	Flux_GraphicsImpl& operator=(const Flux_GraphicsImpl&) = delete;

	// Samplers (initialised in Flux_Graphics::InitialiseSamplers).
	Flux_Sampler                m_xRepeatSampler;
	Flux_Sampler                m_xClampSampler;

	// Shared geometry / per-frame UBO.
	Flux_MeshGeometry           m_xQuadMesh;
	Flux_DynamicConstantBuffer  m_xFrameConstantsBuffer;

	// Fallback assets (pinned handles so UnloadUnused doesn't free them).
	TextureHandle               m_xWhiteTexture;
	TextureHandle               m_xBlackTexture;
	TextureHandle               m_xGridTexture;
	Flux_MeshGeometry           m_xBlankMesh;
	MaterialHandle              m_xBlankMaterial;

	// Scene textures (set during initialization).
	TextureHandle               m_xCubemapTexture;
	TextureHandle               m_xWaterNormalTexture;

	// MRT formats (3 entries by MRTIndex).
	TextureFormat               m_aeMRTFormats[MRT_INDEX_COUNT] = {};

	// Per-frame CPU-side constants struct + binding-group layout.
	Flux_Graphics::FrameConstants m_xFrameConstants;
	Flux_BindingGroupLayout       m_xFrameConstantsLayout;

	// Graph-owned transient render-target handles + graph back-ref.
	Flux_TransientHandle        m_axMRTHandles[MRT_INDEX_COUNT];
	Flux_TransientHandle        m_xFinalRTHandle;
	Flux_TransientHandle        m_xDepthHandle;
	Flux_RenderGraph*           m_pxGraph = nullptr;
};
