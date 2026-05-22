#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_FrustumCulling.h"

struct LightInstance;

// Phase 9: state + behaviour for DynamicLights subsystem.
class Flux_DynamicLightsImpl
{
public:
	Flux_DynamicLightsImpl() = default;
	~Flux_DynamicLightsImpl() = default;

	Flux_DynamicLightsImpl(const Flux_DynamicLightsImpl&) = delete;
	Flux_DynamicLightsImpl& operator=(const Flux_DynamicLightsImpl&) = delete;

	void Initialise();
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
};
