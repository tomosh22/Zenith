#pragma once

class Flux_SDFs
{
public:
	static void Initialise();

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();
};