#pragma once

#include "Zenith_PlatformGraphics_Include.h"
#include "Flux/Flux_Enums.h"

class Flux
{
public:
	Flux() = delete;
	~Flux() = delete;
	static void EarlyInitialise();
	static void LateInitialise();

	static const uint32_t GetFrameCounter() { return s_uFrameCounter; }

	static void AddResChangeCallback(void(*pfnCallback)()) { s_xResChangeCallbacks.push_back(pfnCallback); }
	static void OnResChange();
private:
	static uint32_t s_uFrameCounter;
	static std::vector<void(*)()> s_xResChangeCallbacks;
};