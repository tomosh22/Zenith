#pragma once

#include "Maths/Zenith_Maths.h"

struct Zenith_GraphicsOptions
{
	uint32_t m_uWindowWidth = 1280;
	uint32_t m_uWindowHeight = 720;
	bool m_bFogEnabled = true;
	bool m_bSSREnabled = true;
	bool m_bSSAOEnabled = true;
	bool m_bSSGIEnabled = false;
	bool m_bSkyboxEnabled = true;
	Zenith_Maths::Vector3 m_xSkyboxColour = Zenith_Maths::Vector3(0.f);
};
