#pragma once

#include "Flux/DynamicLights/Flux_DynamicLights.h"
#include "Flux/Flux_Buffers.h"
#include "Maths/Zenith_FrustumCulling.h"

struct LightInstance;

// Phase 7c: per-Engine state for DynamicLights subsystem.
class Flux_DynamicLightsImpl
{
public:
	Flux_DynamicLightsImpl() = default;
	~Flux_DynamicLightsImpl() = default;

	Flux_DynamicLightsImpl(const Flux_DynamicLightsImpl&) = delete;
	Flux_DynamicLightsImpl& operator=(const Flux_DynamicLightsImpl&) = delete;

	bool                        m_bInitialised = false;
	Zenith_Frustum              m_xCameraFrustum;
	u_int                       m_uLightCount = 0;
	Flux_DynamicReadWriteBuffer m_xLightBuffer;
	// LightInstance staging array kept in the .cpp because LightInstance
	// is a file-local POD struct in Flux_DynamicLights.cpp.
};
