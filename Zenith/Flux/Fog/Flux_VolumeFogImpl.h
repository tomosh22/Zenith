#pragma once

#include "Flux/Fog/Flux_VolumeFog.h"
#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "AssetHandling/Zenith_AssetHandle.h"

// Phase 7d: per-Engine state for the VolumeFog (shared fog infra)
// subsystem.
class Flux_VolumeFogImpl
{
public:
	Flux_VolumeFogImpl() = default;
	~Flux_VolumeFogImpl() = default;

	Flux_VolumeFogImpl(const Flux_VolumeFogImpl&) = delete;
	Flux_VolumeFogImpl& operator=(const Flux_VolumeFogImpl&) = delete;

	TextureHandle             m_xNoiseTexture3D;
	TextureHandle             m_xBlueNoiseTexture;
	Flux_RenderAttachment     m_xFroxelDensityGrid;
	Flux_RenderAttachment     m_xFroxelLightingGrid;
	Flux_RenderAttachment     m_xDebugOutput;
	Flux_VolumeFogConstants   m_xSharedConstants;
	Flux_FroxelConfig         m_xFroxelConfig;
};
