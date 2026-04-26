#pragma once

class Flux_CommandList;
class Flux_RenderGraph;

class Flux_SDFs
{
public:
	static void Initialise();
	static void BuildPipelines();
	static void Shutdown();

	static void Render(void*);

	static void SetupRenderGraph(Flux_RenderGraph& xGraph);

private:
	static void ExecuteSDFs(Flux_CommandList* pxCommandList, void* pUserData);
};