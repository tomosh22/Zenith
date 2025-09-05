#pragma once
#include "Flux/Flux.h"

class Flux_DeferredShading
{
public:
	static void Initialise();
	static void BeginFrame();
	static void Render();
};