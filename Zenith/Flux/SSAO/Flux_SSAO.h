#pragma once

class Flux_SSAO
{
public:
	static bool s_bEnabled;

	static void Initialise();
	static void Shutdown();

	static void Reset();

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();

private:
	static void RenderCompute();
	static void RenderBlur();
	static void RenderUpsample();

	static void CreateRenderTargets();
	static void DestroyRenderTargets();
};
