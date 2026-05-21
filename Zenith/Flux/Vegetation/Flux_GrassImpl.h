#pragma once

#include "Flux/Vegetation/Flux_Grass.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_Maths.h"

// Phase 7g: per-Engine state for Grass subsystem.
class Flux_GrassImpl
{
public:
	Flux_GrassImpl() = default;
	~Flux_GrassImpl() = default;

	Flux_GrassImpl(const Flux_GrassImpl&) = delete;
	Flux_GrassImpl& operator=(const Flux_GrassImpl&) = delete;

	// Pipelines + shaders.
	Flux_Pipeline                     m_xGrassPipeline;
	Flux_Shader                       m_xGrassShader;

	// Instance buffer.
	Flux_ReadWriteBuffer              m_xInstanceBuffer;
	u_int                             m_uAllocatedInstances = 0;

	// Chunk management.
	Zenith_Vector<GrassChunk>         m_axChunks;
	u_int                             m_uVisibleBladeCount = 0;
	u_int                             m_uActiveChunkCount  = 0;

	// CPU-side instance storage.
	Zenith_Vector<GrassBladeInstance> m_axAllInstances;
	bool                              m_bInstancesGenerated = false;
	bool                              m_bInstancesUploaded  = false;

	// Configuration.
	float                             m_fDensityScale  = 1.0f;
	float                             m_fMaxDistance   = GrassConfig::fMAX_DISTANCE;
	float                             m_fWindStrength  = 1.0f;
	Zenith_Maths::Vector2             m_xWindDirection = glm::normalize(Zenith_Maths::Vector2(1.0f, 0.2f));

	// Constants buffer.
	Flux_DynamicConstantBuffer        m_xGrassConstantsBuffer;
};
