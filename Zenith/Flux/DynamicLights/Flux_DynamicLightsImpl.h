#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_FrustumCulling.h"

class Flux_GraphicsImpl;

// 64 bytes — sized to spot's worst-case footprint so all light types
// share one struct. Padding fields stay zero on CPU staging.
// Mirrors Common.Lighting.slang LightInstance.
struct LightInstance
{
	Zenith_Maths::Vector4 m_xPositionRange;   // xyz=pos, w=range
	Zenith_Maths::Vector4 m_xColorIntensity;  // xyz=color, w=intensity
	Zenith_Maths::Vector4 m_xDirectionInner;  // xyz=dir, w=cos(inner)
	Zenith_Maths::Vector4 m_xTypeOuter;       // x=cos(outer), y=type tag, zw=pad
};
static_assert(sizeof(LightInstance) == 64, "LightInstance must be 64 bytes — must match Common.Lighting.slang LightInstance");

// Used when total lights exceed uMAX_LIGHTS — pick the highest-priority
// uMAX_LIGHTS to keep, drop the rest.
struct LightSortKey
{
	float m_fPriority;
	u_int m_uIndex;

	bool operator<(const LightSortKey& other) const
	{
		// Sort descending by priority — highest first.
		return m_fPriority > other.m_fPriority;
	}
};

// Phase 9: state + behaviour for DynamicLights subsystem.
class Flux_DynamicLightsImpl
{
public:
	Flux_DynamicLightsImpl() = default;
	~Flux_DynamicLightsImpl() = default;

	Flux_DynamicLightsImpl(const Flux_DynamicLightsImpl&) = delete;
	Flux_DynamicLightsImpl& operator=(const Flux_DynamicLightsImpl&) = delete;

	void Initialise(Flux_MemoryManager& xVulkanMemory, Flux_GraphicsImpl& xFluxGraphics);
	void Shutdown();
	void Reset();

	void GatherLightsFromScene();

	Flux_ShaderResourceView_Buffer& GetLightBufferSRV();
	Flux_DynamicReadWriteBuffer& GetLightBuffer() { return m_xLightBuffer; }
	u_int GetLightCount() const { return m_uLightCount; }

	bool IsInitialised() const { return m_bInitialised; }

	static constexpr u_int uMAX_LIGHTS = 256;

	bool                        m_bInitialised = false;
	Zenith_Frustum              m_xCameraFrustum;
	u_int                       m_uLightCount = 0;
	Flux_DynamicReadWriteBuffer m_xLightBuffer;

	// CPU staging — flat array of all light types, packed by GatherLightsFromScene.
	LightInstance               m_axLightStaging[uMAX_LIGHTS];

	// Priority sort scratch — used when total lights exceed uMAX_LIGHTS.
	Zenith_Vector<LightSortKey> m_xSortBuffer;

	Flux_MemoryManager* m_pxVulkanMemory = nullptr;
	Flux_GraphicsImpl*           m_pxFluxGraphics = nullptr;
};
