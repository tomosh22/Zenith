#pragma once

#include "Flux/Flux.h"

class Flux_DynamicLights
{
public:
	static void Initialise();
	static void Shutdown();
	static void Reset();

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();

	// Called each frame to gather lights from scene
	static void GatherLightsFromScene();

	static bool IsInitialised() { return s_bInitialised; }

	static constexpr u_int uMAX_LIGHTS = 256;

private:
	static bool s_bInitialised;
};
