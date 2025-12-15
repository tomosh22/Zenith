#pragma once
#include "Flux/Flux.h"

class Flux_DeferredShading
{
public:
	static void Initialise();

	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();
};