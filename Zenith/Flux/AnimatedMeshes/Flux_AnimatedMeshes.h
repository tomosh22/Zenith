#pragma once

class Flux_AnimatedMeshes
{
public:
	static void Initialise();

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();
};