#pragma once
#include "Flux/Flux.h"

class Flux_DeferredShading
{
public:
	static void Initialise();
	static void BeginFrame();
	static void Render();

	static Flux_CommandBuffer& GetSkyboxCommandBuffer();
	static Flux_CommandBuffer& GetStaticMeshesCommandBuffer();
	static Flux_CommandBuffer& GetAnimatedMeshesCommandBuffer();
	static Flux_CommandBuffer& GetTerrainCommandBuffer();
};