#pragma once

#include "Maths/Zenith_Maths.h"

// Renderer-neutral scene-sun boundary. EntityComponent owns the concrete
// Zenith_SunComponent and publishes the resolved direction through this POD +
// function pointer; Flux consumes it without a reverse dependency on ECS
// component types.

inline Zenith_Maths::Vector3 Zenith_GetDefaultSunDirection()
{
	// Direction the light travels INTO the scene. This is the exact historical
	// global direction value and is the fallback when no loaded scene authors a Sun.
	return Zenith_Maths::Vector3(-0.4f, -0.7f, -0.55f);
}

struct Zenith_SunAuthorityData
{
	bool                  m_bAuthored = false;
	Zenith_Maths::Vector3 m_xDirection = Zenith_GetDefaultSunDirection();
	u_int                 m_uSourceEntityIndex = 0xFFFFFFFFu;
	u_int                 m_uAuthoredCount = 0u;
	bool                  m_bSourceIsInActiveScene = false;
};

using Zenith_SunAuthorityGatherFn = void (*)(Zenith_SunAuthorityData& xOut);
extern Zenith_SunAuthorityGatherFn g_pfnZenithSunAuthorityGather;
