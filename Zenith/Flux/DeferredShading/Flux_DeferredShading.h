#pragma once
#include "Flux/Flux.h"

class Flux_DeferredShading
{
public:
	static void Initialise();
	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();
};