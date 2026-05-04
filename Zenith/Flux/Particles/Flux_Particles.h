#pragma once

class Flux_CommandList;
class Flux_RenderGraph;

class Flux_Particles
{
public:
	static void Initialise();
	static void BuildPipelines();

	// Drop refs to particle texture before the asset registry shuts down.
	static void ReleaseAssetReferences();

	static void Shutdown();

	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void Render(void*);

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

private:
	static void ExecuteParticles(Flux_CommandList* pxCommandList, void* pUserData);
};